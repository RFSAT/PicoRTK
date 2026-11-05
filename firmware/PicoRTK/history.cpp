// history.cpp - see history.h. Records arrive from core 1 through histToCore0, are buffered in RAM
// and appended to flash every 5 minutes (and at shutdown / restart / download). When the current
// file reaches HIST_FILE_MAX it becomes the old one and a new file starts, so the oldest data goes first.
#include "history.h"
#include "shared.h"
#include "diag.h"
#include <LittleFS.h>

static const char *F_NEW = "/hist0.bin", *F_OLD = "/hist1.bin";
static const int BUF_N = 40;
static HistRec buf[BUF_N];
static int bufN = 0;
static uint32_t lastFlushMs = 0;
static const uint32_t FLUSH_MS = 5u * 60u * 1000u;

static uint32_t fileRecs(const char *p)
{
    if (!LittleFS.exists(p))
        return 0;
    File f = LittleFS.open(p, "r");
    uint32_t n = f ? f.size() / sizeof(HistRec) : 0;
    f.close();
    return n;
}

void historyFlush()
{
    if (!bufN)
        return;
    if (fileRecs(F_NEW) * sizeof(HistRec) >= HIST_FILE_MAX)
    {
        LittleFS.remove(F_OLD);
        LittleFS.rename(F_NEW, F_OLD);
    }
    File f = LittleFS.open(F_NEW, "a");
    if (f)
    {
        f.write((const uint8_t *)buf, bufN * sizeof(HistRec));
        f.close();
    }
    bufN = 0;
    lastFlushMs = millis();
}

static void add(const HistRec &r)
{
    if (bufN >= BUF_N)
        historyFlush();
    buf[bufN++] = r;
}

void historySetup()
{
    HistRec r{};
    r.ymd = HIST_RESTART;
    r.upSec = millis() / 1000;
    add(r);
    lastFlushMs = millis();
}

void historyLoop()
{
    HistRec r;
    while (histToCore0.available() >= sizeof(HistRec))
    {
        diagC0 = 81;
        histToCore0.read((uint8_t *)&r, sizeof(r));
        diagC0 = 82;
        add(r);
        diagC0 = 83;
    }
    if (bufN && millis() - lastFlushMs >= FLUSH_MS)
    {
        diagC0 = 84;
        historyFlush();
    }
}

bool historyClear()
{
    bufN = 0;
    bool ok = true;
    if (LittleFS.exists(F_OLD))
        ok &= LittleFS.remove(F_OLD);
    if (LittleFS.exists(F_NEW))
        ok &= LittleFS.remove(F_NEW);
    return ok;
}

uint32_t historyRecords() { return fileRecs(F_OLD) + fileRecs(F_NEW) + bufN; }
uint32_t historyBytes() { return (fileRecs(F_OLD) + fileRecs(F_NEW)) * sizeof(HistRec); }

void historyForEach(bool (*fn)(const HistRec &, void *), void *ctx)
{
    for (const char *p : {F_OLD, F_NEW})
    {
        if (!LittleFS.exists(p))
            continue;
        File f = LittleFS.open(p, "r");
        HistRec r;
        while (f && f.read((uint8_t *)&r, sizeof(r)) == sizeof(r))
            if (!fn(r, ctx))
            {
                f.close();
                return;
            }
        f.close();
    }
    for (int i = 0; i < bufN; i++)
        if (!fn(buf[i], ctx))
            return;
}

static bool dated(const HistRec &r) { return r.ymd && r.ymd != HIST_RESTART; }
struct FirstCtx
{
    uint32_t y = 0, m = 0;
};
static bool firstCb(const HistRec &r, void *c)
{
    if (!dated(r))
        return true;
    ((FirstCtx *)c)->y = r.ymd;
    ((FirstCtx *)c)->m = r.utcMs;
    return false; // stop at the first dated record
}
void historyInfo(uint32_t &fy, uint32_t &fm, uint32_t &ly, uint32_t &lm)
{
    FirstCtx x;
    historyForEach(firstCb, &x);
    fy = x.y;
    fm = x.m;
    ly = lm = 0;
    for (int i = bufN - 1; i >= 0; i--) // newest: the RAM buffer first, then the end of the newest file
        if (dated(buf[i]))
        {
            ly = buf[i].ymd;
            lm = buf[i].utcMs;
            return;
        }
    for (const char *p : {F_NEW, F_OLD})
    {
        uint32_t n = fileRecs(p);
        if (!n)
            continue;
        File f = LittleFS.open(p, "r");
        HistRec r;
        for (uint32_t k = n; k > 0 && n - k < 64; k--) // look back at most 64 records (restarts without a fix)
        {
            f.seek((k - 1) * sizeof(HistRec));
            if (f.read((uint8_t *)&r, sizeof(r)) == sizeof(r) && dated(r))
            {
                ly = r.ymd;
                lm = r.utcMs;
                f.close();
                return;
            }
        }
        f.close();
    }
}
