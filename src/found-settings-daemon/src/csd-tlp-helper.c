#include <fcntl.h>
#include <glib.h>
#include <unistd.h>

static const char config_path[] = "/etc/default/tlp";

static GString *
format_config(gboolean powersave, gboolean performance)
{
    const char *profile_ac = "default";
    const char *profile_battery = "default";
    const char *dpm_ac = "auto";
    const char *dpm_battery = "auto";
    const char *governor = NULL;

    if (powersave) {
        profile_battery = dpm_battery = "low";
        governor = "powersave";
    } else if (performance) {
        profile_ac = profile_battery = dpm_ac = dpm_battery = "high";
        governor = "performance";
    }

    GString *config = g_string_new("# Cdos Settings Daemon TLP config file\n"
                                   "TLP_ENABLE=1\n");
    g_string_append_printf(config,
                          "RADEON_POWER_PROFILE_ON_AC=%s\n"
                          "RADEON_POWER_PROFILE_ON_BAT=%s\n"
                          "RADEON_DPM_PERF_LEVEL_ON_AC=%s\n"
                          "RADEON_DPM_PERF_LEVEL_ON_BAT=%s\n",
                          profile_ac, profile_battery, dpm_ac, dpm_battery);
    if (governor) {
        g_string_append_printf(config, "CPU_SCALING_GOVERNOR_ON_AC=%s\n"
                                       "CPU_SCALING_GOVERNOR_ON_BAT=%s\n",
                               governor, governor);
    } else {
        g_string_append_c(config, '#');
    }
    g_string_append(config, "#USB_AUTOSUSPEND=0\n");
    return config;
}

int
main(int argc, char **argv)
{
    gboolean powersave = FALSE;
    gboolean balance = FALSE;
    gboolean performance = FALSE;
    const GOptionEntry options[] = {
        { "powersave", 0, 0, G_OPTION_ARG_NONE, &powersave, "Change to powersave mode", NULL },
        { "balance", 0, 0, G_OPTION_ARG_NONE, &balance, "Change to balance mode", NULL },
        { "performance", 0, 0, G_OPTION_ARG_NONE, &performance, "Change to performance mode", NULL },
        { NULL }
    };

    g_autoptr(GOptionContext) context = g_option_context_new(NULL);
    g_option_context_set_summary(context, "Cdos Settings Daemon TLP Helper");
    g_option_context_add_main_entries(context, options, NULL);
    g_option_context_parse(context, &argc, &argv, NULL);
    if (!powersave && !balance && !performance)
        g_warning("No valid option was specified. Fallback to balance mode");

    g_autoptr(GString) config = format_config(powersave, performance);
    int fd = open(config_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        g_warning("failed to open filename: %s", config_path);
        return 3;
    }

    ssize_t written = write(fd, config->str, config->len);
    int closed = close(fd);
    if (written != (ssize_t) config->len || closed < 0) {
        g_warning("writing '%s' to %s failed", config->str, config_path);
        return 3;
    }

    g_autoptr(GError) error = NULL;
    if (!g_spawn_command_line_async("tlp start", &error))
        g_warning("%s", error->message);
    return 0;
}
