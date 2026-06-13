#include "../lcd_animations.h"
#include "../buttons.h"
#include "../ld2410.h"
#include "../st7735.h"
#include "pico/stdlib.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static void pong_serve(float *bx, float *by, float *bvx, float *bvy, int dir) {
  *bx = LCD_WIDTH / 2.0f;
  *by = 71.0f;
  *bvx = 1.45f * (float)dir;
  *bvy = ((fast_rand() % 2) ? 0.55f : -0.55f);
}

void anim_pong_game(uint32_t duration_ms) {
  const int top = 14;
  const int bottom = LCD_HEIGHT - 1;
  const int paddle_x = 8;
  const int cpu_x = LCD_WIDTH - 11;
  const int paddle_w = 3;
  const int paddle_h = 26;
  const int ball_r = 2;

  float paddle_y = LCD_HEIGHT / 2.0f - paddle_h / 2.0f;
  float cpu_y = paddle_y;
  float bx, by, bvx, bvy;
  int player_score = 0;
  int cpu_score = 0;
  bool game_over = false;

  pong_serve(&bx, &by, &bvx, &bvy, (fast_rand() % 2) ? 1 : -1);
  buttons_clear_events();

  while (true) {
    buttons_update();
    if (buttons_dual_long()) {
      return;
    }

    if (game_over) {
      lcd_fill_screen(COLOR_BLACK);
      lcd_draw_string(22, 42,
                      player_score >= 11 ? "PLAYER WINS" : "CPU WINS",
                      COLOR_WHITE, COLOR_BLACK, 2);
      lcd_draw_string(24, 72, "L:RETRY  BOTH:EXIT", COLOR_DIM, COLOR_BLACK, 1);
      lcd_flush();
      if (buttons_left_short()) {
        player_score = 0;
        cpu_score = 0;
        paddle_y = LCD_HEIGHT / 2.0f - paddle_h / 2.0f;
        cpu_y = paddle_y;
        pong_serve(&bx, &by, &bvx, &bvy, 1);
        game_over = false;
        buttons_clear_events();
      }
      game_delay(16);
      continue;
    }

    if (buttons_left_held()) {
      paddle_y -= 3.1f;
    }
    if (buttons_right_held()) {
      paddle_y += 3.1f;
    }
    if (paddle_y < top + 2) paddle_y = top + 2;
    if (paddle_y + paddle_h > bottom - 1) paddle_y = bottom - 1 - paddle_h;

    float predict_y = by + bvy * 8.0f;
    float target_y = predict_y - paddle_h / 2.0f;
    float cpu_speed = fabsf(bvx) > 2.0f ? 2.15f : 1.85f;
    if (cpu_y + paddle_h / 2.0f < target_y + paddle_h / 2.0f - 2.0f) {
      cpu_y += cpu_speed;
    } else if (cpu_y + paddle_h / 2.0f > target_y + paddle_h / 2.0f + 2.0f) {
      cpu_y -= cpu_speed;
    }
    if (cpu_y < top + 2) cpu_y = top + 2;
    if (cpu_y + paddle_h > bottom - 1) cpu_y = bottom - 1 - paddle_h;

    bx += bvx;
    by += bvy;

    if (by - ball_r <= top) {
      by = top + ball_r;
      bvy = -bvy;
    } else if (by + ball_r >= bottom) {
      by = bottom - ball_r;
      bvy = -bvy;
    }

    if (bvx < 0.0f && bx - ball_r <= paddle_x + paddle_w &&
        bx + ball_r >= paddle_x && by >= paddle_y &&
        by <= paddle_y + paddle_h) {
      float hit = (by - (paddle_y + paddle_h / 2.0f)) / (paddle_h / 2.0f);
      bx = paddle_x + paddle_w + ball_r;
      bvx = fabsf(bvx) + 0.08f;
      if (bvx > 3.0f) bvx = 3.0f;
      bvy = hit * 2.1f;
    }

    if (bvx > 0.0f && bx + ball_r >= cpu_x && bx - ball_r <= cpu_x + paddle_w &&
        by >= cpu_y && by <= cpu_y + paddle_h) {
      float hit = (by - (cpu_y + paddle_h / 2.0f)) / (paddle_h / 2.0f);
      bx = cpu_x - ball_r;
      bvx = -(fabsf(bvx) + 0.08f);
      if (bvx < -3.0f) bvx = -3.0f;
      bvy = hit * 2.1f;
    }

    if (bx < -ball_r) {
      cpu_score++;
      if (cpu_score >= 11) {
        game_over = true;
      }
      pong_serve(&bx, &by, &bvx, &bvy, 1);
    } else if (bx > LCD_WIDTH + ball_r) {
      player_score++;
      if (player_score >= 11) {
        game_over = true;
      }
      pong_serve(&bx, &by, &bvx, &bvy, -1);
    }

    lcd_fill_screen(COLOR_BLACK);
    lcd_draw_line(0, top, LCD_WIDTH - 1, top, COLOR_WHITE);
    lcd_draw_line(0, bottom, LCD_WIDTH - 1, bottom, COLOR_WHITE);
    for (int y = top + 5; y < bottom; y += 8) {
      lcd_draw_line(LCD_WIDTH / 2, y, LCD_WIDTH / 2, y + 3, COLOR_DIM);
    }

    lcd_fill_rect(paddle_x, (int)paddle_y, paddle_w, paddle_h, COLOR_WHITE);
    lcd_fill_rect(cpu_x, (int)cpu_y, paddle_w, paddle_h, COLOR_WHITE);
    lcd_fill_rect((int)bx - ball_r, (int)by - ball_r, ball_r * 2 + 1,
                  ball_r * 2 + 1, COLOR_WHITE);

    char score_buf[8];
    snprintf(score_buf, sizeof(score_buf), "%02d", player_score);
    lcd_draw_string(48, 3, score_buf, COLOR_WHITE, COLOR_BLACK, 1);
    snprintf(score_buf, sizeof(score_buf), "%02d", cpu_score);
    lcd_draw_string(100, 3, score_buf, COLOR_WHITE, COLOR_BLACK, 1);

    lcd_flush();
    game_delay(16);
  }
}

