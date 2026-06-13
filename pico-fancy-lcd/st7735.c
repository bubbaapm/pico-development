#include "st7735.h"

#include "hardware/spi.h"
#include "hardware/dma.h"
#include "pico/stdlib.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ─── SPI Instance ───────────────────────────────────────────────────────────
#define LCD_SPI_PORT spi1
#define LCD_SPI_BAUD 32000000 // 32 MHz (ST7735 max is ~30-40 MHz for writes)

// ─── Double Framebuffers (40 KB each) ──────────────
static uint16_t framebuffer_A[LCD_WIDTH * LCD_HEIGHT];
static uint16_t framebuffer_B[LCD_WIDTH * LCD_HEIGHT];
static uint16_t *active_framebuffer = framebuffer_A;
static uint16_t *sending_framebuffer = framebuffer_B;
static int lcd_dma_chan = -1;

// Byte-swap RGB565 for big-endian SPI wire format
static inline uint16_t swap16(uint16_t c) {
  return (uint16_t)((c >> 8) | (c << 8));
}

// ─── Built-in 5×7 Font (ASCII 0x20 – 0x7E) ─────────────────────────────────
// Each character is 5 bytes wide; each byte is a column bitmask (LSB = top
// row).
static const uint8_t font5x7[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, // 0x20 ' '
    0x00, 0x00, 0x5F, 0x00, 0x00, // 0x21 '!'
    0x00, 0x07, 0x00, 0x07, 0x00, // 0x22 '"'
    0x14, 0x7F, 0x14, 0x7F, 0x14, // 0x23 '#'
    0x24, 0x2A, 0x7F, 0x2A, 0x12, // 0x24 '$'
    0x23, 0x13, 0x08, 0x64, 0x62, // 0x25 '%'
    0x36, 0x49, 0x55, 0x22, 0x50, // 0x26 '&'
    0x00, 0x05, 0x03, 0x00, 0x00, // 0x27 '''
    0x00, 0x1C, 0x22, 0x41, 0x00, // 0x28 '('
    0x00, 0x41, 0x22, 0x1C, 0x00, // 0x29 ')'
    0x08, 0x2A, 0x1C, 0x2A, 0x08, // 0x2A '*'
    0x08, 0x08, 0x3E, 0x08, 0x08, // 0x2B '+'
    0x00, 0x50, 0x30, 0x00, 0x00, // 0x2C ','
    0x08, 0x08, 0x08, 0x08, 0x08, // 0x2D '-'
    0x00, 0x60, 0x60, 0x00, 0x00, // 0x2E '.'
    0x20, 0x10, 0x08, 0x04, 0x02, // 0x2F '/'
    0x3E, 0x51, 0x49, 0x45, 0x3E, // 0x30 '0'
    0x00, 0x42, 0x7F, 0x40, 0x00, // 0x31 '1'
    0x42, 0x61, 0x51, 0x49, 0x46, // 0x32 '2'
    0x21, 0x41, 0x45, 0x4B, 0x31, // 0x33 '3'
    0x18, 0x14, 0x12, 0x7F, 0x10, // 0x34 '4'
    0x27, 0x45, 0x45, 0x45, 0x39, // 0x35 '5'
    0x3C, 0x4A, 0x49, 0x49, 0x30, // 0x36 '6'
    0x01, 0x71, 0x09, 0x05, 0x03, // 0x37 '7'
    0x36, 0x49, 0x49, 0x49, 0x36, // 0x38 '8'
    0x06, 0x49, 0x49, 0x29, 0x1E, // 0x39 '9'
    0x00, 0x36, 0x36, 0x00, 0x00, // 0x3A ':'
    0x00, 0x56, 0x36, 0x00, 0x00, // 0x3B ';'
    0x00, 0x08, 0x14, 0x22, 0x41, // 0x3C '<'
    0x14, 0x14, 0x14, 0x14, 0x14, // 0x3D '='
    0x41, 0x22, 0x14, 0x08, 0x00, // 0x3E '>'
    0x02, 0x01, 0x51, 0x09, 0x06, // 0x3F '?'
    0x32, 0x49, 0x79, 0x41, 0x3E, // 0x40 '@'
    0x7E, 0x11, 0x11, 0x11, 0x7E, // 0x41 'A'
    0x7F, 0x49, 0x49, 0x49, 0x36, // 0x42 'B'
    0x3E, 0x41, 0x41, 0x41, 0x22, // 0x43 'C'
    0x7F, 0x41, 0x41, 0x22, 0x1C, // 0x44 'D'
    0x7F, 0x49, 0x49, 0x49, 0x41, // 0x45 'E'
    0x7F, 0x09, 0x09, 0x01, 0x01, // 0x46 'F'
    0x3E, 0x41, 0x41, 0x51, 0x32, // 0x47 'G'
    0x7F, 0x08, 0x08, 0x08, 0x7F, // 0x48 'H'
    0x00, 0x41, 0x7F, 0x41, 0x00, // 0x49 'I'
    0x20, 0x40, 0x41, 0x3F, 0x01, // 0x4A 'J'
    0x7F, 0x08, 0x14, 0x22, 0x41, // 0x4B 'K'
    0x7F, 0x40, 0x40, 0x40, 0x40, // 0x4C 'L'
    0x7F, 0x02, 0x04, 0x02, 0x7F, // 0x4D 'M'
    0x7F, 0x04, 0x08, 0x10, 0x7F, // 0x4E 'N'
    0x3E, 0x41, 0x41, 0x41, 0x3E, // 0x4F 'O'
    0x7F, 0x09, 0x09, 0x09, 0x06, // 0x50 'P'
    0x3E, 0x41, 0x51, 0x21, 0x5E, // 0x51 'Q'
    0x7F, 0x09, 0x19, 0x29, 0x46, // 0x52 'R'
    0x46, 0x49, 0x49, 0x49, 0x31, // 0x53 'S'
    0x01, 0x01, 0x7F, 0x01, 0x01, // 0x54 'T'
    0x3F, 0x40, 0x40, 0x40, 0x3F, // 0x55 'U'
    0x1F, 0x20, 0x40, 0x20, 0x1F, // 0x56 'V'
    0x7F, 0x20, 0x18, 0x20, 0x7F, // 0x57 'W'
    0x63, 0x14, 0x08, 0x14, 0x63, // 0x58 'X'
    0x03, 0x04, 0x78, 0x04, 0x03, // 0x59 'Y'
    0x61, 0x51, 0x49, 0x45, 0x43, // 0x5A 'Z'
    0x00, 0x00, 0x7F, 0x41, 0x41, // 0x5B '['
    0x02, 0x04, 0x08, 0x10, 0x20, // 0x5C '\'
    0x41, 0x41, 0x7F, 0x00, 0x00, // 0x5D ']'
    0x04, 0x02, 0x01, 0x02, 0x04, // 0x5E '^'
    0x40, 0x40, 0x40, 0x40, 0x40, // 0x5F '_'
    0x00, 0x01, 0x02, 0x04, 0x00, // 0x60 '`'
    0x20, 0x54, 0x54, 0x54, 0x78, // 0x61 'a'
    0x7F, 0x48, 0x44, 0x44, 0x38, // 0x62 'b'
    0x38, 0x44, 0x44, 0x44, 0x20, // 0x63 'c'
    0x38, 0x44, 0x44, 0x48, 0x7F, // 0x64 'd'
    0x38, 0x54, 0x54, 0x54, 0x18, // 0x65 'e'
    0x08, 0x7E, 0x09, 0x01, 0x02, // 0x66 'f'
    0x08, 0x14, 0x54, 0x54, 0x3C, // 0x67 'g'
    0x7F, 0x08, 0x04, 0x04, 0x78, // 0x68 'h'
    0x00, 0x44, 0x7D, 0x40, 0x00, // 0x69 'i'
    0x20, 0x40, 0x44, 0x3D, 0x00, // 0x6A 'j'
    0x00, 0x7F, 0x10, 0x28, 0x44, // 0x6B 'k'
    0x00, 0x41, 0x7F, 0x40, 0x00, // 0x6C 'l'
    0x7C, 0x04, 0x18, 0x04, 0x78, // 0x6D 'm'
    0x7C, 0x08, 0x04, 0x04, 0x78, // 0x6E 'n'
    0x38, 0x44, 0x44, 0x44, 0x38, // 0x6F 'o'
    0x7C, 0x14, 0x14, 0x14, 0x08, // 0x70 'p'
    0x08, 0x14, 0x14, 0x18, 0x7C, // 0x71 'q'
    0x7C, 0x08, 0x04, 0x04, 0x08, // 0x72 'r'
    0x48, 0x54, 0x54, 0x54, 0x20, // 0x73 's'
    0x04, 0x3F, 0x44, 0x40, 0x20, // 0x74 't'
    0x3C, 0x40, 0x40, 0x20, 0x7C, // 0x75 'u'
    0x1C, 0x20, 0x40, 0x20, 0x1C, // 0x76 'v'
    0x3C, 0x40, 0x30, 0x40, 0x3C, // 0x77 'w'
    0x44, 0x28, 0x10, 0x28, 0x44, // 0x78 'x'
    0x0C, 0x50, 0x50, 0x50, 0x3C, // 0x79 'y'
    0x44, 0x64, 0x54, 0x4C, 0x44, // 0x7A 'z'
    0x00, 0x08, 0x36, 0x41, 0x00, // 0x7B '{'
    0x00, 0x00, 0x7F, 0x00, 0x00, // 0x7C '|'
    0x00, 0x41, 0x36, 0x08, 0x00, // 0x7D '}'
    0x08, 0x08, 0x2A, 0x1C, 0x08, // 0x7E '~'
};

