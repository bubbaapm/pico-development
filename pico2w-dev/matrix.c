/**
 * @file  matrix.c
 * @brief 8×8 WS2812B matrix implementation — coordinate mapping, drawing,
 *        text rendering, diagnostics, and visual effects.
 *
 * @author  apmiller
 */

#include "matrix.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "pico/stdlib.h"
#include "ws2812.h"

// =============================================================================
//  Coordinate mapping
// =============================================================================

uint16_t matrix_xy(uint8_t x, uint8_t y) {
  if (x >= MATRIX_WIDTH || y >= MATRIX_HEIGHT)
    return MATRIX_NUM_LEDS; // out of bounds

#if MATRIX_ORIGIN == 1
  // Bottom-left origin: flip y so y=0 is the bottom row
  y = (MATRIX_HEIGHT - 1) - y;
#endif

  if (MATRIX_SNAKE_ROWS && (y & 1)) {
    // Odd rows are wired right-to-left
    return (uint16_t)(y * MATRIX_WIDTH + (MATRIX_WIDTH - 1 - x));
  }
  return (uint16_t)(y * MATRIX_WIDTH + x);
}

// =============================================================================
//  Drawing primitives
// =============================================================================

void matrix_set_pixel(uint8_t x, uint8_t y, uint8_t r, uint8_t g, uint8_t b) {
  uint16_t idx = matrix_xy(x, y);
  if (idx < MATRIX_NUM_LEDS) {
    ws2812_set_pixel(idx, r, g, b);
  }
}

void matrix_fill_rect(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2,
                      uint8_t r, uint8_t g, uint8_t b) {
  // Ensure x1 <= x2 and y1 <= y2
  if (x1 > x2) {
    uint8_t t = x1;
    x1 = x2;
    x2 = t;
  }
  if (y1 > y2) {
    uint8_t t = y1;
    y1 = y2;
    y2 = t;
  }

  for (uint8_t y = y1; y <= y2 && y < MATRIX_HEIGHT; y++) {
    for (uint8_t x = x1; x <= x2 && x < MATRIX_WIDTH; x++) {
      matrix_set_pixel(x, y, r, g, b);
    }
  }
}

void matrix_clear(void) { ws2812_clear(); }

void matrix_fill(uint8_t r, uint8_t g, uint8_t b) {
  ws2812_set_all(r, g, b);
  ws2812_show();
}

// =============================================================================
//  3×5 Bitmap font — digits 0-9, space, dash, colon, dot
// =============================================================================

// Each character is encoded as 5 bytes (rows), 3 bits each (MSB-aligned).
// Bit layout per row byte: bit7=left, bit6=middle, bit5=right, bits4-0 unused.
//
// Example: '0' =  0xE0, 0xA0, 0xA0, 0xA0, 0xE0
//                 ###   # #   # #   # #   ###

static const uint8_t font_3x5[][5] = {
    // '0'
    {0xE0, 0xA0, 0xA0, 0xA0, 0xE0},
    // '1'
    {0x40, 0xC0, 0x40, 0x40, 0xE0},
    // '2'
    {0xE0, 0x20, 0xE0, 0x80, 0xE0},
    // '3'
    {0xE0, 0x20, 0xE0, 0x20, 0xE0},
    // '4'
    {0xA0, 0xA0, 0xE0, 0x20, 0x20},
    // '5'
    {0xE0, 0x80, 0xE0, 0x20, 0xE0},
    // '6'
    {0xE0, 0x80, 0xE0, 0xA0, 0xE0},
    // '7'
    {0xE0, 0x20, 0x40, 0x40, 0x40},
    // '8'
    {0xE0, 0xA0, 0xE0, 0xA0, 0xE0},
    // '9'
    {0xE0, 0xA0, 0xE0, 0x20, 0xE0},
};

