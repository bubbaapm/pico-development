#include "lcd_animations.h"
#include "pico/stdlib.h" // stdio_init_all, sleep_ms, tight_loop_contents
#include "st7735.h"
#include <stdio.h> // printf

int main() {
  stdio_init_all();

  printf("Initializing ST7735 LCD...\n");
  lcd_init();
  printf("LCD initialized.\n");

  // ── Demo 1: Character-LCD-style API ──
  // Works just like the CyBot lcd_printf() — clears screen + prints
  lcd_set_text_color(COLOR_GREEN, COLOR_BLACK);
  lcd_printf("Hello, CyBot!\n\nPico 2 LCD Demo\n\nSPI1 @ 32 MHz");
  sleep_ms(2000);

  // ── Demo 2: lcd_gotoLine / lcd_setCursorPos ──
  lcd_clear();
  lcd_set_text_color(COLOR_CYAN, COLOR_BLACK);
  lcd_gotoLine(1);
  lcd_puts("Line 1");
  lcd_gotoLine(3);
  lcd_puts("Line 3");
  lcd_setCursorPos(10, 5);
  lcd_set_text_color(COLOR_YELLOW, COLOR_BLACK);
  lcd_puts("(10,5)");
  sleep_ms(2000);

  // ── Demo 3: Drawing helpers ──
  lcd_fill_screen(COLOR_BLACK);
  lcd_draw_rect(5, 5, 50, 40, COLOR_RED);
  lcd_fill_rect(60, 5, 50, 40, COLOR_BLUE);
  lcd_draw_line(0, 50, 127, 80, COLOR_GREEN);
  lcd_draw_circle(64, 110, 25, COLOR_PINK);
  lcd_fill_circle(64, 110, 12, COLOR_ORANGE);
  lcd_draw_string(10, 140, "Shapes!", COLOR_WHITE, COLOR_BLACK, 2);
  sleep_ms(2000);

  // ── Demo 4: Run all animations ──
  anim_demo_all();

  // Done — hold on final screen
  lcd_fill_screen(COLOR_BLACK);
  lcd_set_text_color(COLOR_WHITE, COLOR_BLACK);
  lcd_set_text_size(1);
  lcd_printf("All demos done!\n\nReady.");

  while (true) {
    // anim_plasma(10000);
    tight_loop_contents();
  }
}
