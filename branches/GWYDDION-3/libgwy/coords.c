/*
 *  $Id$
 *  Copyright (C) 2010-2012 David Nečas (Yeti).
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
    SGNL_FINISHED,
    N_SIGNALS
};

struct _GwyCoordsPrivate {
    GwyUnit **units;
};

typedef struct {
    const gdouble *origdata;
    gdouble *data;
    guint n;
    guint shape_size;
} ExtractFuncData;

typedef struct {
    GwyArray *array;
    gdouble *data;
    const guint *dimension_map;
    const gdouble *transparam;
    const gdouble *transparam2;
    const guint *intmap;
    gdouble *transoff;
    guint shape_size;
    guint bitmask;
    guint dimension;
} TransformFuncData;

typedef struct _GwyCoordsPrivate Coords;

static void     gwy_coords_finalize                     (GObject *object);
static void     gwy_coords_dispose                      (GObject *object);
static void     gwy_coords_serializable_init            (GwySerializableInterface *iface);
static gsize    gwy_coords_n_items                      (GwySerializable *serializable);
static gsize    gwy_coords_itemize                      (GwySerializable *serializable,
                                                         GwySerializableItems *items);
static gboolean gwy_coords_construct                    (GwySerializable *serializable,
                                                         GwySerializableItems *items,
                                                         GwyErrorList **error_list);
static GObject* gwy_coords_duplicate_impl               (GwySerializable *serializable);
static void     gwy_coords_assign_impl                  (GwySerializable *destination,
                                                         GwySerializable *source);
static gboolean class_supports_transforms               (const GwyCoordsClass *klass,
                                                         GwyCoordsTransformFlags flags);
static void     gwy_coords_translate_default            (GwyCoords *coords,
                                                         const GwyIntSet *indices,
                                                         const gdouble *offsets);
static void     gwy_coords_flip_default                 (GwyCoords *coords,
                                                         const GwyIntSet *indices,
                                                         guint axes);
static void     gwy_coords_scale_default                (GwyCoords *coords,
                                                         const GwyIntSet *indices,
                                                         const gdouble *factors);
static void     gwy_coords_transpose_default            (GwyCoords *coords,
                                                         const GwyIntSet *indices,
                                                         const guint *permutation);
static void     gwy_coords_constrain_translation_default(const GwyCoords *coords,
                                                         const GwyIntSet *indices,
                                                         gdouble *offsets,
                                                         const gdouble *lower,
                                                         const gdouble *upper);
static void     extract_func                            (gint value,
                                                         gpointer user_data);
static void     translate_func                          (gint value,
                                                         gpointer user_data);
static void     flip_func                               (gint value,
                                                         gpointer user_data);
static void     scale_func                              (gint value,
                                                         gpointer user_data);
static void     transpose_func                          (gint value,
                                                         gpointer user_data);
static void     constrain_translation_func              (gint value,
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

    /**
     * GwyCoords::finished:
     * @gwycoords: The #GwyCoords which received the signal.
     *
     * The ::finished signal is emitted by a coords user that is continuously
     * modifying it (typically by dragging the shape in a data display widget)
     * when it is done with the modifications and the coords are in the final
     * state.
     **/
    signals[SGNL_FINISHED]
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
                           _("Data length of ‘%s’ is %lu which is not "
                             "a multiple of %u."),
                           G_OBJECT_TYPE_NAME(coords),
                           (gulong)its[0].array_size, shape_size);
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
                               // TRANSLATORS: Error message.
                               _("GwyCoords of type ‘%s’ "
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
        _gwy_assign_unit(dpriv->units + i, spriv->units[i]);
}

/**
 * gwy_coords_new_subset:
 * @coords: A group of coordinates of some geometrical objects.
 * @indices: (allow-none):
 *           Set of indices of objects to extract to the new coords objects.
 *           May be %NULL in which case this method behaves identically to
 *           gwy_coords_duplicate().
 *
 * Creates a new set of coordinates of some geometrical objects as the subset
 * of an existing set.
 *
 * Returns: A newly created group of coordinates.
 **/
