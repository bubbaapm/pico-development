/**
 * @file  ws2812.h
 * @brief WS2812B (NeoPixel) LED driver for RP2350 — PIO at the register level.
 *
 * Uses PIO0, state machine 0 by default.  The driver provides two register
 * access paths (raw volatile pointers vs. SDK struct), a pixel buffer with
 * brightness scaling, and a handful of ready-to-use lighting effects.
 *
 * Hardware:   WS2812B data-in → GPIO 1 (physical pin 2 on Pico 2)
 * Protocol:   800 kHz, GRB byte order, MSB first
 *
 * @author  apmiller
 */

#ifndef WS2812_H
#define WS2812_H

#include <stdbool.h>
#include <stdint.h>

// ─── Pin / strip configuration ──────────────────────────────────────────────
#define WS2812_PIN 1          // GPIO 1 = physical pin 2
#define WS2812_NUM_LEDS 64    // 8x8 matrix
#define WS2812_IS_RGBW false  // true for SK6812 RGBW LEDs
#define WS2812_FREQ_HZ 800000 // 800 kHz (standard WS2812B)

// ─── PIO selection ──────────────────────────────────────────────────────────
#define WS2812_PIO_IDX 0 // 0 = PIO0, 1 = PIO1, 2 = PIO2
#define WS2812_SM 0      // State machine 0

// ─── Configuration toggle ───────────────────────────────────────────────────
extern bool ws2812_use_raw_pointers; // true = raw volatile macros
                                     // false = SDK struct (pio0_hw->)

// ─── Initialisation ─────────────────────────────────────────────────────────

/**
 * @brief Initialise PIO0/SM0 for WS2812B output on WS2812_PIN.
 *
 * Releases peripheral resets, configures the GPIO pad and function select,
 * loads the PIO program into instruction memory, and configures the state
 * machine (clock divider, side-set pin, shift register, FIFO join).
 *
 * Uses either raw register pointers or SDK structs depending on
 * ws2812_use_raw_pointers.
 */
void ws2812_init(void);

// ─── Low-level pixel output ─────────────────────────────────────────────────

/**
 * @brief Push a single 24-bit GRB value directly to the PIO TX FIFO.
 *
 * Blocks until the FIFO has space.  Call this WS2812_NUM_LEDS times,
 * then wait ≥ 50 µs for the latch (or call ws2812_show for buffered use).
 *
 * @param pixel_grb  24-bit value in GRB order, left-justified in bits [23:0].
 *                   Use ws2812_urgb() to pack from R,G,B components.
 */
void ws2812_put_pixel(uint32_t pixel_grb);

// ─── Pixel buffer control ───────────────────────────────────────────────────

/**
 * @brief Set pixel at \p index to an RGB colour (converted to GRB internally).
 */
void ws2812_set_pixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief Set every pixel to the same RGB colour.
 */
void ws2812_set_all(uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief Turn all pixels off (write black to buffer + flush).
 */
void ws2812_clear(void);

/**
 * @brief Flush the internal pixel buffer to the strip.
 *
 * Applies brightness scaling, pushes each pixel through the PIO FIFO,
 * then waits 60 µs for the WS2812B latch (reset code).
 */
void ws2812_show(void);

// ─── Colour helpers ─────────────────────────────────────────────────────────

/**
 * @brief Pack R, G, B components into a 24-bit GRB value suitable for
 *        ws2812_put_pixel().
 */
static inline uint32_t ws2812_urgb(uint8_t r, uint8_t g, uint8_t b) {
  return ((uint32_t)g << 16) | ((uint32_t)r << 8) | (uint32_t)b;
}

/**
 * @brief Convert a hue (0–360) to a fully-saturated RGB pixel (GRB packed).
 *
 * Uses a simple HSV→RGB with S=V=255.  Useful for rainbow effects.
 */
uint32_t ws2812_wheel(uint16_t hue);

// ─── Brightness ─────────────────────────────────────────────────────────────

/**
 * @brief Set a global brightness scaler (0–255).
 *
 * Applied in ws2812_show().  Default is 32 (~12 %) to keep current draw
 * manageable on USB power with long strips.
 */
void ws2812_set_brightness(uint8_t brightness);

/**
 * @brief Get the current global brightness value.
 */
uint8_t ws2812_get_brightness(void);

/**
 * @brief Set the global animation speed (1 to 10).
 */
void ws2812_set_speed(uint8_t speed);

/**
 * @brief Get the current global animation speed.
 */
uint8_t ws2812_get_speed(void);

// ─── Single LED Convenience / Dynamic Brightness Control ────────────────────

/**
 * @brief Set the single LED color with per-call brightness.
 * @param r           Red   0–255
 * @param g           Green 0–255
 * @param b           Blue  0–255
 * @param brightness  0–100 (percent). 0 = off, 100 = full brightness.
 */
void ws2812_set(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness);

/**
 * @brief Set the single LED color using Hue/Saturation/Value (HSV) color space with brightness.
 * @param h           Hue        0–359 (degrees on color wheel)
 * @param s           Saturation 0–255
 * @param v           Value      0–255
 * @param brightness  0–100 (percent). Applied on top of HSV value.
 */
void ws2812_set_hsv(uint16_t h, uint8_t s, uint8_t v, uint8_t brightness);

/**
 * @brief Smoothly cycle through all hues on the LED at a gentle 15% brightness.
 *        Useful for verifying all RGB channels work correctly.
 */
void ws2812_color_demo(void);

void ws2812_effect_stop(void);
void ws2812_effect_solid(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness);
void ws2812_effect_breathe(uint8_t r, uint8_t g, uint8_t b,
                           uint8_t brightness, uint16_t period_ms);
void ws2812_effect_strobe(uint8_t r, uint8_t g, uint8_t b,
                          uint8_t brightness, uint16_t on_ms,
                          uint16_t off_ms);
bool ws2812_effect_gradient(const uint8_t *colors_rgb, uint8_t color_count,
                            uint8_t brightness, uint16_t period_ms);
void ws2812_effect_update(uint32_t now_ms);

// ─── Lighting effects (blocking, run in a loop) ─────────────────────────────

/** @brief Smoothly cycle the full colour spectrum across the strip.  */
void ws2812_rainbow_cycle(uint16_t delay_ms);

/** @brief Pulse a colour up and down in brightness.  */
void ws2812_breathe(uint8_t r, uint8_t g, uint8_t b, uint16_t period_ms);

/** @brief Fill the strip one pixel at a time with a colour.  */
void ws2812_color_wipe(uint8_t r, uint8_t g, uint8_t b, uint16_t delay_ms);

/** @brief Blink the strip between a colour and off.  */
void ws2812_blink(uint8_t r, uint8_t g, uint8_t b, uint16_t on_ms,
                  uint16_t off_ms);

/** @brief Random twinkle / sparkle effect.  */
void ws2812_sparkle(uint8_t r, uint8_t g, uint8_t b, uint16_t delay_ms,
                    uint16_t duration_ms);

// ─── Register introspection ─────────────────────────────────────────────────

/**
 * @brief Print the current PIO0 register state to stdout (UART/USB).
 *
 * Useful for debugging — shows CTRL, FSTAT, SM0 clkdiv/execctrl/shiftctrl/
 * pinctrl, and the first few instruction memory words.
 */
void ws2812_dump_pio_regs(void);

#endif // WS2812_H
