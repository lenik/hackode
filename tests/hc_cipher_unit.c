/*
 * Copyright (C) 2026 Lenik <hackode@bodz.net>
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#define _POSIX_C_SOURCE 200809L

#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "../src/hackode/lib.h"

/* Some libcs may hide mkstemp() prototype even with _POSIX_C_SOURCE. */
int mkstemp(char *template);

static const char *test_dict_words[] = {
    "daemon", "hacker", "kernel", "debian", "shell", "script",
    "proxy", "pixel", "syntax", "binary", "vector", "logic",
};

static hacker_cipher_t *make_cipher(size_t chunk_size) {
    int err = 0;
    return hc_create(
        test_dict_words,
        sizeof(test_dict_words) / sizeof(test_dict_words[0]),
        chunk_size,
        &err
    );
}

START_TEST(test_roundtrip_basic) {
    hacker_cipher_t *hc = make_cipher(2);
    ck_assert_ptr_nonnull(hc);

    const char *plain = "hi debian";
    char *enc = NULL;
    ck_assert_int_eq(hc_encrypt_str(hc, plain, &enc), 0);
    ck_assert_ptr_nonnull(enc);

    char *dec = NULL;
    ck_assert_int_eq(hc_decrypt_str(hc, enc, &dec), 0);
    ck_assert_ptr_nonnull(dec);
    ck_assert_str_eq(dec, plain);

    free(enc);
    free(dec);
    hc_destroy(hc);
}
END_TEST

START_TEST(test_roundtrip_non_multiple_chunk) {
    hacker_cipher_t *hc = make_cipher(3);
    ck_assert_ptr_nonnull(hc);

    /* length 5, chunk_size 3 => last chunk is shorter */
    const char *plain = "abcde";
    char *enc = NULL;
    ck_assert_int_eq(hc_encrypt_str(hc, plain, &enc), 0);

    char *dec = NULL;
    ck_assert_int_eq(hc_decrypt_str(hc, enc, &dec), 0);
    ck_assert_str_eq(dec, plain);

    free(enc);
    free(dec);
    hc_destroy(hc);
}
END_TEST

START_TEST(test_roundtrip_empty) {
    hacker_cipher_t *hc = make_cipher(4);
    ck_assert_ptr_nonnull(hc);

    const char *plain = "";
    char *enc = NULL;
    ck_assert_int_eq(hc_encrypt_str(hc, plain, &enc), 0);
    ck_assert_ptr_nonnull(enc);

    char *dec = NULL;
    ck_assert_int_eq(hc_decrypt_str(hc, enc, &dec), 0);
    ck_assert_ptr_nonnull(dec);
    ck_assert_str_eq(dec, plain);

    free(enc);
    free(dec);
    hc_destroy(hc);
}
END_TEST

START_TEST(test_num_encrypt_decrypt_roundtrip) {
    hacker_cipher_t *hc = make_cipher(2);
    ck_assert_ptr_nonnull(hc);

    for (chunk_t chunk = 0; chunk < 100000; chunk++) {
        chunk_t N = (chunk_t)hc->n_words;
        chunk_t divs[64];
        size_t cnt = 0;
        chunk_t tmp = chunk;

        if (tmp == 0) {
            divs[cnt++] = 0;
        } else {
            while (tmp > 0) {
                divs[cnt++] = tmp % N;
                tmp /= N;
            }
        }

        chunk_t enc = 0;
        for (size_t i = cnt; i > 0; i--) {
            chunk_t div = divs[i - 1];
            enc = enc * N + hc_forward(hc, div);
        }

        chunk_t dec = 0;
        tmp = enc;
        cnt = 0;
        if (tmp == 0) {
            divs[cnt++] = 0;
        } else {
            while (tmp > 0) {
                divs[cnt++] = tmp % N;
                tmp /= N;
            }
        }
        for (size_t i = cnt; i > 0; i--) {
            chunk_t div = divs[i - 1];
            dec = dec * N + hc_inverse(hc, div);
        }
        ck_assert_uint_eq(dec, chunk);
    }

    hc_destroy(hc);
}
END_TEST