GwyCoords*
gwy_coords_new_subset(const GwyCoords *coords,
                      const GwyIntSet *indices)
{
    g_return_val_if_fail(GWY_IS_COORDS(coords), NULL);
    g_return_val_if_fail(!indices || GWY_IS_INT_SET(indices), NULL);

    // XXX: Must use duplicate to ensure all properties beside data are
    // duplicated.  This is not very efficient because the data are copied back
    // and forth then.  Another possibility would be to require that everything
    // beside the data is present as a property.
    GwyCoords *duplicate = gwy_coords_duplicate(coords);
    if (!indices)
        return duplicate;

    guint n = gwy_int_set_size(indices);
    GwyCoordsClass *klass = GWY_COORDS_GET_CLASS(coords);
    guint shape_size = klass->shape_size;
    guint size = shape_size*n*sizeof(gdouble);
    ExtractFuncData efdata = {
        .origdata = (const gdouble*)gwy_array_get_data(GWY_ARRAY(coords)),
        .data = g_slice_alloc(size),
        .n = 0,
        .shape_size = shape_size,
    };
    gwy_int_set_foreach(indices, extract_func, &efdata);
    _gwy_array_set_data_silent(GWY_ARRAY(duplicate), efdata.data, n);
    g_slice_free1(size, efdata.data);

    return duplicate;
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
 * gwy_coords_get_unit().
 *
 * For example, if the 2nd item in the returned item is 0 then the units
 * of the second number in the coords object can be obtained by
 * <literal>gwy_coords_get_unit(coords, 0);</literal> which is the same
 * as <literal>gwy_coords_get_mapped_unit(coords, 2);</literal>.
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
 * gwy_coords_delete_subset:
 * @coords: A group of coordinates of some geometrical objects.
 * @indices: (allow-none):
 *           Set of indices of objects to delete from @coords.  It may be %NULL
 *           but then this method becomes equivalent to gwy_coords_clear().
 *
 * Deletes a subset of coords objects.
 **/
void
gwy_coords_delete_subset(GwyCoords *coords,
                         const GwyIntSet *indices)
{
    g_return_if_fail(GWY_IS_COORDS(coords));
    g_return_if_fail(!indices || GWY_IS_INT_SET(indices));

    if (!indices) {
        gwy_coords_clear(coords);
        return;
    }

    // Copy the values to prevent funny things happening to @indices in
    // "item-deleted" callbacks.
    guint nsel;
    gint *selected = gwy_int_set_values(indices, &nsel);
    GwyArray *array = GWY_ARRAY(coords);
    // FIXME: If we could get the selected values as ranges we would delete the
    // items more efficiently using gwy_array_delete().
    for (guint i = nsel; i; i--)
        gwy_array_delete1(array, selected[i-1]);
    g_free(selected);
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
 * gwy_coords_get_unit:
 * @coords: A group of coordinates of some geometrical objects.
 * @i: Dimension index.
 *
 * Gets the units corresponding to a coords dimension.
 *
 * Returns: (transfer none):
 *          The units of the @i-th dimension.
 **/
GwyUnit*
gwy_coords_get_unit(GwyCoords *coords,
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
 * gwy_coords_get_mapped_unit:
 * @coords: A group of coordinates of some geometrical objects.
 * @i: Coordinate index.
 *
 * Gets the units corresponding to a specific coords coordinate.
 *
 * Returns: (transfer none):
 *          The units of the @i-th coordinate.
 **/
GwyUnit*
gwy_coords_get_mapped_unit(GwyCoords *coords,
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
    g_signal_emit(coords, signals[SGNL_FINISHED], 0);
}

/**
 * gwy_coords_translate:
 * @coords: A group of coordinates of some geometrical objects.
 * @indices: (allow-none):
 *           Set of indices of objects to transform.  Passing %NULL implies
 *           all objects are to be transformed.
 * @offsets: Array of GwyCoordsClass size @dimension specifying the
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
 * @factors: Array of GwyCoordsClass size @dimension specifying the
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
 * gwy_coords_transpose:
 * @coords: A group of coordinates of some geometrical objects.
 * @indices: (allow-none):
 *           Set of indices of objects to transform.  Passing %NULL implies
 *           all objects are to be transformed.
 * @permutation: Array of GwyCoordsClass size @dimension specifying the mapping
 *               of old coordinate indices to new coordinate indices.
 *
 * Remaps dimensions in selected or all objects in a coords.
 *
 * Array @permutation must be a permutation of numbers 0, 1, …, @dimension-1.
 * For instance, to change @x to @y, @y to @z, and @z to @x the array should
 * be <literal>{1, 2, 0}</literal>.  Such permutation would correspond to
 * a clock-wise rotation of right-handed Cartesian coordinates.
 *
 * If you transpose all objects, e.g. because they refer to a #GwyField that
 * is being transposed, you will probably want to also transpose the units
 * with gwy_coords_transpose_units().
 **/
void
gwy_coords_transpose(GwyCoords *coords,
                     GwyIntSet *indices,
                     const guint *permutation)
{
    g_return_if_fail(GWY_IS_COORDS(coords));
    g_return_if_fail(!indices || GWY_IS_INT_SET(indices));
    g_return_if_fail(permutation);

    GwyCoordsClass *klass = GWY_COORDS_GET_CLASS(coords);
    g_return_if_fail(klass->transpose);
    klass->transpose(coords, indices, permutation);
}

/**
 * gwy_coords_transpose_units:
 * @coords: A group of coordinates of some geometrical objects.
 * @permutation: Array of GwyCoordsClass size @dimension specifying the mapping
 *               of old coordinate indices to new coordinate indices.
 *
 * Remaps units for individual dimensions of a coords.
 *
 * See gwy_coords_transpose() for description of @permutation meaning.
 **/
void
gwy_coords_transpose_units(GwyCoords *coords,
                           const guint *permutation)
{
    g_return_if_fail(GWY_IS_COORDS(coords));
    g_return_if_fail(permutation);
    Coords *priv = coords->priv;
    if (!priv->units)
        return;

    GwyCoordsClass *klass = GWY_COORDS_GET_CLASS(coords);
    guint dimension = klass->dimension;

    guint ndifferent = 0, diff[2];
    for (guint i = 0; i < dimension; i++) {
        if (permutation[i] != i) {
            g_return_if_fail(permutation[i] < dimension);
            if (ndifferent < 2)
                diff[ndifferent] = i;
            ndifferent++;
        }
    }

    if (!ndifferent)
        return;

    g_return_if_fail(dimension > 1);
    g_return_if_fail(ndifferent > 1);

    // Handle the sane case of two swapped units first.
    if (ndifferent == 2) {
        guint i = diff[0], j = diff[1];
        g_return_if_fail(permutation[j] == i && permutation[i] == j);
        if (gwy_unit_equal(priv->units[i], priv->units[j]))
            return;

        ensure_units(priv, dimension, TRUE);
        gwy_unit_swap(priv->units[i], priv->units[j]);
        return;
    }

    GwyUnit *units[dimension];
    ensure_units(priv, dimension, TRUE);
    for (guint i = 0; i < dimension; i++)
        units[i] = gwy_unit_duplicate(priv->units[i]);
    for (guint i = 0; i < dimension; i++)
        gwy_unit_assign(priv->units[permutation[i]], units[i]);
    for (guint i = 0; i < dimension; i++)
        g_object_unref(units[i]);
}

/**
 * gwy_coords_constrain_translation:
 * @coords: A group of coordinates of some geometrical objects.
 * @indices: (allow-none):
 *           Set of indices of objects to consider.  Passing %NULL implies
 *           all objects are to be considered.
 * @offsets: Array of size @dimension containing the proposed translation
 *           offsets.  Its contents will be possibly modified if the offsets
 *           need to be constrained; they can only become smaller in absolute
 *           value, never larger.
 * @lower: Array of size @dimension containing the minimum values of invidual
 *         coordinates (upper-left corner of the multi-dimensional bounding
 *         box).  No coordinate may become smaller than the corresponding
 *         value in @lower.
 * @upper: Array of size @dimension containing the maximum values of invidual
 *         coordinates (lower-right corner of the multi-dimensional bounding
 *         box).  No coordinate may become larger than the corresponding
 *         value in @upper.
 *
 * Constrains possible translations of coords objects so that they do stay
 * within given bounding box.
 **/
void
gwy_coords_constrain_translation(const GwyCoords *coords,
                                 const GwyIntSet *indices,
                                 gdouble *offsets,
                                 const gdouble *lower,
                                 const gdouble *upper)
{
    g_return_if_fail(GWY_IS_COORDS(coords));
    g_return_if_fail(!indices || GWY_IS_INT_SET(indices));
    g_return_if_fail(offsets);
    // XXX: In principle, we could allow one-side limited translations.  Would
    // it be good for anything?
    g_return_if_fail(lower);
    g_return_if_fail(upper);

    GwyCoordsClass *klass = GWY_COORDS_GET_CLASS(coords);
    g_return_if_fail(klass->constrain_translation);
    klass->constrain_translation(coords, indices, offsets, lower, upper);
}

/**
 * gwy_coords_can_transform:
 * @coords: A group of coordinates of some geometrical objects.
 * @transforms: Bitmask from %GwyCoordsTransformFlags enum specifying which
 *              transformations are requested.
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
gwy_coords_can_transform(const GwyCoords *coords,
                         GwyCoordsTransformFlags transforms)
{
    g_return_val_if_fail(GWY_IS_COORDS(coords), FALSE);
    return class_supports_transforms(GWY_COORDS_GET_CLASS(coords), transforms);
}

/**
 * gwy_coords_class_can_transform:
 * @klass: Klass of group of coordinates of some geometrical objects.
 * @transforms: Bitmask from %GwyCoordsTransformFlags enum specifying which
 *              transformations are requested.
 *
 * Checks whether given class of groups of coordinates of some geometrical
 * objects can be transformed using given transformation.
 *
 * Not all coordinate types can be transformed using all transformations.  For
 * instance, rectangles and ellipses may not be rotated.
 *
 * Returns: %TRUE if coordinates of class @klass support all transformations
 *          given in @transforms; %FALSE if it doe not support some.
 **/
gboolean
gwy_coords_class_can_transform(const GwyCoordsClass *klass,
                               GwyCoordsTransformFlags transforms)
{
    g_return_val_if_fail(GWY_IS_COORDS_CLASS(klass), FALSE);
    return class_supports_transforms(klass, transforms);
}

/**
 * gwy_coords_class_set_generic_transforms:
 * @klass: Klass of group of coordinates of some geometrical objects.
 * @transforms: Bitmask from %GwyCoordsTransformFlags enum specifying
 *              transformations that will use the generic implementations.
 *
 * Enables default transformation implementations for given class of groups of
 * coordinates of some geometrical objects.
 *
 * This class method is intended for implementation of subclasses.  It sets the
 * implementations of methods corresponding to flags set in @transforms to
 * the generic implementations and all other transformation entries in the
 * virtual table are set to %NULL.  Hence if you need to add or modify
 * transformation methods you must to it afterwards.  Furthermore, this method
 * performs some sanity checks of @dimension, @dimension_map and @shape_size
 * to see whether they conform to what the generic implementations expect.  So
 * these fields need to be filled beforehand:
 * |[
 * my_coords_foo_class_init(MyCoordsFooClass *klass)
 * {
 *     GwyCoordsClass *coords_class = GWY_COORDS_CLASS(klass);
 *     // Fill the coordinates information fields.
 *     coords_class->dimension = 2;
 *     coords_class->dimension_map = dimension_map;
 *     coords_class->shape_size = G_N_ELEMENTS(dimension_map);
 *     // Set up the generic implementations.
 *     gwy_coords_class_set_generic_transforms(coords_class,
 *                                             GWY_COORDS_TRANSFORM_TRANSLATE
 *                                             | GWY_COORDS_TRANSFORM_SCALE
 *                                             | GWY_COORDS_TRANSFORM_FLIP);
 *     // We like the generic translate() implementation but need a different
 *     // implementation of @constrain_translation().  Change it.
 *     coords_class->constrain_translation = my_coords_foo_constrain_translation;
 *     // Make transposition possible with a specialised method.
 *     coords_class->transpose = my_coords_foo_transpose;
 *     // ...
 * }
 * ]|
 * The requirements and behaviour of the generic transformations are
 * enumerated below:
 * <variablelist>
 *   <varlistentry>
 *     <term>%GWY_COORDS_TRANSFORM_TRANSLATE</term>
 *     <listitem>
 *       No specific requirements.  Each coordinate is shifted by the
 *       corresponding offset in @offsets according to @dimension_map.
 *       When constraining translations, the shifted coordinates are not
 *       permitted to exceed the specified boundaries (which is a suitable
 *       requirement if the coordinates represent vertices of the convex hull,
 *       possibly with additional interior points).
 *     </listitem>
 *   </varlistentry>
 *   <varlistentry>
 *     <term>%GWY_COORDS_TRANSFORM_FLIP</term>
 *     <listitem>
 *       No specific requirements.  The sign of each coordinate is inverted
 *       if the corresponding bit is set in the @axes bit mask according to
 *       @dimension_map.
 *     </listitem>
 *   </varlistentry>
 *   <varlistentry>
 *     <term>%GWY_COORDS_TRANSFORM_TRANSPOSE</term>
 *     <listitem>
 *       Coordinates must be organised in one or more blocks of size
 *       @dimension, each block containing all dimension once; they can be
 *       permuted but identicially in each block.  Within each block, they are
 *       then permuted according to @permutation and @dimension_map.
 *     </listitem>
 *   </varlistentry>
 *   <varlistentry>
 *     <term>%GWY_COORDS_TRANSFORM_SCALE</term>
 *     <listitem>
 *       No specific requirements.  Each coordinate is multiplied by the
 *       corresponding factor in @factors according to @dimension_map.
 *     </listitem>
 *   </varlistentry>
 * </variablelist>
 **/
void
gwy_coords_class_set_generic_transforms(GwyCoordsClass *klass,
                                        GwyCoordsTransformFlags transforms)
{
    klass->translate = NULL;
    klass->flip = NULL;
    klass->scale = NULL;
    klass->transpose = NULL;
    klass->constrain_translation = NULL;

    g_return_if_fail(klass->shape_size);
    g_return_if_fail(klass->dimension);
    g_return_if_fail(klass->dimension_map);
    g_return_if_fail(klass->dimension <= klass->shape_size);

    const guint *dimension_map = klass->dimension_map;
    guint shape_size = klass->shape_size, dimension = klass->dimension;
    for (guint i = 0; i < shape_size; i++) {
        g_return_if_fail(dimension_map[i] < dimension);
    }

    if (transforms & GWY_COORDS_TRANSFORM_TRANSLATE) {
        klass->translate = gwy_coords_translate_default;
        klass->constrain_translation = gwy_coords_constrain_translation_default;
    }

    if (transforms & GWY_COORDS_TRANSFORM_SCALE) {
        klass->scale = gwy_coords_scale_default;
    }

    if (transforms & GWY_COORDS_TRANSFORM_FLIP) {
        klass->flip = gwy_coords_flip_default;
    }

    if (transforms & GWY_COORDS_TRANSFORM_TRANSPOSE) {
        g_return_if_fail(shape_size % dimension == 0);
        gboolean seen[dimension];
        gwy_clear(seen, dimension);
        for (guint i = 0; i < dimension; i++) {
            g_assert(!seen[dimension_map[i]]);
            seen[dimension_map[i]] = TRUE;
        }
        guint nblocks = shape_size/dimension;
        for (guint j = 1; j < nblocks; j++) {
            for (guint i = 0; i < dimension; i++) {
                g_assert(dimension_map[j*dimension + i] == dimension_map[i]);
            }
        }
        klass->transpose = gwy_coords_transpose_default;
    }
}

static gboolean
class_supports_transforms(const GwyCoordsClass *klass,
                          GwyCoordsTransformFlags flags)
{
    if ((flags & GWY_COORDS_TRANSFORM_TRANSLATE) && !klass->translate)
        return FALSE;
    if ((flags & GWY_COORDS_TRANSFORM_FLIP) && !klass->flip)
        return FALSE;
    if ((flags & GWY_COORDS_TRANSFORM_SCALE) && !klass->scale)
        return FALSE;
    if ((flags & GWY_COORDS_TRANSFORM_TRANSPOSE) && !klass->transpose)
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
gwy_coords_transpose_default(GwyCoords *coords,
                             const GwyIntSet *indices,
                             const guint *permutation)
{
    GwyCoordsClass *klass = GWY_COORDS_GET_CLASS(coords);
    GwyArray *array = GWY_ARRAY(coords);
    const guint *dimension_map = klass->dimension_map;
    guint shape_size = klass->shape_size, dimension = klass->dimension;
    gdouble *data = (gdouble*)gwy_array_get_data(array);
    g_assert(dimension_map);
    g_assert(shape_size % dimension == 0);
    // We that dimension_map is the sequence 0, 1, ..., dimension; possibly
    // repeated several times. If not and it is just permuted, we might fix it
    // by constructing the inverse dimension map here.

    if (!indices) {
        guint n = gwy_array_size(array);
        for (guint i = 0; i < n; i++) {
            for (guint j = shape_size/dimension; j; j--) {
                gdouble newcoords[dimension];
                for (guint k = 0; k < dimension; k++)
                    newcoords[permutation[k]] = data[k];
                gwy_assign(data, newcoords, dimension);
                data += dimension;
            }
            gwy_array_updated(array, i);
        }
        return;
    }

    TransformFuncData tfdata = {
        .array = array,
        .data = data,
        .intmap = permutation,
        .shape_size = shape_size,
        .dimension_map = dimension_map,
        .dimension = dimension,
    };
    gwy_int_set_foreach(indices, transpose_func, &tfdata);
}

static void
gwy_coords_constrain_translation_default(const GwyCoords *coords,
                                         const GwyIntSet *indices,
                                         gdouble *offsets,
                                         const gdouble *lower,
                                         const gdouble *upper)
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
            for (guint j = shape_size; j; j--, data++, umap++) {
                guint k = *umap;
                offsets[k] = CLAMP(offsets[k],
                                   lower[k] - *data,
                                   upper[k] - *data);
            }
        }
        return;
    }

    TransformFuncData tfdata = {
        .array = array,
        .data = data,
        .transparam = lower,
        .transparam2 = upper,
        .shape_size = shape_size,
        .dimension_map = dimension_map,
        .transoff = offsets,
    };
    gwy_int_set_foreach(indices, constrain_translation_func, &tfdata);
}

static void
extract_func(gint value, gpointer user_data)
{
    ExtractFuncData *efdata = (ExtractFuncData*)user_data;
    guint shape_size = efdata->shape_size;
    gwy_assign(efdata->data + efdata->n*shape_size,
               efdata->origdata + value*shape_size,
               shape_size);
    efdata->n++;
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

static void
transpose_func(gint value, gpointer user_data)
{
    TransformFuncData *tfdata = (TransformFuncData*)user_data;
    const guint *permutation = tfdata->intmap;
    guint shape_size = tfdata->shape_size;
    guint dimension = tfdata->dimension;
    gdouble *data = tfdata->data + value*shape_size;

    for (guint j = shape_size/dimension; j; j--) {
        gdouble newcoords[dimension];
        for (guint k = 0; k < dimension; k++)
            newcoords[permutation[k]] = data[k];
        gwy_assign(data, newcoords, dimension);
        data += dimension;
    }
    gwy_array_updated(tfdata->array, value);
}

static void
constrain_translation_func(gint value, gpointer user_data)
{
    TransformFuncData *tfdata = (TransformFuncData*)user_data;
    const gdouble *lower = tfdata->transparam, *upper = tfdata->transparam2;
    const guint *umap = tfdata->dimension_map;
    guint shape_size = tfdata->shape_size;
    gdouble *data = tfdata->data + value*shape_size;
    gdouble *offsets = tfdata->transoff;

    for (guint j = shape_size; j; j--, data++, umap++) {
        guint k = *umap;
        offsets[k] = CLAMP(offsets[k], lower[k] - *data, upper[k] - *data);
    }
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
 *                 and used with gwy_coords_get_unit(), to individual numbers
 *                 (coordinates) in the coords objects.  The generic
 *                 implementations of transformation methods use this array to
 *                 determine which coordinate corresponds to which offset,
 *                 scale, etc.
 * @translate: Virtual method implementing gwy_coords_translate().  If it is
 *             implemented the class will report the
 *             %GWY_COORDS_TRANSFORM_TRANSLATE capability.  In general, this
 *             should be supported by all subclasses.
 * @flip: Virtual method implementing gwy_coords_flip().  If it is implemented
 *        the class will report the %GWY_COORDS_TRANSFORM_FLIP capability.
 * @scale: Virtual method implementing gwy_coords_scale().  If it is
 *         implemented the class will report the %GWY_COORDS_TRANSFORM_SCALE
 *         capability.
 * @transpose: Virtual method implementing gwy_coords_transpose().  If it is
 *             implemented the class will report the
 *             %GWY_COORDS_TRANSFORM_TRANSPOSE capability.
 * @constrain_translation: Virtual method implementing
 *                         gwy_coords_constrain_translation().
 *
 * Class of groups coordinates of some geometrical objects.
 *
 * Specific, i.e. instantiable, subclasses have to set the data members
 * @shape_size, @dimension and @dimension_map.
 *
 * Transformation method often do not need to be implemented specifically.
 * If the coordinates follow certain conventions, generic implementations
 * can be used.  See gwy_coords_class_set_generic_transforms() for a
 * description of how they work and how they can be enabled for a subclass.
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
 *                                  In general, requesting to find the possible
 *                                  translation limits with
 *                                  gwy_coords_constrain_translation() should
 *                                  be also possible if this flag is present.
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
 *                               Rotation is namely used for two-dimensional
 *                               coordinates as it does not exist for lower
 *                               dimensions and becomes rather convoluted for
 *                               higher dimensions.
 * @GWY_COORDS_TRANSFORM_CROP: Restricting coordinates to certain
 *                             multi-dimensional ranges, typically as the
 *                             result of extraction of a part of the data.
 *                             This ‘transformation’ may remove some objects
 *                             altogether.  If rotation is possible, cropping
 *                             should also be.
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
