/*
 *  $Id$
 *  Copyright (C) 2010 David Nečas (Yeti).
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
#include "libgwy/serialize.h"
#include "libgwy/coords.h"
#include "libgwy/object-internal.h"
#include "libgwy/array-internal.h"

enum { N_ITEMS = 2 };

enum {
    FINISHED,
    N_SIGNALS
};

struct _GwyCoordsPrivate {
    GwyUnit **units;
};

typedef struct {
    GwyArray *array;
    gdouble *data;
    const guint *dimension_map;
    const gdouble *transparam;
    guint shape_size;
    guint bitmask;
} TransformFuncData;

typedef struct _GwyCoordsPrivate Coords;

static void     gwy_coords_finalize         (GObject *object);
static void     gwy_coords_dispose          (GObject *object);
static void     gwy_coords_serializable_init(GwySerializableInterface *iface);
static gsize    gwy_coords_n_items          (GwySerializable *serializable);
static gsize    gwy_coords_itemize          (GwySerializable *serializable,
                                             GwySerializableItems *items);
static gboolean gwy_coords_construct        (GwySerializable *serializable,
                                             GwySerializableItems *items,
                                             GwyErrorList **error_list);
static GObject* gwy_coords_duplicate_impl   (GwySerializable *serializable);
static void     gwy_coords_assign_impl      (GwySerializable *destination,
                                             GwySerializable *source);
static gboolean class_supports_transforms   (GwyCoordsClass *klass,
                                             GwyCoordsTransformFlags flags);
static void     gwy_coords_translate_default(GwyCoords *coords,
                                             const GwyIntSet *indices,
                                             const gdouble *offsets);
static void     gwy_coords_flip_default     (GwyCoords *coords,
                                             const GwyIntSet *indices,
                                             guint axes);
static void     gwy_coords_scale_default    (GwyCoords *coords,
                                             const GwyIntSet *indices,
                                             const gdouble *factors);
static void     translate_func              (gint value,
                                             gpointer user_data);
static void     flip_func                   (gint value,
                                             gpointer user_data);
static void     scale_func                  (gint value,
                                             gpointer user_data);

static const GwySerializableItem serialize_items[N_ITEMS] = {
    { .name = "data",  .ctype = GWY_SERIALIZABLE_DOUBLE_ARRAY, },
    { .name = "units", .ctype = GWY_SERIALIZABLE_OBJECT_ARRAY, },
};

static guint signals[N_SIGNALS];

G_DEFINE_TYPE_EXTENDED
    (GwyCoords, gwy_coords, GWY_TYPE_ARRAY, G_TYPE_FLAG_ABSTRACT,
     GWY_IMPLEMENT_SERIALIZABLE(gwy_coords_serializable_init));

static void
gwy_coords_serializable_init(GwySerializableInterface *iface)
{
    iface->n_items   = gwy_coords_n_items;
    iface->itemize   = gwy_coords_itemize;
    iface->construct = gwy_coords_construct;
    iface->duplicate = gwy_coords_duplicate_impl;
    iface->assign    = gwy_coords_assign_impl;
}

static void
gwy_coords_class_init(GwyCoordsClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    g_type_class_add_private(klass, sizeof(Coords));

    gobject_class->dispose = gwy_coords_dispose;
    gobject_class->finalize = gwy_coords_finalize;

    klass->translate = gwy_coords_translate_default;
    klass->flip = gwy_coords_flip_default;
    klass->scale = gwy_coords_scale_default;

    /**
     * GwyCoords::finished:
     * @gwycoords: The #GwyCoords which received the signal.
     *
     * The ::finished signal is emitted by a coords user that is continuously
     * modifying it (typically by dragging the shape in a data display widget)
     * when it is done with the modifications and the coords are in the final
     * state.
     **/
    signals[FINISHED]
        = g_signal_new_class_handler("finished",
                                     G_OBJECT_CLASS_TYPE(klass),
                                     G_SIGNAL_RUN_FIRST,
                                     NULL, NULL, NULL,
                                     g_cclosure_marshal_VOID__VOID,
                                     G_TYPE_NONE, 0);
}

static void
gwy_coords_init(GwyCoords *coords)
{
    coords->priv = G_TYPE_INSTANCE_GET_PRIVATE(coords, GWY_TYPE_COORDS, Coords);
}

static void
gwy_coords_finalize(GObject *object)
{
    GwyCoords *coords = GWY_COORDS(object);
    Coords *priv = coords->priv;
    guint dimension = GWY_COORDS_GET_CLASS(coords)->dimension;
    if (priv->units) {
        g_slice_free1(dimension*sizeof(GwyUnit*), priv->units);
        priv->units = NULL;
    }
    G_OBJECT_CLASS(gwy_coords_parent_class)->finalize(object);
}

