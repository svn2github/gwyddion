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

#include <glib/gi18n-lib.h>
#include "libgwy/macros.h"
#include "libgwy/rgba.h"
#include "libgwy/object-utils.h"
#include "libgwyui/types.h"
#include "libgwyui/cairo-utils.h"
#include "libgwyui/shapes.h"

enum {
    PROP_0,
    PROP_COORDS,
    PROP_MAX_SHAPES,
    PROP_SELECTABLE,
    PROP_EDITABLE,
    PROP_SNAPPING,
    PROP_SELECTION,
    N_PROPS
};

enum {
    SGNL_EDITING_STARTED,
    SGNL_UPDATED,
    N_SIGNALS
};

typedef struct _GwyShapesPrivate Shapes;

typedef gboolean (*EventMethodMotion)(GwyShapes *shapes, GdkEventMotion *event);
typedef gboolean (*EventMethodButton)(GwyShapes *shapes, GdkEventButton *event);
typedef gboolean (*EventMethodKey)(GwyShapes *shapes, GdkEventKey *event);
typedef void (*ItemMethodInt)(GwyShapes *shapes, gint id);
typedef void (*ItemMethod)(GwyShapes *shapes, guint id);
typedef void (*VoidMethod)(GwyShapes *shapes);

struct _GwyShapesPrivate {
    guint max_shapes;
    GwyShapesSnappingType snap;
    gboolean is_updated : 1;
    gboolean editable : 1;
    gboolean selectable : 1;

    gboolean updating_selection : 1;
    gboolean has_current_point : 1;

    GwyCoords *coords;
    gulong coords_item_inserted_id;
    gulong coords_item_deleted_id;
    gulong coords_item_updated_id;

    // Selection itself is in the public struct.
    gulong selection_added_id;
    gulong selection_removed_id;
    gulong selection_assigned_id;

    GwyXY current_point;
};

static void     gwy_shapes_finalize             (GObject *object);
static void     gwy_shapes_dispose              (GObject *object);
static void     gwy_shapes_set_property         (GObject *object,
                                                 guint prop_id,
                                                 const GValue *value,
                                                 GParamSpec *pspec);
static void     gwy_shapes_get_property         (GObject *object,
                                                 guint prop_id,
                                                 GValue *value,
                                                 GParamSpec *pspec);
static gboolean gwy_shapes_delete_selection_impl(GwyShapes *shapes);
static gboolean set_coords                      (GwyShapes *shapes,
                                                 GwyCoords *coords);
static gboolean set_max_shapes                  (GwyShapes *shapes,
                                                 guint max_shapes);
static gboolean set_selectable                  (GwyShapes *shapes,
                                                 gboolean selectable);
static gboolean set_editable                    (GwyShapes *shapes,
                                                 gboolean editable);
static gboolean set_snapping                    (GwyShapes *shapes,
                                                 GwyShapesSnappingType snap);
static void     cancel_editing                  (GwyShapes *shapes,
                                                 gint id);
static void     coords_item_inserted            (GwyShapes *shapes,
                                                 guint id,
                                                 GwyCoords *coords);
static void     coords_item_deleted             (GwyShapes *shapes,
                                                 guint id,
                                                 GwyCoords *coords);
static void     coords_item_updated             (GwyShapes *shapes,
                                                 guint id,
                                                 GwyCoords *coords);
static void     selection_added                 (GwyShapes *shapes,
                                                 gint value,
                                                 GwyIntSet *selection);
static void     selection_removed               (GwyShapes *shapes,
                                                 gint value,
                                                 GwyIntSet *selection);
static void     selection_assigned              (GwyShapes *shapes,
                                                 GwyIntSet *selection);

static const cairo_rectangle_t unrestricted_bbox = {
    -G_MAXDOUBLE, -G_MAXDOUBLE, G_MAXDOUBLE, G_MAXDOUBLE
};

static GParamSpec *properties[N_PROPS];
static guint signals[N_SIGNALS];

G_DEFINE_ABSTRACT_TYPE(GwyShapes, gwy_shapes, G_TYPE_INITIALLY_UNOWNED);

