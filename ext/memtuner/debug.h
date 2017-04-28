#ifndef __DEBUG_H
#define __DEBUG_H

inline static void write_stdout(char const *s) {
    write(1, s, strlen(s));
}

#endif
