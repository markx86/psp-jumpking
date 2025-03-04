#ifndef __COMPILER_H__
#define __COMPILER_H__

#define UNUSED(x) ((void)(x))

// NOTE: This macros are probably not needed, since MIPS has delay slots, but
//       I'd rather be safe than sorry :^)
#define LIKELY(x)   __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)

#define ALIGNED(x)  __attribute__((aligned(x)))
#define ALWAYS_INLINE inline __attribute__((always_inline))
#define PACKED        __attribute__((packed))

#endif
