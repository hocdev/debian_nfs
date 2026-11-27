#include "installer.h"
#define INSTALLER_ICON "/usr/share/icons/hicolor/scalable/apps/vos-installer.svg"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct {
    GtkWidget *window;
    GtkWidget *stack;
    GtkWidget *step_icons[4];
    GtkWidget *step_labels[4];

    /* Page 1: Welcome */
    GtkWidget *welcome_checks;
    GtkWidget *btn_welcome_next;

    /* Page 2: Disk & User */
    GtkWidget *combo_disk;
    GtkWidget *lbl_boot_mode;
    GtkWidget *entry_fullname;
    GtkWidget *entry_username;
    GtkWidget *entry_password;
    GtkWidget *entry_confirm;
    GtkWidget *entry_hostname;
    GtkWidget *combo_timezone;
    GtkWidget *chk_swapfile;
    GtkWidget *lbl_form_error;
    GList *disks;

    /* Page 3: Progress */
    GtkWidget *progress_bar;
    GtkWidget *lbl_progress_status;
    GtkTextBuffer *log_buffer;

    /* Page 4: Done */
    GtkWidget *lbl_done_title;
    GtkWidget *lbl_done_desc;
} AppState;

typedef struct {
    AppState *app;
    double fraction;
    char *status;
    char *log_line;
} ProgressMsg;

typedef struct {
    AppState *app;
    InstallConfig *cfg;
    gboolean success;
    char *error_msg;
} DoneMsg;

static void set_active_step(AppState *app, int step_idx) {
    for (int i = 0; i < 4; i++) {
        GtkStyleContext *ctx = gtk_widget_get_style_context(app->step_labels[i]);
        if (i == step_idx)
            gtk_style_context_add_class(ctx, "step-active");
        else
            gtk_style_context_remove_class(ctx, "step-active");
    }
}

static gboolean on_idle_progress(gpointer data) {
    ProgressMsg *m = (ProgressMsg *)data;
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(m->app->progress_bar), m->fraction);
    if (m->status)
        gtk_label_set_text(GTK_LABEL(m->app->lbl_progress_status), m->status);
    if (m->log_line && *m->log_line) {
        GtkTextIter end;
        gtk_text_buffer_get_end_iter(m->app->log_buffer, &end);
        gtk_text_buffer_insert(m->app->log_buffer, &end, m->log_line, -1);
        gtk_text_buffer_insert(m->app->log_buffer, &end, "\n", 1);
    }
    g_free(m->status);
    g_free(m->log_line);
    g_free(m);
    return G_SOURCE_REMOVE;
}

static void backend_progress_cb(double fraction, const char *status, const char *log_line, gpointer user_data) {
    ProgressMsg *m = g_new0(ProgressMsg, 1);
    m->app = (AppState *)user_data;
    m->fraction = fraction;
    m->status = g_strdup(status);
    m->log_line = g_strdup(log_line);
    g_idle_add(on_idle_progress, m);
}

static gboolean on_idle_done(gpointer data) {
    DoneMsg *d = (DoneMsg *)data;
    if (d->success) {
        gtk_label_set_text(GTK_LABEL(d->app->lbl_done_title), "Installation Complete");
        gtk_label_set_text(GTK_LABEL(d->app->lbl_done_desc),
                           "VOS Desktop has been installed on your computer.\nYou can restart now or continue testing the live environment.");
    } else {
        gtk_label_set_text(GTK_LABEL(d->app->lbl_done_title), "Installation Failed");
        gtk_label_set_text(GTK_LABEL(d->app->lbl_done_desc),
                           d->error_msg ? d->error_msg : "An unknown error occurred.");
    }
    set_active_step(d->app, 3);
    gtk_stack_set_visible_child_name(GTK_STACK(d->app->stack), "done");

    g_free(d->cfg->disk_device);
    g_free(d->cfg->full_name);
    g_free(d->cfg->username);
    g_free(d->cfg->password);
    g_free(d->cfg->hostname);
    g_free(d->cfg->timezone);
    g_free(d->cfg);
    g_free(d->error_msg);
    g_free(d);
    return G_SOURCE_REMOVE;
}

static gpointer install_thread_fn(gpointer data) {
    DoneMsg *d = (DoneMsg *)data;
    d->success = vos_installer_run(d->cfg, backend_progress_cb, d->app, &d->error_msg);
    g_idle_add(on_idle_done, d);
    return NULL;
}

