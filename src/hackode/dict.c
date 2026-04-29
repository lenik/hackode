/*
 * Copyright (C) 2026 Lenik <hackode@bodz.net>
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#define _POSIX_C_SOURCE 200809L

#include "dict.h"

#include <sys/mman.h>
#include <sys/stat.h>

#include <ctype.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Some libcs may hide mkstemp() declaration; add it explicitly for C99. */
int mkstemp(char *template);

struct dict {
    uint32_t magic_version;
    uint32_t n_words;
    uint32_t blob_bytes;
    uint32_t *offsets;
    char *blob;
    void *map_base;
    size_t map_len;
    int heap_backed;
};

enum {
    DICT_MAGIC = 0x48434449u,
    DICT_MAP_VER = 1,
};

typedef struct dict_map_header {
    uint32_t magic;
    uint32_t version;
    uint32_t n_words;
    uint32_t blob_bytes;
    uint32_t reserved;
} dict_map_header_t;

static bool word_valid(const char *w) {
    if (!w || w[0] == '\0') return false;
    if (strcmp(w, "|") == 0) return false;
    for (; *w; w++) {
        if (isspace((unsigned char)*w)) return false;
    }
    return true;
}

static bool path_ends_with(const char *s, const char *suffix) {
    if (!s || !suffix) return false;
    size_t sl = strlen(s);
    size_t tl = strlen(suffix);
    if (tl > sl) return false;
    return strcmp(s + sl - tl, suffix) == 0;
}

static void dict_free_heap(struct dict *d) {
    if (!d) return;
    free(d->offsets);
    free(d->blob);
    free(d);
}

dict_t *dict_create_from_strings(const char *const *words, size_t n_words, int *err_out) {
    if (err_out) *err_out = HC_OK;
    if (!words || n_words <= 10 || n_words > UINT32_MAX) {
        if (err_out) *err_out = HC_ERR_NWORDS;
        return NULL;
    }

    struct dict *d = calloc(1, sizeof *d);
    if (!d) {
        if (err_out) *err_out = HC_ERR_OOM;
        return NULL;
    }

    d->n_words = (uint32_t)n_words;
    size_t blob_bytes = 0;
    for (size_t i = 0; i < n_words; i++) {
        const char *w = words[i];
        if (!word_valid(w)) {
            if (err_out) *err_out = HC_ERR_DICT;
            dict_free_heap(d);
            return NULL;
        }
        blob_bytes += strlen(w) + 1;
        if (blob_bytes > UINT32_MAX) {
            if (err_out) *err_out = HC_ERR_RANGE;
            dict_free_heap(d);
            return NULL;
        }
    }
    d->blob_bytes = (uint32_t)blob_bytes;

    d->offsets = calloc(n_words, sizeof(uint32_t));
    d->blob = malloc(d->blob_bytes);
    if (!d->offsets || !d->blob) {
        if (err_out) *err_out = HC_ERR_OOM;
        dict_free_heap(d);
        return NULL;
    }

    uint32_t off = 0;
    for (size_t i = 0; i < n_words; i++) {
        d->offsets[i] = off;
        size_t len = strlen(words[i]);
        memcpy(d->blob + off, words[i], len);
        d->blob[off + len] = '\0';
        off += (uint32_t)(len + 1);
    }

    d->magic_version = (DICT_MAGIC << 16) | DICT_MAP_VER;
    d->heap_backed = 1;
    return (dict_t *)d;
}

error_t dict_compile_text_to_map(const char *text_path, const char *map_path, int *err_out) {
    if (err_out) *err_out = HC_OK;
    if (!text_path || !map_path) return -1;

    char **words = NULL;
    size_t cap = 0, n = 0;
    FILE *f = NULL, *out = NULL;
    char *line = NULL;
    size_t linecap = 0;
    uint32_t *offsets = NULL;
    char *blob = NULL;
    size_t blob_bytes = 0;
    int rc = -1;

    do {
        f = fopen(text_path, "r");
        if (!f) break;
        cap = 64;
        words = calloc(cap, sizeof(char *));
        linecap = 4096;
        line = malloc(linecap);
        if (!words || !line) break;

        while (fgets(line, linecap, f)) {
            size_t len = strlen(line);
            while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) line[--len] = '\0';
            char *p = line;
            while (*p && isspace((unsigned char)*p)) p++;
            if (*p == '\0' || *p == '#') continue;
            char *end = p + strlen(p);
            while (end > p && isspace((unsigned char)end[-1])) end--;
            *end = '\0';
            if (!word_valid(p)) break;
            if (n >= cap) {
                size_t ncap = cap * 2;
                char **nw = realloc(words, ncap * sizeof(char *));
                if (!nw) break;
                words = nw;
                cap = ncap;
            }
            words[n] = malloc(strlen(p) + 1);
            if (!words[n]) break;
            strcpy(words[n], p);
            n++;
        }
        if (ferror(f) || n <= 10) break;

        for (size_t i = 0; i < n; i++) {
            blob_bytes += strlen(words[i]) + 1;
            if (blob_bytes > UINT32_MAX) break;
        }
        if (blob_bytes > UINT32_MAX) break;

        offsets = calloc(n, sizeof(uint32_t));
        blob = malloc(blob_bytes);
        if (!offsets || !blob) break;
        uint32_t off = 0;
        for (size_t i = 0; i < n; i++) {
            offsets[i] = off;
            size_t len = strlen(words[i]);
            memcpy(blob + off, words[i], len);
            blob[off + len] = '\0';
            off += (uint32_t)(len + 1);
        }

        out = fopen(map_path, "wb");
        if (!out) break;
        dict_map_header_t hdr = {DICT_MAGIC, DICT_MAP_VER, (uint32_t)n, (uint32_t)blob_bytes, 0};
        if (fwrite(&hdr, 1, sizeof hdr, out) != sizeof hdr) break;
        if (fwrite(offsets, sizeof(uint32_t), n, out) != n) break;
        if (fwrite(blob, 1, blob_bytes, out) != blob_bytes) break;
        rc = 0;
    } while (0);

    if (out) fclose(out);
    if (f) fclose(f);
    if (blob) free(blob);
    if (offsets) free(offsets);
    if (line) free(line);
    if (words) {
        for (size_t i = 0; i < n; i++) free(words[i]);
        free(words);
    }
    if (rc != 0 && err_out && *err_out == HC_OK) *err_out = HC_ERR_INVALID;
    return rc;
}

