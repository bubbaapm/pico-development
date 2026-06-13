#include "../lcd_animations.h"
#include "../buttons.h"
#include "../ld2410.h"
#include "../st7735.h"
#include "pico/stdlib.h"
#include <math.h>
#include <string.h>

// ─── Wave Particle Mesh ─────────────────────────────────────────────────────

void anim_wave_particles(uint32_t duration_ms) {
  absolute_time_t end;
  if (duration_ms > 0) {
    end = make_timeout_time_ms(duration_ms);
  }

  uint16_t frame = 0;
  while (duration_ms == 0 || !time_reached(end)) {
    buttons_update();
    if (buttons_any_event())
      return;

    lcd_fill_screen(COLOR_BLACK);
    
    radar_data_t rdata = radar_get_data();
    float amp_scale = 1.0f;
    float freq_scale = 1.0f;
    if (rdata.data_valid && rdata.target_state != RADAR_TARGET_NONE) {
      uint8_t energy = rdata.moving_energy > rdata.stationary_energy ? rdata.moving_energy : rdata.stationary_energy;
      amp_scale = 1.0f + (energy / 30.0f);
      freq_scale = 1.0f + (energy / 50.0f);
    }

    for (int wave = 0; wave < 3; wave++) {
      uint16_t hue = (frame + wave * 120) % 360;
      uint16_t color = hsv_to_rgb565(hue, 255, 255);

      for (int x = 0; x < LCD_WIDTH; x += 8) {
        float angle1 = (x * 0.04f * freq_scale) + (frame * 0.05f) + (wave * 2.0f);
        float angle2 = (x * 0.015f * freq_scale) - (frame * 0.02f) + (wave * 1.0f);
        
        float y = (LCD_HEIGHT / 2.0f) + 
                  sinf(angle1) * (20.0f * amp_scale) + 
                  cosf(angle2) * (10.0f * amp_scale);

        int py = (int)y;
        if (py >= 0 && py < LCD_HEIGHT) {
          lcd_fill_rect(x, py, 2, 2, color);
          if (wave > 0) {
            float y_prev_wave = (LCD_HEIGHT / 2.0f) + 
                                sinf((x * 0.04f * freq_scale) + (frame * 0.05f) + ((wave - 1) * 2.0f)) * (20.0f * amp_scale) + 
                                cosf((x * 0.015f * freq_scale) - (frame * 0.02f) + ((wave - 1) * 1.0f)) * (10.0f * amp_scale);
            lcd_draw_line(x, py, x, (int)y_prev_wave, hsv_to_rgb565((hue + 40) % 360, 200, 80));
          }
        }
      }
    }

    lcd_flush();
    frame += 2;
    animation_delay(30);
  }
}

// ─── Fireworks Show ──────────────────────────────────────────────────────────

