// outputs.cpp - core 0: NMEA to Bluetooth SPP / BLE / TCP clients, inbound RTCM from them
#include "outputs.h"
#include "config.h"
#include "rtcm.h"
#include "shared.h"
#include <BLE.h>
#include <SerialBT.h>
#include <WiFi.h>

static const int MAX_TCP = 3;
static bool useSpp = false, useBle = false;
static BLEServiceUART bleUart(256, 2048);
static WiFiServer *tcpServer = nullptr;
static WiFiClient tcpClients[MAX_TCP];
static RtcmFramer inFramer;
static uint32_t sentBytes = 0, inboundFrames = 0;
static String btName;

String outputsBtName() { return btName; }

void outputsSetup()
{
    useSpp = (cfg.bt == "spp" || cfg.bt == "both");
    useBle = (cfg.bt == "ble" || cfg.bt == "both");

    // Bluetooth name: device name + last 4 hex digits of the radio MAC, e.g. "PicoRTK-3F2A"
    uint8_t mac[6] = {0};
    WiFi.macAddress(mac);
    char suffix[8];
    snprintf(suffix, sizeof(suffix), "-%02X%02X", mac[4], mac[5]);
    btName = cfg.deviceName + suffix;

    // BLE first: SerialBT then adds the classic SPP service to the running stack.
    // "both" is experimental (two Arduino-Pico Bluetooth libraries share one BTstack).
    if (useBle)
    {
        BLE.begin(btName);
        BLE.server()->addService(&bleUart);
        BLE.startAdvertising();
        bleUart.setAutoflush(20);
    }
    if (useSpp)
    {
        SerialBT.setFIFOSize(4096);
        SerialBT.setName(btName.c_str());
        SerialBT.begin();
    }
}

void outputsStop()
{
    for (auto &c : tcpClients)
        c.stop();
    if (useSpp)
        SerialBT.end();
}

void outputsNetworkUp(bool up)
{
    if (up && cfg.tcpPort && !tcpServer)
    {
        tcpServer = new WiFiServer(cfg.tcpPort);
        tcpServer->begin();
        tcpServer->setNoDelay(true);
    }
    else if (!up && tcpServer)
    {
        for (auto &c : tcpClients)
            c.stop();
        tcpServer->stop();
        delete tcpServer;
        tcpServer = nullptr;
    }
}

static void inbound(const uint8_t *buf, size_t n, bool accept)
{
    if (!accept)
        return;
    for (size_t i = 0; i < n; i++)
        if (inFramer.feed(buf[i]))
            if (rtcmToRx.write(inFramer.frame(), inFramer.frameLength()))
                inboundFrames++;
}

void outputsLoop(bool ntripStreaming)
{
    // New TCP clients
    if (tcpServer)
    {
        WiFiClient nc = tcpServer->accept();
        if (nc)
        {
            bool placed = false;
            for (auto &c : tcpClients)
                if (!c.connected())
                {
                    c.stop();
                    c = nc;
                    c.setNoDelay(true);
                    placed = true;
                    break;
                }
            if (!placed)
                nc.stop();
        }
    }

    // NMEA fan-out
    uint8_t buf[512];
    size_t n;
    while ((n = nmeaToNet.read(buf, sizeof(buf))) > 0)
    {
        if (useSpp && SerialBT)
            SerialBT.write(buf, n);
        if (useBle && bleUart)
            bleUart.write(buf, n);
        for (auto &c : tcpClients)
            if (c.connected())
                c.write(buf, n);
        sentBytes += n;
    }

    // Inbound bytes: RTCM from apps that run their own NTRIP client
    bool accept = cfg.inboundRtcm && !ntripStreaming;
    if (useSpp)
    {
        int a = SerialBT.available();
        if (a > 0)
        {
            a = SerialBT.readBytes(buf, a > (int)sizeof(buf) ? sizeof(buf) : a);
            inbound(buf, a, accept);
        }
    }
    if (useBle)
    {
        int k = 0;
        while (bleUart.available() && k < (int)sizeof(buf))
            buf[k++] = (uint8_t)bleUart.read();
        inbound(buf, k, accept);
    }
    for (auto &c : tcpClients)
    {
        int a = c.connected() ? c.available() : 0;
        if (a > 0)
        {
            a = c.read(buf, a > (int)sizeof(buf) ? sizeof(buf) : a);
            if (a > 0)
                inbound(buf, a, accept);
        }
    }
}

void outputsStatus(OutputStatus &s)
{
    s.sppConnected = useSpp && (bool)SerialBT;
    s.bleConnected = useBle && (bool)bleUart;
    s.tcpClients = 0;
    for (auto &c : tcpClients)
        s.tcpClients += c.connected() ? 1 : 0;
    s.sentBytes = sentBytes;
    s.inboundFrames = inboundFrames;
}