static void on_welcome_next(GtkButton *btn, gpointer user_data) {
    (void)btn;
    AppState *app = (AppState *)user_data;
    set_active_step(app, 1);
    gtk_stack_set_visible_child_name(GTK_STACK(app->stack), "setup");
}

static void on_setup_back(GtkButton *btn, gpointer user_data) {
    (void)btn;
    AppState *app = (AppState *)user_data;
    set_active_step(app, 0);
    gtk_stack_set_visible_child_name(GTK_STACK(app->stack), "welcome");
}

static void on_setup_install(GtkButton *btn, gpointer user_data) {
    (void)btn;
    AppState *app = (AppState *)user_data;

    int disk_idx = gtk_combo_box_get_active(GTK_COMBO_BOX(app->combo_disk));
    if (disk_idx < 0) {
        gtk_label_set_text(GTK_LABEL(app->lbl_form_error), "Please select a target disk.");
        return;
    }

    const char *username = gtk_entry_get_text(GTK_ENTRY(app->entry_username));
    const char *pass1 = gtk_entry_get_text(GTK_ENTRY(app->entry_password));
    const char *pass2 = gtk_entry_get_text(GTK_ENTRY(app->entry_confirm));
    const char *hostname = gtk_entry_get_text(GTK_ENTRY(app->entry_hostname));

    if (!username || !*username || strchr(username, ' ')) {
        gtk_label_set_text(GTK_LABEL(app->lbl_form_error), "Username must be non-empty without spaces.");
        return;
    }
    if (!pass1 || !*pass1 || strcmp(pass1, pass2) != 0) {
        gtk_label_set_text(GTK_LABEL(app->lbl_form_error), "Passwords do not match or are empty.");
        return;
    }
    if (!hostname || !*hostname) {
        gtk_label_set_text(GTK_LABEL(app->lbl_form_error), "Hostname cannot be empty.");
        return;
    }

    DiskInfo *disk = (DiskInfo *)g_list_nth_data(app->disks, (guint)disk_idx);
    GtkWidget *dlg = gtk_message_dialog_new(
        GTK_WINDOW(app->window), GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING, GTK_BUTTONS_OK_CANCEL,
        "Erase all data on %s (%s)?", disk->device, disk->model);
    gtk_message_dialog_format_secondary_text(
        GTK_MESSAGE_DIALOG(dlg),
        "All existing partitions and files on %s will be permanently destroyed.", disk->device);
    int resp = gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg);
    if (resp != GTK_RESPONSE_OK)
        return;

    gtk_label_set_text(GTK_LABEL(app->lbl_form_error), "");

    InstallConfig *cfg = g_new0(InstallConfig, 1);
    cfg->disk_device = g_strdup(disk->device);
    cfg->full_name = g_strdup(gtk_entry_get_text(GTK_ENTRY(app->entry_fullname)));
    cfg->username = g_strdup(username);
    cfg->password = g_strdup(pass1);
    cfg->hostname = g_strdup(hostname);
    cfg->timezone = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(app->combo_timezone));
    cfg->create_swapfile = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->chk_swapfile));

    DoneMsg *d = g_new0(DoneMsg, 1);
    d->app = app;
    d->cfg = cfg;

    set_active_step(app, 2);
    gtk_stack_set_visible_child_name(GTK_STACK(app->stack), "install");
    g_thread_new("vos-install-worker", install_thread_fn, d);
}

static void on_reboot_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    (void)user_data;
    g_spawn_command_line_async("systemctl reboot", NULL);
    gtk_main_quit();
}

static void on_close_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    (void)user_data;
    gtk_main_quit();
}

static GtkWidget *load_scaled_image(const char *filename, int w, int h) {
    char *path = g_build_filename(ASSETS_DIR, filename, NULL);
    GdkPixbuf *pb = gdk_pixbuf_new_from_file_at_scale(path, w, h, TRUE, NULL);
    g_free(path);
    if (pb) {
        GtkWidget *img = gtk_image_new_from_pixbuf(pb);
        g_object_unref(pb);
        return img;
    }
    return gtk_image_new();
}

