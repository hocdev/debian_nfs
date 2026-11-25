#ifndef __FOUND_GTK3_H__
#define __FOUND_GTK3_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef enum {
    GTK_ATTENTION_NONE = 0,
    GTK_ATTENTION_SUGGESTED = 1,
    GTK_ATTENTION_DESTRUCTIVE = 2,
    GTK_ATTENTION_NEEDS = 3,
    GTK_ATTENTION_WARNING = 4,
    GTK_ATTENTION_ERROR = 5,
    GTK_ATTENTION_SUCCESS = 6
} GtkAttentionStyle;

void gtk_widget_set_attention_style(GtkWidget *widget, gint style);
gint gtk_widget_get_attention_style(GtkWidget *widget);
void gtk_widget_style_context_replace_class(GtkWidget *widget,
                                            const gchar *old_class,
                                            const gchar *new_class);

void gtk_dialog_set_button_attention_for_response(GtkDialog *dialog,
                                                  gint response_id,
                                                  gint style);
gint gtk_dialog_get_button_attention_for_response(GtkDialog *dialog,
                                                  gint response_id);

void gtk_message_dialog_set_content_halign(GtkMessageDialog *dialog,
                                           GtkAlign align,
                                           gfloat xalign);

G_END_DECLS

#endif /* __FOUND_GTK3_H__ */
