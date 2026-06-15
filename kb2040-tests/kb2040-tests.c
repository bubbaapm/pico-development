#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "pico/stdlib.h"
#include "ws2812.pio.h"
#include <stdio.h>

#ifdef PICO_DEFAULT_WS2812_PIN
#define WS2812_PIN PICO_DEFAULT_WS2812_PIN
#else
#define WS2812_PIN 17
#endif

#define IS_RGBW false

static inline void put_pixel(uint32_t pixel_grb) {
  pio_sm_put_blocking(pio0, 0, pixel_grb << 8u);
}

static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
  return ((uint32_t)(g) << 16) | ((uint32_t)(r) << 8) | (uint32_t)(b);
}

// Convert HSV to 24-bit GRB value
uint32_t hsv_to_grb(uint16_t h, uint8_t s, uint8_t v) {
  uint8_t r, g, b;
  uint8_t region, remainder, p, q, t;

  if (s == 0) {
    return urgb_u32(v, v, v);
  }

  region = h / 43;
  remainder = (h - (region * 43)) * 6;

  p = (v * (255 - s)) >> 8;
  q = (v * (255 - ((s * remainder) >> 8))) >> 8;
  t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;

  switch (region) {
  case 0:
    r = v;
    g = t;
    b = p;
    break;
  case 1:
    r = q;
    g = v;
    b = p;
    break;
  case 2:
    r = p;
    g = v;
    b = t;
    break;
  case 3:
    r = p;
    g = q;
    b = v;
    break;
  case 4:
    r = t;
    g = p;
    b = v;
    break;
  default:
    r = v;
    g = p;
    b = q;
    break;
  }

  return urgb_u32(r, g, b);
}

int main() {
  stdio_init_all();

  // Give some time for USB serial connection to monitor output
  sleep_ms(2000);
  printf("Initializing KB2040 NeoPixel tests...\n");
  printf("Using WS2812 pin: %d\n", WS2812_PIN);

  PIO pio = pio0;
  int sm = 0;
  uint offset = pio_add_program(pio, &ws2812_program);

  ws2812_program_init(pio, sm, offset, WS2812_PIN, 800000, IS_RGBW);

  uint16_t hue = 0;
  int mode = 0; // 0: Solid Colors, 1: Rainbow Cycle
  uint32_t loop_count = 0;

  while (true) {
    if (mode == 0) {
      // Cycle through solid colors: Red -> Green -> Blue -> White -> Off
      printf("Mode 0: Solid color cycle. Loop count: %ld\n", loop_count++);

      printf("  Red\n");
      put_pixel(urgb_u32(255, 0, 0));
      sleep_ms(1000);

      printf("  Green\n");
      put_pixel(urgb_u32(0, 255, 0));
      sleep_ms(1000);

      printf("  Blue\n");
      put_pixel(urgb_u32(0, 0, 255));
      sleep_ms(1000);

      printf("  White (low intensity)\n");
      put_pixel(urgb_u32(50, 50, 50));
      sleep_ms(1000);

      printf("  Off\n");
      put_pixel(urgb_u32(0, 0, 0));
      sleep_ms(1000);

      // Switch to rainbow mode
      mode = 1;
      hue = 0;
      printf("Switching to Mode 1: Rainbow Wheel\n");
    } else {
      // Rainbow cycle for ~10 seconds (500 steps * 20ms = 10000ms)
      for (int i = 0; i < 500; i++) {
        uint32_t color = hsv_to_grb(hue, 255, 128); // 128 is half brightness
        put_pixel(color);
        hue = (hue + 1) % 256;
        sleep_ms(20);
      }
      // Switch back to solid mode
      mode = 0;
      printf("Switching back to Mode 0\n");
    }
  }
}
