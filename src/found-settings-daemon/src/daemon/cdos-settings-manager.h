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

#ifndef __CDOS_SETTINGS_MANAGER_H
#define __CDOS_SETTINGS_MANAGER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define CDOS_TYPE_SETTINGS_MANAGER         (cdos_settings_manager_get_type ())
#define CDOS_SETTINGS_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), CDOS_TYPE_SETTINGS_MANAGER, CdosSettingsManager))
#define CDOS_SETTINGS_MANAGER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), CDOS_TYPE_SETTINGS_MANAGER, CdosSettingsManagerClass))
#define CDOS_IS_SETTINGS_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), CDOS_TYPE_SETTINGS_MANAGER))
#define CDOS_IS_SETTINGS_MANAGER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), CDOS_TYPE_SETTINGS_MANAGER))
#define CDOS_SETTINGS_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), CDOS_TYPE_SETTINGS_MANAGER, CdosSettingsManagerClass))

typedef struct CdosSettingsManagerPrivate CdosSettingsManagerPrivate;

typedef struct
{
        GObject                      parent;
        CdosSettingsManagerPrivate *priv;
} CdosSettingsManager;

typedef struct
{
        GObjectClass   parent_class;

        void          (* plugin_activated)         (CdosSettingsManager *manager,
                                                    const char           *name);
        void          (* plugin_deactivated)       (CdosSettingsManager *manager,
                                                    const char           *name);
} CdosSettingsManagerClass;

typedef enum
{
        CDOS_SETTINGS_MANAGER_ERROR_GENERAL
} CdosSettingsManagerError;

#define CDOS_SETTINGS_MANAGER_ERROR cdos_settings_manager_error_quark ()

GQuark                 cdos_settings_manager_error_quark         (void);
GType                  cdos_settings_manager_get_type   (void);

CdosSettingsManager * cdos_settings_manager_new        (void);
gboolean               cdos_settings_manager_start      (CdosSettingsManager *manager,
                                                          GError              **error);
void                   cdos_settings_manager_stop       (CdosSettingsManager *manager);

G_END_DECLS

#endif /* __CDOS_SETTINGS_MANAGER_H */
