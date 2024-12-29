#ifndef __MATH2_H__
#define __MATH2_H__

#include <stdint.h>

#define COS_45 0.70710678119f
#define SIN_45 COS_45

typedef struct {
  float x, y;
} vec2f;

typedef struct {
  short x, y;
} vec2i16;

static inline float
absf(float f) {
  uint32_t x = (*(uint32_t*)&f) & ~(1U << 31);
  return *(float*)&x;
}

static inline float
vec2f_dot(vec2f v1, vec2f v2) {
  return v1.x * v2.x + v1.y * v2.x;
}

#endif
