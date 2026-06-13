#include "../lcd_animations.h"
#include "../buttons.h"
#include "../st7735.h"
#include "pico/stdlib.h"
#include <math.h>
#include <string.h>

static bool extra_continue(absolute_time_t end, uint32_t duration_ms) {
  buttons_update();
  return !buttons_any_event() && (duration_ms == 0 || !time_reached(end));
}

static int wrap_i(int v, int max) {
  if (v < 0) return max - 1;
  if (v >= max) return 0;
  return v;
}

void anim_tron_trails(uint32_t duration_ms) {
  absolute_time_t end;
  if (duration_ms > 0) end = make_timeout_time_ms(duration_ms);

#define TRON_BIKES 4
#define TRON_W 80
#define TRON_H 64
  struct bike {
    int x, y, dx, dy;
    uint16_t hue;
    bool alive;
  };
  struct bike bikes[TRON_BIKES] = {
      {10, 12, 1, 0, 184, true},
      {69, 50, -1, 0, 318, true},
      {14, 54, 0, -1, 42, true},
      {65, 10, 0, 1, 260, true},
  };
  uint16_t trail[TRON_W][TRON_H];
  memset(trail, 0, sizeof(trail));
  uint32_t frame = 0;

  while (extra_continue(end, duration_ms)) {
    lcd_fill_screen(COLOR_BLACK);
    for (int g = 0; g <= LCD_WIDTH; g += 16) {
      lcd_draw_line(g, 0, g, LCD_HEIGHT - 1, hsv_to_rgb565(210, 160, 35));
    }
    for (int g = 0; g <= LCD_HEIGHT; g += 16) {
      lcd_draw_line(0, g, LCD_WIDTH - 1, g, hsv_to_rgb565(210, 160, 35));
    }

    for (int i = 0; i < TRON_BIKES; i++) {
      if (!bikes[i].alive) {
        if ((frame + i * 17) % 90 == 0) {
          bikes[i].x = 10 + (fast_rand() % 60);
          bikes[i].y = 8 + (fast_rand() % 48);
          bikes[i].dx = (fast_rand() % 2) ? 1 : -1;
          bikes[i].dy = 0;
          bikes[i].alive = true;
        }
        continue;
      }
      int dirs[3][2] = {
          {bikes[i].dx, bikes[i].dy},
          {-bikes[i].dy, bikes[i].dx},
          {bikes[i].dy, -bikes[i].dx},
      };
      int best = 0;
      int best_clear = -1;
      int forward_clear = 0;
      for (int d = 0; d < 3; d++) {
        int clear = 0;
        int tx = bikes[i].x;
        int ty = bikes[i].y;
        for (int step = 0; step < 18; step++) {
          tx = wrap_i(tx + dirs[d][0], TRON_W);
          ty = wrap_i(ty + dirs[d][1], TRON_H);
          if (trail[tx][ty]) break;
          clear++;
        }
        if (d == 0) forward_clear = clear;
        if (clear > best_clear || (clear == best_clear && (fast_rand() & 1))) {
          best_clear = clear;
          best = d;
        }
      }
      if (best != 0 && (forward_clear < 14 || (frame + i * 13) % 29 == 0)) {
        bikes[i].dx = dirs[best][0];
        bikes[i].dy = dirs[best][1];
      }
      trail[bikes[i].x][bikes[i].y] = bikes[i].hue + 1;
      bikes[i].x = wrap_i(bikes[i].x + bikes[i].dx, TRON_W);
      bikes[i].y = wrap_i(bikes[i].y + bikes[i].dy, TRON_H);
      if (trail[bikes[i].x][bikes[i].y]) {
        bikes[i].alive = false;
        continue;
      }
    }
    for (int x = 0; x < TRON_W; x++) {
      for (int y = 0; y < TRON_H; y++) {
        if (trail[x][y]) {
          lcd_fill_rect(x * 2, y * 2, 2, 2,
                        hsv_to_rgb565(trail[x][y] - 1, 255, 210));
        }
      }
    }
    for (int i = 0; i < TRON_BIKES; i++) {
      if (bikes[i].alive) {
        lcd_fill_rect(bikes[i].x * 2 - 2, bikes[i].y * 2 - 2, 5, 5,
                      hsv_to_rgb565(bikes[i].hue, 120, 255));
      }
    }
    if (frame % 420 == 419) memset(trail, 0, sizeof(trail));
    lcd_flush();
    frame++;
    animation_delay(35);
  }
}

