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

#ifndef __LIBGWYUI_LIST_SELECTION_BINDING_H__
#define __LIBGWYUI_LIST_SELECTION_BINDING_H__

#include <gtk/gtk.h>
#include <libgwy/int-set.h>

G_BEGIN_DECLS

#define GWY_TYPE_LIST_SELECTION_BINDING \
    (gwy_list_selection_binding_get_type())
#define GWY_LIST_SELECTION_BINDING(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_LIST_SELECTION_BINDING, GwyListSelectionBinding))
#define GWY_LIST_SELECTION_BINDING_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_LIST_SELECTION_BINDING, GwyListSelectionBindingClass))
#define GWY_IS_LIST_SELECTION_BINDING(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_LIST_SELECTION_BINDING))
#define GWY_IS_LIST_SELECTION_BINDING_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_LIST_SELECTION_BINDING))
#define GWY_LIST_SELECTION_BINDING_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_LIST_SELECTION_BINDING, GwyListSelectionBindingClass))

typedef struct _GwyListSelectionBinding      GwyListSelectionBinding;
typedef struct _GwyListSelectionBindingClass GwyListSelectionBindingClass;

struct _GwyListSelectionBinding {
    GObject g_object;
    struct _GwyListSelectionBindingPrivate *priv;
};

struct _GwyListSelectionBindingClass {
    /*<private>*/
    GObjectClass g_object_class;
};

GType                    gwy_list_selection_binding_get_type(void)              G_GNUC_CONST;
GwyListSelectionBinding* gwy_list_selection_binding_new     (GwyIntSet *intset) G_GNUC_MALLOC;
void                     gwy_list_selection_binding_add     (GwyListSelectionBinding *binding,
                                                             GtkTreeSelection *selection);
void                     gwy_list_selection_binding_remove  (GwyListSelectionBinding *binding,
                                                             GtkTreeSelection *selection);
void                     gwy_list_selection_binding_unbind  (GwyListSelectionBinding *binding);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