static void
gwy_coords_dispose(GObject *object)
{
    GwyCoords *coords = GWY_COORDS(object);
    Coords *priv = coords->priv;
    guint dimension = GWY_COORDS_GET_CLASS(coords)->dimension;
    if (priv->units)
        for (guint i = 0; i < dimension; i++)
            GWY_OBJECT_UNREF(priv->units[i]);
    G_OBJECT_CLASS(gwy_coords_parent_class)->dispose(object);
}

static void
ensure_units(Coords *priv,
             guint dimension,
             gboolean create_objects)
{
    // Fast path without getting class data
    if (priv->units && !create_objects)
        return;

    if (!priv->units)
        priv->units = g_slice_alloc0(dimension*sizeof(GwyUnit*));

    if (!create_objects)
        return;

    for (guint i = 0; i < dimension; i++) {
        if (!priv->units[i])
            priv->units[i] = gwy_unit_new();
    }
}

static gsize
gwy_coords_n_items(GwySerializable *serializable)
{
    GwyCoords *coords = GWY_COORDS(serializable);
    Coords *priv = coords->priv;
    gsize n = N_ITEMS;

    // We must have all units or none but not something between.
    if (priv->units) {
        guint dimension = GWY_COORDS_GET_CLASS(coords)->dimension;
        ensure_units(priv, dimension, TRUE);
        for (guint i = 0; i < dimension; i++)
            n += gwy_serializable_n_items(GWY_SERIALIZABLE(priv->units[i]));
    }

    return n;
}

static gsize
gwy_coords_itemize(GwySerializable *serializable,
                   GwySerializableItems *items)
{
    g_return_val_if_fail(items->len - items->n >= N_ITEMS, 0);

    GwyCoords *coords = GWY_COORDS(serializable);
    GwyArray *array = GWY_ARRAY(coords);
    GwyCoordsClass *klass = GWY_COORDS_GET_CLASS(coords);
    GwySerializableItem it;
    guint n = 0, size = gwy_array_size(array);

    if (size) {
        g_return_val_if_fail(items->len - items->n, 0);
        it = serialize_items[0];
        it.value.v_double_array = gwy_array_get_data(array);
        it.array_size = size * klass->shape_size;
        items->items[items->n++] = it;
        n++;
    }

    Coords *priv = coords->priv;
    if (priv->units) {
        g_return_val_if_fail(items->len - items->n, 0);
        guint dimension = klass->dimension;
        it = serialize_items[1];
        it.value.v_object_array = (GObject**)priv->units;
        it.array_size = dimension;
        items->items[items->n++] = it;
        n++;

        for (guint i = 0; i < dimension; i++) {
            g_return_val_if_fail(items->len - items->n, 0);
            gwy_serializable_itemize(GWY_SERIALIZABLE(priv->units[i]), items);
            n++;
        }
    }

    return n;
}

