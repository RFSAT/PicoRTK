// net.cpp - core 0: Wi-Fi station with an ordered network list, setup access point as fallback
#include "net.h"
#include "config.h"
#include "util.h"
#include <WiFi.h>
#include <WiFiMulti.h>
#include <SimpleMDNS.h>

static WiFiMulti multi;
static bool apMode = false;
static bool haveNetworks = false;
static uint32_t lastTry = 0, apSince = 0;
static const uint32_t STA_RETRY_MS = 20000;
static const uint32_t AP_RETRY_MS = 180000; // leave the setup AP to retry Wi-Fi when nobody uses it

static String apName()
{
    uint8_t mac[6] = {0};
    WiFi.macAddress(mac);
    char s[40];
    snprintf(s, sizeof(s), "%s-Setup-%02X%02X", cfg.deviceName.c_str(), mac[4], mac[5]);
    return s;
}

void netStartAp()
{
    WiFi.disconnect();
    WiFi.mode(WIFI_AP);
    WiFi.softAP(apName().c_str(), cfg.apPass.c_str());
    apMode = true;
    apSince = millis();
    dbg("[net] setup access point %s, http://%s\r\n", apName().c_str(), WiFi.softAPIP().toString().c_str());
}

// http://<devicename>.local on the office network (Windows 10+, Android 12+, iOS, macOS)
static void startMdns()
{
    static bool started = false;
    if (started)
        return;
    String host = cfg.deviceName;
    host.toLowerCase();
    host.replace(" ", "-");
    if (MDNS.begin(host.c_str()))
    {
        MDNS.addService("http", "tcp", 80);
        started = true;
        dbg("[net] http://%s.local\r\n", host.c_str());
    }
}

static bool tryStation(uint32_t timeoutMs)
{
    if (!haveNetworks)
        return false;
    if (apMode)
    {
        WiFi.softAPdisconnect(false);
        apMode = false;
    }
    WiFi.mode(WIFI_STA);
    bool ok = multi.run(timeoutMs) == WL_CONNECTED;
    lastTry = millis();
    if (ok)
    {
        dbg("[net] connected to %s, %s\r\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
        startMdns();
        static bool ntp = false;
        if (!ntp)
        {
            NTP.begin("pool.ntp.org", "time.google.com"); // UTC time for the bench GGA
            ntp = true;
        }
    }
    return ok;
}

void netSetup()
{
    WiFi.setHostname(cfg.deviceName.c_str());
    for (int i = 0; i < CFG_MAX_WIFI; i++)
        if (cfg.wifi[i].ssid.length())
        {
            multi.addAP(cfg.wifi[i].ssid.c_str(), cfg.wifi[i].pass.length() ? cfg.wifi[i].pass.c_str() : nullptr);
            haveNetworks = true;
        }
    dbg("[net] %s\r\n", haveNetworks ? "joining the configured Wi-Fi" : "no Wi-Fi configured");
    if (!tryStation(15000))
        netStartAp();
}

void netLoop()
{
    uint32_t now = millis();
    if (apMode)
    {
        if (haveNetworks && WiFi.softAPgetStationNum() == 0 && now - apSince > AP_RETRY_MS)
        {
            if (!tryStation(10000))
                netStartAp();
        }
        return;
    }
    if (WiFi.status() != WL_CONNECTED && now - lastTry > STA_RETRY_MS)
        tryStation(8000);
}

bool netUp() { return !apMode && WiFi.status() == WL_CONNECTED; }
bool netApMode() { return apMode; }
String netSsid() { return apMode ? apName() : WiFi.SSID(); }
String netIp() { return apMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString(); }
int netRssi() { return netUp() ? WiFi.RSSI() : 0; }
