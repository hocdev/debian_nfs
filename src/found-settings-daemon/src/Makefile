CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra -fstack-protector-strong
CPPFLAGS ?= -D_FORTIFY_SOURCE=2
CUPS_CONFIG ?= cups-config
CUPS_CFLAGS ?= $(shell $(CUPS_CONFIG) --cflags)
CUPS_LIBS ?= $(shell $(CUPS_CONFIG) --libs)

DESTDIR ?= ../../../build/found-settings-daemon
LIBEXEC = $(DESTDIR)/usr/lib/found-settings-daemon
PLUGINDIR = $(DESTDIR)/usr/lib/cdos-settings-daemon-3.0
GNOME_DESKTOP_LIBRARY ?= ../../vos-compat-libs/usr/lib/x86_64-linux-gnu/libgnome-desktop-3.so.19.0.10
CSD_LIBRARY = $(PLUGINDIR)/libcsd.so
DAEMON = $(LIBEXEC)/cdos-settings-daemon
GTK = gtk+-3.0 x11
INPUT = $(GTK) xi
CORE_SOURCES = core/csd-desktop.c core/cdos-settings-plugin.c core/cdos-settings-profile.c core/cdos-settings-session.c
DAEMON_SOURCES = daemon/main.c daemon/cdos-settings-manager.c daemon/cdos-settings-module.c daemon/cdos-settings-plugin-info.c
PLUGINS = $(PLUGINDIR)/liba11y-settings.so $(PLUGINDIR)/libautomount.so $(PLUGINDIR)/libclipboard.so \
          $(PLUGINDIR)/libdevice-control.so $(PLUGINDIR)/libprint-notifications.so \
          $(PLUGINDIR)/libscreensaver-proxy.so $(PLUGINDIR)/libsound.so
PLUGIN_TESTS = $(LIBEXEC)/csd-test-a11y-settings $(LIBEXEC)/csd-test-automount $(LIBEXEC)/csd-test-device-control \
               $(LIBEXEC)/csd-test-print-notifications $(LIBEXEC)/csd-test-screensaver-proxy $(LIBEXEC)/csd-test-sound
A11Y_MANAGER = a11y-settings/csd-a11y-settings-manager.c a11y-settings/csd-a11y-settings-manager.h
AUTOMOUNT_MANAGER = automount/csd-automount-manager.c automount/csd-automount-manager.h automount/csd-autorun.c automount/csd-autorun.h
DEVICE_MANAGER = device-control/csd-device-control-manager.c device-control/csd-device-control-manager.h
PRINT_MANAGER = print-notifications/csd-print-notifications-manager.c print-notifications/csd-print-notifications-manager.h
SCREENSAVER_MANAGER = screensaver-proxy/csd-screensaver-proxy-manager.c screensaver-proxy/csd-screensaver-proxy-manager.h
SOUND_MANAGER = sound/csd-sound-manager.c sound/csd-sound-manager.h

.PHONY: all
all: $(CSD_LIBRARY) $(DAEMON) $(LIBEXEC)/csd-locate-pointer $(LIBEXEC)/csd-printer $(LIBEXEC)/csd-test-input-helper $(LIBEXEC)/csd-tlp-helper $(PLUGINS) $(PLUGIN_TESTS)

$(LIBEXEC) $(PLUGINDIR):
	mkdir -p "$@"