#define INVADER_ROWS 4
#define INVADER_COLS 7
#define INVADER_W 10
#define INVADER_H 7
#define MAX_PLAYER_SHOTS 2
#define MAX_INVADER_SHOTS 4

struct shooter_laser {
  float x, y;
  bool active;
};

struct shooter_invader {
  int x, y;
  bool active;
};

static void init_invaders(struct shooter_invader *invaders) {
  for (int r = 0; r < INVADER_ROWS; r++) {
    for (int c = 0; c < INVADER_COLS; c++) {
      int idx = r * INVADER_COLS + c;
      invaders[idx].x = 18 + c * 18;
      invaders[idx].y = 20 + r * 12;
      invaders[idx].active = true;
    }
  }
}

static void init_shields(uint8_t shields[3][8]) {
  for (int s = 0; s < 3; s++) {
    for (int b = 0; b < 8; b++) {
      shields[s][b] = 2;
    }
  }
}

static void draw_invader_sprite(int x, int y, int row, bool frame) {
  static const uint8_t a[2][5] = {
      {0x24, 0x7E, 0xDB, 0xFF, 0xA5}, {0x24, 0x7E, 0xDB, 0xFF, 0x5A}};
  static const uint8_t b[2][5] = {
      {0x3C, 0x7E, 0xDB, 0xFF, 0x24}, {0x3C, 0x7E, 0xDB, 0xFF, 0x42}};
  const uint8_t *bits = (row == 0) ? a[frame ? 1 : 0] : b[frame ? 1 : 0];
  uint16_t color = row == 0 ? COLOR_CYAN : COLOR_MAGENTA;

  for (int py = 0; py < 5; py++) {
    for (int px = 0; px < 8; px++) {
      if (bits[py] & (1 << (7 - px))) {
        lcd_draw_pixel(x + px, y + py, color);
        lcd_draw_pixel(x + px, y + py + 1, color);
      }
    }
  }
}

