#ifndef STR_H
#define STR_H

#include "err.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    char *buf;
    size_t len;
    size_t cap;
} strbuf_t;

error_t sb_grow(strbuf_t *sb, size_t extra);
error_t sb_init(strbuf_t *sb);
error_t sb_append_token(strbuf_t *sb, const char *tok);
char *sb_finish(strbuf_t *sb);

bool contains_whitespace(const char *s);
char *hc_strdup(const char *s);
bool ends_with(const char *s, const char *suffix);
void chomp(char *s);
bool parse_number(const char *s, uint64_t *out);
char *strings_join(const char *const *items, size_t n_items, char separator);

#endif /* STR_H */