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
#define __LIBGWYUI_WIDGET_UTILS_H__ 1

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GWY_IMPLEMENT_TREE_MODEL(iface_init) \
    { \
        static const GInterfaceInfo gwy_tree_model_interface_info = { \
            (GInterfaceInitFunc)iface_init, NULL, NULL \
        }; \
        g_type_add_interface_static(g_define_type_id, \
                                    GTK_TYPE_TREE_MODEL, \
                                    &gwy_tree_model_interface_info); \
    }

gdouble gwy_scroll_wheel_delta(GtkAdjustment *adjustment,
                               GdkEventScroll *event,
                               GtkOrientation orientation);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
