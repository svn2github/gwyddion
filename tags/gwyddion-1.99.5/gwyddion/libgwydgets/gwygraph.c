/*
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

#include "config.h"
#include <math.h>
#include <glib-object.h>
#include <gtk/gtk.h>

#include <libgwyddion/gwymacros.h>
#include <libgwydgets/gwygraph.h>
#include <libgwydgets/gwygraphmodel.h>
#include <libgwydgets/gwygraphcurvemodel.h>

#include <stdio.h>
enum {
    SELECTED_SIGNAL,
    ZOOMED_SIGNAL,
    LAST_SIGNAL
};


static void gwy_graph_refresh      (GwyGraph *graph);
static void gwy_graph_size_request (GtkWidget *widget,
                                    GtkRequisition *requisition);
static void gwy_graph_size_allocate(GtkWidget *widget,
                                    GtkAllocation *allocation);
static void rescaled_cb            (GtkWidget *widget,
                                    GwyGraph *graph);
static void replot_cb              (GObject *gobject,
                                    GParamSpec *arg1,
                                    GwyGraph *graph);
static void zoomed_cb              (GwyGraph *graph);
static void label_updated_cb       (GwyAxis *axis,
                                    GwyGraph *graph);
static void gwy_graph_finalize          (GObject *object);
static void gwy_graph_signal_zoomed     (GwyGraph *graph);



static guint gwygraph_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE(GwyGraph, gwy_graph, GTK_TYPE_TABLE)

static void
gwy_graph_class_init(GwyGraphClass *klass)
{
    GtkWidgetClass *widget_class;
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    widget_class = (GtkWidgetClass*)klass;

    widget_class->size_request = gwy_graph_size_request;
    widget_class->size_allocate = gwy_graph_size_allocate;
    gobject_class->finalize = gwy_graph_finalize;

    klass->selected = NULL;
    klass->zoomed = NULL;

    gwygraph_signals[SELECTED_SIGNAL]
                = g_signal_new("selected",
                               G_TYPE_FROM_CLASS(klass),
                               G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                               G_STRUCT_OFFSET(GwyGraphClass, selected),
                               NULL,
                               NULL,
                               g_cclosure_marshal_VOID__VOID,
                               G_TYPE_NONE, 0);

    gwygraph_signals[ZOOMED_SIGNAL]
                = g_signal_new("zoomed",
                               G_TYPE_FROM_CLASS(klass),
                               G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                               G_STRUCT_OFFSET(GwyGraphClass, zoomed),
                               NULL,
                               NULL,
                               g_cclosure_marshal_VOID__VOID,
                               G_TYPE_NONE, 0);
}


static void
gwy_graph_size_request(GtkWidget *widget, GtkRequisition *requisition)
{
    GTK_WIDGET_CLASS(gwy_graph_parent_class)->size_request(widget, requisition);
    requisition->width = 300;
    requisition->height = 200;
}

static void
gwy_graph_size_allocate(GtkWidget *widget, GtkAllocation *allocation)
{
    GwyGraph *graph;
    gwy_debug("");

    graph = GWY_GRAPH(widget);
    GTK_WIDGET_CLASS(gwy_graph_parent_class)->size_allocate(widget, allocation);
}


static void
gwy_graph_init(G_GNUC_UNUSED GwyGraph *graph)
{
    gwy_debug("");

}

static void
gwy_graph_finalize(GObject *object)
{
    GwyGraph *graph = GWY_GRAPH(object);

    if (graph->notify_id) {
        g_signal_handler_disconnect(graph->graph_model,
                                    graph->notify_id);
        graph->notify_id = 0;
    }
    if (graph->layout_updated_id) {
        g_signal_handler_disconnect(graph->graph_model,
                                    graph->layout_updated_id);
        graph->layout_updated_id = 0;
    }

    gwy_object_unref(graph->graph_model);
}


/**
 * gwy_graph_new:
 * @gmodel: A graph model.
 *
 * Creates graph widget based on information in model.
 *
 * Returns: new graph widget.
 **/
