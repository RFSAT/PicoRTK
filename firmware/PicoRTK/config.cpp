// config.cpp - see config.h
#include "config.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "util.h"

Config cfg;
static const char *CFG_FILE = "/config.json";
static const char *MASK = "********";

static void putSecret(JsonObject o, const char *key, const String &v, bool withSecrets)
{
    o[key] = (withSecrets || v.length() == 0) ? v : String(MASK);
}

static void getSecret(JsonVariantConst v, String &dst)
{
    if (v.isNull())
        return;
    String s = v.as<String>();
    if (s == MASK)
        return; // unchanged
    dst = s;
}

static void getStr(JsonVariantConst v, String &dst)
{
    if (!v.isNull())
        dst = v.as<String>();
}

template <typename T> static void getNum(JsonVariantConst v, T &dst)
{
    if (!v.isNull())
        dst = v.as<T>();
}

String configToJson(bool withSecrets)
{
    JsonDocument d;
    d["deviceName"] = cfg.deviceName;
    putSecret(d["web"].to<JsonObject>(), "adminPass", cfg.adminPass, withSecrets);
    d["web"]["apPass"] = withSecrets ? cfg.apPass : String(MASK);

    JsonArray w = d["wifi"].to<JsonArray>();
    for (int i = 0; i < CFG_MAX_WIFI; i++)
    {
        if (!cfg.wifi[i].ssid.length())
            continue;
        JsonObject o = w.add<JsonObject>();
        o["ssid"] = cfg.wifi[i].ssid;
        putSecret(o, "pass", cfg.wifi[i].pass, withSecrets);
    }

    JsonArray n = d["ntrip"].to<JsonArray>();
    for (int i = 0; i < CFG_MAX_SOURCES; i++)
    {
        const NtripSource &s = cfg.src[i];
        JsonObject o = n.add<JsonObject>();
        o["name"] = s.name;
        o["type"] = s.type;
        o["host"] = s.host;
        o["port"] = s.port;
        o["mount"] = s.mount;
        o["user"] = s.user;
        putSecret(o, "pass", s.pass, withSecrets);
        o["gga"] = s.gga;
        o["v2"] = s.v2;
        o["tls"] = s.tls;
    }

    JsonObject f = d["failover"].to<JsonObject>();
    f["enabled"] = cfg.failover;
    f["failbackSec"] = cfg.failbackSec;
    f["retryMs"] = cfg.retryMs;
    JsonArray b = f["backoffSec"].to<JsonArray>();
    for (uint32_t v : cfg.backoffSec)
        b.add(v);
    f["dataTimeoutSec"] = cfg.dataTimeoutSec;
    f["maxAgeSec"] = cfg.maxAgeSec;
    f["ggaSec"] = cfg.ggaSec;
    JsonObject bp = d["benchPosition"].to<JsonObject>();
    bp["lat"] = cfg.benchLat;
    bp["lon"] = cfg.benchLon;
    bp["alt"] = cfg.benchAlt;

    JsonObject out = d["outputs"].to<JsonObject>();
    out["bt"] = cfg.bt;
    out["usb"] = cfg.usb;
    out["tcpPort"] = cfg.tcpPort;
    out["inboundRtcm"] = cfg.inboundRtcm;
    JsonArray nt = out["nmeaTypes"].to<JsonArray>();
    {
        int from = 0;
        while (from < (int)cfg.nmeaTypes.length())
        {
            int comma = cfg.nmeaTypes.indexOf(',', from);
            if (comma < 0)
                comma = cfg.nmeaTypes.length();
            String t = cfg.nmeaTypes.substring(from, comma);
            t.trim();
            if (t.length())
                nt.add(t);
            from = comma + 1;
        }
    }

    JsonObject r = d["receiver"].to<JsonObject>();
    r["uartBaud"] = cfg.uartBaud;
    JsonArray bc = r["bootCmds"].to<JsonArray>();
    for (int i = 0; i < CFG_MAX_BOOTCMDS; i++)
        if (cfg.bootCmd[i].length())
            bc.add(cfg.bootCmd[i]);

    String s;
    serializeJsonPretty(d, s);
    return s;
}

