#include "anycubic_cloud.h"
#include <WiFi.h>
#include "net/legacy_tls_client.h"
#include <PubSubClient.h>
#include <mbedtls/md.h>
#include <mbedtls/base64.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/pk.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>



namespace {

const char* BROKER = "mqtt-universe.anycubic.com";
const uint16_t PORT = 8883;

// The device type is part of both the client id and the username, and the
// broker allows one session per account AND per type: that is why Tiger Studio
// (which signs in as the slicer, "pcf") and the slicer's own Workbench kick each
// other. The phone app stays connected beside the slicer, so at least one other
// type is accepted with the same account. Measured from a desktop with Tiger
// Studio's token: "web", "android" and "ios" are accepted, "app" is refused, and
// a "web" session stays up beside a "pcf" one for as long as both run - so this
// device signs in as "web" and never takes Tiger Studio's session away. "pcf" is
// the fallback: it always works, and it is the one that collides.
const char* const TYPES[] = { "web", "pcf" };
const int NTYPES = sizeof(TYPES) / sizeof(TYPES[0]);
int s_type = 0;                   // index into TYPES; kept once one is accepted
bool s_typeFound = false;

// An account rarely has more than a couple of cloud Anycubics; a ninth is
// refused rather than silently lost, as on the Bambu side.
const int MAX_SUBS = 8;

struct Sub {
    String key;
    String mt;
    String topic;           // what is subscribed: ...v1/+/public/<mt>/<key>/#
    String prefix;          // the /public/<mt>/<key>/ part every report carries
    anycubic_cloud::Sink sink;
    bool   subscribed = false;
};

Sub  s_subs[MAX_SUBS];
int  s_n = 0;

LegacyTlsClient*  s_net  = nullptr;
PubSubClient*     s_mqtt = nullptr;

uint32_t s_lastTry = 0;
bool     s_wasUp = false;
String   s_email, s_token;        // the account the session signs in as
String   s_cid, s_user, s_pass;   // derived from them, rebuilt when they change
String   s_why = "Anycubic Cloud: connecting...";

// The shared Anycubic client identity - the certificate and key the broker
// wants for mutual TLS, and the CA whose public key the token is encrypted to -
// and the account's email and token. Tiger Studio writes them all on every
// cloud printer's account document and the sync hands them here.
//
// RAM ONLY, deliberately. The NVS partition is nearly full and fragmented (see
// g_docIds in tigertag_cloud.cpp): a 238-character token written to a printer's
// positional key failed without a word and left the LAN printer's 15-character
// password in its place, which the broker then refused. The sync runs on every
// boot, so nothing is lost by not storing them. Never logged.
String   s_ca, s_crt, s_key;
String   s_caPem;                 // s_ca as PEM, the broker's trust anchor
String   s_acctEmail, s_acctToken;

bool haveIdentity() {
    return s_ca.length() && s_crt.length() && s_key.length();
}

String md5hex(const String& s) {
    uint8_t out[16];
    mbedtls_md(mbedtls_md_info_from_type(MBEDTLS_MD_MD5),
               (const uint8_t*)s.c_str(), s.length(), out);
    char hex[33];
    for (int i = 0; i < 16; i++) sprintf(hex + 2 * i, "%02x", out[i]);
    return String(hex);
}

// The broker password is the workbench token, RSA-encrypted (PKCS#1 v1.5) to
// the public key of Anycubic's CA and base64-encoded - what the slicer sends.
// PKCS#1 padding is random, so the password differs on every connect; the
// broker decrypts it, so that does not matter.
bool encryptToken(const String& token, String& out) {
    size_t derLen = 0;
    const char* b64 = s_ca.c_str();
    mbedtls_base64_decode(nullptr, 0, &derLen, (const uint8_t*)b64, strlen(b64));
    uint8_t* der = (uint8_t*)malloc(derLen);
    if (!der) return false;
    bool ok = false;
    mbedtls_x509_crt ca;       mbedtls_x509_crt_init(&ca);
    mbedtls_entropy_context e; mbedtls_entropy_init(&e);
    mbedtls_ctr_drbg_context d; mbedtls_ctr_drbg_init(&d);
    uint8_t cipher[512];
    size_t clen = 0;
    if (mbedtls_base64_decode(der, derLen, &derLen, (const uint8_t*)b64, strlen(b64)) == 0 &&
        mbedtls_x509_crt_parse_der(&ca, der, derLen) == 0 &&
        mbedtls_ctr_drbg_seed(&d, mbedtls_entropy_func, &e, (const uint8_t*)"acu", 3) == 0 &&
        mbedtls_pk_encrypt(&ca.pk, (const uint8_t*)token.c_str(), token.length(),
                           cipher, &clen, sizeof(cipher), mbedtls_ctr_drbg_random, &d) == 0) {
        size_t olen = 0;
        mbedtls_base64_encode(nullptr, 0, &olen, cipher, clen);
        char* enc = (char*)malloc(olen + 1);
        if (enc && mbedtls_base64_encode((uint8_t*)enc, olen + 1, &olen, cipher, clen) == 0) {
            enc[olen] = 0;
            out = enc;
            ok = true;
        }
        free(enc);
    }
    mbedtls_ctr_drbg_free(&d); mbedtls_entropy_free(&e); mbedtls_x509_crt_free(&ca);
    free(der);
    return ok;
}

// The three login fields, from the account's email and token.
//   client id  md5(email + <type>)   - one per account and type; enforced
//   password   base64(RSA(token))
//   username   user|<type>|<email>|md5(clientId + password + clientId)
bool buildLogin() {
    const String type = TYPES[s_type];
    s_cid = md5hex(s_email + type);
    if (!encryptToken(s_token, s_pass)) return false;
    s_user = String("user|") + type + "|" + s_email + "|" + md5hex(s_cid + s_pass + s_cid);
    return true;
}

// The broker's certificate is issued by Anycubic's own "AC Root CA" - the same
// CA whose key the token is encrypted to - not by a public authority, so that
// CA is the trust anchor and the root store is not involved. It is signed with
// SHA-1, which is why the session goes through LegacyTlsClient.
void applyIdentity() {
    if (!s_net || !haveIdentity()) return;
    s_caPem = "-----BEGIN CERTIFICATE-----\n";
    for (size_t i = 0; i < s_ca.length(); i += 64) s_caPem += s_ca.substring(i, i + 64) + "\n";
    s_caPem += "-----END CERTIFICATE-----\n";
    s_net->setIdentity(s_caPem.c_str(), s_crt.c_str(), s_key.c_str());
}

void onMessage(char* topic, uint8_t* payload, unsigned int len) {
    // Strictly by topic, as bambu_cloud does: a report handed to the wrong
    // printer would show another machine's spools as its own.
    // Everything for one printer arrives under /public/<mt>/<key>/; only the
    // multiColorBox reports go to the backend, and the rest - print progress,
    // temperatures - is dropped here. Nothing is copied or queued: every
    // message lands in PubSubClient's one buffer and is gone when this returns.
    const char* mcb = strstr(topic, "/multiColorBox/report");
    for (int i = 0; i < s_n; i++) {
        if (!strstr(topic, s_subs[i].prefix.c_str())) continue;
        if (mcb) s_subs[i].sink(payload, len);
        return;
    }
}

void subscribeAll() {
    for (int i = 0; i < s_n; i++)
        s_subs[i].subscribed = s_mqtt->subscribe(s_subs[i].topic.c_str());
}

void open() {
    if (s_mqtt) return;
    s_net  = new LegacyTlsClient();
    s_mqtt = new PubSubClient(*s_net);
    applyIdentity();
    s_net->setHandshakeTimeout(8);
    // A layout report from a Kobra X with its units is a few kilobytes; it
    // lands in PSRAM like every buffer over 4 KB on this build.
    s_mqtt->setBufferSize(32768);
    s_mqtt->setKeepAlive(30);
    s_mqtt->setSocketTimeout(6);
    s_mqtt->setServer(BROKER, PORT);
    s_mqtt->setCallback(onMessage);
    s_lastTry = 0;
    s_wasUp = false;
    Serial.println("[acu-cloud] session object created");
}

void close() {
    if (!s_mqtt) return;
    s_mqtt->disconnect();
    delete s_mqtt; s_mqtt = nullptr;
    delete s_net;  s_net  = nullptr;
    s_wasUp = false;
    s_email = s_token = s_cid = s_user = s_pass = String();
    Serial.println("[acu-cloud] last printer left - session closed");
}

}  // namespace