static gboolean
gwy_coords_construct(GwySerializable *serializable,
                     GwySerializableItems *items,
                     GwyErrorList **error_list)
{
    GwySerializableItem its[N_ITEMS];
    memcpy(its, serialize_items, sizeof(serialize_items));
    gwy_deserialize_filter_items(its, N_ITEMS, items, NULL,
                                 "GwyCoords", error_list);

    gboolean ok = FALSE;

    GwyCoords *coords = GWY_COORDS(serializable);
    // XXX: This is a bit tricky.  We know what the real class is, it's the
    // class of @serializable because the object is already fully constructed
    // as far as GObject is concerned.  So we can check the number of items.
    guint shape_size = GWY_COORDS_GET_CLASS(coords)->shape_size;
    if (!shape_size) {
        g_critical("Object size of coords type %s is zero.",
                   G_OBJECT_TYPE_NAME(coords));
        // This is really a programmer's error, so do not report it in
        // @error_list, just set teeth and leave the coords empty.
        // Let it crash somewhere else...
        ok = TRUE;
        goto fail;
    }

    if (its[0].array_size % shape_size) {
        gwy_error_list_add(error_list, GWY_DESERIALIZE_ERROR,
                           GWY_DESERIALIZE_ERROR_INVALID,
                           _("Coords data length is %lu which is not "
                             "a multiple of %u as is expected for coords "
                             "type ‘%s’."),
                           (gulong)its[0].array_size, shape_size,
                           G_OBJECT_TYPE_NAME(coords));
        goto fail;
    }
    if (its[0].array_size) {
        GwyArray *array = GWY_ARRAY(coords);
        _gwy_array_set_data_silent(array,
                                   its[0].value.v_double_array,
                                   its[0].array_size/shape_size);
    }

    guint dimension = GWY_COORDS_GET_CLASS(coords)->dimension;
    if (its[1].array_size) {
        if (its[1].array_size != dimension) {
            gwy_error_list_add(error_list, GWY_DESERIALIZE_ERROR,
                               GWY_DESERIALIZE_ERROR_INVALID,
                               _("Coords of type type ‘%s’ "
                                 "contains %lu dimension units that do not "
                                 "correspond to its dimension %u."),
                               G_OBJECT_TYPE_NAME(coords),
                               (gulong)its[0].array_size, dimension);
            goto fail;
        }
        if (!_gwy_check_object_component(its + 1, coords,
                                         GWY_TYPE_UNIT, error_list))
            goto fail;

        Coords *priv = coords->priv;
        ensure_units(priv, dimension, FALSE);
        for (guint i = 0; i < dimension; i++) {
            GWY_OBJECT_UNREF(priv->units[i]);
            priv->units[i] = (GwyUnit*)its[1].value.v_object_array[i];
            its[1].value.v_object_array[i] = NULL;
        }
        its[1].value.v_object_array = NULL;
    }

    ok = TRUE;

fail:
    GWY_FREE(its[0].value.v_double_array);
    for (guint i = 0; i < its[1].array_size; i++)
        GWY_OBJECT_UNREF(its[1].value.v_object_array);
    GWY_FREE(its[1].value.v_object_array);
    return ok;
}

// This creates the proper type but assigns just the coordinate data.
// Subclasses that have other (non-coordinate) data must override this.
static GObject*
gwy_coords_duplicate_impl(GwySerializable *serializable)
{
    GType type = G_OBJECT_TYPE(serializable);
    GwyArray *array = GWY_ARRAY(serializable);
    gsize size = gwy_array_size(array);
    const gdouble *array_data = gwy_array_get_data(array);

    GObject *duplicate = g_object_newv(type, 0, NULL);
    _gwy_array_set_data_silent(GWY_ARRAY(duplicate), array_data, size);

    return duplicate;
}

static void
gwy_coords_assign_impl(GwySerializable *destination,
                       GwySerializable *source)
{
    GwyArray *dest = GWY_ARRAY(destination);
    GwyArray *src = GWY_ARRAY(source);
    gwy_array_set_data(dest, gwy_array_get_data(src), gwy_array_size(src));

    Coords *spriv = GWY_COORDS(source)->priv,
           *dpriv = GWY_COORDS(destination)->priv;

    if (!spriv->units && !dpriv->units)
        return;

    guint dimension = GWY_COORDS_GET_CLASS(destination)->dimension;
    ensure_units(dpriv, dimension, FALSE);
    for (guint i = 0; i < dimension; i++)
        _gwy_assign_units(dpriv->units + i, spriv->units[i]);
}

/**
 * gwy_coords_shape_size:
 * @coords: A group of coordinates of some geometrical objects.
 *
 * Obtains the number of floating point values representing one object or shape
 * of the coords.
 *
 * This value is the same for all coords of a specific type.
 *
 * Returns: The number of values a single shape takes.
 **/
guint
gwy_coords_shape_size(const GwyCoords *coords)
{
    GwyCoordsClass *klass = GWY_COORDS_GET_CLASS(coords);
    g_return_val_if_fail(klass, 0);
    return klass->shape_size;
}

/**
 * gwy_coords_dimension:
 * @coords: A group of coordinates of some geometrical objects.
 *
 * Gets the coords dimension.
 *
 * Coords dimension is the number of different coordinates (dimensions)
 * in the coords.  For instance, an x-range or y-range coords on graph are
 * one-dimensional while a rectangular coords are two-dimensional.  The
 * dimension is always smaller than or equal to the shape size.
 *
 * This value is the same for all coords of a specific type.
 *
 * Returns: The number of different dimensions.
 **/
guint
gwy_coords_dimension(const GwyCoords *coords)
{
    GwyCoordsClass *klass = GWY_COORDS_GET_CLASS(coords);
    g_return_val_if_fail(klass, 0);
    return klass->dimension;
}

