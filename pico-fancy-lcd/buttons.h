#ifndef BUTTONS_H
#define BUTTONS_H

#include <stdbool.h>
#include <stdint.h>

// ─── Pin Assignments ────────────────────────────────────────────────────────
#define BTN_LEFT_PIN 6
#define BTN_RIGHT_PIN 27

// ─── Debounce Configuration ─────────────────────────────────────────────────
#define BTN_DEBOUNCE_MS 50 // Minimum ms between state changes

// ─── Button API ─────────────────────────────────────────────────────────────

/**
 * @brief Initialize GPIO pins and set up interrupts on both buttons.
 */
void buttons_init(void);

/**
 * @brief Process raw interrupt states and compute touch gestures.
 *        Call this once per main loop iteration (~10-30ms).
 */
void buttons_update(void);

/**
 * @brief Returns true if left button was touched and released quickly (<800ms).
 */
bool buttons_left_short(void);

/**
 * @brief Returns true if left button was held for >=800ms.
 */
bool buttons_left_long(void);

/**
 * @brief Returns true if right button was touched and released quickly (<800ms).
 */
bool buttons_right_short(void);

/**
 * @brief Returns true if right button was held for >=800ms.
 */
bool buttons_right_long(void);

/**
 * @brief Returns true if both buttons were touched simultaneously.
 */
bool buttons_dual_press(void);

/**
 * @brief Returns true if both buttons were held simultaneously for >= 800ms.
 */
bool buttons_dual_long(void);

/**
 * @brief Returns true if the left button is currently held down.
 */
bool buttons_left_held(void);

/**
 * @brief Returns true if the right button is currently held down.
 */
bool buttons_right_held(void);

/**
 * @brief Returns true if any touch event occurred in this frame (short, long, or dual).
 */
bool buttons_any_event(void);

/**
 * @brief Clear all accumulated button events.
 */
void buttons_clear_events(void);

#endif // BUTTONS_H
