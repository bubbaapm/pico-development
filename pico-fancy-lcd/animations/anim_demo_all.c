#include "../lcd_animations.h"
#include "../st7735.h"
#include "pico/stdlib.h"

void anim_demo_all(void) {
  // Title card
  lcd_fill_screen(COLOR_BLACK);
  lcd_set_text_color(COLOR_CYAN, COLOR_BLACK);
  lcd_set_text_size(2);
  lcd_printf("CyBot\n LCD");
  sleep_ms(1500);

  // 1. Rainbow wipe
  lcd_set_text_color(COLOR_WHITE, COLOR_BLACK);
  lcd_set_text_size(1);
  anim_rainbow_wipe(3);
  sleep_ms(500);

  // 2. Radar pulse
  anim_radar_pulse(15);
  sleep_ms(500);

  // 3. Matrix rain
  anim_matrix_rain(5000);
  sleep_ms(300);

  // 4. Bouncing ball
  anim_bouncing_ball(5000);
  sleep_ms(300);

  // 5. Starfield
  anim_starfield(5000);
  sleep_ms(300);

  // 6. Plasma
  anim_plasma(5000);
  sleep_ms(300);

  // Outro
  lcd_fill_screen(COLOR_BLACK);
  lcd_set_text_color(COLOR_GREEN, COLOR_BLACK);
  lcd_set_text_size(1);
  lcd_gotoLine(8);
  lcd_puts("  Demo complete!");
  sleep_ms(2000);
}
