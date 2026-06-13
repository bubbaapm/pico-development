#ifndef UI_H
#define UI_H

#include <stdint.h>

// ─── Screen IDs ─────────────────────────────────────────────────────────────
typedef enum {
  SCREEN_SPLASH,      // Boot animation
  SCREEN_RADAR_LIVE,  // Live radar data display
  SCREEN_RADAR_GRAPH, // Distance history graph
  SCREEN_SETTINGS,    // Settings / info screen
  SCREEN_COUNT
} screen_id_t;

/**
 * @brief Initialize the UI system (sets up LCD, draws splash).
 */
void ui_init(void);

/**
 * @brief Update the UI — call every main-loop tick.
 *        Reads button state, handles navigation, redraws as needed.
 */
void ui_update(void);

/**
 * @brief Force switch to a specific screen.
 */
void ui_set_screen(screen_id_t screen);

/**
 * @brief Get the currently active screen.
 */
screen_id_t ui_get_screen(void);

#endif // UI_H