bool anycubic_cloud::attach(const String& key, const String& mt,
                            const String& email, const String& token, Sink sink) {
    // Every cloud printer of one account carries the same email and token.
    // A different token means Tiger Studio refreshed it: the password is
    // derived from it, so the session has to sign in again.
    // The account's credentials come from setAccount(); the backend's own copy
    // is only a fallback.
    const String e = s_acctEmail.length() ? s_acctEmail : email;
    const String t = s_acctToken.length() ? s_acctToken : token;
    if (e.length() && t.length() && (e != s_email || t != s_token)) {
        s_email = e; s_token = t; s_cid = String();
        if (s_mqtt && s_mqtt->connected()) s_mqtt->disconnect();
    }
    for (int i = 0; i < s_n; i++)
        if (s_subs[i].key == key) { s_subs[i].sink = sink; return true; }
    if (s_n >= MAX_SUBS) {
        Serial.printf("[acu-cloud] table full - %s not attached\n", key.c_str());
        return false;
    }
    Sub& s = s_subs[s_n++];
    s.key = key;
    s.mt  = mt;
    // The wildcard Tiger Studio subscribes to. The report's first segment is
    // not always "printer", so a narrower topic missed the layout entirely.
    s.topic  = String("anycubic/anycubicCloud/v1/+/public/") + mt + "/" + key + "/#";
    s.prefix = String("/public/") + mt + "/" + key + "/";
    s.sink = sink;
    s.subscribed = false;
    open();
    if (s_mqtt->connected()) s.subscribed = s_mqtt->subscribe(s.topic.c_str());
    Serial.printf("[acu-cloud] %s attached (%d on the session)\n", key.c_str(), s_n);
    return true;
}

