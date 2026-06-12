/**
 * @file  gps.c
 * @brief u-blox NEO-6M NMEA parser — UART0 driver and sentence parser.
 *
 * Parses $GPGGA and $GPRMC sentences.  Non-blocking: call gps_task()
 * from the main loop to process incoming data.
 *
 * @author  apmiller
 */

#include "gps.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hardware/uart.h"
#include "pico/stdlib.h"

// =============================================================================
//  Internal state
// =============================================================================

static gps_data_t gps_data = {0};

// NMEA sentence line buffer
static char sentence_buf[GPS_SENTENCE_MAX_LEN];
static uint8_t sentence_pos = 0;

// Track recording circular buffer
static gps_track_point_t track_buf[GPS_TRACK_BUFFER_SIZE];
static uint16_t track_write_idx = 0;
static uint16_t track_count = 0;
static uint32_t track_last_record_ms = 0;
#define TRACK_RECORD_INTERVAL_MS 2000 // record a point every 2 seconds

// =============================================================================
//  NMEA field parsing helpers
// =============================================================================

/**
 * @brief Get the Nth comma-separated field from an NMEA sentence.
 *
 * @param sentence  Full NMEA sentence string (e.g., "$GPGGA,...")
 * @param field_num Field index (0-based, 0 = the sentence ID like "GPGGA")
 * @param out       Output buffer for the field value
 * @param out_len   Size of the output buffer
 * @return          true if the field was found, false otherwise
 */
static bool nmea_get_field(const char *sentence, uint8_t field_num, char *out,
                           uint8_t out_len) {
  uint8_t current_field = 0;
  const char *p = sentence;

  // Skip the '$' if present
  if (*p == '$')
    p++;

  // Find the start of the requested field
  while (current_field < field_num && *p) {
    if (*p == ',')
      current_field++;
    p++;
  }

  if (current_field != field_num || !*p)
    return false;

  // Copy until next comma, asterisk, or end
  uint8_t i = 0;
  while (*p && *p != ',' && *p != '*' && i < out_len - 1) {
    out[i++] = *p++;
  }
  out[i] = '\0';
  return i > 0;
}

/**
 * @brief Convert NMEA coordinate format (ddmm.mmmm) to decimal degrees.
 *
 * @param raw       Raw NMEA coordinate string (e.g., "4807.038")
 * @param direction Direction character: 'N','S','E','W'
 * @param deg_width Number of digits in the degree part (2 for lat, 3 for lon)
 * @return          Decimal degrees (negative for S/W)
 */
static double nmea_to_decimal_degrees(const char *raw, char direction,
                                      uint8_t deg_width) {
  if (!raw || strlen(raw) < (size_t)(deg_width + 1))
    return 0.0;

  // Extract degree part
  char deg_str[4] = {0};
  strncpy(deg_str, raw, deg_width);
  double degrees = atof(deg_str);

  // Extract minutes part
  double minutes = atof(raw + deg_width);

  double result = degrees + minutes / 60.0;

  if (direction == 'S' || direction == 'W')
    result = -result;

  return result;
}

/**
 * @brief Validate NMEA checksum.
 *
 * The checksum is the XOR of all characters between '$' and '*'.
 *
 * @param sentence  Complete NMEA sentence including '$' and '*XX'
 * @return          true if checksum is valid
 */
static bool nmea_validate_checksum(const char *sentence) {
  if (!sentence || sentence[0] != '$')
    return false;

  uint8_t computed = 0;
  const char *p = sentence + 1; // skip '$'

  while (*p && *p != '*') {
    computed ^= (uint8_t)*p;
    p++;
  }

  if (*p != '*')
    return false;

  // Parse the two-character hex checksum after '*'
  uint8_t expected = (uint8_t)strtol(p + 1, NULL, 16);
  return computed == expected;
}

// =============================================================================
//  Sentence parsers
// =============================================================================

/**
 * @brief Parse $GPGGA sentence (Global Positioning System Fix Data).
 *
 * Format: $GPGGA,hhmmss.ss,llll.ll,a,yyyyy.yy,a,x,xx,x.x,x.x,M,x.x,M,x.x,xxxx*hh
 *
 * Fields:
 *   1: UTC time (hhmmss.ss)
 *   2: Latitude
 *   3: N/S
 *   4: Longitude
 *   5: E/W
 *   6: Fix quality (0=invalid, 1=GPS, 2=DGPS)
 *   7: Satellites in use
 *   8: HDOP
 *   9: Altitude (MSL)
 *  10: Altitude unit (M)
 */
