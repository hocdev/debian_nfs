/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 William Jon McCann <mccann@jhu.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street - Suite 500, Boston, MA 02110-1335, USA.
 *
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib-object.h>
#include <gio/gio.h>

/* ABI from the packaged libgnome-desktop-3.so.19. */
typedef struct _GnomePnpIds GnomePnpIds;
GnomePnpIds *gnome_pnp_ids_new(void);

#include "cdos-settings-plugin-info.h"
#include "cdos-settings-manager.h"

#define CSD_MANAGER_DBUS_PATH "/org/freedesktop/SettingsDaemon"
#define CSD_MANAGER_DBUS_NAME "org.freedesktop.SettingsDaemon"

#define DEFAULT_SETTINGS_PREFIX "org.cdos.settings-daemon"

#define PLUGIN_EXT ".cdos-settings-plugin"

#define CDOS_SETTINGS_MANAGER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CDOS_TYPE_SETTINGS_MANAGER, CdosSettingsManagerPrivate))

static const gchar introspection_xml[] =
"<node name='/org/freedesktop/SettingsDaemon'>"
"  <interface name='org.freedesktop.SettingsDaemon'>"
"    <annotation name='org.freedesktop.DBus.GLib.CSymbol' value='found_settings_manager'/>"
"    <method name='ToggleDebug'></method>"
"    <signal name='PluginActivated'>"
"      <arg name='name' type='s'/>"
"    </signal>"
"    <signal name='PluginDeactivated'>"
"      <arg name='name' type='s'/>"
"    </signal>"
"  </interface>"
"</node>";

struct CdosSettingsManagerPrivate
{
        guint                       owner_id;
        GDBusNodeInfo              *introspection_data;
        GDBusConnection            *connection;
        GSettings                  *settings;
        GnomePnpIds                *pnp_ids;
        GSList                     *plugins;
        guint                       load_source;
        CdosSettingsPluginInfo     *early_xsettings;
};

static void     cdos_settings_manager_class_init  (CdosSettingsManagerClass *klass);
static void     cdos_settings_manager_init        (CdosSettingsManager      *settings_manager);
static void     cdos_settings_manager_finalize    (GObject                   *object);

G_DEFINE_TYPE (CdosSettingsManager, cdos_settings_manager, G_TYPE_OBJECT)

static gpointer manager_object = NULL;

GQuark
cdos_settings_manager_error_quark (void)
{
        static GQuark ret = 0;
        if (ret == 0) {
                ret = g_quark_from_static_string ("found_settings_manager_error");
        }

        return ret;
}

static void
maybe_activate_plugin (CdosSettingsPluginInfo *info, gpointer user_data)
{
        if (cdos_settings_plugin_info_get_enabled (info)) {
                gboolean res;
                res = cdos_settings_plugin_info_activate (info);
                if (res) {
                        g_debug ("Plugin %s: active", cdos_settings_plugin_info_get_location (info));
                } else {
                        g_debug ("Plugin %s: activation failed", cdos_settings_plugin_info_get_location (info));
                }
        } else {
                g_debug ("Plugin %s: inactive", cdos_settings_plugin_info_get_location (info));
        }
}

static gint
compare_location (CdosSettingsPluginInfo *a,
                  CdosSettingsPluginInfo *b)
{
        const char *loc_a;
        const char *loc_b;

        loc_a = cdos_settings_plugin_info_get_location (a);
        loc_b = cdos_settings_plugin_info_get_location (b);

        if (loc_a == NULL || loc_b == NULL) {
                return -1;
        }

        return strcmp (loc_a, loc_b);
}

static int
compare_priority (CdosSettingsPluginInfo *a,
                  CdosSettingsPluginInfo *b)
{
        int prio_a;
        int prio_b;

        prio_a = cdos_settings_plugin_info_get_priority (a);
        prio_b = cdos_settings_plugin_info_get_priority (b);

        return prio_a - prio_b;
}

static void
emit_signal (CdosSettingsManager    *manager,
             const char              *signal,
             const char              *name)
{
        GError *error = NULL;

        /* FIXME: maybe we should queue those up until the D-Bus
         * connection is available... */
        if (manager->priv->connection == NULL)
                return;

        if (g_dbus_connection_emit_signal (manager->priv->connection,
                                           NULL,
                                           CSD_MANAGER_DBUS_PATH,
                                           CSD_MANAGER_DBUS_NAME,
                                           "PluginActivated",
                                           g_variant_new ("(s)", name),
                                           &error) == FALSE) {
                g_debug ("Error emitting signal: %s", error->message);
                g_error_free (error);
        }

}

static void
on_plugin_activated (CdosSettingsPluginInfo *info,
                     CdosSettingsManager    *manager)
{
        const char *name;

        name = cdos_settings_plugin_info_get_location (info);
        g_debug ("CdosSettingsManager: emitting plugin-activated %s", name);
        emit_signal (manager, "PluginActivated", name);
}

static void
on_plugin_deactivated (CdosSettingsPluginInfo *info,
                       CdosSettingsManager    *manager)
{
        const char *name;

        name = cdos_settings_plugin_info_get_location (info);
        g_debug ("CdosSettingsManager: emitting plugin-deactivated %s", name);
        emit_signal (manager, "PluginDeactivated", name);
}

