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

#include <math.h>
#include <glib/gi18n-lib.h>
#include <gdk/gdkkeysyms.h>
#include "libgwy/macros.h"
#include "libgwy/object-utils.h"
#include "libgwy/coords-rectangle.h"
#include "libgwyui/cairo-utils.h"
#include "libgwyui/shapes-rectangle.h"
#include "libgwyui/shapes-internal.h"

#define XCOORD(corner) (2*((corner) & 1))
#define YCOORD(corner) (((corner) & 2) + 1)

enum {
    PROP_0,
    N_PROPS
};

typedef enum {
    MODE_MOVING,
    MODE_CORNER,
    MODE_SELECTING,
    MODE_RUBBERBAND,
} InteractMode;

typedef struct _GwyShapesRectanglePrivate ShapesRectangle;

/*
 * Corners are indexed as follows:
 * 0  1
 * 2  3
 *
 * This makes more sense in binary:
 * 00 01
 * 11 10
 * so the high bit chooses vertical line, the low bit horizontal line.  Use
 * XCOORD(corner), YCOORD(corner) to get the index in xy[] for X and Y
 * coordinate of aspecific corner.
 */
struct _GwyShapesRectanglePrivate {
    gint hover;
    gint clicked;
    guint selection_index;  // within the reduced set of orig coordinates
    InteractMode mode : 4;
    guint corner : 2;  // for CORNER; also determines snapping for MOVING
    gboolean entire_shape : 1;
    gboolean new_shape : 1;
};

static void     gwy_shapes_rectangle_set_property      (GObject *object,
                                                        guint prop_id,
                                                        const GValue *value,
                                                        GParamSpec *pspec);
static void     gwy_shapes_rectangle_get_property      (GObject *object,
                                                        guint prop_id,
                                                        GValue *value,
                                                        GParamSpec *pspec);
static void     gwy_shapes_rectangle_draw              (GwyShapes *shapes,
                                                        cairo_t *cr);
static gboolean gwy_shapes_rectangle_motion_notify     (GwyShapes *shapes,
                                                        GdkEventMotion *event);
static gboolean gwy_shapes_rectangle_button_press      (GwyShapes *shapes,
                                                        GdkEventButton *event);
static gboolean gwy_shapes_rectangle_button_release    (GwyShapes *shapes,
                                                        GdkEventButton *event);
static gboolean gwy_shapes_rectangle_delete_selection  (GwyShapes *shapes);
static void     gwy_shapes_rectangle_cancel_editing    (GwyShapes *shapes,
                                                        gint id);
static void     gwy_shapes_rectangle_selection_added   (GwyShapes *shapes,
                                                        gint value);
static void     gwy_shapes_rectangle_selection_removed (GwyShapes *shapes,
                                                        gint value);
static void     gwy_shapes_rectangle_selection_assigned(GwyShapes *shapes);
static void     draw_rectangles                        (GwyShapes *shapes,
                                                        cairo_t *cr);
static void     draw_rectangle                         (const GwyShapes *shapes,
                                                        cairo_t *cr,
                                                        const gdouble *xy);
static gint     find_near_corner                       (const GwyShapes *shapes,
                                                        gdouble x,
                                                        gdouble y);
static gint     find_near_rectangle                    (const GwyShapes *shapes,
                                                        gdouble x,
                                                        gdouble y);
static void     constrain_movement                     (GwyShapes *shapes,
                                                        GdkModifierType modif,
                                                        GwyXY *dxy);
static void     move_corner                            (GwyShapes *shapes,
                                                        GdkModifierType modif,
                                                        GwyXY *dxy);
static void     constrain_aspect_ratio                 (const GwyShapes *shapes,
                                                        guint corner,
                                                        gdouble *xy);
static void     limit_into_bbox                        (const cairo_rectangle_t *bbox,
                                                        guint corner,
                                                        gdouble *xy);
static void     sort_corners                           (gdouble *xy);
static gboolean add_shape                              (GwyShapes *shapes,
                                                        gdouble x,
                                                        gdouble y);
static void     update_hover                           (GwyShapes *shapes,
                                                        gdouble eventx,
                                                        gdouble eventy);

static GParamSpec *properties[N_PROPS];

G_DEFINE_TYPE(GwyShapesRectangle, gwy_shapes_rectangle, GWY_TYPE_SHAPES);