void anim_neon_rain_city(uint32_t duration_ms) {
  absolute_time_t end;
  if (duration_ms > 0) end = make_timeout_time_ms(duration_ms);

#define RAIN_DROPS 34
  int x[RAIN_DROPS], y[RAIN_DROPS], speed[RAIN_DROPS];
  for (int i = 0; i < RAIN_DROPS; i++) {
    x[i] = fast_rand() % LCD_WIDTH;
    y[i] = fast_rand() % LCD_HEIGHT;
    speed[i] = 2 + (fast_rand() % 5);
  }
  float phase = 0.0f;

  while (extra_continue(end, duration_ms)) {
    lcd_fill_screen(lcd_color565(5, 0, 18));
    for (int i = 0; i < 30; i++) {
      int sx = (i * 41 + 3) % LCD_WIDTH;
      int sy = (i * 17 + 5) % 52;
      lcd_draw_pixel(sx, sy, hsv_to_rgb565(205, 70, 80));
    }
    for (int b = 0, bx = 0; bx < LCD_WIDTH; b++) {
      int bw = 7 + (b * 3) % 10;
      int bh = 24 + (b * 11) % 45;
      lcd_fill_rect(bx, LCD_HEIGHT - bh, bw, bh, lcd_color565(13, 3, 28));
      lcd_draw_line(bx, LCD_HEIGHT - bh, bx + bw - 1, LCD_HEIGHT - bh,
                    hsv_to_rgb565(282, 170, 85));
      for (int wy = LCD_HEIGHT - bh + 5; wy < LCD_HEIGHT - 4; wy += 9) {
        if (((wy + b) % 3) == 0) {
          lcd_fill_rect(bx + 2, wy, 2, 2, hsv_to_rgb565(45, 180, 170));
        }
      }
      bx += bw + 2;
    }
    for (int i = 0; i < RAIN_DROPS; i++) {
      uint16_t c = hsv_to_rgb565((uint16_t)(190 + (i % 3) * 45), 190, 180);
      lcd_draw_line(x[i], y[i], x[i] - 2, y[i] + 7, c);
      y[i] += speed[i];
      x[i] += (int)sinf(phase + i) - 1;
      if (y[i] > LCD_HEIGHT + 8) {
        y[i] = -8;
        x[i] = fast_rand() % LCD_WIDTH;
      }
    }
    lcd_flush();
    phase += 0.08f;
    animation_delay(25);
  }
}

void anim_lowpoly_planet(uint32_t duration_ms) {
  absolute_time_t end;
  if (duration_ms > 0) end = make_timeout_time_ms(duration_ms);
  float angle = 0.0f;

  while (extra_continue(end, duration_ms)) {
    lcd_fill_screen(COLOR_BLACK);
    for (int i = 0; i < 26; i++) {
      lcd_draw_pixel((i * 53 + 11) % LCD_WIDTH, (i * 29 + 7) % LCD_HEIGHT,
                     hsv_to_rgb565(205, 70, 110));
    }
    int cx = LCD_WIDTH / 2;
    int cy = LCD_HEIGHT / 2;
    int r = 36;
    for (int yy = -r; yy <= r; yy += 2) {
      int half = (int)sqrtf((float)(r * r - yy * yy));
      uint8_t v = 70 + (uint8_t)((yy + r) * 2);
      lcd_draw_line(cx - half, cy + yy, cx + half, cy + yy,
                    hsv_to_rgb565(202, 170, v));
    }

    for (int blob = 0; blob < 9; blob++) {
      float lon = angle + blob * 0.92f;
      float lat = -0.75f + (float)(blob % 5) * 0.36f;
      float front = cosf(lon);
      if (front < -0.15f) continue;
      int bx = cx + (int)(sinf(lon) * cosf(lat) * (float)r);
      int by = cy + (int)(sinf(lat) * (float)r * 0.82f);
      int bw = 6 + (blob % 3) * 4;
      int bh = 4 + (blob % 2) * 3;
      uint16_t land = hsv_to_rgb565(112, 210, 120 + (uint8_t)(front * 70.0f));
      lcd_fill_rect(bx - bw / 2, by - bh / 2, bw, bh, land);
      lcd_draw_line(bx - bw, by, bx + bw, by + (blob % 3) - 1, land);
    }

    for (int lat = -2; lat <= 2; lat++) {
      int yy = cy + lat * 10;
      int half = (int)sqrtf((float)(r * r - (yy - cy) * (yy - cy)));
      lcd_draw_line(cx - half, yy, cx + half, yy, hsv_to_rgb565(190, 130, 75));
    }
    for (int lon = 0; lon < 5; lon++) {
      float off = angle + lon * 0.65f;
      int ex = (int)(cosf(off) * r);
      lcd_draw_line(cx + ex, cy - r + 3, cx - ex, cy + r - 3,
                    hsv_to_rgb565(190, 120, 70));
    }
    lcd_draw_circle(cx, cy, r, hsv_to_rgb565(190, 255, 245));
    lcd_draw_line(cx - 55, cy + 16, cx + 55, cy - 16, hsv_to_rgb565(42, 220, 170));
    lcd_draw_line(cx - 52, cy + 20, cx + 52, cy - 12, hsv_to_rgb565(42, 180, 115));
    lcd_flush();
    angle += 0.035f;
    animation_delay(30);
  }
}

