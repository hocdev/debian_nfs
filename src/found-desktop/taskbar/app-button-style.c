#define _GNU_SOURCE
#include <dlfcn.h>
#include <gtk/gtk.h>

enum {
    THEME_ICON_SIZE = 24,
    VISIBLE_ICON_HEIGHT = 20,
    MAX_VISIBLE_ICON_WIDTH = 36,
    MAX_ICON_SIZE = 40,
    VISIBLE_ALPHA = 128
};

static GdkPixbuf *
load_button_icon(GtkIconTheme *theme, gpointer app)
{
    GAppInfo *(*get_app_info)(gpointer) = dlsym(RTLD_NEXT, "cdos_app_get_app_info");
    const GSList *(*get_windows)(gpointer) = dlsym(RTLD_NEXT, "cdos_app_get_windows");
    GAppInfo *info = get_app_info != NULL ? get_app_info(app) : NULL;
    GIcon *icon = info != NULL ? g_app_info_get_icon(info) : NULL;
    g_autoptr(GIcon) fallback = NULL;

    if (info == NULL && get_windows != NULL) {
        const GSList *windows = get_windows(app);
        if (windows != NULL) {
            GObject *window = windows->data;
            GParamSpec *icon_property = g_object_class_find_property(
                G_OBJECT_GET_CLASS(window), "icon");
            cairo_surface_t *surface = NULL;
            if (icon_property != NULL &&
                G_PARAM_SPEC_VALUE_TYPE(icon_property) == G_TYPE_POINTER) {
                // Mutter 3.36 returns a borrowed surface owned by MetaWindow.
                g_object_get(window, "icon", &surface, NULL);
            }
            if (surface != NULL &&
                cairo_surface_get_type(surface) == CAIRO_SURFACE_TYPE_IMAGE) {
                GdkPixbuf *pixels = gdk_pixbuf_get_from_surface(surface, 0, 0,
                    cairo_image_surface_get_width(surface),
                    cairo_image_surface_get_height(surface));
                if (pixels != NULL) {
                    return pixels;
                }
            }
        }
    }

    if (icon == NULL) {
        fallback = g_themed_icon_new("application-x-executable");
        icon = fallback;
    }
    g_autoptr(GtkIconInfo) match = gtk_icon_theme_lookup_by_gicon(
        theme, icon, THEME_ICON_SIZE, GTK_ICON_LOOKUP_FORCE_SIZE);
    return match != NULL ? gtk_icon_info_load_icon(match, NULL) : NULL;
}

static gint
size_for_visible_height(GdkPixbuf *image)
{
    const gint width = gdk_pixbuf_get_width(image);
    const gint height = gdk_pixbuf_get_height(image);
    const gint reference_size = MAX(width, height);
    gint visible_width = width;
    gint visible_height = height;

    if (gdk_pixbuf_get_has_alpha(image)) {
        const guchar *pixels = gdk_pixbuf_read_pixels(image);
        const gint stride = gdk_pixbuf_get_rowstride(image);
        gint left = width, right = -1, top = height, bottom = -1;
        for (gint y = 0; y < height; y++) {
            const guchar *row = pixels + y * stride;
            for (gint x = 0; x < width; x++) {
                if (row[x * 4 + 3] >= VISIBLE_ALPHA) {
                    left = MIN(left, x);
                    right = MAX(right, x);
                    top = MIN(top, y);
                    bottom = MAX(bottom, y);
                }
            }
        }
        if (right < left) {
            return THEME_ICON_SIZE;
        }
        visible_width = right - left + 1;
        visible_height = bottom - top + 1;
    }

    const gint target = (reference_size * VISIBLE_ICON_HEIGHT + visible_height / 2) /
        visible_height;
    const gint width_limit = (reference_size * MAX_VISIBLE_ICON_WIDTH) / visible_width;
    return CLAMP(MIN(target, width_limit), 1, MAX_ICON_SIZE);
}

static void
update_button_icon_size(GtkIconTheme *theme, GObject *button)
{
    gpointer app = g_object_get_data(button, "vos-taskbar-app");
    g_autoptr(GdkPixbuf) image = load_button_icon(theme, app);
    if (image != NULL) {
        g_autofree gchar *style = g_strdup_printf("icon-size: %dpx;",
            size_for_visible_height(image));
        g_object_set(button, "style", style, NULL);
    }
}

gpointer
applet_app_button_new(gpointer app, gboolean is_favorite, gpointer workspace)
{
    gpointer (*create_button)(gpointer, gboolean, gpointer) =
        dlsym(RTLD_NEXT, "applet_app_button_new");

    g_return_val_if_fail(create_button != NULL, NULL);
    GObject *button = create_button(app, is_favorite, workspace);
    GtkIconTheme *theme = gtk_icon_theme_get_default();
    if (button != NULL && app != NULL && theme != NULL) {
        g_object_set_data_full(button, "vos-taskbar-app", g_object_ref(app), g_object_unref);
        update_button_icon_size(theme, button);
        g_signal_connect_object(theme, "changed",
            G_CALLBACK(update_button_icon_size), button, 0);
    }
    return button;
}
