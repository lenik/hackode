/*
 * Copyright (C) 2026 Lenik <hackode@bodz.net>
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#define _POSIX_C_SOURCE 200809L

#include "config.h"
#include "lib.h"
#include "str.h"
#include "util.h"

#include <bas/locale/i18n.h>
#include <bas/log/deflog.h>
#include <bas/proc/env.h>

#include <sys/stat.h>

#include <getopt.h>
#include <inttypes.h>
#include <limits.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

define_logger();

enum { OPT_VERSION = 256 };

/* number/string conversion helpers now come from libhackode (hc_* helpers). */

static int split_stdin_args(char *input, char ***out_argv, int *out_argc) {
    if (out_argv) *out_argv = NULL;
    if (out_argc) *out_argc = 0;
    if (!input || !out_argv || !out_argc) return -1;

    size_t cap = 16;
    size_t n = 0;
    char **args = calloc(cap, sizeof(char *));
    if (!args) return -1;

    char *saveptr = NULL;
    for (char *tok = strtok_r(input, " \t\r\n", &saveptr); tok; tok = strtok_r(NULL, " \t\r\n", &saveptr)) {
        if (n == cap) {
            size_t ncap = cap * 2;
            char **na = realloc(args, ncap * sizeof(char *));
            if (!na) {
                free(args);
                return -1;
            }
            args = na;
            cap = ncap;
        }
        args[n++] = tok;
    }

    *out_argv = args;
    *out_argc = (int)n;
    return 0;
}

static hacker_cipher_t *g_hc = NULL;

static bool args_all_number(int argc, char **argv) {
    for (int i = 0; i < argc; i++) {
        chunk_t n = 0;
        if (!parse_number(argv[i], &n)) return false;
    }
    return true;
}

static int encrypt_textlike(int argc, char **argv) {
    for (int i = 0; i < argc; i++) {
        char *ct = NULL;
        if (hc_encrypt_str(g_hc, argv[i], &ct) != HC_OK || !ct) {
            logerror_fmt(_("hc_encrypt_str failed"));
            return 1;
        }
        puts(ct);
        free(ct);
    }
    return 0;
}

static int encrypt_numlike(int argc, char **argv) {
    for (int i = 0; i < argc; i++) {
        chunk_t chunk = 0;
        if (parse_number(argv[i], &chunk)) {
            mod_t *divs = NULL;
            size_t n_divs = 0;
            if (hc_encrypt_chunk(g_hc, chunk, &divs, &n_divs) != HC_OK || !divs || n_divs == 0) {
                logerror_fmt(_("encryption failed (overflow/range)"));
                if (divs) free(divs);
                return 1;
            }
            for (size_t j = 0; j < n_divs; j++) {
                if (j) putchar(' ');
                printf("%" PRIu64, (uint64_t)divs[j]);
            }
            putchar('\n');
            free(divs);
            continue;
        }

        /* numlike fallback: non-number arg is handled as text */
        char *ct = NULL;
        if (hc_encrypt_str(g_hc, argv[i], &ct) != HC_OK || !ct) {
            logerror_fmt(_("hc_encrypt_str failed"));
            return 1;
        }
        puts(ct);
        free(ct);
    }
    return 0;
}

static int decrypt_textlike(int argc, char **argv) {
    char *ciphertext = strings_join((const char *const *)argv, (size_t)argc, ' ');
    if (!ciphertext) {
        logerror_fmt(_("out of memory"));
        return 1;
    }

    char *pt = NULL;
    if (hc_decrypt_str(g_hc, ciphertext, &pt) != HC_OK || !pt) {
        logerror_fmt(_("hc_decrypt_str failed"));
        free(ciphertext);
        return 1;
    } else {
        puts(pt);
        free(pt);
    }
    free(ciphertext);
    return 0;
}

