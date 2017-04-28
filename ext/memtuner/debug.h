#ifndef __DEBUG_H
#define __DEBUG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern void debug_print(char const *s);
extern void debug_print_hex(uint64_t n);
extern void debug_print_unsigned(uint64_t n);
extern void debug_print_signed(int64_t n);

#ifdef __cplusplus
}
#endif
#endif
