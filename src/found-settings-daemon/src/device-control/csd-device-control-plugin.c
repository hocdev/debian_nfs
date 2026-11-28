#include "cdos-settings-plugin.h"
#include "csd-device-control-manager.h"

typedef struct {
    CdosSettingsPlugin parent;
    CsdDeviceControlManager *manager;
} CsdDeviceControlPlugin;

typedef CdosSettingsPluginClass CsdDeviceControlPluginClass;

CDOS_SETTINGS_PLUGIN_REGISTER(CsdDeviceControlPlugin, csd_device_control_plugin)

static void
csd_device_control_plugin_init(CsdDeviceControlPlugin *plugin)
{
    plugin->manager = csd_device_control_manager_new();
}

static void
activate(CdosSettingsPlugin *base)
{
    CsdDeviceControlPlugin *plugin = (CsdDeviceControlPlugin *) base;
    g_autoptr(GError) error = NULL;
    if (!csd_device_control_manager_start(plugin->manager, &error))
        g_warning("Unable to start device-control manager: %s", error->message);
}

static void
deactivate(CdosSettingsPlugin *base)
{
    CsdDeviceControlPlugin *plugin = (CsdDeviceControlPlugin *) base;
    csd_device_control_manager_stop(plugin->manager);
}

static void
csd_device_control_plugin_finalize(GObject *object)
{
    CsdDeviceControlPlugin *plugin = (CsdDeviceControlPlugin *) object;
    g_clear_object(&plugin->manager);
    G_OBJECT_CLASS(csd_device_control_plugin_parent_class)->finalize(object);
}

static void
csd_device_control_plugin_class_init(CsdDeviceControlPluginClass *class)
{
    G_OBJECT_CLASS(class)->finalize = csd_device_control_plugin_finalize;
    class->activate = activate;
    class->deactivate = deactivate;
}
