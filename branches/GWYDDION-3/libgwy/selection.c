/*
 *  $Id$
 *  Copyright (C) 2010 David Necas (Yeti).
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
#include "libgwy/selection.h"
#include "libgwy/libgwy-aliases.h"
#include "libgwy/array-internal.h"
#include "libgwy/math-internal.h"

enum { N_ITEMS = 1 };

enum {
    FINISHED,
    N_SIGNALS
};

struct _GwySelectionPrivate {
    guint dummy;
};

typedef struct _GwySelectionPrivate Selection;

static void     gwy_selection_serializable_init(GwySerializableInterface *iface);
static gsize    gwy_selection_n_items          (GwySerializable *serializable);
static gsize    gwy_selection_itemize          (GwySerializable *serializable,
                                                GwySerializableItems *items);
static gboolean gwy_selection_construct        (GwySerializable *serializable,
                                                GwySerializableItems *items,
                                                GwyErrorList **error_list);
static void     gwy_selection_assign_impl      (GwySerializable *destination,
                                                GwySerializable *source);

static const GwySerializableItem serialize_items[N_ITEMS] = {
    { .name = "data", .ctype = GWY_SERIALIZABLE_DOUBLE_ARRAY, },
};

static guint selection_signals[N_SIGNALS];

G_DEFINE_TYPE_EXTENDED
    (GwySelection, gwy_selection, GWY_TYPE_ARRAY, G_TYPE_FLAG_ABSTRACT,
     GWY_IMPLEMENT_SERIALIZABLE(gwy_selection_serializable_init))

static void
gwy_selection_serializable_init(GwySerializableInterface *iface)
{
    iface->n_items   = gwy_selection_n_items;
    iface->itemize   = gwy_selection_itemize;
    iface->construct = gwy_selection_construct;
    iface->assign    = gwy_selection_assign_impl;
}

static void
gwy_selection_class_init(GwySelectionClass *klass)
{
    G_GNUC_UNUSED GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    g_type_class_add_private(klass, sizeof(Selection));

    /**
     * GwySelection::finished:
     * @gwyselection: The #GwySelection which received the signal.
     *
     * The ::finished signal is emitted by a selection user that is
     * continuously modifying it (typically a selection layer) when it is done
     * with the modifications and the selection is in the final state.
     **/
    selection_signals[FINISHED]
        = g_signal_new_class_handler("finished",
                                     G_OBJECT_CLASS_TYPE(klass),
                                     G_SIGNAL_RUN_FIRST,
                                     NULL, NULL, NULL,
                                     g_cclosure_marshal_VOID__VOID,
                                     G_TYPE_NONE, 0);
}

static void
gwy_selection_init(GwySelection *selection)
{
    selection->priv = G_TYPE_INSTANCE_GET_PRIVATE(selection,
                                                  GWY_TYPE_SELECTION,
                                                  Selection);
}

static gsize
gwy_selection_n_items(G_GNUC_UNUSED GwySerializable *serializable)
{
    return N_ITEMS;
}

static gsize
gwy_selection_itemize(GwySerializable *serializable,
                      GwySerializableItems *items)
{
    g_return_val_if_fail(items->len - items->n >= N_ITEMS, 0);

    GwySelection *selection = GWY_SELECTION(serializable);
    GwySerializableItem *it = items->items + items->n;

    *it = serialize_items[0];
    it->value.v_double_array = gwy_array_get_data(GWY_ARRAY(selection));
    it++, items->n++;

    return N_ITEMS;
}

static gboolean
gwy_selection_construct(GwySerializable *serializable,
                        GwySerializableItems *items,
                        GwyErrorList **error_list)
{
    GwySerializableItem item = serialize_items[0];
    gwy_deserialize_filter_items(&item, N_ITEMS, items, NULL,
                                 "GwySelection", error_list);

    gboolean ok = FALSE;

    GwySelection *selection = GWY_SELECTION(serializable);
    // XXX: This is a bit tricky.  We know what the real class is, it's the
    // class of @serializable because the object is already fully constructed
    // as far as GObject is concerned so we can check the number of items.
    guint shape_size = GWY_SELECTION_GET_CLASS(selection)->shape_size;
    if (!shape_size) {
        g_critical("Object size of selection type %s is zero.",
                   G_OBJECT_TYPE_NAME(selection));
        // This is really a programmer's error, so do not report it in
        // @error_list, just set teeth and leave the selection empty.
        // Let it crash somewhere else...
        ok = TRUE;
        goto fail;
    }

    if (item.array_size % shape_size) {
        gwy_error_list_add(error_list, GWY_DESERIALIZE_ERROR,
                           GWY_DESERIALIZE_ERROR_INVALID,
                           _("Selection data length is %lu which is not "
                             "a multiple of %u as is expected for selection "
                             "type ‘%s’."),
                           (gulong)item.array_size, shape_size,
                           G_OBJECT_TYPE_NAME(selection));
        goto fail;
    }
    if (item.array_size) {
        GwyArray *array = GWY_ARRAY(selection);
        _gwy_array_set_data_silent(array,
                                   item.value.v_double_array,
                                   item.array_size/shape_size);
    }

    ok = TRUE;

fail:
    GWY_FREE(item.value.v_double_array);
    return ok;
}