START_TEST(test_dictmap_roundtrip) {
    /* Create a temporary text dict file. */
    char dict_template[] = "/tmp/hackode-dict-text-XXXXXX";
    int dfd = mkstemp(dict_template);
    ck_assert_int_ne(dfd, -1);
    FILE *df = fdopen(dfd, "w");
    ck_assert_ptr_nonnull(df);
    for (size_t i = 0; i < sizeof(test_dict_words) / sizeof(test_dict_words[0]); i++) {
        ck_assert_int_eq(fprintf(df, "%s\n", test_dict_words[i]), (int)strlen(test_dict_words[i]) + 1);
    }
    fclose(df); /* closes dfd too */

    /* Create a temporary output map file (will be overwritten). */
    char map_template[] = "/tmp/hackode-dict-map-XXXXXX";
    int mfd = mkstemp(map_template);
    ck_assert_int_ne(mfd, -1);
    close(mfd);

    int err = 0;
    ck_assert_int_eq(dict_compile_text_to_map(dict_template, map_template, &err), 0);

    hacker_cipher_t *hc = hc_create_from_dictmap(map_template, 2, &err);
    ck_assert_ptr_nonnull(hc);

    const char *plain = "hi debian";
    char *enc = NULL;
    ck_assert_int_eq(hc_encrypt_str(hc, plain, &enc), 0);
    ck_assert_ptr_nonnull(enc);

    char *dec = NULL;
    ck_assert_int_eq(hc_decrypt_str(hc, enc, &dec), 0);
    ck_assert_ptr_nonnull(dec);
    ck_assert_str_eq(dec, plain);

    free(enc);
    free(dec);
    hc_destroy(hc);

    unlink(dict_template);
    unlink(map_template);
}
END_TEST

START_TEST(test_create_invalid_args) {
    int err = 0;
    hacker_cipher_t *hc = hc_create(NULL, 12, 2, &err);
    ck_assert_ptr_null(hc);
    ck_assert_int_eq(err, HC_ERR_INVALID);

    hc = hc_create(test_dict_words, sizeof(test_dict_words) / sizeof(test_dict_words[0]), 0, &err);
    ck_assert_ptr_null(hc);
    ck_assert_int_eq(err, HC_ERR_CHUNK);
}
END_TEST

START_TEST(test_lookup_and_word_mapping) {
    hacker_cipher_t *hc = make_cipher(2);
    ck_assert_ptr_nonnull(hc);

    mod_t div = 0;
    ck_assert_int_eq(hc_lookup(hc, "debian", &div), HC_OK);
    ck_assert_ptr_nonnull(hc_word(hc, div));

    /* invalid lookup path */
    ck_assert_int_eq(hc_lookup(hc, "does-not-exist", &div), HC_ERR_NOTFOUND);

    hc_destroy(hc);
}
END_TEST

START_TEST(test_separator_roundtrip) {
    hacker_cipher_t *hc = make_cipher(2);
    ck_assert_ptr_nonnull(hc);

    hc_set_separator(hc, '/');
    ck_assert_int_eq(hc_get_separator(hc), '/');

    const char *plain = "hello";
    char *enc = NULL;
    char *dec = NULL;
    ck_assert_int_eq(hc_encrypt_str(hc, plain, &enc), HC_OK);
    ck_assert_ptr_nonnull(enc);
    ck_assert_ptr_nonnull(strchr(enc, '/'));

    ck_assert_int_eq(hc_decrypt_str(hc, enc, &dec), HC_OK);
    ck_assert_str_eq(dec, plain);

    free(enc);
    free(dec);
    hc_destroy(hc);
}
END_TEST

