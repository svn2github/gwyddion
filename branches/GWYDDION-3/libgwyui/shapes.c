/*
 *  $Id$
 *  Copyright (C) 2011-2012 David Nečas (Yeti).
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

#include <glib.h>
#include <glib/gi18n-lib.h>
#include "libgwy/macros.h"
#include "libgwy/object-utils.h"
#include "libgwyui/shapes.h"

enum {
    PROP_0,
    PROP_COORDS,
    PROP_FOCUS,
    PROP_MAX_SHAPES,
    PROP_EDITABLE,
    N_PROPS
};

enum {
    SHAPE_SELECTED,
    UPDATED,
    N_SIGNALS
};

typedef struct _GwyShapesPrivate Shapes;

typedef gboolean (*EventMethodMotion)(GwyShapes *shapes, GdkEventMotion *event);
typedef gboolean (*EventMethodButton)(GwyShapes *shapes, GdkEventButton *event);
typedef gboolean (*EventMethodKey)(GwyShapes *shapes, GdkEventKey *event);
typedef void (*CancelEditingMethod)(GwyShapes *shapes, gint id);
typedef void (*ItemMethod)(GwyShapes *shapes, guint id);

struct _GwyShapesPrivate {
    gint focus;
    guint max_shapes;
    gboolean is_updated;
    gboolean editable;

    GwyCoords *coords;
    gulong coords_item_inserted_id;
    gulong coords_item_deleted_id;
    gulong coords_item_updated_id;

    // Selection object:
    // (1) Who owns it? @shapes, probably.  Have set from foo/get as foo funcs.
    // (2) How subclasses work with actual representation?
    // (3) Items are selected/deselected/added/removed from both shapes and
    //     programatically.  Must have two way updates:
    //     @coords size changes → selection reflects this
    //     selection is set from outside → ideally impossible to get wrong
    //     item is deleted by @shapes due to user interaction – who is
    //     responsible for deleting it in selection?
    // (4) Representation: A list of selected intervals?  Represents well
    //     all common cases (nothing, a few random, a few large blocks, all).
    // (5) How it should interact with GtkTreeSelection if items are displayed
    //     in a treeview?
};

static void     gwy_shapes_finalize           (GObject *object);
static void     gwy_shapes_dispose            (GObject *object);
static void     gwy_shapes_set_property       (GObject *object,
                                               guint prop_id,
                                               const GValue *value,
                                               GParamSpec *pspec);
static void     gwy_shapes_get_property       (GObject *object,
                                               guint prop_id,
                                               GValue *value,
                                               GParamSpec *pspec);
static gboolean set_coords                    (GwyShapes *shapes,
                                               GwyCoords *coords);
static gboolean set_focus                     (GwyShapes *shapes,
                                               gint id);
static gboolean set_max_shapes                (GwyShapes *shapes,
                                               guint max_shapes);
static gboolean set_editable                  (GwyShapes *shapes,
                                               gboolean editable);
static void     cancel_editing                (GwyShapes *shapes,
                                               gint id);
static void     coords_item_inserted          (GwyShapes *shapes,
                                               guint id,
                                               GwyCoords *coords);
static void     coords_item_deleted           (GwyShapes *shapes,
                                               guint id,
                                               GwyCoords *coords);
static void     coords_item_updated           (GwyShapes *shapes,
                                               guint id,
                                               GwyCoords *coords);

static const cairo_rectangle_t unrestricted_bbox = {
    -G_MAXDOUBLE, -G_MAXDOUBLE, G_MAXDOUBLE, G_MAXDOUBLE
};

static guint signals[N_SIGNALS];
static GParamSpec *properties[N_PROPS];

G_DEFINE_ABSTRACT_TYPE(GwyShapes, gwy_shapes, G_TYPE_INITIALLY_UNOWNED);

static void
gwy_shapes_class_init(GwyShapesClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    g_type_class_add_private(klass, sizeof(Shapes));

    gobject_class->dispose = gwy_shapes_dispose;
    gobject_class->finalize = gwy_shapes_finalize;
    gobject_class->get_property = gwy_shapes_get_property;
    gobject_class->set_property = gwy_shapes_set_property;

    properties[PROP_COORDS]
        = g_param_spec_object("coords",
                              "Coords",
                              "Coordinates of the shape.",
                              // FIXME: This is wrong.  Do it in each specific
                              // subclass with the correct type?
                              GWY_TYPE_COORDS,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_FOCUS]
        = g_param_spec_int("focus",
                           "Focus",
                           "Index of focused shape that only can be "
                           "edited by the user.  Negative value means "
                           "all shapes can be edited.",
                           -1, G_MAXINT, -1,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_MAX_SHAPES]
        = g_param_spec_uint("max-shapes",
                            "Max shapes",
                            "Maximum allowed number of shapes to select. "
                            "Setting it will truncate the current coordinates "
                            "if they contain more objets.",
                            0, G_MAXUINT, 0,
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_EDITABLE]
        = g_param_spec_boolean("editable",
                               "Editable",
                               "Whether shapes can be modified by the user",
                               TRUE,
                               G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    for (guint i = 1; i < N_PROPS; i++)
        g_object_class_install_property(gobject_class, i, properties[i]);

    // FIXME: This is too simplistic.  We may want multiple objects to be
    // selected.  This can be implemented using a GwyMaskLine.
    /**
     * GwyShapes::shape-selected:
     * @gwyshapes: The #GwyShapes which received the signal.
     * @arg1: The number (index) of the chosen selection object.
     *
     * The ::shape-selected signal is emitted when user starts interacting
     * with a shape, even before modifying it.  It is emitted
     * even if the layer is not editable, but it is not emitted when focus
     * is set and the user attempts to choose a different object.
     **/
    signals[SHAPE_SELECTED]
        = g_signal_new_class_handler("shape-selected",
                                     G_OBJECT_CLASS_TYPE(klass),
                                     G_SIGNAL_RUN_FIRST,
                                     NULL, NULL, NULL,
                                     g_cclosure_marshal_VOID__INT,
                                     G_TYPE_NONE, 1, G_TYPE_INT);

    /**
     * GwyShapes::updated:
     * @gwyshapes: The #GwyShapes which received the signal.
     *
     * The ::updated signal is emitted when the coords change, a transform
     * function changes or there is another reason to redraw the shapes.
     * The signal is emitted only once between invocations of the draw()
     * method.
     **/
    signals[UPDATED]
        = g_signal_new_class_handler("updated",
                                     G_OBJECT_CLASS_TYPE(klass),
                                     G_SIGNAL_RUN_FIRST,
                                     NULL, NULL, NULL,
                                     g_cclosure_marshal_VOID__VOID,
                                     G_TYPE_NONE, 0);
}

