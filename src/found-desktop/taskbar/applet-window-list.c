/**
 * Reconstructed from libcdos-desktop.so (found-desktop 4.0.5-m117+1nfs5)
 * Decompiled via REA + Ghidra 12.2
 */

#include "applet-window-list.h"

static AppletWindowList *default_window_list = NULL;

/* ============================================================================
 * AppletWindowList
 * ============================================================================ */

AppletWindowList *
applets_window_list_main(gpointer metadata, gint orientation)
{
    (void)metadata;
    AppletWindowList *self = g_object_new(applet_window_list_get_type(),
                                          "orientation", orientation,
                                          NULL);
    default_window_list = self;

    gpointer global = cdos_global_get();
    cdos_global_get_window_manager(global);

    gpointer workspace_mgr = meta_display_get_workspace_manager(NULL);
    gint active_idx = meta_workspace_manager_get_active_workspace_index(workspace_mgr);
    gpointer active_ws = meta_workspace_manager_get_workspace_by_index(workspace_mgr, active_idx);

    AppletAppList *app_list = applet_app_list_new(active_ws);
    clutter_actor_add_child(CLUTTER_ACTOR(self), CLUTTER_ACTOR(app_list));
    st_widget_add_style_class_name(self, "applet-box-window-list");

    return self;
}

AppletWindowList *
app_window_list_get_default(void)
{
    return default_window_list;
}

/* ============================================================================
 * AppletAppList
 * ============================================================================ */

AppletAppList *
applet_app_list_new(gpointer meta_workspace)
{
    (void)meta_workspace;
    return g_object_new(applet_app_list_get_type(),
                        "x-align", 1,
                        "style-class", "window-list-app-list",
                        NULL);
}

AppletAppGroup *
applet_app_list_get_group_by_app(AppletAppList *self, gpointer cdos_app)
{
    GTree *app_tree = *(GTree **)(*(guint8 **)((guint8 *)self + 0x30) + 0x30);
    return g_tree_lookup(app_tree, cdos_app);
}

/* ============================================================================
 * AppletAppGroup
 * ============================================================================ */

typedef struct {
    gboolean is_favorite;           /* +0x00 */
    gboolean was_favorite;          /* +0x04 */
    gpointer app;                   /* +0x08 */
    GTree *window_buttons;          /* +0x10 */
    AppletAppContextMenu *ctx_menu; /* +0x20 */
    AppletAppThumbnailHoverMenu *hover_menu; /* +0x28 */
    gpointer last_focused_window;   /* +0x30 */
    gpointer button_box;            /* +0x38 */
    AppletAppButton *app_button;    /* +0x40 */
    gpointer menu_mgr_ctx;          /* +0x48 */
    gpointer menu_mgr_hover;        /* +0x50 */
    gpointer workspace;             /* +0x58 */
    gboolean buttons_condense;      /* +0x60 */
    gint arrow_side;                /* +0x70 */
} AppletAppGroupPrivate;

AppletAppGroup *
applet_app_group_new(gpointer cdos_app, gboolean is_favorite, gpointer meta_workspace)
{
    g_return_val_if_fail(G_TYPE_CHECK_INSTANCE_TYPE(cdos_app, cdos_app_get_type()), NULL);

    AppletAppGroup *self = g_object_new(applet_app_group_get_type(), NULL);
    AppletAppGroupPrivate *priv = *(AppletAppGroupPrivate **)((guint8 *)self + 0x28);

    priv->app = cdos_app;
    priv->workspace = meta_workspace;
    priv->is_favorite = is_favorite;
    priv->arrow_side = cdos_ui_plugin_get_applet_side();

    priv->app_button = applet_app_button_new(priv->app, priv->is_favorite, priv->workspace);
    clutter_actor_add_child(CLUTTER_ACTOR(self), CLUTTER_ACTOR(priv->app_button));

    priv->hover_menu = applet_app_thumbnail_hover_menu_new(
        priv->app, priv->is_favorite, CLUTTER_ACTOR(priv->app_button), priv->arrow_side);
    priv->menu_mgr_hover = cdos_popup_menu_manager_new(CLUTTER_ACTOR(self));
    cdos_popup_menu_manager_add_menu(priv->menu_mgr_hover, priv->hover_menu, -1);
    cdos_popup_menu_manager_block_all_events(priv->menu_mgr_hover, FALSE);

    priv->ctx_menu = applet_app_context_menu_new(priv->app, CLUTTER_ACTOR(self), priv->arrow_side);
    priv->menu_mgr_ctx = cdos_popup_menu_manager_new(CLUTTER_ACTOR(self));
    cdos_popup_menu_manager_add_menu(priv->menu_mgr_ctx, priv->ctx_menu, -1);

    gpointer drag_action = cdos_drag_action_new(CLUTTER_ACTOR(self));
    if (priv->is_favorite) {
        const gchar *app_id = cdos_app_get_id(priv->app);
        if (app_id != NULL) {
            g_object_set_data_full(drag_action, "desktop-id", g_strdup(app_id), g_free);
        }
    }

    return self;
}

