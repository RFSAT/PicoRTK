// receiver.cpp - core 1: LG290P UART, USB bridge, NMEA extraction
#include "config.h"
#include "receiver.h"
#include "shared.h"
#include "diag.h"
#include <Arduino.h>

SpscRing<16384> rtcmToRx;
SpscRing<1024> cmdToRx;
SpscRing<16384> nmeaToNet;
SpscRing<8192> histToCore0;
static uint32_t rmcYmd = 0; // UTC date from the latest RMC
SharedGga sharedGga;
static SharedAccuracy sharedAcc;
static MethodTracker tracker; // under ggaMutex
static RefPoint refPoint;     // under ggaMutex
mutex_t ggaMutex;
Stats stats;
std::atomic<bool> configReady{false};

static NmeaAssembler nmea;
NmeaTypeStat nmeaTypes[MAX_NMEA_TYPES];
int nmeaTypeCount = 0;
mutex_t typesMutex;
std::atomic<bool> core1Halt{false};
char rxReplies[RX_REPLIES][NMEA_MAX + 1];
uint32_t rxReplySeq = 0;

// Output filter, parsed once from cfg.nmeaTypes
static char allowIds[MAX_NMEA_TYPES][NMEA_ID_MAX];
static int allowCount = 0;

static void parseFilter()
{
    allowCount = 0;
    String s = cfg.nmeaTypes;
    int from = 0;
    while (from < (int)s.length() && allowCount < MAX_NMEA_TYPES)
    {
        int comma = s.indexOf(',', from);
        if (comma < 0)
            comma = s.length();
        String t = s.substring(from, comma);
        t.trim();
        if (t.length())
            strlcpy(allowIds[allowCount++], t.c_str(), sizeof(allowIds[0]));
        from = comma + 1;
    }
}

static bool allowed(const char *id)
{
    if (allowCount == 0)
        return true; // no filter: everything
    for (int i = 0; i < allowCount; i++)
        if (strcmp(allowIds[i], id) == 0)
            return true;
    return false;
}

static void countType(const char *id, bool sent)
{
    mutex_enter_blocking(&typesMutex);
    int i = 0;
    for (; i < nmeaTypeCount; i++)
        if (strcmp(nmeaTypes[i].id, id) == 0)
            break;
    if (i == nmeaTypeCount && nmeaTypeCount < MAX_NMEA_TYPES)
    {
        strlcpy(nmeaTypes[i].id, id, sizeof(nmeaTypes[i].id));
        nmeaTypes[i].count = 0;
        nmeaTypeCount++;
    }
    if (i < nmeaTypeCount)
    {
        nmeaTypes[i].count++;
        nmeaTypes[i].sent = sent;
    }
    mutex_exit(&typesMutex);
}

void getAccuracy(SharedAccuracy &out)
{
    mutex_enter_blocking(&ggaMutex);
    out = sharedAcc;
    mutex_exit(&ggaMutex);
}

static void parseAccuracy(const char *s, size_t len, const char *id)
{
    uint32_t now = millis();
    if (strcmp(id, "GST") == 0)
    {
        float a, b, c;
        if (nmeaParseGst(s, len, a, b, c))
        {
            mutex_enter_blocking(&ggaMutex);
            sharedAcc.gstLat = a; sharedAcc.gstLon = b; sharedAcc.gstAlt = c; sharedAcc.gstAtMs = now;
            mutex_exit(&ggaMutex);
        }
    }
    else if (strcmp(id, "PQTMEPE") == 0)
    {
        float n, e, d, h, v;
        if (nmeaParseEpe(s, len, n, e, d, h, v))
        {
            mutex_enter_blocking(&ggaMutex);
            sharedAcc.epe2d = h; sharedAcc.epeDown = d; sharedAcc.epeAtMs = now;
            mutex_exit(&ggaMutex);
        }
    }
    else if (strcmp(id, "PQTMPPPNAV") == 0)
    {
        PppNav p;
        if (nmeaParsePppNav(s, len, p))
        {
            mutex_enter_blocking(&ggaMutex);
            sharedAcc.ppp = p; sharedAcc.pppAtMs = now;
            mutex_exit(&ggaMutex);
        }
    }
}

void getMethods(MethodTracker &out, RefPoint &ref)
{
    mutex_enter_blocking(&ggaMutex);
    out = tracker;
    ref = refPoint;
    mutex_exit(&ggaMutex);
}

void setRefPoint(const RefPoint &r)
{
    mutex_enter_blocking(&ggaMutex);
    refPoint = r;
    mutex_exit(&ggaMutex);
}

