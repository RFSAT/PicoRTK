// web.cpp - core 0: configuration page and JSON API (HTTP Basic login "admin" / adminPass)
#include "web.h"
#include "config.h"
#include "net.h"
#include "ntrip.h"
#include "outputs.h"
#include "shared.h"
#include "util.h"
#include "web_page.h"
#include <ArduinoJson.h>
#include <WebServer.h>
#include <WiFi.h>

#ifndef PICORTK_VERSION
#define PICORTK_VERSION "0.1.0"
#endif

static WebServer server(80);
static uint32_t rebootAt = 0, shutdownAt = 0;

static bool auth()
{
    if (server.authenticate("admin", cfg.adminPass.c_str()))
        return true;
    server.requestAuthentication(BASIC_AUTH, "PicoRTK");
    return false;
}

static const char *qualityName(int q)
{
    switch (q)
    {
    case 0: return "no fix";
    case 1: return "GNSS";
    case 2: return "DGNSS";
    case 4: return "RTK fixed";
    case 5: return "RTK float";
    case 6: return "dead reckoning";
    default: return "other";
    }
}

static void handleStatus()
{
    if (!auth())
        return;
    uint32_t now = millis();
    JsonDocument d;
    d["version"] = PICORTK_VERSION;
    d["uptimeSec"] = now / 1000;
    extern const char *bootReason;
    d["lastReset"] = bootReason;
    d["device"] = cfg.deviceName;
    d["privateDefaults"] = configHasPrivateDefaults();
    d["mode"] = cfg.mode;
    d["galileoHas"] = cfg.galileoHas;
    d["bluetoothName"] = outputsBtName();

    JsonObject w = d["wifi"].to<JsonObject>();
    w["mode"] = netApMode() ? "setup access point" : (netUp() ? "connected" : "searching");
    w["ssid"] = netSsid();
    w["ip"] = netIp();
    w["rssi"] = netRssi();

    SharedGga g;
    JsonObject n = d["gnss"].to<JsonObject>();
    if (getLatestGga(g))
    {
        n["quality"] = g.info.quality;
        n["qualityName"] = qualityName(g.info.quality);
        n["sats"] = g.info.sats;
        n["hdop"] = g.info.hdop;
        n["lat"] = serialized(String(g.info.lat, 9));
        n["lon"] = serialized(String(g.info.lon, 9));
        n["alt"] = g.info.alt;
        n["age"] = g.info.age;
        n["station"] = g.info.station;
        n["utc"] = g.info.utc;
        n["ggaAgeMs"] = now - g.atMs;
    }
    else
        n["qualityName"] = "no data from receiver";

    // Accuracy estimates (only those that arrived in the last 5 s)
    SharedAccuracy a;
    getAccuracy(a);
    if (a.gstAtMs && now - a.gstAtMs < 5000)
    {
        n["gstH"] = sqrtf(a.gstLat * a.gstLat + a.gstLon * a.gstLon);
        n["gstV"] = a.gstAlt;
    }
    if (a.epeAtMs && now - a.epeAtMs < 5000)
    {
        n["epeH"] = a.epe2d;
        n["epeV"] = fabsf(a.epeDown);
    }
    if (a.pppAtMs && now - a.pppAtMs < 5000)
    {
        JsonObject p = d["ppp"].to<JsonObject>();
        p["sol"] = a.ppp.sol;
        if (a.ppp.latStd >= 0 && a.ppp.lonStd >= 0)
            p["h"] = sqrtf(a.ppp.latStd * a.ppp.latStd + a.ppp.lonStd * a.ppp.lonStd);
        if (a.ppp.altStd >= 0)
            p["v"] = a.ppp.altStd;
        p["diffId"] = a.ppp.diffId;
        p["age"] = a.ppp.diffAge;
        p["sats"] = a.ppp.satUsed;
    }

    // Methods over time (the receiver computes one at a time), reference point, static-demo hold
    {
        static MethodTracker mt; // large: keep it off the stack
        RefPoint rp;
        getMethods(mt, rp);
        JsonObject m = d["methods"].to<JsonObject>();
        m["current"] = methodName(mt.current());
        m["currentCode"] = (int)mt.current();
        m["currentForSec"] = (now - mt.currentSinceMs()) / 1000;
        int curGrp = methodGroup(mt.current());
        JsonArray ga = m["groups"].to<JsonArray>();
        for (int i = 0; i < G_COUNT; i++)
        {
            const MethodStat &st = mt.stat(i);
            JsonObject o = ga.add<JsonObject>();
            o["name"] = groupName((MethodGroup)i);
            o["seen"] = st.seen;
            if (!st.seen)
                continue;
            o["running"] = i == curGrp && now - st.lastMs < 5000;
            o["last"] = methodName(st.last);
            o["runSec"] = (st.lastMs - st.startMs) / 1000;
            o["endedAgoSec"] = (now - st.lastMs) / 1000;
            if (st.estH >= 0) o["estH"] = st.estH;
            if (st.estV >= 0) o["estV"] = st.estV;
            if (st.errH >= 0) o["errH"] = st.errH;
            if (st.errV >= 0) o["errV"] = st.errV;
            if (st.bestEstH >= 0) o["bestEstH"] = st.bestEstH;
        }
        JsonObject r = m["ref"].to<JsonObject>();
        r["set"] = rp.set;
        if (rp.set)
        {
            r["lat"] = serialized(String(rp.lat, 9));
            r["lon"] = serialized(String(rp.lon, 9));
            r["alt"] = rp.alt;
        }
        JsonObject h = m["hold"].to<JsonObject>();
        h["enabled"] = cfg.holdSbas;
        h["active"] = mt.holding();
        h["reason"] = mt.holdReason();
        if (mt.holdReady())
        {
            h["sbasAgoSec"] = (now - mt.holdFromMs()) / 1000;
            if (mt.holdEstH() >= 0) h["sbasEstH"] = mt.holdEstH();
        }
    }

    NtripStatus s;
    ntripStatus(s);
    JsonObject t = d["ntrip"].to<JsonObject>();
    t["state"] = ntripStateName(s.state);
    t["source"] = s.source + 1;
    t["sourceName"] = cfg.src[s.source].name;
    t["mount"] = cfg.src[s.source].mount;
    t["host"] = cfg.src[s.source].host;
    t["bytes"] = s.bytes;
    t["streamBytes"] = s.streamBytes;
    t["ggaSent"] = s.ggaSent;
    t["lastGga"] = s.lastGga;
    t["frames"] = s.frames;
    t["crcErrors"] = s.crcErrors;
    t["switches"] = s.switches;
    t["streamSec"] = s.streamSec;
    if (s.lastFrameAgoMs != 0xFFFFFFFF)
        t["lastFrameAgoMs"] = s.lastFrameAgoMs;
    t["stationId"] = s.stationId;
    t["backoffLevel"] = s.backoffLevel;
    t["lastEvent"] = s.lastEvent;

    OutputStatus o;
    outputsStatus(o);
    JsonObject out = d["outputs"].to<JsonObject>();
    out["bt"] = cfg.bt;
    out["spp"] = o.sppConnected;
    out["ble"] = o.bleConnected;
    out["tcpClients"] = o.tcpClients;
    out["tcpPort"] = cfg.tcpPort;
    out["sentBytes"] = o.sentBytes;
    out["inboundFrames"] = o.inboundFrames;

    JsonObject u = d["uart"].to<JsonObject>();
    u["baud"] = cfg.uartBaud;
    u["rxBytes"] = stats.uartRxBytes.load();
    u["txBytes"] = stats.uartTxBytes.load();
    u["nmeaGood"] = stats.nmeaGood.load();
    u["nmeaBad"] = stats.nmeaBad.load();
    u["rtcmFrames"] = stats.rtcmFramesToRx.load();
    u["drops"] = rtcmToRx.dropped() + nmeaToNet.dropped();

    String js;
    serializeJson(d, js);
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", js);
}

