# VOS

ISO **VOS** (amd64) hỗ trợ chạy Live và cài đặt. VOS dùng nền Bookworm và môi trường desktop **Found Desktop (CDOS)**.

## Thành phần chính

- **Desktop Shell & Taskbar (`src/`)**: `found-desktop`, `found-desktop-common`, `found-desktop-environment` (`cdos-desktop`, mã nguồn phục dựng taskbar `src/taskbar/`, applets thanh tác vụ `menu@cdos.org`, `window-list@cdos.org`, `systray@cdos.org`, `sound@cdos.org`, `network@cdos.org`, `calendar@cdos.org`, `notifications@cdos.org`, `show-desktop@cdos.org`)
- **Window Manager & Session**: `mutter` (3.36), `mutter-common`, `libmutter-6-0`, `gir1.2-mutter-6`, `found-session`, `found-session-common` (`cdos-session`, `cdos-xorg.desktop`)
- **Settings & Control Center**: `found-settings-daemon`, `found-control-center`, `found-control-center-data`, `found-gsettings-schemas`, `found-tools`, `found-ui-scaling`, `found-livecd-customization`
- **File Manager & Desktop Icons**: `nemo`, `nemo-data`, `libnemo-extension1`, `gir1.2-nemo-3.0`, `nemo-print` (`nemo-desktop`)
- **Themes, Icons, Backgrounds, Boot Splash & Branding**: `found-themes`, `found-icon-theme`, `found-icon-theme-dark`, `found-backgrounds`, `cdos-plymouth-theme-voslogo`, `vos-custom-jjw`, cùng ảnh nền, logo và watermark VOS.
- **Tương thích Bullseye và Bookworm**: `vos-compat-libs` bổ sung các thư viện SONAME cũ (`libcroco-0.6.so.3`, `libgnome-desktop-3.so.19`, `libtracker-sparql-2.0.so.0` + `tracker-2.0`, `libwacom.so.2`, `libssl.so.1.1`/`libcrypto.so.1.1`, `libbfd-2.35.2-system.so`, `libicudata/icui18n/icuuc.so.67`, `libgeocode-glib.so.0`, `libsm_devctl.so.1`) để chạy các thành phần VOS trên Bookworm.

## Cấu trúc thư mục

- [repo/pool/main/](repo/pool/main/): Các gói `.deb` tương thích với Bookworm.
- [src/](src/): Cây gói `.deb` và mã C phục dựng. Build tích hợp hàm lấy thư mục launcher vào `cdos-desktop` và hai hàm kiểm tra môi trường vào `libfound-desktop-4`. Các phần còn lại của hai thư viện vẫn cần binary Found hiện có để cung cấp đủ ABI. [Công cụ ELF của Found Control Center](src/found-control-center/src/found-control-center-elf.c) tái tạo bản VOS từ ELF gốc trong commit `dd4e1c4`. Trên WSL có thể dùng gói Found gốc đã giải nén bằng cách đặt `FOUND_CONTROL_CENTER_ORIGINAL_DIR` khi chạy `bash src/build.sh`.
- Mã C của các helper lấy từ [GNOME Control Center 3.38.5](https://download.gnome.org/sources/gnome-control-center/3.38/), [GNOME Session 3.22.2](https://download.gnome.org/sources/gnome-session/3.22/) và [Mutter 3.36.9](https://download.gnome.org/sources/mutter/3.36/). Giấy phép chung của các mã nguồn này nằm trong [COPYING](COPYING). `mem-usage` được phục dựng theo hành vi của bản Found.
- [tools/vos-exporter/](tools/vos-exporter/): Công cụ Rust + Python hỗ trợ đóng gói và trích xuất tài nguyên hệ thống.
- [live-build/](live-build/): Cấu hình `live-build` (`bookworm`, `amd64`, LightDM mặc định phiên `cdos-xorg`, Calamares, hook `ldd` kiểm tra liên kết động).
- [.github/workflows/build-iso.yml](.github/workflows/build-iso.yml): Workflow GitHub Actions tự động build ISO trong container `debian:bookworm` và tải artifact chứa `vos-desktop-amd64.iso`.

## Build bằng GitHub Actions

1. Khởi tạo git (nếu chưa có), commit toàn bộ repository và push lên GitHub (`main`).
2. Vào tab **Actions** trên GitHub để theo dõi workflow **Build VOS ISO**. Khi chạy qua **Run workflow**, có thể nhập `release_tag` để tạo GitHub Release kèm tệp `.iso` và `.sha256`.
3. Tải artifact `vos-desktop-amd64-iso` sau khi build hoàn tất.

## Cập nhật hệ thống

`apt update` tải danh mục từ các kho Bookworm chính thức, gồm kho bảo mật. ISO tự tạo quy tắc APT từ danh sách gói VOS đã đóng gói. Khi chạy `apt upgrade`, APT bỏ qua bản Debian của các gói đó và cập nhật các gói hệ thống còn lại.