static void parse_gpgga(const char *sentence) {
  char field[20];

  // Field 1: UTC time
  if (nmea_get_field(sentence, 1, field, sizeof(field)) && strlen(field) >= 6) {
    gps_data.hour = (uint8_t)((field[0] - '0') * 10 + (field[1] - '0'));
    gps_data.minute = (uint8_t)((field[2] - '0') * 10 + (field[3] - '0'));
    gps_data.second = (uint8_t)((field[4] - '0') * 10 + (field[5] - '0'));
  }

  // Field 6: Fix quality
  if (nmea_get_field(sentence, 6, field, sizeof(field))) {
    gps_data.fix_quality = (uint8_t)atoi(field);
  }

  // Fields 2-5: Position (only if we have a fix)
  if (gps_data.fix_quality > 0) {
    char lat_str[16], lat_dir[4], lon_str[16], lon_dir[4];

    if (nmea_get_field(sentence, 2, lat_str, sizeof(lat_str)) &&
        nmea_get_field(sentence, 3, lat_dir, sizeof(lat_dir))) {
      gps_data.latitude = nmea_to_decimal_degrees(lat_str, lat_dir[0], 2);
    }

    if (nmea_get_field(sentence, 4, lon_str, sizeof(lon_str)) &&
        nmea_get_field(sentence, 5, lon_dir, sizeof(lon_dir))) {
      gps_data.longitude = nmea_to_decimal_degrees(lon_str, lon_dir[0], 3);
    }
  }

  // Field 7: Satellites in use
  if (nmea_get_field(sentence, 7, field, sizeof(field))) {
    gps_data.satellites = (uint8_t)atoi(field);
  }

  // Field 8: HDOP
  if (nmea_get_field(sentence, 8, field, sizeof(field))) {
    gps_data.hdop = (float)atof(field);
  }

  // Field 9: Altitude
  if (nmea_get_field(sentence, 9, field, sizeof(field))) {
    gps_data.altitude_m = (float)atof(field);
  }
}

/**
 * @brief Parse $GPRMC sentence (Recommended Minimum Navigation Information).
 *
 * Format: $GPRMC,hhmmss.ss,A,llll.ll,a,yyyyy.yy,a,x.x,x.x,ddmmyy,x.x,a*hh
 *
 * Fields:
 *   1: UTC time
 *   2: Status (A=active/valid, V=void/invalid)
 *   3: Latitude
 *   4: N/S
 *   5: Longitude
 *   6: E/W
 *   7: Speed over ground (knots)
 *   8: Course over ground (degrees)
 *   9: Date (ddmmyy)
 */
static void parse_gprmc(const char *sentence) {
  char field[20];

  // Field 2: Status
  if (nmea_get_field(sentence, 2, field, sizeof(field))) {
    gps_data.valid = (field[0] == 'A');
  }

  if (!gps_data.valid)
    return;

  // Field 1: UTC time
  if (nmea_get_field(sentence, 1, field, sizeof(field)) && strlen(field) >= 6) {
    gps_data.hour = (uint8_t)((field[0] - '0') * 10 + (field[1] - '0'));
    gps_data.minute = (uint8_t)((field[2] - '0') * 10 + (field[3] - '0'));
    gps_data.second = (uint8_t)((field[4] - '0') * 10 + (field[5] - '0'));
  }

  // Fields 3-6: Position
  {
    char lat_str[16], lat_dir[4], lon_str[16], lon_dir[4];

    if (nmea_get_field(sentence, 3, lat_str, sizeof(lat_str)) &&
        nmea_get_field(sentence, 4, lat_dir, sizeof(lat_dir))) {
      gps_data.latitude = nmea_to_decimal_degrees(lat_str, lat_dir[0], 2);
    }

    if (nmea_get_field(sentence, 5, lon_str, sizeof(lon_str)) &&
        nmea_get_field(sentence, 6, lon_dir, sizeof(lon_dir))) {
      gps_data.longitude = nmea_to_decimal_degrees(lon_str, lon_dir[0], 3);
    }
  }

  // Field 7: Speed (knots)
  if (nmea_get_field(sentence, 7, field, sizeof(field))) {
    gps_data.speed_knots = (float)atof(field);
    gps_data.speed_kmh = gps_data.speed_knots * 1.852f;
  }

  // Field 8: Course
  if (nmea_get_field(sentence, 8, field, sizeof(field)) && strlen(field) > 0) {
    gps_data.course_deg = (float)atof(field);
  }

  // Field 9: Date
  if (nmea_get_field(sentence, 9, field, sizeof(field)) && strlen(field) >= 6) {
    gps_data.day = (uint8_t)((field[0] - '0') * 10 + (field[1] - '0'));
    gps_data.month = (uint8_t)((field[2] - '0') * 10 + (field[3] - '0'));
    gps_data.year =
        (uint16_t)(2000 + (field[4] - '0') * 10 + (field[5] - '0'));
  }

  // Update fix timestamp
  gps_data.last_fix_ms = to_ms_since_boot(get_absolute_time());
}

// =============================================================================
//  Process a complete NMEA sentence
// =============================================================================

