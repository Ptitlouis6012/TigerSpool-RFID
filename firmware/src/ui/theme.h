#pragma once
#include <lvgl.h>

// The device's visual tokens, in one place.
//
// The screen is a 2.0" 240x320 panel at 200 PPI, so 1 mm is 7.87 px. Every size
// below was chosen against a finger, not by eye: the numbers in millimetres are
// the reason each one is what it is. See docs/ONBOARDING.md.
namespace theme {

// ---- colour ----------------------------------------------------------------
// A physical LCD in a workshop: dark ground, one warm accent, and semantic
// colours kept separate from it so "selected" never reads as "connected".
constexpr uint32_t BG        = 0x07080A;   // screen ground
constexpr uint32_t HEADER    = 0x15181D;
constexpr uint32_t SURFACE   = 0x232A34;   // rows, cells, buttons
constexpr uint32_t TEXT      = 0xFFFFFF;
constexpr uint32_t TEXT_DIM  = 0x7C8590;
constexpr uint32_t ACCENT    = 0xF2C744;   // selection, focus, progress
constexpr uint32_t OK        = 0x3FA85E;   // reachable, success
constexpr uint32_t DANGER    = 0xE0483C;   // unreachable, destructive
constexpr uint32_t GO_BG     = 0x1E5B33;   // confirm button
constexpr uint32_t NO_BG     = 0x5A2320;   // cancel button

// ---- geometry, in device pixels --------------------------------------------
constexpr lv_coord_t SCREEN_W   = 240;
constexpr lv_coord_t SCREEN_H   = 320;
constexpr lv_coord_t HEADER_H   = 44;   // 5.6 mm - the bar is a touch target too
constexpr lv_coord_t ROW_H      = 48;   // 6.1 mm - smallest reliable finger row
constexpr lv_coord_t BUTTON_H   = 52;   // 6.6 mm - the most consequential taps
constexpr lv_coord_t ICON_HIT_W = 52;   // gear / chevron hit width
constexpr lv_coord_t GAP        = 6;
constexpr lv_coord_t PAD        = 8;
constexpr lv_coord_t RADIUS     = 7;
constexpr lv_coord_t CELL_H     = 92;   // slot cell, 11.7 mm tall

void init();                 // build the shared styles; call once after lv_init
lv_style_t* rowStyle();
lv_style_t* rowPressedStyle();
lv_style_t* headerStyle();
lv_style_t* screenStyle();

}  // namespace theme
