#include "reader.h"
#include "config.h"
#include "i18n.h"
#include "tigertag_db.h"
#include <PN532_HSU.h>
#include <PN532.h>

namespace {
    HardwareSerial serNFC(PN532_UART_NUM);
    PN532_HSU      pn532hsu(serNFC);
    PN532         nfc(pn532hsu);
    String        g_err;

    uint16_t be16(const uint8_t* p) { return (uint16_t(p[0]) << 8) | p[1]; }
    uint32_t be32(const uint8_t* p) {
        return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
    }
}

static bool g_diag = true;   // first boot: verbose logging

bool reader::begin() {
    serNFC.begin(PN532_UART_BAUD, SERIAL_8N1, PN532_UART_RX, PN532_UART_TX);
    delay(20);

    if (g_diag) {
        // Are any bytes arriving on RX? (nothing = dead line or no power;
        //  garbage = TX/RX swapped, DIPs not in HSU mode, 5 V level, or wrong UART)
        delay(30);
        int n = serNFC.available(); uint8_t pk[8]; int m = 0;
        while (serNFC.available() && m < 8) pk[m++] = serNFC.read();
        Serial.printf("[reader] cfg UART%d RX=GPIO%d TX=GPIO%d @%d  RX idle bytes=%d",
                      PN532_UART_NUM, PN532_UART_RX, PN532_UART_TX, PN532_UART_BAUD, n);
        for (int i = 0; i < m; i++) Serial.printf(" %02X", pk[i]);
        Serial.println();
    }

    // The vendored library wakes the module before EVERY command. These modules
    // power down between commands: without that preamble only the first command
    // after boot answers, and the reader reports itself ready while reading
    // nothing. See docs/WIRING.md.
    pn532hsu.wakeup();
    delay(50);
    for (int t = 0; t < 5; t++) {
        uint32_t v1 = nfc.getFirmwareVersion();
        nfc.SAMConfig();
        uint32_t v2 = nfc.getFirmwareVersion();   // tem de responder DEPOIS do 1o
        if (g_diag)
            Serial.printf("[reader] init try %d: v1=0x%08X v2=0x%08X\n", t, v1, v2);
        if (v1 && v2) {
            Serial.printf("[reader] PN532 fw %u.%u OK\n",
                          (uint8_t)((v1 >> 16) & 0xFF), (uint8_t)((v1 >> 8) & 0xFF));
            nfc.setPassiveActivationRetries(0x02);
            g_diag = false;
            return true;
        }
        pn532hsu.wakeup();                        // wake it again before retrying
        delay(200);
    }
    g_err = "PN532 not answering (HSU)";
    return false;
}

bool reader::present() {
    uint8_t uid[7], ul = 0;
    return nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &ul, 120);
}

bool reader::read(TagInfo& out) {
    out = TagInfo{};
    uint8_t payload[32];             // paginas 0x04..0x0B (material, cor, temps)
    bool win = false;

    uint8_t uid[7] = {0}; uint8_t ul = 0;
    int okReads = 0, zeroReads = 0;

    for (int attempt = 0; attempt < 20 && !win; attempt++) {
        if (!nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &ul, 150)) { delay(8); continue; }
        delay(5);                    // deixa a tag assentar no campo

        bool got;
        if (attempt < 10) {
            // Two 16-byte transactions: the READ command returns four pages at
            // once, which is far more reliable than eight small reads.
            got = nfc.mifareultralight_ReadPage16(0x04, payload + 0)
               && nfc.mifareultralight_ReadPage16(0x08, payload + 16);
        } else {
            // Fallback: eight 4-byte reads. Smaller frames survive a poor link
            // better on a marginal HSU link)
            got = true;
            for (uint8_t i = 0; i < 8 && got; i++)
                got = nfc.mifareultralight_ReadPage(0x04 + i, payload + i * 4);
        }
        if (!got) { delay(8); continue; }
        okReads++;

        // Count zero bytes: a written TigerTag has fewer than ~10 in these pages
        int zc = 0;
        for (int i = 0; i < 32; i++) if (payload[i] == 0) zc++;
        if (zc > 20 || be32(payload + 0) == 0 || be32(payload + 4) == 0) {
            zeroReads++;
            delay(10);
            continue;
        }
        win = true;
    }

    // Diagnostic dump
    { String u; char h[4];
      for (uint8_t i = 0; i < ul; i++) { snprintf(h, sizeof(h), "%02X", uid[i]); u += h; }
      String px; for (int i = 0; i < 32; i++) { snprintf(h, sizeof(h), "%02X", payload[i]); px += h; if (i % 4 == 3) px += ' '; }
      Serial.printf("[reader] UID=%s (%ub)  reads ok=%d zeros=%d  win=%d\n      p04-0B: %s\n",
                    u.c_str(), ul, okReads, zeroReads, win, px.c_str()); }

    if (!win) { g_err = i18n::T(S_READ_UNSTABLE); return false; }

    out.idProduct  = be32(payload + 4);
    out.idMaterial = be16(payload + 8);
    out.idBrand    = be16(payload + 14);
    out.r = payload[16]; out.g = payload[17]; out.b = payload[18];
    out.nozMin = be16(payload + 24);
    out.nozMax = be16(payload + 26);
    out.bedMin = payload[30];
    out.bedMax = payload[31];

    const char* m = tt_material(out.idMaterial);
    const char* br = tt_brand(out.idBrand);
    out.material = m ? String(m) : (String("MAT#") + out.idMaterial);
    out.brand    = br ? String(br) : (String("brand#") + out.idBrand);
    out.ok = true;
    Serial.printf("[reader] %s / %s  #%02X%02X%02X  nozzle %u-%u  bed %u-%u\n",
                  out.material.c_str(), out.brand.c_str(), out.r, out.g, out.b,
                  out.nozMin, out.nozMax, out.bedMin, out.bedMax);
    return true;
}

String reader::lastError() { return g_err; }
