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

#ifndef __LIBGWYUI_RESOURCE_LIST_H__
#define __LIBGWYUI_RESOURCE_LIST_H__

#include <libgwy/resource.h>
#include <libgwyui/inventory-store.h>

G_BEGIN_DECLS

#define GWY_TYPE_RESOURCE_LIST \
    (gwy_resource_list_get_type())
#define GWY_RESOURCE_LIST(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_RESOURCE_LIST, GwyResourceList))
#define GWY_RESOURCE_LIST_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_RESOURCE_LIST, GwyResourceListClass))
#define GWY_IS_RESOURCE_LIST(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_RESOURCE_LIST))
#define GWY_IS_RESOURCE_LIST_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_RESOURCE_LIST))
#define GWY_RESOURCE_LIST_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_RESOURCE_LIST, GwyResourceListClass))

typedef struct _GwyResourceList      GwyResourceList;
typedef struct _GwyResourceListClass GwyResourceListClass;

struct _GwyResourceList {
    GtkTreeView tree_view;
    struct _GwyResourceListPrivate *priv;
};

struct _GwyResourceListClass {
    /*<private>*/
    GtkTreeViewClass tree_view_class;
    void (*reserved1)(void);
    void (*reserved2)(void);
};

GType              gwy_resource_list_get_type          (void)                        G_GNUC_CONST;
GtkWidget*         gwy_resource_list_new               (GType resource_type)         G_GNUC_MALLOC;
GwyInventoryStore* gwy_resource_list_get_store         (const GwyResourceList *list) G_GNUC_PURE;
gboolean           gwy_resource_list_set_active        (GwyResourceList *list,
                                                        const gchar *name);
const gchar*       gwy_resource_list_get_active        (const GwyResourceList *list) G_GNUC_PURE;
void               gwy_resource_list_set_only_preferred(GwyResourceList *list,
                                                        gboolean onlypreferred);
gboolean           gwy_resource_list_get_only_preferred(GwyResourceList *list)       G_GNUC_PURE;

GtkTreeViewColumn* gwy_resource_list_create_column_name     (GwyResourceList *list) G_GNUC_MALLOC;
GtkTreeViewColumn* gwy_resource_list_create_column_preferred(GwyResourceList *list) G_GNUC_MALLOC;
GtkTreeViewColumn* gwy_resource_list_create_column_gradient (GwyResourceList *list) G_GNUC_MALLOC;

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

