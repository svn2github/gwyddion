/*
 *  $Id$
 *  Copyright (C) 2012-2013 David Nečas (Yeti).
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
#include <glib/gi18n-lib.h>
#include <gdk/gdkkeysyms.h>
#include "libgwy/macros.h"
#include "libgwy/object-utils.h"
#include "libgwy/coords-point.h"
#include "libgwyui/cairo-utils.h"
#include "libgwyui/shapes-point.h"

#define NEAR_DIST2 30.0

enum {
    PROP_0,
    PROP_RADIUS,
    N_PROPS
};

typedef enum {
    MODE_MOVING,
    MODE_SELECTING,
    MODE_RUBBERBAND,
} InteractMode;

typedef struct _GwyShapesPointPrivate ShapesPoint;

struct _GwyShapesPointPrivate {
    gdouble radius;
    gdouble ticklen;
    gdouble rx, ry;

    gint hover;
    gint clicked;
    guint selection_index;  // within the reduced set of orig coordinates
    InteractMode mode : 4;
};

static void     gwy_shapes_point_set_property      (GObject *object,
                                                    guint prop_id,
                                                    const GValue *value,
                                                    GParamSpec *pspec);
static void     gwy_shapes_point_get_property      (GObject *object,
                                                    guint prop_id,
                                                    GValue *value,
                                                    GParamSpec *pspec);
static void     gwy_shapes_point_draw              (GwyShapes *shapes,
                                                    cairo_t *cr);
static gboolean gwy_shapes_point_motion_notify     (GwyShapes *shapes,
                                                    GdkEventMotion *event);
static gboolean gwy_shapes_point_button_press      (GwyShapes *shapes,
                                                    GdkEventButton *event);
static gboolean gwy_shapes_point_button_release    (GwyShapes *shapes,
                                                    GdkEventButton *event);
static gboolean gwy_shapes_point_delete_selection  (GwyShapes *shapes);
static void     gwy_shapes_point_cancel_editing    (GwyShapes *shapes,
                                                    gint id);
static void     gwy_shapes_point_selection_added   (GwyShapes *shapes,
                                                    gint value);
static void     gwy_shapes_point_selection_removed (GwyShapes *shapes,
                                                    gint value);
static void     gwy_shapes_point_selection_assigned(GwyShapes *shapes);
static gboolean set_radius                         (GwyShapesPoint *points,
                                                    gdouble radius);
static void     draw_crosses                       (GwyShapes *shapes,
                                                    cairo_t *cr);
static void     draw_cross                         (GwyShapes *shapes,
                                                    cairo_t *cr,
                                                    const gdouble *xy);
static void     draw_radii                         (GwyShapes *shapes,
                                                    cairo_t *cr);
static void     draw_radius                        (GwyShapes *shapes,
                                                    cairo_t *cr,
                                                    const gdouble *xy);
static gint     find_near_point                    (GwyShapes *shapes,
                                                    gdouble x,
                                                    gdouble y);
static void     constrain_movement                 (GwyShapes *shapes,
                                                    GdkModifierType modif,
                                                    GwyXY *dxy);
static void     constrain_horiz_vert               (GwyShapes *shapes,
                                                    GwyXY *dxy);
static gboolean add_shape                          (GwyShapes *shapes,
                                                    gdouble x,
                                                    gdouble y);
static void     update_hover                       (GwyShapes *shapes,
                                                    gdouble eventx,
                                                    gdouble eventy);
static gboolean snap_point                         (GwyShapes *shapes,
                                                    gdouble *x,
                                                    gdouble *y);

static GParamSpec *properties[N_PROPS];

G_DEFINE_TYPE(GwyShapesPoint, gwy_shapes_point, GWY_TYPE_SHAPES);

static void
gwy_shapes_point_class_init(GwyShapesPointClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GwyShapesClass *shapes_class = GWY_SHAPES_CLASS(klass);

    g_type_class_add_private(klass, sizeof(ShapesPoint));

    gobject_class->get_property = gwy_shapes_point_get_property;
    gobject_class->set_property = gwy_shapes_point_set_property;

    shapes_class->coords_type = GWY_TYPE_COORDS_POINT;
    shapes_class->draw = gwy_shapes_point_draw;
    shapes_class->delete_selection = gwy_shapes_point_delete_selection;
    shapes_class->motion_notify = gwy_shapes_point_motion_notify;
    shapes_class->button_press = gwy_shapes_point_button_press;
    shapes_class->button_release = gwy_shapes_point_button_release;
    shapes_class->cancel_editing = gwy_shapes_point_cancel_editing;
    shapes_class->selection_added = gwy_shapes_point_selection_added;
    shapes_class->selection_removed = gwy_shapes_point_selection_removed;
    shapes_class->selection_assigned = gwy_shapes_point_selection_assigned;

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
gwy_shapes_point_init(GwyShapesPoint *points)
{
    points->priv = G_TYPE_INSTANCE_GET_PRIVATE(points, GWY_TYPE_SHAPES_POINT,
                                               ShapesPoint);
    ShapesPoint *priv = points->priv;
    priv->ticklen = 5.0;
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

static void
gwy_shapes_point_draw(GwyShapes *shapes,
                      cairo_t *cr)
{
    GwyCoords *coords = gwy_shapes_get_coords(shapes);
    if (!coords || !gwy_coords_size(coords))
        return;

    draw_crosses(shapes, cr);
    draw_radii(shapes, cr);
}

static gboolean
gwy_shapes_point_motion_notify(GwyShapes *shapes,
                               GdkEventMotion *event)
{
    ShapesPoint *priv = GWY_SHAPES_POINT(shapes)->priv;
    // FIXME: Change once we implement MODE_RUBBERBAND
    if (priv->mode == MODE_RUBBERBAND)
        return FALSE;

    if (priv->clicked == -1) {
        update_hover(shapes, event->x, event->y);
        return FALSE;
    }

    if (priv->mode == MODE_SELECTING) {
        priv->mode = MODE_RUBBERBAND;
        g_warning("We should start rubberband selection.  Implement it...");
        return FALSE;
    }

    g_assert(priv->mode == MODE_MOVING);
    GwyXY xy = { event->x, event->y }, dxy;
    if (!gwy_shapes_check_movement(shapes, &xy, &dxy))
        return FALSE;

    constrain_movement(shapes, event->state, &dxy);
    gwy_shapes_move(shapes, &dxy);

    return TRUE;
}

static gboolean
gwy_shapes_point_button_press(GwyShapes *shapes,
                              GdkEventButton *event)
{
    ShapesPoint *priv = GWY_SHAPES_POINT(shapes)->priv;
    GwyIntSet *selection = shapes->selection;
    gdouble x = event->x, y = event->y;

    if (event->state & GDK_SHIFT_MASK || !gwy_shapes_get_editable(shapes))
        priv->mode = MODE_SELECTING;
    else
        priv->mode = MODE_MOVING;

    // XXX: All the selection updates must be done in motion_notify or
    // button_release: only based on whether the pointer has moved we know
    // whether the user wants to select things or move them.
    update_hover(shapes, x, y);
    if (priv->hover != -1) {
        priv->clicked = priv->hover;
        // If we clicked on an already selected shape, we will move the entire
        // group.  If we clicked on an unselected shape we will need to select
        // only this one.
        if (priv->mode == MODE_MOVING
            && !gwy_int_set_contains(shapes->selection, priv->clicked)) {
            gwy_shapes_start_updating_selection(shapes);
            gwy_int_set_update(shapes->selection, &priv->clicked, 1);
            gwy_shapes_stop_updating_selection(shapes);
        }
    }
    else if (priv->mode == MODE_MOVING) {
        if (!add_shape(shapes, x, y)) {
            priv->clicked = -1;
            return FALSE;
        }
        gwy_shapes_start_updating_selection(shapes);
        gwy_int_set_update(selection, &priv->clicked, 1);
        gwy_shapes_stop_updating_selection(shapes);
    }
    // Cache the position of the snapping point within the selected subset.
    priv->selection_index = gwy_int_set_index(selection, priv->clicked);
    gwy_shapes_set_origin(shapes, &(GwyXY){ x, y });

    return FALSE;
}

static gboolean
gwy_shapes_point_button_release(GwyShapes *shapes,
                                GdkEventButton *event)
{
    ShapesPoint *priv = GWY_SHAPES_POINT(shapes)->priv;
    if (priv->clicked == -1) {
        update_hover(shapes, event->x, event->y);
        return FALSE;
    }

    if (!gwy_shapes_has_moved(shapes)) {
        GwyIntSet *selection = shapes->selection;

        gwy_shapes_start_updating_selection(shapes);
        if (priv->mode == MODE_SELECTING)
            gwy_int_set_toggle(selection, priv->clicked);
        else if (priv->mode == MODE_MOVING)
            gwy_int_set_update(selection, &priv->clicked, 1);
        gwy_shapes_stop_updating_selection(shapes);

        // FIXME: May not be necessary if we respond to the selection signals.
        gwy_shapes_update(shapes);
    }
    else {
        GwyXY xy = { event->x, event->y }, dxy;
        gwy_shapes_check_movement(shapes, &xy, &dxy);
        constrain_movement(shapes, event->state, &dxy);
        gwy_shapes_move(shapes, &dxy);
        gwy_coords_finished(gwy_shapes_get_coords(shapes));
    }
    gwy_shapes_unset_current_point(shapes);
    priv->clicked = -1;
    update_hover(shapes, event->x, event->y);

    return TRUE;
}

static gboolean
gwy_shapes_point_delete_selection(GwyShapes *shapes)
{
    GwyShapesClass *klass = GWY_SHAPES_CLASS(gwy_shapes_point_parent_class);
    if (klass->delete_selection(shapes)) {
        update_hover(shapes, NAN, NAN);
        return TRUE;
    }
    return FALSE;
}

static void
gwy_shapes_point_cancel_editing(GwyShapes *shapes,
                                G_GNUC_UNUSED gint id)
{
    ShapesPoint *priv = GWY_SHAPES_POINT(shapes)->priv;
    // FIXME: We might want to do something like the finishing touches at the
    // end of button_released() here.
    priv->clicked = -1;
}

// FIXME: May be common.
static void
gwy_shapes_point_selection_added(GwyShapes *shapes,
                                 G_GNUC_UNUSED gint value)
{
    if (gwy_shapes_is_updating_selection(shapes))
        return;
    // TODO
    gwy_shapes_point_cancel_editing(shapes, -1);
    gwy_shapes_update(shapes);
}

static void
gwy_shapes_point_selection_removed(GwyShapes *shapes,
                                   G_GNUC_UNUSED gint value)
{
    if (gwy_shapes_is_updating_selection(shapes))
        return;
    // TODO
    gwy_shapes_point_cancel_editing(shapes, -1);
    gwy_shapes_update(shapes);
}

static void
gwy_shapes_point_selection_assigned(GwyShapes *shapes)
{
    if (gwy_shapes_is_updating_selection(shapes))
        return;
    // TODO
    gwy_shapes_point_cancel_editing(shapes, -1);
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
draw_crosses(GwyShapes *shapes, cairo_t *cr)
{
    ShapesPoint *priv = GWY_SHAPES_POINT(shapes)->priv;
    if (!priv->ticklen)
        return;
    cairo_save(cr);
    cairo_set_line_width(cr, 1.683);
    gwy_shapes_draw_markers(shapes, cr, priv->hover, &draw_cross);
    cairo_restore(cr);
}

static void
draw_cross(GwyShapes *shapes,
           cairo_t *cr,
           const gdouble *xy)
{
    ShapesPoint *priv = GWY_SHAPES_POINT(shapes)->priv;
    gdouble x = xy[0], y = xy[1];
    cairo_matrix_transform_point(&shapes->coords_to_view, &x, &y);
    gwy_cairo_cross(cr, x, y, priv->ticklen);
}

static void
draw_radii(GwyShapes *shapes, cairo_t *cr)
{
    ShapesPoint *priv = GWY_SHAPES_POINT(shapes)->priv;
    if (!priv->radius)
        return;
    priv->rx = priv->ry = priv->radius;
    const cairo_matrix_t *matrix = &shapes->pixel_to_view;
    cairo_matrix_transform_distance(matrix, &priv->rx, &priv->ry);
    cairo_save(cr);
    cairo_set_line_width(cr, 1.0);
    gwy_shapes_draw_markers(shapes, cr, priv->hover, &draw_radius);
    cairo_restore(cr);
}

static void
draw_radius(GwyShapes *shapes,
            cairo_t *cr,
            const gdouble *xy)
{
    ShapesPoint *priv = GWY_SHAPES_POINT(shapes)->priv;
    gdouble x = xy[0], y = xy[1];
    cairo_matrix_transform_point(&shapes->coords_to_view, &x, &y);
    gwy_cairo_ellipse(cr, x, y, priv->rx, priv->ry);
}

static gint
find_near_point(GwyShapes *shapes,
                gdouble x, gdouble y)
{
    const cairo_matrix_t *matrix = &shapes->coords_to_view;
    GwyCoords *coords = gwy_shapes_get_coords(shapes);
    guint n = gwy_coords_size(coords);
    gdouble mindist2 = G_MAXDOUBLE;
    gint mini = -1;
    gdouble xy[2];

    for (guint i = 0; i < n; i++) {
        gwy_coords_get(coords, i, xy);
        cairo_matrix_transform_point(matrix, xy + 0, xy + 1);
        gdouble xd = x - xy[0];
        gdouble yd = y - xy[1];
        gdouble dist2 = xd*xd + yd*yd;
        if (dist2 <= NEAR_DIST2 && dist2 < mindist2) {
            mindist2 = dist2;
            mini = 2*i;
        }
    }

    return mini;
}

static void
constrain_movement(GwyShapes *shapes,
                   GdkModifierType modif,
                   GwyXY *dxy)
{
    // Constrain movement in view space, pressing Ctrl limits it to
    // horizontal/vertical.
    if (modif & GDK_CONTROL_MASK)
        constrain_horiz_vert(shapes, dxy);

    // Constrain final position in coords space, perform snapping and ensure
    // it does not move anything outside the bounding box.
    GwyShapesPoint *points = GWY_SHAPES_POINT(shapes);
    const GwyCoords *orig_coords = gwy_shapes_get_starting_coords(shapes);
    gdouble xy[2], diff[2];
    gwy_coords_get(orig_coords, points->priv->selection_index, xy);
    diff[0] = xy[0] + dxy->x;
    diff[1] = xy[1] + dxy->y;
    snap_point(shapes, diff+0, diff+1);
    diff[0] -= xy[0];
    diff[1] -= xy[1];

    const cairo_rectangle_t *bbox = &shapes->bounding_box;
    gdouble lower[2] = { bbox->x, bbox->y };
    gdouble upper[2] = { bbox->x + bbox->width, bbox->y + bbox->height };
    gwy_coords_constrain_translation(orig_coords, NULL, diff, lower, upper);
    dxy->x = diff[0];
    dxy->y = diff[1];
}

// FIXME: Common
static void
constrain_horiz_vert(GwyShapes *shapes, GwyXY *dxy)
{
    const cairo_matrix_t *matrix = &shapes->coords_to_view;
    gdouble x = dxy->x, y = dxy->y;
    cairo_matrix_transform_distance(matrix, &x, &y);
    if (fabs(x) <= fabs(y))
        dxy->x = 0.0;
    else
        dxy->y = 0.0;
}

static gboolean
add_shape(GwyShapes *shapes, gdouble x, gdouble y)
{
    ShapesPoint *priv = GWY_SHAPES_POINT(shapes)->priv;
    GwyCoords *coords = gwy_shapes_get_coords(shapes);
    guint n = gwy_coords_size(coords);
    if (n >= gwy_shapes_get_max_shapes(shapes))
        return FALSE;

    const cairo_matrix_t *matrix = &shapes->view_to_coords;
    cairo_matrix_transform_point(matrix, &x, &y);
    snap_point(shapes, &x, &y);

    const cairo_rectangle_t *bbox = &shapes->bounding_box;
    if (CLAMP(x, bbox->x, bbox->x + bbox->width) != x
        || CLAMP(y, bbox->y, bbox->y + bbox->height) != y)
        return FALSE;

    gwy_shapes_set_moved(shapes);
    gwy_shapes_editing_started(shapes);

    gdouble xy2[2] = { x, y };
    priv->hover = priv->clicked = n;
    gwy_coords_set(coords, priv->clicked, xy2);
    return TRUE;
}

static void
update_hover(GwyShapes *shapes, gdouble eventx, gdouble eventy)
{
    ShapesPoint *priv = GWY_SHAPES_POINT(shapes)->priv;
    gint i = -1;

    if (isfinite(eventx) && isfinite(eventy))
        i = find_near_point(shapes, eventx, eventy);
    if (priv->hover == i)
        return;

    GdkCursorType cursor_type = GDK_ARROW;
    if (i != -1 && gwy_shapes_get_selectable(shapes))
        cursor_type = GDK_FLEUR;
    gwy_shapes_set_cursor_type(shapes, cursor_type);

    priv->hover = i;
    gwy_shapes_update(shapes);
}

static gboolean
snap_point(GwyShapes *shapes,
           gdouble *x, gdouble *y)
{
    if (!gwy_shapes_get_snapping(shapes))
        return FALSE;

    cairo_matrix_transform_point(&shapes->coords_to_view, x, y);
    cairo_matrix_transform_point(&shapes->view_to_pixel, x, y);
    *x = gwy_round_to_half(*x);
    *y = gwy_round_to_half(*y);
    cairo_matrix_transform_point(&shapes->pixel_to_view, x, y);
    cairo_matrix_transform_point(&shapes->view_to_coords, x, y);
    return TRUE;
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
