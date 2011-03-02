/*
 *  $Id$
 *  Copyright (C) 2009 David Nečas (Yeti).
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

#ifndef __LIBGWY_LINE_INTERNAL_H__
#define __LIBGWY_LINE_INTERNAL_H__

#include "libgwy/line.h"

G_BEGIN_DECLS

struct _GwyLinePrivate {
    GwyUnit *unit_x;
    GwyUnit *unit_y;
    gboolean allocated;
    gdouble storage;
};

typedef struct _GwyLinePrivate Line;

G_GNUC_INTERNAL
gboolean _gwy_line_check_part(const GwyLine *line,
                              const GwyLinePart *lpart,
                              guint *pos,
                              guint *len);

G_GNUC_INTERNAL
gboolean _gwy_line_limit_parts(const GwyLine *src,
                               const GwyLinePart *srcpart,
                               const GwyLine *dest,
                               guint destpos,
                               guint *pos,
                               guint *len);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