static bool hit_shield(uint8_t shields[3][8], int x, int y) {
  for (int s = 0; s < 3; s++) {
    int sx = 24 + s * 48;
    int sy = 92;
    if (x < sx || x >= sx + 24 || y < sy || y >= sy + 12) {
      continue;
    }
    int bx = (x - sx) / 6;
    int by = (y - sy) / 4;
    int idx = by * 4 + bx;
    if (idx >= 0 && idx < 8 && shields[s][idx] > 0) {
      shields[s][idx]--;
      return true;
    }
  }
  return false;
}

static void draw_shields(uint8_t shields[3][8]) {
  for (int s = 0; s < 3; s++) {
    int sx = 24 + s * 48;
    int sy = 92;
    for (int b = 0; b < 8; b++) {
      if (!shields[s][b]) continue;
      int bx = sx + (b % 4) * 6;
      int by = sy + (b / 4) * 4;
      uint16_t color = shields[s][b] == 2 ? COLOR_GREEN : hsv_to_rgb565(95, 220, 120);
      lcd_fill_rect(bx, by, 5, 3, color);
    }
  }
}

void anim_space_shooter(uint32_t duration_ms) {
  int ship_x = LCD_WIDTH / 2 - 6;
  const int ship_y = 116;
  struct shooter_laser shots[MAX_PLAYER_SHOTS] = {0};
  struct shooter_laser bombs[MAX_INVADER_SHOTS] = {0};
  struct shooter_invader invaders[INVADER_ROWS * INVADER_COLS];
  uint8_t shields[3][8];
  int inv_dir = 1;
  uint32_t last_player_shot = 0;
  uint32_t last_bomb = 0;
  uint32_t fleet_tick = 0;
  int score = 0;
  int lives = 3;
  bool game_over = false;
  bool victory = false;
  bool anim_frame = false;

  init_invaders(invaders);
  init_shields(shields);
  buttons_clear_events();

  while (true) {
    buttons_update();
    if (buttons_dual_long()) {
      return;
    }

    if (game_over || victory) {
      lcd_fill_screen(COLOR_BLACK);
      lcd_draw_string(26, 42, victory ? "YOU WIN" : "GAME OVER",
                      victory ? COLOR_GREEN : COLOR_RED, COLOR_BLACK, 2);
      lcd_draw_string(24, 72, "L:RETRY  BOTH:EXIT", COLOR_DIM, COLOR_BLACK, 1);
      char score_buf[16];
      snprintf(score_buf, sizeof(score_buf), "SCORE %04d", score);
      lcd_draw_string(48, 94, score_buf, COLOR_WHITE, COLOR_BLACK, 1);
      lcd_flush();
      if (buttons_left_short()) {
        ship_x = LCD_WIDTH / 2 - 6;
        memset(shots, 0, sizeof(shots));
        memset(bombs, 0, sizeof(bombs));
        init_invaders(invaders);
        init_shields(shields);
        inv_dir = 1;
        score = 0;
        lives = 3;
        game_over = false;
        victory = false;
        buttons_clear_events();
      }
      game_delay(16);
      continue;
    }

    if (buttons_left_held()) ship_x -= 3;
    if (buttons_right_held()) ship_x += 3;
    if (ship_x < 0) ship_x = 0;
    if (ship_x > LCD_WIDTH - 13) ship_x = LCD_WIDTH - 13;

    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (now - last_player_shot > 360) {
      for (int i = 0; i < MAX_PLAYER_SHOTS; i++) {
        if (!shots[i].active) {
          shots[i].x = ship_x + 6;
          shots[i].y = ship_y - 3;
          shots[i].active = true;
          last_player_shot = now;
          break;
        }
      }
    }

    int active_count = 0;
    int leftmost = LCD_WIDTH;
    int rightmost = 0;
    for (int i = 0; i < INVADER_ROWS * INVADER_COLS; i++) {
      if (!invaders[i].active) continue;
      active_count++;
      if (invaders[i].x < leftmost) leftmost = invaders[i].x;
      if (invaders[i].x + INVADER_W > rightmost) rightmost = invaders[i].x + INVADER_W;
      if (invaders[i].y + INVADER_H >= ship_y) game_over = true;
    }
    if (active_count == 0) {
      victory = true;
    }

    fleet_tick++;
    uint32_t step_delay = 28 - (uint32_t)((INVADER_ROWS * INVADER_COLS - active_count) / 2);
    if (step_delay < 8) step_delay = 8;
    if (fleet_tick >= step_delay) {
      fleet_tick = 0;
      bool edge = (leftmost <= 3 && inv_dir < 0) || (rightmost >= LCD_WIDTH - 3 && inv_dir > 0);
      if (edge) {
        inv_dir = -inv_dir;
        for (int i = 0; i < INVADER_ROWS * INVADER_COLS; i++) {
          if (invaders[i].active) invaders[i].y += 6;
        }
      } else {
        for (int i = 0; i < INVADER_ROWS * INVADER_COLS; i++) {
          if (invaders[i].active) invaders[i].x += inv_dir * 4;
        }
      }
      anim_frame = !anim_frame;
    }

    if (now - last_bomb > 520 && active_count > 0) {
      int tries = 0;
      while (tries++ < 18) {
        int col = fast_rand() % INVADER_COLS;
        int chosen = -1;
        for (int r = INVADER_ROWS - 1; r >= 0; r--) {
          int idx = r * INVADER_COLS + col;
          if (invaders[idx].active) {
            chosen = idx;
            break;
          }
        }
        if (chosen >= 0) {
          for (int b = 0; b < MAX_INVADER_SHOTS; b++) {
            if (!bombs[b].active) {
              bombs[b].x = invaders[chosen].x + 5;
              bombs[b].y = invaders[chosen].y + INVADER_H;
              bombs[b].active = true;
              last_bomb = now;
              tries = 99;
              break;
            }
          }
        }
      }
    }

    for (int i = 0; i < MAX_PLAYER_SHOTS; i++) {
      if (!shots[i].active) continue;
      shots[i].y -= 4.5f;
      if (shots[i].y < 0 || hit_shield(shields, (int)shots[i].x, (int)shots[i].y)) {
        shots[i].active = false;
        continue;
      }
      for (int e = 0; e < INVADER_ROWS * INVADER_COLS; e++) {
        if (invaders[e].active && shots[i].x >= invaders[e].x &&
            shots[i].x <= invaders[e].x + INVADER_W &&
            shots[i].y >= invaders[e].y &&
            shots[i].y <= invaders[e].y + INVADER_H) {
          invaders[e].active = false;
          shots[i].active = false;
          score += 10 + (INVADER_ROWS - e / INVADER_COLS) * 5;
          break;
        }
      }
    }

    for (int i = 0; i < MAX_INVADER_SHOTS; i++) {
      if (!bombs[i].active) continue;
      bombs[i].y += 2.7f;
      if (hit_shield(shields, (int)bombs[i].x, (int)bombs[i].y) ||
          bombs[i].y > LCD_HEIGHT) {
        bombs[i].active = false;
      } else if (bombs[i].x >= ship_x && bombs[i].x <= ship_x + 13 &&
                 bombs[i].y >= ship_y && bombs[i].y <= ship_y + 8) {
        bombs[i].active = false;
        lives--;
        if (lives <= 0) game_over = true;
      }
    }

    lcd_fill_screen(COLOR_BLACK);
    for (int i = 0; i < MAX_PLAYER_SHOTS; i++) {
      if (shots[i].active) {
        lcd_draw_line((int)shots[i].x, (int)shots[i].y, (int)shots[i].x,
                      (int)shots[i].y + 4, COLOR_WHITE);
      }
    }
    for (int i = 0; i < MAX_INVADER_SHOTS; i++) {
      if (bombs[i].active) {
        lcd_draw_line((int)bombs[i].x, (int)bombs[i].y, (int)bombs[i].x,
                      (int)bombs[i].y + 4, COLOR_RED);
      }
    }
    for (int i = 0; i < INVADER_ROWS * INVADER_COLS; i++) {
      if (invaders[i].active) {
        draw_invader_sprite(invaders[i].x, invaders[i].y, i / INVADER_COLS,
                            anim_frame);
      }
    }
    draw_shields(shields);
    lcd_fill_rect(ship_x + 5, ship_y, 3, 3, COLOR_GREEN);
    lcd_fill_rect(ship_x + 2, ship_y + 3, 9, 3, COLOR_GREEN);
    lcd_fill_rect(ship_x, ship_y + 6, 13, 2, COLOR_GREEN);

    char hud_buf[24];
    snprintf(hud_buf, sizeof(hud_buf), "%04d  L%d", score, lives);
    lcd_draw_string(2, 2, hud_buf, COLOR_WHITE, COLOR_BLACK, 1);

    lcd_flush();
    game_delay(16);
  }
}