static void
gwy_shapes_class_init(GwyShapesClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    g_type_class_add_private(klass, sizeof(Shapes));

    klass->delete_selection = gwy_shapes_delete_selection_impl;

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

    /**
     * GwyShapes:max-shapes:
     *
     * Maximum allowed number of shapes.
     *
     * This property not only limits the number of shapes the user can add
     * interactively but setting it will also truncate the current coordinates
     * if they contain more objets.
     **/
    properties[PROP_MAX_SHAPES]
        = g_param_spec_uint("max-shapes",
                            "Max shapes",
                            "Maximum allowed number of shapes.",
                            0, G_MAXUINT, 0,
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_SELECTABLE]
        = g_param_spec_boolean("selectable",
                               "Selectable",
                               "Whether shapes can be selected by the user.",
                               TRUE,
                               G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_EDITABLE]
        = g_param_spec_boolean("editable",
                               "Editable",
                               "Whether shapes can be modified by the user.",
                               TRUE,
                               G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_SNAPPING]
        = g_param_spec_enum("snapping",
                            "Snapping",
                            "Snapping mode.",
                            GWY_TYPE_SHAPES_SNAPPING_TYPE,
                            GWY_SHAPES_SNAP_NONE,
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_SELECTION]
        = g_param_spec_object("selection",
                              "Selection",
                              "The set of currently selected shapes.",
                              GWY_TYPE_INT_SET,
                              G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    for (guint i = 1; i < N_PROPS; i++)
        g_object_class_install_property(gobject_class, i, properties[i]);

    /**
     * GwyShapes::editing-started:
     * @gwyshapes: The #GwyShapes which received the signal.
     *
     * The ::editing-started signal is emitted when user starts modifying the
     * shapes.  More precisely, it is emitted immediately before so that the
     * coordinate values have not changed yet.  It is not emitted when shapes
     * are only selected or deselected, you can use signals of the shapes'
     * selection for this.
     *
     * The end of the modification can be catched using the GwyCoords::finished
     * signal.
     **/
    signals[SGNL_EDITING_STARTED]
        = g_signal_new_class_handler("editing-started",
                                     G_OBJECT_CLASS_TYPE(klass),
                                     G_SIGNAL_RUN_FIRST,
                                     NULL, NULL, NULL,
                                     g_cclosure_marshal_VOID__VOID,
                                     G_TYPE_NONE, 0);

    /**
     * GwyShapes::updated:
     * @gwyshapes: The #GwyShapes which received the signal.
     *
     * The ::updated signal is emitted when the coords change, a transform
     * function changes or there is another reason to redraw the shapes.
     * The signal is emitted only once between invocations of the draw()
     * method.
     **/
    signals[SGNL_UPDATED]
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
    priv->selectable = TRUE;
    priv->editable = TRUE;
    priv->snap = GWY_SHAPES_SNAP_NONE;
    priv->current_point = (GwyXY){ NAN, NAN };
    GwyIntSet *selection = shapes->selection = gwy_int_set_new();
    priv->selection_added_id
        = g_signal_connect_swapped(selection, "added",
                                   G_CALLBACK(selection_added), shapes);
    priv->selection_removed_id
        = g_signal_connect_swapped(selection, "removed",
                                   G_CALLBACK(selection_removed), shapes);
    priv->selection_assigned_id
        = g_signal_connect_swapped(selection, "assigned",
                                   G_CALLBACK(selection_assigned), shapes);
}

static void
gwy_shapes_finalize(GObject *object)
{
    GwyShapes *shapes = GWY_SHAPES(object);
    Shapes *priv = shapes->priv;
    g_signal_handler_disconnect(shapes->selection, priv->selection_assigned_id);
    g_signal_handler_disconnect(shapes->selection, priv->selection_removed_id);
    g_signal_handler_disconnect(shapes->selection, priv->selection_added_id);
    GWY_OBJECT_UNREF(shapes->selection);
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

        case PROP_MAX_SHAPES:
        set_max_shapes(shapes, g_value_get_uint(value));
        break;

        case PROP_SELECTABLE:
        set_selectable(shapes, g_value_get_boolean(value));
        break;

        case PROP_EDITABLE:
        set_editable(shapes, g_value_get_boolean(value));
        break;

        case PROP_SNAPPING:
        set_snapping(shapes, g_value_get_enum(value));
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
    GwyShapes *shapes = GWY_SHAPES(object);
    Shapes *priv = shapes->priv;

    switch (prop_id) {
        case PROP_COORDS:
        g_value_set_object(value, priv->coords);
        break;

        case PROP_MAX_SHAPES:
        g_value_set_uint(value, priv->max_shapes);
        break;

        case PROP_SELECTABLE:
        g_value_set_boolean(value, priv->selectable);
        break;

        case PROP_EDITABLE:
        g_value_set_boolean(value, priv->editable);
        break;

        case PROP_SELECTION:
        g_value_set_object(value, shapes->selection);
        break;

        case PROP_SNAPPING:
        g_value_set_enum(value, priv->snap);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static gboolean
gwy_shapes_delete_selection_impl(GwyShapes *shapes)
{
    if (!gwy_shapes_get_editable(shapes)
        || !gwy_int_set_is_nonempty(shapes->selection))
        return FALSE;

    cancel_editing(shapes, -1);
    GwyCoords *coords = shapes->priv->coords;
    guint nsel;
    gint *selected = gwy_int_set_values(shapes->selection, &nsel);
    gwy_shapes_editing_started(shapes);
    gwy_shapes_start_updating_selection(shapes);
    gwy_int_set_update(shapes->selection, NULL, 0);
    gwy_shapes_stop_updating_selection(shapes);
    // FIXME: Can we use gwy_coords_delete_subset()?  Obviously not
    // after clearing @selection.  But deleting items while they are
    // in @selection can, obviously, lead to problems if anything is
    // connected to "item-deleted".
    for (guint i = 0; i < nsel; i++)
        gwy_coords_delete(coords, selected[nsel-1 - i]);
    g_free(selected);
    gwy_coords_finished(coords);

    return TRUE;
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

    cancel_editing(shapes, -1);
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
 * @cr: A Cairo context to draw to.
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
 * gwy_shapes_set_selectable:
 * @shapes: A group of geometrical shapes.
 * @selectable: %TRUE to permit selection of shapes by the user, %FALSE to
 *              disallow it.
 *
 * Sets whether geometrical shapes can be selected by the user.
 *
 * If shapes cannot be selected they cannot be edited either.  So in order to
 * permit modification by the user it is necessary for both
 * GwyShapes:selectable and GwyShapes:editable to be %TRUE.  However,
 * permitting only selection but no modification is meaningful.
 **/
void
gwy_shapes_set_selectable(GwyShapes *shapes,
                          gboolean selectable)
{
    g_return_if_fail(GWY_IS_SHAPES(shapes));
    if (!set_selectable(shapes, selectable))
        return;

    g_object_notify_by_pspec(G_OBJECT(shapes), properties[PROP_SELECTABLE]);

    if (!selectable)
        cancel_editing(shapes, -1);
}

/**
 * gwy_shapes_get_selectable:
 * @shapes: A group of geometrical shapes.
 *
 * Obtains whether geometrical shapes can be selected by the user.
 *
 * Returns: %TRUE if selection of shapes by the user is permitted, %FALSE if
 *          it is disallowed.
 **/
gboolean
gwy_shapes_get_selectable(const GwyShapes *shapes)
{
    g_return_val_if_fail(GWY_IS_SHAPES(shapes), FALSE);
    return shapes->priv->selectable;
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

    if (!editable)
        cancel_editing(shapes, -1);
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

/**
 * gwy_shapes_set_snapping:
 * @shapes: A group of geometrical shapes.
 * @snap: Snapping mode.
 *
 * Sets the snapping mode of a group of geometrical shapes.
 *
 * Changing the snapping mode does not cause re-snapping of existing shape
 * coodrdinates.  Only future shape editing is affected.
 **/
void
gwy_shapes_set_snapping(GwyShapes *shapes,
                        GwyShapesSnappingType snap)
{
    g_return_if_fail(GWY_IS_SHAPES(shapes));
    if (!set_snapping(shapes, snap))
        return;

    g_object_notify_by_pspec(G_OBJECT(shapes), properties[PROP_SNAPPING]);
}

/**
 * gwy_shapes_get_snapping:
 * @shapes: A group of geometrical shapes.
 *
 * Gets the snapping mode of a group of geometrical shapes.
 *
 * Returns: The snapping mode in effect.
 **/
GwyShapesSnappingType
gwy_shapes_get_snapping(const GwyShapes *shapes)
{
    g_return_val_if_fail(GWY_IS_SHAPES(shapes), GWY_SHAPES_SNAP_NONE);
    return shapes->priv->snap;
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
 * gwy_shapes_delete_selection:
 * @shapes: A group of geometrical shapes.
 *
 * Deletes all selected shapes in a group of geometrical shapes.
 *
 * Returns: %TRUE if anything was deleted, %FALSE if nothing was deleted
 *          (because nothing was selected or shapes are not editable).
 **/
gboolean
gwy_shapes_delete_selection(GwyShapes *shapes)
{
    g_return_val_if_fail(GWY_IS_SHAPES(shapes), FALSE);
    GwyShapesClass *klass = GWY_SHAPES_GET_CLASS(shapes);
    g_return_val_if_fail(klass->delete_selection, FALSE);
    return klass->delete_selection(shapes);
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
    if (!shapes->priv->coords || !shapes->priv->selectable)
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
    if (!shapes->priv->coords || !shapes->priv->selectable)
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
    if (!shapes->priv->coords || !shapes->priv->selectable)
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
    if (!shapes->priv->coords || !shapes->priv->selectable)
        return FALSE;

    if (event->keyval == GDK_KEY_Delete || event->keyval == GDK_KEY_BackSpace) {
        gwy_shapes_delete_selection(shapes);
        return TRUE;
    }

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
    if (!shapes->priv->coords || !shapes->priv->selectable)
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
    g_signal_emit(shapes, signals[SGNL_UPDATED], 0);
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
 * gwy_shapes_editing_started:
 * @shapes: A group of geometrical shapes.
 *
 * Emits signal ::editing-started on a group of geometrical shapes.
 *
 * This method is namely intended for subclasses.  If you emit this signal make
 * sure you emit GwyCoords::finished on the corresponding coordinates object
 * afterwards.
 *
 * Generally, the two signals should be paired.  Users interested in
 * continuous updates watch will all changes of the coordinates.  Users that
 * want to perform some operation when the shapes stop moving will watch
 * only GwyCoords::finished.  Users that need the ability to undo changes
 * will watch also GwyShapes::editing-started.
 **/
void
gwy_shapes_editing_started(GwyShapes *shapes)
{
    g_return_if_fail(GWY_IS_SHAPES(shapes));
    g_signal_emit(shapes, signals[SGNL_EDITING_STARTED], 0);
}

/**
 * gwy_shapes_get_current_point:
 * @shapes: A group of geometrical shapes.
 * @xy: (out) (allow-none):
 *      Location where to store the current point.  May be %NULL to just check
 *      whether any current point exists.
 *
 * Tests whether a group of geometrical shapes has a current point and obtains
 * its physical coordinates.
 *
 * The current point exists, in general, if user interacts with the shapes
 * somehow and the widget should always attempt to display an area containing
 * this point if possible.
 *
 * Subclasses should use gwy_shapes_set_current_point() to set the current
 * point and gwy_shapes_unset_current_point() to unset it.
 *
 * Returns: %TRUE a current point exists; %FALSE it it does not exist.
 *          Coordinates @xy are set in both cases (if non-%NULL), however,
 *          if no current point exists they will be set to
 *          <constant>NAN</constant>.
 **/
gboolean
gwy_shapes_get_current_point(const GwyShapes *shapes,
                             GwyXY *xy)
{
    g_return_val_if_fail(GWY_IS_SHAPES(shapes), FALSE);
    Shapes *priv = shapes->priv;
    if (!priv->has_current_point)
        return FALSE;

    GWY_MAYBE_SET(xy, priv->current_point);
    return TRUE;
}

/**
 * gwy_shapes_set_current_point:
 * @shapes: A group of geometrical shapes.
 * @xy: Location of the current point in @coords coordinates.
 *
 * Sets the location of the current point in a group of geometrical shapes and
 * makes it exist.
 **/
void
gwy_shapes_set_current_point(GwyShapes *shapes,
                             const GwyXY *xy)
{
    g_return_if_fail(GWY_IS_SHAPES(shapes));
    g_return_if_fail(xy);
    Shapes *priv = shapes->priv;
    priv->has_current_point = TRUE;
    priv->current_point = *xy;
}

/**
 * gwy_shapes_unset_current_point:
 * @shapes: A group of geometrical shapes.
 *
 * Makes the current point in a group of geometrical shapes non-existent.
 **/
void
gwy_shapes_unset_current_point(GwyShapes *shapes)
{
    g_return_if_fail(GWY_IS_SHAPES(shapes));
    Shapes *priv = shapes->priv;
    priv->has_current_point = FALSE;
    // Make things fail badly if they somehow read non-existent current point.
    priv->current_point = (GwyXY){ NAN, NAN };
}

/**
 * gwy_shapes_snap:
 * @shapes: A group of geometrical shapes.
 * @xy: Location of a shape point, in @coords coordinates.
 *
 * Snaps point coordinates according to the current snapping mode.
 *
 * It is necessary to set the pixel coordinates transformations with
 * gwy_shapes_set_pixel_matrices() for snapping.
 **/
void
gwy_shapes_snap(const GwyShapes *shapes,
                GwyXY *xy)
{
    g_return_if_fail(GWY_IS_SHAPES(shapes));
    Shapes *priv = shapes->priv;
    if (priv->snap == GWY_SHAPES_SNAP_NONE)
        return;

    cairo_matrix_transform_point(&shapes->coords_to_view, &xy->x, &xy->y);
    cairo_matrix_transform_point(&shapes->view_to_pixel, &xy->x, &xy->y);
    if (priv->snap == GWY_SHAPES_SNAP_CORNERS) {
        xy->x = gwy_round(xy->x);
        xy->y = gwy_round(xy->y);
    }
    else if (priv->snap == GWY_SHAPES_SNAP_CENTERS) {
        xy->x = gwy_round_to_half(xy->x);
        xy->y = gwy_round_to_half(xy->y);
    }
    else {
        g_warning("Wrong snapping type %u.\n", priv->snap);
    }
    cairo_matrix_transform_point(&shapes->pixel_to_view, &xy->x, &xy->y);
    cairo_matrix_transform_point(&shapes->view_to_coords, &xy->x, &xy->y);
}

/* Each shape can be ‘selected’ in the following independent ways:
 * HOVER – mouse is near the shape so that clicking would select or deselect it
 *         or start editing it
 * SELECTED – shape is a part of GwyShapes selection so future actions would
 *            apply to it
 * EDITED – shape is being currently edited by the user using mouse
 *
 * Constraints:
 * – At most one shape can have HOVER selection and it must be SELECTED or
 *   nothing.
 * – SELECTED and EDITED are two exclusing states applying usually to the same
 *   set of shapes.
 *
 * So all possible states are only:
 * NORMAL
 * HOVER
 * SELECTED
 * SELECTED + HOVER
 * EDITED
 *
 * ⇒ EDITED and SELECTED need not look differently.
 * ⇒ HOVER visualisation must be possible to combine with SELECTED (EDITED).
 * ⇒ HOVER can be an ‘expensive’ or ‘disruptive’ visualisation.
 */

/**
 * gwy_shapes_stroke:
 * @shapes: A group of geometrical shapes.
 * @cr: A Cairo context to draw to.  Its current path should represent the
 *      shapes in state @state or their markers.
 * @state: State of the shapes, determining how they will be hightlighted.
 *
 * Performes a Cairo stroke drawing a group of geometrical shapes in given
 * state.
 *
 * The current path is will cleared after drawing it and, generally, the Cairo
 * context will not be meaningful for the caller after stroking; use explicit
 * cairo_save()/cairo_restore() if preservation is required.
 **/
void
gwy_shapes_stroke(G_GNUC_UNUSED GwyShapes *shapes,
                  cairo_t *cr,
                  GwyShapesStateType state)
{
    static const GwyRGBA normal_color = { 1.0, 1.0, 1.0, 1.0 };
    static const GwyRGBA selected_color = { 1.0, 1.0, 0.5, 1.0 };
    static const GwyRGBA outline_color = { 0.0, 0.0, 0.0, 1.0 };
    GwyShapesStateType basestate = state & ~GWY_SHAPES_STATE_PRELIGHT;
    gboolean prelight = !!(state & GWY_SHAPES_STATE_PRELIGHT);
    GwyRGBA color = outline_color;
    gdouble width = cairo_get_line_width(cr) * (prelight ? 1.3 : 1.0);
    gdouble outline_width = 1.1*width + 0.4;
    gdouble alpha = prelight ? 1.0 : 0.7;

    color.a = alpha;
    // or: cairo_push_group(cr);
    gwy_cairo_set_source_rgba(cr, &color);
    cairo_set_line_width(cr, outline_width);
    cairo_stroke_preserve(cr);

    if (basestate == GWY_SHAPES_STATE_NORMAL)
        color = normal_color;
    else if (basestate == GWY_SHAPES_STATE_SELECTED)
        color = selected_color;
    else {
        g_critical("Unknown base shapes state %u.\n", basestate);
    }

    color.a = alpha;
    // or: dont't do this
    gwy_cairo_set_source_rgba(cr, &color);
    cairo_set_line_width(cr, width);
    cairo_stroke(cr);
    // or:
    // cairo_pop_group_to_source(cr);
    // cairo_paint_with_alpha(cr, alpha);
}

/**
 * gwy_shapes_start_updating_selection:
 * @shapes: A group of geometrical shapes.
 *
 * Declares that the user initiated a change of the selection of a group of
 * geometrical shapes.
 *
 * This method is namely intended for subclasses that should wrap #GwyIntSet
 * calls modifying the selection <emphasis>in response to user
 * interaction</emphasis>:
 * |[
 * // User deselects all shapes.
 * gwy_shapes_start_updating_selection(shapes);
 * gwy_int_set_fill(shapes->selection, NULL, 0);
 * gwy_shapes_stop_updating_selection(shapes);
 * ]|
 *
 * The reason is resolution of conflicts of programmatic changes of the
 * selection with user-initiated changes.  The standard resolution is that if
 * the selection changes programmatically while the user is editing it, which
 * should not be the normal course of action, the editing is cancelled to
 * prevent all the possible strange consequences (using
 * GwyShapesClass.cancel_editing()).  This requires distinguishing user and
 * programmatic changes which is done by wrapping it as described above.
 *
 * The state can be tested with gwy_shapes_is_updating_selection().
 *
 * It is an error to nest calls to this method; one user-initiated change must
 * finish before another can start.
 **/
void
gwy_shapes_start_updating_selection(GwyShapes *shapes)
{
    g_return_if_fail(GWY_IS_SHAPES(shapes));
    Shapes *priv = shapes->priv;
    g_return_if_fail(!priv->updating_selection);
    priv->updating_selection = TRUE;
}

/**
 * gwy_shapes_stop_updating_selection:
 * @shapes: A group of geometrical shapes.
 *
 * Declares that the user finished a change of the selection of a group of
 * geometrical shapes.
 *
 * See gwy_shapes_start_updating_selection() for a discussion.
 **/
void
gwy_shapes_stop_updating_selection(GwyShapes *shapes)
{
    g_return_if_fail(GWY_IS_SHAPES(shapes));
    Shapes *priv = shapes->priv;
    g_return_if_fail(priv->updating_selection);
    priv->updating_selection = FALSE;
}

/**
 * gwy_shapes_is_updating_selection:
 * @shapes: A group of geometrical shapes.
 *
 * Tests whether a user-initiated change of the selection of a group of
 * geometrical shapes is underway.
 *
 * See gwy_shapes_start_updating_selection() for a discussion.
 *
 * Returns: %TRUE is a user-initiated selection change is underway.  %FALSE
 *          otherwise.
 **/
gboolean
gwy_shapes_is_updating_selection(const GwyShapes *shapes)
{
    g_return_val_if_fail(GWY_IS_SHAPES(shapes), FALSE);
    return shapes->priv->updating_selection;
}

static gboolean
set_selectable(GwyShapes *shapes,
               gboolean selectable)
{
    Shapes *priv = shapes->priv;
    if (!selectable == !priv->selectable)
        return FALSE;

    priv->selectable = selectable;
    if (!selectable)
        cancel_editing(shapes, -1);
    return TRUE;
}

static gboolean
set_editable(GwyShapes *shapes,
             gboolean editable)
{
    Shapes *priv = shapes->priv;
    if (!editable == !priv->editable)
        return FALSE;

    priv->editable = editable;
    if (!editable)
        cancel_editing(shapes, -1);
    return TRUE;
}

static gboolean
set_snapping(GwyShapes *shapes,
             GwyShapesSnappingType snap)
{
    Shapes *priv = shapes->priv;
    if (snap == priv->snap)
        return FALSE;

    if (snap > GWY_SHAPES_SNAP_CENTERS) {
        g_warning("Wrong snapping type %u.\n", snap);
        return FALSE;
    }

    priv->snap = snap;
    // XXX: Should we do anything with existing coords?  Probably not.
    return TRUE;
}

static void
cancel_editing(GwyShapes *shapes, gint id)
{
    if (!shapes->priv->coords)
        return;
    ItemMethodInt method = GWY_SHAPES_GET_CLASS(shapes)->cancel_editing;
    if (method)
        method(shapes, id);
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
    cancel_editing(shapes, id);
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

static void
selection_added(GwyShapes *shapes,
                gint value,
                GwyIntSet *selection)
{
    g_assert(shapes->selection == selection);
    ItemMethodInt method = GWY_SHAPES_GET_CLASS(shapes)->selection_added;
    if (method)
        method(shapes, value);
    gwy_shapes_update(shapes);
}

static void
selection_removed(GwyShapes *shapes,
                  gint value,
                  GwyIntSet *selection)
{
    g_assert(shapes->selection == selection);
    ItemMethodInt method = GWY_SHAPES_GET_CLASS(shapes)->selection_removed;
    if (method)
        method(shapes, value);
    gwy_shapes_update(shapes);
}

static void
selection_assigned(GwyShapes *shapes,
                   GwyIntSet *selection)
{
    g_assert(shapes->selection == selection);
    VoidMethod method = GWY_SHAPES_GET_CLASS(shapes)->selection_assigned;
    if (method)
        method(shapes);
    gwy_shapes_update(shapes);
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
 * @selection: Integer set object representing the set of currently selected
 *             shapes.
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
 * @delete_selection: Virtual method implementing
 *                    gwy_shapes_delete_selection().  It has a default
 *                    implementation, however, subclasses will likely want to
 *                    extend it by updating visual clues such as hover.
 * @coords_item_inserted: Virtual method called when the @coords object
 *                        emits #GwyArray::item-inserted.
 * @coords_item_deleted: Virtual method called when the @coords object
 *                       emits #GwyArray::item-deleted.
 * @coords_item_updated: Virtual method called when the @coords object
 *                       emits #GwyArray::item-updated.
 * @selection_added: Virtual method called when the selection object
 *                   emits #GwyIntSet::added.
 * @selection_removed: Virtual method called when the selection object
 *                     emits #GwyIntSet::removed.
 * @selection_assigned: Virtual method called when the selection object
 *                      emits #GwyIntSet::assigned.
 *
 * Class of groups of selectable geometrical shapes.
 *
 * Specific, i.e. instantiable, subclasses have to set @coords_type and
 * implement at least the draw() method.  Of the user interaction methods they
 * should implement only those actually needed (possibly none if the subclass
 * is intended only for passive visualisation).
 *
 * The user interaction methods are never invoked if @shapes does not have any
 * coordinates object set.  Neither they are invoked if @shapes is not
 * selectable (nevertheless, they are invoked if they are just not editable).
 *
 * Drawing of the shapes with gwy_shapes_draw() requires providing a matrix for
 * the transformation of physical coordinates to view coordinates using
 * gwy_shapes_set_coords_matrices().  Drawing markers and other features with
 * sizes in field pixels requires also setting the transform between real and
 * pixel coordinates using gwy_shapes_set_pixel_matrices().
 **/

/**
 * GwyShapesStateType:
 * @GWY_SHAPES_STATE_NORMAL: Normal state.
 * @GWY_SHAPES_STATE_SELECTED: Shape is selected or just being moved.
 * @GWY_SHAPES_STATE_PRELIGHT: Mouse is hovering over the shape so clicking
 *                             would interact with it.
 *
 * Type of state a shape in a group of geometric shapes can be in.
 *
 * All states except %GWY_SHAPES_STATE_NORMAL are flags and can be combined
 * together or with %GWY_SHAPES_STATE_NORMAL which is an alias for zero.
 **/

/**
 * GwyCoordScaleType:
 * @GWY_COORD_SCALE_REAL: Rulers display real coordinates in physical units.
 * @GWY_COORD_SCALE_PIXEL: Rulers display pixel coordinates.
 *
 * Type of rules scales (coordinates).
 **/

/**
 * GwyShapesSnappingType:
 * @GWY_SHAPES_SNAP_NONE: No snapping.
 * @GWY_SHAPES_SNAP_CORNERS: Coordinates are restricted to corners of pixels
 *                           in two-dimensional data.
 * @GWY_SHAPES_SNAP_CENTERS: Coordinates are restricted to centres of pixels
 *                           in two-dimensional data.
 *
 * Type of snapping used in editing of shapes.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
