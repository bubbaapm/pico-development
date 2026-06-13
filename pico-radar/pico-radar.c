#include "buttons.h"
#include "lcd_animations.h"
#include "ld2410.h"
#include "pico/stdlib.h"
#include "ui.h"
#include <stdio.h>


int main() {
  stdio_init_all();

  // ── Initialize all subsystems ──
  buttons_init();
  radar_init();
  ui_init(); // Also calls lcd_init()

  printf("[pico-radar] All systems initialized\n");

  // ── Splash screen with boot animation ──
  sleep_ms(2000); // Hold splash screen
  anim_rainbow_wipe(2);
  sleep_ms(500);

  // ── Switch to live radar view ──
  ui_set_screen(SCREEN_RADAR_LIVE);

  // ── Main loop ──
  while (true) {
    buttons_update();
    radar_update();
    ui_update();

    sleep_ms(30); // ~33 fps target
  }
}
