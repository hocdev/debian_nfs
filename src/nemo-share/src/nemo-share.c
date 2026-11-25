#include "nemo-ext-compat.h"
#include "shares.h"

#include <glib/gi18n-lib.h>
#include <sys/stat.h>
#include <string.h>

typedef struct {
    GObject parent;
} NemoShare;

typedef struct {
    GObjectClass parent_class;
} NemoShareClass;

typedef struct {
    char *path;
    NemoFileInfo *fileinfo;
    GtkBuilder *xml;
    GtkWidget *main;
    GtkWidget *checkbutton_share_folder;
    GtkWidget *hbox_share_name;
    GtkWidget *hbox_share_comment;
    GtkWidget *entry_share_name;
    GtkWidget *checkbutton_share_rw_ro;
    GtkWidget *checkbutton_share_guest_ok;
    GtkWidget *entry_share_comment;
    GtkWidget *label_status;
    GtkWidget *button_cancel;
    GtkWidget *button_apply;
    GtkWidget *button_change;
    GtkWidget *standalone_window;
    gboolean was_initially_shared;
    gboolean was_writable;
    gboolean is_dirty;
} PropertyPage;

static GType nemo_share_type = 0;
static GType type_list[1];
static GList *pending_pages = NULL;
static gboolean installing_service = FALSE;
static gboolean last_shared_guest = FALSE;

static void property_page_check_sensitivity(PropertyPage *page);

static void get_share_info_for_file(NemoFileInfo *file, ShareInfo **info, gboolean *is_shareable) {
    *info = NULL;
    *is_shareable = FALSE;
    char *uri = nemo_file_info_get_uri(file);
    GFile *loc = nemo_file_info_get_location(file);
    char *path = NULL;
    if (uri != NULL) {
        if (g_str_has_prefix(uri, "network:///share-")) {
            if (!shares_get_share_info_for_share_name(uri + 17, info, NULL))
                *info = NULL;
            *is_shareable = TRUE;
        } else if (nemo_file_info_is_directory(file)) {
            path = g_file_get_path(loc);
            if (path != NULL && g_file_is_native(loc)) {
                if (shares_get_share_info_for_path(path, info, NULL))
                    *is_shareable = TRUE;
            }
        }
    }
    g_object_unref(loc);
    g_free(uri);
    g_free(path);
}

static void free_property_page(gpointer data) {
    PropertyPage *page = (PropertyPage *)data;
    pending_pages = g_list_remove(pending_pages, page);
    g_free(page->path);
    g_object_unref(page->fileinfo);
    g_object_unref(page->xml);
    g_free(page);
}

static void set_share_name_too_long(PropertyPage *page) {
    gtk_label_set_text(GTK_LABEL(page->label_status),
                       g_dgettext("nemo-share", "Share name is too long"));
    GdkColor color;
    gdk_color_parse("#ECDF62", &color);
    gtk_widget_modify_base(page->entry_share_name, GTK_STATE_NORMAL, &color);
}

static void property_page_check_sensitivity(PropertyPage *page) {
    gboolean active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->checkbutton_share_folder));
    gtk_widget_set_sensitive(page->entry_share_name, active);
    gtk_widget_set_sensitive(page->entry_share_comment, active);
    gtk_widget_set_sensitive(page->hbox_share_comment, active);
    gtk_widget_set_sensitive(page->hbox_share_name, active);
    gtk_widget_set_sensitive(page->checkbutton_share_rw_ro, active);

    gboolean apply_sens = page->was_initially_shared;
    if (!active) {
        gtk_widget_set_sensitive(page->checkbutton_share_guest_ok, FALSE);
    } else {
        gboolean guest_ok = FALSE;
        shares_supports_guest_ok(&guest_ok, NULL);
        gtk_widget_set_sensitive(page->checkbutton_share_guest_ok, guest_ok);
        apply_sens = page->is_dirty || !page->was_initially_shared;
    }
    gtk_widget_set_sensitive(page->button_apply, apply_sens);

    gtk_button_set_label(GTK_BUTTON(page->button_apply),
                         page->was_initially_shared
                             ? g_dgettext("nemo-share", "Modify _Share")
                             : g_dgettext("nemo-share", "Create _Share"));

    gtk_button_set_label(GTK_BUTTON(page->button_change),
                         g_file_test("/etc/samba/smbpasswd", G_FILE_TEST_EXISTS)
                             ? g_dgettext("nemo-share", "Change Password")
                             : g_dgettext("nemo-share", "Setting Password"));
}

