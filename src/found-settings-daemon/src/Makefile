CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra -fstack-protector-strong
CPPFLAGS ?= -D_FORTIFY_SOURCE=2

DESTDIR ?= ../../../build/found-settings-daemon
LIBEXEC = $(DESTDIR)/usr/lib/found-settings-daemon
PLUGINDIR = $(DESTDIR)/usr/lib/cdos-settings-daemon-3.0
CSD_LIBDIR ?= ../usr/lib/cdos-settings-daemon-3.0
GTK = gtk+-3.0 x11
INPUT = $(GTK) xi
PLUGINS = $(PLUGINDIR)/liba11y-settings.so $(PLUGINDIR)/libclipboard.so \
          $(PLUGINDIR)/libscreensaver-proxy.so $(PLUGINDIR)/libsound.so
PLUGIN_TESTS = $(LIBEXEC)/csd-test-a11y-settings $(LIBEXEC)/csd-test-screensaver-proxy $(LIBEXEC)/csd-test-sound
A11Y_MANAGER = a11y-settings/csd-a11y-settings-manager.c a11y-settings/csd-a11y-settings-manager.h
SCREENSAVER_MANAGER = screensaver-proxy/csd-screensaver-proxy-manager.c screensaver-proxy/csd-screensaver-proxy-manager.h
SOUND_MANAGER = sound/csd-sound-manager.c sound/csd-sound-manager.h

.PHONY: all
all: $(LIBEXEC)/csd-locate-pointer $(LIBEXEC)/csd-test-input-helper $(LIBEXEC)/csd-tlp-helper $(PLUGINS) $(PLUGIN_TESTS)

$(LIBEXEC) $(PLUGINDIR):
	mkdir -p "$@"

$(LIBEXEC)/csd-locate-pointer: locate-pointer/csd-locate-pointer.c locate-pointer/csd-timeline.c locate-pointer/csd-locate-pointer.h locate-pointer/csd-timeline.h | $(LIBEXEC)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Wno-deprecated-declarations -Wno-unused-parameter $(shell pkg-config --cflags $(GTK)) $(filter %.c,$^) $(LDFLAGS) $(shell pkg-config --libs $(GTK)) -lm -o "$@"

$(LIBEXEC)/csd-test-input-helper: input-helper/test-input-helper.c input-helper/csd-input-helper.c input-helper/csd-input-helper.h | $(LIBEXEC)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Wno-deprecated-declarations $(shell pkg-config --cflags $(INPUT)) $(filter %.c,$^) $(LDFLAGS) $(shell pkg-config --libs $(INPUT)) -o "$@"

$(LIBEXEC)/csd-tlp-helper: csd-tlp-helper.c | $(LIBEXEC)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(shell pkg-config --cflags glib-2.0) $< $(LDFLAGS) $(shell pkg-config --libs glib-2.0) -o "$@"

$(PLUGINDIR)/liba11y-settings.so: $(A11Y_MANAGER) a11y-settings/csd-a11y-settings-plugin.c a11y-settings/csd-a11y-settings-plugin.h
$(PLUGINDIR)/libclipboard.so: clipboard/csd-clipboard-manager.c clipboard/csd-clipboard-manager.h clipboard/csd-clipboard-plugin.c clipboard/csd-clipboard-plugin.h clipboard/list.c clipboard/list.h clipboard/xutils.c clipboard/xutils.h
$(PLUGINDIR)/libscreensaver-proxy.so: $(SCREENSAVER_MANAGER) screensaver-proxy/csd-screensaver-proxy-plugin.c screensaver-proxy/csd-screensaver-proxy-plugin.h
$(PLUGINDIR)/libsound.so: $(SOUND_MANAGER) sound/csd-sound-plugin.c sound/csd-sound-plugin.h
$(PLUGINDIR)/libsound.so $(LIBEXEC)/csd-test-sound: AUDIO_LIB = libpulse
$(PLUGINS): common/config.h common/cdos-settings-plugin.h $(CSD_LIBDIR)/libcsd.so | $(PLUGINDIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Wno-deprecated-declarations -Wno-unused-parameter -Icommon $(shell pkg-config --cflags $(GTK) gmodule-2.0 $(AUDIO_LIB)) $(filter %.c,$^) \
	  $(LDFLAGS) -shared -fPIC -Wl,-z,defs -Wl,-rpath,'$$ORIGIN' -L"$(CSD_LIBDIR)" -l:libcsd.so $(shell pkg-config --libs $(GTK) gmodule-2.0 $(AUDIO_LIB)) -o "$@"

$(LIBEXEC)/csd-test-a11y-settings: $(A11Y_MANAGER) a11y-settings/test-a11y-settings.c
$(LIBEXEC)/csd-test-screensaver-proxy: $(SCREENSAVER_MANAGER) screensaver-proxy/test-screensaver-proxy.c
$(LIBEXEC)/csd-test-sound: $(SOUND_MANAGER) sound/test-sound.c
$(PLUGIN_TESTS): $(LIBEXEC)/csd-test-%: common/config.h common/test-plugin.h | $(LIBEXEC)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Wno-deprecated-declarations -Wno-unused-parameter -Icommon -DPLUGIN_NAME='"$*"' \
	  $(shell pkg-config --cflags gtk+-3.0 libnotify $(AUDIO_LIB)) $(filter %.c,$^) $(LDFLAGS) $(shell pkg-config --libs gtk+-3.0 libnotify $(AUDIO_LIB)) -o "$@"
