#include "../lcd_animations.h"
#include "../buttons.h"
#include "../ld2410.h"
#include "../st7735.h"
#include "pico/stdlib.h"
#include <math.h>

#define SYN_COLS 17
#define SYN_ROWS 14

static float radar_anim_scale(float *speed_scale) {
  radar_data_t rdata = radar_get_data();
  float amp_scale = 1.0f;
  *speed_scale = 1.0f;
  if (rdata.data_valid && rdata.target_state != RADAR_TARGET_NONE) {
    uint8_t energy = rdata.moving_energy > rdata.stationary_energy
                         ? rdata.moving_energy
                         : rdata.stationary_energy;
    amp_scale = 1.0f + (energy / 80.0f);
    *speed_scale = 1.0f + (energy / 70.0f);
  }
  return amp_scale;
}

static void draw_synth_sky(int horizon, uint8_t red_bias) {
  for (int y = 0; y < horizon; y += 2) {
    uint8_t r = red_bias + (uint8_t)(y / 5);
    uint8_t b = (uint8_t)(24 + (y * 56) / horizon);
    lcd_draw_line(0, y, LCD_WIDTH - 1, y, lcd_color565(r, 0, b));
  }
}

static void draw_horizon_glow(int horizon, uint16_t hue) {
  for (int y = horizon - 18; y < horizon + 12; y += 2) {
    if (y < 0 || y >= LCD_HEIGHT) continue;
    int d = y > horizon ? y - horizon : horizon - y;
    uint8_t v = 120 - d * 5;
    if (v < 20) v = 20;
    lcd_draw_line(0, y, LCD_WIDTH - 1, y, hsv_to_rgb565(hue, 180, v));
  }
}

static void draw_stars(float phase, int count, int max_y) {
  for (int i = 0; i < count; i++) {
    int x = (i * 37 + 19) % LCD_WIDTH;
    int y = (i * 23 + 7) % max_y;
    uint8_t v = 70 + (uint8_t)((sinf(phase * 0.45f + (float)i) + 1.0f) * 58.0f);
    lcd_draw_pixel(x, y, hsv_to_rgb565(205, 70, v));
  }
}

static void draw_segmented_sun(int cx, int cy, int r, float phase,
                               uint16_t hue, int horizon) {
  for (int y = -r; y <= r; y += 2) {
    if (cy + y >= horizon) continue;
    int stripe = (y + r) / 5;
    if (stripe > 3 && (stripe % 2) == 1) {
      continue;
    }
    int half = (int)sqrtf((float)(r * r - y * y));
    uint8_t val = (uint8_t)(210 + ((r - y) * 45) / (r * 2));
    int wobble = (int)(sinf(phase + (float)y * 0.17f) * 1.2f);
    lcd_draw_line(cx - half + wobble, cy + y, cx + half + wobble, cy + y,
                  hsv_to_rgb565(hue, 190, val));
  }
}

static void draw_filled_sun(int cx, int cy, int r, uint16_t hue, int horizon) {
  for (int y = -r; y <= r; y++) {
    if (cy + y >= horizon) continue;
    int half = (int)sqrtf((float)(r * r - y * y));
    uint8_t val = (uint8_t)(205 + ((r - y) * 50) / (r * 2));
    lcd_draw_line(cx - half, cy + y, cx + half, cy + y,
                  hsv_to_rgb565(hue, 180, val));
  }
}

static void draw_wire_mountains(int horizon, float phase, uint16_t hue) {
  int last_x = 0;
  int last_y = horizon;
  for (int x = 0; x < LCD_WIDTH; x += 5) {
    float a = (float)x * 0.052f + phase * 0.2f;
    int y = horizon - 5 - (int)(sinf(a) * 8.0f + sinf(a * 0.47f) * 6.0f);
    if (x == 0) {
      last_y = y;
    }
    lcd_draw_line(last_x, last_y, x, y, hsv_to_rgb565(hue, 220, 185));
    lcd_draw_line(last_x, last_y + 2, x, y + 2,
                  hsv_to_rgb565((hue + 95) % 360, 240, 95));
    last_x = x;
    last_y = y;
  }
}

