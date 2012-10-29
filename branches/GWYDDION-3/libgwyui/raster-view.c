/*
 *  $Id$
 *  Copyright (C) 2012 David Nečas (Yeti).
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

#include "libgwy/macros.h"
#include "libgwy/object-utils.h"
#include "libgwyui/raster-view.h"

enum {
    PROP_0,
    PROP_SCROLLER,
    PROP_AREA,
    PROP_HRULER,
    PROP_VRULER,
    PROP_HSCROLLBAR,
    PROP_VSCROLLBAR,
    PROP_COLOR_AXIS,
    N_PROPS,
};

struct _GwyRasterViewPrivate {
    GwyScroller *scroller;
    GwyRasterArea *area;
    GwyRuler *hruler;
    GtkScrollbar *hscrollbar;
    GwyRuler *vruler;
    GtkScrollbar *vscrollbar;
    GwyColorAxis *coloraxis;
};

typedef struct _GwyRasterViewPrivate RasterView;

static void     gwy_raster_view_set_property        (GObject *object,
                                                     guint prop_id,
                                                     const GValue *value,
                                                     GParamSpec *pspec);
static void     gwy_raster_view_get_property        (GObject *object,
                                                     guint prop_id,
                                                     GValue *value,
                                                     GParamSpec *pspec);

static GParamSpec *properties[N_PROPS];

G_DEFINE_TYPE(GwyRasterView, gwy_raster_view, GTK_TYPE_GRID);

static void
gwy_raster_view_class_init(GwyRasterViewClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    //GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

    g_type_class_add_private(klass, sizeof(RasterView));

    gobject_class->get_property = gwy_raster_view_get_property;
    gobject_class->set_property = gwy_raster_view_set_property;

    properties[PROP_SCROLLER]
        = g_param_spec_object("scroller",
                              "Scroller",
                              "Scroller widget within the raster view.",
                              GWY_TYPE_SCROLLER,
                              G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    properties[PROP_AREA]
        = g_param_spec_object("area",
                              "Area",
                              "Raster area widget within the raster view.",
                              GWY_TYPE_RASTER_AREA,
                              G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    properties[PROP_HRULER]
        = g_param_spec_object("hruler",
                              "Horizontal ruler",
                              "Horizontal ruler widget within the raster view.",
                              GWY_TYPE_RULER,
                              G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    properties[PROP_VRULER]
        = g_param_spec_object("vruler",
                              "Vertical ruler",
                              "Vertical ruler widget within the raster view.",
                              GWY_TYPE_RULER,
                              G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    properties[PROP_HSCROLLBAR]
        = g_param_spec_object("hscrollbar",
                              "Horizontal scrollbar",
                              "Horizontal scrollbar widget within the raster view.",
                              GTK_TYPE_SCROLLBAR,
                              G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    properties[PROP_VSCROLLBAR]
        = g_param_spec_object("vscrollbar",
                              "Vertical scrollbar",
                              "Vertical scrollbar widget within the raster view.",
                              GTK_TYPE_SCROLLBAR,
                              G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    properties[PROP_COLOR_AXIS]
        = g_param_spec_object("color-axis",
                              "Color axis",
                              "Colo raxis widget within the raster view.",
                              GWY_TYPE_COLOR_AXIS,
                              G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    for (guint i = 1; i < N_PROPS; i++)
        g_object_class_install_property(gobject_class, i, properties[i]);
}

static void
gwy_raster_view_init(GwyRasterView *rasterview)
{
    rasterview->priv = G_TYPE_INSTANCE_GET_PRIVATE(rasterview,
                                                   GWY_TYPE_RASTER_VIEW,
                                                   RasterView);
    RasterView *priv = rasterview->priv;

    GtkGrid *grid = GTK_GRID(rasterview);

    GtkWidget *scroller = gwy_scroller_new();
    priv->scroller = GWY_SCROLLER(scroller);
    gtk_grid_attach(grid, scroller, 2, 2, 1, 1);
    gtk_widget_show(scroller);

    GtkWidget *area = gwy_raster_area_new();
    priv->area = GWY_RASTER_AREA(area);
    gtk_container_add(GTK_CONTAINER(scroller), area);
    gtk_widget_show(area);

    GtkScrollable *scrollable = GTK_SCROLLABLE(area);

    GtkAdjustment *hadj = gtk_scrollable_get_hadjustment(scrollable);
    GtkWidget *hscrollbar = gtk_scrollbar_new(GTK_ORIENTATION_HORIZONTAL, hadj);
    priv->hscrollbar = GTK_SCROLLBAR(hscrollbar);
    gtk_grid_attach(grid, hscrollbar, 2, 0, 1, 1);
    gtk_widget_show(hscrollbar);

    GtkAdjustment *vadj = gtk_scrollable_get_vadjustment(scrollable);
    GtkWidget *vscrollbar = gtk_scrollbar_new(GTK_ORIENTATION_VERTICAL, vadj);
    priv->vscrollbar = GTK_SCROLLBAR(vscrollbar);
    gtk_grid_attach(grid, vscrollbar, 0, 2, 1, 1);
    gtk_widget_show(vscrollbar);

    GtkWidget *hruler = gwy_ruler_new();
    priv->hruler = GWY_RULER(hruler);
    g_object_set(hruler,
                 "edge", GTK_POS_TOP,
                 "max-tick-level", 3,
                 "show-mark", TRUE,
                 NULL);
    gtk_grid_attach(grid, hruler, 2, 1, 1, 1);
    gtk_widget_show(hruler);

    GtkWidget *vruler = gwy_ruler_new();
    priv->vruler = GWY_RULER(vruler);
    g_object_set(vruler,
                 "edge", GTK_POS_BOTTOM,
                 "max-tick-level", 3,
                 "show-mark", TRUE,
                 NULL);
    gtk_grid_attach(grid, vruler, 1, 2, 1, 1);
    gtk_widget_show(vruler);

    GtkWidget *coloraxis = gwy_color_axis_new();
    priv->coloraxis = GWY_COLOR_AXIS(coloraxis);
    g_object_set(coloraxis,
                 "edge", GTK_POS_RIGHT,
                 "max-tick-level", 2,
                 "ticks-at-edges", TRUE,
                 NULL);
    gtk_grid_attach(grid, coloraxis, 3, 2, 1, 1);
    gtk_widget_show(coloraxis);
}

static void
gwy_raster_view_set_property(G_GNUC_UNUSED GObject *object,
                             guint prop_id,
                             G_GNUC_UNUSED const GValue *value,
                             GParamSpec *pspec)
{
    //GwyRasterView *rasterview = GWY_RASTER_VIEW(object);

    switch (prop_id) {
        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_raster_view_get_property(GObject *object,
                             guint prop_id,
                             GValue *value,
                             GParamSpec *pspec)
{
    RasterView *priv = GWY_RASTER_VIEW(object)->priv;

    switch (prop_id) {
        case PROP_SCROLLER:
        g_value_set_object(value, priv->scroller);
        break;

        case PROP_AREA:
        g_value_set_object(value, priv->area);
        break;

        case PROP_HRULER:
        g_value_set_object(value, priv->hruler);
        break;

        case PROP_VRULER:
        g_value_set_object(value, priv->vruler);
        break;

        case PROP_HSCROLLBAR:
        g_value_set_object(value, priv->hscrollbar);
        break;

        case PROP_VSCROLLBAR:
        g_value_set_object(value, priv->vscrollbar);
        break;

        case PROP_COLOR_AXIS:
        g_value_set_object(value, priv->coloraxis);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

/**
 * gwy_raster_view_new:
 *
 * Creates a new raster view.
 *
 * Returns: A new raster view.
 **/
