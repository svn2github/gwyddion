/*
 *  @(#) $Id$
 *  Copyright (C) 2014 David Necas (Yeti).
 *  E-mail: yeti@gwyddion.net.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#ifndef __GWY_APP_LOG_H__
#define __GWY_APP_LOG_H__

#include <gtk/gtk.h>
#include <libgwyddion/gwycontainer.h>
#include <libgwyddion/gwystringlist.h>

G_BEGIN_DECLS

void       gwy_app_channel_log_add        (GwyContainer *data,
                                           gint previd,
                                           gint newid,
                                           const gchar *function,
                                           ...);
void       gwy_app_volume_log_add         (GwyContainer *data,
                                           gint previd,
                                           gint newid,
                                           const gchar *function,
                                           ...);
GtkWidget* gwy_app_log_browser_for_channel(GwyContainer *data,
                                           gint id);
GtkWidget* gwy_app_log_browser_for_volume (GwyContainer *data,
                                           gint id);
gboolean   gwy_log_get_enabled            (void);
void       gwy_log_set_enabled            (gboolean setting);

G_END_DECLS

#endif /* __GWY_APP_LOG_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