static void draw_layered_hills(int horizon, float phase) {
  for (int layer = 0; layer < 3; layer++) {
    int last_x = 0;
    int last_y = horizon + 10 + layer * 8;
    uint16_t c = layer == 0 ? hsv_to_rgb565(222, 255, 125)
                            : hsv_to_rgb565(302, 230, 100 - layer * 20);
    for (int x = 0; x < LCD_WIDTH; x += 4) {
      float a = phase * (0.16f + layer * 0.04f) + x * (0.035f + layer * 0.012f);
      int y = horizon + 2 + layer * 10 -
              (int)(sinf(a) * (6 + layer * 5) + sinf(a * 0.47f) * 5);
      lcd_draw_line(last_x, last_y, x, y, c);
      last_x = x;
      last_y = y;
    }
  }
}

static void project_floor_point(int col, float row, float phase,
                                float amp_scale, int horizon, int style,
                                int *x, int *y) {
  float lane = (float)(col - SYN_COLS / 2) / (float)(SYN_COLS / 2);
  float depth = row / (float)(SYN_ROWS - 1);
  float curve = depth * depth;
  float spread = 8.0f + curve * 132.0f;
  float base_y = (float)horizon + curve * (float)(LCD_HEIGHT - horizon + 42);
  float wave = 0.0f;

  if (style == 0) {
    wave = sinf(phase + lane * 2.2f + depth * 4.4f) * (0.35f + fabsf(lane)) *
           curve * 4.6f * amp_scale;
  } else {
    wave = (sinf(phase + lane * 5.4f + depth * 8.0f) +
            cosf(phase * 0.62f - lane * 3.0f)) *
           curve * 6.2f * amp_scale;
    spread += sinf(depth * 6.0f + phase * 0.35f) * curve * 17.0f;
  }

  *x = (int)(LCD_WIDTH / 2.0f + lane * spread);
  *y = (int)(base_y + wave);
}

static void draw_floor_mesh(float scroll, float phase, float amp_scale,
                            int horizon, int style, uint16_t row_hue,
                            uint16_t col_hue) {
  int px[SYN_ROWS][SYN_COLS];
  int py[SYN_ROWS][SYN_COLS];
  for (int r = 0; r < SYN_ROWS; r++) {
    float row = (float)r + scroll;
    for (int c = 0; c < SYN_COLS; c++) {
      project_floor_point(c, row, phase, amp_scale, horizon, style, &px[r][c],
                          &py[r][c]);
    }
  }

  for (int c = 0; c < SYN_COLS; c++) {
    uint8_t v = (c == SYN_COLS / 2) ? 220 : 135;
    for (int r = 0; r < SYN_ROWS - 1; r++) {
      if (py[r][c] >= horizon || py[r + 1][c] >= horizon) {
        lcd_draw_line(px[r][c], py[r][c], px[r + 1][c], py[r + 1][c],
                      hsv_to_rgb565(col_hue, 225, v));
      }
    }
  }

  for (int r = 0; r < SYN_ROWS; r++) {
    uint8_t v = 90 + r * 12;
    if (v > 245) v = 245;
    for (int c = 0; c < SYN_COLS - 1; c++) {
      if (py[r][c] >= horizon || py[r][c + 1] >= horizon) {
        lcd_draw_line(px[r][c], py[r][c], px[r][c + 1], py[r][c + 1],
                      hsv_to_rgb565(row_hue, 245, v));
      }
    }
  }
}

static void project_grid3d(float wx, float wz, int horizon, int *sx, int *sy) {
  float z = wz;
  if (z < 8.0f) z = 8.0f;
  *sx = (int)(LCD_WIDTH / 2.0f + (wx * 220.0f) / z);
  *sy = (int)((float)horizon + 840.0f / z);
}

