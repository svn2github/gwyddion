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

#ifndef __GWY_APP_APP_H__
#define __GWY_APP_APP_H__

#include <gtk/gtkwidget.h>
#include <libgwyddion/gwycontainer.h>
#include <libgwydgets/gwydatawindow.h>
#include <libgwydgets/gwygraph.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

GwyContainer*   gwy_app_get_current_data          (void);
GwyDataWindow*  gwy_app_data_window_get_current   (void);
void            gwy_app_data_window_set_current   (GwyDataWindow *window);
void            gwy_app_data_window_remove        (GwyDataWindow *window);
void            gwy_app_data_window_foreach       (GFunc func,
                                                   gpointer user_data);
GtkWidget*      gwy_app_data_window_create        (GwyContainer *data);
gint            gwy_app_data_window_set_untitled  (GwyDataWindow *window,
                                                   const gchar *templ);
void            gwy_app_data_view_update          (GtkWidget *data_view);
void            gwy_app_undo_checkpoint           (GwyContainer *data,
                                                   const gchar *what);
void            gwy_app_undo_undo                 (void);
void            gwy_app_undo_redo                 (void);
void            gwy_app_change_mask_color_cb      (gpointer unused,
                                                   gboolean defaultc);

GtkWidget*      gwy_app_graph_window_get_current  (void);
void            gwy_app_graph_window_set_current  (GtkWidget *window);
void            gwy_app_graph_window_remove       (GtkWidget *window);
GtkWidget*      gwy_app_graph_window_create       (GtkWidget *graph);

void            gwy_app_tool_use_cb               (const gchar *toolname,
                                                   GtkWidget *button);
void            gwy_app_zoom_set_cb               (gpointer data);
void            gwy_app_mask_kill_cb              (void);
void            gwy_app_show_kill_cb              (void);
GtkWidget*      gwy_app_main_window_get           (void);
void            gwy_app_main_window_set           (GtkWidget *window);
gboolean        gwy_app_quit                      (void);


/* FIXME: ugly. to be moved somewhere? refactored? */
void       gwy_app_clean_up_data            (GwyContainer *data);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GWY_APP_APP_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

