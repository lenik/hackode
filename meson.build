project(
    'hackode',
    'c',
    version: run_command(
        'sh',
        '-c', '''
        v=$(git describe --tags --always --dirty 2>/dev/null || true)
        v=${v#v}
        if [ -n "$v" ]; then
            if [ "${v%.*}" = "$v" ]; then
                v="0.0.0-$v"
            fi
            printf "%s" "$v"
        else
            echo "0.0.0"
        fi
        ''',
        check: false,
    ).stdout().strip(),
    license: 'AGPL-3.0-or-later',
    default_options: [
        'warning_level=2',
    ],
)

project_author = 'Lenik'
project_email = 'hackode@bodz.net'
project_year = 2026

fs = import('fs')
c = meson.get_compiler('c')

prefix = get_option('prefix')
bindir = prefix / get_option('bindir')
datadir = prefix / get_option('datadir')
libdir = prefix / get_option('libdir')
localedir = prefix / get_option('localedir')
mandir = prefix / get_option('mandir')
pkgdatadir = datadir / meson.project_name()
pkgdocdir = datadir / 'doc' / meson.project_name()

config_h = configuration_data()
config_h.set_quoted('PROJECT_VERSION', meson.project_version())
config_h.set_quoted('VERSION', meson.project_version().split('-')[0])
config_h.set_quoted('PROJECT_AUTHOR', project_author)
config_h.set_quoted('PROJECT_EMAIL', project_email)
config_h.set('PROJECT_YEAR', project_year)

config_h.set_quoted('PREFIX', prefix)
config_h.set_quoted('DATADIR', datadir)
config_h.set_quoted('MANDIR', mandir)
config_h.set_quoted('PKGDATADIR', pkgdatadir)
config_h.set_quoted('LOCALEDIR', localedir)

config_h.set_quoted('HACKODE_DICTMAP_PATH', (pkgdatadir / 'hackode.map'))
config_h.set_quoted('HACKODE_DICTMAP_DEV_PATH', (meson.current_build_dir() / 'hackode.map'))

configure_file(
    output: 'config.h',
    configuration: config_h,
)

bas_c_dep = dependency('bas-c', required: true)
# bas_cpp_dep = dependency('bas-cpp', required: true)

# bas_c_static_dep = dependency('bas-c-static', required: true)
# bas_cpp_static_dep = dependency('bas-cpp-static', required: true)

lib_objects = []

# Build assets.zip every time (no file dependency)
assets_dir = meson.current_source_dir() / 'assets'
assets_src = 'src/assets.s'
if fs.exists(assets_dir) and fs.exists(assets_src)
    build_dir = meson.current_build_dir()
    assets_zip = custom_target(
        'assets_zip',
        output: 'assets.zip',
        input: [],
        command: [
            'sh',
            '-c', 'dst=$(readlink -f "@1@"); cd "@0@" && zip -r "$dst" .'.format(assets_dir, '@OUTPUT@'),
        ],
        #build_always : true,
        build_by_default: true,
        install: false,
    )

    # Compile assembly file that embeds assets.zip using .incbin
    # Use configure_file to copy assets.s to build directory
    assets_s = configure_file(
        input: 'src/assets.s',
        output: 'assets.s',
        copy: true,
    )

    # Compile the assembly file (assets.zip will be in the same directory)
    as = find_program('as', required: true)
    assets_o = custom_target(
        'assets_o',
        output: 'assets.o',
        input: [assets_s, assets_zip],
        command: [as, '-o', '@OUTPUT@', '@INPUT0@'],
        depends: assets_zip,
    )

    lib_objects += [assets_o]
endif

# Disable all warnings (wx 3.0 and deps are noisy)
nowarn = c.get_supported_arguments(['-w', '/w'])
bas_args = ['-DLOGGER_NAME=hackode', '-DASSETS_NAME=hackode']

c_pch = 'src/c_pch.h'
c_args = [nowarn, bas_args, '-DTEXT_DOMAIN=hackode']

lib_inc = [
    include_directories('src'),
]

lib_deps = [
    bas_c_dep,
]

# ---- Compile default assets/hackode.dict -> hackode.map ----
# This generates the default dictionary image used by enckode.
dict_tool = executable(
    'hcdict-tool',
    ['src/hackode/dict_tool.c', 'src/hackode/dict.c'],
    c_args: c_args,
    c_pch: c_pch,
    include_directories: lib_inc,
    dependencies: [bas_c_dep],
    install: false,
)

hackode_dict_src = meson.current_source_dir() / 'assets' / 'hackode.dict'
hackode_map = custom_target(
    'hackode_map',
    output: 'hackode.map',
    input: hackode_dict_src,
    command: [dict_tool, '@INPUT0@', '@OUTPUT@'],
    depends: dict_tool,
    build_by_default: true,
    install: true,
    install_dir: pkgdatadir,
)

subdir('po')

lib_sources = [
    'src/hackode/lib.c',
    'src/hackode/dict.c',
    'src/hackode/imath.c',
    'src/hackode/str.c',
    'src/hackode/util.c',
]

lib_version = meson.project_version().split('-')[0]
lib_soversion = lib_version.split('.')[0]

lib_hackode = shared_library(
    'hackode',
    lib_sources,
    lib_objects,
    c_args: c_args,
    c_pch: c_pch,
    include_directories: lib_inc,
    dependencies: lib_deps,
    version: lib_version,
    soversion: lib_soversion,
    install: true,
    install_dir: libdir,
)

