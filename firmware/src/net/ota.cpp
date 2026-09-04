#include "ota.h"
#include "version.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "tls.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Update.h>
#include <mbedtls/sha256.h>
#include <algorithm>

// Declared rather than included. <esp_ota_ops.h> drags in FreeRTOSConfig.h and
// a chain of ESP-IDF headers whose include paths differ between Arduino core
// generations, and this file needs exactly one symbol from it. A prototype
// costs nothing and does not care which layout the SDK happens to have.
extern "C" int esp_ota_mark_app_valid_cancel_rollback(void);

namespace {

// The published manifest. One file, generated at release time, never committed:
// a committed copy and a generated one drift, and the drift is invisible until
// a fleet acts on it. See docs/OTA.md.
const char* MANIFEST_URL =
    "https://tigertag-project.github.io/TigerSpool-RFID/version.json";

// A TLS record can genuinely gap for several seconds on a busy device. Ten was
// too aggressive in the sibling project: the abort looked like a network fault
// when it was impatience.
constexpr uint32_t STALL_MS = 30000;

volatile ota::State s_state   = ota::IDLE;
volatile int        s_percent = 0;
char                s_latest[16]  = "";
char                s_message[64] = "";
String              s_url;
String              s_sha;
volatile bool       s_busy = false;

void fail(const char* why) {
    snprintf(s_message, sizeof(s_message), "%s", why);
    s_state = ota::FAILED;
    Serial.printf("[ota] %s\n", why);
}

String sha256Hex(const uint8_t b[32]) {
    char out[65];
    for (int i = 0; i < 32; i++) snprintf(out + i * 2, 3, "%02x", b[i]);
    return String(out);
}

// "1.2.3" against "1.10.0" — compared field by field, because a string compare
// puts 1.10.0 before 1.2.0 and would silently stop offering updates after the
// ninth minor release.
bool newerThan(const char* candidate, const char* running) {
    int a[3] = {0, 0, 0}, b[3] = {0, 0, 0};
    sscanf(candidate, "%d.%d.%d", &a[0], &a[1], &a[2]);
    sscanf(running,   "%d.%d.%d", &b[0], &b[1], &b[2]);
    for (int i = 0; i < 3; i++) {
        if (a[i] != b[i]) return a[i] > b[i];
    }
    return false;
}

bool fetchManifest() {
    if (!WiFi.isConnected()) { fail("no network"); return false; }

    WiFiClientSecure client;
    tls::secure(client);
    client.setTimeout(15);

    HTTPClient http;
    http.setTimeout(15000);
    http.setReuse(false);
    if (!http.begin(client, MANIFEST_URL)) { fail("cannot reach the manifest"); return false; }
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    int code = http.GET();
    String body = http.getString();
    http.end();
    if (code != 200) {
        char m[64];
        snprintf(m, sizeof(m), "manifest HTTP %d", code);
        fail(m);
        return false;
    }

    // Parse through a FILTER rather than into a document sized for the whole
    // file. The manifest carries more than this device needs - installer
    // offsets, per-asset entries, release links - and it will keep growing. A
    // fixed document over the whole thing eventually fails with NoMemory, which
    // surfaces as "manifest unreachable" and makes a parser fault look like a
    // network one. With a filter, only the three fields below are ever
    // allocated, so the manifest can grow without limit and an older device
    // keeps reading it.
    StaticJsonDocument<192> filter;
    filter["version"]         = true;
    filter["firmware_url"]    = true;
    filter["firmware_sha256"] = true;

    StaticJsonDocument<512> doc;
    if (deserializeJson(doc, body, DeserializationOption::Filter(filter))) {
        fail("manifest is not valid JSON");
        return false;
    }

    String v   = doc["version"]         | "";
    String url = doc["firmware_url"]    | "";
    String sha = doc["firmware_sha256"] | "";
    if (v.isEmpty() || url.isEmpty()) { fail("manifest is missing version or url"); return false; }

    snprintf(s_latest, sizeof(s_latest), "%s", v.c_str());
    s_url = url;
    s_sha = sha;
    Serial.printf("[ota] published %s, running %s\n", s_latest, TIGERSPOOL_FW_VERSION);
    return true;
}

bool download() {
    if (s_url.isEmpty()) { fail("nothing to install"); return false; }
    if (!ota::slotAvailable()) { fail("no spare app slot"); return false; }

    // What a TLS handshake needs is the largest CONTIGUOUS block, not the total
    // free heap: mbedTLS wants one big buffer. A second attempt in the same
    // session can fail here with plenty of heap left, which is fragmentation
    // rather than exhaustion - so log both numbers.
    Serial.printf("[ota] heap before connect: free=%u largest=%u\n",
                  (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());

    WiFiClientSecure client;
    tls::secure(client);
    client.setTimeout(15);

    HTTPClient http;
    http.setTimeout(15000);
    http.setReuse(false);
    if (!http.begin(client, s_url)) { fail("cannot reach the firmware"); return false; }
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    int code = http.GET();
    if (code != 200) {
        char m[64]; snprintf(m, sizeof(m), "firmware HTTP %d", code);
        fail(m); http.end(); return false;
    }

    int total = http.getSize();
    if (total <= 0) { fail("firmware has no length"); http.end(); return false; }

    if (!Update.begin(total, U_FLASH)) {
        char m[64];
        snprintf(m, sizeof(m), "need %d bytes, slot has %d",
                 total, (int)ESP.getFreeSketchSpace());
        fail(m); http.end(); return false;
    }

    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);

    WiFiClient* stream = http.getStreamPtr();
    uint8_t buf[1024];
    int written = 0;
    uint32_t lastByte = millis();

    auto abort = [&](const char* why) {
        Update.abort();
        mbedtls_sha256_free(&ctx);
        http.end();
        fail(why);
    };

    while (http.connected() && written < total) {
        size_t avail = stream->available();
        if (avail > 0) {
            int n = stream->readBytes(buf, std::min(avail, sizeof(buf)));
            if (n <= 0) break;
            mbedtls_sha256_update(&ctx, buf, n);
            if (Update.write(buf, n) != (size_t)n) { abort("write to the slot failed"); return false; }
            written += n;
            lastByte = millis();
            s_percent = (written * 100) / total;
        } else {
            if (millis() - lastByte > STALL_MS) {
                Serial.printf("[ota] silent for %lu s at %d/%d bytes (heap=%u largest=%u)\n",
                              (unsigned long)(STALL_MS / 1000), written, total,
                              (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
                abort("download stalled");
                return false;
            }
            delay(2);
        }
    }

    uint8_t digest[32];
    mbedtls_sha256_finish(&ctx, digest);
    mbedtls_sha256_free(&ctx);
    String got = sha256Hex(digest);
    http.end();

    if (written != total) { Update.abort(); fail("download was cut short"); return false; }

    // A missing hash is refused rather than skipped. The sibling project logs it
    // and installs anyway; here it would mean flashing whatever answered the URL
    // with no check at all, which is worse than not updating.
    if (s_sha.isEmpty()) { Update.abort(); fail("manifest carried no checksum"); return false; }
    if (!got.equalsIgnoreCase(s_sha)) {
        Serial.printf("[ota] sha mismatch\n  expected %s\n  got      %s\n",
                      s_sha.c_str(), got.c_str());
        Update.abort();
        fail("checksum did not match");
        return false;
    }

    if (!Update.end(true)) { fail(Update.errorString()); return false; }

    Serial.printf("[ota] slot written and verified, %d bytes\n", written);
    return true;
}

void checkTask(void*) {
    s_state = ota::CHECKING;
    if (fetchManifest()) {
        s_state = newerThan(s_latest, TIGERSPOOL_FW_VERSION) ? ota::AVAILABLE
                                                             : ota::UP_TO_DATE;
    }
    s_busy = false;
    vTaskDelete(nullptr);
}

void applyTask(void*) {
    s_state = ota::DOWNLOADING;
    s_percent = 0;
    if (download()) s_state = ota::DONE;
    s_busy = false;
    vTaskDelete(nullptr);
}

}  // namespace

namespace ota {

void begin() {
    // Confirm the running image. Without this the bootloader can keep treating
    // it as a candidate; with rollback enabled it would eventually revert to the
    // previous slot even though this one boots perfectly well.
    esp_ota_mark_app_valid_cancel_rollback();

    Serial.printf("[ota] version %s, spare slot %s (%u bytes)\n",
                  TIGERSPOOL_FW_VERSION,
                  slotAvailable() ? "available" : "MISSING",
                  (unsigned)ESP.getFreeSketchSpace());
}

// getFreeSketchSpace() reports the size of the partition Update would write
// into, which is the spare OTA slot - so a non-zero answer is exactly the
// question "is there somewhere to put a new image". Zero means the partition
// table has no second app slot, and no amount of retrying will help.
bool slotAvailable() { return ESP.getFreeSketchSpace() > 0; }

bool checkAsync() {
    if (s_busy || !WiFi.isConnected()) return false;
    s_busy = true;
    s_message[0] = 0;
    // 12 KB: a TLS handshake plus a small JSON parse. Measured against the
    // sibling project's manifest task, which runs in the same space.
    if (xTaskCreatePinnedToCore(checkTask, "otaCheck", 12288, nullptr, 1, nullptr, 1) != pdPASS) {
        s_busy = false;
        fail("could not start the check");
        return false;
    }
    return true;
}

bool applyAsync() {
    if (s_busy || s_state != AVAILABLE) return false;
    s_busy = true;
    s_message[0] = 0;
    // 16 KB: the same handshake, plus the streaming write and the hash context.
    if (xTaskCreatePinnedToCore(applyTask, "otaApply", 16384, nullptr, 1, nullptr, 1) != pdPASS) {
        s_busy = false;
        fail("could not start the download");
        return false;
    }
    return true;
}

State state()              { return s_state; }
int   percent()            { return s_percent; }
const char* latestVersion(){ return s_latest; }
const char* message()      { return s_message; }

}  // namespace ota
