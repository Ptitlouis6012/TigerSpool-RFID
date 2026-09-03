#include "screen_setup.h"
#include "theme.h"
#include "i18n.h"
#include <Arduino.h>

namespace {

lv_obj_t* s_screen = nullptr;
lv_obj_t* s_body   = nullptr;
bool      s_active = false;
int       s_lang   = -1;
bool      s_pair   = false;

void onLang(lv_event_t* e) { s_lang = (int)(intptr_t)lv_event_get_user_data(e); }
void onPair(lv_event_t*)   { s_pair = true; }

// One screen object reused across all three steps: building and destroying a
// screen per step would flash the panel between them, and the whole point of
// this sequence is that it feels like one continuous thing.
lv_obj_t* frame(const char* title) {
    if (!s_screen) {
        s_screen = lv_obj_create(nullptr);
        lv_obj_add_style(s_screen, theme::screenStyle(), 0);
        lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);
    }
    lv_obj_clean(s_screen);

    lv_obj_t* header = lv_obj_create(s_screen);
    lv_obj_remove_style_all(header);
    lv_obj_add_style(header, theme::headerStyle(), 0);
    lv_obj_set_size(header, theme::SCREEN_W, theme::HEADER_H);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* t = lv_label_create(header);
    lv_label_set_text(t, title);
    lv_obj_set_style_text_font(t, &lv_font_montserrat_16, 0);
    lv_obj_align(t, LV_ALIGN_LEFT_MID, 9, 0);

    s_body = lv_obj_create(s_screen);
    lv_obj_remove_style_all(s_body);
    lv_obj_set_size(s_body, theme::SCREEN_W, theme::SCREEN_H - theme::HEADER_H);
    lv_obj_align(s_body, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_pad_all(s_body, theme::PAD, 0);
    lv_obj_set_style_pad_row(s_body, theme::GAP, 0);
    lv_obj_set_flex_flow(s_body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_body, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(s_body, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(s_body, LV_OBJ_FLAG_SCROLLABLE);

    if (!s_active) { lv_scr_load(s_screen); s_active = true; }
    lv_obj_invalidate(s_screen);
    return s_body;
}

lv_obj_t* caption(const char* text, uint32_t colour = theme::TEXT_DIM) {
    lv_obj_t* l = lv_label_create(s_body);
    lv_label_set_text(l, text);
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(l, theme::SCREEN_W - 2 * theme::PAD - 8);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(colour), 0);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_12, 0);
    return l;
}

// A QR big enough to scan from arm's length. 132 px is 16.8 mm; below about
// 15 mm a phone has to be held close enough that the screen glares.
lv_obj_t* qr(const char* payload, lv_coord_t size = 132) {
    lv_obj_t* q = lv_qrcode_create(s_body, size,
                                   lv_color_black(), lv_color_white());
    lv_qrcode_update(q, payload, strlen(payload));
    lv_obj_set_style_border_width(q, 5, 0);          // quiet zone
    lv_obj_set_style_border_color(q, lv_color_white(), 0);
    return q;
}

}  // namespace

