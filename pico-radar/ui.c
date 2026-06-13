#include "ui.h"
#include "buttons.h"
#include "lcd_animations.h"
#include "ld2410.h"
#include "pico/stdlib.h"
#include "st7735.h"
#include <stdio.h>
#include <string.h>


// ─── State ──────────────────────────────────────────────────────────────────
static screen_id_t current_screen = SCREEN_SPLASH;
static bool needs_redraw = true;

// History buffer for the graph screen
#define HISTORY_LEN LCD_WIDTH // 160 samples
static uint16_t distance_history[HISTORY_LEN];
static uint16_t history_idx = 0;
static bool history_full = false;

// ─── Color Palette ──────────────────────────────────────────────────────────
#define UI_BG COLOR_BLACK
#define UI_HEADER_BG lcd_color565(20, 20, 40)
#define UI_TEXT COLOR_WHITE
#define UI_ACCENT COLOR_CYAN
#define UI_ALERT COLOR_RED
#define UI_OK COLOR_GREEN
#define UI_DIM lcd_color565(80, 80, 80)

// ─── Helper: Draw Status Header ─────────────────────────────────────────────
static void draw_header(const char *title) {
  lcd_fill_rect(0, 0, LCD_WIDTH, 12, UI_HEADER_BG);
  lcd_draw_string(2, 2, title, UI_ACCENT, UI_HEADER_BG, 1);
}

// ─── Helper: Draw Navigation Footer ────────────────────────────────────────
static void draw_footer(const char *left_label, const char *right_label) {
  uint16_t y = LCD_HEIGHT - 10;
  lcd_fill_rect(0, y, LCD_WIDTH, 10, UI_HEADER_BG);
  if (left_label)
    lcd_draw_string(2, y + 1, left_label, UI_DIM, UI_HEADER_BG, 1);
  if (right_label) {
    uint16_t rx = LCD_WIDTH - (strlen(right_label) * 6) - 2;
    lcd_draw_string(rx, y + 1, right_label, UI_DIM, UI_HEADER_BG, 1);
  }
}

// ─── Screen: Splash ─────────────────────────────────────────────────────────
static void draw_splash(void) {
  lcd_fill_screen(UI_BG);
  lcd_set_text_color(UI_ACCENT, UI_BG);
  lcd_set_text_size(2);
  lcd_setCursorPos(2, 3);
  lcd_puts("PICO");
  lcd_setCursorPos(1, 5);
  lcd_puts("RADAR");
  lcd_set_text_size(1);
  lcd_setCursorPos(3, 14);
  lcd_puts("LD2410C + RP2350");
}

// ─── Screen: Live Radar Data ────────────────────────────────────────────────
static void draw_radar_live(void) {
  radar_data_t d = radar_get_data();

  draw_header("LIVE RADAR");

  // Target state indicator — big text
  uint16_t state_color;
  switch (d.target_state) {
  case RADAR_TARGET_MOVING:
    state_color = UI_ALERT;
    break;
  case RADAR_TARGET_STATIONARY:
    state_color = COLOR_YELLOW;
    break;
  case RADAR_TARGET_BOTH:
    state_color = COLOR_ORANGE;
    break;
  default:
    state_color = UI_OK;
    break;
  }

  // Clear main area
  lcd_fill_rect(0, 14, LCD_WIDTH, LCD_HEIGHT - 24, UI_BG);

  // State label
  lcd_draw_string(4, 18, radar_state_str(d.target_state), state_color, UI_BG,
                  2);

  // Moving target info
  char buf[32];
  lcd_draw_string(4, 42, "MOV:", UI_DIM, UI_BG, 1);
  snprintf(buf, sizeof(buf), "%3ucm E:%u", d.moving_distance_cm,
           d.moving_energy);
  lcd_draw_string(34, 42, buf, UI_TEXT, UI_BG, 1);

  // Stationary target info
  lcd_draw_string(4, 54, "STA:", UI_DIM, UI_BG, 1);
  snprintf(buf, sizeof(buf), "%3ucm E:%u", d.stationary_distance_cm,
           d.stationary_energy);
  lcd_draw_string(34, 54, buf, UI_TEXT, UI_BG, 1);

  // Detection distance
  lcd_draw_string(4, 66, "DET:", UI_DIM, UI_BG, 1);
  snprintf(buf, sizeof(buf), "%ucm", d.detection_distance_cm);
  lcd_draw_string(34, 66, buf, UI_TEXT, UI_BG, 1);

  // Energy bar for moving target
  lcd_draw_string(4, 82, "M Energy", UI_DIM, UI_BG, 1);
  uint16_t bar_w = (d.moving_energy * 100) / 100;
  lcd_fill_rect(4, 92, 100, 6, UI_DIM);
  if (bar_w > 0)
    lcd_fill_rect(4, 92, bar_w, 6, UI_ALERT);

  // Energy bar for stationary target
  lcd_draw_string(4, 102, "S Energy", UI_DIM, UI_BG, 1);
  bar_w = (d.stationary_energy * 100) / 100;
  lcd_fill_rect(4, 112, 100, 6, UI_DIM);
  if (bar_w > 0)
    lcd_fill_rect(4, 112, bar_w, 6, COLOR_YELLOW);

  draw_footer("<Prev", "Next>");
}

