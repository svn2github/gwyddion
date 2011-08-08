/*
 *  $Id$
 *  Copyright (C) 2009 David Nečas (Yeti).
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
#include <stdlib.h>
#include <glib/gi18n-lib.h>
#include "libgwy/macros.h"
#include "libgwy/strfuncs.h"
#include "libgwy/serializable.h"
#include "libgwy/inventory.h"
#include "libgwy/object-internal.h"

enum {
    ITEM_INSERTED,
    ITEM_DELETED,
    ITEM_UPDATED,
    ITEMS_REORDERED,
    DEFAULT_CHANGED,
    N_SIGNALS
};

struct _GwyInventoryPrivate {
    GObject g_object;

    GSequence *items;
    GHashTable *hash;

    GwyInventoryItemType item_type;
    gboolean has_item_type : 1;
    gboolean is_sorted : 1;
    gboolean is_object : 1;
    gboolean is_watchable : 1;
    gboolean can_make_copies : 1;
    gchar *default_key;
};

typedef struct _GwyInventoryPrivate Inventory;

static void         gwy_inventory_finalize(GObject *object);
static void         gwy_inventory_dispose (GObject *object);
static void         make_hash             (Inventory *inventory);
static void         emit_item_updated     (GwyInventory *inventory,
                                           GSequenceIter *iter);
static void         discard_item          (GwyInventory *inventory,
                                           GSequenceIter *iter);
static void         register_item         (GwyInventory *inventory,
                                           GSequenceIter *iter,
                                           gpointer item);
static void         item_changed          (GwyInventory *inventory,
                                           gpointer item);
static gboolean     item_is_in_order      (Inventory *inventory,
                                           GSequenceIter *iter,
                                           gpointer item);
static const gchar* invent_item_name      (Inventory *inventory,
                                           const gchar *prefix);

static guint inventory_signals[N_SIGNALS];

G_DEFINE_TYPE(GwyInventory, gwy_inventory, G_TYPE_OBJECT);

static void
gwy_inventory_class_init(GwyInventoryClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    g_type_class_add_private(klass, sizeof(Inventory));

    gobject_class->dispose = gwy_inventory_dispose;
    gobject_class->finalize = gwy_inventory_finalize;

    /**
     * GwyInventory::item-inserted:
     * @gwyinventory: The #GwyInventory which received the signal.
     * @arg1: Position the item was inserted at.
     *
     * The ::item-inserted signal is emitted when an item is inserted into
     * the inventory.
     **/
    inventory_signals[ITEM_INSERTED]
        = g_signal_new_class_handler("item-inserted",
                                     GWY_TYPE_INVENTORY,
                                     G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE,
                                     NULL, NULL, NULL,
                                     g_cclosure_marshal_VOID__UINT,
                                     G_TYPE_NONE, 1, G_TYPE_UINT);

    /**
     * GwyInventory::item-deleted:
     * @gwyinventory: The #GwyInventory which received the signal.
     * @arg1: Position the item was deleted from.
     *
     * The ::item-deleted signal is emitted when an item is deleted from
     * the inventory.
     **/
    inventory_signals[ITEM_DELETED]
        = g_signal_new_class_handler("item-deleted",
                                     GWY_TYPE_INVENTORY,
                                     G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE,
                                     NULL, NULL, NULL,
                                     g_cclosure_marshal_VOID__UINT,
                                     G_TYPE_NONE, 1, G_TYPE_UINT);

    /**
     * GwyInventory::item-updated:
     * @gwyinventory: The #GwyInventory which received the signal.
     * @arg1: Position of updated item.
     *
     * The ::item-updated signal is emitted when an item in the inventory
     * is updated.
     **/
    inventory_signals[ITEM_UPDATED]
        = g_signal_new_class_handler("item-updated",
                                     GWY_TYPE_INVENTORY,
                                     G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE,
                                     NULL, NULL, NULL,
                                     g_cclosure_marshal_VOID__UINT,
                                     G_TYPE_NONE, 1, G_TYPE_UINT);

    /**
     * GwyInventory::items-reordered:
     * @gwyinventory: The #GwyInventory which received the signal.
     * @arg1: New item order map as in #GtkTreeModel,
     *        @arg1[new_position] = old_position.
     *
     * The ::items-reordered signal is emitted when item in the inventory
     * are reordered.
     **/
    inventory_signals[ITEMS_REORDERED]
        = g_signal_new_class_handler("items-reordered",
                                     GWY_TYPE_INVENTORY,
                                     G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE,
                                     NULL, NULL, NULL,
                                     g_cclosure_marshal_VOID__POINTER,
                                     G_TYPE_NONE, 1, G_TYPE_POINTER);

    /**
     * GwyInventory::default-changed:
     * @gwyinventory: The #GwyInventory which received the signal.
     *
     * The ::default-changed signal is emitted when either
     * default inventory item name changes or the presence of such an item
     * in the inventory changes.
     **/
    inventory_signals[DEFAULT_CHANGED]
        = g_signal_new_class_handler("default-changed",
                                     GWY_TYPE_INVENTORY,
                                     G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE,
                                     NULL, NULL, NULL,
                                     g_cclosure_marshal_VOID__VOID,
                                     G_TYPE_NONE, 0);
}

static void
gwy_inventory_init(GwyInventory *inventory)
{
    inventory->priv = G_TYPE_INSTANCE_GET_PRIVATE(inventory, GWY_TYPE_INVENTORY,
                                                  Inventory);
    inventory->priv->items = g_sequence_new(NULL);
}

