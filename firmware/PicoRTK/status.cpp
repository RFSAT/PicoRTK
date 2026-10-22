// status.cpp - core 0: LEDs and the setup button
//   GP14 LED "RTK":          on = RTK fixed, blinking = RTK float, off = no RTK
//   GP15 LED "Corrections":  on = streaming, blinking = connecting, off = idle/no network
//   GP16 button to GND:      hold 3 s = open the setup access point
//   On-board LED:            heartbeat
#include "status.h"
#include "net.h"
#include "ntrip.h"
#include "shared.h"
#include <Arduino.h>

static const int PIN_LED_RTK = 14;
static const int PIN_LED_CORR = 15;
static const int PIN_BUTTON = 16;
static uint32_t pressedAt = 0;
static bool apTriggered = false;

void statusSetup()
{
    pinMode(PIN_LED_RTK, OUTPUT);
    pinMode(PIN_LED_CORR, OUTPUT);
    pinMode(PIN_BUTTON, INPUT_PULLUP);
    pinMode(LED_BUILTIN, OUTPUT);
}

void statusLoop()
{
    uint32_t now = millis();
    bool blink = (now / 250) & 1;

    SharedGga g;
    int q = getLatestGga(g) && (now - g.atMs) < 3000 ? g.info.quality : 0;
    digitalWrite(PIN_LED_RTK, q == 4 ? HIGH : (q == 5 ? blink : LOW));

    NtripStatus s;
    ntripStatus(s);
    bool connecting = s.state == NtripState::Waiting || s.state == NtripState::WaitFix ||
                      s.state == NtripState::Connecting || s.state == NtripState::Reply;
    digitalWrite(PIN_LED_CORR, s.state == NtripState::Streaming ? HIGH : (connecting ? blink : LOW));

    static uint32_t lastBeat = 0;
    if (now - lastBeat > (netApMode() ? 200u : 1000u))
    {
        lastBeat = now;
        static bool on = false;
        on = !on;
        digitalWrite(LED_BUILTIN, on);
    }

    if (digitalRead(PIN_BUTTON) == LOW)
    {
        if (!pressedAt)
            pressedAt = now;
        else if (!apTriggered && now - pressedAt > 3000)
        {
            apTriggered = true;
            netStartAp();
        }
    }
    else
    {
        pressedAt = 0;
        apTriggered = false;
    }
}