// ─── Low-level SPI helpers ──────────────────────────────────────────────────

static inline void cs_select(void) { gpio_put(LCD_PIN_CS, 0); }

static inline void cs_deselect(void) { gpio_put(LCD_PIN_CS, 1); }

static inline void dc_command(void) { gpio_put(LCD_PIN_DC, 0); }

static inline void dc_data(void) { gpio_put(LCD_PIN_DC, 1); }

/**
 * Write a single command byte (DC low).
 */
static void lcd_write_cmd(uint8_t cmd) {
  dc_command();
  cs_select();
  spi_write_blocking(LCD_SPI_PORT, &cmd, 1);
  cs_deselect();
}

/**
 * Write data bytes (DC high).
 */
static void lcd_write_data(const uint8_t *data, size_t len) {
  dc_data();
  cs_select();
  spi_write_blocking(LCD_SPI_PORT, data, len);
  cs_deselect();
}

/**
 * Write a single data byte.
 */
static void lcd_write_data_byte(uint8_t data) { lcd_write_data(&data, 1); }

// ─── Initialization ─────────────────────────────────────────────────────────

void lcd_init(void) {
  // ── SPI1 peripheral setup ──
  spi_init(LCD_SPI_PORT, LCD_SPI_BAUD);
  gpio_set_function(LCD_PIN_SCK, GPIO_FUNC_SPI);
  gpio_set_function(LCD_PIN_MOSI, GPIO_FUNC_SPI);
  // MISO (GPIO 12) left unconfigured — not connected

  // ── Manual CS, DC, RST as GPIO outputs ──
  gpio_init(LCD_PIN_CS);
  gpio_set_dir(LCD_PIN_CS, GPIO_OUT);
  gpio_put(LCD_PIN_CS, 1); // idle high

  gpio_init(LCD_PIN_DC);
  gpio_set_dir(LCD_PIN_DC, GPIO_OUT);
  gpio_put(LCD_PIN_DC, 1);

  gpio_init(LCD_PIN_RST);
  gpio_set_dir(LCD_PIN_RST, GPIO_OUT);
  gpio_put(LCD_PIN_RST, 1);

  // ── Hardware reset ──
  gpio_put(LCD_PIN_RST, 1);
  sleep_ms(50);
  gpio_put(LCD_PIN_RST, 0);
  sleep_ms(50);
  gpio_put(LCD_PIN_RST, 1);
  sleep_ms(150);

  // ── Software reset ──
  lcd_write_cmd(ST7735_SWRESET);
  sleep_ms(150);

  // ── Sleep out ──
  lcd_write_cmd(ST7735_SLPOUT);
  sleep_ms(150);

  // ── Frame rate control (normal mode) ──
  lcd_write_cmd(ST7735_FRMCTR1);
  {
    uint8_t d[] = {0x01, 0x2C, 0x2D};
    lcd_write_data(d, sizeof(d));
  }

  // ── Frame rate control (idle mode) ──
  lcd_write_cmd(ST7735_FRMCTR2);
  {
    uint8_t d[] = {0x01, 0x2C, 0x2D};
    lcd_write_data(d, sizeof(d));
  }

  // ── Frame rate control (partial mode) ──
  lcd_write_cmd(ST7735_FRMCTR3);
  {
    uint8_t d[] = {0x01, 0x2C, 0x2D, 0x01, 0x2C, 0x2D};
    lcd_write_data(d, sizeof(d));
  }

  // ── Display inversion control ──
  lcd_write_cmd(ST7735_INVCTR);
  lcd_write_data_byte(0x07);

  // ── Power control 1 ──
  lcd_write_cmd(ST7735_PWCTR1);
  {
    uint8_t d[] = {0xA2, 0x02, 0x84};
    lcd_write_data(d, sizeof(d));
  }

  // ── Power control 2 ──
  lcd_write_cmd(ST7735_PWCTR2);
  lcd_write_data_byte(0xC5);

  // ── Power control 3 (normal mode) ──
  lcd_write_cmd(ST7735_PWCTR3);
  {
    uint8_t d[] = {0x0A, 0x00};
    lcd_write_data(d, sizeof(d));
  }

  // ── Power control 4 (idle mode) ──
  lcd_write_cmd(ST7735_PWCTR4);
  {
    uint8_t d[] = {0x8A, 0x2A};
    lcd_write_data(d, sizeof(d));
  }

  // ── Power control 5 (partial mode) ──
  lcd_write_cmd(ST7735_PWCTR5);
  {
    uint8_t d[] = {0x8A, 0xEE};
    lcd_write_data(d, sizeof(d));
  }

  // ── VCOM control ──
  lcd_write_cmd(ST7735_VMCTR1);
  lcd_write_data_byte(0x0E);

  // ── Display inversion off ──
  lcd_write_cmd(ST7735_INVOFF);

  // ── Memory access control ──
  // 90° CCW rotation: MV | MY gives landscape, flipped from MV | MX
  lcd_write_cmd(ST7735_MADCTL);
  lcd_write_data_byte(MADCTL_MV | MADCTL_MY | MADCTL_RGB);

  // ── Color mode: 16-bit (RGB565) ──
  lcd_write_cmd(ST7735_COLMOD);
  lcd_write_data_byte(0x05);

  // ── Column address set (0 – 127) ──
  lcd_write_cmd(ST7735_CASET);
  {
    uint8_t d[] = {0x00, 0x00, 0x00, 0x7F};
    lcd_write_data(d, sizeof(d));
  }

  // ── Row address set (0 – 159) ──
  lcd_write_cmd(ST7735_RASET);
  {
    uint8_t d[] = {0x00, 0x00, 0x00, 0x9F};
    lcd_write_data(d, sizeof(d));
  }

  // ── Positive gamma correction ──
  lcd_write_cmd(ST7735_GMCTRP1);
  {
    uint8_t d[] = {0x02, 0x1C, 0x07, 0x12, 0x37, 0x32, 0x29, 0x2D,
                   0x29, 0x25, 0x2B, 0x39, 0x00, 0x01, 0x03, 0x10};
    lcd_write_data(d, sizeof(d));
  }

  // ── Negative gamma correction ──
  lcd_write_cmd(ST7735_GMCTRN1);
  {
    uint8_t d[] = {0x03, 0x1D, 0x07, 0x06, 0x2E, 0x2C, 0x29, 0x2D,
                   0x2E, 0x2E, 0x37, 0x3F, 0x00, 0x00, 0x02, 0x10};
    lcd_write_data(d, sizeof(d));
  }

  // ── Normal display mode on ──
  lcd_write_cmd(ST7735_NORON);
  sleep_ms(10);

  // ── Display on ──
  lcd_write_cmd(ST7735_DISPON);
  sleep_ms(100);

  // ── Clear framebuffers ──
  memset(framebuffer_A, 0, sizeof(framebuffer_A));
  memset(framebuffer_B, 0, sizeof(framebuffer_B));

  // ── Configure SPI DMA Channel ──
  lcd_dma_chan = dma_claim_unused_channel(true);
  dma_channel_config c = dma_channel_get_default_config(lcd_dma_chan);
  channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
  channel_config_set_read_increment(&c, true);
  channel_config_set_write_increment(&c, false);
  channel_config_set_dreq(&c, spi_get_dreq(LCD_SPI_PORT, true));

  dma_channel_configure(
      lcd_dma_chan,
      &c,
      &spi_get_hw(LCD_SPI_PORT)->dr, // write address
      NULL,                          // read address
      LCD_WIDTH * LCD_HEIGHT * 2,    // transfer count (total bytes)
      false                          // trigger manually
  );
}