static void
gwy_inventory_finalize(GObject *object)
{
    GwyInventory *inventory = GWY_INVENTORY(object);
    Inventory *priv = inventory->priv;

    GWY_FREE(priv->default_key);
    g_sequence_free(priv->items);
    if (priv->hash)
        g_hash_table_destroy(priv->hash);

    G_OBJECT_CLASS(gwy_inventory_parent_class)->finalize(object);
}

/* This removes items indiscriminately, objects or not. */
static void
gwy_inventory_dispose(GObject *object)
{
    GwyInventory *inventory = GWY_INVENTORY(object);
    Inventory *priv = inventory->priv;
    GSequenceIter *iter, *next;

    for (iter = g_sequence_get_begin_iter(priv->items);
         !g_sequence_iter_is_end(iter);
         iter = next) {
        next = g_sequence_iter_next(iter);
        discard_item(inventory, iter);
    }
    G_OBJECT_CLASS(gwy_inventory_parent_class)->dispose(object);
}

static inline GSequenceIter*
lookup_item(Inventory *inventory,
            const gchar *name)
{
    if (G_UNLIKELY(!inventory->hash))
        make_hash(inventory);

    return g_hash_table_lookup(inventory->hash, name);
}

/**
 * gwy_inventory_new:
 *
 * Creates a new inventory.
 *
 * Returns: A new inventory.
 **/
GwyInventory*
gwy_inventory_new(void)
{
    return g_object_newv(GWY_TYPE_INVENTORY, 0, NULL);
}

/**
 * gwy_inventory_new_with_type:
 * @itype: Type of items the inventory will contain.
 *
 * Creates a new inventory with specified item type.
 *
 * Returns: A new inventory.
 **/
GwyInventory*
gwy_inventory_new_with_type(const GwyInventoryItemType *itype)
{
    GwyInventory *inventory = g_object_newv(GWY_TYPE_INVENTORY, 0, NULL);
    gwy_inventory_set_item_type(inventory, itype);
    return inventory;
}

/**
 * gwy_inventory_new_with_items:
 * @itype: Type of items the inventory will contain.
 * @nitems: The number of pointers in @items.
 * @pitems: Array of item pointers to fill the newly created inventory with.
 *
 * Creates a new inventory and fills it with items.
 *
 * Returns: A new inventory.
 **/
GwyInventory*
gwy_inventory_new_with_items(const GwyInventoryItemType *itype,
                             guint nitems,
                             gpointer pitems)
{
    GwyInventory *inventory = gwy_inventory_new_with_type(itype);
    Inventory *priv = inventory->priv;
    gpointer *items = pitems;

    for (guint i = 0; i < nitems; i++) {
        gpointer item = items[i];
        GSequenceIter *iter = g_sequence_append(priv->items, item);
        if (priv->is_sorted && i)
            priv->is_sorted = (itype->compare(items[i-1], item) < 0);
        register_item(inventory, iter, item);
    }

    return inventory;
}

/**
 * gwy_inventory_set_item_type:
 * @inventory: An inventory.
 * @itype: Type of items the inventory will contain.
 *
 * Sets the item type information of an inventory.
 *
 * This function can be called only just after the initialization, before any
 * items are set or looked up.  It is intended for classes that inherit from
 * inventory and to permit generic construction with g_object_new().
 **/
void
gwy_inventory_set_item_type(GwyInventory *inventory,
                            const GwyInventoryItemType *itype)
{
    g_return_if_fail(GWY_IS_INVENTORY(inventory));
    Inventory *priv = inventory->priv;
    g_return_if_fail(g_sequence_get_length(priv->items) == 0);

    priv->has_item_type = TRUE;
    priv->item_type = *itype;
    if (itype->type) {
        priv->is_object = g_type_is_a(itype->type, G_TYPE_OBJECT);
        priv->is_watchable = (itype->watchable_signal != NULL);
    }
    else {
        priv->is_object = priv->is_watchable = FALSE;
    }

    priv->can_make_copies = itype->rename && itype->copy;
    priv->is_sorted = (itype->compare != NULL);
}

/**
 * gwy_inventory_size:
 * @inventory: An inventory.
 *
 * Returns the number of items in an inventory.
 *
 * Returns: The number of items.
 **/
guint
gwy_inventory_size(GwyInventory *inventory)
{
    g_return_val_if_fail(GWY_IS_INVENTORY(inventory), 0);
    Inventory *priv = inventory->priv;
    g_return_val_if_fail(priv->has_item_type, 0);
    return (guint)g_sequence_get_length(priv->items);
}

/**
 * gwy_inventory_can_make_copies:
 * @inventory: An inventory.
 *
 * Returns whether an inventory can create new items itself.
 *
 * Returns: %TRUE if inventory can create new items itself.
 **/
gboolean
gwy_inventory_can_make_copies(GwyInventory *inventory)
{
    g_return_val_if_fail(GWY_IS_INVENTORY(inventory), FALSE);
    Inventory *priv = inventory->priv;
    g_return_val_if_fail(priv->has_item_type, FALSE);
    return priv->can_make_copies;
}

/**
 * gwy_inventory_get_item_type:
 * @inventory: An inventory.
 *
 * Returns the type of item an inventory holds.
 *
 * Returns: The item type.  It is owned by inventory and must not be modified
 *          or freed.
 **/
const GwyInventoryItemType*
gwy_inventory_get_item_type(GwyInventory *inventory)
{
    g_return_val_if_fail(GWY_IS_INVENTORY(inventory), NULL);
    Inventory *priv = inventory->priv;
    g_return_val_if_fail(priv->has_item_type, NULL);
    return &priv->item_type;
}

