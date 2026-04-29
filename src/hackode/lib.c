/*
 * Copyright (C) 2026 Lenik <hackode@bodz.net>
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#define _POSIX_C_SOURCE 200809L

#include "lib.h"

#include "imath.h"
#include "str.h"

#include <bas/locale/i18n.h>
#include <bas/log/deflog.h>

#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

__attribute__((weak))
define_logger();

static error_t sb_append_with_sep(strbuf_t *sb, const char *tok, char separator) {
    size_t tlen = strlen(tok);
    if (tlen == 0) return HC_ERR_INVALID;
    error_t r;
    if (sb->len != 0) {
        r = sb_grow(sb, 1 + tlen);
        if (r != HC_OK) return r;
        sb->buf[sb->len++] = separator;
    } else {
        r = sb_grow(sb, tlen);
        if (r != HC_OK) return r;
    }
    memcpy(sb->buf + sb->len, tok, tlen);
    sb->len += tlen;
    sb->buf[sb->len] = '\0';
    return HC_OK;
}

int copy_stream(FILE *in, FILE *out) {
    char buf[8192];
    size_t n;

    while ((n = fread(buf, 1, sizeof buf, in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) {
            return -1;
        }
    }
    if (ferror(in)) {
        return -1;
    }
    return 0;
}

int copy_file(const char *prog, const char *path) {
    loginfo_fmt(_("%s: copying from %s"), prog, path);

    FILE *f = fopen(path, "rb");
    if (!f) {
        logerror_fmt(_("%s: %s: %s"), prog, path, strerror(errno));
        return -1;
    }

    int r = copy_stream(f, stdout);
    if (fclose(f) != 0) {
        r = -1;
    }
    if (r != 0) {
        logerror_fmt(_("%s: write error"), prog);
        return -1;
    }
    return 0;
}

/* ---------------- Hacker cipher (dictionary-based) ---------------- */

static hacker_cipher_t *hc_create_with_dict(dict_t *dict, size_t chunk_size, int *err_out) {
    if (err_out) *err_out = HC_OK;
    if (!dict) {
        if (err_out) *err_out = HC_ERR_INVALID;
        return NULL;
    }
    if (chunk_size < 1 || chunk_size > 8) {
        if (err_out) *err_out = HC_ERR_CHUNK;
        return NULL;
    }

    hacker_cipher_t *hc = calloc(1, sizeof *hc);
    if (!hc) {
        if (err_out) *err_out = HC_ERR_OOM;
        return NULL;
    }

    hc->dict = dict;
    hc->n_words = dict_n_words(dict);
    hc->chunk_size = chunk_size;
    hc->separator = ' ';

    mod_t N = (mod_t)hc->n_words;
    mod_t start = isqrt_chunk(N) + 1;
    if (start <= 2) start = 3;

    mod_t p = start;
    for (mod_t tries = 0; tries < (1ULL << 32); tries++, p++) {
        if (p > 2 && gcd_chunk(p, N) == 1) break;
        if (p == UINT64_MAX) break;
    }
    if (gcd_chunk(p, N) != 1) {
        if (err_out) *err_out = HC_ERR_MATH;
        goto fail;
    }
    hc->p = p % N; /* keep p in mod_t domain: 0 <= p < N */

    error_t inv_err = HC_OK;
    hc->q = mod_inverse(hc->p, N, &inv_err); /* q is also a mod_t in Z_N */
    if (inv_err != HC_OK) {
        if (err_out) *err_out = inv_err;
        goto fail;
    }

    error_t r = ht_init(hc);
    if (r != HC_OK) {
        if (err_out) *err_out = r;
        goto fail;
    }

    return hc;

fail:
    hc_destroy(hc);
    return NULL;
}

