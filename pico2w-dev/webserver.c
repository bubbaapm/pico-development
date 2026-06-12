/**
 * @file  webserver.c
 * @brief HTTP web server — SSI and CGI handlers for LED control & GPS data.
 *
 * SSI tags inject live GPS data into HTML pages.
 * CGI endpoints handle LED colour/effect commands from the browser.
 *
 * @author  apmiller
 */

#include "webserver.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gps.h"
#include "matrix.h"
#include "ws2812.h"

#include "lwip/apps/httpd.h"
#include "pico/cyw43_arch.h"

// =============================================================================
//  Active effect state
// =============================================================================

static web_effect_t active_effect = WEB_EFFECT_NONE;

web_effect_t webserver_get_active_effect(void) { return active_effect; }

// =============================================================================
//  SSI (Server-Side Includes) — Dynamic data injection
// =============================================================================

// SSI tag names — these appear in the HTML as <!--#tagname-->
// Max tag name length is configured by LWIP_HTTPD_MAX_TAG_NAME_LEN in lwipopts.h
static const char *ssi_tags[] = {
    "gps_lat",     // 0: Latitude
    "gps_lon",     // 1: Longitude
    "gps_alt",     // 2: Altitude
    "gps_spd",     // 3: Speed (km/h)
    "gps_crs",     // 4: Course (degrees)
    "gps_sat",     // 5: Satellite count
    "gps_fix",     // 6: Fix quality
    "gps_hdop",    // 7: HDOP
    "gps_time",    // 8: UTC time
    "gps_date",    // 9: Date
    "gps_valid",   // 10: Valid fix flag
    "gps_scount",  // 11: Sentence count
    "gps_track",   // 12: Track data as JSON array
    "led_bright",  // 13: Current brightness
    "led_effect",  // 14: Current effect name
};

#define SSI_TAG_COUNT (sizeof(ssi_tags) / sizeof(ssi_tags[0]))

/**
 * @brief SSI handler callback — called by lwIP httpd for each SSI tag.
 *
 * @param iIndex   Index into ssi_tags[] for the matched tag
 * @param pcInsert Buffer to write the replacement text into
 * @param iInsertLen  Maximum length of the replacement text
 * @return         Number of characters written to pcInsert
 */
static u16_t ssi_handler(int iIndex, char *pcInsert, int iInsertLen,
                         u16_t current_tag_part, u16_t *next_tag_part) {
  (void)current_tag_part;
  (void)next_tag_part;
  const gps_data_t *gps = gps_get_data();

  switch (iIndex) {
  case 0: // gps_lat
    snprintf(pcInsert, (size_t)iInsertLen, "%.6f", gps->latitude);
    break;
  case 1: // gps_lon
    snprintf(pcInsert, (size_t)iInsertLen, "%.6f", gps->longitude);
    break;
  case 2: // gps_alt
    snprintf(pcInsert, (size_t)iInsertLen, "%.1f", (double)gps->altitude_m);
    break;
  case 3: // gps_spd
    snprintf(pcInsert, (size_t)iInsertLen, "%.1f", (double)gps->speed_kmh);
    break;
  case 4: // gps_crs
    snprintf(pcInsert, (size_t)iInsertLen, "%.1f", (double)gps->course_deg);
    break;
  case 5: // gps_sat
    snprintf(pcInsert, (size_t)iInsertLen, "%d", gps->satellites);
    break;
  case 6: // gps_fix
    snprintf(pcInsert, (size_t)iInsertLen, "%d", gps->fix_quality);
    break;
  case 7: // gps_hdop
    snprintf(pcInsert, (size_t)iInsertLen, "%.1f", (double)gps->hdop);
    break;
  case 8: // gps_time
    snprintf(pcInsert, (size_t)iInsertLen, "%02d:%02d:%02d", gps->hour,
             gps->minute, gps->second);
    break;
  case 9: // gps_date
    snprintf(pcInsert, (size_t)iInsertLen, "%04d-%02d-%02d", gps->year,
             gps->month, gps->day);
    break;
  case 10: // gps_valid
    snprintf(pcInsert, (size_t)iInsertLen, "%s",
             gps->valid ? "true" : "false");
    break;
  case 11: // gps_scount
    snprintf(pcInsert, (size_t)iInsertLen, "%lu",
             (unsigned long)gps->sentence_count);
    break;
  case 12: { // gps_track — JSON array of recent positions
    uint16_t count = 0;
    const gps_track_point_t *track = gps_get_track(&count);
    uint16_t start = gps_get_track_start_index();

    int written = 0;
    written += snprintf(pcInsert + written, (size_t)(iInsertLen - written), "[");

    for (uint16_t i = 0; i < count && written < iInsertLen - 40; i++) {
      uint16_t idx = (start + i) % GPS_TRACK_BUFFER_SIZE;
      if (i > 0)
        written +=
            snprintf(pcInsert + written, (size_t)(iInsertLen - written), ",");
      written += snprintf(pcInsert + written, (size_t)(iInsertLen - written),
                          "[%.6f,%.6f,%.1f]", (double)track[idx].lat,
                          (double)track[idx].lon, (double)track[idx].speed_kmh);
    }

    written += snprintf(pcInsert + written, (size_t)(iInsertLen - written), "]");
    return (u16_t)written;
  }
  case 13: // led_bright
    snprintf(pcInsert, (size_t)iInsertLen, "%d", ws2812_get_brightness());
    break;
  case 14: { // led_effect
    const char *names[] = {"none",    "solid",   "rainbow",    "breathe",
                           "strobe",  "fire",    "rain",       "compass",
                           "gps_status", "gradient", "radar", "snow", "wave"};
    int idx = (int)active_effect;
    if (idx < 0 || idx >= (int)(sizeof(names) / sizeof(names[0])))
      idx = 0;
    snprintf(pcInsert, (size_t)iInsertLen, "%s", names[idx]);
    break;
  }
  default:
    pcInsert[0] = '\0';
    break;
  }

  return (u16_t)strlen(pcInsert);
}

