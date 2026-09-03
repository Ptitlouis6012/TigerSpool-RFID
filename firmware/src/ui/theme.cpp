#include "theme.h"

namespace {
    lv_style_t s_screen, s_header, s_row, s_rowPressed;
    bool s_ready = false;
}

namespace theme {

void init() {
    if (s_ready) return;
    s_ready = true;

    lv_style_init(&s_screen);
    lv_style_set_bg_color(&s_screen, lv_color_hex(BG));
    lv_style_set_bg_opa(&s_screen, LV_OPA_COVER);
    lv_style_set_border_width(&s_screen, 0);
    lv_style_set_pad_all(&s_screen, 0);
    lv_style_set_text_color(&s_screen, lv_color_hex(TEXT));

    lv_style_init(&s_header);
    lv_style_set_bg_color(&s_header, lv_color_hex(HEADER));
    lv_style_set_bg_opa(&s_header, LV_OPA_COVER);
    lv_style_set_border_width(&s_header, 0);
    lv_style_set_radius(&s_header, 0);
    lv_style_set_pad_all(&s_header, 0);

    lv_style_init(&s_row);
    lv_style_set_bg_color(&s_row, lv_color_hex(SURFACE));
    lv_style_set_bg_opa(&s_row, LV_OPA_COVER);
    lv_style_set_radius(&s_row, RADIUS);
    lv_style_set_border_width(&s_row, 0);
    lv_style_set_pad_left(&s_row, 10);
    lv_style_set_pad_right(&s_row, 10);
    lv_style_set_text_color(&s_row, lv_color_hex(TEXT));

    // A press has to be visible: on a 240 px screen with no cursor, the only
    // feedback that a tap landed is the row itself changing.
    lv_style_init(&s_rowPressed);
    lv_style_set_bg_color(&s_rowPressed, lv_color_hex(0x333D4A));
}

lv_style_t* screenStyle()     { return &s_screen; }
lv_style_t* headerStyle()     { return &s_header; }
lv_style_t* rowStyle()        { return &s_row; }
lv_style_t* rowPressedStyle() { return &s_rowPressed; }

}  // namespace theme
