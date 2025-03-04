#ifndef __MATH2_H__
#define __MATH2_H__

#include <math.h>
#include <stdint.h>

#define COS_45 0.70710678119f
#define SIN_45 COS_45

#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MIN(x, y) ((x) < (y) ? (x) : (y))

typedef struct {
  float x, y;
} vec2f;

typedef struct {
  short x, y;
} vec2i16;

static inline int
is_float_pos(float f) {
  uint32_t x = (*(uint32_t*)&f) & (1U << 31);
  return !x;
}

static inline int
is_float_zero(float f) {
  return f <= 0.001f && f >= -0.001f;
}

static inline int
clamp(int x, int m, int M) {
  return x < m ? m : x > M ? M : x;
}

static inline float
clampf(float x, float m, float M) {
  return x < m ? m : x > M ? M : x;
}

static inline int
signf(float f) {
  int x = !((*(uint32_t*)&f) & (1U << 31));
  return (x << 1) - 1;
}

static inline float
absf(float f) {
  uint32_t x = (*(uint32_t*)&f) & ~(1U << 31);
  return *(float*)&x;
}

static inline int
abs(int x) {
  return x < 0 ? -x : x;
}

static inline float
vec2f_dot(const vec2f* v1, const vec2f* v2) {
  return v1->x * v2->x + v1->y * v2->y;
}

static inline float
vec2f_len(const vec2f* v) {
  return sqrtf(v->x * v->x + v->y * v->y);
}

static inline vec2f
vec2f_ortho(const vec2f* v) {
  return (vec2f) { .x = v->y, .y = -v->x };
}

static inline uint32_t
to_pow2_size(uint32_t v) {
  --v;
  v |= (v >> 1);
  v |= (v >> 2);
  v |= (v >> 4);
  v |= (v >> 8);
  v |= (v >> 16);
  return ++v;
}

#endif
