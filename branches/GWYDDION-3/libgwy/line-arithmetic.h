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

#ifndef __LIBGWY_LINE_ARITHMETIC_H__
#define __LIBGWY_LINE_ARITHMETIC_H__

#include <libgwy/line.h>
#include <libgwy/field.h>

G_BEGIN_DECLS

void      gwy_line_accumulate   (GwyLine *line);
void      gwy_line_distribute   (GwyLine *line);
void      gwy_line_add          (GwyLine *line,
                                 gdouble value);
void      gwy_line_multiply     (GwyLine *line,
                                 gdouble value);
GwyField* gwy_line_outer_product(const GwyLine *column,
                                 const GwyLine *row)    G_GNUC_MALLOC;

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
