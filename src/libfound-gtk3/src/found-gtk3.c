/**
 * Reconstructed 100% from libfound-gtk3.so.0.0.0 (4.0.0-m3+1nfs5)
 * Decompiled via REA + Ghidra 12.2
 */

#include "found-gtk3.h"

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "FoundGTK3"

void
gtk_widget_set_attention_style(GtkWidget *widget, gint style)
{
    g_return_if_fail(GTK_IS_WIDGET(widget));

    gint current = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), "attention-style"));
    if (style == current) {
        return;
    }

    const gchar *new_class = NULL;
    switch (style) {
    case 1:
        new_class = "suggested-action";
        break;
    case 2:
        new_class = "destructive-action";
        break;
    case 3:
        new_class = "needs-attention";
        break;
    case 4:
        new_class = "warning";
        break;
    case 5:
        new_class = "error";
        break;
    case 6:
        new_class = "success";
        break;
    default:
        new_class = NULL;
        break;
    }

    GtkStyleContext *ctx = gtk_widget_get_style_context(widget);
    const gchar *old_class = g_object_get_data(G_OBJECT(widget), "attention-class");
    if (old_class != NULL && gtk_style_context_has_class(ctx, old_class)) {
        gtk_style_context_remove_class(ctx, old_class);
    }

    if (new_class != NULL) {
        gtk_style_context_add_class(ctx, new_class);
    }

    g_object_set_data(G_OBJECT(widget), "attention-class", (gpointer)new_class);
    g_object_set_data(G_OBJECT(widget), "attention-style", GINT_TO_POINTER(style));
}

gint
gtk_widget_get_attention_style(GtkWidget *widget)
{
    g_return_val_if_fail(GTK_IS_WIDGET(widget), 0);
    return GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), "attention-style"));
}

void
gtk_widget_style_context_replace_class(GtkWidget *widget,
                                       const gchar *old_class,
                                       const gchar *new_class)
{
    g_return_if_fail(GTK_IS_WIDGET(widget));

    GtkStyleContext *ctx = gtk_widget_get_style_context(widget);
    if (gtk_style_context_has_class(ctx, old_class)) {
        gtk_style_context_remove_class(ctx, old_class);
    }
    gtk_style_context_add_class(ctx, new_class);
}

void
gtk_dialog_set_button_attention_for_response(GtkDialog *dialog,
                                             gint response_id,
                                             gint style)
{
    g_return_if_fail(GTK_IS_DIALOG(dialog));

    GtkWidget *btn = gtk_dialog_get_widget_for_response(dialog, response_id);
    if (btn != NULL) {
        gtk_widget_set_attention_style(btn, style);
    }
}

gint
gtk_dialog_get_button_attention_for_response(GtkDialog *dialog,
                                             gint response_id)
{
    g_return_val_if_fail(GTK_IS_DIALOG(dialog), 0);

    GtkWidget *btn = gtk_dialog_get_widget_for_response(dialog, response_id);
    if (btn == NULL) {
        return 0;
    }
    return gtk_widget_get_attention_style(btn);
}

void
gtk_message_dialog_set_content_halign(GtkMessageDialog *dialog,
                                      GtkAlign align,
                                      gfloat xalign)
{
    g_return_if_fail(GTK_IS_MESSAGE_DIALOG(dialog));

    GtkWidget *area = gtk_message_dialog_get_message_area(dialog);
    GList *children = gtk_container_get_children(GTK_CONTAINER(area));
    for (GList *l = children; l != NULL; l = l->next) {
        GtkWidget *child = GTK_WIDGET(l->data);
        gtk_widget_set_halign(child, align);
        if (GTK_IS_LABEL(child)) {
            gtk_label_set_xalign(GTK_LABEL(child), xalign);
        }
    }
    g_list_free(children);
}
