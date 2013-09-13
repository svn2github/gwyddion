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
    PROP_X_AUTORANGE,
    PROP_Y_AUTORANGE,
    N_PROPS,
};

struct _GwyGraphPrivate {
    GwyGraphArea *area;
    GwyGraphAxis *axis[4];

    gboolean xautorange : 1;
    gboolean yautorange : 1;
};

typedef void (*SetGridFunc)(GwyGraphArea *area,
                            const gdouble *grid,
                            guint n);

typedef struct _GwyGraphPrivate Graph;

static void     gwy_graph_dispose     (GObject *object);
static void     gwy_graph_finalize    (GObject *object);
static void     gwy_graph_set_property(GObject *object,
                                       guint prop_id,
                                       const GValue *value,
                                       GParamSpec *pspec);
static void     gwy_graph_get_property(GObject *object,
                                       guint prop_id,
                                       GValue *value,
                                       GParamSpec *pspec);
static void     gwy_graph_realize     (GtkWidget *widget);
static gboolean gwy_graph_draw        (GtkWidget *widget,
                                       cairo_t *cr);
static gboolean set_x_autorange       (GwyGraph *graph,
                                       gboolean setting);
static gboolean set_y_autorange       (GwyGraph *graph,
                                       gboolean setting);
static void     area_item_updated     (GwyGraph *graph,
                                       guint i);
static void     area_item_inserted    (GwyGraph *graph,
                                       guint i);
static void     area_item_deleted     (GwyGraph *graph,
                                       guint i);
static void     update_xrange         (GwyGraph *graph);
static void     update_yrange         (GwyGraph *graph);
static void     negotiate_xrange      (GwyGraph *graph);
static void     negotiate_yrange      (GwyGraph *graph);
static void     set_fixed_xrange      (GwyGraph *graph);
static void     set_fixed_yrange      (GwyGraph *graph);
static void     xticks_placed         (GwyGraph *graph,
                                       GwyAxis *axis);
static void     yticks_placed         (GwyGraph *graph,
                                       GwyAxis *axis);
static void     set_grid_from_ticks   (GwyAxis *axis,
                                       GwyGraphArea *area,
                                       SetGridFunc func);

static GParamSpec *properties[N_PROPS];

G_DEFINE_TYPE(GwyGraph, gwy_graph, GTK_TYPE_GRID);

