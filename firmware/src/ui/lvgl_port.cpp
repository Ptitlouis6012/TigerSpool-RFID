#include "lvgl_port.h"
#include "theme.h"
#include <Arduino.h>
#include <LovyanGFX.hpp>
#include "LGFX_ESP32_S3_Touch_LCD_2.h"
#include "config.h"

extern LGFX        lcd;
extern LGFX_Sprite canvas;      // shadow framebuffer, only for /screen.bmp
extern bool        canvasReady;

namespace {

// ---------------------------------------------------------------------------
//  Draw buffers: INTERNAL, DMA-capable RAM. Not PSRAM.
//
//  This is the single most important choice for fluidity on this chip. The CPU
//  writes every pixel of a draw buffer, then DMA reads all of it back out.
//  Internal SRAM runs at several hundred MB/s; PSRAM over the octal bus runs at
//  a fraction of that, and it is the same bus the framebuffer sprite and the
//  network stacks are already using. LVGL's *heap* belongs in PSRAM (widget
//  metadata, touched rarely - see lv_conf.h); its draw buffers do not.
//
//  Two buffers of 40 lines. LVGL renders into one while the other is being
//  transferred, so rendering and DMA overlap instead of taking turns. 40 lines
//  is 1/8 of the screen, comfortably above LVGL's 1/10 guidance, and costs
//  2 x 19.2 KB of a 320 KB pool.
// ---------------------------------------------------------------------------
constexpr uint32_t BUF_LINES = 40;
constexpr size_t   BUF_PX    = SCR_W * BUF_LINES;

lv_color_t*        s_buf1 = nullptr;
lv_color_t*        s_buf2 = nullptr;
lv_disp_draw_buf_t s_drawBuf;
lv_disp_drv_t      s_dispDrv;
lv_indev_drv_t     s_indevDrv;
uint8_t            s_backlight  = 100;
volatile bool      s_capture    = false;   // mirror into the sprite this frame
bool               s_dmaStarted = false;

// One-shot: dump the bytes that are about to reach the panel, for the band that
// contains the header bar. If these match what LVGL resolved, the fault is
// downstream in the panel; if they do not, it is in this function.
static bool s_dumpArmed = true;

void flushCb(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* px) {
    const int32_t w = area->x2 - area->x1 + 1;
    const int32_t h = area->y2 - area->y1 + 1;

    if (s_dumpArmed && area->y1 <= 12 && area->y2 >= 12) {
        s_dumpArmed = false;
        const lv_color_t* row = px + (12 - area->y1) * w;   // a row inside the header
        Serial.printf("[ui] flush area x%d..%d y%d..%d  header row y=12:",
                      area->x1, area->x2, area->y1, area->y2);
        for (int i = 0; i < 6 && i < w; i++) Serial.printf(" %04X", row[i].full);
        Serial.printf("   sizeof(lv_color_t)=%u\n", (unsigned)sizeof(lv_color_t));
    }

    // Wait for the PREVIOUS transfer, not this one. By the time we get here
    // LVGL has already rendered into the other buffer, so that wait is usually
    // already satisfied and costs nothing - which is the entire point of double
    // buffering. Waiting after the push instead would serialise the two.
    lcd.waitDMA();

    if (!s_dmaStarted) { lcd.startWrite(); s_dmaStarted = true; }
    lcd.pushImageDMA(area->x1, area->y1, w, h, (lgfx::rgb565_t*)px);

    // The shadow copy is only made when someone actually asked for a screenshot.
    // Doing it every frame would put a PSRAM write in the hot path for a feature
    // used a few times a day.
    if (s_capture && canvasReady)
        canvas.pushImage(area->x1, area->y1, w, h, (lgfx::rgb565_t*)px);

    // Hold the SPI transaction open across every area of one refresh and close
    // it on the last: one bus setup per frame instead of one per rectangle.
    // Closing it also matters because the legacy raw-drawn screens still share
    // this bus, and they must not find it held.
    if (lv_disp_flush_is_last(drv)) { lcd.endWrite(); s_dmaStarted = false; }

    lv_disp_flush_ready(drv);
}

void touchCb(lv_indev_drv_t*, lv_indev_data_t* data) {
    int32_t x, y;
    // A dark screen must not accept taps that the user cannot see the result
    // of: the first touch wakes it, and that touch is consumed by the wake.
    if (s_backlight && lcd.getTouch(&x, &y)) {
        data->point.x = x;
        data->point.y = y;
        data->state   = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

}  // namespace

namespace lvgl_port {

void begin() {
    lv_init();

    // MALLOC_CAP_DMA implies internal RAM on this chip and guarantees the
    // alignment the SPI DMA engine needs.
    s_buf1 = (lv_color_t*)heap_caps_malloc(BUF_PX * sizeof(lv_color_t), MALLOC_CAP_DMA);
    s_buf2 = (lv_color_t*)heap_caps_malloc(BUF_PX * sizeof(lv_color_t), MALLOC_CAP_DMA);
    if (!s_buf1 || !s_buf2) {
        // Half the buffers rather than half the screen: a single buffer still
        // draws correctly, it just cannot overlap render and transfer.
        Serial.println("[ui] DMA buffers unavailable at 40 lines - retrying at 20");
        if (s_buf1) heap_caps_free(s_buf1);
        if (s_buf2) heap_caps_free(s_buf2);
        s_buf1 = (lv_color_t*)heap_caps_malloc(BUF_PX / 2 * sizeof(lv_color_t), MALLOC_CAP_DMA);
        s_buf2 = nullptr;
        lv_disp_draw_buf_init(&s_drawBuf, s_buf1, nullptr, BUF_PX / 2);
    } else {
        lv_disp_draw_buf_init(&s_drawBuf, s_buf1, s_buf2, BUF_PX);
    }

    lv_disp_drv_init(&s_dispDrv);
    s_dispDrv.hor_res      = SCR_W;
    s_dispDrv.ver_res      = SCR_H;
    s_dispDrv.flush_cb     = flushCb;
    s_dispDrv.draw_buf     = &s_drawBuf;
    s_dispDrv.full_refresh = 0;      // dirty rectangles only
    lv_disp_drv_register(&s_dispDrv);

    lv_indev_drv_init(&s_indevDrv);
    s_indevDrv.type    = LV_INDEV_TYPE_POINTER;
    s_indevDrv.read_cb = touchCb;
    lv_indev_drv_register(&s_indevDrv);

    theme::init();

    // Diagnostic, printed once. A colour that looks wrong on the glass has two
    // possible causes and they need opposite fixes: either LVGL resolved the
    // wrong colour, or it resolved the right one and the panel path mangled it.
    // Asking LVGL what it thinks the ground is separates the two in one line.
    {
        lv_obj_t* scr = lv_scr_act();
        lv_color_t c  = lv_obj_get_style_bg_color(scr, LV_PART_MAIN);
        lv_color_t want = lv_color_hex(theme::BG);
        Serial.printf("[ui] screen bg: raw=0x%04X  r=%u g=%u b=%u   "
                      "expected raw=0x%04X r=%u g=%u b=%u\n",
                      c.full, c.ch.red, c.ch.green, c.ch.blue,
                      want.full, want.ch.red, want.ch.green, want.ch.blue);
        // And what the panel is actually fed for that colour.
        Serial.printf("[ui] LV_COLOR_16_SWAP=%d  LV_COLOR_DEPTH=%d\n",
                      LV_COLOR_16_SWAP, LV_COLOR_DEPTH);
    }
    Serial.printf("[ui] LVGL %d.%d ready - %ux%u, %u KB DMA draw buffer%s\n",
                  LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, SCR_W, SCR_H,
                  (unsigned)((s_buf2 ? 2 : 1) * (s_buf2 ? BUF_PX : BUF_PX / 2)
                             * sizeof(lv_color_t) / 1024),
                  s_buf2 ? " (double)" : " (single)");
}

uint32_t loop() { return lv_timer_handler(); }

void requestCapture(bool on) { s_capture = on; }
bool capturing()             { return s_capture; }

void setBacklight(uint8_t percent) {
    if (percent > 100) percent = 100;
    s_backlight = percent;
    lcd.setBrightness(percent == 0 ? 0 : (uint8_t)(percent * 255 / 100));
}

uint8_t backlight() { return s_backlight; }

}  // namespace lvgl_port
