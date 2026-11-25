#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORK_DIR="${ROOT_DIR}/build"
OUT_DIR="${ROOT_DIR}/out"
REPO_DIR="${ROOT_DIR}/repo"
CACHE_DIR="${ROOT_DIR}/.lb-cache"

mkdir -p "${OUT_DIR}" "${CACHE_DIR}"

# Build APT repository metadata for local NFS packages
cd "${REPO_DIR}"
dpkg-scanpackages --multiversion pool/main /dev/null > Packages
gzip -9c Packages > Packages.gz
apt-ftparchive \
  -o APT::FTPArchive::Release::Origin="DebianNFS" \
  -o APT::FTPArchive::Release::Label="DebianNFS" \
  release . > Release

# Serve local APT repo over HTTP so live-build chroot apt-get resolves all dependencies cleanly
python3 -m http.server 18123 --bind 127.0.0.1 --directory "${REPO_DIR}" >/tmp/nfs-apt-http.log 2>&1 &
HTTP_PID=$!
cleanup() {
    kill "${HTTP_PID}" >/dev/null 2>&1 || true
    if [ -d "${WORK_DIR}/cache" ]; then
        # Pack bootstrap rootfs as root so actions/cache preserves uid/gid, setuid, and /tmp 1777
        if [ -d "${WORK_DIR}/cache/bootstrap" ]; then
            tar --numeric-owner --zstd -cpf "${WORK_DIR}/cache/bootstrap.tar.zst.tmp" -C "${WORK_DIR}/cache" bootstrap \
              && mv -f "${WORK_DIR}/cache/bootstrap.tar.zst.tmp" "${WORK_DIR}/cache/bootstrap.tar.zst" \
              || rm -f "${WORK_DIR}/cache/bootstrap.tar.zst.tmp"
            rm -rf "${WORK_DIR}/cache/bootstrap"
        fi
        rm -rf "${CACHE_DIR}"
        mv "${WORK_DIR}/cache" "${CACHE_DIR}" || true
    fi
    if [ -d "${CACHE_DIR}" ]; then
        chown -R "$(stat -c '%u:%g' "${ROOT_DIR}")" "${CACHE_DIR}" >/dev/null 2>&1 || true
        chmod -R u+rwX,go+rX "${CACHE_DIR}" >/dev/null 2>&1 || true
    fi
}
trap cleanup EXIT

for _ in $(seq 1 30); do
    if python3 -c "import urllib.request; urllib.request.urlopen('http://127.0.0.1:18123/Release', timeout=1)" >/dev/null 2>&1; then
        break
    fi
    sleep 0.2
done

rm -rf "${WORK_DIR}"
mkdir -p "${WORK_DIR}"
if [ -d "${CACHE_DIR}" ] && [ "$(ls -A "${CACHE_DIR}" 2>/dev/null || true)" ]; then
    # Purge only local NFS packages from cache so updated .deb files are always used
    find "${CACHE_DIR}" -type f \( \
        -name "found-*" -o -name "libfound-*" -o -name "mutter*" -o -name "libmutter-6-0*" \
        -o -name "gir1.2-mutter-6*" -o -name "nemo*" -o -name "libnemo-*" -o -name "gir1.2-nemo-*" \
        -o -name "libxapp1*" -o -name "xapps-common*" -o -name "gir1.2-xapp-1.0*" \
        -o -name "nfs-*" -o -name "cdos-*" \
    \) -delete || true
    mv "${CACHE_DIR}" "${WORK_DIR}/cache"
    if [ -f "${WORK_DIR}/cache/bootstrap.tar.zst" ]; then
        rm -rf "${WORK_DIR}/cache/bootstrap"
        tar --numeric-owner --zstd -xpf "${WORK_DIR}/cache/bootstrap.tar.zst" -C "${WORK_DIR}/cache" \
          || rm -rf "${WORK_DIR}/cache/bootstrap"
        rm -f "${WORK_DIR}/cache/bootstrap.tar.zst"
    fi
