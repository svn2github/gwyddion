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

#ifndef __LIBGWYUI_ARRAY_STORE_H__
#define __LIBGWYUI_ARRAY_STORE_H__

#include <gtk/gtk.h>
#include <libgwy/array.h>

G_BEGIN_DECLS

#define GWY_TYPE_ARRAY_STORE \
    (gwy_array_store_get_type())
#define GWY_ARRAY_STORE(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_ARRAY_STORE, GwyArrayStore))
#define GWY_ARRAY_STORE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_ARRAY_STORE, GwyArrayStoreClass))
#define GWY_IS_ARRAY_STORE(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_ARRAY_STORE))
#define GWY_IS_ARRAY_STORE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_ARRAY_STORE))
#define GWY_ARRAY_STORE_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_ARRAY_STORE, GwyArrayStoreClass))

typedef struct _GwyArrayStore      GwyArrayStore;
typedef struct _GwyArrayStoreClass GwyArrayStoreClass;

struct _GwyArrayStore {
    GInitiallyUnowned unowned;
    struct _GwyArrayStorePrivate *priv;
};

struct _GwyArrayStoreClass {
    /*<private>*/
    GInitiallyUnownedClass unowned_class;
    void (*reserved1)(void);
    void (*reserved2)(void);
    /*<public>*/
};

GType          gwy_array_store_get_type     (void)                       G_GNUC_CONST;
GwyArrayStore* gwy_array_store_new          (GwyArray *array);
GwyArray*      gwy_array_store_get_array    (const GwyArrayStore *store) G_GNUC_PURE;
gboolean       gwy_array_store_get_iter     (const GwyArrayStore *store,
                                             const guint i,
                                             GtkTreeIter *iter);
gboolean       gwy_array_store_iter_is_valid(const GwyArrayStore *store,
                                             GtkTreeIter *iter);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
