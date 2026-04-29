#include "util.h"

#include "str.h"

#include <stdio.h>
#include <stdlib.h>

char *read_all_stdin(void) {
    size_t cap = 4096;
    size_t len = 0;
    char *buf = malloc(cap);
    if (!buf) return NULL;

    for (;;) {
        if (len + 1024 >= cap) {
            size_t ncap = cap * 2;
            char *nb = realloc(buf, ncap);
            if (!nb) {
                free(buf);
                return NULL;
            }
            buf = nb;
            cap = ncap;
        }

        size_t n = fread(buf + len, 1, cap - len - 1, stdin);
        len += n;
        buf[len] = '\0';
        if (n == 0) break;
        if (feof(stdin)) break;
        if (ferror(stdin)) break;
    }

    chomp(buf);
    return buf;
}
