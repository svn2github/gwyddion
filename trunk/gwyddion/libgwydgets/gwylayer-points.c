/*
 *  @(#) $Id$
 *  Copyright (C) 2003,2004 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@physics.muni.cz, klapetek@physics.muni.cz.
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

#include <string.h>
#include <glib-object.h>

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/datafield.h>
#include "gwylayer-points.h"
#include "gwydataview.h"

#define GWY_LAYER_POINTS_TYPE_NAME "GwyLayerPoints"

#define PROXIMITY_DISTANCE 8
#define CROSS_SIZE 8

#define BITS_PER_SAMPLE 8

/* Forward declarations */

static void       gwy_layer_points_class_init      (GwyLayerPointsClass *klass);
static void       gwy_layer_points_init            (GwyLayerPoints *layer);
static void       gwy_layer_points_finalize        (GObject *object);
static void       gwy_layer_points_draw            (GwyVectorLayer *layer,
                                                    GdkDrawable *drawable);
static void       gwy_layer_points_draw_point      (GwyLayerPoints *layer,
                                                    GdkDrawable *drawable,
                                                    gint i);
static gboolean   gwy_layer_points_motion_notify   (GwyVectorLayer *layer,
                                                    GdkEventMotion *event);
static gboolean   gwy_layer_points_button_pressed  (GwyVectorLayer *layer,
                                                    GdkEventButton *event);
static gboolean   gwy_layer_points_button_released (GwyVectorLayer *layer,
                                                    GdkEventButton *event);
static void       gwy_layer_points_plugged         (GwyDataViewLayer *layer);
static void       gwy_layer_points_unplugged       (GwyDataViewLayer *layer);
static gint       gwy_layer_points_get_nselected   (GwyVectorLayer *layer);
static void       gwy_layer_points_unselect        (GwyVectorLayer *layer);
static void       gwy_layer_points_save            (GwyLayerPoints *layer,
                                                    gint i);
static void       gwy_layer_points_restore         (GwyLayerPoints *layer);

static gint       gwy_layer_points_near_point      (GwyLayerPoints *layer,
                                                    gdouble xreal,
                                                    gdouble yreal);

/* Local data */

static GtkObjectClass *parent_class = NULL;

GType
gwy_layer_points_get_type(void)
{
    static GType gwy_layer_points_type = 0;

    if (!gwy_layer_points_type) {
        static const GTypeInfo gwy_layer_points_info = {
            sizeof(GwyLayerPointsClass),
            NULL,
            NULL,
            (GClassInitFunc)gwy_layer_points_class_init,
            NULL,
            NULL,
            sizeof(GwyLayerPoints),
            0,
            (GInstanceInitFunc)gwy_layer_points_init,
            NULL,
        };
        gwy_debug("");
        gwy_layer_points_type
            = g_type_register_static(GWY_TYPE_VECTOR_LAYER,
                                     GWY_LAYER_POINTS_TYPE_NAME,
                                     &gwy_layer_points_info,
                                     0);
    }

    return gwy_layer_points_type;
}

static void
gwy_layer_points_class_init(GwyLayerPointsClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GwyDataViewLayerClass *layer_class = GWY_DATA_VIEW_LAYER_CLASS(klass);
    GwyVectorLayerClass *vector_class = GWY_VECTOR_LAYER_CLASS(klass);

    gwy_debug("");

    parent_class = g_type_class_peek_parent(klass);

    gobject_class->finalize = gwy_layer_points_finalize;

    layer_class->plugged = gwy_layer_points_plugged;
    layer_class->unplugged = gwy_layer_points_unplugged;

    vector_class->draw = gwy_layer_points_draw;
    vector_class->motion_notify = gwy_layer_points_motion_notify;
    vector_class->button_press = gwy_layer_points_button_pressed;
    vector_class->button_release = gwy_layer_points_button_released;
    vector_class->get_nselected = gwy_layer_points_get_nselected;
    vector_class->unselect = gwy_layer_points_unselect;

    klass->near_cursor = NULL;
    klass->move_cursor = NULL;
}