static int decrypt_numlike(int argc, char **argv) {
    for (int i = 0; i < argc; i++) {
        chunk_t tmp = 0;
        if (parse_number(argv[i], &tmp)) {
            mod_t div = (mod_t)tmp;
            chunk_t plain = 0;
            if (hc_decrypt_divs_to_chunk(g_hc, &div, 1, &plain) != HC_OK) {
                logerror_fmt(_("number div decryption failed"));
                return 1;
            }
            printf("%" PRIu64 "\n", plain);
            continue;
        }

        /* numlike fallback: text ciphertext => decrypted text => parse number */
        char *pt = NULL;
        if (hc_decrypt_str(g_hc, argv[i], &pt) != HC_OK || !pt) {
            logerror_fmt(_("hc_decrypt_str failed"));
            return 1;
        }
        chunk_t val = 0;
        if (!parse_number(pt, &val)) {
            logerror_fmt(_("decrypted text is not an unsigned integer"));
            free(pt);
            return 1;
        }
        printf("%" PRIu64 "\n", val);
        free(pt);
    }
    return 0;
}

void usage(FILE *out) {
    fputs(_("Usage: enckode [OPTION]... [args]...\n"), out);
    fputs(_("\nEncrypt/Decrypt using a shared dictionary-based reversible transform.\n"), out);
    fputs(_("\nWhen no args specified, reads one message from stdin.\n"), out);

    fputs("\n", out);
    fputs("  -D, --dict FILE   ", out);
    fputs(_("Dictionary source. If FILE ends with .map it is treated as a precompiled mmap image; otherwise it's treated as a text dictionary (one word per line). #... and empty lines are ignored.\n"),
          out);
    fputs("  -e, --encrypt     ", out);
    fputs(_("encryption mode (default)\n"), out);
    fputs("  -d, --decrypt     ", out);
    fputs(_("decryption mode\n"), out);
    fputs("  -n, --number      ", out);
    fputs(_("number mode: output/input base-N div tokens as numbers\n"), out);
    fputs("  -a, --auto        ", out);
    fputs(_("auto detect each arg as number or string (numbers are treated as number by default unless --string)\n"),
          out);
    fputs("  -s, --string      ", out);
    fputs(_("always treat args as strings (overrides --auto)\n"), out);
    fputs("  -c, --stdout      ", out);
    fputs(_("output to stdout (default)\n"), out);

    fputs("\n", out);
    fputs("  -h, --help         ", out);
    fputs(_("display this help and exit\n"), out);
    fputs("      --version      ", out);
    fputs(_("output version information and exit\n"), out);

    fputs("\n", out);
    fprintf(out, _("Report bugs to: <%s>\n"), PROJECT_EMAIL);
}

