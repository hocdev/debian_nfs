#ifndef VOS_INSTALLER_H
#define VOS_INSTALLER_H

#include <glib.h>
#include <gtk/gtk.h>

#define ASSETS_DIR "/usr/share/vos-installer/assets"
#define SQUASHFS_PATH "/run/live/medium/live/filesystem.squashfs"
#define TARGET_MOUNT "/tmp/vos-target"

typedef struct {
    char *device;       /* e.g. /dev/sda or /dev/nvme0n1 */
    char *model;
    guint64 size_bytes;
} DiskInfo;

typedef struct {
    char *disk_device;
    char *full_name;
    char *username;
    char *password;
    char *hostname;
    char *timezone;
    gboolean create_swapfile;
} InstallConfig;

typedef void (*ProgressCallback)(double fraction, const char *status, const char *log_line, gpointer user_data);

GList *vos_installer_list_disks(void);
void vos_installer_free_disks(GList *disks);
gboolean vos_installer_is_uefi(void);
guint64 vos_installer_get_ram_mb(void);

gboolean vos_installer_run(const InstallConfig *cfg,
                           ProgressCallback cb,
                           gpointer cb_data,
                           char **error_out);

#endif /* VOS_INSTALLER_H */
