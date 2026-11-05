// diag.h - restart recorder. Each core writes a checkpoint number as it works; the numbers and any
// hard fault (code address) are kept across a restart in RAM that start-up does not clear, so after a
// watchdog restart or a crash the next run can say where the previous one stopped.
// Costs one RAM write per step; nothing is printed while the device runs normally.
#pragma once
#include <Arduino.h>
#include <stdint.h>

// Checkpoints. Core 0: 1xx = start-up, 1-9 = main loop, 8x = history. Core 1: 2x = UART loop, 3x-4x = NMEA.
extern volatile uint32_t diagC0, diagC1;
static const uint32_t DIAG_CLEAN = 200; // set by Restart / Shut down from the page

void diagBoot(bool watchdogReset); // at start-up, before anything else writes a checkpoint
String diagText();                 // the previous run, one line (System tab, /api/status, USB at start-up)
bool diagProblem();                // the previous run ended in a fault or a watchdog restart
const char *diagStepName(int core, uint32_t c);
