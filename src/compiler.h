#ifndef __COMPILER_H__
#define __COMPILER_H__

#define ALIGNED(x)  __attribute__((aligned(x)))
#define LIKELY(x)   __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!(x), 1)
#define ALWAYS_INLINE __attribute__((always_inline))
#define UNUSED(x) ((void)(x))

#endif