void anycubic_cloud::detach(const String& key) {
    for (int i = 0; i < s_n; i++) {
        if (s_subs[i].key != key) continue;
        if (s_mqtt && s_mqtt->connected()) s_mqtt->unsubscribe(s_subs[i].topic.c_str());
        for (int j = i; j < s_n - 1; j++) s_subs[j] = s_subs[j + 1];
        s_subs[--s_n] = Sub();
        Serial.printf("[acu-cloud] %s detached (%d left)\n", key.c_str(), s_n);
        break;
    }
    if (s_n == 0) close();
}

void anycubic_cloud::loop() {
    if (!s_mqtt) return;
    if (s_mqtt->connected()) {
        s_mqtt->loop();
        return;
    }
    if (s_wasUp) {
        // The usual reason is not the network: the broker keeps one session per
        // account, and Tiger Studio or the slicer's Workbench just took it.
        Serial.printf("[acu-cloud] session lost, state %d\n", s_mqtt->state());
        s_wasUp = false;
        for (int i = 0; i < s_n; i++) s_subs[i].subscribed = false;
    }
    if (millis() - s_lastTry < 5000) return;
    s_lastTry = millis();

    if (!haveIdentity()) {
        // Normal until the first account sync of the boot; a sync that brought
        // none means Tiger Studio is older than the fields it has to write.
        if (s_why.indexOf("certificate") < 0)
            Serial.println("[acu-cloud] no client certificate yet - it arrives with the account "
                           "sync (Tiger Studio writes it on the cloud printer)");
        s_why = "Anycubic Cloud: waiting for the client certificate from the account";
        return;
    }
    if (s_email.isEmpty() || s_token.isEmpty()) {
        s_why = "Anycubic Cloud: no token in the account - add the printer in Tiger Studio";
        return;
    }
    if (s_cid.isEmpty() && !buildLogin()) {
        s_why = "Anycubic Cloud: could not sign the login";
        Serial.println("[acu-cloud] login fields could not be built");
        return;
    }
    Serial.printf("[acu-cloud] connecting to %s as '%s' for %d printer(s)... heap=%u\n",
                  BROKER, TYPES[s_type], s_n, (unsigned)ESP.getFreeHeap());
    if (s_mqtt->connect(s_cid.c_str(), s_user.c_str(), s_pass.c_str())) {
        subscribeAll();
        s_wasUp = true;
        s_typeFound = true;
        s_why = "Anycubic Cloud: connected";
        Serial.printf("[acu-cloud] up as '%s' - %d subscription(s)\n", TYPES[s_type], s_n);
    } else {
        const int st = s_mqtt->state();
        // Refused credentials while still looking for a type of our own: try
        // the next one before concluding the token is at fault.
        if ((st == 4 || st == 5) && !s_typeFound && s_type < NTYPES - 1) {
            Serial.printf("[acu-cloud] '%s' refused (state %d) - trying '%s'\n",
                          TYPES[s_type], st, TYPES[s_type + 1]);
            s_type++;
            s_cid = String();
            s_lastTry = 0;
            return;
        }
        // Every type refused: the token is the likely cause. Start again from
        // the first type, so a refreshed token is tried the way Studio signs in.
        if ((st == 4 || st == 5) && !s_typeFound) s_type = 0;
        // 4 and 5 are the broker refusing the credentials: the token was
        // revoked by a newer slicer sign-in. Only Tiger Studio can fetch a new
        // one; the next account sync brings it here.
        s_why = (st == 4 || st == 5)
              ? "Anycubic Cloud: sign-in expired - refresh the printer in Tiger Studio"
              : String("Anycubic Cloud: connect failed, state ") + st;
        if (st == -2) {
            char err[120] = "";
            const int e = s_net->lastError(err, sizeof(err));
            Serial.printf("[acu-cloud] TLS: %d %s\n", e, err);
        }
        // A fresh encryption next time, in case the refusal was the password.
        s_cid = String();
        Serial.println(s_why);
    }
}