GtkWidget*
gwy_graph_new(GwyGraphModel *gmodel)
{
    GwyGraph *graph;

    gwy_debug("");

    graph = GWY_GRAPH(g_object_new(gwy_graph_get_type(), NULL));

    graph->area = GWY_GRAPH_AREA(gwy_graph_area_new(NULL, NULL));
    graph->area->status = GWY_GRAPH_STATUS_PLAIN;
    graph->enable_user_input = TRUE;

    if (gmodel)
        gwy_graph_set_model(GWY_GRAPH(graph), gmodel);

    gtk_table_resize(GTK_TABLE(graph), 3, 3);
    gtk_table_set_homogeneous(GTK_TABLE(graph), FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(graph), 0);
    gtk_table_set_col_spacings(GTK_TABLE(graph), 0);

    graph->grid_type = GWY_GRAPH_GRID_AUTO;

    if (gmodel != NULL) {
        graph->axis_top
            = GWY_AXIS(gwy_axis_new(GTK_POS_TOP, 2.24, 5.21,
                                    graph->graph_model->top_label->str));
        graph->axis_bottom
            = GWY_AXIS(gwy_axis_new(GTK_POS_BOTTOM, 2.24, 5.21,
                                    graph->graph_model->bottom_label->str));
        graph->axis_left
            = GWY_AXIS(gwy_axis_new(GTK_POS_LEFT, 100, 500,
                                    graph->graph_model->left_label->str));
        graph->axis_right
            = GWY_AXIS(gwy_axis_new(GTK_POS_RIGHT, 100, 500,
                                    graph->graph_model->right_label->str));
    }

    gwy_graph_set_axis_visible(graph, GTK_POS_LEFT, FALSE);
    gwy_graph_set_axis_visible(graph, GTK_POS_TOP, FALSE);

    g_signal_connect(graph->axis_left, "rescaled",
                     G_CALLBACK(rescaled_cb), graph);
    g_signal_connect(graph->axis_bottom, "rescaled",
                     G_CALLBACK(rescaled_cb), graph);

    g_signal_connect(graph->axis_left, "label-updated",
                     G_CALLBACK(label_updated_cb), graph);
    g_signal_connect(graph->axis_right, "label-updated",
                     G_CALLBACK(label_updated_cb), graph);
    g_signal_connect(graph->axis_top, "label-updated",
                     G_CALLBACK(label_updated_cb), graph);
    g_signal_connect(graph->axis_bottom, "label-updated",
                     G_CALLBACK(label_updated_cb), graph);


    gtk_table_attach(GTK_TABLE(graph), GTK_WIDGET(graph->axis_top),
                     1, 2, 0, 1,
                     GTK_FILL | GTK_EXPAND | GTK_SHRINK, GTK_FILL, 0, 0);
    gtk_table_attach(GTK_TABLE(graph), GTK_WIDGET(graph->axis_bottom),
                     1, 2, 2, 3,
                     GTK_FILL | GTK_EXPAND | GTK_SHRINK, GTK_FILL, 0, 0);
    gtk_table_attach(GTK_TABLE(graph), GTK_WIDGET(graph->axis_left),
                     2, 3, 1, 2,
                     GTK_FILL, GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 0);
    gtk_table_attach(GTK_TABLE(graph), GTK_WIDGET(graph->axis_right),
                     0, 1, 1, 2,
                     GTK_FILL, GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 0);
    gtk_widget_show(GTK_WIDGET(graph->axis_top));
    gtk_widget_show(GTK_WIDGET(graph->axis_bottom));
    gtk_widget_show(GTK_WIDGET(graph->axis_left));
    gtk_widget_show(GTK_WIDGET(graph->axis_right));

    graph->corner_tl = GWY_GRAPH_CORNER(gwy_graph_corner_new());
    graph->corner_bl = GWY_GRAPH_CORNER(gwy_graph_corner_new());
    graph->corner_tr = GWY_GRAPH_CORNER(gwy_graph_corner_new());
    graph->corner_br = GWY_GRAPH_CORNER(gwy_graph_corner_new());


    gtk_table_attach(GTK_TABLE(graph), GTK_WIDGET(graph->corner_tl), 0, 1, 0, 1,
                     GTK_FILL, GTK_FILL, 0, 0);
    gtk_table_attach(GTK_TABLE(graph), GTK_WIDGET(graph->corner_bl), 2, 3, 0, 1,
                     GTK_FILL, GTK_FILL, 0, 0);
    gtk_table_attach(GTK_TABLE(graph), GTK_WIDGET(graph->corner_tr), 0, 1, 2, 3,
                     GTK_FILL, GTK_FILL, 0, 0);
    gtk_table_attach(GTK_TABLE(graph), GTK_WIDGET(graph->corner_br), 2, 3, 2, 3,
                     GTK_FILL, GTK_FILL, 0, 0);

    gtk_widget_show(GTK_WIDGET(graph->corner_tl));
    gtk_widget_show(GTK_WIDGET(graph->corner_bl));
    gtk_widget_show(GTK_WIDGET(graph->corner_tr));
    gtk_widget_show(GTK_WIDGET(graph->corner_br));

    g_signal_connect_swapped(graph->area, "selected",
                     G_CALLBACK(gwy_graph_signal_selected), graph);

    g_signal_connect_swapped(graph->area, "zoomed",
                     G_CALLBACK(zoomed_cb), graph);

    gtk_table_attach(GTK_TABLE(graph), GTK_WIDGET(graph->area), 1, 2, 1, 2,
                     GTK_FILL | GTK_EXPAND | GTK_SHRINK,
                     GTK_FILL | GTK_EXPAND | GTK_SHRINK,
                     0, 0);

    gtk_widget_show_all(GTK_WIDGET(graph->area));

    gwy_graph_refresh(graph);

    return GTK_WIDGET(graph);
}

