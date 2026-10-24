// outputs.h - core 0: NMEA to Bluetooth SPP / BLE / TCP clients, inbound RTCM from them
#pragma once
#include <Arduino.h>

void outputsSetup();            // after configLoad(); starts Bluetooth
void outputsStop();             // before a restart
void outputsNetworkUp(bool up); // starts/stops the TCP server
void outputsLoop(bool ntripStreaming);

struct OutputStatus
{
    bool sppConnected;
    bool bleConnected;
    int tcpClients;
    uint32_t sentBytes;
    uint32_t inboundFrames;
};
void outputsStatus(OutputStatus &s);
String outputsBtName();
String outputsMonitor(); // the last ~3 KB of NMEA sent to Bluetooth/TCP, whole lines
