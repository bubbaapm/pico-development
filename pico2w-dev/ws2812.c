/**
 * @file  ws2812.c
 * @brief Register-level WS2812B driver for RP2350 using PIO0, state machine 0.
 *
 * Two access paths:
 *   1. Raw volatile pointer macros  (ws2812_use_raw_pointers == true)
 *   2. SDK hardware structs          (ws2812_use_raw_pointers == false)
 *
 * Both paths perform identical register operations - the raw path is useful
 * for embedded-systems, while the struct path is more readable for production
 * code.
 */

#include "ws2812.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "hardware/clocks.h"
#include "pico/stdlib.h"

// --- Register-level headers --------------------------------------------------
#include "hardware/regs/addressmap.h"
#include "hardware/regs/io_bank0.h"
#include "hardware/regs/pads_bank0.h"
#include "hardware/regs/pio.h"
#include "hardware/regs/resets.h"

// --- SDK struct headers ------------------------------------------------------
#include "hardware/structs/iobank0.h"
#include "hardware/structs/pads_bank0.h"
#include "hardware/structs/pio.h"
#include "hardware/structs/resets.h"

// --- PIO-generated header (from ws2812_pio.pio via CMake) --------------------
#include "ws2812_pio.pio.h"

// =============================================================================
//  Configuration
// =============================================================================

bool ws2812_use_raw_pointers = true;

// =============================================================================
//  Internal state
// =============================================================================

static uint32_t pixel_buf[WS2812_NUM_LEDS];
static uint8_t global_brightness = 32;
static uint8_t global_speed = 5;

typedef enum {
  WS2812_EFFECT_NONE,
  WS2812_EFFECT_SOLID,
  WS2812_EFFECT_BREATHE,
  WS2812_EFFECT_STROBE,
  WS2812_EFFECT_GRADIENT
} ws2812_effect_mode_t;

#define WS2812_MAX_EFFECT_COLORS 5

typedef struct {
  ws2812_effect_mode_t mode;
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t brightness;
  uint16_t period_ms;
  uint16_t on_ms;
  uint16_t off_ms;
  uint8_t gradient[WS2812_MAX_EFFECT_COLORS][3];
  uint8_t gradient_count;
  uint32_t start_ms;
  uint32_t last_update_ms;
} ws2812_effect_state_t;

static ws2812_effect_state_t effect_state = {0};

// =============================================================================
//  PIO base address selection
// =============================================================================

#if WS2812_PIO_IDX == 0
#define WS2812_PIO_BASE PIO0_BASE
#define WS2812_PIO_HW pio0_hw
#define WS2812_PIO_RST RESETS_RESET_PIO0_BITS
#define WS2812_PIO_INST pio0
#elif WS2812_PIO_IDX == 1
#define WS2812_PIO_BASE PIO1_BASE
#define WS2812_PIO_HW pio1_hw
#define WS2812_PIO_RST RESETS_RESET_PIO1_BITS
#define WS2812_PIO_INST pio1
#else
#define WS2812_PIO_BASE PIO2_BASE
#define WS2812_PIO_HW pio2_hw
#define WS2812_PIO_RST RESETS_RESET_PIO2_BITS
#define WS2812_PIO_INST pio2
#endif

#define SM WS2812_SM

// =============================================================================
//  Raw pointer macros - direct register access via volatile casts
// =============================================================================

// --- PIO registers -----------------------------------------------------------
#define RAW_PIO_CTRL (*(volatile uint32_t *)(WS2812_PIO_BASE + PIO_CTRL_OFFSET))
#define RAW_PIO_FSTAT                                                          \
  (*(volatile uint32_t *)(WS2812_PIO_BASE + PIO_FSTAT_OFFSET))
#define RAW_PIO_FDEBUG                                                         \
  (*(volatile uint32_t *)(WS2812_PIO_BASE + PIO_FDEBUG_OFFSET))
#define RAW_PIO_FLEVEL                                                         \
  (*(volatile uint32_t *)(WS2812_PIO_BASE + PIO_FLEVEL_OFFSET))
#define RAW_PIO_TXF(sm)                                                        \
  (*(volatile uint32_t *)(WS2812_PIO_BASE + PIO_TXF0_OFFSET + (sm) * 4))
#define RAW_PIO_INSTR_MEM(n)                                                   \
  (*(volatile uint32_t *)(WS2812_PIO_BASE + PIO_INSTR_MEM0_OFFSET + (n) * 4))

// --- Per-SM registers --------------------------------------------------------
//     SM register block stride is 0x18 (6 registers x 4 bytes)
#define SM_REG_STRIDE 0x18
#define RAW_SM_CLKDIV(sm)                                                      \
  (*(volatile uint32_t *)(WS2812_PIO_BASE + PIO_SM0_CLKDIV_OFFSET +            \
                          (sm) * SM_REG_STRIDE))
