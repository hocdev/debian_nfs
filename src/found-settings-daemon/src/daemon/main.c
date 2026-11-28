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
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA  02110-1335  USA
 */

#include "config.h"

#include <stdlib.h>
#include <unistd.h>
#include <libintl.h>
#include <errno.h>
#include <locale.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/wait.h>

#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <gio/gio.h>
#include <libnotify/notify.h>

#include "cdos-settings-manager.h"

#define CSD_DBUS_NAME         "org.freedesktop.SettingsDaemon"

#define GNOME_SESSION_DBUS_NAME      "org.gnome.SessionManager"
#define GNOME_SESSION_DBUS_OBJECT    "/org/gnome/SessionManager"
#define GNOME_SESSION_DBUS_INTERFACE "org.gnome.SessionManager"
#define GNOME_SESSION_CLIENT_PRIVATE_DBUS_INTERFACE "org.gnome.SessionManager.ClientPrivate"

static gboolean   debug        = FALSE;
static gboolean   do_timed_exit = FALSE;
static int        term_signal_pipe_fds[2];
static guint      name_id      = 0;
static CdosSettingsManager *manager = NULL;
GThreadPool *thread_pool;

static GOptionEntry entries[] = {
        {"debug", 0, 0, G_OPTION_ARG_NONE, &debug, N_("Enable debugging code"), NULL },
        { "timed-exit", 0, 0, G_OPTION_ARG_NONE, &do_timed_exit, N_("Exit after a time (for debugging)"), NULL },
        {NULL}
};

static gboolean
timed_exit_cb (gpointer data)
{
        g_debug ("Doing timed exit");
        gtk_main_quit ();
        return FALSE;
}

static void
stop_manager (CdosSettingsManager *manager)
{
        cdos_settings_manager_stop (manager);
        gtk_main_quit ();
}

static void
on_session_over (GDBusProxy *proxy,
                 gchar      *sender_name,
                 gchar      *signal_name,
                 GVariant   *parameters,
                 gpointer    user_data)
{
        if (g_strcmp0 (signal_name, "SessionOver") == 0) {
                g_debug ("Got a SessionOver signal - stopping");
                stop_manager (manager);
        }
}

static void
respond_to_end_session (GDBusProxy *proxy)
{
        /* we must answer with "EndSessionResponse" */
        g_dbus_proxy_call (proxy, "EndSessionResponse",
                           g_variant_new ("(bs)",
                                          TRUE, ""),
                           G_DBUS_CALL_FLAGS_NONE,
                           -1, NULL, NULL, NULL);
}

static void
client_proxy_signal_cb (GDBusProxy *proxy,
                        gchar *sender_name,
                        gchar *signal_name,
                        GVariant *parameters,
                        gpointer user_data)
{
        if (g_strcmp0 (signal_name, "QueryEndSession") == 0) {
                g_debug ("Got QueryEndSession signal");
                respond_to_end_session (proxy);
        } else if (g_strcmp0 (signal_name, "EndSession") == 0) {
                g_debug ("Got EndSession signal");
                respond_to_end_session (proxy);
        } else if (g_strcmp0 (signal_name, "Stop") == 0) {
                g_debug ("Got Stop signal");
                stop_manager (manager);
        }
}

static void
got_client_proxy (GObject *object,
                  GAsyncResult *res,
                  gpointer user_data)
{
        GDBusProxy *client_proxy;
        GError *error = NULL;

        client_proxy = g_dbus_proxy_new_for_bus_finish (res, &error);

        if (error != NULL) {
                g_debug ("Unable to get the session client proxy: %s", error->message);
                g_error_free (error);
                return;
        }

        g_signal_connect (client_proxy, "g-signal",
                          G_CALLBACK (client_proxy_signal_cb), manager);
}

static void
on_client_registered (GObject             *source_object,
                      GAsyncResult        *res,
                      gpointer             user_data)
{
        GVariant *variant;
        GError *error = NULL;
        gchar *object_path = NULL;

        variant = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object), res, &error);
        if (error != NULL) {
                g_warning ("Unable to register client: %s", error->message);
                g_error_free (error);
        } else {
                g_variant_get (variant, "(o)", &object_path);

                g_debug ("Registered client at path %s", object_path);

                g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION, 0, NULL,
                                          GNOME_SESSION_DBUS_NAME,
                                          object_path,
                                          GNOME_SESSION_CLIENT_PRIVATE_DBUS_INTERFACE,
                                          NULL,
                                          got_client_proxy,
                                          manager);

                g_free (object_path);
                g_variant_unref (variant);
        }
}

static gboolean
on_term_signal_pipe_closed (GIOChannel *source,
                            GIOCondition condition,
                            gpointer data)
{
        term_signal_pipe_fds[0] = -1;

        g_debug ("Received SIGTERM - shutting down");
        /* Got SIGTERM, time to clean up and get out
         */
        gtk_main_quit ();

        return FALSE;
}

static void
on_term_signal (int signal)
{
        /* Wake up main loop to tell it to shutdown */
        close (term_signal_pipe_fds[1]);
        term_signal_pipe_fds[1] = -1;
}

