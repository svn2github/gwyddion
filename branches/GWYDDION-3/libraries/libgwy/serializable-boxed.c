/*
 *  $Id$
 *  Copyright (C) 2009,2013 David Nečas (Yeti).
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

#include <string.h>
#include "libgwy/serializable-boxed.h"

#define serializable_boxed_index(a, i) \
    g_array_index((a), GwySerializableBoxedData, (i))

typedef struct {
    GType type;
    const GwySerializableBoxedInfo *info;
} GwySerializableBoxedData;

G_LOCK_DEFINE_STATIC(serializable_boxed);
static GArray *serializable_boxed_data = NULL;

static const GwySerializableBoxedInfo*
find_serializable_boxed_info(GType type)
{
    g_return_val_if_fail(serializable_boxed_data, NULL);
    G_LOCK(serializable_boxed);
    GArray *data = serializable_boxed_data;
    for (guint i = 0; i < data->len; i++) {
        if (serializable_boxed_index(data, i).type == type) {
            GwySerializableBoxedData retval = serializable_boxed_index(data, i);
            if (i) {
                /* The just looked-up type is likely to be looked-up again
                 * soon. */
                serializable_boxed_index(data, i) 
                    = serializable_boxed_index(data, 0);
                serializable_boxed_index(data, 0) = retval;
            }
            G_UNLOCK(serializable_boxed);
            return retval.info;
        }
    }
    return NULL;
}

/**
 * gwy_boxed_type_is_serializable:
 * @type: Type to query.  Any #GType may be given, not just boxed types.
 *
 * Determines if a type supports the boxed serialisable protocol.
 *
 * Returns: %TRUE if @type was registered as serialisable boxed type, %FALSE
 *          if it was not.
 **/
gboolean
gwy_boxed_type_is_serializable(GType type)
{
    return (serializable_boxed_data
            && find_serializable_boxed_info(type) != NULL);
}

/**
 * gwy_serializable_boxed_register_static:
 * @type: Boxed type to register as serialisable.
 * @info: Information how the type implements the serialization protocol.
 *        It is not not copied; hence @info must be static data or allocated on
 *        heap and never freed.
 *
 * Registers a serialisable boxed type.
 *
 * A boxed type must be registered before it can be deserialised.  Usually this
 * function is called in <function>gwy_foo_get_type<!-- -->()</function> for
 * the type although it is possible to register foreign boxed types as
 * serialisable.  They must be registered <quote>soon enough</quote> then.
 *
 * Standard Gwyddion boxed serialisable types are registered by
 * gwy_type_init(), i.e. at the very latest when deserialization is attempted.
 **/
void
gwy_serializable_boxed_register_static(GType type,
                                       const GwySerializableBoxedInfo *info)
{
    static gsize data_initialized = 0;
    if (G_UNLIKELY(g_once_init_enter(&data_initialized))) {
        serializable_boxed_data = g_array_new(FALSE, FALSE,
                                              sizeof(GwySerializableBoxedData));
        g_once_init_leave(&data_initialized, 1);
    }

    g_return_if_fail(G_TYPE_IS_BOXED(type));
    g_return_if_fail(!G_TYPE_IS_ABSTRACT(type));    // That's GBoxed for you
    g_return_if_fail(info);
    g_return_if_fail(info->n_items);
    g_return_if_fail(info->itemize);
    g_return_if_fail(info->construct);
    g_return_if_fail(info->size || (info->assign && info->equal));

    G_LOCK(serializable_boxed);
    GArray *data = serializable_boxed_data;
    for (guint i = 0; i < data->len; i++) {
        if (G_UNLIKELY(serializable_boxed_index(data, i).type == type)) {
            G_UNLOCK(serializable_boxed);
            g_warning("Type %s has been already registered as serializable "
                      "boxed.", g_type_name(type));
            return;
        }
    }
    GwySerializableBoxedData newitem = { type, info };
    g_array_append_val(serializable_boxed_data, newitem);
    G_UNLOCK(serializable_boxed);
}

/**
 * gwy_serializable_boxed_assign:
 * @type: Type of @destination and @source.  It must be a serialisable boxed
 *        type.
 * @destination: Pointer to boxed struct that should be made equal to @source.
 * @source: Pointer to source boxed struct.
 *
 * Copies the value of a boxed struct to another boxed struct.
 **/
void
gwy_serializable_boxed_assign(GType type,
                              gpointer destination,
                              gconstpointer source)
{
    const GwySerializableBoxedInfo *info = find_serializable_boxed_info(type);
    g_return_if_fail(info);
    g_return_if_fail(destination);
    g_return_if_fail(source);
    if (info->size)
        memcpy(destination, source, info->size);
    else
        info->assign(destination, source);
}

/**
 * gwy_serializable_boxed_equal:
 * @type: Type of @a and @b.  It must be a serialisable boxed type.
 * @a: Pointer to one boxed struct of type @type.
 * @b: Pointer to another boxed struct of type @type.
 *
 * Compares the values of two boxed structs for equality.
 *
 * Returns: %TRUE if @a and @b are equal, %FALSE if they differ.
 **/
