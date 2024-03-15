#include "qoi.h"
#include "state.h"
#include <pspdisplay.h>
#include <string.h>

#define QOI_MAGIC 0x66696F71

#define ISOP_INDEX(t) (((t)&0xC0) == 0x00)
#define ISOP_DIFF(t) (((t)&0xC0) == 0x40)
#define ISOP_LUMA(t) (((t)&0xC0) == 0x80)
#define ISOP_RUN(t) (((t)&0xC0) == 0xC0)

static inline __attribute__((always_inline)) uint32_t bswap32(uint32_t n) {
  uint32_t b;
  b = (n & 0xFF) << 24;
  b |= (n & 0xFF00) << 8;
  b |= (n & 0xFF0000) >> 8;
  b |= (n & 0xFF000000) >> 24;
  return b;
}

static inline __attribute__((always_inline)) uint32_t hash(qoi_color_t* px) {
  return (
            (px->r << 1) + px->r +
            (px->g << 2) + px->g +
            (px->b << 3) - px->b +
            (px->a << 4) - (px->a << 2) - px->a
        ) & 63;
}

int qoi_start_job(qoi_job_descriptor_t* desc, void* src, void* dst) {
  qoi_header_t* hdr = (qoi_header_t*)src;
  if (hdr->magic != QOI_MAGIC || hdr->channels != 4) {
    return -1;
  }
  desc->src = (const uint8_t*)(hdr + 1);
  desc->dst = (qoi_color_t*)dst;
  memset(desc->colors, 0, sizeof(desc->colors));
  desc->width = bswap32(hdr->width);
  desc->height = bswap32(hdr->height);
  desc->pixels_left = desc->width * desc->height;
  desc->pixel = (qoi_color_t){.r = 0, .g = 0, .b = 0, .a = 255};
  return 0;
}

int qoi_lazy_decode(qoi_job_descriptor_t* d) {
  qoi_color_t* px = &d->pixel;
  while (d->pixels_left > 0 &&
         sceDisplayGetCurrentHcount() < PSP_SCREEN_HEIGHT) {
    uint8_t b = *(d->src++);
    if (ISOP_INDEX(b)) {
      px->c = d->colors[b & 0x3F].c;
    } else if (ISOP_RUN(b)) {
      uint8_t run = (b & 0x3F);
      if (run < 62) {
        d->pixels_left -= ++run;
        for (; run > 0; --run) {
          (d->dst++)->c = px->c;
        }
        continue;
      } else {
        int is_rgba = run & 1;
        px->r = *(d->src++);
        px->g = *(d->src++);
        px->b = *(d->src++);
        px->a = is_rgba ? *(d->src++) : px->a;
      }
    } else if (ISOP_LUMA(b)) {
      int8_t dg = (b & 0x3F) - 32;
      b = *(d->src++);
      px->b += (b & 0xF) + dg - 8;
      px->r += ((b >> 4) & 0xF) + dg - 8;
      px->g += dg;
    } else if (ISOP_DIFF(b)) {
      px->b += (b & 0x3) - 2;
      b >>= 2;
      px->g += (b & 0x3) - 2;
      b >>= 2;
      px->r += (b & 0x3) - 2;
    }
    (d->dst++)->c = px->c;
    d->colors[hash(px)].c = px->c;
    --d->pixels_left;
  }
  return d->pixels_left > 0;
}

int qoi_decode(
    void* in_src,
    void* in_dst,
    uint32_t* out_width,
    uint32_t* out_height) {
  if (in_src == NULL) {
    return -1;
  }

  qoi_header_t* hdr = in_src;
  if (hdr->magic != QOI_MAGIC || hdr->channels != 4) {
    return -1;
  }

  int width = bswap32(hdr->width);
  if (out_width != NULL) {
    *out_width = width;
  }
  int height = bswap32(hdr->height);
  if (out_height != NULL) {
    *out_height = height;
  }

  if (in_dst == NULL) {
    return -1;
  }

  const uint8_t* src = (uint8_t*)(hdr + 1);
  qoi_color_t* dst = in_dst;
  uint32_t pixels_left = width * height;
  qoi_color_t px = (qoi_color_t){.r = 0, .g = 0, .b = 0, .a = 255};
  qoi_color_t colors[64];
  memset(colors, 0, sizeof(colors));

  while (pixels_left > 0) {
    uint8_t b = *(src++);
    if (ISOP_INDEX(b)) {
      px.c = colors[b & 0x3F].c;
    } else if (ISOP_RUN(b)) {
      uint8_t run = (b & 0x3F);
      if (run < 62) {
        pixels_left -= ++run;
        for (; run > 0; --run) {
          (dst++)->c = px.c;
        }
        continue;
      } else {
        int is_rgba = run & 1;
        px.r = *(src++);
        px.g = *(src++);
        px.b = *(src++);
        px.a = is_rgba ? *(src++) : px.a;
      }
    } else if (ISOP_LUMA(b)) {
      int8_t dg = (b & 0x3F) - 32;
      b = *(src++);
      px.b += (b & 0xF) + dg - 8;
      px.r += ((b >> 4) & 0xF) + dg - 8;
      px.g += dg;
    } else if (ISOP_DIFF(b)) {
      px.b += (b & 0x3) - 2;
      b >>= 2;
      px.g += (b & 0x3) - 2;
      b >>= 2;
      px.r += (b & 0x3) - 2;
    }
    (dst++)->c = px.c;
    colors[hash(&px)].c = px.c;
    --pixels_left;
  }
  return 0;
}
