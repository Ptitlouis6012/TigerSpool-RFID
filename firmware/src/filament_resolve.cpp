#include "filament_resolve.h"

#include <ArduinoJson.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace filament {
namespace {

// A string field counts when it says something. The endpoint and the table use
// all three of these for "nothing".
bool given(const char* s) {
    return s && *s && strcmp(s, "-") != 0;
}

// A temperature pair counts only whole and in order. A min from one source
// beside a max from another describes no filament anybody made, so a pair is
// taken or passed over together.
bool validPair(uint16_t mn, uint16_t mx) {
    return mn > 0 && mx > 0 && mn <= mx;
}

// A number, or a string holding nothing but a number. The endpoint sends
// crealityPressureAdvance as a STRING, usually "": reading it as a number only
// would treat every real value it ever sends as absent.
double positiveNumber(JsonVariantConst v) {
    if (v.is<double>()) {
        const double d = v.as<double>();
        return d > 0 ? d : 0;
    }
    if (v.is<const char*>()) {
        const char* s = v.as<const char*>();
        if (!given(s)) return 0;
        char* end = nullptr;
        const double d = strtod(s, &end);
        while (end && (*end == ' ')) end++;
        if (!end || *end != '\0') return 0;      // "0.04abc" is not a number
        return d > 0 ? d : 0;
    }
    return 0;
}

uint16_t temperature(JsonVariantConst v) {
    const double d = positiveNumber(v);
    return (d >= 1 && d <= 65535) ? (uint16_t)d : 0;
}

void copyGiven(char* dst, size_t cap, JsonVariantConst v) {
    dst[0] = '\0';
    if (!v.is<const char*>()) return;            // null, a number, missing
    const char* s = v.as<const char*>();
    if (given(s)) snprintf(dst, cap, "%s", s);
}

}  // namespace

const char* sourceName(Source s) {
    switch (s) {
        case SRC_API:  return "api";
        case SRC_CHIP: return "chip";
        case SRC_DB:   return "db";
        default:       return "default";
    }
}

bool parseProduct(const char* json, size_t len, uint32_t expectedId, ProductData& out) {
    out = ProductData{};
    if (!json || !len) return false;

    // Only the six fields used. The full answer carries descriptions, image
    // lists and links that have no business in RAM on this device.
    JsonDocument filter;
    filter["id"] = true;
    filter["nozzle"]["temp_min"] = true;
    filter["nozzle"]["temp_max"] = true;
    filter["metadata"]["crealityID"] = true;
    filter["metadata"]["crealityLabel"] = true;
    filter["metadata"]["crealityPressureAdvance"] = true;

    JsonDocument doc;
    if (deserializeJson(doc, json, len, DeserializationOption::Filter(filter))) return false;
    if (!doc.is<JsonObject>()) return false;

    // The answer must be about the product asked for. An error object has no
    // id at all, and an id that differs is somebody else's filament.
    JsonVariantConst id = doc["id"];
    long long got = -1;
    if (id.is<long long>()) got = id.as<long long>();
    else if (id.is<const char*>()) {
        char* end = nullptr;
        const char* s = id.as<const char*>();
        got = strtoll(s, &end, 10);
        if (!end || end == s || *end) got = -1;
    }
    if (got < 0 || (uint64_t)got != expectedId) return false;
    out.id = expectedId;

    out.nozMin = temperature(doc["nozzle"]["temp_min"]);
    out.nozMax = temperature(doc["nozzle"]["temp_max"]);
    copyGiven(out.crealityId, sizeof(out.crealityId), doc["metadata"]["crealityID"]);
    copyGiven(out.crealityLabel, sizeof(out.crealityLabel), doc["metadata"]["crealityLabel"]);
    out.pressure = positiveNumber(doc["metadata"]["crealityPressureAdvance"]);
    return true;
}

ResolvedFilament resolve(const ChipFacts& chip, const ProductData* api, const MaterialInfo* db) {
    ResolvedFilament r;

    // The endpoint only ever speaks for a TigerTag+, and only for its own
    // product. A plain TigerTag never asks it, and an answer handed in anyway
    // is not listened to.
    const ProductData* a =
        (chip.protocol == PROTOCOL_TIGERTAG_PLUS && api && api->id == chip.idProduct) ? api : nullptr;
    const MaterialInfo* d = (db && db->id == chip.idMaterial) ? db : nullptr;

    // Temperatures: endpoint, chip, table, default. For a plain TigerTag the
    // first step is simply not there.
    if (a && validPair(a->nozMin, a->nozMax)) {
        r.nozMin = a->nozMin; r.nozMax = a->nozMax; r.tempSrc = SRC_API;
    } else if (validPair(chip.nozMin, chip.nozMax)) {
        r.nozMin = chip.nozMin; r.nozMax = chip.nozMax; r.tempSrc = SRC_CHIP;
    } else if (d && validPair(d->nozMin, d->nozMax)) {
        r.nozMin = d->nozMin; r.nozMax = d->nozMax; r.tempSrc = SRC_DB;
    } else {
        r.nozMin = DEFAULT_NOZ_MIN; r.nozMax = DEFAULT_NOZ_MAX; r.tempSrc = SRC_DEFAULT;
    }

    // The material FAMILY, which is what a Creality slot's type means: "PLA"
    // for a spool whose material is "PLA High Speed". The table has it as
    // material_type; without it, the material's own label, as before.
    if (d && given(d->materialType)) {
        snprintf(r.materialType, sizeof(r.materialType), "%s", d->materialType); r.typeSrc = SRC_DB;
    } else {
        snprintf(r.materialType, sizeof(r.materialType), "%s", chip.material ? chip.material : "");
        r.typeSrc = SRC_DEFAULT;
    }

    // The printer's material id: endpoint, table, "0". The chip has none - it
    // carries the material id that is the table's key, nothing more.
    if (a && given(a->crealityId)) {
        snprintf(r.crealityId, sizeof(r.crealityId), "%s", a->crealityId); r.idSrc = SRC_API;
    } else if (d && given(d->crealityId)) {
        snprintf(r.crealityId, sizeof(r.crealityId), "%s", d->crealityId); r.idSrc = SRC_DB;
    } else {
        snprintf(r.crealityId, sizeof(r.crealityId), "%s", DEFAULT_CREALITY_ID); r.idSrc = SRC_DEFAULT;
    }

    // Pressure advance: the same order as the id.
    if (a && a->pressure > 0) {
        r.pressure = a->pressure; r.pressureSrc = SRC_API;
    } else if (d && d->pressure > 0) {
        r.pressure = d->pressure; r.pressureSrc = SRC_DB;
    } else {
        r.pressure = DEFAULT_PRESSURE; r.pressureSrc = SRC_DEFAULT;
    }

    // The name the printer shows: the endpoint's Creality label, or a generic
    // one built from the material.
    if (a && given(a->crealityLabel)) {
        snprintf(r.crealityName, sizeof(r.crealityName), "%s", a->crealityLabel); r.nameSrc = SRC_API;
    } else {
        snprintf(r.crealityName, sizeof(r.crealityName), "Generic %s",
                 chip.material ? chip.material : "");
        r.nameSrc = SRC_DEFAULT;
    }
    return r;
}

}  // namespace filament