bool configFromJson(const String &json, String &err)
{
    JsonDocument d;
    DeserializationError e = deserializeJson(d, json);
    if (e)
    {
        err = String("JSON error: ") + e.c_str();
        return false;
    }
    Config c = cfg; // validate into a copy
    getStr(d["deviceName"], c.deviceName);
    getSecret(d["web"]["adminPass"], c.adminPass);
    getSecret(d["web"]["apPass"], c.apPass);

    if (d["wifi"].is<JsonArrayConst>())
    {
        WifiNet old[CFG_MAX_WIFI];
        for (int i = 0; i < CFG_MAX_WIFI; i++)
            old[i] = c.wifi[i];
        int i = 0;
        for (JsonObjectConst o : d["wifi"].as<JsonArrayConst>())
        {
            if (i >= CFG_MAX_WIFI)
                break;
            WifiNet nw;
            getStr(o["ssid"], nw.ssid);
            if (!nw.ssid.length())
                continue;
            // A masked password keeps the stored one for the same SSID
            for (int j = 0; j < CFG_MAX_WIFI; j++)
                if (old[j].ssid == nw.ssid)
                    nw.pass = old[j].pass;
            getSecret(o["pass"], nw.pass);
            c.wifi[i++] = nw;
        }
        for (; i < CFG_MAX_WIFI; i++)
            c.wifi[i] = WifiNet();
    }

    if (d["ntrip"].is<JsonArrayConst>())
    {
        int i = 0;
        for (JsonObjectConst o : d["ntrip"].as<JsonArrayConst>())
        {
            if (i >= CFG_MAX_SOURCES)
                break;
            NtripSource &s = c.src[i];
            String oldHost = s.host, oldUser = s.user;
            getStr(o["name"], s.name);
            getStr(o["type"], s.type);
            getStr(o["host"], s.host);
            getNum(o["port"], s.port);
            getStr(o["mount"], s.mount);
            getStr(o["user"], s.user);
            getSecret(o["pass"], s.pass);
            if ((s.host != oldHost || s.user != oldUser) && o["pass"].as<String>() == MASK)
                s.pass = ""; // never send one caster's password to another
            getNum(o["gga"], s.gga);
            getNum(o["v2"], s.v2);
            getNum(o["tls"], s.tls);
            s.host.trim();
            s.mount.trim();
            if (s.mount.startsWith("/"))
                s.mount.remove(0, 1);
            i++;
        }
        for (; i < CFG_MAX_SOURCES; i++)
            c.src[i] = NtripSource();
    }

    JsonVariantConst f = d["failover"];
    if (!f.isNull())
    {
        getNum(f["enabled"], c.failover);
        getNum(f["failbackSec"], c.failbackSec);
        getNum(f["retryMs"], c.retryMs);
        if (f["backoffSec"].is<JsonArrayConst>())
        {
            int i = 0;
            for (JsonVariantConst v : f["backoffSec"].as<JsonArrayConst>())
                if (i < 5)
                    c.backoffSec[i++] = v.as<uint32_t>();
            for (; i < 5 && i > 0; i++)
                c.backoffSec[i] = c.backoffSec[i - 1];
        }
        getNum(f["dataTimeoutSec"], c.dataTimeoutSec);
        getNum(f["maxAgeSec"], c.maxAgeSec);
        getNum(f["ggaSec"], c.ggaSec);
    }

    JsonVariantConst bp = d["benchPosition"];
    if (!bp.isNull())
    {
        getNum(bp["lat"], c.benchLat);
        getNum(bp["lon"], c.benchLon);
        getNum(bp["alt"], c.benchAlt);
    }
    if (c.benchLat < -90 || c.benchLat > 90 || c.benchLon < -180 || c.benchLon > 180)
    {
        err = "benchPosition out of range";
        return false;
    }

    JsonVariantConst o = d["outputs"];
    if (!o.isNull())
    {
        getStr(o["bt"], c.bt);
        getNum(o["usb"], c.usb);
        getNum(o["tcpPort"], c.tcpPort);
        getNum(o["inboundRtcm"], c.inboundRtcm);
        if (o["nmeaTypes"].is<JsonArrayConst>())
        {
            String list;
            for (JsonVariantConst v : o["nmeaTypes"].as<JsonArrayConst>())
            {
                String t = v.as<String>();
                t.trim();
                t.toUpperCase();
                if (!t.length() || t.length() > 10)
                    continue;
                if (list.length())
                    list += ",";
                list += t;
            }
            c.nmeaTypes = list;
        }
    }

    JsonVariantConst r = d["receiver"];
    if (!r.isNull())
    {
        getNum(r["uartBaud"], c.uartBaud);
        if (r["bootCmds"].is<JsonArrayConst>())
        {
            int i = 0;
            for (JsonVariantConst v : r["bootCmds"].as<JsonArrayConst>())
                if (i < CFG_MAX_BOOTCMDS)
                    c.bootCmd[i++] = v.as<String>();
            for (; i < CFG_MAX_BOOTCMDS; i++)
                c.bootCmd[i] = "";
        }
    }

    // Validation
    if (c.bt != "spp" && c.bt != "ble" && c.bt != "both" && c.bt != "off")
    {
        err = "outputs.bt must be spp, ble, both or off";
        return false;
    }
    if (c.apPass.length() < 8)
    {
        err = "web.apPass must have at least 8 characters";
        return false;
    }
    if (c.uartBaud < 9600 || c.uartBaud > 3000000)
    {
        err = "receiver.uartBaud out of range";
        return false;
    }
    if (c.retryMs < 500)
        c.retryMs = 500;
    if (c.dataTimeoutSec < 5)
        c.dataTimeoutSec = 5;
    if (c.ggaSec < 1)
        c.ggaSec = 1;
    if (!c.deviceName.length())
        c.deviceName = "PicoRTK";
    cfg = c;
    return true;
}