void anim_vhs_glitch(uint32_t duration_ms) {
  absolute_time_t end;
  if (duration_ms > 0) end = make_timeout_time_ms(duration_ms);
  uint32_t frame = 0;

  while (extra_continue(end, duration_ms)) {
    lcd_fill_screen(lcd_color565(6, 6, 10));
    for (int y = 0; y < LCD_HEIGHT; y += 2) {
      for (int x = 0; x < LCD_WIDTH; x += 4) {
        uint8_t n = fast_rand() & 0x3F;
        lcd_draw_line(x, y, x + 2, y, lcd_color565(n, n, n + 12));
      }
    }
    for (int y = 0; y < LCD_HEIGHT; y += 4) {
      lcd_draw_line(0, y, LCD_WIDTH - 1, y, lcd_color565(0, 0, 0));
    }
    for (int band = 0; band < 9; band++) {
      int y = (band * 23 + frame * (band + 2)) % LCD_HEIGHT;
      int shift = (int)(fast_rand() % 31) - 15;
      uint16_t c = hsv_to_rgb565((frame * 3 + band * 55) % 360, 110, 165);
      lcd_draw_line(shift, y, LCD_WIDTH - 1 + shift, y, c);
      lcd_draw_line(-shift, y + 1, LCD_WIDTH - 1 - shift, y + 1, c);
    }
    if ((frame % 45) < 8) {
      lcd_draw_string(24 + (fast_rand() % 5), 54, "NO SIGNAL", COLOR_WHITE,
                      COLOR_BLACK, 2);
    } else {
      lcd_draw_string(31, 54, "PICO VHS", hsv_to_rgb565(190, 100, 200),
                      COLOR_BLACK, 2);
    }
    lcd_flush();
    frame++;
    animation_delay(35);
  }
}

void anim_metaballs(uint32_t duration_ms) {
  absolute_time_t end;
  if (duration_ms > 0) end = make_timeout_time_ms(duration_ms);

#define BALLS 4
  float bx[BALLS] = {28, 70, 116, 92};
  float by[BALLS] = {34, 86, 48, 102};
  float vx[BALLS] = {1.2f, -0.9f, 1.0f, -1.1f};
  float vy[BALLS] = {0.8f, 1.0f, -0.7f, -0.9f};

  while (extra_continue(end, duration_ms)) {
    lcd_fill_screen(COLOR_BLACK);
    for (int i = 0; i < BALLS; i++) {
      bx[i] += vx[i];
      by[i] += vy[i];
      if (bx[i] < 14 || bx[i] > LCD_WIDTH - 14) vx[i] = -vx[i];
      if (by[i] < 14 || by[i] > LCD_HEIGHT - 14) vy[i] = -vy[i];
    }
    for (int y = 0; y < LCD_HEIGHT; y += 2) {
      for (int x = 0; x < LCD_WIDTH; x += 2) {
        float sum = 0.0f;
        for (int i = 0; i < BALLS; i++) {
          float dx = (float)x - bx[i];
          float dy = (float)y - by[i];
          sum += 210.0f / (dx * dx + dy * dy + 24.0f);
        }
        if (sum > 0.78f && sum < 1.55f) {
          uint8_t v = (uint8_t)(130.0f + sum * 70.0f);
          lcd_fill_rect(x, y, 2, 2, hsv_to_rgb565((uint16_t)(185 + v / 5), 220, v));
        }
      }
    }
    for (int i = 0; i < BALLS; i++) {
      lcd_draw_circle((int)bx[i], (int)by[i], 12, hsv_to_rgb565(190, 255, 210));
    }
    lcd_flush();
    animation_delay(35);
  }
}