static void
gwy_graph_refresh(GwyGraph *graph)
{

    GwyGraphModel *model;
    GwyGraphCurveModel *curvemodel;
    gdouble x_reqmin, x_reqmax, y_reqmin, y_reqmax;
    gint i, j, nc, ndata;
    const gdouble *xdata, *ydata;
    gboolean has_data;


    if (graph->graph_model == NULL)
        return;
    model = GWY_GRAPH_MODEL(graph->graph_model);

    gwy_axis_set_logarithmic(graph->axis_left,   model->y_is_logarithmic);
    gwy_axis_set_logarithmic(graph->axis_right,  model->y_is_logarithmic);
    gwy_axis_set_logarithmic(graph->axis_top,    model->x_is_logarithmic);
    gwy_axis_set_logarithmic(graph->axis_bottom, model->x_is_logarithmic);

    gwy_axis_set_unit(graph->axis_top, model->x_unit);
    gwy_axis_set_unit(graph->axis_bottom, model->x_unit);
    gwy_axis_set_unit(graph->axis_left, model->y_unit);
    gwy_axis_set_unit(graph->axis_right, model->y_unit);
    nc = gwy_graph_model_get_n_curves(model);
    if (nc > 0) {
        /*refresh axis and reset axis requirements*/
        x_reqmin = y_reqmin = G_MAXDOUBLE;
        x_reqmax = y_reqmax = -G_MAXDOUBLE;
        has_data = FALSE;
        for (i = 0; i < nc; i++) {
            curvemodel = gwy_graph_model_get_curve_by_index(model, i);
            ndata = gwy_graph_curve_model_get_ndata(curvemodel);
            xdata = gwy_graph_curve_model_get_xdata(curvemodel);
            ydata = gwy_graph_curve_model_get_ydata(curvemodel);
            for (j = 0; j < ndata; j++) {
                if (x_reqmin > xdata[j])
                    x_reqmin = xdata[j];
                if (y_reqmin > ydata[j])
                    y_reqmin = ydata[j];
                if (x_reqmax < xdata[j])
                    x_reqmax = xdata[j];
                if (y_reqmax < ydata[j])
                    y_reqmax = ydata[j];
                has_data = TRUE;
            }
        }
        if (!has_data) {
            x_reqmin = y_reqmin = 0;
            x_reqmax = y_reqmax = 1;
        }
        gwy_axis_set_req(graph->axis_top, x_reqmin, x_reqmax);
        gwy_axis_set_req(graph->axis_bottom, x_reqmin, x_reqmax);
        gwy_axis_set_req(graph->axis_left, y_reqmin, y_reqmax);
        gwy_axis_set_req(graph->axis_right, y_reqmin, y_reqmax);

        model->x_max = gwy_axis_get_maximum(graph->axis_bottom);
        model->x_min = gwy_axis_get_minimum(graph->axis_bottom);
        model->y_max = gwy_axis_get_maximum(graph->axis_left);
        model->y_min = gwy_axis_get_minimum(graph->axis_left);
    }
    else {
        gwy_axis_set_req(graph->axis_top, 0, 1);
        gwy_axis_set_req(graph->axis_bottom, 0, 1);
        gwy_axis_set_req(graph->axis_left, 0, 1);
        gwy_axis_set_req(graph->axis_right, 0, 1);

        model->x_max = gwy_axis_get_maximum(graph->axis_bottom);
        model->x_min = gwy_axis_get_minimum(graph->axis_bottom);
        model->y_max = gwy_axis_get_maximum(graph->axis_left);
        model->y_min = gwy_axis_get_minimum(graph->axis_left);

    }


    /*refresh widgets*/
    gwy_graph_area_refresh(graph->area);
}

