// methods.cpp - see methods.h
#include "methods.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

const char *methodName(Method m)
{
    switch (m)
    {
    case M_GNSS: return "GNSS only";
    case M_SBAS: return "SBAS (EGNOS)";
    case M_DGNSS: return "DGNSS (RTCM)";
    case M_RTK_FLOAT: return "RTK float";
    case M_RTK_FIXED: return "RTK fixed";
    case M_HAS_CONV: return "HAS converging";
    case M_HAS_DONE: return "HAS converged";
    case M_DR: return "Dead reckoning";
    default: return "No fix";
    }
}

const char *groupName(MethodGroup g)
{
    switch (g)
    {
    case G_RTK: return "RTK (RTCM)";
    case G_HAS: return "Galileo HAS";
    case G_SBAS: return "SBAS (EGNOS)";
    default: return "GNSS only";
    }
}

int methodGroup(Method m)
{
    switch (m)
    {
    case M_RTK_FIXED:
    case M_RTK_FLOAT:
    case M_DGNSS: return G_RTK;
    case M_HAS_CONV:
    case M_HAS_DONE: return G_HAS;
    case M_SBAS: return G_SBAS;
    case M_GNSS: return G_GNSS;
    default: return -1;
    }
}

Method classifyMethod(const GgaInfo &g, const PppNav *ppp, bool rtcmActive)
{
    if (!g.valid)
        return M_NONE;
    const int q = g.quality, st = g.station;
    const bool has = st == 9002 || (ppp && ppp->diffId == 9002 && (ppp->sol == 6 || ppp->sol == 7));
    if (has && q != 0)
        return ((ppp && ppp->sol == 7) || (!ppp && q == 4)) ? M_HAS_DONE : M_HAS_CONV;
    switch (q)
    {
    case 4: return M_RTK_FIXED;
    case 5: return M_RTK_FLOAT;
    case 2:
    {
        // SBAS fixes report no station, or the EGNOS satellite number (PRN 120-158) as the station
        const bool sbasPrn = st >= 120 && st <= 158;
        if (st >= 0 && !(sbasPrn && !rtcmActive))
            return M_DGNSS;
        return M_SBAS;
    }
    case 1: return M_GNSS;
    case 6: return M_DR;
    default: return M_NONE;
    }
}

void refError(const RefPoint &r, double lat, double lon, float alt, float &errH, float &errV)
{
    if (!r.set)
    {
        errH = errV = -1;
        return;
    }
    const double R = 6378137.0, d2r = M_PI / 180.0;
    const double dN = (lat - r.lat) * d2r * R;
    const double dE = (lon - r.lon) * d2r * R * cos(r.lat * d2r);
    errH = (float)sqrt(dN * dN + dE * dE);
    errV = fabsf(alt - r.alt);
}

