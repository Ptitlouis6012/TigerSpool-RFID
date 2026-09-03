#include "backend_ff.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

namespace {
    String    g_host, g_sn, g_cc;
    bool      g_auth = false;
    String    g_status = "FF: a ligar...";
    SlotState g_slots[4];
    uint32_t  g_lastReq = 0;
    uint32_t  g_lastAuth = 0;
    const char* LABELS[4] = { "T1", "T2", "T3", "T4" };

    // TigerTag material -> the Creator 5's list of 21 materials
    String ffMaterial(const String& in) {
        String s = in; s.toUpperCase();
        auto has = [&](const char* k) { return s.indexOf(k) >= 0; };
        if (has("PPS"))                 return "PPS-CF";
        if (has("PAHT"))               return "PAHT-CF";
        if (has("PA") && has("CF"))    return "PA-CF";
        if (has("PET") && has("CF") && !has("PETG")) return "PET-CF";
        if (has("PETG") && has("CF"))  return "PETG-CF";
        if (has("PLA") && has("CF"))   return "PLA-CF";
        if (has("PC") && has("ABS"))   return "PC-ABS";
        if (has("TPU")) { if (has("64")) return "TPU-64D";
                          if (has("90")) return "TPU-90A"; return "TPU-95A"; }
        if (has("SILK"))               return "SILK";
        if (has("PETG"))               return "PETG";
        if (has("PLA"))                return "PLA";
        if (has("ABS"))                return "ABS";
        if (has("ASA"))                return "ASA";
        if (has("HIPS"))              return "HIPS";
        if (has("PVA"))                return "PVA";
        if (has("PC"))                 return "PC";
        if (has("PA"))                 return "PA";
        return "PLA";
    }

    // The Creator 5's own 24-colour palette. The printer accepts nothing else:
    // any other hex is silently ignored and the slot reverts to white, while the
    // call still answers success. The tag's colour is snapped to the nearest
    // entry here - see docs/PRINTER-COMPATIBILITY.md.
    struct FfColor { uint32_t rgb; const char* name; };
    const FfColor FF_PALETTE[24] = {
        { 0xFFFFFF, "White" },      { 0xFFF245, "Yellow" },     { 0xDEF578, "Light Green" },
        { 0x21CC3D, "Green" },      { 0x167A4B, "Dark Green" }, { 0x156682, "Teal" },
        { 0x24E4A0, "Cyan" },       { 0x7BD9F0, "Light Blue" }, { 0x4CAAF8, "Blue" },
        { 0x2E54DD, "Dark Blue" },  { 0x48358C, "Purple" },     { 0xA341F7, "Violet" },
        { 0xF435F6, "Magenta" },    { 0xD5B4DE, "Pink" },       { 0xFA6173, "Coral" },
        { 0xF82D29, "Red" },        { 0x805003, "Brown" },      { 0xF9903B, "Orange" },
        { 0xFCEBD7, "Cream" },      { 0xD5C5A1, "Tan" },        { 0xB17C38, "Dark Brown" },
        { 0x8C8C89, "Gray" },       { 0xBEBEBE, "Light Gray" }, { 0x1B1B1B, "Black" },
    };

    // Nearest colour in the palette ("redmean" distance, a good perceptual
    // approximation for the price)
    const FfColor& ffNearest(uint8_t r, uint8_t g, uint8_t b) {
        int best = 0; long bestD = 0x7fffffffL;
        for (int i = 0; i < 24; i++) {
            long pr = (FF_PALETTE[i].rgb >> 16) & 0xFF;
            long pg = (FF_PALETTE[i].rgb >> 8) & 0xFF;
            long pb =  FF_PALETTE[i].rgb        & 0xFF;
            long dr = (long)r - pr, dg = (long)g - pg, db = (long)b - pb;
            long rm = ((long)r + pr) / 2;
            long d  = (((512 + rm) * dr * dr) >> 8) + 4 * dg * dg + (((767 - rm) * db * db) >> 8);
            if (d < bestD) { bestD = d; best = i; }
        }
        return FF_PALETTE[best];
    }

    // POST JSON to http://host:8898<path>. Returns the body, or "" on failure.
    // Preenche 'httpCode'. O firmware manda Content-Type "appliation/json" (typo).
    String post(const String& path, const String& body, int& httpCode) {
        if (WiFi.status() != WL_CONNECTED) { httpCode = -1; return ""; }
        WiFiClient client;
        HTTPClient http;
        String url = "http://" + g_host + ":8898" + path;
        if (!http.begin(client, url)) { httpCode = -2; return ""; }
        http.setTimeout(4000);
        http.addHeader("Content-Type", "application/json");
        httpCode = http.POST((uint8_t*)body.c_str(), body.length());
        String resp = (httpCode > 0) ? http.getString() : String();
        http.end();
        return resp;
    }

    String authBody() {
        JsonDocument d;
        d["serialNumber"] = g_sn;
        d["checkCode"]    = g_cc;
        String s; serializeJson(d, s); return s;
    }

    void parseHex(const char* s, uint8_t& r, uint8_t& g, uint8_t& b) {
        if (!s || !*s) return;
        if (*s == '#') s++;
        long v = strtol(s, nullptr, 16);
        r = (v >> 16) & 0xFF; g = (v >> 8) & 0xFF; b = v & 0xFF;
    }
}

