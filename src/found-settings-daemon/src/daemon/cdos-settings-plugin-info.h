/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 Red Hat, Inc.
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

#ifndef __CDOS_SETTINGS_PLUGIN_INFO_H__
#define __CDOS_SETTINGS_PLUGIN_INFO_H__

#include <glib-object.h>
#include <gmodule.h>

G_BEGIN_DECLS
#define CDOS_TYPE_SETTINGS_PLUGIN_INFO              (cdos_settings_plugin_info_get_type())
#define CDOS_SETTINGS_PLUGIN_INFO(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), CDOS_TYPE_SETTINGS_PLUGIN_INFO, CdosSettingsPluginInfo))
#define CDOS_SETTINGS_PLUGIN_INFO_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass),  CDOS_TYPE_SETTINGS_PLUGIN_INFO, CdosSettingsPluginInfoClass))
#define CDOS_IS_SETTINGS_PLUGIN_INFO(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), CDOS_TYPE_SETTINGS_PLUGIN_INFO))
#define CDOS_IS_SETTINGS_PLUGIN_INFO_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), CDOS_TYPE_SETTINGS_PLUGIN_INFO))
#define CDOS_SETTINGS_PLUGIN_INFO_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj),  CDOS_TYPE_SETTINGS_PLUGIN_INFO, CdosSettingsPluginInfoClass))

typedef struct CdosSettingsPluginInfoPrivate CdosSettingsPluginInfoPrivate;

typedef struct
{
        GObject                         parent;
        CdosSettingsPluginInfoPrivate *priv;
} CdosSettingsPluginInfo;

typedef struct
{
        GObjectClass parent_class;

        void          (* activated)         (CdosSettingsPluginInfo *info);
        void          (* deactivated)       (CdosSettingsPluginInfo *info);
} CdosSettingsPluginInfoClass;

GType            cdos_settings_plugin_info_get_type           (void) G_GNUC_CONST;

CdosSettingsPluginInfo *cdos_settings_plugin_info_new_from_file (const char *filename);

void             cdos_settings_plugin_info_set_settings_prefix (CdosSettingsPluginInfo *info, const char *settings_prefix);
gboolean         cdos_settings_plugin_info_activate        (CdosSettingsPluginInfo *info);
gboolean         cdos_settings_plugin_info_deactivate      (CdosSettingsPluginInfo *info);

gboolean         cdos_settings_plugin_info_is_active       (CdosSettingsPluginInfo *info);
gboolean         cdos_settings_plugin_info_get_enabled     (CdosSettingsPluginInfo *info);
gboolean         cdos_settings_plugin_info_is_available    (CdosSettingsPluginInfo *info);

const char      *cdos_settings_plugin_info_get_name        (CdosSettingsPluginInfo *info);
const char      *cdos_settings_plugin_info_get_description (CdosSettingsPluginInfo *info);
const char     **cdos_settings_plugin_info_get_authors     (CdosSettingsPluginInfo *info);
const char      *cdos_settings_plugin_info_get_website     (CdosSettingsPluginInfo *info);
const char      *cdos_settings_plugin_info_get_copyright   (CdosSettingsPluginInfo *info);
const char      *cdos_settings_plugin_info_get_location    (CdosSettingsPluginInfo *info);
int              cdos_settings_plugin_info_get_priority    (CdosSettingsPluginInfo *info);

void             cdos_settings_plugin_info_set_priority    (CdosSettingsPluginInfo *info,
                                                             int                      priority);

G_END_DECLS

#endif  /* __CDOS_SETTINGS_PLUGIN_INFO_H__ */