bool MethodTracker::update(uint32_t now, const GgaInfo &g, Method m, float estH, float estV, const RefPoint &ref, bool holdWanted)
{
    if (m != _cur)
    {
        int og = methodGroup(_cur), ng = methodGroup(m);
        if (ng >= 0 && (og != ng || !_st[ng].seen || now - _st[ng].lastMs > 5000))
        {
            _st[ng].startMs = now;
            _st[ng].bestEstH = -1;
        }
        _cur = m;
        _curSince = now;
    }
    float errH = -1, errV = -1;
    if (m != M_NONE)
        refError(ref, g.lat, g.lon, g.alt, errH, errV);
    const int grp = methodGroup(m);
    if (grp >= 0)
    {
        MethodStat &s = _st[grp];
        if (!s.seen)
            s.startMs = now;
        s.seen = true;
        s.lastMs = now;
        s.last = m;
        s.estH = estH;
        s.estV = estV;
        s.errH = errH;
        s.errV = errV;
        if (estH >= 0 && (s.bestEstH < 0 || estH < s.bestEstH))
            s.bestEstH = estH;
    }

    // Last SBAS position, for the static-demo hold
    if (m == M_SBAS)
    {
        _sbasPos.valid = true;
        _sbasPos.lat = g.lat;
        _sbasPos.lon = g.lon;
        _sbasPos.alt = g.alt;
        _sbasPos.estH = estH;
        _sbasPos.atMs = now;
    }

    // Hold state: only while HAS converges; released for good (until HAS stops) once HAS is as good
    bool hold = false;
    if (m != M_HAS_CONV)
        _released = false;
    if (!holdWanted)
        _holdReason = "off";
    else if (m != M_HAS_CONV)
        _holdReason = m == M_HAS_DONE ? "HAS converged" : "only while HAS converges";
    else if (_released)
        _holdReason = "HAS is now as good as SBAS";
    else if (!_sbasPos.valid)
        _holdReason = "no SBAS position yet";
    else if (now - _sbasPos.atMs > HOLD_MAX_MS)
        _holdReason = "SBAS position older than 30 min";
    else if (estH >= 0 && _sbasPos.estH >= 0 && estH <= _sbasPos.estH)
    {
        _released = true;
        _holdReason = "HAS is now as good as SBAS";
    }
    else
    {
        hold = true;
        _holdReason = "HAS still worse than the last SBAS fix";
    }
    _holding = hold;

    // Trace, one sample every 10 s
    if (!_traceStarted || now - _lastTraceMs >= TRACE_STEP_MS)
    {
        _traceStarted = true;
        _lastTraceMs = now;
        TraceSample &t = _tr[_traceHead];
        t.tSec = now / 1000;
        t.method = m;
        t.estH = estH;
        t.errH = errH;
        _traceHead = (_traceHead + 1) % TRACE_N;
        if (_traceN < TRACE_N)
            _traceN++;
        return true;
    }
    return false;
}

static void numOrEmpty(char *o, size_t n, float v, int dec)
{
    if (v < 0)
        o[0] = 0;
    else
        snprintf(o, n, "%.*f", dec, v);
}

size_t histCsvLine(const HistRec &r, char *out, size_t n)
{
    if (r.ymd == HIST_RESTART)
        return (size_t)snprintf(out, n, ",,%lu,restart,,,,,,,,,,\r\n", (unsigned long)r.upSec);
    char d[16] = "", t[16] = "", a[4][16];
    if (r.ymd)
        snprintf(d, sizeof(d), "%04lu-%02lu-%02lu", (unsigned long)(r.ymd / 10000), (unsigned long)(r.ymd / 100 % 100), (unsigned long)(r.ymd % 100));
    unsigned long ms = r.utcMs;
    snprintf(t, sizeof(t), "%02lu:%02lu:%02lu.%01lu", ms / 3600000, ms / 60000 % 60, ms / 1000 % 60, ms / 100 % 10);
    numOrEmpty(a[0], 16, r.estH, 3);
    numOrEmpty(a[1], 16, r.estV, 3);
    numOrEmpty(a[2], 16, r.errH, 3);
    numOrEmpty(a[3], 16, r.errV, 3);
    const bool pos = r.method != M_NONE;
    char la[20] = "", lo[20] = "", al[16] = "";
    if (pos)
    {
        snprintf(la, sizeof(la), "%.9f", r.lat);
        snprintf(lo, sizeof(lo), "%.9f", r.lon);
        snprintf(al, sizeof(al), "%.3f", r.alt);
    }
    return (size_t)snprintf(out, n, "%s,%s,%lu,%s,%s,%s,%s,%s,%s,%s,%s,%u,%.2f,%s\r\n", d, t, (unsigned long)r.upSec,
                            methodName((Method)r.method), a[0], a[1], a[2], a[3], la, lo, al, r.sats, r.hdop, r.hold ? "yes" : "no");
}

uint32_t rmcDate(const char *s, size_t len)
{
    // field 9 of $xxRMC
    int f = 0;
    size_t i = 0;
    for (; i < len && f < 9; i++)
        if (s[i] == ',')
            f++;
    if (f < 9 || i + 6 > len)
        return 0;
    for (int k = 0; k < 6; k++)
        if (s[i + k] < '0' || s[i + k] > '9')
            return 0;
    auto two = [&](int k) { return (uint32_t)((s[i + k] - '0') * 10 + (s[i + k + 1] - '0')); };
    return (2000 + two(4)) * 10000 + two(2) * 100 + two(0);
}