static void
gwy_graph_class_init(GwyGraphClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

    g_type_class_add_private(klass, sizeof(Graph));

    gobject_class->dispose = gwy_graph_dispose;
    gobject_class->finalize = gwy_graph_finalize;
    gobject_class->get_property = gwy_graph_get_property;
    gobject_class->set_property = gwy_graph_set_property;

    widget_class->realize = gwy_graph_realize;
    widget_class->draw = gwy_graph_draw;

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

    properties[PROP_X_AUTORANGE]
        = g_param_spec_boolean("x-autorange",
                               "X autorange",
                               "Whether abscissa adapts automatically to data.",
                               TRUE,
                               G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_Y_AUTORANGE]
        = g_param_spec_boolean("y-autorange",
                               "Y autorange",
                               "Whether ordinate adapts automatically to data.",
                               TRUE,
                               G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

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
    priv->xautorange = TRUE;
    priv->yautorange = TRUE;

    GtkGrid *grid = GTK_GRID(graph);

    GtkWidget *area = gwy_graph_area_new();
    priv->area = GWY_GRAPH_AREA(area);
    gtk_widget_set_hexpand(area, TRUE);
    gtk_widget_set_vexpand(area, TRUE);
    gtk_grid_attach(grid, area, 1, 1, 1, 1);
    gtk_widget_show(area);
    g_signal_connect_swapped(area, "item-updated",
                             G_CALLBACK(area_item_updated), graph);
    g_signal_connect_swapped(area, "item-inserted",
                             G_CALLBACK(area_item_inserted), graph);
    g_signal_connect_swapped(area, "item-deleted",
                             G_CALLBACK(area_item_deleted), graph);

    for (GtkPositionType edge = GTK_POS_LEFT; edge <= GTK_POS_BOTTOM; edge++) {
        gboolean primary = (edge == GTK_POS_LEFT || edge == GTK_POS_BOTTOM);
        GtkWidget *widget = gwy_graph_axis_new();
        GwyAxis *axis = GWY_AXIS(widget);
        priv->axis[edge] = GWY_GRAPH_AXIS(widget);
        g_object_set(axis,
                     "edge", edge,
                     "ticks-at-edges", TRUE,
                     "show-tick-labels", primary,
                     "show-label", primary,
                     NULL);
        gtk_grid_attach(grid, widget,
                        axis_attachments[2*edge], axis_attachments[2*edge + 1],
                        1, 1);
        gtk_widget_show(widget);
    }

    gwy_axis_set_mirror(GWY_AXIS(priv->axis[GTK_POS_RIGHT]),
                        GWY_AXIS(priv->axis[GTK_POS_LEFT]));
    g_signal_connect_swapped(GWY_AXIS(priv->axis[GTK_POS_LEFT]),
                             "ticks-placed",
                             G_CALLBACK(yticks_placed), graph);

    gwy_axis_set_mirror(GWY_AXIS(priv->axis[GTK_POS_TOP]),
                        GWY_AXIS(priv->axis[GTK_POS_BOTTOM]));
    g_signal_connect_swapped(GWY_AXIS(priv->axis[GTK_POS_BOTTOM]),
                             "ticks-placed",
                             G_CALLBACK(xticks_placed), graph);
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
                       const GValue *value,
                       GParamSpec *pspec)
{
    GwyGraph *graph = GWY_GRAPH(object);

    switch (prop_id) {
        case PROP_X_AUTORANGE:
        set_x_autorange(graph, g_value_get_boolean(value));
        break;

        case PROP_Y_AUTORANGE:
        set_y_autorange(graph, g_value_get_boolean(value));
        break;

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

        case PROP_X_AUTORANGE:
        g_value_set_boolean(value, priv->xautorange);
        break;

        case PROP_Y_AUTORANGE:
        g_value_set_boolean(value, priv->yautorange);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_graph_realize(GtkWidget *widget)
{
    GTK_WIDGET_CLASS(gwy_graph_parent_class)->realize(widget);
    GwyGraph *graph = GWY_GRAPH(widget);
    update_xrange(graph);
    update_yrange(graph);
}

static gboolean
gwy_graph_draw(GtkWidget *widget,
               cairo_t *cr)
{
    cairo_save(cr);
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_paint(cr);
    cairo_restore(cr);
    return GTK_WIDGET_CLASS(gwy_graph_parent_class)->draw(widget, cr);
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
 * gwy_graph_set_x_autorange:
 * @graph: A graph.
 * @setting: %TRUE to enable automatic adaptation of abscissa to data, %FALSE
 *           to disable it.
 *
 * Sets whether the abscissa of a graph adapts automatically to data.
 *
 * If abscissa range adaptation is disabled the graph area should have a
 * horizontal range set explicitly with gwy_graph_area_set_xrange().  The
 * horizontal axes will then adapt automatically.
 **/
void
gwy_graph_set_x_autorange(GwyGraph *graph,
                          gboolean setting)
{
    g_return_if_fail(GWY_IS_GRAPH(graph));
    if (!set_x_autorange(graph, setting))
        return;

    g_object_notify_by_pspec(G_OBJECT(graph), properties[PROP_X_AUTORANGE]);
}

/**
 * gwy_graph_get_x_autorange:
 * @graph: A graph.
 *
 * Gets whether the abscissa of a graph adapts automatically to data.
 *
 * Returns: %TRUE if the abscissa adapts to data, %FALSE if its range is fixed.
 **/
gboolean
gwy_graph_get_x_autorange(const GwyGraph *graph)
{
    g_return_val_if_fail(GWY_IS_GRAPH(graph), FALSE);
    return graph->priv->xautorange;
}

/**
 * gwy_graph_set_y_autorange:
 * @graph: A graph.
 * @setting: %TRUE to enable automatic adaptation of ordinate to data, %FALSE
 *           to disable it.
 *
 * Sets whether the ordinate of a graph adapts automatically to data.
 *
 * If ordinate range adaptation is disabled the graph area should have a
 * vertical range set explicitly with gwy_graph_area_set_yrange().  The
 * vertical axes will then adapt automatically.
 **/
void
gwy_graph_set_y_autorange(GwyGraph *graph,
                          gboolean setting)
{
    g_return_if_fail(GWY_IS_GRAPH(graph));
    if (!set_y_autorange(graph, setting))
        return;

    g_object_notify_by_pspec(G_OBJECT(graph), properties[PROP_Y_AUTORANGE]);
}

/**
 * gwy_graph_get_y_autorange:
 * @graph: A graph.
 *
 * Gets whether the ordinate of a graph adapts automatically to data.
 *
 * Returns: %TRUE if the abscissa adapts to data, %FALSE if its range is fixed.
 **/
gboolean
gwy_graph_get_y_autorange(const GwyGraph *graph)
{
    g_return_val_if_fail(GWY_IS_GRAPH(graph), FALSE);
    return graph->priv->yautorange;
}

static gboolean
set_x_autorange(GwyGraph *graph,
                gboolean setting)
{
    Graph *priv = graph->priv;
    if (!setting == !priv->xautorange)
        return FALSE;

    priv->xautorange = !!setting;
    if (priv->xautorange)
        negotiate_xrange(graph);
    else
        set_fixed_xrange(graph);

    return TRUE;
}

static gboolean
set_y_autorange(GwyGraph *graph,
                gboolean setting)
{
    Graph *priv = graph->priv;
    if (!setting == !priv->yautorange)
        return FALSE;

    priv->yautorange = !!setting;
    if (priv->yautorange)
        negotiate_yrange(graph);
    else
        set_fixed_yrange(graph);

    return TRUE;
}

static void
area_item_updated(GwyGraph *graph,
                  G_GNUC_UNUSED guint i)
{
    negotiate_xrange(graph);
    negotiate_yrange(graph);
}

static void
area_item_inserted(GwyGraph *graph,
                   G_GNUC_UNUSED guint i)
{
    negotiate_xrange(graph);
    negotiate_yrange(graph);
}

static void
area_item_deleted(GwyGraph *graph,
                  G_GNUC_UNUSED guint i)
{
    negotiate_xrange(graph);
    negotiate_yrange(graph);
}

static void
update_xrange(GwyGraph *graph)
{
    if (graph->priv->xautorange)
        negotiate_xrange(graph);
    else
        set_fixed_xrange(graph);
}

static void
update_yrange(GwyGraph *graph)
{
    if (graph->priv->yautorange)
        negotiate_yrange(graph);
    else
        set_fixed_yrange(graph);
}

static void
negotiate_xrange(GwyGraph *graph)
{
    Graph *priv = graph->priv;
    if (!priv->xautorange)
        return;

    GtkWidget *widget = GTK_WIDGET(graph);
    if (!gtk_widget_get_realized(widget))
        return;

    GwyGraphScaleType scale = gwy_graph_area_get_xscale(priv->area);
    GwyRange range;
    if (scale == GWY_GRAPH_SCALE_LOG) {
        if (!gwy_graph_area_full_xposrange(priv->area, &range))
            range = (GwyRange){ 0.1, 10.0 };
    }
    else {
        if (!gwy_graph_area_full_xrange(priv->area, &range))
            range = (GwyRange){ 0.0, 1.0 };
    }

    GwyAxis *ticksaxis = GWY_AXIS(priv->axis[GTK_POS_BOTTOM]);
    GwyAxis *otheraxis = GWY_AXIS(priv->axis[GTK_POS_TOP]);

    gwy_axis_set_snap_to_ticks(ticksaxis, TRUE);
    gwy_axis_set_snap_to_ticks(otheraxis, TRUE);

    gwy_axis_request_range(ticksaxis, &range);
    // FIXME: It this a good way to ensure recalculation?
    gwy_axis_ticks(ticksaxis, NULL);
    gwy_axis_get_range(ticksaxis, &range);
    gwy_graph_area_set_xrange(priv->area, &range);
    // This will cause redraw but ticks are taken from tickaxis.
    gwy_axis_request_range(otheraxis, &range);
}

static void
negotiate_yrange(GwyGraph *graph)
{
    Graph *priv = graph->priv;
    if (!priv->yautorange)
        return;

    GtkWidget *widget = GTK_WIDGET(graph);
    if (!gtk_widget_get_realized(widget))
        return;

    GwyGraphScaleType scale = gwy_graph_area_get_yscale(priv->area);
    GwyRange range;
    if (scale == GWY_GRAPH_SCALE_LOG) {
        if (!gwy_graph_area_full_yposrange(priv->area, &range))
            range = (GwyRange){ 0.1, 10.0 };
    }
    else {
        if (!gwy_graph_area_full_yrange(priv->area, &range))
            range = (GwyRange){ 0.0, 1.0 };
    }

    GwyAxis *ticksaxis = GWY_AXIS(priv->axis[GTK_POS_LEFT]);
    GwyAxis *otheraxis = GWY_AXIS(priv->axis[GTK_POS_RIGHT]);

    gwy_axis_set_snap_to_ticks(ticksaxis, TRUE);
    gwy_axis_set_snap_to_ticks(otheraxis, TRUE);

    gwy_axis_request_range(ticksaxis, &range);
    // FIXME: It this a good way to ensure recalculation?
    gwy_axis_ticks(ticksaxis, NULL);
    gwy_axis_get_range(ticksaxis, &range);
    // This will cause redraw but ticks are taken from tickaxis.
    gwy_axis_request_range(otheraxis, &range);
}

static void
set_fixed_xrange(GwyGraph *graph)
{
    Graph *priv = graph->priv;
    if (priv->xautorange)
        return;

    GwyAxis *firstaxis = GWY_AXIS(priv->axis[GTK_POS_BOTTOM]);
    GwyAxis *secondaxis = GWY_AXIS(priv->axis[GTK_POS_TOP]);

    GwyRange range;
    gwy_graph_area_get_xrange(priv->area, &range);
    gwy_axis_set_snap_to_ticks(firstaxis, FALSE);
    gwy_axis_set_snap_to_ticks(secondaxis, FALSE);
    gwy_axis_request_range(firstaxis, &range);
    gwy_axis_request_range(secondaxis, &range);
}

static void
set_fixed_yrange(GwyGraph *graph)
{
    Graph *priv = graph->priv;
    if (priv->yautorange)
        return;

    GwyAxis *firstaxis = GWY_AXIS(priv->axis[GTK_POS_LEFT]);
    GwyAxis *secondaxis = GWY_AXIS(priv->axis[GTK_POS_RIGHT]);

    GwyRange range;
    gwy_graph_area_get_yrange(priv->area, &range);
    gwy_axis_set_snap_to_ticks(firstaxis, FALSE);
    gwy_axis_set_snap_to_ticks(secondaxis, FALSE);
    gwy_axis_request_range(firstaxis, &range);
    gwy_axis_request_range(secondaxis, &range);
}

static void
xticks_placed(GwyGraph *graph, GwyAxis *axis)
{
    Graph *priv = graph->priv;
    GwyRange range;
    gwy_axis_get_range(axis, &range);
    gwy_graph_area_set_xrange(priv->area, &range);
    set_grid_from_ticks(axis, priv->area, gwy_graph_area_set_xgrid);
}

static void
yticks_placed(GwyGraph *graph, GwyAxis *axis)
{
    Graph *priv = graph->priv;
    GwyRange range;
    gwy_axis_get_range(axis, &range);
    gwy_graph_area_set_yrange(priv->area, &range);
    set_grid_from_ticks(axis, priv->area, gwy_graph_area_set_ygrid);
}

static void
set_grid_from_ticks(GwyAxis *axis,
                    GwyGraphArea *area,
                    SetGridFunc set_func)
{
    guint nticks;
    const GwyAxisTick *ticks = gwy_axis_ticks(axis, &nticks);
    guint n = 0;
    for (guint i = 0; i < nticks; i++) {
        if (ticks[i].level == GWY_AXIS_TICK_MAJOR)
            n++;
    }
    if (!n) {
        set_func(area, NULL, 0);
        return;
    }

    gdouble *grid = g_new(gdouble, n);
    n = 0;
    for (guint i = 0; i < nticks; i++) {
        if (ticks[i].level == GWY_AXIS_TICK_MAJOR)
            grid[n++] = ticks[i].value;
    }
    set_func(area, grid, n);
    g_free(grid);
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