GtkWidget*
gwy_raster_view_new(void)
{
    return g_object_newv(GWY_TYPE_RASTER_VIEW, 0, NULL);
}

/**
 * gwy_raster_view_get_area:
 * @rasterview: A raster view.
 *
 * Gets the raster area widget used by a raster view.
 *
 * Returns: (transfer none):
 *          #GwyRasterArea widget used by the raster view.
 **/
GwyRasterArea*
gwy_raster_view_get_area(const GwyRasterView *rasterview)
{
    g_return_val_if_fail(GWY_IS_RASTER_VIEW(rasterview), NULL);
    return rasterview->priv->area;
}

/**
 * gwy_raster_view_get_scroller:
 * @rasterview: A raster view.
 *
 * Gets the scroller widget used by a raster view.
 *
 * Returns: (transfer none):
 *          #GwyScroller widget used by the raster view.
 **/
GwyScroller*
gwy_raster_view_get_scroller(const GwyRasterView *rasterview)
{
    g_return_val_if_fail(GWY_IS_RASTER_VIEW(rasterview), NULL);
    return rasterview->priv->scroller;
}

/**
 * gwy_raster_view_get_hruler:
 * @rasterview: A raster view.
 *
 * Gets the horizontal ruler widget used by a raster view.
 *
 * Returns: (transfer none):
 *          #GwyRuler widget used by the raster view as the horizontal ruler.
 **/