/**
 * gwy_inventory_get:
 * @inventory: An inventory.
 * @name: Item name.
 *
 * Looks up an item in an inventory.
 *
 * Returns: Item called @name, or %NULL if there is no such item.
 **/
gpointer
gwy_inventory_get(GwyInventory *inventory,
                  const gchar *name)
{
    g_return_val_if_fail(GWY_IS_INVENTORY(inventory), NULL);
    Inventory *priv = inventory->priv;
    g_return_val_if_fail(priv->has_item_type, NULL);
    GSequenceIter *iter;

    if ((iter = lookup_item(priv, name)))
        return g_sequence_get(iter);
    else
        return NULL;
}

/**
 * gwy_inventory_get_or_default:
 * @inventory: An inventory.
 * @name: Item name.
 *
 * Looks up an item in an inventory, eventually falling back to default.
 *
 * The lookup order is: the item of requested name, default item (if set), any
 * inventory item, %NULL (this can happen only when inventory is empty).
 *
 * Returns: Item called @name, or default item.
 **/
gpointer
gwy_inventory_get_or_default(GwyInventory *inventory,
                             const gchar *name)
{
    g_return_val_if_fail(GWY_IS_INVENTORY(inventory), NULL);
    Inventory *priv = inventory->priv;
    g_return_val_if_fail(priv->has_item_type, NULL);
    GSequenceIter *iter;

    if ((name && (iter = lookup_item(priv, name)))
        || (priv->default_key
            && (iter = lookup_item(priv, priv->default_key)))
        || ((iter = g_sequence_get_begin_iter(priv->items))
            && !g_sequence_iter_is_end(iter)))
        return g_sequence_get(iter);

    return NULL;
}

/**
 * gwy_inventory_get_nth:
 * @inventory: An inventory.
 * @n: Item position.  It must be between zero and the number of items in
 *     inventory, inclusive.  If it is equal to the number of items, %NULL
 *     is returned.  In other words, inventory behaves like a %NULL-terminated
 *     array, you can simply iterate over it until gwy_inventory_get_nth()
 *     returns %NULL.
 *
 * Returns item at given position in an inventory.
 *
 * Returns: Item at given position.
 **/
gpointer
gwy_inventory_get_nth(GwyInventory *inventory,
                      guint n)
{
    g_return_val_if_fail(GWY_IS_INVENTORY(inventory), NULL);
    Inventory *priv = inventory->priv;
    g_return_val_if_fail(priv->has_item_type, NULL);
    guint nitems = (guint)g_sequence_get_length(priv->items);
    g_return_val_if_fail(n <= nitems, NULL);
    if (G_UNLIKELY(n == nitems))
        return NULL;
    else
        return g_sequence_get(g_sequence_get_iter_at_pos(priv->items, n));
}

/**
 * gwy_inventory_position:
 * @inventory: An inventory.
 * @name: Item name.
 *
 * Finds position of an item in an inventory.
 *
 * Returns: Item position, or %G_MAXUINT if there is no such
 * item.
 **/
guint
gwy_inventory_position(GwyInventory *inventory,
                       const gchar *name)
{
    GSequenceIter *iter;

    g_return_val_if_fail(GWY_IS_INVENTORY(inventory), G_MAXUINT);
    Inventory *priv = inventory->priv;
    g_return_val_if_fail(priv->has_item_type, G_MAXUINT);
    if (!(iter = lookup_item(priv, name)))
        return G_MAXUINT;

    return g_sequence_iter_get_position(iter);
}

/**
 * gwy_inventory_foreach:
 * @inventory: An inventory.
 * @function: A function to call on each item.  It must not modify @inventory.
 * @user_data: Data passed to @function.
 *
 * Calls a function on each item of an inventory, in order.
 **/
void
gwy_inventory_foreach(GwyInventory *inventory,
                      GwyInventoryForeachFunc function,
                      gpointer user_data)
{
    g_return_if_fail(GWY_IS_INVENTORY(inventory));
    Inventory *priv = inventory->priv;
    g_return_if_fail(priv->has_item_type);
    g_return_if_fail(function);

    guint i = 0;
    for (GSequenceIter *iter = g_sequence_get_begin_iter(priv->items);
         !g_sequence_iter_is_end(iter);
         iter = g_sequence_iter_next(iter), i++) {
        function(i, g_sequence_get(iter), user_data);
    }
}

/**
 * gwy_inventory_find:
 * @inventory: An inventory.
 * @predicate: A function testing some item property.  It must not modify
 *             @inventory.
 * @user_data: Data passed to @predicate.
 *
 * Finds an inventory item using user-specified predicate function.
 *
 * @predicate is called for each item in @inventory (in order) until it returns
 * %TRUE.
 *
 * Returns: The item for which @predicate returned %TRUE.  If there is no
 *          such item in the inventory, %NULL is returned.
 **/
gpointer
gwy_inventory_find(GwyInventory *inventory,
                   GwyInventoryFindFunc predicate,
                   gpointer user_data)
{
    g_return_val_if_fail(GWY_IS_INVENTORY(inventory), NULL);
    Inventory *priv = inventory->priv;
    g_return_val_if_fail(priv->has_item_type, NULL);
    g_return_val_if_fail(predicate, NULL);

    guint i = 0;
    for (GSequenceIter *iter = g_sequence_get_begin_iter(priv->items);
         !g_sequence_iter_is_end(iter);
         iter = g_sequence_iter_next(iter), i++) {
        if (predicate(i, g_sequence_get(iter), user_data))
            return g_sequence_get(iter);
    }

    return NULL;
}

/**
 * gwy_inventory_get_default:
 * @inventory: An inventory.
 *
 * Returns the default item of an inventory.
 *
 * Returns: The default item.  If there is no default item, %NULL is returned.
 **/