static void on_show_password_toggled(GtkToggleButton *btn, gpointer user_data) {
    g_return_if_fail(btn != NULL && user_data != NULL);
    GtkEntry *entry = GTK_ENTRY(user_data);
    gtk_entry_set_visibility(entry, !gtk_entry_get_visibility(entry));
}

static void on_button_change_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    PropertyPage *page = (PropertyPage *)user_data;
    const char *title;
    const char *msg;

    if (!g_file_test("/etc/samba/smbpasswd", G_FILE_TEST_EXISTS)) {
        title = g_dgettext("nemo-share", "No Samba account available, please create account firstly before shared");
        msg = g_dgettext("nemo-share", "Not finde samba config file in you system, you can added for now.\nUse 123456 as default password, max length is 12.\nComplete and choose \"Share this Folder\" again.");
    } else {
        title = g_dgettext("nemo-share", "Change Password");
        msg = g_dgettext("nemo-share", "Use 123456 as default password, max length is 12.");
    }

    GtkWidget *dlg = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_NONE, "%s", title);
    gtk_message_dialog_format_secondary_markup(GTK_MESSAGE_DIALOG(dlg), "%s", msg);
    GtkWidget *area = gtk_message_dialog_get_message_area(GTK_MESSAGE_DIALOG(dlg));

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *lbl = gtk_label_new(g_dgettext("nemo-share", "Password:"));
    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(entry), FALSE);
    gtk_entry_set_invisible_char(GTK_ENTRY(entry), '*');
    gtk_entry_set_max_length(GTK_ENTRY(entry), 12);
    gtk_entry_set_text(GTK_ENTRY(entry), "123456");
    gtk_box_pack_start(GTK_BOX(hbox), lbl, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(area), hbox, TRUE, TRUE, 0);

    GtkWidget *chk = gtk_check_button_new_with_label(g_dgettext("nemo-share", "Show Password"));
    gtk_widget_set_halign(chk, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(area), chk, TRUE, FALSE, 0);
    g_signal_connect(chk, "toggled", G_CALLBACK(on_show_password_toggled), entry);

    gtk_dialog_add_button(GTK_DIALOG(dlg), g_dgettext("nemo-share", "Close"), GTK_RESPONSE_CANCEL);
    gtk_dialog_add_button(GTK_DIALOG(dlg), g_dgettext("nemo-share", "OK"), GTK_RESPONSE_OK);
    gtk_dialog_add_button(GTK_DIALOG(dlg), g_dgettext("nemo-share", "Clear Samba Account"), -10);
    gtk_widget_show_all(dlg);

    int resp = gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_hide(dlg);

    GError *err = NULL;
    const char *user = g_getenv("USER");
    if (resp == GTK_RESPONSE_OK) {
        char *cmd = g_strdup_printf("smb-add %s %s", user ? user : "root", gtk_entry_get_text(GTK_ENTRY(entry)));
        g_spawn_command_line_async(cmd, &err);
        g_free(cmd);
        gtk_button_set_label(GTK_BUTTON(page->button_change), g_dgettext("nemo-share", "Change Password"));
    } else if (resp == -10) {
        char *cmd = g_strdup_printf("smb-clear %s", user ? user : "root");
        g_spawn_command_line_async(cmd, &err);
        g_free(cmd);
        gtk_button_set_label(GTK_BUTTON(page->button_change), g_dgettext("nemo-share", "Setting Password"));
    }

    char *saved_label = g_strdup(gtk_button_get_label(GTK_BUTTON(page->button_change)));
    ShareInfo *info = NULL;
    if (err) { g_error_free(err); err = NULL; }
    if (!shares_get_share_info_for_path(page->path, &info, &err) || info == NULL) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(page->checkbutton_share_folder), FALSE);
    } else {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(page->checkbutton_share_folder), TRUE);
        shares_free_share_info(info);
    }
    gtk_button_set_label(GTK_BUTTON(page->button_change), saved_label);
    g_free(saved_label);
    if (err) g_error_free(err);
    gtk_widget_destroy(dlg);
}

