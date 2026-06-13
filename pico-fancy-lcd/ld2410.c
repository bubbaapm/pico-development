#include "ld2410.h"

void radar_init(void) {}

void radar_update(void) {}

radar_data_t radar_get_data(void) {
  radar_data_t data = {
      .target_state = RADAR_TARGET_NONE,
      .moving_distance_cm = 0,
      .moving_energy = 0,
      .stationary_distance_cm = 0,
      .stationary_energy = 0,
      .detection_distance_cm = 0,
      .data_valid = false,
      .last_update_ms = 0,
  };
  return data;
}