/**
 * gwy_coords_dimension_map:
 * @coords: A group of coordinates of some geometrical objects.
 *
 * Obtains the map between shape coordinates and their units.
 *
 * The unit map assigns physical units to each of the number describing a
 * single shape/object (i.e. the coordinate).  The units can be obtained with
 * gwy_coords_get_units().
 *
 * For example, if the 2nd item in the returned item is 0 then the units
 * of the second number in the coords object can be obtained by
 * <literal>gwy_coords_get_units(coords, 0);</literal> which is the same
 * as <literal>gwy_coords_get_mapped_units(coords, 2);</literal>.
 *
 * This map is the same for all coords of a specific type.
 *
 * Returns: Array of gwy_coords_shape_size() items owned by the coords
 *          class, containing the units map.
 **/
const guint*
gwy_coords_dimension_map(const GwyCoords *coords)
{
    GwyCoordsClass *klass = GWY_COORDS_GET_CLASS(coords);
    g_return_val_if_fail(klass, 0);
    return klass->dimension_map;
}

/**
 * gwy_coords_clear:
 * @coords: A group of coordinates of some geometrical objects.
 *
 * Removes all objects in a coords.
 **/
void
gwy_coords_clear(GwyCoords *coords)
{
    g_return_if_fail(GWY_IS_COORDS(coords));
    GwyArray *array = GWY_ARRAY(coords);
    gwy_array_delete(array, 0, gwy_array_size(array));
}

/**
 * gwy_coords_get:
 * @coords: A group of coordinates of some geometrical objects.
 * @i: Object index.
 * @data: Array of length at least gwy_coords_shape_size() to store the
 *        data to.
 *
 * Obtains the data of a single coords object.
 *
 * The interpretation of the data depends on the coords type.
 *
 * Returns: %TRUE if there is an @i-th object and @data filled with values.
 *          %FALSE if these is no such object and @data was left untouched.
 **/
gboolean
gwy_coords_get(const GwyCoords *coords,
               guint i,
               gdouble *data)
{
    g_return_val_if_fail(GWY_IS_COORDS(coords), FALSE);
    gpointer item = gwy_array_get(GWY_ARRAY(coords), i);
    if (item) {
        g_return_val_if_fail(data, FALSE);
        guint shape_size = GWY_COORDS_GET_CLASS(coords)->shape_size;
        gwy_assign(data, item, shape_size);
    }
    return item != NULL;
}

/**
 * gwy_coords_set:
 * @coords: A group of coordinates of some geometrical objects.
 * @i: Object index.  It must correspond to an existing coords object or
 *     it can point after the end of the coords.  In the second case the
 *     object will be appended to the end.
 * @data: Data of a single coords object size in an array of size
 *        gwy_coords_shape_size().
 *
 * Sets the data of a single coords object.
 **/
void
gwy_coords_set(GwyCoords *coords,
               guint i,
               const gdouble *data)
{
    g_return_if_fail(GWY_IS_COORDS(coords));
    GwyArray *array = GWY_ARRAY(coords);
    guint n = gwy_array_size(array);
    if (i < n)
        gwy_array_replace1(GWY_ARRAY(coords), i, data);
    else if (i == n)
        gwy_array_append1(GWY_ARRAY(coords), data);
    else {
        g_critical("Coords object index %u is beyond the end of the data.", i);
        gwy_array_append1(GWY_ARRAY(coords), data);
    }
}

/**
 * gwy_coords_delete:
 * @coords: A group of coordinates of some geometrical objects.
 * @i: Object index.
 *
 * Deletes a signle coords object.
 **/
void
gwy_coords_delete(GwyCoords *coords,
                  guint i)
{
    g_return_if_fail(GWY_IS_COORDS(coords));
    gwy_array_delete1(GWY_ARRAY(coords), i);
}

/**
 * gwy_coords_size:
 * @coords: A group of coordinates of some geometrical objects.
 *
 * Obtains the number of objects in a group of coordinates.
 *
 * Returns: The number of objects in @coords.
 **/
guint
gwy_coords_size(const GwyCoords *coords)
{
    g_return_val_if_fail(GWY_IS_COORDS(coords), 0);
    return gwy_array_size(GWY_ARRAY(coords));
}

/**
 * gwy_coords_get_data:
 * @coords: A group of coordinates of some geometrical objects.
 * @data: Array of sufficient length to to store the complete coords data
 *        to.
 *
 * Obtains the data of an entire group of coords.
 **/
void
gwy_coords_get_data(const GwyCoords *coords,
                    gdouble *data)
{
    g_return_if_fail(GWY_IS_COORDS(coords));
    g_return_if_fail(data);
    guint shape_size = GWY_COORDS_GET_CLASS(coords)->shape_size;
    GwyArray *array = GWY_ARRAY(coords);
    const gdouble *array_data = gwy_array_get_data(array);
    gsize size = gwy_array_size(array);
    g_assert(array_data);
    gwy_assign(data, array_data, size*shape_size);
}