static gboolean
contained (const char * const *items,
           const char         *item)
{
        while (*items) {
                if (g_strcmp0 (*items++, item) == 0) {
                        return TRUE;
                }
        }

        return FALSE;
}

static gboolean
is_schema (const char *schema)
{
        return contained (g_settings_list_schemas (), schema);
}


static void
_load_file (CdosSettingsManager *manager,
            const char           *filename)
{
        CdosSettingsPluginInfo *info;
        char                    *key_name;
        GSList                  *l;

        g_debug ("Loading plugin: %s", filename);

        info = cdos_settings_plugin_info_new_from_file (filename);
        if (info == NULL) {
                goto out;
        }

        l = g_slist_find_custom (manager->priv->plugins,
                                 info,
                                 (GCompareFunc) compare_location);
        if (l != NULL) {
                goto out;
        }

        key_name = g_strdup_printf ("%s.plugins.%s",
                                    DEFAULT_SETTINGS_PREFIX,
                                    cdos_settings_plugin_info_get_location (info));

        /* Ignore unknown schemas or else we'll assert */
        if (is_schema (key_name)) {
                manager->priv->plugins = g_slist_prepend (manager->priv->plugins,
                                                          g_object_ref (info));

                g_signal_connect (info, "activated",
                                  G_CALLBACK (on_plugin_activated), manager);
                g_signal_connect (info, "deactivated",
                                  G_CALLBACK (on_plugin_deactivated), manager);

                cdos_settings_plugin_info_set_settings_prefix (info, key_name);
        } else {
                g_warning ("Ignoring unknown module '%s'", key_name);
        }

        /* Priority is set in the call above */
        g_free (key_name);

 out:
        if (info != NULL) {
                g_object_unref (info);
        }

}

static void
_load_dir (CdosSettingsManager *manager,
           const char           *path)
{
        GError     *error;
        GDir       *d;
        const char *name;

        g_debug ("Loading settings plugins from dir: %s", path);

        error = NULL;
        d = g_dir_open (path, 0, &error);
        if (d == NULL) {
                g_warning ("%s", error->message);
                g_error_free (error);
                return;
        }

        while ((name = g_dir_read_name (d))) {
                char *filename;

                if (!g_str_has_suffix (name, PLUGIN_EXT)) {
                        continue;
                }

                filename = g_build_filename (path, name, NULL);
                if (g_file_test (filename, G_FILE_TEST_IS_REGULAR)) {
                        _load_file (manager, filename);
                }
                g_free (filename);
        }

        g_dir_close (d);

}

static void
activate_later_plugin (CdosSettingsPluginInfo *info, gpointer data)
{
        const char *name = cdos_settings_plugin_info_get_location (info);

        if (g_strcmp0 (name, "xsettings") == 0 || g_strcmp0 (name, "xrandr") == 0)
                return;

        g_message ("Cdos-settings-daemon: activate plugin %s\n", name);
        maybe_activate_plugin (info, data);
}

static gboolean
load_all (gpointer data)
{
        CdosSettingsManager *manager = data;
        manager->priv->load_source = 0;
        _load_dir (manager, CDOS_SETTINGS_PLUGINDIR G_DIR_SEPARATOR_S);
        manager->priv->plugins = g_slist_sort (manager->priv->plugins, (GCompareFunc) compare_priority);
        g_message ("IN LOAD ALL\n");
        g_slist_foreach (manager->priv->plugins, (GFunc) activate_later_plugin, NULL);
        return G_SOURCE_REMOVE;
}

static void
_unload_plugin (CdosSettingsPluginInfo *info, gpointer user_data)
{
        if (cdos_settings_plugin_info_get_enabled (info)) {
                cdos_settings_plugin_info_deactivate (info);
        }
        g_object_unref (info);
}

static void
_unload_all (CdosSettingsManager *manager)
{
         g_slist_foreach (manager->priv->plugins, (GFunc) _unload_plugin, NULL);
         g_slist_free (manager->priv->plugins);
         manager->priv->plugins = NULL;
}

static void
handle_manager_method (GDBusConnection *connection, const gchar *sender,
                       const gchar *path, const gchar *interface,
                       const gchar *method, GVariant *parameters,
                       GDBusMethodInvocation *invocation, gpointer data)
{
        if (g_strcmp0 (interface, CSD_MANAGER_DBUS_NAME) != 0) {
                g_warning ("not recognised interface: %s", interface);
                return;
        }

        g_debug ("Calling method '%s.%s' for Settings Manager", interface, method);
        if (g_strcmp0 (method, "ToggleDebug") != 0)
                return;

        if (g_getenv ("G_MESSAGES_DEBUG"))
                g_unsetenv ("G_MESSAGES_DEBUG");
        else
                g_setenv ("G_MESSAGES_DEBUG", "all", FALSE);
        g_dbus_method_invocation_return_value (invocation, NULL);
}

static const GDBusInterfaceVTable manager_vtable = {
        .method_call = handle_manager_method,
};

