#!/bin/sh
# Provision the minimum supported Qt (6.4) and build Listista against it.
#
# The package targets Qt 6.4. Ubuntu 24.04 (noble) ships Qt 6.4.2, but a build
# machine may not have it installed, and this project must build without root.
# This script downloads the pinned noble packages into a cache, unpacks them
# with dpkg-deb into a private prefix (default ~/qt64) and rewrites the
# absolute /usr/ paths in the relocated CMake, pkg-config and mkspecs files.
# It then configures, builds and runs the unit tests in build-qt64/.
#
# Usage:
#   tools/build-qt64.sh              provision if needed, build and test
#   tools/build-qt64.sh --refresh    re-unpack the prefix before building
#   tools/build-qt64.sh --setup-only only provision the prefix
#   tools/build-qt64.sh --no-tests   skip the unit tests
#   tools/build-qt64.sh --jobs N     parallel build jobs
#   tools/build-qt64.sh --help       show this text
#
# Environment:
#   QT64_PREFIX        private Qt prefix (default: ~/qt64)
#   QT64_CACHE         .deb cache (default: ~/.cache/listista-qt64)
#   QT64_BASE_URL      Ubuntu archive base
#                      (default: https://archive.ubuntu.com/ubuntu)
#   QT64_EXTRA_PREFIX  extra CMAKE_PREFIX_PATH entry for libcurl/libarchive;
#                      defaults to ~/qt6 when that prefix has libcurl
#   QT64_JOBS          default parallel build jobs
#
# The CMake prefix is <QT64_PREFIX>/usr, not <QT64_PREFIX>: the packages
# install their CMake configs under usr/lib/x86_64-linux-gnu/cmake.
#
# The package list below is the single source of truth: it pins the Ubuntu
# 24.04 versions and a fixed SHA-256 for every download. fetch_debs only
# downloads what the list names, and provision unpacks exactly the same list,
# so an extra .deb left in the cache cannot change the toolchain. The Ubuntu
# pool files are immutable for a given version, so the recipe keeps working
# after a runner restart.

set -eu

PREFIX=${QT64_PREFIX:-${HOME:?HOME is not set}/qt64}
CACHE=${QT64_CACHE:-$HOME/.cache/listista-qt64}
BASE_URL=${QT64_BASE_URL:-https://archive.ubuntu.com/ubuntu}
JOBS=${QT64_JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)}
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

refresh=0
setup_only=0
run_tests=1

die() {
    printf 'build-qt64: %s\n' "$*" >&2
    exit 1
}

usage() {
    sed -n '/^# Usage:/,/^#$/p' "$0" | sed 's/^# \{0,1\}//'
}

while [ $# -gt 0 ]; do
    case $1 in
        --refresh) refresh=1 ;;
        --setup-only) setup_only=1 ;;
        --no-tests) run_tests=0 ;;
        --jobs)
            [ $# -ge 2 ] || die "--jobs needs a number"
            JOBS=$2
            shift
            ;;
        --jobs=*) JOBS=${1#*=} ;;
        -h|--help) usage; exit 0 ;;
        *) die "unknown option: $1 (try --help)" ;;
    esac
    shift
done

case $JOBS in
    ''|*[!0-9]*) die "jobs must be a positive integer (got '$JOBS')" ;;
esac
[ "$JOBS" -gt 0 ] || die "jobs must be a positive integer (got '$JOBS')"

