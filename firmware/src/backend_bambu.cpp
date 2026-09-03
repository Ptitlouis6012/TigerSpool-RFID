#include "backend_bambu.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

namespace {
    // --- mapa de slots (indice UI -> ams_id / tray_id), descoberto do pushall ---
    static const int BMAX = 17;                 // Ext + 4 unidades x 4 tabuleiros
    struct BSlot { char name[4]; int ams; int tray; };
    BSlot g_map[BMAX] = {
        { "Ext", 255, 254 },                    // spool externo (vt_tray)
        { "A", 0, 0 }, { "B", 0, 1 }, { "C", 0, 2 }, { "D", 0, 3 },   // palpite AMS Lite
    };
    int  g_nSlots = 5;                          // ate ao 1o relatorio

    WiFiClientSecure net;
    PubSubClient     mqtt(net);
    String           g_host, g_sn, g_cc;
    String           g_topReport, g_topRequest;
    bool             g_connected = false;
    String           g_status = "Bambu: a ligar...";
    SlotState        g_slots[BMAX];
    uint32_t         g_seq = 0;
    uint32_t         g_lastTry = 0, g_lastPush = 0;

    void setDefaultMap() {
        static const BSlot def[5] = {
            { "Ext", 255, 254 }, { "A", 0, 0 }, { "B", 0, 1 }, { "C", 0, 2 }, { "D", 0, 3 },
        };
        for (int i = 0; i < 5; i++) g_map[i] = def[i];
        g_nSlots = 5;
    }

    // reconstroi g_map a partir das unidades AMS presentes no relatorio
    void rebuildMap(JsonArrayConst amsArr) {
        int ids[4], nu = 0;
        if (!amsArr.isNull())
            for (JsonObjectConst a : amsArr) {
                int id = a["id"].as<int>();
                if (id < 0 || id > 3) continue;
                bool dup = false;
                for (int i = 0; i < nu; i++) if (ids[i] == id) dup = true;
                if (!dup && nu < 4) ids[nu++] = id;
            }
        for (int i = 0; i < nu; i++)
            for (int j = i + 1; j < nu; j++)
                if (ids[j] < ids[i]) { int t = ids[i]; ids[i] = ids[j]; ids[j] = t; }

        int n = 0;
        strcpy(g_map[n].name, "Ext"); g_map[n].ams = 255; g_map[n].tray = 254; n++;
        for (int u = 0; u < nu && n < BMAX; u++)
            for (int tr = 0; tr < 4 && n < BMAX; tr++) {
                char L = 'A' + tr;
                if (nu > 1) snprintf(g_map[n].name, 4, "%d%c", ids[u] + 1, L);
                else        snprintf(g_map[n].name, 4, "%c", L);
                g_map[n].ams = ids[u];
                g_map[n].tray = tr;
                n++;
            }
        if (n != g_nSlots) Serial.printf("[bambu] AMS: %d unidade(s) -> %d slots\n", nu, n);
        g_nSlots = n;
    }

    // TigerTag material -> { tray_info_idx, tray_type } genericos da Bambu
    struct BMat { const char* idx; const char* type; };
    BMat bambuMat(const String& in) {
        String s = in; s.toUpperCase();
        auto has = [&](const char* k) { return s.indexOf(k) >= 0; };
        if (has("PLA") && has("CF"))  return { "GFL98", "PLA-CF" };
        if (has("PETG"))              return { "GFG99", "PETG" };
        if (has("PLA"))               return { "GFL99", "PLA" };
        if (has("ASA"))               return { "GFB98", "ASA" };
        if (has("ABS"))               return { "GFB99", "ABS" };
        if (has("TPU"))               return { "GFU99", "TPU" };
        if (has("PVA"))               return { "GFS99", "PVA" };
        if (has("PC"))                return { "GFC99", "PC" };
        if (has("PA") && has("CF"))   return { "GFN98", "PA-CF" };
        if (has("PA"))                return { "GFN99", "PA" };
        return { "GFL99", "PLA" };
    }

    void parseCol(const char* s, uint8_t& r, uint8_t& g, uint8_t& b) {
        if (!s || strlen(s) < 6) return;
        auto hx = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            c |= 0x20; return (c >= 'a' && c <= 'f') ? c - 'a' + 10 : 0;
        };
        r = hx(s[0]) * 16 + hx(s[1]);
        g = hx(s[2]) * 16 + hx(s[3]);
        b = hx(s[4]) * 16 + hx(s[5]);
    }

    void applyTray(int ams, int trayId, JsonObjectConst t) {
        int si = -1;
        for (int i = 0; i < g_nSlots; i++) if (g_map[i].ams == ams && g_map[i].tray == trayId) si = i;
        if (si < 0) return;
        SlotState& s = g_slots[si];
        const char* tt = t["tray_type"] | "";
        const char* tc = t["tray_color"] | "";
        s.type  = tt;
        s.known = strlen(tt) > 0;
        uint8_t r = 90, g = 90, b = 90;
        parseCol(tc, r, g, b);
        s.r = r; s.g = g; s.b = b;
    }

    void onMqtt(char* /*topic*/, uint8_t* payload, unsigned int len) {
        // filtro: so os campos dos slots (o pushall completo passa dos 50 KB)
        JsonDocument filter;
        {
            JsonObject tr = filter["print"]["ams"]["ams"][0].to<JsonObject>();
            tr["id"] = true;
            JsonObject ty = tr["tray"][0].to<JsonObject>();
            ty["id"] = true; ty["tray_type"] = true; ty["tray_color"] = true;
            JsonObject vt = filter["print"]["vt_tray"].to<JsonObject>();
            vt["tray_type"] = true; vt["tray_color"] = true;
        }
        JsonDocument doc;
        if (deserializeJson(doc, payload, len,
                            DeserializationOption::Filter(filter),
                            DeserializationOption::NestingLimit(20))) return;
        JsonObjectConst pr = doc["print"];
        if (pr.isNull()) return;

        // AMS (o "id" pode vir como string "0" ou como numero)
        JsonArrayConst amsArr = pr["ams"]["ams"].as<JsonArrayConst>();
        if (!amsArr.isNull()) {
            rebuildMap(amsArr);                 // topologia real (AMS Lite / AMS / multi)
            for (JsonObjectConst a : amsArr) {
                int amsId = a["id"].as<int>();
                JsonArrayConst trays = a["tray"].as<JsonArrayConst>();
                if (trays.isNull()) continue;
                for (JsonObjectConst t : trays) {
                    if (t["id"].isNull()) continue;
                    applyTray(amsId, t["id"].as<int>(), t);
                }
            }
            g_status = "Bambu: slots atualizados";
        }
        // spool externo
        JsonObjectConst vt = pr["vt_tray"];
        if (!vt.isNull()) applyTray(255, 254, vt);
    }

    void pubRequest(const String& body) {
        Serial.printf("[bambu] -> %s\n", body.c_str());
        mqtt.publish(g_topRequest.c_str(), body.c_str());
    }
}