lib_hackode_static = static_library(
    'hackode',
    lib_sources,
    lib_objects,
    c_args: c_args,
    c_pch: c_pch,
    include_directories: lib_inc,
    dependencies: lib_deps,
)

hackode_dep = declare_dependency(
    link_with: lib_hackode,
    include_directories: lib_inc,
)
hackode_static_dep = declare_dependency(
    link_with: lib_hackode_static,
    include_directories: lib_inc,
)

meson.override_dependency('hackode', hackode_dep)
meson.override_dependency('hackode-static', hackode_static_dep)

# ---- pkg-config ----
pkgconfig = import('pkgconfig')
pkgconfig.generate(
    lib_hackode,
    name: 'hackode',
    description: 'Zephyr shared library',
    version: meson.project_version(),
    subdirs: 'hackode',
    requires: ['bas-c'],
)

pkgconfig.generate(
    lib_hackode_static,
    name: 'hackode-static',
    description: 'Zephyr static library',
    version: meson.project_version(),
    subdirs: 'hackode',
    requires: ['bas-c'],
)

# ---- Headers for mesonfmt package (includedir/meson) ----
public_headers = files(
    'src/hackode/err.h',
    'src/hackode/dict.h',
    'src/hackode/imath.h',
    'src/hackode/str.h',
    'src/hackode/util.h',
    'src/hackode/lib.h',
)

install_headers(
    public_headers,
    subdir: 'hackode',
)

app_sources = [
    'src/hackode/enckode.c',
]

foreach source : app_sources
    split = source.split('/')
    base = split[-1]
    name = fs.stem(base)
    executable(
        name,
        source,
        c_args: c_args,
        c_pch: c_pch,
        include_directories: lib_inc,
        dependencies: lib_deps,
        link_with: lib_hackode_static,
        install: true,
        install_dir: bindir,
    )
endforeach

install_symlink(
    'deckode',
    pointing_to: 'enckode',
    install_dir: bindir,
)

bash_files = [
    'enckode.bash',
]

foreach file : bash_files
    name = fs.stem(file)
    install_data(
        file,
        install_dir: datadir / 'bash-completion' / 'completions',
        rename: name,
    )
endforeach

install_symlink(
    'deckode',
    pointing_to: 'enckode',
    install_dir: datadir / 'bash-completion' / 'completions',
)

install_data(
    [
        'LICENSE',
        'README.md',
        'README-zh.md',
    ],
    install_dir: pkgdocdir,
)

install_subdir(
    'subprojects/chatapp',
    install_dir: datadir / 'doc',
)

config_man = configuration_data()
config_man.set('PROJECT_VERSION', meson.project_version())
config_man.set('PROJECT_AUTHOR', project_author)
config_man.set('PROJECT_EMAIL', project_email)
config_man.set('PROJECT_YEAR', project_year)

configure_file(
    input: 'enckode.1.in',
    output: 'enckode.1',
    configuration: config_man,
    install: true,
    install_dir: mandir / 'man1',
)

install_symlink(
    'deckode.1',
    pointing_to: 'enckode.1',
    install_dir: mandir / 'man1',
)

# look task: list what installed files looks like
# ninja install <tmpdir> && tree <tmpdir> && rm -fr <tmpdir>
run_target(
    'look',
    command: [
        'bash',
        '-euc', '''
            tmpdir=$(mktemp -d)
            DESTDIR=$tmpdir meson install -C "@BUILD_ROOT@"
            tree "$tmpdir"
            rm -fr "$tmpdir"
        ''',
    ],
)

# Fast local debugging against the active installation prefix:
# symlink installed paths to project/build outputs.
run_target(
    'install-symlinks',
    command: [
        'bash',
        '-euc', '''
            prefix="@0@"
            bindir="@1@"
            datadir="@2@"
            localedir="@3@"
            mandir="@4@"

            mkdir -p "$bindir" "$datadir/bash-completion/completions" "$localedir" "$mandir/man1"

            sudo ln -sfn "@BUILD_ROOT@/enckode" "$bindir/enckode"
            sudo ln -sfn "enckode" "$bindir/deckode"
            sudo ln -sfn "@BUILD_ROOT@/enckode.1" "$mandir/man1/enckode.1"
            sudo ln -sfn "enckode.1" "$mandir/man1/deckode.1"
            sudo ln -sfn "@SOURCE_ROOT@/enckode.bash" "$datadir/bash-completion/completions/enckode"
            sudo ln -sfn "enckode" "$datadir/bash-completion/completions/deckode"

            printf "Symlinks installed under %s\n" "$prefix"
        '''.format(
            get_option('prefix'), #
            bindir,
            datadir,
            localedir,
            mandir,
        ),
    ],
)

run_target(
    'uninstall-symlinks',
    command: [
        'bash',
        '-euc', '''
            prefix="@0@"
            bindir="@1@"
            datadir="@2@"
            localedir="@3@"
            mandir="@4@"

            for p in \
                "$bindir/enckode" \
                "$bindir/deckode" \
                "$mandir/man1/enckode.1" \
                "$mandir/man1/deckode.1" \
                "$datadir/bash-completion/completions/enckode" \
                "$datadir/bash-completion/completions/deckode"
            do
                if [ -L "$p" ]; then
                    sudo rm -f "$p"
                    printf "Removed %s\n" "$p"
                else
                    printf "Skipped (not a symlink): %s\n" "$p"
                fi
            done
        '''.format(
            get_option('prefix'), #
            bindir,
            datadir,
            localedir,
            mandir,
        ),
    ],
)

subdir('tests')