fi

cp -a "${ROOT_DIR}/live-build/config" "${WORK_DIR}/config"
chmod +x "${WORK_DIR}/config/hooks/live/"*.hook.chroot

# Build isolated libgoa stubs so found-control-center (libsoup-2.4) does not pull Debian 12 libsoup-3.0
COMPAT_GOA_DIR="${WORK_DIR}/config/includes.chroot/usr/lib/found-control-center-compat"
mkdir -p "${COMPAT_GOA_DIR}"
cat > "${WORK_DIR}/goa_stub.c" <<'EOF'
#include <glib-object.h>
GType goa_provider_get_type(void) { return g_object_get_type(); }
void goa_account_call_remove(void) {}
gboolean goa_account_call_remove_finish(void) { return FALSE; }
gboolean goa_account_call_remove_sync(void) { return FALSE; }
const gchar *goa_account_get_id(void) { return NULL; }
gboolean goa_account_get_is_locked(void) { return FALSE; }
const gchar *goa_account_get_presentation_identity(void) { return NULL; }
gpointer goa_account_get_provider_icon(void) { return NULL; }
const gchar *goa_account_get_provider_name(void) { return NULL; }
const gchar *goa_account_get_provider_type(void) { return NULL; }
GList *goa_client_get_accounts(void) { return NULL; }
gpointer goa_client_new_sync(void) { return NULL; }
gpointer goa_object_peek_account(void) { return NULL; }
gpointer goa_provider_add_account(void) { return NULL; }
void goa_provider_get_all(void) {}
gboolean goa_provider_get_all_finish(void) { return FALSE; }
gpointer goa_provider_get_for_provider_type(void) { return NULL; }
guint goa_provider_get_provider_features(void) { return 0; }
gpointer goa_provider_get_provider_icon(void) { return NULL; }
const gchar *goa_provider_get_provider_name(void) { return NULL; }
void goa_provider_show_account(void) {}
EOF
gcc -shared -fPIC -Wl,-soname,libgoa-1.0.so.0 "${WORK_DIR}/goa_stub.c" $(pkg-config --cflags --libs gobject-2.0) -o "${COMPAT_GOA_DIR}/libgoa-1.0.so.0"
gcc -shared -fPIC -Wl,-soname,libgoa-backend-1.0.so.1 "${WORK_DIR}/goa_stub.c" $(pkg-config --cflags --libs gobject-2.0) -o "${COMPAT_GOA_DIR}/libgoa-backend-1.0.so.1"
rm -f "${WORK_DIR}/goa_stub.c"

cd "${WORK_DIR}"

lb clean --binary || true
rm -rf "${WORK_DIR}/chroot" "${WORK_DIR}/binary" "${WORK_DIR}/.build"

lb config \
  --mode debian \
  --distribution bookworm \
  --archive-areas "main contrib non-free non-free-firmware" \
  --debian-installer false \
  --architectures amd64 \
  --linux-flavours amd64 \
  --cache true \
  --cache-indices true \
  --cache-packages true \
  --cache-stages "bootstrap" \
  --bootappend-live "boot=live components quiet splash locales=en_US.UTF-8,vi_VN.UTF-8" \
  --iso-application "Debian 12 NFS Desktop" \
  --iso-publisher "Debian NFS" \
  --iso-volume "DEBIAN_NFS_12" \
  --memtest none \
  --checksums sha256 \
  --zsync false

lb build

ISO_FILE="$(ls -1 "${WORK_DIR}"/*.iso | head -n 1)"
if [ -z "${ISO_FILE}" ]; then
    echo "ERROR: No ISO produced by live-build"
    exit 1
fi

OUT_ISO="${OUT_DIR}/debian-12-nfs-desktop-amd64.iso"
mv -f "${ISO_FILE}" "${OUT_ISO}"
(cd "${OUT_DIR}" && sha256sum "$(basename "${OUT_ISO}")" > "$(basename "${OUT_ISO}").sha256")
ls -lh "${OUT_DIR}"