static void handleGetConfig()
{
    if (!auth())
        return;
    bool secrets = server.hasArg("secrets") && server.arg("secrets") == "1";
    if (secrets)
        server.sendHeader("Content-Disposition", "attachment; filename=\"picortk-config.json\"");
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", configToJson(secrets));
}

static void handlePostConfig()
{
    if (!auth())
        return;
    String err;
    if (!configFromJson(server.arg("plain"), err))
    {
        server.send(400, "text/plain", err);
        return;
    }
    if (!configSave())
    {
        server.send(500, "text/plain", "Could not write the settings to flash");
        return;
    }
    server.send(200, "text/plain", "Saved. Restarting in 2 s.");
    rebootAt = millis() + 2000;
}

static void handleCmd()
{
    if (!auth())
        return;
    String c = server.arg("plain");
    c.trim();
    if (!c.startsWith("$") || c.length() > 120)
    {
        server.send(400, "text/plain", "A receiver command starts with $ and is at most 120 characters");
        return;
    }
    char out[130];
    size_t n = nmeaFinish(c.c_str(), out, sizeof(out));
    if (!n || !cmdToRx.write((const uint8_t *)out, n))
    {
        server.send(503, "text/plain", "Command queue full, try again");
        return;
    }
    server.send(200, "text/plain", String("Sent ") + out);
}

