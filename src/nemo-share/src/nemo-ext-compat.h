#ifndef NEMO_EXT_COMPAT_H
#define NEMO_EXT_COMPAT_H

#include <glib-object.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

typedef struct _NemoFileInfo NemoFileInfo;
typedef struct _NemoPropertyPage NemoPropertyPage;
typedef struct _NemoMenuItem NemoMenuItem;
typedef struct _NemoOperationHandle NemoOperationHandle;

typedef enum {
    NEMO_OPERATION_COMPLETE,
    NEMO_OPERATION_FAILED,
    NEMO_OPERATION_IN_PROGRESS
} NemoOperationResult;

typedef struct {
    GTypeInterface g_iface;
    GList *(*get_pages)(gpointer provider, GList *files);
} NemoPropertyPageProviderIface;

typedef struct {
    GTypeInterface g_iface;
    NemoOperationResult (*update_file_info)(gpointer provider, NemoFileInfo *file,
                                            GClosure *update_complete, NemoOperationHandle **handle);
    void (*cancel_update)(gpointer provider, NemoOperationHandle *handle);
} NemoInfoProviderIface;

typedef struct {
    GTypeInterface g_iface;
    GList *(*get_file_items)(gpointer provider, GtkWidget *window, GList *files);
    GList *(*get_background_items)(gpointer provider, GtkWidget *window, NemoFileInfo *current_folder);
} NemoMenuProviderIface;

typedef struct {
    GTypeInterface g_iface;
    GList *(*get_name_and_desc)(gpointer provider);
} NemoNameAndDescProviderIface;

GType nemo_file_info_get_type(void);
#define NEMO_FILE_INFO(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), nemo_file_info_get_type(), NemoFileInfo))
GType nemo_property_page_provider_get_type(void);
GType nemo_info_provider_get_type(void);
GType nemo_menu_provider_get_type(void);
GType nemo_name_and_desc_provider_get_type(void);

char *nemo_file_info_get_uri(NemoFileInfo *file);
GFile *nemo_file_info_get_location(NemoFileInfo *file);
gboolean nemo_file_info_is_directory(NemoFileInfo *file);
void nemo_file_info_add_emblem(NemoFileInfo *file, const char *emblem_name);

NemoPropertyPage *nemo_property_page_new(const char *name, GtkWidget *label, GtkWidget *page);
NemoMenuItem *nemo_menu_item_new(const char *name, const char *label, const char *tip, const char *icon);
NemoMenuItem *nemo_menu_item_new_separator(const char *name);

#endif /* NEMO_EXT_COMPAT_H */
