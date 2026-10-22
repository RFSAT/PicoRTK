// net.h - core 0: Wi-Fi station / setup access point
#pragma once
#include <Arduino.h>

void netSetup();
void netLoop();
void netStartAp(); // button: open the setup access point now
bool netUp();      // station connected (internet assumed)
bool netApMode();
String netSsid();
String netIp();
int netRssi();