#define RAW_SM_EXECCTRL(sm)                                                    \
  (*(volatile uint32_t *)(WS2812_PIO_BASE + PIO_SM0_EXECCTRL_OFFSET +          \
                          (sm) * SM_REG_STRIDE))
#define RAW_SM_SHIFTCTRL(sm)                                                   \
  (*(volatile uint32_t *)(WS2812_PIO_BASE + PIO_SM0_SHIFTCTRL_OFFSET +         \
                          (sm) * SM_REG_STRIDE))
#define RAW_SM_ADDR(sm)                                                        \
  (*(volatile uint32_t *)(WS2812_PIO_BASE + PIO_SM0_ADDR_OFFSET +              \
                          (sm) * SM_REG_STRIDE))
#define RAW_SM_INSTR(sm)                                                       \
  (*(volatile uint32_t *)(WS2812_PIO_BASE + PIO_SM0_INSTR_OFFSET +             \
                          (sm) * SM_REG_STRIDE))
#define RAW_SM_PINCTRL(sm)                                                     \
  (*(volatile uint32_t *)(WS2812_PIO_BASE + PIO_SM0_PINCTRL_OFFSET +           \
                          (sm) * SM_REG_STRIDE))

// --- Reset controller --------------------------------------------------------
#define RAW_RESETS_RESET                                                       \
  (*(volatile uint32_t *)(RESETS_BASE + RESETS_RESET_OFFSET))
#define RAW_RESETS_RESET_DONE                                                  \
  (*(volatile uint32_t *)(RESETS_BASE + RESETS_RESET_DONE_OFFSET))

// --- GPIO function select & pad config ---------------------------------------
#define RAW_IO_BANK0_GPIO_CTRL(pin)                                            \
  (*(volatile uint32_t *)(IO_BANK0_BASE + IO_BANK0_GPIO0_CTRL_OFFSET +         \
                          ((pin) * 8)))
#define RAW_PADS_BANK0_GPIO(pin)                                               \
  (*(volatile uint32_t *)(PADS_BANK0_BASE + PADS_BANK0_GPIO0_OFFSET +          \
                          ((pin) * 4)))

// --- SIO (software output enable / output) -----------------------------------
#define RAW_SIO_GPIO_OE_SET                                                    \
  (*(volatile uint32_t *)(SIO_BASE + SIO_GPIO_OE_SET_OFFSET))
#define RAW_SIO_GPIO_SET                                                       \
  (*(volatile uint32_t *)(SIO_BASE + SIO_GPIO_OUT_SET_OFFSET))
#define RAW_SIO_GPIO_CLR                                                       \
  (*(volatile uint32_t *)(SIO_BASE + SIO_GPIO_OUT_CLR_OFFSET))

// --- Pad configuration - clear ISO, enable input (needed for PIO) ------------
#define WS2812_PAD_CONFIG                                                      \
  (PADS_BANK0_GPIO0_IE_BITS |           /* input enable (for PIO) */           \
   (3u << PADS_BANK0_GPIO0_DRIVE_LSB) | /* 12 mA drive */                      \
   PADS_BANK0_GPIO0_SCHMITT_BITS)
// ISO (bit 8) left at 0 -> pad is de-isolated (RP2350 requirement)

// =============================================================================
//  PIO FUNCSEL value for GPIO
// =============================================================================
//
// On RP2350, GPIO function 6 = PIO0, function 7 = PIO1, function 8 = PIO2.
// (see RP2350 datasheet section 2.19.2 "GPIO Function Select")

#if WS2812_PIO_IDX == 0
#define PIO_GPIO_FUNCSEL 6
#elif WS2812_PIO_IDX == 1
#define PIO_GPIO_FUNCSEL 7
#else
#define PIO_GPIO_FUNCSEL 8
#endif

// =============================================================================
//  Initialisation
// =============================================================================