bool anycubic_cloud::connected(const String& key) {
    if (!s_mqtt || !s_mqtt->connected()) return false;
    for (int i = 0; i < s_n; i++)
        if (s_subs[i].key == key) return s_subs[i].subscribed;
    return false;
}

bool anycubic_cloud::publish(const String& key, const String& body) {
    if (!s_mqtt || !s_mqtt->connected()) return false;
    for (int i = 0; i < s_n; i++) {
        if (s_subs[i].key != key) continue;
        const String t = String("anycubic/anycubicCloud/v1/web/printer/") + s_subs[i].mt +
                         "/" + key + "/multiColorBox";
        return s_mqtt->publish(t.c_str(), body.c_str());
    }
    return false;
}

bool anycubic_cloud::active() { return s_mqtt != nullptr; }

String anycubic_cloud::why() { return s_why; }

void anycubic_cloud::setIdentity(const String& caDerB64, const String& certPem,
                                 const String& keyPem) {
    if (caDerB64.isEmpty() || certPem.isEmpty() || keyPem.isEmpty()) return;
    if (caDerB64 == s_ca && certPem == s_crt && keyPem == s_key) return;
    s_ca = caDerB64; s_crt = certPem; s_key = keyPem;
    Serial.println("[acu-cloud] client identity updated from the account");
    // A session already built holds the old pointers; the next connect rebuilds.
    if (s_mqtt) { s_mqtt->disconnect(); applyIdentity(); }
    s_cid = String();
}

void anycubic_cloud::setAccount(const String& email, const String& token) {
    if (email.isEmpty() || token.isEmpty()) return;
    if (email == s_acctEmail && token == s_acctToken) return;
    s_acctEmail = email; s_acctToken = token;
    Serial.println("[acu-cloud] account token updated from the account");
    // The password derives from the token: sign in again with the new one.
    if (s_email.length() && (email != s_email || token != s_token)) {
        s_email = email; s_token = token; s_cid = String();
        if (s_mqtt && s_mqtt->connected()) s_mqtt->disconnect();
    }
}