// ─── Drawing Primitives (all write to framebuffer) ──────────────────────────

void lcd_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
  x0 += LCD_COL_OFFSET;
  x1 += LCD_COL_OFFSET;
  y0 += LCD_ROW_OFFSET;
  y1 += LCD_ROW_OFFSET;

  lcd_write_cmd(ST7735_CASET);
  {
    uint8_t d[] = {(uint8_t)(x0 >> 8), (uint8_t)x0, (uint8_t)(x1 >> 8),
                   (uint8_t)x1};
    lcd_write_data(d, sizeof(d));
  }

  lcd_write_cmd(ST7735_RASET);
  {
    uint8_t d[] = {(uint8_t)(y0 >> 8), (uint8_t)y0, (uint8_t)(y1 >> 8),
                   (uint8_t)y1};
    lcd_write_data(d, sizeof(d));
  }

  lcd_write_cmd(ST7735_RAMWR);
}

// ─── Framebuffer Flush ──────────────────────────────────────────────────────

void lcd_flush(void) {
  // Wait for the previous DMA to finish
  if (lcd_dma_chan >= 0 && dma_channel_is_busy(lcd_dma_chan)) {
    dma_channel_wait_for_finish_blocking(lcd_dma_chan);
  }
  // Wait for the SPI peripheral to finish shifting out the last bytes
  while (spi_is_busy(LCD_SPI_PORT)) {
    tight_loop_contents();
  }
  // CS was selected for the previous DMA, deselect it now
  cs_deselect();

  // Swap active and sending framebuffers
  uint16_t *temp = active_framebuffer;
  active_framebuffer = sending_framebuffer;
  sending_framebuffer = temp;

  // Setup window for the new frame
  lcd_set_window(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1);
  dc_data();
  cs_select();

  // Start DMA transfer
  dma_channel_set_read_addr(lcd_dma_chan, sending_framebuffer, true);
}

