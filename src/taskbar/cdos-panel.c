/**
 * Reconstructed from libcdos-desktop.so (found-desktop 4.0.5-m117+1nfs5)
 * Decompiled via REA + Ghidra 12.2
 */

#include "cdos-panel.h"
#include <libintl.h>

#define _(String) dgettext(NULL, String)

typedef struct _CdosPopupMenu CdosPopupMenu;
typedef struct _CdosPopupMenuManager CdosPopupMenuManager;

extern CdosPopupMenu *cdos_popup_menu_new_with_params(gdouble align, ClutterActor *source, gint arrow_side);
extern void cdos_popup_menu_set_arrow_side(CdosPopupMenu *menu, gint arrow_side);
extern void cdos_popup_menu_close(CdosPopupMenu *menu, gboolean animate);
extern gpointer cdos_popup_menu_get_box_pointer(CdosPopupMenu *menu);
extern void cdos_box_pointer_set_source_alignment(gfloat alignment, gpointer box_pointer);
extern void cdos_popup_menu_base_add_menu_item(gpointer menu, gpointer item, gint position);
extern void cdos_popup_menu_base_toggle(gpointer menu);
extern gpointer cdos_popup_separator_menu_item_new(void);
extern gpointer cdos_popup_switch_menu_item_new_with_params(const gchar *text, gboolean active, gpointer params);
extern void cdos_popup_base_menu_item_set_keep_menu(gpointer item, gboolean keep);
extern gpointer cdos_drop_action_new(ClutterActor *actor);

struct _CdosPanel {
    GObject parent_instance;

    ClutterActor *box;              /* +0x30 */
    ClutterActor *left_box;
    ClutterActor *center_box;
    ClutterActor *right_box;
    gint hide_delay_ms;             /* +0x48 */
    guint enable_timeout_id;        /* +0x50 */
    guint disable_timeout_id;       /* +0x54 */
    guint8 flags;                   /* +0x40: bit 0 = bottom_position */
    CdosPopupMenu *context_menu;    /* +0x70 */
    CdosPopupMenuManager *menu_mgr; /* +0x78 */
    GList *menu_items;              /* +0x80 */
    GSettings *settings;            /* +0x88 */
    gint monitor_index;             /* +0x90 */
    gboolean hideable;
    gboolean hidden;
};

static gpointer
panel_create_action_menu_item(CdosPanel *self, const gchar *label, const gchar *icon,
                              GCallback callback, gpointer user_data)
{
    (void)self;
    (void)label;
    (void)icon;
    (void)callback;
    (void)user_data;
    return NULL;
}

