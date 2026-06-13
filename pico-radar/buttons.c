#include "buttons.h"
#include "pico/stdlib.h"

typedef struct {
  uint gpio;
  bool raw_last;
  bool debounced;
  bool prev_debounced;
  absolute_time_t last_change;
} btn_internal_t;

static btn_internal_t btn_left;
static btn_internal_t btn_right;

static void btn_init_one(btn_internal_t *btn, uint gpio) {
  btn->gpio = gpio;
  btn->raw_last = false;
  btn->debounced = false;
  btn->prev_debounced = false;
  btn->last_change = get_absolute_time();

  gpio_init(gpio);
  gpio_set_dir(gpio, GPIO_IN);
  gpio_pull_down(gpio); // Cap-touch modules drive HIGH when touched
}

void buttons_init(void) {
  btn_init_one(&btn_left, BTN_LEFT_PIN);
  btn_init_one(&btn_right, BTN_RIGHT_PIN);
}

static void btn_update_one(btn_internal_t *btn) {
  btn->prev_debounced = btn->debounced;
  bool raw = gpio_get(btn->gpio);

  if (raw != btn->raw_last) {
    btn->last_change = get_absolute_time();
    btn->raw_last = raw;
  }

  int64_t elapsed =
      absolute_time_diff_us(btn->last_change, get_absolute_time()) / 1000;
  if (elapsed >= BTN_DEBOUNCE_MS) {
    btn->debounced = btn->raw_last;
  }
}

void buttons_update(void) {
  btn_update_one(&btn_left);
  btn_update_one(&btn_right);
}

static button_state_t btn_get_state(const btn_internal_t *btn) {
  button_state_t s;
  s.pressed = btn->debounced;
  s.just_pressed = btn->debounced && !btn->prev_debounced;
  s.just_released = !btn->debounced && btn->prev_debounced;
  return s;
}

button_state_t buttons_get_left(void) { return btn_get_state(&btn_left); }
button_state_t buttons_get_right(void) { return btn_get_state(&btn_right); }