void ws2812_init(void) {
  uint32_t reset_mask = WS2812_PIO_RST | RESETS_RESET_IO_BANK0_BITS |
                        RESETS_RESET_PADS_BANK0_BITS;

  // -- WS2812 PIO timing constants --
  uint32_t cycles_per_bit = ws2812_T1 + ws2812_T2 + ws2812_T3; // 10
  float clkdiv = (float)clock_get_hz(clk_sys) /
                 ((float)WS2812_FREQ_HZ * (float)cycles_per_bit);

  // Convert to 16.8 fixed-point for the CLKDIV register
  uint16_t clkdiv_int = (uint16_t)clkdiv;
  uint8_t clkdiv_frac = (uint8_t)((clkdiv - clkdiv_int) * 256.0f);

  // Number of bits before autopull (24 for RGB, 32 for RGBW)
  uint32_t pull_thresh = WS2812_IS_RGBW ? 32 : 24;

  if (ws2812_use_raw_pointers) {
    // =================================================================
    //  RAW POINTER PATH - direct register manipulation
    // =================================================================

    // 1. Release peripheral resets (PIO, IO_BANK0, PADS_BANK0)
    RAW_RESETS_RESET &= ~reset_mask;
    while ((RAW_RESETS_RESET_DONE & reset_mask) != reset_mask)
      tight_loop_contents();

    // 2. Configure GPIO pad - clear ISO, enable IE, set drive strength
    RAW_PADS_BANK0_GPIO(WS2812_PIN) = WS2812_PAD_CONFIG;

    // 3. Set GPIO function to PIO
    RAW_IO_BANK0_GPIO_CTRL(WS2812_PIN) = PIO_GPIO_FUNCSEL;

    // 4. Set pin direction to output via PIO
    //    We'll also set it as SIO output as a safety measure
    RAW_SIO_GPIO_OE_SET = (1u << WS2812_PIN);
    RAW_SIO_GPIO_CLR = (1u << WS2812_PIN); // idle low

    // 5. Disable state machine before configuration
    RAW_PIO_CTRL &= ~(1u << SM); // clear SM_ENABLE bit

    // 6. Restart the state machine (clear stale state)
    RAW_PIO_CTRL |= (1u << (SM + 4)); // SM_RESTART bit

    // 7. Load PIO program into instruction memory at offset 0
    for (uint i = 0; i < ws2812_program.length; i++) {
      RAW_PIO_INSTR_MEM(i) = ws2812_program_instructions[i];
    }

    // 8. Configure clock divider
    RAW_SM_CLKDIV(SM) = ((uint32_t)clkdiv_int << PIO_SM0_CLKDIV_INT_LSB) |
                        ((uint32_t)clkdiv_frac << PIO_SM0_CLKDIV_FRAC_LSB);

    // 9. Configure EXECCTRL - set wrap top/bottom to program boundaries
    {
      uint32_t exec = RAW_SM_EXECCTRL(SM);
      // Clear wrap bits, set them to program wrap values
      exec &=
          ~(PIO_SM0_EXECCTRL_WRAP_TOP_BITS | PIO_SM0_EXECCTRL_WRAP_BOTTOM_BITS);
      exec |=
          ((uint32_t)ws2812_wrap_target << PIO_SM0_EXECCTRL_WRAP_BOTTOM_LSB);
      exec |= ((uint32_t)ws2812_wrap << PIO_SM0_EXECCTRL_WRAP_TOP_LSB);
      // side_set affects pin directions? No - side_set drives pin values
      exec &= ~PIO_SM0_EXECCTRL_SIDE_PINDIR_BITS;
      // side_set enable bit NOT used (all instructions have side-set)
      exec &= ~PIO_SM0_EXECCTRL_SIDE_EN_BITS;
      RAW_SM_EXECCTRL(SM) = exec;
    }

    // 10. Configure SHIFTCTRL - autopull, shift-left, join TX FIFO
    {
      uint32_t shift = 0;
      // Shift out to the LEFT (MSB first) -> OUT_SHIFTDIR = 0
      // Autopull enabled
      shift |= PIO_SM0_SHIFTCTRL_AUTOPULL_BITS;
      // Pull threshold
      shift |= ((pull_thresh & 0x1f) << PIO_SM0_SHIFTCTRL_PULL_THRESH_LSB);
      // Join TX FIFO (double depth: 8 entries instead of 4)
      shift |= PIO_SM0_SHIFTCTRL_FJOIN_TX_BITS;
      RAW_SM_SHIFTCTRL(SM) = shift;
    }

    // 11. Configure PINCTRL - side-set pin, side-set count = 1
    {
      uint32_t pinctrl = 0;
      // side-set base pin
      pinctrl |= ((uint32_t)WS2812_PIN << PIO_SM0_PINCTRL_SIDESET_BASE_LSB);
      // side-set count = 1
      pinctrl |= (1u << PIO_SM0_PINCTRL_SIDESET_COUNT_LSB);
      // SET and OUT aren't used by this program, leave at 0
      RAW_SM_PINCTRL(SM) = pinctrl;
    }

    // 12. Set initial pin direction to output (via PIO SET PINDIRS)
    //     Execute `set pindirs, 1` instruction:
    //     Opcode: 111_00000_100_00001 = 0xe081
    {
      // First set SET_BASE to our pin so SET PINDIRS affects the right pin
      uint32_t pinctrl_tmp = RAW_SM_PINCTRL(SM);
      uint32_t saved = pinctrl_tmp;
      pinctrl_tmp &= ~PIO_SM0_PINCTRL_SET_BASE_BITS;
      pinctrl_tmp |= ((uint32_t)WS2812_PIN << PIO_SM0_PINCTRL_SET_BASE_LSB);
      pinctrl_tmp &= ~PIO_SM0_PINCTRL_SET_COUNT_BITS;
      pinctrl_tmp |= (1u << PIO_SM0_PINCTRL_SET_COUNT_LSB);
      RAW_SM_PINCTRL(SM) = pinctrl_tmp;

      // Execute: set pindirs, 1  (opcode 0xe081)
      RAW_SM_INSTR(SM) = 0xe081;
      // Small delay for instruction to execute
      busy_wait_us(10);

      // Restore pinctrl
      RAW_SM_PINCTRL(SM) = saved;
    }

    // 13. Set program counter to program start (offset 0)
    //     Execute `jmp 0` instruction = 0x0000
    RAW_SM_INSTR(SM) = 0x0000;

    // 14. Enable the state machine
    RAW_PIO_CTRL |= (1u << SM);

  } else {
    // =================================================================
    //  SDK STRUCT PATH - using pio0_hw->, resets_hw->, etc.
    // =================================================================

    // 1. Release resets
    resets_hw->reset &= ~reset_mask;
    while ((resets_hw->reset_done & reset_mask) != reset_mask)
      tight_loop_contents();

    // 2. Pad config
    pads_bank0_hw->io[WS2812_PIN] = WS2812_PAD_CONFIG;

    // 3. GPIO function select -> PIO
    iobank0_hw->io[WS2812_PIN].ctrl = PIO_GPIO_FUNCSEL;

    // 4. Disable SM
    WS2812_PIO_HW->ctrl &= ~(1u << SM);

    // 5. Restart SM
    WS2812_PIO_HW->ctrl |= (1u << (SM + 4));

    // 6. Load program
    for (uint i = 0; i < ws2812_program.length; i++) {
      WS2812_PIO_HW->instr_mem[i] = ws2812_program_instructions[i];
    }

    // 7. Clock divider
    WS2812_PIO_HW->sm[SM].clkdiv =
        ((uint32_t)clkdiv_int << PIO_SM0_CLKDIV_INT_LSB) |
        ((uint32_t)clkdiv_frac << PIO_SM0_CLKDIV_FRAC_LSB);

    // 8. EXECCTRL - wrap
    {
      uint32_t exec = WS2812_PIO_HW->sm[SM].execctrl;
      exec &=
          ~(PIO_SM0_EXECCTRL_WRAP_TOP_BITS | PIO_SM0_EXECCTRL_WRAP_BOTTOM_BITS);
      exec |=
          ((uint32_t)ws2812_wrap_target << PIO_SM0_EXECCTRL_WRAP_BOTTOM_LSB);
      exec |= ((uint32_t)ws2812_wrap << PIO_SM0_EXECCTRL_WRAP_TOP_LSB);
      exec &= ~PIO_SM0_EXECCTRL_SIDE_PINDIR_BITS;
      exec &= ~PIO_SM0_EXECCTRL_SIDE_EN_BITS;
      WS2812_PIO_HW->sm[SM].execctrl = exec;
    }

    // 9. SHIFTCTRL
    {
      uint32_t shift = 0;
      shift |= PIO_SM0_SHIFTCTRL_AUTOPULL_BITS;
      shift |= ((pull_thresh & 0x1f) << PIO_SM0_SHIFTCTRL_PULL_THRESH_LSB);
      shift |= PIO_SM0_SHIFTCTRL_FJOIN_TX_BITS;
      WS2812_PIO_HW->sm[SM].shiftctrl = shift;
    }

    // 10. PINCTRL
    {
      uint32_t pinctrl = 0;
      pinctrl |= ((uint32_t)WS2812_PIN << PIO_SM0_PINCTRL_SIDESET_BASE_LSB);
      pinctrl |= (1u << PIO_SM0_PINCTRL_SIDESET_COUNT_LSB);
      WS2812_PIO_HW->sm[SM].pinctrl = pinctrl;
    }

    // 11. Set pin direction via PIO (set pindirs, 1)
    {
      uint32_t saved = WS2812_PIO_HW->sm[SM].pinctrl;
      uint32_t tmp = saved;
      tmp &= ~PIO_SM0_PINCTRL_SET_BASE_BITS;
      tmp |= ((uint32_t)WS2812_PIN << PIO_SM0_PINCTRL_SET_BASE_LSB);
      tmp &= ~PIO_SM0_PINCTRL_SET_COUNT_BITS;
      tmp |= (1u << PIO_SM0_PINCTRL_SET_COUNT_LSB);
      WS2812_PIO_HW->sm[SM].pinctrl = tmp;

      WS2812_PIO_HW->sm[SM].instr = 0xe081; // set pindirs, 1
      busy_wait_us(10);

      WS2812_PIO_HW->sm[SM].pinctrl = saved;
    }

    // 12. Jump to program start
    WS2812_PIO_HW->sm[SM].instr = 0x0000;

    // 13. Enable SM
    WS2812_PIO_HW->ctrl |= (1u << SM);
  }

  // Clear the pixel buffer
  ws2812_clear();

  /*printf("[ws2812] Init complete - PIO%d SM%d, GPIO %d, %.1f kHz, %s\n",
         WS2812_PIO_IDX, SM, WS2812_PIN, (float)WS2812_FREQ_HZ / 1000.0f,
         WS2812_IS_RGBW ? "RGBW" : "GRB");*/
}

