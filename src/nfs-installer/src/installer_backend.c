#define _GNU_SOURCE
#include "installer.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <sys/wait.h>
#include <unistd.h>

static gboolean run_cmd(const char *cmd, ProgressCallback cb, gpointer cb_data, double frac, const char *status) {
    if (cb) cb(frac, status, cmd, cb_data);

    char *argv[] = { "/bin/sh", "-c", (char *)cmd, NULL };
    char *out = NULL;
    char *err = NULL;
    int exit_status = 0;
    GError *gerr = NULL;

    gboolean ok = g_spawn_sync(NULL, argv, NULL, G_SPAWN_SEARCH_PATH,
                               NULL, NULL, &out, &err, &exit_status, &gerr);
    if (!ok) {
        if (cb && gerr) cb(frac, status, gerr->message, cb_data);
        if (gerr) g_error_free(gerr);
        g_free(out);
        g_free(err);
        return FALSE;
    }
    if (out && *out && cb) cb(frac, status, out, cb_data);
    if (err && *err && cb) cb(frac, status, err, cb_data);
    g_free(out);
    g_free(err);
    return WIFEXITED(exit_status) && WEXITSTATUS(exit_status) == 0;
}

static char *part_path(const char *disk, int num) {
    size_t len = strlen(disk);
    if (len > 0 && isdigit((unsigned char)disk[len - 1]))
        return g_strdup_printf("%sp%d", disk, num);
    return g_strdup_printf("%s%d", disk, num);
}

static char *read_cmd_line(const char *cmd) {
    char *out = NULL;
    int status = 0;
    char *argv[] = { "/bin/sh", "-c", (char *)cmd, NULL };
    if (!g_spawn_sync(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, &out, NULL, &status, NULL))
        return NULL;
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        g_free(out);
        return NULL;
    }
    g_strstrip(out);
    return out;
}

gboolean nfs_installer_is_uefi(void) {
    return g_file_test("/sys/firmware/efi", G_FILE_TEST_IS_DIR);
}

guint64 nfs_installer_get_ram_mb(void) {
    struct sysinfo si;
    if (sysinfo(&si) != 0)
        return 0;
    return ((guint64)si.totalram * si.mem_unit) / (1024ULL * 1024ULL);
}

GList *nfs_installer_list_disks(void) {
    char *out = read_cmd_line("lsblk -dn -b -P -o NAME,SIZE,MODEL,TYPE 2>/dev/null");
    if (!out)
        return NULL;

    GList *list = NULL;
    char **lines = g_strsplit(out, "\n", -1);
    for (int i = 0; lines[i] != NULL; i++) {
        char *line = lines[i];
        if (!strstr(line, "TYPE=\"disk\""))
            continue;

        char name[128] = {0};
        char model[256] = {0};
        guint64 size = 0;

        char *p = strstr(line, "NAME=\"");
        if (p) sscanf(p + 6, "%127[^\"]", name);
        if (strncmp(name, "loop", 4) == 0 || strncmp(name, "zram", 4) == 0 || strncmp(name, "sr", 2) == 0)
            continue;

        p = strstr(line, "SIZE=\"");
        if (p) size = g_ascii_strtoull(p + 6, NULL, 10);
        if (size < 4ULL * 1024 * 1024 * 1024)
            continue;

        p = strstr(line, "MODEL=\"");
        if (p) sscanf(p + 7, "%255[^\"]", model);
        g_strstrip(model);

        DiskInfo *d = g_new0(DiskInfo, 1);
        d->device = g_strdup_printf("/dev/%s", name);
        d->model = g_strdup(model[0] ? model : "Disk");
        d->size_bytes = size;
        list = g_list_append(list, d);
    }
    g_strfreev(lines);
    g_free(out);
    return list;
}

void nfs_installer_free_disks(GList *disks) {
    for (GList *l = disks; l != NULL; l = l->next) {
        DiskInfo *d = (DiskInfo *)l->data;
        g_free(d->device);
        g_free(d->model);
        g_free(d);
    }
    g_list_free(disks);
}

