// receiver.cpp - core 1: LG290P UART, USB bridge, NMEA extraction
#include "config.h"
#include "receiver.h"
#include "shared.h"
#include <Arduino.h>

SpscRing<16384> rtcmToRx;
SpscRing<1024> cmdToRx;
SpscRing<16384> nmeaToNet;
SharedGga sharedGga;
mutex_t ggaMutex;
Stats stats;
std::atomic<bool> configReady{false};

static NmeaAssembler nmea;

bool getLatestGga(SharedGga &out)
{
    mutex_enter_blocking(&ggaMutex);
    out = sharedGga;
    mutex_exit(&ggaMutex);
    return out.len > 0;
}

void receiverInitShared()
{
    mutex_init(&ggaMutex);
    sharedGga.len = 0;
}

void receiverSetup1()
{
    // Wait until core 0 has loaded the settings
    while (!configReady.load())
        delay(5);
    Serial1.setTX(PIN_RX_TXD);
    Serial1.setRX(PIN_RX_RXD);
    Serial1.setFIFOSize(8192);
    Serial1.begin(cfg.uartBaud);
    pinMode(PIN_RX_RTK, INPUT_PULLDOWN);
}

static void handleSentence(const char *s, size_t len)
{
    stats.nmeaGood.store(nmea.good());
    nmeaToNet.write((const uint8_t *)s, len); // drops the sentence if nobody drains the ring
    if (nmeaIsGga(s, len))
    {
        GgaInfo g;
        if (nmeaParseGga(s, len, g))
        {
            mutex_enter_blocking(&ggaMutex);
            memcpy(sharedGga.sentence, s, len);
            sharedGga.sentence[len] = 0;
            sharedGga.len = len;
            sharedGga.info = g;
            sharedGga.atMs = millis();
            mutex_exit(&ggaMutex);
        }
    }
}

void receiverLoop1()
{
    uint8_t buf[512];

    // Receiver -> USB (raw) and NMEA extraction
    int n = Serial1.available();
    if (n > 0)
    {
        if (n > (int)sizeof(buf))
            n = sizeof(buf);
        n = Serial1.readBytes(buf, n);
        stats.uartRxBytes += n;
        if (cfg.usb && Serial && Serial.availableForWrite() >= n)
            Serial.write(buf, n);
        for (int i = 0; i < n; i++)
            if (nmea.feed(buf[i]))
                handleSentence(nmea.sentence(), nmea.length());
        stats.nmeaBad.store(nmea.bad());
    }

    // USB -> receiver (QGNSS through the Pico)
    if (cfg.usb)
    {
        int u = Serial.available();
        if (u > 0)
        {
            if (u > (int)sizeof(buf))
                u = sizeof(buf);
            u = Serial.readBytes(buf, u);
            Serial1.write(buf, u);
            stats.uartTxBytes += u;
        }
    }

    // Commands and corrections from core 0 -> receiver
    size_t c = cmdToRx.read(buf, sizeof(buf));
    if (c)
    {
        Serial1.write(buf, c);
        stats.uartTxBytes += c;
    }
    size_t r = rtcmToRx.read(buf, sizeof(buf));
    if (r)
    {
        Serial1.write(buf, r);
        stats.uartTxBytes += r;
        stats.lastRtcmToRxMs.store(millis());
    }
}

bool receiverRtkPin() { return digitalRead(PIN_RX_RTK); }