// =============================================================================
//  Low-level pixel output
// =============================================================================

void ws2812_put_pixel(uint32_t pixel_grb) {
  // Wait until the TX FIFO is not full, then push the pixel data.
  // Data is left-justified: for 24-bit mode, shift left by 8.
  uint32_t data = WS2812_IS_RGBW ? pixel_grb : (pixel_grb << 8u);

  if (ws2812_use_raw_pointers) {
    // Poll FSTAT.TXFULL bit for our SM
    while (RAW_PIO_FSTAT & (1u << (PIO_FSTAT_TXFULL_LSB + SM)))
      tight_loop_contents();
    RAW_PIO_TXF(SM) = data;
  } else {
    while (WS2812_PIO_HW->fstat & (1u << (PIO_FSTAT_TXFULL_LSB + SM)))
      tight_loop_contents();
    WS2812_PIO_HW->txf[SM] = data;
  }
}

// =============================================================================
//  Pixel buffer control
// =============================================================================

void ws2812_set_pixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b) {
  if (index < WS2812_NUM_LEDS)
    pixel_buf[index] = ws2812_urgb(r, g, b);
}

void ws2812_set_all(uint8_t r, uint8_t g, uint8_t b) {
  uint32_t c = ws2812_urgb(r, g, b);
  for (uint16_t i = 0; i < WS2812_NUM_LEDS; i++)
    pixel_buf[i] = c;
}

