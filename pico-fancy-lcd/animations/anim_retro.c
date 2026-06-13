#include "../lcd_animations.h"
#include "../buttons.h"
#include "../ld2410.h"
#include "../st7735.h"
#include "pico/stdlib.h"
#include <string.h>
#include <stdio.h>

// ─── Conway's Game of Life ──────────────────────────────────────────────────

void anim_game_of_life(uint32_t duration_ms) {
#define LIFE_COLS 40
#define LIFE_ROWS 32
#define CELL_SZ 4

  uint8_t grid[LIFE_COLS][LIFE_ROWS];
  uint8_t next_grid[LIFE_COLS][LIFE_ROWS];

  for (int x = 0; x < LIFE_COLS; x++) {
    for (int y = 0; y < LIFE_ROWS; y++) {
      grid[x][y] = (fast_rand() % 4 == 0) ? 1 : 0;
    }
  }

  absolute_time_t end;
  if (duration_ms > 0) {
    end = make_timeout_time_ms(duration_ms);
  }
  uint16_t generation = 0;

  while (duration_ms == 0 || !time_reached(end)) {
    buttons_update();
    if (buttons_any_event())
      return;

    radar_data_t rdata = radar_get_data();
    if (rdata.data_valid && rdata.target_state == RADAR_TARGET_MOVING) {
      uint16_t d = rdata.moving_distance_cm;
      if (d > 600)
        d = 600;
      int ix = (d * (LIFE_COLS - 6)) / 600 + 3;
      int iy = LIFE_ROWS / 2;

      grid[ix][iy] = 1;
      grid[ix + 1][iy + 1] = 1;
      grid[ix - 1][iy + 2] = 1;
      grid[ix][iy + 2] = 1;
      grid[ix + 1][iy + 2] = 1;
    }

    lcd_fill_screen(COLOR_BLACK);

    for (int x = 0; x < LIFE_COLS; x++) {
      for (int y = 0; y < LIFE_ROWS; y++) {
        if (grid[x][y]) {
          uint16_t hue = (x * 360 / LIFE_COLS + generation) % 360;
          lcd_fill_rect(x * CELL_SZ, y * CELL_SZ, CELL_SZ - 1, CELL_SZ - 1,
                        hsv_to_rgb565(hue, 220, 255));
        }

        int neighbors = 0;
        for (int dx = -1; dx <= 1; dx++) {
          for (int dy = -1; dy <= 1; dy++) {
            if (dx == 0 && dy == 0)
              continue;
            int nx = x + dx;
            int ny = y + dy;
            if (nx < 0)
              nx = LIFE_COLS - 1;
            if (nx >= LIFE_COLS)
              nx = 0;
            if (ny < 0)
              ny = LIFE_ROWS - 1;
            if (ny >= LIFE_ROWS)
              ny = 0;

            neighbors += grid[nx][ny];
          }
        }

        if (grid[x][y]) {
          next_grid[x][y] = (neighbors == 2 || neighbors == 3) ? 1 : 0;
        } else {
          next_grid[x][y] = (neighbors == 3) ? 1 : 0;
        }
      }
    }

    memcpy(grid, next_grid, sizeof(grid));
    generation++;

    char buf[16];
    snprintf(buf, sizeof(buf), "GEN %u", generation);
    lcd_draw_string(2, LCD_HEIGHT - 10, buf, COLOR_WHITE, COLOR_BLACK, 1);

    lcd_flush();
    animation_delay(80);
  }
}

// ─── Demoscene Fire ──────────────────────────────────────────────────────────

void anim_fire(uint32_t duration_ms) {
#define FIRE_COLS 80
#define FIRE_ROWS 64
#define FIRE_CELL_SZ 2

  static uint8_t fire_pixels[FIRE_COLS][FIRE_ROWS] = {{0}};
  memset(fire_pixels, 0, sizeof(fire_pixels));

  absolute_time_t end;
  if (duration_ms > 0) {
    end = make_timeout_time_ms(duration_ms);
  }

  uint16_t fire_palette[256];
  for (int i = 0; i < 256; i++) {
    if (i < 85) {
      fire_palette[i] = lcd_color565(i * 3, 0, 0);
    } else if (i < 170) {
      fire_palette[i] = lcd_color565(255, (i - 85) * 3, 0);
    } else {
      fire_palette[i] = lcd_color565(255, 255, (i - 170) * 3);
    }
  }

  while (duration_ms == 0 || !time_reached(end)) {
    buttons_update();
    if (buttons_any_event())
      return;

    radar_data_t rdata = radar_get_data();
    uint8_t base_heat = 160;
    if (rdata.data_valid) {
      uint16_t dist = rdata.moving_distance_cm > 0 ? rdata.moving_distance_cm
                                                   : rdata.stationary_distance_cm;
      if (dist > 0 && dist < 200) {
        base_heat = 240;
      } else if (dist > 0) {
        base_heat = 200;
      }
    }

    for (int x = 0; x < FIRE_COLS; x++) {
      fire_pixels[x][FIRE_ROWS - 1] =
          (fast_rand() % 2 == 0)
              ? base_heat + (fast_rand() % (256 - base_heat))
              : 0;
    }

    for (int y = 0; y < FIRE_ROWS - 1; y++) {
      for (int x = 0; x < FIRE_COLS; x++) {
        int x1 = (x - 1 + FIRE_COLS) % FIRE_COLS;
        int x2 = (x + 1) % FIRE_COLS;
        int y1 = y + 1;
        int y2 = (y + 2 < FIRE_ROWS) ? y + 2 : FIRE_ROWS - 1;

        int sum = fire_pixels[x1][y1] + fire_pixels[x][y1] +
                  fire_pixels[x2][y1] + fire_pixels[x][y2];
        int avg = sum / 4;
        int decay = (fast_rand() % 3);

        if (avg > decay) {
          fire_pixels[x][y] = avg - decay;
        } else {
          fire_pixels[x][y] = 0;
        }
      }
    }

    for (int y = 0; y < FIRE_ROWS; y++) {
      for (int x = 0; x < FIRE_COLS; x++) {
        uint8_t heat = fire_pixels[x][y];
        uint16_t color = fire_palette[heat];
        lcd_fill_rect(x * FIRE_CELL_SZ, y * FIRE_CELL_SZ, FIRE_CELL_SZ,
                      FIRE_CELL_SZ, color);
      }
    }

    lcd_draw_string(4, 4, "PLASMA FIRE", COLOR_WHITE, COLOR_BLACK, 1);
    lcd_flush();
    animation_delay(25);
  }
}
