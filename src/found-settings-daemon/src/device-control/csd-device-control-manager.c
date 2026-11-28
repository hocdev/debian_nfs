#define G_LOG_DOMAIN "device-control-plugin"

#include "csd-device-control-manager.h"

struct _CsdDeviceControlManager {
    GObject parent;
    GSettings *privacy;
};

G_DEFINE_TYPE(CsdDeviceControlManager, csd_device_control_manager, G_TYPE_OBJECT)

static void
camera_changed(GSettings *settings, const char *key, gpointer data)
{
    gboolean enable = !g_settings_get_boolean(settings, key);
    char *arguments[] = {
        "pkexec", "/usr/lib/found-settings-daemon/csd-camera-helper",
        enable ? "1" : "0", NULL
    };
    static char **environment;
    if (!environment)
        environment = g_environ_unsetenv(g_get_environ(), "SHELL");

    g_debug("ready to set camera: %d", enable);
    g_autoptr(GError) error = NULL;
    GPid pid;
    if (!g_spawn_async(NULL, arguments, environment,
                       G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD,
                       NULL, NULL, &pid, &error)) {
        g_warning("failed to set camera %d, err: %s", enable, error->message);
        return;
    }
    g_spawn_close_pid(pid);
}

static void
microphone_changed(GSettings *settings, const char *key, gpointer data)
{
    g_debug("Mic changed");
}

gboolean
csd_device_control_manager_start(CsdDeviceControlManager *manager, GError **error)
{
    g_debug("Starting device-control manager");
    if (manager->privacy)
        return TRUE;

    manager->privacy = g_settings_new("org.gnome.desktop.privacy");
    g_signal_connect(manager->privacy, "changed::disable-camera", G_CALLBACK(camera_changed), NULL);
    g_signal_connect(manager->privacy, "changed::disable-microphone", G_CALLBACK(microphone_changed), NULL);
    camera_changed(manager->privacy, "disable-camera", NULL);
    microphone_changed(manager->privacy, "disable-microphone", NULL);
    return TRUE;
}

void
csd_device_control_manager_stop(CsdDeviceControlManager *manager)
{
    g_debug("Stopping device-control manager");
    g_clear_object(&manager->privacy);
}

static void
csd_device_control_manager_finalize(GObject *object)
{
    csd_device_control_manager_stop(CSD_DEVICE_CONTROL_MANAGER(object));
    G_OBJECT_CLASS(csd_device_control_manager_parent_class)->finalize(object);
}

static void
csd_device_control_manager_class_init(CsdDeviceControlManagerClass *class)
{
    G_OBJECT_CLASS(class)->finalize = csd_device_control_manager_finalize;
}

static void
csd_device_control_manager_init(CsdDeviceControlManager *manager)
{
}

CsdDeviceControlManager *
csd_device_control_manager_new(void)
{
    static CsdDeviceControlManager *manager;
    if (manager)
        return g_object_ref(manager);

    manager = g_object_new(CSD_TYPE_DEVICE_CONTROL_MANAGER, NULL);
    g_object_add_weak_pointer(G_OBJECT(manager), (gpointer *) &manager);
    return manager;
}
