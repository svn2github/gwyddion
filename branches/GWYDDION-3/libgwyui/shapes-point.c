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
#include <gdk/gdkkeysyms.h>
#include "libgwy/macros.h"
#include "libgwy/object-utils.h"
#include "libgwy/coords-point.h"
#include "libgwyui/cairo-utils.h"
#include "libgwyui/shapes-point.h"

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

typedef void (*MarkerDrawFunc)(cairo_t *cr,
                               const gdouble *xy,
                               gpointer user_data);

typedef struct _GwyShapesPointPrivate ShapesPoint;

struct _GwyShapesPointPrivate {
    gdouble radius;
    // Cached data in view coordinates.  May not correspond to @coords if they
    // are changed simultaneously from more sources.
    GArray *data;

    gint hover;
    gint clicked;
    gboolean changing_selection;
    gboolean has_moved;
    InteractMode mode;
    GwyXY xypress;    // Event (view) coordinates.
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
static gboolean gwy_shapes_point_key_press     (GwyShapes *shapes,
                                                GdkEventKey *event);
static void     gwy_shapes_point_cancel_editing(GwyShapes *shapes,
                                                gint id);
static void     gwy_shapes_selection_added     (GwyShapes *shapes,
                                                gint value);
static void     gwy_shapes_selection_removed   (GwyShapes *shapes,
                                                gint value);
static void     gwy_shapes_selection_assigned  (GwyShapes *shapes);

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
    shapes_class->key_press = gwy_shapes_point_key_press;
    shapes_class->cancel_editing = gwy_shapes_point_cancel_editing;
    shapes_class->selection_added = gwy_shapes_selection_added;
    shapes_class->selection_removed = gwy_shapes_selection_removed;
    shapes_class->selection_assigned = gwy_shapes_selection_assigned;

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
draw_markers(GwyShapes *shapes, cairo_t *cr,
             MarkerDrawFunc function, gpointer user_data)
{
    ShapesPoint *priv = GWY_SHAPES_POINT(shapes)->priv;
    const gdouble *data = (const gdouble*)priv->data->data;
    gint n = priv->data->len/2;

    if (!n)
        return;

    GwyIntSet *selection = shapes->selection;
    gboolean hoverselected = FALSE;
    guint count = 0;

    for (gint i = 0; i < n; i++) {
        if (i != priv->hover && !gwy_int_set_contains(selection, i)) {
            function(cr, data + 2*i, user_data);
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
                function(cr, data + 2*iter.value, user_data);
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
        function(cr, data + 2*priv->hover, user_data);
        gwy_shapes_stroke(shapes, cr, state);
    }
}

static void
draw_cross(cairo_t *cr,
           const gdouble *xy,
           gpointer user_data)
{
    const gdouble *pticklen = (const gdouble*)user_data;
    gwy_cairo_cross(cr, xy[0], xy[1], *pticklen);
}

static void
draw_crosses(GwyShapes *shapes, cairo_t *cr)
{
    gdouble ticklen = 5.0;
    cairo_save(cr);
    cairo_set_line_width(cr, 1.683);
    draw_markers(shapes, cr, &draw_cross, &ticklen);
    cairo_restore(cr);
}

static void
draw_radius(cairo_t *cr,
            const gdouble *xy,
            gpointer user_data)
{
    const gdouble *radii = (const gdouble*)user_data;
    gwy_cairo_ellipse(cr, xy[0], xy[1], radii[0], radii[1]);
}

static void
draw_radii(GwyShapes *shapes, cairo_t *cr)
{
    ShapesPoint *priv = GWY_SHAPES_POINT(shapes)->priv;
    gdouble radii[2] = { priv->radius, priv->radius };
    const cairo_matrix_t *matrix = &shapes->pixel_to_view;
    cairo_matrix_transform_distance(matrix, radii+0, radii+1);
    cairo_save(cr);
    cairo_set_line_width(cr, 1.0);
    draw_markers(shapes, cr, &draw_radius, radii);
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
    draw_crosses(shapes, cr);
    draw_radii(shapes, cr);
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
    ShapesPoint *priv = GWY_SHAPES_POINT(shapes)->priv;

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
add_point(GwyShapes *shapes,
          gdouble x, gdouble y)
{
    ShapesPoint *priv = GWY_SHAPES_POINT(shapes)->priv;
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
    ShapesPoint *priv = GWY_SHAPES_POINT(shapes)->priv;
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
        priv->has_moved = TRUE;
        // If we clicked on an already selected shape, we will move the entire
        // group.  If we clicked on an unselected shape we will need to select
        // only this one.
        if (!gwy_int_set_contains(shapes->selection, priv->clicked)) {
            priv->changing_selection = TRUE;
            gwy_int_set_update(shapes->selection, &priv->clicked, 1);
            priv->changing_selection = FALSE;
        }
    }

    GwyCoords *coords = gwy_shapes_get_coords(shapes);
    GwyXY dxy;
    constrain_movement(shapes, event->x, event->y, event->state, &dxy);
    gwy_coords_translate(coords, shapes->selection, (const gdouble*)&dxy);

    GwyXY xy;
    gwy_coords_get(coords, priv->clicked, (gdouble*)&xy);
    gwy_shapes_set_current_point(shapes, &xy);

    return TRUE;
}

static gboolean
gwy_shapes_point_button_press(GwyShapes *shapes,
                              GdkEventButton *event)
{
    ShapesPoint *priv = GWY_SHAPES_POINT(shapes)->priv;
    GwyIntSet *selection = shapes->selection;

    priv->xypress = (GwyXY){ event->x, event->y };
    if (event->state & GDK_SHIFT_MASK || !gwy_shapes_get_editable(shapes))
        priv->mode = MODE_SELECTING;
    else
        priv->mode = MODE_MOVING;

    // XXX: All the selection updates must be done in motion_notify or
    // button_release: only based on whether the pointer has moved we know
    // whether the user wants to select things or move them.
    if (priv->hover != -1) {
        priv->clicked = priv->hover;
    }
    else if (priv->mode == MODE_MOVING) {
        if (!add_point(shapes, event->x, event->y)) {
            priv->clicked = -1;
            return FALSE;
        }
        priv->changing_selection = TRUE;
        gwy_int_set_update(selection, &priv->clicked, 1);
        priv->changing_selection = FALSE;
    }
    priv->has_moved = FALSE;

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

    if (!priv->has_moved) {
        GwyIntSet *selection = shapes->selection;

        priv->changing_selection = TRUE;
        if (priv->mode == MODE_SELECTING)
            gwy_int_set_toggle(selection, priv->clicked);
        else if (priv->mode == MODE_MOVING)
            gwy_int_set_update(selection, &priv->clicked, 1);
        priv->changing_selection = FALSE;

        // FIXME: May not be necessary if we respond to the selection signals.
        gwy_shapes_update(shapes);
    }
    else {
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
gwy_shapes_point_key_press(GwyShapes *shapes,
                           GdkEventKey *event)
{
    ShapesPoint *priv = GWY_SHAPES_POINT(shapes)->priv;

    if (!gwy_shapes_get_editable(shapes))
        return FALSE;

    if (event->keyval == GDK_KEY_Delete || event->keyval == GDK_KEY_BackSpace) {
        gwy_shapes_point_cancel_editing(shapes, -1);
        GwyCoords *coords = gwy_shapes_get_coords(shapes);
        guint nsel;
        gint *selected = gwy_int_set_values(shapes->selection, &nsel);
        priv->changing_selection = TRUE;
        gwy_int_set_update(shapes->selection, NULL, 0);
        priv->changing_selection = FALSE;
        for (guint i = 0; i < nsel; i++)
            gwy_coords_delete(coords, selected[nsel-1 - i]);
        g_free(selected);
    }
    return FALSE;
}

static void
gwy_shapes_point_cancel_editing(GwyShapes *shapes,
                                gint id)
{
    ShapesPoint *priv = GWY_SHAPES_POINT(shapes)->priv;
    if (priv->clicked == -1 || id != priv->clicked)
        return;

    // FIXME: We might want to do something like the finishing touches at the
    // end of button_released() here.
    priv->clicked = -1;
    gwy_shapes_unset_current_point(shapes);
    gwy_coords_finished(gwy_shapes_get_coords(shapes));
    gwy_shapes_update(shapes);
}

static void
gwy_shapes_selection_added(GwyShapes *shapes,
                           G_GNUC_UNUSED gint value)
{
    ShapesPoint *priv = GWY_SHAPES_POINT(shapes)->priv;
    if (priv->changing_selection)
        return;
    // TODO
    gwy_shapes_point_cancel_editing(shapes, -1);
    gwy_shapes_update(shapes);
}

static void
gwy_shapes_selection_removed(GwyShapes *shapes,
                             G_GNUC_UNUSED gint value)
{
    ShapesPoint *priv = GWY_SHAPES_POINT(shapes)->priv;
    if (priv->changing_selection)
        return;
    // TODO
    gwy_shapes_point_cancel_editing(shapes, -1);
    gwy_shapes_update(shapes);
}

static void
gwy_shapes_selection_assigned(GwyShapes *shapes)
{
    ShapesPoint *priv = GWY_SHAPES_POINT(shapes)->priv;
    if (priv->changing_selection)
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