const uint16_t *lcd_get_display_framebuffer(void) { return sending_framebuffer; }

// ─── Pixel / Fill (write to framebuffer with clipping) ───────────────────────

void lcd_draw_pixel(int16_t x, int16_t y, uint16_t color) {
  if (x < 0 || x >= LCD_WIDTH || y < 0 || y >= LCD_HEIGHT)
    return;
  active_framebuffer[y * LCD_WIDTH + x] = swap16(color);
}

void lcd_fill_screen(uint16_t color) {
  uint16_t sc = swap16(color);
  for (uint32_t i = 0; i < LCD_WIDTH * LCD_HEIGHT; i++)
    active_framebuffer[i] = sc;
}

void lcd_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h,
                   uint16_t color) {
  if (x >= LCD_WIDTH || y >= LCD_HEIGHT)
    return;
  if (w <= 0 || h <= 0)
    return;

  // Clip left
  if (x < 0) {
    w += x;
    x = 0;
  }
  // Clip top
  if (y < 0) {
    h += y;
    y = 0;
  }
  // Clip right
  if (x + w > LCD_WIDTH) {
    w = LCD_WIDTH - x;
  }
  // Clip bottom
  if (y + h > LCD_HEIGHT) {
    h = LCD_HEIGHT - y;
  }

  if (w <= 0 || h <= 0)
    return;

  uint16_t sc = swap16(color);
  for (int16_t row = y; row < y + h; row++) {
    uint16_t *p = &active_framebuffer[row * LCD_WIDTH + x];
    for (int16_t col = 0; col < w; col++)
      p[col] = sc;
  }
}

