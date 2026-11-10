// receiver.h - core 1: LG290P UART, USB bridge, NMEA extraction
#pragma once
#include <atomic>

// Pico 2 W pins (see the wiring table in the design document)
static const int PIN_RX_TXD = 0; // GP0 = UART0 TX -> LG290P RXD
static const int PIN_RX_RXD = 1; // GP1 = UART0 RX <- LG290P TXD
static const int PIN_RX_RTK = 2; // GP2 <- LG290P RTK status

extern std::atomic<bool> configReady;

// Method ladder (ladder.h), run on every GGA. By default it only advises and reports (status page, event log);
// it pauses NTRIP only when cfg.ladderAutoPause is on.
#include "ladder.h"
#include <cstddef>
extern std::atomic<bool> ladderSourceAvailable; // set by core 0
extern std::atomic<bool> ladderAllowRtk;        // the ladder's advice, read by core 0
void ladderConfigure();                         // core 0, after the settings are loaded or changed
void getLadder(Ladder &out);
bool ladderTakeChange(char *reason, size_t n, LadderState &st);

void receiverInitShared(); // core 0, before configReady
void receiverSetup1();     // core 1
void receiverLoop1();      // core 1
bool receiverRtkPin();