hacker_cipher_t *hc_create(const char *const *dictionary, size_t n_words, size_t chunk_size, int *err_out) {
    if (err_out) *err_out = HC_OK;
    if (!dictionary) {
        if (err_out) *err_out = HC_ERR_INVALID;
        return NULL;
    }

    int dict_err = HC_OK;
    dict_t *dict = dict_create_from_strings(dictionary, n_words, &dict_err);
    if (!dict) {
        if (err_out) *err_out = dict_err;
        return NULL;
    }
    return hc_create_with_dict(dict, chunk_size, err_out);
}

hacker_cipher_t *hc_create_from_dictmap(const char *map_path, size_t chunk_size, int *err_out) {
    if (err_out) *err_out = HC_OK;
    if (!map_path) {
        if (err_out) *err_out = HC_ERR_INVALID;
        return NULL;
    }

    int dict_err = HC_OK;
    dict_t *dict = NULL;
    if (dict_load_map(map_path, &dict, &dict_err) != 0 || !dict) {
        if (err_out) *err_out = dict_err;
        return NULL;
    }
    return hc_create_with_dict(dict, chunk_size, err_out);
}

hacker_cipher_t *hc_create_from_dictsource(const char *dict_path, size_t chunk_size, int *err_out) {
    if (err_out) *err_out = HC_OK;
    if (!dict_path) {
        if (err_out) *err_out = HC_ERR_INVALID;
        return NULL;
    }

    int dict_err = HC_OK;
    dict_t *dict = NULL;
    if (dict_load_auto(dict_path, &dict, &dict_err) != HC_OK || !dict) {
        if (err_out) *err_out = dict_err;
        return NULL;
    }
    return hc_create_with_dict(dict, chunk_size, err_out);
}

void hc_destroy(hacker_cipher_t *hc) {
    if (!hc) return;
    if (hc->dict) dict_destroy(hc->dict);
    free(hc->ht_keys);
    free(hc->ht_vals);
    free(hc);
}

error_t ht_init(hacker_cipher_t *hc) {
    size_t cap = next_pow2(hc->n_words * 2 + 1);
    hc->ht_cap = cap;
    hc->ht_keys = calloc(cap, sizeof(char *));
    hc->ht_vals = calloc(cap, sizeof(size_t));
    if (!hc->ht_keys || !hc->ht_vals) return HC_ERR_OOM;

    for (size_t i = 0; i < hc->n_words; i++) {
        const char *key = dict_word(hc->dict, i);
        if (!key) return HC_ERR_DICT;
        if (strcmp(key, "|") == 0) return HC_ERR_DICT;
        if (contains_whitespace(key)) return HC_ERR_DICT;

        uint64_t h = fnv1a_hash(key);
        size_t pos = (size_t)(h & (cap - 1));
        for (;;) {
            if (hc->ht_keys[pos] == NULL) {
                hc->ht_keys[pos] = key;
                hc->ht_vals[pos] = i;
                break;
            }
            if (strcmp(hc->ht_keys[pos], key) == 0) {
                return HC_ERR_DICT; /* duplicate */
            }
            pos = (pos + 1) & (cap - 1);
        }
    }
    return HC_OK;
}

mod_t hc_forward(hacker_cipher_t *hc, mod_t div) {
    return (div * hc->p) % (mod_t)hc->n_words;
}

mod_t hc_inverse(hacker_cipher_t *hc, mod_t div) {
    return (div * hc->q) % (mod_t)hc->n_words;
}

error_t hc_lookup(hacker_cipher_t *hc, const char *word, mod_t *out_div) {
    size_t idx = 0;
    error_t r = ht_lookup(hc, word, &idx);
    if (r != HC_OK) return r;
    *out_div = hc_inverse(hc, (mod_t)idx);
    return HC_OK;
}

const char *hc_word(hacker_cipher_t *hc, mod_t div) {
    mod_t idx = hc_forward(hc, div);
    return dict_word(hc->dict, (size_t)idx);
}

void hc_set_separator(hacker_cipher_t *hc, char separator) {
    if (!hc) return;
    hc->separator = separator ? separator : ' ';
}

char hc_get_separator(hacker_cipher_t *hc) {
    if (!hc) return ' ';
    return hc->separator ? hc->separator : ' ';
}

