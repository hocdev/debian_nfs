#include "found-desktop-utils.h"

gboolean
fd_env_lang_is_en(void)
{
    const gchar *lang = g_getenv("LANG");
    return lang != NULL && g_str_has_prefix(lang, "en_");
}

gboolean
fd_boot_is_live(void)
{
    static gsize boot_type;

    if (g_once_init_enter(&boot_type)) {
        gchar *cmdline = NULL;
        gboolean is_live = g_file_get_contents("/proc/cmdline", &cmdline, NULL, NULL) &&
                           g_strrstr(cmdline, "boot=live") != NULL;
        g_free(cmdline);
        g_once_init_leave(&boot_type, is_live ? 2 : 1);
    }
    return boot_type == 2;
}
