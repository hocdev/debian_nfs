#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORK_DIR="${ROOT_DIR}/build"
OUT_DIR="${ROOT_DIR}/out"

rm -rf "${WORK_DIR}"
mkdir -p "${WORK_DIR}" "${OUT_DIR}"

cp -a "${ROOT_DIR}/live-build/config" "${WORK_DIR}/config"
mkdir -p "${WORK_DIR}/config/packages.chroot"
cp -f "${ROOT_DIR}/repo/pool/main/"*.deb "${WORK_DIR}/config/packages.chroot/"
chmod +x "${WORK_DIR}/config/hooks/live/"*.hook.chroot

cd "${WORK_DIR}"

lb clean --purge || true

lb config \
  --mode debian \
  --distribution bookworm \
  --archive-areas "main contrib non-free non-free-firmware" \
  --debian-installer false \
  --architectures amd64 \
  --linux-flavours amd64 \
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
