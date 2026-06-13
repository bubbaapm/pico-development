#include "../lcd_animations.h"
#include "../buttons.h"
#include "../ld2410.h"
#include "../st7735.h"
#include "pico/stdlib.h"
#include <math.h>
#include <stdlib.h>

// ─── Vector Zoom Tunnel ─────────────────────────────────────────────────────

void anim_vector_tunnel(uint32_t duration_ms) {
  absolute_time_t end;
  if (duration_ms > 0) {
    end = make_timeout_time_ms(duration_ms);
  }

  float angle = 0.0f;
  float depth = 0.0f;
  uint16_t hue = 288;

  while (duration_ms == 0 || !time_reached(end)) {
    buttons_update();
    if (buttons_any_event())
      return;

    lcd_fill_screen(COLOR_BLACK);

    radar_data_t rdata = radar_get_data();
    float speed = 1.0f;
    if (rdata.data_valid && rdata.target_state != RADAR_TARGET_NONE) {
      uint8_t energy = rdata.moving_energy > rdata.stationary_energy ? rdata.moving_energy : rdata.stationary_energy;
      speed = 1.0f + (energy * 0.03f);
    }

    depth += 0.028f * speed;
    if (depth >= 1.0f) {
      depth -= 1.0f;
    }
    angle += 0.012f * speed;

    float cx = LCD_WIDTH / 2.0f;
    float cy = LCD_HEIGHT / 2.0f;

    int px[12][4];
    int py[12][4];
    uint16_t colors[12];
    bool valid[12];

    // Compute square corner projections
    for (int i = 0; i < 12; i++) {
      float layer = (float)i + depth;
      float size = powf(1.315f, layer) * 8.0f;
      valid[i] = (size <= 210.0f);
      if (!valid[i]) continue;

      float a = angle + layer * 0.18f;
      float sin_a = sinf(a), cos_a = cosf(a);

      float half_sz = size / 2.0f;
      float corners[4][2] = {
        {-half_sz, -half_sz},
        {half_sz, -half_sz},
        {half_sz, half_sz},
        {-half_sz, half_sz}
      };

      for (int c = 0; c < 4; c++) {
        px[i][c] = (int)(cx + (corners[c][0] * cos_a - corners[c][1] * sin_a));
        py[i][c] = (int)(cy + (corners[c][0] * sin_a + corners[c][1] * cos_a));
      }

      uint8_t val = 70 + (uint8_t)(layer * 15.0f);
      if (val > 255) val = 255;
      if (size > 135.0f) {
        float f = (size - 135.0f) / 75.0f;
        if (f > 1.0f) f = 1.0f;
        val = (uint8_t)((float)val * (1.0f - f));
      }
      colors[i] = hsv_to_rgb565((hue + (int)(layer * 9.0f)) % 360, 230, val);
    }

    // Draw squares
    for (int i = 0; i < 12; i++) {
      if (!valid[i]) continue;
      for (int c = 0; c < 4; c++) {
        int next_c = (c + 1) % 4;
        lcd_draw_line(px[i][c], py[i][c], px[i][next_c], py[i][next_c], colors[i]);
      }
    }

    // Draw longitudinal lines connecting corners of consecutive squares
    for (int i = 0; i < 11; i++) {
      if (valid[i] && valid[i+1]) {
        for (int c = 0; c < 4; c++) {
          lcd_draw_line(px[i][c], py[i][c], px[i+1][c], py[i+1][c], colors[i]);
        }
      }
    }

    lcd_flush();
    hue = (hue + 1) % 360;
    animation_delay(30);
  }
}

// ─── 3D Ring Tunnel ─────────────────────────────────────────────────────────