void lcd_draw_char(int16_t x, int16_t y, char ch, uint16_t color, uint16_t bg,
                   uint8_t size) {
  if (ch < 0x20 || ch > 0x7E)
    ch = '?';

  const uint8_t *glyph = &font5x7[(ch - 0x20) * 5];

  if (size == 1) {
    // ── Fast path: direct framebuffer writes for 6×8 character cell ──
    // Clip character cell bounds
    if (x + 6 > LCD_WIDTH || y + 8 > LCD_HEIGHT || x < 0 || y < 0) {
      // Fallback to pixel-by-pixel if partially off-screen
      for (uint8_t col = 0; col < 5; col++) {
        uint8_t column_data = glyph[col];
        for (uint8_t row = 0; row < 7; row++) {
          uint16_t c = (column_data & (1 << row)) ? color : bg;
          lcd_draw_pixel(x + col, y + row, c);
        }
      }
      // Gap column
      for (uint8_t row = 0; row < 7; row++) {
        lcd_draw_pixel(x + 5, y + row, bg);
      }
      return;
    }

    uint16_t fg_s = swap16(color);
    uint16_t bg_s = swap16(bg);

    for (uint8_t row = 0; row < 8; row++) {
      uint16_t *p = &active_framebuffer[(y + row) * LCD_WIDTH + x];
      for (uint8_t col = 0; col < 6; col++) {
        bool lit = (col < 5 && row < 7) && (glyph[col] & (1 << row));
        p[col] = lit ? fg_s : bg_s;
      }
    }
  } else {
    // ── Scaled path: use fill_rect for each sub-block ──
    for (uint8_t col = 0; col < 5; col++) {
      uint8_t column_data = glyph[col];
      for (uint8_t row = 0; row < 7; row++) {
        uint16_t c = (column_data & (1 << row)) ? color : bg;
        lcd_fill_rect(x + col * size, y + row * size, size, size, c);
      }
    }
    // 1-pixel-wide gap column (background), scaled
    for (uint8_t row = 0; row < 7; row++) {
      lcd_fill_rect(x + 5 * size, y + row * size, size, size, bg);
    }
  }
}

