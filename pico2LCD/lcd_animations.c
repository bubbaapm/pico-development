#include "lcd_animations.h"
#include "pico/stdlib.h"
#include "st7735.h"
#include <stdlib.h> // abs()

// ─── Helper: HSV to RGB565 ──────────────────────────────────────────────────
// h: 0–360, s: 0–255, v: 0–255
static uint16_t hsv_to_rgb565(uint16_t h, uint8_t s, uint8_t v) {
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
static uint32_t fast_rand(void) {
  rng_state = rng_state * 1103515245 + 12345;
  return (rng_state >> 16) & 0x7FFF;
}

// ─── Rainbow Wipe ───────────────────────────────────────────────────────────

void anim_rainbow_wipe(uint8_t speed_ms) {
  for (uint16_t y = 0; y < LCD_HEIGHT; y++) {
    uint16_t hue = (y * 360) / LCD_HEIGHT;
    uint16_t color = hsv_to_rgb565(hue, 255, 255);
    lcd_fill_rect(0, y, LCD_WIDTH, 1, color);
    if (speed_ms)
      sleep_ms(speed_ms);
  }
}

// ─── Radar Pulse ────────────────────────────────────────────────────────────

void anim_radar_pulse(uint8_t speed_ms) {
  uint16_t cx = LCD_WIDTH / 2;
  uint16_t cy = LCD_HEIGHT / 2;
  uint16_t max_r = (LCD_WIDTH < LCD_HEIGHT ? LCD_WIDTH : LCD_HEIGHT) / 2;

  lcd_fill_screen(COLOR_BLACK);

  for (uint16_t r = 1; r <= max_r; r += 2) {
    uint16_t hue = (r * 360) / max_r;
    uint16_t color = hsv_to_rgb565(hue, 255, 200);
    lcd_draw_circle(cx, cy, r, color);
    if (speed_ms)
      sleep_ms(speed_ms);
  }
}

// ─── Matrix Rain ────────────────────────────────────────────────────────────

void anim_matrix_rain(uint32_t duration_ms) {
  lcd_fill_screen(COLOR_BLACK);

#define MATRIX_COLS LCD_TEXT_COLS
  uint8_t drops[MATRIX_COLS];

  // Initialize drops at random rows
  for (int i = 0; i < MATRIX_COLS; i++) {
    drops[i] = fast_rand() % LCD_TEXT_ROWS;
  }

  absolute_time_t end = make_timeout_time_ms(duration_ms);

  while (!time_reached(end)) {
    for (int col = 0; col < MATRIX_COLS; col++) {
      // Draw the leading bright character
      char ch = '!' + (fast_rand() % 94); // printable ASCII
      uint16_t px = col * LCD_CHAR_WIDTH;
      uint16_t py = drops[col] * LCD_CHAR_HEIGHT;
      lcd_draw_char(px, py, ch, COLOR_WHITE, COLOR_BLACK, 1);

      // Dim the character two rows back (trail effect)
      if (drops[col] >= 2) {
        uint16_t trail_y = (drops[col] - 2) * LCD_CHAR_HEIGHT;
        char tc = '!' + (fast_rand() % 94);
        uint16_t dim_green = lcd_color565(0, 100, 0);
        lcd_draw_char(px, trail_y, tc, dim_green, COLOR_BLACK, 1);
      }

      // Erase five rows back
      if (drops[col] >= 5) {
        uint16_t erase_y = (drops[col] - 5) * LCD_CHAR_HEIGHT;
        lcd_fill_rect(px, erase_y, LCD_CHAR_WIDTH, LCD_CHAR_HEIGHT,
                      COLOR_BLACK);
      }

      // Advance the drop
      drops[col]++;
      if (drops[col] >= LCD_TEXT_ROWS) {
        drops[col] = 0;
        // Randomly skip sometimes for visual variety
        if (fast_rand() % 3 == 0)
          drops[col] = fast_rand() % 4;
      }
    }
    sleep_ms(60);
  }
}

// ─── Bouncing Ball ──────────────────────────────────────────────────────────

void anim_bouncing_ball(uint32_t duration_ms) {
  lcd_fill_screen(COLOR_BLACK);

  const int radius = 8;
  int x = LCD_WIDTH / 2;
  int y = LCD_HEIGHT / 2;
  int dx = 3;
  int dy = 2;
  uint16_t hue = 0;

  absolute_time_t end = make_timeout_time_ms(duration_ms);

  while (!time_reached(end)) {
    // Erase old ball
    lcd_fill_circle(x, y, radius + 1, COLOR_BLACK);

    // Move
    x += dx;
    y += dy;

    // Bounce off walls
    if (x - radius <= 0 || x + radius >= LCD_WIDTH) {
      dx = -dx;
      hue = (hue + 40) % 360;
    }
    if (y - radius <= 0 || y + radius >= LCD_HEIGHT) {
      dy = -dy;
      hue = (hue + 40) % 360;
    }

    // Clamp
    if (x < radius)
      x = radius;
    if (x > LCD_WIDTH - radius)
      x = LCD_WIDTH - radius;
    if (y < radius)
      y = radius;
    if (y > LCD_HEIGHT - radius)
      y = LCD_HEIGHT - radius;

    // Draw new ball
    uint16_t color = hsv_to_rgb565(hue, 255, 255);
    lcd_fill_circle(x, y, radius, color);

    // Draw shadow/glow ring
    lcd_draw_circle(x, y, radius + 1, hsv_to_rgb565(hue, 200, 100));

    sleep_ms(20);
  }
}

// ─── Starfield ──────────────────────────────────────────────────────────────

void anim_starfield(uint32_t duration_ms) {
  lcd_fill_screen(COLOR_BLACK);

#define NUM_STARS 40
  struct star {
    int16_t x, y;   // current position (fixed point, ×16)
    int16_t ox, oy; // origin offset from center (×16)
    uint8_t speed;
  };

  struct star stars[NUM_STARS];
  int16_t cx = LCD_WIDTH / 2;
  int16_t cy = LCD_HEIGHT / 2;

  // Initialize stars
  for (int i = 0; i < NUM_STARS; i++) {
    stars[i].ox = (int16_t)(fast_rand() % LCD_WIDTH) - cx;
    stars[i].oy = (int16_t)(fast_rand() % LCD_HEIGHT) - cy;
    if (stars[i].ox == 0 && stars[i].oy == 0)
      stars[i].ox = 1;
    stars[i].speed = 1 + (fast_rand() % 3);
    stars[i].x = cx + stars[i].ox;
    stars[i].y = cy + stars[i].oy;
  }

  absolute_time_t end = make_timeout_time_ms(duration_ms);

  while (!time_reached(end)) {
    for (int i = 0; i < NUM_STARS; i++) {
      // Erase old position
      lcd_draw_pixel(stars[i].x, stars[i].y, COLOR_BLACK);

      // Move outward from center
      stars[i].ox += (stars[i].ox > 0 ? stars[i].speed : -stars[i].speed);
      stars[i].oy += (stars[i].oy > 0 ? stars[i].speed : -stars[i].speed);

      stars[i].x = cx + stars[i].ox;
      stars[i].y = cy + stars[i].oy;

      // If off screen, respawn near center
      if (stars[i].x < 0 || stars[i].x >= LCD_WIDTH || stars[i].y < 0 ||
          stars[i].y >= LCD_HEIGHT) {
        stars[i].ox = (int16_t)(fast_rand() % 20) - 10;
        stars[i].oy = (int16_t)(fast_rand() % 20) - 10;
        if (stars[i].ox == 0 && stars[i].oy == 0)
          stars[i].ox = 1;
        stars[i].speed = 1 + (fast_rand() % 3);
        stars[i].x = cx + stars[i].ox;
        stars[i].y = cy + stars[i].oy;
      }

      // Brightness based on distance from center
      int dist = abs(stars[i].ox) + abs(stars[i].oy);
      uint8_t brightness = (dist > 80) ? 255 : (dist * 255 / 80);
      uint16_t color = lcd_color565(brightness, brightness, brightness);
      lcd_draw_pixel(stars[i].x, stars[i].y, color);
    }
    sleep_ms(30);
  }
}

// ─── Plasma Effect ──────────────────────────────────────────────────────────

void anim_plasma(uint32_t duration_ms) {
  // Integer sine table (0–255 output, 0–255 input maps to 0–2π)
  static const uint8_t sin_table[256] = {
      128, 131, 134, 137, 140, 143, 146, 149, 152, 155, 158, 162, 165, 167, 170,
      173, 176, 179, 182, 185, 188, 190, 193, 196, 198, 201, 203, 206, 208, 211,
      213, 215, 218, 220, 222, 224, 226, 228, 230, 232, 234, 235, 237, 239, 240,
      241, 243, 244, 245, 246, 248, 249, 250, 250, 251, 252, 253, 253, 254, 254,
      254, 255, 255, 255, 255, 255, 255, 255, 254, 254, 254, 253, 253, 252, 251,
      250, 250, 249, 248, 246, 245, 244, 243, 241, 240, 239, 237, 235, 234, 232,
      230, 228, 226, 224, 222, 220, 218, 215, 213, 211, 208, 206, 203, 201, 198,
      196, 193, 190, 188, 185, 182, 179, 176, 173, 170, 167, 165, 162, 158, 155,
      152, 149, 146, 143, 140, 137, 134, 131, 128, 124, 121, 118, 115, 112, 109,
      106, 103, 100, 97,  93,  90,  88,  85,  82,  79,  76,  73,  70,  67,  65,
      62,  59,  57,  54,  52,  49,  47,  44,  42,  40,  37,  35,  33,  31,  29,
      27,  25,  23,  21,  20,  18,  16,  15,  14,  12,  11,  10,  9,   7,   6,
      5,   5,   4,   3,   2,   2,   1,   1,   1,   0,   0,   0,   0,   0,   0,
      0,   1,   1,   1,   2,   2,   3,   4,   5,   5,   6,   7,   9,   10,  11,
      12,  14,  15,  16,  18,  20,  21,  23,  25,  27,  29,  31,  33,  35,  37,
      40,  42,  44,  47,  49,  52,  54,  57,  59,  62,  65,  67,  70,  73,  76,
      79,  82,  85,  88,  90,  93,  97,  100, 103, 106, 109, 112, 115, 118, 121,
      124,
  };

// Render at reduced resolution for speed (4×4 blocks)
#define PLASMA_SCALE 4
  uint16_t pw = LCD_WIDTH / PLASMA_SCALE;
  uint16_t ph = LCD_HEIGHT / PLASMA_SCALE;

  uint16_t frame = 0;
  absolute_time_t end = make_timeout_time_ms(duration_ms);

  while (!time_reached(end)) {
    for (uint16_t py = 0; py < ph; py++) {
      for (uint16_t px = 0; px < pw; px++) {
        uint8_t v1 = sin_table[(uint8_t)(px * 8 + frame)];
        uint8_t v2 = sin_table[(uint8_t)(py * 6 + frame * 2)];
        uint8_t v3 = sin_table[(uint8_t)((px + py) * 4 + frame)];
        uint8_t v4 = sin_table[(uint8_t)((px * px + py * py) / 4 + frame * 3)];

        uint16_t hue = ((uint16_t)v1 + v2 + v3 + v4) * 360 / 1024;
        uint16_t color = hsv_to_rgb565(hue % 360, 220, 200);

        lcd_fill_rect(px * PLASMA_SCALE, py * PLASMA_SCALE, PLASMA_SCALE,
                      PLASMA_SCALE, color);
      }
    }
    frame += 3;
    // No extra delay — rendering is the bottleneck
  }
}

// ─── Demo All ───────────────────────────────────────────────────────────────

void anim_demo_all(void) {
  // Title card
  lcd_fill_screen(COLOR_BLACK);
  lcd_set_text_color(COLOR_CYAN, COLOR_BLACK);
  lcd_set_text_size(2);
  lcd_printf("CyBot\n LCD");
  sleep_ms(1500);

  // 1. Rainbow wipe
  lcd_set_text_color(COLOR_WHITE, COLOR_BLACK);
  lcd_set_text_size(1);
  anim_rainbow_wipe(3);
  sleep_ms(500);

  // 2. Radar pulse
  anim_radar_pulse(15);
  sleep_ms(500);

  // 3. Matrix rain
  anim_matrix_rain(5000);
  sleep_ms(300);

  // 4. Bouncing ball
  anim_bouncing_ball(5000);
  sleep_ms(300);

  // 5. Starfield
  anim_starfield(5000);
  sleep_ms(300);

  // 6. Plasma
  anim_plasma(5000);
  sleep_ms(300);

  // Outro
  lcd_fill_screen(COLOR_BLACK);
  lcd_set_text_color(COLOR_GREEN, COLOR_BLACK);
  lcd_set_text_size(1);
  lcd_gotoLine(8);
  lcd_puts("  Demo complete!");
  sleep_ms(2000);
}
