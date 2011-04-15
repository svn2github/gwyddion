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

/*< private_header >*/

#ifndef __LIBGWY_BRICK_INTERNAL_H__
#define __LIBGWY_BRICK_INTERNAL_H__

#include "libgwy/line-part.h"
#include "libgwy/field-part.h"
#include "libgwy/brick.h"

G_BEGIN_DECLS

struct _GwyBrickPrivate {
    /* FIXME: Consider permitting x-units != y-units. */
    GwyUnit *unit_xy;
    GwyUnit *unit_z;
    GwyUnit *unit_w;
    gboolean allocated;
    gdouble storage;
};

typedef struct _GwyBrickPrivate Brick;

G_GNUC_INTERNAL
gboolean _gwy_brick_check_part(const GwyBrick *brick,
                               const GwyBrickPart *bpart,
                               guint *col,
                               guint *row,
                               guint *level,
                               guint *width,
                               guint *height,
                               guint *depth);

G_GNUC_INTERNAL
gboolean _gwy_brick_check_plane_part(const GwyBrick *brick,
                                     const GwyFieldPart *fpart,
                                     guint *col,
                                     guint *row,
                                     guint level,
                                     guint *width,
                                     guint *height);

G_GNUC_INTERNAL
gboolean _gwy_brick_check_line_part(const GwyBrick *brick,
                                    const GwyLinePart *lpart,
                                    guint col,
                                    guint row,
                                    guint *level,
                                    guint *depth);

G_GNUC_INTERNAL
gboolean _gwy_brick_limit_parts(const GwyBrick *src,
                                const GwyBrickPart *srcpart,
                                const GwyBrick *dest,
                                guint destcol,
                                guint destrow,
                                guint destlevel,
                                guint *col,
                                guint *row,
                                guint *level,
                                guint *width,
                                guint *height,
                                guint *depth);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
