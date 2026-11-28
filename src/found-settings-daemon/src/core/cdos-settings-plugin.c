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

#include "config.h"

#include "cdos-settings-plugin.h"

G_DEFINE_TYPE (CdosSettingsPlugin, cdos_settings_plugin, G_TYPE_OBJECT)

static void
dummy (CdosSettingsPlugin *plugin)
{
        /* Empty */
}

static void
cdos_settings_plugin_class_init (CdosSettingsPluginClass *klass)
{
        klass->activate = dummy;
        klass->deactivate = dummy;
}

static void
cdos_settings_plugin_init (CdosSettingsPlugin *plugin)
{
        /* Empty */
}

void
cdos_settings_plugin_activate (CdosSettingsPlugin *plugin)
{
        g_return_if_fail (CDOS_IS_SETTINGS_PLUGIN (plugin));

        CDOS_SETTINGS_PLUGIN_GET_CLASS (plugin)->activate (plugin);
}

void
cdos_settings_plugin_deactivate  (CdosSettingsPlugin *plugin)
{
        g_return_if_fail (CDOS_IS_SETTINGS_PLUGIN (plugin));

        CDOS_SETTINGS_PLUGIN_GET_CLASS (plugin)->deactivate (plugin);
}
