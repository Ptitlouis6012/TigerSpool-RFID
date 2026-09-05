// The DNS side of the captive portal: every name resolves to this device.
//
// This replaces the core's DNSServer, which cannot be used here for one
// specific reason. Its replyWithIP() writes an answer of type A whatever type
// was asked for, so a query for an AAAA record comes back with a question
// section saying AAAA and an answer section holding four bytes of IPv4. That
// is not a valid response, and a resolver is entitled to throw it away.
//
// Android queries A and AAAA in parallel and waits for both. The A answer
// arrives; the AAAA answer is discarded as malformed and the lookup sits there
// until it times out. Android's captive-portal probe has a short deadline, so
// the probe fails even though the portal was reachable the whole time - which
// is exactly the symptom reported from the field, on firmware where the access
// point was already up, encrypted, and serving the page.
//
// iOS is unaffected because its resolver is more forgiving and its probe
// retries. That asymmetry is what made this hard to see.
//
// What this does instead: A queries get the device's address, and every other
// type gets NOERROR with no answers, which is the correct way to say "this
// name exists, it has no record of that type". The client then uses the A
// record it already has.
#pragma once
#include <Arduino.h>
#include <IPAddress.h>

namespace captive_dns {

void begin(const IPAddress& ip);   // starts listening on port 53
void loop();                       // call from the main loop
void end();

}  // namespace captive_dns
