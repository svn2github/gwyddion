/*
 *  $Id$
 *  Copyright (C) 2009-2011 David Nečas (Yeti).
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

typedef enum {
    GWY_LINE_COMPATIBLE_RES     = 1 << 0,
    GWY_LINE_COMPATIBLE_REAL    = 1 << 1,
    GWY_LINE_COMPATIBLE_DX      = 1 << 2,
    GWY_LINE_COMPATIBLE_LATERAL = 1 << 3,
    GWY_LINE_COMPATIBLE_VALUE   = 1 << 4,
    GWY_LINE_COMPATIBLE_UNITS   = GWY_LINE_COMPATIBLE_LATERAL | GWY_LINE_COMPATIBLE_VALUE,
    GWY_LINE_COMPATIBLE_ALL     = 0x001fu
} GwyLineCompatibilityFlags;

GwyLineCompatibilityFlags gwy_line_is_incompatible(const GwyLine *line1,
                                                   const GwyLine *line2,
                                                   GwyLineCompatibilityFlags check);

void      gwy_line_add          (GwyLine *line,
                                 gdouble value);
void      gwy_line_multiply     (GwyLine *line,
                                 gdouble value);
void      gwy_line_clear        (GwyLine *line,
                                 const GwyLinePart *lpart);
void      gwy_line_fill         (GwyLine *line,
                                 const GwyLinePart *lpart,
                                 gdouble value);
void      gwy_line_add_line     (const GwyLine *src,
                                 const GwyLinePart *srcpart,
                                 GwyLine *dest,
                                 guint destpos,
                                 gdouble factor);
GwyField* gwy_line_outer_product(const GwyLine *column,
                                 const GwyLine *row)         G_GNUC_MALLOC;

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
