/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __GWY_DATA_WINDOW_H__
#define __GWY_DATA_WINDOW_H__

#include <gtk/gtkwindow.h>
#include <gtk/gtktooltips.h>

#include <libgwyddion/gwycontainer.h>
#include <libgwyddion/gwysiunit.h>
#include <libgwydgets/gwydataview.h>

G_BEGIN_DECLS

#define GWY_TYPE_DATA_WINDOW            (gwy_data_window_get_type())
#define GWY_DATA_WINDOW(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_DATA_WINDOW, GwyDataWindow))
#define GWY_DATA_WINDOW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_DATA_WINDOW, GwyDataWindowClass))
#define GWY_IS_DATA_WINDOW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_DATA_WINDOW))
#define GWY_IS_DATA_WINDOW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_DATA_WINDOW))
#define GWY_DATA_WINDOW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_DATA_WINDOW, GwyDataWindowClass))

typedef struct _GwyDataWindow      GwyDataWindow;
typedef struct _GwyDataWindowClass GwyDataWindowClass;

struct _GwyDataWindow {
    GtkWindow parent_instance;

    GtkWidget *table;
    GtkWidget *data_view;
    GtkWidget *hruler;
    GtkWidget *vruler;
    GtkWidget *statusbar;
    GtkWidget *coloraxis;
    GtkWidget *grad_selector;
    GtkAllocation old_allocation;

    GwySIValueFormat *coord_format;
    GwySIValueFormat *value_format;

    GtkWidget *ul_corner;
    GString *data_name;

    gulong id1;
    gulong id2;

    gpointer reserved1;
    gpointer reserved2;
    gpointer reserved3;
    gpointer reserved4;
};

struct _GwyDataWindowClass {
    GtkWindowClass parent_class;

    void (*reserved1)(void);
    void (*reserved2)(void);
    void (*reserved3)(void);
};

GType         gwy_data_window_get_type             (void) G_GNUC_CONST;
GtkWidget*    gwy_data_window_new                  (GwyDataView *data_view);
GwyDataView*  gwy_data_window_get_data_view        (GwyDataWindow *data_window);
GtkWidget*    gwy_data_window_get_color_axis       (GwyDataWindow *data_window);
GwyContainer* gwy_data_window_get_data             (GwyDataWindow *data_window);
void          gwy_data_window_set_zoom             (GwyDataWindow *data_window,
                                                    gint izoom);
const gchar*  gwy_data_window_get_data_name        (GwyDataWindow *data_window);
void          gwy_data_window_set_data_name        (GwyDataWindow *data_window,
                                                    const gchar *data_name);
GtkWidget*    gwy_data_window_get_ul_corner_widget (GwyDataWindow *data_window);
void          gwy_data_window_set_ul_corner_widget (GwyDataWindow *data_window,
                                                    GtkWidget *corner);
void          gwy_data_window_class_set_tooltips   (GtkTooltips *tips);
GtkTooltips*  gwy_data_window_class_get_tooltips   (void);

G_END_DECLS

#endif /* __GWY_DATA_WINDOW_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
