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

#include <math.h>
#include <glib.h>
#include <glib/gi18n-lib.h>
#include "libgwy/macros.h"
#include "libgwy/object-utils.h"
#include "libgwy/coords-point.h"
#include "libgwy/rgba.h"
#include "libgwyui/utils.h"
#include "libgwyui/shapes-point.h"

enum {
    PROP_0,
    PROP_RADIUS,
    N_PROPS
};

typedef struct _GwyShapesPointPrivate ShapesPoint;

struct _GwyShapesPointPrivate {
    gboolean radius;
    // Cached data in view coordinates.  May not correspond to @coords if they
    // are changed simultaneously from more sources.
    GArray *data;

    gint active;
    gint moving;
    gdouble xyorig[2];
    GwyRGBA marker_color;
    GwyRGBA marker_highlight;
};

static void     gwy_shapes_point_finalize      (GObject *object);
static void     gwy_shapes_point_set_property  (GObject *object,
                                                guint prop_id,
                                                const GValue *value,
                                                GParamSpec *pspec);
static void     gwy_shapes_point_get_property  (GObject *object,
                                                guint prop_id,
                                                GValue *value,
                                                GParamSpec *pspec);
static gboolean set_radius                     (GwyShapesPoint *points,
                                                gdouble radius);
static void     calculate_data                 (GwyShapesPoint *points);
static void     gwy_shapes_point_draw          (GwyShapes *shapes,
                                                cairo_t *cr);
static gboolean gwy_shapes_point_motion_notify (GwyShapes *shapes,
                                                GdkEventMotion *event);
static gboolean gwy_shapes_point_button_press  (GwyShapes *shapes,
                                                GdkEventButton *event);
static gboolean gwy_shapes_point_button_release(GwyShapes *shapes,
                                                GdkEventButton *event);
static void     gwy_shapes_point_cancel_editing(GwyShapes *shapes,
                                                gint id);

static GParamSpec *properties[N_PROPS];

G_DEFINE_TYPE(GwyShapesPoint, gwy_shapes_point, GWY_TYPE_SHAPES);