gpointer
gwy_inventory_get_default(GwyInventory *inventory)
{
    g_return_val_if_fail(GWY_IS_INVENTORY(inventory), NULL);
    Inventory *priv = inventory->priv;
    g_return_val_if_fail(priv->has_item_type, NULL);
    if (!priv->default_key)
        return NULL;

    GSequenceIter *iter;
    if ((iter = lookup_item(priv, priv->default_key)))
        return g_sequence_get(iter);
    return NULL;
}

/**
 * gwy_inventory_get_default_name:
 * @inventory: An inventory.
 *
 * Returns the name of the default item of an inventory.
 *
 * Returns: The default item name, %NULL if no default name is set.
 *          Item of this name may or may not exist in the inventory.
 **/
const gchar*
gwy_inventory_get_default_name(GwyInventory *inventory)
{
    g_return_val_if_fail(GWY_IS_INVENTORY(inventory), NULL);
    Inventory *priv = inventory->priv;
    g_return_val_if_fail(priv->has_item_type, NULL);

    return priv->default_key;
}

/**
 * gwy_inventory_set_default_name:
 * @inventory: An inventory.
 * @name: Item name, pass %NULL to unset default item.
 *
 * Sets the default of an inventory.
 *
 * Item @name must already exist in the inventory.
 **/
void
gwy_inventory_set_default_name(GwyInventory *inventory,
                               const gchar *name)
{
    g_return_if_fail(GWY_IS_INVENTORY(inventory));
    Inventory *priv = inventory->priv;
    g_return_if_fail(priv->has_item_type);
    if (_gwy_assign_string(&priv->default_key, name))
        g_signal_emit(inventory, inventory_signals[DEFAULT_CHANGED], 0);
}

/**
 * gwy_inventory_updated:
 * @inventory: An inventory.
 * @name: Item name.
 *
 * Notifies inventory an item was updated.
 *
 * This function makes sense primarily for non-object items, as object items
 * can notify inventory via signals.
 **/
void
gwy_inventory_updated(GwyInventory *inventory,
                      const gchar *name)
{
    GSequenceIter *iter;

    g_return_if_fail(GWY_IS_INVENTORY(inventory));
    Inventory *priv = inventory->priv;
    g_return_if_fail(priv->has_item_type);
    if (!(iter = lookup_item(priv, name)))
        g_warning("Item ‘%s’ does not exist", name);
    else
        emit_item_updated(inventory, iter);
}

/**
 * gwy_inventory_nth_updated:
 * @inventory: An inventory.
 * @n: Item position.
 *
 * Notifies inventory item at given position was updated.
 *
 * This function makes sense primarily for non-object items, as object items
 * can notify inventory via signals.
 **/
void
gwy_inventory_nth_updated(GwyInventory *inventory,
                          guint n)
{
    g_return_if_fail(GWY_IS_INVENTORY(inventory));
    Inventory *priv = inventory->priv;
    g_return_if_fail(priv->has_item_type);
    g_return_if_fail(n < (guint)g_sequence_get_length(priv->items));

    g_signal_emit(inventory, inventory_signals[ITEM_UPDATED], 0, n);
}

/**
 * gwy_inventory_insert:
 * @inventory: An inventory.
 * @item: An item to insert.
 *
 * Inserts an item into an inventory.
 *
 * Item of the same name must not exist yet.
 *
 * If the inventory is sorted, item is inserted to keep order.  If the
 * inventory is unsorted, item is simply added to the end.
 *
 * Returns: @item, for convenience.
 **/
gpointer
gwy_inventory_insert(GwyInventory *inventory,
                     gpointer item)
{
    g_return_val_if_fail(GWY_IS_INVENTORY(inventory), NULL);
    Inventory *priv = inventory->priv;
    g_return_val_if_fail(priv->has_item_type, NULL);
    g_return_val_if_fail(item, NULL);

    const gchar *name = priv->item_type.get_name(item);
    if (lookup_item(priv, name)) {
        g_warning("Item ‘%s’ already exists", name);
        return NULL;
    }

    GSequenceIter *iter;
    if (priv->is_sorted) {
        GCompareDataFunc compare
            = (GCompareDataFunc)priv->item_type.compare;
        iter = g_sequence_insert_sorted(priv->items, item, compare, NULL);
    }
    else
        iter = g_sequence_append(priv->items, item);

    register_item(inventory, iter, item);

    g_signal_emit(inventory, inventory_signals[ITEM_INSERTED], 0,
                  g_sequence_iter_get_position(iter));
    if (priv->default_key
        && gwy_strequal(name, priv->default_key))
        g_signal_emit(inventory, inventory_signals[DEFAULT_CHANGED], 0);

    return item;
}

/**
 * gwy_inventory_insert_nth:
 * @inventory: An inventory.
 * @item: An item to insert.
 * @n: Position to insert @item to.
 *
 * Inserts an item to an explicit position in an inventory.
 *
 * Item of the same name must not exist yet.
 *
 * If the item is sorted in a position where it does not belong according to
 * the item comparation function, the inventory becomes unsorted.
 *
 * Returns: @item, for convenience.
 **/
