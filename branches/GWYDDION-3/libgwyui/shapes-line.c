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
#include "libgwy/coords-line.h"
#include "libgwyui/cairo-utils.h"
#include "libgwyui/shapes-line.h"

enum {
    PROP_0,
    PROP_THICKNESS,
    N_PROPS
};

typedef enum {
    MODE_MOVING,
    MODE_SELECTING,
    MODE_RUBBERBAND,
} InteractMode;

typedef void (*MarkerDrawFunc)(cairo_t *cr,
                               const gdouble *xy,
                               gpointer user_data);

typedef struct _GwyShapesLinePrivate ShapesLine;

struct _GwyShapesLinePrivate {
    gdouble thickness;
    // Cached data in view coordinates.  May not correspond to @coords if they
    // are changed simultaneously from more sources.
    GArray *data;

    gint hover;
    gint clicked;
    guint endpoint;  // endpoint being moved 1 == first, 2 == second, 3 == both
    gboolean has_moved;
    InteractMode mode;
    GwyXY xypress;    // Event (view) coordinates.
};

static void     gwy_shapes_line_finalize          (GObject *object);
static void     gwy_shapes_line_set_property      (GObject *object,
                                                   guint prop_id,
                                                   const GValue *value,
                                                   GParamSpec *pspec);
static void     gwy_shapes_line_get_property      (GObject *object,
                                                   guint prop_id,
                                                   GValue *value,
                                                   GParamSpec *pspec);
static void     gwy_shapes_line_draw              (GwyShapes *shapes,
                                                   cairo_t *cr);
static gboolean gwy_shapes_line_motion_notify     (GwyShapes *shapes,
                                                   GdkEventMotion *event);
static gboolean gwy_shapes_line_button_press      (GwyShapes *shapes,
                                                   GdkEventButton *event);
static gboolean gwy_shapes_line_button_release    (GwyShapes *shapes,
                                                   GdkEventButton *event);
static gboolean gwy_shapes_line_delete_selection  (GwyShapes *shapes);
static void     gwy_shapes_line_cancel_editing    (GwyShapes *shapes,
                                                   gint id);
static void     gwy_shapes_line_selection_added   (GwyShapes *shapes,
                                                   gint value);
static void     gwy_shapes_line_selection_removed (GwyShapes *shapes,
                                                   gint value);
static void     gwy_shapes_line_selection_assigned(GwyShapes *shapes);
static gboolean set_thickness                     (GwyShapesLine *lines,
                                                   gdouble thickness);
static void     calculate_data                    (GwyShapesLine *lines);
static void     draw_lines                        (GwyShapes *shapes,
                                                   cairo_t *cr);
static void     draw_line                         (cairo_t *cr,
                                                   const gdouble *xy,
                                                   gpointer user_data);
static void     draw_thicknesses                  (GwyShapes *shapes,
                                                   cairo_t *cr);
static void     draw_thickness                    (cairo_t *cr,
                                                   const gdouble *xy,
                                                   gpointer user_data);
static void     draw_markers                      (GwyShapes *shapes,
                                                   cairo_t *cr,
                                                   MarkerDrawFunc function,
                                                   gpointer user_data);
static gint     find_near_point                   (GwyShapesLine *lines,
                                                   gdouble x,
                                                   gdouble y);
static gint     find_near_line                    (GwyShapesLine *lines,
                                                   gdouble x,
                                                   gdouble y);
static void     constrain_movement                (GwyShapes *shapes,
                                                   gdouble eventx,
                                                   gdouble eventy,
                                                   GdkModifierType modif,
                                                   GwyXY *dxy);
static gboolean add_line                          (GwyShapes *shapes,
                                                   gdouble x,
                                                   gdouble y);
static void     update_hover                      (GwyShapes *shapes,
                                                   gdouble eventx,
                                                   gdouble eventy);
static gboolean snap_line                         (GwyShapes *shapes,
                                                   gdouble *x,
                                                   gdouble *y);

static GParamSpec *properties[N_PROPS];

G_DEFINE_TYPE(GwyShapesLine, gwy_shapes_line, GWY_TYPE_SHAPES);