static void
gwy_selection_assign_impl(GwySerializable *destination,
                          GwySerializable *source)
{
    GwyArray *dest = GWY_ARRAY(destination);
    GwyArray *src = GWY_ARRAY(source);
    gwy_array_set_data(dest, gwy_array_get_data(src), gwy_array_size(src));
}

/**
 * gwy_selection_shape_size:
 * @selection: A group of shapes selected on data.
 *
 * Obtains the number of floating point values representing one object or shape
 * of the selection.
 *
 * This value is the same for all selections of a specific type.
 *
 * Returns: The number of values a single shape takes.
 **/
guint
gwy_selection_shape_size(GwySelection *selection)
{
    GwySelectionClass *klass = GWY_SELECTION_GET_CLASS(selection);
    g_return_val_if_fail(klass, 0);
    return klass->shape_size;
}

/**
 * gwy_selection_clear:
 * @selection: A group of shapes selected on data.
 *
 * Removes all objects in a selection.
 **/
void
gwy_selection_clear(GwySelection *selection)
{
    g_return_if_fail(GWY_IS_SELECTION(selection));
    GwyArray *array = GWY_ARRAY(selection);
    gwy_array_delete(array, 0, gwy_array_size(array));
}

/**
 * gwy_selection_get:
 * @selection: A group of shapes selected on data.
 * @i: Object index.
 * @data: Array of length at least gwy_selection_shape_size() to store the
 *        data to.
 *
 * Obtains the data of a single selection object.
 *
 * The interpretation of the data depends on the selection type.
 *
 * Returns: %TRUE if there is an @i-th object and @data filled with values.
 *          %FALSE if these is no such object and @data was left untouched.
 **/
gboolean
gwy_selection_get(GwySelection *selection,
                  guint i,
                  gdouble *data)
{
    g_return_val_if_fail(GWY_IS_SELECTION(selection), FALSE);
    gpointer item = gwy_array_get(GWY_ARRAY(selection), i);
    if (item) {
        g_return_val_if_fail(data, FALSE);
        guint shape_size = GWY_SELECTION_GET_CLASS(selection)->shape_size;
        ASSIGN(data, item, shape_size);
    }
    return item != NULL;
}

/**
 * gwy_selection_set:
 * @selection: A group of shapes selected on data.
 * @i: Object index.  It must correspond to an existing selection object or
 *     it can point after the end of the selection.  In the second case the
 *     object will be appended to the end.
 * @data: Data of a single selection object size in an array of size 
 *        gwy_selection_shape_size().
 *
 * Sets the data of a single selection object.
 **/
void
gwy_selection_set(GwySelection *selection,
                  guint i,
                  const gdouble *data)
{
    g_return_if_fail(GWY_IS_SELECTION(selection));
    GwyArray *array = GWY_ARRAY(selection);
    guint n = gwy_array_size(array);
    if (i < n)
        gwy_array_replace1(GWY_ARRAY(selection), i, data);
    else if (i == n)
        gwy_array_append1(GWY_ARRAY(selection), data);
    else {
        g_critical("Selection object index %u is beyond the end of the data.");
        gwy_array_append1(GWY_ARRAY(selection), data);
    }
}

/**
 * gwy_selection_delete:
 * @selection: A group of shapes selected on data.
 * @i: Object index.
 *
 * Deletes a signle selection object.
 **/
void
gwy_selection_delete(GwySelection *selection,
                     guint i)
{
    g_return_if_fail(GWY_IS_SELECTION(selection));
    gwy_array_delete1(GWY_ARRAY(selection), i);
}

/**
 * gwy_selection_size:
 * @selection: A group of shapes selected on data.
 *
 * Obtains the number of objects in a selection.
 **/