void lcd_draw_string(uint16_t x, uint16_t y, const char *str, uint16_t color,
                     uint16_t bg, uint8_t size) {
  uint16_t cursor_x = x;
  while (*str) {
    if (cursor_x + 6 * size > LCD_WIDTH) {
      cursor_x = 0;
      y += 8 * size;
    }
    if (y + 7 * size > LCD_HEIGHT)
      break;

    lcd_draw_char(cursor_x, y, *str, color, bg, size);
    cursor_x += 6 * size; // 5 pixels + 1 gap, scaled
    str++;
  }
}

// ─── Character-LCD-Style Text Terminal ──────────────────────────────────────

// Internal cursor state
static uint8_t cursor_col = 0;
static uint8_t cursor_row = 0;
static uint16_t text_fg = COLOR_WHITE;
static uint16_t text_bg = COLOR_BLACK;
static uint8_t text_size = 1;

void lcd_clear(void) {
  lcd_fill_screen(text_bg);
  cursor_col = 0;
  cursor_row = 0;
}

void lcd_home(void) {
  cursor_col = 0;
  cursor_row = 0;
}

void lcd_gotoLine(uint8_t lineNum) {
  // 1-indexed to match the original CyBot lcd.h convention
  if (lineNum < 1)
    lineNum = 1;
  uint8_t max_rows = LCD_TEXT_ROWS / text_size;
  if (lineNum > max_rows)
    lineNum = max_rows;
  cursor_row = lineNum - 1;
  cursor_col = 0;
}