static void draw_projected_floor(float scroll, int horizon, uint16_t row_hue,
                                 uint16_t col_hue) {
  const float near_z = 7.0f;
  const float far_z = 190.0f;
  const float row_step = 18.0f;
  const float col_step = 8.0f;

  for (float wx = -160.0f; wx <= 160.0f; wx += col_step) {
    if (wx > -0.1f && wx < 0.1f) continue;
    int prev_x = 0;
    int prev_y = 0;
    bool have_prev = false;
    for (float wz = far_z; wz >= near_z; wz -= 7.0f) {
      int x, y;
      project_grid3d(wx, wz, horizon, &x, &y);
      if (have_prev) {
        lcd_draw_line(prev_x, prev_y, x, y, hsv_to_rgb565(col_hue, 230, 155));
      }
      prev_x = x;
      prev_y = y;
      have_prev = true;
    }
  }

  for (int r = 0; r < 12; r++) {
    float wz = near_z + fmodf((float)r * row_step - scroll * row_step +
                                  (far_z - near_z),
                               far_z - near_z);
    int prev_x = 0;
    int prev_y = 0;
    bool have_prev = false;
    for (float wx = -132.0f; wx <= 132.0f; wx += 8.0f) {
      int x, y;
      project_grid3d(wx, wz, horizon, &x, &y);
      if (have_prev) {
        uint8_t v = 90 + (uint8_t)((far_z - wz) * 0.75f);
        if (v > 245) v = 245;
        lcd_draw_line(prev_x, prev_y, x, y, hsv_to_rgb565(row_hue, 245, v));
      }
      prev_x = x;
      prev_y = y;
      have_prev = true;
    }
  }
}

static void sort_depths(float *depths, int count) {
  for (int i = 1; i < count; i++) {
    float v = depths[i];
    int j = i - 1;
    while (j >= 0 && depths[j] > v) {
      depths[j + 1] = depths[j];
      j--;
    }
    depths[j + 1] = v;
  }
}

static float noise1d(float z) {
  int z0 = (int)floorf(z);
  int z1 = z0 + 1;
  float t = z - (float)z0;
  float fade = t * t * (3.0f - 2.0f * t); // smoothstep
  
  // Deterministic integer hash mapped to 0..1
  #define HASH(x) ((float)(((int)(x) * 1234567891U + 987654321U) % 10007) / 10007.0f)
  float h0 = HASH(z0);
  float h1 = HASH(z1);
  return h0 + (h1 - h0) * fade;
}

static float terrain_noise(int c, float z) {
  float seed = (float)c * 17.31f;
  float z_scaled = z * 0.05f;
  
  // Octave 1: low frequency, high amplitude
  float n1 = noise1d(z_scaled + seed);
  // Octave 2: high frequency, low amplitude
  float n2 = noise1d(z_scaled * 2.5f + seed * 1.5f);
  
  return n1 * 0.72f + n2 * 0.28f;
}

static float get_column_scale(int c) {
  float dist_from_center = fabsf((float)c - 11.0f);
  float norm_dist = dist_from_center / 11.0f;
  float curve = norm_dist * norm_dist * norm_dist; // cubic curve for a wide valley floor
  return 0.08f + curve * 0.92f;
}

static float terrain_height(int c, float wz) {
  if (c < 0 || c >= 23) return 0.0f;
  float noise = terrain_noise(c, wz);
  float base_max_height = 42.0f;
  float h = noise * base_max_height * get_column_scale(c);
  
  // Fade out mountain height in foreground to prevent grid lines from folding up at the bottom
  float near_fade = (wz - 4.0f) / 16.0f;
  if (near_fade < 0.0f) near_fade = 0.0f;
  if (near_fade > 1.0f) near_fade = 1.0f;

  return h * near_fade;
}

