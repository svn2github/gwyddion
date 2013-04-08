/*
 *  $Id$
 *  Copyright (C) 2013 David Nečas (Yeti).
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

#include "libgwy/macros.h"
#include "libgwy/listable.h"

enum {
    SGNL_ITEM_INSERTED,
    SGNL_ITEM_DELETED,
    SGNL_ITEM_UPDATED,
    SGNL_ITEMS_REORDERED,
    N_SIGNALS
};

G_DEFINE_INTERFACE(GwyListable, gwy_listable, G_TYPE_OBJECT);

static guint signals[N_SIGNALS];

static void
gwy_listable_default_init(G_GNUC_UNUSED GwyListableInterface *iface)
{
    GType type = G_TYPE_FROM_INTERFACE(iface);

    /**
     * GwyListable::item-inserted:
     * @gwyarray: The #GwyListable which received the signal.
     * @arg1: Position an item was inserted at.
     *
     * The ::item-inserted signal is emitted when an item is inserted into
     * the listable.
     **/
    signals[SGNL_ITEM_INSERTED]
        = g_signal_new_class_handler("item-inserted", type,
                                     G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE,
                                     NULL, NULL, NULL,
                                     g_cclosure_marshal_VOID__UINT,
                                     G_TYPE_NONE, 1, G_TYPE_UINT);

    /**
     * GwyListable::item-deleted:
     * @gwyarray: The #GwyListable which received the signal.
     * @arg1: Position an item was deleted from.
     *
     * The ::item-deleted signal is emitted when an item is deleted from
     * the listable.
     **/
    signals[SGNL_ITEM_DELETED]
        = g_signal_new_class_handler("item-deleted", type,
                                     G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE,
                                     NULL, NULL, NULL,
                                     g_cclosure_marshal_VOID__UINT,
                                     G_TYPE_NONE, 1, G_TYPE_UINT);

    /**
     * GwyListable::item-updated:
     * @gwyarray: The #GwyListable which received the signal.
     * @arg1: Position of updated item.
     *
     * The ::item-updated signal is emitted when an item in the listable
     * is updated.
     **/
    signals[SGNL_ITEM_UPDATED]
        = g_signal_new_class_handler("item-updated", type,
                                     G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE,
                                     NULL, NULL, NULL,
                                     g_cclosure_marshal_VOID__UINT,
                                     G_TYPE_NONE, 1, G_TYPE_UINT);

  /**
   * GwyListable::items-reordered:
   * @gwyarray: The #GwyListable which received the signal.
   * @arg1: New item order map as in #GtkTreeModel,
   *        @arg1[new_position] = old_position.
   *
   * The ::items-reordered signal is emitted when item in the listable
   * are reordered.
   **/
  signals[SGNL_ITEMS_REORDERED]
      = g_signal_new_class_handler("items-reordered", type,
                                   G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE,
                                   NULL, NULL, NULL,
                                   g_cclosure_marshal_VOID__POINTER,
                                   G_TYPE_NONE, 1, G_TYPE_POINTER);
}

/**
 * gwy_listable_size:
 * @listable: A listable.
 *
 * Obtains the number of items in a listable.
 *
 * Returns: The number of items.
 **/
guint
gwy_listable_size(const GwyListable *listable)
{
    GwyListableInterface *iface = GWY_LISTABLE_GET_INTERFACE(listable);
    g_return_val_if_fail(iface && iface->size, 0);
    return iface->size(listable);
}

/**
 * gwy_listable_get:
 * @listable: A listable.
 * @pos: Item position.
 *
 * Gets the item at given position in a listable.
 *
 * Returns: The item at position @pos.
 **/
gpointer
gwy_listable_get(const GwyListable *listable,
                 guint pos)
{
    GwyListableInterface *iface = GWY_LISTABLE_GET_INTERFACE(listable);
    g_return_val_if_fail(iface && iface->get, NULL);
    return iface->get(listable, pos);
}

/**
 * gwy_listable_item_inserted:
 * @listable: A listable.
 * @pos: Position at which a new item was inserted.
 *
 * Emits signal GwyListable::item-inserted on a listable.
 **/
void
gwy_listable_item_inserted(GwyListable *listable,
                           guint pos)
{
    g_signal_emit(listable, signals[SGNL_ITEM_INSERTED], 0, pos);
}

/**
 * gwy_listable_item_deleted:
 * @listable: A listable.
 * @pos: Position at which an item was deleted.
 *
 * Emits signal GwyListable::item-deleted on a listable.
 **/
void
gwy_listable_item_deleted(GwyListable *listable,
                          guint pos)
{
    g_signal_emit(listable, signals[SGNL_ITEM_DELETED], 0, pos);
}

/**
 * gwy_listable_item_updated:
 * @listable: A listable.
 * @pos: Position at which an item was updated.
 *
 * Emits signal GwyListable::item-updated on a listable.
 **/
void
gwy_listable_item_updated(GwyListable *listable,
                          guint pos)
{
    g_signal_emit(listable, signals[SGNL_ITEM_UPDATED], 0, pos);
}

/**
 * gwy_listable_items_reordered:
 * @listable: A listable.
 * @new_order: New item order map as in #GtkTreeModel,
 *             @new_order[new_position] = old_position.
 *
 * Emits signal GwyListable::items-reordered on a listable.
 **/
void
gwy_listable_items_reordered(GwyListable *listable,
                             const guint *new_order)
{
    g_signal_emit(listable, signals[SGNL_ITEMS_REORDERED], 0, new_order);
}

/**
 * SECTION: listable
 * @title: GwyListable
 * @short_description: Enumerable list-like object interface
 *
 * #GwyListable is an abstract interface for container objects with items that
 * are enumerable and indexable by integers.  By implementing this interface
 * they can be easily turned into Gtk+ tree models.
 *
 * Container items are represented by pointers and they are fully opaque to
 * #GwyListable but they are assumed to be unique, i.e. an item should be
 * present at most once in a single listable.
 **/

/**
 * GwyListable:
 *
 * Formal type of listable objects.
 **/

/**
 * GWY_IMPLEMENT_LISTABLE:
 * @interface_init: The interface init function.
 *
 * Declares that a type implements #GwyListable.
 *
 * This is a specialization of G_IMPLEMENT_INTERFACE() for
 * #GwyListableInterface.  It is intended to be used in last
 * G_DEFINE_TYPE_EXTENDED() argument:
 * |[
 * G_DEFINE_TYPE_EXTENDED
 *     (GwyFoo, gwy_foo, G_TYPE_OBJECT, 0,
 *      GWY_IMPLEMENT_LISTABLE(gwy_foo_listable_init));
 * ]|
 **/

/**
 * GwyListableInterface:
 * @size: Obtains the number of items in the object.  It is assumed not to
 *        change unless a signal indicating insertion/deletion of an item is
 *        emitted and users of #GwyListable may cache the value.
 * @get: Gets the item at given position.
 * @find: Finds the position of an item by the item pointer.  This method need
 *        not be provided since it has a default implementation which just
 *        iterates through the items.
 *
 * Interface implemented by listable objects.
 *
 * The object class must implement all the methods, except
 * #GwyListableInterface.find() which is optional.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
