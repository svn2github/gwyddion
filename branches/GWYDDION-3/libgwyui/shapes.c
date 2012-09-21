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
    N_PROPS
};

enum {
    SHAPE_SELECTED,
    UPDATED,
    N_SIGNALS
};

typedef struct _GwyShapesPrivate Shapes;

typedef gboolean (*EventMethod)(GwyShapes *shapes, GdkEvent *event);
typedef void (*ItemMethod)(GwyShapes *shapes, guint id);

typedef struct {
    GwyShapesTransformFunc func;
    gpointer data;
    GDestroyNotify destroy;
} CoordsTransform;

struct _GwyShapesPrivate {
    gint focus;
    gboolean is_updated;

    GwyCoords *coords;
    gulong coords_item_inserted_id;
    gulong coords_item_deleted_id;
    gulong coords_item_changed_id;

    CoordsTransform coords_to_view;
    CoordsTransform view_to_coords;
};

static void     gwy_shapes_finalize    (GObject *object);
static void     gwy_shapes_dispose     (GObject *object);
static void     gwy_shapes_set_property(GObject *object,
                                        guint prop_id,
                                        const GValue *value,
                                        GParamSpec *pspec);
static void     gwy_shapes_get_property(GObject *object,
                                        guint prop_id,
                                        GValue *value,
                                        GParamSpec *pspec);
static gboolean set_coords             (GwyShapes *shapes,
                                        GwyCoords *coords);
static gboolean set_focus              (GwyShapes *shapes,
                                        gint id);
static void     coords_item_inserted   (GwyShapes *shapes,
                                        guint id,
                                        GwyCoords *coords);
static void     coords_item_deleted    (GwyShapes *shapes,
                                        guint id,
                                        GwyCoords *coords);
static void     coords_item_changed    (GwyShapes *shapes,
                                        guint id,
                                        GwyCoords *coords);

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
    Shapes *priv = shapes->priv;

    CoordsTransform *transform = &priv->view_to_coords;
    gwy_set_user_func(NULL, NULL, NULL,
                      &transform->func, &transform->data, &transform->destroy);
    transform = &priv->coords_to_view;
    gwy_set_user_func(NULL, NULL, NULL,
                      &transform->func, &transform->data, &transform->destroy);

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
                               "item-changed", &coords_item_changed,
                               &priv->coords_item_changed_id,
                               G_CONNECT_SWAPPED,
                               NULL))
        return FALSE;

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
 * @shapes: The class of a group of geometrical shapes.
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

/**
 * gwy_shapes_set_coords_to_view_transform:
 * @shapes: A group of geometrical shapes.
 * @func: Function transforming @coords coordinates to view coordinates.
 * @user_data: Data passed to @func;
 * @destroy: Destroy notifier for @user_data.
 *
 * Sets the function for transformation of physical coordinates to view
 * coordinates for a group of geometrical shapes.
 *
 * The sizes of the arrays the function works with must match the actual
 * number of coordinates for the specific shapes and coords classes.
 **/
void
gwy_shapes_set_coords_to_view_transform(GwyShapes *shapes,
                                        GwyShapesTransformFunc func,
                                        gpointer user_data,
                                        GDestroyNotify destroy)
{
    g_return_if_fail(GWY_IS_SHAPES(shapes));

    CoordsTransform *transform = &shapes->priv->coords_to_view;
    if (!gwy_set_user_func(func, user_data, destroy,
                           &transform->func, &transform->data,
                           &transform->destroy))
        return;

    gwy_shapes_update(shapes);
}

/**
 * gwy_shapes_set_view_to_coords_transform:
 * @shapes: A group of geometrical shapes.
 * @func: Function transforming view coordinates to @coords coordinates.
 * @user_data: Data passed to @func;
 * @destroy: Destroy notifier for @user_data.
 *
 * Sets the function for transformation of view coordinates to physical
 * coordinates for a group of geometrical shapes.
 *
 * The sizes of the arrays the function works with must match the actual
 * number of coordinates for the specific shapes and coords classes.
 **/
