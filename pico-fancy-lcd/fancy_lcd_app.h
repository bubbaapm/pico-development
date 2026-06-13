#ifndef FANCY_LCD_APP_H
#define FANCY_LCD_APP_H

#include <stdbool.h>
#include <stdint.h>

typedef void (*animation_fn_t)(uint32_t duration_ms);

typedef struct {
  const char *id;
  const char *label;
  animation_fn_t run;
} animation_entry_t;

const animation_entry_t *app_animation_list(uint8_t *count);
uint8_t app_get_animation_index(void);
void app_set_animation_index(uint8_t index);
void app_next_animation(void);
void app_previous_animation(void);
uint32_t app_get_animation_duration_ms(void);
void app_set_animation_duration_ms(uint32_t duration_ms);
bool app_take_run_request(void);
void app_request_run(void);
uint32_t app_get_completed_runs(void);
bool app_is_new_request_pending(void);

#endif
