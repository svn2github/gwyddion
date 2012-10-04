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
#include "libgwy/math.h"
#include "libgwy/coords-point.h"
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

    gint hover;
    gint clicked;
    gboolean has_moved;
    GdkModifierType modif;
    GwyXY xypress;    // Event (view) coordinates.
    GwyXY xyorig;     // Coords coordinates.
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
    priv->hover = priv->clicked = -1;
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
    const cairo_matrix_t *matrix = &shapes->coords_to_view;
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

    GwyShapes *shapes = GWY_SHAPES(points);
    GwyIntSet *selection = shapes->selection;
    gboolean hoverselected = FALSE;

    cairo_save(cr);
    cairo_set_line_width(cr, 1.683);
    for (gint i = 0; i < n; i++) {
        if (i != priv->hover && !gwy_int_set_contains(selection, i))
            gwy_cairo_cross(cr, data[2*i], data[2*i + 1], ticklen);
    }
    gwy_shapes_stroke(shapes, cr, GWY_SHAPES_STATE_NORMAL);

    if (gwy_int_set_size(selection)) {
        guint nsel;
        gint *selected = gwy_int_set_values(shapes->selection, &nsel);
        for (guint i = 0; i < nsel; i++) {
            gint j = selected[i];
            if (j == priv->hover)
                hoverselected = TRUE;
            else
                gwy_cairo_cross(cr, data[2*j], data[2*j + 1], ticklen);
        }
        g_free(selected);
        gwy_shapes_stroke(shapes, cr, GWY_SHAPES_STATE_SELECTED);
    }

    if (priv->hover != -1) {
        GwyShapesStateType state = GWY_SHAPES_STATE_PRELIGHT;
        gint i = priv->hover;
        if (hoverselected)
            state |= GWY_SHAPES_STATE_SELECTED;
        gwy_cairo_cross(cr, data[2*i], data[2*i + 1], ticklen);
        gwy_shapes_stroke(shapes, cr, state);
    }
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
    const cairo_matrix_t *matrix = &shapes->pixel_to_view;

    cairo_matrix_transform_distance(matrix, &xr, &yr);

    cairo_save(cr);
    cairo_set_line_width(cr, 1.0);
    for (gint i = 0; i < n; i++) {
        if (i != priv->hover && i != priv->clicked)
            gwy_cairo_ellipse(cr, data[2*i], data[2*i + 1], xr, yr);
    }
    gwy_shapes_stroke(shapes, cr, GWY_SHAPES_STATE_NORMAL);

    if (priv->clicked != -1) {
        gint i = priv->clicked;
        gwy_cairo_ellipse(cr, data[2*i], data[2*i + 1], xr, yr);
        gwy_shapes_stroke(shapes, cr, GWY_SHAPES_STATE_SELECTED);
    }
    else if (priv->hover != -1) {
        gint i = priv->hover;
        gwy_cairo_ellipse(cr, data[2*i], data[2*i + 1], xr, yr);
        gwy_shapes_stroke(shapes, cr, GWY_SHAPES_STATE_PRELIGHT);
    }
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
constrain_movement(GwyShapes *shapes,
                   gdouble eventx, gdouble eventy,
                   GdkModifierType modif,
                   GwyXY *dxy)
{
    GwyShapesPoint *points = GWY_SHAPES_POINT(shapes);
    ShapesPoint *priv = points->priv;

    // Constrain movement in view space, pressing Ctrl limits it to
    // horizontal/vertical.
    if (modif & GDK_CONTROL_MASK) {
        const GwyXY *xypress = &priv->xypress;
        gdouble xd = eventx - xypress->x, yd = eventy - xypress->y;
        if (fabs(xd) <= fabs(yd))
            eventx = xypress->x;
        else
            eventy = xypress->y;
    }

    // Constrain movement in coords space, cannot move anything outside the
    // bounding box.
    const cairo_matrix_t *matrix = &shapes->view_to_coords;
    cairo_matrix_transform_point(matrix, &eventx, &eventy);
    const GwyXY *xyorig = &priv->xyorig;
    gdouble xd = eventx - xyorig->x, yd = eventy - xyorig->y;
    guint nsel;
    gint *selected = gwy_int_set_values(shapes->selection, &nsel);
    const cairo_rectangle_t *bbox = &shapes->bounding_box;
    GwyCoords *coords = gwy_shapes_get_coords(shapes);
    gdouble xysel[2];
    for (guint i = 0; i < nsel; i++) {
        gwy_coords_get(coords, selected[i], xysel);
        xd = CLAMP(xd, bbox->x - xysel[0], bbox->x + bbox->width - xysel[0]);
        yd = CLAMP(yd, bbox->y - xysel[1], bbox->y + bbox->height - xysel[1]);
    }
    g_free(selected);

    gwy_coords_get(coords, priv->clicked, xysel);
    dxy->x = (xysel[0] - xyorig->x) + xd;
    dxy->y = (xysel[1] - xyorig->y) + yd;
}

