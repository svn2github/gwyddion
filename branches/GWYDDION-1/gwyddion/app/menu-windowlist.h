/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
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

#ifndef __GWY_APP_MENU_WINDOWLIST_H__
#define __GWY_APP_MENU_WINDOWLIST_H__

#include <gtk/gtkwidget.h>

G_BEGIN_DECLS

typedef gboolean (*GwyDataWindowMenuFilterFunc)(GwyDataWindow *data_window,
                                                gpointer user_data);

GtkWidget* gwy_option_menu_data_window             (GCallback callback,
                                                    gpointer cbdata,
                                                    const gchar *none_label,
                                                    GtkWidget *current);
GtkWidget* gwy_option_menu_data_window_filtered    (GCallback callback,
                                                    gpointer cbdata,
                                                    const gchar *none_label,
                                                    GtkWidget *current,
                                                    GwyDataWindowMenuFilterFunc filter,
                                                    gpointer user_data);
gboolean   gwy_option_menu_data_window_set_history (GtkWidget *option_menu,
                                                    GtkWidget *current);
GtkWidget* gwy_option_menu_data_window_get_history (GtkWidget *option_menu);

G_END_DECLS

#endif /* __GWY_APP_MENU_WINDOWLIST_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