static void on_dirty_changed(GtkWidget *w, gpointer user_data) {
    (void)w;
    PropertyPage *page = (PropertyPage *)user_data;
    page->is_dirty = TRUE;
    property_page_check_sensitivity(page);
}

static void on_share_name_changed(GtkEditable *ed, gpointer user_data) {
    (void)ed;
    PropertyPage *page = (PropertyPage *)user_data;
    page->is_dirty = TRUE;
    const char *txt = gtk_entry_get_text(GTK_ENTRY(page->entry_share_name));
    if (g_utf8_strlen(txt, -1) > 13)
        set_share_name_too_long(page);
    else {
        gtk_label_set_text(GTK_LABEL(page->label_status), "");
        gtk_widget_modify_base(page->entry_share_name, GTK_STATE_NORMAL, NULL);
    }
    property_page_check_sensitivity(page);
}

static void on_share_folder_toggled(GtkToggleButton *btn, gpointer user_data) {
    (void)btn;
    PropertyPage *page = (PropertyPage *)user_data;
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->checkbutton_share_folder)) &&
        !g_file_test("/usr/sbin/smbd", G_FILE_TEST_EXISTS) && !installing_service) {
        installing_service = TRUE;
        pending_pages = g_list_append(pending_pages, page);
    }
    page->is_dirty = TRUE;
    property_page_check_sensitivity(page);
}

static void on_button_apply_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    PropertyPage *page = (PropertyPage *)user_data;
    gboolean active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->checkbutton_share_folder));
    GError *err = NULL;

    if (!active) {
        if (page->was_initially_shared) {
            if (!shares_modify_share(page->path, NULL, &err)) {
                gtk_label_set_text(GTK_LABEL(page->label_status), err ? err->message : "Error");
                if (err) g_error_free(err);
                return;
            }
            page->was_initially_shared = FALSE;
        }
    } else {
        ShareInfo info;
        info.path = page->path;
        info.share_name = (char *)gtk_entry_get_text(GTK_ENTRY(page->entry_share_name));
        info.comment = (char *)gtk_entry_get_text(GTK_ENTRY(page->entry_share_comment));
        info.is_writable = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->checkbutton_share_rw_ro));
        info.guest_ok = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->checkbutton_share_guest_ok));

        struct stat st;
        if (stat(page->path, &st) == 0) {
            mode_t need = 0005 | (info.is_writable ? 0002 : 0);
            if ((st.st_mode & need) != need) {
                char *base = g_filename_display_basename(page->path);
                GtkWidget *dlg = gtk_message_dialog_new(
                    GTK_WINDOW(gtk_widget_get_toplevel(page->main)),
                    GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
                    g_dgettext("nemo-share", "Nemo needs to add some permissions to your folder \"%s\" in order to share it"),
                    base);
                gtk_message_dialog_format_secondary_text(
                    GTK_MESSAGE_DIALOG(dlg),
                    g_dgettext("nemo-share", "The folder \"%s\" needs the following extra permissions for sharing to work:\n%s%s%sDo you want Nemo to add these permissions to the folder automatically?"),
                    base, "", "", "");
                g_free(base);
                gtk_dialog_add_button(GTK_DIALOG(dlg), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
                gtk_dialog_add_button(GTK_DIALOG(dlg),
                                      g_dgettext("nemo-share", "Add the permissions automatically"),
                                      GTK_RESPONSE_OK);
                gtk_dialog_set_default_response(GTK_DIALOG(dlg), GTK_RESPONSE_OK);
                int resp = gtk_dialog_run(GTK_DIALOG(dlg));
                gtk_widget_destroy(dlg);
                if (resp == GTK_RESPONSE_OK)
                    chmod(page->path, st.st_mode | need);
            }
        }

        if (!shares_modify_share(page->was_initially_shared ? page->path : NULL, &info, &err)) {
            gtk_label_set_text(GTK_LABEL(page->label_status), err ? err->message : "Error");
            if (err) g_error_free(err);
            return;
        }
        page->was_initially_shared = TRUE;
        page->was_writable = info.is_writable;
        last_shared_guest = info.guest_ok;
    }

    page->is_dirty = FALSE;
    property_page_check_sensitivity(page);
    if (page->standalone_window)
        gtk_widget_destroy(page->standalone_window);
}