static void project_terrain_point(int c, float wx, float wz, int horizon,
                                  int *sx, int *sy) {
  if (wz < 2.0f) wz = 2.0f; // clamp to prevent division by zero or negative Z
  float sx_proj = 80.0f + wx * 75.0f / wz;
  float h = terrain_height(c, wz);
  
  float y_floor = (float)horizon + 580.0f / wz;
  float y_lift = h * 160.0f / (wz + 30.0f);
  
  *sx = (int)sx_proj;
  *sy = (int)(y_floor - y_lift);
}

static void draw_terrain_mesh(float scroll, float phase, int horizon) {
  (void)phase;
  enum { T_ROWS = 22, T_COLS = 23 };
  const float near_z = 4.0f;
  const float far_z = 172.0f;
  const float row_step = (far_z - near_z) / (float)(T_ROWS - 1);
  const float col_step = 6.5f;
  float depths[T_ROWS];
  int px[T_ROWS][T_COLS];
  int py[T_ROWS][T_COLS];

  // Compute depths without wrapping individually so they maintain sorted order (r=0 is closest, r=T_ROWS-1 is furthest)
  for (int r = 0; r < T_ROWS; r++) {
    float z = near_z + (float)r * row_step - scroll * row_step;
    if (z < 2.0f) z = 2.0f; // clamp to prevent negative Z flip
    depths[r] = z;
  }

  for (int r = 0; r < T_ROWS; r++) {
    for (int c = 0; c < T_COLS; c++) {
      float wx = ((float)c - (float)(T_COLS / 2)) * col_step;
      project_terrain_point(c, wx, depths[r], horizon, &px[r][c],
                            &py[r][c]);
    }
  }

  // Draw quads back-to-front (depths[0] is closest, depths[T_ROWS-1] is furthest)
  // So we run the loop from r = T_ROWS - 2 down to 0
  for (int r = T_ROWS - 2; r >= 0; r--) {
    // Fill the quad polygons in this row first to perform hidden-line removal
    for (int c = 0; c < T_COLS - 1; c++) {
      lcd_fill_triangle(px[r][c], py[r][c], px[r][c+1], py[r][c+1], px[r+1][c], py[r+1][c], COLOR_BLACK);
      lcd_fill_triangle(px[r][c+1], py[r][c+1], px[r+1][c+1], py[r+1][c+1], px[r+1][c], py[r+1][c], COLOR_BLACK);
    }

    // Now draw the wireframe lines on top of the black filled quad
    float z_r = depths[r];     // closer depth
    float z_r1 = depths[r+1];  // further depth
    
    uint8_t v_r = 80 + (uint8_t)((far_z - z_r) * 0.95f);
    if (v_r > 245) v_r = 245;

    uint8_t v_r1 = 80 + (uint8_t)((far_z - z_r1) * 0.95f);
    if (v_r1 > 245) v_r1 = 245;

    // Draw horizontal segments for the row r+1 (further/back edge of current quad)
    for (int c = 0; c < T_COLS - 1; c++) {
      lcd_draw_line(px[r+1][c], py[r+1][c], px[r+1][c+1], py[r+1][c+1],
                    hsv_to_rgb565(196, 255, v_r1));
    }

    // If we are on the front-most row (r == 0), we also draw the front-most horizontal segments (row 0)
    if (r == 0) {
      for (int c = 0; c < T_COLS - 1; c++) {
        lcd_draw_line(px[0][c], py[0][c], px[0][c+1], py[0][c+1],
                      hsv_to_rgb565(196, 255, v_r));
      }
    }

    // Draw vertical/longitudinal segments connecting row r (closer) to r+1 (further)
    for (int c = 0; c < T_COLS; c++) {
      uint8_t v = (c == T_COLS / 2) ? 220 : 160;
      v = 80 + (uint8_t)((far_z - z_r) * 0.95f * (v / 220.0f));
      if (v > 245) v = 245;
      lcd_draw_line(px[r][c], py[r][c], px[r + 1][c], py[r + 1][c],
                    hsv_to_rgb565(202, 235, v));
    }
  }
}

