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
make -C "$SRC_ROOT/libfound-gtk3/src" clean all
strip --strip-unneeded "$SRC_ROOT/libfound-gtk3/src/libfound-gtk3.so.0.0.0"
mkdir -p "$SRC_ROOT/libfound-gtk3/usr/lib/x86_64-linux-gnu"
cp "$SRC_ROOT/libfound-gtk3/src/libfound-gtk3.so.0.0.0" "$SRC_ROOT/libfound-gtk3/usr/lib/x86_64-linux-gnu/libfound-gtk3.so.0.0.0"

make -C "$SRC_ROOT/nemo-share/src" clean all
strip --strip-unneeded "$SRC_ROOT/nemo-share/src/libnemo-share.so"
mkdir -p "$SRC_ROOT/nemo-share/usr/lib/x86_64-linux-gnu/nemo/extensions-3.0"
cp "$SRC_ROOT/nemo-share/src/libnemo-share.so" "$SRC_ROOT/nemo-share/usr/lib/x86_64-linux-gnu/nemo/extensions-3.0/libnemo-share.so"

make -C "$SRC_ROOT/vos-installer/src" clean all
strip --strip-unneeded "$SRC_ROOT/vos-installer/src/vos-installer"
mkdir -p "$SRC_ROOT/vos-installer/usr/bin"
cp "$SRC_ROOT/vos-installer/src/vos-installer" "$SRC_ROOT/vos-installer/usr/bin/vos-installer"

build_pkg_from_src() {
  local name="$1"
  local deb_file="$2"
  local out_deb="$OUT_DIR/$deb_file"
  local src_dir="$SRC_ROOT/$name"
  local stage="/tmp/stage-$name"

  rm -rf "$stage"
  cp -a "$src_dir" "$stage"
  rm -rf "$stage/src" "$stage/taskbar"
  case "$name" in
    found-desktop)
      local launcher_lib="libcdos-panel-launchers.so.0"
      local desktop_libdir="$stage/usr/lib/x86_64-linux-gnu/found-desktop"
      # Put the source implementation first in the shell's ELF lookup order.
      gcc -O2 -fPIC -shared -Wl,-z,defs -Wl,-soname,"$launcher_lib" \
        $(pkg-config --cflags gio-unix-2.0 gtk+-3.0) \
        "$TASKBAR_DIR/panel-launchers-directory.c" "$TASKBAR_DIR/app-button-style.c" \
        $(pkg-config --libs gtk+-3.0) -ldl -o "$desktop_libdir/$launcher_lib"
      chmod 0644 "$desktop_libdir/$launcher_lib"
      patchelf --add-needed "$launcher_lib" "$stage/usr/bin/cdos-desktop"
      local first_needed
      first_needed=$(patchelf --print-needed "$stage/usr/bin/cdos-desktop")
      if [ "${first_needed%%$'\n'*}" != "$launcher_lib" ]; then
        echo "ERROR: $launcher_lib must be the first dependency of cdos-desktop" >&2
        return 1
      fi
      ;;
    libfound-desktop-4)
      local libdir="$stage/usr/lib/x86_64-linux-gnu"
      local library="libfound-desktop-4.so.0.0.0"
      local legacy="libfound-desktop-4-legacy.so.0"
      # Keep unresolved API calls available through the original implementation.
      mv "$libdir/$library" "$libdir/$legacy"
      patchelf --set-soname "$legacy" "$libdir/$legacy"
      gcc -O2 -fPIC -shared -Wl,-z,defs -Wl,-soname,libfound-desktop-4.so.0 \
        -Wl,-rpath,'$ORIGIN' $(pkg-config --cflags gio-2.0) \
        "$src_dir/src/found-desktop-environment.c" $(pkg-config --libs glib-2.0) \
        -L"$libdir" -Wl,--no-as-needed -l:"$legacy" -Wl,--as-needed -o "$libdir/$library"
      chmod 0644 "$libdir/$library" "$libdir/$legacy"
      ;;
  esac
  if [ -f "$stage/case_collisions.list" ]; then
    while IFS=$'\t' read -r orig_rel stored_rel; do
      [ -z "$orig_rel" ] && continue
      mkdir -p "$stage/$(dirname "$orig_rel")"
      mv "$stage/$stored_rel" "$stage/$orig_rel"
    done < "$stage/case_collisions.list"
    rm -rf "$stage/_case_collisions" "$stage/case_collisions.list"
  fi
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
  python3 - "$stage" <<'PY'
import os, sys

stage = sys.argv[1]
debian_dir = os.path.join(stage, "DEBIAN")
for root, _, files in os.walk(stage):
    if root == debian_dir or root.startswith(debian_dir + os.sep):
        continue
    for name in files:
        path = os.path.join(root, name)
        if os.path.islink(path):
            continue
        try:
            with open(path, "rb") as f:
                magic = f.read(4)
        except OSError:
            continue
        if magic == b"\x7fELF" or magic.startswith(b"#!"):
            if name.endswith(".so") or ".so." in name:
                continue
            os.chmod(path, 0o755)