# The prefix is wiped and rewritten on provisioning, and it ends up inside
# sed patterns and CMake/LD paths, so keep it absolute and free of characters
# those places would interpret.
case $PREFIX in
    /*) ;;
    *) die "QT64_PREFIX must be an absolute path (got '$PREFIX')" ;;
esac
# Strip a trailing slash so the paths, sed patterns and CMake guard agree.
while [ "${PREFIX%/}" != "$PREFIX" ]; do
    PREFIX=${PREFIX%/}
done
case $PREFIX in
    ''|/|"$HOME") die "QT64_PREFIX must be a dedicated directory (got '$PREFIX')" ;;
esac
case $PREFIX in
    *'|'*|*'&'*|*'\'*|*:*|*';'*)
        die "QT64_PREFIX must not contain | & \\ : ; (got '$PREFIX')" ;;
esac

# The cloud workspace keeps cmake and ninja in a pip prefix rather than in
# /usr, and the cmake wrapper there needs its package directory on PYTHONPATH.
if [ -x "$HOME/pypkg/bin/cmake" ] &&
   { ! command -v cmake >/dev/null 2>&1 || ! command -v ninja >/dev/null 2>&1; }; then
    PATH=$HOME/pypkg/bin:$PATH
    if [ -d "$HOME/pypkg/cmake" ] && [ -z "${PYTHONPATH:-}" ]; then
        PYTHONPATH=$HOME/pypkg
    fi
    export PATH PYTHONPATH
fi

for tool in cmake ninja dpkg-deb sha256sum; do
    command -v "$tool" >/dev/null 2>&1 || die "$tool is required"
done
if ! command -v curl >/dev/null 2>&1 && ! command -v wget >/dev/null 2>&1; then
    die "curl or wget is required to download the Qt packages"
fi

fetch() {
    if command -v curl >/dev/null 2>&1; then
        curl -fL --retry 3 --retry-delay 2 --connect-timeout 20 -o "$2" "$1"
    else
        wget -q --tries=3 --timeout=20 -O "$2" "$1"
    fi
}

verify_one() {
    printf '%s  %s\n' "$1" "$2" | sha256sum -c - >/dev/null 2>&1
}

packages() {
    cat <<'PACKAGES'
c23173e035b257118540266098bd4f1928c67d918ad981b1e13e6e0d9847d69c|libb2-1_0.98.1-1.1build1_amd64.deb|pool/universe/libb/libb2/libb2-1_0.98.1-1.1build1_amd64.deb
33f5e07c74f73c2bfb44086ae0f9e6da52acd6c104d30ffe8fd768d8253d5e82|libglvnd0_1.7.0-1build1_amd64.deb|pool/main/libg/libglvnd/libglvnd0_1.7.0-1build1_amd64.deb
34fd2e7a7aa62ada2597cffd4529f086cb5058a6d7ac0048695254bd9fd882d7|libmd4c0_0.4.8-1build1_amd64.deb|pool/universe/m/md4c/libmd4c0_0.4.8-1build1_amd64.deb
da36bd4d30c13b40b697fa8b72cc09d309ff36a5bd9750341ef78492bf3f3966|libopengl0_1.7.0-1build1_amd64.deb|pool/main/libg/libglvnd/libopengl0_1.7.0-1build1_amd64.deb
458bcc5f69954fed963354f248ab5ab78b9a06f5823f9e2a4b88a6e32fa940c6|libqt6concurrent6t64_6.4.2+dfsg-21.1build5_amd64.deb|pool/universe/q/qt6-base/libqt6concurrent6t64_6.4.2+dfsg-21.1build5_amd64.deb
1f6e26589271c1d7aa549cb86d0156ff589b764d37edf356712fecebfe5f30f8|libqt6core6t64_6.4.2+dfsg-21.1build5_amd64.deb|pool/universe/q/qt6-base/libqt6core6t64_6.4.2+dfsg-21.1build5_amd64.deb
20891a752f6569b8b0772b0c81b7251eb3394fdef57bf52870f4825cb0e9c0de|libqt6dbus6t64_6.4.2+dfsg-21.1build5_amd64.deb|pool/universe/q/qt6-base/libqt6dbus6t64_6.4.2+dfsg-21.1build5_amd64.deb
f8774151f151a932d389817a371a3241004b1c3031cf53afd91feed73c8dd68e|libqt6gui6t64_6.4.2+dfsg-21.1build5_amd64.deb|pool/universe/q/qt6-base/libqt6gui6t64_6.4.2+dfsg-21.1build5_amd64.deb
743f289d5f7b1a4cafa021de62bcd705122bad06ba5d3a2c971fd7d34b87752f|libqt6network6t64_6.4.2+dfsg-21.1build5_amd64.deb|pool/universe/q/qt6-base/libqt6network6t64_6.4.2+dfsg-21.1build5_amd64.deb
dc7b43aa74d5694ab10e02bee7751c1bea0905ed6f0aaf527bff1952c582403f|libqt6opengl6t64_6.4.2+dfsg-21.1build5_amd64.deb|pool/universe/q/qt6-base/libqt6opengl6t64_6.4.2+dfsg-21.1build5_amd64.deb
68a328430e1d354532d718969c74f2df4579777375d3a47d5e2bc999819cdc9d|libqt6openglwidgets6t64_6.4.2+dfsg-21.1build5_amd64.deb|pool/universe/q/qt6-base/libqt6openglwidgets6t64_6.4.2+dfsg-21.1build5_amd64.deb
5e068edcc290d740bcbf34d2bcc7be8efb738f47bb2460849742ba221cbb7b15|libqt6printsupport6t64_6.4.2+dfsg-21.1build5_amd64.deb|pool/universe/q/qt6-base/libqt6printsupport6t64_6.4.2+dfsg-21.1build5_amd64.deb
35155bb0ef40d8769004f75a2a000029f86adfdd3cd76a14eeab2e68951f407e|libqt6sql6t64_6.4.2+dfsg-21.1build5_amd64.deb|pool/universe/q/qt6-base/libqt6sql6t64_6.4.2+dfsg-21.1build5_amd64.deb
7d0f27c84fab37ec9f574d8cb47f117d411bceffba2eada10436928bccb22537|libqt6test6t64_6.4.2+dfsg-21.1build5_amd64.deb|pool/universe/q/qt6-base/libqt6test6t64_6.4.2+dfsg-21.1build5_amd64.deb
9d5aba1eeb471c1230a94eb1865a95df0595ed8311971acf50fff2652452562d|libqt6widgets6t64_6.4.2+dfsg-21.1build5_amd64.deb|pool/universe/q/qt6-base/libqt6widgets6t64_6.4.2+dfsg-21.1build5_amd64.deb
7999c22f56a9b8d23e4d5e7cd7d0e679df581c8e72015baaa4e2b870746245ad|libqt6xml6t64_6.4.2+dfsg-21.1build5_amd64.deb|pool/universe/q/qt6-base/libqt6xml6t64_6.4.2+dfsg-21.1build5_amd64.deb
85005a23941c2ac634d02758ffcb7dd9ddaebe88aa15b3bcf4914ca3ee3774e7|qmake6-bin_6.4.2+dfsg-21.1build5_amd64.deb|pool/universe/q/qt6-base/qmake6-bin_6.4.2+dfsg-21.1build5_amd64.deb
2226948252b282854a26311054930964895863fdcab65151591ea11a1a1731e6|qmake6_6.4.2+dfsg-21.1build5_amd64.deb|pool/universe/q/qt6-base/qmake6_6.4.2+dfsg-21.1build5_amd64.deb
6d24069f67e2ffdde4b7295780ebc89673d7ee66a1ecf9cc383405d0d651fb23|qt6-base-dev-tools_6.4.2+dfsg-21.1build5_amd64.deb|pool/universe/q/qt6-base/qt6-base-dev-tools_6.4.2+dfsg-21.1build5_amd64.deb
f81ea0070597621c2a1027d753c343c8204a2fce601b1a92d6e264d909a32bfb|qt6-base-dev_6.4.2+dfsg-21.1build5_amd64.deb|pool/universe/q/qt6-base/qt6-base-dev_6.4.2+dfsg-21.1build5_amd64.deb
10999ac52ea568d6d4ea896b16ccf931b1eb2fd9e96e058a743f00e2e7f099d2|qt6-qpa-plugins_6.4.2+dfsg-21.1build5_amd64.deb|pool/universe/q/qt6-base/qt6-qpa-plugins_6.4.2+dfsg-21.1build5_amd64.deb
PACKAGES
}

fetch_debs() {
    mkdir -p "$CACHE"
    packages | while IFS='|' read -r sum file path; do
        dest=$CACHE/$file
        if [ -f "$dest" ] && verify_one "$sum" "$dest"; then
            continue
        fi
        rm -f "$dest"
        printf '== Fetching %s\n' "$file"
        if ! fetch "$BASE_URL/$path" "$dest.part"; then
            rm -f "$dest.part"
            die "download failed: $file"
        fi
        if ! verify_one "$sum" "$dest.part"; then
            rm -f "$dest.part"
            die "checksum mismatch: $file"
        fi
        mv "$dest.part" "$dest"
    done
}

rewrite_prefix() {
    for dir in \
        "$PREFIX/usr/lib/x86_64-linux-gnu/cmake" \
        "$PREFIX/usr/lib/x86_64-linux-gnu/pkgconfig" \
        "$PREFIX/usr/lib/x86_64-linux-gnu/qt6/mkspecs"
    do
        [ -d "$dir" ] || continue
        grep -rl --include='*.cmake' --include='*.conf' --include='*.pri' \
            --include='*.prl' --include='*.pc' -e '/usr/' "$dir" 2>/dev/null |
        while IFS= read -r file; do
            sed -i "s|/usr/|$PREFIX/usr/|g" "$file" ||
                die "could not rewrite $file"
        done
    done

    # pkg-config files declare the prefix without a trailing slash.
    pcdir=$PREFIX/usr/lib/x86_64-linux-gnu/pkgconfig
    if [ -d "$pcdir" ]; then
        grep -rl --include='*.pc' -e '^prefix=/usr$' "$pcdir" 2>/dev/null |
        while IFS= read -r file; do
            sed -i "s|^prefix=/usr$|prefix=$PREFIX/usr|" "$file" ||
                die "could not rewrite $file"
        done
    fi
}

provision() {
    fetch_debs

    printf '== Unpacking Qt 6.4 packages into %s\n' "$PREFIX"
    rm -rf "$PREFIX"
    mkdir -p "$PREFIX"
    packages | while IFS='|' read -r sum file path; do
        dpkg-deb -x "$CACHE/$file" "$PREFIX"
    done
    rewrite_prefix

    [ -f "$PREFIX/usr/lib/x86_64-linux-gnu/cmake/Qt6/Qt6Config.cmake" ] ||
        die "unpacking did not produce Qt6Config.cmake under $PREFIX/usr"
}

qt_version() {
    version=$(grep -m1 'set(PACKAGE_VERSION "' \
        "$PREFIX/usr/lib/x86_64-linux-gnu/cmake/Qt6Core/Qt6CoreConfigVersionImpl.cmake" 2>/dev/null |
        sed 's/.*"\([^"]*\)".*/\1/')
    printf '%s\n' "${version:-unknown}"
}