static void
replot_cb(G_GNUC_UNUSED GObject *gobject,
          G_GNUC_UNUSED GParamSpec *arg1,
          GwyGraph *graph)
{
    if (graph == NULL || graph->graph_model == NULL)
        return;
    gwy_graph_refresh(graph);
}

/**
 * gwy_graph_set_model:
 * @graph: A graph widget.
 * @gmodel: new graph model
 *
 * Changes the graph model.
 *
 * Everything in graph widgets will be reset to reflect the new data.
 **/
void
gwy_graph_set_model(GwyGraph *graph, GwyGraphModel *gmodel)
{
    if (graph->notify_id) {
        g_signal_handler_disconnect(graph->graph_model,
                                    graph->notify_id);
        graph->notify_id = 0;
    }
    if (graph->layout_updated_id) {
        g_signal_handler_disconnect(graph->graph_model,
                                    graph->layout_updated_id);
        graph->layout_updated_id = 0;
    }

    if (gmodel)
        g_object_ref(gmodel);
    gwy_object_unref(graph->graph_model);
    graph->graph_model = gmodel;

    if (gmodel) {
        graph->notify_id
            = g_signal_connect_swapped(gmodel, "notify",
                                       G_CALLBACK(gwy_graph_refresh), graph);
        graph->layout_updated_id
            = g_signal_connect_swapped(gmodel, "layout-updated",
                                       G_CALLBACK(gwy_graph_refresh), graph);
    }

    gwy_graph_area_set_model(graph->area, gmodel);
}

static void
rescaled_cb(G_GNUC_UNUSED GtkWidget *widget, GwyGraph *graph)
{
    GArray *array;
    GwyGraphModel *model;

    if (graph->graph_model == NULL)
        return;

    array = g_array_new(FALSE, FALSE, sizeof(gdouble));
    model = GWY_GRAPH_MODEL(graph->graph_model);
    model->x_max = gwy_axis_get_maximum(graph->axis_bottom);
    model->x_min = gwy_axis_get_minimum(graph->axis_bottom);
    model->y_max = gwy_axis_get_maximum(graph->axis_left);
    model->y_min = gwy_axis_get_minimum(graph->axis_left);

    if (graph->grid_type == GWY_GRAPH_GRID_AUTO) {
        gwy_axis_set_grid_data(graph->axis_left, array);
        gwy_graph_area_set_x_grid_data(graph->area, array);
        gwy_axis_set_grid_data(graph->axis_bottom, array);
        gwy_graph_area_set_y_grid_data(graph->area, array);

        g_array_free(array, TRUE);
    }

    gwy_graph_area_refresh(graph->area);
}

/**
 * gwy_graph_get_model:
 * @graph: A graph widget.
 *
 * Returns: Graph model associated with this graph widget (do not free).
 **/
GwyGraphModel*
gwy_graph_get_model(GwyGraph *graph)
{
    return  graph->graph_model;
}