error_t hc_encrypt_chunk_to_words(hacker_cipher_t *hc, chunk_t chunk, char **out_words_message) {
    if (out_words_message) *out_words_message = NULL;
    if (!hc || !out_words_message) return HC_ERR_INVALID;

    mod_t N = (mod_t)hc->n_words;
    if (N < 2) return HC_ERR_INVALID;

    mod_t divs[128];
    size_t cnt = 0;
    if (chunk == 0) {
        divs[cnt++] = 0;
    } else {
        while (chunk > 0) {
            if (cnt >= sizeof(divs) / sizeof(divs[0])) return HC_ERR_RANGE;
            divs[cnt++] = (mod_t)(chunk % (chunk_t)N);
            chunk /= (chunk_t)N;
        }
    }

    strbuf_t sb;
    error_t r = sb_init(&sb);
    if (r != HC_OK) return r;

    for (size_t i = cnt; i > 0; i--) {
        mod_t div = divs[i - 1];
        const char *w = hc_word(hc, div);
        if (!w) {
            free(sb.buf);
            return HC_ERR_DICT;
        }
        r = sb_append_with_sep(&sb, w, hc_get_separator(hc));
        if (r != HC_OK) {
            free(sb.buf);
            return r;
        }
    }

    *out_words_message = sb_finish(&sb);
    return HC_OK;
}

error_t hc_encrypt_chunk(hacker_cipher_t *hc, chunk_t chunk, mod_t **out_divs, size_t *out_n_divs) {
    if (out_divs) *out_divs = NULL;
    if (out_n_divs) *out_n_divs = 0;
    if (!hc || !out_divs || !out_n_divs) return HC_ERR_INVALID;

    mod_t N = (mod_t)hc->n_words;
    if (N < 2) return HC_ERR_INVALID;

    mod_t divs[128];
    size_t cnt = 0;
    if (chunk == 0) {
        divs[cnt++] = 0;
    } else {
        while (chunk > 0) {
            if (cnt >= sizeof(divs) / sizeof(divs[0])) return HC_ERR_RANGE;
            divs[cnt++] = (mod_t)(chunk % (chunk_t)N);
            chunk /= (chunk_t)N;
        }
    }

    mod_t *enc_divs = malloc(cnt * sizeof(mod_t));
    if (!enc_divs) return HC_ERR_OOM;

    for (size_t i = cnt; i > 0; i--) {
        mod_t div = divs[i - 1];
        enc_divs[cnt - i] = hc_forward(hc, div);
    }

    *out_divs = enc_divs;
    *out_n_divs = cnt;
    return HC_OK;
}

error_t hc_decrypt_divs_to_chunk(hacker_cipher_t *hc, const mod_t *divs, size_t n_divs, chunk_t *out_chunk) {
    if (!hc || !divs || !out_chunk || n_divs == 0) return HC_ERR_INVALID;
    *out_chunk = 0;

    mod_t N = (mod_t)hc->n_words;
    chunk_t out = 0;

    for (size_t i = 0; i < n_divs; i++) {
        mod_t enc_div = divs[i];
        if (enc_div >= N) {
            return HC_ERR_RANGE;
        }
        mod_t div = hc_inverse(hc, enc_div);
        unsigned long long next;
        if (__builtin_mul_overflow((unsigned long long)out, (unsigned long long)N, &next) ||
            __builtin_add_overflow(next, (unsigned long long)div, &next)) {
            return HC_ERR_RANGE;
        }
        out = (chunk_t)next;
    }
    *out_chunk = out;
    return HC_OK;
}

error_t ht_lookup(hacker_cipher_t *hc, const char *key, size_t *out_idx) {
    if (!key || !out_idx) return HC_ERR_INVALID;
    uint64_t h = fnv1a_hash(key);
    size_t pos = (size_t)(h & (hc->ht_cap - 1));
    for (;;) {
        const char *k = hc->ht_keys[pos];
        if (k == NULL) return HC_ERR_NOTFOUND;
        if (strcmp(k, key) == 0) {
            *out_idx = hc->ht_vals[pos];
            return HC_OK;
        }
        pos = (pos + 1) & (hc->ht_cap - 1);
    }
}