void anim_ring_tunnel(uint32_t duration_ms) {
  absolute_time_t end;
  if (duration_ms > 0) {
    end = make_timeout_time_ms(duration_ms);
  }

  float depth_offset = 18.0f;
  float angle_offset = 0.0f;
  uint16_t hue = 188;

  while (duration_ms == 0 || !time_reached(end)) {
    buttons_update();
    if (buttons_any_event())
      return;

    lcd_fill_screen(COLOR_BLACK);

    radar_data_t rdata = radar_get_data();
    float speed = 1.0f;
    if (rdata.data_valid && rdata.target_state != RADAR_TARGET_NONE) {
      uint8_t energy = rdata.moving_energy > rdata.stationary_energy ? rdata.moving_energy : rdata.stationary_energy;
      speed = 1.0f + (energy / 25.0f);
    }

    depth_offset -= 1.15f * speed;
    if (depth_offset <= 2.4f) depth_offset += 18.0f;
    angle_offset += 0.01f * speed;

    float cx = LCD_WIDTH / 2.0f;
    float cy = LCD_HEIGHT / 2.0f;

    float px[8][16];
    float py[8][16];
    uint16_t colors[8];
    bool valid[8];
    int centers_x[8];
    int centers_y[8];
    int ring_radii[8];

    for (int i = 0; i < 8; i++) {
      float z = depth_offset + (float)i * 18.0f;
      float radius = (34.0f * 62.0f) / z;
      float bend_x = sinf(angle_offset + z * 0.035f) * (z * 0.08f);
      float bend_y = cosf(angle_offset * 0.8f + z * 0.025f) * (z * 0.045f);
      float center_x = cx + bend_x;
      float center_y = cy + bend_y;
      valid[i] = radius > 4.0f && radius < 760.0f;
      centers_x[i] = (int)center_x;
      centers_y[i] = (int)center_y;
      ring_radii[i] = (int)radius;
      colors[i] = hsv_to_rgb565((hue + i * 12) % 360, 220,
                                (uint8_t)(245 - i * 18));
      if (!valid[i]) continue;
      for (int k = 0; k < 16; k++) {
        float theta = (float)k * 3.14159f / 8.0f + angle_offset + z * 0.015f;
        px[i][k] = center_x + cosf(theta) * radius;
        py[i][k] = center_y + sinf(theta) * radius;
      }
    }

    for (int i = 0; i < 8; i++) {
      if (!valid[i]) continue;
      lcd_draw_circle(centers_x[i], centers_y[i], ring_radii[i], colors[i]);
    }
    for (int i = 0; i < 7; i++) {
      if (valid[i] && valid[i+1]) {
        for (int k = 0; k < 16; k++) {
          lcd_draw_line((int)px[i][k], (int)py[i][k], (int)px[i+1][k], (int)py[i+1][k], colors[i]);
        }
      }
    }

    lcd_flush();
    hue = (hue + 1) % 360;
    animation_delay(30);
  }
}

// ─── 3D Star Tunnel ─────────────────────────────────────────────────────────

