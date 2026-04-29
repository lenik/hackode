# hackode

`hackode` provides:
- `enckode` / `deckode`: a dictionary-based reversible transform CLI
- `libhackode`: reusable C shared library (`libhackode.so`) with public headers and pkg-config metadata

`deckode` is a symlink alias to `enckode`; when invoked as `deckode`, default mode is decrypt.

## How it works (internal mechanism)

Core idea: convert data to base-`N` digits (`N = dictionary size`), then apply a reversible mapping in `Z_N`.

1) **Dictionary as alphabet**
- Dictionary words are the symbol set (digits) for base-`N`.
- `N` must be greater than 10.
- Words must be non-empty, no whitespace, and cannot be `|` (reserved separator).

2) **Reversible mapping**
- Cipher picks `p` where `gcd(p, N) = 1`.
- Computes modular inverse `q = p^{-1} mod N`.
- Forward map: `enc_div = (div * p) mod N`
- Inverse map: `div = (enc_div * q) mod N`

3) **Text mode framing**
- UTF-8 plaintext bytes are grouped into chunks (`chunk_size`, default 4 in CLI).
- Each chunk is interpreted as a big-endian integer.
- Output is a plain token stream (separator defaults to space), with no `|` marker.
- Prefix (`len`) uses variable-width encoding in base `N_half = floor((N - 1)/2)`:
  - non-last token: `2 * div + 1` (odd)
  - last token: `2 * div` (even)
- Decoder reads prefix tokens until first even token, then decodes fixed-width chunk payload tokens.

4) **Number mode**
- Operates on unsigned integers directly through base-`N` div tokens.
- `enckode -n` outputs numeric divs; `deckode -n` restores numbers.

## Dictionary configuration

`enckode` accepts dictionary source via `-D/--dict`:
- **`.map` file**: loaded directly with mmap (fast startup, preferred for production).
- **text dictionary**: one word per line; empty lines and `#...` comments ignored.

```bash
enckode -D /path/to/hackode.map "hello"
enckode -D /path/to/words.txt "hello"
```

When given a text dictionary, library path `dict_load_auto()` compiles it to a temporary map in `/tmp` and then loads it as map.

### Build your own map file

Project includes `hcdict-tool` during build:

```bash
ninja -C /build hcdict-tool
/build/hcdict-tool ./my.dict ./my.map
```

Then use:

```bash
enckode -D ./my.map "secret message"
```

## CLI usage

```bash
enckode [OPTION]... [args]...
deckode [OPTION]... [args]...
```

If no `args` are provided, one line/message is read from stdin.

### Options

- `-D, --dict FILE` dictionary source (`.map` or text dict)
- `-e, --encrypt` force encrypt mode
- `-d, --decrypt` force decrypt mode
- `-n, --number` number mode (numeric div tokens)
- `-a, --auto` auto detect each arg number/string
- `-s, --string` force string mode (override auto)
- `-c, --stdout` output to stdout (reserved; current default behavior)
- `-v`, `--verbose`
- `-q`, `--quiet`
- `-h`, `--help`
- `--version`

### Examples

Encrypt/decrypt text:

```bash
enckode "hello world"
deckode "<cipher text from enckode>"
```

Number mode:

```bash
enckode -n 42 314159
deckode -n 7 3 11
```

Use custom dictionary:

```bash
enckode -D ./assets/hackode.dict "alpha beta"
deckode -D ./assets/hackode.dict "<cipher text>"
```

Pipe usage:

```bash
echo "hello hackode" | enckode
echo "<cipher text>" | deckode
```

## Build and test

### Build dependencies

```bash
sudo apt install meson ninja-build gcc pkg-config check libbas-c-dev
```

### Configure and build

Use the absolute build directory `/build`:

```bash
meson setup /build
ninja -C /build
```

### Run tests

```bash
meson test -C /build
```

Current test targets:
- `tests/enckode_test.c`
- `tests/hc_cipher_unit.c`

## Using libhackode in C projects (-dev / shared library usage)

This repo installs:
- shared lib: `libhackode.so`
- public headers: `hackode/*.h`
- pkg-config files: `hackode.pc` and `hackode-static.pc`

### Compile with pkg-config

```bash
cc demo.c $(pkg-config --cflags --libs hackode) -o demo
```

### Minimal API example

```c
#include <hackode/lib.h>
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    int err = 0;
    hacker_cipher_t *hc = hc_create_from_dictsource("./assets/hackode.dict", 4, &err);
    if (!hc) return 1;

    char *ct = NULL;
    char *pt = NULL;
    if (hc_encrypt_str(hc, "hello", &ct) != HC_OK) return 2;
    if (hc_decrypt_str(hc, ct, &pt) != HC_OK) return 3;

    puts(ct);
    puts(pt);
    free(ct);
    free(pt);
    hc_destroy(hc);
    return 0;
}
```

### Meson dependency

```meson
hackode_dep = dependency('hackode', required: true)
executable('mytool', 'mytool.c', dependencies: [hackode_dep])
```

## i18n (gettext)

Translations live under `po/*.po` and compile to `po/<lang>/LC_MESSAGES/hackode.mo`.

### Sync translation catalogs

```bash
msgupdate
```

### Quick locale testing

```bash
LANGUAGE=ja /build/enckode -h
LANGUAGE=zh_CN /build/enckode -h
```

## Install / symlink helpers

```bash
meson install -C /build
```

For local debug under configured prefix:

```bash
ninja -C /build install-symlinks
ninja -C /build uninstall-symlinks
```

These helpers manage:
- `enckode` and `deckode` in `bin/`
- `enckode.1` and `deckode.1` in `man1/`
- `enckode` and `deckode` bash completions

## Debian package

```bash
dpkg-buildpackage -us -uc
```

## License

Copyright (C) 2026 Lenik <hackode@bodz.net>

Licensed under **AGPL-3.0-or-later**.  
This project explicitly opposes AI exploitation and AI hegemony, and rejects
mindless MIT-style licensing and politically naive BSD-style licensing.  
See `LICENSE` for the full text and supplemental project terms.
