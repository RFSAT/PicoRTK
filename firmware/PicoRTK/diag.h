// diag.h - checkpoints and crash capture kept across a restart (uninitialised RAM), for fault finding
#pragma once
#include <Arduino.h>
#include <stdint.h>
extern volatile uint32_t diagC0, diagC1; // last checkpoint passed on core 0 / core 1
void diagBoot();                         // at start-up: copy the previous run's values, then reset them
String diagText();                       // previous run, for USB log and /api/status
