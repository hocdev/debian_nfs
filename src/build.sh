#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
SRC_ROOT="$REPO_ROOT/src"
TASKBAR_DIR="$SRC_ROOT/found-desktop/taskbar"
OUT_DIR="$REPO_ROOT/build/debs"

source "$SCRIPT_DIR/branding.env"
mkdir -p "$OUT_DIR"

echo "=== 1. Compiling reconstructed C sources ==="
make -C "$TASKBAR_DIR" clean all
strip --strip-unneeded "$TASKBAR_DIR/libcdos-desktop-taskbar.so"

make -C "$SRC_ROOT/libfound-gtk3/src" clean all
strip --strip-unneeded "$SRC_ROOT/libfound-gtk3/src/libfound-gtk3.so.0.0.0"
mkdir -p "$SRC_ROOT/libfound-gtk3/usr/lib/x86_64-linux-gnu"
cp "$SRC_ROOT/libfound-gtk3/src/libfound-gtk3.so.0.0.0" "$SRC_ROOT/libfound-gtk3/usr/lib/x86_64-linux-gnu/libfound-gtk3.so.0.0.0"

make -C "$SRC_ROOT/libfound-desktop-4/src" clean all
strip --strip-unneeded "$SRC_ROOT/libfound-desktop-4/src/libfound-desktop-4-reconstructed.so"

make -C "$SRC_ROOT/nemo-share/src" clean all
strip --strip-unneeded "$SRC_ROOT/nemo-share/src/libnemo-share.so"
mkdir -p "$SRC_ROOT/nemo-share/usr/lib/x86_64-linux-gnu/nemo/extensions-3.0"
cp "$SRC_ROOT/nemo-share/src/libnemo-share.so" "$SRC_ROOT/nemo-share/usr/lib/x86_64-linux-gnu/nemo/extensions-3.0/libnemo-share.so"

build_pkg_from_src() {
  local name="$1"
  local deb_file="$2"
  local out_deb="$OUT_DIR/$deb_file"
  local src_dir="$SRC_ROOT/$name"
  local stage="/tmp/stage-$name"

  rm -rf "$stage"
  cp -a "$src_dir" "$stage"
  rm -rf "$stage/src" "$stage/taskbar"
  if [ -f "$stage/symlinks.list" ]; then
    while IFS=$'\t' read -r link_rel link_target; do
      [ -z "$link_rel" ] && continue
      mkdir -p "$stage/$(dirname "$link_rel")"
      ln -sfn "$link_target" "$stage/$link_rel"
    done < "$stage/symlinks.list"
    rm -f "$stage/symlinks.list"
  fi
  find "$stage" -type d -exec chmod 0755 {} +
  chmod 0755 "$stage/DEBIAN"
  for s in preinst postinst prerm postrm; do
    [ -f "$stage/DEBIAN/$s" ] && chmod 0755 "$stage/DEBIAN/$s" || true
  done
  for d in usr/bin usr/sbin usr/libexec; do
    if [ -d "$stage/$d" ]; then
      find "$stage/$d" -type f -exec chmod 0755 {} +
    fi
  done
  find "$stage" -type f ! -path "$stage/DEBIAN/*" -exec sh -c '
    for f do
      head -c 4 "$f" 2>/dev/null | grep -qE "^(\x7fELF|#!)" && chmod 0755 "$f" || true
    done
  ' sh {} +

  local size_dirs=""
  for d in usr etc lib var opt; do
    [ -d "$stage/$d" ] && size_dirs="$size_dirs $stage/$d" || true
  done
  local installed_kb=0
  if [ -n "$size_dirs" ]; then
    installed_kb=$(du -sk $size_dirs | awk '{sum+=$1} END{print sum}')
  fi

  sed -i -e "s|^Maintainer:.*|Maintainer: $MAINTAINER_NAME <$MAINTAINER_EMAIL>|" \
         -e "s|^Installed-Size:.*|Installed-Size: $installed_kb|" \
         "$stage/DEBIAN/control"

  dpkg-deb --root-owner-group --build "$stage" "$out_deb"
  rm -rf "$stage"
}

echo "=== 2. Building packages from src/ ==="
build_pkg_from_src "found-desktop"              "found-desktop_4.0.5-m117+1nfs5_amd64.deb"
build_pkg_from_src "found-desktop-common"       "found-desktop-common_4.0.5-m117+1nfs5_all.deb"
build_pkg_from_src "found-desktop-environment"  "found-desktop-environment_4.0.6-m1+1nfs5_all.deb"
build_pkg_from_src "found-ui-scaling"           "found-ui-scaling_1.0.0-m1+1nfs5_all.deb"
build_pkg_from_src "found-livecd-customization" "found-livecd-customization_1.0.0-1+0nfs5_amd64.deb"
build_pkg_from_src "nfs-custom-jjw"             "nfs-custom-jjw_1.0.1-m5+1nfs5_all.deb"
build_pkg_from_src "cdos-plymouth-theme-nfslogo" "cdos-plymouth-theme-nfslogo_2.11-m13+1nfs5_all.deb"
build_pkg_from_src "found-gsettings-schemas"    "found-gsettings-schemas_4.0.1-m5+1nfs5_all.deb"
build_pkg_from_src "libfound-desktop-data"      "libfound-desktop-data_4.0.10-m1+2nfs5_all.deb"
build_pkg_from_src "libfound-gtk-data"          "libfound-gtk-data_4.0.0-m3+1nfs5_all.deb"
build_pkg_from_src "found-themes"               "found-themes_4.0.6-m4+1nfs5_all.deb"
build_pkg_from_src "found-session-common"       "found-session-common_3.22.7-m5+1nfs5_all.deb"
build_pkg_from_src "mutter-common"              "mutter-common_3.36.11-m38+1nfs5_all.deb"
build_pkg_from_src "nemo-data"                  "nemo-data_4.8.6-m262+1nfs5_all.deb"
build_pkg_from_src "xapps-common"               "xapps-common_2.0.7-1+nfs5_all.deb"
build_pkg_from_src "found-backgrounds"          "found-backgrounds_4.0.2-m3+1nfs5_all.deb"
build_pkg_from_src "found-control-center-data"  "found-control-center-data_1_3.38.5-m120+1nfs5_all.deb"
build_pkg_from_src "found-icon-theme"           "found-icon-theme_4.0.6-m10+1nfs5_all.deb"
build_pkg_from_src "libfound-gtk3"              "libfound-gtk3_4.0.0-m3+1nfs5_amd64.deb"
build_pkg_from_src "libfound-desktop-4"         "libfound-desktop-4_4.0.10-m1+2nfs5_amd64.deb"
build_pkg_from_src "gir1.2-xapp-1.0"            "gir1.2-xapp-1.0_2.0.7-1+nfs5_amd64.deb"
build_pkg_from_src "gir1.2-mutter-6"            "gir1.2-mutter-6_3.36.11-m38+1nfs5_amd64.deb"
build_pkg_from_src "gir1.2-nemo-3.0"            "gir1.2-nemo-3.0_4.8.6-m262+1nfs5_amd64.deb"
build_pkg_from_src "nemo-share"                 "nemo-share_3.2.0+2-m11+1nfs5_amd64.deb"

echo "=== 3. Built packages in $OUT_DIR ==="
ls -lh "$OUT_DIR"
