#include "backend_creality.h"
#include "i18n.h"
#include "product_api.h"
#include <WebSocketsClient.h>
#include <ArduinoJson.h>

// ---- K2 slot map (UI index -> boxId/slot) -------------------------------
namespace {
    struct CrealitySlot { const char* name; uint8_t box; uint8_t slot; };
    const CrealitySlot CREALITY_SLOTS[5] = {
        // A null name means the external holder - the spool that is not in the
        // CFS. Its label is the one slot name that is a word rather than a
        // position, so it comes from the translation table at draw time.
        { nullptr, 0, 0 }, { "1A", 1, 0 }, { "1B", 1, 1 }, { "1C", 1, 2 }, { "1D", 1, 3 },
    };

    // A number that is written WITH a decimal point, always.
    //
    // A Creality printer stores a slot's minTemp/maxTemp only when the JSON
    // number has one: sent as 215.0 it reads back 215, sent as 215 it reads
    // back 0 - measured on an Ender-3 V4 with a CFS, three times over, the
    // only difference in the frame. The TigerTag Connect app got it right by
    // accident: Dart writes a double as "215.0". ArduinoJson writes the same
    // double as "215", so the text is built here and handed over verbatim.
    String withPoint(double v) {
        char buf[24];
        snprintf(buf, sizeof(buf), "%.3f", v);          // "215.000", "0.040"
        char* end = buf + strlen(buf) - 1;
        while (*end == '0' && *(end - 1) != '.') *end-- = '\0';
        return String(buf);                             // "215.0", "0.04"
    }

    int slotIndex(int box, int mat) {
        for (int i = 0; i < 5; i++)
            if (CREALITY_SLOTS[i].box == box && CREALITY_SLOTS[i].slot == mat) return i;
        return -1;
    }
    void parseColor(const char* s, uint8_t& r, uint8_t& g, uint8_t& b) {
        if (!s || !*s) return;
        if (*s == '#') s++;
        if (strlen(s) == 7) s++;             // "#0RRGGBB" -> salta o 0
        long v = strtol(s, nullptr, 16);
        r = (v >> 16) & 0xFF; g = (v >> 8) & 0xFF; b = v & 0xFF;
    }
}

void CrealityBackend::applyBoxsInfo(JsonObjectConst bi) {
    JsonArrayConst boxes = bi["materialBoxs"];
    if (boxes.isNull()) return;
    for (JsonObjectConst box : boxes) {
        int boxId = box["id"] | -1;
        for (JsonObjectConst m : box["materials"].as<JsonArrayConst>()) {
            int si = slotIndex(boxId, m["id"] | -1);
            if (si < 0) continue;
            SlotState& s = slots_[si];
            const char* type = m["type"] | "";
            int st = m["state"] | 0;
            s.known = (st != 0) || strlen(type);
            s.type = type;
            s.brand = (const char*)(m["vendor"] | "");
            s.percent = m["percent"] | 0;
            s.selected = (m["selected"] | 0) != 0;
            uint8_t r = 90, g = 90, b = 90;
            parseColor(m["color"] | "", r, g, b);
            s.r = r; s.g = g; s.b = b;
        }
    }
    status_ = "K2: slots atualizados";
}
void CrealityBackend::onMsg(uint8_t* payload, size_t len) {
    const char* p = (const char*)payload;
    JsonDocument doc;
    if (deserializeJson(doc, payload, len)) return;
    if (doc["boxsInfo"].is<JsonObject>()) applyBoxsInfo(doc["boxsInfo"].as<JsonObjectConst>());
    if (doc["err"].is<JsonObject>()) {
        int ec = doc["err"]["errcode"] | 0;
        if (ec) { status_ = String("K2 error ") + ec; }
    }
    (void)p;
}
void CrealityBackend::onEvent(WStype_t type, uint8_t* payload, size_t len) {
    switch (type) {
        case WStype_CONNECTED:    connected_ = true;  status_ = "K2: ligado"; break;
        case WStype_DISCONNECTED: connected_ = false; status_ = "K2: desligado"; break;
        case WStype_TEXT:         onMsg(payload, len); break;
        default: break;
    }
}
bool CrealityBackend::sendDoc(JsonDocument& d) {
    if (!connected_) return false;
    String out; serializeJson(d, out);
    Serial.printf("[creality] -> %s\n", out.c_str());
    return ws_.sendTXT(out);
}