static void
gwy_layer_points_init(GwyLayerPoints *layer)
{
    GwyLayerPointsClass *klass;

    gwy_debug("");

    klass = GWY_LAYER_POINTS_GET_CLASS(layer);
    gwy_vector_layer_cursor_new_or_ref(&klass->near_cursor, GDK_FLEUR);
    gwy_vector_layer_cursor_new_or_ref(&klass->move_cursor, GDK_CROSS);

    layer->npoints = 3;
    layer->nselected = 0;
    layer->inear = -1;
    layer->points = g_new(gdouble, 2*layer->npoints);
}

static void
gwy_layer_points_finalize(GObject *object)
{
    GwyLayerPointsClass *klass;
    GwyLayerPoints *layer;

    gwy_debug("");

    g_return_if_fail(object != NULL);
    g_return_if_fail(GWY_IS_LAYER_POINTS(object));

    layer = (GwyLayerPoints*)object;
    klass = GWY_LAYER_POINTS_GET_CLASS(object);
    gwy_vector_layer_cursor_free_or_unref(&klass->near_cursor);
    gwy_vector_layer_cursor_free_or_unref(&klass->move_cursor);

    g_free(layer->points);

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

/**
 * gwy_layer_points_new:
 *
 * Creates a new point selection layer.
 *
 * The default number of points to select is three.
 *
 * Container keys: "/0/select/points/0/x", "/0/select/points/0/y",
 * "/0/select/points/1/x", "/0/select/points/1/y", etc.,
 * and "/0/select/points/nselected".
 *
 * Returns: The newly created layer.
 **/
GtkObject*
gwy_layer_points_new(void)
{
    GtkObject *object;

    gwy_debug("");
    object = g_object_new(GWY_TYPE_LAYER_POINTS, NULL);

    return object;
}

/**
 * gwy_layer_points_set_max_points:
 * @layer: A #GwyLayerPoints.
 * @npoints: The number of points to select.
 *
 * Sets the number of points to @npoints.
 *
 * This is the maximum number of points user can select and also the number of
 * points to be selected to emit the "finished" signal.
 **/
void
gwy_layer_points_set_max_points(GwyLayerPoints *layer,
                                gint npoints)
{
    g_return_if_fail(GWY_IS_LAYER_POINTS(layer));
    g_return_if_fail(npoints > 0 && npoints < 1024);

    layer->npoints = npoints;
    layer->nselected = MIN(layer->nselected, npoints);
    if (layer->inear >= npoints)
        layer->inear = -1;
    layer->points = g_renew(gdouble, layer->points, 2*layer->npoints);
}

/**
 * gwy_layer_points_get_max_points:
 * @layer: A #GwyLayerPoints.
 *
 * Returns the number of selection points for this layer.
 *
 * Returns: The number of points to select.
 **/
gint
gwy_layer_points_get_max_points(GwyLayerPoints *layer)
{
    g_return_val_if_fail(GWY_IS_LAYER_POINTS(layer), 0);

    return layer->npoints;
}

static void
gwy_layer_points_draw(GwyVectorLayer *layer,
                      GdkDrawable *drawable)
{
    GwyLayerPoints *points_layer;
    gint i;

    g_return_if_fail(GWY_IS_LAYER_POINTS(layer));
    g_return_if_fail(GDK_IS_DRAWABLE(drawable));

    points_layer = GWY_LAYER_POINTS(layer);
    for (i = 0; i < points_layer->nselected; i++)
        gwy_layer_points_draw_point(points_layer, drawable, i);
}

static void
gwy_layer_points_draw_point(GwyLayerPoints *layer,
                            GdkDrawable *drawable,
                            gint i)
{
    GwyDataView *data_view;
    GwyVectorLayer *vector_layer;
    gint xc, yc, xmin, xmax, ymin, ymax;

    g_return_if_fail(GWY_IS_LAYER_POINTS(layer));
    g_return_if_fail(GDK_IS_DRAWABLE(drawable));
    vector_layer = GWY_VECTOR_LAYER(layer);
    data_view = GWY_DATA_VIEW(GWY_DATA_VIEW_LAYER(layer)->parent);
    g_return_if_fail(i >= 0 && i < layer->nselected);

    if (!vector_layer->gc)
        gwy_vector_layer_setup_gc(vector_layer);

    gwy_data_view_coords_real_to_xy(data_view,
                                    layer->points[2*i],
                                    layer->points[2*i + 1],
                                    &xc, &yc);
    xmin = xc - CROSS_SIZE + 1;
    xmax = xc + CROSS_SIZE - 1;
    ymin = yc - CROSS_SIZE + 1;
    ymax = yc + CROSS_SIZE - 1;
    gwy_data_view_coords_xy_clamp(data_view, &xmin, &ymin);
    gwy_data_view_coords_xy_clamp(data_view, &xmax, &ymax);
    gdk_draw_line(drawable, vector_layer->gc, xmin, yc, xmax, yc);
    gdk_draw_line(drawable, vector_layer->gc, xc, ymin, xc, ymax);
}

static gboolean
gwy_layer_points_motion_notify(GwyVectorLayer *layer,
                               GdkEventMotion *event)
{
    GwyDataView *data_view;
    GwyLayerPointsClass *klass;
    GwyLayerPoints *points_layer;
    GdkWindow *window;
    gint x, y, i;
    gdouble xreal, yreal;

    data_view = GWY_DATA_VIEW(GWY_DATA_VIEW_LAYER(layer)->parent);
    window = GTK_WIDGET(data_view)->window;

    points_layer = GWY_LAYER_POINTS(layer);
    i = points_layer->inear;
    x = event->x;
    y = event->y;
    gwy_data_view_coords_xy_clamp(data_view, &x, &y);
    gwy_data_view_coords_xy_to_real(data_view, x, y, &xreal, &yreal);
    if (i > -1
        && xreal == points_layer->points[2*i]
        && yreal == points_layer->points[2*i + 1])
        return FALSE;

    if (!points_layer->button) {
        i = gwy_layer_points_near_point(points_layer, xreal, yreal);
        klass = GWY_LAYER_POINTS_GET_CLASS(points_layer);
        gdk_window_set_cursor(window, i == -1 ? NULL : klass->near_cursor);
        return FALSE;
    }

    g_assert(points_layer->inear != -1);
    points_layer->points[2*i] = xreal;
    points_layer->points[2*i + 1] = yreal;
    gwy_layer_points_save(points_layer, i);

    gwy_data_view_layer_updated(GWY_DATA_VIEW_LAYER(layer));

    return FALSE;
}

static gboolean
gwy_layer_points_button_pressed(GwyVectorLayer *layer,
                                GdkEventButton *event)
{
    GwyDataView *data_view;
    GdkWindow *window;
    GwyLayerPoints *points_layer;
    gint x, y, i;
    gdouble xreal, yreal;

    gwy_debug("");
    points_layer = GWY_LAYER_POINTS(layer);
    data_view = GWY_DATA_VIEW(GWY_DATA_VIEW_LAYER(layer)->parent);
    window = GTK_WIDGET(data_view)->window;

    x = event->x;
    y = event->y;
    gwy_data_view_coords_xy_clamp(data_view, &x, &y);
    gwy_debug("[%d,%d]", x, y);
    /* do nothing when we are outside */
    if (x != event->x || y != event->y)
        return FALSE;

    gwy_data_view_coords_xy_to_real(data_view, x, y, &xreal, &yreal);
    /* handle existing points */
    i = gwy_layer_points_near_point(points_layer, xreal, yreal);
    if (i >= 0) {
        points_layer->inear = i;
        gwy_layer_points_draw_point(points_layer, window, i);
    }
    else {
        /* add a point, or do nothing when maximum is reached */
        if (points_layer->nselected == points_layer->npoints)
            return FALSE;
        i = points_layer->inear = points_layer->nselected;
        points_layer->nselected++;
    }
    points_layer->button = event->button;
    points_layer->points[2*i] = xreal;
    points_layer->points[2*i + 1] = yreal;

    gdk_window_set_cursor(window,
                          GWY_LAYER_POINTS_GET_CLASS(layer)->move_cursor);

    return FALSE;
}

static gboolean
gwy_layer_points_button_released(GwyVectorLayer *layer,
                                 GdkEventButton *event)
{
    GwyDataView *data_view;
    GdkWindow *window;
    GwyLayerPointsClass *klass;
    GwyLayerPoints *points_layer;
    gint x, y, i;
    gdouble xreal, yreal;
    gboolean outside;

    points_layer = GWY_LAYER_POINTS(layer);
    if (!points_layer->button)
        return FALSE;
    data_view = GWY_DATA_VIEW(GWY_DATA_VIEW_LAYER(layer)->parent);
    window = GTK_WIDGET(data_view)->window;

    points_layer->button = 0;
    x = event->x;
    y = event->y;
    i = points_layer->inear;
    gwy_debug("i = %d", i);
    gwy_data_view_coords_xy_clamp(data_view, &x, &y);
    outside = (event->x != x) || (event->y != y);
    gwy_data_view_coords_xy_to_real(data_view, x, y, &xreal, &yreal);
    points_layer->points[2*i] = xreal;
    points_layer->points[2*i + 1] = yreal;
    gwy_layer_points_save(points_layer, i);
    gwy_layer_points_draw_point(points_layer, window, i);
    gwy_data_view_layer_updated(GWY_DATA_VIEW_LAYER(layer));
    if (points_layer->nselected == points_layer->npoints)
        gwy_vector_layer_selection_finished(layer);

    klass = GWY_LAYER_POINTS_GET_CLASS(points_layer);
    i = gwy_layer_points_near_point(points_layer, xreal, yreal);
    gdk_window_set_cursor(window,
                          (i == -1 || outside) ? NULL : klass->near_cursor);

    return FALSE;
}

/**
 * gwy_layer_points_get_points:
 * @layer: A #GwyLayerPoints.
 * @points: Where the point coordinates should be stored in, or NULL (to get
 *          only the number of selected points).
 *
 * Obtains the selected points.
 *
 * The @points array should be twice the size of
 * gwy_layer_points_get_max_points(), points are stored as x, y.
 * If less than gwy_layer_points_get_max_points() points are actually selected
 * the remaining items will not have meaningful values.
 *
 * Returns: The number of actually selected points.
 **/
gint
gwy_layer_points_get_points(GwyLayerPoints *layer,
                            gdouble *points)
{
    g_return_val_if_fail(GWY_IS_LAYER_POINTS(layer), 0);

    if (points && layer->nselected)
        memcpy(points, layer->points, 2*layer->nselected*sizeof(gdouble));

    return layer->nselected;
}

void
gwy_layer_points_unselect(GwyVectorLayer *layer)
{
    GwyLayerPoints *points_layer;
    GtkWidget *parent;

    g_return_if_fail(GWY_IS_LAYER_POINTS(layer));
    points_layer = GWY_LAYER_POINTS(layer);

    if (points_layer->nselected == 0)
        return;

    parent = GWY_DATA_VIEW_LAYER(layer)->parent;
    /* this is in fact undraw */
    if (parent)
        gwy_layer_points_draw(layer, parent->window);
    points_layer->nselected = 0;
    gwy_layer_points_save(points_layer, -1);
}

gint
gwy_layer_points_get_nselected(GwyVectorLayer *layer)
{
    g_return_val_if_fail(GWY_IS_LAYER_POINTS(layer), 0);
    return GWY_LAYER_POINTS(layer)->nselected;
}

static void
gwy_layer_points_plugged(GwyDataViewLayer *layer)
{
    GwyLayerPoints *points_layer;

    gwy_debug("");
    g_return_if_fail(GWY_IS_LAYER_POINTS(layer));
    points_layer = GWY_LAYER_POINTS(layer);

    points_layer->nselected = 0;
    GWY_DATA_VIEW_LAYER_CLASS(parent_class)->plugged(layer);
    gwy_layer_points_restore(points_layer);
    gwy_data_view_layer_updated(layer);
}

static void
gwy_layer_points_unplugged(GwyDataViewLayer *layer)
{
    gwy_debug("");
    g_return_if_fail(GWY_IS_LAYER_POINTS(layer));

    GWY_LAYER_POINTS(layer)->nselected = 0;
    GWY_DATA_VIEW_LAYER_CLASS(parent_class)->unplugged(layer);
}

static void
gwy_layer_points_save(GwyLayerPoints *layer,
                      gint i)
{
    GwyContainer *data;
    gchar key[64];
    gint from, to, n;

    data = GWY_DATA_VIEW_LAYER(layer)->data;
    /* TODO Container */
    gwy_container_set_int32_by_name(data, "/0/select/points/nselected",
                                    layer->nselected);
    if (i < 0) {
        from = 0;
        to = layer->nselected - 1;
    }
    else
        from = to = i;

    for (i = from; i <= to; i++) {
        gdouble *coords = layer->points + 2*i;

        n = g_snprintf(key, sizeof(key), "/0/select/points/%d/x", i);
        gwy_container_set_double_by_name(data, key, coords[0]);
        key[n-1] = 'y';
        gwy_container_set_double_by_name(data, key, coords[1]);
    }
}

static void
gwy_layer_points_restore(GwyLayerPoints *layer)
{
    GwyContainer *data;
    GwyDataField *dfield;
    gchar key[24];
    gdouble xreal, yreal;
    gint i, n, nsel;

    data = GWY_DATA_VIEW_LAYER(layer)->data;
    /* TODO Container */
    if (!gwy_container_contains_by_name(data, "/0/select/points/nselected"))
        return;

    nsel = gwy_container_get_int32_by_name(data, "/0/select/points/nselected");
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    xreal = gwy_data_field_get_xreal(dfield);
    yreal = gwy_data_field_get_yreal(dfield);
    for (i = layer->nselected = 0;
         i < nsel && layer->nselected < layer->npoints;
         i++) {
        gdouble *coords = layer->points + 2*layer->nselected;

        n = g_snprintf(key, sizeof(key), "/0/select/points/%d/x", i);
        coords[0] = gwy_container_get_double_by_name(data, key);
        key[n-1] = 'y';
        coords[1] = gwy_container_get_double_by_name(data, key);
        if (coords[0] >= 0.0 && coords[0] <= xreal
            && coords[1] >= 0.0 && coords[1] <= yreal)
            layer->nselected++;
    }
}

static gint
gwy_layer_points_near_point(GwyLayerPoints *layer,
                            gdouble xreal, gdouble yreal)
{
    GwyDataViewLayer *dlayer;
    gdouble d2min;
    gint i;

    if (!layer->nselected)
        return -1;

    i = gwy_math_find_nearest_point(xreal, yreal, &d2min,
                                    layer->nselected, layer->points);

    dlayer = (GwyDataViewLayer*)layer;
    /* FIXME: this is simply nonsense when x measure != y measure */
    d2min /= gwy_data_view_get_xmeasure(GWY_DATA_VIEW(dlayer->parent))
             *gwy_data_view_get_ymeasure(GWY_DATA_VIEW(dlayer->parent));

    if (d2min > PROXIMITY_DISTANCE*PROXIMITY_DISTANCE)
        return -1;
    return i;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
