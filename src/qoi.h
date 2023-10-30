#ifndef __QOI_H__
#define __QOI_H__

#include <stdint.h>

typedef struct __attribute__((packed)) {
  uint32_t magic;
  uint32_t width;
  uint32_t height;
  uint8_t channels;
  uint8_t colorspace;
} qoi_header_t;

typedef union {
  struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
  };
  uint32_t c;
} qoi_color_t;

typedef struct {
  uint32_t width;
  uint32_t height;
  uint32_t pixels_left;
  qoi_color_t pixel;
  qoi_color_t colors[64];
  const uint8_t *src;
  qoi_color_t *dst;
} qoi_job_descriptor_t;

int qoi_start_job(qoi_job_descriptor_t *desc, void *src, void *dst);
int qoi_lazy_decode(qoi_job_descriptor_t *desc);
int qoi_decode(void *src, void *dst, uint32_t *width, uint32_t *height);

#endif
