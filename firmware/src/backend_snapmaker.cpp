#include "backend_snapmaker.h"
#include <WebSocketsClient.h>
#include <ArduinoJson.h>

namespace {
    const char* SLOTS[4] = { "A", "B", "C", "D" };

    WebSocketsClient ws;
    bool      g_connected = false;
    String    g_status = "Snap: a ligar...";
    SlotState g_slots[4];
    uint32_t  g_lastReq = 0;

    void hexRGBA(const char* s, uint8_t& r, uint8_t& g, uint8_t& b) {
        if (!s) return;
        if (*s == '#') s++;
        if (strlen(s) < 6) return;
        auto hx = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            c |= 0x20; return (c >= 'a' && c <= 'f') ? c - 'a' + 10 : 0;
        };
        r = hx(s[0]) * 16 + hx(s[1]);
        g = hx(s[2]) * 16 + hx(s[3]);
        b = hx(s[4]) * 16 + hx(s[5]);
    }

    // print_task_config: 4 arrays paralelos (color_rgba / type / vendor)
    void applyConfig(JsonObjectConst c) {
        JsonArrayConst col = c["filament_color_rgba"];
        JsonArrayConst typ = c["filament_type"];
        if (col.isNull() && typ.isNull()) return;
        for (int i = 0; i < 4; i++) {
            SlotState& s = g_slots[i];
            const char* t  = typ.isNull() ? "" : (const char*)(typ[i] | "");
            const char* cc = col.isNull() ? "" : (const char*)(col[i] | "");
            s.type  = t;
            s.known = strlen(t) > 0;
            uint8_t r = 90, g = 90, b = 90;
            hexRGBA(cc, r, g, b);
            s.r = r; s.g = g; s.b = b;
        }
        g_status = "Snap: slots atualizados";
    }

    void onMsg(uint8_t* payload, size_t len) {
        JsonDocument d;
        if (deserializeJson(d, payload, len)) return;
        // resposta a query -> result.status.print_task_config
        JsonObjectConst r1 = d["result"]["status"]["print_task_config"];
        if (!r1.isNull()) { applyConfig(r1); return; }
        // push -> {"method":"notify_status_update","params":[{print_task_config:{...}}, t]}
        if (String(d["method"] | "") == "notify_status_update") {
            JsonObjectConst r2 = d["params"][0]["print_task_config"];
            if (!r2.isNull()) applyConfig(r2);
        }
    }

    void onEvent(WStype_t type, uint8_t* payload, size_t len) {
        switch (type) {
            case WStype_CONNECTED:    g_connected = true;  g_status = "Snap: ligado"; break;
            case WStype_DISCONNECTED: g_connected = false; g_status = "Snap: desligado"; break;
            case WStype_TEXT:         onMsg(payload, len); break;
            default: break;
        }
    }

    bool sendRaw(String s) {
        if (!g_connected) return false;
        Serial.printf("[snap] -> %s\n", s.c_str());
        return ws.sendTXT(s);
    }
    // G-code arguments are separated by spaces, so strip spaces out of the
    // vendor and type
    String noSpace(const String& in) {
        String o; o.reserve(in.length());
        for (size_t i = 0; i < in.length(); i++) o += (in[i] == ' ' ? '_' : in[i]);
        return o;
    }
}

void SnapmakerBackend::begin(const PrinterCfg& cfg) {
    for (int i = 0; i < 4; i++) g_slots[i] = SlotState{};
    g_connected = false;
    g_status = "Snap: a ligar...";
    ws.begin(cfg.host, 7125, "/websocket");
    ws.onEvent(onEvent);
    ws.setReconnectInterval(10000);
    ws.enableHeartbeat(15000, 3000, 2);
}

void SnapmakerBackend::loop() {
    ws.loop();
    if (g_connected && millis() - g_lastReq > 5000) { g_lastReq = millis(); refresh(); }
}

void SnapmakerBackend::stop() {
    ws.disconnect();
    g_connected = false;
    g_status = "Snap: parado";
}

bool SnapmakerBackend::connected() { return g_connected; }
String SnapmakerBackend::status()  { return g_status; }
const char* SnapmakerBackend::slotLabel(int i) { return SLOTS[(i >= 0 && i < 4) ? i : 0]; }
const SlotState& SnapmakerBackend::slot(int i) { return g_slots[(i >= 0 && i < 4) ? i : 0]; }

void SnapmakerBackend::refresh() {
    sendRaw("{\"jsonrpc\":\"2.0\",\"method\":\"printer.objects.query\","
            "\"params\":{\"objects\":{\"print_task_config\":null}},\"id\":1001}");
}

bool SnapmakerBackend::assign(int idx, const TagInfo& t) {
    if (idx < 0 || idx >= 4 || !g_connected) return false;
    String vend = noSpace(t.brand.length()    ? t.brand    : String("Generic"));
    String mat  = noSpace(t.material.length() ? t.material : String("PLA"));
    char script[200];
    snprintf(script, sizeof(script),
        "SET_PRINT_FILAMENT_CONFIG CONFIG_EXTRUDER=%d VENDOR=%s FILAMENT_TYPE=%s "
        "FILAMENT_SUBTYPE= FILAMENT_COLOR_RGBA=%02X%02X%02XFF",
        idx, vend.c_str(), mat.c_str(), t.r, t.g, t.b);

    JsonDocument d;
    d["jsonrpc"] = "2.0";
    d["method"]  = "printer.gcode.script";
    d["params"]["script"] = script;
    d["id"] = 201;
    String out; serializeJson(d, out);

    bool ok = sendRaw(out);
    g_status = ok ? (String("enviado -> ") + SLOTS[idx]) : "falha no envio";
    if (ok) { delay(200); refresh(); }
    return ok;
}
