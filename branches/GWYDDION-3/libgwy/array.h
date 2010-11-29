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

#ifndef __LIBGWY_ARRAY_H__
#define __LIBGWY_ARRAY_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define GWY_TYPE_ARRAY \
    (gwy_array_get_type())
#define GWY_ARRAY(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_ARRAY, GwyArray))
#define GWY_ARRAY_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_ARRAY, GwyArrayClass))
#define GWY_IS_ARRAY(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_ARRAY))
#define GWY_IS_ARRAY_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_ARRAY))
#define GWY_ARRAY_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_ARRAY, GwyArrayClass))

typedef struct _GwyArray GwyArray;
typedef struct _GwyArrayClass GwyArrayClass;

struct _GwyArray {
    GObject g_object;
    struct _GwyArrayPrivate *priv;
};

struct _GwyArrayClass {
    /*<private>*/
    GObjectClass g_object_class;
};

GType     gwy_array_get_type     (void)                    G_GNUC_CONST;
GwyArray* gwy_array_new          (void)                    G_GNUC_MALLOC;
GwyArray* gwy_array_new_with_data(gsize size,
                                  GDestroyNotify destroy,
                                  gconstpointer items,
                                  guint nitems)            G_GNUC_MALLOC;
void      gwy_array_set_item_type(GwyArray *array,
                                  gsize size,
                                  GDestroyNotify destroy);
guint     gwy_array_size         (GwyArray *array)         G_GNUC_PURE;
gpointer  gwy_array_get          (GwyArray *array,
                                  guint n)                 G_GNUC_PURE;

#define gwy_array_insert1(array,n,item) \
    gwy_array_insert((array),(n),(item),1)
#define gwy_array_append1(array,item) \
    gwy_array_append((array),(item),1)
#define gwy_array_delete1(array,n) \
    gwy_array_delete((array),(n),1)
#define gwy_array_replace1(array,n,item) \
    gwy_array_replace((array),(n),(item),1)

gpointer gwy_array_insert  (GwyArray *array,
                            guint n,
                            gconstpointer items,
                            guint nitems);
gpointer gwy_array_append  (GwyArray *array,
                            gconstpointer items,
                            guint nitems);
void     gwy_array_delete  (GwyArray *array,
                            guint n,
                            guint nitems);
void     gwy_array_replace (GwyArray *array,
                            guint n,
                            gconstpointer items,
                            guint nitems);
void     gwy_array_updated (GwyArray *array,
                            guint n);
gpointer gwy_array_get_data(GwyArray *array) G_GNUC_PURE;
void     gwy_array_set_data(GwyArray *array,
                            gconstpointer items,
                            guint nitems);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
