#include "ld2410.h"
#include "hardware/uart.h"
#include "pico/stdlib.h"
#include <string.h>

// ─── Frame Constants ────────────────────────────────────────────────────────
// Data frame header: F4 F3 F2 F1
// Data frame footer: F8 F7 F6 F5
static const uint8_t DATA_HEADER[] = {0xF4, 0xF3, 0xF2, 0xF1};
static const uint8_t DATA_FOOTER[] = {0xF8, 0xF7, 0xF6, 0xF5};

#define MAX_FRAME_LEN 64

// ─── Parser State Machine ───────────────────────────────────────────────────
typedef enum {
  PARSE_HEADER,  // Looking for F4 F3 F2 F1
  PARSE_LENGTH,  // Reading 2-byte length
  PARSE_PAYLOAD, // Reading payload bytes
  PARSE_FOOTER,  // Verifying F8 F7 F6 F5
} parse_state_t;

static parse_state_t parse_state = PARSE_HEADER;
static uint8_t header_idx = 0;
static uint8_t frame_buf[MAX_FRAME_LEN];
static uint16_t frame_len = 0; // Expected payload length
static uint16_t frame_pos = 0; // Current position in frame_buf
static uint8_t footer_idx = 0;
static uint8_t length_buf[2];
static uint8_t length_idx = 0;

static radar_data_t current_data = {0};

// ─── Init ───────────────────────────────────────────────────────────────────

void radar_init(void) {
  uart_init(RADAR_UART_ID, RADAR_BAUD_RATE);
  gpio_set_function(RADAR_TX_PIN, GPIO_FUNC_UART);
  gpio_set_function(RADAR_RX_PIN, GPIO_FUNC_UART);

  // 8N1 format
  uart_set_format(RADAR_UART_ID, 8, 1, UART_PARITY_NONE);
  uart_set_fifo_enabled(RADAR_UART_ID, true);

  // Reset parser
  parse_state = PARSE_HEADER;
  header_idx = 0;
  memset(&current_data, 0, sizeof(current_data));
}

// ─── Frame Parser ───────────────────────────────────────────────────────────

static void parse_frame(const uint8_t *payload, uint16_t len) {
  // Minimum data frame payload:
  //   byte 0: data type (0x02 = target data, 0x01 = engineering)
  //   byte 1: head (0xAA)
  //   byte 2: target state
  //   bytes 3-4: moving distance (LE)
  //   byte 5: moving energy
  //   bytes 6-7: stationary distance (LE)
  //   byte 8: stationary energy
  //   bytes 9-10: detection distance (LE)
  if (len < 11)
    return;

  // Check for standard target data frame
  if (payload[0] != 0x02 || payload[1] != 0xAA)
    return;

  current_data.target_state = (radar_target_state_t)payload[2];
  current_data.moving_distance_cm = payload[3] | (payload[4] << 8);
  current_data.moving_energy = payload[5];
  current_data.stationary_distance_cm = payload[6] | (payload[7] << 8);
  current_data.stationary_energy = payload[8];
  current_data.detection_distance_cm = payload[9] | (payload[10] << 8);
  current_data.data_valid = true;
  current_data.last_update_ms = to_ms_since_boot(get_absolute_time());
}

static void process_byte(uint8_t byte) {
  switch (parse_state) {
  case PARSE_HEADER:
    if (byte == DATA_HEADER[header_idx]) {
      header_idx++;
      if (header_idx >= 4) {
        parse_state = PARSE_LENGTH;
        length_idx = 0;
        header_idx = 0;
      }
    } else {
      header_idx = (byte == DATA_HEADER[0]) ? 1 : 0;
    }
    break;

  case PARSE_LENGTH:
    length_buf[length_idx++] = byte;
    if (length_idx >= 2) {
      frame_len = length_buf[0] | (length_buf[1] << 8);
      if (frame_len > MAX_FRAME_LEN) {
        // Invalid length, reset
        parse_state = PARSE_HEADER;
        header_idx = 0;
      } else {
        frame_pos = 0;
        parse_state = PARSE_PAYLOAD;
      }
    }
    break;

  case PARSE_PAYLOAD:
    frame_buf[frame_pos++] = byte;
    if (frame_pos >= frame_len) {
      parse_state = PARSE_FOOTER;
      footer_idx = 0;
    }
    break;

  case PARSE_FOOTER:
    if (byte == DATA_FOOTER[footer_idx]) {
      footer_idx++;
      if (footer_idx >= 4) {
        // Complete valid frame!
        parse_frame(frame_buf, frame_len);
        parse_state = PARSE_HEADER;
        header_idx = 0;
      }
    } else {
      // Bad footer, discard and resync
      parse_state = PARSE_HEADER;
      header_idx = (byte == DATA_HEADER[0]) ? 1 : 0;
    }
    break;
  }
}

// ─── Public API ─────────────────────────────────────────────────────────────

void radar_update(void) {
  while (uart_is_readable(RADAR_UART_ID)) {
    uint8_t byte = uart_getc(RADAR_UART_ID);
    process_byte(byte);
  }
}

radar_data_t radar_get_data(void) { return current_data; }

const char *radar_state_str(radar_target_state_t state) {
  switch (state) {
  case RADAR_TARGET_NONE:
    return "No Target";
  case RADAR_TARGET_MOVING:
    return "Moving";
  case RADAR_TARGET_STATIONARY:
    return "Stationary";
  case RADAR_TARGET_BOTH:
    return "Mov+Stat";
  default:
    return "Unknown";
  }
}