#define RUN_LANES 5
#define RUN_SEGS 10

struct run2_segment {
  float z;
  uint8_t mask;
  uint8_t block_lane;
};

static void run2_reset(struct run2_segment *segs) {
  for (int i = 0; i < RUN_SEGS; i++) {
    segs[i].z = 20.0f + i * 12.0f;
    segs[i].mask = 0x1F;
    segs[i].block_lane = 255;
  }
}

static void run2_project(float lane, float z, float roll, int *x, int *y) {
  float depth = 1.0f - z / 136.0f;
  float spread = 8.0f + depth * depth * 142.0f;
  float yy = 24.0f + depth * depth * 112.0f;
  float xx = LCD_WIDTH / 2.0f + lane * spread;
  float cx = LCD_WIDTH / 2.0f;
  float cy = LCD_HEIGHT / 2.0f;
  float dx = xx - cx;
  float dy = yy - cy;
  float c = cosf(roll);
  float s = sinf(roll);
  *x = (int)(cx + dx * c - dy * s);
  *y = (int)(cy + dx * s + dy * c);
}

void anim_run_game(uint32_t duration_ms) {
  struct run2_segment segs[RUN_SEGS];
  int lane = RUN_LANES / 2;
  float jump = 0.0f;
  float vy = 0.0f;
  float roll = 0.0f;
  float target_roll = 0.0f;
  int score = 0;
  uint32_t frame = 0;
  bool game_over = false;
  int left_repeat = 0;
  int right_repeat = 0;

  run2_reset(segs);
  buttons_clear_events();

  while (true) {
    buttons_update();
    if (buttons_dual_long()) return;

    if (game_over) {
      lcd_fill_screen(COLOR_BLACK);
      lcd_draw_string(25, 40, "GAME OVER", COLOR_RED, COLOR_BLACK, 2);
      lcd_draw_string(24, 68, "L:RETRY  BOTH:EXIT", COLOR_DIM, COLOR_BLACK, 1);
      char score_buf[16];
      snprintf(score_buf, sizeof(score_buf), "SCORE %04d", score);
      lcd_draw_string(46, 94, score_buf, COLOR_WHITE, COLOR_BLACK, 1);
      lcd_flush();
      if (buttons_left_short()) {
        run2_reset(segs);
        lane = RUN_LANES / 2;
        jump = 0.0f;
        vy = 0.0f;
        roll = 0.0f;
        target_roll = 0.0f;
        score = 0;
        frame = 0;
        left_repeat = right_repeat = 0;
        game_over = false;
        buttons_clear_events();
      }
      game_delay(16);
      continue;
    }

    if (buttons_left_held()) {
      left_repeat++;
      if (left_repeat == 1 || (left_repeat > 12 && left_repeat % 7 == 0)) lane--;
    } else {
      left_repeat = 0;
    }
    if (buttons_right_held()) {
      right_repeat++;
      if (right_repeat == 1 || (right_repeat > 12 && right_repeat % 7 == 0)) lane++;
    } else {
      right_repeat = 0;
    }
    if (lane < 0) {
      lane = RUN_LANES - 1;
      target_roll += 1.5708f;
    } else if (lane >= RUN_LANES) {
      lane = 0;
      target_roll -= 1.5708f;
    }
    if (buttons_dual_press() && jump <= 0.0f) {
      vy = 3.9f;
      buttons_clear_events();
    }
    if (jump > 0.0f || vy != 0.0f) {
      jump += vy;
      vy -= 0.26f;
      if (jump <= 0.0f) {
        jump = 0.0f;
        vy = 0.0f;
      }
    }
    roll += (target_roll - roll) * 0.16f;

    lcd_fill_screen(COLOR_BLACK);
    for (int i = 0; i < 34; i++) {
      lcd_draw_pixel((i * 41 + 7) % LCD_WIDTH, (i * 29 + frame) % LCD_HEIGHT,
                     COLOR_WHITE);
    }

    for (int i = 0; i < RUN_SEGS; i++) {
      segs[i].z -= 1.25f + score * 0.001f;
      if (segs[i].z < 8.0f) {
        segs[i].z = 136.0f;
        segs[i].mask = 0x1F;
        if (score > 8 && fast_rand() % 3 == 0) {
          segs[i].mask &= ~(1 << (fast_rand() % RUN_LANES));
        }
        if (score > 16 && fast_rand() % 5 == 0) {
          segs[i].block_lane = fast_rand() % RUN_LANES;
          if (!(segs[i].mask & (1 << segs[i].block_lane))) {
            segs[i].block_lane = 255;
          }
        } else {
          segs[i].block_lane = 255;
        }
      }

      if (segs[i].z > 11.0f && segs[i].z < 18.0f) {
        if (!(segs[i].mask & (1 << lane)) && jump < 7.0f) game_over = true;
        if (segs[i].block_lane == lane && jump < 11.0f) game_over = true;
      }
    }

    for (int i = RUN_SEGS - 1; i >= 0; i--) {
      for (int l = 0; l < RUN_LANES; l++) {
        if (!(segs[i].mask & (1 << l))) continue;
        int x1, y1, x2, y2, x3, y3, x4, y4;
        float left = ((float)l - 2.5f) / 2.5f;
        float right = ((float)l - 1.5f) / 2.5f;
        run2_project(left, segs[i].z, roll, &x1, &y1);
        run2_project(right, segs[i].z, roll, &x2, &y2);
        run2_project(right, segs[i].z - 9.0f, roll, &x3, &y3);
        run2_project(left, segs[i].z - 9.0f, roll, &x4, &y4);
        uint16_t edge = hsv_to_rgb565(222, 190, 170 + i * 7);
        lcd_draw_line(x1, y1, x2, y2, edge);
        lcd_draw_line(x2, y2, x3, y3, edge);
        lcd_draw_line(x3, y3, x4, y4, edge);
        lcd_draw_line(x4, y4, x1, y1, edge);
        if (l == lane && i < 3) {
          lcd_draw_line(x1, y1, x3, y3, hsv_to_rgb565(210, 120, 80));
        }
        if (segs[i].block_lane == l) {
          int bx = (x1 + x2 + x3 + x4) / 4;
          int by = (y1 + y2 + y3 + y4) / 4;
          lcd_fill_rect(bx - 4, by - 5, 8, 10, COLOR_RED);
        }
      }
    }

    int px, py;
    run2_project(((float)lane - 2.0f) / 2.5f, 10.0f, roll, &px, &py);
    py -= (int)jump;
    lcd_fill_circle(px, py - 7, 4, lcd_color565(190, 190, 170));
    lcd_fill_rect(px - 5, py - 5, 10, 9, lcd_color565(170, 170, 155));
    lcd_draw_pixel(px - 2, py - 7, COLOR_BLACK);
    lcd_draw_pixel(px + 2, py - 7, COLOR_BLACK);

    score = frame / 8;
    frame++;
    char hud_buf[16];
    snprintf(hud_buf, sizeof(hud_buf), "%04d", score);
    lcd_draw_string(2, 2, hud_buf, COLOR_YELLOW, COLOR_BLACK, 1);
    lcd_flush();
    game_delay(16);
  }
}