/**
 * gwy_coords_set_data:
 * @coords: A group of coordinates of some geometrical objects.
 * @n: New coords size (number of objects in @data).
 * @data: New coords data.
 *
 * Sets the data of an entire coords.
 *
 * Note this can emit lots of signals, see gwy_array_set_data().
 **/
void
gwy_coords_set_data(GwyCoords *coords,
                    guint n,
                    const gdouble *data)
{
    g_return_if_fail(GWY_IS_COORDS(coords));
    g_return_if_fail(data || !n);
    gwy_array_set_data(GWY_ARRAY(coords), data, n);
}

/**
 * gwy_coords_filter:
 * @coords: A group of coordinates of some geometrical objects.
 * @filter: (scope call):
 *          Function returning %TRUE for objects that should be kept, %FALSE
 *          for objects that should be deleted.
 * @user_data: Data passed to @filter;
 *
 * Deletes coords objects matching certain criteria.
 **/
void
gwy_coords_filter(GwyCoords *coords,
                  GwyCoordsFilterFunc filter,
                  gpointer user_data)
{
    g_return_if_fail(GWY_IS_COORDS(coords));
    if (!filter)
        return;

    GwyArray *array = GWY_ARRAY(coords);
    guint n = gwy_array_size(array);
    for (guint i = n; i; i--) {
        if (!filter(coords, i-1, user_data))
            gwy_array_delete1(array, i-1);
    }
}

/**
 * gwy_coords_get_units:
 * @coords: A group of coordinates of some geometrical objects.
 * @i: Dimension index.
 *
 * Gets the units corresponding to a coords dimension.
 *
 * Returns: (transfer none):
 *          The units of the @i-th dimension.
 **/
GwyUnit*
gwy_coords_get_units(GwyCoords *coords,
                     guint i)
{
    g_return_val_if_fail(GWY_IS_COORDS(coords), NULL);
    guint dimension = GWY_COORDS_GET_CLASS(coords)->dimension;
    g_return_val_if_fail(i < dimension, NULL);
    Coords *priv = coords->priv;
    ensure_units(priv, dimension, FALSE);
    if (!priv->units[i])
        priv->units[i] = gwy_unit_new();
    return priv->units[i];
}

/**
 * gwy_coords_get_mapped_units:
 * @coords: A group of coordinates of some geometrical objects.
 * @i: Coordinate index.
 *
 * Gets the units corresponding to a specific coords coordinate.
 *
 * Returns: (transfer none):
 *          The units of the @i-th coordinate.
 **/
GwyUnit*
gwy_coords_get_mapped_units(GwyCoords *coords,
                            guint i)
{
    g_return_val_if_fail(GWY_IS_COORDS(coords), NULL);
    GwyCoordsClass *klass = GWY_COORDS_GET_CLASS(coords);
    guint shape_size = klass->shape_size;
    g_return_val_if_fail(i < shape_size, NULL);
    g_return_val_if_fail(klass->dimension_map, NULL);
    guint mi = klass->dimension_map[i];
    Coords *priv = coords->priv;
    guint dimension = klass->dimension;
    ensure_units(priv, dimension, FALSE);
    if (!priv->units[mi])
        priv->units[mi] = gwy_unit_new();
    return priv->units[mi];
}

/**
 * gwy_coords_finished:
 * @coords: A group of coordinates of some geometrical objects.
 *
 * Emits signal ::finished on a coords.
 **/
void
gwy_coords_finished(GwyCoords *coords)
{
    g_return_if_fail(GWY_IS_COORDS(coords));
    g_signal_emit(coords, signals[FINISHED], 0);
}

/**
 * gwy_coords_translate:
 * @coords: A group of coordinates of some geometrical objects.
 * @indices: (allow-none):
 *           Set of indices of objects to transform.  Passing %NULL implies
 *           all objects are to be transformed.
 * @offsets: Array of GwyCoordsClass @dimension field specifying the
 *           offsets in each dimension.
 *
 * Translates selected or all objects in a coords.
 **/
void
gwy_coords_translate(GwyCoords *coords,
                     GwyIntSet *indices,
                     const gdouble *offsets)
{
    g_return_if_fail(GWY_IS_COORDS(coords));
    g_return_if_fail(!indices || GWY_IS_INT_SET(indices));
    g_return_if_fail(offsets);

    GwyCoordsClass *klass = GWY_COORDS_GET_CLASS(coords);
    g_return_if_fail(klass->translate);
    klass->translate(coords, indices, offsets);
}