CdosPanel *
cdos_panel_new(gboolean bottom_position, gboolean is_primary)
{
    CdosPanel *self = g_object_new(CDOS_TYPE_PANEL,
                                   "bottom-position", bottom_position,
                                   "is-primary", is_primary,
                                   NULL);

    ClutterActor *actor = CLUTTER_ACTOR(self);
    self->context_menu = cdos_popup_menu_new_with_params(0.0, actor, 2);

    ClutterActor *menu_actor = *(ClutterActor **)((guint8 *)self->context_menu + 0x68);
    clutter_actor_add_child(cdos_ui_plugin_get_ui_group(), menu_actor);
    st_widget_add_style_class_name(menu_actor, "popup-contextmenu");

    gpointer item_show_desktop = panel_create_action_menu_item(self, _("Show desktop"), NULL, NULL, self);
    cdos_popup_menu_base_add_menu_item(self->context_menu, item_show_desktop, -1);
    self->menu_items = g_list_append(self->menu_items, item_show_desktop);

    cdos_popup_menu_base_add_menu_item(self->context_menu, cdos_popup_separator_menu_item_new(), -1);

    gpointer item_sysmon = panel_create_action_menu_item(self, _("Open the system monitor"), NULL, NULL, NULL);
    cdos_popup_menu_base_add_menu_item(self->context_menu, item_sysmon, -1);
    self->menu_items = g_list_append(self->menu_items, item_sysmon);

    gpointer item_stack = panel_create_action_menu_item(self, _("Stack windows"), NULL, NULL, NULL);
    cdos_popup_menu_base_add_menu_item(self->context_menu, item_stack, -1);
    self->menu_items = g_list_append(self->menu_items, item_stack);

    gpointer item_abreast = panel_create_action_menu_item(self, _("Abreast windows"), NULL, NULL, NULL);
    cdos_popup_menu_base_add_menu_item(self->context_menu, item_abreast, -1);
    self->menu_items = g_list_append(self->menu_items, item_abreast);

    cdos_popup_menu_base_add_menu_item(self->context_menu, cdos_popup_separator_menu_item_new(), -1);

    gpointer switch_top = cdos_popup_switch_menu_item_new_with_params(_("Placed at the top of the screen"), FALSE, NULL);
    cdos_popup_base_menu_item_set_keep_menu(switch_top, FALSE);
    cdos_panel_add_menu_item(self, switch_top);
    g_settings_bind_with_mapping(self->settings, "desktop-layout",
                                 switch_top, "state",
                                 G_SETTINGS_BIND_DEFAULT,
                                 NULL, NULL, self, NULL);

    gpointer switch_autohide = cdos_popup_switch_menu_item_new_with_params(_("Auto hide panel"), FALSE, NULL);
    cdos_popup_base_menu_item_set_keep_menu(switch_autohide, FALSE);
    cdos_panel_add_menu_item(self, switch_autohide);
    g_settings_bind(self->settings, "panel-autohide",
                    switch_autohide, "state", G_SETTINGS_BIND_DEFAULT);

    self->menu_mgr = cdos_popup_menu_manager_new(actor);
    cdos_popup_menu_manager_add_menu(self->menu_mgr, self->context_menu, -1);

    (void)cdos_drop_action_new(self->box);

    return self;
}

ClutterActor *
cdos_panel_get_leftbox(CdosPanel *self)
{
    g_return_val_if_fail(CDOS_IS_PANEL(self), NULL);
    return self->left_box;
}

ClutterActor *
cdos_panel_get_centerbox(CdosPanel *self)
{
    g_return_val_if_fail(CDOS_IS_PANEL(self), NULL);
    return self->center_box;
}

ClutterActor *
cdos_panel_get_rightbox(CdosPanel *self)
{
    g_return_val_if_fail(CDOS_IS_PANEL(self), NULL);
    return self->right_box;
}

void
cdos_panel_enable(CdosPanel *self, gint delay_ms)
{
    g_return_if_fail(CDOS_IS_PANEL(self));
    if (delay_ms < 0) {
        delay_ms = 250;
    }
    self->enable_timeout_id = g_timeout_add((guint)delay_ms, NULL, self);
}

void
cdos_panel_disable(CdosPanel *self, gint delay_ms)
{
    g_return_if_fail(CDOS_IS_PANEL(self));
    if (delay_ms < 0) {
        delay_ms = self->hide_delay_ms;
    }
    self->disable_timeout_id = g_timeout_add((guint)delay_ms, NULL, self);
}

gboolean
cdos_panel_get_hidden(CdosPanel *self)
{
    g_return_val_if_fail(CDOS_IS_PANEL(self), FALSE);
    return self->hidden;
}

gboolean
cdos_panel_get_hideable(CdosPanel *self)
{
    g_return_val_if_fail(CDOS_IS_PANEL(self), FALSE);
    return self->hideable;
}

void
cdos_panel_set_hideable(CdosPanel *self, gboolean hideable)
{
    g_return_if_fail(CDOS_IS_PANEL(self));
    self->hideable = hideable;
}

gboolean
cdos_panel_get_bottom_position(CdosPanel *self)
{
    g_return_val_if_fail(CDOS_IS_PANEL(self), TRUE);
    return (self->flags & 0x01) != 0;
}

