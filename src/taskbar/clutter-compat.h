#ifndef __CLUTTER_COMPAT_H__
#define __CLUTTER_COMPAT_H__

#include <glib-object.h>
#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>
#include <glib/gstdio.h>

G_BEGIN_DECLS

typedef struct _ClutterActor ClutterActor;
typedef struct _ClutterStage ClutterStage;
typedef union  _ClutterEvent ClutterEvent;

#define CLUTTER_ACTOR(obj) ((ClutterActor *)(obj))
#define CLUTTER_PICK_ALL 1

extern GType clutter_actor_get_type(void);
extern void clutter_actor_add_child(ClutterActor *self, ClutterActor *child);
extern void clutter_actor_remove_child(ClutterActor *self, ClutterActor *child);
extern GList *clutter_actor_get_children(ClutterActor *self);
extern void clutter_actor_show(ClutterActor *self);
extern void clutter_actor_hide(ClutterActor *self);
extern gfloat clutter_actor_get_width(ClutterActor *self);
extern gfloat clutter_actor_get_height(ClutterActor *self);
extern void clutter_actor_queue_relayout(ClutterActor *self);
extern gboolean clutter_actor_contains(ClutterActor *self, ClutterActor *descendant);
extern gboolean clutter_actor_transform_stage_point(ClutterActor *self, gfloat x, gfloat y,
                                                    gfloat *x_out, gfloat *y_out);

extern guint32 clutter_event_get_button(const ClutterEvent *event);
extern void clutter_event_get_coords(const ClutterEvent *event, gfloat *x_out, gfloat *y_out);
extern ClutterActor *clutter_stage_get_actor_at_pos(ClutterStage *stage, gint pick_mode,
                                                    gfloat x, gfloat y);

extern GType st_widget_get_type(void);
extern GType st_box_layout_get_type(void);
extern void st_box_layout_set_vertical(gpointer box, gboolean vertical);
extern void st_widget_set_style_class_name(gpointer widget, const gchar *class_name);
extern void st_widget_add_style_class_name(gpointer widget, const gchar *class_name);
extern void st_widget_remove_style_class_name(gpointer widget, const gchar *class_name);
extern void st_widget_add_style_pseudo_class(gpointer widget, const gchar *pseudo_class);
extern void st_widget_remove_style_pseudo_class(gpointer widget, const gchar *pseudo_class);

extern gpointer cdos_global_get(void);
extern ClutterStage *cdos_global_get_stage(gpointer global);
extern gpointer cdos_global_get_window_manager(gpointer global);
extern ClutterActor *cdos_ui_plugin_get_ui_group(void);
extern gint cdos_ui_plugin_get_applet_side(void);

extern GType cdos_app_get_type(void);
extern const gchar *cdos_app_get_id(gpointer app);
extern GSList *cdos_app_get_windows(gpointer app);

extern gpointer cdos_window_tracker_get_default(void);
extern gpointer cdos_window_tracker_get_window_app(gpointer tracker, gpointer meta_window);
extern gboolean cdos_window_tracker_is_window_interesting(gpointer meta_window);

extern gpointer meta_display_get_workspace_manager(gpointer display);
extern gint meta_workspace_manager_get_active_workspace_index(gpointer mgr);
extern gpointer meta_workspace_manager_get_workspace_by_index(gpointer mgr, gint index);
extern GList *meta_workspace_list_windows(gpointer workspace);
extern gint meta_window_get_window_type(gpointer window);
extern gpointer meta_window_get_workspace(gpointer window);
extern gboolean meta_window_appears_focused(gpointer window);
extern gboolean meta_window_has_focus(gpointer window);

extern GType applet_icon_label_button_get_type(void);
extern gpointer applet_icon_label_button_get_app(gpointer button);
extern GType applet_window_button_get_type(void);
extern gpointer applet_window_button_new(gpointer app, gboolean is_favorite, gpointer meta_window);
extern void applet_button_box_add_button(gpointer box, gpointer button);
extern void applet_button_box_set_last_focused_window(gpointer box, gpointer meta_window);
extern gpointer applet_tree_get_first_node(GTree *tree);

extern GType cdos_popup_menu_get_type(void);
extern GType cdos_popup_menu_base_get_type(void);
extern GType cdos_popup_base_menu_item_get_type(void);
extern gpointer cdos_popup_menu_base_get_box(gpointer menu);
extern void cdos_popup_menu_base_add_menu_item(gpointer menu, gpointer item, gint position);
extern void cdos_popup_menu_base_set_source_actor(gpointer menu, gpointer actor);
extern void cdos_popup_menu_open(gpointer menu, gboolean animate);
extern gpointer cdos_popup_menu_manager_new(ClutterActor *owner);
extern void cdos_popup_menu_manager_add_menu(gpointer mgr, gpointer menu, gint position);
extern void cdos_popup_menu_manager_block_all_events(gpointer mgr, gboolean block);
extern gpointer cdos_drag_action_new(ClutterActor *actor);

G_END_DECLS

#endif /* __CLUTTER_COMPAT_H__ */