static void draw_palm(int x, int base, int scale) {
  lcd_draw_line(x, base, x + scale / 2, base - scale * 4, COLOR_BLACK);
  int cx = x + scale / 2;
  int cy = base - scale * 4;
  for (int i = -3; i <= 3; i++) {
    int ex = cx + i * scale;
    int ey = cy + (i < 0 ? -i : i) * scale / 2 - scale;
    lcd_draw_line(cx, cy, ex, ey, COLOR_BLACK);
  }
}

static bool anim_continue(absolute_time_t end, uint32_t duration_ms) {
  buttons_update();
  return !buttons_any_event() && (duration_ms == 0 || !time_reached(end));
}

void anim_synth_grid(uint32_t duration_ms) {
  absolute_time_t end;
  if (duration_ms > 0) end = make_timeout_time_ms(duration_ms);

  float scroll = 0.0f;
  float phase = 0.0f;
  const int horizon = 52;

  while (anim_continue(end, duration_ms)) {
    float speed;
    float amp = radar_anim_scale(&speed);

    lcd_fill_screen(COLOR_BLACK);
    draw_synth_sky(horizon, 3);
    draw_stars(phase, 18, horizon - 8);
    draw_horizon_glow(horizon, 314);
    draw_segmented_sun(LCD_WIDTH / 2, 37, 24, phase, 32, horizon);
    draw_layered_hills(horizon, phase);
    draw_wire_mountains(horizon, phase, 204);
    lcd_draw_line(0, horizon, LCD_WIDTH - 1, horizon, hsv_to_rgb565(315, 255, 220));
    draw_projected_floor(scroll, horizon, 198, 205);

    lcd_flush();
    scroll += 0.035f * speed;
    if (scroll >= 1.0f) scroll -= 1.0f;
    phase += 0.038f * speed;
    animation_delay(25);
  }
}

void anim_synth_gradient(uint32_t duration_ms) {
  absolute_time_t end;
  if (duration_ms > 0) end = make_timeout_time_ms(duration_ms);

  float scroll = 0.0f;
  float phase = 0.0f;
  const int horizon = 57;

  while (anim_continue(end, duration_ms)) {
    float speed;
    (void)radar_anim_scale(&speed);

    lcd_fill_screen(COLOR_BLACK);
    for (int y = 0; y < horizon; y++) {
      uint8_t r = 20 + (uint8_t)(y * 3);
      uint8_t g = (y > horizon / 2) ? (uint8_t)((y - horizon / 2) * 3) : 0;
      uint8_t b = 95 + (uint8_t)(y / 2);
      lcd_draw_line(0, y, LCD_WIDTH - 1, y, lcd_color565(r, g, b));
    }
    draw_filled_sun(LCD_WIDTH / 2, 40, 28, 34, horizon);
    draw_projected_floor(scroll, horizon, 318, 286);

    lcd_flush();
    scroll += 0.06f * speed;
    if (scroll >= 1.0f) scroll -= 1.0f;
    phase += 0.03f * speed;
    animation_delay(25);
  }
}

void anim_synth_terrain(uint32_t duration_ms) {
  absolute_time_t end;
  if (duration_ms > 0) end = make_timeout_time_ms(duration_ms);

  float scroll = 0.0f;
  float phase = 0.0f;
  const int horizon = 44;

  while (anim_continue(end, duration_ms)) {
    float speed;
    (void)radar_anim_scale(&speed);

    lcd_fill_screen(COLOR_BLACK);
    
    // Smooth purple to pink-orange sky gradient
    for (int y = 0; y < horizon; y++) {
      float t = (float)y / (float)horizon;
      uint8_t r = (uint8_t)(25.0f + t * 180.0f);
      uint8_t g = (uint8_t)(0.0f + t * 15.0f);
      uint8_t b = (uint8_t)(55.0f + t * 25.0f);
      lcd_draw_line(0, y, LCD_WIDTH - 1, y, lcd_color565(r, g, b));
    }
    
    draw_horizon_glow(horizon, 314);
    draw_stars(phase, 16, horizon - 6);
    draw_segmented_sun(LCD_WIDTH / 2, 38, 26, phase, 32, horizon);
    
    draw_terrain_mesh(scroll, phase, horizon);

    lcd_flush();
    scroll += 0.035f * speed;
    if (scroll >= 1.0f) scroll -= 1.0f;
    phase += 0.032f * speed;
    animation_delay(25);
  }
}

