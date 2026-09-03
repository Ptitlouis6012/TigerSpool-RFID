#pragma once
#include <Arduino.h>

// One decoded TigerTag.
//
// Colour and temperatures come straight off the chip. Material and brand are
// numeric ids on the tag, resolved to names through the generated table in
// include/tigertag_db.h - so a scan needs no network.
struct TagInfo {
    bool     ok = false;
    uint32_t idProduct = 0;
    uint16_t idMaterial = 0, idBrand = 0;
    uint8_t  r = 0, g = 0, b = 0;
    uint16_t nozMin = 0, nozMax = 0;
    uint8_t  bedMin = 0, bedMax = 0;
    String   material;               // resolved label, e.g. "PETG"
    String   brand;                  // resolved label, e.g. "Polymaker"

    // Creality wants seven hex digits: a '0' after the '#', then RRGGBB.
    String colorHexCreality() const {
        char buf[10];
        snprintf(buf, sizeof(buf), "#0%02x%02x%02x", r, g, this->b);
        return String(buf);
    }
};

namespace reader {
    // Brings the PN532 up over HSU. Returns false if it does not answer, which
    // is normal on the first attempt - the caller retries every two seconds.
    bool begin();

    // Is a tag in the field? A fast poll, safe to call from the UI loop.
    bool present();

    // Reads and decodes. Makes several attempts: one successful read is not
    // evidence of a good read on a link this marginal. See docs/WIRING.md.
    bool read(TagInfo& out);

    // Written for the user, not the developer: "move the spool closer" rather
    // than "read error".
    String lastError();
}