gpointer
gwy_inventory_insert_nth(GwyInventory *inventory,
                         gpointer item,
                         guint n)
{
    g_return_val_if_fail(GWY_IS_INVENTORY(inventory), NULL);
    Inventory *priv = inventory->priv;
    g_return_val_if_fail(priv->has_item_type, NULL);
    g_return_val_if_fail(item, NULL);
    guint nitems = (guint)g_sequence_get_length(priv->items);
    g_return_val_if_fail(n <= nitems, NULL);

    const gchar *name = priv->item_type.get_name(item);
    if (lookup_item(priv, name)) {
        g_warning("Item ‘%s’ already exists", name);
        return NULL;
    }

    GSequenceIter *iter;
    if (n == nitems)
        iter = g_sequence_append(priv->items, item);
    else {
        iter = g_sequence_get_iter_at_pos(priv->items, n);
        iter = g_sequence_insert_before(iter, item);
    }

    priv->is_sorted = item_is_in_order(priv, iter, item);
    register_item(inventory, iter, item);

    g_signal_emit(inventory, inventory_signals[ITEM_INSERTED], 0, n);
    if (priv->default_key
        && gwy_strequal(name, priv->default_key))
        g_signal_emit(inventory, inventory_signals[DEFAULT_CHANGED], 0);

    return item;
}

/**
 * gwy_inventory_restore_order:
 * @inventory: An inventory.
 *
 * Ensures an inventory is sorted.
 *
 * If the inventory was not sorted it will be completely sorted.   This is
 * a relatively expensive operation.
 **/
void
gwy_inventory_restore_order(GwyInventory *inventory)
{
    g_return_if_fail(GWY_IS_INVENTORY(inventory));
    Inventory *priv = inventory->priv;
    g_return_if_fail(priv->has_item_type);
    if (priv->is_sorted || !priv->item_type.compare)
        return;

    // Remember old positions if we have to.
    guint nitems = (guint)g_sequence_get_length(priv->items);
    GSequenceIter **positions = NULL;
    if (g_signal_has_handler_pending(inventory,
                                     inventory_signals[ITEMS_REORDERED],
                                     0, FALSE)) {
        positions = g_slice_alloc(sizeof(GSequenceIter*)*nitems);
        GSequenceIter *iter = g_sequence_get_begin_iter(priv->items);
        for (guint i = 0; i < nitems; i++) {
            positions[i] = iter;
            iter = g_sequence_iter_next(iter);
        }
    }

    // Sort.
    g_sequence_sort(priv->items,
                    (GCompareDataFunc)priv->item_type.compare, NULL);
    priv->is_sorted = TRUE;
    if (!positions)
        return;

    // Fill new_order with indices: new_order[new_position] = old_position
    guint *new_order = g_slice_alloc(sizeof(guint)*nitems);
    for (guint i = 0; i < nitems; i++)
        new_order[g_sequence_iter_get_position(positions[i])] = i;

    g_slice_free1(sizeof(GSequenceIter*)*nitems, positions);
    g_signal_emit(inventory, inventory_signals[ITEMS_REORDERED], 0,
                  new_order);
    g_slice_free1(sizeof(guint)*nitems, new_order);
}

/**
 * gwy_inventory_forget_order:
 * @inventory: An inventory.
 *
 * Forces an inventory to be unsorted.
 *
 * Item positions do not change, but future gwy_inventory_insert() will
 * not try to insert items in order.
 **/
void
gwy_inventory_forget_order(GwyInventory *inventory)
{
    g_return_if_fail(GWY_IS_INVENTORY(inventory));
    Inventory *priv = inventory->priv;
    g_return_if_fail(priv->has_item_type);
    priv->is_sorted = FALSE;
}

/**
 * delete_item:
 * @inventory: An inventory.
 * @iter: #GSequence iterator.
 * @name: Item name (to avoid double lookups from gwy_inventory_delete_item()).
 * @n: Item position.
 *
 * Removes an item from an inventory given its physical position.
 **/
static void
delete_item(GwyInventory *inventory,
            GSequenceIter *iter,
            const gchar *name,
            guint n)
{
    Inventory *priv = inventory->priv;
    if (priv->item_type.is_modifiable) {
        gpointer item = g_sequence_get(iter);
        if (!priv->item_type.is_modifiable(item)) {
            g_warning("Cannot delete non-modifiable item ‘%s’", name);
            return;
        }
    }

    gboolean emit_change = (priv->default_key
                            && gwy_strequal(name, priv->default_key));
    discard_item(inventory, iter);

    g_signal_emit(inventory, inventory_signals[ITEM_DELETED], 0, n);
    if (emit_change)
        g_signal_emit(inventory, inventory_signals[DEFAULT_CHANGED], 0);
}

/**
 * gwy_inventory_delete:
 * @inventory: An inventory.
 * @name: Name of item to delete.
 *
 * Deletes an item from an inventory.
 **/
void
gwy_inventory_delete(GwyInventory *inventory,
                     const gchar *name)
{
    g_return_if_fail(GWY_IS_INVENTORY(inventory));
    Inventory *priv = inventory->priv;
    g_return_if_fail(priv->has_item_type);

    GSequenceIter *iter;
    if (!(iter = lookup_item(priv, name))) {
        g_warning("Item ‘%s’ does not exist", name);
        return;
    }

    delete_item(inventory, iter, name, g_sequence_iter_get_position(iter));
}

/**
 * gwy_inventory_delete_nth:
 * @inventory: An inventory.
 * @n: Position of @item to delete.
 *
 * Deletes the item at given position from an inventory.
 **/
void
gwy_inventory_delete_nth(GwyInventory *inventory,
                         guint n)
{
    g_return_if_fail(GWY_IS_INVENTORY(inventory));
    Inventory *priv = inventory->priv;
    g_return_if_fail(priv->has_item_type);
    g_return_if_fail(n < (guint)g_sequence_get_length(priv->items));

    GSequenceIter *iter = g_sequence_get_iter_at_pos(priv->items, n);
    const gchar *name = priv->item_type.get_name(g_sequence_get(iter));
    delete_item(inventory, iter, name, n);
}

