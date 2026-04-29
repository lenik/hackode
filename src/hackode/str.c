#include "str.h"

#include "err.h"

#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

error_t sb_grow(strbuf_t *sb, size_t extra) {
    if (sb->len + extra + 1 <= sb->cap) return HC_OK;
    size_t new_cap = sb->cap ? sb->cap : 256;
    while (new_cap < sb->len + extra + 1) new_cap *= 2;
    char *nb = realloc(sb->buf, new_cap);
    if (!nb) return HC_ERR_OOM;
    sb->buf = nb;
    sb->cap = new_cap;
    return HC_OK;
}

error_t sb_init(strbuf_t *sb) {
    memset(sb, 0, sizeof *sb);
    return HC_OK;
}

error_t sb_append_token(strbuf_t *sb, const char *tok) {
    size_t tlen = strlen(tok);
    if (tlen == 0) return HC_ERR_INVALID;
    int r;

    if (sb->len != 0) {
        r = sb_grow(sb, 1 + tlen);
        if (r != HC_OK) return r;
        sb->buf[sb->len++] = ' ';
    } else {
        r = sb_grow(sb, tlen);
        if (r != HC_OK) return r;
    }

    memcpy(sb->buf + sb->len, tok, tlen);
    sb->len += tlen;
    sb->buf[sb->len] = '\0';
    return HC_OK;
}

char *sb_finish(strbuf_t *sb) {
    if (!sb->buf) return NULL;
    sb->buf[sb->len] = '\0';
    return sb->buf; /* ownership transfer */
}

bool contains_whitespace(const char *s) {
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f')
            return true;
    }
    return false;
}

char *hc_strdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *p = malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n + 1);
    return p;
}

bool ends_with(const char *s, const char *suffix) {
    if (!s || !suffix) return false;
    size_t sl = strlen(s);
    size_t tl = strlen(suffix);
    if (tl > sl) return false;
    return strcmp(s + sl - tl, suffix) == 0;
}

void chomp(char *s) {
    if (!s) return;
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r')) {
        s[n - 1] = '\0';
        n--;
    }
}

bool parse_number(const char *s, uint64_t *out) {
    if (!s || !out) return false;
    const char *p = s;
    while (*p && isspace((unsigned char)*p)) p++;
    if (!*p || *p == '-') return false;
    for (const char *q = p; *q && !isspace((unsigned char)*q); q++) {
        if (!isdigit((unsigned char)*q)) return false;
    }

    errno = 0;
    char *end = NULL;
    unsigned long long v = strtoull(p, &end, 10);
    if (errno != 0 || !end) return false;
    while (*end && isspace((unsigned char)*end)) end++;
    if (*end != '\0') return false;
    *out = (uint64_t)v;
    return true;
}

char *strings_join(const char *const *items, size_t n_items, char separator) {
    if (!items) return NULL;
    size_t total = 0;
    for (size_t i = 0; i < n_items; i++) {
        if (!items[i]) return NULL;
        total += strlen(items[i]);
    }
    if (n_items > 0) total += n_items - 1;

    char *buf = malloc(total + 1);
    if (!buf) return NULL;

    size_t off = 0;
    for (size_t i = 0; i < n_items; i++) {
        size_t len = strlen(items[i]);
        memcpy(buf + off, items[i], len);
        off += len;
        if (i + 1 < n_items) buf[off++] = separator;
    }
    buf[off] = '\0';
    return buf;
}