void anim_fireworks(uint32_t duration_ms) {
  absolute_time_t end;
  if (duration_ms > 0) {
    end = make_timeout_time_ms(duration_ms);
  }

#define MAX_PARTICLES 80
  struct fw_particle {
    float x, y;
    float vx, vy;
    uint16_t color;
    uint16_t hue;
    uint8_t life;
    uint8_t max_life;
    bool active;
  };
  struct fw_particle particles[MAX_PARTICLES];
  memset(particles, 0, sizeof(particles));

#define MAX_ROCKETS 2
  struct fw_rocket {
    float x, y;
    float vx, vy;
    uint16_t hue;
    bool active;
  };
  struct fw_rocket rockets[MAX_ROCKETS];
  memset(rockets, 0, sizeof(rockets));

  int launch_delay[MAX_ROCKETS] = {0, 0};
  uint16_t main_hue = 0;

  while (duration_ms == 0 || !time_reached(end)) {
    buttons_update();
    if (buttons_any_event())
      return;

    lcd_fill_screen(COLOR_BLACK);

    radar_data_t rdata = radar_get_data();
    uint8_t energy = 0;
    if (rdata.data_valid) {
      energy = rdata.moving_energy > rdata.stationary_energy ? rdata.moving_energy : rdata.stationary_energy;
    }

    // Process queued rocket launches
    for (int r = 0; r < MAX_ROCKETS; r++) {
      if (launch_delay[r] > 0) {
        launch_delay[r]--;
        if (launch_delay[r] == 0) {
          rockets[r].x = (float)(30 + (fast_rand() % (LCD_WIDTH - 60)));
          rockets[r].y = (float)LCD_HEIGHT;
          rockets[r].vx = (float)((int)(fast_rand() % 40) - 20) / 25.0f;
          rockets[r].vy = -2.8f - ((float)(fast_rand() % 16) / 10.0f);
          rockets[r].hue = main_hue;
          rockets[r].active = true;
          main_hue = (main_hue + 70) % 360;
        }
      }
    }

    // Check if we can initiate a new launch sequence (only if no active rockets and no pending launches)
    bool launch_sequence_active = rockets[0].active || rockets[1].active || (launch_delay[0] > 0) || (launch_delay[1] > 0);
    
    // Count active particles
    int active_particles = 0;
    for (int i = 0; i < MAX_PARTICLES; i++) {
      if (particles[i].active) active_particles++;
    }

    int spawn_chance = (energy > 20) ? 12 : 35;
    if (!launch_sequence_active && (active_particles < 10 || fast_rand() % spawn_chance == 0)) {
      // Decide single vs double launch (70% single / 30% double)
      bool double_launch = (fast_rand() % 100 < 30);
      
      // Spawn first rocket immediately
      rockets[0].x = (float)(30 + (fast_rand() % (LCD_WIDTH - 60)));
      rockets[0].y = (float)LCD_HEIGHT;
      rockets[0].vx = (float)((int)(fast_rand() % 40) - 20) / 25.0f;
      rockets[0].vy = -2.8f - ((float)(fast_rand() % 16) / 10.0f);
      rockets[0].hue = main_hue;
      rockets[0].active = true;
      main_hue = (main_hue + 70) % 360;
      
      if (double_launch) {
        // Queue second rocket with delay
        launch_delay[1] = 12 + (fast_rand() % 13); // 12 to 24 frames delay
      }
    }

    // Update rockets
    for (int r = 0; r < MAX_ROCKETS; r++) {
      if (rockets[r].active) {
        rockets[r].x += rockets[r].vx;
        rockets[r].y += rockets[r].vy;
        rockets[r].vy += 0.08f; // gravity deceleration

        lcd_fill_rect((int)rockets[r].x - 1, (int)rockets[r].y - 1, 3, 3, COLOR_WHITE);
        lcd_draw_pixel((int)rockets[r].x, (int)rockets[r].y + 2, COLOR_YELLOW);

        // Explode at gravity peak or if reaching screen boundary
        if (rockets[r].vy >= -0.1f || rockets[r].y <= 15.0f) {
          rockets[r].active = false;

          uint16_t exp_hue = rockets[r].hue;
          uint16_t exp_color = hsv_to_rgb565(exp_hue, 255, 255);
          
          uint8_t rand_type = fast_rand() % 100;
          bool super_explosion = (rand_type < 10); // 10% chance for a super-explosion
          bool large_explosion = (!super_explosion && rand_type < 35); // 25% chance for large
          
          int num_sparks;
          if (super_explosion) {
            num_sparks = 50 + (fast_rand() % 15); // 50 to 65 sparks
          } else if (large_explosion) {
            num_sparks = 30 + (fast_rand() % 10); // 30 to 40 sparks
          } else {
            num_sparks = 18 + (fast_rand() % 8);  // 18 to 25 sparks
          }
          int spawned = 0;

          for (int i = 0; i < MAX_PARTICLES; i++) {
            if (!particles[i].active) {
              float vx_spark, vy_spark;
              float angle = (float)(fast_rand() % 360) * 3.14159f / 180.0f;
              
              float spd;
              if (super_explosion) {
                spd = 0.8f + ((float)(fast_rand() % 180) / 60.0f); // 0.8 to 3.8
              } else if (large_explosion) {
                spd = 0.6f + ((float)(fast_rand() % 120) / 60.0f); // 0.6 to 2.6
              } else {
                spd = 0.4f + ((float)(fast_rand() % 60) / 60.0f);  // 0.4 to 1.4
              }
              vx_spark = cosf(angle) * spd;
              vy_spark = sinf(angle) * spd;

              particles[i].x = rockets[r].x;
              particles[i].y = rockets[r].y;
              particles[i].vx = vx_spark;
              particles[i].vy = vy_spark;
              particles[i].color = exp_color;
              particles[i].hue = exp_hue;
              
              if (super_explosion) {
                particles[i].life = 35 + (fast_rand() % 20); // 35 to 55 frames
              } else if (large_explosion) {
                particles[i].life = 25 + (fast_rand() % 15); // 25 to 40 frames
              } else {
                particles[i].life = 15 + (fast_rand() % 10); // 15 to 25 frames
              }
              particles[i].max_life = particles[i].life;
              particles[i].active = true;

              spawned++;
              if (spawned >= num_sparks) break;
            }
          }
        }
      }
    }

    // Update and draw particles
    for (int i = 0; i < MAX_PARTICLES; i++) {
      if (particles[i].active) {
        particles[i].x += particles[i].vx;
        particles[i].y += particles[i].vy;
        particles[i].vy += 0.03f;
        particles[i].life--;

        if (particles[i].life == 0 || particles[i].x < 0 || particles[i].x >= LCD_WIDTH || particles[i].y >= LCD_HEIGHT) {
          particles[i].active = false;
        } else {
          uint8_t life_ratio = (particles[i].life * 255) / particles[i].max_life;
          uint16_t col = hsv_to_rgb565(particles[i].hue, 255, life_ratio);
          lcd_draw_pixel((int)particles[i].x, (int)particles[i].y, col);
        }
      }
    }

    lcd_flush();
    animation_delay(25);
  }
}

