#include "../lcd_animations.h"
#include "../buttons.h"
#include "../st7735.h"
#include "pico/stdlib.h"
#include <stdlib.h>

// ─── Rainbow Wipe ───────────────────────────────────────────────────────────

void anim_rainbow_wipe(uint32_t duration_ms) {
  absolute_time_t end;
  if (duration_ms > 0) {
    end = make_timeout_time_ms(duration_ms);
  }

  uint16_t offset = 0;
  while (duration_ms == 0 || !time_reached(end)) {
    for (uint16_t y = 0; y < LCD_HEIGHT; y++) {
      buttons_update();
      if (buttons_any_event())
        return;

      uint16_t hue = (y * 360 / LCD_HEIGHT + offset) % 360;
      uint16_t color = hsv_to_rgb565(hue, 255, 255);
      lcd_fill_rect(0, y, LCD_WIDTH, 1, color);
    }
    lcd_flush();
    offset = (offset + 5) % 360;
    
    if (duration_ms > 0 && time_reached(end)) {
      break;
    }
    animation_delay(20);
  }
}

// ─── Radar Pulse ────────────────────────────────────────────────────────────

void anim_radar_pulse(uint32_t duration_ms) {
  uint16_t cx = LCD_WIDTH / 2;
  uint16_t cy = LCD_HEIGHT / 2;
  uint16_t max_r = (LCD_WIDTH < LCD_HEIGHT ? LCD_WIDTH : LCD_HEIGHT) / 2;

  absolute_time_t end;
  if (duration_ms > 0) {
    end = make_timeout_time_ms(duration_ms);
  }

  uint16_t current_r = 1;
  while (duration_ms == 0 || !time_reached(end)) {
    buttons_update();
    if (buttons_any_event())
      return;

    lcd_fill_screen(COLOR_BLACK);

    for (uint16_t r = current_r; r > 0; r = (r > 6 ? r - 6 : 0)) {
      uint16_t hue = (r * 360) / max_r;
      uint16_t color = hsv_to_rgb565(hue, 255, 200);
      lcd_draw_circle(cx, cy, r, color);
    }

    lcd_flush();
    current_r = (current_r + 2) % max_r;
    if (current_r == 0)
      current_r = 1;

    animation_delay(20);
  }
}

// ─── Matrix Rain ────────────────────────────────────────────────────────────

void anim_matrix_rain(uint32_t duration_ms) {
#define MATRIX_COLS LCD_TEXT_COLS
  uint8_t drops[MATRIX_COLS];

  for (int i = 0; i < MATRIX_COLS; i++) {
    drops[i] = fast_rand() % LCD_TEXT_ROWS;
  }

  absolute_time_t end;
  if (duration_ms > 0) {
    end = make_timeout_time_ms(duration_ms);
  }

  while (duration_ms == 0 || !time_reached(end)) {
    buttons_update();
    if (buttons_any_event())
      return;

    lcd_fill_screen(COLOR_BLACK);

    for (int col = 0; col < MATRIX_COLS; col++) {
      int head = drops[col];
      for (int i = 0; i < 6; i++) {
        int row = head - i;
        if (row >= 0 && row < LCD_TEXT_ROWS) {
          uint16_t px = col * LCD_CHAR_WIDTH;
          uint16_t py = row * LCD_CHAR_HEIGHT;
          char ch = '!' + ((row * 7 + col * 13 + (to_ms_since_boot(get_absolute_time()) / 200)) % 94);
          
          uint16_t color;
          if (i == 0) color = COLOR_WHITE;
          else if (i == 1) color = lcd_color565(50, 255, 50);
          else if (i == 2) color = lcd_color565(0, 200, 0);
          else if (i == 3) color = lcd_color565(0, 150, 0);
          else if (i == 4) color = lcd_color565(0, 100, 0);
          else color = lcd_color565(0, 50, 0);

          lcd_draw_char(px, py, ch, color, COLOR_BLACK, 1);
        }
      }

      drops[col]++;
      if (drops[col] >= LCD_TEXT_ROWS + 6) {
        drops[col] = 0;
        if (fast_rand() % 3 == 0)
          drops[col] = fast_rand() % 4;
      }
    }
    lcd_flush();
    animation_delay(60);
  }
}