void lcd_setCursorPos(uint8_t col, uint8_t row) {
  uint8_t max_cols = LCD_TEXT_COLS / text_size;
  uint8_t max_rows = LCD_TEXT_ROWS / text_size;
  if (col >= max_cols || row >= max_rows)
    return;
  cursor_col = col;
  cursor_row = row;
}

void lcd_putc(char c) {
  uint8_t max_cols = LCD_TEXT_COLS / text_size;
  uint8_t max_rows = LCD_TEXT_ROWS / text_size;

  if (c == '\n') {
    cursor_col = 0;
    cursor_row++;
    if (cursor_row >= max_rows)
      cursor_row = 0; // wrap
    return;
  }
  if (c == '\r') {
    cursor_col = 0;
    return;
  }

  uint16_t px = cursor_col * LCD_CHAR_WIDTH * text_size;
  uint16_t py = cursor_row * LCD_CHAR_HEIGHT * text_size;
  lcd_draw_char(px, py, c, text_fg, text_bg, text_size);

  cursor_col++;
  if (cursor_col >= max_cols) {
    cursor_col = 0;
    cursor_row++;
    if (cursor_row >= max_rows)
      cursor_row = 0;
  }
}

void lcd_puts(const char *str) {
  while (*str) {
    lcd_putc(*str);
    str++;
  }
}

void lcd_printf(const char *format, ...) {
  char buffer[LCD_TEXT_COLS * LCD_TEXT_ROWS + 1];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);

  lcd_clear();
  lcd_puts(buffer);
}

void lcd_set_text_color(uint16_t fg, uint16_t bg) {
  text_fg = fg;
  text_bg = bg;
}

void lcd_set_text_size(uint8_t size) {
  if (size < 1)
    size = 1;
  text_size = size;
}

// ─── Additional Drawing Helpers ─────────────────────────────────────────────

void lcd_draw_rect(int16_t x, int16_t y, int16_t w, int16_t h,
                   uint16_t color) {
  // Top and bottom horizontal lines
  lcd_fill_rect(x, y, w, 1, color);
  lcd_fill_rect(x, y + h - 1, w, 1, color);
  // Left and right vertical lines
  lcd_fill_rect(x, y, 1, h, color);
  lcd_fill_rect(x + w - 1, y, 1, h, color);
}

void lcd_draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                   uint16_t color) {
  // Bresenham's line algorithm
  int dx = abs(x1 - x0);
  int dy = -abs(y1 - y0);
  int sx = (x0 < x1) ? 1 : -1;
  int sy = (y0 < y1) ? 1 : -1;
  int err = dx + dy;

  for (;;) {
    lcd_draw_pixel(x0, y0, color);
    if (x0 == x1 && y0 == y1)
      break;
    int e2 = 2 * err;
    if (e2 >= dy) {
      err += dy;
      x0 += sx;
    }
    if (e2 <= dx) {
      err += dx;
      y0 += sy;
    }
  }
}

