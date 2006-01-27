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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 */

#ifndef __GWY_GRAPH_H__
#define __GWY_GRAPH_H__

#include <gdk/gdk.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtktable.h>

#include <libprocess/dataline.h>

#include <libgwydgets/gwyaxis.h>
#include <libgwydgets/gwygraphmodel.h>
#include <libgwydgets/gwygraphbasics.h>
#include <libgwydgets/gwygraphlabel.h>
#include <libgwydgets/gwygraphcorner.h>
#include <libgwydgets/gwygrapharea.h>

G_BEGIN_DECLS

#define GWY_TYPE_GRAPH            (gwy_graph_get_type())
#define GWY_GRAPH(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_GRAPH, GwyGraph))
#define GWY_GRAPH_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_GRAPH, GwyGraphClass))
#define GWY_IS_GRAPH(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_GRAPH))
#define GWY_IS_GRAPH_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_GRAPH))
#define GWY_GRAPH_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_GRAPH, GwyGraphClass))

typedef struct _GwyGraph      GwyGraph;
typedef struct _GwyGraphClass GwyGraphClass;


struct _GwyGraph {
    GtkTable table;

    GwyAxis *axis_top;
    GwyAxis *axis_left;
    GwyAxis *axis_right;
    GwyAxis *axis_bottom;

    GwyGraphCorner *corner_tl;
    GwyGraphCorner *corner_bl;
    GwyGraphCorner *corner_tr;
    GwyGraphCorner *corner_br;

    GwyGraphArea *area;

    GwyGraphModel *graph_model;

    GwyGraphGridType grid_type;
    gboolean enable_user_input;

    gulong notify_id;
    gulong layout_updated_id;

    gpointer reserved1;
    gpointer reserved2;
};

struct _GwyGraphClass {
    GtkTableClass parent_class;

    void (*gwygraph)(GwyGraph *grapher);
    void (*selected)(GwyGraph *grapher);
    void (*zoomed)(GwyGraph *grapher);

    gpointer reserved1;
    gpointer reserved2;
};

GtkWidget *gwy_graph_new(GwyGraphModel *gmodel);
GType      gwy_graph_get_type(void) G_GNUC_CONST;

GwyAxis*   gwy_graph_get_axis(GwyGraph *graph, GtkPositionType type);
void       gwy_graph_set_axis_visible(GwyGraph *graph, GtkPositionType type, gboolean is_visible);

GtkWidget *gwy_graph_get_area(GwyGraph *graph);

void       gwy_graph_signal_selected(GwyGraph *graph);

void       gwy_graph_set_model(GwyGraph *graph,
                                    GwyGraphModel *gmodel);
void       gwy_graph_set_status(GwyGraph *graph,
                                  GwyGraphStatusType status);
GwyGraphStatusType  gwy_graph_get_status(GwyGraph *graph);

GwyGraphModel *gwy_graph_get_model(GwyGraph *graph);


void       gwy_graph_get_cursor(GwyGraph *graph,
                                  gdouble *x_cursor, gdouble *y_cursor);

void       gwy_graph_request_x_range(GwyGraph *graph, gdouble x_min_req, gdouble x_max_req);
void       gwy_graph_request_y_range(GwyGraph *graph, gdouble y_min_req, gdouble y_max_req);
void       gwy_graph_get_x_range(GwyGraph *graph, gdouble *x_min, gdouble *x_max);
void       gwy_graph_get_y_range(GwyGraph *graph, gdouble *y_min, gdouble *y_max);

void       gwy_graph_enable_user_input(GwyGraph *graph, gboolean enable);


GdkPixbuf* gwy_graph_export_pixmap(GwyGraph *graph, 
                                     gboolean export_title, gboolean export_axis,
                                     gboolean export_labels, GdkPixbuf *pixbuf);
GString*   gwy_graph_export_postscript(GwyGraph *graph, 
                                         gboolean export_title, gboolean export_axis,
                                         gboolean export_labels, GString *str);

void       gwy_graph_zoom_in(GwyGraph *graph);
void       gwy_graph_zoom_out(GwyGraph *graph);

void       gwy_graph_set_grid_type(GwyGraph *graph, GwyGraphGridType grid_type);
GwyGraphGridType gwy_graph_get_grid_type(GwyGraph *graph);

void gwy_graph_set_x_grid_data(GwyGraph *graph, GArray *grid_data);
void gwy_graph_set_y_grid_data(GwyGraph *graph, GArray *grid_data);

const GArray* gwy_graph_get_x_grid_data(GwyGraph *graph);
const GArray* gwy_graph_get_y_grid_data(GwyGraph *graph);


G_END_DECLS

#endif /* __GWY_GRAPH_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
