#pragma once
#include <lvgl.h>

// LVGL bound to this board's panel and touch controller.
//
// Rendering path: LVGL draws into small DMA-capable buffers in INTERNAL RAM and
// those go straight to the panel by DMA, with two buffers so rendering and
// transfer overlap. The full-screen sprite is a shadow copy for /screen.bmp
// only, written on the frames where a screenshot was actually requested - a
// PSRAM write per frame would be the slowest thing in the loop, in service of a
// feature used a few times a day. See lvgl_port.cpp for the reasoning.
namespace lvgl_port {

void begin();
// Pumps LVGL. Returns the milliseconds LVGL wants before the next call, so the
// main loop can idle instead of spinning.
uint32_t loop();

// Screenshot support: mirror the next full repaint into the sprite.
void requestCapture(bool on);
bool capturing();

void setBacklight(uint8_t percent);   // 0 = off, used by screen sleep
uint8_t backlight();

// Screen sleep. Call every loop with the user's settings; it dims, then goes
// dark, and wakes on the next touch. Scanning and printer polling never stop.
void sleepTick(int timeoutSec, uint8_t awakeBrightness);
bool asleep();

}  // namespace lvgl_port
