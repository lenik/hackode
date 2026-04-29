#ifndef LIB_H
#define LIB_H

#include "dict.h"
#include "err.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef uint64_t chunk_t;
typedef uint64_t mod_t; /* modular element: 0 <= div < N */
#define mod_t mod_t

int copy_stream(FILE *in, FILE *out);
int copy_file(const char *prog, const char *path);

/* --- Hacker cipher (dictionary-based) --- */
struct hacker_cipher {
    mod_t p;       /* forward multiplier in Z_N */
    mod_t q;       /* inverse multiplier in Z_N */
    size_t n_words;   /* N */
    size_t chunk_size; /* plaintext bytes per chunk */
    char separator;    /* word/message separator, default ' ' */

    dict_t *dict; /* mmap-friendly dictionary */

    /* String -> index hash table */
    const char **ht_keys; /* [ht_cap], NULL means empty */
    size_t *ht_vals;      /* [ht_cap] */
    size_t ht_cap;        /* power-of-two */
};

typedef struct hacker_cipher hacker_cipher_t;

/* Initialize the hash table for a cipher context. 
 *
 * The hash table is used to map dictionary words to indices. This must be called
 * after the dictionary is set up in hc_create() and before any other operations.
 *
 * Parameters:
 *   - hc: cipher context.
 *
 * Returns: HC_OK on success, negative error code on failure.
 */
error_t ht_init(hacker_cipher_t *hc);

/* Create a cipher context.
 *
 * - dictionary: array of N words (UTF-8 strings), each must not contain whitespace.
 * - N (dictionary length): must be > 10.
 * - chunk_size: plaintext bytes per chunk, used for chunk_value = big-endian integer.
 *   For this C implementation, chunk_size must be in [1, 8] (uint64 backing).
 *
 * Returns: heap-allocated context, or NULL on error.
 * The caller owns the returned context and must call hc_destroy().
 *
 * err_out (optional): receives a negative error code on failure.
 */
hacker_cipher_t *
hc_create(const char *const *dictionary, size_t n_words, size_t chunk_size, int *err_out);

/* Free a cipher context created by hc_create(). */
void hc_destroy(hacker_cipher_t *hc);

/* Create cipher from a precompiled dict.map (mmap-backed).
 * Ownership of the dict is transferred to the cipher and will be released
 * by hc_destroy().
 */
hacker_cipher_t *hc_create_from_dictmap(const char *map_path, size_t chunk_size, int *err_out);
hacker_cipher_t *hc_create_from_dictsource(const char *dict_path, size_t chunk_size, int *err_out);

/* Forward mapping: 
 *
 * This encrypts a number to be messy but reversible.
 *
 * Parameters:
 *   - hc: cipher context.
 *   - div: original number.
 *
 * Returns: the forward mapped number.
 */
mod_t hc_forward(hacker_cipher_t *hc, mod_t div);

/* Inverse mapping:
 *
 * This decrypts a messy number to the original number.
 *
 * Parameters:
 *   - hc: cipher context.
 *   - div: messy number.
 *
 * Returns: the inverse mapped number.
 */
mod_t hc_inverse(hacker_cipher_t *hc, mod_t div);

/* Caller-side helpers for number/string conversion. */
error_t hc_lookup(hacker_cipher_t *hc, const char *word, mod_t *out_div);
const char *hc_word(hacker_cipher_t *hc, mod_t div);
void hc_set_separator(hacker_cipher_t *hc, char separator);
char hc_get_separator(hacker_cipher_t *hc);

/* Convenience helpers for example apps:
 * - convert a numeric chunk to mapped word message
 * - convert a numeric chunk to mapped div array
 * - parse mapped div array back to chunk
 */
error_t hc_encrypt_chunk_to_words(hacker_cipher_t *hc, chunk_t chunk, char **out_words_message);
error_t hc_encrypt_chunk(hacker_cipher_t *hc, chunk_t chunk, mod_t **out_divs, size_t *out_n_divs);
error_t hc_decrypt_divs_to_chunk(hacker_cipher_t *hc, const mod_t *divs, size_t n_divs, chunk_t *out_chunk);

/* Lookup a word in the dictionary.
 *
 * Parameters:
 *   - hc: cipher context.
 *   - word: word to lookup.
 *   - out_idx: receives the dictionary index of the word.
 *
 * Returns: HC_OK on success, negative error code on failure.
 */
error_t ht_lookup(hacker_cipher_t *hc, const char *word, size_t *out_idx);

/* Encrypt UTF-8 text.
 *
 * Output format (tokens separated by separator, default space):
 *   [var-width len_prefix][fixed-width chunk1_divs][fixed-width chunk2_divs]...
 *
 * len_prefix uses a base floor((N-1)/2) variable-width scheme:
 * each encoded div uses odd value for continuation and even value for last.
 * Chunk payload keeps fixed div counts derived from base-N and chunk_size.
 *
 * On success, the caller owns *out_ciphertext and must free() it.
 * On failure, *out_ciphertext is set to NULL.
 */
error_t hc_encrypt_str(hacker_cipher_t *hc, const char *text, char **out_ciphertext);

/* Decrypt UTF-8 text produced by hc_encrypt_str().
 *
 * On success, the caller owns *out_plaintext and must free() it.
 * On failure, *out_plaintext is set to NULL.
 */
error_t hc_decrypt_str(hacker_cipher_t *hc, const char *ciphertext, char **out_plaintext);

#endif /* LIB_H */