// =============================================================================
//  CGI (Common Gateway Interface) — LED control endpoints
// =============================================================================

/**
 * @brief Find a query parameter value by name.
 *
 * @param params  Array of parameter names
 * @param values  Array of parameter values
 * @param count   Number of parameters
 * @param name    Parameter name to search for
 * @return        Parameter value string, or NULL if not found
 */
static const char *cgi_get_param(int count, char *params[], char *values[],
                                 const char *name) {
  for (int i = 0; i < count; i++) {
    if (strcmp(params[i], name) == 0)
      return values[i];
  }
  return NULL;
}

static uint8_t cgi_parse_uint8(const char *val, uint8_t default_val) {
  if (!val || !*val)
    return default_val;
  int v = atoi(val);
  if (v < 0)
    return 0;
  if (v > 255)
    return 255;
  return (uint8_t)v;
}

// --- /led/set?r=&g=&b=&bright= ---
static const char *cgi_led_set(int iIndex, int iNumParams, char *pcParam[],
                               char *pcValue[]) {
  (void)iIndex;

  uint8_t r = cgi_parse_uint8(cgi_get_param(iNumParams, pcParam, pcValue, "r"), 0);
  uint8_t g = cgi_parse_uint8(cgi_get_param(iNumParams, pcParam, pcValue, "g"), 0);
  uint8_t b = cgi_parse_uint8(cgi_get_param(iNumParams, pcParam, pcValue, "b"), 0);

  const char *bright_str = cgi_get_param(iNumParams, pcParam, pcValue, "bright");
  if (bright_str) {
    int bright = atoi(bright_str);
    if (bright < 0) bright = 0;
    if (bright > 100) bright = 100;
    ws2812_set_brightness((uint8_t)((uint16_t)bright * 255 / 100));
  }

  ws2812_set_all(r, g, b);
  ws2812_show();
  active_effect = WEB_EFFECT_SOLID;

  return "/index.shtml";
}

// --- /led/effect?type=rainbow|breathe|fire|rain|compass|gps|off ---
static const char *cgi_led_effect(int iIndex, int iNumParams, char *pcParam[],
                                  char *pcValue[]) {
  (void)iIndex;

  const char *type = cgi_get_param(iNumParams, pcParam, pcValue, "type");
  if (!type)
    return "/index.shtml";

  if (strcmp(type, "rainbow") == 0) {
    active_effect = WEB_EFFECT_RAINBOW;
  } else if (strcmp(type, "breathe") == 0) {
    uint8_t r = cgi_parse_uint8(cgi_get_param(iNumParams, pcParam, pcValue, "r"), 0);
    uint8_t g = cgi_parse_uint8(cgi_get_param(iNumParams, pcParam, pcValue, "g"), 0);
    uint8_t b = cgi_parse_uint8(cgi_get_param(iNumParams, pcParam, pcValue, "b"), 255);
    const char *period_str = cgi_get_param(iNumParams, pcParam, pcValue, "period");
    uint16_t period = period_str ? (uint16_t)atoi(period_str) : 2000;

    ws2812_effect_breathe(r, g, b, 80, period);
    active_effect = WEB_EFFECT_BREATHE;
  } else if (strcmp(type, "strobe") == 0) {
    uint8_t r = cgi_parse_uint8(cgi_get_param(iNumParams, pcParam, pcValue, "r"), 255);
    uint8_t g = cgi_parse_uint8(cgi_get_param(iNumParams, pcParam, pcValue, "g"), 255);
    uint8_t b = cgi_parse_uint8(cgi_get_param(iNumParams, pcParam, pcValue, "b"), 255);

    ws2812_effect_strobe(r, g, b, 80, 50, 100);
    active_effect = WEB_EFFECT_STROBE;
  } else if (strcmp(type, "fire") == 0) {
    active_effect = WEB_EFFECT_FIRE;
  } else if (strcmp(type, "rain") == 0) {
    active_effect = WEB_EFFECT_RAIN;
  } else if (strcmp(type, "compass") == 0) {
    active_effect = WEB_EFFECT_COMPASS;
  } else if (strcmp(type, "gps") == 0) {
    active_effect = WEB_EFFECT_GPS_STATUS;
  } else if (strcmp(type, "radar") == 0) {
    active_effect = WEB_EFFECT_RADAR;
  } else if (strcmp(type, "snow") == 0) {
    active_effect = WEB_EFFECT_SNOW;
  } else if (strcmp(type, "wave") == 0) {
    active_effect = WEB_EFFECT_WAVE;
  } else if (strcmp(type, "off") == 0) {
    ws2812_effect_stop();
    ws2812_clear();
    active_effect = WEB_EFFECT_NONE;
  }

  return "/index.shtml";
}