GwyRuler*
gwy_raster_view_get_hruler(const GwyRasterView *rasterview)
{
    g_return_val_if_fail(GWY_IS_RASTER_VIEW(rasterview), NULL);
    return rasterview->priv->hruler;
}

/**
 * gwy_raster_view_get_vruler:
 * @rasterview: A raster view.
 *
 * Gets the vertical ruler widget used by a raster view.
 *
 * Returns: (transfer none):
 *          #GwyRuler widget used by the raster view as the vertical ruler.
 **/
GwyRuler*
gwy_raster_view_get_vruler(const GwyRasterView *rasterview)
{
    g_return_val_if_fail(GWY_IS_RASTER_VIEW(rasterview), NULL);
    return rasterview->priv->vruler;
}

/**
 * gwy_raster_view_get_hscrollbar:
 * @rasterview: A raster view.
 *
 * Gets the horizontal scrollbar widget used by a raster view.
 *
 * Returns: (transfer none):
 *          #GtkScrollbar widget used by the raster view as the horizontal
 *          scrollbar.
 **/
GtkScrollbar*
gwy_raster_view_get_hscrollbar(const GwyRasterView *rasterview)
{
    g_return_val_if_fail(GWY_IS_RASTER_VIEW(rasterview), NULL);
    return rasterview->priv->hscrollbar;
}

/**
 * gwy_raster_view_get_vscrollbar:
 * @rasterview: A raster view.
 *
 * Gets the vertical scrollbar widget used by a raster view.
 *
 * Returns: (transfer none):
 *          #GtkScrollbar widget used by the raster view as the vertical
 *          scrollbar.
 **/
GtkScrollbar*
gwy_raster_view_get_vscrollbar(const GwyRasterView *rasterview)
{
    g_return_val_if_fail(GWY_IS_RASTER_VIEW(rasterview), NULL);
    return rasterview->priv->vscrollbar;
}

/**
 * gwy_raster_view_get_color_axis:
 * @rasterview: A raster view.
 *
 * Gets the color axis widget used by a raster view.
 *
 * Returns: (transfer none):
 *          #GwyColorAxis widget used by the raster view as the colour axis.
 **/
GwyColorAxis*
gwy_raster_view_get_color_axis(const GwyRasterView *rasterview)
{
    g_return_val_if_fail(GWY_IS_RASTER_VIEW(rasterview), NULL);
    return rasterview->priv->coloraxis;
}

/**
 * SECTION: raster-view
 * @section_id: GwyRasterView
 * @title: GwyRasterView
 * @short_description: Display fields with rulers and color axes
 **/

/**
 * GwyRasterView:
 *
 * Widget for raster display of two-dimensional data using false colour maps.
 *
 * The #GwyRasterView struct contains private data only and should be accessed
 * using the functions below.
 **/

/**
 * GwyRasterViewClass:
 *
 * Class of two-dimensional raster views.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
