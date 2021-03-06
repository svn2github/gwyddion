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

#include <math.h>
#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>
#include <stdio.h>

#include <libgwyddion/gwymacros.h>
#include "gwygrapher.h"
#include "gwygraphmodel.h"
#include "gwygraphcurvemodel.h"

#define GWY_GRAPHER_TYPE_NAME "GwyGrapher"


static void     gwy_grapher_class_init           (GwyGrapherClass *klass);
static void     gwy_grapher_init                 (GwyGrapher *grapher);
static void     gwy_grapher_size_request         (GtkWidget *widget,
                                                GtkRequisition *requisition);
static void     gwy_grapher_size_allocate        (GtkWidget *widget,
                                                GtkAllocation *allocation);
static void     rescaled_cb                    (GtkWidget *widget,
                                                GwyGrapher *grapher);
static void     replot_cb                        (GObject *gobject, 
                                                  GParamSpec *arg1, 
                                                  GwyGrapher *grapher);
static GtkWidgetClass *parent_class = NULL;


GType
gwy_grapher_get_type(void)
{
    static GType gwy_grapher_type = 0;
    if (!gwy_grapher_type) {
        static const GTypeInfo gwy_grapher_info = {
         sizeof(GwyGrapherClass),
         NULL,
         NULL,
         (GClassInitFunc)gwy_grapher_class_init,
         NULL,
         NULL,
         sizeof(GwyGrapher),
         0,
         (GInstanceInitFunc)gwy_grapher_init,
         NULL,
         };
        gwy_debug("");
        gwy_grapher_type = g_type_register_static (GTK_TYPE_TABLE,
                                                 GWY_GRAPHER_TYPE_NAME,
                                                 &gwy_grapher_info,
                                                 0);

    }

    return gwy_grapher_type;
}

static void
gwy_grapher_class_init(GwyGrapherClass *klass)
{
    GtkWidgetClass *widget_class;

    gwy_debug("");

    widget_class = (GtkWidgetClass*)klass;
    parent_class = g_type_class_peek_parent(klass);

    widget_class->size_request = gwy_grapher_size_request;
    widget_class->size_allocate = gwy_grapher_size_allocate;

}


static void
gwy_grapher_size_request(GtkWidget *widget, GtkRequisition *requisition)
{
    GTK_WIDGET_CLASS(parent_class)->size_request(widget, requisition);
    requisition->width = 300;
    requisition->height = 200;
}

static void
gwy_grapher_size_allocate(GtkWidget *widget, GtkAllocation *allocation)
{
    GwyGrapher *grapher;
    gwy_debug("");

    grapher = GWY_GRAPHER(widget);
    GTK_WIDGET_CLASS(parent_class)->size_allocate(widget, allocation);
}


