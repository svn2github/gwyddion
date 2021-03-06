/*
 *  @(#) $Id$
 *  Copyright (C) 2004 David Necas (Yeti), Petr Klapetek.
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

#ifndef __GWY_GRAPH_MODEL_H__
#define __GWY_GRAPH_MODEL_H__

#include <libgwydgets/gwygraph.h>

#ifdef I_WANT_A_BROKEN_GWY_GRAPH_MODEL

G_BEGIN_DECLS

#define GWY_TYPE_GRAPH_MODEL                  (gwy_graph_model_get_type())
#define GWY_GRAPH_MODEL(obj)                  (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_GRAPH_MODEL, GwyGraphModel))
#define GWY_GRAPH_MODEL_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_GRAPH_MODEL, GwyGraphModelClass))
#define GWY_IS_GRAPH_MODEL(obj)               (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_GRAPH_MODEL))
#define GWY_IS_GRAPH_MODEL_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_GRAPH_MODEL))
#define GWY_GRAPH_MODEL_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_GRAPH_MODEL, GwyGraphModelClass))


typedef struct _GwyGraphModel GwyGraphModel;
typedef struct _GwyGraphModelClass GwyGraphModelClass;

/* XXX: really NEVER access these fields directly. They don't have to reflect
 * the graph values! */
struct _GwyGraphModel {
    GObject parent_instance;

    GwyGraph *graph;
    gulong graph_destroy_hid;

    gint ncurves;
    gint nautocurves;
    GObject **curves;

    GString *title;    /* XXX: GwyGraph has no such thing */

    gdouble x_reqmax;
    gdouble x_reqmin;
    gdouble y_reqmax;
    gdouble y_reqmin;

    gboolean has_x_unit;
    gboolean has_y_unit;
    GObject *x_unit;    /* XXX: Silly graph doesn't use GwySIUnit itself */
    GObject *y_unit;

    GString *top_label;
    GString *bottom_label;
    GString *left_label;
    GString *right_label;

    /* like GwyGraphLabelParams */
    GwyGraphLabelPosition label_position;
    gboolean label_has_frame;
    gint label_frame_thickness;

    /* reserved stuff */
    gint int1;
    gint int2;
    GwyGraphPointType enum1;
    GwyGraphPointType enum2;
    gpointer reserved1;
    gpointer reserved2;
    gpointer reserved3;
    gpointer reserved4;
};

struct _GwyGraphModelClass {
    GObjectClass parent_class;

    void (*value_changed)(GwyGraphModel *gmodel);  /* XXX: only formal */

    gpointer reserved1;
    gpointer reserved2;
};


GType      gwy_graph_model_get_type       (void) G_GNUC_CONST;
GObject*   gwy_graph_model_new            (GwyGraph *graph);
GtkWidget* gwy_graph_new_from_model       (GwyGraphModel *gmodel);
gint       gwy_graph_model_get_n_curves   (GwyGraphModel *gmodel);

G_END_DECLS
#endif  /* I_WANT_A_BROKEN_GWY_GRAPH_MODEL */

#endif /* __GWY_GRAPH_MODEL_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