void ws2812_clear(void) {
  for (uint16_t i = 0; i < WS2812_NUM_LEDS; i++)
    pixel_buf[i] = 0;
  ws2812_show();
}

void ws2812_show(void) {
  for (uint16_t i = 0; i < WS2812_NUM_LEDS; i++) {
    uint32_t c = pixel_buf[i];
    // Extract GRB components and apply brightness scaling
    uint8_t g = (c >> 16) & 0xFF;
    uint8_t r = (c >> 8) & 0xFF;
    uint8_t b = (c) & 0xFF;

    g = (uint8_t)(((uint16_t)g * global_brightness + 127) / 255);
    r = (uint8_t)(((uint16_t)r * global_brightness + 127) / 255);
    b = (uint8_t)(((uint16_t)b * global_brightness + 127) / 255);

    ws2812_put_pixel(((uint32_t)g << 16) | ((uint32_t)r << 8) | b);
  }

  // Wait for the PIO TX FIFO to be completely empty to ensure transmission is done
  if (ws2812_use_raw_pointers) {
    while (!(RAW_PIO_FSTAT & (1u << (PIO_FSTAT_TXEMPTY_LSB + SM))))
      tight_loop_contents();
  } else {
    while (!pio_sm_is_tx_fifo_empty(WS2812_PIO_INST, SM))
      tight_loop_contents();
  }

  // Latch: WS2812B requires >= 50 us of low to latch data.
  // We wait 300 us to be absolutely sure the last bit has fully finished shifting
  // out of the OSR and a robust reset pulse has been completed on the physical wire.
  busy_wait_us(300);
}

// =============================================================================
//  Brightness
// =============================================================================

void ws2812_set_brightness(uint8_t brightness) {
  global_brightness = brightness;
}

uint8_t ws2812_get_brightness(void) { return global_brightness; }

void ws2812_set_speed(uint8_t speed) {
  if (speed < 1) speed = 1;
  if (speed > 10) speed = 10;
  global_speed = speed;
}

uint8_t ws2812_get_speed(void) { return global_speed; }

static void show_scaled_rgb(uint8_t r, uint8_t g, uint8_t b,
                            uint8_t scale_percent) {
  uint8_t r_scaled = (uint8_t)(((uint16_t)r * scale_percent) / 100);
  uint8_t g_scaled = (uint8_t)(((uint16_t)g * scale_percent) / 100);
  uint8_t b_scaled = (uint8_t)(((uint16_t)b * scale_percent) / 100);
  ws2812_set_all(r_scaled, g_scaled, b_scaled);
  ws2812_show();
}

void ws2812_effect_stop(void) { effect_state.mode = WS2812_EFFECT_NONE; }

void ws2812_effect_solid(uint8_t r, uint8_t g, uint8_t b,
                         uint8_t brightness) {
  effect_state.mode = WS2812_EFFECT_SOLID;
  effect_state.r = r;
  effect_state.g = g;
  effect_state.b = b;
  effect_state.brightness = brightness;
  show_scaled_rgb(r, g, b, brightness);
}

