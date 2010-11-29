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

/*< private_header >*/

#ifndef __LIBGWY_LINE_INTERNAL_H__
#define __LIBGWY_LINE_INTERNAL_H__

G_BEGIN_DECLS

// Evaluates to TRUE if intervals [pos1, pos1+len1-1] and [pos2, pos2+len2-1]
// are overlapping.  Arguments are evaluated many times.
#define OVERLAPPING(pos1, len1, pos2, len2) \
    (MAX((pos1) + (len1), (pos2) + (len2)) - MIN((pos1), (pos2)) \
     < (len1) + (len2))

struct _GwyLinePrivate {
    GwyUnit *unit_x;
    GwyUnit *unit_y;
    gboolean allocated;
};

typedef struct _GwyLinePrivate Line;

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
