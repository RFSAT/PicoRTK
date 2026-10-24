// shared.h - data exchanged between core 0 (network, radio) and core 1 (receiver UART, USB)
#pragma once
#include "nmea.h"
#include "ring.h"
#include <atomic>
#include <pico/mutex.h>

// core 0 -> core 1: validated RTCM frames for the receiver
extern SpscRing<16384> rtcmToRx;
// core 0 -> core 1: receiver commands (boot commands, web console)
extern SpscRing<1024> cmdToRx;
// core 1 -> core 0: valid NMEA sentences for Bluetooth and TCP clients
extern SpscRing<16384> nmeaToNet;

// Latest GGA from the receiver (core 1 writes, core 0 reads)
struct SharedGga
{
    char sentence[NMEA_MAX + 3];
    size_t len;
    GgaInfo info;
    uint32_t atMs;
};
extern SharedGga sharedGga;
extern mutex_t ggaMutex;

// Counters, written by one core only
struct Stats
{
    std::atomic<uint32_t> uartRxBytes{0};
    std::atomic<uint32_t> uartTxBytes{0};
    std::atomic<uint32_t> nmeaGood{0};
    std::atomic<uint32_t> nmeaBad{0};
    std::atomic<uint32_t> rtcmFramesToRx{0};
    std::atomic<uint32_t> lastRtcmToRxMs{0};
};
extern Stats stats;

// NMEA sentence types seen from the receiver (core 1 writes, core 0 reads under typesMutex)
static const int MAX_NMEA_TYPES = 24;
struct NmeaTypeStat
{
    char id[NMEA_ID_MAX];
    uint32_t count;
    bool sent; // passes the output filter
};
extern NmeaTypeStat nmeaTypes[MAX_NMEA_TYPES];
extern int nmeaTypeCount;
extern mutex_t typesMutex;
// Replies of the receiver to commands ($PQTM...,OK / ERROR, version, id), for the web page
static const int RX_REPLIES = 24;
extern char rxReplies[RX_REPLIES][NMEA_MAX + 1];
extern uint32_t rxReplySeq; // total replies since start-up; newest is at (rxReplySeq-1) % RX_REPLIES
extern std::atomic<bool> core1Halt; // safe shutdown: core 1 stops touching the UART

// Copy the latest GGA (thread-safe). Returns false when none has arrived yet.
bool getLatestGga(SharedGga &out);