static void
gwy_shapes_init(GwyShapes *shapes)
{
    shapes->priv = G_TYPE_INSTANCE_GET_PRIVATE(shapes, GWY_TYPE_SHAPES, Shapes);
    Shapes *priv = shapes->priv;
    cairo_matrix_init_identity(&shapes->coords_to_view);
    cairo_matrix_init_identity(&shapes->view_to_coords);
    cairo_matrix_init_identity(&shapes->pixel_to_view);
    cairo_matrix_init_identity(&shapes->view_to_pixel);
    shapes->bounding_box = unrestricted_bbox;
    priv->max_shapes = G_MAXUINT;
    priv->editable = TRUE;
}

static void
gwy_shapes_finalize(GObject *object)
{
    G_OBJECT_CLASS(gwy_shapes_parent_class)->finalize(object);
}

static void
gwy_shapes_dispose(GObject *object)
{
    GwyShapes *shapes = GWY_SHAPES(object);
    set_coords(shapes, NULL);
    G_OBJECT_CLASS(gwy_shapes_parent_class)->dispose(object);
}

static void
gwy_shapes_set_property(GObject *object,
                        guint prop_id,
                        const GValue *value,
                        GParamSpec *pspec)
{
    GwyShapes *shapes = GWY_SHAPES(object);

    switch (prop_id) {
        case PROP_COORDS:
        set_coords(shapes, g_value_get_object(value));
        break;

        case PROP_FOCUS:
        set_focus(shapes, g_value_get_int(value));
        break;

        case PROP_MAX_SHAPES:
        set_max_shapes(shapes, g_value_get_uint(value));
        break;

        case PROP_EDITABLE:
        set_editable(shapes, g_value_get_boolean(value));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_shapes_get_property(GObject *object,
                        guint prop_id,
                        GValue *value,
                        GParamSpec *pspec)
{
    Shapes *priv = GWY_SHAPES(object)->priv;

    switch (prop_id) {
        case PROP_COORDS:
        g_value_set_object(value, priv->coords);
        break;

        case PROP_FOCUS:
        g_value_set_int(value, priv->focus);
        break;

        case PROP_MAX_SHAPES:
        g_value_set_uint(value, priv->max_shapes);
        break;

        case PROP_EDITABLE:
        g_value_set_boolean(value, priv->editable);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static gboolean
set_coords(GwyShapes *shapes,
           GwyCoords *coords)
{
    Shapes *priv = shapes->priv;
    GwyShapesClass *klass = GWY_SHAPES_GET_CLASS(shapes);

    if (!gwy_set_member_object(shapes, coords, klass->coords_type,
                               &priv->coords,
                               "item-deleted", &coords_item_deleted,
                               &priv->coords_item_deleted_id,
                               G_CONNECT_SWAPPED,
                               "item-inserted", &coords_item_inserted,
                               &priv->coords_item_inserted_id,
                               G_CONNECT_SWAPPED,
                               "item-updated", &coords_item_updated,
                               &priv->coords_item_updated_id,
                               G_CONNECT_SWAPPED,
                               NULL))
        return FALSE;

    // FIXME: We should cancel whatever manipulation the users performs now.
    gwy_shapes_update(shapes);
    return TRUE;
}

/**
 * gwy_shapes_set_coords:
 * @shapes: A group of geometrical shapes.
 * @coords: (allow-none):
 *          A group of coordinates.
 *
 * Sets the coordinates to be visualised by a shapes object.
 *
 * The type of the coords object must match the type used by the specific
 * shapes class.
 **/
void
gwy_shapes_set_coords(GwyShapes *shapes,
                      GwyCoords *coords)
{
    g_return_if_fail(GWY_IS_SHAPES(shapes));
    if (!set_coords(shapes, coords))
        return;

    g_object_notify_by_pspec(G_OBJECT(shapes), properties[PROP_COORDS]);
}

/**
 * gwy_shapes_get_coords:
 * @shapes: A group of geometrical shapes.
 *
 * Obtains the coordinates visualised by a shapes object.
 *
 * Returns: (transfer none):
 *          The coordinates object being visualised.
 **/
GwyCoords*
gwy_shapes_get_coords(GwyShapes *shapes)
{
    g_return_val_if_fail(GWY_IS_SHAPES(shapes), NULL);
    return shapes->priv->coords;
}

/**
 * gwy_shapes_coords_type:
 * @shapes: A group of geometrical shapes.
 *
 * Obtains the type of coordinates object used by a group of shapes.
 *
 * Returns: The #GwyCoords subclass used to represent the coordinates of
 *          @shape.
 **/
GType
gwy_shapes_coords_type(const GwyShapes *shapes)
{
    GwyShapesClass *klass = GWY_SHAPES_GET_CLASS(shapes);
    g_return_val_if_fail(klass, 0);
    return klass->coords_type;
}

/**
 * gwy_shapes_class_coords_type:
 * @klass: The class of a group of geometrical shapes.
 *
 * Obtains the type of coordinates object used by a group of shapes class.
 *
 * Returns: The #GwyCoords subclass used to represent the coordinates of
 *          shapes of this class.
 **/
GType
gwy_shapes_class_coords_type(const GwyShapesClass *klass)
{
    g_return_val_if_fail(GWY_IS_SHAPES_CLASS(klass), 0);
    return klass->coords_type;
}

static gboolean
set_matrices(cairo_matrix_t *desta, cairo_matrix_t *destb,
             const cairo_matrix_t *srca, const cairo_matrix_t *srcb)
{
    if (!srca && !srcb) {
        cairo_matrix_t matrix;
        cairo_matrix_init_identity(&matrix);
        if (gwy_equal(&matrix, desta) && gwy_equal(&matrix, destb))
            return FALSE;

        *destb = matrix;
        *desta = matrix;
    }
    else if (!srca) {
        if (gwy_equal(srcb, destb))
            return FALSE;

        *destb = *srcb;
        *desta = *srcb;
        cairo_matrix_invert(desta);
    }
    else if (!srcb) {
        if (gwy_equal(srca, desta))
            return FALSE;

        *desta = *srca;
        *destb = *srca;
        cairo_matrix_invert(destb);
    }
    else {
        if (gwy_equal(srca, desta) && gwy_equal(srcb, destb))
            return FALSE;

        *desta = *srca;
        *destb = *srcb;
    }
    return TRUE;
}

/**
 * gwy_shapes_set_coords_matrices:
 * @shapes: A group of geometrical shapes.
 * @coords_to_view: (allow-none):
 *                  Affine transformation from @shapes' @coords coordinates to
 *                  view coordinates.
 * @view_to_coords: (allow-none):
 *                  Affine transformation from @shapes' view coordinates to
 *                  @coords coordinates.
 *
 * Sets the matrices for transformation between physical coordinates and view
 * coordinates for a group of geometrical shapes.
 *
 * If both matrices are given they must be invertible and inverses of each
 * other.  It is possible to give only one matrix; the other is then created as
 * its inverse.  They are copied by @shapes.
 **/
void
gwy_shapes_set_coords_matrices(GwyShapes *shapes,
                               const cairo_matrix_t *coords_to_view,
                               const cairo_matrix_t *view_to_coords)
{
    g_return_if_fail(GWY_IS_SHAPES(shapes));
    if (set_matrices(&shapes->coords_to_view, &shapes->view_to_coords,
                     coords_to_view, view_to_coords))
        gwy_shapes_update(shapes);
}

/**
 * gwy_shapes_set_pixel_matrices:
 * @shapes: A group of geometrical shapes.
 * @pixel_to_view: (allow-none):
 *                  Affine transformation from @shapes' pixel coordinates to
 *                  view coordinates.
 * @view_to_pixel: (allow-none):
 *                  Affine transformation from @shapes' view coordinates to
 *                  pixel coordinates.
 *
 * Sets the matrices for transformation between physical coordinates and view
 * coordinates for a group of geometrical shapes.
 *
 * Shapes are defined using real coordinates, however, pixel dimensions are
 * used to visualise for instance averaging radii or lengths.
 *
 * If both matrices are given they must be invertible and inverses of each
 * other.  It is possible to give only one matrix; the other is then created as
 * its inverse.  They are copied by @shapes.
 **/
void
gwy_shapes_set_pixel_matrices(GwyShapes *shapes,
                              const cairo_matrix_t *pixel_to_view,
                              const cairo_matrix_t *view_to_pixel)
{
    g_return_if_fail(GWY_IS_SHAPES(shapes));
    if (set_matrices(&shapes->pixel_to_view, &shapes->view_to_pixel,
                     pixel_to_view, view_to_pixel))
        gwy_shapes_update(shapes);
}

/**
 * gwy_shapes_draw:
 * @shapes: A group of geometrical shapes.
 * @cr: A cairo context to draw to.
 *
 * Draws a group of geometrical shapes using a Cairo context.
 **/
void
gwy_shapes_draw(GwyShapes *shapes,
                cairo_t *cr)
{
    g_return_if_fail(GWY_IS_SHAPES(shapes));
    g_return_if_fail(cr);

    GwyShapesClass *klass = GWY_SHAPES_GET_CLASS(shapes);
    g_return_if_fail(klass->draw);
    klass->draw(shapes, cr);
    shapes->priv->is_updated = FALSE;
}

/**
 * gwy_shapes_set_focus:
 * @shapes: A group of geometrical shapes.
 * @id: Index of the shape to focus.  Pass -1 to permit interaction with all
 *      shapes.
 *
 * Limits user interaction with a group of geometrical shapes to a single
 * shape.
 **/
void
gwy_shapes_set_focus(GwyShapes *shapes,
                     gint id)
{
    g_return_if_fail(GWY_IS_SHAPES(shapes));
    if (!set_focus(shapes, id))
        return;

    g_object_notify_by_pspec(G_OBJECT(shapes), properties[PROP_FOCUS]);
}

static gboolean
set_focus(GwyShapes *shapes,
          gint id)
{
    Shapes *priv = shapes->priv;

    // Map all negative values to -1.
    id = MAX(id, -1);
    if (priv->focus == id)
        return FALSE;
    priv->focus = id;
    return TRUE;
}

/**
 * gwy_shapes_get_focus:
 * @shapes: A group of geometrical shapes.
 *
 * Obtains the index of the focused shape in a group of geometrical shapes.
 *
 * Returns: Index of the focused shape.  Value -1 is returned if no shape is
 *          focused.
 **/
gint
gwy_shapes_get_focus(const GwyShapes *shapes)
{
    g_return_val_if_fail(GWY_IS_SHAPES(shapes), -1);
    return shapes->priv->focus;
}

/**
 * gwy_shapes_set_max_shapes:
 * @shapes: A group of geometrical shapes.
 * @max_shapes: Maximum number of shapes that can be present.  If the
 *              coordinates object already contains more shapes than that it
 *              will be truncated.
 *
 * Sets the maximum number of geometrical shapes that can be present.
 **/
void
gwy_shapes_set_max_shapes(GwyShapes *shapes,
                          guint max_shapes)
{
    g_return_if_fail(GWY_IS_SHAPES(shapes));
    if (!set_max_shapes(shapes, max_shapes))
        return;

    g_object_notify_by_pspec(G_OBJECT(shapes), properties[PROP_MAX_SHAPES]);
}

static gboolean
set_max_shapes(GwyShapes *shapes,
               guint max_shapes)
{
    Shapes *priv = shapes->priv;
    if (max_shapes == priv->max_shapes)
        return FALSE;

    priv->max_shapes = max_shapes;
    if (!priv->coords)
        return TRUE;

    guint n = gwy_coords_size(priv->coords);
    while (n > priv->max_shapes) {
        gwy_coords_delete(priv->coords, n-1);
        n--;
    }
    return TRUE;
}

/**
 * gwy_shapes_get_max_shapes:
 * @shapes: A group of geometrical shapes.
 *
 * Obtains the maximum number of geometrical shapes that can be present.
 **/
guint
gwy_shapes_get_max_shapes(const GwyShapes *shapes)
{
    g_return_val_if_fail(GWY_IS_SHAPES(shapes), 0);
    return shapes->priv->max_shapes;
}

/**
 * gwy_shapes_set_editable:
 * @shapes: A group of geometrical shapes.
 * @editable: %TRUE to permit modification of shapes by the user, %FALSE to
 *            disallow it.
 *
 * Sets whether geometrical shapes can be modified by the user.
 **/
void
gwy_shapes_set_editable(GwyShapes *shapes,
                        gboolean editable)
{
    g_return_if_fail(GWY_IS_SHAPES(shapes));
    if (!set_editable(shapes, editable))
        return;

    g_object_notify_by_pspec(G_OBJECT(shapes), properties[PROP_EDITABLE]);
}

static gboolean
set_editable(GwyShapes *shapes,
             gboolean editable)
{
    Shapes *priv = shapes->priv;
    editable = !!editable;
    if (editable == priv->editable)
        return FALSE;

    priv->editable = editable;
    if (!editable)
        cancel_editing(shapes, -1);
    return TRUE;
}

/**
 * gwy_shapes_get_editable:
 * @shapes: A group of geometrical shapes.
 *
 * Obtains whether geometrical shapes can be modified by the user.
 *
 * Returns: %TRUE if modification of shapes by the user is permitted, %FALSE if
 *          it is disallowed.
 **/
gboolean
gwy_shapes_get_editable(const GwyShapes *shapes)
{
    g_return_val_if_fail(GWY_IS_SHAPES(shapes), FALSE);
    return shapes->priv->editable;
}

static void
cancel_editing(GwyShapes *shapes, gint id)
{
    if (!shapes->priv->coords)
        return;
    CancelEditingMethod method = GWY_SHAPES_GET_CLASS(shapes)->cancel_editing;
    if (method)
        method(shapes, id);
}

// FIXME: What is this good for?
/**
 * gwy_shapes_gdk_event_mask:
 * @shapes: A group of geometrical shapes.
 *
 * Gets the set of Gdk events a group of geometrical shapes object requires for
 * user interaction.
 **/
GdkEventMask
gwy_shapes_gdk_event_mask(const GwyShapes *shapes)
{
    GwyShapesClass *klass = GWY_SHAPES_GET_CLASS(shapes);
    g_return_val_if_fail(klass, 0);
    GdkEventMask mask = 0;

    if (klass->button_press)
        mask |= GDK_BUTTON_PRESS_MASK;
    if (klass->button_release)
        mask |= GDK_BUTTON_RELEASE_MASK;
    if (klass->motion_notify)
        mask |= (GDK_BUTTON1_MOTION_MASK
                 | GDK_POINTER_MOTION_MASK
                 | GDK_POINTER_MOTION_HINT_MASK);
    if (klass->key_press)
        mask |= GDK_KEY_PRESS_MASK;
    if (klass->key_release)
        mask |= GDK_KEY_RELEASE_MASK;

    return mask;
}

/**
 * gwy_shapes_button_press:
 * @shapes: A group of geometrical shapes.
 * @event: A #GdkEventButton event.
 *
 * Forwards a button press event to a group of geometrical shapes.
 *
 * Returns: %TRUE if the event was handled.
 **/
gboolean
gwy_shapes_button_press(GwyShapes *shapes,
                        GdkEventButton *event)
{
    g_return_val_if_fail(GWY_IS_SHAPES(shapes), FALSE);
    if (!shapes->priv->coords)
        return FALSE;
    EventMethodButton method = GWY_SHAPES_GET_CLASS(shapes)->button_press;
    return method ? method(shapes, event) : FALSE;
}

/**
 * gwy_shapes_button_release:
 * @shapes: A group of geometrical shapes.
 * @event: A #GdkEventButton event.
 *
 * Forwards a button release event to a group of geometrical shapes.
 *
 * Returns: %TRUE if the event was handled.
 **/
gboolean
gwy_shapes_button_release(GwyShapes *shapes,
                          GdkEventButton *event)
{
    g_return_val_if_fail(GWY_IS_SHAPES(shapes), FALSE);
    if (!shapes->priv->coords)
        return FALSE;
    EventMethodButton method = GWY_SHAPES_GET_CLASS(shapes)->button_release;
    return method ? method(shapes, event) : FALSE;
}

/**
 * gwy_shapes_motion_notify:
 * @shapes: A group of geometrical shapes.
 * @event: A #GdkEventMotion event.
 *
 * Forwards a motion notify event to a group of geometrical shapes.
 *
 * Returns: %TRUE if the event was handled.
 **/
gboolean
gwy_shapes_motion_notify(GwyShapes *shapes,
                         GdkEventMotion *event)
{
    g_return_val_if_fail(GWY_IS_SHAPES(shapes), FALSE);
    if (!shapes->priv->coords)
        return FALSE;
    EventMethodMotion method = GWY_SHAPES_GET_CLASS(shapes)->motion_notify;
    return method ? method(shapes, event) : FALSE;
}

/**
 * gwy_shapes_key_press:
 * @shapes: A group of geometrical shapes.
 * @event: A #GdkEventKey event.
 *
 * Forwards a key press event to a group of geometrical shapes.
 *
 * Returns: %TRUE if the event was handled.
 **/
gboolean
gwy_shapes_key_press(GwyShapes *shapes,
                     GdkEventKey *event)
{
    g_return_val_if_fail(GWY_IS_SHAPES(shapes), FALSE);
    if (!shapes->priv->coords)
        return FALSE;
    EventMethodKey method = GWY_SHAPES_GET_CLASS(shapes)->key_press;
    return method ? method(shapes, event) : FALSE;
}

/**
 * gwy_shapes_key_release:
 * @shapes: A group of geometrical shapes.
 * @event: A #GdkEventKey event.
 *
 * Forwards a key release event to a group of geometrical shapes.
 *
 * Returns: %TRUE if the event was handled.
 **/
gboolean
gwy_shapes_key_release(GwyShapes *shapes,
                       GdkEventKey *event)
{
    g_return_val_if_fail(GWY_IS_SHAPES(shapes), FALSE);
    if (!shapes->priv->coords)
        return FALSE;
    EventMethodKey method = GWY_SHAPES_GET_CLASS(shapes)->key_release;
    return method ? method(shapes, event) : FALSE;
}

/**
 * gwy_shapes_set_bounding_box:
 * @shapes: A group of geometrical shapes.
 * @bbox: (allow-none):
 *        Bounding box in @coords coordinates that determines range of
 *        permissible shape positions.  Passing %NULL resets the bounding box
 *        to unlimited.
 *
 * Sets the bounding box for a group of geometrical shapes.
 **/
void
gwy_shapes_set_bounding_box(GwyShapes *shapes,
                            const cairo_rectangle_t *bbox)
{
    g_return_if_fail(GWY_IS_SHAPES(shapes));
    if (!bbox)
        bbox = &unrestricted_bbox;

    if (gwy_equal(bbox, &shapes->bounding_box))
        return;

    shapes->bounding_box = *bbox;
    gwy_shapes_update(shapes);
}

static void
coords_item_inserted(GwyShapes *shapes,
                     guint id,
                     GwyCoords *coords)
{
    g_assert(shapes->priv->coords == coords);
    ItemMethod method = GWY_SHAPES_GET_CLASS(shapes)->coords_item_inserted;
    if (method)
        method(shapes, id);
    gwy_shapes_update(shapes);
}

static void
coords_item_deleted(GwyShapes *shapes,
                    guint id,
                    GwyCoords *coords)
{
    g_assert(shapes->priv->coords == coords);
    ItemMethod method = GWY_SHAPES_GET_CLASS(shapes)->coords_item_deleted;
    if (method)
        method(shapes, id);
    CancelEditingMethod cmethod = GWY_SHAPES_GET_CLASS(shapes)->cancel_editing;
    if (cmethod)
        cmethod(shapes, id);
    gwy_shapes_update(shapes);
}

static void
coords_item_updated(GwyShapes *shapes,
                    guint id,
                    GwyCoords *coords)
{
    g_assert(shapes->priv->coords == coords);
    ItemMethod method = GWY_SHAPES_GET_CLASS(shapes)->coords_item_updated;
    if (method)
        method(shapes, id);
    gwy_shapes_update(shapes);
}

/**
 * gwy_shapes_update:
 * @shapes: A group of geometrical shapes.
 *
 * Enforces a group of geometrical shapes to ‘updated’ state.
 *
 * Is this is the first time after drawing the shapes they are updated the
 * GwyShapes::updated signal is emitted.  Otherwise this method is no-op.
 **/
void
gwy_shapes_update(GwyShapes *shapes)
{
    g_return_if_fail(GWY_IS_SHAPES(shapes));
    Shapes *priv = shapes->priv;
    if (priv->is_updated)
        return;

    priv->is_updated = TRUE;
    g_signal_emit(shapes, signals[UPDATED], 0);
}

/**
 * gwy_shapes_is_updated:
 * @shapes: A group of geometrical shapes.
 *
 * Checks whether a group of geometrical shapes has been updates since the last
 * drawing.
 *
 * Returns: %TRUE if @shapes has been updated, %FALSE otherwise.
 **/
gboolean
gwy_shapes_is_updated(GwyShapes *shapes)
{
    g_return_val_if_fail(GWY_IS_SHAPES(shapes), FALSE);
    return shapes->priv->is_updated;
}

/**
 * SECTION: shapes
 * @title: GwyShapes
 * @short_description: Base class for selectable geometrical shapes.
 *
 * #GwyShapes represents a group of geometrical shapes with coordinates given
 * by a #GwyCoords object.
 **/

/**
 * GwyShapes:
 * @bounding_box: Bounding box that determines permissible shape positions,
 *                in @coords (physical) coordinates.
 * @coords_to_view: Affine transformation from @shapes' @coords coordinates to
 *                  view coordinates.
 * @view_to_coords: Affine transformation from @shapes' view coordinates to
 *                  @coords coordinates.
 * @pixel_to_view: Affine transformation from @shapes' pixel coordinates to
 *                 view coordinates.
 * @view_to_pixel: Affine transformation from @shapes' view coordinates to
 *                 pixel coordinates.
 *
 * Object representing a group of selectable geometrical shapes.
 *
 * The #GwyShapes struct contains some public fields that can be directly
 * accessed for reading which is useful namely for subclassing.  To set them,
 * you must always use the methods such as gwy_shapes_set_bounding_box().
 **/

/**
 * GwyShapesClass:
 * @coords_type: Type of #GwyCoords subclass this shapes class visualises.
 * @draw: Virtual method implementing gwy_shapes_draw().
 * @button_press: Virtual method implementing gwy_shapes_button_press().
 * @button_release: Virtual method implementing gwy_shapes_button_release().
 * @motion_notify: Virtual method implementing gwy_shapes_motion_notify().
 * @key_press: Virtual method implementing gwy_shapes_key_press().
 * @key_release: Virtual method implementing gwy_shapes_key_release().
 * @cancel_editing: Virtual method that stops user modification of shapes,
 *                  namely because something has changed that makes continuing
 *                  not meaningful.
 * @coords_item_inserted: Virtual method called when the @coords object
 *                        emits #GwyArray::item-inserted.
 * @coords_item_deleted: Virtual method called when the @coords object
 *                       emits #GwyArray::item-deleted.
 * @coords_item_updated: Virtual method called when the @coords object
 *                       emits #GwyArray::item-updated.
 *
 * Class of groups of selectable geometrical shapes.
 *
 * Specific, i.e. instantiable, subclasses have to set @coords_type and
 * implement at least the draw() method.  Of the user interaction methods they
 * should implement only those actually needed (possibly none if the subclass
 * is intended only for passive visualisation).
 *
 * The user interaction methods are never invoked if @shapes does not have any
 * coordinates object set.
 *
 * Drawing of the shapes with gwy_shapes_draw() requires providing a matrix for
 * the transformation of physical coordinates to view coordinates using
 * gwy_shapes_set_coords_matrices().  Drawing markers and other features with
 * sizes in field pixels requires also setting the transform between real and
 * pixel coordinates using gwy_shapes_set_pixel_matrices().
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
