/*
 *  $Id$
 *  Copyright (C) 2009 David Necas (Yeti).
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
#include "libgwy/macros.h"
#include "libgwy/array.h"
#include "libgwy/libgwy-aliases.h"

#define gwy_array_index(a,i) \
    ((gpointer)((guchar*)((a)->items->data) + (i)*((a)->size)))

#define gwy_data_index(a,p,i) \
    ((gpointer)((guchar*)(p) + (i)*((a)->size)))

enum {
    ITEM_INSERTED,
    ITEM_DELETED,
    ITEM_UPDATED,
    //ITEMS_REORDERED,  maybe, if/when we add sort()
    N_SIGNALS
};

struct _GwyArrayPrivate {
    GObject g_object;

    GArray *items;

    gsize size;
    GDestroyNotify destroy;
    gboolean type_set : 1;
};

typedef struct _GwyArrayPrivate Array;

static void         gwy_array_finalize(GObject *object);
static void         gwy_array_dispose (GObject *object);

static guint gwy_array_signals[N_SIGNALS];

G_DEFINE_TYPE(GwyArray, gwy_array, G_TYPE_OBJECT)

static void
gwy_array_class_init(GwyArrayClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    g_type_class_add_private(klass, sizeof(Array));

    gobject_class->dispose = gwy_array_dispose;
    gobject_class->finalize = gwy_array_finalize;

    /**
     * GwyArray::item-inserted:
     * @gwyarray: The #GwyArray which received the signal.
     * @arg1: Position an item was inserted at.
     *
     * The ::item-inserted signal is emitted when an item is inserted into
     * the array.
     **/
    gwy_array_signals[ITEM_INSERTED]
        = g_signal_new_class_handler("item-inserted",
                                     GWY_TYPE_ARRAY,
                                     G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE,
                                     NULL, NULL, NULL,
                                     g_cclosure_marshal_VOID__UINT,
                                     G_TYPE_NONE, 1, G_TYPE_UINT);

    /**
     * GwyArray::item-deleted:
     * @gwyarray: The #GwyArray which received the signal.
     * @arg1: Position an item was deleted from.
     *
     * The ::item-deleted signal is emitted when an item is deleted from
     * the array.
     **/
    gwy_array_signals[ITEM_DELETED]
        = g_signal_new_class_handler("item-deleted",
                                     GWY_TYPE_ARRAY,
                                     G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE,
                                     NULL, NULL, NULL,
                                     g_cclosure_marshal_VOID__UINT,
                                     G_TYPE_NONE, 1, G_TYPE_UINT);

    /**
     * GwyArray::item-updated:
     * @gwyarray: The #GwyArray which received the signal.
     * @arg1: Position of updated item.
     *
     * The ::item-updated signal is emitted when an item in the array
     * is updated.
     **/
    gwy_array_signals[ITEM_UPDATED]
        = g_signal_new_class_handler("item-updated",
                                     GWY_TYPE_ARRAY,
                                     G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE,
                                     NULL, NULL, NULL,
                                     g_cclosure_marshal_VOID__UINT,
                                     G_TYPE_NONE, 1, G_TYPE_UINT);

//  /**
//   * GwyArray::items-reordered:
//   * @gwyarray: The #GwyArray which received the signal.
//   * @arg1: New item order map as in #GtkTreeModel,
//   *        @arg1[new_position] = old_position.
//   *
//   * The ::items-reordered signal is emitted when item in the array
//   * are reordered.
//   **/
//  gwy_array_signals[ITEMS_REORDERED]
//      = g_signal_new("items-reordered",
//                     GWY_TYPE_ARRAY,
//                     G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE,
//                     G_STRUCT_OFFSET(GwyArrayClass, items_reordered),
//                     NULL, NULL,
//                     g_cclosure_marshal_VOID__POINTER,
//                     G_TYPE_NONE, 1, G_TYPE_POINTER);
}