uint32_t ggaTimeMs(const char *u)
{
    if (strlen(u) < 6)
        return 0;
    auto two = [&](int k) { return (uint32_t)((u[k] - '0') * 10 + (u[k + 1] - '0')); };
    uint32_t ms = (two(0) * 3600 + two(2) * 60 + two(4)) * 1000;
    if (u[6] == '.')
        ms += (uint32_t)(atof(u + 6) * 1000 + 0.5);
    return ms;
}

const TraceSample &MethodTracker::trace(int i) const
{
    int start = (_traceHead - _traceN + TRACE_N) % TRACE_N;
    return _tr[(start + i) % TRACE_N];
}

// ---- sentence rewriting for the hold ----

static void fmtLat(double v, char *o, size_t n, char *hemi)
{
    *hemi = v < 0 ? 'S' : 'N';
    v = fabs(v);
    int d = (int)v;
    double m = (v - d) * 60.0;
    if (m >= 59.99999995) { d++; m = 0; }
    snprintf(o, n, "%02d%010.7f", d, m);
}
static void fmtLon(double v, char *o, size_t n, char *hemi)
{
    *hemi = v < 0 ? 'W' : 'E';
    v = fabs(v);
    int d = (int)v;
    double m = (v - d) * 60.0;
    if (m >= 59.99999995) { d++; m = 0; }
    snprintf(o, n, "%03d%010.7f", d, m);
}

size_t nmeaReplacePosition(const char *s, size_t len, double lat, double lon, float alt, char *out, size_t outSize)
{
    char id[NMEA_ID_MAX];
    if (!nmeaSentenceId(s, len, id, sizeof(id)))
        return 0;
    int iLat;
    bool gga = false, gns = false;
    if (!strcmp(id, "GGA")) { iLat = 2; gga = true; }
    else if (!strcmp(id, "GNS")) { iLat = 2; gns = true; }
    else if (!strcmp(id, "RMC")) iLat = 3;
    else if (!strcmp(id, "GLL")) iLat = 1;
    else return 0;

    // Split the body ($ ... before *) into fields
    size_t end = 0;
    while (end < len && s[end] != '*')
        end++;
    if (end >= len || s[0] != '$')
        return 0;
    static const int MAXF = 24;
    const char *fs[MAXF];
    size_t fl[MAXF];
    int nf = 0;
    size_t st = 1;
    for (size_t i = 1; i <= end && nf < MAXF; i++)
        if (i == end || s[i] == ',')
        {
            fs[nf] = s + st;
            fl[nf] = i - st;
            nf++;
            st = i + 1;
        }
    if (nf <= iLat + 3)
        return 0;

    char la[20], lo[20], hl, ho, al[16];
    fmtLat(lat, la, sizeof(la), &hl);
    fmtLon(lon, lo, sizeof(lo), &ho);
    snprintf(al, sizeof(al), "%.3f", alt);
    char h1[2] = {hl, 0}, h2[2] = {ho, 0};

    size_t o = 0;
    auto put = [&](const char *p, size_t n) {
        if (o + n < outSize) { memcpy(out + o, p, n); o += n; }
        else o = outSize; // overflow marker
    };
    put("$", 1);
    for (int i = 0; i < nf; i++)
    {
        if (i)
            put(",", 1);
        const char *rep = nullptr;
        if (i == iLat) rep = la;
        else if (i == iLat + 1) rep = h1;
        else if (i == iLat + 2) rep = lo;
        else if (i == iLat + 3) rep = h2;
        else if (gga && i == 6) rep = "2";
        else if ((gga || gns) && i == 9) rep = al;
        else if ((gga && (i == 13 || i == 14)) || (gns && (i == 11 || i == 12))) rep = "";
        if (rep)
            put(rep, strlen(rep));
        else
            put(fs[i], fl[i]);
    }
    if (o >= outSize || o + 6 > outSize)
        return 0;
    uint8_t c = nmeaChecksum(out + 1, o - 1);
    o += snprintf(out + o, outSize - o, "*%02X\r\n", c);
    return o;
}
