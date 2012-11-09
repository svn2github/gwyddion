/*
 *  $Id$
 *  Copyright (C) 2012 David Nečas (Yeti).
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

#ifndef __LIBGWYUI_INVENTORY_STORE_H__
#define __LIBGWYUI_INVENTORY_STORE_H__

#include <gtk/gtk.h>
#include <libgwy/inventory.h>

G_BEGIN_DECLS

#define GWY_TYPE_INVENTORY_STORE \
    (gwy_inventory_store_get_type())
#define GWY_INVENTORY_STORE(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_INVENTORY_STORE, GwyInventoryStore))
#define GWY_INVENTORY_STORE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_INVENTORY_STORE, GwyInventoryStoreClass))
#define GWY_IS_INVENTORY_STORE(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_INVENTORY_STORE))
#define GWY_IS_INVENTORY_STORE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_INVENTORY_STORE))
#define GWY_INVENTORY_STORE_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_INVENTORY_STORE, GwyInventoryStoreClass))

typedef struct _GwyInventoryStore      GwyInventoryStore;
typedef struct _GwyInventoryStoreClass GwyInventoryStoreClass;

struct _GwyInventoryStore {
    GInitiallyUnowned unowned;
    struct _GwyInventoryStorePrivate *priv;
};

struct _GwyInventoryStoreClass {
    /*<private>*/
    GInitiallyUnownedClass unowned_class;
    void (*reserved1)(void);
    void (*reserved2)(void);
    /*<public>*/
};

GType              gwy_inventory_store_get_type     (void)                           G_GNUC_CONST;
GwyInventoryStore* gwy_inventory_store_new          (GwyInventory *inventory);
GwyInventory*      gwy_inventory_store_get_inventory(const GwyInventoryStore *store) G_GNUC_PURE;
gint               gwy_inventory_store_find_column  (const GwyInventoryStore *store,
                                                     const gchar *name)              G_GNUC_PURE;
gboolean           gwy_inventory_store_get_iter     (const GwyInventoryStore *store,
                                                     const gchar *name,
                                                     GtkTreeIter *iter);
gboolean           gwy_inventory_store_iter_is_valid(const GwyInventoryStore *store,
                                                     GtkTreeIter *iter);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