/**
 * gwy_graph_get_axis:
 * @graph: A graph widget.
 * @type: Axis orientation
 *
 * Returns: the #GwyAxis (of given orientation) within @graph (do not free).
 **/
GwyAxis*
gwy_graph_get_axis(GwyGraph *graph, GtkPositionType type)
{
    switch (type) {
        case GTK_POS_TOP:
        return graph->axis_top;
        break;

        case GTK_POS_BOTTOM:
        return graph->axis_bottom;
        break;

        case GTK_POS_LEFT:
        return graph->axis_left;
        break;

        case GTK_POS_RIGHT:
        return graph->axis_right;
        break;
    }

    g_return_val_if_reached(NULL);
}

/**
 * gwy_graph_set_axis_visible:
 * @graph: A graph widget.
 * @type: Axis orientation
 * @is_visible: set/unset axis visibility within graph widget
 *
 * Sets the visibility of graph axis of given orientation.
 **/
void
gwy_graph_set_axis_visible(GwyGraph *graph,
                           GtkPositionType type,
                           gboolean is_visible)
{
    switch (type) {
        case GTK_POS_TOP:
        gwy_axis_set_visible(graph->axis_top, is_visible);
        break;

        case GTK_POS_BOTTOM:
        gwy_axis_set_visible(graph->axis_bottom, is_visible);
        break;

        case GTK_POS_LEFT:
        gwy_axis_set_visible(graph->axis_left, is_visible);
        break;

        case GTK_POS_RIGHT:
        gwy_axis_set_visible(graph->axis_right, is_visible);
        break;
    }
}

/**
 * gwy_graph_get_area:
 * @graph: A graph widget.
 *
 * Returns: the #GwyGraphArea within @graph (do not free).
 **/
GtkWidget*
gwy_graph_get_area(GwyGraph *graph)
{
    return GTK_WIDGET(graph->area);
}

/**
 * gwy_graph_set_status:
 * @graph: A graph widget.
 * @status: graph status
 *
 * Set status of the graph widget. Status determines how the graph
 * reacts on mouse events. This includes point or area selection and zooming.
 *
 **/
void
gwy_graph_set_status(GwyGraph *graph, GwyGraphStatusType status)
{
    graph->area->status = status;
}

/**
 * gwy_graph_get_status:
 * @graph: A graph widget.
 *
 * Get status of the graph widget.Status determines how the graph
 * reacts on mouse events. This includes point or area selection and zooming.
 *
 * Returns: graph status
 **/
GwyGraphStatusType
gwy_graph_get_status(GwyGraph *graph)
{
    return graph->area->status;
}

/*XXX: what are the units? */
/**
 * gwy_graph_request_x_range:
 * @graph: A graph widget.
 * @x_min_req: x minimum request
 * @x_max_req: x maximum request
 *
 * Ask graph to set the axis and area ranges to the requested values.
 * Note that the axis scales must have reasonably aligned ticks, therefore
 * the result might not exactly match the requested values.
 * Use gwy_graph_get_x_range() if you want to know the result.
 **/
void
gwy_graph_request_x_range(GwyGraph *graph,
                          gdouble x_min_req,
                          gdouble x_max_req)
{
    GwyGraphModel *model;

    if (graph->graph_model == NULL)
        return;
    model = GWY_GRAPH_MODEL(graph->graph_model);

    gwy_axis_set_req(graph->axis_top, x_min_req, x_max_req);
    gwy_axis_set_req(graph->axis_bottom, x_min_req, x_max_req);

    model->x_max = gwy_axis_get_maximum(graph->axis_bottom);
    model->x_min = gwy_axis_get_minimum(graph->axis_bottom);

    /*refresh widgets*/
    gwy_graph_area_refresh(graph->area);
 }

/**
 * gwy_graph_request_y_range:
 * @graph: A graph widget.
 * @y_min_req: y minimum request
 * @y_max_req: y maximum request
 *
 * Ask graph to set the axis and area ranges to the requested values.
 * Note that the axis scales must have reasonably aligned ticks, therefore
 * the result might not exactly match the requested values.
 * Use gwy_graph_get_y_range() if you want to know the result.
 **/
 void