static void
gwy_grapher_init(GwyGrapher *grapher)
{
    gwy_debug("");

    grapher->n_of_autocurves = 0;

    grapher->autoproperties.is_line = 1;
    grapher->autoproperties.is_point = 1;
    grapher->autoproperties.point_size = 8;
    grapher->autoproperties.line_size = 1;

    gtk_table_resize (GTK_TABLE (grapher), 3, 3);
    gtk_table_set_homogeneous (GTK_TABLE (grapher), FALSE);
    gtk_table_set_row_spacings (GTK_TABLE (grapher), 0);
    gtk_table_set_col_spacings (GTK_TABLE (grapher), 0);

    grapher->axis_top = GWY_AXISER(gwy_axiser_new(GWY_AXISER_SOUTH, 2.24, 5.21, "x"));
    grapher->axis_bottom = GWY_AXISER(gwy_axiser_new(GWY_AXISER_NORTH, 2.24, 5.21, "x"));
    grapher->axis_left = GWY_AXISER(gwy_axiser_new(GWY_AXISER_EAST, 100, 500, "y"));
    grapher->axis_right = GWY_AXISER(gwy_axiser_new(GWY_AXISER_WEST, 100, 500, "y"));

    g_signal_connect(grapher->axis_left, "rescaled", G_CALLBACK(rescaled_cb), grapher);
    g_signal_connect(grapher->axis_bottom, "rescaled", G_CALLBACK(rescaled_cb), grapher);
    

    gtk_table_attach(GTK_TABLE (grapher), GTK_WIDGET(grapher->axis_top), 1, 2, 0, 1,
                     GTK_FILL | GTK_EXPAND | GTK_SHRINK, GTK_FILL, 0, 0);
    gtk_table_attach(GTK_TABLE (grapher), GTK_WIDGET(grapher->axis_bottom), 1, 2, 2, 3,
                     GTK_FILL | GTK_EXPAND | GTK_SHRINK, GTK_FILL, 0, 0);
    gtk_table_attach(GTK_TABLE (grapher), GTK_WIDGET(grapher->axis_left), 2, 3, 1, 2,
                     GTK_FILL, GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 0);
    gtk_table_attach(GTK_TABLE (grapher), GTK_WIDGET(grapher->axis_right), 0, 1, 1, 2,
                     GTK_FILL, GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 0);
    gtk_widget_show(GTK_WIDGET(grapher->axis_top));
    gtk_widget_show(GTK_WIDGET(grapher->axis_bottom));
    gtk_widget_show(GTK_WIDGET(grapher->axis_left));
    gtk_widget_show(GTK_WIDGET(grapher->axis_right));

    grapher->corner_tl = GWY_GRAPHER_CORNER(gwy_grapher_corner_new());
    grapher->corner_bl = GWY_GRAPHER_CORNER(gwy_grapher_corner_new());
    grapher->corner_tr = GWY_GRAPHER_CORNER(gwy_grapher_corner_new());
    grapher->corner_br = GWY_GRAPHER_CORNER(gwy_grapher_corner_new());


    gtk_table_attach(GTK_TABLE (grapher), GTK_WIDGET(grapher->corner_tl), 0, 1, 0, 1,
                     GTK_FILL, GTK_FILL, 0, 0);
    gtk_table_attach(GTK_TABLE (grapher), GTK_WIDGET(grapher->corner_bl), 2, 3, 0, 1,
                     GTK_FILL, GTK_FILL , 0, 0);
    gtk_table_attach(GTK_TABLE (grapher), GTK_WIDGET(grapher->corner_tr), 0, 1, 2, 3,
                     GTK_FILL, GTK_FILL, 0, 0);
    gtk_table_attach(GTK_TABLE (grapher), GTK_WIDGET(grapher->corner_br), 2, 3, 2, 3,
                     GTK_FILL, GTK_FILL, 0, 0);

    gtk_widget_show(GTK_WIDGET(grapher->corner_tl));
    gtk_widget_show(GTK_WIDGET(grapher->corner_bl));
    gtk_widget_show(GTK_WIDGET(grapher->corner_tr));
    gtk_widget_show(GTK_WIDGET(grapher->corner_br));

    grapher->area = GWY_GRAPHER_AREA(gwy_grapher_area_new(NULL,NULL));

    grapher->area->status = GWY_GRAPHER_STATUS_PLAIN;

    gtk_table_attach(GTK_TABLE (grapher), GTK_WIDGET(grapher->area), 1, 2, 1, 2,
                     GTK_FILL | GTK_EXPAND | GTK_SHRINK, GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 0);

    gtk_widget_show_all(GTK_WIDGET(grapher->area));

}

GtkWidget*
gwy_grapher_new(GwyGraphModel *gmodel)
{
    GtkWidget *grapher = GTK_WIDGET(g_object_new(gwy_grapher_get_type(), NULL));
    gwy_debug("");

    if (gmodel != NULL)
       gwy_grapher_change_model(GWY_GRAPHER(grapher), gmodel);    
    return grapher;
}



/**
 * gwy_grapher_enable_axis_label_update:
 * @grapher: A grapher widget.
 * @enable: Enable or disable user to change label
 *
 * Enables/disables user to interact with grapher label by clickig on it and
 * changing text.
 *
 * Since: 1.3.
 **/
void
gwy_grapher_enable_axis_label_edit(GwyGrapher *grapher, gboolean enable)
{
    gwy_axiser_enable_label_edit(grapher->axis_top, enable);
    gwy_axiser_enable_label_edit(grapher->axis_bottom, enable);
    gwy_axiser_enable_label_edit(grapher->axis_left, enable);
    gwy_axiser_enable_label_edit(grapher->axis_right, enable);
}



/*refresh everything in graph according to the model: reset axis requirements,
 reset all points etc.*/