static void handleLog()
{
    if (!auth())
        return;
    String s;
    ntripEventLog(s);
    server.send(200, "text/plain", s);
}

static void handleScan()
{
    if (!auth())
        return;
    int n = WiFi.scanNetworks();
    JsonDocument d;
    JsonArray a = d.to<JsonArray>();
    for (int i = 0; i < n && i < 20; i++)
    {
        JsonObject o = a.add<JsonObject>();
        o["ssid"] = WiFi.SSID(i);
        o["rssi"] = WiFi.RSSI(i);
    }
    String js;
    serializeJson(d, js);
    server.send(200, "application/json", js);
}

void webSetup()
{
    server.on("/", HTTP_GET, []() {
        if (!auth())
            return;
        server.send_P(200, "text/html", INDEX_HTML);
    });
    server.on("/api/status", HTTP_GET, handleStatus);
    server.on("/api/config", HTTP_GET, handleGetConfig);
    server.on("/api/config", HTTP_POST, handlePostConfig);
    server.on("/api/cmd", HTTP_POST, handleCmd);
    server.on("/api/log", HTTP_GET, handleLog);
    server.on("/api/scan", HTTP_GET, handleScan);
    server.on("/api/reboot", HTTP_POST, []() {
        if (!auth())
            return;
        server.send(200, "text/plain", "Restarting");
        rebootAt = millis() + 1000;
    });
    // Switch between RTK (NTRIP) and Galileo HAS without a restart; saved, so it survives power cycles
    server.on("/api/mode", HTTP_POST, []() {
        if (!auth())
            return;
        String m = server.arg("plain");
        m.trim();
        if (m != "rtk" && m != "has")
        {
            server.send(400, "text/plain", "Mode must be rtk or has");
            return;
        }
        cfg.mode = m;
        if (!configSave())
        {
            server.send(500, "text/plain", "Could not write the settings to flash");
            return;
        }
        extern void sendHasCommand();
        sendHasCommand();
        ntripRestart();
        server.send(200, "text/plain", m == "has" ? "Galileo HAS mode: NTRIP off. Convergence takes several minutes." : "RTK mode: NTRIP corrections on.");
    });
    // Reference point: "set" = current position, only from an RTK fixed solution; "force" = any fix; "clear"
    server.on("/api/ref", HTTP_POST, []() {
        if (!auth())
            return;
        String a = server.arg("plain");
        a.trim();
        RefPoint r;
        if (a == "set" || a == "force")
        {
            SharedGga g;
            static MethodTracker mt;
            RefPoint old;
            getMethods(mt, old);
            if (!getLatestGga(g) || millis() - g.atMs > 3000 || g.info.quality == 0)
            {
                server.send(409, "text/plain", "No current position from the receiver");
                return;
            }
            if (a == "set" && mt.current() != M_RTK_FIXED)
            {
                server.send(409, "text/plain", "Needs an RTK fixed solution (now: " + String(methodName(mt.current())) + ")");
                return;
            }
            r.set = true;
            r.lat = g.info.lat;
            r.lon = g.info.lon;
            r.alt = g.info.alt;
        }
        else if (a != "clear")
        {
            server.send(400, "text/plain", "Use set, force or clear");
            return;
        }
        cfg.refSet = r.set;
        cfg.refLat = r.lat;
        cfg.refLon = r.lon;
        cfg.refAlt = r.alt;
        if (!configSave())
        {
            server.send(500, "text/plain", "Could not write the settings to flash");
            return;
        }
        setRefPoint(r);
        server.send(200, "text/plain", r.set ? "Reference point recorded at the current position." : "Reference point cleared.");
    });
    // Static-demo hold of the last SBAS position: "on" / "off"
    server.on("/api/hold", HTTP_POST, []() {
        if (!auth())
            return;
        String a = server.arg("plain");
        a.trim();
        if (a != "on" && a != "off")
        {
            server.send(400, "text/plain", "Use on or off");
            return;
        }
        cfg.holdSbas = a == "on";
        if (!configSave())
        {
            server.send(500, "text/plain", "Could not write the settings to flash");
            return;
        }
        server.send(200, "text/plain", cfg.holdSbas ? "Static-demo hold on: only for an antenna that does not move." : "Hold off.");
    });
    // Accuracy trace: last 30 min, one sample every 10 s: [uptime s, method code, estimate H, error H] (-1 = none)
    server.on("/api/trace", HTTP_GET, []() {
        if (!auth())
            return;
        static MethodTracker mt;
        RefPoint rp;
        getMethods(mt, rp);
        String js;
        js.reserve(TRACE_N * 28 + 64);
        js = "{\"now\":" + String(millis() / 1000) + ",\"step\":" + String(TRACE_STEP_MS / 1000) + ",\"samples\":[";
        for (int i = 0; i < mt.traceCount(); i++)
        {
            const TraceSample &t = mt.trace(i);
            if (i)
                js += ',';
            js += '[';
            js += String(t.tSec);
            js += ',';
            js += String(t.method);
            js += ',';
            js += String(t.estH, 3);
            js += ',';
            js += String(t.errH, 3);
            js += ']';
        }
        js += "]}";
        server.sendHeader("Cache-Control", "no-store");
        server.send(200, "application/json", js);
    });
    server.on("/api/factory", HTTP_POST, []() {
        if (!auth())
            return;
        if (!configReset())
        {
            server.send(500, "text/plain", "Could not delete the saved settings");
            return;
        }
        server.send(200, "text/plain", "Saved settings deleted. Restarting with the built-in defaults.");
        rebootAt = millis() + 1000;
    });
    server.on("/api/shutdown", HTTP_POST, []() {
        if (!auth())
            return;
        server.send(200, "text/plain", "Shutting down. Wait until the LEDs are off (about 2 s), then unplug.");
        shutdownAt = millis() + 1000;
    });
    server.on("/api/nmea", HTTP_GET, []() {
        if (!auth())
            return;
        JsonDocument d;
        JsonArray a = d["types"].to<JsonArray>();
        mutex_enter_blocking(&typesMutex);
        for (int i = 0; i < nmeaTypeCount; i++)
        {
            JsonObject o = a.add<JsonObject>();
            o["id"] = nmeaTypes[i].id;
            o["count"] = nmeaTypes[i].count;
            o["sent"] = nmeaTypes[i].sent;
        }
        // Receiver replies, oldest first
        JsonArray r = d["replies"].to<JsonArray>();
        uint32_t first = rxReplySeq > RX_REPLIES ? rxReplySeq - RX_REPLIES : 0;
        for (uint32_t k = first; k < rxReplySeq; k++)
            r.add(rxReplies[k % RX_REPLIES]);
        d["replySeq"] = rxReplySeq;
        mutex_exit(&typesMutex);
        d["filter"] = cfg.nmeaTypes;
        d["text"] = outputsMonitor();
        String js;
        serializeJson(d, js);
        server.sendHeader("Cache-Control", "no-store");
        server.send(200, "application/json", js);
    });
    server.onNotFound([]() { server.send(404, "text/plain", "Not found"); });
    server.begin();
}

void webLoop()
{
    server.handleClient();
    if (shutdownAt && (int32_t)(millis() - shutdownAt) > 0)
        safeShutdown();
    if (rebootAt && (int32_t)(millis() - rebootAt) > 0)
        safeReboot();
}
