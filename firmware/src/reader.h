#pragma once
#include <Arduino.h>

struct TagInfo {
    bool     ok = false;              // leu e descodificou uma TigerTag valida
    uint32_t idProduct = 0;
    uint16_t idMaterial = 0, idBrand = 0;
    uint8_t  r = 0, g = 0, b = 0;
    uint16_t nozMin = 0, nozMax = 0;
    uint8_t  bedMin = 0, bedMax = 0;
    String   material;               // label resolvido (ex "PETG")
    String   brand;                  // label resolvido (ex "Polymaker")

    String colorHexCreality() const {      // "#0RRGGBB"
        char b8[10]; snprintf(b8, sizeof(b8), "#0%02x%02x%02x", r, g, this->b); return String(b8);
    }
};

namespace reader {
    bool begin();                    // inicia o PN532 (HSU). false se nao responder
    bool present();                  // ha uma tag no campo? (poll rapido)
    // Tenta ler+descodificar. 'out' preenchido se ok. Faz varias tentativas.
    bool read(TagInfo& out);
    String lastError();
}