static GtkWidget *build_sidebar(AppState *app) {
    GtkWidget *side = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_size_request(side, 220, -1);
    gtk_style_context_add_class(gtk_widget_get_style_context(side), "vos-sidebar");
    gtk_container_set_border_width(GTK_CONTAINER(side), 18);

    GtkWidget *logo = gtk_image_new_from_icon_name("vos-installer", GTK_ICON_SIZE_DIALOG);
    gtk_image_set_pixel_size(GTK_IMAGE(logo), 64);
    gtk_box_pack_start(GTK_BOX(side), logo, FALSE, FALSE, 4);

    GtkWidget *title = gtk_label_new("VOS Desktop");
    gtk_style_context_add_class(gtk_widget_get_style_context(title), "sidebar-title");
    gtk_box_pack_start(GTK_BOX(side), title, FALSE, FALSE, 8);

    const char *names[4] = { "1. Welcome", "2. Disk & Account", "3. Installing", "4. Finish" };
    const char *icons[4] = { "Step1.png", "Step2.png", "Step3.png", "Step4.png" };
    for (int i = 0; i < 4; i++) {
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
        app->step_icons[i] = load_scaled_image(icons[i], 22, 22);
        app->step_labels[i] = gtk_label_new(names[i]);
        gtk_widget_set_halign(app->step_labels[i], GTK_ALIGN_START);
        gtk_style_context_add_class(gtk_widget_get_style_context(app->step_labels[i]), "step-label");
        gtk_box_pack_start(GTK_BOX(row), app->step_icons[i], FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(row), app->step_labels[i], TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(side), row, FALSE, FALSE, 4);
    }
    return side;
}

static GtkWidget *build_welcome_page(AppState *app) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_container_set_border_width(GTK_CONTAINER(box), 24);

    GtkWidget *hdr = gtk_label_new("Welcome to VOS Desktop Installer");
    gtk_style_context_add_class(gtk_widget_get_style_context(hdr), "page-title");
    gtk_widget_set_halign(hdr, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(box), hdr, FALSE, FALSE, 0);

    GtkWidget *banner = load_scaled_image("welcome.png", 460, 200);
    gtk_box_pack_start(GTK_BOX(box), banner, FALSE, FALSE, 4);

    guint64 ram_mb = vos_installer_get_ram_mb();
    gboolean is_root = (geteuid() == 0);
    gboolean has_squashfs = g_file_test(SQUASHFS_PATH, G_FILE_TEST_EXISTS);
    gboolean has_disk = (app->disks != NULL);

    char *checks = g_strdup_printf(
        "System Readiness Checks:\n"
        "  • RAM: %llu MB %s\n"
        "  • Privileges: %s\n"
        "  • Live Image: %s\n"
        "  • Target Disks: %s\n"
        "  • Boot Firmware: %s",
        (unsigned long long)ram_mb, ram_mb >= 900 ? "(OK)" : "(Low RAM)",
        is_root ? "Administrator (root)" : "Not root (run via pkexec/sudo)",
        has_squashfs ? "Found" : "Missing (" SQUASHFS_PATH ")",
        has_disk ? "Available" : "No suitable disk found",
        vos_installer_is_uefi() ? "UEFI (GPT)" : "BIOS / Legacy (MBR)");
    app->welcome_checks = gtk_label_new(checks);
    g_free(checks);
    gtk_widget_set_halign(app->welcome_checks, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(box), app->welcome_checks, TRUE, TRUE, 0);

    GtkWidget *btns = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    app->btn_welcome_next = gtk_button_new_with_label("Next");
    gtk_style_context_add_class(gtk_widget_get_style_context(app->btn_welcome_next), "suggested-action");
    g_signal_connect(app->btn_welcome_next, "clicked", G_CALLBACK(on_welcome_next), app);
    gtk_box_pack_end(GTK_BOX(btns), app->btn_welcome_next, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(box), btns, FALSE, FALSE, 0);
    return box;
}