if [ "$refresh" = 1 ] ||
   [ ! -f "$PREFIX/usr/lib/x86_64-linux-gnu/cmake/Qt6/Qt6Config.cmake" ]; then
    provision
fi

export PATH=$PREFIX/usr/bin:$PATH
extra=${QT64_EXTRA_PREFIX:-}
if [ -z "$extra" ] && [ -f "$HOME/qt6/lib/libcurl.so" ]; then
    extra=$HOME/qt6
fi
if [ -n "$extra" ] && [ ! -d "$extra" ]; then
    die "QT64_EXTRA_PREFIX does not exist: $extra"
fi
export LD_LIBRARY_PATH=$PREFIX/usr/lib/x86_64-linux-gnu${extra:+:$extra/lib}${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}
export QT_PLUGIN_PATH=$PREFIX/usr/lib/x86_64-linux-gnu/qt6/plugins${QT_PLUGIN_PATH:+:$QT_PLUGIN_PATH}

version=$(qt_version)
printf '== Qt %s in %s/usr\n' "$version" "$PREFIX"

if [ "$setup_only" = 1 ]; then
    printf 'Setup only; CMake prefix: %s/usr\n' "$PREFIX"
    exit 0
fi

cmake_prefix=$PREFIX/usr
if [ -n "$extra" ]; then
    # CMake lists use ';', not ':' (the loader's separator). With ':' the
    # extra prefix is silently ignored and libcurl/libarchive/GL are missed.
    cmake_prefix="$cmake_prefix;$extra"
