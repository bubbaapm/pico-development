/**
 * @file  webserver.h
 * @brief HTTP web server for LED control and GPS data display.
 *
 * Uses lwIP's built-in httpd with SSI (Server-Side Includes) for dynamic
 * GPS data and CGI (Common Gateway Interface) for LED control commands.
 *
 * @author  apmiller
 */

#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <stdbool.h>
#include <stdint.h>

// ─── Active effect tracking ─────────────────────────────────────────────────

typedef enum {
  WEB_EFFECT_NONE,
  WEB_EFFECT_SOLID,
  WEB_EFFECT_RAINBOW,
  WEB_EFFECT_BREATHE,
  WEB_EFFECT_STROBE,
  WEB_EFFECT_FIRE,
  WEB_EFFECT_RAIN,
  WEB_EFFECT_COMPASS,
  WEB_EFFECT_GPS_STATUS,
  WEB_EFFECT_GRADIENT,
  WEB_EFFECT_RADAR,
  WEB_EFFECT_SNOW,
  WEB_EFFECT_WAVE
} web_effect_t;

/**
 * @brief Initialise the HTTP server.
 *
 * Registers SSI tags and CGI handlers, then starts the lwIP httpd
 * listening on port 80.
 *
 * Must be called after cyw43_arch_init() and WiFi connection.
 */
void webserver_init(void);

/**
 * @brief Get the currently active web-controlled effect.
 *
 * Used by the main loop to know which matrix effect to call.
 */
web_effect_t webserver_get_active_effect(void);

#endif // WEBSERVER_H