static void
gwy_array_init(GwyArray *array)
{
    array->priv =  G_TYPE_INSTANCE_GET_PRIVATE(array, GWY_TYPE_ARRAY, Array);
}

static void
gwy_array_finalize(GObject *object)
{
    Array *array = GWY_ARRAY(object)->priv;

    if (array->items)
        g_array_free(array->items, TRUE);

    G_OBJECT_CLASS(gwy_array_parent_class)->finalize(object);
}

static void
gwy_array_dispose(GObject *object)
{
    Array *array = GWY_ARRAY(object)->priv;

    // Destroy items in dispose() because they may contain pointers to objects.
    if (array->items && array->items->len && array->destroy) {
        GArray *items = array->items;
        while (items->len) {
            array->destroy(gwy_array_index(array, items->len - 1));
            items->len--;
        }
    }

    G_OBJECT_CLASS(gwy_array_parent_class)->dispose(object);
}

static inline void
ensure_items(Array *array)
{
    if (G_UNLIKELY(!array->items))
        array->items = g_array_new(FALSE, FALSE, array->size);
}

/**
 * gwy_array_new:
 *
 * Creates a new array.
 *
 * Returns: A new array.
 **/
GwyArray*
gwy_array_new(void)
{
    return g_object_newv(GWY_TYPE_ARRAY, 0, NULL);
}

/**
 * gwy_array_new_with_data:
 * @size: Item size, in bytes.
 * @destroy: Function to call on items before they are removed from the array.
 *           May be %NULL.
 * @items: Items to fill the array with.
 * @nitems: Number of items in @items.
 *
 * Creates a new array with specified item type and fills it with items.
 *
 * This is the preferred method to create an array filled with values as it
 * does not emit GwyArray::item-inserted for each item.
 *
 * Returns: A new array.
 **/
GwyArray*
gwy_array_new_with_data(gsize size,
                        GDestroyNotify destroy,
                        gpointer items,
                        guint nitems)
{
    GwyArray *array = g_object_newv(GWY_TYPE_ARRAY, 0, NULL);
    gwy_array_set_item_type(array, size, destroy);
    ensure_items(array->priv);
    if (items && nitems)
        g_array_append_vals(array->priv->items, items, nitems);
    return array;
}

/**
 * gwy_array_set_item_type:
 * @array: An array.
 * @size: Item size in bytes.
 * @destroy: Destroy notification function called immediately before an item
 *           is removed from the array.
 *
 * Sets the item type information of an array.
 *
 * This function can be called only just after the initialization, before any
 * items are added.  It is intended for classes that inherit from array and to
 * permit generic construction with g_object_new().
 **/
void
gwy_array_set_item_type(GwyArray *array,
                        gsize size,
                        GDestroyNotify destroy)
{
    g_return_if_fail(GWY_IS_ARRAY(array));
    Array *priv = array->priv;
    g_return_if_fail(!priv->items);
    g_return_if_fail(size);

    priv->type_set = TRUE;
    priv->size = size;
    priv->destroy = destroy;
}

/**
 * gwy_array_size:
 * @array: An array.
 *
 * Returns the number of items in an array.
 *
 * Returns: The number of items.
 **/
guint
gwy_array_size(GwyArray *array)
{
    g_return_val_if_fail(GWY_IS_ARRAY(array), 0);
    Array *priv = array->priv;
    return priv->items ? priv->items->len : 0;
}

/**
 * gwy_array_get:
 * @array: An array.
 * @n: Item position.
 *
 * Returns pointer to an item of an array.
 *
 * Returns: Item at position @n, %NULL if there is no such item.
 **/
gpointer
gwy_array_get(GwyArray *array,
              guint n)
{
    g_return_val_if_fail(GWY_IS_ARRAY(array), NULL);
    Array *priv = array->priv;
    g_return_val_if_fail(priv->items, NULL);
    return (n < priv->items->len) ? gwy_array_index(priv, n) : NULL;
}