/**
 * Append a number to a string buffer as a sequence of dictionary words.
 *
 * Parameters:
 *   - hc: cipher context.
 *   - sb: string buffer.
 *   - num: number to append.
 *
 * Returns: HC_OK on success, negative error code on failure.
 */
static error_t append_num_divs_words(hacker_cipher_t *hc, strbuf_t *sb, uint64_t num) {
    if (hc->n_words < 2) return HC_ERR_INVALID;

    if (num == 0) {
        mod_t wd = hc_forward(hc, 0);
        return sb_append_with_sep(sb, dict_word(hc->dict, wd), hc->separator);
    }

    mod_t divs[128];
    size_t cnt = 0;
    while (num > 0) {
        if (cnt >= sizeof(divs) / sizeof(divs[0])) return HC_ERR_RANGE;
        divs[cnt++] = (mod_t)(num % (chunk_t)hc->n_words);
        num /= (chunk_t)hc->n_words;
    }

    /* High div first. */
    for (size_t i = cnt; i > 0; i--) {
        mod_t div = divs[i - 1];
        mod_t wd = hc_forward(hc, div);
        const char *w = dict_word(hc->dict, wd);
        if (!w) return HC_ERR_DICT;
        error_t r = sb_append_with_sep(sb, w, hc->separator);
        if (r != HC_OK) return r;
    }
    return HC_OK;
}

static error_t words_to_num(hacker_cipher_t *hc, char **words, size_t n_words, uint64_t *out_num) {
    if (!hc || !words || !out_num) return HC_ERR_INVALID;
    uint64_t N = (uint64_t)hc->n_words;
    uint64_t n = 0;

    for (size_t i = 0; i < n_words; i++) {
        size_t idx = 0;
        error_t r = ht_lookup(hc, words[i], &idx);
        if (r != HC_OK) return r;

        mod_t div = hc_inverse(hc, (mod_t)idx);

        unsigned long long tmp;
        if (__builtin_mul_overflow((unsigned long long)n, (unsigned long long)N, &tmp)) return HC_ERR_RANGE;
        if (__builtin_add_overflow(tmp, (unsigned long long)div, &tmp)) return HC_ERR_RANGE;
        n = (uint64_t)tmp;
    }

    *out_num = n;
    return HC_OK;
}

static size_t div_count_for_max_u64(uint64_t max_value, uint64_t base) {
    size_t cnt = 1;
    while (max_value >= base) {
        max_value /= base;
        cnt++;
    }
    return cnt;
}

static error_t append_chunk_var_prefix(hacker_cipher_t *hc, strbuf_t *sb, uint64_t chunk) {
    if (!hc || !sb) return HC_ERR_INVALID;
    if (hc->n_words < 3) return HC_ERR_INVALID;

    uint64_t N = (uint64_t)hc->n_words;
    uint64_t N_half = (N - 1) / 2;
    if (N_half == 0) return HC_ERR_INVALID;

    mod_t raw_divs[128];
    size_t cnt = 0;

    do {
        if (cnt >= sizeof(raw_divs) / sizeof(raw_divs[0])) return HC_ERR_RANGE;
        uint64_t d = chunk % N_half;
        chunk /= N_half;
        raw_divs[cnt++] = (mod_t)d;
    } while (chunk);

    /* Emit MSB->LSB; only the final emitted token is marked as last (even). */
    for (size_t i = cnt; i > 0; i--) {
        uint64_t payload = (uint64_t)raw_divs[i - 1];
        mod_t div = (i == 1) ? (mod_t)(payload * 2) : (mod_t)(payload * 2 + 1);
        const char *w = hc_word(hc, div);
        if (!w) return HC_ERR_DICT;
        error_t r = sb_append_with_sep(sb, w, hc->separator);
        if (r != HC_OK) return r;
    }
    return HC_OK;
}

