/**
 * Reconstructed from libfound-desktop-4.so.0.0.0 (4.0.10-m1+2nfs5)
 * Decompiled via REA + Ghidra 12.2
 */

#include "found-desktop-utils.h"
#include <string.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "FoundDesktop"

static gint cached_boot_is_live = -1;

gboolean
fd_env_lang_is_en(void)
{
    const gchar *lang = g_getenv("LANG");
    if (lang != NULL) {
        return g_str_has_prefix(lang, "en");
    }
    return FALSE;
}

gboolean
fd_boot_is_live(void)
{
    if (cached_boot_is_live == -1) {
        gchar *cmdline = NULL;
        g_spawn_command_line_sync("cat /proc/cmdline", &cmdline, NULL, NULL, NULL);
        gboolean is_live = (cmdline != NULL && g_strrstr(cmdline, "boot=live") != NULL);
        g_free(cmdline);
        cached_boot_is_live = is_live ? 1 : 0;
    }
    return cached_boot_is_live == 1;
}

gchar **
fd_strv_del_by_nth(gchar **strv, gint nth)
{
    if (strv == NULL || nth < 0) {
        return strv;
    }
    guint len = g_strv_length(strv);
    if ((guint)nth >= len) {
        return strv;
    }
    g_free(strv[nth]);
    for (guint i = (guint)nth; i < len; i++) {
        strv[i] = strv[i + 1];
    }
    return strv;
}

gchar **
fd_strv_del_by_str(gchar **strv, const gchar *target)
{
    if (strv == NULL || target == NULL) {
        return strv;
    }
    for (guint i = 0; strv[i] != NULL; ) {
        if (g_strcmp0(strv[i], target) == 0) {
            fd_strv_del_by_nth(strv, (gint)i);
        } else {
            i++;
        }
    }
    return strv;
}

gchar **
fd_strv_del_by_func(gchar **strv, FdStrvFilterFunc func, gpointer user_data)
{
    if (strv == NULL || func == NULL) {
        return strv;
    }
    for (guint i = 0; strv[i] != NULL; ) {
        if (func(strv[i], user_data)) {
            fd_strv_del_by_nth(strv, (gint)i);
        } else {
            i++;
        }
    }
    return strv;
}

gchar **
fd_strv_insert(gchar **strv, const gchar *str, gint pos)
{
    guint len = strv ? g_strv_length(strv) : 0;
    if (pos < 0 || (guint)pos > len) {
        pos = (gint)len;
    }
    gchar **res = g_new0(gchar *, len + 2);
    for (gint i = 0; i < pos; i++) {
        res[i] = g_strdup(strv[i]);
    }
    res[pos] = g_strdup(str);
    for (guint i = (guint)pos; i < len; i++) {
        res[i + 1] = g_strdup(strv[i]);
    }
    g_strfreev(strv);
    return res;
}

gchar **
fd_strv_get_sub_strv(gchar **strv, gint start, gint len)
{
    if (strv == NULL || start < 0 || len <= 0) {
        return g_new0(gchar *, 1);
    }
    guint total = g_strv_length(strv);
    if ((guint)start >= total) {
        return g_new0(gchar *, 1);
    }
    guint count = MIN((guint)len, total - (guint)start);
    gchar **res = g_new0(gchar *, count + 1);
    for (guint i = 0; i < count; i++) {
        res[i] = g_strdup(strv[start + i]);
    }
    return res;
}

gchar **
fd_strv_get_sub_strv_by_func(gchar **strv, FdStrvFilterFunc func, gpointer user_data)
{
    GPtrArray *arr = g_ptr_array_new();
    if (strv != NULL && func != NULL) {
        for (guint i = 0; strv[i] != NULL; i++) {
            if (func(strv[i], user_data)) {
                g_ptr_array_add(arr, g_strdup(strv[i]));
            }
        }
    }
    g_ptr_array_add(arr, NULL);
    return (gchar **)g_ptr_array_free(arr, FALSE);
}

const gchar *
fd_action_get_name(gint action_id)
{
    switch (action_id) {
    case 1: return "shutdown";
    case 2: return "reboot";
    case 3: return "suspend";
    case 4: return "hibernate";
    case 5: return "logout";
    case 6: return "lock";
    case 7: return "switch-user";
    default: return NULL;
    }
}

const gchar *
fd_action_get_display_name(gint action_id)
{
    return fd_action_get_name(action_id);
}

gboolean
fd_action_is_disabled_by_name(const gchar *section, gint action_id)
{
    if (section == NULL || action_id == 0) {
        return FALSE;
    }
    const gchar *conf = "/etc/desktop/found-actions";
    if (!g_file_test(conf, G_FILE_TEST_EXISTS)) {
        return FALSE;
    }
    GKeyFile *kf = g_key_file_new();
    GError *err = NULL;
    if (!g_key_file_load_from_file(kf, conf, G_KEY_FILE_NONE, &err)) {
        g_warning("Load %s failed(%d): %s.", conf, err ? err->code : 0, err ? err->message : "");
        g_clear_error(&err);
        g_key_file_unref(kf);
        return FALSE;
    }

    gboolean disabled = FALSE;
    gchar *key = g_ascii_strdown(section, -1);
    if (g_key_file_has_key(kf, "Disabled Actions", key, NULL)) {
        gchar **list = g_key_file_get_string_list(kf, "Disabled Actions", key, NULL, NULL);
        const gchar *name = fd_action_get_name(action_id);
        if (list != NULL && name != NULL) {
            for (guint i = 0; list[i] != NULL; i++) {
                if (g_ascii_strcasecmp(list[i], name) == 0) {
                    disabled = TRUE;
                    break;
                }
            }
        }
        g_strfreev(list);
    }
    g_free(key);
    g_key_file_unref(kf);
    return disabled;
}

gboolean
fd_action_is_disabled(gint action_id)
{
    return fd_action_is_disabled_by_name("global", action_id);
}
