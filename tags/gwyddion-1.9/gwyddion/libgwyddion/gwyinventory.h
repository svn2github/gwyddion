/*
 *  @(#) $Id$
 *  Copyright (C) 2003,2004 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 */

#ifndef __GWY_INVENTORY_H__
#define __GWY_INVENTORY_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define GWY_TYPE_INVENTORY                  (gwy_inventory_get_type ())
#define GWY_INVENTORY(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GWY_TYPE_INVENTORY, GwyInventory))
#define GWY_INVENTORY_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), GWY_TYPE_INVENTORY, GwyInventoryClass))
#define GWY_IS_INVENTORY(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GWY_TYPE_INVENTORY))
#define GWY_IS_INVENTORY_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), GWY_TYPE_INVENTORY))
#define GWY_INVENTORY_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), GWY_TYPE_INVENTORY, GwyInventoryClass))

/* Rationale:
 * 1. Prescribing access methods is better than prescribing data members,
 *    because it introduces one level indirection, but allows for example to
 *    get_name, get_text and get_data return the same string.
 * 2. All deep-derivable types in GLib type system are not useful (objects),
 *    and we would need deep derivation some GLib type <- Abstract item
 *    <- Particular items.  So it seems use of the type system would be too
 *    much pain, too little gain.  The basic trouble is we want to derive
 *    interfaces, but don't want instantiable types.
 */
typedef struct _GwyItemType GwyItemType;

struct _GwyItemType {
    GType         type;
    const gchar* (*get_name) (gconstpointer item);
    const gchar* (*get_text) (gconstpointer item);
    /* this is going to be a GdkPixbuf down the road, but we can't declare
     * it here; XXX: may move to GwyResource -- anything containing a Pixbuf
     * can be object itself */
    gpointer     (*get_image)(gconstpointer item);
    gint         (*compare)  (gconstpointer item1,
                              gconstpointer item2);
    void         (*rename)   (gconstpointer item,
                              const gchar *newname);
};

typedef struct _GwyInventory GwyInventory;
typedef struct _GwyInventoryClass GwyInventoryClass;

struct _GwyInventory {
    GObject parent_instance;

    GwyItemType item_type;
    gboolean needs_reindex : 1;
    gboolean is_sorted : 1;
    gboolean is_object : 1;
    gboolean is_watchable : 1;
    gboolean can_make_copies : 1;
    gboolean has_default : 1;

    GString *default_key;

    GArray *items;
    GArray *idx;
    GHashTable *hash;
};

struct _GwyInventoryClass {
    GObjectClass parent_class;

    void (*item_inserted)(GwyInventory *inventory,
                          guint position);
    void (*item_deleted)(GwyInventory *inventory,
                         guint position);
    void (*item_updated)(GwyInventory *inventory,
                         guint position);
    void (*items_reordered)(GwyInventory *inventory,
                            const gint *new_order);
};

GType         gwy_inventory_get_type             (void) G_GNUC_CONST;
GwyInventory* gwy_inventory_new                  (const GwyItemType *item_type);
GwyInventory* gwy_inventory_new_filled           (const GwyItemType *item_type,
                                                  guint nitems,
                                                  gpointer *items);

/* Information */
guint         gwy_inventory_get_n_items          (GwyInventory *inventory);
gboolean      gwy_inventory_can_make_copies      (GwyInventory *inventory);
const GwyItemType* gwy_inventory_get_item_type   (GwyInventory *inventory);

/* Retrieving items */
gboolean      gwy_inventory_item_exists          (GwyInventory *inventory,
                                                  const gchar *name);
gpointer      gwy_inventory_get_item             (GwyInventory *inventory,
                                                  const gchar *name);
gpointer      gwy_inventory_get_item_or_default  (GwyInventory *inventory,
                                                  const gchar *name);
gpointer      gwy_inventory_get_nth_item         (GwyInventory *inventory,
                                                  guint n);
guint         gwy_inventory_get_item_position    (GwyInventory *inventory,
                                                  const gchar *name);

void          gwy_inventory_foreach              (GwyInventory *inventory,
                                                  GHFunc function,
                                                  gpointer user_data);
void          gwy_inventory_set_default_item     (GwyInventory *inventory,
                                                  const gchar *name);
gpointer      gwy_inventory_get_default_item     (GwyInventory *inventory);

/* Modifiable inventories */
void          gwy_inventory_item_updated         (GwyInventory *inventory,
                                                  const gchar *name);
void          gwy_inventory_nth_item_updated     (GwyInventory *inventory,
                                                  guint n);
void          gwy_inventory_restore_order        (GwyInventory *inventory);
void          gwy_inventory_forget_order         (GwyInventory *inventory);
gpointer      gwy_inventory_insert_item          (GwyInventory *inventory,
                                                  gpointer item);
gpointer      gwy_inventory_insert_nth_item      (GwyInventory *inventory,
                                                  gpointer item,
                                                  guint n);
gboolean      gwy_inventory_delete_item          (GwyInventory *inventory,
                                                  const gchar *name);
gboolean      gwy_inventory_delete_nth_item      (GwyInventory *inventory,
                                                  guint n);
gpointer      gwy_inventory_rename_item          (GwyInventory *inventory,
                                                  const gchar *name,
                                                  const gchar *newname);

/* Serializable object inventories */
gpointer      gwy_inventory_new_item             (GwyInventory *inventory,
                                                  const gchar *newname);
gpointer      gwy_inventory_new_item_as_copy     (GwyInventory *inventory,
                                                  const gchar *newname);
/* FIXME: more stuff could be here: loading and saving from directories,
 * setting default fallback item, and whatnot */

G_END_DECLS

#endif /* __GWY_INVENTORY_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