/**
 * gwy_coords_flip:
 * @coords: A group of coordinates of some geometrical objects.
 * @indices: (allow-none):
 *           Set of indices of objects to transform.  Passing %NULL implies
 *           all objects are to be transformed.
 * @axes: Bitmask of axes (dimensions) to flip, starting from the lowest bit.
 *
 * Flips selected or all objects in a coords.
 **/
void
gwy_coords_flip(GwyCoords *coords,
                GwyIntSet *indices,
                guint axes)
{
    g_return_if_fail(GWY_IS_COORDS(coords));
    g_return_if_fail(!indices || GWY_IS_INT_SET(indices));

    GwyCoordsClass *klass = GWY_COORDS_GET_CLASS(coords);
    g_return_if_fail(klass->flip);
    if (!(axes & ((1 << klass->dimension) - 1)))
        return;
    klass->flip(coords, indices, axes);
}

/**
 * gwy_coords_scale:
 * @coords: A group of coordinates of some geometrical objects.
 * @indices: (allow-none):
 *           Set of indices of objects to transform.  Passing %NULL implies
 *           all objects are to be transformed.
 * @factors: Array of GwyCoordsClass @dimension field specifying the
 *           scaling factors in each dimension.
 *
 * Scales selected or all objects in a coords.
 **/
void
gwy_coords_scale(GwyCoords *coords,
                 GwyIntSet *indices,
                 const gdouble *factors)
{
    g_return_if_fail(GWY_IS_COORDS(coords));
    g_return_if_fail(!indices || GWY_IS_INT_SET(indices));
    g_return_if_fail(factors);

    GwyCoordsClass *klass = GWY_COORDS_GET_CLASS(coords);
    g_return_if_fail(klass->scale);
    klass->scale(coords, indices, factors);
}

/**
 * gwy_coords_can_transform:
 * @coords: A group of coordinates of some geometrical objects.
 * @transforms: Bitmask from %GwyCoordsTransformFlags enum specifying which
 *              transformatons are requested.
 *
 * Checks whether a group of coordinates of some geometrical objects can be
 * transformed using given transformation.
 *
 * Not all coordinate types can be transformed using all transformations.  For
 * instance, rectangles and ellipses cannot be rotated.
 *
 * Returns: %TRUE if @coords supports all transformations given in @transforms;
 *          %FALSE if it does not support some.
 **/
gboolean
gwy_coords_can_transform(GwyCoords *coords,
                         GwyCoordsTransformFlags transforms)
{
    g_return_val_if_fail(GWY_IS_COORDS(coords), FALSE);
    return class_supports_transforms(GWY_COORDS_GET_CLASS(coords), transforms);
}

/**
 * gwy_coords_class_can_transform:
 * @klass: Klass of group of coordinates of some geometrical objects.
 * @transforms: Bitmask from %GwyCoordsTransformFlags enum specifying which
 *              transformatons are requested.
 *
 * Checks whether given class of groups of coordinates of some geometrical
 * objects can be transformed using given transformation.
 *
 * Not all coordinate types can be transformed using all transformations.  For
 * instance, rectangles and ellipses cannot be rotated.
 *
 * Returns: %TRUE if coordinates of class @klass support all transformations
 *          given in @transforms; %FALSE if it doe not support some.
 **/
gboolean
gwy_coords_class_can_transform(GwyCoordsClass *klass,
                               GwyCoordsTransformFlags transforms)
{
    g_return_val_if_fail(GWY_IS_COORDS_CLASS(klass), FALSE);
    return class_supports_transforms(klass, transforms);
}

static gboolean
class_supports_transforms(GwyCoordsClass *klass,
                          GwyCoordsTransformFlags flags)
{
    if ((flags & GWY_COORDS_TRANSFORM_TRANSLATE) && !klass->translate)
        return FALSE;
    if ((flags & GWY_COORDS_TRANSFORM_FLIP) && !klass->flip)
        return FALSE;
    if ((flags & GWY_COORDS_TRANSFORM_SCALE) && !klass->scale)
        return FALSE;
    // TODO: Other transformations
    return TRUE;
}