void       
gwy_grapher_refresh(GwyGrapher *grapher)
{
    GwyGraphModel *model;
    GwyGraphCurveModel *curvemodel;
    gdouble x_reqmin, x_reqmax, y_reqmin, y_reqmax;
    gint i, j;
    
    if (grapher->graph_model == NULL) return;
    model = GWY_GRAPH_MODEL(grapher->graph_model);

    if (model->ncurves > 0)
    {
    
        /*refresh axis and reset axis requirements*/
        x_reqmin = y_reqmin = G_MAXDOUBLE;
        x_reqmax = y_reqmax = -G_MAXDOUBLE;
        for (i=0; i<model->ncurves; i++)
        {
            curvemodel = GWY_GRAPH_CURVE_MODEL(model->curves[i]);
            for (j=0; j<curvemodel->n; j++)
            {
                if (x_reqmin > curvemodel->xdata[j]) x_reqmin = curvemodel->xdata[j];
                if (y_reqmin > curvemodel->ydata[j]) y_reqmin = curvemodel->ydata[j];
                if (x_reqmax < curvemodel->xdata[j]) x_reqmax = curvemodel->xdata[j];
                if (y_reqmax < curvemodel->ydata[j]) y_reqmax = curvemodel->ydata[j];
            }
        }
        gwy_axiser_set_req(grapher->axis_top, x_reqmin, x_reqmax);
        gwy_axiser_set_req(grapher->axis_bottom, x_reqmin, x_reqmax);
        gwy_axiser_set_req(grapher->axis_left, y_reqmin, y_reqmax);
        gwy_axiser_set_req(grapher->axis_right, y_reqmin, y_reqmax);

        model->x_max = gwy_axiser_get_maximum(grapher->axis_bottom);
        model->x_min = gwy_axiser_get_minimum(grapher->axis_bottom);
        model->y_max = gwy_axiser_get_maximum(grapher->axis_left);
        model->y_min = gwy_axiser_get_minimum(grapher->axis_left);
    }

    /*refresh widgets*/
    gwy_grapher_area_refresh(grapher->area);
    
}

static void 
replot_cb(GObject *gobject, GParamSpec *arg1, GwyGrapher *grapher)
{
    if (grapher == NULL || grapher->graph_model == NULL) return;
    gwy_grapher_refresh(grapher);
}

void
gwy_grapher_change_model(GwyGrapher *grapher, GwyGraphModel *gmodel)
{
    grapher->graph_model = gmodel;

    g_signal_connect(gmodel, "notify", G_CALLBACK(replot_cb), grapher);
    gwy_grapher_area_change_model(grapher->area, gmodel);
}

static void     
rescaled_cb(GtkWidget *widget, GwyGrapher *grapher)
{   
    GwyGraphModel *model;
    if (grapher->graph_model == NULL) return;
    model = GWY_GRAPH_MODEL(grapher->graph_model);
    model->x_max = gwy_axiser_get_maximum(grapher->axis_bottom);
    model->x_min = gwy_axiser_get_minimum(grapher->axis_bottom);
    model->y_max = gwy_axiser_get_maximum(grapher->axis_left);
    model->y_min = gwy_axiser_get_minimum(grapher->axis_left);

    gwy_grapher_area_refresh(grapher->area);
}


void
gwy_grapher_set_status(GwyGrapher *grapher, GwyGrapherStatusType status)
{
    grapher->area->status = status;
}

gint       
gwy_grapher_get_selection_number(GwyGrapher *grapher)
{
    if (grapher->area->status == GWY_GRAPHER_STATUS_XSEL)
        return grapher->area->areasdata->data_areas->len;
    else return 0;
}

void
gwy_grapher_get_selection(GwyGrapher *grapher, gdouble *selection)
{
    gint i;
    GwyGrapherDataArea *data_area;
    GwyGrapherDataPoint *data_point;
    
    if (selection == NULL) return;

    switch (grapher->area->status)
    {
        case GWY_GRAPHER_STATUS_CURSOR:
        selection[0] = grapher->area->cursordata->data_point.x;
        selection[0] = grapher->area->cursordata->data_point.y;
        break;
        
        case GWY_GRAPHER_STATUS_XSEL:    
        for (i = 0; i < grapher->area->areasdata->data_areas->len; i++)
        {
            data_area = &g_array_index(grapher->area->areasdata->data_areas, GwyGrapherDataArea, i);
            selection[2*i] = data_area->xmin;
            selection[2*i + 1] = data_area->xmax;
        }
        break;

        case GWY_GRAPHER_STATUS_YSEL:
        for (i = 0; i < grapher->area->areasdata->data_areas->len; i++)
        {
            data_area = &g_array_index(grapher->area->areasdata->data_areas, GwyGrapherDataArea, i);
            selection[2*i] = data_area->ymin;
            selection[2*i + 1] = data_area->ymax;
        }
        break;

        case GWY_GRAPHER_STATUS_POINTS:
        for (i = 0; i < grapher->area->pointsdata->data_points->len; i++)
        {
            data_point = &g_array_index(grapher->area->pointsdata->data_points, GwyGrapherDataPoint, i);
            selection[2*i] = data_point->x;
            selection[2*i + 1] = data_point->y;
        }
        break;

        default:
        g_assert_not_reached();   
    }
}

void       
gwy_grapher_clear_selection(GwyGrapher *grapher)
{
    gwy_grapher_area_clear_selection(grapher->area);
}



/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
