#!/usr/bin/env bash
#
# scripts/build.sh — Single entry point for ShadowSSH builds.
#
# Usage:
#   scripts/build.sh                              # build every target supported on this host
#   scripts/build.sh --version 1.2.3              # stamp 1.2.3 into binary + bundle + metadata
#   scripts/build.sh --target macos-arm64         # build a single target
#   scripts/build.sh --target windows-x64 --version 1.2.3
#
# Supported targets:
#   macos-arm64        Apple Silicon .app + tarball
#   macos-x64          Intel .app + tarball
#   linux-x64          Linux x86_64 binary + tarball
#   linux-arm64        Linux aarch64 binary + tarball
#   windows-x64        Windows x86_64 .exe + zip
#
# Behavior:
#   - --version defaults to 0.0.0 when omitted.
#   - The version is injected at build time everywhere (binary, .app Info.plist,
#     archive filenames).
#   - Without --target, every target that can be built on the current host runs
#     sequentially; targets requiring a different OS are skipped with a warning
#     (real cross-target builds happen in CI).

set -euo pipefail

ROOT="$( cd "$( dirname "${BASH_SOURCE[0]}" )/.." && pwd )"
cd "$ROOT"

VERSION="0.0.0"
ONLY_TARGET=""
DIST_DIR="$ROOT/dist"

usage() {
    sed -n '2,25p' "${BASH_SOURCE[0]}"
    exit "${1:-0}"
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --version) VERSION="$2"; shift 2;;
        --target)  ONLY_TARGET="$2"; shift 2;;
        -h|--help) usage 0;;
        *) echo "Unknown argument: $1" >&2; usage 1;;
    esac
done

HOST_OS="$(uname -s)"
HOST_ARCH="$(uname -m)"

log()  { printf '\033[1;36m[build]\033[0m %s\n' "$*"; }
warn() { printf '\033[1;33m[skip]\033[0m  %s\n' "$*"; }
die()  { printf '\033[1;31m[fail]\033[0m  %s\n' "$*" >&2; exit 1; }

mkdir -p "$DIST_DIR"

# ---------- target implementations -----------------------------------------

build_unix_target() {
    # $1 = target id, $2 = cmake args, $3 = packaging callback
    local target="$1"
    local cmake_args=("$2")
    local pack_fn="$3"

    local build_dir="$ROOT/build/$target"
    rm -rf "$build_dir"
    mkdir -p "$build_dir"

    log "Configuring $target (version $VERSION)"
    # shellcheck disable=SC2086
    cmake -S "$ROOT" -B "$build_dir" \
        -DCMAKE_BUILD_TYPE=Release \
        -DSHADOWSSH_VERSION="$VERSION" \
        ${cmake_args[@]}

    log "Compiling $target"
    cmake --build "$build_dir" --parallel

    "$pack_fn" "$target" "$build_dir"
}

pack_macos() {
    local target="$1"
    local build_dir="$2"
    local app="$build_dir/ShadowSSH.app"
    [[ -d "$app" ]] || die ".app bundle missing at $app"

    local out="$DIST_DIR/ShadowSSH-$VERSION-$target"
    rm -rf "$out"
    mkdir -p "$out"
    cp -R "$app" "$out/"

    ( cd "$DIST_DIR" && tar -czf "ShadowSSH-$VERSION-$target.tar.gz" "ShadowSSH-$VERSION-$target" )
    log "Packaged $DIST_DIR/ShadowSSH-$VERSION-$target.tar.gz"
}

pack_linux() {
    local target="$1"
    local build_dir="$2"
    local bin="$build_dir/ShadowSSH"
    [[ -x "$bin" ]] || die "Linux binary missing at $bin"

    local out="$DIST_DIR/ShadowSSH-$VERSION-$target"
    rm -rf "$out"
    mkdir -p "$out"
    cp "$bin" "$out/"
    cp "$ROOT/LICENSE" "$ROOT/README.md" "$out/" 2>/dev/null || true

    ( cd "$DIST_DIR" && tar -czf "ShadowSSH-$VERSION-$target.tar.gz" "ShadowSSH-$VERSION-$target" )
    log "Packaged $DIST_DIR/ShadowSSH-$VERSION-$target.tar.gz"
}

pack_windows() {
    local target="$1"
    local build_dir="$2"
    local exe
    exe="$(find "$build_dir" -maxdepth 3 -name 'ShadowSSH.exe' | head -1)"
    [[ -n "$exe" ]] || die "Windows binary missing under $build_dir"

    local out="$DIST_DIR/ShadowSSH-$VERSION-$target"
    rm -rf "$out"
    mkdir -p "$out"
    cp "$exe" "$out/"
    cp "$ROOT/LICENSE" "$ROOT/README.md" "$out/" 2>/dev/null || true

    ( cd "$DIST_DIR" && \
      if command -v zip >/dev/null 2>&1; then
          zip -qr "ShadowSSH-$VERSION-$target.zip" "ShadowSSH-$VERSION-$target"
      else
          tar -czf "ShadowSSH-$VERSION-$target.tar.gz" "ShadowSSH-$VERSION-$target"
      fi )
    log "Packaged $DIST_DIR/ShadowSSH-$VERSION-$target archive"
}

build_macos_arm64() {
    [[ "$HOST_OS" == "Darwin" ]] || { warn "macos-arm64 requires macOS host"; return; }
    build_unix_target "macos-arm64" "-DCMAKE_OSX_ARCHITECTURES=arm64" pack_macos
}
build_macos_x64() {
    [[ "$HOST_OS" == "Darwin" ]] || { warn "macos-x64 requires macOS host"; return; }
    build_unix_target "macos-x64"   "-DCMAKE_OSX_ARCHITECTURES=x86_64" pack_macos
}
build_linux_x64() {
    [[ "$HOST_OS" == "Linux" ]] || { warn "linux-x64 requires a Linux host"; return; }
    build_unix_target "linux-x64" "" pack_linux
}
build_linux_arm64() {
    [[ "$HOST_OS" == "Linux" ]] || { warn "linux-arm64 requires a Linux host"; return; }
    if [[ "$HOST_ARCH" != "aarch64" && "$HOST_ARCH" != "arm64" ]]; then
        warn "linux-arm64 must run on an aarch64 Linux host (got $HOST_ARCH)"; return
    fi
    build_unix_target "linux-arm64" "" pack_linux
}
build_windows_x64() {
    if [[ "$HOST_OS" != MINGW* && "$HOST_OS" != MSYS* && "$HOST_OS" != CYGWIN* ]]; then
        warn "windows-x64 must run on a Windows host (msys/git-bash). Use CI for cross-host builds."
        return
    fi
    build_unix_target "windows-x64" "-G Ninja" pack_windows
}

# ---------- dispatch --------------------------------------------------------

run_target() {
    case "$1" in
        macos-arm64)  build_macos_arm64;;
        macos-x64)    build_macos_x64;;
        linux-x64)    build_linux_x64;;
        linux-arm64)  build_linux_arm64;;
        windows-x64)  build_windows_x64;;
        *) die "Unknown target '$1'";;
    esac
}

log "ShadowSSH build · version=$VERSION · host=$HOST_OS/$HOST_ARCH"

if [[ -n "$ONLY_TARGET" ]]; then
    run_target "$ONLY_TARGET"
else
    for t in macos-arm64 macos-x64 linux-x64 linux-arm64 windows-x64; do
        run_target "$t"
    done
fi

log "All artifacts in: $DIST_DIR"