static void
gwy_coords_translate_default(GwyCoords *coords,
                             const GwyIntSet *indices,
                             const gdouble *offsets)
{
    GwyCoordsClass *klass = GWY_COORDS_GET_CLASS(coords);
    GwyArray *array = GWY_ARRAY(coords);
    const guint *dimension_map = klass->dimension_map;
    guint shape_size = klass->shape_size;
    gdouble *data = (gdouble*)gwy_array_get_data(array);
    g_assert(dimension_map);

    if (!indices) {
        guint n = gwy_array_size(array);
        for (guint i = 0; i < n; i++) {
            const guint *umap = dimension_map;
            for (guint j = shape_size; j; j--, data++, umap++)
                *data += offsets[*umap];
            gwy_array_updated(array, i);
        }
        return;
    }

    TransformFuncData tfdata = {
        .array = array,
        .data = data,
        .transparam = offsets,
        .shape_size = shape_size,
        .dimension_map = dimension_map,
    };
    gwy_int_set_foreach(indices, translate_func, &tfdata);
}

static void
gwy_coords_flip_default(GwyCoords *coords,
                        const GwyIntSet *indices,
                        const guint axes)
{
    GwyCoordsClass *klass = GWY_COORDS_GET_CLASS(coords);
    GwyArray *array = GWY_ARRAY(coords);
    const guint *dimension_map = klass->dimension_map;
    guint shape_size = klass->shape_size;
    gdouble *data = (gdouble*)gwy_array_get_data(array);
    g_assert(dimension_map);

    if (!indices) {
        guint n = gwy_array_size(array);
        for (guint i = 0; i < n; i++) {
            const guint *umap = dimension_map;
            for (guint j = shape_size; j; j--, data++, umap++)
                if (axes & (1 << *umap))
                    *data = -*data;
            gwy_array_updated(array, i);
        }
        return;
    }

    TransformFuncData tfdata = {
        .array = array,
        .data = data,
        .bitmask = axes,
        .shape_size = shape_size,
        .dimension_map = dimension_map,
    };
    gwy_int_set_foreach(indices, flip_func, &tfdata);
}

static void
gwy_coords_scale_default(GwyCoords *coords,
                         const GwyIntSet *indices,
                         const gdouble *factors)
{
    GwyCoordsClass *klass = GWY_COORDS_GET_CLASS(coords);
    GwyArray *array = GWY_ARRAY(coords);
    const guint *dimension_map = klass->dimension_map;
    guint shape_size = klass->shape_size;
    gdouble *data = (gdouble*)gwy_array_get_data(array);
    g_assert(dimension_map);

    if (!indices) {
        guint n = gwy_array_size(array);
        for (guint i = 0; i < n; i++) {
            const guint *umap = dimension_map;
            for (guint j = shape_size; j; j--, data++, umap++)
                *data *= factors[*umap];
            gwy_array_updated(array, i);
        }
        return;
    }

    TransformFuncData tfdata = {
        .array = array,
        .data = data,
        .transparam = factors,
        .shape_size = shape_size,
        .dimension_map = dimension_map,
    };
    gwy_int_set_foreach(indices, scale_func, &tfdata);
}

static void
translate_func(gint value, gpointer user_data)
{
    TransformFuncData *tfdata = (TransformFuncData*)user_data;
    const gdouble *offsets = tfdata->transparam;
    const guint *umap = tfdata->dimension_map;
    guint shape_size = tfdata->shape_size;
    gdouble *data = tfdata->data + value*shape_size;

    for (guint j = shape_size; j; j--, data++, umap++)
        *data += offsets[*umap];
    gwy_array_updated(tfdata->array, value);
}

static void
flip_func(gint value, gpointer user_data)
{
    TransformFuncData *tfdata = (TransformFuncData*)user_data;
    guint axes = tfdata->bitmask;
    const guint *umap = tfdata->dimension_map;
    guint shape_size = tfdata->shape_size;
    gdouble *data = tfdata->data + value*shape_size;

    for (guint j = shape_size; j; j--, data++, umap++) {
        if (axes & (1 << *umap))
            *data = -*data;
    }
    gwy_array_updated(tfdata->array, value);
}

static void
scale_func(gint value, gpointer user_data)
{
    TransformFuncData *tfdata = (TransformFuncData*)user_data;
    const gdouble *factors = tfdata->transparam;
    const guint *umap = tfdata->dimension_map;
    guint shape_size = tfdata->shape_size;
    gdouble *data = tfdata->data + value*shape_size;

    for (guint j = shape_size; j; j--, data++, umap++)
        *data *= factors[*umap];
    gwy_array_updated(tfdata->array, value);
}

