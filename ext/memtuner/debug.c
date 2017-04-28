#include <unistd.h>
#include <string.h>
#include "debug.h"

void memtuner_debug_print(char const *str) {
	if (str != NULL && str[0] != '\n')
	    write(1, str, strlen(str));
}

void memtuner_debug_println(char const *str) {
	if (str != NULL && str[0] != '\n')
	    write(1, str, strlen(str));
	write(1, "\n", 1);
}

static size_t uint64_t_to_hex(uint64_t n, char* buf) {
	size_t index = 0;
	int shift = 60;
	int first = 1;

	while (shift >= 0) {
		uint64_t d = (0xf << shift) & n;
		if (!first || d != 0){
			first = 0;
			buf[index++] = d < 10 ? '0' + d : 'A' + (d - 10);
		}
		shift -= 4;
	}
	if (index == 0)
		buf[index++] = '0';
	return index;
}

static size_t uint64_t_to_s(uint64_t n, char* buf) {
	size_t index = 0;
	uint64_t m = 10000000000000000000ULL;
	int first = 1;

	while (m != 0) {
		uint64_t d = n/m;
		if (!first || d != 0){
			first = 0;
			buf[index++] = '0' + d;
		}
		n %= m;
		m /= 10;
	}
	if (index == 0)
		buf[index++] = '0';
	return index;
}

static size_t int64_t_to_s(int64_t n, char* buf) {
	uint64_t nn;
	size_t index = 0;
	if (n < 0)
		buf[index++] = '-';
	if (n == INT64_MIN) {
		nn = (uint64_t)(-(n + 1)) + 1;
	} else if (n < 0) {
		nn = (uint64_t)(-n);
	} else {
		nn = (uint64_t)n;
	}
	index += uint64_t_to_s(nn, buf + index);
	return index;
}

void memtuner_debug_print_hex(char const* str, uint64_t n) {
	char buf[32];
	size_t const len = uint64_t_to_hex(n, buf);
	memtuner_debug_print(str);
	write(1, buf, len);
}

void memtuner_debug_print_unsigned(char const* str, uint64_t n) {
	char buf[32];
	size_t const len = uint64_t_to_s(n, buf);
	memtuner_debug_print(str);
	write(1, buf, len);
}

void memtuner_debug_print_signed(char const* str, int64_t n) {
	char buf[32];
	size_t const len = int64_t_to_s(n, buf);
	memtuner_debug_print(str);
	write(1, buf, len);
}

void memtuner_debug_println_hex(char const* str, uint64_t n) {
	char buf[32];
	size_t len = uint64_t_to_hex(n, buf);
	buf[len++] = '\n';
	memtuner_debug_print(str);
	write(1, buf, len);
}

void memtuner_debug_println_unsigned(char const* str, uint64_t n) {
	char buf[32];
	size_t len = uint64_t_to_s(n, buf);
	buf[len++] = '\n';
	memtuner_debug_print(str);
	write(1, buf, len);
}

void memtuner_debug_println_signed(char const* str, int64_t n) {
	char buf[32];
	size_t len = int64_t_to_s(n, buf);
	buf[len++] = '\n';
	memtuner_debug_print(str);
	write(1, buf, len);
}
