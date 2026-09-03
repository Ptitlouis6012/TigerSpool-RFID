// LovyanGFX device config for the Waveshare ESP32-S3-Touch-LCD-2
// Display: ST7789 240x320 IPS (SPI3)  |  Touch: CST816S (I2C0)
// Pin source: the community-known configuration for this board (2026).
// If the panel comes up blank or mirrored, adjust invert / rgb_order / rotation.
#pragma once
#include <LovyanGFX.hpp>

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789  panel_instance_;
  lgfx::Bus_SPI       bus_instance_;
  lgfx::Light_PWM     light_instance_;
  lgfx::Touch_CST816S touch_instance_;

 public:
  LGFX(void) {
    { // --- SPI bus ---
      auto cfg = bus_instance_.config();
      cfg.spi_host   = SPI3_HOST;
      cfg.spi_mode   = 0;
      cfg.freq_write = 40000000;
      cfg.freq_read  = 16000000;
      cfg.spi_3wire  = true;
      cfg.use_lock   = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk = 39;
      cfg.pin_mosi = 38;
      cfg.pin_miso = 40;
      cfg.pin_dc   = 42;
      bus_instance_.config(cfg);
      panel_instance_.setBus(&bus_instance_);
    }
    { // --- Painel ---
      auto cfg = panel_instance_.config();
      cfg.pin_cs   = 45;
      cfg.pin_rst  = -1;
      cfg.pin_busy = -1;
      cfg.panel_width  = 240;
      cfg.panel_height = 320;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.offset_rotation = 0;
      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits  = 1;
      cfg.readable  = true;
      cfg.invert    = true;    // IPS
      cfg.rgb_order = false;
      cfg.dlen_16bit = false;
      cfg.bus_shared = true;
      panel_instance_.config(cfg);
    }
    { // --- Backlight ---
      auto cfg = light_instance_.config();
      cfg.pin_bl = 1;
      cfg.invert = false;
      cfg.freq   = 44100;
      cfg.pwm_channel = 7;
      light_instance_.config(cfg);
      panel_instance_.setLight(&light_instance_);
    }
    { // --- Touch CST816S (I2C0, addr 0x15) ---
      auto cfg = touch_instance_.config();
      cfg.i2c_port = 0;
      cfg.i2c_addr = 0x15;
      cfg.pin_sda  = 48;
      cfg.pin_scl  = 47;
      cfg.pin_int  = -1;
      cfg.pin_rst  = -1;
      cfg.freq     = 400000;
      cfg.x_min = 0; cfg.x_max = 239;
      cfg.y_min = 0; cfg.y_max = 319;
      cfg.offset_rotation = 0;
      touch_instance_.config(cfg);
      panel_instance_.setTouch(&touch_instance_);
    }
    setPanel(&panel_instance_);
  }
};