static void
gwy_shapes_line_class_init(GwyShapesLineClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GwyShapesClass *shapes_class = GWY_SHAPES_CLASS(klass);

    g_type_class_add_private(klass, sizeof(ShapesLine));

    gobject_class->finalize = gwy_shapes_line_finalize;
    gobject_class->get_property = gwy_shapes_line_get_property;
    gobject_class->set_property = gwy_shapes_line_set_property;

    shapes_class->coords_type = GWY_TYPE_COORDS_LINE;
    shapes_class->draw = gwy_shapes_line_draw;
    shapes_class->delete_selection = gwy_shapes_line_delete_selection;
    shapes_class->motion_notify = gwy_shapes_line_motion_notify;
    shapes_class->button_press = gwy_shapes_line_button_press;
    shapes_class->button_release = gwy_shapes_line_button_release;
    shapes_class->cancel_editing = gwy_shapes_line_cancel_editing;
    shapes_class->selection_added = gwy_shapes_line_selection_added;
    shapes_class->selection_removed = gwy_shapes_line_selection_removed;
    shapes_class->selection_assigned = gwy_shapes_line_selection_assigned;

    properties[PROP_THICKNESS]
        = g_param_spec_double("thickness",
                              "Thickness",
                              "Thickness of endpoint orthogonal markers.",
                              0, G_MAXDOUBLE, 0.0,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    for (guint i = 1; i < N_PROPS; i++)
        g_object_class_install_property(gobject_class, i, properties[i]);
}

static void
gwy_shapes_line_finalize(GObject *object)
{
    GwyShapesLine *lines = GWY_SHAPES_LINE(object);
    ShapesLine *priv = lines->priv;
    if (priv->data) {
        g_array_free(priv->data, TRUE);
        priv->data = NULL;
    }
}

static void
gwy_shapes_line_init(GwyShapesLine *lines)
{
    lines->priv = G_TYPE_INSTANCE_GET_PRIVATE(lines, GWY_TYPE_SHAPES_LINE,
                                              ShapesLine);
    ShapesLine *priv = lines->priv;
    priv->hover = priv->clicked = -1;
}

static void
gwy_shapes_line_set_property(GObject *object,
                             guint prop_id,
                             const GValue *value,
                             GParamSpec *pspec)
{
    GwyShapesLine *lines = GWY_SHAPES_LINE(object);

    switch (prop_id) {
        case PROP_THICKNESS:
        set_thickness(lines, g_value_get_double(value));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_shapes_line_get_property(GObject *object,
                             guint prop_id,
                             GValue *value,
                             GParamSpec *pspec)
{
    ShapesLine *priv = GWY_SHAPES_LINE(object)->priv;

    switch (prop_id) {
        case PROP_THICKNESS:
        g_value_set_double(value, priv->thickness);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_shapes_line_draw(GwyShapes *shapes,
                     cairo_t *cr)
{
    GwyCoords *coords = gwy_shapes_get_coords(shapes);
    if (!coords || !gwy_coords_size(coords))
        return;

    GwyShapesLine *lines = GWY_SHAPES_LINE(shapes);
    calculate_data(lines);
    draw_lines(shapes, cr);
    draw_thicknesses(shapes, cr);
}

static gboolean
gwy_shapes_line_motion_notify(GwyShapes *shapes,
                              GdkEventMotion *event)
{
    ShapesLine *priv = GWY_SHAPES_LINE(shapes)->priv;
    // FIXME: Change once we implement MODE_RUBBERBAND
    if (!priv->data || priv->mode == MODE_RUBBERBAND)
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
    if (!priv->has_moved && (event->x != priv->xypress.x
                             || event->y != priv->xypress.y)) {
        gwy_shapes_editing_started(shapes);
        priv->has_moved = TRUE;
        // If we clicked on an already selected shape, we will move the entire
        // group.  If we clicked on an unselected shape we will need to select
        // only this one.
        if (!gwy_int_set_contains(shapes->selection, priv->clicked)) {
            gwy_shapes_start_updating_selection(shapes);
            gwy_int_set_update(shapes->selection, &priv->clicked, 1);
            gwy_shapes_stop_updating_selection(shapes);
        }
    }

    GwyCoords *coords = gwy_shapes_get_coords(shapes);
    GwyXY dxy;
    if (priv->endpoint == 3) {
        // Moving the entire line.
        constrain_movement(shapes, event->x, event->y, event->state, &dxy);
        gwy_coords_translate(coords, shapes->selection, (const gdouble*)&dxy);

        GwyXY xy;
        gwy_coords_get(coords, priv->clicked, (gdouble*)&xy);
        gwy_shapes_set_current_point(shapes, &xy);
    }
    else {
        // TODO: Move a single endpoint.  Makes no sense if more shapes are
        // selected.  We can
        // (a) Always move entire lines if more than one is selected.
        // (b) Do not move other selected lines if an endpoint it moved.
    }

    return TRUE;
}

static gboolean
gwy_shapes_line_button_press(GwyShapes *shapes,
                             GdkEventButton *event)
{
    ShapesLine *priv = GWY_SHAPES_LINE(shapes)->priv;
    GwyIntSet *selection = shapes->selection;

    priv->xypress = (GwyXY){ event->x, event->y };
    if (event->state & GDK_SHIFT_MASK || !gwy_shapes_get_editable(shapes))
        priv->mode = MODE_SELECTING;
    else
        priv->mode = MODE_MOVING;

    // XXX: All the selection updates must be done in motion_notify or
    // button_release: only based on whether the pointer has moved we know
    // whether the user wants to select things or move them.
    update_hover(shapes, event->x, event->y);
    if (priv->hover != -1) {
        priv->clicked = priv->hover;
    }
    else if (priv->mode == MODE_MOVING) {
        if (!add_line(shapes, event->x, event->y)) {
            priv->clicked = -1;
            return FALSE;
        }
        gwy_shapes_start_updating_selection(shapes);
        gwy_int_set_update(selection, &priv->clicked, 1);
        gwy_shapes_stop_updating_selection(shapes);
    }
    priv->has_moved = FALSE;

    return FALSE;
}

static gboolean
gwy_shapes_line_button_release(GwyShapes *shapes,
                               GdkEventButton *event)
{
    ShapesLine *priv = GWY_SHAPES_LINE(shapes)->priv;
    if (priv->clicked == -1) {
        update_hover(shapes, event->x, event->y);
        return FALSE;
    }

    if (!priv->has_moved) {
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
        // TODO: Also needs to differentiate between endpoint/entire line
        // movement.  The logic should be factored out of motion-notify and
        // this function to a common subroutine.
        GwyCoords *coords = gwy_shapes_get_coords(shapes);
        GwyXY dxy;
        constrain_movement(shapes, event->x, event->y, event->state, &dxy);
        gwy_coords_translate(coords, shapes->selection, (const gdouble*)&dxy);
        gwy_coords_finished(coords);
    }
    gwy_shapes_unset_current_point(shapes);
    priv->clicked = -1;
    update_hover(shapes, event->x, event->y);

    return TRUE;
}

static gboolean
gwy_shapes_line_delete_selection(GwyShapes *shapes)
{
    if (GWY_SHAPES_CLASS
                   (gwy_shapes_line_parent_class)->delete_selection(shapes)) {
        update_hover(shapes, NAN, NAN);
        return TRUE;
    }
    return FALSE;
}

static void
gwy_shapes_line_cancel_editing(GwyShapes *shapes,
                               gint id)
{
    ShapesLine *priv = GWY_SHAPES_LINE(shapes)->priv;
    if (priv->clicked == -1 || id != priv->clicked)
        return;

    // FIXME: We might want to do something like the finishing touches at the
    // end of button_released() here.
    priv->clicked = -1;
    gwy_shapes_unset_current_point(shapes);
    if (priv->has_moved) {
        gwy_coords_finished(gwy_shapes_get_coords(shapes));
        priv->has_moved = FALSE;
    }
    gwy_shapes_update(shapes);
}

// FIXME: May be common.
static void
gwy_shapes_line_selection_added(GwyShapes *shapes,
                                G_GNUC_UNUSED gint value)
{
    if (gwy_shapes_is_updating_selection(shapes))
        return;
    // TODO
    gwy_shapes_line_cancel_editing(shapes, -1);
    gwy_shapes_update(shapes);
}

static void
gwy_shapes_line_selection_removed(GwyShapes *shapes,
                                  G_GNUC_UNUSED gint value)
{
    if (gwy_shapes_is_updating_selection(shapes))
        return;
    // TODO
    gwy_shapes_line_cancel_editing(shapes, -1);
    gwy_shapes_update(shapes);
}

static void
gwy_shapes_line_selection_assigned(GwyShapes *shapes)
{
    if (gwy_shapes_is_updating_selection(shapes))
        return;
    // TODO
    gwy_shapes_line_cancel_editing(shapes, -1);
    gwy_shapes_update(shapes);
}

/**
 * gwy_shapes_line_new:
 *
 * Creates a new group of selectable lines.
 *
 * Return: A newly created group of selectable lines.
 **/
GwyShapes*
gwy_shapes_line_new(void)
{
    return g_object_new(GWY_TYPE_SHAPES_LINE, 0, NULL);
}

static gboolean
set_thickness(GwyShapesLine *lines,
           gdouble thickness)
{
    ShapesLine *priv = lines->priv;
    if (thickness == priv->thickness)
        return FALSE;

    priv->thickness = thickness;
    gwy_shapes_update(GWY_SHAPES(lines));
    return TRUE;
}

// FIXME: May be common.
static void
calculate_data(GwyShapesLine *lines)
{
    ShapesLine *priv = lines->priv;
    GwyShapes *shapes = GWY_SHAPES(lines);
    GwyCoords *coords = gwy_shapes_get_coords(shapes);
    guint shape_size = gwy_coords_shape_size(coords);
    guint n = gwy_coords_size(coords);
    guint ncoord = n*shape_size;
    if (!priv->data)
        priv->data = g_array_sized_new(FALSE, FALSE, sizeof(gdouble), ncoord);
    g_array_set_size(priv->data, ncoord);
    const cairo_matrix_t *matrix = &shapes->coords_to_view;
    gdouble *data = (gdouble*)priv->data->data;
    gwy_coords_get_data(coords, data);
    for (guint i = 0; i < ncoord/2; i++)
        cairo_matrix_transform_point(matrix, data + 2*i, data + 2*i + 1);
}

static void
draw_lines(GwyShapes *shapes, cairo_t *cr)
{
    cairo_save(cr);
    cairo_set_line_width(cr, 1.0);
    draw_markers(shapes, cr, &draw_line, NULL);
    cairo_restore(cr);
}

static void
draw_line(cairo_t *cr,
          const gdouble *xy,
          G_GNUC_UNUSED gpointer user_data)
{
    cairo_move_to(cr, xy[0], xy[1]);
    cairo_line_to(cr, xy[2], xy[3]);
}

static void
draw_thicknesses(GwyShapes *shapes, cairo_t *cr)
{
    ShapesLine *priv = GWY_SHAPES_LINE(shapes)->priv;
    gdouble thicknesses[2] = { priv->thickness, priv->thickness };
    const cairo_matrix_t *matrix = &shapes->pixel_to_view;
    cairo_matrix_transform_distance(matrix, thicknesses+0, thicknesses+1);
    cairo_save(cr);
    cairo_set_line_width(cr, 1.0);
    draw_markers(shapes, cr, &draw_thickness, thicknesses);
    cairo_restore(cr);
}

static void
draw_thickness(cairo_t *cr,
               const gdouble *xy,
               gpointer user_data)
{
    const gdouble *thicknesses = (const gdouble*)user_data;
    gdouble vx = xy[2] - xy[0], vy = xy[3] - xy[1];
    if (!vx && !vy)
        return;

    gdouble v = hypot(vx, vy), cosphi = vx/v, sinphi = vy/v;
    cairo_move_to(cr,
                  xy[0] - sinphi*thicknesses[0], xy[1] + cosphi*thicknesses[1]);
    cairo_line_to(cr,
                  xy[0] + sinphi*thicknesses[0], xy[1] - cosphi*thicknesses[1]);
    cairo_move_to(cr,
                  xy[2] - sinphi*thicknesses[0], xy[3] + cosphi*thicknesses[1]);
    cairo_line_to(cr,
                  xy[2] + sinphi*thicknesses[0], xy[3] - cosphi*thicknesses[1]);
}

// FIXME: May be common.
static void
draw_markers(GwyShapes *shapes, cairo_t *cr,
             MarkerDrawFunc function, gpointer user_data)
{
    ShapesLine *priv = GWY_SHAPES_LINE(shapes)->priv;
    const gdouble *data = (const gdouble*)priv->data->data;
    GwyCoords *coords = gwy_shapes_get_coords(shapes);
    guint shape_size = gwy_coords_shape_size(coords);
    gint n = priv->data->len/shape_size;

    if (!n)
        return;

    GwyIntSet *selection = shapes->selection;
    gboolean hoverselected = FALSE;
    guint count = 0;

    for (gint i = 0; i < n; i++) {
        if (i != priv->hover && !gwy_int_set_contains(selection, i)) {
            function(cr, data + shape_size*i, user_data);
            count++;
        }
    }
    if (count) {
        gwy_shapes_stroke(shapes, cr, GWY_SHAPES_STATE_NORMAL);
        count = 0;
    }

    GwyIntSetIter iter;
    if (gwy_int_set_first(selection, &iter)) {
        do {
            if (iter.value == priv->hover)
                hoverselected = TRUE;
            else {
                function(cr, data + shape_size*iter.value, user_data);
                count++;
            }
        } while (gwy_int_set_next(selection, &iter));

        if (count) {
            gwy_shapes_stroke(shapes, cr, GWY_SHAPES_STATE_SELECTED);
            count = 0;
        }
    }

    if (priv->hover != -1) {
        GwyShapesStateType state = GWY_SHAPES_STATE_PRELIGHT;
        if (hoverselected)
            state |= GWY_SHAPES_STATE_SELECTED;
        function(cr, data + shape_size*priv->hover, user_data);
        gwy_shapes_stroke(shapes, cr, state);
    }
}

// FIXME: The same as in ShapesLine, just for both endlines.
static gint
find_near_point(GwyShapesLine *lines,
                gdouble x, gdouble y)
{
    ShapesLine *priv = lines->priv;
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

static gint
find_near_line(GwyShapesLine *lines,
               gdouble x, gdouble y)
{
    ShapesLine *priv = lines->priv;
    guint n = priv->data->len/4;
    const gdouble *data = (const gdouble*)priv->data->data;
    gdouble mindist2 = G_MAXDOUBLE;
    gint mini = -1;

    for (guint i = 0; i < n; i++) {
        gdouble xf = data[4*i], yf = data[4*i + 1],
                xt = data[4*i + 2], yt = data[4*i + 3];
        gdouble vx = xt - xf, vy = yt - yf, v2 = vx*vx + vy*vy;
        if (!v2
            || vx*(x - xt) > vy*(yt - y)
            || vx*(x - xf) < vy*(yf - y))
            continue;
        gdouble dist2 = x*vy - y*vx + xt*yf - xf*yt;
        dist2 *= dist2/v2;
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
    ShapesLine *priv = GWY_SHAPES_LINE(shapes)->priv;

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
    snap_line(shapes, &eventx, &eventy);
    GwyCoords *coords = gwy_shapes_get_coords(shapes);

    gdouble d[2];
    gwy_coords_get(coords, priv->clicked, d);
    d[0] = eventx - d[0];
    d[1] = eventy - d[1];
    const cairo_rectangle_t *bbox = &shapes->bounding_box;
    gdouble lower[2] = { bbox->x, bbox->y };
    gdouble upper[2] = { bbox->x + bbox->width, bbox->y + bbox->height };
    gwy_coords_constrain_translation(coords, shapes->selection,
                                     d, lower, upper);
    dxy->x = d[0];
    dxy->y = d[1];
}

static gboolean
add_line(GwyShapes *shapes, gdouble x, gdouble y)
{
    ShapesLine *priv = GWY_SHAPES_LINE(shapes)->priv;
    GwyCoords *coords = gwy_shapes_get_coords(shapes);
    guint n = gwy_coords_size(coords);
    if (n >= gwy_shapes_get_max_shapes(shapes))
        return FALSE;

    const cairo_matrix_t *matrix = &shapes->view_to_coords;
    cairo_matrix_transform_point(matrix, &x, &y);
    snap_line(shapes, &x, &y);

    const cairo_rectangle_t *bbox = &shapes->bounding_box;
    if (CLAMP(x, bbox->x, bbox->x + bbox->width) != x
        || CLAMP(y, bbox->y, bbox->y + bbox->height) != y)
        return FALSE;

    gdouble xy2[4] = { x, y, x, y };
    priv->endpoint = 2;
    priv->hover = priv->clicked = n;
    gwy_coords_set(coords, priv->clicked, xy2);
    return TRUE;
}

static void
update_hover(GwyShapes *shapes, gdouble eventx, gdouble eventy)
{
    GwyShapesLine *lines = GWY_SHAPES_LINE(shapes);
    ShapesLine *priv = lines->priv;
    guint endpoint = 0;
    gint i = -1;

    if (isfinite(eventx) && isfinite(eventy)) {
        if ((i = find_near_point(lines, eventx, eventy)) >= 0) {
            endpoint = 1 << (i % 2);
            i /= 2;
        }
        else {
            i = find_near_line(lines, eventx, eventy);
            if (i >= 0)
                endpoint = 3;
        }
    }
    if (priv->hover == i && priv->endpoint == endpoint)
        return;

    priv->hover = i;
    priv->endpoint = endpoint;
    gwy_shapes_update(shapes);
}

static gboolean
snap_line(GwyShapes *shapes,
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
 * SECTION: shapes-line
 * @title: GwyShapesLine
 * @short_description: Geometrical shapes – lines.
 *
 * #GwyShapesLine represents a group of selectable lines with coordinates
 * given by a #GwyCoordsLine object.
 **/

/**
 * GwyShapesLine:
 *
 * Object representing a group of selectable lines.
 *
 * The #GwyShapesLine struct contains private data only and should be accessed
 * using the functions below.
 **/

/**
 * GwyShapesLineClass:
 *
 * Class of groups of selectable lines.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