// Emit "items-reordered" for an item moved from position @nold to @nnew in
// the sequence.
static void
emit_reordered_for_move(GwyInventory *inventory,
                        guint nold,
                        guint nnew)
{
    if (nold == nnew
        || !g_signal_has_handler_pending(inventory,
                                         inventory_signals[ITEMS_REORDERED],
                                         0, FALSE))
        return;

    Inventory *priv = inventory->priv;
    guint nitems = (guint)g_sequence_get_length(priv->items);
    guint *new_order = g_slice_alloc(sizeof(guint)*nitems);
    if (nold < nnew) {
        for (guint i = 0; i < nold; i++)
            new_order[i] = i;
        for (guint i = nold; i < nnew; i++)
            new_order[i] = i + 1;
        new_order[nnew] = nold;
        for (guint i = nnew+1; i < nitems; i++)
            new_order[i] = i;
    }
    else {
        for (guint i = 0; i < nnew; i++)
            new_order[i] = i;
        new_order[nnew] = nold;
        for (guint i = nnew+1; i < nold; i++)
            new_order[i] = i - 1;
        for (guint i = nold; i < nitems; i++)
            new_order[i] = i;
    }

    g_signal_emit(inventory, inventory_signals[ITEMS_REORDERED], 0,
                  new_order);
    g_slice_free1(sizeof(guint)*nitems, new_order);
}

/**
 * gwy_inventory_rename:
 * @inventory: An inventory.
 * @name: Name of item to rename.
 * @newname: New name of item.
 *
 * Renames an inventory item.
 *
 * It is an error to rename an item to @newname that is already present in
 * @inventory.
 *
 * If the item needs to be moved in order to keep the inventory sorted, this
 * will cause reordering of the items and emission of
 * GwyInventory::items-reordered.  Signals GwyInventory::item-updated and
 * possibly GwyInventory::default-changed will be emitted subsequently, when
 * the item is already in the final position.
 *
 * Returns: The item, for convenience.
 **/
gpointer
gwy_inventory_rename(GwyInventory *inventory,
                     const gchar *name,
                     const gchar *newname)
{
    g_return_val_if_fail(GWY_IS_INVENTORY(inventory), NULL);
    Inventory *priv = inventory->priv;
    g_return_val_if_fail(priv->has_item_type, NULL);
    g_return_val_if_fail(newname, NULL);
    g_return_val_if_fail(priv->item_type.rename, NULL);

    GSequenceIter *iter;
    if (!(iter = lookup_item(priv, name))) {
        g_warning("Item ‘%s’ does not exist", name);
        return NULL;
    }

    gpointer item = g_sequence_get(iter);
    if (priv->item_type.is_modifiable
        && !priv->item_type.is_modifiable(item)) {
        g_warning("Cannot rename non-modifiable item ‘%s’", name);
        return NULL;
    }
    if (gwy_strequal(name, newname))
        return item;

    if (lookup_item(priv, newname)) {
        g_warning("Item ‘%s’ already exists", newname);
        return NULL;
    }

    g_hash_table_remove(priv->hash, name);
    gboolean was_sorted = item_is_in_order(priv, iter, item);
    priv->item_type.rename(item, newname);
    gboolean is_sorted_now = item_is_in_order(priv, iter, item);
    const gchar *realnewname = priv->item_type.get_name(item);
    if (!gwy_strequal(realnewname, newname)) {
        g_warning("Item ‘%s’ was asked to rename to ‘%s’ but it renamed self "
                  "to ‘%s’.", name, newname, realnewname);
        if (lookup_item(priv, realnewname))
            g_critical("Failed rename led to duplicite items ‘%s’.",
                       realnewname);
    }

    /* Use newname even if the item did not obey. */
    g_hash_table_insert(priv->hash, (gpointer)newname, iter);
    /* Move, if necessary for keeping the sort order. */
    guint n = (guint)g_sequence_iter_get_position(iter);
    if (was_sorted && !is_sorted_now) {
        guint nold = n;
        g_sequence_remove(iter);
        GCompareDataFunc compare = (GCompareDataFunc)priv->item_type.compare;
        iter = g_sequence_insert_sorted(priv->items, item, compare, NULL);
        guint nnew = (guint)g_sequence_iter_get_position(iter);
        emit_reordered_for_move(inventory, nold, nnew);
        n = nnew;
    }

    g_signal_emit(inventory, inventory_signals[ITEM_UPDATED], 0, n);
    if (priv->default_key
        && (gwy_strequal(name, priv->default_key)
            || gwy_strequal(newname, priv->default_key)))
        g_signal_emit(inventory, inventory_signals[DEFAULT_CHANGED], 0);

    return iter;
}

/**
 * gwy_inventory_copy:
 * @inventory: An inventory.
 * @name: Name of item to duplicate, may be %NULL to use default item (the
 *        same happens when @name does not exist).
 * @newname: Name of new item, it must not exist yet.  It may be %NULL, the
 *           new name is based on @name then.
 *
 * Creates a new item as a copy of existing one and inserts it to inventory.
 *
 * The newly created item can be called differently than @newname if that
 * already exists.
 *
 * Returns: The newly added item.
 **/
