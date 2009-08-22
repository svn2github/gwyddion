/*
 *  $Id$
 *  Copyright (C) 2009 David Necas (Yeti).
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

#ifndef __LIBGWY_ERROR_LIST_H__
#define __LIBGWY_ERROR_LIST_H__

#include <glib.h>

G_BEGIN_DECLS

typedef GSList GwyErrorList;

void gwy_error_list_add      (GwyErrorList **list,
                              GQuark domain,
                              gint code,
                              const gchar *format,
                              ...) G_GNUC_PRINTF(4, 5);
void gwy_error_list_propagate(GwyErrorList **list,
                              GError *error);
void gwy_error_list_clear    (GwyErrorList **list);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
