/*
 *  $Id$
 *  Copyright (C) 2013 David Nečas (Yeti).
 *  E-mail: yeti@gwyddion.net.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __LIBGWYUI_GRAPH_H__
#define __LIBGWYUI_GRAPH_H__

#include <gtk/gtk.h>
#include <libgwyui/graph-area.h>
#include <libgwyui/graph-axis.h>

G_BEGIN_DECLS

#define GWY_TYPE_GRAPH \
    (gwy_graph_get_type())
#define GWY_GRAPH(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_GRAPH, GwyGraph))
#define GWY_GRAPH_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_GRAPH, GwyGraphClass))
#define GWY_IS_GRAPH(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_GRAPH))
#define GWY_IS_GRAPH_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_GRAPH))
#define GWY_GRAPH_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_GRAPH, GwyGraphClass))

typedef struct _GwyGraph      GwyGraph;
typedef struct _GwyGraphClass GwyGraphClass;

struct _GwyGraph {
    GtkGrid grid;
    struct _GwyGraphPrivate *priv;
};

struct _GwyGraphClass {
    /*<private>*/
    GtkGridClass grid_class;
};

GType         gwy_graph_get_type       (void)                  G_GNUC_CONST;
GtkWidget*    gwy_graph_new            (void)                  G_GNUC_MALLOC;
GwyGraphArea* gwy_graph_get_area       (const GwyGraph *graph) G_GNUC_PURE;
GwyGraphAxis* gwy_graph_get_left_axis  (const GwyGraph *graph) G_GNUC_PURE;
GwyGraphAxis* gwy_graph_get_right_axis (const GwyGraph *graph) G_GNUC_PURE;
GwyGraphAxis* gwy_graph_get_top_axis   (const GwyGraph *graph) G_GNUC_PURE;
GwyGraphAxis* gwy_graph_get_bottom_axis(const GwyGraph *graph) G_GNUC_PURE;
void          gwy_graph_set_x_autorange(GwyGraph *graph,
                                        gboolean setting);
gboolean      gwy_graph_get_x_autorange(const GwyGraph *graph) G_GNUC_PURE;
void          gwy_graph_set_y_autorange(GwyGraph *graph,
                                        gboolean setting);
gboolean      gwy_graph_get_y_autorange(const GwyGraph *graph) G_GNUC_PURE;

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