static void
gwy_shapes_rectangle_class_init(GwyShapesRectangleClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GwyShapesClass *shapes_class = GWY_SHAPES_CLASS(klass);

    g_type_class_add_private(klass, sizeof(ShapesRectangle));

    gobject_class->get_property = gwy_shapes_rectangle_get_property;
    gobject_class->set_property = gwy_shapes_rectangle_set_property;

    shapes_class->coords_type = GWY_TYPE_COORDS_RECTANGLE;
    shapes_class->draw = gwy_shapes_rectangle_draw;
    shapes_class->delete_selection = gwy_shapes_rectangle_delete_selection;
    shapes_class->motion_notify = gwy_shapes_rectangle_motion_notify;
    shapes_class->button_press = gwy_shapes_rectangle_button_press;
    shapes_class->button_release = gwy_shapes_rectangle_button_release;
    shapes_class->cancel_editing = gwy_shapes_rectangle_cancel_editing;
    shapes_class->selection_added = gwy_shapes_rectangle_selection_added;
    shapes_class->selection_removed = gwy_shapes_rectangle_selection_removed;
    shapes_class->selection_assigned = gwy_shapes_rectangle_selection_assigned;

    for (guint i = 1; i < N_PROPS; i++)
        g_object_class_install_property(gobject_class, i, properties[i]);
}

static void
gwy_shapes_rectangle_init(GwyShapesRectangle *rectangles)
{
    rectangles->priv = G_TYPE_INSTANCE_GET_PRIVATE(rectangles,
                                                   GWY_TYPE_SHAPES_RECTANGLE,
                                                   ShapesRectangle);
    ShapesRectangle *priv = rectangles->priv;
    priv->hover = priv->clicked = -1;
}

