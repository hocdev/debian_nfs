#ifndef CSD_DEVICE_CONTROL_MANAGER_H
#define CSD_DEVICE_CONTROL_MANAGER_H

#include <gio/gio.h>

#define CSD_TYPE_DEVICE_CONTROL_MANAGER (csd_device_control_manager_get_type())
G_DECLARE_FINAL_TYPE(CsdDeviceControlManager, csd_device_control_manager, CSD, DEVICE_CONTROL_MANAGER, GObject)

CsdDeviceControlManager *csd_device_control_manager_new(void);
gboolean csd_device_control_manager_start(CsdDeviceControlManager *manager, GError **error);
void csd_device_control_manager_stop(CsdDeviceControlManager *manager);

#endif
