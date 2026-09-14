#include "product_api.h"
#include "net/tls.h"
#include "tt_db.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

namespace {

const char* PRODUCT_URL = "https://api.tigertag.io/api:tigertag/product/get";

// A new TLS session needs one piece of about this size. Below it the
// handshake fails outright - so a request that starts short of it asks the
// background links to step aside (needsRoom), and the task waits a moment for
// that rather than spend its budget on a sure failure.
const uint32_t TLS_BLOCK = 40000;

// Anything bigger is not a product description and is not read.
const int MAX_BODY = 32768;

portMUX_TYPE g_mux = portMUX_INITIALIZER_UNLOCKED;
volatile bool g_busy = false;
volatile bool g_needRoom = false;
uint32_t g_reqProduct = 0;
uint64_t g_reqUid = 0;
uint32_t g_startedAt = 0;

// The last answer that counted, for the product it describes. One entry: the
// point is a spool put back on the reader, not a catalogue.
bool g_haveCache = false;
filament::ProductData g_cache;

void fetchTask(void*) {
    const uint32_t product = g_reqProduct;
    const uint64_t uid = g_reqUid;
    const uint32_t t0 = g_startedAt;

    // Room for the handshake, or give up within the budget.
    while (ESP.getMaxAllocHeap() < TLS_BLOCK && millis() - t0 < 1000) delay(50);

    char url[160];
    snprintf(url, sizeof(url), "%s?uid=%llu&product_id=%lu", PRODUCT_URL,
             (unsigned long long)uid, (unsigned long)product);

    int code = -100;
    String body;
    if (WiFi.status() == WL_CONNECTED) {
        WiFiClientSecure c;
        tls::secure(c);
        c.setHandshakeTimeout(3);                 // seconds
        HTTPClient h;
        if (h.begin(c, url)) {
            h.setConnectTimeout(2500);
            h.setTimeout(2500);
            code = h.GET();
            if (code == 200) {
                const int size = h.getSize();     // -1 when the server does not say
                if (size <= MAX_BODY) body = h.getString();
                else code = -3;
            }
            h.end();
        } else {
            code = -2;
        }
    }

    filament::ProductData data;
    const bool ok = code == 200 &&
                    filament::parseProduct(body.c_str(), body.length(), product, data);
    const uint32_t ms = millis() - t0;
    if (ok) {
        portENTER_CRITICAL(&g_mux);
        g_cache = data;
        g_haveCache = true;
        portEXIT_CRITICAL(&g_mux);
        Serial.printf("[product] %lu: %lu ms - nozzle %u/%u crealityID \"%s\" label \"%s\" pa %g%s\n",
                      (unsigned long)product, (unsigned long)ms, data.nozMin, data.nozMax,
                      data.crealityId, data.crealityLabel, data.pressure,
                      ms > product_api::BUDGET_MS ? " (after the budget: kept for next time)" : "");
    } else {
        // The heap is in the line because it is the likeliest reason a TLS
        // request fails on this device, and the one a log cannot otherwise show.
        Serial.printf("[product] %lu: no usable answer (HTTP %d, %u bytes, %lu ms)"
                      " heap %u maxblk %u\n",
                      (unsigned long)product, code, (unsigned)body.length(), (unsigned long)ms,
                      (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
    }
    g_busy = false;
    vTaskDelete(nullptr);
}

}  // namespace

namespace product_api {

bool request(const TagInfo& t) {
    if (t.protocol != filament::PROTOCOL_TIGERTAG_PLUS) return false;
    if (!t.idProduct) return false;
    {
        filament::ProductData have;
        if (cached(t.idProduct, have)) return false;
    }
    if (g_busy) {
        Serial.printf("[product] %lu: another fetch is running - resolving without\n",
                      (unsigned long)t.idProduct);
        return false;
    }
    if (WiFi.status() != WL_CONNECTED) return false;

    // The endpoint wants the chip's UID as a decimal integer - the seven bytes
    // read as one unsigned number - and refuses a request without it.
    char* end = nullptr;
    const uint64_t uid = strtoull(t.uid.c_str(), &end, 16);
    if (!uid || !end || *end) return false;

    g_reqProduct = t.idProduct;
    g_reqUid = uid;
    g_startedAt = millis();
    // Measured when the request starts, with a margin for what the loop
    // allocates meanwhile.
    g_needRoom = ESP.getMaxAllocHeap() < TLS_BLOCK + 8000;
    g_busy = true;
    // 16 KB and core 0, as tt_db's update: a TLS handshake and a JSON parse on
    // this stack, and the interface keeps core 1.
    if (xTaskCreatePinnedToCore(fetchTask, "product", 16384, nullptr, 1, nullptr, 0) != pdPASS) {
        g_busy = false;
        Serial.println("[product] xTaskCreate failed - resolving without");
        return false;
    }
    return true;
}

bool busy() { return g_busy; }
bool needsRoom() { return g_busy && g_needRoom; }

bool waiting(uint32_t idProduct) {
    return g_busy && g_reqProduct == idProduct && millis() - g_startedAt < BUDGET_MS;
}

bool cached(uint32_t idProduct, filament::ProductData& out) {
    bool hit = false;
    portENTER_CRITICAL(&g_mux);
    if (g_haveCache && g_cache.id == idProduct) { out = g_cache; hit = true; }
    portEXIT_CRITICAL(&g_mux);
    return hit;
}

filament::ResolvedFilament resolveFor(const TagInfo& t) {
    filament::ChipFacts chip;
    chip.protocol   = t.protocol;
    chip.idProduct  = t.idProduct;
    chip.idMaterial = t.idMaterial;
    chip.nozMin     = t.nozMin;
    chip.nozMax     = t.nozMax;
    chip.material   = t.material.c_str();

    filament::ProductData api;
    // Only a TigerTag+ is ever looked up; resolve() would ignore the rest anyway.
    const bool haveApi = t.protocol == filament::PROTOCOL_TIGERTAG_PLUS &&
                         cached(t.idProduct, api);
    MaterialInfo db;
    const bool haveDb = tt_db::materialInfo(t.idMaterial, db);
    return filament::resolve(chip, haveApi ? &api : nullptr, haveDb ? &db : nullptr);
}

}  // namespace product_api