static void unmount_target_all(void) {
    const char *mounts[] = {
        TARGET_MOUNT "/sys/firmware/efi/efivars",
        TARGET_MOUNT "/run",
        TARGET_MOUNT "/sys",
        TARGET_MOUNT "/proc",
        TARGET_MOUNT "/dev/pts",
        TARGET_MOUNT "/dev",
        TARGET_MOUNT "/boot/efi",
        TARGET_MOUNT
    };
    for (size_t i = 0; i < G_N_ELEMENTS(mounts); i++) {
        char *cmd = g_strdup_printf("umount -lf '%s' 2>/dev/null || true", mounts[i]);
        run_cmd(cmd, NULL, NULL, 0.0, NULL);
        g_free(cmd);
    }
}

static gboolean unpack_squashfs(ProgressCallback cb, gpointer cb_data) {
    if (cb) cb(0.20, "Unpacking system image (filesystem.squashfs)...", "Running unsquashfs...", cb_data);

    char *argv[] = {
        "unsquashfs", "-f", "-n", "-d", TARGET_MOUNT, SQUASHFS_PATH, NULL
    };
    int stdout_fd = -1;
    GPid pid = 0;
    GError *err = NULL;

    if (!g_spawn_async_with_pipes(NULL, argv, NULL,
                                  G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD,
                                  NULL, NULL, &pid, NULL, &stdout_fd, NULL, &err)) {
        if (cb && err) cb(0.20, "Failed to launch unsquashfs", err->message, cb_data);
        if (err) g_error_free(err);
        return FALSE;
    }

    FILE *fp = fdopen(stdout_fd, "r");
    char buf[512];
    double last_frac = 0.20;
    if (fp) {
        while (fgets(buf, sizeof(buf), fp) != NULL) {
            /* unsquashfs prints progress percentage when -n is omitted or summary */
            char *pct = strrchr(buf, '%');
            if (pct && pct > buf) {
                char *s = pct - 1;
                while (s > buf && (isdigit((unsigned char)*s) || *s == ' '))
                    s--;
                int val = atoi(s);
                if (val >= 0 && val <= 100) {
                    last_frac = 0.20 + 0.50 * (val / 100.0);
                    if (cb) cb(last_frac, "Unpacking system image...", NULL, cb_data);
                }
            }
        }
        fclose(fp);
    }

    int status = 0;
    waitpid(pid, &status, 0);
    g_spawn_close_pid(pid);
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

gboolean nfs_installer_run(const InstallConfig *cfg,
                           ProgressCallback cb,
                           gpointer cb_data,
                           char **error_out) {
    gboolean uefi = nfs_installer_is_uefi();
    char *p_efi = NULL;
    char *p_root = NULL;
    gboolean ok = FALSE;

    if (!g_file_test(SQUASHFS_PATH, G_FILE_TEST_EXISTS)) {
        *error_out = g_strdup("Live image not found at " SQUASHFS_PATH);
        return FALSE;
    }

    unmount_target_all();

    /* 1. Unmount existing partitions on target disk */
    char *cmd = g_strdup_printf(
        "for p in $(lsblk -ln -o NAME '%s' | tail -n +2); do umount -lf \"/dev/$p\" 2>/dev/null || true; swapoff \"/dev/$p\" 2>/dev/null || true; done",
        cfg->disk_device);
    run_cmd(cmd, cb, cb_data, 0.05, "Preparing target disk...");
    g_free(cmd);

    /* 2. Partition & format */
    if (uefi) {
        p_efi = part_path(cfg->disk_device, 1);
        p_root = part_path(cfg->disk_device, 2);
        cmd = g_strdup_printf(
            "parted -s '%s' mklabel gpt mkpart ESP fat32 1MiB 513MiB set 1 esp on mkpart primary ext4 513MiB 100%% && partprobe '%s' && sleep 1",
            cfg->disk_device, cfg->disk_device);
    } else {
        p_root = part_path(cfg->disk_device, 1);
        cmd = g_strdup_printf(
            "parted -s '%s' mklabel msdos mkpart primary ext4 1MiB 100%% set 1 boot on && partprobe '%s' && sleep 1",
            cfg->disk_device, cfg->disk_device);
    }
    if (!run_cmd(cmd, cb, cb_data, 0.10, "Partitioning disk...")) {
        g_free(cmd);
        *error_out = g_strdup("Failed to partition target disk.");
        goto out;
    }
    g_free(cmd);

    if (uefi) {
        cmd = g_strdup_printf("mkfs.vfat -F32 -n EFI '%s'", p_efi);
        if (!run_cmd(cmd, cb, cb_data, 0.14, "Formatting EFI System Partition...")) {
            g_free(cmd);
            *error_out = g_strdup("Failed to format EFI partition.");
            goto out;
        }
        g_free(cmd);
    }

    cmd = g_strdup_printf("mkfs.ext4 -F -L NFS_OS '%s'", p_root);
    if (!run_cmd(cmd, cb, cb_data, 0.18, "Formatting root (ext4) partition...")) {
        g_free(cmd);
        *error_out = g_strdup("Failed to format root partition.");
        goto out;
    }
    g_free(cmd);

    /* 3. Mount target */
    g_mkdir_with_parents(TARGET_MOUNT, 0755);
    cmd = g_strdup_printf("mount '%s' '%s'", p_root, TARGET_MOUNT);
    if (!run_cmd(cmd, cb, cb_data, 0.19, "Mounting target filesystem...")) {
        g_free(cmd);
        *error_out = g_strdup("Failed to mount root partition.");
        goto out;
    }
    g_free(cmd);

    if (uefi) {
        g_mkdir_with_parents(TARGET_MOUNT "/boot/efi", 0755);
        cmd = g_strdup_printf("mount '%s' '%s/boot/efi'", p_efi, TARGET_MOUNT);
        if (!run_cmd(cmd, cb, cb_data, 0.20, "Mounting EFI partition...")) {
            g_free(cmd);
            *error_out = g_strdup("Failed to mount EFI partition.");
            goto out;
        }
        g_free(cmd);
    }

    /* 4. Unpack squashfs */
    if (!unpack_squashfs(cb, cb_data)) {
        *error_out = g_strdup("Failed to unpack system image.");
        goto out;
    }

    /* 5. Bind mounts for chroot */
    const char *bind_cmd =
        "mkdir -p " TARGET_MOUNT "/dev " TARGET_MOUNT "/dev/pts " TARGET_MOUNT "/proc " TARGET_MOUNT "/sys " TARGET_MOUNT "/run && "
        "mount --bind /dev " TARGET_MOUNT "/dev && "
        "mount --bind /dev/pts " TARGET_MOUNT "/dev/pts && "
        "mount -t proc proc " TARGET_MOUNT "/proc && "
        "mount -t sysfs sysfs " TARGET_MOUNT "/sys && "
        "mount --bind /run " TARGET_MOUNT "/run";
    if (!run_cmd(bind_cmd, cb, cb_data, 0.72, "Preparing chroot environment...")) {
        *error_out = g_strdup("Failed to bind-mount system directories.");
        goto out;
    }
    if (uefi && g_file_test("/sys/firmware/efi/efivars", G_FILE_TEST_IS_DIR)) {
        run_cmd("mount --bind /sys/firmware/efi/efivars " TARGET_MOUNT "/sys/firmware/efi/efivars 2>/dev/null || true",
                cb, cb_data, 0.72, "Binding efivars...");
    }

    /* 6. Write /etc/fstab */
    cmd = g_strdup_printf("blkid -s UUID -o value '%s'", p_root);
    char *root_uuid = read_cmd_line(cmd);
    g_free(cmd);

    char *efi_uuid = NULL;
    if (uefi) {
        cmd = g_strdup_printf("blkid -s UUID -o value '%s'", p_efi);
        efi_uuid = read_cmd_line(cmd);
        g_free(cmd);
    }

    GString *fstab = g_string_new("# /etc/fstab: static file system information.\n");
    if (root_uuid && *root_uuid)
        g_string_append_printf(fstab, "UUID=%s\t/\text4\terrors=remount-ro\t0\t1\n", root_uuid);
    else
        g_string_append_printf(fstab, "%s\t/\text4\terrors=remount-ro\t0\t1\n", p_root);

    if (uefi && efi_uuid && *efi_uuid)
        g_string_append_printf(fstab, "UUID=%s\t/boot/efi\tvfat\tumask=0077\t0\t1\n", efi_uuid);

    if (cfg->create_swapfile) {
        if (run_cmd("fallocate -l 2G " TARGET_MOUNT "/swapfile 2>/dev/null || dd if=/dev/zero of=" TARGET_MOUNT "/swapfile bs=1M count=2048 status=none",
                    cb, cb_data, 0.75, "Creating 2 GB swapfile...") &&
            run_cmd("chmod 600 " TARGET_MOUNT "/swapfile && mkswap " TARGET_MOUNT "/swapfile",
                    cb, cb_data, 0.76, "Formatting swapfile...")) {
            g_string_append(fstab, "/swapfile\tnone\tswap\tsw\t0\t0\n");
        }
    }
    g_file_set_contents(TARGET_MOUNT "/etc/fstab", fstab->str, -1, NULL);
    g_string_free(fstab, TRUE);
    g_free(root_uuid);
    g_free(efi_uuid);

    /* 7. Hostname, hosts, timezone, machine-id */
    g_file_set_contents(TARGET_MOUNT "/etc/hostname", cfg->hostname, -1, NULL);
    char *hosts = g_strdup_printf(
        "127.0.0.1\tlocalhost\n127.0.1.1\t%s\n\n::1\tlocalhost ip6-localhost ip6-loopback\n",
        cfg->hostname);
    g_file_set_contents(TARGET_MOUNT "/etc/hosts", hosts, -1, NULL);
    g_free(hosts);

    if (cfg->timezone && *cfg->timezone) {
        g_file_set_contents(TARGET_MOUNT "/etc/timezone", cfg->timezone, -1, NULL);
        cmd = g_strdup_printf("chroot " TARGET_MOUNT " ln -sf '/usr/share/zoneinfo/%s' /etc/localtime", cfg->timezone);
        run_cmd(cmd, cb, cb_data, 0.78, "Configuring timezone...");
        g_free(cmd);
    }

    run_cmd("rm -f " TARGET_MOUNT "/etc/machine-id && chroot " TARGET_MOUNT " systemd-machine-id-setup",
            cb, cb_data, 0.80, "Generating machine-id...");

    /* 8. User creation & live cleanup */
    char *q_user = g_shell_quote(cfg->username);
    char *q_full = g_shell_quote(cfg->full_name);
    char *q_pass = g_shell_quote(cfg->password);
    cmd = g_strdup_printf(
        "chroot " TARGET_MOUNT " useradd -m -s /bin/bash -c %s -G sudo,adm,cdrom,dip,plugdev,netdev %s && "
        "printf '%%s:%%s\\n' %s %s | chroot " TARGET_MOUNT " chpasswd",
        q_full, q_user, q_user, q_pass);
    if (!run_cmd(cmd, cb, cb_data, 0.84, "Creating user account...")) {
        g_free(cmd);
        g_free(q_user);
        g_free(q_full);
        g_free(q_pass);
        *error_out = g_strdup("Failed to create user account.");
        goto out;
    }
    g_free(cmd);
    g_free(q_user);
    g_free(q_full);
    g_free(q_pass);

    run_cmd("chroot " TARGET_MOUNT " sh -c 'DEBIAN_FRONTEND=noninteractive apt-get purge -y live-boot live-config live-config-systemd 2>/dev/null || true; "
            "rm -f /etc/lightdm/lightdm.conf.d/10-nfs-live.conf /etc/skel/Desktop/nfs-install.desktop /home/*/Desktop/nfs-install.desktop; "
            "userdel -r user 2>/dev/null || true'",
            cb, cb_data, 0.87, "Removing live session configuration...");

    /* 9. Install GRUB2 bootloader */
    if (uefi) {
        cmd = g_strdup(
            "chroot " TARGET_MOUNT " grub-install --target=x86_64-efi --efi-directory=/boot/efi --bootloader-id=NFS --recheck --removable && "
            "chroot " TARGET_MOUNT " grub-mkconfig -o /boot/grub/grub.cfg");
    } else {
        cmd = g_strdup_printf(
            "chroot " TARGET_MOUNT " grub-install --target=i386-pc --recheck '%s' && "
            "chroot " TARGET_MOUNT " grub-mkconfig -o /boot/grub/grub.cfg",
            cfg->disk_device);
    }
    if (!run_cmd(cmd, cb, cb_data, 0.93, "Installing GRUB2 bootloader...")) {
        g_free(cmd);
        *error_out = g_strdup("Failed to install GRUB2 bootloader.");
        goto out;
    }
    g_free(cmd);

    run_cmd("chroot " TARGET_MOUNT " update-initramfs -u -k all",
            cb, cb_data, 0.98, "Updating initramfs...");

    ok = TRUE;
    if (cb) cb(1.0, "Installation completed successfully.", "Done.", cb_data);

out:
    unmount_target_all();
    g_free(p_efi);
    g_free(p_root);
    return ok;
}