static void process_sentence(const char *sentence) {
  if (!nmea_validate_checksum(sentence))
    return;

  gps_data.sentence_count++;

  // Identify sentence type (field 0, after the '$')
  if (strncmp(sentence + 1, "GPGGA", 5) == 0 ||
      strncmp(sentence + 1, "GNGGA", 5) == 0) {
    parse_gpgga(sentence);
  } else if (strncmp(sentence + 1, "GPRMC", 5) == 0 ||
             strncmp(sentence + 1, "GNRMC", 5) == 0) {
    parse_gprmc(sentence);
  }
  // Other sentence types (GSV, GSA, etc.) are silently ignored for now

  // Record track point if we have a valid fix
  if (gps_data.valid) {
    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (now - track_last_record_ms >= TRACK_RECORD_INTERVAL_MS) {
      track_last_record_ms = now;

      track_buf[track_write_idx].lat = (float)gps_data.latitude;
      track_buf[track_write_idx].lon = (float)gps_data.longitude;
      track_buf[track_write_idx].speed_kmh = gps_data.speed_kmh;
      track_buf[track_write_idx].timestamp_ms = now;

      track_write_idx = (track_write_idx + 1) % GPS_TRACK_BUFFER_SIZE;
      if (track_count < GPS_TRACK_BUFFER_SIZE)
        track_count++;
    }
  }
}

// =============================================================================
//  Public API
// =============================================================================

void gps_init(void) {
  // Initialise UART0 at 9600 baud
  uart_init(GPS_UART_ID, GPS_UART_BAUD);

  // Set pin functions
  gpio_set_function(GPS_TX_PIN, GPIO_FUNC_UART);
  gpio_set_function(GPS_RX_PIN, GPIO_FUNC_UART);

  // Configure UART: 8 data bits, 1 stop bit, no parity
  uart_set_format(GPS_UART_ID, 8, 1, UART_PARITY_NONE);

  // Disable hardware flow control
  uart_set_hw_flow(GPS_UART_ID, false, false);

  // Enable FIFO
  uart_set_fifo_enabled(GPS_UART_ID, true);

  // Clear any stale data
  while (uart_is_readable(GPS_UART_ID)) {
    uart_getc(GPS_UART_ID);
  }

  memset(&gps_data, 0, sizeof(gps_data));
  sentence_pos = 0;

  printf("[gps] UART%d init: %d baud on GP%d(TX)/GP%d(RX)\n",
         uart_get_index(GPS_UART_ID), GPS_UART_BAUD, GPS_TX_PIN, GPS_RX_PIN);
}

void gps_task(void) {
  // Process all available UART bytes (non-blocking)
  while (uart_is_readable(GPS_UART_ID)) {
    char c = uart_getc(GPS_UART_ID);

    // DEBUG: Echo raw character to USB serial so we can see if it is communicating
    if ((c >= 32 && c <= 126) || c == '\r' || c == '\n') {
      putchar(c);
    } else {
      printf("[0x%02X]", (uint8_t)c);
    }

    if (c == '$') {
      // Start of a new NMEA sentence
      sentence_pos = 0;
      sentence_buf[sentence_pos++] = c;
    } else if (c == '\r' || c == '\n') {
      // End of sentence — process if we have content
      if (sentence_pos > 6) { // minimum valid: $XXXXX*HH
        sentence_buf[sentence_pos] = '\0';
        process_sentence(sentence_buf);
      }
      sentence_pos = 0;
    } else if (sentence_pos > 0 && sentence_pos < GPS_SENTENCE_MAX_LEN - 1) {
      sentence_buf[sentence_pos++] = c;
    } else if (sentence_pos >= GPS_SENTENCE_MAX_LEN - 1) {
      // Sentence too long — discard
      sentence_pos = 0;
    }
  }
}

const gps_data_t *gps_get_data(void) { return &gps_data; }

bool gps_has_fix(void) { return gps_data.valid && gps_data.fix_quality > 0; }

const gps_track_point_t *gps_get_track(uint16_t *count) {
  if (count)
    *count = track_count;
  return track_buf;
}

uint16_t gps_get_track_start_index(void) {
  if (track_count < GPS_TRACK_BUFFER_SIZE)
    return 0;
  return track_write_idx; // oldest entry in the circular buffer
}

void gps_print_status(void) {
  const gps_data_t *d = &gps_data;
  printf("\n--- GPS Status ---\n");
  printf("Fix: %s (quality=%d, sats=%d, HDOP=%.1f)\n",
         d->valid ? "VALID" : "NO FIX", d->fix_quality, d->satellites,
         (double)d->hdop);

  if (d->valid) {
    printf("Position: %.6f, %.6f  Alt: %.1f m\n", d->latitude, d->longitude,
           (double)d->altitude_m);
    printf("Speed: %.1f km/h  Course: %.1f°\n", (double)d->speed_kmh,
           (double)d->course_deg);
  }

  printf("UTC: %04d-%02d-%02d %02d:%02d:%02d\n", d->year, d->month, d->day,
         d->hour, d->minute, d->second);
  printf("Sentences parsed: %lu  Track points: %d\n",
         (unsigned long)d->sentence_count, track_count);
  printf("-------------------\n");
}