static GtkWidget *build_setup_page(AppState *app) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_container_set_border_width(GTK_CONTAINER(box), 24);

    GtkWidget *hdr = gtk_label_new("Disk Partitioning & User Account");
    gtk_style_context_add_class(gtk_widget_get_style_context(hdr), "page-title");
    gtk_widget_set_halign(hdr, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(box), hdr, FALSE, FALSE, 0);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);

    app->combo_disk = gtk_combo_box_text_new();
    for (GList *l = app->disks; l != NULL; l = l->next) {
        DiskInfo *d = (DiskInfo *)l->data;
        char *txt = g_strdup_printf("%s — %s (%.1f GB)",
                                    d->device, d->model, d->size_bytes / (1024.0 * 1024.0 * 1024.0));
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->combo_disk), txt);
        g_free(txt);
    }
    if (app->disks)
        gtk_combo_box_set_active(GTK_COMBO_BOX(app->combo_disk), 0);

    app->lbl_boot_mode = gtk_label_new(vos_installer_is_uefi()
                                       ? "Mode: UEFI (512 MB FAT32 ESP + ext4 Root)"
                                       : "Mode: BIOS/MBR (ext4 Root)");
    gtk_widget_set_halign(app->lbl_boot_mode, GTK_ALIGN_START);

    app->entry_fullname = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(app->entry_fullname), "VOS User");
    app->entry_username = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(app->entry_username), "vos");
    app->entry_password = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(app->entry_password), FALSE);
    gtk_entry_set_text(GTK_ENTRY(app->entry_password), "123456");
    app->entry_confirm = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(app->entry_confirm), FALSE);
    gtk_entry_set_text(GTK_ENTRY(app->entry_confirm), "123456");
    app->entry_hostname = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(app->entry_hostname), "vos-desktop");

    app->combo_timezone = gtk_combo_box_text_new();
    const char *tzs[] = { "Asia/Ho_Chi_Minh", "UTC", "Asia/Shanghai", "Asia/Tokyo", "America/New_York", "Europe/London" };
    for (size_t i = 0; i < G_N_ELEMENTS(tzs); i++)
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->combo_timezone), tzs[i]);
    gtk_combo_box_set_active(GTK_COMBO_BOX(app->combo_timezone), 0);

    app->chk_swapfile = gtk_check_button_new_with_label("Create 2 GB /swapfile");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->chk_swapfile), TRUE);

    const char *labels[] = {
        "Target Disk:", "Partition Layout:", "Full Name:", "Username:",
        "Password:", "Confirm Password:", "Hostname:", "Timezone:"
    };
    GtkWidget *widgets[] = {
        app->combo_disk, app->lbl_boot_mode, app->entry_fullname, app->entry_username,
        app->entry_password, app->entry_confirm, app->entry_hostname, app->combo_timezone
    };
    for (int i = 0; i < 8; i++) {
        GtkWidget *lbl = gtk_label_new(labels[i]);
        gtk_widget_set_halign(lbl, GTK_ALIGN_END);
        gtk_widget_set_hexpand(widgets[i], TRUE);
        gtk_grid_attach(GTK_GRID(grid), lbl, 0, i, 1, 1);
        gtk_grid_attach(GTK_GRID(grid), widgets[i], 1, i, 1, 1);
    }
    gtk_grid_attach(GTK_GRID(grid), app->chk_swapfile, 1, 8, 1, 1);
    gtk_box_pack_start(GTK_BOX(box), grid, TRUE, TRUE, 0);

    app->lbl_form_error = gtk_label_new("");
    gtk_style_context_add_class(gtk_widget_get_style_context(app->lbl_form_error), "error-label");
    gtk_box_pack_start(GTK_BOX(box), app->lbl_form_error, FALSE, FALSE, 0);

    GtkWidget *btns = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *btn_back = gtk_button_new_with_label("Back");
    GtkWidget *btn_inst = gtk_button_new_with_label("Install Now");
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_inst), "destructive-action");
    g_signal_connect(btn_back, "clicked", G_CALLBACK(on_setup_back), app);
    g_signal_connect(btn_inst, "clicked", G_CALLBACK(on_setup_install), app);
    gtk_box_pack_start(GTK_BOX(btns), btn_back, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(btns), btn_inst, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(box), btns, FALSE, FALSE, 0);
    return box;
}

