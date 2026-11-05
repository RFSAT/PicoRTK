// diag.cpp - see diag.h
#include "diag.h"
#include <hardware/sync.h>
#include <hardware/watchdog.h>
#include <pico/platform.h>

static uint32_t __uninitialized_ram(dMagic);
static uint32_t __uninitialized_ram(dPc);
static uint32_t __uninitialized_ram(dLr);
static uint32_t __uninitialized_ram(dCore);
static uint32_t __uninitialized_ram(dFault);
volatile uint32_t __uninitialized_ram(diagC0);
volatile uint32_t __uninitialized_ram(diagC1);
static const uint32_t MAGIC = 0xD1A6D1A6;
static String prev;
static bool problem = false;

// Hard fault on either core: keep the code address, then restart at once
extern "C" void diagFault(uint32_t *frame)
{
    dMagic = MAGIC;
    dPc = frame[6];
    dLr = frame[5];
    dCore = get_core_num();
    dFault = 1;
    watchdog_reboot(0, 0, 10);
    while (true)
        tight_loop_contents();
}

extern "C" __attribute__((naked)) void isr_hardfault(void)
{
    __asm volatile("tst lr, #4      \n"
                   "ite eq          \n"
                   "mrseq r0, msp   \n"
                   "mrsne r0, psp   \n"
                   "b diagFault     \n");
}

const char *diagStepName(int core, uint32_t c)
{
    if (core == 0)
        switch (c)
        {
        case 0: return "idle";
        case 1: return "Wi-Fi";
        case 2: return "network state";
        case 3: return "web server";
        case 4: return "NTRIP client";
        case 5: return "Bluetooth/TCP outputs";
        case 6: return "status";
        case 8: return "history";
        case 9: return "end of main loop";
        case 81: return "history: reading records";
        case 82: return "history: buffering";
        case 83: return "history: next record";
        case 84: return "history: writing flash";
        case 101: return "start-up: shared data";
        case 102: return "start-up: flash file system";
        case 103: return "start-up: loading settings";
        case 104: return "start-up: reference point";
        case 105: return "start-up: history";
        case 106: return "start-up: settings loaded";
        case 107: return "start-up: status";
        case 108: return "start-up: Bluetooth";
        case 109: return "start-up: Wi-Fi";
        case 110: return "start-up: web server";
        case 111: return "start-up: NTRIP";
        case 112: return "start-up done";
        case DIAG_CLEAN: return "restart requested";
        default: return "?";
        }
    switch (c)
    {
    case 0: return "idle";
    case 1: return "UART loop";
    case 20: return "NMEA framing";
    case 21: return "UART loop";
    case 30: return "NMEA sentence";
    case 31: return "NMEA filter";
    case 32: return "NMEA counters";
    case 33: return "accuracy messages";
    case 34: return "method";
    case 35: return "waiting for shared data";
    case 36: return "method tracker";
    case 37: return "after method tracker";
    case 38: return "history record";
    case 39: return "history record queued";
    case 40: return "NMEA output";
    default: return "?";
    }
}

void diagBoot(bool watchdogReset)
{
    char b[220];
    const uint32_t c0 = diagC0, c1 = diagC1;
    if (dMagic == MAGIC && dFault)
    {
        snprintf(b, sizeof(b), "crashed (fault on core %lu at pc=0x%08lx lr=0x%08lx); core 0 at %lu %s, core 1 at %lu %s",
                 (unsigned long)dCore, (unsigned long)dPc, (unsigned long)dLr, (unsigned long)c0, diagStepName(0, c0),
                 (unsigned long)c1, diagStepName(1, c1));
        problem = true;
    }
    else if (dMagic == MAGIC && c0 == DIAG_CLEAN) // checked first: Restart itself uses the watchdog
        snprintf(b, sizeof(b), "ended normally (restart or shut down from the page)");
    else if (dMagic == MAGIC && watchdogReset)
    {
        snprintf(b, sizeof(b), "hung, restarted by the watchdog; core 0 at %lu %s, core 1 at %lu %s", (unsigned long)c0,
                 diagStepName(0, c0), (unsigned long)c1, diagStepName(1, c1));
        problem = true;
    }
    else if (dMagic == MAGIC)
        snprintf(b, sizeof(b), "ended without a fault (power removed, reset button or new firmware)");
    else
        snprintf(b, sizeof(b), "nothing recorded (power-up)");
    prev = b;
    dFault = 0;
    diagC0 = diagC1 = 0;
    dMagic = MAGIC;
}

String diagText() { return prev; }
bool diagProblem() { return problem; }