static PropertyPage *create_property_page(NemoFileInfo *fileinfo) {
    PropertyPage *page = g_new0(PropertyPage, 1);
    g_assert(fileinfo != NULL);
    GFile *loc = nemo_file_info_get_location(fileinfo);
    char *fullpath = g_file_get_path(loc);
    g_assert(fullpath != NULL && g_file_is_native(loc));
    g_object_unref(loc);

    page->path = fullpath;
    page->fileinfo = g_object_ref(fileinfo);

    ShareInfo *info = NULL;
    GError *err = NULL;
    if (!shares_get_share_info_for_path(page->path, &info, &err)) {
        GtkWidget *dlg = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, "%s",
                                                g_dgettext("nemo-share", "There was an error while getting the sharing information"));
        if (err) {
            gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dlg), "%s", err->message);
            g_error_free(err);
            err = NULL;
        }
        gtk_widget_show(dlg);
        info = NULL;
    }

    page->xml = gtk_builder_new();
    gtk_builder_set_translation_domain(page->xml, "nemo-share");
    g_assert(gtk_builder_add_from_file(page->xml, "/usr/share/nemo-share/interfaces/share-dialog.ui", &err));

    page->main = GTK_WIDGET(gtk_builder_get_object(page->xml, "vbox1"));
    g_assert(page->main != NULL);
    g_object_set_data_full(G_OBJECT(page->main), "PropertyPage", page, free_property_page);

    page->checkbutton_share_folder = GTK_WIDGET(gtk_builder_get_object(page->xml, "checkbutton_share_folder"));
    page->hbox_share_comment = GTK_WIDGET(gtk_builder_get_object(page->xml, "hbox_share_comment"));
    page->hbox_share_name = GTK_WIDGET(gtk_builder_get_object(page->xml, "hbox_share_name"));
    page->checkbutton_share_rw_ro = GTK_WIDGET(gtk_builder_get_object(page->xml, "checkbutton_share_rw_ro"));
    page->checkbutton_share_guest_ok = GTK_WIDGET(gtk_builder_get_object(page->xml, "checkbutton_share_guest_ok"));
    page->entry_share_name = GTK_WIDGET(gtk_builder_get_object(page->xml, "entry_share_name"));
    page->entry_share_comment = GTK_WIDGET(gtk_builder_get_object(page->xml, "entry_share_comment"));
    page->label_status = GTK_WIDGET(gtk_builder_get_object(page->xml, "label_status"));
    page->button_cancel = GTK_WIDGET(gtk_builder_get_object(page->xml, "button_cancel"));
    page->button_apply = GTK_WIDGET(gtk_builder_get_object(page->xml, "button_apply"));
    page->button_change = GTK_WIDGET(gtk_builder_get_object(page->xml, "button_change"));

    g_assert(page->checkbutton_share_folder && page->hbox_share_comment && page->hbox_share_name &&
             page->checkbutton_share_rw_ro && page->checkbutton_share_guest_ok && page->entry_share_name &&
             page->entry_share_comment && page->label_status && page->button_cancel &&
             page->button_apply && page->button_change);

    if (info == NULL) {
        char *base = g_filename_display_basename(page->path);
        gtk_entry_set_text(GTK_ENTRY(page->entry_share_name), base);
        g_free(base);
    } else {
        page->was_initially_shared = TRUE;
        page->was_writable = info->is_writable;
        gtk_entry_set_text(GTK_ENTRY(page->entry_share_name), info->share_name);
    }

    gtk_entry_set_text(GTK_ENTRY(page->entry_share_comment), (info && info->comment) ? info->comment : "");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(page->checkbutton_share_folder), info != NULL);
    if (g_utf8_strlen(gtk_entry_get_text(GTK_ENTRY(page->entry_share_name)), -1) > 13)
        set_share_name_too_long(page);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(page->checkbutton_share_rw_ro), info && info->is_writable);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(page->checkbutton_share_guest_ok), info && info->guest_ok);

    gtk_button_set_use_underline(GTK_BUTTON(page->button_apply), TRUE);
    gtk_button_set_image(GTK_BUTTON(page->button_apply), gtk_image_new_from_stock("gtk-save", GTK_ICON_SIZE_BUTTON));
    gtk_widget_set_sensitive(page->button_apply, FALSE);

    property_page_check_sensitivity(page);

    g_signal_connect(page->checkbutton_share_folder, "toggled", G_CALLBACK(on_share_folder_toggled), page);
    g_signal_connect(page->checkbutton_share_rw_ro, "toggled", G_CALLBACK(on_dirty_changed), page);
    g_signal_connect(page->checkbutton_share_guest_ok, "toggled", G_CALLBACK(on_dirty_changed), page);
    g_signal_connect(page->entry_share_name, "changed", G_CALLBACK(on_share_name_changed), page);
    g_signal_connect(page->entry_share_comment, "changed", G_CALLBACK(on_dirty_changed), page);
    g_signal_connect(page->button_apply, "clicked", G_CALLBACK(on_button_apply_clicked), page);
    g_signal_connect(page->button_change, "clicked", G_CALLBACK(on_button_change_clicked), page);

    if (info)
        shares_free_share_info(info);

    last_shared_guest = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->checkbutton_share_folder)) &&
                        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->checkbutton_share_guest_ok));
    return page;
}

