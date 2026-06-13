#ifndef LD2410_H
#define LD2410_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
  RADAR_TARGET_NONE = 0,
  RADAR_TARGET_MOVING,
  RADAR_TARGET_STATIONARY,
  RADAR_TARGET_BOTH
} radar_target_state_t;

typedef struct {
  radar_target_state_t target_state;
  uint16_t moving_distance_cm;
  uint8_t moving_energy;
  uint16_t stationary_distance_cm;
  uint8_t stationary_energy;
  uint16_t detection_distance_cm;
  bool data_valid;
  uint32_t last_update_ms;
} radar_data_t;

void radar_init(void);
void radar_update(void);
radar_data_t radar_get_data(void);

#endif