void
gwy_shapes_set_view_to_coords_transform(GwyShapes *shapes,
                                        GwyShapesTransformFunc func,
                                        gpointer user_data,
                                        GDestroyNotify destroy)
{
    g_return_if_fail(GWY_IS_SHAPES(shapes));

    CoordsTransform *transform = &shapes->priv->view_to_coords;
    if (!gwy_set_user_func(func, user_data, destroy,
                           &transform->func, &transform->data,
                           &transform->destroy))
        return;

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
    g_return_if_fail(shapes->priv->coords_to_view.func);

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
 * gwy_shapes_coords_to_view:
 * @shapes: A group of geometrical shapes.
 * @coords: Physical coordinates of a single geometrical shape.
 * @view: Array to store the corresponding view coordinates to.
 *
 * Transforms physical coordinates of a single shape of a geometrical shape
 * group to view coordinates.
 **/
void
gwy_shapes_coords_to_view(const GwyShapes *shapes,
                          const gdouble *coords,
                          gdouble *view)
{
    g_return_if_fail(GWY_IS_SHAPES(shapes));
    g_return_if_fail(coords);
    g_return_if_fail(view);
    const CoordsTransform *transform = &shapes->priv->coords_to_view;
    g_return_if_fail(transform->func);
    transform->func(coords, view, transform->data);
}

/**
 * gwy_shapes_view_to_coords:
 * @shapes: A group of geometrical shapes.
 * @coords: Physical coordinates of a single geometrical shape.
 * @view: Array to store the corresponding view coordinates to.
 *
 * Transforms view coordinates of a single shape of a geometrical shape group
 * to physical coordinates.
 **/
void
gwy_shapes_view_to_coords(const GwyShapes *shapes,
                          const gdouble *coords,
                          gdouble *view)
{
    g_return_if_fail(GWY_IS_SHAPES(shapes));
    g_return_if_fail(coords);
    g_return_if_fail(view);
    const CoordsTransform *transform = &shapes->priv->view_to_coords;
    g_return_if_fail(transform->func);
    transform->func(coords, view, transform->data);
}

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
                        GdkEvent *event)
{
    g_return_val_if_fail(GWY_IS_SHAPES(shapes), FALSE);
    EventMethod method = GWY_SHAPES_GET_CLASS(shapes)->button_press;
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
                          GdkEvent *event)
{
    g_return_val_if_fail(GWY_IS_SHAPES(shapes), FALSE);
    EventMethod method = GWY_SHAPES_GET_CLASS(shapes)->button_release;
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
                         GdkEvent *event)
{
    g_return_val_if_fail(GWY_IS_SHAPES(shapes), FALSE);
    EventMethod method = GWY_SHAPES_GET_CLASS(shapes)->motion_notify;
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
                     GdkEvent *event)
{
    g_return_val_if_fail(GWY_IS_SHAPES(shapes), FALSE);
    EventMethod method = GWY_SHAPES_GET_CLASS(shapes)->key_press;
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
                       GdkEvent *event)
{
    g_return_val_if_fail(GWY_IS_SHAPES(shapes), FALSE);
    EventMethod method = GWY_SHAPES_GET_CLASS(shapes)->key_release;
    return method ? method(shapes, event) : FALSE;
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
    gwy_shapes_update(shapes);
}

static void
coords_item_changed(GwyShapes *shapes,
                    guint id,
                    GwyCoords *coords)
{
    g_assert(shapes->priv->coords == coords);
    ItemMethod method = GWY_SHAPES_GET_CLASS(shapes)->coords_item_changed;
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
 *
 * Object representing a group of selectable geometrical shapes.
 *
 * The #GwyShapes struct contains private data only and should be accessed
 * using the functions below.
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
 * @coords_item_inserted: Virtual method called when the @coords object
 *                        emits #GwyArray::item-inserted.
 * @coords_item_deleted: Virtual method called when the @coords object
 *                       emits #GwyArray::item-deleted.
 * @coords_item_changed: Virtual method called when the @coords object
 *                       emits #GwyArray::item-changed.
 *
 * Class of groups of selectable geometrical shapes.
 *
 * Specific, i.e. instantiable, subclasses have to set @coords_type and
 * implement at least the draw() method.  Of the user interaction methods they
 * should implement only those actually needed (possibly none if the subclass
 * is intended only for passive visualisation).
 *
 * Drawing of the shapes with gwy_shapes_draw() requires providing a function
 * for the transformation of physical coordinates to view coordinates using
 * gwy_shapes_set_coords_to_view_transform().  User interaction requires also
 * the reverse function that can be set with
 * gwy_shapes_set_view_to_coords_transform().
 **/

/**
 * GwyShapesTransformFunc:
 * @coords_from: Coordinates to transform.
 * @coords_to: Location to store the transformed coordinates.
 * @user_data: User data set with gwy_shapes_set_coords_to_view_transform()
 *             or gwy_shapes_set_view_to_coords_transform().
 *
 * Type of coordination transformation function.
 *
 * The number of items in @coords_from and @coords_to depends on the specific
 * subclass of #GwyShapes and #GwyCoords.  The view coordinates are normally
 * two per point because the screen is two-dimensional but the number of coords
 * coordinates may differ.  For instance, if horizontal profiles are selected
 * in two-dimensional data then @coords coordinates consists only the
 * y-coordinate.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
