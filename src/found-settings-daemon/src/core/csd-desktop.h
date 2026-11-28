#ifndef CSD_DESKTOP_H
#define CSD_DESKTOP_H

#include <gio/gio.h>

gboolean csd_is_wayland_session(void);
gboolean is_found_desktop_env(void);
gboolean g_settings_has_key(GSettings *gsettings, const char *key);
const char *power_action_type_to_string(int action);
char *csd_settings_get_chassis_type(void);

GDBusProxy *csd_get_key_grabber_proxy(void);
GDBusProxy *csd_get_power_mode_proxy(void);
GDBusProxy *csd_get_power_brightness_proxy(void);
GDBusProxy *csd_get_shortcuts_proxy(void);
GDBusProxy *csd_get_keyboard_backlight_proxy(void);
GDBusProxy *csd_get_touchpad_mode_proxy(void);
GDBusProxy *csd_get_session_proxy(void);

#endif
