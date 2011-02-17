/*
 *  $Id$
 *  Copyright (C) 2011 David Nečas (Yeti).
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

#include <libgwy/brick.h>

G_BEGIN_DECLS

typedef enum {
    GWY_BRICK_COMPATIBLE_XRES    = 1 << 0,
    GWY_BRICK_COMPATIBLE_YRES    = 1 << 1,
    GWY_BRICK_COMPATIBLE_ZRES    = 1 << 2,
    GWY_BRICK_COMPATIBLE_RES     = GWY_BRICK_COMPATIBLE_XRES | GWY_BRICK_COMPATIBLE_YRES | GWY_BRICK_COMPATIBLE_ZRES,
    GWY_BRICK_COMPATIBLE_XREAL   = 1 << 3,
    GWY_BRICK_COMPATIBLE_YREAL   = 1 << 4,
    GWY_BRICK_COMPATIBLE_ZREAL   = 1 << 5,
    GWY_BRICK_COMPATIBLE_REAL    = GWY_BRICK_COMPATIBLE_XREAL | GWY_BRICK_COMPATIBLE_YREAL | GWY_BRICK_COMPATIBLE_ZREAL,
    GWY_BRICK_COMPATIBLE_DX      = 1 << 6,
    GWY_BRICK_COMPATIBLE_DY      = 1 << 7,
    GWY_BRICK_COMPATIBLE_DZ      = 1 << 8,
    GWY_BRICK_COMPATIBLE_DXDY    = GWY_BRICK_COMPATIBLE_DX | GWY_BRICK_COMPATIBLE_DY,
    GWY_BRICK_COMPATIBLE_DXDYDZ  = GWY_BRICK_COMPATIBLE_DXDY | GWY_BRICK_COMPATIBLE_DZ,
    GWY_BRICK_COMPATIBLE_LATERAL = 1 << 9,
    GWY_BRICK_COMPATIBLE_DEPTH   = 1 << 10,
    GWY_BRICK_COMPATIBLE_VALUE   = 1 << 11,
    GWY_BRICK_COMPATIBLE_ALL     = 0x0fffu
} GwyBrickCompatibilityFlags;

GwyBrickCompatibilityFlags gwy_brick_is_incompatible(GwyBrick *brick1,
                                                     GwyBrick *brick2,
                                                     GwyBrickCompatibilityFlags check);

void     gwy_brick_clear_full (GwyBrick *brick);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
