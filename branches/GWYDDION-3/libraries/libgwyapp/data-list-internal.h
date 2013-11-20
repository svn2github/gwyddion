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

/*< private_header >*/

#ifndef __LIBGWYAPP_DATA_LIST_INTERNAL_H__
#define __LIBGWYAPP_DATA_LIST_INTERNAL_H__

#include "libgwyapp/data-list.h"
#include "libgwyapp/data-item.h"

G_BEGIN_DECLS

#define GWY_DATA_NKINDS (GWY_DATA_SURFACE+1)

#define MAX_FILE_ID 0xffffff
#define MAX_ITEM_ID G_MAXUINT

G_GNUC_INTERNAL
void _gwy_data_item_set_list_and_id(GwyDataItem *dataitem,
                                    GwyDataList *datalist,
                                    guint id);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
