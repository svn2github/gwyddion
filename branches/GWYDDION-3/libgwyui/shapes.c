/*
 *  $Id$
 *  Copyright (C) 2011 David Nečas (Yeti).
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
static void     gwy_shapes_updated     (GwyShapes *shaped);

static guint shapes_signals[N_SIGNALS];
static GParamSpec *shapes_pspecs[N_PROPS];

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

    shapes_pspecs[PROP_COORDS]
        = g_param_spec_object("coords",
                              "Coords",
                              "Coordinates of the shape.",
                              // FIXME: This is wrong.  Do it in each specific
                              // subclass with the correct type?
                              GWY_TYPE_COORDS,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    shapes_pspecs[PROP_FOCUS]
        = g_param_spec_int("focus",
                           "Focus",
                           "Index of focused shape that only can be "
                           "edited by the user.  Negative value means "
                           "all shapes can be edited.",
                           -1, G_MAXINT, -1,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    for (guint i = 1; i < N_PROPS; i++)
        g_object_class_install_property(gobject_class, i, shapes_pspecs[i]);

    // FIXME: This is too simplistic.  We may want multiple objects to be
    // selected.
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
    shapes_signals[SHAPE_SELECTED]
        = g_signal_new_class_handler("shape-selected",
                                     G_OBJECT_CLASS_TYPE(klass),
                                     G_SIGNAL_RUN_FIRST,
                                     NULL, NULL, NULL,
                                     g_cclosure_marshal_VOID__INT,
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

    gwy_shapes_updated(shapes);
    return TRUE;
}

void
gwy_shapes_set_coords(GwyShapes *shapes,
                      GwyCoords *coords)
{
    g_return_if_fail(GWY_IS_SHAPES(shapes));
    if (!set_coords(shapes, coords))
        return;

    g_object_notify_by_pspec(G_OBJECT(shapes), shapes_pspecs[PROP_COORDS]);
}

GwyCoords*
gwy_shapes_get_coords(GwyShapes *shapes)
{
    g_return_val_if_fail(GWY_IS_SHAPES(shapes), NULL);
    return shapes->priv->coords;
}

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

    gwy_shapes_updated(shapes);
}

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

    gwy_shapes_updated(shapes);
}

void
gwy_shapes_draw(GwyShapes *shapes,
                cairo_t *cr)
{
    GwyShapesClass *klass = GWY_SHAPES_GET_CLASS(shapes);
    if (klass->draw)
        klass->draw(shapes, cr);
}

void
gwy_shapes_set_focus(GwyShapes *shapes,
                     gint id)
{
    g_return_if_fail(GWY_IS_SHAPES(shapes));
    if (!set_focus(shapes, id))
        return;

    g_object_notify_by_pspec(G_OBJECT(shapes), shapes_pspecs[PROP_FOCUS]);
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

gboolean
gwy_shapes_button_press(GwyShapes *shapes,
                        GdkEvent *event)
{
    g_return_val_if_fail(GWY_IS_SHAPES(shapes), FALSE);
    EventMethod method = GWY_SHAPES_GET_CLASS(shapes)->button_press;
    return method ? method(shapes, event) : FALSE;
}

gboolean
gwy_shapes_button_release(GwyShapes *shapes,
                          GdkEvent *event)
{
    g_return_val_if_fail(GWY_IS_SHAPES(shapes), FALSE);
    EventMethod method = GWY_SHAPES_GET_CLASS(shapes)->button_release;
    return method ? method(shapes, event) : FALSE;
}

gboolean
gwy_shapes_motion_notify(GwyShapes *shapes,
                         GdkEvent *event)
{
    g_return_val_if_fail(GWY_IS_SHAPES(shapes), FALSE);
    EventMethod method = GWY_SHAPES_GET_CLASS(shapes)->motion_notify;
    return method ? method(shapes, event) : FALSE;
}

gboolean
gwy_shapes_key_press(GwyShapes *shapes,
                     GdkEvent *event)
{
    g_return_val_if_fail(GWY_IS_SHAPES(shapes), FALSE);
    EventMethod method = GWY_SHAPES_GET_CLASS(shapes)->key_press;
    return method ? method(shapes, event) : FALSE;
}

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
    gwy_shapes_updated(shapes);
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
    gwy_shapes_updated(shapes);
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
    gwy_shapes_updated(shapes);
}

static void
gwy_shapes_updated(GwyShapes *shapes)
{
    g_printerr("Shapes %p (%s) updated.\n", shapes, G_OBJECT_TYPE_NAME(shapes));
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
 * @coords_type:
 * @draw:
 * @button_press:
 * @button_release:
 * @motion_notify:
 * @key_press:
 * @key_release:
 *
 * Class of groups of selectable geometrical shapes.
 *
 * Specific, i.e. instantiable, subclasses have to set @coords_type and, to be
 * actually useful, at least the draw() method.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
