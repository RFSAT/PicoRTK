// PicoRTK - RTK GNSS rover firmware for Raspberry Pi Pico 2 W + Quectel LG290P (Waveshare module)
// RFSAT Ltd. The LG290P computes the RTK solution; the Pico fetches NTRIP corrections over Wi-Fi
// (ordered sources with failover) and serves NMEA over Bluetooth SPP / BLE, USB and TCP.
//
// Core 0: Wi-Fi, Bluetooth, NTRIP, web page (everything that waits on the radio)
// Core 1: receiver UART and USB bridge (tight loop, never blocked by the network)
//
// Board: Raspberry Pi Pico 2 W, IP stack "IPv4 + Bluetooth", flash "4MB (Sketch: 3MB, FS: 1MB)"

#include "config.h"
#include "net.h"
#include "ntrip.h"
#include "outputs.h"
#include "receiver.h"
#include "shared.h"
#include "status.h"
#include "util.h"
#include "web.h"
#include <LittleFS.h>
#include <WiFi.h>
#include <hardware/gpio.h>
#include <hardware/watchdog.h>
#include <hardware/structs/watchdog.h>
#include <stdarg.h>

#ifndef PICORTK_VERSION
#define PICORTK_VERSION "dev"
#endif

static uint32_t bootCmdsAt = 0;

void dbg(const char *fmt, ...)
{
    // USB carries the receiver stream once the receiver talks; until then it carries this log
    if (cfg.usb && stats.uartRxBytes.load() > 0)
        return;
    char buf[192];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    Serial.print(buf);
}

// Core 0 heartbeat: core 1 feeds the hardware watchdog only while core 0 is alive,
// so a hang on either core restarts the board instead of leaving it dead.
std::atomic<uint32_t> core0Beat{0};
std::atomic<bool> wdtArmed{false};
static const uint32_t WDT_MS = 8000;           // hardware watchdog period
static const uint32_t CORE0_STALL_MS = 60000;  // longest blocking call on core 0 (Wi-Fi join, TCP connect)

void safeReboot()
{
    // Safety net first: whatever happens below, the watchdog resets the chip within 3 s
    wdtArmed = false;
    watchdog_enable(3000, true);
    dbg("[boot] restarting\r\n");
    // Power the CYW43 radio off straight away (WL_ON = GPIO23 on the Pico 2 W), no graceful
    // Bluetooth/Wi-Fi shutdown that could itself block, so the radio starts cold after the reset
    gpio_init(23);
    gpio_set_dir(23, GPIO_OUT);
    gpio_put(23, 0);
    busy_wait_ms(300);
    watchdog_reboot(0, 0, 10);
    while (true)
        tight_loop_contents();
}

void safeShutdown()
{
    dbg("[boot] shutting down\r\n");
    ntripStop();
    outputsStop();
    core1Halt = true;      // core 1 stops using the UART and USB
    delay(100);
    Serial1.flush();
    wdtArmed = false;
    hw_clear_bits(&watchdog_hw->ctrl, WATCHDOG_CTRL_ENABLE_BITS); // no automatic restart
    // LEDs off, radio off (the on-board LED is driven by the radio chip, so it goes dark too)
    digitalWrite(14, LOW);
    digitalWrite(15, LOW);
    gpio_init(23);
    gpio_set_dir(23, GPIO_OUT);
    gpio_put(23, 0);
    dbg("[boot] halted, safe to unplug\r\n");
    while (true)
        __wfi();
}

static const char *resetReasonName()
{
    switch (rp2040.getResetReason())
    {
    case RP2040::PWRON_RESET: return "power-on";
    case RP2040::RUN_PIN_RESET: return "RUN pin";
    case RP2040::SOFT_RESET: return "software restart";
    case RP2040::WDT_RESET: return "watchdog (recovered from a hang)";
    case RP2040::DEBUG_RESET: return "debug";
    case RP2040::GLITCH_RESET: return "power glitch";
    case RP2040::BROWNOUT_RESET: return "brown-out";
    default: return "unknown";
    }
}
const char *bootReason = "unknown";

void setup()
{
    Serial.begin(115200);
    // Give a serial terminal a moment to attach so the start-up log is not lost
    for (uint32_t t = millis(); !Serial && millis() - t < 1500;)
        delay(10);
    bootReason = resetReasonName();
    dbg("\r\n[boot] PicoRTK %s, last reset: %s\r\n", PICORTK_VERSION, bootReason);
    receiverInitShared();
    if (!LittleFS.begin())
    {
        LittleFS.format();
        LittleFS.begin();
    }
    configLoad();
    dbg("[boot] settings loaded, Bluetooth %s\r\n", cfg.bt.c_str());
    configReady = true; // core 1 may start the UART now
    core0Beat = millis();
    watchdog_enable(WDT_MS, true); // fed by core 1 (see loop1)
    wdtArmed = true;

    statusSetup();
    outputsSetup();
    dbg("[boot] Bluetooth started as %s\r\n", outputsBtName().c_str());
    netSetup();
    webSetup();
    ntripSetup();
    bootCmdsAt = millis() + 3000; // let the receiver boot first
}

// Status line on USB every 5 s (only while no receiver data is flowing, see dbg)
static void logStatus()
{
    static uint32_t last = 0;
    if (millis() - last < 5000)
        return;
    last = millis();
    NtripStatus s;
    ntripStatus(s);
    dbg("[picortk %s] wifi=%s ssid=%s ip=%s rssi=%d | ntrip=%s src=%d | uart rx=%lu B\r\n", PICORTK_VERSION,
        netApMode() ? "setup-AP" : (netUp() ? "connected" : "searching"), netSsid().c_str(), netIp().c_str(), netRssi(),
        ntripStateName(s.state), s.source + 1, (unsigned long)stats.uartRxBytes.load());
}

static void sendBootCommands()
{
    for (int i = 0; i < CFG_MAX_BOOTCMDS; i++)
    {
        if (!cfg.bootCmd[i].length())
            continue;
        char out[140];
        size_t n = nmeaFinish(cfg.bootCmd[i].c_str(), out, sizeof(out));
        if (n)
            cmdToRx.write((const uint8_t *)out, n);
    }
}

void loop()
{
    core0Beat = millis();
    netLoop();
    bool up = netUp();
    outputsNetworkUp(up);
    webLoop();
    ntripLoop(up);
    outputsLoop(ntripStreaming());
    statusLoop();
    logStatus();
    if (bootCmdsAt && (int32_t)(millis() - bootCmdsAt) > 0)
    {
        bootCmdsAt = 0;
        sendBootCommands();
    }
}

void setup1() { receiverSetup1(); }

void loop1()
{
    receiverLoop1();
    if (wdtArmed && millis() - core0Beat.load() < CORE0_STALL_MS)
        watchdog_update();
}