static void
move_points(GwyShapes *shapes, const GwyXY *dxy)
{
    GwyCoords *coords = gwy_shapes_get_coords(shapes);
    guint nsel;
    gint *selected = gwy_int_set_values(shapes->selection, &nsel);
    for (guint i = 0; i < nsel; i++) {
        gdouble xysel[2];
        gwy_coords_get(coords, selected[i], xysel);
        xysel[0] += dxy->x;
        xysel[1] += dxy->y;
        gwy_coords_set(gwy_shapes_get_coords(shapes), selected[i], xysel);
    }
    g_free(selected);
    gwy_shapes_update(shapes);
}

static gboolean
add_point(GwyShapes *shapes,
          gdouble x, gdouble y)
{
    GwyShapesPoint *points = GWY_SHAPES_POINT(shapes);
    ShapesPoint *priv = points->priv;
    GwyCoords *coords = gwy_shapes_get_coords(shapes);
    guint n = gwy_coords_size(coords);
    if (n >= gwy_shapes_get_max_shapes(shapes))
        return FALSE;

    const cairo_matrix_t *matrix = &shapes->view_to_coords;
    cairo_matrix_transform_point(matrix, &x, &y);

    const cairo_rectangle_t *bbox = &shapes->bounding_box;
    if (CLAMP(x, bbox->x, bbox->x + bbox->width) != x
        || CLAMP(y, bbox->y, bbox->y + bbox->height) != y)
        return FALSE;

    gdouble xy[2] = { x, y };
    priv->hover = priv->clicked = n;
    gwy_coords_set(coords, priv->clicked, xy);
    return TRUE;
}

static void
update_hover(GwyShapes *shapes, gdouble eventx, gdouble eventy)
{
    GwyShapesPoint *points = GWY_SHAPES_POINT(shapes);
    ShapesPoint *priv = points->priv;

    gint i = find_near_point(points, eventx, eventy);
    if (priv->hover == i)
        return;

    priv->hover = i;
    gwy_shapes_update(shapes);
}

static gboolean
gwy_shapes_point_motion_notify(GwyShapes *shapes,
                               GdkEventMotion *event)
{
    GwyShapesPoint *points = GWY_SHAPES_POINT(shapes);
    ShapesPoint *priv = points->priv;
    if (!priv->data)
        return FALSE;

    if (priv->clicked == -1) {
        update_hover(shapes, event->x, event->y);
        return FALSE;
    }

    if (event->x != priv->xypress.x || event->y != priv->xypress.y)
        priv->has_moved = TRUE;

    GwyXY dxy;
    constrain_movement(shapes, event->x, event->y, event->state, &dxy);
    move_points(shapes, &dxy);

    return TRUE;
}

static gboolean
gwy_shapes_point_button_press(GwyShapes *shapes,
                              GdkEventButton *event)
{
    GwyShapesPoint *points = GWY_SHAPES_POINT(shapes);
    ShapesPoint *priv = points->priv;

    priv->xypress = (GwyXY){ event->x, event->y };
    if (priv->hover != -1) {
        if (priv->clicked != -1)
            g_warning("Button pressed while already moving a point.");
        priv->clicked = priv->hover;
    }
    else {
        if (!add_point(shapes, event->x, event->y)) {
            priv->clicked = -1;
            return FALSE;
        }
    }
    gwy_coords_get(gwy_shapes_get_coords(shapes),
                   priv->clicked, (gdouble*)&priv->xyorig);
    priv->has_moved = FALSE;
    // FIXME: If shift is pressed, we might want to start rubber-band selection
    // now.
    priv->modif = event->state;

    return FALSE;
}

static gboolean
gwy_shapes_point_button_release(GwyShapes *shapes,
                                GdkEventButton *event)
{
    GwyShapesPoint *points = GWY_SHAPES_POINT(shapes);
    ShapesPoint *priv = points->priv;
    if (priv->clicked == -1)
        return FALSE;

    if (!priv->has_moved) {
        GwyIntSet *selection = shapes->selection;

        if (priv->modif & GDK_SHIFT_MASK) {
            if (gwy_int_set_contains(selection, priv->clicked))
                gwy_int_set_remove(selection, priv->clicked);
            else
                gwy_int_set_add(selection, priv->clicked);
        }
        else {
            gwy_int_set_update(selection, &priv->clicked, 1);
        }

        // FIXME: May not be necessary if we respond to the selection signals.
        gwy_shapes_update(shapes);
    }
    else {
        GwyXY dxy;
        constrain_movement(shapes, event->x, event->y, event->state, &dxy);
        move_points(shapes, &dxy);
        gwy_coords_finished(gwy_shapes_get_coords(shapes));
    }
    priv->clicked = -1;
    update_hover(shapes, event->x, event->y);

    return TRUE;
}

static void
gwy_shapes_point_cancel_editing(GwyShapes *shapes,
                                gint id)
{
    GwyShapesPoint *points = GWY_SHAPES_POINT(shapes);
    ShapesPoint *priv = points->priv;
    if (priv->clicked == -1 || id != priv->clicked)
        return;

    priv->clicked = -1;
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
