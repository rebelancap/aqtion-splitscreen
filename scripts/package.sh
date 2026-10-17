#!/bin/bash
# Clean release build + release-candidate tarball (Linux counterpart of
# scripts/package.ps1):
#   build/aqtion-splitscreen-<VERSION>-linux-x86_64.tar.gz
#
# Runs scripts/build.sh --clean (release, in the aqdev distrobox), checks
# build/linux/BUILD-INFO.txt (version = VERSION, configuration: release) and
# packs the two files a player gets, at the archive root, owner 0:0:
#
#   q2pro-splitscreen  our client (stripped, mode 0755). It hosts with
#                      Steam's action/gamex86_64.so (legacy game API 4,
#                      src/server/game.c), so no game library is shipped.
#   README.txt         scripts/release/README.txt with @VERSION@ replaced
#                      (the same file and substitution as package.ps1; LF
#                      line ends here, CRLF in the Windows zip)
#
# The binary links the system's shared SDL2 / libcurl / libpng / libjpeg /
# zlib / libGL / Wayland / X11 (BUILD-INFO.txt "runtime"); release binaries
# for the Steam Deck come from CI on Ubuntu 22.04 (glibc 2.35), see
# docs/BUILD-AND-PROVENANCE.md. The release number and the GitHub release
# are the maintainer's call; this only builds the candidate for VERSION.
#
# usage: scripts/package.sh [--no-build]   (--no-build: pack build/linux as is)
set -e
REPO="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$REPO/build/linux"

NOBUILD=0
case "$1" in
  --no-build) NOBUILD=1 ;;
  "") ;;
  *) echo "usage: $0 [--no-build]" >&2; exit 2 ;;
esac
[ "$NOBUILD" = 1 ] || "$REPO/scripts/build.sh" --clean

VERSION="$(head -n1 "$REPO/VERSION" | tr -d '[:space:]')"
NAME="aqtion-splitscreen-$VERSION-linux-x86_64"
TARBALL="$REPO/build/$NAME.tar.gz"

for f in q2pro-splitscreen BUILD-INFO.txt; do
  [ -f "$OUT/$f" ] || { echo "build/linux/$f is missing: run without --no-build" >&2; exit 1; }
done
grep -qx "version: $VERSION" "$OUT/BUILD-INFO.txt" || {
  echo "build/linux/BUILD-INFO.txt is $(sed -n 's/^version: //p' "$OUT/BUILD-INFO.txt"), VERSION says $VERSION: rebuild" >&2; exit 1; }
grep -qx "configuration: release" "$OUT/BUILD-INFO.txt" || {
  echo "build/linux/BUILD-INFO.txt is not a release build: run without --no-build" >&2; exit 1; }
# the version is also in the binary's banner string
grep -aq "$VERSION" "$OUT/q2pro-splitscreen" || { echo "q2pro-splitscreen does not contain $VERSION: rebuild" >&2; exit 1; }

STAGE="$REPO/build/pkg-$NAME"
rm -rf "$STAGE" "$TARBALL"
mkdir -p "$STAGE"
install -m 0755 "$OUT/q2pro-splitscreen" "$STAGE/q2pro-splitscreen"
sed "s/@VERSION@/$VERSION/g" "$REPO/scripts/release/README.txt" > "$STAGE/README.txt"
chmod 0644 "$STAGE/README.txt"
tar -C "$STAGE" --owner=0 --group=0 --numeric-owner -czf "$TARBALL" q2pro-splitscreen README.txt
rm -rf "$STAGE"

echo "packaged $TARBALL ($(du -k "$TARBALL" | cut -f1) KiB)"
tar tzvf "$TARBALL"
