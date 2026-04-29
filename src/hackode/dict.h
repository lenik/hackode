/*
 * Copyright (C) 2026 Lenik <hackode@bodz.net>
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#ifndef DICT_H
#define DICT_H

#include "err.h"

#include <stddef.h>
#include <stdint.h>

typedef struct dict dict_t;

dict_t *dict_create_from_strings(const char *const *words, size_t n_words, int *err_out);
error_t dict_compile_text_to_map(const char *text_path, const char *map_path, int *err_out);
error_t dict_load_map(const char *map_path, dict_t **out_dict, int *err_out);
error_t dict_load_auto(const char *dict_path, dict_t **out_dict, int *err_out);

const char *dict_word(const dict_t *dict, size_t idx);
size_t dict_n_words(const dict_t *dict);

void dict_destroy(dict_t *dict);

#endif /* DICT_H */

