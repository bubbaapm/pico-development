#include "../lcd_animations.h"
#include "../buttons.h"
#include "../ld2410.h"
#include "../st7735.h"
#include "pico/stdlib.h"
#include <math.h>
#include <stdlib.h>

// ─── 3D Wireframe Cube ──────────────────────────────────────────────────────

void anim_3d_cube(uint32_t duration_ms) {
  lcd_fill_screen(COLOR_BLACK);

  // 8 vertices of a cube (centered at 0, 0, 0)
  float vertices[8][3] = {
      {-20, -20, -20}, {20, -20, -20}, {20, 20, -20}, {-20, 20, -20},
      {-20, -20, 20},  {20, -20, 20},  {20, 20, 20},  {-20, 20, 20}};

  // 12 edges connecting the vertices
  int edges[12][2] = {
      {0, 1}, {1, 2}, {2, 3}, {3, 0}, // Back face
      {4, 5}, {5, 6}, {6, 7}, {7, 4}, // Front face
      {0, 4}, {1, 5}, {2, 6}, {3, 7}  // Connectors
  };

  float angle_x = 0;
  float angle_y = 0;
  float angle_z = 0;

  absolute_time_t end;
  if (duration_ms > 0) {
    end = make_timeout_time_ms(duration_ms);
  }
  uint16_t color_hue = 0;

  while (duration_ms == 0 || !time_reached(end)) {
    buttons_update();
    if (buttons_any_event())
      return;

    radar_data_t rdata = radar_get_data();
    float speed_scale = 1.0f;
    if (rdata.data_valid) {
      uint8_t energy = rdata.moving_energy > rdata.stationary_energy
                           ? rdata.moving_energy
                           : rdata.stationary_energy;
      if (energy > 0) {
        speed_scale = 1.0f + (energy * 0.02f);
      }
    }

    angle_x += 0.005f * speed_scale;
    angle_y += 0.007f * speed_scale;
    angle_z += 0.003f * speed_scale;

    int px[8], py[8];
    float rad_x = angle_x;
    float rad_y = angle_y;
    float rad_z = angle_z;

    float sin_x = sinf(rad_x), cos_x = cosf(rad_x);
    float sin_y = sinf(rad_y), cos_y = cosf(rad_y);
    float sin_z = sinf(rad_z), cos_z = cosf(rad_z);

    for (int i = 0; i < 8; i++) {
      float x = vertices[i][0];
      float y = vertices[i][1];
      float z = vertices[i][2];

      float y1 = y * cos_x - z * sin_x;
      float z1 = y * sin_x + z * cos_x;

      float x2 = x * cos_y + z1 * sin_y;
      float z2 = -x * sin_y + z1 * cos_y;

      float x3 = x2 * cos_z - y1 * sin_z;
      float y3 = x2 * sin_z + y1 * cos_z;

      float distance = 60.0f;
      float scale = 90.0f;
      float proj_z = z2 + distance;
      if (proj_z == 0)
        proj_z = 0.1f;

      px[i] = (int)(LCD_WIDTH / 2 + (x3 * scale) / proj_z);
      py[i] = (int)(LCD_HEIGHT / 2 + (y3 * scale) / proj_z);
    }

    lcd_fill_screen(COLOR_BLACK);

    uint16_t draw_color;
    if (rdata.data_valid && (rdata.moving_distance_cm > 0 ||
                             rdata.stationary_distance_cm > 0)) {
      uint16_t d = rdata.moving_distance_cm > 0 ? rdata.moving_distance_cm
                                                : rdata.stationary_distance_cm;
      if (d > 600)
        d = 600;
      uint16_t hue = (d * 240) / 600;
      draw_color = hsv_to_rgb565(hue, 255, 255);
    } else {
      color_hue = (color_hue + 2) % 360;
      draw_color = hsv_to_rgb565(color_hue, 255, 255);
    }

    for (int i = 0; i < 12; i++) {
      lcd_draw_line(px[edges[i][0]], py[edges[i][0]], px[edges[i][1]],
                    py[edges[i][1]], draw_color);
    }

    lcd_flush();
    animation_delay(30);
  }
}

// ─── 3D Spinning Torus ──────────────────────────────────────────────────────

void anim_3d_torus(uint32_t duration_ms) {
  absolute_time_t end;
  if (duration_ms > 0) {
    end = make_timeout_time_ms(duration_ms);
  }

  float angle_x = 0;
  float angle_y = 0;
  const float R1 = 25.0f;
  const float R2 = 12.0f;

  uint16_t hue = 0;
  while (duration_ms == 0 || !time_reached(end)) {
    buttons_update();
    if (buttons_any_event())
      return;

    lcd_fill_screen(COLOR_BLACK);

    radar_data_t rdata = radar_get_data();
    float speed = 1.0f;
    if (rdata.data_valid && rdata.target_state != RADAR_TARGET_NONE) {
      uint8_t energy = rdata.moving_energy > rdata.stationary_energy ? rdata.moving_energy : rdata.stationary_energy;
      speed = 1.0f + (energy * 0.02f);
    }

    angle_x += 0.01f * speed;
    angle_y += 0.015f * speed;

    float sin_x = sinf(angle_x), cos_x = cosf(angle_x);
    float sin_y = sinf(angle_y), cos_y = cosf(angle_y);

    for (float theta = 0; theta < 6.28f; theta += 0.4f) {
      float costheta = cosf(theta), sintheta = sinf(theta);
      for (float phi = 0; phi < 6.28f; phi += 0.2f) {
        float cosphi = cosf(phi), sinphi = sinf(phi);

        float x = (R1 + R2 * costheta) * cosphi;
        float y = (R1 + R2 * costheta) * sinphi;
        float z = R2 * sintheta;

        float y1 = y * cos_x - z * sin_x;
        float z1 = y * sin_x + z * cos_x;

        float x2 = x * cos_y + z1 * sin_y;
        float z2 = -x * sin_y + z1 * cos_y;

        float distance = 60.0f;
        float scale = 95.0f;
        float proj_z = z2 + distance;
        if (proj_z <= 0.1f) proj_z = 0.1f;

        int px = (int)(LCD_WIDTH / 2.0f + (x2 * scale) / proj_z);
        int py = (int)(LCD_HEIGHT / 2.0f + (y1 * scale) / proj_z);

        if (px >= 0 && px < LCD_WIDTH && py >= 0 && py < LCD_HEIGHT) {
          uint8_t br = (uint8_t)(150.0f + 105.0f * (z2 / R2));
          uint16_t color = hsv_to_rgb565((hue + (int)(phi * 57.0f)) % 360, 255, br);
          lcd_draw_pixel(px, py, color);
        }
      }
    }

    lcd_flush();
    hue = (hue + 3) % 360;
    animation_delay(30);
  }
}
