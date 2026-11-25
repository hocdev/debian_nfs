/**
 * Reconstructed from libcdos-desktop.so (found-desktop 4.0.5-m117+1nfs5)
 * Decompiled via REA + Ghidra 12.2
 *
 * Key addresses:
 *   applet_panel_launchers_get_dirname           @ 0x1e2060
 *   applet_panel_launchers_remove_invalid_files  @ 0x1e22e0
 *   applet_panel_launchers_update_files          @ 0x1e2470
 *   applet_panel_launchers_create_icon_texture   @ 0x1e24e0
 *   applet_panel_launchers_get_display_name      @ 0x1e25e0
 *   applet_panel_launchers_get_uri               @ 0x1e26a0
 *   applet_panel_launchers_edit_properties       @ 0x1e2710
 */

#include "applet-panel-launchers.h"

extern gpointer st_icon_new(void);
extern GType st_icon_get_type(void);
extern void st_icon_set_icon_size(gpointer icon, gint size);
extern void st_icon_set_fallback_icon_name(gpointer icon, const gchar *name);
extern void st_icon_set_gicon(gpointer icon, GIcon *gicon);

static GAppInfo *
panel_launchers_lookup_app_info(const gchar *desktop_id)
{
    if (desktop_id == NULL) {
        return NULL;
    }
    GDesktopAppInfo *info = g_desktop_app_info_new(desktop_id);
    if (info != NULL) {
        return (GAppInfo *)info;
    }
    gchar *dir = applet_panel_launchers_get_dirname();
    gchar *path = g_build_filename(dir, desktop_id, NULL);
    info = g_desktop_app_info_new_from_filename(path);
    g_free(path);
    g_free(dir);
    return (GAppInfo *)info;
}

gchar *
applet_panel_launchers_get_dirname(void)
{
    gchar *dir = g_build_filename(g_get_user_data_dir(), "/cdos/panel-launchers/", NULL);
    if (!g_file_test(dir, G_FILE_TEST_IS_DIR)) {
        g_mkdir_with_parents(dir, 0755);
    }
    return dir;
}

void
applet_panel_launchers_remove_invalid_files(void)
{
    gchar *dir = applet_panel_launchers_get_dirname();
    GDir *d = g_dir_open(dir, 0, NULL);
    if (d != NULL) {
        const gchar *name;
        while ((name = g_dir_read_name(d)) != NULL) {
            gchar *path = g_build_filename(dir, name, NULL);
            if (g_str_has_suffix(name, ".desktop")) {
                GDesktopAppInfo *info = g_desktop_app_info_new_from_filename(path);
                if (info == NULL) {
                    g_unlink(path);
                } else {
                    g_object_unref(info);
                }
            }
            g_free(path);
        }
        g_dir_close(d);
    }
    g_free(dir);
}

void
applet_panel_launchers_update_files(void)
{
    applet_panel_launchers_remove_invalid_files();
}

gpointer
applet_panel_launchers_create_icon_texture(gpointer cdos_app, gint icon_size)
{
    g_return_val_if_fail(G_TYPE_CHECK_INSTANCE_TYPE(cdos_app, cdos_app_get_type()), NULL);

    const gchar *app_id = cdos_app_get_id(cdos_app);
    GAppInfo *info = panel_launchers_lookup_app_info(app_id);

    gpointer icon = st_icon_new();
    st_icon_set_icon_size(icon, icon_size);
    st_icon_set_fallback_icon_name(icon, "application-x-executable");

    if (info != NULL) {
        GIcon *gicon = g_app_info_get_icon(info);
        if (gicon != NULL) {
            st_icon_set_gicon(icon, gicon);
        }
        g_object_unref(info);
    }

    return icon;
}

gchar *
applet_panel_launchers_get_display_name(gpointer cdos_app)
{
    g_return_val_if_fail(G_TYPE_CHECK_INSTANCE_TYPE(cdos_app, cdos_app_get_type()), NULL);

    const gchar *app_id = cdos_app_get_id(cdos_app);
    GAppInfo *info = panel_launchers_lookup_app_info(app_id);
    if (info == NULL) {
        return NULL;
    }

    const gchar *name = g_app_info_get_display_name(info);
    gchar *result = name != NULL ? g_strdup(name) : NULL;
    g_object_unref(info);
    return result;
}

gchar *
applet_panel_launchers_get_uri(gpointer cdos_app)
{
    g_return_val_if_fail(G_TYPE_CHECK_INSTANCE_TYPE(cdos_app, cdos_app_get_type()), NULL);
    const gchar *app_id = cdos_app_get_id(cdos_app);
    if (app_id == NULL) {
        return NULL;
    }
    gchar *dir = applet_panel_launchers_get_dirname();
    gchar *path = g_build_filename(dir, app_id, NULL);
    g_free(dir);
    return path;
}

void
applet_panel_launchers_edit_properties(gpointer cdos_app)
{
    g_return_if_fail(G_TYPE_CHECK_INSTANCE_TYPE(cdos_app, cdos_app_get_type()));
}
