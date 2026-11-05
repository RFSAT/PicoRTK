// diag.cpp - see diag.h
#include "diag.h"
#include <hardware/watchdog.h>
#include <pico/platform.h>
#include <hardware/sync.h>

static uint32_t __uninitialized_ram(dMagic);
volatile uint32_t __uninitialized_ram(diagC0);
volatile uint32_t __uninitialized_ram(diagC1);
static uint32_t __uninitialized_ram(dPc);
static uint32_t __uninitialized_ram(dLr);
static uint32_t __uninitialized_ram(dCore);
static uint32_t __uninitialized_ram(dFault);
static String prev;

extern "C" void diagFault(uint32_t *frame)
{
    dMagic = 0xD1A6D1A6;
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


void diagBoot()
{
    if (dMagic == 0xD1A6D1A6)
    {
        char b[160];
        if (dFault)
            snprintf(b, sizeof(b), "previous run: FAULT on core %lu at pc=0x%08lx lr=0x%08lx, checkpoints c0=%lu c1=%lu",
                     (unsigned long)dCore, (unsigned long)dPc, (unsigned long)dLr, (unsigned long)diagC0, (unsigned long)diagC1);
        else
            snprintf(b, sizeof(b), "previous run: no fault recorded, last checkpoints c0=%lu c1=%lu", (unsigned long)diagC0, (unsigned long)diagC1);
        prev = b;
    }
    else
        prev = "previous run: nothing recorded (power-up)";
    dFault = 0;
    diagC0 = diagC1 = 0;
    dMagic = 0xD1A6D1A6;
}

String diagText() { return prev; }