gwy_graph_request_y_range(GwyGraph *graph,
                          gdouble y_min_req,
                          gdouble y_max_req)
{
    GwyGraphModel *model;

    if (graph->graph_model == NULL)
        return;
    model = GWY_GRAPH_MODEL(graph->graph_model);

    gwy_axis_set_req(graph->axis_left, y_min_req, y_max_req);
    gwy_axis_set_req(graph->axis_right, y_min_req, y_max_req);

    model->y_max = gwy_axis_get_maximum(graph->axis_left);
    model->y_min = gwy_axis_get_minimum(graph->axis_left);

     /*refresh widgets*/
    gwy_graph_area_refresh(graph->area);
 }

/**
 * gwy_graph_get_x_range:
 * @graph: A graph widget.
 * @x_min: x minimum
 * @x_max: x maximum
 *
 * Get the actual boudaries of graph area and axis in the x direction.
 **/
void
gwy_graph_get_x_range(GwyGraph *graph, gdouble *x_min, gdouble *x_max)
{
    *x_min = gwy_axis_get_minimum(graph->axis_bottom);
    *x_max = gwy_axis_get_maximum(graph->axis_bottom);
}

/**
 * gwy_graph_get_y_range:
 * @graph: A graph widget.
 * @y_min: y minimum
 * @y_max: y maximum
 *
 * Get the actual boudaries of graph area and axis in the y direction.
 **/
void
gwy_graph_get_y_range(GwyGraph *graph, gdouble *y_min, gdouble *y_max)
{
    *y_min = gwy_axis_get_minimum(graph->axis_left);
    *y_max = gwy_axis_get_maximum(graph->axis_left);
}

/**
 * gwy_graph_enable_user_input:
 * @graph: A graph widget.
 * @enable: whether to enable user input
 *
 * Enables/disables all the graph/curve settings dialogs to be invoked by mouse clicks.
 **/
void
gwy_graph_enable_user_input(GwyGraph *graph, gboolean enable)
{
    graph->enable_user_input = enable;
    gwy_graph_area_enable_user_input(graph->area, enable);
    gwy_axis_enable_label_edit(graph->axis_top, enable);
    gwy_axis_enable_label_edit(graph->axis_bottom, enable);
    gwy_axis_enable_label_edit(graph->axis_left, enable);
    gwy_axis_enable_label_edit(graph->axis_right, enable);
}

/*XXX: Does this need to be public? */
void
gwy_graph_signal_selected(GwyGraph *graph)
{
    g_signal_emit(G_OBJECT(graph), gwygraph_signals[SELECTED_SIGNAL], 0);
}

static void
gwy_graph_signal_zoomed(GwyGraph *graph)
{
    g_signal_emit(G_OBJECT(graph), gwygraph_signals[ZOOMED_SIGNAL], 0);
}

/**
 * gwy_graph_get_cursor:
 * @graph: A graph widget.
 * @x_cursor: x position of cursor
 * @y_cursor: y position of cursor
 *
 * Get the mouse pointer position within the graph area. Values are
 * in physical units corresponding to the graph axes.
 **/
void
gwy_graph_get_cursor(GwyGraph *graph, gdouble *x_cursor, gdouble *y_cursor)
{
    gwy_graph_area_get_cursor(graph->area, x_cursor, y_cursor);
}

/**
 * gwy_graph_zoom_in:
 * @graph: A graph widget.
 *
 * Switch to zoom status. Graph will expect zoom selection
 * and will zoom afterwards automatically.
 **/
void
gwy_graph_zoom_in(GwyGraph *graph)
{
    gwy_graph_set_status(graph, GWY_GRAPH_STATUS_ZOOM);
}

/**
 * gwy_graph_zoom_out:
 * @graph: A graph widget.
 *
 * Zoom out to see all the data points.
 **/
void
gwy_graph_zoom_out(GwyGraph *graph)
{
    gwy_graph_refresh(graph);
}