gboolean
gwy_serializable_boxed_equal(GType type,
                             gconstpointer a,
                             gconstpointer b)
{
    const GwySerializableBoxedInfo *info = find_serializable_boxed_info(type);
    g_return_val_if_fail(info, FALSE);
    g_return_val_if_fail(a, FALSE);
    g_return_val_if_fail(b, FALSE);
    return info->size ? !memcmp(a, b, info->size) : info->equal(a, b);
}

/**
 * gwy_serializable_boxed_n_items:
 * @type: Serialisable boxed type.
 *
 * Obtains the number of items a boxed type serialises to.
 **/
gsize
gwy_serializable_boxed_n_items(GType type)
{
    const GwySerializableBoxedInfo *info = find_serializable_boxed_info(type);
    g_return_val_if_fail(info, 0);
    return info->n_items + 1;
}

/**
 * gwy_serializable_boxed_itemize:
 * @type: Serialisable boxed type.
 * @boxed: Pointer to boxed struct of type @type.
 * @items: List of flattened object tree items to append the representation of
 *         @boxed to.
 *
 * Creates the flattened representation of a serialisable boxed struct.
 **/
void
gwy_serializable_boxed_itemize(GType type,
                               gpointer boxed,
                               GwySerializableItems *items)
{
    g_return_if_fail(items->n < items->len);
    const GwySerializableBoxedInfo *info = find_serializable_boxed_info(type);
    g_return_if_fail(info);
    GwySerializableItem *item = items->items + items->n;
    items->n++;
    item->name = g_type_name(type);
    item->value.v_size = 0;
    item->ctype = GWY_SERIALIZABLE_HEADER;
    item->array_size = info->itemize(boxed, items);
}

/**
 * gwy_serializable_boxed_construct:
 * @type: Serialisable boxed type.
 * @items: List of items to construct the boxed struct from.
 * @error_list: Location to store the errors occuring, %NULL to ignore.
 *
 * Constructs a serialisable struct type from its flattened representation.
 *
 * Returns: (transfer full):
 *          Newly allocated boxed type with the deserialised data, possibly
 *          %NULL on fatal failure.
 **/
gpointer
gwy_serializable_boxed_construct(GType type,
                                 GwySerializableItems *items,
                                 GwyErrorList **error_list)
{
    const GwySerializableBoxedInfo *info = find_serializable_boxed_info(type);
    /* This is a hard error, the caller must check the type beforehand. */
    g_return_val_if_fail(info, NULL);
    return info->construct(items, error_list);
}


/**
 * SECTION: serializable-boxed
 * @title: Serializable boxed
 * @section_id: libgwy4-serializable-boxed
 * @short_description: Making boxed types serialisable
 *
 * The serialisable boxed protocol is an analogue of the serialisable object
 * protocol described in section #GwySerializable where more detailed
 * discussion of the concept can be found.  The protocl is intended for simple
 * structs that do not do their own memory management and typically can be
 * bitwise copied.  Complex data should be represented as objects.
 *
 * Boxed structs can be serialised as parts of objects, the top-level item
 * of a serialised representation must be always an object.  Also, serialisable
 * boxed structs may contain only atomic types (FIXME: this is not enforced by
 * the implementation at this moment).
 **/

/**
 * GwySerializableBoxedInfo:
 * @size: Size of the boxed data for bit-wise operable plain old data.  If it
 *        is non-zero @assign and @equal must be %NULL; assignment and
 *        comparison is performed by direct memory copying and comparing.
 *        If it is zero these operation are performed usinng @assign and @equal
 *        that must be defined.
 * @n_items: Number of items the boxed struct serialises to.
 * @itemize: Appends flattened representation of the boxed struct to the
 *           @items list.
 * @construct: Deserialises a boxed struct from array of flattened data
 *             items.  It returns a newly allocated boxed struct that must be
 *             freeable with g_boxed_free().
 * @assign: Makes the content of a boxed struct identical to the content of
 *          another boxed struct of the same type.
 * @equal: Compares two boxed data for equality, returns %TRUE if they are
 *         equal, %FALSE if they differ.
 *
 * Interface implemented by serialisable boxed types.
 *
 * The fields and methods are similar to #GwySerializableInterface where a more
 * detailed discussion can be found.  Some notes to the differences:
 *
 * The number of items @n_items is a constant, not a method, which is
 * appropriate for plain old data.  However, it is still an upper bound so you
 * can actually serialise less items in #GwySerializableBoxedInfo.itemize().
 * For the same reason, no equivalent of #GwySerializableInterface.done()
 * method exists.
 *
 * Duplication is realised by the
 * <link linkend="gobject-Boxed-Types">GBoxed</link>
 * protocol, namely g_boxed_copy() and hence needs not be specified.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
