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

#ifndef __LIBGWYUI_STOCK_H__
#define __LIBGWYUI_STOCK_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GWY_ICON_SIZE_ABOUT gwy_icon_size_about()
#define GWY_ICON_SIZE_COLOR_AXIS gwy_icon_size_color_axis()

void gwy_register_stock_items(void);
GtkIconSize gwy_icon_size_about(void) G_GNUC_CONST;
GtkIconSize gwy_icon_size_color_axis(void) G_GNUC_CONST;

#define GWY_STOCK_AXIS_RANGE "gwy-axis-range"
#define GWY_STOCK_COLOR_AXIS "gwy-color-axis"
#define GWY_STOCK_GRADIENTS "gwy-gradients"

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