void FlashForgeC5Backend::begin(const PrinterCfg& cfg) {
    g_host = cfg.host; g_sn = cfg.sn; g_cc = cfg.cc;
    // FlashForge's API wants the serial prefixed with "SN"; the TigerTag import
    // hands it over without one
    if (g_sn.length() && !g_sn.startsWith("SN")) g_sn = "SN" + g_sn;
    for (int i = 0; i < 4; i++) g_slots[i] = SlotState{};
    g_auth = false;
    g_status = "FF: a validar...";

    tryAuth();
}

void FlashForgeC5Backend::tryAuth() {
    int code;
    String resp = post("/checkCode", authBody(), code);
    Serial.printf("[flashforge] /checkCode http=%d resp=%s\n", code, resp.c_str());
    JsonDocument d;
    if (code == 200 && !deserializeJson(d, resp)) {
        int c = d["code"] | -99;
        if (c == 0)       { g_auth = true;  g_status = "FF: autenticado"; }
        else if (c == -2) g_status = "FF: Modo LAN desligado";
        else if (c == 1)  g_status = "FF: access code errado";
        else if (c == 3)  g_status = "FF: nao autorizado";
        else if (c == 5)  g_status = "FF: serial errado";
        else              g_status = String("FF: checkCode code ") + c;
    } else {
        g_status = String("FF: sem resposta (") + code + ")";
    }
    g_lastAuth = millis();
    if (g_auth) refresh();
}

void FlashForgeC5Backend::loop() {
    if (g_auth) {
        if (millis() - g_lastReq > 6000) { g_lastReq = millis(); refresh(); }
    } else if (millis() - g_lastAuth > 20000) {   // re-authenticate every 20 s
        tryAuth();
    }
}

bool FlashForgeC5Backend::connected() { return g_auth; }
String FlashForgeC5Backend::status()  { return g_status; }
const char* FlashForgeC5Backend::slotLabel(int i) { return LABELS[i & 3]; }
const SlotState& FlashForgeC5Backend::slot(int i) { return g_slots[i & 3]; }

void FlashForgeC5Backend::refresh() {
    int code;
    String resp = post("/detail", authBody(), code);
    if (code != 200) { g_status = String("FF: /detail http ") + code; return; }
    JsonDocument d;
    if (deserializeJson(d, resp)) { g_status = "FF: /detail json?"; return; }

    // Dump the material station (for debugging colour/material)
    { String ms; serializeJson(d["detail"]["matlStationInfo"], ms);
      Serial.printf("[flashforge] matlStationInfo: %.*s\n", (int)(ms.length() > 320 ? 320 : ms.length()), ms.c_str()); }

    JsonArrayConst si = d["detail"]["matlStationInfo"]["slotInfos"].as<JsonArrayConst>();
    if (si.isNull()) { g_status = "FF: sem estacao de material"; return; }
    for (JsonObjectConst s : si) {
        int id = s["slotId"] | 0;             // 1-based
        if (id < 1 || id > 4) continue;
        SlotState& st = g_slots[id - 1];
        const char* mn = s["materialName"] | "";
        const char* mc = s["materialColor"] | "";
        st.type = mn;
        st.known = strlen(mn) > 0;
        uint8_t r = 90, g = 90, b = 90;
        parseHex(mc, r, g, b);
        st.r = r; st.g = g; st.b = b;
        Serial.printf("[flashforge] slot %d: '%s' color '%s'\n", id, mn, mc);
    }
    int cur = d["detail"]["matlStationInfo"]["currentSlot"] | 0;
    for (int i = 0; i < 4; i++) g_slots[i].selected = (cur == i + 1);
    g_status = "FF: slots atualizados";
}

bool FlashForgeC5Backend::assign(int idx, const TagInfo& t) {
    if (idx < 0 || idx >= 4) return false;
    String mt = ffMaterial(t.material);

    // Snap the tag's colour to the nearest entry in the Creator 5's palette.
    // The C5 demands "#RRGGBB" in UPPERCASE and WITH the '#' (the public doc is
    // wrong. Any other value is silently ignored and the slot reverts to
    // #FFFFFF) even though it answers code:0.
    const FfColor& pc = ffNearest(t.r, t.g, t.b);
    char rgb[9]; snprintf(rgb, sizeof(rgb), "#%06X", pc.rgb);
    Serial.printf("[flashforge] cor tag #%02X%02X%02X -> paleta %s %s\n", t.r, t.g, t.b, pc.name, rgb);

    JsonDocument d;
    d["serialNumber"] = g_sn;
    d["checkCode"]    = g_cc;
    JsonObject pl = d["payload"].to<JsonObject>();
    pl["cmd"] = "msConfig_cmd";
    JsonObject a = pl["args"].to<JsonObject>();
    a["slot"] = idx + 1;                       // 1-based
    a["mt"]   = mt;
    a["rgb"]  = rgb;                           // "#RRGGBB" MAIUSC. COM '#' (paleta C5)
    String body; serializeJson(d, body);
    Serial.printf("[flashforge] -> /control %s\n", body.c_str());

    int code;
    String resp = post("/control", body, code);
    Serial.printf("[flashforge] <- http=%d %s\n", code, resp.c_str());
    JsonDocument r;
    bool ok = (code == 200) && !deserializeJson(r, resp) && ((r["code"] | -1) == 0);
    g_status = ok ? (String("enviado -> ") + LABELS[idx] + " " + mt) : "FF: falha no envio";
    if (ok) { delay(150); refresh(); }        // confirma relendo (cmd desconhecido e ACKed na mesma)
    return ok;
}