static void
zoomed_cb(GwyGraph *graph)
{
    gdouble x_reqmin, x_reqmax, y_reqmin, y_reqmax;
    gdouble selection[4];

    if (graph->area->status != GWY_GRAPH_STATUS_ZOOM)
        return;
    gwy_graph_area_get_selection(GWY_GRAPH_AREA(gwy_graph_get_area(graph)), selection);

    x_reqmin = selection[0];
    x_reqmax = selection[0] + selection[1];
    y_reqmin = selection[2];
    y_reqmax = selection[2] + selection[3];

    gwy_axis_set_req(graph->axis_top, x_reqmin, x_reqmax);
    gwy_axis_set_req(graph->axis_bottom, x_reqmin, x_reqmax);
    gwy_axis_set_req(graph->axis_left, y_reqmin, y_reqmax);
    gwy_axis_set_req(graph->axis_right, y_reqmin, y_reqmax);

    graph->graph_model->x_max = gwy_axis_get_maximum(graph->axis_bottom);
    graph->graph_model->x_min = gwy_axis_get_minimum(graph->axis_bottom);
    graph->graph_model->y_max = gwy_axis_get_maximum(graph->axis_left);
    graph->graph_model->y_min = gwy_axis_get_minimum(graph->axis_left);

    /*refresh widgets*/
    gwy_graph_set_status(graph, GWY_GRAPH_STATUS_PLAIN);
    gwy_graph_area_refresh(graph->area);
    gwy_graph_signal_zoomed(graph);
}

/* XXX What the f**k is this? XXX
 *
 * Why it does exactly the thing GString attempts to avoid?  Shouldn't it do:
 * g_string_assign(graph->graph_model->some_label, gwy_axis_set_label(axis));
 * Except that GwyGraphModel should have a *method* for setting axis labels. */
static void
label_updated_cb(GwyAxis *axis, GwyGraph *graph)
{
    switch (axis->orientation)
    {
        case GTK_POS_TOP:
        if (graph->graph_model->top_label)
            g_string_free(graph->graph_model->top_label, TRUE);
        graph->graph_model->top_label
            = g_string_new((gwy_axis_get_label(axis))->str);
        break;

        case GTK_POS_BOTTOM:
        if (graph->graph_model->bottom_label)
            g_string_free(graph->graph_model->bottom_label, TRUE);
        graph->graph_model->bottom_label
            = g_string_new((gwy_axis_get_label(axis))->str);
        break;

        case GTK_POS_LEFT:
        if (graph->graph_model->left_label)
            g_string_free(graph->graph_model->left_label, TRUE);
        graph->graph_model->left_label
            = g_string_new((gwy_axis_get_label(axis))->str);
        break;

        case GTK_POS_RIGHT:
        if (graph->graph_model->right_label)
            g_string_free(graph->graph_model->right_label, TRUE);
        graph->graph_model->right_label
            = g_string_new((gwy_axis_get_label(axis))->str);
        break;
    }
}

void
gwy_graph_set_grid_type(GwyGraph *graph, GwyGraphGridType grid_type)
{
    graph->grid_type = grid_type;
    gwy_graph_refresh(graph);
}

GwyGraphGridType
gwy_graph_get_grid_type(GwyGraph *graph)
{
    return graph->grid_type;
}

void
gwy_graph_set_x_grid_data(GwyGraph *graph, GArray *grid_data)
{
    gwy_graph_area_set_x_grid_data(graph->area, grid_data);
}

void
gwy_graph_set_y_grid_data(GwyGraph *graph, GArray *grid_data)
{
    gwy_graph_area_set_y_grid_data(graph->area, grid_data);
}

const GArray*
gwy_graph_get_x_grid_data(GwyGraph *graph)
{
    return gwy_graph_get_x_grid_data(graph->area);
}

const GArray*
gwy_graph_get_y_grid_data(GwyGraph *graph)
{
    return gwy_graph_get_y_grid_data(graph->area);
}

/************************** Documentation ****************************/

/**
 * SECTION:gwygraph
 * @title: GwyGraph
 * @short_description: Widget for displaying graphs
 *
 * #GwyGraph is a basic widget for displaying graphs.
 * It consists of several widgets that can also be used separately.
 * However, it is recomended (and it should be easy)
 * to use the whole #GwyGraph widget and its API for most purposes.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
