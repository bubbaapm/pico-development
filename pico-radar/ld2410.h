#ifndef LD2410_H
#define LD2410_H

#include <stdbool.h>
#include <stdint.h>

// ─── Pin Assignments (UART0) ────────────────────────────────────────────────
#define RADAR_UART_ID uart0
#define RADAR_TX_PIN 16 // Pico TX → Radar RX
#define RADAR_RX_PIN 17 // Pico RX → Radar TX
#define RADAR_BAUD_RATE 256000

// ─── Target State Enum ──────────────────────────────────────────────────────
typedef enum {
  RADAR_TARGET_NONE = 0x00,
  RADAR_TARGET_MOVING = 0x01,
  RADAR_TARGET_STATIONARY = 0x02,
  RADAR_TARGET_BOTH = 0x03,
} radar_target_state_t;

// ─── Parsed Radar Data ──────────────────────────────────────────────────────
typedef struct {
  radar_target_state_t target_state;

  uint16_t moving_distance_cm; // Distance to moving target (cm)
  uint8_t moving_energy;       // Moving target energy (0–100)

  uint16_t stationary_distance_cm; // Distance to stationary target (cm)
  uint8_t stationary_energy;       // Stationary target energy (0–100)

  uint16_t detection_distance_cm; // Max detection distance (cm)

  bool data_valid;         // true if at least one good frame received
  uint32_t last_update_ms; // timestamp of last valid frame
} radar_data_t;

/**
 * @brief Initialize UART0 for the LD2410C radar at 256000 baud.
 */
void radar_init(void);

/**
 * @brief Process any available UART bytes. Call frequently from main loop.
 *        Parses frames and updates internal state when a complete
 *        valid frame is received.
 */
void radar_update(void);

/**
 * @brief Get a copy of the latest parsed radar data.
 */
radar_data_t radar_get_data(void);

/**
 * @brief Get a human-readable string for the target state.
 */
const char *radar_state_str(radar_target_state_t state);

#endif // LD2410_H
