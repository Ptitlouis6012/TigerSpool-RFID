#include "tls.h"

// The root CA store the Arduino core embeds and links into every build. The
// symbol is emitted by the core's own build, so nothing is added to this
// repository and nothing has to be kept up to date by hand.
extern const uint8_t rootca_crt_bundle_start[] asm("_binary_x509_crt_bundle_start");

namespace tls {

void secure(WiFiClientSecure& client) {
    client.setCACertBundle(rootca_crt_bundle_start);
    // Long enough for a handshake on a slow uplink, short enough that a screen
    // waiting on it does not look wedged.
    client.setHandshakeTimeout(15);
}

}  // namespace tls
