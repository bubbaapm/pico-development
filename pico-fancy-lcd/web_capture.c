#include "web_capture.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "lwip/tcp.h"
#include "st7735.h"

#define CAPTURE_PORT 8080
#define BMP_HEADER_SIZE 54
#define FRAME_PIXELS (LCD_WIDTH * LCD_HEIGHT)
#define FRAME_BYTES (FRAME_PIXELS * 3)
#define BMP_BYTES (BMP_HEADER_SIZE + FRAME_BYTES)
#define PIXELS_PER_CHUNK 96

struct capture_state {
  uint32_t offset;
  bool close_after_send;
};

static void bmp_header(uint8_t out[BMP_HEADER_SIZE]) {
  memset(out, 0, BMP_HEADER_SIZE);
  uint32_t file_size = BMP_BYTES;
  uint32_t pixel_offset = BMP_HEADER_SIZE;
  uint32_t dib_size = 40;
  int32_t width = LCD_WIDTH;
  int32_t height = -LCD_HEIGHT;
  uint16_t planes = 1;
  uint16_t bpp = 24;
  uint32_t image_size = FRAME_BYTES;

  out[0] = 'B';
  out[1] = 'M';
  memcpy(&out[2], &file_size, sizeof(file_size));
  memcpy(&out[10], &pixel_offset, sizeof(pixel_offset));
  memcpy(&out[14], &dib_size, sizeof(dib_size));
  memcpy(&out[18], &width, sizeof(width));
  memcpy(&out[22], &height, sizeof(height));
  memcpy(&out[26], &planes, sizeof(planes));
  memcpy(&out[28], &bpp, sizeof(bpp));
  memcpy(&out[34], &image_size, sizeof(image_size));
}

static uint16_t unswap565(uint16_t wire) {
  return (uint16_t)((wire >> 8) | (wire << 8));
}

static void make_pixel_chunk(uint8_t *out, uint32_t first_pixel,
                             uint32_t count) {
  const uint16_t *fb = lcd_get_display_framebuffer();
  for (uint32_t i = 0; i < count; i++) {
    uint16_t c = unswap565(fb[first_pixel + i]);
    uint8_t r = (uint8_t)(((c >> 11) & 0x1F) * 255 / 31);
    uint8_t g = (uint8_t)(((c >> 5) & 0x3F) * 255 / 63);
    uint8_t b = (uint8_t)((c & 0x1F) * 255 / 31);
    out[i * 3 + 0] = b;
    out[i * 3 + 1] = g;
    out[i * 3 + 2] = r;
  }
}

static err_t capture_close(struct tcp_pcb *pcb, struct capture_state *state) {
  tcp_arg(pcb, NULL);
  tcp_sent(pcb, NULL);
  tcp_recv(pcb, NULL);
  tcp_err(pcb, NULL);
  free(state);
  return tcp_close(pcb);
}

static err_t capture_pump(struct tcp_pcb *pcb, struct capture_state *state) {
  static const char response[] =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: image/bmp\r\n"
      "Cache-Control: no-store\r\n"
      "Connection: close\r\n"
      "\r\n";

  while (tcp_sndbuf(pcb) > 512 && state->offset < sizeof(response) - 1) {
    uint32_t remain = (uint32_t)(sizeof(response) - 1) - state->offset;
    uint16_t n =
        remain > tcp_sndbuf(pcb) ? tcp_sndbuf(pcb) : (uint16_t)remain;
    err_t err =
        tcp_write(pcb, response + state->offset, n, TCP_WRITE_FLAG_COPY);
    if (err != ERR_OK) return err;
    state->offset += n;
  }

  if (state->offset >= sizeof(response) - 1 &&
      state->offset < sizeof(response) - 1 + BMP_HEADER_SIZE) {
    uint8_t header[BMP_HEADER_SIZE];
    bmp_header(header);
    uint32_t header_offset = state->offset - ((uint32_t)sizeof(response) - 1);
    uint32_t remain = BMP_HEADER_SIZE - header_offset;
    uint16_t n =
        remain > tcp_sndbuf(pcb) ? tcp_sndbuf(pcb) : (uint16_t)remain;
    if (n > 0) {
      err_t err =
          tcp_write(pcb, header + header_offset, n, TCP_WRITE_FLAG_COPY);
      if (err != ERR_OK) return err;
      state->offset += n;
    }
  }

  while (tcp_sndbuf(pcb) >= PIXELS_PER_CHUNK * 3 &&
         state->offset < sizeof(response) - 1 + BMP_BYTES) {
    uint32_t bmp_offset = state->offset - ((uint32_t)sizeof(response) - 1);
    uint32_t pixel_byte_offset = bmp_offset - BMP_HEADER_SIZE;
    uint32_t first_pixel = pixel_byte_offset / 3;
    uint32_t remain_pixels = FRAME_PIXELS - first_pixel;
    uint32_t count =
        remain_pixels > PIXELS_PER_CHUNK ? PIXELS_PER_CHUNK : remain_pixels;
    uint8_t chunk[PIXELS_PER_CHUNK * 3];
    make_pixel_chunk(chunk, first_pixel, count);
    err_t err =
        tcp_write(pcb, chunk, (uint16_t)(count * 3), TCP_WRITE_FLAG_COPY);
    if (err != ERR_OK) return err;
    state->offset += count * 3;
  }

  tcp_output(pcb);
  if (state->offset >= sizeof(response) - 1 + BMP_BYTES) {
    state->close_after_send = true;
  }
  return ERR_OK;
}

static err_t capture_sent(void *arg, struct tcp_pcb *pcb, u16_t len) {
  (void)len;
  struct capture_state *state = (struct capture_state *)arg;
  if (state->close_after_send) {
    return capture_close(pcb, state);
  }
  err_t err = capture_pump(pcb, state);
  if (err != ERR_OK) {
    return capture_close(pcb, state);
  }
  return ERR_OK;
}

static err_t capture_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p,
                          err_t err) {
  struct capture_state *state = (struct capture_state *)arg;
  if (!p || err != ERR_OK) {
    if (p) pbuf_free(p);
    return capture_close(pcb, state);
  }
  tcp_recved(pcb, p->tot_len);
  pbuf_free(p);
  
  // If we have already started sending the response, ignore any subsequent data from the client
  if (state->offset > 0) {
    return ERR_OK;
  }
  
  err_t pump_err = capture_pump(pcb, state);
  if (pump_err != ERR_OK) {
    return capture_close(pcb, state);
  }
  return ERR_OK;
}

static void capture_err(void *arg, err_t err) {
  (void)err;
  free(arg);
}

static err_t capture_accept(void *arg, struct tcp_pcb *pcb, err_t err) {
  (void)arg;
  if (err != ERR_OK || !pcb) return ERR_VAL;

  struct capture_state *state = calloc(1, sizeof(*state));
  if (!state) {
    tcp_close(pcb);
    return ERR_MEM;
  }

  tcp_arg(pcb, state);
  tcp_sent(pcb, capture_sent);
  tcp_recv(pcb, capture_recv);
  tcp_err(pcb, capture_err);
  return ERR_OK;
}

void web_capture_init(void) {
  struct tcp_pcb *pcb = tcp_new_ip_type(IPADDR_TYPE_ANY);
  if (!pcb) return;
  if (tcp_bind(pcb, IP_ANY_TYPE, CAPTURE_PORT) != ERR_OK) {
    tcp_close(pcb);
    return;
  }
  pcb = tcp_listen(pcb);
  tcp_accept(pcb, capture_accept);
}