guint
gwy_selection_size(GwySelection *selection)
{
    g_return_val_if_fail(GWY_IS_SELECTION(selection), 0);
    return gwy_array_size(GWY_ARRAY(selection));
}

/**
 * gwy_selection_get_data:
 * @selection: A group of shapes selected on data.
 * @data: Array of sufficient length to to store the complete selection data
 *        to.
 *
 * Obtains the data of an entire selection.
 **/
void
gwy_selection_get_data(GwySelection *selection,
                       gdouble *data)
{
    g_return_if_fail(GWY_IS_SELECTION(selection));
    g_return_if_fail(data);
    guint shape_size = GWY_SELECTION_GET_CLASS(selection)->shape_size;
    GwyArray *array = GWY_ARRAY(selection);
    const gdouble *array_data = gwy_array_get_data(array);
    gsize size = gwy_array_size(array);
    g_assert(array_data);
    ASSIGN(data, array_data, size*shape_size);
}

/**
 * gwy_selection_set_data:
 * @selection: A group of shapes selected on data.
 * @n: New selection size (number of objects in @data).
 * @data: New selection data.
 *
 * Sets the data of an entire selection.
 *
 * Note this can emit lots of signals, see gwy_array_set_data().
 **/
void
gwy_selection_set_data(GwySelection *selection,
                       guint n,
                       const gdouble *data)
{
    g_return_if_fail(GWY_IS_SELECTION(selection));
    g_return_if_fail(data);
    gwy_array_set_data(GWY_ARRAY(selection), data, n);
}

/**
 * gwy_selection_filter:
 * @selection: A group of shapes selected on data.
 * @filter: Function returning %TRUE for objects that should be kept, %FALSE
 *          for objects that should be deleted.
 * @user_data: Data passed to @filter;
 *
 * Deletes selection objects matching certain criteria.
 **/
void
gwy_selection_filter(GwySelection *selection,
                     GwySelectionFilterFunc filter,
                     gpointer user_data)
{
}

/**
 * gwy_selection_finished:
 * @selection: A group of shapes selected on data.
 *
 * Emits signal ::finished on a selection.
 **/
void
gwy_selection_finished(GwySelection *selection)
{
    g_return_if_fail(GWY_IS_SELECTION(selection));
    g_signal_emit(selection, selection_signals[FINISHED], 0);
}

#define __LIBGWY_SELECTION_C__
#include "libgwy/libgwy-aliases.c"

/**
 * SECTION: selection
 * @title: GwySelection
 * @short_description: Base class for shapes selected on data
 *
 * Selections are groups of objects or shapes selected on data.  Each object
 * is represented by a fixed-size (for a specific selection type) chunk of
 * floating point values, i.e. coordinates and/or dimensions, that describe the
 * shape.  The meaning of these values differs among individual selection
 * types.  #GwySelection methods perform common operations that do not require
 * the knowledge of the data interpretation.
 *
 * #GwySelection is a subclass of #GwyArray which offers a wider range of
 * data managing methods that you can freely use.  The #GwySelection methods
 * are somewhat more type-safe as they have #gdouble* arguments and they ensure
 * GwyArray::item-updated is emitted when necessary while you might need to do
 * this manually with #GwyArrays if you modify the data directly.
 **/

/**
 * GwySelection:
 *
 * Object representing a group of shapes selected on data.
 *
 * The #GwySelection struct contains private data only and should be accessed
 * using the functions below.
 **/

/**
 * GwySelectionClass:
 * @shape_size: The number of double values used to described one selected
 *              shape.  Concrete subclasses have to set this value.
 *
 * Class of groups of shapes selected on data.
 **/

/**
 * gwy_selection_duplicate:
 * @selection: A group of shapes selected on data.
 *
 * Duplicates a group of shapes selected on data.
 *
 * This is a convenience wrapper of gwy_serializable_duplicate().
 **/

/**
 * gwy_selection_assign:
 * @dest: Destination group of shapes selected on data.
 * @src: Source group of shapes selected on data.
 *
 * Copies the value of a group of shapes selected on data.
 *
 * This is a convenience wrapper of gwy_serializable_assign().
 **/

/**
 * GwySelectionFilterFunc:
 * @selection: A group of shapes selected on data.
 * @i: Index of the selection object to consider.
 * @user_data: User data passed to gwy_selection_filter().
 *
 * Type of selection filtering function.
 *
 * Returns: %TRUE for objects that should be kept, %FALSE for objects that
 *          should be removed.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
