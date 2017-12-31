#ifndef __TYPES_BASE_H
#define __TYPES_BASE_H

#ifndef uint_64
#define uint_64 uint64_t
#define int_64 int64_t
#endif

#ifndef uint_32
#define uint_32 uint32_t
#define int_32 int32_t
#endif

#ifndef LONG64SPRINTF
#define LONG64SPRINTF uint_64
#endif

#if defined(__GNUC__)
#ifndef likely
#define likely(expr) __builtin_expect(!!(expr), 1)
#else
#define likely(expr)
#endif
#endif

#if defined(__GNUC__)
#ifndef unlikely
#define unlikely(expr) __builtin_expect(!!(expr), 0)
#else
#define unlikely(expr)
#endif
#endif

#endif