void
cdos_panel_set_bottom_position(CdosPanel *self, gboolean bottom_position)
{
    g_return_if_fail(CDOS_IS_PANEL(self));
    self->flags = (self->flags & ~0x01) | (bottom_position ? 1 : 0);
    if (self->context_menu != NULL) {
        cdos_popup_menu_set_arrow_side(self->context_menu, bottom_position ? 2 : 0);
    }
}

gfloat
cdos_panel_get_height(CdosPanel *self)
{
    g_return_val_if_fail(CDOS_IS_PANEL(self), 0.0f);
    return clutter_actor_get_height(CLUTTER_ACTOR(self));
}

void
cdos_panel_set_monitor(CdosPanel *self, gint monitor_index)
{
    g_return_if_fail(CDOS_IS_PANEL(self));
    self->monitor_index = monitor_index;
    clutter_actor_queue_relayout(CLUTTER_ACTOR(self));
}

void
cdos_panel_add_launcher(CdosPanel *self, const gchar *desktop_id)
{
    g_return_if_fail(CDOS_IS_PANEL(self));
    gpointer item = panel_create_action_menu_item(self, desktop_id, NULL, NULL, NULL);
    cdos_popup_menu_base_add_menu_item(self->context_menu, item, -1);
}

void
cdos_panel_add_separator_item(CdosPanel *self)
{
    g_return_if_fail(CDOS_IS_PANEL(self));
    cdos_popup_menu_base_add_menu_item(self->context_menu, cdos_popup_separator_menu_item_new(), -1);
}

void
cdos_panel_add_menu_item(CdosPanel *self, gpointer menu_item)
{
    g_return_if_fail(CDOS_IS_PANEL(self));
    cdos_popup_menu_base_add_menu_item(self->context_menu, menu_item, -1);
    st_widget_add_style_class_name(menu_item, "popup-contextmenu-item");
}

gboolean
cdos_panel_on_button_press(CdosPanel *self, ClutterEvent *event)
{
    guint button = clutter_event_get_button(event);
    gboolean ctx_disabled = g_settings_get_boolean(self->settings, "panel-contextmenu-disabled");

    if (button == 1) {
        if (self->context_menu != NULL) {
            gboolean is_open = *(gint *)((guint8 *)self->context_menu + 0x4c) != 0;
            if (is_open) {
                cdos_popup_menu_close(self->context_menu, TRUE);
                return TRUE;
            }
        }
    } else if (button == 3 && !ctx_disabled) {
        gfloat x = 0.0f, y = 0.0f;
        clutter_event_get_coords(event, &x, &y);
        ClutterStage *stage = cdos_global_get_stage(cdos_global_get());
        ClutterActor *hit = clutter_stage_get_actor_at_pos(stage, CLUTTER_PICK_ALL, x, y);

        if (clutter_actor_contains(self->box, hit)) {
            cdos_popup_menu_base_toggle(self->context_menu);
        }

        gboolean is_open = *(gint *)((guint8 *)self->context_menu + 0x4c) != 0;
        if (!is_open) {
            return FALSE;
        }

        gpointer box_ptr = cdos_popup_menu_get_box_pointer(self->context_menu);
        gfloat width = clutter_actor_get_width(CLUTTER_ACTOR(self));
        gfloat local_x = 0.0f;
        clutter_actor_transform_stage_point(CLUTTER_ACTOR(self), x, 0.0f, &local_x, NULL);
        cdos_box_pointer_set_source_alignment(local_x / width, box_ptr);
    }

    return TRUE;
}

CdosPanelItemTooltip *
cdos_panel_item_tooltip_new(ClutterActor *item, const gchar *text)
{
    return g_object_new(cdos_panel_item_tooltip_get_type(),
                        "item", item,
                        "text", text,
                        NULL);
}