static void
gwy_shapes_point_class_init(GwyShapesPointClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GwyShapesClass *shapes_class = GWY_SHAPES_CLASS(klass);

    g_type_class_add_private(klass, sizeof(ShapesPoint));

    gobject_class->finalize = gwy_shapes_point_finalize;
    gobject_class->get_property = gwy_shapes_point_get_property;
    gobject_class->set_property = gwy_shapes_point_set_property;

    shapes_class->coords_type = GWY_TYPE_COORDS_POINT;
    shapes_class->draw = gwy_shapes_point_draw;
    shapes_class->motion_notify = gwy_shapes_point_motion_notify;
    shapes_class->button_press = gwy_shapes_point_button_press;
    shapes_class->button_release = gwy_shapes_point_button_release;
    shapes_class->cancel_editing = gwy_shapes_point_cancel_editing;

    properties[PROP_RADIUS]
        = g_param_spec_double("radius",
                              "Radius",
                              "Radius of circle drawn around the marker.",
                              0, G_MAXDOUBLE, 0.0,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    for (guint i = 1; i < N_PROPS; i++)
        g_object_class_install_property(gobject_class, i, properties[i]);
}

static void
gwy_shapes_point_finalize(GObject *object)
{
    GwyShapesPoint *points = GWY_SHAPES_POINT(object);
    ShapesPoint *priv = points->priv;
    if (priv->data) {
        g_array_free(priv->data, TRUE);
        priv->data = NULL;
    }
}

static void
gwy_shapes_point_init(GwyShapesPoint *points)
{
    points->priv = G_TYPE_INSTANCE_GET_PRIVATE(points, GWY_TYPE_SHAPES_POINT,
                                               ShapesPoint);
    ShapesPoint *priv = points->priv;
    priv->marker_color = (GwyRGBA){ 0.9, 0.8, 0.3, 0.5 };
    priv->marker_highlight = (GwyRGBA){ 1.0, 0.9, 0.5, 0.8 };
    priv->active = priv->moving = -1;
}

static void
gwy_shapes_point_set_property(GObject *object,
                              guint prop_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
    GwyShapesPoint *points = GWY_SHAPES_POINT(object);

    switch (prop_id) {
        case PROP_RADIUS:
        set_radius(points, g_value_get_double(value));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_shapes_point_get_property(GObject *object,
                              guint prop_id,
                              GValue *value,
                              GParamSpec *pspec)
{
    ShapesPoint *priv = GWY_SHAPES_POINT(object)->priv;

    switch (prop_id) {
        case PROP_RADIUS:
        g_value_set_double(value, priv->radius);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static gboolean
set_radius(GwyShapesPoint *points,
           gdouble radius)
{
    ShapesPoint *priv = points->priv;
    if (radius == priv->radius)
        return FALSE;

    priv->radius = radius;
    gwy_shapes_update(GWY_SHAPES(points));
    return TRUE;
}

static void
calculate_data(GwyShapesPoint *points)
{
    ShapesPoint *priv = points->priv;
    GwyShapes *shapes = GWY_SHAPES(points);
    GwyCoords *coords = gwy_shapes_get_coords(shapes);
    guint n = gwy_coords_size(coords);
    if (!priv->data)
        priv->data = g_array_sized_new(FALSE, FALSE, sizeof(gdouble), 2*n);
    g_array_set_size(priv->data, 2*n);
    const cairo_matrix_t *matrix = gwy_shapes_get_coords_to_view_matrix(shapes);
    gdouble *data = (gdouble*)priv->data->data;
    gwy_coords_get_data(coords, data);
    for (guint i = 0; i < n; i++)
        cairo_matrix_transform_point(matrix, data + 2*i, data + 2*i + 1);
}

static void
draw_crosses(GwyShapesPoint *points, cairo_t *cr)
{
    ShapesPoint *priv = points->priv;
    const gdouble *data = (const gdouble*)priv->data->data;
    gint n = priv->data->len/2;
    gdouble ticklen = 5.0;

    if (!n)
        return;

    cairo_save(cr);
    cairo_set_line_width(cr, 1.683);
    gwy_cairo_set_source_rgba(cr, &priv->marker_color);
    for (gint i = 0; i < n; i++) {
        if (i != priv->active)
            gwy_cairo_cross(cr, data[2*i], data[2*i + 1], ticklen);
    }
    cairo_stroke(cr);

    if (priv->active != -1) {
        gint i = priv->active;
        gwy_cairo_set_source_rgba(cr, &priv->marker_highlight);
        gwy_cairo_cross(cr, data[2*i], data[2*i + 1], ticklen);
    }
    cairo_stroke(cr);
    cairo_restore(cr);
}

static void
draw_radii(GwyShapesPoint *points, cairo_t *cr)
{
    ShapesPoint *priv = points->priv;
    const gdouble *data = (const gdouble*)priv->data->data;
    gint n = priv->data->len/2;

    if (!priv->radius || !n)
        return;

    GwyShapes *shapes = GWY_SHAPES(points);
    gdouble xr = priv->radius, yr = priv->radius;
    const cairo_matrix_t *matrix = gwy_shapes_get_pixel_to_view_matrix(shapes);

    cairo_matrix_transform_distance(matrix, &xr, &yr);

    cairo_save(cr);
    cairo_set_line_width(cr, 0.683);
    gwy_cairo_set_source_rgba(cr, &priv->marker_color);
    for (gint i = 0; i < n; i++) {
        if (i != priv->active)
            gwy_cairo_ellipse(cr, data[2*i], data[2*i + 1], xr, yr);
    }
    cairo_stroke(cr);

    if (priv->active != -1) {
        gint i = priv->active;
        gwy_cairo_set_source_rgba(cr, &priv->marker_highlight);
        gwy_cairo_ellipse(cr, data[2*i], data[2*i + 1], xr, yr);
    }
    cairo_stroke(cr);
    cairo_restore(cr);
}

static void
gwy_shapes_point_draw(GwyShapes *shapes,
                      cairo_t *cr)
{
    GwyCoords *coords = gwy_shapes_get_coords(shapes);
    if (!coords || !gwy_coords_size(coords))
        return;

    GwyShapesPoint *points = GWY_SHAPES_POINT(shapes);
    calculate_data(points);
    draw_crosses(points, cr);
    draw_radii(points, cr);
}

static gint
find_near_point(GwyShapesPoint *points,
                gdouble x, gdouble y)
{
    ShapesPoint *priv = points->priv;
    guint n = priv->data->len/2;
    const gdouble *data = (const gdouble*)priv->data->data;
    gdouble mindist2 = G_MAXDOUBLE;
    gint mini = -1;

    for (guint i = 0; i < n; i++) {
        gdouble xd = x - data[2*i], yd = y - data[2*i + 1];
        gdouble dist2 = xd*xd + yd*yd;
        if (dist2 <= 30.0 && dist2 < mindist2) {
            mindist2 = dist2;
            mini = i;
        }
    }

    return mini;
}

static void
move_point(GwyShapes *shapes,
           gdouble x, gdouble y, GdkModifierType modif)
{
    const cairo_matrix_t *matrix = gwy_shapes_get_view_to_coords_matrix(shapes);
    cairo_matrix_transform_point(matrix, &x, &y);

    const cairo_rectangle_t *bbox = gwy_shapes_get_bounding_box(shapes);
    x = CLAMP(x, bbox->x, bbox->x + bbox->width);
    y = CLAMP(y, bbox->y, bbox->y + bbox->height);

    GwyShapesPoint *points = GWY_SHAPES_POINT(shapes);
    ShapesPoint *priv = points->priv;
    gdouble xy[2] = { x, y };

    if (modif & GDK_SHIFT_MASK) {
        const gdouble *xyorig = priv->xyorig;
        gdouble xd = xy[0] - xyorig[0], yd = xy[1] - xyorig[1];
        matrix = gwy_shapes_get_coords_to_view_matrix(shapes);
        cairo_matrix_transform_distance(matrix, &xd, &yd);
        if (fabs(xd) <= fabs(yd))
            xy[0] = xyorig[0];
        else
            xy[1] = xyorig[1];
    }

    gwy_coords_set(gwy_shapes_get_coords(shapes), priv->moving, xy);
    gwy_shapes_update(shapes);
}

static void
add_point(GwyShapes *shapes,
          gdouble x, gdouble y)
{
    GwyShapesPoint *points = GWY_SHAPES_POINT(shapes);
    ShapesPoint *priv = points->priv;
    GwyCoords *coords = gwy_shapes_get_coords(shapes);
    guint n = gwy_coords_size(coords);
    if (n >= gwy_shapes_get_max_shapes(shapes))
        return;

    const cairo_matrix_t *matrix = gwy_shapes_get_view_to_coords_matrix(shapes);
    cairo_matrix_transform_point(matrix, &x, &y);

    const cairo_rectangle_t *bbox = gwy_shapes_get_bounding_box(shapes);
    if (CLAMP(x, bbox->x, bbox->x + bbox->width) != x
        || CLAMP(y, bbox->y, bbox->y + bbox->height) != y)
        return;

    gdouble xy[2] = { x, y };
    priv->active = priv->moving = n;
    gwy_coords_set(coords, priv->moving, xy);
}

static gboolean
gwy_shapes_point_motion_notify(GwyShapes *shapes,
                               GdkEventMotion *event)
{
    GwyShapesPoint *points = GWY_SHAPES_POINT(shapes);
    ShapesPoint *priv = points->priv;
    if (!priv->data)
        return FALSE;

    if (priv->moving != -1) {
        move_point(shapes, event->x, event->y, event->state);
        return TRUE;
    }

    gint i = find_near_point(points, event->x, event->y);
    if (priv->active == i)
        return FALSE;

    priv->active = i;
    gwy_shapes_update(shapes);
    return FALSE;
}

static gboolean
gwy_shapes_point_button_press(GwyShapes *shapes,
                              GdkEventButton *event)
{
    GwyShapesPoint *points = GWY_SHAPES_POINT(shapes);
    ShapesPoint *priv = points->priv;
    if (priv->active != -1) {
        if (priv->moving != -1)
            g_warning("Button pressed while already moving a point.");
        priv->moving = priv->active;
        gwy_coords_get(gwy_shapes_get_coords(shapes),
                       priv->moving, priv->xyorig);
        gwy_shapes_set_focus(shapes, priv->moving);
        move_point(shapes, event->x, event->y, event->state);
    }
    else {
        add_point(shapes, event->x, event->y);
    }
    return FALSE;
}

static gboolean
gwy_shapes_point_button_release(GwyShapes *shapes,
                                GdkEventButton *event)
{
    GwyShapesPoint *points = GWY_SHAPES_POINT(shapes);
    ShapesPoint *priv = points->priv;
    if (priv->moving == -1)
        return FALSE;

    move_point(shapes, event->x, event->y, event->state);
    priv->moving = -1;
    priv->active = find_near_point(points, event->x, event->y);
    gwy_shapes_set_focus(shapes, priv->moving);
    gwy_coords_finished(gwy_shapes_get_coords(shapes));
    return TRUE;
}

static void
gwy_shapes_point_cancel_editing(GwyShapes *shapes,
                                gint id)
{
    GwyShapesPoint *points = GWY_SHAPES_POINT(shapes);
    ShapesPoint *priv = points->priv;
    if (priv->moving == -1 || id != priv->moving)
        return;

    priv->moving = -1;
    gwy_shapes_update(shapes);
}

/**
 * gwy_shapes_point_new:
 *
 * Creates a new group of selectable points.
 *
 * Return: A newly created group of selectable points.
 **/
GwyShapes*
gwy_shapes_point_new(void)
{
    return g_object_new(GWY_TYPE_SHAPES_POINT, 0, NULL);
}

/**
 * SECTION: shapes-point
 * @title: GwyShapesPoint
 * @short_description: Geometrical shapes – points.
 *
 * #GwyShapesPoint represents a group of selectable points with coordinates
 * given by a #GwyCoordsPoint object.
 **/

/**
 * GwyShapesPoint:
 *
 * Object representing a group of selectable points.
 *
 * The #GwyShapesPoint struct contains private data only and should be accessed
 * using the functions below.
 **/

/**
 * GwyShapesPointClass:
 *
 * Class of groups of selectable points.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