namespace screen_setup {

void showLanguage(bool force) {
    if (force) { s_active = false; }
    // Built once. frame() clears and rebuilds, and this screen is called from
    // the main loop, so without this guard the list would be torn down and
    // rebuilt every iteration - which cancels the scroll and eats the taps.
    static bool built = false;
    if (built && s_active) return;
    built = true;

    lv_obj_t* body = frame("Language");
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(body, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(body, LV_SCROLLBAR_MODE_AUTO);

    for (int i = 0; i < LANG_N; i++) {
        lv_obj_t* row = lv_btn_create(body);
        lv_obj_remove_style_all(row);
        lv_obj_add_style(row, theme::rowStyle(), 0);
        lv_obj_add_style(row, theme::rowPressedStyle(), LV_STATE_PRESSED);
        lv_obj_set_size(row, LV_PCT(100), theme::ROW_H);
        lv_obj_add_event_cb(row, onLang, LV_EVENT_CLICKED, (void*)(intptr_t)i);

        lv_obj_t* l = lv_label_create(row);
        // Every language is written in itself. A user looking for Portugues
        // should not have to recognise the English word "Portuguese" first.
        lv_label_set_text(l, i18n::name((Lang)i));
        lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
        lv_obj_center(l);
    }
}

int takeLanguage() { int v = s_lang; s_lang = -1; return v; }

void showWifi(const char* apSsid, int clientsConnected) {
    frame(i18n::T(S_AP_TITLE));
    // The payload is the standard Wi-Fi join format both phone cameras
    // recognise natively - no app, and no SSID read off a small screen.
    char payload[96];
    snprintf(payload, sizeof(payload), "WIFI:S:%s;T:nopass;;", apSsid);
    qr(payload);
    caption(i18n::T(S_AP_JOIN));

    lv_obj_t* ssid = lv_label_create(s_body);
    lv_label_set_text(ssid, apSsid);
    lv_obj_set_style_text_font(ssid, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(ssid, lv_color_hex(theme::ACCENT), 0);

    char status[40];
    if (clientsConnected) snprintf(status, sizeof(status), i18n::T(S_AP_CLIENTS), clientsConnected);
    else                  snprintf(status, sizeof(status), "%s", i18n::T(S_AP_WAITING));
    caption(status, clientsConnected ? theme::OK : theme::TEXT_DIM);
}

void showWifiConnecting(const char* ssid, int secondsLeft) {
    frame("Wi-Fi");
    lv_obj_t* sp = lv_spinner_create(s_body, 1200, 60);
    lv_obj_set_size(sp, 72, 72);
    lv_obj_set_style_arc_color(sp, lv_color_hex(theme::ACCENT), LV_PART_INDICATOR);

    lv_obj_t* n = lv_label_create(s_body);
    lv_label_set_text(n, ssid);
    lv_obj_set_style_text_font(n, &lv_font_montserrat_16, 0);

    char t[32];
    snprintf(t, sizeof(t), "%s  %ds", i18n::T(S_CONNECTING), secondsLeft);
    caption(t);
}

void showWifiFailed(const char* ssid) {
    frame("Wi-Fi");
    lv_obj_t* x = lv_label_create(s_body);
    lv_label_set_text(x, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_font(x, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(x, lv_color_hex(theme::DANGER), 0);

    lv_obj_t* n = lv_label_create(s_body);
    lv_label_set_text(n, ssid);
    lv_obj_set_style_text_font(n, &lv_font_montserrat_16, 0);

    // Naming the likely cause is the whole difference between a message a user
    // can act on and one that sends them to a forum.
    caption(i18n::T(S_WIFI_BAD_PASSWORD), theme::TEXT);
}

void showAccountIntro() {
    frame(i18n::T(S_TT_ACCOUNT));
    lv_obj_t* icon = lv_label_create(s_body);
    lv_label_set_text(icon, LV_SYMBOL_DOWNLOAD);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(icon, lv_color_hex(theme::ACCENT), 0);

    caption(i18n::T(S_ACCOUNT_WHY), theme::TEXT);

    lv_obj_t* b = lv_btn_create(s_body);
    lv_obj_remove_style_all(b);
    lv_obj_add_style(b, theme::rowStyle(), 0);
    lv_obj_add_style(b, theme::rowPressedStyle(), LV_STATE_PRESSED);
    lv_obj_set_size(b, LV_PCT(100), theme::BUTTON_H);
    lv_obj_set_style_bg_color(b, lv_color_hex(theme::GO_BG), 0);
    lv_obj_add_event_cb(b, onPair, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* l = lv_label_create(b);
    lv_label_set_text(l, i18n::T(S_LINK_ACCOUNT));
    lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
    lv_obj_center(l);
}

void showPairing(const char* verifyUrl, const char* code, int secondsLeft) {
    frame(i18n::T(S_TT_ACCOUNT));
    qr(verifyUrl, 124);

    lv_obj_t* c = lv_label_create(s_body);
    lv_label_set_text(c, code);
    lv_obj_set_style_text_font(c, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(c, lv_color_hex(theme::ACCENT), 0);
    lv_obj_set_style_text_letter_space(c, 2, 0);

    caption(i18n::T(S_SCAN_TO_LINK));

    char t[24];
    snprintf(t, sizeof(t), "%d:%02d", secondsLeft / 60, secondsLeft % 60);
    caption(t);
}

void showPairFailed(const char* reason) {
    frame(i18n::T(S_TT_ACCOUNT));
    lv_obj_t* x = lv_label_create(s_body);
    lv_label_set_text(x, LV_SYMBOL_WARNING);
    lv_obj_set_style_text_font(x, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(x, lv_color_hex(theme::DANGER), 0);
    caption(reason, theme::TEXT);
}

bool takeStartPairing() { bool v = s_pair; s_pair = false; return v; }

void hide()   { s_active = false; }
bool active() { return s_active; }

}  // namespace screen_setup