gpointer
gwy_inventory_copy(GwyInventory *inventory,
                   const gchar *name,
                   const gchar *newname)
{
    g_return_val_if_fail(GWY_IS_INVENTORY(inventory), NULL);
    Inventory *priv = inventory->priv;
    g_return_val_if_fail(priv->has_item_type, NULL);
    g_return_val_if_fail(priv->can_make_copies, NULL);

    /* Find which item we should base copy on */
    if (!name && priv->default_key)
        name = priv->default_key;

    GSequenceIter *iter = NULL;
    if ((!name || !(iter = lookup_item(priv, name)))
        && g_sequence_get_length(priv->items))
        iter = g_sequence_get_begin_iter(priv->items);

    if (!iter) {
        g_warning("No default item to base new item on");
        return NULL;
    }

    gpointer item = g_sequence_get(iter);
    if (!name)
        name = priv->item_type.get_name(item);

    /* Find new name */
    if (!newname)
        newname = invent_item_name(priv, name);
    else if (lookup_item(priv, newname))
        newname = invent_item_name(priv, newname);

    /* Create new item */
    item = priv->item_type.copy(item);
    priv->item_type.rename(item, newname);
    const gchar *realnewname = priv->item_type.get_name(item);
    if (!gwy_strequal(realnewname, newname)) {
        g_warning("Item ‘%s’ was asked to rename to ‘%s’ but it renamed self "
                  "to ‘%s’.", name, newname, realnewname);
    }
    gwy_inventory_insert(inventory, item);
    // Insert took a reference we don't want two.
    if (priv->is_object)
        g_object_unref(item);

    return item;
}

/**
 * emit_item_updated:
 * @inventory: An inventory.
 * @iter: #GSequence iterator.
 *
 * Emits "item-updated" signal.
 **/
static void
emit_item_updated(GwyInventory *inventory,
                  GSequenceIter *iter)
{
    g_signal_emit(inventory, inventory_signals[ITEM_UPDATED], 0,
                  g_sequence_iter_get_position(iter));
}

/**
 * item_changed:
 * @inventory: An inventory.
 * @item: Item that has changed.
 *
 * Handles inventory item `changed' signal.
 **/
static void
item_changed(GwyInventory *inventory,
             gpointer item)
{
    const gchar *name = inventory->priv->item_type.get_name(item);
    GSequenceIter *iter = lookup_item(inventory->priv, name);
    g_assert(iter);
    emit_item_updated(inventory, iter);
}

/* Not a complete inverse of register_item() because it also removes the
 * item physically from inventory->items. */
static void
discard_item(GwyInventory *inventory,
             GSequenceIter *iter)
{
    Inventory *priv = inventory->priv;
    gpointer item = g_sequence_get(iter);
    g_return_if_fail(item);
    if (priv->is_watchable)
        g_signal_handlers_disconnect_by_func(item, item_changed, inventory);
    g_sequence_remove(iter);
    g_hash_table_remove(priv->hash, priv->item_type.get_name(item));
    if (priv->item_type.destroy)
        priv->item_type.destroy(item);
    if (priv->is_object)
        g_object_unref(item);
}

static void
register_item(GwyInventory *inventory,
              GSequenceIter *iter,
              gpointer item)
{
    Inventory *priv = inventory->priv;
    if (priv->hash)
        g_hash_table_insert(priv->hash,
                            (gpointer)priv->item_type.get_name(item), iter);

    if (priv->is_object) {
        g_object_ref(item);
        if (priv->is_watchable)
            g_signal_connect_swapped(item,
                                     priv->item_type.watchable_signal,
                                     G_CALLBACK(item_changed), inventory);
    }
}

static gboolean
item_is_in_order(Inventory *inventory,
                 GSequenceIter *iter,
                 gpointer item)
{
    gboolean is_sorted = inventory->is_sorted;
    gint (*cmp)(gconstpointer, gconstpointer) = inventory->item_type.compare;

    if (is_sorted && !g_sequence_iter_is_begin(iter))
        is_sorted = cmp(item, g_sequence_get(g_sequence_iter_prev(iter))) > 0;
    if (is_sorted && !g_sequence_iter_is_end(iter))
        is_sorted = cmp(item, g_sequence_get(g_sequence_iter_next(iter))) < 0;

    return is_sorted;
}

static void
make_hash(Inventory *inventory)
{
    g_assert(!inventory->hash);
    inventory->hash = g_hash_table_new(g_str_hash, g_str_equal);

    const gchar* (*get_name)(gconstpointer) = inventory->item_type.get_name;
    for (GSequenceIter *iter = g_sequence_get_begin_iter(inventory->items);
         !g_sequence_iter_is_end(iter);
         iter = g_sequence_iter_next(iter)) {
        g_hash_table_insert(inventory->hash,
                           (gpointer)get_name(g_sequence_get(iter)), iter);
    }
}

/**
 * invent_item_name:
 * @inventory: An inventory.
 * @prefix: Name prefix.
 *
 * Finds a name of form "prefix number" that does not identify any item in
 * an inventory yet.
 *
 * Returns: The invented name as a string that is owned by this function and
 *          valid only until next call to it.
 **/
static const gchar*
invent_item_name(Inventory *inventory,
                 const gchar *prefix)
{
    static GString *str = NULL;
    const gchar *p, *last;
    gint n, i;

    if (!str)
        str = g_string_new("");

    g_string_assign(str, prefix ? prefix : _("Untitled"));
    if (!lookup_item(inventory, str->str))
        return str->str;

    last = str->str + MAX(str->len-1, 0);
    for (p = last; p >= str->str; p--) {
        if (!g_ascii_isdigit(*p))
            break;
    }
    if (p == last || (p >= str->str && !g_ascii_isspace(*p)))
        p = last;
    while (p >= str->str && g_ascii_isspace(*p))
        p--;
    g_string_truncate(str, p+1 - str->str);

    g_string_append_c(str, ' ');
    n = str->len;
    for (i = 1; i < 10000; i++) {
        g_string_append_printf(str, "%d", i);
        if (!lookup_item(inventory, str->str))
            return str->str;

        g_string_truncate(str, n);
    }
    g_assert_not_reached();
    return NULL;
}