static void
watch_for_term_signal (CdosSettingsManager *manager)
{
        GIOChannel *channel;

        if (-1 == pipe (term_signal_pipe_fds) ||
            -1 == fcntl (term_signal_pipe_fds[0], F_SETFD, FD_CLOEXEC) ||
            -1 == fcntl (term_signal_pipe_fds[1], F_SETFD, FD_CLOEXEC)) {
                g_error ("Could not create pipe: %s", g_strerror (errno));
                exit (EXIT_FAILURE);
        }

        channel = g_io_channel_unix_new (term_signal_pipe_fds[0]);
        g_io_channel_set_encoding (channel, NULL, NULL);
        g_io_channel_set_buffered (channel, FALSE);
        g_io_add_watch (channel, G_IO_HUP, on_term_signal_pipe_closed, manager);
        g_io_channel_unref (channel);

        signal (SIGTERM, on_term_signal);
}

static void
set_session_over_handler (GDBusConnection *bus)
{
        g_assert (bus != NULL);

        watch_for_term_signal (manager);
}

static void
name_acquired_handler (GDBusConnection *connection,
                       const gchar *name,
                       gpointer user_data)
{
        set_session_over_handler (connection);
}

static void
name_lost_handler (GDBusConnection *connection,
                   const gchar *name,
                   gpointer user_data)
{
        /* Name was already taken, or the bus went away */

        g_warning ("Name taken or bus went away - shutting down");
        gtk_main_quit ();
}

static gboolean
do_register_client (gpointer user_data)
{
        GDBusProxy *proxy = (GDBusProxy *) user_data;
        g_assert (proxy != NULL);

        const char *startup_id = g_getenv ("DESKTOP_AUTOSTART_ID");
        g_dbus_proxy_call (proxy,
                           "RegisterClient",
                           g_variant_new ("(ss)", "cdos-settings-daemon", startup_id ? startup_id : ""),
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           NULL,
                           (GAsyncReadyCallback) on_client_registered,
                           manager);

        return FALSE;
}

static void
queue_register_client (void)
{
        GDBusConnection *bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
        if (!bus)
                return;

        GError *error = NULL;
        GDBusProxy *proxy = g_dbus_proxy_new_sync (bus,
                                                   G_DBUS_PROXY_FLAGS_NONE,
                                                   NULL,
                                                   GNOME_SESSION_DBUS_NAME,
                                                   GNOME_SESSION_DBUS_OBJECT,
                                                   GNOME_SESSION_DBUS_INTERFACE,
                                                   NULL,
                                                   &error);
        g_object_unref (bus);

        if (proxy == NULL) {
                g_debug ("Could not connect to the Session manager: %s", error->message);
                g_error_free (error);
                return;
        }

        /* Register the daemon with cinnamon-session */
        g_signal_connect (G_OBJECT (proxy), "g-signal",
                          G_CALLBACK (on_session_over), NULL);

        g_idle_add_full (G_PRIORITY_DEFAULT, do_register_client, proxy, NULL);
}

static void
bus_register (void)
{
        name_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                                  CSD_DBUS_NAME,
                                  G_BUS_NAME_OWNER_FLAGS_NONE,
                                  NULL,
                                  (GBusNameAcquiredCallback) name_acquired_handler,
                                  (GBusNameLostCallback) name_lost_handler,
                                  NULL,
                                  NULL);
}

static void
csd_log_default_handler (const gchar   *log_domain,
                         GLogLevelFlags log_level,
                         const gchar   *message,
                         gpointer       unused_data)
{
        /* ToggleDebug changes this while the daemon is running. */
        if ((log_level & G_LOG_LEVEL_MASK) == G_LOG_LEVEL_DEBUG
            && g_getenv ("G_MESSAGES_DEBUG") == NULL) {
                return;
        }

        g_log_default_handler (log_domain,
                               log_level,
                               message,
                               unused_data);
}

static void
parse_args (int *argc, char ***argv)
{
        GError *error;
        GOptionContext *context;



        context = g_option_context_new (NULL);

        g_option_context_add_main_entries (context, entries, NULL);
        g_option_context_add_group (context, gtk_get_option_group (FALSE));

        error = NULL;
        if (!g_option_context_parse (context, argc, argv, &error)) {
                if (error != NULL) {
                        g_warning ("%s", error->message);
                        g_error_free (error);
                } else {
                        g_warning ("Unable to initialize GTK+");
                }
                exit (EXIT_FAILURE);
        }

        g_option_context_free (context);


        if (debug)
                g_setenv ("G_MESSAGES_DEBUG", "all", FALSE);
}

int
main (int argc, char *argv[])
{

        gboolean              res;
        GError               *error;


        bindtextdomain (GETTEXT_PACKAGE, CDOS_SETTINGS_LOCALEDIR);
        bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
        textdomain (GETTEXT_PACKAGE);
        setlocale (LC_ALL, "");

        parse_args (&argc, &argv);

        if (! gtk_init_check (NULL, NULL)) {
                g_warning ("Unable to initialize GTK+");
                exit (EXIT_FAILURE);
        }
        g_log_set_default_handler (csd_log_default_handler, NULL);

        notify_init ("cdos-settings-daemon");

        queue_register_client ();

        bus_register ();

        manager = cdos_settings_manager_new ();
        if (manager == NULL) {
                g_warning ("Unable to register object");
                goto out;
        }

        error = NULL;
        res = cdos_settings_manager_start (manager, &error);
        if (! res) {
                g_warning ("Unable to start: %s", error->message);
                g_error_free (error);
                goto out;
        }

        if (do_timed_exit) {
                g_timeout_add_seconds (30, timed_exit_cb, NULL);
        }

        gtk_main ();

        g_debug ("Shutting down");

out:
        if (name_id > 0) {
                g_bus_unown_name (name_id);
                name_id = 0;
        }
        if (manager != NULL) {
                g_object_unref (manager);
        }

        g_debug ("SettingsDaemon finished");

        return 0;
}