void
applet_app_group_is_favorite(AppletAppGroup *self, gboolean is_favorite)
{
    g_return_if_fail(G_TYPE_CHECK_INSTANCE_TYPE(self, applet_app_group_get_type()));
    AppletAppGroupPrivate *priv = *(AppletAppGroupPrivate **)((guint8 *)self + 0x28);
    priv->is_favorite = is_favorite;
    priv->was_favorite = !is_favorite;
    AppletAppSwitcherItem *item = applet_app_thumbnail_hover_menu_get_app_switcher_item(priv->hover_menu);
    applet_app_switcher_item_set_is_favorite(item, is_favorite);
}

gboolean
applet_app_group_get_is_favorite_app(AppletAppGroup *self)
{
    AppletAppGroupPrivate *priv = *(AppletAppGroupPrivate **)((guint8 *)self + 0x28);
    return priv->is_favorite;
}

gboolean
applet_app_group_get_was_favorite_app(AppletAppGroup *self)
{
    AppletAppGroupPrivate *priv = *(AppletAppGroupPrivate **)((guint8 *)self + 0x28);
    return priv->was_favorite;
}

gpointer
applet_app_group_get_window_button_box(AppletAppGroup *self)
{
    AppletAppGroupPrivate *priv = *(AppletAppGroupPrivate **)((guint8 *)self + 0x28);
    return priv->button_box;
}

AppletAppButton *
applet_app_group_get_app_button(AppletAppGroup *self)
{
    AppletAppGroupPrivate *priv = *(AppletAppGroupPrivate **)((guint8 *)self + 0x28);
    return priv->app_button;
}

AppletAppSwitcherItem *
applet_app_group_get_app_switcher_item(AppletAppGroup *self)
{
    AppletAppGroupPrivate *priv = *(AppletAppGroupPrivate **)((guint8 *)self + 0x28);
    return applet_app_thumbnail_hover_menu_get_app_switcher_item(priv->hover_menu);
}

void
applet_app_group_update_meta_windows(AppletAppGroup *self, gpointer meta_workspace)
{
    AppletAppGroupPrivate *priv = *(AppletAppGroupPrivate **)((guint8 *)self + 0x28);
    gpointer tracker = cdos_window_tracker_get_default();
    GList *windows = meta_workspace_list_windows(meta_workspace);

    for (GList *l = windows; l != NULL; l = l->next) {
        gpointer win = l->data;
        gpointer win_app = cdos_window_tracker_get_window_app(tracker, win);
        if (win_app == priv->app &&
            cdos_window_tracker_is_window_interesting(win) &&
            meta_workspace != NULL && win != NULL &&
            g_tree_lookup(priv->window_buttons, win) == NULL &&
            meta_window_get_window_type(win) == 0) {

            gpointer wbtn = applet_window_button_new(priv->app, priv->is_favorite, win);
            applet_button_box_add_button(priv->button_box, wbtn);
            priv->last_focused_window = win;
            applet_app_context_menu_set_meta_window(priv->ctx_menu, win);
            applet_app_thumbnail_hover_menu_set_meta_window(priv->hover_menu, win);
            applet_button_box_set_last_focused_window(priv->button_box, win);
            g_tree_insert(priv->window_buttons, win, wbtn);

            if (priv->is_favorite) {
                applet_app_group_is_favorite(self, FALSE);
            }
        }
    }
    g_list_free(windows);

    if (priv->last_focused_window == NULL) {
        applet_app_context_menu_set_meta_window(priv->ctx_menu, NULL);
        applet_app_thumbnail_hover_menu_set_meta_window(priv->hover_menu, NULL);
        applet_button_box_set_last_focused_window(priv->button_box, NULL);
    }
}

void
applet_app_group_window_removed(AppletAppGroup *self, gpointer workspace, gpointer meta_window)
{
    (void)workspace;
    AppletAppGroupPrivate *priv = *(AppletAppGroupPrivate **)((guint8 *)self + 0x28);

    g_tree_remove(priv->window_buttons, meta_window);
    gpointer first_win = applet_tree_get_first_node(priv->window_buttons);
    gpointer source = priv->app_button;
    if (!priv->buttons_condense) {
        gpointer found = g_tree_lookup(priv->window_buttons, first_win);
        if (found != NULL) {
            source = found;
        }
    }

    g_signal_handlers_disconnect_matched(meta_window, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, self);
    cdos_popup_menu_base_set_source_actor(priv->ctx_menu, source);
    applet_app_context_menu_set_meta_window(priv->ctx_menu, first_win);
    applet_app_thumbnail_hover_menu_set_is_keep_menu(priv->hover_menu, first_win != NULL);

    priv->last_focused_window = first_win;
    applet_app_context_menu_set_meta_window(priv->ctx_menu, first_win);
    applet_app_thumbnail_hover_menu_set_meta_window(priv->hover_menu, first_win);
    applet_button_box_set_last_focused_window(priv->button_box, first_win);
    applet_app_thumbnail_hover_menu_refresh(priv->hover_menu);
}