static void
gwy_shapes_rectangle_set_property(GObject *object,
                                  guint prop_id,
                                  const GValue *value,
                                  GParamSpec *pspec)
{
    GwyShapesRectangle *rectangles = GWY_SHAPES_RECTANGLE(object);

    switch (prop_id) {
        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_shapes_rectangle_get_property(GObject *object,
                                  guint prop_id,
                                  GValue *value,
                                  GParamSpec *pspec)
{
    ShapesRectangle *priv = GWY_SHAPES_RECTANGLE(object)->priv;

    switch (prop_id) {
        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_shapes_rectangle_draw(GwyShapes *shapes,
                          cairo_t *cr)
{
    const GwyCoords *coords = gwy_shapes_get_coords(shapes);
    if (!coords || !gwy_coords_size(coords))
        return;

    draw_rectangles(shapes, cr);
}

static gboolean
gwy_shapes_rectangle_motion_notify(GwyShapes *shapes,
                                   GdkEventMotion *event)
{
    ShapesRectangle *priv = GWY_SHAPES_RECTANGLE(shapes)->priv;
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

    GwyXY xy = { event->x, event->y }, dxy;
    if (!gwy_shapes_check_movement(shapes, &xy, &dxy))
        return FALSE;

    if (priv->mode == MODE_MOVING) {
        // Moving the entire rectangle.
        constrain_movement(shapes, event->state, &dxy);
        gwy_shapes_move(shapes, &dxy);
    }
    else if (priv->mode == MODE_CORNER) {
        // Moving a single corner.
        move_corner(shapes, event->state, &dxy);
    }
    else {
        g_assert_not_reached();
    }

    return TRUE;
}

static gboolean
gwy_shapes_rectangle_button_press(GwyShapes *shapes,
                                  GdkEventButton *event)
{
    ShapesRectangle *priv = GWY_SHAPES_RECTANGLE(shapes)->priv;
    GwyIntSet *selection = shapes->selection;
    gdouble x = event->x, y = event->y;

    if (event->state & GDK_SHIFT_MASK || !gwy_shapes_get_editable(shapes))
        priv->mode = MODE_SELECTING;
    else if (priv->entire_shape)
        priv->mode = MODE_MOVING;
    else
        priv->mode = MODE_CORNER;

    // XXX: All the selection updates must be done in motion_notify or
    // button_release: only based on whether the pointer has moved we know
    // whether the user wants to select things or move them.
    update_hover(shapes, x, y);
    priv->new_shape = FALSE;
    if (priv->hover != -1) {
        priv->clicked = priv->hover;
        // If we clicked on an already selected shape, we will move the entire
        // group.  If we clicked on an unselected shape we will need to select
        // only this one.
        if ((priv->mode == MODE_MOVING || priv->mode == MODE_CORNER)
            && !gwy_int_set_contains(shapes->selection, priv->clicked)) {
            gwy_shapes_start_updating_selection(shapes);
            gwy_int_set_update(shapes->selection, &priv->clicked, 1);
            gwy_shapes_stop_updating_selection(shapes);
        }
    }
    else if (priv->mode == MODE_MOVING || priv->mode == MODE_CORNER) {
        if (!add_shape(shapes, x, y)) {
            priv->clicked = -1;
            return FALSE;
        }
        gwy_shapes_start_updating_selection(shapes);
        gwy_int_set_update(selection, &priv->clicked, 1);
        gwy_shapes_stop_updating_selection(shapes);
    }
    gwy_shapes_set_origin(shapes, &(GwyXY){ x, y });

    return FALSE;
}

static gboolean
gwy_shapes_rectangle_button_release(GwyShapes *shapes,
                                    GdkEventButton *event)
{
    ShapesRectangle *priv = GWY_SHAPES_RECTANGLE(shapes)->priv;
    if (priv->clicked == -1) {
        update_hover(shapes, event->x, event->y);
        return FALSE;
    }

    gboolean emit_finished = TRUE;
    if (!gwy_shapes_has_moved(shapes)) {
        GwyIntSet *selection = shapes->selection;

        emit_finished = priv->new_shape;
        gwy_shapes_start_updating_selection(shapes);
        if (priv->mode == MODE_SELECTING)
            gwy_int_set_toggle(selection, priv->clicked);
        else if (priv->mode == MODE_MOVING)
            gwy_int_set_update(selection, &priv->clicked, 1);
        else if (priv->mode == MODE_CORNER && !priv->new_shape)
            gwy_int_set_update(selection, &priv->clicked, 1);
        gwy_shapes_stop_updating_selection(shapes);

        if (priv->mode == MODE_CORNER && priv->new_shape)
            gwy_coords_delete(gwy_shapes_get_coords(shapes), priv->clicked);

        // FIXME: May not be necessary if we respond to the selection signals.
        gwy_shapes_update(shapes);
    }
    else if (priv->mode == MODE_MOVING) {
        GwyXY xy = { event->x, event->y }, dxy;
        gwy_shapes_check_movement(shapes, &xy, &dxy);
        constrain_movement(shapes, event->state, &dxy);
        gwy_shapes_move(shapes, &dxy);
    }
    else if (priv->mode == MODE_CORNER) {
        GwyXY xy = { event->x, event->y }, dxy;
        gwy_shapes_check_movement(shapes, &xy, &dxy);
        move_corner(shapes, event->state, &dxy);
        _gwy_shapes_remove_null_box(shapes, priv->clicked);
    }

    if (emit_finished)
        gwy_coords_finished(gwy_shapes_get_coords(shapes));

    gwy_shapes_unset_current_point(shapes);
    priv->clicked = -1;
    update_hover(shapes, event->x, event->y);

    return TRUE;
}

static gboolean
gwy_shapes_rectangle_delete_selection(GwyShapes *shapes)
{
    GwyShapesClass *klass = GWY_SHAPES_CLASS(gwy_shapes_rectangle_parent_class);
    if (klass->delete_selection(shapes)) {
        update_hover(shapes, NAN, NAN);
        return TRUE;
    }
    return FALSE;
}

static void
gwy_shapes_rectangle_cancel_editing(GwyShapes *shapes,
                                    G_GNUC_UNUSED gint id)
{
    ShapesRectangle *priv = GWY_SHAPES_RECTANGLE(shapes)->priv;
    // FIXME: We might want to do something like the finishing touches at the
    // end of button_released() here.
    priv->clicked = -1;
}

// FIXME: May be common.
static void
gwy_shapes_rectangle_selection_added(GwyShapes *shapes,
                                     G_GNUC_UNUSED gint value)
{
    if (gwy_shapes_is_updating_selection(shapes))
        return;
    // TODO
    gwy_shapes_rectangle_cancel_editing(shapes, -1);
    gwy_shapes_update(shapes);
}

static void
gwy_shapes_rectangle_selection_removed(GwyShapes *shapes,
                                       G_GNUC_UNUSED gint value)
{
    if (gwy_shapes_is_updating_selection(shapes))
        return;
    // TODO
    gwy_shapes_rectangle_cancel_editing(shapes, -1);
    gwy_shapes_update(shapes);
}

static void
gwy_shapes_rectangle_selection_assigned(GwyShapes *shapes)
{
    if (gwy_shapes_is_updating_selection(shapes))
        return;
    // TODO
    gwy_shapes_rectangle_cancel_editing(shapes, -1);
    gwy_shapes_update(shapes);
}

/**
 * gwy_shapes_rectangle_new:
 *
 * Creates a new group of selectable rectangles.
 *
 * Return: A newly created group of selectable rectangles.
 **/
GwyShapes*
gwy_shapes_rectangle_new(void)
{
    return g_object_new(GWY_TYPE_SHAPES_RECTANGLE, 0, NULL);
}

static void
draw_rectangles(GwyShapes *shapes, cairo_t *cr)
{
    cairo_save(cr);
    cairo_set_line_width(cr, 1.0);
    ShapesRectangle *priv = GWY_SHAPES_RECTANGLE(shapes)->priv;
    gwy_shapes_draw_markers(shapes, cr, priv->hover, &draw_rectangle);
    cairo_restore(cr);
}

static void
draw_rectangle(const GwyShapes *shapes,
               cairo_t *cr,
               const gdouble *xy)
{
    const cairo_matrix_t *matrix = &shapes->coords_to_view;
    gdouble xf = xy[0], yf = xy[1], xt = xy[2], yt = xy[3];
    cairo_matrix_transform_point(matrix, &xf, &yf);
    cairo_matrix_transform_point(matrix, &xt, &yt);
    cairo_move_to(cr, xf, yf);
    cairo_line_to(cr, xf, yt);
    cairo_line_to(cr, xt, yt);
    cairo_line_to(cr, xt, yf);
    cairo_close_path(cr);
}

// FIXME: May be common.
// FIXME: The same as in ShapesPoint, just for both all corners.
/* returns the index of corner, not rectangle */
static gint
find_near_corner(const GwyShapes *shapes,
                 gdouble x, gdouble y)
{
    // Primary corners first to prefer them in case of equality.
    static const guint corners[] = { 0, 3, 1, 2 };
    const cairo_matrix_t *matrix = &shapes->coords_to_view;
    const GwyCoords *coords = gwy_shapes_get_coords(shapes);
    guint n = gwy_coords_size(coords);
    gdouble mindist2 = G_MAXDOUBLE;
    gint mini = -1;

    for (guint i = 0; i < n; i++) {
        gdouble xy[4];
        gwy_coords_get(coords, i, xy);
        cairo_matrix_transform_point(matrix, xy + 0, xy + 1);
        cairo_matrix_transform_point(matrix, xy + 2, xy + 3);
        for (guint j = 0; j < G_N_ELEMENTS(corners); j++) {
            guint corner = corners[j];
            gdouble xd = x - xy[XCOORD(corner)];
            gdouble yd = y - xy[YCOORD(corner)];
            gdouble dist2 = xd*xd + yd*yd;
            if (dist2 <= NEAR_DIST2 && dist2 < mindist2) {
                mindist2 = dist2;
                mini = 4*i + corner;
            }
        }
    }

    return mini;
}

/* returns the index of corner, not rectangle */
static gint
find_near_rectangle(const GwyShapes *shapes,
                    gdouble x, gdouble y)
{
    const cairo_matrix_t *matrix = &shapes->coords_to_view;
    const GwyCoords *coords = gwy_shapes_get_coords(shapes);
    guint n = gwy_coords_size(coords);
    gdouble mindist2 = G_MAXDOUBLE;
    gint mini = -1;

    for (guint i = 0; i < n; i++) {
        gdouble xy[4];
        gwy_coords_get(coords, i, xy);
        cairo_matrix_transform_point(matrix, xy + 0, xy + 1);
        cairo_matrix_transform_point(matrix, xy + 2, xy + 3);
        guint other;
        for (guint j = 0, corner = 0; j < 4; corner = other, j++) {
            guint endpoint;
            other = corner ^ (1 << (j % 2));
            gdouble dist2 = gwy_line_point_distance2(xy[XCOORD(corner)],
                                                     xy[YCOORD(corner)],
                                                     xy[XCOORD(other)],
                                                     xy[YCOORD(other)],
                                                     x, y, &endpoint);
            if (dist2 <= NEAR_DIST2 && dist2 < mindist2) {
                mindist2 = dist2;
                mini = 4*i + (endpoint ? other : corner);
            }
        }
    }

    return mini;
}

// FIXME: This is suboptimal.  We should snap @clicked rectangle as the last
// thing as long as it does not make other selected lies fall outside the bbox.
static void
constrain_movement(GwyShapes *shapes,
                   GdkModifierType modif,
                   GwyXY *dxy)
{
    // Constrain movement in view space, pressing Ctrl limits it to
    // horizontal/vertical.
    if (modif & GDK_CONTROL_MASK)
        _gwy_shapes_constrain_horiz_vert(shapes, dxy);

    // Constrain final position in coords space, perform snapping and ensure
    // it does not move anything outside the bounding box.
    GwyShapesRectangle *rectangles = GWY_SHAPES_RECTANGLE(shapes);
    ShapesRectangle *priv = rectangles->priv;
    guint corner = priv->corner;
    const GwyCoords *orig_coords = gwy_shapes_get_starting_coords(shapes);
    gdouble xy[4], diff[2];
    gwy_coords_get(orig_coords, priv->selection_index, xy);
    diff[0] = xy[XCOORD(corner)] + dxy->x;
    diff[1] = xy[YCOORD(corner)] + dxy->y;
    _gwy_shapes_snap_to_pixel_corner(shapes, diff+0, diff+1);
    diff[0] -= xy[XCOORD(corner)];
    diff[1] -= xy[YCOORD(corner)];

    const cairo_rectangle_t *bbox = &shapes->bounding_box;
    gdouble lower[2] = { bbox->x, bbox->y };
    gdouble upper[2] = { bbox->x + bbox->width, bbox->y + bbox->height };
    gwy_coords_constrain_translation(orig_coords, NULL, diff, lower, upper);
    dxy->x = diff[0];
    dxy->y = diff[1];
}

static void
move_corner(GwyShapes *shapes,
            GdkModifierType modif,
            GwyXY *dxy)
{
    // Constrain movement in view space, pressing Ctrl limits it to
    // horizontal/vertical.
    if (modif & GDK_CONTROL_MASK)
        _gwy_shapes_constrain_horiz_vert(shapes, dxy);

    // Constrain final position in coords space, perform positional and angular
    // snapping and ensure it does not move anything outside the bounding box.
    const GwyCoords *orig_coords = gwy_shapes_get_starting_coords(shapes);
    GwyCoords *coords = gwy_shapes_get_coords(shapes);
    GwyShapesRectangle *rectangles = GWY_SHAPES_RECTANGLE(shapes);
    ShapesRectangle *priv = rectangles->priv;
    guint corner = priv->corner;
    gdouble xy[4];

    gwy_coords_get(orig_coords, priv->selection_index, xy);
    xy[XCOORD(corner)] += dxy->x;
    xy[YCOORD(corner)] += dxy->y;

    limit_into_bbox(&shapes->bounding_box, corner, xy);
    if (modif & GDK_SHIFT_MASK)
       constrain_aspect_ratio(shapes, corner, xy);
    _gwy_shapes_snap_to_pixel_corner(shapes,
                                     xy + XCOORD(corner), xy + YCOORD(corner));
    sort_corners(xy);
    gwy_coords_set(coords, priv->clicked, xy);
}

// Constrain the aspect ratio in view coordinates.  This seems natural as the
// user gets pixel squares in pixel view and physical squares in physical view.
static void
constrain_aspect_ratio(const GwyShapes *shapes,
                       guint corner,
                       gdouble *xy)
{
    gdouble lx = xy[2] - xy[0], ly = xy[3] - xy[1];
    cairo_matrix_transform_distance(&shapes->coords_to_view, &lx, &ly);
    gdouble alx = fabs(lx), aly = fabs(ly);
    if (alx < 1e-3 || aly < 1e-3 || alx/aly < 1e-2 || aly/alx < 1e-2)
        return;

    if (alx < aly) {
        guint ratio = gwy_round(aly/alx);
        if (ratio >= aly/alx) {
            // Shorten the short side.
            lx = copysign(aly/ratio, lx);
        }
        else {
            // Shorten the long side.
            ly = copysign(alx*ratio, ly);
        }
    }
    else {
        guint ratio = gwy_round(alx/aly);
        if (ratio >= alx/aly) {
            // Shorten the short side.
            ly = copysign(alx/ratio, ly);
        }
        else {
            // Shorten the long side.
            lx = copysign(aly*ratio, lx);
        }
    }

    cairo_matrix_transform_distance(&shapes->view_to_coords, &lx, &ly);

    if (corner & 1)
        xy[2] = xy[0] + lx;
    else
        xy[0] = xy[2] - lx;

    if (corner & 2)
        xy[3] = xy[1] + ly;
    else
        xy[1] = xy[3] - ly;
}

static void
limit_into_bbox(const cairo_rectangle_t *bbox, guint corner, gdouble *xy)
{
    xy[XCOORD(corner)] = CLAMP(xy[XCOORD(corner)],
                               bbox->x, bbox->x + bbox->width);
    xy[YCOORD(corner)] = CLAMP(xy[YCOORD(corner)],
                               bbox->y, bbox->y + bbox->height);
}

static void
sort_corners(gdouble *xy)
{
    GWY_ORDER(gdouble, xy[0], xy[2]);
    GWY_ORDER(gdouble, xy[1], xy[3]);
}

// FIXME: The top part is common
static gboolean
add_shape(GwyShapes *shapes, gdouble x, gdouble y)
{
    GwyCoords *coords = gwy_shapes_get_coords(shapes);
    guint n = gwy_coords_size(coords);
    if (n >= gwy_shapes_get_max_shapes(shapes))
        return FALSE;

    const cairo_matrix_t *matrix = &shapes->view_to_coords;
    cairo_matrix_transform_point(matrix, &x, &y);
    _gwy_shapes_snap_to_pixel_corner(shapes, &x, &y);

    const cairo_rectangle_t *bbox = &shapes->bounding_box;
    if (CLAMP(x, bbox->x, bbox->x + bbox->width) != x
        || CLAMP(y, bbox->y, bbox->y + bbox->height) != y)
        return FALSE;

    ShapesRectangle *priv = GWY_SHAPES_RECTANGLE(shapes)->priv;
    priv->mode = MODE_CORNER;
    priv->corner = 3;
    priv->entire_shape = FALSE;
    priv->new_shape = TRUE;
    priv->hover = priv->clicked = n;
    priv->selection_index = 0;
    gdouble xy[4] = { x, y, x, y };
    gwy_coords_set(coords, priv->clicked, xy);
    return TRUE;
}

static void
update_hover(GwyShapes *shapes, gdouble eventx, gdouble eventy)
{
    ShapesRectangle *priv = GWY_SHAPES_RECTANGLE(shapes)->priv;
    guint corner = G_MAXUINT;
    gboolean entire_shape = (gwy_int_set_size(shapes->selection) > 1);
    gint i = -1;

    if (isfinite(eventx) && isfinite(eventy)) {
        if (!entire_shape
            && (i = find_near_corner(shapes, eventx, eventy)) >= 0) {
            corner = i % 4;
            i /= 4;
        }
        else if ((i = find_near_rectangle(shapes, eventx, eventy)) >= 0) {
            corner = i % 4;
            entire_shape = TRUE;
            i /= 4;
        }
    }
    if (priv->hover == i && priv->entire_shape == entire_shape)
        return;

    gboolean do_update = priv->hover != i;
    priv->entire_shape = entire_shape;
    priv->corner = corner;
    priv->hover = i;

    GdkCursorType cursor_type = GDK_ARROW;
    if (i != -1 && gwy_shapes_get_selectable(shapes)) {
        if (!entire_shape && gwy_shapes_get_editable(shapes))
            cursor_type = GDK_CROSS;
        else
            cursor_type = GDK_FLEUR;
    }
    gwy_shapes_set_cursor_type(shapes, cursor_type);

    if (do_update)
        gwy_shapes_update(shapes);
}

/**
 * SECTION: shapes-rectangle
 * @title: GwyShapesRectangle
 * @short_description: Geometrical shapes – rectangles.
 *
 * #GwyShapesRectangle represents a group of selectable rectangles with
 * coordinates given by a #GwyCoordsRectangle object.
 **/

/**
 * GwyShapesRectangle:
 *
 * Object representing a group of selectable rectangles.
 *
 * The #GwyShapesRectangle struct contains private data only and should be
 * accessed using the functions below.
 **/

/**
 * GwyShapesRectangleClass:
 *
 * Class of groups of selectable rectangles.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