bool configLoad()
{
    // Defaults: the RFSAT source order with the hosts filled in, credentials empty
    cfg = Config();
    cfg.src[0].name = "Own base";
    cfg.src[0].type = "base";
    // Metrica nearest station first: 6 mm measured with the EOS Arrow Gold on MSM_NEAR
    cfg.src[1].name = "Metrica nearest";
    cfg.src[1].type = "near";
    cfg.src[1].host = "gr.nrtk.eu";
    cfg.src[1].port = 7801;
    cfg.src[1].mount = "MSM_NEAR";
    cfg.src[1].gga = true;
    cfg.src[2].name = "Metrica VRS";
    cfg.src[2].type = "vrs";
    cfg.src[2].host = "gr.nrtk.eu";
    cfg.src[2].port = 7801;
    cfg.src[2].mount = "MSM_VRS";
    cfg.src[2].gga = true;
    cfg.src[3].name = "EUREF";
    cfg.src[3].type = "near";
    cfg.src[3].host = "euref-ip.net";
    cfg.src[3].port = 2101;

    File f = LittleFS.open(CFG_FILE, "r");
    if (!f)
        return false;
    String json = f.readString();
    f.close();
    String err;
    if (!configFromJson(json, err))
    {
        dbg("[cfg] %s, using defaults\r\n", err.c_str());
        return false;
    }
    return true;
}

bool configSave()
{
    String json = configToJson(true);
    File f = LittleFS.open("/config.tmp", "w");
    if (!f)
        return false;
    size_t n = f.print(json);
    f.close();
    if (n != json.length())
        return false;
    LittleFS.remove(CFG_FILE);
    return LittleFS.rename("/config.tmp", CFG_FILE);
}
