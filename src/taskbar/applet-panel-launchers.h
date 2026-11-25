#ifndef __APPLET_PANEL_LAUNCHERS_H__
#define __APPLET_PANEL_LAUNCHERS_H__

#include "clutter-compat.h"

G_BEGIN_DECLS

gchar *applet_panel_launchers_get_dirname(void);
void applet_panel_launchers_remove_invalid_files(void);
void applet_panel_launchers_update_files(void);
gpointer applet_panel_launchers_create_icon_texture(gpointer cdos_app, gint icon_size);
gchar *applet_panel_launchers_get_display_name(gpointer cdos_app);
gchar *applet_panel_launchers_get_uri(gpointer cdos_app);
void applet_panel_launchers_edit_properties(gpointer cdos_app);

G_END_DECLS

#endif /* __APPLET_PANEL_LAUNCHERS_H__ */
