/*
 * Copyright (C) 2026 Lenik <hackode@bodz.net>
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#define _POSIX_C_SOURCE 200809L

#include "dict.h"

#include <bas/locale/i18n.h>
#include <bas/log/deflog.h>

__attribute__((weak))
define_logger();

int main(int argc, char **argv) {
    if (argc != 3) {
        logerror_fmt(_("Usage: %s <text_dict> <dict.map>"), argv[0]);
        return 1;
    }

    int err = 0;
    if (dict_compile_text_to_map(argv[1], argv[2], &err) != 0) {
        logerror_fmt(_("%s: failed to compile %s -> %s (err=%d)"), argv[0], argv[1], argv[2], err);
        return 1;
    }
    return 0;
}

