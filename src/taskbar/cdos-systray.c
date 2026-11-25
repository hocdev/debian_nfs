/**
 * Reconstructed from libcdos-desktop.so (found-desktop 4.0.5-m117+1nfs5)
 * Decompiled via REA + Ghidra 12.2
 *
 * Key addresses:
 *   cdos_systray_manager_new             @ 0x1aeef0
 *   cdos_systray_manager_register_role   @ 0x1aec50
 *   cdos_systray_manager_unregister_role @ 0x1aece0
 *   cdos_systray_manager_unregister_id   @ 0x1aede0
 *   cdos_systray_manager_get_roles       @ 0x1aeec0
 *   applets_systray_main                 @ 0x1d71d0
 */

#include "cdos-systray.h"

struct _CdosSystrayManager {
    GObject parent_instance;
    GList *roles; /* +0x18 */
};

static CdosSystrayManager *global_systray_manager = NULL;

CdosSystrayManager *
cdos_systray_manager_new(void)
{
    return g_object_new(CDOS_TYPE_SYSTRAY_MANAGER, NULL);
}

void
cdos_systray_manager_register_role(CdosSystrayManager *self, const gchar *role, const gchar *id)
{
    g_return_if_fail(CDOS_IS_SYSTRAY_MANAGER(self));

    CdosSystrayRoleEntry *entry = g_malloc0(sizeof(CdosSystrayRoleEntry));
    entry->role = (gchar *)role;
    entry->id = (gchar *)id;
    self->roles = g_list_append(self->roles, entry);
    g_signal_emit_by_name(self, "changed");
}

void
cdos_systray_manager_unregister_role(CdosSystrayManager *self, const gchar *role, const gchar *id)
{
    g_return_if_fail(CDOS_IS_SYSTRAY_MANAGER(self));

    for (GList *l = self->roles; l != NULL; ) {
        GList *next = l->next;
        CdosSystrayRoleEntry *entry = l->data;
        if (entry != NULL && g_strcmp0(entry->role, role) == 0 && g_strcmp0(entry->id, id) == 0) {
            self->roles = g_list_delete_link(self->roles, l);
            g_free(entry);
            g_signal_emit_by_name(self, "changed");
        }
        l = next;
    }
}

void
cdos_systray_manager_unregister_id(CdosSystrayManager *self, const gchar *id)
{
    g_return_if_fail(CDOS_IS_SYSTRAY_MANAGER(self));

    for (GList *l = self->roles; l != NULL; ) {
        GList *next = l->next;
        CdosSystrayRoleEntry *entry = l->data;
        if (entry != NULL && g_strcmp0(entry->id, id) == 0) {
            self->roles = g_list_delete_link(self->roles, l);
            g_free(entry);
            g_signal_emit_by_name(self, "changed");
        }
        l = next;
    }
}

GList *
cdos_systray_manager_get_roles(CdosSystrayManager *self)
{
    g_return_val_if_fail(CDOS_IS_SYSTRAY_MANAGER(self), NULL);
    return self->roles;
}

CdosSystrayManager *
cdos_ui_plugin_get_systray_manager(void)
{
    return global_systray_manager;
}

void
cdos_ui_plugin_set_systray_manager(CdosSystrayManager *mgr)
{
    global_systray_manager = mgr;
}

AppletsSystray *
applets_systray_main(gpointer metadata, gint orientation)
{
    (void)metadata;
    return g_object_new(applets_systray_get_type(),
                        "orientation", orientation,
                        NULL);
}
