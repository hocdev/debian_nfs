#include "applet-panel-launchers.h"

gchar *
applet_panel_launchers_get_dirname(void)
{
    gchar *dir = g_build_filename(g_get_user_data_dir(), "/cdos/panel-launchers/", NULL);
    if (!g_file_test(dir, G_FILE_TEST_IS_DIR)) {
        g_mkdir_with_parents(dir, 0755);
    }
    return dir;
}
