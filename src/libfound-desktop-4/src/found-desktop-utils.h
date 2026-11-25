#ifndef __FOUND_DESKTOP_UTILS_H__
#define __FOUND_DESKTOP_UTILS_H__

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

typedef gboolean (*FdStrvFilterFunc)(const gchar *str, gpointer user_data);

gboolean fd_env_lang_is_en(void);
gboolean fd_boot_is_live(void);

gchar **fd_strv_del_by_nth(gchar **strv, gint nth);
gchar **fd_strv_del_by_str(gchar **strv, const gchar *target);
gchar **fd_strv_del_by_func(gchar **strv, FdStrvFilterFunc func, gpointer user_data);
gchar **fd_strv_insert(gchar **strv, const gchar *str, gint pos);
gchar **fd_strv_get_sub_strv(gchar **strv, gint start, gint len);
gchar **fd_strv_get_sub_strv_by_func(gchar **strv, FdStrvFilterFunc func, gpointer user_data);

const gchar *fd_action_get_name(gint action_id);
const gchar *fd_action_get_display_name(gint action_id);
gboolean fd_action_is_disabled_by_name(const gchar *section, gint action_id);
gboolean fd_action_is_disabled(gint action_id);

G_END_DECLS

#endif /* __FOUND_DESKTOP_UTILS_H__ */
