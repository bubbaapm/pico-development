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

// ─── Public API ─────────────────────────────────────────────────────────────

/**
 * @brief Initialize the ST7735 LCD.
 *
 * Sets up SPI1, configures GPIO pins (CS, DC, RST),
 * performs hardware reset, and runs the full ST7735R
 * (BLACKTAB) initialization command sequence.
 */
void lcd_init(void);

/**
 * @brief Set the drawing window (column/row address range).
 *
 * Subsequent RAMWR data will fill pixels left-to-right,
 * top-to-bottom within this window.
 *
 * @param x0  Start column (0–127).
 * @param y0  Start row    (0–159).
 * @param x1  End column   (0–127, inclusive).
 * @param y1  End row      (0–159, inclusive).
 */
void lcd_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);

/**
 * @brief Draw a single pixel.
 * @param x      Column (0–127).
 * @param y      Row    (0–159).
 * @param color  16-bit RGB565 color.
 */
void lcd_draw_pixel(uint16_t x, uint16_t y, uint16_t color);

/**
 * @brief Fill the entire screen with one color.
 * @param color  16-bit RGB565 color.
 */
void lcd_fill_screen(uint16_t color);

/**
 * @brief Fill a rectangle.
 * @param x      Top-left column.
 * @param y      Top-left row.
 * @param w      Width in pixels.
 * @param h      Height in pixels.
 * @param color  16-bit RGB565 color.
 */
void lcd_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                   uint16_t color);

/**
 * @brief Draw a single ASCII character using a built-in 5×7 font.
 * @param x     Column of top-left corner.
 * @param y     Row of top-left corner.
 * @param ch    Character to draw (ASCII 0x20–0x7E).
 * @param color Foreground color (RGB565).
 * @param bg    Background color (RGB565).
 * @param size  Scale factor (1 = 5×7, 2 = 10×14, etc.).
 */
void lcd_draw_char(uint16_t x, uint16_t y, char ch, uint16_t color, uint16_t bg,
                   uint8_t size);

/**
 * @brief Draw a null-terminated string.
 * @param x     Starting column.
 * @param y     Starting row.
 * @param str   Null-terminated ASCII string.
 * @param color Foreground color (RGB565).
 * @param bg    Background color (RGB565).
 * @param size  Scale factor.
 */
void lcd_draw_string(uint16_t x, uint16_t y, const char *str, uint16_t color,
                     uint16_t bg, uint8_t size);

// ─── Character‑LCD‑Style Convenience API ────────────────────────────────────
// Mirrors the CyBot 4×20 character LCD (lcd.h) but on the graphical display.
// An internal cursor tracks the current text position in character‑grid coords.

/**
 * @brief Clear the screen and reset the text cursor to (0,0).
 */
void lcd_clear(void);

/**
 * @brief Move cursor to line 0, column 0.
 */
void lcd_home(void);

/**
 * @brief Move cursor to the beginning of a 1‑indexed line (1–20).
 */
void lcd_gotoLine(uint8_t lineNum);

/**
 * @brief Set cursor to a character‑grid position (0‑indexed).
 * @param col  Column (0 – LCD_TEXT_COLS-1).
 * @param row  Row    (0 – LCD_TEXT_ROWS-1).
 */
void lcd_setCursorPos(uint8_t col, uint8_t row);

/**
 * @brief Print a single character at the cursor and advance it.
 */
void lcd_putc(char c);

/**
 * @brief Print a null‑terminated string at the cursor.
 */
void lcd_puts(const char *str);

/**
 * @brief printf‑style formatted print to the LCD.
 *
 * Clears the screen, then prints the formatted string starting at (0,0).
 * Supports \n for line breaks — same usage as the CyBot lcd_printf().
 */
void lcd_printf(const char *format, ...);

/**
 * @brief Set the foreground and background colors used by the
 *        character‑mode functions (lcd_putc, lcd_puts, lcd_printf).
 */
void lcd_set_text_color(uint16_t fg, uint16_t bg);

/**
 * @brief Set the text scale factor (1 = 5×7, 2 = 10×14, …).
 */
void lcd_set_text_size(uint8_t size);

// ─── Additional Drawing Helpers ─────────────────────────────────────────────

/** Draw a 1‑px outline rectangle (no fill). */
void lcd_draw_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                   uint16_t color);

/** Draw a line between two points (Bresenham). */
void lcd_draw_line(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                   uint16_t color);

/** Draw a filled circle. */
void lcd_fill_circle(uint16_t cx, uint16_t cy, uint16_t r, uint16_t color);

/** Draw a circle outline. */
void lcd_draw_circle(uint16_t cx, uint16_t cy, uint16_t r, uint16_t color);

/** Create an RGB565 color from 8‑bit R, G, B. */
static inline uint16_t lcd_color565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

#endif // ST7735_H