static error_t decrypt_chunk_var_prefix(hacker_cipher_t *hc, char **words, size_t n_words, uint64_t *out_chunk,
                                        size_t *out_consumed) {
    if (!hc || !words || !out_chunk || !out_consumed) return HC_ERR_INVALID;
    if (hc->n_words < 3) return HC_ERR_INVALID;

    uint64_t N = (uint64_t)hc->n_words;
    uint64_t N_half = (N - 1) / 2;
    if (N_half == 0) return HC_ERR_INVALID;

    uint64_t chunk = 0;
    for (size_t i = 0; i < n_words; i++) {
        size_t idx = 0;
        error_t r = ht_lookup(hc, words[i], &idx);
        if (r != HC_OK) return r;

        mod_t div = hc_inverse(hc, (mod_t)idx);
        uint64_t v = (uint64_t)div;
        if (v >= 2 * N_half) return HC_ERR_PARSE;

        uint64_t payload = v / 2;
        unsigned long long next = 0;
        if (__builtin_mul_overflow((unsigned long long)chunk, (unsigned long long)N_half, &next) ||
            __builtin_add_overflow(next, (unsigned long long)payload, &next)) {
            return HC_ERR_RANGE;
        }
        chunk = (uint64_t)next;

        if ((v % 2) == 0) {
            *out_chunk = chunk;
            *out_consumed = i + 1;
            return HC_OK;
        }
    }

    return HC_ERR_PARSE;
}

static error_t append_num_divs_words_fixed(hacker_cipher_t *hc, strbuf_t *sb, uint64_t num, size_t fixed_cnt) {
    if (!hc || !sb || fixed_cnt == 0) return HC_ERR_INVALID;
    if (hc->n_words < 2) return HC_ERR_INVALID;

    uint64_t N = (uint64_t)hc->n_words;
    mod_t divs[128];
    if (fixed_cnt > sizeof(divs) / sizeof(divs[0])) return HC_ERR_RANGE;

    for (size_t i = 0; i < fixed_cnt; i++) {
        divs[fixed_cnt - 1 - i] = (mod_t)(num % N);
        num /= N;
    }
    if (num != 0) return HC_ERR_RANGE;

    for (size_t i = 0; i < fixed_cnt; i++) {
        const char *w = hc_word(hc, divs[i]);
        if (!w) return HC_ERR_DICT;
        error_t r = sb_append_with_sep(sb, w, hc->separator);
        if (r != HC_OK) return r;
    }
    return HC_OK;
}

static error_t bytes_to_chunk_be(const unsigned char *bytes, size_t len, chunk_t *out) {
    if (!bytes || !out) return HC_ERR_INVALID;
    uint64_t v = 0;
    for (size_t i = 0; i < len; i++) v = (v << 8) | (uint64_t)bytes[i];
    *out = v;
    return HC_OK;
}

static error_t chunk_to_bytes_be(chunk_t v, unsigned char *out_bytes, size_t len) {
    if (!out_bytes) return HC_ERR_INVALID;
    uint64_t tmp = v;
    for (size_t i = 0; i < len; i++) {
        out_bytes[len - 1 - i] = (unsigned char)(tmp & 0xff);
        tmp >>= 8;
    }
    if (tmp != 0) return HC_ERR_RANGE;
    return HC_OK;
}