fi

printf '== Configuring build-qt64\n'
cmake -S "$ROOT" -B "$ROOT/build-qt64" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="$cmake_prefix" \
    -DLISTISTA_BUILD_TESTS=ON

# CMAKE_PREFIX_PATH wins over system paths on a fresh configure, but an old
# build-qt64/ cache would keep pointing at whatever Qt it was configured with.
cache=$ROOT/build-qt64/CMakeCache.txt
if ! grep -Fq "Qt6Core_DIR:PATH=$PREFIX/" "$cache"; then
    die "build-qt64 was configured against another Qt; remove build-qt64 (or use a fresh build directory) and rerun"
fi

printf '== Building with %s jobs\n' "$JOBS"
cmake --build "$ROOT/build-qt64" --parallel "$JOBS"

if [ "$run_tests" = 1 ]; then
    for test in "$ROOT"/build-qt64/tests/test_*; do
        [ -f "$test" ] && [ -x "$test" ] || continue
        printf '== Running %s\n' "${test##*/}"
        "$test"
    done
fi

printf '\nQt %s build ready: %s\n' "$version" "$ROOT/build-qt64/listista"
printf 'Run it off-screen with:\n'
printf '  QT_QPA_PLATFORM=offscreen %s --path /tmp --screenshot /tmp/listista-qt64.png\n' \
    "$ROOT/build-qt64/listista"
