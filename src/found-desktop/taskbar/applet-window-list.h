#ifndef __APPLET_WINDOW_LIST_H__
#define __APPLET_WINDOW_LIST_H__

#include "clutter-compat.h"

G_BEGIN_DECLS

typedef struct _AppletWindowList AppletWindowList;
typedef struct _AppletAppList AppletAppList;
typedef struct _AppletAppGroup AppletAppGroup;
typedef struct _AppletAppButton AppletAppButton;
typedef struct _AppletAppSwitcherItem AppletAppSwitcherItem;
typedef struct _AppletAppThumbnailHoverMenu AppletAppThumbnailHoverMenu;
typedef struct _AppletAppContextMenu AppletAppContextMenu;

/* Window List Applet (window-list@cdos.org) */
GType applet_window_list_get_type(void);
AppletWindowList *applets_window_list_main(gpointer metadata, gint orientation);
AppletWindowList *app_window_list_get_default(void);
gfloat applet_window_list_get_panel_height(AppletWindowList *self);
GSettings *applet_window_list_get_settings(AppletWindowList *self);

/* App List (.window-list-app-list) */
GType applet_app_list_get_type(void);
AppletAppList *applet_app_list_new(gpointer meta_workspace);
void applet_app_list_set_max_width(AppletAppList *self, gfloat max_width);
void applet_app_list_set_orientation(AppletAppList *self, gint orientation);
AppletAppGroup *applet_app_list_get_group_by_app(AppletAppList *self, gpointer cdos_app);

/* App Group */
GType applet_app_group_get_type(void);
AppletAppGroup *applet_app_group_new(gpointer cdos_app, gboolean is_favorite, gpointer meta_workspace);
void applet_app_group_update_meta_windows(AppletAppGroup *self, gpointer meta_workspace);
void applet_app_group_window_removed(AppletAppGroup *self, gpointer workspace, gpointer meta_window);
void applet_app_group_is_favorite(AppletAppGroup *self, gboolean is_favorite);
gboolean applet_app_group_get_is_favorite_app(AppletAppGroup *self);
gboolean applet_app_group_get_was_favorite_app(AppletAppGroup *self);
void applet_app_group_set_buttons_condense(AppletAppGroup *self, gboolean condense);
gpointer applet_app_group_get_window_button_box(AppletAppGroup *self);
void applet_app_group_set_orientation(AppletAppGroup *self, gint orientation);
AppletAppButton *applet_app_group_get_app_button(AppletAppGroup *self);
AppletAppSwitcherItem *applet_app_group_get_app_switcher_item(AppletAppGroup *self);

/* App Button (.window-list-app-button) */
GType applet_app_button_get_type(void);
AppletAppButton *applet_app_button_new(gpointer cdos_app, gboolean is_favorite, gpointer meta_workspace);
void applet_app_button_update_actor_style(AppletAppButton *self);
void applet_app_button_highlight_start(AppletAppButton *self);
void applet_app_button_highlight_stop(AppletAppButton *self);

/* Context Menu (.window-list-context-menu) */
GType applet_app_context_menu_get_type(void);
AppletAppContextMenu *applet_app_context_menu_new(gpointer cdos_app, ClutterActor *source, gint arrow_side);
void applet_app_context_menu_set_meta_window(AppletAppContextMenu *self, gpointer meta_window);
gpointer applet_app_context_menu_get_meta_window(AppletAppContextMenu *self);
gpointer applet_app_context_menu_get_app(AppletAppContextMenu *self);

/* Switcher Item (.switcher-list .item-box) */
GType applet_app_switcher_item_get_type(void);
AppletAppSwitcherItem *applet_app_switcher_item_new(gpointer cdos_app, gboolean is_favorite, gpointer meta_window);
void applet_app_switcher_item_set_is_favorite(AppletAppSwitcherItem *self, gboolean is_favorite);
void applet_app_switcher_item_refresh(AppletAppSwitcherItem *self);
void applet_app_switcher_item_set_meta_window(AppletAppSwitcherItem *self, gpointer meta_window);
gpointer applet_app_switcher_item_get_meta_window(AppletAppSwitcherItem *self);
gpointer applet_app_switcher_item_get_app(AppletAppSwitcherItem *self);
ClutterActor *applet_app_switcher_item_get_box(AppletAppSwitcherItem *self);

/* Thumbnail Hover Menu (.window-list-hover-menu) */
GType applet_app_thumbnail_hover_menu_get_type(void);
AppletAppThumbnailHoverMenu *applet_app_thumbnail_hover_menu_new(
    gpointer cdos_app, gboolean is_favorite, ClutterActor *source_actor, gint arrow_side);
void applet_app_thumbnail_hover_menu_open(AppletAppThumbnailHoverMenu *self, gboolean animate);
void applet_app_thumbnail_hover_menu_close(AppletAppThumbnailHoverMenu *self, gboolean animate);
void applet_app_thumbnail_hover_menu_set_meta_window(AppletAppThumbnailHoverMenu *self, gpointer meta_window);
gpointer applet_app_thumbnail_hover_menu_get_meta_window(AppletAppThumbnailHoverMenu *self);
AppletAppSwitcherItem *applet_app_thumbnail_hover_menu_get_app_switcher_item(AppletAppThumbnailHoverMenu *self);
void applet_app_thumbnail_hover_menu_set_should_open(AppletAppThumbnailHoverMenu *self, gboolean should_open);
void applet_app_thumbnail_hover_menu_refresh(AppletAppThumbnailHoverMenu *self);
void applet_app_thumbnail_hover_menu_set_context_menu(AppletAppThumbnailHoverMenu *self, AppletAppContextMenu *menu);
void applet_app_thumbnail_hover_menu_set_is_keep_menu(AppletAppThumbnailHoverMenu *self, gboolean keep);

G_END_DECLS

#endif /* __APPLET_WINDOW_LIST_H__ */