// ─── Bouncing Ball ──────────────────────────────────────────────────────────

void anim_bouncing_ball(uint32_t duration_ms) {
  const int radius = 8;
  int x = LCD_WIDTH / 2;
  int y = LCD_HEIGHT / 2;
  int dx = 3;
  int dy = 2;
  uint16_t hue = 0;

  absolute_time_t end;
  if (duration_ms > 0) {
    end = make_timeout_time_ms(duration_ms);
  }

  while (duration_ms == 0 || !time_reached(end)) {
    buttons_update();
    if (buttons_any_event())
      return;

    lcd_fill_screen(COLOR_BLACK);

    x += dx;
    y += dy;

    if (x - radius <= 0 || x + radius >= LCD_WIDTH) {
      dx = -dx;
      hue = (hue + 40) % 360;
    }
    if (y - radius <= 0 || y + radius >= LCD_HEIGHT) {
      dy = -dy;
      hue = (hue + 40) % 360;
    }

    if (x < radius)
      x = radius;
    if (x > LCD_WIDTH - radius)
      x = LCD_WIDTH - radius;
    if (y < radius)
      y = radius;
    if (y > LCD_HEIGHT - radius)
      y = LCD_HEIGHT - radius;

    uint16_t color = hsv_to_rgb565(hue, 255, 255);
    lcd_fill_circle(x, y, radius, color);
    lcd_draw_circle(x, y, radius + 1, hsv_to_rgb565(hue, 200, 100));

    lcd_flush();
    animation_delay(20);
  }
}

// ─── Starfield ──────────────────────────────────────────────────────────────

void anim_starfield(uint32_t duration_ms) {
  lcd_fill_screen(COLOR_BLACK);

#define NUM_STARS 50
  struct star {
    float x;
    float y;
    float z;
  };

  struct star stars[NUM_STARS];
  float cx = LCD_WIDTH / 2.0f;
  float cy = LCD_HEIGHT / 2.0f;

  for (int i = 0; i < NUM_STARS; i++) {
    stars[i].x = (float)((int)(fast_rand() % 300) - 150);
    stars[i].y = (float)((int)(fast_rand() % 240) - 120);
    stars[i].z = (float)(1 + (fast_rand() % 100));
  }

  absolute_time_t end;
  if (duration_ms > 0) {
    end = make_timeout_time_ms(duration_ms);
  }

  while (duration_ms == 0 || !time_reached(end)) {
    buttons_update();
    if (buttons_any_event())
      return;

    lcd_fill_screen(COLOR_BLACK);

    float scale = 65.0f;

    for (int i = 0; i < NUM_STARS; i++) {
      stars[i].z -= 1.5f;

      int px = (int)(cx + (stars[i].x * scale) / stars[i].z);
      int py = (int)(cy + (stars[i].y * scale) / stars[i].z);

      if (stars[i].z <= 1.5f || px < 0 || px >= LCD_WIDTH || py < 0 ||
          py >= LCD_HEIGHT) {
        stars[i].x = (float)((int)(fast_rand() % 300) - 150);
        stars[i].y = (float)((int)(fast_rand() % 240) - 120);
        stars[i].z = 100.0f;
        px = (int)(cx + (stars[i].x * scale) / stars[i].z);
        py = (int)(cy + (stars[i].y * scale) / stars[i].z);
      }

      uint8_t brightness = (uint8_t)(255.0f * (1.0f - (stars[i].z / 100.0f)));
      uint16_t color = lcd_color565(brightness, brightness, brightness);
      lcd_draw_pixel(px, py, color);
    }

    lcd_flush();
    animation_delay(30);
  }
}
