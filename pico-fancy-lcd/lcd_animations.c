#include "lcd_animations.h"
#include "buttons.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "st7735.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void animation_delay(uint32_t delay_ms) {
  absolute_time_t target = make_timeout_time_ms(delay_ms);
  while (!time_reached(target)) {
    cyw43_arch_poll();
    buttons_update();
    if (buttons_any_event()) {
      break;
    }
    sleep_ms(1);
  }
}

void game_delay(uint32_t delay_ms) {
  absolute_time_t target = make_timeout_time_ms(delay_ms);
  while (!time_reached(target)) {
    cyw43_arch_poll();
    buttons_update();
    sleep_ms(1);
  }
}

// ─── Helper: HSV to RGB565 ──────────────────────────────────────────────────
uint16_t hsv_to_rgb565(uint16_t h, uint8_t s, uint8_t v) {
  uint8_t r, g, b;

  if (s == 0) {
    r = g = b = v;
  } else {
    uint8_t region = h / 60;
    uint8_t rem = (h - (region * 60)) * 255 / 60;

    uint8_t p = (v * (255 - s)) >> 8;
    uint8_t q = (v * (255 - ((s * rem) >> 8))) >> 8;
    uint8_t t = (v * (255 - ((s * (255 - rem)) >> 8))) >> 8;

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
  }
  return lcd_color565(r, g, b);
}

// Simple LCG pseudo-random (avoids pulling in full rand)
static uint32_t rng_state = 12345;
uint32_t fast_rand(void) {
  rng_state = rng_state * 1103515245 + 12345;
  return (rng_state >> 16) & 0x7FFF;
}