// ─── Screen: Distance Graph ─────────────────────────────────────────────────
static void draw_radar_graph(void) {
  draw_header("DISTANCE GRAPH");

  // Graph area: y=14 to y=LCD_HEIGHT-12
  uint16_t graph_top = 16;
  uint16_t graph_bot = LCD_HEIGHT - 12;
  uint16_t graph_h = graph_bot - graph_top;

  lcd_fill_rect(0, graph_top, LCD_WIDTH, graph_h, UI_BG);

  // Draw grid lines
  for (int i = 0; i < 5; i++) {
    uint16_t gy = graph_top + (i * graph_h / 4);
    for (uint16_t gx = 0; gx < LCD_WIDTH; gx += 4)
      lcd_draw_pixel(gx, gy, UI_DIM);
  }

  // Plot distance history
  uint16_t max_dist = 600; // 6 meters max scale
  uint16_t count = history_full ? HISTORY_LEN : history_idx;

  for (uint16_t i = 1; i < count; i++) {
    uint16_t idx_prev =
        (history_idx - count + i - 1 + HISTORY_LEN) % HISTORY_LEN;
    uint16_t idx_curr = (history_idx - count + i + HISTORY_LEN) % HISTORY_LEN;

    uint16_t y_prev =
        graph_bot - (distance_history[idx_prev] * graph_h / max_dist);
    uint16_t y_curr =
        graph_bot - (distance_history[idx_curr] * graph_h / max_dist);

    // Clamp
    if (y_prev < graph_top)
      y_prev = graph_top;
    if (y_curr < graph_top)
      y_curr = graph_top;

    lcd_draw_line(i - 1, y_prev, i, y_curr, UI_ACCENT);
  }

  draw_footer("<Prev", "Next>");
}

// ─── Screen: Settings / Info ────────────────────────────────────────────────
static void draw_settings(void) {
  draw_header("INFO");

  lcd_fill_rect(0, 14, LCD_WIDTH, LCD_HEIGHT - 24, UI_BG);

  lcd_draw_string(4, 18, "Board: Pico 2", UI_TEXT, UI_BG, 1);
  lcd_draw_string(4, 30, "Radar: LD2410C", UI_TEXT, UI_BG, 1);
  lcd_draw_string(4, 42, "UART: 256000 baud", UI_TEXT, UI_BG, 1);
  lcd_draw_string(4, 54, "LCD: ST7735 160x128", UI_TEXT, UI_BG, 1);
  lcd_draw_string(4, 66, "SPI: 32MHz", UI_TEXT, UI_BG, 1);

  radar_data_t d = radar_get_data();
  char buf[32];
  snprintf(buf, sizeof(buf), "Radar: %s",
           d.data_valid ? "Connected" : "No data");
  lcd_draw_string(4, 82, buf, d.data_valid ? UI_OK : UI_ALERT, UI_BG, 1);

  draw_footer("<Prev", "");
}

// ─── UI Core ────────────────────────────────────────────────────────────────

void ui_init(void) {
  lcd_init();
  current_screen = SCREEN_SPLASH;
  needs_redraw = true;
  memset(distance_history, 0, sizeof(distance_history));
}

void ui_update(void) {
  // ── Handle button navigation ──
  button_state_t left = buttons_get_left();
  button_state_t right = buttons_get_right();

  if (right.just_pressed && current_screen < SCREEN_COUNT - 1) {
    current_screen++;
    needs_redraw = true;
  }
  if (left.just_pressed && current_screen > SCREEN_SPLASH) {
    current_screen--;
    needs_redraw = true;
  }

  // ── Update distance history for graph ──
  if (current_screen == SCREEN_RADAR_LIVE ||
      current_screen == SCREEN_RADAR_GRAPH) {
    radar_data_t d = radar_get_data();
    if (d.data_valid) {
      // Use the nearest non-zero distance
      uint16_t dist = d.moving_distance_cm;
      if (dist == 0)
        dist = d.stationary_distance_cm;
      distance_history[history_idx] = dist;
      history_idx = (history_idx + 1) % HISTORY_LEN;
      if (history_idx == 0)
        history_full = true;
    }
  }

  // ── Draw the current screen ──
  // For SPLASH, only draw once. For live screens, redraw periodically.
  if (needs_redraw || current_screen == SCREEN_RADAR_LIVE ||
      current_screen == SCREEN_RADAR_GRAPH) {
    switch (current_screen) {
    case SCREEN_SPLASH:
      draw_splash();
      break;
    case SCREEN_RADAR_LIVE:
      draw_radar_live();
      break;
    case SCREEN_RADAR_GRAPH:
      draw_radar_graph();
      break;
    case SCREEN_SETTINGS:
      draw_settings();
      break;
    default:
      break;
    }
    needs_redraw = false;
  }
}

void ui_set_screen(screen_id_t screen) {
  if (screen < SCREEN_COUNT) {
    current_screen = screen;
    needs_redraw = true;
  }
}

screen_id_t ui_get_screen(void) { return current_screen; }