// Receiver's horizontal / vertical estimate for the method in use (-1 = none)
static void estimateFor(Method m, uint32_t now, float &h, float &v)
{
    h = v = -1;
    const SharedAccuracy &a = sharedAcc;
    if ((m == M_HAS_CONV || m == M_HAS_DONE) && a.pppAtMs && now - a.pppAtMs < 3000 && a.ppp.latStd >= 0 && a.ppp.lonStd >= 0)
    {
        h = sqrtf(a.ppp.latStd * a.ppp.latStd + a.ppp.lonStd * a.ppp.lonStd);
        v = a.ppp.altStd;
        return;
    }
    if (a.epeAtMs && now - a.epeAtMs < 3000 && a.epe2d >= 0)
    {
        h = a.epe2d;
        v = fabsf(a.epeDown);
        return;
    }
    if (a.gstAtMs && now - a.gstAtMs < 3000 && a.gstLat >= 0 && a.gstLon >= 0)
    {
        h = sqrtf(a.gstLat * a.gstLat + a.gstLon * a.gstLon);
        v = a.gstAlt;
    }
}

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
    mutex_init(&typesMutex);
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
    parseFilter();
}

static void handleSentence(const char *s, size_t len)
{
    diagC1 = 30;
    stats.nmeaGood.store(nmea.good());
    char id[NMEA_ID_MAX];
    bool send = true;
    if (nmeaIsReply(s, len))
    {
        // Command replies: kept for the web page, not counted as message types
        mutex_enter_blocking(&typesMutex);
        size_t n = len;
        while (n && (s[n - 1] == '\r' || s[n - 1] == '\n'))
            n--;
        if (n > NMEA_MAX)
            n = NMEA_MAX;
        char *dst = rxReplies[rxReplySeq % RX_REPLIES];
        memcpy(dst, s, n);
        dst[n] = 0;
        rxReplySeq++;
        mutex_exit(&typesMutex);
        if (nmeaSentenceId(s, len, id, sizeof(id)))
            send = allowed(id);
    }
    else if (nmeaSentenceId(s, len, id, sizeof(id)))
    {
        send = allowed(id);
        diagC1 = 31;
        countType(id, send);
        diagC1 = 32;
        parseAccuracy(s, len, id);
        diagC1 = 33;
        if (strcmp(id, "RMC") == 0)
        {
            uint32_t d = rmcDate(s, len);
            if (d)
                rmcYmd = d;
        }
    }
    static bool holding = false;
    if (nmeaIsGga(s, len))
    {
        GgaInfo g;
        if (nmeaParseGga(s, len, g))
        {
            uint32_t now = millis();
            const PppNav *ppp = (sharedAcc.pppAtMs && now - sharedAcc.pppAtMs < 3000) ? &sharedAcc.ppp : nullptr;
            uint32_t lastRtcm = stats.lastRtcmToRxMs.load();
            bool rtcmActive = lastRtcm && now - lastRtcm < 10000;
            diagC1 = 34;
            Method m = classifyMethod(g, ppp, rtcmActive);
            float eh, ev;
            estimateFor(m, now, eh, ev);
            diagC1 = 35;
            mutex_enter_blocking(&ggaMutex);
            diagC1 = 36;
            memcpy(sharedGga.sentence, s, len);
            sharedGga.sentence[len] = 0;
            sharedGga.len = len;
            sharedGga.info = g;
            sharedGga.atMs = now;
            bool sample = tracker.update(now, g, m, eh, ev, refPoint, cfg.holdSbas);
            holding = tracker.holding();
            const MethodStat &ms = tracker.stat(methodGroup(m) >= 0 ? methodGroup(m) : 0);
            mutex_exit(&ggaMutex);
            diagC1 = 37;
            if (sample)
            {
                diagC1 = 38;
                HistRec r{};
                r.ymd = rmcYmd;
                r.utcMs = ggaTimeMs(g.utc);
                r.upSec = now / 1000;
                r.estH = eh;
                r.estV = ev;
                r.errH = methodGroup(m) >= 0 ? ms.errH : -1;
                r.errV = methodGroup(m) >= 0 ? ms.errV : -1;
                r.lat = g.lat;
                r.lon = g.lon;
                r.alt = g.alt;
                r.hdop = g.hdop;
                r.method = m;
                r.sats = g.sats > 255 ? 255 : g.sats;
                r.hold = holding;
                histToCore0.write((const uint8_t *)&r, sizeof(r)); // dropped if core 0 is stuck
                diagC1 = 39;
            }
        }
    }
    diagC1 = 40;
    if (send)
    {
        // Static-demo hold: the apps get the last SBAS position while HAS is still worse
        static char held[NMEA_MAX + 3];
        size_t hn = 0;
        if (holding)
        {
            MethodTracker *t = &tracker; // core 1 is the only writer, reading here is safe
            hn = nmeaReplacePosition(s, len, t->holdLat(), t->holdLon(), t->holdAlt(), held, sizeof(held));
        }
        if (hn)
            nmeaToNet.write((const uint8_t *)held, hn);
        else
            nmeaToNet.write((const uint8_t *)s, len); // dropped if nobody drains the ring
    }
}

void receiverLoop1()
{
    if (core1Halt.load())
        return;
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
        diagC1 = 20;
        for (int i = 0; i < n; i++)
            if (nmea.feed(buf[i]))
                handleSentence(nmea.sentence(), nmea.length());
        diagC1 = 21;
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