$(CSD_LIBRARY): $(CORE_SOURCES) $(wildcard core/*.h) common/config.h common/cdos-settings-plugin.h | $(PLUGINDIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Wno-deprecated-declarations -Wno-unused-parameter -Icommon $(shell pkg-config --cflags gio-2.0 libsystemd) \
	  $(CORE_SOURCES) $(LDFLAGS) -shared -fPIC -Wl,-z,defs -Wl,-soname,libcsd.so $(shell pkg-config --libs gio-2.0 libsystemd) -o "$@"

$(DAEMON): $(DAEMON_SOURCES) $(wildcard daemon/*.h) common/config.h common/cdos-settings-plugin.h $(CSD_LIBRARY) $(GNOME_DESKTOP_LIBRARY) | $(LIBEXEC)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Wno-deprecated-declarations -Wno-unused-parameter -Icommon $(shell pkg-config --cflags gtk+-3.0 libnotify gmodule-2.0) \
	  $(DAEMON_SOURCES) $(LDFLAGS) -Wl,-rpath,'$$ORIGIN/../cdos-settings-daemon-3.0' \
	  -L"$(PLUGINDIR)" -l:libcsd.so "$(GNOME_DESKTOP_LIBRARY)" $(shell pkg-config --libs gtk+-3.0 libnotify gmodule-2.0) -o "$@"

$(LIBEXEC)/csd-locate-pointer: locate-pointer/csd-locate-pointer.c locate-pointer/csd-timeline.c locate-pointer/csd-locate-pointer.h locate-pointer/csd-timeline.h | $(LIBEXEC)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Wno-deprecated-declarations -Wno-unused-parameter $(shell pkg-config --cflags $(GTK)) $(filter %.c,$^) $(LDFLAGS) $(shell pkg-config --libs $(GTK)) -lm -o "$@"

$(LIBEXEC)/csd-test-input-helper: input-helper/test-input-helper.c input-helper/csd-input-helper.c input-helper/csd-input-helper.h | $(LIBEXEC)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Wno-deprecated-declarations $(shell pkg-config --cflags $(INPUT)) $(filter %.c,$^) $(LDFLAGS) $(shell pkg-config --libs $(INPUT)) -o "$@"

$(LIBEXEC)/csd-tlp-helper: csd-tlp-helper.c | $(LIBEXEC)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(shell pkg-config --cflags glib-2.0) $< $(LDFLAGS) $(shell pkg-config --libs glib-2.0) -o "$@"

$(LIBEXEC)/csd-printer: print-notifications/csd-printer.c common/config.h | $(LIBEXEC)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Wno-deprecated-declarations -Wno-unused-parameter -Icommon $(CUPS_CFLAGS) $(shell pkg-config --cflags gtk+-3.0 libnotify) \
	  $< $(LDFLAGS) $(CUPS_LIBS) $(shell pkg-config --libs gtk+-3.0 libnotify) -o "$@"

$(PLUGINDIR)/liba11y-settings.so: $(A11Y_MANAGER) a11y-settings/csd-a11y-settings-plugin.c a11y-settings/csd-a11y-settings-plugin.h
$(PLUGINDIR)/libautomount.so: $(AUTOMOUNT_MANAGER) automount/csd-automount-plugin.c automount/csd-automount-plugin.h core/cdos-settings-session.h
$(PLUGINDIR)/libclipboard.so: clipboard/csd-clipboard-manager.c clipboard/csd-clipboard-manager.h clipboard/csd-clipboard-plugin.c clipboard/csd-clipboard-plugin.h clipboard/list.c clipboard/list.h clipboard/xutils.c clipboard/xutils.h
$(PLUGINDIR)/libdevice-control.so: $(DEVICE_MANAGER) device-control/csd-device-control-plugin.c
$(PLUGINDIR)/libprint-notifications.so: $(PRINT_MANAGER) print-notifications/csd-print-notifications-plugin.c print-notifications/csd-print-notifications-plugin.h
$(PLUGINDIR)/libscreensaver-proxy.so: $(SCREENSAVER_MANAGER) screensaver-proxy/csd-screensaver-proxy-plugin.c screensaver-proxy/csd-screensaver-proxy-plugin.h
$(PLUGINDIR)/libsound.so: $(SOUND_MANAGER) sound/csd-sound-plugin.c sound/csd-sound-plugin.h
$(PLUGINDIR)/libsound.so $(LIBEXEC)/csd-test-sound: AUDIO_LIB = libpulse
$(PLUGINDIR)/libprint-notifications.so $(LIBEXEC)/csd-test-print-notifications: PRINT_CFLAGS = $(CUPS_CFLAGS)
$(PLUGINDIR)/libprint-notifications.so $(LIBEXEC)/csd-test-print-notifications: PRINT_LIBS = $(CUPS_LIBS)
$(PLUGINDIR)/libprint-notifications.so: NOTIFY_LIB = libnotify
$(PLUGINS): common/config.h common/cdos-settings-plugin.h $(CSD_LIBRARY) | $(PLUGINDIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Wno-deprecated-declarations -Wno-unused-parameter -Icommon -Icore $(PRINT_CFLAGS) $(shell pkg-config --cflags $(GTK) gmodule-2.0 $(AUDIO_LIB) $(NOTIFY_LIB)) $(filter %.c,$^) \
	  $(LDFLAGS) -shared -fPIC -Wl,-z,defs -Wl,-rpath,'$$ORIGIN' -L"$(PLUGINDIR)" -l:libcsd.so $(PRINT_LIBS) $(shell pkg-config --libs $(GTK) gmodule-2.0 $(AUDIO_LIB) $(NOTIFY_LIB)) -o "$@"

$(LIBEXEC)/csd-test-a11y-settings: $(A11Y_MANAGER) a11y-settings/test-a11y-settings.c
$(LIBEXEC)/csd-test-automount: $(AUTOMOUNT_MANAGER) automount/test-automount.c core/cdos-settings-session.h $(CSD_LIBRARY)
$(LIBEXEC)/csd-test-automount: TEST_FEATURES = x11
$(LIBEXEC)/csd-test-automount: TEST_LINK = -Wl,-rpath,'$$ORIGIN/../cdos-settings-daemon-3.0' -L"$(PLUGINDIR)" -l:libcsd.so
$(LIBEXEC)/csd-test-device-control: $(DEVICE_MANAGER) device-control/test-device-control.c
$(LIBEXEC)/csd-test-print-notifications: $(PRINT_MANAGER) print-notifications/test-print-notifications.c
$(LIBEXEC)/csd-test-screensaver-proxy: $(SCREENSAVER_MANAGER) screensaver-proxy/test-screensaver-proxy.c
$(LIBEXEC)/csd-test-sound: $(SOUND_MANAGER) sound/test-sound.c
$(PLUGIN_TESTS): $(LIBEXEC)/csd-test-%: common/config.h common/test-plugin.h | $(LIBEXEC)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Wno-deprecated-declarations -Wno-unused-parameter -Icommon -Icore -DPLUGIN_NAME='"$*"' \
	  $(PRINT_CFLAGS) $(shell pkg-config --cflags gtk+-3.0 libnotify $(AUDIO_LIB) $(TEST_FEATURES)) $(filter %.c,$^) \
	  $(LDFLAGS) $(TEST_LINK) $(PRINT_LIBS) $(shell pkg-config --libs gtk+-3.0 libnotify $(AUDIO_LIB) $(TEST_FEATURES)) -o "$@"