void ws2812_effect_breathe(uint8_t r, uint8_t g, uint8_t b,
                           uint8_t brightness, uint16_t period_ms) {
  if (period_ms < 200)
    period_ms = 200;
  effect_state.mode = WS2812_EFFECT_BREATHE;
  effect_state.r = r;
  effect_state.g = g;
  effect_state.b = b;
  effect_state.brightness = brightness;
  effect_state.period_ms = period_ms;
  effect_state.start_ms = to_ms_since_boot(get_absolute_time());
  effect_state.last_update_ms = 0;
}

void ws2812_effect_strobe(uint8_t r, uint8_t g, uint8_t b,
                          uint8_t brightness, uint16_t on_ms,
                          uint16_t off_ms) {
  if (on_ms < 20)
    on_ms = 20;
  if (off_ms < 20)
    off_ms = 20;
  effect_state.mode = WS2812_EFFECT_STROBE;
  effect_state.r = r;
  effect_state.g = g;
  effect_state.b = b;
  effect_state.brightness = brightness;
  effect_state.on_ms = on_ms;
  effect_state.off_ms = off_ms;
  effect_state.start_ms = to_ms_since_boot(get_absolute_time());
  effect_state.last_update_ms = 0;
}

bool ws2812_effect_gradient(const uint8_t *colors_rgb, uint8_t color_count,
                            uint8_t brightness, uint16_t period_ms) {
  if (!colors_rgb || color_count < 2 || color_count > WS2812_MAX_EFFECT_COLORS)
    return false;
  if (period_ms < 200)
    period_ms = 200;

  effect_state.mode = WS2812_EFFECT_GRADIENT;
  effect_state.gradient_count = color_count;
  effect_state.brightness = brightness;
  effect_state.period_ms = period_ms;
  effect_state.start_ms = to_ms_since_boot(get_absolute_time());
  effect_state.last_update_ms = 0;

  for (uint8_t i = 0; i < color_count; i++) {
    effect_state.gradient[i][0] = colors_rgb[i * 3 + 0];
    effect_state.gradient[i][1] = colors_rgb[i * 3 + 1];
    effect_state.gradient[i][2] = colors_rgb[i * 3 + 2];
  }
  return true;
}

void ws2812_effect_update(uint32_t now_ms) {
  if (effect_state.mode == WS2812_EFFECT_NONE ||
      effect_state.mode == WS2812_EFFECT_SOLID)
    return;

  if (now_ms - effect_state.last_update_ms < 20)
    return;
  effect_state.last_update_ms = now_ms;

  switch (effect_state.mode) {
  case WS2812_EFFECT_BREATHE: {
    uint32_t period = 4000 - ((uint32_t)global_speed * 350);
    uint32_t phase = (now_ms - effect_state.start_ms) % period;
    uint32_t half = period / 2;
    uint32_t level = (phase < half) ? phase : (period - phase);
    uint8_t bright = (uint8_t)(((uint32_t)effect_state.brightness * level) /
                               (half ? half : 1));
    show_scaled_rgb(effect_state.r, effect_state.g, effect_state.b, bright);
    break;
  }
  case WS2812_EFFECT_STROBE: {
    uint32_t period = 1000 - ((uint32_t)global_speed * 90);
    uint32_t on_ms = period / 4;
    if (on_ms < 20) on_ms = 20;
    uint32_t phase = (now_ms - effect_state.start_ms) % period;
    if (phase < on_ms)
      show_scaled_rgb(effect_state.r, effect_state.g, effect_state.b,
                      effect_state.brightness);
    else
      show_scaled_rgb(0, 0, 0, 0);
    break;
  }
  case WS2812_EFFECT_GRADIENT: {
    uint8_t count = effect_state.gradient_count;
    uint32_t period = 6000 - ((uint32_t)global_speed * 500);
    uint32_t phase = (now_ms - effect_state.start_ms) % period;
    uint32_t scaled = phase * count;
    uint8_t idx = scaled / period;
    uint8_t next = (idx + 1) % count;
    uint16_t frac =
        (uint16_t)(((scaled % period) * 255) /
                   period);
    uint8_t r0 = effect_state.gradient[idx][0];
    uint8_t g0 = effect_state.gradient[idx][1];
    uint8_t b0 = effect_state.gradient[idx][2];
    uint8_t r1 = effect_state.gradient[next][0];
    uint8_t g1 = effect_state.gradient[next][1];
    uint8_t b1 = effect_state.gradient[next][2];
    uint8_t r = (uint8_t)(((uint16_t)r0 * (255 - frac) +
                           (uint16_t)r1 * frac) /
                          255);
    uint8_t g = (uint8_t)(((uint16_t)g0 * (255 - frac) +
                           (uint16_t)g1 * frac) /
                          255);
    uint8_t b = (uint8_t)(((uint16_t)b0 * (255 - frac) +
                           (uint16_t)b1 * frac) /
                          255);
    show_scaled_rgb(r, g, b, effect_state.brightness);
    break;
  }
  default:
    break;
  }
}

