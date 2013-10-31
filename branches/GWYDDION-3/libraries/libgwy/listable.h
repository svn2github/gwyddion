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

#ifndef __LIBGWY_LISTABLE_H__
#define __LIBGWY_LISTABLE_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define GWY_TYPE_LISTABLE \
    (gwy_listable_get_type())
#define GWY_LISTABLE(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_LISTABLE, GwyListable))
#define GWY_IS_LISTABLE(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_LISTABLE))
#define GWY_LISTABLE_GET_INTERFACE(obj) \
    (G_TYPE_INSTANCE_GET_INTERFACE((obj), GWY_TYPE_LISTABLE, GwyListableInterface))

#define GWY_IMPLEMENT_LISTABLE(interface_init) \
    { \
        static const GInterfaceInfo gwy_listable_interface_info = { \
            (GInterfaceInitFunc)interface_init, NULL, NULL \
        }; \
        g_type_add_interface_static(g_define_type_id, \
                                    GWY_TYPE_LISTABLE, \
                                    &gwy_listable_interface_info); \
    }

typedef struct _GwyListableInterface GwyListableInterface;
typedef struct _GwyListable          GwyListable;           // dummy

struct _GwyListableInterface {
    /*<private>*/
    GTypeInterface g_interface;

    /* virtual table */
    /*<public>*/
    guint    (*size)(const GwyListable *listable);
    gpointer (*get) (const GwyListable *listable,
                     guint pos);
};

GType    gwy_listable_get_type       (void)                        G_GNUC_CONST;
guint    gwy_listable_size           (const GwyListable *listable) G_GNUC_PURE;
gpointer gwy_listable_get            (const GwyListable *listable,
                                      guint pos)                   G_GNUC_PURE;
void     gwy_listable_item_inserted  (GwyListable *listable,
                                      guint pos);
void     gwy_listable_item_deleted   (GwyListable *listable,
                                      guint pos);
void     gwy_listable_item_updated   (GwyListable *listable,
                                      guint pos);
void     gwy_listable_items_reordered(GwyListable *listable,
                                      const guint *new_order);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