/**
 * SECTION: coords
 * @title: GwyCoords
 * @short_description: Base class for coordinates of geometrical objects.
 *
 * #GwyCoords represents a group of coordinates of geometrical objects/shapes
 * selected on data.  Each object/shape is represented by a fixed-size (for a
 * specific coords type) chunk of floating point values, i.e. coordinates
 * and/or dimensions, that describe the shape.  The meaning of these values
 * differs among individual coords types.  #GwyCoords methods perform common
 * operations that do not require the knowledge of the data interpretation.
 *
 * #GwyCoords is a subclass of #GwyArray which offers a wider range of
 * data managing methods that you can freely use.  The #GwyCoords methods
 * are somewhat more type-safe as they have #gdouble* arguments and they ensure
 * GwyArray::item-updated is emitted when necessary while you might need to do
 * this manually with #GwyArrays if you modify the data directly.
 **/

/**
 * GwyCoords:
 *
 * Object representing a group of coordinates of some geometrical objects.
 *
 * The #GwyCoords struct contains private data only and should be accessed
 * using the functions below.
 **/

/**
 * GwyCoordsClass:
 * @shape_size: Number of double values used to described one geometrical
 *              object/shape.
 * @dimension: Number of different coodinates in the coords.
 * @dimension_map: Map assigning dimensios, corresponding also to indices
 *                 and used with gwy_coords_get_units(), to individual numbers
 *                 (coordinates) in the coords objects.  The generic
 *                 implementations of transformation methods use this array to
 *                 determine which coordinate corresponds to which offset,
 *                 scale, etc.
 * @translate: Virtual method implementing gwy_coords_translate().  If it is
 *             implemented the class will report
 *             %GWY_COORDS_TRANSFORM_TRANSLATE capability.  In general, this
 *             should be supported by all subclasses.
 * @flip: Virtual method implementing gwy_coords_flip().  If it is implemented
 *        the class will report %GWY_COORDS_TRANSFORM_FLIP capability.
 * @scale: Virtual method implementing gwy_coords_scale().  If it is
 *         implemented the class will report %GWY_COORDS_TRANSFORM_SCALE
 *         capability.
 *
 * Class of groups coordinates of some geometrical objects.
 *
 * Specific, i.e. instantiable, subclasses have to set the data members
 * @shape_size, @dimension and @dimension_map.
 *
 * Transformation method often do not need to be implemented speficically.
 * They have default generic implementations that work for coordinates that are
 * plain bunches of point coordinates defining the convex hull of some shape.
 * So they are fine for points, lines, boxes, ... in an arbitrary dimension.
 * If a subclass does <emphasis>not</emphasis> support some transformation that
 * has a default implementation it must set the method to %NULL explicitly to
 * indicate this type of transformation is not possible.
 **/

/**
 * gwy_coords_duplicate:
 * @coords: A group of coordinates of some geometrical objects.
 *
 * Duplicates a group coordinates of some geometrical objects.
 *
 * This is a convenience wrapper of gwy_serializable_duplicate().
 **/

/**
 * gwy_coords_assign:
 * @dest: Destination group of coordinates of some geometrical objects.
 * @src: Source group of coordinates of some geometrical objects.
 *
 * Copies the value of a group of coordinates of some geometrical objects.
 *
 * This is a convenience wrapper of gwy_serializable_assign().
 **/

/**
 * GwyCoordsTransformFlags:
 * @GWY_COORDS_TRANSFORM_TRANSLATE: Translation is possible, this should be
 *                                  possible with all coordinate types.
 * @GWY_COORDS_TRANSFORM_FLIP: Flipping around horizontal and vertical axes
 *                             is possible (this implies also rotation by π).
 * @GWY_COORDS_TRANSFORM_TRANSPOSE: Transposing congruence transformations are
 *                                  possible, see
 *                                  gwy_plane_congruence_is_transposition().
 *                             is possible (this implies also rotation by π).
 * @GWY_COORDS_TRANSFORM_SCALE: General scaling is possible, non-uniform along
 *                              different axes.
 * @GWY_COORDS_TRANSFORM_ROTATE: Rotation around arbitrary points about
 *                               arbitrary axes is possible.  This implies any
 *                               congruence transformation is possible.
 * @GWY_COORDS_TRANSFORM_FUNCTION: Arbitrary point-wise transformation is
 *                                 possbile.
 *
 * Flags describing the types of transformation that various coordinates
 * classes support.
 **/

/**
 * GwyCoordsFilterFunc:
 * @coords: A group of coordinates of some geometrical objects.
 * @i: Index of the coords object to consider.
 * @user_data: User data passed to gwy_coords_filter().
 *
 * Type of coords filtering function.
 *
 * The function must not modify the coords.
 *
 * Returns: %TRUE for objects that should be kept, %FALSE for objects that
 *          should be removed.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
