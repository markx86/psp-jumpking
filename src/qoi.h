#ifndef __QOI_H__
#define __QOI_H__

#include <stdint.h>

typedef struct __attribute__((packed)) {
  uint32_t magic;
  uint32_t width;
  uint32_t height;
  uint8_t channels;
  uint8_t colorspace;
} QoiHeader;

typedef union {
  struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
  };
  uint32_t c;
} QoiColor;

typedef struct {
  uint32_t width;
  uint32_t height;
  uint32_t pixelsLeft;
  QoiColor pixel;
  QoiColor colors[64];
  const uint8_t *src;
  QoiColor *dst;
} QoiJobDescriptor;

int qoiInitJob(QoiJobDescriptor *desc, void *src, void *dst);
int qoiDecodeJobWhileVBlank(QoiJobDescriptor *desc);
int qoiDecode(void *src, void *dst, uint32_t *width, uint32_t *height);

#endif
