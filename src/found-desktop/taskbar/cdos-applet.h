#ifndef __CDOS_APPLET_H__
#define __CDOS_APPLET_H__

#include "clutter-compat.h"

G_BEGIN_DECLS

#define CDOS_TYPE_APPLET (cdos_applet_get_type())
G_DECLARE_FINAL_TYPE(CdosApplet, cdos_applet, CDOS, APPLET, GObject)

GType cdos_applet_get_type(void);

ClutterActor *cdos_applet_get_actor(CdosApplet *self);
const gchar *cdos_applet_get_uuid(CdosApplet *self);
void cdos_applet_set_uuid(CdosApplet *self, const gchar *uuid);
gpointer cdos_applet_get_extension(CdosApplet *self);
void cdos_applet_set_extension(CdosApplet *self, gpointer extension);

gint cdos_applet_get_order(CdosApplet *self);
void cdos_applet_set_order(CdosApplet *self, gint order);
gint cdos_applet_get_new_order(CdosApplet *self);
void cdos_applet_set_new_order(CdosApplet *self, gint new_order);

gint cdos_applet_get_panel_location(CdosApplet *self);
void cdos_applet_set_panel_location(CdosApplet *self, gint location);
gint cdos_applet_get_new_panel_location(CdosApplet *self);
void cdos_applet_set_new_panel_location(CdosApplet *self, gint location);

gfloat cdos_applet_get_panel_height(CdosApplet *self);
void cdos_applet_set_panel_height(CdosApplet *self, gfloat height);
void cdos_applet_set_orientation(CdosApplet *self, gint orientation);
void cdos_applet_set_applet_tooltip(CdosApplet *self, const gchar *text);

gpointer cdos_applet_get_popup_menu(CdosApplet *self);
gboolean cdos_applet_get_popup_menu_active(CdosApplet *self);
void cdos_applet_set_popup_menu_active(CdosApplet *self, gboolean active);

gpointer cdos_applet_get_context_menu(CdosApplet *self);
gboolean cdos_applet_get_context_menu_active(CdosApplet *self);
void cdos_applet_set_context_menu_active(CdosApplet *self, gboolean active);
gpointer cdos_applet_get_menu_manager(CdosApplet *self);

/* Applet Manager */
void cdos_applet_manager_init(void);
void cdos_applet_manager_finish_extension_load(void);
void cdos_applet_manager_remove_applet_from_panel(const gchar *uuid, const gchar *id);
void cdos_applet_manager_save_applets_positions(void);
void cdos_applet_manager_set_applets_orientation(gint orientation);

G_END_DECLS

#endif /* __CDOS_APPLET_H__ */