int main(int argc, char **argv) {
    const char *exe = self_exe();
    init_i18n(LOCALEDIR);

    static const struct option long_opts[] = {
        {"help", no_argument, NULL, 'h'},
        {"version", no_argument, NULL, OPT_VERSION},
        {"dict", required_argument, NULL, 'D'},
        {"stdout", no_argument, NULL, 'c'},
        {"encrypt", no_argument, NULL, 'e'},
        {"decrypt", no_argument, NULL, 'd'},
        {"string", no_argument, NULL, 's'},
        {"auto", no_argument, NULL, 'a'},
        {"number", no_argument, NULL, 'n'},
        {"verbose", no_argument, NULL, 'v'},
        {"quiet", no_argument, NULL, 'q'},
        {NULL, 0, NULL, 0},
    };

    int decrypt_mode = 0;
    int number_mode = 0;
    int auto_detect = 1;
    int force_string = 0;
    const char *dict_path = NULL;

    /* deckode symlink => default decrypt. */
    const char *base = strrchr(exe, '/');
    base = base ? base + 1 : exe;
    if (strcmp(base, "deckode") == 0) decrypt_mode = 1;

    for (;;) {
        int c = getopt_long(argc, argv, "vqhD:edsanc", long_opts, NULL);
        if (c == -1) {
            break;
        }
        switch (c) {
        case 'v':
            log_more();
            break;
        case 'q':
            log_less();
            break;
        case 'h':
            usage(stdout);
            return 0;
        case OPT_VERSION:
            printf("enckode %s\n", PROJECT_VERSION);
            printf(_("Copyright (C) %d %s\n"), PROJECT_YEAR, PROJECT_AUTHOR);
            fputs(_("License AGPL-3.0-or-later: <https://www.gnu.org/licenses/agpl-3.0.html>\n"),
                  stdout);
            fputs(_("This is free software: you are free to change and redistribute it.\n"),
                  stdout);
            fputs(_("This project opposes AI exploitation and AI hegemony.\n"), stdout);
            fputs(_("This project rejects mindless MIT-style licensing and politically naive "
                    "BSD-style licensing.\n"),
                  stdout);
            fputs(_("There is NO WARRANTY, to the extent permitted by law.\n"), stdout);
            return 0;
        case 'D':
            dict_path = optarg;
            break;
        case 'c':
            /* Reserved: output is always stdout in this program. */
            break;
        case 'e':
            decrypt_mode = 0;
            break;
        case 'd':
            decrypt_mode = 1;
            break;
        case 's':
            force_string = 1;
            break;
        case 'a':
            auto_detect = 1;
            break;
        case 'n':
            number_mode = 1;
            break;
        default:
            usage(stderr);
            return 1;
        }
    }

    argc -= optind;
    argv += optind;

    char *stdin_buf = NULL;
    char **stdin_argv = NULL;
    int stdin_argc = 0;
    if (argc == 0) {
        stdin_buf = read_all_stdin();
        if (!stdin_buf) {
            logerror_fmt(_("stdin read error"));
            return 1;
        }

        /* Keep stdin as one message (do not split by whitespace). */
        size_t slen = strlen(stdin_buf);
        while (slen > 0 && (stdin_buf[slen - 1] == '\n' || stdin_buf[slen - 1] == '\r')) {
            stdin_buf[--slen] = '\0';
        }

        stdin_argv = calloc(1, sizeof(char *));
        if (!stdin_argv) {
            logerror_fmt(_("out of memory"));
            if (stdin_buf) free(stdin_buf);
            return 1;
        }
        stdin_argv[0] = stdin_buf;
        stdin_argc = (slen > 0) ? 1 : 0;

        argv = stdin_argv;
        argc = stdin_argc;
        if (argc == 0) {
            logdebug_fmt(_("nothing to do"));
        }
    }

    /* Default dict: use the installed hackode.map (prefer dev path if present). */
    const char *default_map = HACKODE_DICTMAP_DEV_PATH;
    if (access(default_map, R_OK) != 0) default_map = HACKODE_DICTMAP_PATH;
    if (!dict_path) dict_path = default_map;

    int hc_err = 0;
    g_hc = hc_create_from_dictsource(dict_path, 4, &hc_err);
    if (!g_hc) {
        logerror_fmt(_("cipher init failed: %d"), hc_err);
        if (stdin_argv) free(stdin_argv);
        if (stdin_buf) free(stdin_buf);
        return 1;
    }

    bool use_numlike = false;
    if (force_string) {
        use_numlike = false;
    } else if (number_mode) {
        use_numlike = true;
    } else if (auto_detect) {
        use_numlike = args_all_number(argc, argv);
    } else {
        use_numlike = false;
    }

    int exit_code;
    if (decrypt_mode) {
        exit_code = use_numlike ? decrypt_numlike(argc, argv) : decrypt_textlike(argc, argv);
    } else {
        exit_code = use_numlike ? encrypt_numlike(argc, argv) : encrypt_textlike(argc, argv);
    }

    hc_destroy(g_hc);
    if (stdin_argv) free(stdin_argv);
    if (stdin_buf) free(stdin_buf);
    return exit_code;
}