static GtkWidget *build_install_page(AppState *app) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_container_set_border_width(GTK_CONTAINER(box), 24);

    GtkWidget *hdr = gtk_label_new("Installing VOS Desktop...");
    gtk_style_context_add_class(gtk_widget_get_style_context(hdr), "page-title");
    gtk_widget_set_halign(hdr, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(box), hdr, FALSE, FALSE, 0);

    GtkWidget *slide = load_scaled_image("slide1.png", 440, 210);
    gtk_box_pack_start(GTK_BOX(box), slide, FALSE, FALSE, 4);

    app->progress_bar = gtk_progress_bar_new();
    gtk_box_pack_start(GTK_BOX(box), app->progress_bar, FALSE, FALSE, 0);

    app->lbl_progress_status = gtk_label_new("Preparing...");
    gtk_widget_set_halign(app->lbl_progress_status, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(box), app->lbl_progress_status, FALSE, FALSE, 0);

    GtkWidget *sw = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_size_request(sw, -1, 130);
    GtkWidget *tv = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(tv), FALSE);
    app->log_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(tv));
    gtk_container_add(GTK_CONTAINER(sw), tv);
    gtk_box_pack_start(GTK_BOX(box), sw, TRUE, TRUE, 0);
    return box;
}

static GtkWidget *build_done_page(AppState *app) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_container_set_border_width(GTK_CONTAINER(box), 24);

    GtkWidget *img = load_scaled_image("Sucess.png", 96, 96);
    gtk_box_pack_start(GTK_BOX(box), img, FALSE, FALSE, 12);

    app->lbl_done_title = gtk_label_new("Installation Complete");
    gtk_style_context_add_class(gtk_widget_get_style_context(app->lbl_done_title), "page-title");
    gtk_box_pack_start(GTK_BOX(box), app->lbl_done_title, FALSE, FALSE, 0);

    app->lbl_done_desc = gtk_label_new("");
    gtk_label_set_justify(GTK_LABEL(app->lbl_done_desc), GTK_JUSTIFY_CENTER);
    gtk_box_pack_start(GTK_BOX(box), app->lbl_done_desc, TRUE, TRUE, 0);

    GtkWidget *btns = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *btn_close = gtk_button_new_with_label("Close");
    GtkWidget *btn_reboot = gtk_button_new_with_label("Restart Now");
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_reboot), "suggested-action");
    g_signal_connect(btn_close, "clicked", G_CALLBACK(on_close_clicked), app);
    g_signal_connect(btn_reboot, "clicked", G_CALLBACK(on_reboot_clicked), app);
    gtk_box_pack_end(GTK_BOX(btns), btn_reboot, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(btns), btn_close, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(box), btns, FALSE, FALSE, 0);
    return box;
}

int main(int argc, char **argv) {
    gtk_init(&argc, &argv);

    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_data(
        css,
        ".vos-sidebar { background-color: #2c3133; }"
        ".sidebar-title { color: #ffffff; font-size: 16px; font-weight: bold; }"
        ".step-label { color: #b0b8bc; font-size: 13px; }"
        ".step-active { color: #ffffff; font-weight: bold; }"
        ".page-title { font-size: 18px; font-weight: bold; }"
        ".error-label { color: #cc0000; font-weight: bold; }",
        -1, NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
                                              GTK_STYLE_PROVIDER(css),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    AppState app = {0};
    app.disks = vos_installer_list_disks();

    app.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(app.window), "VOS Desktop Installer");
    gtk_window_set_icon_from_file(GTK_WINDOW(app.window), INSTALLER_ICON, NULL);
    gtk_window_set_default_size(GTK_WINDOW(app.window), 860, 560);
    gtk_window_set_position(GTK_WINDOW(app.window), GTK_WIN_POS_CENTER);
    g_signal_connect(app.window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_container_add(GTK_CONTAINER(app.window), root);
    gtk_box_pack_start(GTK_BOX(root), build_sidebar(&app), FALSE, FALSE, 0);

    app.stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(app.stack), GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);
    gtk_stack_add_named(GTK_STACK(app.stack), build_welcome_page(&app), "welcome");
    gtk_stack_add_named(GTK_STACK(app.stack), build_setup_page(&app), "setup");
    gtk_stack_add_named(GTK_STACK(app.stack), build_install_page(&app), "install");
    gtk_stack_add_named(GTK_STACK(app.stack), build_done_page(&app), "done");
    gtk_box_pack_start(GTK_BOX(root), app.stack, TRUE, TRUE, 0);

    set_active_step(&app, 0);
    gtk_widget_show_all(app.window);
    gtk_main();

    vos_installer_free_disks(app.disks);
    return 0;
}
