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

#ifdef __LIBGWY_ALIASES_H__
#error Public headers must be included before the aliasing header.
#endif

#ifndef __LIBGWY_INVENTORY_H__
#define __LIBGWY_INVENTORY_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define GWY_TYPE_INVENTORY \
    (gwy_inventory_get_type())
#define GWY_INVENTORY(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_INVENTORY, GwyInventory))
#define GWY_INVENTORY_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_INVENTORY, GwyInventoryClass))
#define GWY_IS_INVENTORY(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_INVENTORY))
#define GWY_IS_INVENTORY_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_INVENTORY))
#define GWY_INVENTORY_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_INVENTORY, GwyInventoryClass))

/*
 * Rationale:
 * 1. Prescribing access methods is better than prescribing data members,
 *    because it introduces one level indirection, but allows for example to
 *    get_name, get_text and get_data return the same string.
 * 2. All deep-derivable types in GLib type system are not useful (objects),
 *    and we would need deep derivation some GLib type <- Abstract item
 *    <- Particular items.  So it seems use of the type system would be too
 *    much pain, too little gain.  The basic trouble is we want to derive
 *    interfaces, but don't want instantiable types.
 */
typedef struct _GwyInventoryItemType GwyInventoryItemType;

struct _GwyInventoryItemType {
    GType         type;
    const gchar  *watchable_signal;
    const gchar* (*get_name)       (gconstpointer item);
    gboolean     (*is_modifiable)  (gconstpointer item);
    GCompareFunc compare;
    void         (*rename)         (gpointer item,
                                    const gchar *newname);
    GDestroyNotify destroy;
    gpointer     (*copy)           (gconstpointer item);
    const GType* (*get_traits)     (guint *ntraits);
    const gchar* (*get_trait_name) (guint i);
    void         (*get_trait_value)(gconstpointer item,
                                    guint i,
                                    GValue *value);
};

typedef struct _GwyInventory GwyInventory;
typedef struct _GwyInventoryClass GwyInventoryClass;

struct _GwyInventory {
    GObject g_object;
    struct _GwyInventoryPrivate *priv;
};

struct _GwyInventoryClass {
    /*<private>*/
    GObjectClass g_object_class;
};

typedef void (*GwyInventoryForeachFunc)(guint n,
                                        gpointer item,
                                        gpointer user_data);
typedef gboolean (*GwyInventoryFindFunc)(guint n,
                                         gpointer item,
                                         gpointer user_data);

/* FIXME: GSequence can change internally when an item is looked up.
 * This should not be observable from outside though. */
GType                       gwy_inventory_get_type        (void)                               G_GNUC_CONST;
GwyInventory*               gwy_inventory_new             (void)                               G_GNUC_MALLOC;
GwyInventory*               gwy_inventory_new_with_type   (const GwyInventoryItemType *itype)  G_GNUC_MALLOC;
GwyInventory*               gwy_inventory_new_with_items  (const GwyInventoryItemType *itype,
                                                           guint nitems,
                                                           gpointer pitems)                    G_GNUC_MALLOC;
void                        gwy_inventory_set_item_type   (GwyInventory *inventory,
                                                           const GwyInventoryItemType *itype);
guint                       gwy_inventory_n_items         (GwyInventory *inventory)            G_GNUC_PURE;
const GwyInventoryItemType* gwy_inventory_get_item_type   (GwyInventory *inventory)            G_GNUC_PURE;
gboolean                    gwy_inventory_can_make_copies (GwyInventory *inventory)            G_GNUC_PURE;
gpointer                    gwy_inventory_get             (GwyInventory *inventory,
                                                           const gchar *name)                  G_GNUC_PURE;
gpointer                    gwy_inventory_get_or_default  (GwyInventory *inventory,
                                                           const gchar *name)                  G_GNUC_PURE;
gpointer                    gwy_inventory_get_nth         (GwyInventory *inventory,
                                                           guint n)                            G_GNUC_PURE;
guint                       gwy_inventory_position        (GwyInventory *inventory,
                                                           const gchar *name)                  G_GNUC_PURE;
void                        gwy_inventory_foreach         (GwyInventory *inventory,
                                                           GwyInventoryForeachFunc function,
                                                           gpointer user_data);
gpointer                    gwy_inventory_find            (GwyInventory *inventory,
                                                           GwyInventoryFindFunc predicate,
                                                           gpointer user_data);
void                        gwy_inventory_set_default_name(GwyInventory *inventory,
                                                           const gchar *name);
const gchar*                gwy_inventory_get_default_name(GwyInventory *inventory)            G_GNUC_PURE;
gpointer                    gwy_inventory_get_default     (GwyInventory *inventory)            G_GNUC_PURE;
void                        gwy_inventory_updated         (GwyInventory *inventory,
                                                           const gchar *name);
void                        gwy_inventory_nth_updated     (GwyInventory *inventory,
                                                           guint n);
void                        gwy_inventory_restore_order   (GwyInventory *inventory);
void                        gwy_inventory_forget_order    (GwyInventory *inventory);
gpointer                    gwy_inventory_insert          (GwyInventory *inventory,
                                                           gpointer item);
gpointer                    gwy_inventory_insert_nth      (GwyInventory *inventory,
                                                           gpointer item,
                                                           guint n);
void                        gwy_inventory_delete          (GwyInventory *inventory,
                                                           const gchar *name);
void                        gwy_inventory_delete_nth      (GwyInventory *inventory,
                                                           guint n);
gpointer                    gwy_inventory_rename          (GwyInventory *inventory,
                                                           const gchar *name,
                                                           const gchar *newname);
gpointer                    gwy_inventory_copy            (GwyInventory *inventory,
                                                           const gchar *name,
                                                           const gchar *newname);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
