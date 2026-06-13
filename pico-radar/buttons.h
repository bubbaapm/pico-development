#ifndef BUTTONS_H
#define BUTTONS_H

#include <stdbool.h>
#include <stdint.h>

// ─── Pin Assignments ────────────────────────────────────────────────────────
#define BTN_LEFT_PIN 6
#define BTN_RIGHT_PIN 27

// ─── Debounce Configuration ─────────────────────────────────────────────────
#define BTN_DEBOUNCE_MS 50 // Minimum ms between state changes

// ─── Button State ───────────────────────────────────────────────────────────
typedef struct {
  bool pressed;       // Current debounced state (true = touched)
  bool just_pressed;  // Edge: was just pressed this poll cycle
  bool just_released; // Edge: was just released this poll cycle
} button_state_t;

/**
 * @brief Initialize GPIO pins for capacitive buttons.
 *        Configures as inputs with pull-downs (capacitive touch
 *        modules typically drive HIGH when touched).
 */
void buttons_init(void);

/**
 * @brief Poll both buttons and update debounced state.
 *        Call this once per main loop iteration (~10-30ms).
 */
void buttons_update(void);

/**
 * @brief Get current state of the left button.
 */
button_state_t buttons_get_left(void);

/**
 * @brief Get current state of the right button.
 */
button_state_t buttons_get_right(void);

#endif // BUTTONS_H
