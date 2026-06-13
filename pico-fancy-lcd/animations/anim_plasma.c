#include "../lcd_animations.h"
#include "../buttons.h"
#include "../st7735.h"
#include "pico/stdlib.h"

void anim_plasma(uint32_t duration_ms) {
  static const uint8_t sin_table[256] = {
      128, 131, 134, 137, 140, 143, 146, 149, 152, 155, 158, 162, 165, 167, 170,
      173, 176, 179, 182, 185, 188, 190, 193, 196, 198, 201, 203, 206, 208, 211,
      213, 215, 218, 220, 222, 224, 226, 228, 230, 232, 234, 235, 237, 239, 240,
      241, 243, 244, 245, 246, 248, 249, 250, 250, 251, 252, 253, 253, 254, 254,
      254, 255, 255, 255, 255, 255, 255, 255, 254, 254, 254, 253, 253, 252, 251,
      250, 250, 249, 248, 246, 245, 244, 243, 241, 240, 239, 237, 235, 234, 232,
      230, 228, 226, 224, 222, 220, 218, 215, 213, 211, 208, 206, 203, 201, 198,
      196, 193, 190, 188, 185, 182, 179, 176, 173, 170, 167, 165, 162, 158, 155,
      152, 149, 146, 143, 140, 137, 134, 131, 128, 124, 121, 118, 115, 112, 109,
      106, 103, 100, 97,  93,  90,  88,  85,  82,  79,  76,  73,  70,  67,  65,
      62,  59,  57,  54,  52,  49,  47,  44,  42,  40,  37,  35,  33,  31,  29,
      27,  25,  23,  21,  20,  18,  16,  15,  14,  12,  11,  10,  9,   7,   6,
      5,   5,   4,   3,   2,   2,   1,   1,   1,   0,   0,   0,   0,   0,   0,
      0,   1,   1,   1,   2,   2,   3,   4,   5,   5,   6,   7,   9,   10,  11,
      12,  14,  15,  16,  18,  20,  21,  23,  25,  27,  29,  31,  33,  35,  37,
      40,  42,  44,  47,  49,  52,  54,  57,  59,  62,  65,  67,  70,  73,  76,
      79,  82,  85,  88,  90,  93,  97,  100, 103, 106, 109, 112, 115, 118, 121,
      124,
  };

#define PLASMA_SCALE 4
  uint16_t pw = LCD_WIDTH / PLASMA_SCALE;
  uint16_t ph = LCD_HEIGHT / PLASMA_SCALE;

  uint16_t frame = 0;
  absolute_time_t end;
  if (duration_ms > 0) {
    end = make_timeout_time_ms(duration_ms);
  }

  while (duration_ms == 0 || !time_reached(end)) {
    buttons_update();
    if (buttons_any_event())
      return;

    for (uint16_t py = 0; py < ph; py++) {
      for (uint16_t px = 0; px < pw; px++) {
        uint8_t v1 = sin_table[(uint8_t)(px * 8 + frame)];
        uint8_t v2 = sin_table[(uint8_t)(py * 6 + frame * 2)];
        uint8_t v3 = sin_table[(uint8_t)((px + py) * 4 + frame)];
        uint8_t v4 = sin_table[(uint8_t)((px * px + py * py) / 4 + frame * 3)];

        uint16_t hue = ((uint16_t)v1 + v2 + v3 + v4) * 360 / 1024;
        uint16_t color = hsv_to_rgb565(hue % 360, 220, 200);

        lcd_fill_rect(px * PLASMA_SCALE, py * PLASMA_SCALE, PLASMA_SCALE,
                      PLASMA_SCALE, color);
      }
    }
    lcd_flush();
    frame += 3;
  }
}
