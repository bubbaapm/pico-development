#ifndef LCD_ANIMATIONS_H
#define LCD_ANIMATIONS_H

#include <stdint.h>

/**
 * @brief Rainbow color wipe — fills the screen row by row cycling
 *        through rainbow hues. Visually striking startup animation.
 * @param speed_ms  Delay between each row (2–5 looks smooth).
 */
void anim_rainbow_wipe(uint8_t speed_ms);

/**
 * @brief Expanding concentric circles that radiate outward from the
 *        center, each a different color. Looks like a radar pulse.
 * @param speed_ms  Delay between each ring (10–30 works well).
 */
void anim_radar_pulse(uint8_t speed_ms);

/**
 * @brief Matrix-style "digital rain" — green characters cascade
 *        down the screen in random columns.
 * @param duration_ms  Total animation run time.
 */
void anim_matrix_rain(uint32_t duration_ms);

/**
 * @brief Bouncing ball — a filled circle that bounces around the
 *        screen with simple physics, leaving a fading trail.
 * @param duration_ms  Total animation run time.
 */
void anim_bouncing_ball(uint32_t duration_ms);

/**
 * @brief Starfield — dots fly outward from the center like
 *        traveling through hyperspace.
 * @param duration_ms  Total animation run time.
 */
void anim_starfield(uint32_t duration_ms);

/**
 * @brief Plasma effect — a smoothly undulating color pattern that
 *        creates a psychedelic look.
 * @param duration_ms  Total animation run time.
 */
void anim_plasma(uint32_t duration_ms);

/**
 * @brief Run all demo animations in sequence. Good for showing off!
 */
void anim_demo_all(void);

#endif // LCD_ANIMATIONS_H