// =============================================================================
//  Color helpers
// =============================================================================

uint32_t ws2812_wheel(uint16_t hue) {
  // hue 0-360 -> RGB via 60 degree sectors
  hue = hue % 360;
  uint8_t sector = hue / 60;
  uint8_t frac = (uint8_t)(((uint32_t)(hue % 60) * 255) / 60);

  uint8_t r, g, b;
  switch (sector) {
  case 0:
    r = 255;
    g = frac;
    b = 0;
    break;
  case 1:
    r = 255 - frac;
    g = 255;
    b = 0;
    break;
  case 2:
    r = 0;
    g = 255;
    b = frac;
    break;
  case 3:
    r = 0;
    g = 255 - frac;
    b = 255;
    break;
  case 4:
    r = frac;
    g = 0;
    b = 255;
    break;
  default:
    r = 255;
    g = 0;
    b = 255 - frac;
    break;
  }
  return ws2812_urgb(r, g, b);
}

// =============================================================================
//  Effects
// =============================================================================

void ws2812_rainbow_cycle(uint16_t delay_ms) {
  for (uint16_t hue = 0; hue < 360; hue++) {
    for (uint16_t i = 0; i < WS2812_NUM_LEDS; i++) {
      uint16_t pixel_hue = (hue + (i * 360 / WS2812_NUM_LEDS)) % 360;
      pixel_buf[i] = ws2812_wheel(pixel_hue);
    }
    ws2812_show();
    sleep_ms(delay_ms);
  }
}

void ws2812_breathe(uint8_t r, uint8_t g, uint8_t b, uint16_t period_ms) {
  // One full breath cycle: ramp brightness 0->255->0 using a cosine curve
  uint16_t steps = 100;
  uint16_t step_ms = period_ms / steps;
  if (step_ms < 1)
    step_ms = 1;

  uint8_t saved_brightness = global_brightness;

  for (uint16_t s = 0; s < steps; s++) {
    // Cosine: brightness = (1 - cos(2*pi*s/steps)) / 2 * max_brightness
    float phase = (float)s / (float)steps;
    float val = (1.0f - cosf(2.0f * 3.14159f * phase)) / 2.0f;
    uint8_t bright = (uint8_t)(val * (float)saved_brightness);

    global_brightness = bright;
    ws2812_set_all(r, g, b);
    ws2812_show();
    sleep_ms(step_ms);
  }

  global_brightness = saved_brightness;
}

void ws2812_color_wipe(uint8_t r, uint8_t g, uint8_t b, uint16_t delay_ms) {
  for (uint16_t i = 0; i < WS2812_NUM_LEDS; i++) {
    ws2812_set_pixel(i, r, g, b);
    ws2812_show();
    sleep_ms(delay_ms);
  }
}

void ws2812_blink(uint8_t r, uint8_t g, uint8_t b, uint16_t on_ms,
                  uint16_t off_ms) {
  ws2812_set_all(r, g, b);
  ws2812_show();
  sleep_ms(on_ms);

  ws2812_set_all(0, 0, 0);
  ws2812_show();
  sleep_ms(off_ms);
}

void ws2812_sparkle(uint8_t r, uint8_t g, uint8_t b, uint16_t delay_ms,
                    uint16_t duration_ms) {
  uint32_t elapsed = 0;
  while (elapsed < duration_ms) {
    // All off
    for (uint16_t i = 0; i < WS2812_NUM_LEDS; i++)
      pixel_buf[i] = 0;

    // Light a random pixel
    uint16_t idx = rand() % WS2812_NUM_LEDS;
    pixel_buf[idx] = ws2812_urgb(r, g, b);

    ws2812_show();
    sleep_ms(delay_ms);
    elapsed += delay_ms;
  }
}

// =============================================================================
//  Register dump (debug)
// =============================================================================