void lcd_draw_circle(int16_t cx, int16_t cy, int16_t r, uint16_t color) {
  // Midpoint circle algorithm
  int x = (int)r;
  int y = 0;
  int err = 1 - x;

  while (x >= y) {
    lcd_draw_pixel(cx + x, cy + y, color);
    lcd_draw_pixel(cx - x, cy + y, color);
    lcd_draw_pixel(cx + x, cy - y, color);
    lcd_draw_pixel(cx - x, cy - y, color);
    lcd_draw_pixel(cx + y, cy + x, color);
    lcd_draw_pixel(cx - y, cy + x, color);
    lcd_draw_pixel(cx + y, cy - x, color);
    lcd_draw_pixel(cx - y, cy - x, color);
    y++;
    if (err < 0) {
      err += 2 * y + 1;
    } else {
      x--;
      err += 2 * (y - x) + 1;
    }
  }
}

void lcd_fill_circle(int16_t cx, int16_t cy, int16_t r, uint16_t color) {
  // Draw horizontal lines across the circle for each scan line
  int x = (int)r;
  int y = 0;
  int err = 1 - x;

  while (x >= y) {
    // Draw horizontal lines between symmetric points
    lcd_fill_rect(cx - x, cy + y, 2 * x + 1, 1, color);
    lcd_fill_rect(cx - x, cy - y, 2 * x + 1, 1, color);
    lcd_fill_rect(cx - y, cy + x, 2 * y + 1, 1, color);
    lcd_fill_rect(cx - y, cy - x, 2 * y + 1, 1, color);
    y++;
    if (err < 0) {
      err += 2 * y + 1;
    } else {
      x--;
      err += 2 * (y - x) + 1;
    }
  }
}

static void swap_int16s(int16_t *a, int16_t *b) {
  int16_t t = *a;
  *a = *b;
  *b = t;
}

void lcd_fill_triangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                       int16_t x2, int16_t y2, uint16_t color) {
  // Sort coordinates by y (y0 <= y1 <= y2)
  if (y0 > y1) { swap_int16s(&x0, &x1); swap_int16s(&y0, &y1); }
  if (y1 > y2) { swap_int16s(&x1, &x2); swap_int16s(&y1, &y2); }
  if (y0 > y1) { swap_int16s(&x0, &x1); swap_int16s(&y0, &y1); }

  if (y0 == y2) return; // Degenerate triangle

  int32_t total_height = y2 - y0;
  int32_t dx02 = x2 - x0;
  int32_t dx01 = x1 - x0;
  int32_t dx12 = x2 - x1;

  for (int16_t y = y0; y <= y2; y++) {
    if (y < 0 || y >= LCD_HEIGHT) continue;
    
    int32_t cur_y_diff = y - y0;
    // Calculate ax (on edge 0-2)
    int16_t ax = x0 + (dx02 * cur_y_diff) / total_height;
    
    // Calculate bx (on edge 0-1 or 1-2)
    int16_t bx;
    if (y < y1) {
      int32_t h = y1 - y0;
      if (h == 0) bx = x0;
      else bx = x0 + (dx01 * cur_y_diff) / h;
    } else {
      int32_t h = y2 - y1;
      if (h == 0) bx = x1;
      else bx = x1 + (dx12 * (y - y1)) / h;
    }
    
    if (ax > bx) swap_int16s(&ax, &bx);
    
    int16_t start_x = ax < 0 ? 0 : ax;
    int16_t end_x = bx >= LCD_WIDTH ? LCD_WIDTH - 1 : bx;
    
    for (int16_t x = start_x; x <= end_x; x++) {
      lcd_draw_pixel(x, y, color);
    }
  }
}

void lcd_draw_triangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                       int16_t x2, int16_t y2, uint16_t color) {
  lcd_draw_line(x0, y0, x1, y1, color);
  lcd_draw_line(x1, y1, x2, y2, color);
  lcd_draw_line(x2, y2, x0, y0, color);
}
