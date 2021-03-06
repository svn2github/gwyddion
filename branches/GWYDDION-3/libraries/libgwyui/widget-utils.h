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

#ifndef __LIBGWYUI_WIDGET_UTILS_H__
#define __LIBGWYUI_WIDGET_UTILS_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GWY_IMPLEMENT_TREE_MODEL(interface_init) \
    { \
        static const GInterfaceInfo gwy_tree_model_interface_info = { \
            (GInterfaceInitFunc)interface_init, NULL, NULL \
        }; \
        g_type_add_interface_static(g_define_type_id, \
                                    GTK_TYPE_TREE_MODEL, \
                                    &gwy_tree_model_interface_info); \
    }

gdouble    gwy_scroll_wheel_delta                (GtkAdjustment *adjustment,
                                                  GdkEventScroll *event,
                                                  GtkOrientation orientation);
void       gwy_list_store_row_changed            (GtkListStore *store,
                                                  GtkTreeIter *iter,
                                                  GtkTreePath *path,
                                                  gint row);
gboolean   gwy_widget_get_activate_on_unfocus    (GtkWidget *widget);
void       gwy_widget_set_activate_on_unfocus    (GtkWidget *widget,
                                                  gboolean activate);
void       gwy_widget_add_sensitivity_follower   (GtkWidget *leader,
                                                  GtkWidget *follower);
void       gwy_widget_remove_sensitivity_follower(GtkWidget *follower,
                                                  GtkWidget *leader);
GtkWidget* gwy_widget_get_sensitivity_leader     (GtkWidget *follower);
void       gwy_toggle_add_visibility_follower    (GObject *toggle,
                                                  GtkWidget *follower);
void       gwy_toggle_remove_visibility_follower (GObject *toggle,
                                                  GtkWidget *follower);
GObject*   gwy_widget_get_visibility_toggle      (GtkWidget *follower);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
