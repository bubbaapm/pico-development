/**
 * @file  pico2w-dev.c
 * @brief Main application — orchestrates WS2812 LED matrix, GPS, and WiFi
 *        web server on the Raspberry Pi Pico 2W.
 *
 * Startup sequence:
 *   1. Initialise stdio (USB serial)
 *   2. Initialise WS2812 PIO driver (8×8 matrix on GP1)
 *   3. Run boot animation on the matrix
 *   4. Initialise GPS (UART0, 9600 baud)
 *   5. Connect to WiFi (station mode)
 *   6. Start HTTP web server (lwIP httpd on port 80)
 *   7. Enter main loop — poll GPS, animate effects, service network
 *
 * @author  apmiller
 */

#include <stdio.h>
#include <stdlib.h>

#include "hardware/watchdog.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

#include "gps.h"
#include "matrix.h"
#include "webserver.h"
#include "wifi_credentials.h"
#include "ws2812.h"

// =============================================================================
//  WiFi connection
// =============================================================================

static bool wifi_connected = false;

static bool wifi_connect(void) {
  printf("[wifi] Connecting to '%s'...\n", WIFI_SSID);

  cyw43_arch_enable_sta_mode();

  // Try to connect, retry up to 5 times
  int retries = 5;
  int result = -1;
  for (int i = 1; i <= retries; i++) {
    printf("[wifi] Connection attempt %d/%d...\n", i, retries);
    result = cyw43_arch_wifi_connect_timeout_ms(
        WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 15000);
    if (result == 0) {
      break;
    }
    printf("[wifi] Attempt %d failed (error %d). Retrying in 2 seconds...\n", i, result);
    sleep_ms(2000);
  }

  if (result != 0) {
    printf("[wifi] Connection failed after %d attempts (error %d)\n", retries, result);
    return false;
  }

  // Get and print IP address
  const ip4_addr_t *ip = netif_ip4_addr(netif_default);
  printf("[wifi] Connected! IP address: %s\n", ip4addr_ntoa(ip));

  wifi_connected = true;
  return true;
}

// =============================================================================
//  IP address display on LED matrix
// =============================================================================

/**
 * @brief Briefly display the last octet of the IP address on the matrix.
 *
 * This helps identify the Pico on the network without needing USB serial.
 * The last octet is shown as a 1-3 digit number for ~5 seconds.
 */
static void show_ip_on_matrix(void) {
  if (!wifi_connected)
    return;

  const ip4_addr_t *ip = netif_ip4_addr(netif_default);
  uint8_t last_octet = ip4_addr4_16(ip) & 0xFF;

  ws2812_set_all(0, 0, 0);
  ws2812_set_brightness(32);

  char buf[4];
  snprintf(buf, sizeof(buf), "%d", last_octet);

  // Centre the number on the 8×8 matrix
  uint8_t len = (uint8_t)strlen(buf);
  uint8_t x_start = (8 - len * 4 + 1) / 2; // 4 = char width + gap

  matrix_draw_string(x_start, 1, buf, 0, 150, 255);
  ws2812_show();

  sleep_ms(5000);
  ws2812_clear();
}

// =============================================================================
//  Rainbow effect (non-blocking, for main loop)
// =============================================================================

static uint32_t rainbow_last_ms = 0;
static uint16_t rainbow_hue = 0;

static void rainbow_update(uint32_t now_ms) {
  uint32_t interval = 150 - ((uint32_t)ws2812_get_speed() * 12);
  if (now_ms - rainbow_last_ms < interval)
    return;
  rainbow_last_ms = now_ms;

  for (uint8_t y = 0; y < MATRIX_HEIGHT; y++) {
    for (uint8_t x = 0; x < MATRIX_WIDTH; x++) {
      uint16_t pixel_hue =
          (rainbow_hue + (x + y * MATRIX_WIDTH) * 360 / MATRIX_NUM_LEDS) % 360;
      uint32_t color = ws2812_wheel(pixel_hue);

      uint8_t r = (color >> 8) & 0xFF;
      uint8_t g = (color >> 16) & 0xFF;
      uint8_t b = (color) & 0xFF;
      matrix_set_pixel(x, y, r, g, b);
    }
  }
  ws2812_show();

  rainbow_hue = (rainbow_hue + 2) % 360;
}

// =============================================================================
//  Main loop
// =============================================================================

