#include "webserver.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fancy_lcd_app.h"
#include "lwip/apps/httpd.h"

static const char *ssi_tags[] = {
    "anim_name",
    "anim_index",
    "anim_count",
    "duration_ms",
    "run_count",
    "anim_options",
};

static const char *cgi_get_param(int count, char *params[], char *values[],
                                 const char *name) {
  for (int i = 0; i < count; i++) {
    if (strcmp(params[i], name) == 0) {
      return values[i];
    }
  }
  return NULL;
}

static u16_t ssi_handler(int index, char *insert, int insert_len,
                         u16_t current_tag_part, u16_t *next_tag_part) {
  (void)current_tag_part;
  (void)next_tag_part;

  uint8_t count = 0;
  const animation_entry_t *animations = app_animation_list(&count);
  uint8_t active = app_get_animation_index();

  switch (index) {
  case 0:
    snprintf(insert, (size_t)insert_len, "%s", animations[active].label);
    break;
  case 1:
    snprintf(insert, (size_t)insert_len, "%u", active);
    break;
  case 2:
    snprintf(insert, (size_t)insert_len, "%u", count);
    break;
  case 3:
    snprintf(insert, (size_t)insert_len, "%lu",
             (unsigned long)app_get_animation_duration_ms());
    break;
  case 4:
    snprintf(insert, (size_t)insert_len, "%lu",
             (unsigned long)app_get_completed_runs());
    break;
  case 5: {
    int written = 0;
    for (uint8_t i = 0; i < count; i++) {
      int remaining = insert_len - written;
      if (remaining <= 1) break;
      int res = snprintf(insert + written, (size_t)remaining,
                          "%s<option value=\"%u\"%s>%s</option>",
                          i == 0 ? "" : "\n", i,
                          i == active ? " selected" : "",
                          animations[i].label);
      if (res < 0) {
        break;
      }
      if (res >= remaining) {
        written = insert_len - 1;
        break;
      }
      written += res;
    }
    return (u16_t)written;
  }
  default:
    insert[0] = '\0';
    break;
  }

  return (u16_t)strlen(insert);
}

static const char *cgi_select(int index, int param_count, char *params[],
                              char *values[]) {
  (void)index;
  const char *anim = cgi_get_param(param_count, params, values, "idx");
  if (anim) {
    app_set_animation_index((uint8_t)atoi(anim));
    app_request_run();
  }
  return "/index.shtml";
}

static const char *cgi_next(int index, int param_count, char *params[],
                            char *values[]) {
  (void)index;
  (void)param_count;
  (void)params;
  (void)values;
  app_next_animation();
  app_request_run();
  return "/index.shtml";
}

static const char *cgi_previous(int index, int param_count, char *params[],
                                char *values[]) {
  (void)index;
  (void)param_count;
  (void)params;
  (void)values;
  app_previous_animation();
  app_request_run();
  return "/index.shtml";
}

static const char *cgi_duration(int index, int param_count, char *params[],
                                char *values[]) {
  (void)index;
  const char *duration = cgi_get_param(param_count, params, values, "ms");
  if (duration) {
    app_set_animation_duration_ms((uint32_t)strtoul(duration, NULL, 10));
  }
  return "/index.shtml";
}

static const char *cgi_run(int index, int param_count, char *params[],
                           char *values[]) {
  (void)index;
  (void)param_count;
  (void)params;
  (void)values;
  app_request_run();
  return "/index.shtml";
}

static const tCGI cgi_handlers[] = {
    {"/anim/select", cgi_select},
    {"/anim/next", cgi_next},
    {"/anim/previous", cgi_previous},
    {"/anim/duration", cgi_duration},
    {"/anim/run", cgi_run},
};

void webserver_init(void) {
  http_set_ssi_handler(ssi_handler, ssi_tags,
                       sizeof(ssi_tags) / sizeof(ssi_tags[0]));
  http_set_cgi_handlers(cgi_handlers,
                        sizeof(cgi_handlers) / sizeof(cgi_handlers[0]));
  httpd_init();
  printf("[web] HTTP server started\n");
}