// Special characters: space, dash, colon, dot
static const uint8_t font_space[5] = {0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t font_dash[5] = {0x00, 0x00, 0xE0, 0x00, 0x00};
static const uint8_t font_colon[5] = {0x00, 0x40, 0x00, 0x40, 0x00};
static const uint8_t font_dot[5] = {0x00, 0x00, 0x00, 0x00, 0x40};

static const uint8_t *font_get_char(char ch) {
  if (ch >= '0' && ch <= '9')
    return font_3x5[ch - '0'];
  if (ch == ' ')
    return font_space;
  if (ch == '-')
    return font_dash;
  if (ch == ':')
    return font_colon;
  if (ch == '.')
    return font_dot;
  return font_space; // fallback
}

void matrix_draw_char(uint8_t x, uint8_t y, char ch, uint8_t r, uint8_t g,
                      uint8_t b) {
  const uint8_t *glyph = font_get_char(ch);
  for (uint8_t row = 0; row < 5; row++) {
    uint8_t bits = glyph[row];
    for (uint8_t col = 0; col < 3; col++) {
      if (bits & (0x80 >> col)) {
        matrix_set_pixel(x + col, y + row, r, g, b);
      }
    }
  }
}

void matrix_draw_string(uint8_t x, uint8_t y, const char *str, uint8_t r,
                        uint8_t g, uint8_t b) {
  while (*str) {
    matrix_draw_char(x, y, *str, r, g, b);
    x += 4; // 3 pixel char width + 1 pixel gap
    str++;
    if (x >= MATRIX_WIDTH)
      break;
  }
}

// =============================================================================
//  Diagnostics
// =============================================================================

void matrix_test_sequence(void) {
  // Light each LED by linear index 0→63 to verify wiring order
  for (uint16_t i = 0; i < MATRIX_NUM_LEDS; i++) {
    ws2812_clear();
    ws2812_set_pixel(i, 0, 255, 0); // green
    ws2812_show();
    sleep_ms(200);
  }
  ws2812_clear();
}

void matrix_test_coordinates(void) {
  // Sweep columns in red
  for (uint8_t col = 0; col < MATRIX_WIDTH; col++) {
    ws2812_clear();
    for (uint8_t row = 0; row < MATRIX_HEIGHT; row++) {
      matrix_set_pixel(col, row, 255, 0, 0);
    }
    ws2812_show();
    sleep_ms(500);
  }

  // Sweep rows in blue
  for (uint8_t row = 0; row < MATRIX_HEIGHT; row++) {
    ws2812_clear();
    for (uint8_t col = 0; col < MATRIX_WIDTH; col++) {
      matrix_set_pixel(col, row, 0, 0, 255);
    }
    ws2812_show();
    sleep_ms(500);
  }

  ws2812_clear();
}

// =============================================================================
//  Boot animation — expanding coloured square from centre
// =============================================================================

void matrix_effect_boot(void) {
  uint8_t cx = MATRIX_WIDTH / 2;
  uint8_t cy = MATRIX_HEIGHT / 2;

  for (int radius = 0; radius <= 4; radius++) {
    ws2812_clear();

    // Pick a hue based on radius for a rainbow expansion
    uint16_t hue = (uint16_t)(radius * 72) % 360; // 72° steps
    uint32_t color = ws2812_wheel(hue);
    uint8_t r = (color >> 8) & 0xFF;
    uint8_t g = (color >> 16) & 0xFF;
    uint8_t b = (color) & 0xFF;

    int x1 = cx - radius - 1;
    int y1 = cy - radius - 1;
    int x2 = cx + radius;
    int y2 = cy + radius;

    if (x1 < 0)
      x1 = 0;
    if (y1 < 0)
      y1 = 0;
    if (x2 >= MATRIX_WIDTH)
      x2 = MATRIX_WIDTH - 1;
    if (y2 >= MATRIX_HEIGHT)
      y2 = MATRIX_HEIGHT - 1;

    matrix_fill_rect((uint8_t)x1, (uint8_t)y1, (uint8_t)x2, (uint8_t)y2, r, g,
                     b);
    ws2812_show();
    sleep_ms(300);
  }

  sleep_ms(500);
  ws2812_clear();
}

// =============================================================================
//  Matrix rain effect (non-blocking)
// =============================================================================

static uint8_t rain_drops[MATRIX_WIDTH]; // current y position of each drop
static uint32_t rain_last_ms = 0;
static bool rain_inited = false;

void matrix_effect_rain(uint32_t now_ms) {
  if (!rain_inited) {
    for (uint8_t i = 0; i < MATRIX_WIDTH; i++) {
      rain_drops[i] = rand() % MATRIX_HEIGHT;
    }
    rain_inited = true;
    rain_last_ms = now_ms;
  }

  if (now_ms - rain_last_ms < 80)
    return;
  rain_last_ms = now_ms;

  // Dim existing pixels (fade trail)
  for (uint8_t y = 0; y < MATRIX_HEIGHT; y++) {
    for (uint8_t x = 0; x < MATRIX_WIDTH; x++) {
      uint16_t idx = matrix_xy(x, y);
      if (idx >= MATRIX_NUM_LEDS)
        continue;

      // Read current pixel, dim green channel
      // We can't easily read back from the driver, so we maintain a shadow
      // buffer. For simplicity, we'll just redraw each frame.
    }
  }

  // Clear and redraw
  ws2812_set_all(0, 0, 0);

  for (uint8_t x = 0; x < MATRIX_WIDTH; x++) {
    // Draw trail (3 pixels fading)
    for (uint8_t t = 0; t < 4; t++) {
      int y = (int)rain_drops[x] - t;
      if (y < 0)
        y += MATRIX_HEIGHT; // wrap around

      uint8_t brightness = (uint8_t)(255 >> t); // 255, 127, 63, 31
      matrix_set_pixel(x, (uint8_t)y, 0, brightness, 0);
    }

    // Advance drop
    rain_drops[x]++;
    if (rain_drops[x] >= MATRIX_HEIGHT) {
      rain_drops[x] = 0;
      // Randomly skip some columns for variation
      if (rand() % 3 == 0) {
        rain_drops[x] = MATRIX_HEIGHT - 1; // restart from top next frame
      }
    }
  }

  ws2812_show();
}

// =============================================================================
//  Fire effect (non-blocking)
// =============================================================================

static uint8_t heat[MATRIX_WIDTH * MATRIX_HEIGHT];
static uint32_t fire_last_ms = 0;

// Heat-to-colour mapping: black → red → yellow → white
static void heat_to_rgb(uint8_t heat_val, uint8_t *r, uint8_t *g, uint8_t *b) {
  if (heat_val < 85) {
    *r = heat_val * 3;
    *g = 0;
    *b = 0;
  } else if (heat_val < 170) {
    *r = 255;
    *g = (heat_val - 85) * 3;
    *b = 0;
  } else {
    *r = 255;
    *g = 255;
    *b = (heat_val - 170) * 3;
  }
}

void matrix_effect_fire(uint32_t now_ms) {
  if (now_ms - fire_last_ms < 50)
    return;
  fire_last_ms = now_ms;

  // Step 1: Cool each cell a little
  for (uint8_t i = 0; i < MATRIX_WIDTH * MATRIX_HEIGHT; i++) {
    uint8_t cooling = (uint8_t)(rand() % 20);
    heat[i] = (heat[i] > cooling) ? heat[i] - cooling : 0;
  }

  // Step 2: Heat rises — diffuse upward (y=7 is bottom, y=0 is top)
  for (uint8_t y = 0; y < MATRIX_HEIGHT - 1; y++) {
    for (uint8_t x = 0; x < MATRIX_WIDTH; x++) {
      // Average heat from the row below
      uint16_t sum = heat[(y + 1) * MATRIX_WIDTH + x];
      if (x > 0)
        sum += heat[(y + 1) * MATRIX_WIDTH + x - 1];
      if (x < MATRIX_WIDTH - 1)
        sum += heat[(y + 1) * MATRIX_WIDTH + x + 1];
      if (y + 2 < MATRIX_HEIGHT)
        sum += heat[(y + 2) * MATRIX_WIDTH + x];
      else
        sum += heat[(y + 1) * MATRIX_WIDTH + x];

      heat[y * MATRIX_WIDTH + x] = (uint8_t)(sum / 4);
    }
  }

  // Step 3: Ignite new sparks at the bottom row
  for (uint8_t x = 0; x < MATRIX_WIDTH; x++) {
    if (rand() % 4 == 0) {
      uint8_t idx = (MATRIX_HEIGHT - 1) * MATRIX_WIDTH + x;
      uint8_t spark = (uint8_t)(160 + rand() % 96); // 160–255
      heat[idx] = (heat[idx] + spark > 255) ? 255 : heat[idx] + spark;
    }
  }

  // Step 4: Render
  for (uint8_t y = 0; y < MATRIX_HEIGHT; y++) {
    for (uint8_t x = 0; x < MATRIX_WIDTH; x++) {
      uint8_t r, g, b;
      heat_to_rgb(heat[y * MATRIX_WIDTH + x], &r, &g, &b);
      matrix_set_pixel(x, y, r, g, b);
    }
  }

  ws2812_show();
}

// =============================================================================
//  Compass heading display
// =============================================================================

void matrix_effect_compass(float heading_deg) {
  ws2812_set_all(0, 0, 0);

  // Centre point
  float cx = 3.5f;
  float cy = 3.5f;

  // Draw compass background — dim ring
  for (uint8_t y = 0; y < MATRIX_HEIGHT; y++) {
    for (uint8_t x = 0; x < MATRIX_WIDTH; x++) {
      float dx = (float)x - cx;
      float dy = (float)y - cy;
      float dist = sqrtf(dx * dx + dy * dy);
      if (dist >= 2.5f && dist <= 3.8f) {
        matrix_set_pixel(x, y, 10, 10, 20); // dim blue ring
      }
    }
  }

  // N/S/E/W markers
  matrix_set_pixel(3, 0, 255, 0, 0); // N = red
  matrix_set_pixel(4, 0, 255, 0, 0);
  matrix_set_pixel(3, 7, 60, 60, 60); // S = dim white
  matrix_set_pixel(4, 7, 60, 60, 60);
  matrix_set_pixel(0, 3, 60, 60, 60); // W
  matrix_set_pixel(0, 4, 60, 60, 60);
  matrix_set_pixel(7, 3, 60, 60, 60); // E
  matrix_set_pixel(7, 4, 60, 60, 60);

  // Arrow pointing in the heading direction
  float rad = heading_deg * 3.14159f / 180.0f;
  // Arrow tip (2.5 pixels from centre)
  float tip_x = cx + 2.5f * sinf(rad);
  float tip_y = cy - 2.5f * cosf(rad);
  // Arrow mid
  float mid_x = cx + 1.2f * sinf(rad);
  float mid_y = cy - 1.2f * cosf(rad);

  // Centre dot
  matrix_set_pixel(3, 3, 0, 100, 255);
  matrix_set_pixel(4, 3, 0, 100, 255);
  matrix_set_pixel(3, 4, 0, 100, 255);
  matrix_set_pixel(4, 4, 0, 100, 255);

  // Arrow pixels
  if ((uint8_t)tip_x < MATRIX_WIDTH && (uint8_t)tip_y < MATRIX_HEIGHT)
    matrix_set_pixel((uint8_t)tip_x, (uint8_t)tip_y, 0, 255, 100);
  if ((uint8_t)mid_x < MATRIX_WIDTH && (uint8_t)mid_y < MATRIX_HEIGHT)
    matrix_set_pixel((uint8_t)mid_x, (uint8_t)mid_y, 0, 200, 80);

  ws2812_show();
}

// =============================================================================
//  GPS status display
// =============================================================================

void matrix_effect_gps_status(uint8_t fix_quality, uint8_t satellites) {
  // Background colour based on fix quality
  uint8_t bg_r = 0, bg_g = 0, bg_b = 0;

  if (fix_quality == 0) {
    bg_r = 30;
    bg_g = 0;
    bg_b = 0; // dim red = no fix
  } else if (satellites < 6) {
    bg_r = 30;
    bg_g = 20;
    bg_b = 0; // dim yellow = weak fix
  } else {
    bg_r = 0;
    bg_g = 20;
    bg_b = 0; // dim green = good fix
  }

  // Fill background
  ws2812_set_all(bg_r, bg_g, bg_b);

  // Draw satellite count as 1-2 digit number centred on the matrix
  char buf[4];
  if (satellites > 99)
    satellites = 99;

  if (satellites < 10) {
    // Single digit — centre at (3, 1)
    buf[0] = '0' + satellites;
    buf[1] = '\0';
    matrix_draw_string(3, 1, buf, 255, 255, 255);
  } else {
    // Two digits — centre at (1, 1)
    buf[0] = '0' + (satellites / 10);
    buf[1] = '0' + (satellites % 10);
    buf[2] = '\0';
    matrix_draw_string(1, 1, buf, 255, 255, 255);
  }

  ws2812_show();
}

// =============================================================================
//  Radar effect (non-blocking)
// =============================================================================

static int radar_pos = 0;
static int radar_dir = 1;
static uint32_t radar_last_ms = 0;

void matrix_effect_radar(uint32_t now_ms) {
  uint32_t interval = 150 - ((uint32_t)ws2812_get_speed() * 12);
  if (now_ms - radar_last_ms < interval)
    return;
  radar_last_ms = now_ms;

  ws2812_set_all(0, 0, 0);

  for (int y = 0; y < MATRIX_HEIGHT; y++) {
    // Main line
    matrix_set_pixel(radar_pos, y, 255, 0, 0);

    // Trail
    int tail1 = radar_pos - radar_dir;
    if (tail1 >= 0 && tail1 < MATRIX_WIDTH) {
      matrix_set_pixel((uint8_t)tail1, y, 80, 0, 0);
    }
    int tail2 = radar_pos - 2 * radar_dir;
    if (tail2 >= 0 && tail2 < MATRIX_WIDTH) {
      matrix_set_pixel((uint8_t)tail2, y, 20, 0, 0);
    }
  }
  ws2812_show();

  radar_pos += radar_dir;
  if (radar_pos >= MATRIX_WIDTH - 1) {
    radar_pos = MATRIX_WIDTH - 1;
    radar_dir = -1;
  } else if (radar_pos <= 0) {
    radar_pos = 0;
    radar_dir = 1;
  }
}

// =============================================================================
//  Snow / Twinkle effect (non-blocking)
// =============================================================================

static uint32_t snow_last_ms = 0;
static uint8_t snow_matrix[MATRIX_WIDTH * MATRIX_HEIGHT] = {0};

void matrix_effect_snow(uint32_t now_ms) {
  uint32_t interval = 100 - ((uint32_t)ws2812_get_speed() * 8);
  if (now_ms - snow_last_ms < interval)
    return;
  snow_last_ms = now_ms;

  // Fade existing stars
  for (int i = 0; i < MATRIX_NUM_LEDS; i++) {
    if (snow_matrix[i] > 20) {
      snow_matrix[i] -= 20;
    } else {
      snow_matrix[i] = 0;
    }
  }

  // Spark new stars randomly
  if (rand() % 3 == 0) {
    int idx = rand() % MATRIX_NUM_LEDS;
    snow_matrix[idx] = 255;
  }

  // Render
  for (uint8_t y = 0; y < MATRIX_HEIGHT; y++) {
    for (uint8_t x = 0; x < MATRIX_WIDTH; x++) {
      uint8_t val = snow_matrix[y * MATRIX_WIDTH + x];
      // Dim blue background + twinkling white stars
      uint8_t r = val;
      uint8_t g = val;
      uint8_t b = val + (val > 0 ? 0 : 5); // very faint blue ambient if star is off
      matrix_set_pixel(x, y, r, g, b);
    }
  }
  ws2812_show();
}

// =============================================================================
//  Plasma Wave effect (non-blocking)
// =============================================================================

static uint32_t wave_last_ms = 0;
static float wave_time = 0.0f;

void matrix_effect_wave(uint32_t now_ms) {
  uint32_t interval = 50; // smooth 20 fps
  if (now_ms - wave_last_ms < interval)
    return;
  wave_last_ms = now_ms;

  wave_time += (float)ws2812_get_speed() * 0.05f;

  for (uint8_t y = 0; y < MATRIX_HEIGHT; y++) {
    for (uint8_t x = 0; x < MATRIX_WIDTH; x++) {
      float cx = (float)x - 3.5f;
      float cy = (float)y - 3.5f;
      float dist = sqrtf(cx * cx + cy * cy);

      float v = sinf(dist * 0.8f - wave_time) + sinf((float)x * 0.5f + wave_time * 0.7f);
      uint16_t hue = (uint16_t)((v + 2.0f) * 90.0f) % 360;

      uint32_t color = ws2812_wheel(hue);
      uint8_t r = (color >> 8) & 0xFF;
      uint8_t g = (color >> 16) & 0xFF;
      uint8_t b = (color) & 0xFF;

      matrix_set_pixel(x, y, r, g, b);
    }
  }
  ws2812_show();
}
