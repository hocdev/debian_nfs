#ifndef __CDOS_PANEL_H__
#define __CDOS_PANEL_H__

#include "clutter-compat.h"

G_BEGIN_DECLS

#define CDOS_TYPE_PANEL (cdos_panel_get_type())
G_DECLARE_FINAL_TYPE(CdosPanel, cdos_panel, CDOS, PANEL, GObject)

typedef struct _CdosPanelItemTooltip CdosPanelItemTooltip;

GType cdos_panel_get_type(void);
CdosPanel *cdos_panel_new(gboolean bottom_position, gboolean is_primary);

void cdos_panel_enable(CdosPanel *self, gint delay_ms);
void cdos_panel_disable(CdosPanel *self, gint delay_ms);

ClutterActor *cdos_panel_get_leftbox(CdosPanel *self);
ClutterActor *cdos_panel_get_centerbox(CdosPanel *self);
ClutterActor *cdos_panel_get_rightbox(CdosPanel *self);

gboolean cdos_panel_get_hidden(CdosPanel *self);
gboolean cdos_panel_get_hideable(CdosPanel *self);
void cdos_panel_set_hideable(CdosPanel *self, gboolean hideable);

gboolean cdos_panel_get_bottom_position(CdosPanel *self);
void cdos_panel_set_bottom_position(CdosPanel *self, gboolean bottom_position);

gfloat cdos_panel_get_height(CdosPanel *self);
void cdos_panel_set_monitor(CdosPanel *self, gint monitor_index);

gboolean cdos_panel_on_button_press(CdosPanel *self, ClutterEvent *event);

void cdos_panel_add_launcher(CdosPanel *self, const gchar *desktop_id);
void cdos_panel_add_separator_item(CdosPanel *self);
void cdos_panel_add_menu_item(CdosPanel *self, gpointer menu_item);

GType cdos_panel_item_tooltip_get_type(void);
CdosPanelItemTooltip *cdos_panel_item_tooltip_new(ClutterActor *item, const gchar *text);

G_END_DECLS

#endif /* __CDOS_PANEL_H__ */
