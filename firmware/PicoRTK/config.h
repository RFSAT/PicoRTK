// config.h - settings stored as /config.json in LittleFS
#pragma once
#include <Arduino.h>

static const int CFG_MAX_WIFI = 4;
static const int CFG_MAX_SOURCES = 4;
static const int CFG_MAX_BOOTCMDS = 8;

struct WifiNet
{
    String ssid, pass;
};

struct NtripSource
{
    String name;  // "Own base EOS"
    String type;  // base | vrs | near | other
    String host;
    uint16_t port = 2101;
    String mount, user, pass;
    bool gga = false; // send our position (VRS / nearest station)
    bool v2 = false;  // NTRIP 2.0 request
    bool tls = false; // TLS (certificate not verified)
    bool valid() const { return host.length() && port && mount.length(); }
};

struct Config
{
    String deviceName = "PicoRTK";
    String adminPass = "picortk"; // web page login (user "admin")
    String apPass = "picortk-setup"; // setup access point, 8+ characters

    WifiNet wifi[CFG_MAX_WIFI];
    NtripSource src[CFG_MAX_SOURCES];

    // Failover
    bool failover = true;
    uint32_t failbackSec = 600;  // return to source 1 (0 = never)
    uint32_t retryMs = 2000;     // delay before the next source
    uint32_t backoffSec[5] = {15, 30, 60, 120, 300};
    uint32_t dataTimeoutSec = 30; // no valid RTCM frame for this long = source failure
    uint32_t maxAgeSec = 10;      // GGA correction age above this while streaming = failure
    uint32_t ggaSec = 10;         // GGA upload interval

    // Outputs
    String bt = "spp";   // spp | ble | both | off
    bool usb = true;     // receiver stream on USB (also bridges USB input to the receiver)
    uint16_t tcpPort = 2947; // NMEA TCP server, 0 = off
    bool inboundRtcm = true; // accept RTCM from Bluetooth/TCP clients when NTRIP is not streaming

    // Receiver
    uint32_t uartBaud = 460800;
    String bootCmd[CFG_MAX_BOOTCMDS]; // sent to the LG290P after start-up ($...; checksum added if missing)
};

extern Config cfg;

bool configLoad();                       // from flash, defaults when missing
bool configSave();                       // to flash
String configToJson(bool withSecrets);   // secrets replaced by "********" unless requested
bool configFromJson(const String &json, String &err); // merges; "********" keeps the stored secret