START_TEST(test_encrypt_decrypt_chunk_apis) {
    hacker_cipher_t *hc = make_cipher(2);
    ck_assert_ptr_nonnull(hc);

    mod_t *divs = NULL;
    size_t n_divs = 0;
    chunk_t out = 0;

    ck_assert_int_eq(hc_encrypt_chunk(hc, 123456, &divs, &n_divs), HC_OK);
    ck_assert_ptr_nonnull(divs);
    ck_assert_uint_gt(n_divs, 0);
    ck_assert_int_eq(hc_decrypt_divs_to_chunk(hc, divs, n_divs, &out), HC_OK);
    ck_assert_uint_eq(out, 123456);
    free(divs);

    /* invalid arguments and range checks */
    ck_assert_int_eq(hc_encrypt_chunk(hc, 1, NULL, &n_divs), HC_ERR_INVALID);
    ck_assert_int_eq(hc_encrypt_chunk(hc, 1, &divs, NULL), HC_ERR_INVALID);
    ck_assert_int_eq(hc_decrypt_divs_to_chunk(hc, NULL, 1, &out), HC_ERR_INVALID);
    ck_assert_int_eq(hc_decrypt_divs_to_chunk(hc, (mod_t[]){0}, 0, &out), HC_ERR_INVALID);
    ck_assert_int_eq(hc_decrypt_divs_to_chunk(hc, (mod_t[]){(mod_t)hc->n_words}, 1, &out), HC_ERR_RANGE);

    hc_destroy(hc);
}
END_TEST

START_TEST(test_encrypt_chunk_to_words_and_invalid_inputs) {
    hacker_cipher_t *hc = make_cipher(2);
    ck_assert_ptr_nonnull(hc);

    char *words = NULL;
    ck_assert_int_eq(hc_encrypt_chunk_to_words(hc, 42, &words), HC_OK);
    ck_assert_ptr_nonnull(words);
    ck_assert_uint_gt(strlen(words), 0);
    free(words);

    ck_assert_int_eq(hc_encrypt_chunk_to_words(hc, 42, NULL), HC_ERR_INVALID);
    ck_assert_int_eq(hc_encrypt_str(hc, NULL, &words), HC_ERR_INVALID);
    ck_assert_int_eq(hc_decrypt_str(hc, NULL, &words), HC_ERR_INVALID);

    hc_destroy(hc);
}
END_TEST

START_TEST(test_decrypt_invalid_ciphertexts) {
    hacker_cipher_t *hc = make_cipher(2);
    ck_assert_ptr_nonnull(hc);

    char *out = NULL;

    /* unknown token should fail during variable header lookup. */
    ck_assert_int_eq(hc_decrypt_str(hc, "unknown-token", &out), HC_ERR_NOTFOUND);
    ck_assert_ptr_null(out);

    /* truncated payload must fail parse under fixed-width framing */
    ck_assert_int_eq(hc_decrypt_str(hc, "hacker", &out), HC_ERR_PARSE);
    ck_assert_ptr_null(out);

    hc_destroy(hc);
}
END_TEST

static Suite *hc_cipher_suite(void) {
    Suite *s = suite_create("hc_cipher");
    TCase *tc_core = tcase_create("core");

    tcase_add_test(tc_core, test_roundtrip_basic);
    tcase_add_test(tc_core, test_roundtrip_non_multiple_chunk);
    tcase_add_test(tc_core, test_roundtrip_empty);
    tcase_add_test(tc_core, test_num_encrypt_decrypt_roundtrip);
    tcase_add_test(tc_core, test_dictmap_roundtrip);
    tcase_add_test(tc_core, test_create_invalid_args);
    tcase_add_test(tc_core, test_lookup_and_word_mapping);
    tcase_add_test(tc_core, test_separator_roundtrip);
    tcase_add_test(tc_core, test_encrypt_decrypt_chunk_apis);
    tcase_add_test(tc_core, test_encrypt_chunk_to_words_and_invalid_inputs);
    tcase_add_test(tc_core, test_decrypt_invalid_ciphertexts);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void) {
    Suite *s = hc_cipher_suite();
    SRunner *sr = srunner_create(s);
    int failed;

    srunner_run_all(sr, CK_NORMAL);
    failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return failed == 0 ? 0 : 1;
}