static void
on_bus_gotten (GObject             *source_object,
               GAsyncResult        *res,
               CdosSettingsManager *manager)
{
        GDBusConnection *connection;
        GError *error = NULL;

        connection = g_bus_get_finish (res, &error);
        if (connection == NULL) {
                g_warning ("Could not get session bus: %s", error->message);
                g_error_free (error);
                return;
        }
        manager->priv->connection = connection;

        g_dbus_connection_register_object (connection,
                                           CSD_MANAGER_DBUS_PATH,
                                           manager->priv->introspection_data->interfaces[0],
                                           &manager_vtable,
                                           NULL,
                                           NULL,
                                           NULL);
}

static void
register_manager (CdosSettingsManager *manager)
{
        manager->priv->introspection_data = g_dbus_node_info_new_for_xml (introspection_xml, NULL);
        g_assert (manager->priv->introspection_data != NULL);

        g_bus_get (G_BUS_TYPE_SESSION,
                   NULL,
                   (GAsyncReadyCallback) on_bus_gotten,
                   manager);
}

gboolean
cdos_settings_manager_start (CdosSettingsManager *manager,
                              GError              **error)
{
        gboolean ret;
        g_autofree gchar *filename = NULL;

        g_debug ("Starting settings manager");

        ret = FALSE;


        if (!g_module_supported ()) {
                g_warning ("cdos-settings-daemon is not able to initialize the plugins.");
                g_set_error (error,
                             CDOS_SETTINGS_MANAGER_ERROR,
                             CDOS_SETTINGS_MANAGER_ERROR_GENERAL,
                             "Plugins not supported");

                goto out;
        }

        g_debug ("loading PNPIDs");
        manager->priv->pnp_ids = gnome_pnp_ids_new ();

        manager->priv->settings = g_settings_new (DEFAULT_SETTINGS_PREFIX ".plugins");

        filename = g_build_filename (CDOS_SETTINGS_PLUGINDIR,
                                     "xsettings.cdos-settings-plugin", NULL);
        manager->priv->early_xsettings = cdos_settings_plugin_info_new_from_file (filename);
        if (manager->priv->early_xsettings != NULL) {
                CdosSettingsPluginInfo *info = manager->priv->early_xsettings;
                g_signal_connect (info, "activated", G_CALLBACK (on_plugin_activated), manager);
                g_signal_connect (info, "deactivated", G_CALLBACK (on_plugin_deactivated), manager);
                cdos_settings_plugin_info_set_settings_prefix (info, DEFAULT_SETTINGS_PREFIX ".plugins.xsettings");
                maybe_activate_plugin (info, manager);
        }
        manager->priv->load_source = g_timeout_add (1000, load_all, manager);

        ret = TRUE;
 out:

        return ret;
}

void
cdos_settings_manager_stop (CdosSettingsManager *manager)
{
        g_debug ("Stopping settings manager");

        g_clear_handle_id (&manager->priv->load_source, g_source_remove);
        _unload_all (manager);

        if (manager->priv->owner_id > 0) {
                g_bus_unown_name (manager->priv->owner_id);
                manager->priv->owner_id = 0;
        }

        if (manager->priv->settings != NULL) {
                g_object_unref (manager->priv->settings);
                manager->priv->settings = NULL;
        }

        if (manager->priv->pnp_ids != NULL) {
                g_object_unref (manager->priv->pnp_ids);
                manager->priv->pnp_ids = NULL;
        }
}

static void
cdos_settings_manager_dispose (GObject *object)
{
        CdosSettingsManager *manager;

        manager = CDOS_SETTINGS_MANAGER (object);

        cdos_settings_manager_stop (manager);

        G_OBJECT_CLASS (cdos_settings_manager_parent_class)->dispose (object);
}

static void
cdos_settings_manager_class_init (CdosSettingsManagerClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);

        object_class->dispose = cdos_settings_manager_dispose;
        object_class->finalize = cdos_settings_manager_finalize;

        g_type_class_add_private (klass, sizeof (CdosSettingsManagerPrivate));
}

static void
cdos_settings_manager_init (CdosSettingsManager *manager)
{

        manager->priv = CDOS_SETTINGS_MANAGER_GET_PRIVATE (manager);
}

static void
cdos_settings_manager_finalize (GObject *object)
{
        CdosSettingsManager *manager;

        g_return_if_fail (object != NULL);
        g_return_if_fail (CDOS_IS_SETTINGS_MANAGER (object));

        manager = CDOS_SETTINGS_MANAGER (object);

        g_return_if_fail (manager->priv != NULL);
        g_clear_object (&manager->priv->early_xsettings);

        G_OBJECT_CLASS (cdos_settings_manager_parent_class)->finalize (object);
}

CdosSettingsManager *
cdos_settings_manager_new (void)
{
        if (manager_object != NULL) {
                g_object_ref (manager_object);
        } else {
                manager_object = g_object_new (CDOS_TYPE_SETTINGS_MANAGER,
                                               NULL);
                g_object_add_weak_pointer (manager_object,
                                           (gpointer *) &manager_object);
                register_manager (manager_object);
        }

        return CDOS_SETTINGS_MANAGER (manager_object);
}
