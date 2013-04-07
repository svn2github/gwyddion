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

#ifndef __LIBGWY_SHAPES_INTERNAL_H__
#define __LIBGWY_SHAPES_INTERNAL_H__

#include "libgwyui/shapes.h"

G_BEGIN_DECLS

#define NEAR_DIST2 30.0
#define NULL_DIST2 0.2
#define ANGLE_STEP (G_PI/12.0)

G_GNUC_INTERNAL
void _gwy_shapes_constrain_horiz_vert(const GwyShapes *shapes, GwyXY *dxy);

G_GNUC_INTERNAL
void _gwy_shapes_remove_null_box(GwyShapes *shapes, guint i);

G_GNUC_INTERNAL
gboolean _gwy_shapes_snap_to_pixel_centre(const GwyShapes *shapes,
                                          gdouble *x, gdouble *y);

G_GNUC_INTERNAL
gboolean _gwy_shapes_snap_to_pixel_corner(const GwyShapes *shapes,
                                          gdouble *x, gdouble *y);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