/**
 * gwy_array_updated:
 * @array: An array.
 * @n: Item position.
 *
 * Notifies array users that item at given position was updated.
 *
 * This method emits GwyArray::item-updated.
 **/
void
gwy_array_updated(GwyArray *array,
                  guint n)
{
    g_return_if_fail(GWY_IS_ARRAY(array));
    Array *priv = array->priv;
    g_return_if_fail(priv->items);
    g_return_if_fail(n < priv->items->len);

    g_signal_emit(array, gwy_array_signals[ITEM_UPDATED], 0, n);
}

/**
 * gwy_array_insert1:
 * @array: An array.
 * @n: Position to insert @item at.
 * @item: Item to insert.
 *
 * Inserts an item into an array.
 *
 * Returns: Pointer to the inserted item in the array, for convenience.
 **/
/**
 * gwy_array_insert:
 * @array: An array.
 * @n: Position to insert @items at.
 * @items: Items to insert.
 * @nitems: Number of items to insert.
 *
 * Inserts items into an array.
 *
 * Returns: Pointer to the first inserted item in the array, for convenience.
 **/
gpointer
gwy_array_insert(GwyArray *array,
                 guint n,
                 gpointer items,
                 guint nitems)
{
    g_return_val_if_fail(GWY_IS_ARRAY(array), NULL);
    Array *priv = array->priv;
    g_return_val_if_fail(priv->type_set, NULL);
    if (G_UNLIKELY(!nitems)) {
        if (priv->items)
            return priv->items->data;
        return NULL;
    }
    g_return_val_if_fail(items, NULL);

    ensure_items(priv);
    g_return_val_if_fail(n <= priv->items->len, NULL);
    for (guint i = 0; i < nitems; i++) {
        g_array_insert_vals(priv->items, n + i,
                            gwy_data_index(priv, items, i), 1);
        g_signal_emit(array, gwy_array_signals[ITEM_INSERTED], 0, n + i);
    }

    return gwy_array_index(priv, n);
}

/**
 * gwy_array_append1:
 * @array: An array.
 * @item: Item to append.
 *
 * Appends an item to an array.
 *
 * Returns: Pointer to the appended item in the array, for convenience.
 **/
/**
 * gwy_array_append:
 * @array: An array.
 * @items: Items to append.
 * @nitems: Number of items to append.
 *
 * Appends items into an array.
 *
 * Returns: Pointer to the first appended item in the array, for convenience.
 **/
gpointer
gwy_array_append(GwyArray *array,
                 gpointer items,
                 guint nitems)
{
    g_return_val_if_fail(GWY_IS_ARRAY(array), NULL);
    Array *priv = array->priv;
    g_return_val_if_fail(priv->type_set, NULL);
    if (G_UNLIKELY(!nitems))
        return NULL;
    g_return_val_if_fail(items, NULL);

    ensure_items(priv);
    for (guint i = 0; i < nitems; i++) {
        g_array_append_vals(priv->items, gwy_data_index(priv, items, i), 1);
        g_signal_emit(array, gwy_array_signals[ITEM_INSERTED], 0,
                      priv->items->len - 1);
    }

    return gwy_array_index(priv, priv->items->len - nitems);
}

/**
 * gwy_array_delete1:
 * @array: An array.
 * @n: Position of the item to delete.
 *
 * Deletes an item from an array.
 **/
/**
 * gwy_array_delete:
 * @array: An array.
 * @n: Position of items to delete.
 * @nitems: Number of items to delete.
 *
 * Deletes items from an array.
 **/
