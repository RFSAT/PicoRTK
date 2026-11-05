// methods.h - which positioning method the receiver is using, per-method accuracy figures,
// error against a reference point, a 30-minute trace, and the optional static-demo SBAS hold.
// Portable (no Arduino calls), unit-tested on the PC.
//
// The LG290P computes ONE solution at a time. "Methods" here are the methods seen over time:
// the one running now, and the last figures of those that stopped. Nothing is computed in parallel.
#pragma once
#include "nmea.h"
#include <cstddef>
#include <cstdint>

enum Method : uint8_t
{
    M_NONE = 0,   // no fix / no data
    M_GNSS,       // standalone, no corrections
    M_SBAS,       // EGNOS
    M_DGNSS,      // code corrections from RTCM
    M_RTK_FLOAT,
    M_RTK_FIXED,
    M_HAS_CONV,   // Galileo HAS converging
    M_HAS_DONE,   // Galileo HAS converged
    M_DR,         // dead reckoning
    M_COUNT
};
const char *methodName(Method m);

// Groups shown as rows on the page
enum MethodGroup : uint8_t
{
    G_RTK = 0, // RTK fixed / float and DGNSS from RTCM
    G_HAS,
    G_SBAS,
    G_GNSS,
    G_COUNT
};
const char *groupName(MethodGroup g);
int methodGroup(Method m); // -1 for none / dead reckoning

// ppp may be nullptr when no fresh PQTMPPPNAV is available.
// rtcmActive: RTCM corrections were passed to the receiver in the last few seconds.
Method classifyMethod(const GgaInfo &g, const PppNav *ppp, bool rtcmActive);

struct RefPoint
{
    bool set = false;
    double lat = 0, lon = 0; // degrees
    float alt = 0;           // m above mean sea level (as in GGA)
};
// Horizontal and vertical distance of a position from the reference (m)
void refError(const RefPoint &r, double lat, double lon, float alt, float &errH, float &errV);

struct MethodStat
{
    bool seen = false;
    uint32_t startMs = 0, lastMs = 0; // start of the latest run, last epoch
    Method last = M_NONE;             // last method in this group
    float estH = -1, estV = -1;       // receiver's estimate at the last epoch (-1 = none)
    float errH = -1, errV = -1;       // error against the reference at the last epoch (-1 = no reference)
    float bestEstH = -1;              // best estimate during the latest run
};

struct TraceSample
{
    uint32_t tSec;   // uptime (s)
    uint8_t method;  // Method
    float estH, errH; // -1 = none
};
static const int TRACE_N = 180;     // 30 min at one sample every 10 s
static const uint32_t TRACE_STEP_MS = 10000;

class MethodTracker
{
  public:
    // One epoch (per GGA). estH/estV = receiver estimate (-1 none). holdWanted = static-demo hold enabled.
    // Returns true when this epoch was added to the trace (one every 10 s): also the moment to log it.
    bool update(uint32_t nowMs, const GgaInfo &g, Method m, float estH, float estV, const RefPoint &ref, bool holdWanted);

    Method current() const { return _cur; }
    uint32_t currentSinceMs() const { return _curSince; }
    const MethodStat &stat(int grp) const { return _st[grp]; }

    // Static-demo hold: while HAS converges and its estimate is worse than the last SBAS one,
    // the app output carries the last SBAS position instead.
    bool holding() const { return _holding; }
    bool holdReady() const { return _sbasPos.valid; } // an SBAS position exists to hold
    uint32_t holdFromMs() const { return _sbasPos.atMs; }
    float holdEstH() const { return _sbasPos.estH; }
    double holdLat() const { return _sbasPos.lat; }
    double holdLon() const { return _sbasPos.lon; }
    float holdAlt() const { return _sbasPos.alt; }
    const char *holdReason() const { return _holdReason; }

    int traceCount() const { return _traceN; }
    const TraceSample &trace(int i) const; // 0 = oldest

    static const uint32_t HOLD_MAX_MS = 30u * 60u * 1000u; // never hold a position older than this

  private:
    Method _cur = M_NONE;
    uint32_t _curSince = 0;
    MethodStat _st[G_COUNT];
    struct
    {
        bool valid = false;
        double lat = 0, lon = 0;
        float alt = 0, estH = -1;
        uint32_t atMs = 0;
    } _sbasPos;
    bool _holding = false, _released = false;
    const char *_holdReason = "";
    TraceSample _tr[TRACE_N];
    int _traceN = 0, _traceHead = 0;
    uint32_t _lastTraceMs = 0;
    bool _traceStarted = false;
    float _trEst = -1, _trErr = -1; // worst values since the last sample
    uint8_t _trMethod = M_NONE;
};

// One history record (kept in flash across restarts), written every 10 s
struct HistRec
{
    double lat, lon;   // first, so the record packs to 56 bytes
    uint32_t ymd;      // UTC date as yyyymmdd from RMC, 0 = not known yet; 0xFFFFFFFF = restart marker
    uint32_t utcMs;    // UTC time of day (ms) from GGA
    uint32_t upSec;    // uptime
    float estH, estV, errH, errV; // -1 = none
    float alt, hdop;
    uint8_t method, sats, hold, flags;
};
static const uint32_t HIST_RESTART = 0xFFFFFFFFu;
static const char HIST_CSV_HEADER[] =
    "date_utc,time_utc,uptime_s,method,est_h_m,est_v_m,err_h_m,err_v_m,lat_deg,lon_deg,alt_msl_m,sats,hdop,apps_get_held_sbas\r\n";
// One CSV line for a record. Returns its length.
size_t histCsvLine(const HistRec &r, char *out, size_t outSize);
// RMC date field ddmmyy -> yyyymmdd (0 when missing)
uint32_t rmcDate(const char *s, size_t len);
// GGA time hhmmss.ss -> ms of day (0 when missing)
uint32_t ggaTimeMs(const char *utc);

// Rewrite the position in a GGA, RMC, GLL or GNS sentence with a held position.
// GGA/GNS also get the altitude, quality 2 (SBAS) and empty correction age / station.
// Returns the new length, or 0 when the sentence is not one of these (send the original).
size_t nmeaReplacePosition(const char *s, size_t len, double lat, double lon, float alt, char *out, size_t outSize);
