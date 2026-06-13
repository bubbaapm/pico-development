#ifndef ST7735_H
#define ST7735_H

#include <stdbool.h>
#include <stdint.h>

// ─── Pin Assignments (SPI1) ─────────────────────────────────────────────────
//  Physical pins 14-17 → GPIO 10-13
#define LCD_PIN_SCK 10  // SPI1 SCK
#define LCD_PIN_MOSI 11 // SPI1 TX
// GPIO 12 (SPI1 RX) not used — LCD is write-only
#define LCD_PIN_CS 13  // Chip select (manual GPIO)
#define LCD_PIN_RST 14 // Reset (pin 19)
#define LCD_PIN_DC 15  // Data/Command (pin 20)

// ─── Display Dimensions ─────────────────────────────────────────────────────
#define LCD_WIDTH 160
#define LCD_HEIGHT 128

// ST7735 internal RAM is 132×162; the panel is 128×160.
// These offsets shift the drawing origin to the correct visible area.
// After 90° CW rotation the axes swap, so offsets swap too.
#define LCD_COL_OFFSET 1
#define LCD_ROW_OFFSET 2

// ─── Text Terminal Grid ─────────────────────────────────────────────────────
// At size=1 the 5×7 font with 1px gap gives 6×8 px per character cell.
// 128/6 = 21 chars/line, 160/8 = 20 lines  (generous compared to 4×20!)
#define LCD_CHAR_WIDTH 6  // pixels per char cell (5 font + 1 gap)
#define LCD_CHAR_HEIGHT 8 // pixels per char cell (7 font + 1 gap)
#define LCD_TEXT_COLS (LCD_WIDTH / LCD_CHAR_WIDTH)   // 21
#define LCD_TEXT_ROWS (LCD_HEIGHT / LCD_CHAR_HEIGHT) // 20

// ─── ST7735 Command Definitions ─────────────────────────────────────────────
#define ST7735_NOP 0x00
#define ST7735_SWRESET 0x01
#define ST7735_RDDID 0x04
#define ST7735_RDDST 0x09

#define ST7735_SLPIN 0x10
#define ST7735_SLPOUT 0x11
#define ST7735_PTLON 0x12
#define ST7735_NORON 0x13

#define ST7735_INVOFF 0x20
#define ST7735_INVON 0x21
#define ST7735_DISPOFF 0x28
#define ST7735_DISPON 0x29

#define ST7735_CASET 0x2A
#define ST7735_RASET 0x2B
#define ST7735_RAMWR 0x2C
#define ST7735_RAMRD 0x2E

#define ST7735_PTLAR 0x30
#define ST7735_COLMOD 0x3A
#define ST7735_MADCTL 0x36

#define ST7735_FRMCTR1 0xB1
#define ST7735_FRMCTR2 0xB2
#define ST7735_FRMCTR3 0xB3
#define ST7735_INVCTR 0xB4

#define ST7735_PWCTR1 0xC0
#define ST7735_PWCTR2 0xC1
#define ST7735_PWCTR3 0xC2
#define ST7735_PWCTR4 0xC3
#define ST7735_PWCTR5 0xC4
#define ST7735_VMCTR1 0xC5

#define ST7735_GMCTRP1 0xE0
#define ST7735_GMCTRN1 0xE1

// ─── MADCTL Bits ────────────────────────────────────────────────────────────
#define MADCTL_MY 0x80  // Row address order
#define MADCTL_MX 0x40  // Column address order
#define MADCTL_MV 0x20  // Row/Column exchange
#define MADCTL_ML 0x10  // Vertical refresh order
#define MADCTL_RGB 0x00 // RGB color order
#define MADCTL_BGR 0x08 // BGR color order
#define MADCTL_MH 0x04  // Horizontal refresh order

// ─── Common 16-bit RGB565 Colors ────────────────────────────────────────────
#define COLOR_BLACK 0x0000
#define COLOR_RED 0xF800
#define COLOR_GREEN 0x07E0
#define COLOR_BLUE 0x001F
#define COLOR_YELLOW 0xFFE0
#define COLOR_ORANGE 0xFBE0
#define COLOR_PINK 0xF81F
#define COLOR_WHITE 0xFFFF
#define COLOR_CYAN 0x07FF
#define COLOR_MAGENTA 0xF81F
#define COLOR_DIM 0x528A

// ─── Public API ─────────────────────────────────────────────────────────────

void lcd_init(void);
void lcd_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void lcd_draw_pixel(int16_t x, int16_t y, uint16_t color);
void lcd_fill_screen(uint16_t color);
void lcd_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
void lcd_draw_char(int16_t x, int16_t y, char ch, uint16_t color, uint16_t bg, uint8_t size);
void lcd_draw_string(uint16_t x, uint16_t y, const char *str, uint16_t color, uint16_t bg, uint8_t size);

// ─── Character‑LCD‑Style Convenience API ────────────────────────────────────
void lcd_clear(void);
void lcd_home(void);
void lcd_gotoLine(uint8_t lineNum);
void lcd_setCursorPos(uint8_t col, uint8_t row);
void lcd_putc(char c);
void lcd_puts(const char *str);
void lcd_printf(const char *format, ...);
void lcd_set_text_color(uint16_t fg, uint16_t bg);
void lcd_set_text_size(uint8_t size);

// ─── Additional Drawing Helpers ─────────────────────────────────────────────
void lcd_draw_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
void lcd_draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
void lcd_fill_circle(int16_t cx, int16_t cy, int16_t r, uint16_t color);
void lcd_draw_circle(int16_t cx, int16_t cy, int16_t r, uint16_t color);

/** Draw a filled triangle. */
void lcd_fill_triangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color);

/** Draw a triangle outline. */
void lcd_draw_triangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color);

void lcd_flush(void);
const uint16_t *lcd_get_display_framebuffer(void);

static inline uint16_t lcd_color565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

#endif // ST7735_H