static GList *nemo_share_get_pages(gpointer provider, GList *files) {
    (void)provider;
    if (files == NULL || files->next != NULL)
        return NULL;
    NemoFileInfo *file = NEMO_FILE_INFO(files->data);
    ShareInfo *info = NULL;
    gboolean is_shareable = FALSE;
    get_share_info_for_file(file, &info, &is_shareable);
    if (!is_shareable)
        return NULL;
    PropertyPage *page = create_property_page(file);
    gtk_widget_hide(page->button_cancel);
    if (info)
        shares_free_share_info(info);
    GtkWidget *lbl = gtk_label_new(g_dgettext("nemo-share", "Share"));
    return g_list_append(NULL, nemo_property_page_new("NemoShare::share", lbl, page->main));
}

static NemoOperationResult nemo_share_update_file_info(gpointer provider, NemoFileInfo *file,
                                                       GClosure *update_complete, NemoOperationHandle **handle) {
    (void)provider;
    (void)update_complete;
    (void)handle;
    ShareInfo *info = NULL;
    gboolean is_shareable = FALSE;
    get_share_info_for_file(file, &info, &is_shareable);
    if (is_shareable && info != NULL) {
        shares_free_share_info(info);
        nemo_file_info_add_emblem(file, "shared");
    }
    return NEMO_OPERATION_COMPLETE;
}

static void nemo_share_cancel_update(gpointer provider, NemoOperationHandle *handle) {
    (void)provider;
    (void)handle;
}

static void on_standalone_cancel(GtkButton *btn, gpointer user_data) {
    (void)btn;
    gtk_widget_destroy(GTK_WIDGET(user_data));
}

static void on_menu_item_activate(NemoMenuItem *item, gpointer user_data) {
    (void)item;
    NemoFileInfo *file = (NemoFileInfo *)user_data;
    g_assert(file != NULL);
    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), g_dgettext("nemo-share", "Folder Sharing"));
    gtk_window_set_position(GTK_WINDOW(win), GTK_WIN_POS_CENTER);
    PropertyPage *page = create_property_page(file);
    page->standalone_window = win;
    g_signal_connect(page->button_cancel, "clicked", G_CALLBACK(on_standalone_cancel), win);
    gtk_container_add(GTK_CONTAINER(win), page->main);
    gtk_widget_show(win);
}