error_t hc_encrypt_str(hacker_cipher_t *hc, const char *text, char **out_ciphertext) {
    if (out_ciphertext) *out_ciphertext = NULL;
    if (!hc || !text || !out_ciphertext) return HC_ERR_INVALID;

    const unsigned char *data = (const unsigned char *)text;
    size_t len = strlen(text);

    strbuf_t sb;
    error_t r = sb_init(&sb);
    if (r != HC_OK) return r;

    uint64_t N = (uint64_t)hc->n_words;
    uint64_t chunk_max = (hc->chunk_size >= 8) ? UINT64_MAX : ((1ULL << (hc->chunk_size * 8)) - 1ULL);
    size_t chunk_divs = div_count_for_max_u64(chunk_max, N);

    /* Header: variable-length prefix with parity termination bit. */
    r = append_chunk_var_prefix(hc, &sb, (uint64_t)len);
    if (r != HC_OK) goto fail;

    for (size_t off = 0; off < len;) {
        size_t chunk_len = len - off;
        if (chunk_len > hc->chunk_size) chunk_len = hc->chunk_size;

        chunk_t chunk = 0;
        r = bytes_to_chunk_be(data + off, chunk_len, &chunk);
        if (r != HC_OK) goto fail;

        r = append_num_divs_words_fixed(hc, &sb, (uint64_t)chunk, chunk_divs);
        if (r != HC_OK) goto fail;

        off += chunk_len;
    }

    *out_ciphertext = sb_finish(&sb);
    return HC_OK;

fail:
    free(sb.buf);
    return r;
}

error_t hc_decrypt_str(hacker_cipher_t *hc, const char *ciphertext, char **out_plaintext) {
    if (out_plaintext) *out_plaintext = NULL;
    if (!hc || !ciphertext || !out_plaintext) return HC_ERR_INVALID;

    char *tmp = hc_strdup(ciphertext);
    if (!tmp) return HC_ERR_OOM;

    size_t cap = 128;
    size_t n_words = 0;
    char **words = calloc(cap, sizeof(char *));
    if (!words) {
        free(tmp);
        return HC_ERR_OOM;
    }

    uint64_t plain_len = 0;
    unsigned char *out = NULL;
    error_t r = HC_OK;
    uint64_t N = (uint64_t)hc->n_words;
    uint64_t chunk_max = (hc->chunk_size >= 8) ? UINT64_MAX : ((1ULL << (hc->chunk_size * 8)) - 1ULL);
    size_t chunk_divs = div_count_for_max_u64(chunk_max, N);

    char delim[2] = {hc->separator ? hc->separator : ' ', '\0'};
    char *saveptr = NULL;
    for (char *tok = strtok_r(tmp, delim, &saveptr); tok; tok = strtok_r(NULL, delim, &saveptr)) {
        if (n_words >= cap) {
            size_t ncap = cap * 2;
            char **nw = realloc(words, ncap * sizeof(char *));
            if (!nw) {
                r = HC_ERR_OOM;
                goto done;
            }
            words = nw;
            cap = ncap;
        }
        if (n_words >= (1U << 20)) {
            r = HC_ERR_RANGE;
            goto done;
        }
        words[n_words++] = tok;
    }

    size_t header_divs = 0;
    r = decrypt_chunk_var_prefix(hc, words, n_words, &plain_len, &header_divs);
    if (r != HC_OK) goto done;

    size_t n_chunks = (size_t)((plain_len + hc->chunk_size - 1) / hc->chunk_size);
    size_t expected_words = header_divs + n_chunks * chunk_divs;
    if (n_words != expected_words) {
        r = HC_ERR_PARSE;
        goto done;
    }

    out = (unsigned char *)malloc((size_t)plain_len + 1);
    if (!out) {
        r = HC_ERR_OOM;
        goto done;
    }

    size_t out_off = 0;
    for (size_t i = 0; i < n_chunks; i++) {
        uint64_t num = 0;
        size_t pos = header_divs + i * chunk_divs;
        r = words_to_num(hc, &words[pos], chunk_divs, &num);
        if (r != HC_OK) goto done;

        size_t remaining = (size_t)plain_len - out_off;
        size_t chunk_len = remaining > hc->chunk_size ? hc->chunk_size : remaining;
        r = chunk_to_bytes_be((chunk_t)num, out + out_off, chunk_len);
        if (r != HC_OK) goto done;
        out_off += chunk_len;
    }

    out[plain_len] = '\0';
    *out_plaintext = (char *)out;

done:
    free(words);
    free(tmp);
    if (r != HC_OK) {
        free(out);
        *out_plaintext = NULL;
    }
    return r;
}