/* ============================================================================
 * AppletAppButton
 * ============================================================================ */

AppletAppButton *
applet_app_button_new(gpointer cdos_app, gboolean is_favorite, gpointer meta_workspace)
{
    (void)meta_workspace;
    return g_object_new(applet_app_button_get_type(),
                        "app", cdos_app,
                        "is-favorite", is_favorite,
                        "style-class", "window-list-app-button",
                        NULL);
}

void
applet_app_button_update_actor_style(AppletAppButton *self)
{
    g_return_if_fail(G_TYPE_CHECK_INSTANCE_TYPE(self, applet_app_button_get_type()));

    gpointer app = applet_icon_label_button_get_app(self);
    GSList *windows = cdos_app_get_windows(app);

    st_widget_remove_style_class_name(self, "window-list-item-box-one-active");
    st_widget_remove_style_class_name(self, "window-list-item-box-two-active");
    st_widget_remove_style_class_name(self, "window-list-item-box-more-active");

    if (windows == NULL) {
        st_widget_remove_style_pseudo_class(self, "focus");
        return;
    }

    gboolean any_focused = FALSE;
    gint count = 0;

    for (GSList *l = windows; l != NULL; l = l->next) {
        gpointer meta_win = l->data;
        if (meta_window_get_window_type(meta_win) == 0) {
            if (meta_window_appears_focused(meta_win) || meta_window_has_focus(meta_win)) {
                any_focused = TRUE;
            }
            count++;
            if (count > 2) {
                break;
            }
        }
    }

    if (!any_focused) {
        st_widget_remove_style_pseudo_class(self, "focus");
        return;
    }

    st_widget_add_style_pseudo_class(self, "focus");
    const gchar *style_class = "window-list-item-box-more-active";
    if (count == 1) {
        style_class = "window-list-item-box-one-active";
    } else if (count == 2) {
        style_class = "window-list-item-box-two-active";
    }
    st_widget_add_style_class_name(self, style_class);
}

/* ============================================================================
 * AppletAppThumbnailHoverMenu & AppletAppSwitcherItem & AppletAppContextMenu
 * ============================================================================ */

AppletAppThumbnailHoverMenu *
applet_app_thumbnail_hover_menu_new(gpointer cdos_app, gboolean is_favorite,
                                    ClutterActor *source_actor, gint arrow_side)
{
    AppletAppThumbnailHoverMenu *self = g_object_new(
        applet_app_thumbnail_hover_menu_get_type(),
        "source-actor", source_actor,
        "is-popup", TRUE,
        "arrow-side", arrow_side,
        "arrow-alignment", 0,
        NULL);

    st_widget_set_style_class_name(self, "window-list-hover-menu");

    AppletAppSwitcherItem *item = applet_app_switcher_item_new(cdos_app, is_favorite, NULL);
    cdos_popup_menu_base_add_menu_item(self, item, -1);

    ClutterActor *box = CLUTTER_ACTOR(cdos_popup_menu_base_get_box(self));
    clutter_actor_hide(box);
    clutter_actor_add_child(cdos_ui_plugin_get_ui_group(), box);

    return self;
}

void
applet_app_thumbnail_hover_menu_open(AppletAppThumbnailHoverMenu *self, gboolean animate)
{
    g_return_if_fail(G_TYPE_CHECK_INSTANCE_TYPE(self, applet_app_thumbnail_hover_menu_get_type()));
    ClutterActor *box = CLUTTER_ACTOR(cdos_popup_menu_base_get_box(self));
    clutter_actor_show(box);
    cdos_popup_menu_open(self, animate);
}

AppletAppContextMenu *
applet_app_context_menu_new(gpointer cdos_app, ClutterActor *source, gint arrow_side)
{
    AppletAppContextMenu *self = g_object_new(
        applet_app_context_menu_get_type(),
        "is-popup", TRUE,
        "source-actor", source,
        "arrow-side", arrow_side,
        NULL);
    (void)cdos_app;
    return self;
}

AppletAppSwitcherItem *
applet_app_switcher_item_new(gpointer cdos_app, gboolean is_favorite, gpointer meta_window)
{
    AppletAppSwitcherItem *self = g_object_new(applet_app_switcher_item_get_type(), NULL);
    (void)cdos_app;
    (void)is_favorite;
    (void)meta_window;
    applet_app_switcher_item_refresh(self);
    return self;
}
