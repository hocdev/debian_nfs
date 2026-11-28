#include "csd-desktop.h"

#include <glib/gi18n.h>

static const char keyboard_service[] = "com.vos.KeyboardGrabber";

static GDBusProxy *
get_proxy(GDBusProxy **cache, GBusType bus, GDBusProxyFlags flags,
          const char *name, const char *path, const char *interface,
          const char *function, gboolean ref_cached)
{
    if (*cache)
        return ref_cached ? g_object_ref(*cache) : *cache;

    g_autoptr(GError) error = NULL;
    *cache = g_dbus_proxy_new_for_bus_sync(bus, flags, NULL, name, path, interface,
                                          NULL, &error);
    if (*cache) {
        g_object_add_weak_pointer(G_OBJECT(*cache), (gpointer *) cache);
    } else if (ref_cached) {
        g_warning("Failed to connect to the session manager: %s", error->message);
    } else {
        g_warning("%s: %s", function, error->message);
    }
    return *cache;
}

GDBusProxy *
csd_get_key_grabber_proxy(void)
{
    static GDBusProxy *proxy;
    return get_proxy(&proxy, G_BUS_TYPE_SESSION, G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
                     "org.freedesktop.KeyGrabber", "/org/freedesktop/KeyGrabber",
                     "org.freedesktop.KeyGrabber", G_STRFUNC, FALSE);
}

GDBusProxy *
csd_get_power_mode_proxy(void)
{
    static GDBusProxy *proxy;
    return get_proxy(&proxy, G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE,
                     keyboard_service, "/com/vos/PowerMode", "com.vos.PowerMode", G_STRFUNC, FALSE);
}

GDBusProxy *
csd_get_power_brightness_proxy(void)
{
    static GDBusProxy *proxy;
    return get_proxy(&proxy, G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE,
                     keyboard_service, "/com/vos/Brightness", "com.vos.Brightness", G_STRFUNC, FALSE);
}

GDBusProxy *
csd_get_shortcuts_proxy(void)
{
    static GDBusProxy *proxy;
    return get_proxy(&proxy, G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE,
                     keyboard_service, "/com/vos/Shortcuts", "com.vos.Shortcuts", G_STRFUNC, FALSE);
}

GDBusProxy *
csd_get_keyboard_backlight_proxy(void)
{
    static GDBusProxy *proxy;
    return get_proxy(&proxy, G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE,
                     keyboard_service, "/com/vos/KbdBacklightMode", "com.vos.KbdBacklightMode", G_STRFUNC, FALSE);
}

GDBusProxy *
csd_get_touchpad_mode_proxy(void)
{
    static GDBusProxy *proxy;
    return get_proxy(&proxy, G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE,
                     keyboard_service, "/com/vos/TouchpadMode", "com.vos.TouchpadMode", G_STRFUNC, FALSE);
}

GDBusProxy *
csd_get_session_proxy(void)
{
    static GDBusProxy *proxy;
    return get_proxy(&proxy, G_BUS_TYPE_SESSION, G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
                     "org.gnome.SessionManager", "/org/gnome/SessionManager",
                     "org.gnome.SessionManager", G_STRFUNC, TRUE);
}

char *
csd_settings_get_chassis_type(void)
{
    const char service[] = "org.freedesktop.hostname1";
    g_autoptr(GError) error = NULL;
    g_autoptr(GDBusConnection) bus = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
    if (!bus) {
        g_warning("system bus not available: %s", error->message);
        return NULL;
    }

    g_autoptr(GVariant) reply = g_dbus_connection_call_sync(
        bus, service, "/org/freedesktop/hostname1", "org.freedesktop.DBus.Properties", "Get",
        g_variant_new("(ss)", service, "Chassis"), G_VARIANT_TYPE("(v)"),
        G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);
    if (!reply) {
        g_debug("Failed to get property '%s': %s", "Chassis", error->message);
        return NULL;
    }

    g_autoptr(GVariant) value = NULL;
    g_variant_get(reply, "(v)", &value);
    return g_variant_dup_string(value, NULL);
}

gboolean
csd_is_wayland_session(void)
{
    return FALSE;
}

gboolean
is_found_desktop_env(void)
{
    const char *desktop = g_getenv("XDG_CURRENT_DESKTOP");
    return desktop && (g_ascii_strcasecmp(desktop, "found") == 0 ||
                       g_ascii_strcasecmp(desktop, "cdos") == 0);
}

gboolean
g_settings_has_key(GSettings *gsettings, const char *key)
{
    g_return_val_if_fail(G_IS_SETTINGS(gsettings), FALSE);
    g_return_val_if_fail(key != NULL, FALSE);

    g_autoptr(GSettingsSchema) schema = NULL;
    g_object_get(gsettings, "settings-schema", &schema, NULL);
    return g_settings_schema_has_key(schema, key);
}

const char *
power_action_type_to_string(int action)
{
    static const char *const labels[] = {
        N_("lock screen"), N_("suspend"), N_("shutdown"), N_("hibernate"), N_("interactive")
    };
    return (guint) action < G_N_ELEMENTS(labels) ? _(labels[action]) : NULL;
}
