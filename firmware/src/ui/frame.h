#pragma once
#include "icons.h"
#include <lvgl.h>

// The shell every screen shares: a 44 px header with an optional back chevron,
// a title, optional status dots, and a body underneath.
//
// It exists so the header is defined once. Six copies of "build a bar, place a
// title, place a chevron" is six places for the hit area to drift, and the hit
// area is the thing that has to stay 5.6 mm.
namespace frame {

using Callback = void (*)();

// Builds a fresh screen and returns its body container. `onBack` null means no
// chevron - the home screen and the first-boot screens have nowhere to go back
// to. The body is a flex column; callers set alignment and scrolling.
lv_obj_t* build(const char* title, Callback onBack);

lv_obj_t* screen();          // the lv_obj the last build() produced
lv_obj_t* header();
lv_obj_t* body();

// Status dots on the right of the header, in the order: account sync, Wi-Fi,
// reader. Pass -1 to leave a dot out.
void setDots(int syncing, int wifiUp, int readerUp);

// Centred content helpers, used by the screens that are a message rather than
// a list.
lv_obj_t* caption(const char* text, uint32_t colour, const lv_font_t* font = nullptr);
lv_obj_t* bigLabel(const char* text, uint32_t colour);

// A full-width action button, 52 px tall. `tone`: 0 neutral, 1 confirm,
// 2 destructive.
lv_obj_t* button(lv_obj_t* parent, const char* text, int tone, Callback onClick);

// A 48 px row with a label, an optional dim value and an optional chevron.
//
// `icon` takes the left of the row. It is where the row's state is expressed -
// green for reachable, red for not, orange for what interrupts - so the label
// itself stays white and readable. Pass 0 for `iconColour` to leave it plain
// white, which is what most rows want: an icon that is coloured on every row is
// an icon that says nothing on any of them.
lv_obj_t* row(lv_obj_t* parent, const char* label, const char* value,
              bool chevron, lv_event_cb_t cb, void* userData,
              icons::Id icon = icons::NONE, uint32_t iconColour = 0);

}  // namespace frame
