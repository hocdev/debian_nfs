/**
 * Reconstructed from libcdos-desktop.so (found-desktop 4.0.5-m117+1nfs5)
 * Decompiled via REA + Ghidra 12.2
 *
 * Key addresses:
 *   cdos_applet_get_actor                     @ 0x164660
 *   cdos_applet_set_popup_menu_active         @ 0x164ad0
 *   cdos_applet_manager_remove_applet_from_panel @ 0x165da0
 *   cdos_applet_manager_init                  @ 0x165ec0
 */

#include "cdos-applet.h"

extern gpointer cdos_ui_plugin_get_status_icon_dispatcher(void);
extern gchar **cdos_strv_elem_del_by_nth(gchar **strv, guint idx);

typedef struct {
    gchar *uuid;
    gpointer extension;
    gint order;
    gint new_order;
    gint panel_location;
    gint new_panel_location;
    gfloat panel_height;
    gint orientation;
    gpointer popup_menu;        /* +0x58 */
    gpointer context_menu;
    gpointer menu_manager;
    gboolean popup_menu_active; /* +0x70 */
    gboolean context_menu_active;
    gulong popup_open_sig_id;   /* +0x78 */
} CdosAppletPrivate;

struct _CdosApplet {
    GObject parent_instance;
    CdosAppletPrivate *priv;
};

static GTree *applet_definitions_tree = NULL;
static gchar **grouped_applets = NULL;

ClutterActor *
cdos_applet_get_actor(CdosApplet *self)
{
    g_return_val_if_fail(CDOS_IS_APPLET(self), NULL);
    return CLUTTER_ACTOR(self);
}

const gchar *
cdos_applet_get_uuid(CdosApplet *self)
{
    g_return_val_if_fail(CDOS_IS_APPLET(self), NULL);
    return self->priv->uuid;
}

void
cdos_applet_set_uuid(CdosApplet *self, const gchar *uuid)
{
    g_return_if_fail(CDOS_IS_APPLET(self));
    g_free(self->priv->uuid);
    self->priv->uuid = g_strdup(uuid);
}

gpointer
cdos_applet_get_extension(CdosApplet *self)
{
    g_return_val_if_fail(CDOS_IS_APPLET(self), NULL);
    return self->priv->extension;
}

void
cdos_applet_set_extension(CdosApplet *self, gpointer extension)
{
    g_return_if_fail(CDOS_IS_APPLET(self));
    self->priv->extension = extension;
}

gint cdos_applet_get_order(CdosApplet *self) { return self->priv->order; }
void cdos_applet_set_order(CdosApplet *self, gint order) { self->priv->order = order; }
gint cdos_applet_get_new_order(CdosApplet *self) { return self->priv->new_order; }
void cdos_applet_set_new_order(CdosApplet *self, gint new_order) { self->priv->new_order = new_order; }

gint cdos_applet_get_panel_location(CdosApplet *self) { return self->priv->panel_location; }
void cdos_applet_set_panel_location(CdosApplet *self, gint loc) { self->priv->panel_location = loc; }
gint cdos_applet_get_new_panel_location(CdosApplet *self) { return self->priv->new_panel_location; }
void cdos_applet_set_new_panel_location(CdosApplet *self, gint loc) { self->priv->new_panel_location = loc; }

gfloat cdos_applet_get_panel_height(CdosApplet *self) { return self->priv->panel_height; }
void cdos_applet_set_panel_height(CdosApplet *self, gfloat h) { self->priv->panel_height = h; }
void cdos_applet_set_orientation(CdosApplet *self, gint orientation) { self->priv->orientation = orientation; }
void cdos_applet_set_applet_tooltip(CdosApplet *self, const gchar *text) { (void)self; (void)text; }

gpointer cdos_applet_get_popup_menu(CdosApplet *self) { return self->priv->popup_menu; }
gboolean cdos_applet_get_popup_menu_active(CdosApplet *self) { return self->priv->popup_menu_active; }
void cdos_applet_set_popup_menu_active(CdosApplet *self, gboolean active) { self->priv->popup_menu_active = active; }

gpointer cdos_applet_get_context_menu(CdosApplet *self) { return self->priv->context_menu; }
gboolean cdos_applet_get_context_menu_active(CdosApplet *self) { return self->priv->context_menu_active; }
void cdos_applet_set_context_menu_active(CdosApplet *self, gboolean active) { self->priv->context_menu_active = active; }
gpointer cdos_applet_get_menu_manager(CdosApplet *self) { return self->priv->menu_manager; }

static gint
applet_key_cmp(gconstpointer a, gconstpointer b, gpointer user_data)
{
    (void)user_data;
    return g_strcmp0((const gchar *)a, (const gchar *)b);
}

void
cdos_applet_manager_init(void)
{
    GSettings *settings = g_settings_new("org.cdos");
    grouped_applets = g_settings_get_strv(settings, "grouped-applets");
    cdos_ui_plugin_get_status_icon_dispatcher();

    if (applet_definitions_tree == NULL) {
        applet_definitions_tree = g_tree_new_full(applet_key_cmp, NULL, g_free, NULL);
    }
    g_object_unref(settings);
}

void
cdos_applet_manager_finish_extension_load(void)
{
}

void
cdos_applet_manager_remove_applet_from_panel(const gchar *uuid, const gchar *id)
{
    (void)id;
    GSettings *settings = g_settings_new("org.cdos");
    gchar **enabled = g_settings_get_strv(settings, "enabled-applets");
    if (enabled != NULL) {
        for (guint i = 0; enabled[i] != NULL; i++) {
            if (uuid != NULL && g_str_has_suffix(enabled[i], uuid)) {
                gchar **updated = cdos_strv_elem_del_by_nth(g_strdupv(enabled), i);
                g_settings_set_strv(settings, "enabled-applets", (const gchar *const *)updated);
                g_strfreev(updated);
                break;
            }
        }
        g_strfreev(enabled);
    }
    g_object_unref(settings);
}

void cdos_applet_manager_save_applets_positions(void) {}
void cdos_applet_manager_set_applets_orientation(gint orientation) { (void)orientation; }
