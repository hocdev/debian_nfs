# Phục dựng Taskbar (`found-desktop`)

Thư mục này chứa tài liệu, mã nguồn dịch ngược (Ghidra 12.2 via REA) và mã nguồn C phục dựng cho **Taskbar** của `found-desktop` (`4.0.5-m117+1nfs5`).

## Nguồn `.deb`
- `found-desktop_4.0.5-m117+1nfs5_amd64.deb`:
  - `/usr/lib/x86_64-linux-gnu/found-desktop/libcdos-desktop.so` (chứa toàn bộ logic Panel & Window List bằng C/GObject/Clutter/St)
  - `/usr/share/found-desktop/cdos-desktop-theme.gresource` (`style.css` và các icon `window-list/*.png`)
- `found-desktop-common_4.0.5-m117+1nfs5_all.deb`:
  - `/usr/share/found-desktop/applets/window-list@cdos.org/metadata.json`
  - `/usr/share/found-desktop/applets/panel-launchers@cdos.org/metadata.json`
  - `/usr/share/glib-2.0/schemas/org.cdos.gschema.xml`

## Kiến trúc Taskbar
1. **`CdosPanel`** (`cdos-panel.h`, `cdos-panel.c`):
   - Thanh panel chính (`#panel`, `.panel-bottom` / `.panel-top`, cao 40px) gồm `#panelLeft`, `#panelCenter`, `#panelRight`.
   - Context menu (chuột phải): *Show desktop*, *Open the system monitor*, *Stack windows*, *Abreast windows*, *Placed at the top of the screen* (`desktop-layout`), *Auto hide panel* (`panel-autohide`).
2. **`AppletWindowList` & `AppletAppList` & `AppletAppGroup`** (`applet-window-list.h`, `applet-window-list.c`):
   - Quản lý danh sách ứng dụng ghim (favorite) và cửa sổ đang mở theo workspace hiện tại.
   - Nút ứng dụng (`AppletAppButton`, `.window-list-app-button`) tự đổi class theo số lượng cửa sổ (`window-list-item-box-one-active`, `window-list-item-box-two-active`, `window-list-item-box-more-active`) và trạng thái `focus`.
   - Hover menu (`AppletAppThumbnailHoverMenu`, `.window-list-hover-menu`) hiển thị thumbnail cửa sổ khi rê chuột.
