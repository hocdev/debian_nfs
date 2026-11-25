#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
SRC_ROOT="$REPO_ROOT/src"
OUT_DIR="$REPO_ROOT/build/debs"

source "$SCRIPT_DIR/branding.env"
mkdir -p "$OUT_DIR"

echo "=== 1. Compiling reconstructed taskbar (libcdos-desktop-taskbar.so) ==="
make -C "$SCRIPT_DIR" clean all
strip --strip-unneeded "$SCRIPT_DIR/libcdos-desktop-taskbar.so"

build_pkg_from_src() {
  local name="$1"
  local out_deb="$2"
  local src_dir="$SRC_ROOT/$name"
  local stage="/tmp/stage-$name"

  rm -rf "$stage"
  cp -a "$src_dir" "$stage"
  chmod 0755 "$stage/DEBIAN"
  [ -f "$stage/DEBIAN/postinst" ] && chmod 0755 "$stage/DEBIAN/postinst" || true

  local installed_kb
  installed_kb=$(du -sk "$stage/usr" | awk '{print $1}')
  sed -i -e "s|^Maintainer:.*|Maintainer: $MAINTAINER_NAME <$MAINTAINER_EMAIL>|" \
         -e "s|^Installed-Size:.*|Installed-Size: $installed_kb|" \
         "$stage/DEBIAN/control"

  dpkg-deb --root-owner-group --build "$stage" "$out_deb"
  rm -rf "$stage"
}

echo "=== 2. Building found-desktop_4.0.5-m117+1nfs5_amd64.deb from src/ ==="
build_pkg_from_src "found-desktop" "$OUT_DIR/found-desktop_4.0.5-m117+1nfs5_amd64.deb"

echo "=== 3. Building found-desktop-common_4.0.5-m117+1nfs5_all.deb from src/ ==="
build_pkg_from_src "found-desktop-common" "$OUT_DIR/found-desktop-common_4.0.5-m117+1nfs5_all.deb"

echo "=== 4. Built packages in $OUT_DIR ==="
ls -lh "$OUT_DIR"