void CrealityBackend::begin(const PrinterCfg& cfg) {
    for (int i = 0; i < 5; i++) slots_[i] = SlotState{};
    connected_ = false;
    status_ = "K2: connecting...";
    ws_.begin(cfg.host, 9999, "/");
    ws_.onEvent([this](WStype_t t, uint8_t* p, size_t l) { onEvent(t, p, l); });
    ws_.setReconnectInterval(10000);           // printer offline -> stop hammering it
    ws_.enableHeartbeat(15000, 3000, 2);
}

void CrealityBackend::loop() {
    ws_.loop();
    if (connected_ && millis() - lastReq_ > 5000) { lastReq_ = millis(); refresh(); }
}

void CrealityBackend::stop() {
    ws_.disconnect();
    connected_ = false;
    status_ = "K2: parado";
}

bool CrealityBackend::connected() { return connected_; }
String CrealityBackend::status()  { return status_; }
const char* CrealityBackend::slotLabel(int i) {
    const char* n = CREALITY_SLOTS[i < 5 ? i : 0].name;
    return n ? n : i18n::T(S_HOLDER);
}
const SlotState& CrealityBackend::slot(int i) { return slots_[i < 5 ? i : 0]; }

void CrealityBackend::refresh() {
    // Not straight after a write. The printer answers boxsInfo with the slot's
    // OLD contents for about a second after modifyMaterial, so an immediate
    // re-read paints the old spool back and the slot flickers - the TigerTag
    // Connect app leaves the same gap on purpose. The request is moved to a
    // second and a half after the write instead of dropped: loop() polls when
    // lastReq_ is five seconds old.
    if (assignedAt_ && millis() - assignedAt_ < REREAD_AFTER_ASSIGN_MS) {
        lastReq_ = assignedAt_ + REREAD_AFTER_ASSIGN_MS - 5000;
        return;
    }
    JsonDocument d;
    d["method"] = "get";
    d["params"]["boxsInfo"] = 1;
    sendDoc(d);
}

bool CrealityBackend::assign(int idx, const TagInfo& t) {
    if (idx < 0 || idx >= 5) return false;

    // Temperatures, the printer's own material id, pressure advance and the
    // name, each from the first source that has it: the TigerTag+ product
    // endpoint, the chip, the material table, a default. The endpoint was
    // asked when the spool was read; this only reads what came back.
    const filament::ResolvedFilament f = product_api::resolveFor(t);
    Serial.printf("[creality] %s type=%s(%s) rfid=%s(%s) temp=%u/%u(%s) pa=%g(%s) name=\"%s\"(%s)\n",
                  slotLabel(idx), f.materialType, filament::sourceName(f.typeSrc),
                  f.crealityId, filament::sourceName(f.idSrc),
                  f.nozMin, f.nozMax, filament::sourceName(f.tempSrc),
                  f.pressure, filament::sourceName(f.pressureSrc),
                  f.crealityName, filament::sourceName(f.nameSrc));

    // The frame the TigerTag Connect app sends, field for field.
    //
    // rfid is the printer's material id, and "0" is not one: sent that way the
    // printer never recognised the filament. editStatus 1 marks the write as an
    // application's - without it some firmware overwrites the slot again.
    JsonDocument d;
    d["method"] = "set";
    JsonObject m = d["params"]["modifyMaterial"].to<JsonObject>();
    m["id"]         = CREALITY_SLOTS[idx].slot;
    m["boxId"]      = CREALITY_SLOTS[idx].box;
    m["rfid"]       = f.crealityId;
    // The material family from the table ("PLA" for "PLA High Speed"): a
    // Creality slot's type is a family, and the label is not one.
    m["type"]       = f.materialType;
    m["vendor"]     = t.brand;
    m["name"]       = f.crealityName;
    m["color"]      = t.colorHexCreality();
    // With a decimal point, or the printer drops them - see withPoint().
    m["minTemp"]    = serialized(withPoint(f.nozMin));
    m["maxTemp"]    = serialized(withPoint(f.nozMax));
    m["pressure"]   = serialized(withPoint(f.pressure));
    m["selected"]   = 1;
    m["percent"]    = 100;
    m["editStatus"] = 1;
    m["state"]      = 1;
    bool ok = sendDoc(d);
    // slotLabel, not the raw name: slot 0 is the external holder and its name
    // is deliberately null, so concatenating it here produced a String that
    // Arduino invalidates - an empty status line where a report should be.
    status_ = ok ? (String("sent -> ") + slotLabel(idx)) : "send failed";
    if (ok) assignedAt_ = millis();
    return ok;
}