void anim_synth_city(uint32_t duration_ms) {
  absolute_time_t end;
  if (duration_ms > 0) end = make_timeout_time_ms(duration_ms);

  float scroll = 0.0f;
  float phase = 0.0f;
  const int horizon = 58;

  while (anim_continue(end, duration_ms)) {
    float speed;
    (void)radar_anim_scale(&speed);

    lcd_fill_screen(COLOR_BLACK);
    draw_synth_sky(horizon, 8);
    draw_horizon_glow(horizon, 318);
    draw_segmented_sun(LCD_WIDTH / 2, 40, 25, phase, 28, horizon);

    for (int i = 0, x = 0; x < LCD_WIDTH; i++) {
      int w = 6 + (i * 5) % 9;
      int h = 12 + (i * 11) % 27;
      lcd_fill_rect(x, horizon - h, w, h, lcd_color565(4, 0, 14));
      if ((i % 3) == 0) {
        lcd_draw_pixel(x + 2, horizon - h + 5, hsv_to_rgb565(190, 90, 140));
      }
      x += w + 2;
    }
    draw_palm(10, horizon + 2, 7);
    draw_palm(142, horizon + 2, 8);

    lcd_draw_line(0, horizon, LCD_WIDTH - 1, horizon, hsv_to_rgb565(315, 255, 180));
    draw_projected_floor(scroll, horizon, 318, 286);

    lcd_flush();
    scroll += 0.07f * speed;
    if (scroll >= 1.0f) scroll -= 1.0f;
    phase += 0.035f * speed;
    animation_delay(25);
  }
}

void anim_synth_ocean(uint32_t duration_ms) {
  absolute_time_t end;
  if (duration_ms > 0) end = make_timeout_time_ms(duration_ms);

  float phase = 0.0f;
  const int horizon = 55;

  while (anim_continue(end, duration_ms)) {
    float speed;
    float amp = radar_anim_scale(&speed);

    lcd_fill_screen(COLOR_BLACK);
    draw_synth_sky(horizon, 4);
    draw_stars(phase, 20, horizon - 8);
    draw_horizon_glow(horizon, 190);
    draw_segmented_sun(LCD_WIDTH / 2, 42, 25, phase, 24, horizon);
    lcd_draw_line(0, horizon, LCD_WIDTH - 1, horizon, hsv_to_rgb565(190, 220, 150));

    for (int y = horizon + 3; y < LCD_HEIGHT + 18; y += 5) {
      float depth = (float)(y - horizon) / (float)(LCD_HEIGHT - horizon);
      int half = (int)(42.0f + depth * 125.0f);
      int cy = y + (int)(sinf(phase + depth * 8.0f) * depth * 6.0f * amp);
      for (int x = LCD_WIDTH / 2 - half; x < LCD_WIDTH / 2 + half; x += 5) {
        int yy = cy + (int)(sinf(phase * 1.4f + (float)x * 0.08f) * depth * 5.0f * amp);
        lcd_draw_line(x, yy, x + 4, yy, hsv_to_rgb565(190, 220, 95 + (uint8_t)(depth * 140.0f)));
      }
    }
    for (int i = 0; i < 14; i++) {
      int x = LCD_WIDTH / 2 + (int)(sinf(phase + i * 0.7f) * (5 + i * 6));
      int y = horizon + 4 + i * 5;
      lcd_draw_line(x - i * 2, y, x + i * 2, y, hsv_to_rgb565(32, 240, 180 - i * 5));
    }

    lcd_flush();
    phase += 0.045f * speed;
    animation_delay(25);
  }
}
