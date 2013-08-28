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

#include <glib/gi18n-lib.h>
#include "libgwy/macros.h"
#include "libgwy/object-utils.h"
#include "libgwyui/types.h"
#include "libgwyui/graph-area.h"
#include "libgwyui/graph-axis.h"
#include "libgwyui/graph.h"

enum {
    PROP_0,
    PROP_AREA,
    PROP_LEFT_AXIS,
    PROP_RIGHT_AXIS,
    PROP_TOP_AXIS,
    PROP_BOTTOM_AXIS,
    N_PROPS,
};

struct _GwyGraphPrivate {
    GwyGraphArea *area;
    GwyGraphAxis *axis[4];
};

typedef struct _GwyGraphPrivate Graph;

static void gwy_graph_dispose     (GObject *object);
static void gwy_graph_finalize    (GObject *object);
static void gwy_graph_set_property(GObject *object,
                                   guint prop_id,
                                   const GValue *value,
                                   GParamSpec *pspec);
static void gwy_graph_get_property(GObject *object,
                                   guint prop_id,
                                   GValue *value,
                                   GParamSpec *pspec);

static GParamSpec *properties[N_PROPS];

G_DEFINE_TYPE(GwyGraph, gwy_graph, GTK_TYPE_GRID);

static void
gwy_graph_class_init(GwyGraphClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    //GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

    g_type_class_add_private(klass, sizeof(Graph));

    gobject_class->dispose = gwy_graph_dispose;
    gobject_class->finalize = gwy_graph_finalize;
    gobject_class->get_property = gwy_graph_get_property;
    gobject_class->set_property = gwy_graph_set_property;

    properties[PROP_AREA]
        = g_param_spec_object("area",
                              "Area",
                              "Graph area widget within the graph.",
                              GWY_TYPE_GRAPH_AREA,
                              G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    properties[PROP_LEFT_AXIS]
        = g_param_spec_object("left-axis",
                              "Left ruler",
                              "Left axis widget within the graph.",
                              GWY_TYPE_GRAPH_AXIS,
                              G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    properties[PROP_RIGHT_AXIS]
        = g_param_spec_object("right-axis",
                              "Right ruler",
                              "Right axis widget within the graph.",
                              GWY_TYPE_GRAPH_AXIS,
                              G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    properties[PROP_TOP_AXIS]
        = g_param_spec_object("top-axis",
                              "Top ruler",
                              "Top axis widget within the graph.",
                              GWY_TYPE_GRAPH_AXIS,
                              G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    properties[PROP_BOTTOM_AXIS]
        = g_param_spec_object("bottom-axis",
                              "Bottom ruler",
                              "Bottom axis widget within the graph.",
                              GWY_TYPE_GRAPH_AXIS,
                              G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    for (guint i = 1; i < N_PROPS; i++)
        g_object_class_install_property(gobject_class, i, properties[i]);
}

static void
gwy_graph_init(GwyGraph *graph)
{
    static const guint axis_attachments[] = {
        0, 1, 2, 1, 1, 0, 1, 2
    };

    graph->priv = G_TYPE_INSTANCE_GET_PRIVATE(graph, GWY_TYPE_GRAPH, Graph);
    Graph *priv = graph->priv;

    GtkGrid *grid = GTK_GRID(graph);

    GtkWidget *area = gwy_graph_area_new();
    priv->area = GWY_GRAPH_AREA(area);
    gtk_widget_set_hexpand(area, TRUE);
    gtk_widget_set_vexpand(area, TRUE);
    gtk_grid_attach(grid, area, 1, 1, 1, 1);
    gtk_widget_show(area);

    for (GtkPositionType edge = GTK_POS_LEFT; edge <= GTK_POS_BOTTOM; edge++) {
        GtkWidget *widget = gwy_graph_axis_new();
        GwyAxis *axis = GWY_AXIS(widget);
        priv->axis[edge] = GWY_GRAPH_AXIS(widget);
        gwy_axis_set_edge(axis, edge);
        gtk_grid_attach(grid, widget,
                        axis_attachments[2*edge], axis_attachments[2*edge + 1],
                        1, 1);
        gtk_widget_show(widget);
    }
}

static void
gwy_graph_dispose(GObject *object)
{
    G_OBJECT_CLASS(gwy_graph_parent_class)->dispose(object);
}

static void
gwy_graph_finalize(GObject *object)
{
    G_OBJECT_CLASS(gwy_graph_parent_class)->finalize(object);
}

static void
gwy_graph_set_property(GObject *object,
                       guint prop_id,
                       G_GNUC_UNUSED const GValue *value,
                       GParamSpec *pspec)
{
    G_GNUC_UNUSED GwyGraph *graph = GWY_GRAPH(object);

    switch (prop_id) {
        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_graph_get_property(GObject *object,
                       guint prop_id,
                       GValue *value,
                       GParamSpec *pspec)
{
    Graph *priv = GWY_GRAPH(object)->priv;

    switch (prop_id) {
        case PROP_AREA:
        g_value_set_object(value, priv->area);
        break;

        case PROP_LEFT_AXIS:
        g_value_set_object(value, priv->axis[GTK_POS_LEFT]);
        break;

        case PROP_RIGHT_AXIS:
        g_value_set_object(value, priv->axis[GTK_POS_RIGHT]);
        break;

        case PROP_TOP_AXIS:
        g_value_set_object(value, priv->axis[GTK_POS_TOP]);
        break;

        case PROP_BOTTOM_AXIS:
        g_value_set_object(value, priv->axis[GTK_POS_BOTTOM]);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

/**
 * gwy_graph_new:
 *
 * Creates a new graph.
 *
 * Returns: A new graph.
 **/
GtkWidget*
gwy_graph_new(void)
{
    return g_object_newv(GWY_TYPE_GRAPH, 0, NULL);
}

/**
 * gwy_graph_get_area:
 * @graph: A graph.
 *
 * Gets the graph area widget used by a graph.
 *
 * Returns: (transfer none):
 *          #GwyGraphArea widget used by the graph.
 **/
GwyGraphArea*
gwy_graph_get_area(const GwyGraph *graph)
{
    g_return_val_if_fail(GWY_IS_GRAPH(graph), NULL);
    return graph->priv->area;
}

/**
 * gwy_graph_get_left_axis:
 * @graph: A graph.
 *
 * Gets the left axis widget used by a graph.
 *
 * Returns: (transfer none):
 *          #GwyGraphAxis widget used by the graph as the left axis.
 **/
GwyGraphAxis*
gwy_graph_get_left_axis(const GwyGraph *graph)
{
    g_return_val_if_fail(GWY_IS_GRAPH(graph), NULL);
    return graph->priv->axis[GTK_POS_LEFT];
}

/**
 * gwy_graph_get_right_axis:
 * @graph: A graph.
 *
 * Gets the right axis widget used by a graph.
 *
 * Returns: (transfer none):
 *          #GwyGraphAxis widget used by the graph as the right axis.
 **/
GwyGraphAxis*
gwy_graph_get_right_axis(const GwyGraph *graph)
{
    g_return_val_if_fail(GWY_IS_GRAPH(graph), NULL);
    return graph->priv->axis[GTK_POS_RIGHT];
}

/**
 * gwy_graph_get_top_axis:
 * @graph: A graph.
 *
 * Gets the top axis widget used by a graph.
 *
 * Returns: (transfer none):
 *          #GwyGraphAxis widget used by the graph as the top axis.
 **/
GwyGraphAxis*
gwy_graph_get_top_axis(const GwyGraph *graph)
{
    g_return_val_if_fail(GWY_IS_GRAPH(graph), NULL);
    return graph->priv->axis[GTK_POS_TOP];
}

/**
 * gwy_graph_get_bottom_axis:
 * @graph: A graph.
 *
 * Gets the bottom axis widget used by a graph.
 *
 * Returns: (transfer none):
 *          #GwyGraphAxis widget used by the graph as the bottom axis.
 **/
GwyGraphAxis*
gwy_graph_get_bottom_axis(const GwyGraph *graph)
{
    g_return_val_if_fail(GWY_IS_GRAPH(graph), NULL);
    return graph->priv->axis[GTK_POS_BOTTOM];
}

/**
 * SECTION: graph
 * @title: GwyGraph
 * @short_description: Display one-dimensonal data as graphs
 **/

/**
 * GwyGraph:
 *
 * Widget for display of one-dimensional data as graphs.
 *
 * The #GwyGraph struct contains private data only and should be accessed
 * using the functions below.
 **/

/**
 * GwyGraphClass:
 *
 * Class of graphs.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
