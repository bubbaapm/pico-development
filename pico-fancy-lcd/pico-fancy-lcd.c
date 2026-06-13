#include <stdio.h>

#include "buttons.h"
#include "fancy_lcd_app.h"
#include "lcd_animations.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "st7735.h"
#include "web_capture.h"
#include "webserver.h"
#include "wifi_config.h"

static bool wifi_connected = false;

static void draw_status(const char *line1, const char *line2) {
  lcd_fill_screen(COLOR_BLACK);
  lcd_set_text_color(COLOR_CYAN, COLOR_BLACK);
  lcd_set_text_size(1);
  lcd_gotoLine(2);
  lcd_puts("Pico Fancy LCD");
  lcd_gotoLine(5);
  lcd_set_text_color(COLOR_WHITE, COLOR_BLACK);
  lcd_puts(line1);
  lcd_gotoLine(7);
  lcd_puts(line2);
  lcd_flush();
}

static bool wifi_connect(void) {
  if (WIFI_SSID[0] == '\0') {
    printf("[wifi] No wifi_credentials.h found; running LCD-only.\n");
    return false;
  }

  cyw43_arch_enable_sta_mode();
  printf("[wifi] Connecting to %s...\n", WIFI_SSID);
  draw_status("Connecting WiFi", WIFI_SSID);

  int result = cyw43_arch_wifi_connect_timeout_ms(
      WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 15000);
  if (result != 0) {
    printf("[wifi] Connection failed: %d\n", result);
    draw_status("WiFi failed", "LCD continues");
    sleep_ms(1200);
    return false;
  }

  const ip4_addr_t *ip = netif_ip4_addr(netif_default);
  printf("[wifi] Connected: %s\n", ip4addr_ntoa(ip));

  char ip_line[32];
  snprintf(ip_line, sizeof(ip_line), "%s", ip4addr_ntoa(ip));
  draw_status("Web UI ready", ip_line);
  sleep_ms(1500);
  return true;
}

static void handle_buttons(void) {
  buttons_update();
  if (buttons_left_short()) {
    app_previous_animation();
    app_request_run();
  } else if (buttons_right_short()) {
    app_next_animation();
    app_request_run();
  } else if (buttons_dual_press()) {
    app_request_run();
  }
  buttons_clear_events();
}

static void run_active_animation(void) {
  uint8_t count = 0;
  const animation_entry_t *animations = app_animation_list(&count);
  uint8_t index = app_get_animation_index();
  uint32_t duration = app_get_animation_duration_ms();

  printf("[anim] Running %s for %lu ms\n", animations[index].id,
         (unsigned long)duration);

  if (animations[index].run) {
    animations[index].run(duration);
  } else {
    anim_demo_all();
  }
}

int main(void) {
  stdio_init_all();
  sleep_ms(1000);

  printf("\n[pico-fancy-lcd] boot\n");
  buttons_init();
  lcd_init();
  draw_status("Booting", "LCD animation host");

  if (cyw43_arch_init()) {
    printf("[wifi] CYW43 init failed; continuing without network.\n");
  } else {
    wifi_connected = wifi_connect();
    if (wifi_connected) {
      webserver_init();
      web_capture_init();
    }
  }

  while (true) {
    if (wifi_connected) {
      cyw43_arch_poll();
    }

    handle_buttons();

    if (app_take_run_request()) {
      run_active_animation();
      app_request_run();
    }

    sleep_ms(1);
  }
}
