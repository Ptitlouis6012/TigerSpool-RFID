#include "icons.h"

namespace icons {
namespace {

// Every primitive needs the same three things, and each of them is a bug if it
// is left out. remove_style_all, or LVGL's theme arrives with a grey fill on
// what was meant to be an outline. Clearing SCROLLABLE and CLICKABLE, or every
// stroke becomes an object that swallows the press meant for the row - and the
// row stops responding where it is touched, which reads as a dead menu entry.
lv_obj_t* piece(lv_obj_t* p, int x, int y, int w, int h) {
    lv_obj_t* o = lv_obj_create(p);
    lv_obj_remove_style_all(o);
    lv_obj_set_size(o, w, h);
    lv_obj_set_pos(o, x, y);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE);
    return o;
}

// An outline: border, no fill. The stroke stays 2 px whatever the box, because
// a 1 px border disappears at this pixel density and a border does not scale
// with the shape it draws.
void outline(lv_obj_t* p, int x, int y, int w, int h, int r, uint32_t c) {
    lv_obj_t* o = piece(p, x, y, w, h);
    lv_obj_set_style_radius(o, r, 0);
    lv_obj_set_style_border_width(o, 2, 0);
    lv_obj_set_style_border_color(o, lv_color_hex(c), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
}

void ring(lv_obj_t* p, int x, int y, int d, uint32_t c) {
    outline(p, x, y, d, d, LV_RADIUS_CIRCLE, c);
}

// A solid: fill, no border.
void bar(lv_obj_t* p, int x, int y, int w, int h, int r, uint32_t c) {
    lv_obj_t* o = piece(p, x, y, w, h);
    lv_obj_set_style_radius(o, r, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_bg_color(o, lv_color_hex(c), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
}

lv_obj_t* symbol(lv_obj_t* parent, const char* glyph, uint32_t colour) {
    lv_obj_t* box = piece(parent, 0, 0, BOX, BOX);
    lv_obj_t* g = lv_label_create(box);
    lv_label_set_text(g, glyph);
    lv_obj_set_style_text_font(g, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(g, lv_color_hex(colour), 0);
    lv_obj_center(g);
    return box;
}

}  // namespace

lv_obj_t* build(lv_obj_t* parent, Id id, uint32_t c) {
    switch (id) {
    case WIFI:    return symbol(parent, LV_SYMBOL_WIFI, c);
    case UPDATE:  return symbol(parent, LV_SYMBOL_DOWNLOAD, c);
    case RESTART: return symbol(parent, LV_SYMBOL_REFRESH, c);
    case ERASE:   return symbol(parent, LV_SYMBOL_TRASH, c);
    case NONE:    return nullptr;
    default:      break;
    }

    lv_obj_t* box = piece(parent, 0, 0, BOX, BOX);

    switch (id) {
    case USER:
        // A head, and shoulders that run off the bottom of the box. LVGL clips
        // a child to its parent, so the lower half of the pill never draws and
        // what is left reads as a dome rather than a capsule.
        ring(box, 7, 1, 8, c);
        outline(box, 2, 12, 18, 16, 8, c);
        break;

    case GLOBE:
        // Three strokes: the sphere, the equator, one meridian. That is enough
        // for the eye to finish it as a globe. These are the TigerScale's own
        // coordinates for a 22 px box, taken as given rather than scaled from
        // its 26 px ones - a 2 px stroke does not scale with the shape it
        // draws, so scaled coordinates come out wrong.
        ring(box, 1, 1, 19, c);
        bar(box, 1, 10, 19, 2, 0, c);
        outline(box, 7, 1, 8, 19, 4, c);
        break;

    case PRINTER:
        // The sheet going in, the body, the sheet coming out.
        bar(box, 6, 0, 10, 5, 1, c);
        outline(box, 2, 6, 18, 10, 2, c);
        bar(box, 6, 17, 10, 5, 1, c);
        break;

    case SCREEN:
        // A sun: brightness and sleep on one row.
        ring(box, 6, 6, 10, c);
        bar(box, 10, 0, 2, 4, 1, c);
        bar(box, 10, 18, 2, 4, 1, c);
        bar(box, 0, 10, 4, 2, 1, c);
        bar(box, 18, 10, 4, 2, 1, c);
        break;

    default:
        break;
    }
    return box;
}

}  // namespace icons
