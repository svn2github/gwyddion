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

#ifndef __LIBGWY_FIELD_TRANSFORM_H__
#define __LIBGWY_FIELD_TRANSFORM_H__

#include <libgwy/field.h>

G_BEGIN_DECLS

typedef enum {
    GWY_PLANE_IDENTITY = 0,
    GWY_PLANE_MIRROR_HORIZONTALLY,
    GWY_PLANE_MIRROR_VERTICALLY,
    GWY_PLANE_MIRROR_DIAGONALLY,
    GWY_PLANE_MIRROR_ANTIDIAGONALLY,
    GWY_PLANE_MIRROR_BOTH,
    GWY_PLANE_ROTATE_UPSIDE_DOWN = GWY_PLANE_MIRROR_BOTH,
    GWY_PLANE_ROTATE_CLOCKWISE,
    GWY_PLANE_ROTATE_COUNTERCLOCKWISE,
} GwyPlaneCongruenceType;

gboolean  gwy_plane_congruence_is_transposition(GwyPlaneCongruenceType transformation);
void      gwy_field_transform_congruent(GwyField *field,
                                        GwyPlaneCongruenceType transformation);
GwyField* gwy_field_new_congruent      (const GwyField *field,
                                        const GwyFieldPart *fpart,
                                        GwyPlaneCongruenceType transformation)  G_GNUC_MALLOC;
void      gwy_field_transform_offsets  (const GwyField *source,
                                        const GwyFieldPart *srcpart,
                                        GwyField *dest,
                                        GwyPlaneCongruenceType transformation,
                                        const GwyXY *origin);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
