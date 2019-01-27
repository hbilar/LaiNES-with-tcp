#pragma once
#include <cstdint>


#define NTH_BIT(x, n) (((x) >> (n)) & 1)

/* Integer type shortcuts */
typedef uint8_t  u8;  typedef int8_t  s8;
typedef uint16_t u16; typedef int16_t s16;
typedef uint32_t u32; typedef int32_t s32;
typedef uint64_t u64; typedef int64_t s64;

// string equals, to get around the horrid == 0 bit for comparisons
#define streq(s1, s2) (strcmp((const char *)s1, (const char*)s2) == 0)
// make a "string starts with" define
#define strstarts(needle, haystack) (strstr(haystack, needle) == haystack)

// get rid of those annoying ISO C++11 string conversion warnings with a wrapper
#define DIE(s) die_with_error_message((char*) s)
extern void die_with_error_message(char *);
