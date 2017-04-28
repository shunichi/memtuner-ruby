#ifndef __DEBUG_H
#define __DEBUG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern void memtuner_debug_print(char const *str);
extern void memtuner_debug_print_hex(char const* str, uint64_t n);
extern void memtuner_debug_print_unsigned(char const* str, uint64_t n);
extern void memtuner_debug_print_signed(char const* str, int64_t n);
extern void memtuner_debug_println(char const *str);
extern void memtuner_debug_println_hex(char const* str, uint64_t n);
extern void memtuner_debug_println_unsigned(char const* str, uint64_t n);
extern void memtuner_debug_println_signed(char const* str, int64_t n);

#ifdef __cplusplus
}
#endif
#endif
