/**
 * @file  matrix.h
 * @brief 8×8 WS2812B matrix coordinate mapper and effects.
 *
 * Translates (x, y) pixel coordinates to the linear LED index used by
 * the ws2812 driver, handling snake (zigzag) wiring automatically.
 *
 * Coordinate system (default, MATRIX_ORIGIN_TOP_LEFT):
 *   (0,0) = top-left       x increases rightward
 *                           y increases downward
 *
 * @author  apmiller
 */

#ifndef MATRIX_H
#define MATRIX_H

#include <stdbool.h>
#include <stdint.h>

// ─── Matrix dimensions ──────────────────────────────────────────────────────
#define MATRIX_WIDTH 8
#define MATRIX_HEIGHT 8
#define MATRIX_NUM_LEDS (MATRIX_WIDTH * MATRIX_HEIGHT)

// ─── Wiring configuration ───────────────────────────────────────────────────
// Set to true if odd rows are wired in reverse (snake / zigzag pattern).
// Set to false for purely sequential (all rows wired left-to-right).
#define MATRIX_SNAKE_ROWS false

// ─── Origin configuration ───────────────────────────────────────────────────
// Where is LED index 0 physically located?
//   0 = top-left      (default — x+ right, y+ down)
//   1 = bottom-left   (x+ right, y+ up — rotated 90° CW from default)
#define MATRIX_ORIGIN 0

// ─── Coordinate mapping ─────────────────────────────────────────────────────

/**
 * @brief Convert (x, y) coordinate to the linear LED index.
 *
 * Handles snake wiring and origin configuration automatically.
 * Returns MATRIX_NUM_LEDS (out of bounds sentinel) if x or y are invalid.
 *
 * @param x  Column (0 = left, 7 = right)
 * @param y  Row    (0 = top,  7 = bottom)  [when MATRIX_ORIGIN == 0]
 * @return   Linear LED index 0–63, or 64 if out of bounds.
 */
uint16_t matrix_xy(uint8_t x, uint8_t y);

// ─── Drawing primitives ─────────────────────────────────────────────────────

/**
 * @brief Set a single pixel by (x, y) coordinate.
 *        Does nothing if coordinates are out of bounds.
 */
void matrix_set_pixel(uint8_t x, uint8_t y, uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief Fill a rectangular region (inclusive corners).
 */
void matrix_fill_rect(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t r,
                      uint8_t g, uint8_t b);

/**
 * @brief Clear the entire matrix (all pixels off) and flush.
 */
void matrix_clear(void);

/**
 * @brief Fill the entire matrix with one colour and flush.
 */
void matrix_fill(uint8_t r, uint8_t g, uint8_t b);

// ─── Text rendering ─────────────────────────────────────────────────────────

/**
 * @brief Draw a single ASCII character using a built-in 3×5 font.
 *
 * Characters are 3 pixels wide and 5 pixels tall.  Only digits 0-9
 * and a handful of symbols are included to save flash.
 *
 * @param x   Top-left column of the character
 * @param y   Top-left row of the character
 * @param ch  ASCII character to draw
 * @param r,g,b  Colour
 */
void matrix_draw_char(uint8_t x, uint8_t y, char ch, uint8_t r, uint8_t g,
                      uint8_t b);

/**
 * @brief Draw a short string (max 2 characters fit side by side on 8×8).
 */
void matrix_draw_string(uint8_t x, uint8_t y, const char *str, uint8_t r,
                        uint8_t g, uint8_t b);

// ─── Diagnostics ────────────────────────────────────────────────────────────

/**
 * @brief Light LEDs 0→63 one at a time (1 Hz) to verify wiring order.
 *
 * Blocking — runs for ~64 seconds.  Useful for confirming snake vs
 * sequential wiring and identifying the physical location of each index.
 * Each LED is lit green for ~200ms then the next one lights.
 */
void matrix_test_sequence(void);

/**
 * @brief Light each column then each row to verify coordinate mapping.
 *
 * Blocking — runs for ~16 seconds.  Sweeps columns 0-7 in red,
 * then rows 0-7 in blue.
 */
void matrix_test_coordinates(void);

// ─── Matrix-specific effects (non-blocking, call from main loop) ────────────

/**
 * @brief Boot-up animation — expanding square from centre.
 *        Blocking, runs for ~2 seconds.
 */
void matrix_effect_boot(void);

/**
 * @brief Falling green "Matrix rain" columns.
 *        Non-blocking — call repeatedly from main loop.
 *
 * @param now_ms  Current time in ms (from to_ms_since_boot).
 */
void matrix_effect_rain(uint32_t now_ms);

/**
 * @brief Fire simulation using heat diffusion.
 *        Non-blocking — call repeatedly from main loop.
 *
 * @param now_ms  Current time in ms.
 */
void matrix_effect_fire(uint32_t now_ms);

/**
 * @brief Display a compass heading as an arrow on the matrix.
 *
 * @param heading_deg  Compass heading 0–359 (0 = North, 90 = East).
 */
void matrix_effect_compass(float heading_deg);

/**
 * @brief Display GPS fix status — satellite count and fix quality.
 *
 * Shows satellite count as a number, and colours the background:
 *   Red    = no fix
 *   Yellow = fix with few satellites (<6)
 *   Green  = good fix (≥6 satellites)
 *
 * @param fix_quality  0=no fix, 1=GPS, 2=DGPS
 * @param satellites   Number of satellites in use
 */
void matrix_effect_gps_status(uint8_t fix_quality, uint8_t satellites);

/**
 * @brief Red radar line sweeping back and forth.
 *        Non-blocking — call repeatedly from main loop.
 *
 * @param now_ms  Current time in ms.
 */
void matrix_effect_radar(uint32_t now_ms);

/**
 * @brief Twinkling white snow/stars on a dim blue background.
 *        Non-blocking — call repeatedly from main loop.
 *
 * @param now_ms  Current time in ms.
 */
void matrix_effect_snow(uint32_t now_ms);

/**
 * @brief Smoothly shifting sine-wave plasma colors across the grid.
 *        Non-blocking — call repeatedly from main loop.
 *
 * @param now_ms  Current time in ms.
 */
void matrix_effect_wave(uint32_t now_ms);

#endif // MATRIX_H
