#ifndef LCD_ANIMATIONS_H
#define LCD_ANIMATIONS_H

#include <stdint.h>

/**
 * @brief Rainbow color wipe — fills the screen row by row cycling
 *        through rainbow hues. Visually striking startup animation.
 * @param duration_ms  Total animation run time (0 for indefinite).
 */
void anim_rainbow_wipe(uint32_t duration_ms);

/**
 * @brief Expanding concentric circles that radiate outward from the
 *        center, each a different color. Looks like a radar pulse.
 * @param duration_ms  Total animation run time (0 for indefinite).
 */
void anim_radar_pulse(uint32_t duration_ms);

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
 * @brief 3D Wireframe Cube — a spinning wireframe cube whose rotation speeds
 *        and color are modulated by the radar sensor data.
 * @param duration_ms  Total animation run time.
 */
void anim_3d_cube(uint32_t duration_ms);

/**
 * @brief Conway's Game of Life — retro cellular automaton simulation.
 *        Movement detected by radar spawns new cells at the center.
 * @param duration_ms  Total animation run time.
 */
void anim_game_of_life(uint32_t duration_ms);

/**
 * @brief Demoscene Fire — classic retro pixel fire simulation that burns
 *        hotter when a target is close.
 * @param duration_ms  Total animation run time.
 */
void anim_fire(uint32_t duration_ms);

// ─── 8 New Visual Animations ────────────────────────────────────────────────
void anim_star_tunnel(uint32_t duration_ms);
void anim_wave_particles(uint32_t duration_ms);
void anim_3d_torus(uint32_t duration_ms);
void anim_vector_tunnel(uint32_t duration_ms);
void anim_fireworks(uint32_t duration_ms);
void anim_synth_grid(uint32_t duration_ms);
void anim_synth_terrain(uint32_t duration_ms);
void anim_synth_city(uint32_t duration_ms);
void anim_synth_gradient(uint32_t duration_ms);
void anim_synth_ocean(uint32_t duration_ms);
void anim_ring_tunnel(uint32_t duration_ms);
void anim_rain_ripples(uint32_t duration_ms);
void anim_tron_trails(uint32_t duration_ms);
void anim_neon_rain_city(uint32_t duration_ms);
void anim_lowpoly_planet(uint32_t duration_ms);
void anim_vhs_glitch(uint32_t duration_ms);
void anim_metaballs(uint32_t duration_ms);

// ─── 2 Interactive Games ───────────────────────────────────────────────────
void anim_pong_game(uint32_t duration_ms);
void anim_space_shooter(uint32_t duration_ms);
void anim_run_game(uint32_t duration_ms);

/**
 * @brief Run all demo animations in sequence. Good for showing off!
 */
void anim_demo_all(void);

/**
 * @brief Non-blocking delay that processes UART radar frames and debounces buttons.
 */
void animation_delay(uint32_t delay_ms);
void game_delay(uint32_t delay_ms);
uint16_t hsv_to_rgb565(uint16_t h, uint8_t s, uint8_t v);
uint32_t fast_rand(void);

#endif // LCD_ANIMATIONS_H