// ─── Rain Ripples ───────────────────────────────────────────────────────────

void anim_rain_ripples(uint32_t duration_ms) {
  absolute_time_t end;
  if (duration_ms > 0) {
    end = make_timeout_time_ms(duration_ms);
  }

#define MAX_RIPPLES 8
  struct ripple {
    int x, y;
    int r;
    int max_r;
    int speed;
    uint8_t active;
  };
  struct ripple ripples[MAX_RIPPLES];
  memset(ripples, 0, sizeof(ripples));

  uint16_t hue = 200;

  while (duration_ms == 0 || !time_reached(end)) {
    buttons_update();
    if (buttons_any_event())
      return;

    lcd_fill_screen(COLOR_BLACK);

    radar_data_t rdata = radar_get_data();
    int spawn_chance = 15;
    if (rdata.data_valid && rdata.target_state != RADAR_TARGET_NONE) {
      uint8_t energy = rdata.moving_energy > rdata.stationary_energy ? rdata.moving_energy : rdata.stationary_energy;
      spawn_chance = 15 - (energy / 8);
      if (spawn_chance < 2) spawn_chance = 2;
    }

    if (fast_rand() % spawn_chance == 0) {
      for (int i = 0; i < MAX_RIPPLES; i++) {
        if (!ripples[i].active) {
          ripples[i].x = 15 + (fast_rand() % (LCD_WIDTH - 30));
          ripples[i].y = 15 + (fast_rand() % (LCD_HEIGHT - 30));
          ripples[i].r = 1;
          ripples[i].max_r = 8 + (fast_rand() % 23);
          ripples[i].speed = 1 + (fast_rand() % 2);
          ripples[i].active = 1;
          break;
        }
      }
    }

    for (int i = 0; i < MAX_RIPPLES; i++) {
      if (ripples[i].active) {
        uint8_t alpha = 255 - (ripples[i].r * 255 / ripples[i].max_r);
        uint16_t col = hsv_to_rgb565(hue, 200, alpha);

        lcd_draw_circle(ripples[i].x, ripples[i].y, ripples[i].r, col);
        if (ripples[i].r > 4) {
          lcd_draw_circle(ripples[i].x, ripples[i].y, ripples[i].r - 3, hsv_to_rgb565(hue, 150, alpha / 2));
        }

        ripples[i].r += ripples[i].speed;
        if (ripples[i].r >= ripples[i].max_r) {
          ripples[i].active = 0;
        }
      }
    }

    lcd_flush();
    animation_delay(40);
  }
}
