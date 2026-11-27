/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2002-2005 Paolo Maggi
 * Copyright (C) 2007      William Jon McCann <mccann@jhu.edu>
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
 * Foundation, Inc., 51 Franklin Street, Suite 500,
 * Boston, MA 02110-1335, USA.
 */

#ifndef __CDOS_SETTINGS_PLUGIN_H__
#define __CDOS_SETTINGS_PLUGIN_H__

#include <glib-object.h>
#include <gmodule.h>

G_BEGIN_DECLS
#define CDOS_TYPE_SETTINGS_PLUGIN              (cdos_settings_plugin_get_type())
#define CDOS_SETTINGS_PLUGIN(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), CDOS_TYPE_SETTINGS_PLUGIN, CdosSettingsPlugin))
#define CDOS_SETTINGS_PLUGIN_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass),  CDOS_TYPE_SETTINGS_PLUGIN, CdosSettingsPluginClass))
#define CDOS_IS_SETTINGS_PLUGIN(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), CDOS_TYPE_SETTINGS_PLUGIN))
#define CDOS_IS_SETTINGS_PLUGIN_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), CDOS_TYPE_SETTINGS_PLUGIN))
#define CDOS_SETTINGS_PLUGIN_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj),  CDOS_TYPE_SETTINGS_PLUGIN, CdosSettingsPluginClass))

typedef struct
{
        GObject parent;
} CdosSettingsPlugin;

typedef struct
{
        GObjectClass parent_class;

        /* Virtual public methods */
        void            (*activate)                     (CdosSettingsPlugin *plugin);
        void            (*deactivate)                   (CdosSettingsPlugin *plugin);
} CdosSettingsPluginClass;

GType            cdos_settings_plugin_get_type           (void) G_GNUC_CONST;

void             cdos_settings_plugin_activate           (CdosSettingsPlugin *plugin);
void             cdos_settings_plugin_deactivate         (CdosSettingsPlugin *plugin);

/*
 * Utility macro used to register plugins
 *
 * use: CDOS_SETTINGS_PLUGIN_REGISTER (PluginName, plugin_name)
 */
#define CDOS_SETTINGS_PLUGIN_REGISTER(PluginName, plugin_name)                \
        G_DEFINE_DYNAMIC_TYPE (PluginName,                                     \
                               plugin_name,                                    \
                               CDOS_TYPE_SETTINGS_PLUGIN)                     \
                                                                               \
G_MODULE_EXPORT GType                                                          \
register_cdos_settings_plugin (GTypeModule *type_module)                      \
{                                                                              \
        plugin_name##_register_type (type_module);                             \
                                                                               \
        return plugin_name##_get_type();                                       \
}                                                                              \
                                                                               \
static void                                                                    \
plugin_name##_class_finalize (PluginName##Class *plugin_name##_class)          \
{                                                                              \
}                                                                              \


G_END_DECLS

#endif  /* __CDOS_SETTINGS_PLUGIN_H__ */
