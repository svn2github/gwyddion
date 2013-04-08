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

#ifndef __LIBGWYUI_LIST_STORE_H__
#define __LIBGWYUI_LIST_STORE_H__

#include <libgwy/listable.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GWY_TYPE_LIST_STORE \
    (gwy_list_store_get_type())
#define GWY_LIST_STORE(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_LIST_STORE, GwyListStore))
#define GWY_LIST_STORE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_LIST_STORE, GwyListStoreClass))
#define GWY_IS_LIST_STORE(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_LIST_STORE))
#define GWY_IS_LIST_STORE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_LIST_STORE))
#define GWY_LIST_STORE_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_LIST_STORE, GwyListStoreClass))

typedef struct _GwyListStore      GwyListStore;
typedef struct _GwyListStoreClass GwyListStoreClass;

struct _GwyListStore {
    GInitiallyUnowned unowned;
    struct _GwyListStorePrivate *priv;
};

struct _GwyListStoreClass {
    /*<private>*/
    GInitiallyUnownedClass unowned_class;
    void (*reserved1)(void);
    void (*reserved2)(void);
    /*<public>*/
};

GType         gwy_list_store_get_type     (void)                      G_GNUC_CONST;
GwyListStore* gwy_list_store_new          (GwyListable *backend)      G_GNUC_MALLOC;
GwyListable*  gwy_list_store_get_backend  (const GwyListStore *store) G_GNUC_PURE;
gboolean      gwy_list_store_get_iter     (const GwyListStore *store,
                                           const guint i,
                                           GtkTreeIter *iter);
gboolean      gwy_list_store_iter_is_valid(const GwyListStore *store,
                                           GtkTreeIter *iter);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
