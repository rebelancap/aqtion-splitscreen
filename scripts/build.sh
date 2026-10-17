#!/bin/bash
# Builds the aqtion-splitscreen client for Linux x86_64 with Meson into
# build/linux/ (the Meson build dir; the Windows box uses build/ itself for
# MSVC, so the Linux one lives one level down and the two never mix).
#
# Bazzite is immutable, so the compile runs inside the "aqdev" distrobox
# (Fedora 43 + gcc/meson/ninja/SDL2-devel/...; create line in STATUS.md /
# docs/LINUX-BOOTSTRAP.md).  Run from the host: the script re-enters itself
# in the box.  Without distrobox (a CI runner with the -dev packages
# installed) it runs meson directly.  The binary links the host's shared
# libSDL2 / libcurl / libpng / libjpeg / zlib / libGL / libwayland / libX11,
# so it runs on the host.
#
# Outputs in build/linux/:
#   q2pro-splitscreen   client (copy of q2pro, stripped in release builds)
#   q2pro, q2proded     Meson's own outputs (q2proded is not shipped)
#   gamex86_64.so       our src/action build, dev runs via sys_forcegamelib only
#   BUILD-INFO.txt      our record only, not shipped
#
# usage: scripts/build.sh [--clean] [debug] [extra meson setup args...]
set -e
REPO="$(cd "$(dirname "$0")/.." && pwd)"
BOX="${AQDEV_BOX:-aqdev}"

CLEAN=0
if [ "$1" = "--clean" ]; then CLEAN=1; shift; fi
BUILDTYPE=release
if [ "$1" = "debug" ]; then BUILDTYPE=debugoptimized; shift; fi

VERSION="$(head -n1 "$REPO/VERSION" | tr -d '[:space:]')"
case "$VERSION" in
  [0-9]*.[0-9]*.[0-9]*) ;;
  *) echo "Bad VERSION '$VERSION'" >&2; exit 1 ;;
esac
if [ -n "$AQ_BUILD_COMMIT" ]; then
  COMMIT="$AQ_BUILD_COMMIT"
else
  COMMIT="$(git -C "$REPO" rev-parse --short HEAD 2>/dev/null || echo unknown)"
  [ -n "$(git -C "$REPO" status --porcelain --untracked-files=no 2>/dev/null)" ] && COMMIT="$COMMIT-dirty"
fi

if [ -z "$CONTAINER_ID" ] && command -v distrobox >/dev/null 2>&1; then
  if ! distrobox list 2>/dev/null | grep -q " $BOX "; then
    echo "distrobox '$BOX' not found; create it as in STATUS.md (Current state, Linux)" >&2
    exit 1
  fi
  echo "== aqtion-splitscreen $VERSION ($COMMIT) $BUILDTYPE linux-x86_64 (in distrobox $BOX) =="
  exec distrobox enter "$BOX" -- env AQ_BUILD_COMMIT="$COMMIT" "$REPO/scripts/build.sh" \
    $([ "$CLEAN" = 1 ] && echo --clean) $([ "$BUILDTYPE" = debugoptimized ] && echo debug) "$@"
fi

for tool in meson ninja gcc pkg-config python3; do
  command -v "$tool" >/dev/null 2>&1 || { echo "$tool not found (install the build prerequisites)" >&2; exit 1; }
done

OUT="$REPO/build/linux"

# Upstream's Linux CI set (.github/workflows/build.yml MESON_ARGS_LINUX, minus
# --auto-features/--fatal-meson-warnings) plus the splitscreen options of
# scripts/build.ps1. System libraries only (nofallback: nothing is
# downloaded); gamepads reuse the engine's SDL2 (meson.build). No system-wide
# install: basedir/libdir default to ".", no default homedir (the exe locates
# itself, src/unix/system.c split_self_locate).
MESON_OPTIONS=(
  "--buildtype=$BUILDTYPE"
  -Dwrap_mode=nofallback
  -Daqtion-build=true
  -Dgamepad=enabled
  -Dsystem-wide=false
  -Dhomedir=
  -Ddiscord-sdk=false
  -Davcodec=disabled
  -Dsdl2=enabled
  -Dx11=enabled
  -Dwayland=enabled
  -Dwindows-crash-dumps=disabled
  -Dtests=false
  -Dwerror=false
)

cd "$REPO"
if [ "$CLEAN" = 1 ]; then rm -rf "$OUT"; fi
START=$(date +%s)
if [ -f "$OUT/build.ninja" ]; then
  meson setup --reconfigure "$OUT" "${MESON_OPTIONS[@]}" "$@"
else
  meson setup "$OUT" "${MESON_OPTIONS[@]}" "$@"
fi
meson compile -C "$OUT"
END=$(date +%s)

install -m 0755 "$OUT/q2pro" "$OUT/q2pro-splitscreen"
[ "$BUILDTYPE" = release ] && strip "$OUT/q2pro-splitscreen"

{
  echo "aqtion-splitscreen"
  echo "version: $VERSION"
  echo "git: $COMMIT"
  echo "configuration: $BUILDTYPE"
  echo "built: $(date '+%Y-%m-%d %H:%M:%S %z')"
  echo "toolchain: $(gcc --version | head -n1), meson $(meson --version), SDL2 $(pkg-config --modversion sdl2 2>/dev/null || echo '?') headers, ${CONTAINER_ID:+distrobox }${CONTAINER_ID:-host}"
  echo "meson options: ${MESON_OPTIONS[*]} $*"
  echo "q2pro-splitscreen: client$([ "$BUILDTYPE" = release ] && echo ' (stripped)'); q2proded and gamex86_64.so: dev only, not shipped"
  echo "runtime: host shared libSDL2-2.0.so.0, libcurl, libpng, libjpeg, libz, libGL/libEGL, libwayland, libX11 (gamepads through the engine's SDL2)"
  echo "build time: $((END - START)) s"
} > "$OUT/BUILD-INFO.txt"

echo "== OK: $OUT"
cat "$OUT/BUILD-INFO.txt"
ls -l "$OUT/q2pro-splitscreen" "$OUT/q2pro" "$OUT/q2proded" "$OUT/gamex86_64.so" 2>/dev/null || true