PY

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
build_pkg_from_src "found-desktop"              "found-desktop_4.0.5-m117+1vos5_amd64.deb"
build_pkg_from_src "found-desktop-common"       "found-desktop-common_4.0.5-m117+1vos5_all.deb"
build_pkg_from_src "found-desktop-environment"  "found-desktop-environment_4.0.6-m1+1vos5_all.deb"
build_pkg_from_src "found-ui-scaling"           "found-ui-scaling_1.0.0-m1+1vos5_all.deb"
build_pkg_from_src "found-livecd-customization" "found-livecd-customization_1.0.0-1+0vos5_amd64.deb"
build_pkg_from_src "vos-custom-jjw"             "vos-custom-jjw_1.0.1-m5+1vos5_all.deb"
build_pkg_from_src "cdos-plymouth-theme-voslogo" "cdos-plymouth-theme-voslogo_2.11-m13+1vos5_all.deb"
build_pkg_from_src "found-gsettings-schemas"    "found-gsettings-schemas_4.0.1-m5+1vos5_all.deb"
build_pkg_from_src "libfound-desktop-data"      "libfound-desktop-data_4.0.10-m1+2vos5_all.deb"
build_pkg_from_src "libfound-gtk-data"          "libfound-gtk-data_4.0.0-m3+1vos5_all.deb"
build_pkg_from_src "found-themes"               "found-themes_4.0.6-m4+1vos5_all.deb"
build_pkg_from_src "found-session-common"       "found-session-common_3.22.7-m5+1vos5_all.deb"
build_pkg_from_src "mutter-common"              "mutter-common_3.36.11-m38+1vos5_all.deb"
build_pkg_from_src "nemo-data"                  "nemo-data_4.8.6-m262+1vos5_all.deb"
build_pkg_from_src "xapps-common"               "xapps-common_2.0.7-1+vos5_all.deb"
build_pkg_from_src "found-backgrounds"          "found-backgrounds_4.0.2-m3+1vos5_all.deb"
build_pkg_from_src "found-control-center-data"  "found-control-center-data_1_3.38.5-m120+1vos5_all.deb"
build_pkg_from_src "found-icon-theme"           "found-icon-theme_4.0.6-m10+1vos5_all.deb"
build_pkg_from_src "libfound-gtk3"              "libfound-gtk3_4.0.0-m3+1vos5_amd64.deb"
build_pkg_from_src "libfound-desktop-4"         "libfound-desktop-4_4.0.10-m1+2vos5_amd64.deb"
build_pkg_from_src "gir1.2-xapp-1.0"            "gir1.2-xapp-1.0_2.0.7-1+vos5_amd64.deb"
build_pkg_from_src "gir1.2-mutter-6"            "gir1.2-mutter-6_3.36.11-m38+1vos5_amd64.deb"
build_pkg_from_src "gir1.2-nemo-3.0"            "gir1.2-nemo-3.0_4.8.6-m262+1vos5_amd64.deb"
build_pkg_from_src "nemo-share"                 "nemo-share_3.2.0+2-m11+1vos5_amd64.deb"
build_pkg_from_src "libxapp1"                   "libxapp1_2.0.7-1+vos5_amd64.deb"
build_pkg_from_src "nemo-print"                 "nemo-print_1.0.5-m2+1vos5_amd64.deb"
build_pkg_from_src "mutter"                     "mutter_3.36.11-m38+1vos5_amd64.deb"
build_pkg_from_src "libnemo-extension1"         "libnemo-extension1_4.8.6-m262+1vos5_amd64.deb"
build_pkg_from_src "found-session"              "found-session_3.22.7-m5+1vos5_amd64.deb"
build_pkg_from_src "found-tools"                "found-tools_4.0.0-m18+1vos5_amd64.deb"
build_pkg_from_src "found-settings-daemon"      "found-settings-daemon_4.0.4-m19+1vos5_amd64.deb"
build_pkg_from_src "libmutter-6-0"              "libmutter-6-0_3.36.11-m38+1vos5_amd64.deb"
build_pkg_from_src "found-control-center"       "found-control-center_1_3.38.5-m120+1vos5_amd64.deb"
build_pkg_from_src "nemo"                       "nemo_4.8.6-m262+1vos5_amd64.deb"
build_pkg_from_src "vos-compat-libs"            "vos-compat-libs_1.0+vos5_amd64.deb"
build_pkg_from_src "found-icon-theme-dark"      "found-icon-theme-dark_4.0.6-m10+1vos5_all.deb"
build_pkg_from_src "vos-installer"              "vos-installer_1.0.0+1vos5_amd64.deb"

echo "=== 3. Built packages in $OUT_DIR ==="
ls -lh "$OUT_DIR"