static GList *nemo_share_get_file_items(gpointer provider, GtkWidget *window, GList *files) {
    (void)provider;
    (void)window;
    if (files == NULL || files->next != NULL)
        return NULL;
    NemoFileInfo *file = (NemoFileInfo *)files->data;
    ShareInfo *info = NULL;
    gboolean is_shareable = FALSE;
    get_share_info_for_file(file, &info, &is_shareable);
    if (!is_shareable)
        return NULL;
    if (info)
        shares_free_share_info(info);
    g_object_ref(file);
    NemoMenuItem *item = nemo_menu_item_new("NemoShare::share",
                                            g_dgettext("nemo-share", "Sharing Options"),
                                            g_dgettext("nemo-share", "Share this Folder"),
                                            "folder-remote");
    g_signal_connect(item, "activate", G_CALLBACK(on_menu_item_activate), file);
    g_object_set_data_full(G_OBJECT(item), "files", file, g_object_unref);
    GList *ret = g_list_append(NULL, nemo_menu_item_new_separator("NemoShare::separator"));
    ret = g_list_append(ret, item);
    return ret;
}

static GList *nemo_share_get_name_and_desc(gpointer provider) {
    (void)provider;
    return g_list_append(NULL, "nemo-share:::Allows to quickly share a folder");
}

static void nemo_share_property_page_iface_init(NemoPropertyPageProviderIface *iface) {
    iface->get_pages = nemo_share_get_pages;
}
static void nemo_share_info_iface_init(NemoInfoProviderIface *iface) {
    iface->update_file_info = nemo_share_update_file_info;
    iface->cancel_update = nemo_share_cancel_update;
}
static void nemo_share_menu_iface_init(NemoMenuProviderIface *iface) {
    iface->get_file_items = nemo_share_get_file_items;
}
static void nemo_share_name_desc_iface_init(NemoNameAndDescProviderIface *iface) {
    iface->get_name_and_desc = nemo_share_get_name_and_desc;
}

static void nemo_share_class_init(NemoShareClass *klass) {
    g_type_class_peek_parent(klass);
}
static void nemo_share_instance_init(NemoShare *self) {
    (void)self;
}

void nemo_module_initialize(GTypeModule *module) {
    bindtextdomain("nemo-share", "/usr/share/locale");
    bind_textdomain_codeset("nemo-share", "UTF-8");

    static const GTypeInfo info = {
        sizeof(NemoShareClass), NULL, NULL,
        (GClassInitFunc)nemo_share_class_init, NULL, NULL,
        sizeof(NemoShare), 0,
        (GInstanceInitFunc)nemo_share_instance_init, NULL
    };
    nemo_share_type = g_type_module_register_type(module, G_TYPE_OBJECT, "NemoShare", &info, 0);

    static const GInterfaceInfo pp_info = { (GInterfaceInitFunc)nemo_share_property_page_iface_init, NULL, NULL };
    static const GInterfaceInfo ip_info = { (GInterfaceInitFunc)nemo_share_info_iface_init, NULL, NULL };
    static const GInterfaceInfo mp_info = { (GInterfaceInitFunc)nemo_share_menu_iface_init, NULL, NULL };
    static const GInterfaceInfo nd_info = { (GInterfaceInitFunc)nemo_share_name_desc_iface_init, NULL, NULL };

    g_type_module_add_interface(module, nemo_share_type, nemo_property_page_provider_get_type(), &pp_info);
    g_type_module_add_interface(module, nemo_share_type, nemo_info_provider_get_type(), &ip_info);
    g_type_module_add_interface(module, nemo_share_type, nemo_menu_provider_get_type(), &mp_info);
    g_type_module_add_interface(module, nemo_share_type, nemo_name_and_desc_provider_get_type(), &nd_info);
}

void nemo_module_shutdown(void) {
}

void nemo_module_list_types(const GType **types, int *num_types) {
    type_list[0] = nemo_share_type;
    *types = type_list;
    *num_types = 1;
}
