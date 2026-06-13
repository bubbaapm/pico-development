#include "fancy_lcd_app.h"

#include <stddef.h>

#include "lcd_animations.h"

static const animation_entry_t animations[] = {
    {"rainbow", "Rainbow wipe", anim_rainbow_wipe},
    {"radar", "Radar pulse", anim_radar_pulse},
    {"matrix", "Matrix rain", anim_matrix_rain},
    {"ball", "Bouncing ball", anim_bouncing_ball},
    {"starfield", "Starfield", anim_starfield},
    {"plasma", "Plasma", anim_plasma},
    {"cube", "3D cube", anim_3d_cube},
    {"torus", "3D torus", anim_3d_torus},
    {"grid", "Synth grid", anim_synth_grid},
    {"terrain", "Synth terrain", anim_synth_terrain},
    {"city", "Synth city", anim_synth_city},
    {"gradient", "Synth gradient", anim_synth_gradient},
    {"ocean", "Synth ocean", anim_synth_ocean},
    {"fire", "Demoscene fire", anim_fire},
    {"metaballs", "Metaballs", anim_metaballs},
    {"demo", "Demo sequence", NULL},
};

static uint8_t active_animation_index = 0;
static uint32_t animation_duration_ms = 5000;
static bool run_requested = true;
static uint32_t completed_runs = 0;

const animation_entry_t *app_animation_list(uint8_t *count) {
  if (count) {
    *count = (uint8_t)(sizeof(animations) / sizeof(animations[0]));
  }
  return animations;
}

uint8_t app_get_animation_index(void) { return active_animation_index; }

void app_set_animation_index(uint8_t index) {
  uint8_t count = 0;
  app_animation_list(&count);
  if (index < count) {
    active_animation_index = index;
  }
}

void app_next_animation(void) {
  uint8_t count = 0;
  app_animation_list(&count);
  active_animation_index = (uint8_t)((active_animation_index + 1) % count);
}

void app_previous_animation(void) {
  uint8_t count = 0;
  app_animation_list(&count);
  active_animation_index =
      (uint8_t)((active_animation_index + count - 1) % count);
}

uint32_t app_get_animation_duration_ms(void) { return animation_duration_ms; }

void app_set_animation_duration_ms(uint32_t duration_ms) {
  if (duration_ms < 250) {
    duration_ms = 250;
  }
  if (duration_ms > 60000) {
    duration_ms = 60000;
  }
  animation_duration_ms = duration_ms;
}

bool app_take_run_request(void) {
  bool requested = run_requested;
  run_requested = false;
  if (requested) {
    completed_runs++;
  }
  return requested;
}

void app_request_run(void) { run_requested = true; }

uint32_t app_get_completed_runs(void) { return completed_runs; }

bool app_is_new_request_pending(void) { return run_requested; }
