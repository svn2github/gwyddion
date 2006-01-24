/*
 *  @(#) $Id$
 *  Copyright (C) 2003,2004 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 */

#ifndef __GWY_MODULE_GRAPH_H__
#define __GWY_MODULE_GRAPH_H__

#include <libgwydgets/gwygraph.h>

G_BEGIN_DECLS

typedef gboolean    (*GwyGraphFunc)     (GwyGraph *graph,
                                         const gchar *name);

gboolean     gwy_graph_func_register            (const gchar *name,
                                                 GwyGraphFunc func,
                                                 const gchar *menu_path,
                                                 const gchar *stock_id,
                                                 guint sens_mask,
                                                 const gchar *tooltip);
gboolean     gwy_graph_func_run                 (const guchar *name,
                                                 GwyGraph *graph);
gboolean     gwy_graph_func_exists              (const gchar *name);
const gchar* gwy_graph_func_get_menu_path       (const gchar *name);
const gchar* gwy_graph_func_get_stock_id        (const gchar *name);
const gchar* gwy_graph_func_get_tooltip         (const gchar *name);
guint        gwy_graph_func_get_sensitivity_mask(const gchar *name);
void         gwy_graph_func_foreach             (GFunc function,
                                                 gpointer user_data);

G_END_DECLS

#endif /* __GWY_MODULE_GRAPH_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
