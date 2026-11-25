# Debian 12 NFS Desktop

Bản dựng **Debian 12 (Bookworm) Live + Installable ISO (amd64)** thay thế hoàn toàn môi trường desktop, taskbar, trình quản lý cửa sổ, session, trung tâm điều khiển, trình quản lý tệp, theme, icon, hình nền và Plymouth boot splash gốc bằng bộ giao diện **Found Desktop (CDOS)** và các tùy biến được xuất trực tiếp từ `NFSDesktop-5.0-G240-251209-amd64.iso` kết hợp bộ `branding_edit` từ `C:\Users\Administrator\Desktop\NFS`.

## Thành phần đã xuất từ NFSDesktop-5.0

- **Desktop Shell & Taskbar**: `found-desktop`, `found-desktop-common`, `found-desktop-environment` (`cdos-desktop`, applets thanh tác vụ `menu@cdos.org`, `window-list@cdos.org`, `systray@cdos.org`, `sound@cdos.org`, `network@cdos.org`, `calendar@cdos.org`, `notifications@cdos.org`, `show-desktop@cdos.org`)
- **Window Manager & Session**: `mutter` (3.36), `mutter-common`, `libmutter-6-0`, `gir1.2-mutter-6`, `found-session`, `found-session-common` (`cdos-session`, `cdos-xorg.desktop`)
- **Settings & Control Center**: `found-settings-daemon`, `found-control-center`, `found-control-center-data`, `found-gsettings-schemas`, `found-tools`, `found-ui-scaling`, `found-livecd-customization`
- **File Manager & Desktop Icons**: `nemo`, `nemo-data`, `libnemo-extension1`, `gir1.2-nemo-3.0`, `nemo-print` (`nemo-desktop`)
- **Themes, Icons, Backgrounds, Boot Splash & Branding**: `found-themes`, `found-icon-theme`, `found-icon-theme-dark`, `found-backgrounds`, `cdos-plymouth-theme-nfslogo`, `nfs-custom-jjw`, cùng các tệp ghi đè từ `branding_edit` (logo menu, watermark đã chỉnh sửa, Calamares Debian branding, `isolinux/splash.png`)
- **Tương thích Debian 11 -> Debian 12**: `nfs-compat-libs` đóng gói các thư viện SONAME cũ (`libcroco-0.6.so.3`, `libgnome-desktop-3.so.19`, `libtracker-sparql-2.0.so.0` + `tracker-2.0`, `libwacom.so.2`, `libssl.so.1.1`/`libcrypto.so.1.1`, `libbfd-2.35.2-system.so`, `libicudata/icui18n/icuuc.so.67`, `libgeocode-glib.so.0`, `libsm_devctl.so.1`) mà không xung đột với thư viện gốc của Debian 12.

## Cấu trúc thư mục

- [repo/pool/main/](repo/pool/main/): 32 gói `.deb` đã xuất và điều chỉnh phụ thuộc cho Debian 12 Bookworm.
- [tools/nfs-exporter/](tools/nfs-exporter/): Công cụ Rust + Python để trích xuất lại các gói từ `NFSDesktop-5.0-G240-251209-amd64.iso` và `branding_edit`.
- [live-build/](live-build/): Cấu hình `live-build` (`bookworm`, `amd64`, LightDM mặc định phiên `cdos-xorg`, Calamares, hook `ldd` kiểm tra liên kết động).
- [.github/workflows/build-iso.yml](.github/workflows/build-iso.yml): Workflow GitHub Actions tự động build ISO trong container `debian:bookworm` và tải artifact `debian-12-nfs-desktop-amd64.iso`.

## Build bằng GitHub Actions

1. Khởi tạo git (nếu chưa có), commit toàn bộ repository và push lên GitHub (`main`).
2. Vào tab **Actions** trên GitHub để theo dõi workflow **Build Debian 12 NFS Desktop ISO** (hoặc chạy thủ công qua **Run workflow**, có thể nhập `release_tag` để tạo GitHub Release kèm tệp `.iso` và `.sha256`).
3. Tải artifact `debian-12-nfs-desktop-amd64-iso` sau khi build hoàn tất.
