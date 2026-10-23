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

void safeReboot()
{
    dbg("[boot] restarting\r\n");
    Serial.flush();
    outputsStop();
    WiFi.disconnect();
    WiFi.softAPdisconnect(true);
    delay(200);
    // Power the CYW43 radio off (WL_ON = GPIO23 on the Pico 2 W) so it is reset with the RP2350
    gpio_init(23);
    gpio_set_dir(23, GPIO_OUT);
    gpio_put(23, 0);
    delay(100);
    watchdog_reboot(0, 0, 10);
    while (true)
        tight_loop_contents();
}

void setup()
{
    Serial.begin(115200);
    // Give a serial terminal a moment to attach so the start-up log is not lost
    for (uint32_t t = millis(); !Serial && millis() - t < 1500;)
        delay(10);
    dbg("\r\n[boot] PicoRTK %s\r\n", PICORTK_VERSION);
    receiverInitShared();
    if (!LittleFS.begin())
    {
        LittleFS.format();
        LittleFS.begin();
    }
    configLoad();
    dbg("[boot] settings loaded, Bluetooth %s\r\n", cfg.bt.c_str());
    configReady = true; // core 1 may start the UART now

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

void loop1() { receiverLoop1(); }
