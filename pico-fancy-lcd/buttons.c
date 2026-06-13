#include "buttons.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include "fancy_lcd_app.h"

// ─── ISR Volatile Variables ──────────────────────────────────────────────────
static volatile bool left_is_down = false;
static volatile bool right_is_down = false;

static volatile bool left_was_pressed = false;
static volatile bool left_was_released = false;
static volatile bool right_was_pressed = false;
static volatile bool right_was_released = false;

static volatile uint64_t left_press_time = 0;
static volatile uint64_t right_press_time = 0;

// Unified GPIO Interrupt Callback (No debounce block to capture fast taps)
static void gpio_callback(uint gpio, uint32_t events) {
  bool state = gpio_get(gpio);
  uint64_t now = to_us_since_boot(get_absolute_time());

  if (gpio == BTN_LEFT_PIN) {
    left_is_down = state;
    if (state) {
      left_was_pressed = true;
      left_press_time = now;
    } else {
      left_was_released = true;
    }
  } else if (gpio == BTN_RIGHT_PIN) {
    right_is_down = state;
    if (state) {
      right_was_pressed = true;
      right_press_time = now;
    } else {
      right_was_released = true;
    }
  }
}

// ─── Initialization ─────────────────────────────────────────────────────────
void buttons_init(void) {
  // Setup GPIO pins as pull-down inputs
  gpio_init(BTN_LEFT_PIN);
  gpio_set_dir(BTN_LEFT_PIN, GPIO_IN);
  gpio_pull_down(BTN_LEFT_PIN);

  gpio_init(BTN_RIGHT_PIN);
  gpio_set_dir(BTN_RIGHT_PIN, GPIO_IN);
  gpio_pull_down(BTN_RIGHT_PIN);

  // Set interrupts for rise and fall edges with our callback
  gpio_set_irq_enabled_with_callback(BTN_LEFT_PIN,
                                     GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL,
                                     true, &gpio_callback);
  gpio_set_irq_enabled(BTN_RIGHT_PIN,
                       GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL,
                       true);
}

// ─── Gesture State Tracking ──────────────────────────────────────────────────
static bool left_long_triggered = false;
static bool right_long_triggered = false;
static bool dual_active = false;
static uint64_t dual_press_time = 0;
static bool dual_long_triggered = false;

// Edge events for the current tick
static bool left_short_event = false;
static bool left_long_event = false;
static bool right_short_event = false;
static bool right_long_event = false;
static bool dual_press_event = false;
static bool dual_long_event = false;

void buttons_update(void) {

  // Local thread-safe snapshot of volatile variables
  bool l_down = left_is_down;
  bool r_down = right_is_down;
  bool l_pressed = left_was_pressed;
  bool r_pressed = right_was_pressed;
  bool l_released = left_was_released;
  bool r_released = right_was_released;
  uint64_t l_press_t = left_press_time;
  uint64_t r_press_t = right_press_time;
  uint64_t now = to_us_since_boot(get_absolute_time());

  // Clear consumed volatile flags
  if (l_pressed) left_was_pressed = false;
  if (r_pressed) right_was_pressed = false;
  if (l_released) left_was_released = false;
  if (r_released) right_was_released = false;

  // 1. Detect dual press (both currently down)
  if (l_down && r_down) {
    if (!dual_active) {
      dual_active = true;
      dual_press_event = true;
      dual_press_time = now;
      dual_long_triggered = false;
      // Mark as long-triggered to prevent individual release actions
      left_long_triggered = true;
      right_long_triggered = true;
    } else {
      if (!dual_long_triggered && (now - dual_press_time >= 800000)) { // 800ms
        dual_long_triggered = true;
        dual_long_event = true;
      }
    }
  }

  // Handle dual-active teardown
  if (dual_active) {
    if (!l_down && !r_down) {
      dual_active = false;
      left_long_triggered = false;
      right_long_triggered = false;
      dual_long_triggered = false;
    }
    return; // Ignore individual buttons while dual press is active
  }

  // 2. Left button gesture tracking
  if (l_down) {
    if (!left_long_triggered && (now - l_press_t >= 500000)) { // 500ms threshold
      left_long_triggered = true;
      left_long_event = true;
    }
  }
  if (l_released) {
    if (left_long_triggered) {
      left_long_triggered = false;
    } else {
      left_short_event = true;
    }
  }

  // 3. Right button gesture tracking
  if (r_down) {
    if (!right_long_triggered && (now - r_press_t >= 500000)) { // 500ms threshold
      right_long_triggered = true;
      right_long_event = true;
    }
  }
  if (r_released) {
    if (right_long_triggered) {
      right_long_triggered = false;
    } else {
      right_short_event = true;
    }
  }
}

// ─── Public Queries ─────────────────────────────────────────────────────────
bool buttons_left_short(void) { return left_short_event; }
bool buttons_left_long(void) { return left_long_event; }
bool buttons_right_short(void) { return right_short_event; }
bool buttons_right_long(void) { return right_long_event; }
bool buttons_dual_press(void) { return dual_press_event; }
bool buttons_dual_long(void) { return dual_long_event; }

void buttons_clear_events(void) {
  left_short_event = false;
  left_long_event = false;
  right_short_event = false;
  right_long_event = false;
  dual_press_event = false;
  dual_long_event = false;
}
bool buttons_left_held(void) { return left_is_down; }
bool buttons_right_held(void) { return right_is_down; }
bool buttons_any_event(void) {
  return left_short_event || left_long_event || right_short_event ||
         right_long_event || dual_press_event || dual_long_event ||
         app_is_new_request_pending();
}