void
gwy_array_delete(GwyArray *array,
                 guint n,
                 guint nitems)
{
    g_return_if_fail(GWY_IS_ARRAY(array));
    Array *priv = array->priv;
    g_return_if_fail(priv->type_set);
    if (nitems == 0)
        return;
    g_return_if_fail(priv->items && n + nitems <= priv->items->len);

    for (guint i = 0; i < nitems; i++) {
        guint j = n + nitems-1 - i;
        if (priv->destroy)
            priv->destroy(gwy_array_index(priv, j));
        g_array_remove_index(priv->items, j);
        g_signal_emit(array, gwy_array_signals[ITEM_DELETED], 0, j);
    }
}

/**
 * gwy_array_replace1:
 * @array: An array.
 * @n: Position of the item to replace.
 * @item: Item to replace the existing item with.
 *
 * Replaces an item in an array.
 **/
/**
 * gwy_array_replace:
 * @array: An array.
 * @n: Position of items to replace.
 * @items: Items to replace existing items with.
 * @nitems: Number of items to replace.
 *
 * Replaces items in an array.
 **/
void
gwy_array_replace(GwyArray *array,
                  guint n,
                  gpointer items,
                  guint nitems)
{
    g_return_if_fail(GWY_IS_ARRAY(array));
    Array *priv = array->priv;
    g_return_if_fail(priv->type_set);
    if (nitems == 0)
        return;
    g_return_if_fail(priv->items && n + nitems <= priv->items->len);

    for (guint i = 0; i < nitems; i++) {
        guint j = n + i;
        if (priv->destroy)
            priv->destroy(gwy_array_index(priv, j));
        memcpy(gwy_array_index(priv, j), gwy_data_index(priv, items, i),
               priv->size);
        g_signal_emit(array, gwy_array_signals[ITEM_UPDATED], 0, j);
    }
}

/**
 * gwy_array_get_data:
 * @array: An array.
 *
 * Returns pointer to array data.
 *
 * Returns: The complete array data, owned by the array.  It must not be
 *          freed.  The returned pointer remains valid only as long as the
 *          array do not change.  If the array is empty, %NULL is returned.
 **/
gpointer
gwy_array_get_data(GwyArray *array)
{
    g_return_val_if_fail(GWY_IS_ARRAY(array), NULL);
    Array *priv = array->priv;
    g_return_val_if_fail(priv->type_set, NULL);
    return (priv->items && priv->items->len) ? priv->items->data : NULL;
}

/**
 * gwy_array_set_data:
 * @array: An array.
 * @items: Items to fill the array with.
 * @nitems: Number of items.
 *
 * Completely replaces the contents of an array.
 *
 * Note this can emit lots of signals, use gwy_array_new_with_data() to
 * construct an array filled with items.  The replacement is performed by
 * first deleting any extra items if there are more than @nitems, then
 * replacing the existing items with data from @items and finally appending
 * remaining items if there were less items than @nitems.
 **/
void
gwy_array_set_data(GwyArray *array,
                   gpointer items,
                   guint nitems)
{
    g_return_if_fail(GWY_IS_ARRAY(array));
    Array *priv = array->priv;
    g_return_if_fail(priv->type_set);

    ensure_items(priv);
    if (nitems < priv->items->len)
        gwy_array_delete(array, nitems, priv->items->len - nitems);
    if (nitems) {
        gwy_array_replace(array, 0, items, priv->items->len);
        if (nitems > priv->items->len)
            gwy_array_append(array, gwy_data_index(priv, items,
                                                   priv->items->len),
                             nitems - priv->items->len);
    }
}

#define __LIBGWY_ARRAY_C__
#include "libgwy/libgwy-aliases.c"

/**
 * SECTION: array
 * @title: GwyArray
 * @short_description: #GArray wrapper capable of emitting signals
 * @see_also: #GwyInventory
 *
 * #GwyArray is a uniform container.
 **/

/**
 * GwyArray:
 *
 * Object representing an array.
 *
 * The #GwyArray struct contains private data only and should be accessed
 * using the functions below.
 **/

/**
 * GwyArrayClass:
 *
 * Class of arrays.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
