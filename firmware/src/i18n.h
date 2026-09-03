#pragma once
#include <Arduino.h>

// User-visible strings.
//
// English first, because it is the fallback and the language the source is
// written in. The rest follow TigerScale's locale set so the two products speak
// the same languages.
//
// Chinese is deliberately absent: Montserrat covers Latin-1 only, and offering a
// language whose glyphs render as empty boxes is worse than not offering it.
// Adding it means generating a CJK subset font the way TigerScale does
// (scripts/make-cjk-font.sh there) - tracked in docs/MIGRATION.md.
enum Lang : uint8_t {
    LANG_EN = 0,   // English
    LANG_FR,       // Francais
    LANG_DE,       // Deutsch
    LANG_ES,       // Espanol
    LANG_IT,       // Italiano
    LANG_PL,       // Polski
    LANG_PT,       // Portugues (BR)
    LANG_PT_PT,    // Portugues (PT)
    LANG_N
};

enum StrId : uint8_t {
    S_TOUCH_SLOT = 0,
    S_SLOT,
    S_BRING_TAG,
    S_TO_READER,
    S_CANCEL,
    S_NO,
    S_SEND,
    S_SEND_TO,
    S_NOZZLE,
    S_BED,
    S_OK,
    S_ERR,
    S_TAP_BACK,
    S_CONNECTING,
    S_WIFI_FAIL,
    S_NO_NETWORK,
    S_CONFIG_HINT,
    S_UPDATED,
    S_PRINTER_OFF,
    S_SEND_FAIL,
    S_HOLDER,
    S_CHOOSE_LANG,
    S_READ_UNSTABLE,
    S_BLANK_TAG,
    S_PRINTER,
    S_NO_PRINTERS,
    S_TT_LINKED,
    S_ADD_WEB,
    S_CONFIG_WEB,
    S_AP_TITLE,
    S_AP_JOIN,
    S_AP_OPEN,
    S_AP_CHOOSE,
    S_TT_IMPORTING,
    S_TT_ACCOUNT,
    S_ONLINE,
    S_OFFLINE,
    S_BACK,
    S_FIND_PRINTERS,
    S_NO_ONLINE,
    // First-boot journey
    S_WIFI_BAD_PASSWORD,
    S_ACCOUNT_WHY,
    S_LINK_ACCOUNT,
    S_SCAN_TO_LINK,
    S_COLOUR_ADAPTED,
    S_COUNT
};

namespace i18n {
    void  begin();                 // load the saved choice from NVS
    bool  chosen();                // has the user ever picked one?
    void  set(Lang l);             // persist to NVS
    Lang  current();
    const char* T(StrId id);       // the string in the current language
    const char* name(Lang l);      // each language written in itself
}