void anim_star_tunnel(uint32_t duration_ms) {
#define ST_STARS 40
  struct st_star {
    float angle;
    float r;
    float z;
  };
  struct st_star stars[ST_STARS];
  for (int i = 0; i < ST_STARS; i++) {
    stars[i].angle = (float)(fast_rand() % 360) * 3.14159f / 180.0f;
    stars[i].r = (float)(20 + (fast_rand() % 40));
    stars[i].z = (float)(1 + (fast_rand() % 100));
  }

  absolute_time_t end;
  if (duration_ms > 0) {
    end = make_timeout_time_ms(duration_ms);
  }

  float depth_offset = 25.0f;
  float t = 0.0f;
  uint16_t hue = 0;

  while (duration_ms == 0 || !time_reached(end)) {
    buttons_update();
    if (buttons_any_event())
      return;

    lcd_fill_screen(COLOR_BLACK);
    float cx = LCD_WIDTH / 2.0f;
    float cy = LCD_HEIGHT / 2.0f;

    radar_data_t rdata = radar_get_data();
    float speed = 1.0f;
    if (rdata.data_valid && rdata.target_state != RADAR_TARGET_NONE) {
      uint8_t energy = rdata.moving_energy > rdata.stationary_energy ? rdata.moving_energy : rdata.stationary_energy;
      speed = 1.0f + (energy * 0.02f);
    }

    t += 0.03f * speed;
    float curve_x = sinf(t) * 25.0f;
    float curve_y = cosf(t * 0.75f) * 15.0f;

    // Shrink tunnel size by setting scale factor to 60.0f instead of 80.0f
    float tunnel_scale = 60.0f;

    // Project and draw stars
    for (int i = 0; i < ST_STARS; i++) {
      stars[i].z -= 1.5f * speed;
      if (stars[i].z <= 1.0f) {
        stars[i].z = 100.0f;
        stars[i].angle = (float)(fast_rand() % 360) * 3.14159f / 180.0f;
        stars[i].r = (float)(20 + (fast_rand() % 40));
      }

      float sz = stars[i].z;
      float x = cosf(stars[i].angle) * stars[i].r;
      float y = sinf(stars[i].angle) * stars[i].r;

      float cx_shifted = cx + curve_x * (sz / 100.0f);
      float cy_shifted = cy + curve_y * (sz / 100.0f);

      int px = (int)(cx_shifted + (x * tunnel_scale) / sz);
      int py = (int)(cy_shifted + (y * tunnel_scale) / sz);

      if (px >= 0 && px < LCD_WIDTH && py >= 0 && py < LCD_HEIGHT) {
        uint8_t br = (uint8_t)(255.0f * (1.0f - (sz / 100.0f)));
        uint16_t color = hsv_to_rgb565((hue + (int)sz) % 360, 255, br);
        lcd_draw_pixel(px, py, color);
        
        if (sz < 30.0f) {
          lcd_draw_pixel(px + 1, py, color);
          lcd_draw_pixel(px - 1, py, color);
          lcd_draw_pixel(px, py + 1, color);
          lcd_draw_pixel(px, py - 1, color);
        }
      }
    }

    // Scroll depth offset smoothly
    depth_offset -= 1.5f * speed;
    if (depth_offset <= 0.0f) {
      depth_offset += 25.0f;
    }

    float rx[4][8];
    float ry[4][8];
    uint16_t rcolors[4];
    bool rvalid[4];

    // Project 4 guide rings sorted by depth
    for (int r = 0; r < 4; r++) {
      float rz = depth_offset + (float)r * 25.0f;
      int radius = (int)((30.0f * tunnel_scale) / rz);
      rvalid[r] = (radius > 2 && radius < 120);
      if (rvalid[r]) {
        uint8_t br = (uint8_t)(255.0f * (1.0f - (rz / 100.0f)));
        rcolors[r] = hsv_to_rgb565((hue + radius) % 360, 255, br / 4);

        float cx_shifted = cx + curve_x * (rz / 100.0f);
        float cy_shifted = cy + curve_y * (rz / 100.0f);

        // Draw ring
        lcd_draw_circle((int)cx_shifted, (int)cy_shifted, radius, rcolors[r]);

        // Precompute 8 vertices on each ring
        for (int k = 0; k < 8; k++) {
          float theta = (float)k * 3.14159f / 4.0f;
          rx[r][k] = cx_shifted + cosf(theta) * radius;
          ry[r][k] = cy_shifted + sinf(theta) * radius;
        }
      }
    }

    // Draw longitudinal lines connecting rings
    for (int r = 0; r < 3; r++) {
      if (rvalid[r] && rvalid[r+1]) {
        for (int k = 0; k < 8; k++) {
          lcd_draw_line((int)rx[r][k], (int)ry[r][k], (int)rx[r+1][k], (int)ry[r+1][k], rcolors[r]);
        }
      }
    }

    lcd_flush();
    hue = (hue + 2) % 360;
    animation_delay(30);
  }
}
