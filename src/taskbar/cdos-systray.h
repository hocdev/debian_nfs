#ifndef __CDOS_SYSTRAY_H__
#define __CDOS_SYSTRAY_H__

#include "clutter-compat.h"

G_BEGIN_DECLS

#define CDOS_TYPE_SYSTRAY_MANAGER (cdos_systray_manager_get_type())
G_DECLARE_FINAL_TYPE(CdosSystrayManager, cdos_systray_manager, CDOS, SYSTRAY_MANAGER, GObject)

typedef struct _AppletsSystray AppletsSystray;

typedef struct {
    gchar *role;
    gchar *id;
} CdosSystrayRoleEntry;

GType cdos_systray_manager_get_type(void);
CdosSystrayManager *cdos_systray_manager_new(void);
void cdos_systray_manager_register_role(CdosSystrayManager *self, const gchar *role, const gchar *id);
void cdos_systray_manager_unregister_role(CdosSystrayManager *self, const gchar *role, const gchar *id);
void cdos_systray_manager_unregister_id(CdosSystrayManager *self, const gchar *id);
GList *cdos_systray_manager_get_roles(CdosSystrayManager *self);

CdosSystrayManager *cdos_ui_plugin_get_systray_manager(void);
void cdos_ui_plugin_set_systray_manager(CdosSystrayManager *mgr);

GType applets_systray_get_type(void);
AppletsSystray *applets_systray_main(gpointer metadata, gint orientation);

G_END_DECLS

#endif /* __CDOS_SYSTRAY_H__ */
