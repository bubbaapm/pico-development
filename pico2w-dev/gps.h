/**
 * @file  gps.h
 * @brief u-blox NEO-6M GPS NMEA parser for RP2350.
 *
 * Reads NMEA sentences from UART0 (GP16 TX, GP17 RX) and parses
 * position, velocity, time, and satellite information.
 *
 * Non-blocking design: call gps_task() from the main loop.
 *
 * @author  apmiller
 */

#ifndef GPS_H
#define GPS_H

#include <stdbool.h>
#include <stdint.h>

// ─── UART pin configuration ─────────────────────────────────────────────────
#define GPS_UART_ID   uart0
#define GPS_UART_BAUD 9600
#define GPS_TX_PIN    16  // Pico TX → GPS RX (for sending commands to GPS)
#define GPS_RX_PIN    17  // Pico RX ← GPS TX (GPS sends NMEA data here)

// ─── Internal buffer sizes ──────────────────────────────────────────────────
#define GPS_SENTENCE_MAX_LEN 120
#define GPS_TRACK_BUFFER_SIZE 100

// ─── GPS data structure ─────────────────────────────────────────────────────

typedef struct {
  // Position
  double latitude;   // Decimal degrees (positive = North)
  double longitude;  // Decimal degrees (positive = East)
  float altitude_m;  // Metres above mean sea level

  // Motion
  float speed_knots; // Speed over ground (knots)
  float speed_kmh;   // Speed over ground (km/h) — derived
  float course_deg;  // Course over ground (degrees true)

  // Fix quality
  uint8_t fix_quality; // 0=invalid, 1=GPS fix, 2=DGPS
  uint8_t satellites;  // Satellites in use
  float hdop;          // Horizontal dilution of precision

  // UTC time from GPS
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
  uint8_t day;
  uint8_t month;
  uint16_t year;

  // Status
  bool valid;              // True if we have a valid fix (RMC status 'A')
  uint32_t last_fix_ms;    // Timestamp of last valid fix (ms since boot)
  uint32_t sentence_count; // Total NMEA sentences successfully parsed
} gps_data_t;

// ─── Track point for plotting ───────────────────────────────────────────────

typedef struct {
  float lat;
  float lon;
  float speed_kmh;
  uint32_t timestamp_ms;
} gps_track_point_t;

// ─── Public API ─────────────────────────────────────────────────────────────

/**
 * @brief Initialise UART0 for GPS communication (9600 baud, 8N1).
 *
 * Configures GP16 as UART0 TX and GP17 as UART0 RX.
 * Enables the UART RX interrupt for efficient data reception.
 */
void gps_init(void);

/**
 * @brief Process any buffered UART data and parse complete NMEA sentences.
 *
 * Non-blocking — returns immediately if no data is available or if a
 * sentence is not yet complete.  Call this from the main loop.
 */
void gps_task(void);

/**
 * @brief Get a pointer to the latest parsed GPS data.
 *
 * The returned pointer is valid for the lifetime of the program.
 * Data is updated in-place by gps_task().
 *
 * @return  Pointer to the current GPS data structure.
 */
const gps_data_t *gps_get_data(void);

/**
 * @brief Quick check if we have a valid GPS fix.
 * @return  true if the last GGA/RMC sentence indicated a valid fix.
 */
bool gps_has_fix(void);

/**
 * @brief Get the circular buffer of recent track points.
 *
 * @param[out] count  Number of valid points in the buffer.
 * @return  Pointer to the track point array (GPS_TRACK_BUFFER_SIZE entries).
 *          Points are stored in chronological order starting at the
 *          internal write index; use gps_get_track_start_index() to find
 *          the oldest entry.
 */
const gps_track_point_t *gps_get_track(uint16_t *count);

/**
 * @brief Get the index of the oldest track point in the circular buffer.
 */
uint16_t gps_get_track_start_index(void);

/**
 * @brief Print the current GPS state to stdout (USB serial).
 *        Useful for debugging.
 */
void gps_print_status(void);

#endif // GPS_H