void ws2812_dump_pio_regs(void) {
  printf("\n+----------------------------------------------------------+\n");
  printf("|  PIO%d Register Dump (SM%d)                               |\n",
         WS2812_PIO_IDX, SM);
  printf("+----------------------------------------------------------+\n");

  if (ws2812_use_raw_pointers) {
    printf("|  CTRL       = 0x%08" PRIX32 "                              |\n",
           RAW_PIO_CTRL);
    printf("|  FSTAT      = 0x%08" PRIX32 "                              |\n",
           RAW_PIO_FSTAT);
    printf("|  FDEBUG     = 0x%08" PRIX32 "                              |\n",
           RAW_PIO_FDEBUG);
    printf("|  FLEVEL     = 0x%08" PRIX32 "                              |\n",
           RAW_PIO_FLEVEL);
    printf("+----------------------------------------------------------+\n");
    printf("|  SM%d_CLKDIV   = 0x%08" PRIX32 "                            |\n",
           SM, RAW_SM_CLKDIV(SM));
    printf("|  SM%d_EXECCTRL = 0x%08" PRIX32 "                            |\n",
           SM, RAW_SM_EXECCTRL(SM));
    printf("|  SM%d_SHIFTCTRL= 0x%08" PRIX32 "                            |\n",
           SM, RAW_SM_SHIFTCTRL(SM));
    printf("|  SM%d_PINCTRL  = 0x%08" PRIX32 "                            |\n",
           SM, RAW_SM_PINCTRL(SM));
    printf("|  SM%d_ADDR     = 0x%08" PRIX32 "                            |\n",
           SM, RAW_SM_ADDR(SM));
  } else {
    printf("|  CTRL       = 0x%08" PRIX32 "                              |\n",
           WS2812_PIO_HW->ctrl);
    printf("|  FSTAT      = 0x%08" PRIX32 "                              |\n",
           WS2812_PIO_HW->fstat);
    printf("|  FDEBUG     = 0x%08" PRIX32 "                              |\n",
           WS2812_PIO_HW->fdebug);
    printf("|  FLEVEL     = 0x%08" PRIX32 "                              |\n",
           WS2812_PIO_HW->flevel);
    printf("+----------------------------------------------------------+\n");
    printf("|  SM%d_CLKDIV   = 0x%08" PRIX32 "                            |\n",
           SM, WS2812_PIO_HW->sm[SM].clkdiv);
    printf("|  SM%d_EXECCTRL = 0x%08" PRIX32 "                            |\n",
           SM, WS2812_PIO_HW->sm[SM].execctrl);
    printf("|  SM%d_SHIFTCTRL= 0x%08" PRIX32 "                            |\n",
           SM, WS2812_PIO_HW->sm[SM].shiftctrl);
    printf("|  SM%d_PINCTRL  = 0x%08" PRIX32 "                            |\n",
           SM, WS2812_PIO_HW->sm[SM].pinctrl);
    printf("|  SM%d_ADDR     = 0x%08" PRIX32 "                            |\n",
           SM, WS2812_PIO_HW->sm[SM].addr);
  }

  printf("+----------------------------------------------------------+\n");
  printf("|  Instruction Memory [0..3]:                              |\n");
  for (int i = 0; i < 4; i++) {
    printf("|    INSTR_MEM[%d] = 0x%04X                               |\n", i,
           ws2812_program_instructions[i]);
  }
  printf("+----------------------------------------------------------+\n\n");
}

// =============================================================================
//  Single LED Convenience / Dynamic Brightness Control
// =============================================================================

void ws2812_set(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness) {
  if (brightness > 100)
    brightness = 100;
  ws2812_set_all(r, g, b);
  ws2812_set_brightness((uint16_t)brightness * 255 / 100);
}

void ws2812_set_hsv(uint16_t h, uint8_t s, uint8_t v, uint8_t brightness) {
  if (brightness > 100)
    brightness = 100;

  // HSV to RGB conversion (integer-only)
  uint8_t r, g, b;
  if (s == 0) {
    r = g = b = v;
  } else {
    uint8_t region = h / 60;
    uint8_t remainder = (h - (region * 60)) * 255 / 60;

    uint8_t p = (uint8_t)((uint16_t)v * (255 - s) / 255);
    uint8_t q =
        (uint8_t)((uint16_t)v * (255 - ((uint16_t)s * remainder / 255)) / 255);
    uint8_t t =
        (uint8_t)((uint16_t)v *
                  (255 - ((uint16_t)s * (255 - remainder) / 255)) / 255);

    switch (region) {
    case 0:
      r = v;
      g = t;
      b = p;
      break;
    case 1:
      r = q;
      g = v;
      b = p;
      break;
    case 2:
      r = p;
      g = v;
      b = t;
      break;
    case 3:
      r = p;
      g = q;
      b = v;
      break;
    case 4:
      r = t;
      g = p;
      b = v;
      break;
    default:
      r = v;
      g = p;
      b = q;
      break;
    }
  }
  ws2812_set_all(r, g, b);
  ws2812_set_brightness((uint16_t)brightness * 255 / 100);
}

void ws2812_color_demo(void) {
  uint8_t saved_brightness = ws2812_get_brightness();
  for (uint16_t hue = 0; hue < 360; hue++) {
    ws2812_set_hsv(hue, 255, 255, 15);
    ws2812_show();
    sleep_ms(10);
  }
  ws2812_set_brightness(saved_brightness);
  ws2812_clear();
}