void BambuBackend::begin(const PrinterCfg& cfg) {
    g_host = cfg.host; g_sn = cfg.sn; g_cc = cfg.cc;   // user "bblp", pass = access code (Modo LAN)
    setDefaultMap();                              // Ext + A..D ate ao 1o pushall
    for (int i = 0; i < BMAX; i++) g_slots[i] = SlotState{};
    g_connected = false;
    g_status = "Bambu: a ligar...";
    g_topReport  = String("device/") + g_sn + "/report";
    g_topRequest = String("device/") + g_sn + "/request";

    net.setInsecure();                 // the printer presents a self-signed cert
    mqtt.setServer(g_host.c_str(), 8883);
    // A pushall from an X1 with four AMS units reaches about 50 KB. If the
    // buffer cannot hold it the topology
    // (nr de unidades) nunca e detetada. Buffer generoso (heap chega, o sprite
    // LVGL esta em PSRAM).
    mqtt.setBufferSize(51200);
    mqtt.setKeepAlive(30);
    mqtt.setCallback(onMqtt);
    g_lastTry = 0;
}

void BambuBackend::loop() {
    if (!mqtt.connected()) {
        g_connected = false;
        if (millis() - g_lastTry < 4000) return;
        g_lastTry = millis();
        Serial.printf("[bambu] a ligar a %s:8883...\n", g_host.c_str());
        String cid = "tigertag-" + String((uint32_t)ESP.getEfuseMac(), HEX);
        if (mqtt.connect(cid.c_str(), "bblp", g_cc.c_str())) {
            mqtt.subscribe(g_topReport.c_str());
            g_connected = true;
            g_status = "Bambu: ligado";
            Serial.println("[bambu] ligado + subscrito");
            refresh();
        } else {
            g_status = String("Bambu: MQTT rc=") + mqtt.state() + " (access code?)";
            Serial.println(g_status);
        }
        return;
    }
    mqtt.loop();
    if (millis() - g_lastPush > 8000) { g_lastPush = millis(); refresh(); }
}

void BambuBackend::stop() {
    mqtt.disconnect();
    g_connected = false;
    g_status = "Bambu: parado";
}

bool BambuBackend::connected() { return g_connected; }
String BambuBackend::status()  { return g_status; }
int  BambuBackend::slotCount() { return g_nSlots; }
const char* BambuBackend::slotLabel(int i) { return g_map[(i >= 0 && i < g_nSlots) ? i : 0].name; }
const SlotState& BambuBackend::slot(int i) { return g_slots[(i >= 0 && i < g_nSlots) ? i : 0]; }

void BambuBackend::refresh() {
    if (!g_connected) return;
    JsonDocument d;
    JsonObject p = d["pushing"].to<JsonObject>();
    p["sequence_id"] = String(g_seq++);
    p["command"] = "pushall";
    p["version"] = 1;
    p["push_target"] = 1;
    String b; serializeJson(d, b);
    pubRequest(b);
}

bool BambuBackend::assign(int idx, const TagInfo& t) {
    if (idx < 0 || idx >= g_nSlots || !g_connected) return false;
    BMat m = bambuMat(t.material);
    char col[9]; snprintf(col, sizeof(col), "%02X%02X%02XFF", t.r, t.g, t.b);   // RRGGBBAA

    JsonDocument d;
    JsonObject p = d["print"].to<JsonObject>();
    p["sequence_id"]    = String(g_seq++);
    p["command"]        = "ams_filament_setting";
    p["ams_id"]         = g_map[idx].ams;
    p["tray_id"]        = g_map[idx].tray;
    p["tray_info_idx"]  = m.idx;
    p["tray_color"]     = col;
    p["nozzle_temp_min"] = t.nozMin ? t.nozMin : 190;
    p["nozzle_temp_max"] = t.nozMax ? t.nozMax : 240;
    p["tray_type"]      = m.type;
    String b; serializeJson(d, b);
    pubRequest(b);

    g_status = String("enviado -> ") + g_map[idx].name + " " + m.type;
    delay(200);
    refresh();
    return true;
}