error_t dict_load_map(const char *map_path, dict_t **out_dict, int *err_out) {
    if (out_dict) *out_dict = NULL;
    if (err_out) *err_out = HC_OK;
    if (!map_path || !out_dict) return -1;

    int fd = -1, rc = -1;
    void *map_base = NULL;
    size_t map_len = 0;
    struct stat st;
    struct dict *d = NULL;

    do {
        fd = open(map_path, O_RDONLY);
        if (fd < 0) break;
        if (fstat(fd, &st) != 0) break;
        if (st.st_size < (off_t)sizeof(dict_map_header_t)) break;
        map_len = (size_t)st.st_size;
        map_base = mmap(NULL, map_len, PROT_READ, MAP_PRIVATE, fd, 0);
        if (map_base == MAP_FAILED) {
            map_base = NULL;
            break;
        }
        dict_map_header_t hdr;
        memcpy(&hdr, map_base, sizeof hdr);
        if (hdr.magic != DICT_MAGIC || hdr.version != DICT_MAP_VER || hdr.n_words <= 10) break;
        size_t off_bytes = sizeof(dict_map_header_t) + (size_t)hdr.n_words * sizeof(uint32_t);
        if (off_bytes > map_len || off_bytes + hdr.blob_bytes > map_len) break;

        d = calloc(1, sizeof *d);
        if (!d) break;
        d->magic_version = (hdr.magic << 16) | hdr.version;
        d->n_words = hdr.n_words;
        d->blob_bytes = hdr.blob_bytes;
        d->offsets = (uint32_t *)((char *)map_base + sizeof(dict_map_header_t));
        d->blob = (char *)map_base + off_bytes;
        d->map_base = map_base;
        d->map_len = map_len;
        d->heap_backed = 0;
        *out_dict = (dict_t *)d;
        rc = 0;
    } while (0);

    if (fd >= 0) close(fd);
    if (rc != 0) {
        if (d) free(d);
        if (map_base) munmap(map_base, map_len);
        if (err_out && *err_out == HC_OK) *err_out = HC_ERR_INVALID;
    }
    return rc;
}

error_t dict_load_auto(const char *dict_path, dict_t **out_dict, int *err_out) {
    if (out_dict) *out_dict = NULL;
    if (err_out) *err_out = HC_OK;
    if (!dict_path || !out_dict) return -1;

    if (path_ends_with(dict_path, ".map")) {
        return dict_load_map(dict_path, out_dict, err_out);
    }

    char tmp_template[] = "/tmp/hackode-dictXXXXXX";
    int fd = mkstemp(tmp_template);
    if (fd < 0) {
        if (err_out) *err_out = HC_ERR_INVALID;
        return -1;
    }
    close(fd);

    error_t rc = dict_compile_text_to_map(dict_path, tmp_template, err_out);
    if (rc != HC_OK) {
        unlink(tmp_template);
        return rc;
    }

    rc = dict_load_map(tmp_template, out_dict, err_out);
    unlink(tmp_template);
    return rc;
}

const char *dict_word(const dict_t *dict, size_t idx) {
    const struct dict *d = (const struct dict *)dict;
    if (!d || idx >= d->n_words) return NULL;
    uint32_t off = d->offsets[idx];
    if (off >= d->blob_bytes) return NULL;
    return d->blob + off;
}

size_t dict_n_words(const dict_t *dict) {
    const struct dict *d = (const struct dict *)dict;
    if (!d) return 0;
    return (size_t)d->n_words;
}

void dict_destroy(dict_t *dict) {
    if (!dict) return;
    struct dict *d = (struct dict *)dict;
    if (d->map_base) munmap(d->map_base, d->map_len);
    if (d->heap_backed) {
        free(d->offsets);
        free(d->blob);
    }
    free(d);
}

