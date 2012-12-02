/*
 *  $Id$
 *  Copyright (C) 2011-2012 David Nečas (Yeti).
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

#ifndef __LIBGWY_BRICK_ARITHMETIC_H__
#define __LIBGWY_BRICK_ARITHMETIC_H__

#include <libgwy/line-arithmetic.h>
#include <libgwy/field-arithmetic.h>
#include <libgwy/brick.h>

G_BEGIN_DECLS

typedef enum {
    GWY_BRICK_COMPAT_XRES    = 1 << 0,
    GWY_BRICK_COMPAT_YRES    = 1 << 1,
    GWY_BRICK_COMPAT_ZRES    = 1 << 2,
    GWY_BRICK_COMPAT_RES     = GWY_BRICK_COMPAT_XRES | GWY_BRICK_COMPAT_YRES | GWY_BRICK_COMPAT_ZRES,
    GWY_BRICK_COMPAT_XREAL   = 1 << 3,
    GWY_BRICK_COMPAT_YREAL   = 1 << 4,
    GWY_BRICK_COMPAT_ZREAL   = 1 << 5,
    GWY_BRICK_COMPAT_REAL    = GWY_BRICK_COMPAT_XREAL | GWY_BRICK_COMPAT_YREAL | GWY_BRICK_COMPAT_ZREAL,
    GWY_BRICK_COMPAT_DX      = 1 << 6,
    GWY_BRICK_COMPAT_DY      = 1 << 7,
    GWY_BRICK_COMPAT_DZ      = 1 << 8,
    GWY_BRICK_COMPAT_DXDY    = GWY_BRICK_COMPAT_DX | GWY_BRICK_COMPAT_DY,
    GWY_BRICK_COMPAT_DXDYDZ  = GWY_BRICK_COMPAT_DXDY | GWY_BRICK_COMPAT_DZ,
    GWY_BRICK_COMPAT_X       = 1 << 9,
    GWY_BRICK_COMPAT_Y       = 1 << 10,
    GWY_BRICK_COMPAT_LATERAL = GWY_BRICK_COMPAT_X | GWY_BRICK_COMPAT_Y,
    GWY_BRICK_COMPAT_Z       = 1 << 10,
    GWY_BRICK_COMPAT_SPACE   = GWY_BRICK_COMPAT_LATERAL | GWY_BRICK_COMPAT_Z,
    GWY_BRICK_COMPAT_VALUE   = 1 << 11,
    GWY_BRICK_COMPAT_UNITS   = GWY_BRICK_COMPAT_SPACE | GWY_BRICK_COMPAT_VALUE,
    GWY_BRICK_COMPAT_ALL     = 0x0fffu
} GwyBrickCompatFlags;

GwyBrickCompatFlags gwy_brick_is_incompatible(const GwyBrick *brick1,
                                              const GwyBrick *brick2,
                                              GwyBrickCompatFlags check);
GwyFieldCompatFlags gwy_brick_is_incompatible_with_field(const GwyBrick *brick,
                                                         const GwyField *field,
                                                         GwyFieldCompatFlags check);
GwyLineCompatFlags gwy_brick_is_incompatible_with_line(const GwyBrick *brick,
                                                       const GwyLine *line,
                                                       GwyLineCompatFlags check);

void gwy_brick_extract_plane(const GwyBrick *brick,
                             GwyField *target,
                             const GwyFieldPart *fpart,
                             guint level,
                             gboolean keep_offsets);
void gwy_brick_extract_line (const GwyBrick *brick,
                             GwyLine *target,
                             const GwyLinePart *lpart,
                             guint col,
                             guint row,
                             gboolean keep_offsets);
void gwy_brick_clear_full   (GwyBrick *brick);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