int main() {
  // 1. Initialise standard I/O (USB serial)
  stdio_init_all();

  // Wait a moment for USB serial to connect (optional, helps see boot messages)
  sleep_ms(2000);

  printf("\n");
  printf("========================================\n");
  printf("  Pico 2W Multi-Peripheral Platform\n");
  printf("  LED Matrix | GPS | WiFi Dashboard\n");
  printf("========================================\n\n");

  // 2. Initialise WS2812 LED matrix (PIO0 SM0 on GP1)
  printf("[init] WS2812 8x8 matrix on GP%d...\n", WS2812_PIN);
  ws2812_init();
  ws2812_set_brightness(32); // ~12% — safe for USB power

  // 3. Boot animation
  printf("[init] Running boot animation...\n");
  matrix_effect_boot();

  // 4. Initialise GPS (UART0)
  printf("[init] GPS module (UART0)...\n");
  gps_init();

  // 5. Initialise WiFi chip
  printf("[init] WiFi chip (CYW43439)...\n");
  if (cyw43_arch_init()) {
    printf("[init] WiFi chip init FAILED!\n");
    // Continue without WiFi — LED and GPS still work
  } else {
    // Show "connecting" indicator on matrix
    ws2812_set_all(0, 0, 40); // dim blue = connecting
    ws2812_show();

    // 6. Connect to WiFi
    if (wifi_connect()) {
      // Flash green on success
      ws2812_set_all(0, 60, 0);
      ws2812_show();
      sleep_ms(500);

      // Show IP address on matrix
      show_ip_on_matrix();

      // 7. Start HTTP server
      printf("[init] Starting HTTP server...\n");
      webserver_init();
    } else {
      // Flash red on failure
      ws2812_set_all(60, 0, 0);
      ws2812_show();
      sleep_ms(2000);
    }
  }

  ws2812_clear();

  printf("\n[init] Startup complete. Entering main loop.\n\n");

  // Onboard LED heartbeat
  cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);

  // GPS status print interval
  uint32_t last_gps_print_ms = 0;
  uint32_t last_heartbeat_ms = 0;
  bool heartbeat_state = true;

  // ==========================================================================
  //  Main loop — cooperative multitasking
  // ==========================================================================

  while (true) {
    uint32_t now_ms = to_ms_since_boot(get_absolute_time());

    // ── GPS: read and parse NMEA data ──
    gps_task();

    // ── LED: update active effect ──
    web_effect_t effect = webserver_get_active_effect();

    switch (effect) {
    case WEB_EFFECT_RAINBOW:
      rainbow_update(now_ms);
      break;

    case WEB_EFFECT_BREATHE:
    case WEB_EFFECT_STROBE:
    case WEB_EFFECT_GRADIENT:
      ws2812_effect_update(now_ms);
      break;

    case WEB_EFFECT_FIRE:
      matrix_effect_fire(now_ms);
      break;

    case WEB_EFFECT_RAIN:
      matrix_effect_rain(now_ms);
      break;

    case WEB_EFFECT_RADAR:
      matrix_effect_radar(now_ms);
      break;

    case WEB_EFFECT_SNOW:
      matrix_effect_snow(now_ms);
      break;

    case WEB_EFFECT_WAVE:
      matrix_effect_wave(now_ms);
      break;

    case WEB_EFFECT_COMPASS: {
      const gps_data_t *gps = gps_get_data();
      if (now_ms % 500 < 20) { // update every ~500ms
        matrix_effect_compass(gps->course_deg);
      }
      break;
    }

    case WEB_EFFECT_GPS_STATUS: {
      const gps_data_t *gps = gps_get_data();
      if (now_ms % 1000 < 20) { // update every ~1s
        matrix_effect_gps_status(gps->fix_quality, gps->satellites);
      }
      break;
    }

    case WEB_EFFECT_SOLID:
    case WEB_EFFECT_NONE:
    default:
      break;
    }

    // ── Network: service WiFi + TCP stack ──
    if (wifi_connected) {
      cyw43_arch_poll();
    }

    // ── Heartbeat: blink onboard LED ──
    if (now_ms - last_heartbeat_ms >= 1000) {
      last_heartbeat_ms = now_ms;
      heartbeat_state = !heartbeat_state;
      cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, heartbeat_state);
    }

    // ── Debug: print GPS status every 10 seconds ──
    if (now_ms - last_gps_print_ms >= 10000) {
      last_gps_print_ms = now_ms;
      gps_print_status();
    }

    // ── Yield ──
    sleep_ms(1);
  }
}
