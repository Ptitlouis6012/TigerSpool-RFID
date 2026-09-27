#pragma once
#include <Arduino.h>
#include <functional>

// One TLS session for every Anycubic printer an account has in cloud mode.
//
// WHY ONE. Anycubic's cloud broker fixes the MQTT client id per ACCOUNT -
// md5(email + "pcf") - and refuses any other. Two sessions for one account
// would kick each other off in turn, so there cannot be one per printer even if
// the memory allowed it. Every cloud printer on the account rides this one
// session; its reports are routed by topic, exactly as bambu_cloud does.
//
// WHAT IT NEEDS. The account document Tiger Studio writes for a cloud printer:
// the email and the workbench token of the Anycubic account, the printer's
// broker key and its machine type. The session signs in the way the slicer does
// - the token encrypted to the broker's CA key, a signed username, and the
// shared Anycubic client certificate for mutual TLS. That certificate is not in
// this repository either: Tiger Studio writes it on the cloud printer's account
// document (acuCloudCaDerB64, acuCloudClientCertPem, acuCloudClientKeyPem) and
// the sync passes it to setIdentity(), the email and token to setAccount().
//
// NEXT TO TIGER STUDIO. The broker allows one session per account AND per
// device type, and the newest takes the place of an older one. Tiger Studio and
// the slicer sign in as "pcf"; this device signs in as "web", so the two stay up
// side by side (measured on the bench, both directions). Slots are read and set
// over this session with the LAN's own getInfo / setInfo messages: no REST call,
// so no second TLS session - a second one beside this is what ran the device out
// of memory on the bench.
namespace anycubic_cloud {

// Called with the raw bytes of one multiColorBox report for that printer.
using Sink = std::function<void(uint8_t* payload, unsigned len)>;

// A cloud-mode backend joins with its broker key and machine type, and brings
// the account's credentials. The first one opens the session; the last one to
// leave closes it. Returns false if the table is full.
bool attach(const String& key, const String& machineType,
            const String& email, const String& token, Sink sink);
void detach(const String& key);

// Connect, reconnect and pump. Idempotent; reconnects are rate-limited here.
void loop();

// True when the session is up AND this printer's subscription is in place.
bool connected(const String& key);

// Publish a multiColorBox command to that printer over the shared session.
bool publish(const String& key, const String& body);

// Whether a session object exists - what makes a second cloud printer cheap.
bool active();

// The shared client identity, from the account document, in RAM. A change makes
// the next connect use it.
void setIdentity(const String& caDerB64, const String& certPem, const String& keyPem);

// The Anycubic account's email and workbench token, from the account document.
// Held in RAM and nowhere else - see the note in anycubic_cloud.cpp.
void setAccount(const String& email, const String& token);

// Why the session is not up, in a line fit for a status: missing certificates,
// a token the broker refused, or simply "connecting".
String why();

}  // namespace anycubic_cloud