// --- /led/pixel?x=&y=&r=&g=&b= ---
static const char *cgi_led_pixel(int iIndex, int iNumParams, char *pcParam[],
                                 char *pcValue[]) {
  (void)iIndex;

  uint8_t x = cgi_parse_uint8(cgi_get_param(iNumParams, pcParam, pcValue, "x"), 0);
  uint8_t y = cgi_parse_uint8(cgi_get_param(iNumParams, pcParam, pcValue, "y"), 0);
  uint8_t r = cgi_parse_uint8(cgi_get_param(iNumParams, pcParam, pcValue, "r"), 255);
  uint8_t g = cgi_parse_uint8(cgi_get_param(iNumParams, pcParam, pcValue, "g"), 255);
  uint8_t b = cgi_parse_uint8(cgi_get_param(iNumParams, pcParam, pcValue, "b"), 255);

  matrix_set_pixel(x, y, r, g, b);
  ws2812_show();
  active_effect = WEB_EFFECT_NONE; // stop any running effect

  return "/index.shtml";
}

// --- /led/brightness?val= ---
static const char *cgi_led_brightness(int iIndex, int iNumParams,
                                      char *pcParam[], char *pcValue[]) {
  (void)iIndex;

  const char *val = cgi_get_param(iNumParams, pcParam, pcValue, "val");
  if (val) {
    int bright = atoi(val);
    if (bright < 0) bright = 0;
    if (bright > 100) bright = 100;
    ws2812_set_brightness((uint8_t)((uint16_t)bright * 255 / 100));
    ws2812_show();
  }

  return "/index.shtml";
}

// --- /led/speed?val= ---
static const char *cgi_led_speed(int iIndex, int iNumParams,
                                 char *pcParam[], char *pcValue[]) {
  (void)iIndex;

  const char *val = cgi_get_param(iNumParams, pcParam, pcValue, "val");
  if (val) {
    int speed = atoi(val);
    if (speed < 1) speed = 1;
    if (speed > 10) speed = 10;
    ws2812_set_speed((uint8_t)speed);
  }

  return "/index.shtml";
}

// --- /gps/data — returns the same page but browser fetches for SSI data ---
static const char *cgi_gps_data(int iIndex, int iNumParams, char *pcParam[],
                                char *pcValue[]) {
  (void)iIndex;
  (void)iNumParams;
  (void)pcParam;
  (void)pcValue;

  // Return the GPS data fragment (SSI-populated)
  return "/gpsdata.shtml";
}

// ─── CGI URL table ──────────────────────────────────────────────────────────

static const tCGI cgi_handlers[] = {
    {"/led/set", cgi_led_set},
    {"/led/effect", cgi_led_effect},
    {"/led/pixel", cgi_led_pixel},
    {"/led/brightness", cgi_led_brightness},
    {"/led/speed", cgi_led_speed},
    {"/gps/data", cgi_gps_data},
};

#define CGI_HANDLER_COUNT (sizeof(cgi_handlers) / sizeof(cgi_handlers[0]))

// =============================================================================
//  Initialisation
// =============================================================================

void webserver_init(void) {
  // Register SSI tags and handler
  http_set_ssi_handler(ssi_handler, ssi_tags, SSI_TAG_COUNT);

  // Register CGI handlers
  http_set_cgi_handlers(cgi_handlers, CGI_HANDLER_COUNT);

  // Start the HTTP server on port 80
  httpd_init();

  printf("[webserver] HTTP server started on port 80\n");
  printf("[webserver] SSI tags: %d, CGI handlers: %d\n", (int)SSI_TAG_COUNT,
         (int)CGI_HANDLER_COUNT);
}
