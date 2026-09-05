// Row icons, drawn rather than imported.
//
// There is no icon font and no icon set. Everything here is a handful of bare
// lv_obj primitives - a border and no fill for an outline, a fill and no border
// for a solid - assembled inside one 22x22 box. That box is what gets aligned;
// the strokes inside it never are. Change the box and everything follows.
//
// The reason it is done this way rather than with a generated font: it costs
// about a kilobyte of code and no data at all, it recolours perfectly because
// the colour is an argument rather than a style on a glyph, it carries no
// licence obligation, and there is no generated file for a guard to police.
// The TigerScale reached the same conclusion; the recipes below come from it.
//
// Three mechanisms live side by side on purpose: draw what LVGL does not have,
// use LV_SYMBOL_* for what it does. The menu looks of a piece because every
// icon lands in the same box under the same colour rule, not because they all
// come from the same place.
#pragma once
#include <lvgl.h>

namespace icons {

enum Id {
    NONE = 0,
    PRINTER,     // drawn - a body, the sheet above it, the sheet below
    WIFI,        // LV_SYMBOL
    USER,        // drawn - LVGL has no person, and an envelope says "messages"
    SCREEN,      // drawn - a sun, for brightness and sleep
    GLOBE,       // drawn - LVGL has no globe, and a keyboard is not a language
    UPDATE,      // LV_SYMBOL
    RESTART,     // LV_SYMBOL
    ERASE,       // LV_SYMBOL
};

// Builds the icon into a 22x22 box parented to `parent`. `colour` is applied to
// every stroke. Returns the box, or nullptr for NONE.
lv_obj_t* build(lv_obj_t* parent, Id id, uint32_t colour);

constexpr lv_coord_t BOX = 22;

}  // namespace icons
