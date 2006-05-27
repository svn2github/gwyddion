/*
 *  @(#) $Id$
 *  Copyright (C) 2006 David Necas (Yeti), Petr Klapetek.
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

#ifndef __GWY_GRAPH_DATA_H__
#define __GWY_GRAPH_DATA_H__

#include <gtk/gtktreeview.h>
#include <libgwydgets/gwygraphmodel.h>
#include <libgwydgets/gwynullstore.h>

G_BEGIN_DECLS

#define GWY_TYPE_GRAPH_DATA            (gwy_graph_data_get_type())
#define GWY_GRAPH_DATA(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_GRAPH_DATA, GwyGraphData))
#define GWY_GRAPH_DATA_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_GRAPH_DATA, GwyGraphDataClass))
#define GWY_IS_GRAPH_DATA(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_GRAPH_DATA))
#define GWY_IS_GRAPH_DATA_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_GRAPH_DATA))
#define GWY_GRAPH_DATA_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_GRAPH_DATA, GwyGraphDataClass))

typedef struct _GwyGraphData      GwyGraphData;
typedef struct _GwyGraphDataClass GwyGraphDataClass;

struct _GwyGraphData {
    GtkTreeView treeview;

    GwyGraphModel *graph_model;
    GwyNullStore *store;
    GArray *curves;

    gulong notify_id;
    GwySIValueFormat *vformatx;
    GwySIValueFormat *vformaty;

    gpointer reserved1;
    gpointer reserved2;
    gpointer reserved3;
    gpointer reserved4;
};

struct _GwyGraphDataClass {
    GtkTreeViewClass parent_class;

    gpointer reserved1;
    gpointer reserved2;
    gpointer reserved3;
    gpointer reserved4;
};

GType          gwy_graph_data_get_type (void) G_GNUC_CONST;
GtkWidget*     gwy_graph_data_new      (GwyGraphModel *gmodel);
void           gwy_graph_data_set_model(GwyGraphData *graph_data,
                                        GwyGraphModel *gmodel);
GwyGraphModel* gwy_graph_data_get_model(GwyGraphData *graph_data);

G_END_DECLS

#endif /* __GWY_GRAPH_DATA_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
