/*
 *  $Id$
 *  Copyright (C) 2009-2013 David Nečas (Yeti).
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

#ifndef __LIBGWY_MAIN_H__
#define __LIBGWY_MAIN_H__

#include <glib.h>

G_BEGIN_DECLS

void    gwy_type_init          (void);
gchar*  gwy_library_directory  (const gchar *subdir) G_GNUC_MALLOC;
gchar*  gwy_data_directory     (const gchar *subdir) G_GNUC_MALLOC;
gchar*  gwy_locale_directory   (const gchar *subdir) G_GNUC_MALLOC;
gchar*  gwy_user_directory     (const gchar *subdir) G_GNUC_MALLOC;
gchar** gwy_library_search_path(const gchar *subdir) G_GNUC_MALLOC;
gchar** gwy_data_search_path   (const gchar *subdir) G_GNUC_MALLOC;
void    gwy_tune_algorithms    (const gchar *key,
                                const gchar *value);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