/**
 * SECTION: inventory
 * @title: GwyInventory
 * @short_description: Ordered item inventory, indexed by both name and
 *                     position
 * @see_also: #GwyContainer, #GwyInventoryModel
 *
 * #GwyInventory is a uniform container that offers both hash table and array
 * (sorted or unsorted) interfaces.  Both types of access are fast.  Inventory
 * can also maintain a notion of default item used as a fallback or default
 * in certain cases.
 *
 * Possible operations with data items stored in an inventory are specified
 * upon inventory creation with #GwyInventoryItemType structure.  Not all
 * fields are mandatory, with items allowing more operations the inventory is
 * more capable too.  For example, if items offer a method to make copies,
 * gwy_inventory_copy() can be used to directly create new items in the
 * inventory (this capability can be tested with
 * gwy_inventory_can_make_copies()).
 *
 * #GwyInventory is also designed to be used as storage backend for
 * #GtkTreeModel<!-- -->s.  Upon modification, it emits signals that directly
 * map onto #GtkTreeModel signals.
 *
 * Item can have `traits', that is data that can be obtained generically. They
 * are similar to #GObject properties.  Actually, if items are objects, they
 * should simply map object properties to traits.  But it is possible to define
 * traits for simple structures too.
 **/

/**
 * GwyInventory:
 *
 * Object representing an item inventory.
 *
 * The #GwyInventory struct contains private data only and should be accessed
 * using the functions below.
 **/

/**
 * GwyInventoryClass:
 *
 * Class of item inventories.
 **/

/**
 * GwyInventoryItemType:
 * @type: Object type, if item is object or other type with registered GType.
 *        May be zero to indicate an unregistered item type.
 *        If items are objects inventory takes a reference on them.  Therefore
 *        it is usually unnecessary to set g_object_unref() as
 *        #GwyInventoryItemType.destroy().
 * @watchable_signal: Item signal name to watch, used only for objects.
 *                    When item emits this signal, inventory emits
 *                    "item-updated" signal for it.
 *                    May be %NULL to indicate no signal should be watched.
 *                    you can still emit "item-updated" manually with
 *                    gwy_inventory_updated() or
 *                    gwy_inventory_nth_updated().
 * @get_name: Returns item name (the string is owned by item and it is assumed
 *            to exist until item ceases to exist or is renamed).  This
 *            function is obligatory.
 * @is_modifiable: If not %NULL and returns %FALSE for some item, such an item
 *                 cannot be removed from inventory and it cannot be renamed.
 *                 Non-modifiable items can be freely added though.
 *                 Modifiability is checked on each attempt to change the item.
 *                 If this method is unimplemented, items are considered
 *                 modifiable.
 * @compare: Item comparation function for sorting.
 *           If %NULL, inventory never attempts to keep any item order
 *           and gwy_inventory_restore_order() does nothing.
 *           Otherwise inventory is sorted unless sorting is (temporarily)
 *           disabled with gwy_inventory_forget_order() or it was created
 *           with gwy_inventory_new_with_items() and the initial array was
 *           not sorted according to @compare.
 * @rename: Function to rename an item.  If not %NULL, it is possible to use
 *          gwy_inventory_rename().  Note items must not be renamed by any
 *          other means than this method, because when an item is renamed and
 *          the inventory does not know it, very bad things will happen and you
 *          will lose all your good karma.  Also,
 *          #GwyInventoryItemType.get_name() must return the new name after
 *          renaming.
 * @destroy: Destructor/clean-up function called on item before it is removed
 *           from inventory.  May be %NULL.
 * @copy: Function to create the copy of an item.  If this function and @rename
 *        are defined it is possible to use gwy_inventory_copy().
 *        Inventory sets the copy's name immediately after creation, so it
 *        normally does not matter which name @copy gives it.
 * @get_traits: Function to get item traits.  It returns array of item trait
 *              #GTypes (keeping its ownership) and if @nitems is not %NULL,
 *              it stores the length of returned array there.
 * @get_trait_name: Returns name of @i-th trait (keeping ownership of the
 *                  returned string).  It is not obligatory, but advisable to
 *                  give traits names.
 * @get_trait_value: Sets @value to value of @i-th trait of item.
 *
 * Infromation about a #GwyInventory item type.
 *
 * Note only one of the fields must be always defined: @get_name.  All the
 * others give inventory (and thus inventory users) some additional powers over
 * items.  They may be set to %NULL or 0 if particular item type does not
 * (want to) support this operation.
 *
 * The three trait methods are not used by #GwyInventory itself, but allows
 * #GwyInventoryStore to generically map item properties to virtual columns
 * of a #GtkTreeModel.  If items are objects, you will usually want to
 * directly map some or all #GObject properties to item traits.  If they are
 * plain C structs or something else, you can easily export their data members
 * as virtual #GtkTreeModel columns by defining traits for them.
 **/

/**
 * GwyInventoryForeachFunc:
 * @n: Position of @item.
 * @item: Item value.
 * @user_data: Data passed to gwy_inventory_foreach().
 *
 * Type of function passed to gwy_inventory_foreach().
 *
 * The function must not modify the inventory.
 **/

/**
 * GwyInventoryFindFunc:
 * @n: Position of @item.
 * @item: Item value.
 * @user_data: Data passed to gwy_inventory_find().
 *
 * Type of function passed to gwy_inventory_find().
 *
 * The function must not modify the inventory.
 *
 * Returns: %TRUE to stop searching; the current item is then returned as
 *          found.  %FALSE to continue.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
