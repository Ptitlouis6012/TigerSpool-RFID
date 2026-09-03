#include "screen_settings.h"
#include "frame.h"
#include "theme.h"
#include "i18n.h"
#include "version.h"
#include <lvgl.h>

namespace {
screen_settings::Entry s_entry = screen_settings::E_NONE;
bool s_back = false;
int  s_toggled = -1;
uint32_t s_menuSig = 0;
uint32_t s_pickSig = 0;

void onEntry(lv_event_t* e) {
    s_entry = (screen_settings::Entry)(intptr_t)lv_event_get_user_data(e);
}
void onBack()  { s_back = true; }
void onToggle(lv_event_t* e) { s_toggled = (int)(intptr_t)lv_event_get_user_data(e); }

uint32_t hashOf(const char* s, uint32_t h = 2166136261u) {
    for (; s && *s; s++) h = h * 16777619u ^ (uint8_t)*s;
    return h;
}
}  // namespace

namespace screen_settings {

void invalidate() { s_menuSig = 0; s_pickSig = 0; }

void showMenu(const char* network, const char* account,
              int visiblePrinters, int totalPrinters) {
    uint32_t sig = hashOf(network) ^ hashOf(account)
                 ^ ((uint32_t)visiblePrinters << 8) ^ (uint32_t)totalPrinters;
    if (sig == s_menuSig) return;
    s_menuSig = sig;

    lv_obj_t* body = frame::build(i18n::T(S_SETTINGS), onBack);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(body, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(body, LV_SCROLLBAR_MODE_AUTO);

    char printersVal[16];
    snprintf(printersVal, sizeof(printersVal), "%d/%d", visiblePrinters, totalPrinters);

    struct Row { Entry id; const char* label; const char* value; };
    const Row rows[] = {
        { E_PRINTERS, i18n::T(S_PRINTER),      printersVal },
        { E_WIFI,     "Wi-Fi",                 network     },
        { E_ACCOUNT,  i18n::T(S_TT_ACCOUNT),   account     },
        { E_SCREEN,   "Screen",                ""          },
        { E_LANGUAGE, "Language",              i18n::name(i18n::current()) },
        { E_UPDATE,   "Update",                TIGERSPOOL_FW_VERSION       },
        { E_RESTART,  "Restart",               ""          },
        { E_FACTORY,  "Factory reset",         ""          },
    };
    for (auto& r : rows) {
        lv_obj_t* row = frame::row(body, r.label, r.value, true, onEntry,
                                   (void*)(intptr_t)r.id);
        if (r.id == E_FACTORY) {
            // The one entry that cannot be undone reads as such before it is
            // opened, not only after.
            lv_obj_t* label = lv_obj_get_child(row, 0);
            lv_obj_set_style_text_color(label, lv_color_hex(theme::DANGER), 0);
        }
    }
}

Entry takeEntry() { Entry v = s_entry; s_entry = E_NONE; return v; }
bool  takeBack()  { bool v = s_back; s_back = false; return v; }

void showPrinters(const PrinterCfg* printers, int count) {
    uint32_t sig = 2166136261u;
    for (int i = 0; i < count; i++) {
        if (printers[i].type == PT_NONE) continue;
        sig = hashOf(printers[i].name.c_str(), sig);
        sig = sig * 16777619u ^ (uint32_t)printers[i].visible;
    }
    if (sig == s_pickSig) return;
    s_pickSig = sig;

    lv_obj_t* body = frame::build(i18n::T(S_PRINTER), onBack);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(body, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(body, LV_SCROLLBAR_MODE_AUTO);

    int shown = 0;
    for (int i = 0; i < count; i++) {
        if (printers[i].type == PT_NONE) continue;
        shown++;

        lv_obj_t* row = lv_obj_create(body);
        lv_obj_remove_style_all(row);
        lv_obj_add_style(row, theme::rowStyle(), 0);
        lv_obj_set_size(row, LV_PCT(100), theme::ROW_H);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(row, 8, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* name = lv_label_create(row);
        lv_label_set_text(name, printers[i].name.c_str());
        lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
        lv_obj_set_flex_grow(name, 1);
        lv_obj_set_style_text_font(name, &lv_font_montserrat_14, 0);

        // The switch is the control, and the whole row is its target: a 40 px
        // switch on a 240 px row is a small thing to aim at when the row it
        // sits in is already the obvious place to press.
        lv_obj_t* sw = lv_switch_create(row);
        lv_obj_set_size(sw, 44, 24);
        lv_obj_clear_flag(sw, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_color(sw, lv_color_hex(0x2A313B), LV_PART_MAIN);
        lv_obj_set_style_bg_color(sw, lv_color_hex(theme::ACCENT),
                                  LV_PART_INDICATOR | LV_STATE_CHECKED);
        if (printers[i].visible) lv_obj_add_state(sw, LV_STATE_CHECKED);

        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(row, onToggle, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    }

    if (!shown) frame::caption(i18n::T(S_NO_PRINTERS), theme::TEXT_DIM);
}

int takeToggled() { int v = s_toggled; s_toggled = -1; return v; }

}  // namespace screen_settings
