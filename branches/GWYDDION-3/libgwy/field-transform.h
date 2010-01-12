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

#ifndef __LIBGWY_FIELD_TRANSFORM_H__
#define __LIBGWY_FIELD_TRANSFORM_H__

#include <libgwy/field.h>

G_BEGIN_DECLS

typedef enum {
    GWY_SIMPLE_ROTATE_NONE             = 0,
    GWY_SIMPLE_ROTATE_COUNTERCLOCKWISE = 90,
    GWY_SIMPLE_ROTATE_UPSIDEDOWN       = 180,
    GWY_SIMPLE_ROTATE_CLOCKWISE        = 270,
} GwySimpleRotation;

void      gwy_field_flip               (GwyField *field,
                                        gboolean horizontally,
                                        gboolean vertically,
                                        gboolean transform_offsets);
GwyField* gwy_field_new_rotated_simple (const GwyField *field,
                                        GwySimpleRotation rotation,
                                        gboolean transform_offsets)  G_GNUC_MALLOC;
GwyField* gwy_field_new_transposed     (const GwyField *field)       G_GNUC_MALLOC;
GwyField* gwy_field_new_part_transposed(const GwyField *field,
                                        guint col,
                                        guint row,
                                        guint width,
                                        guint height,
                                        gboolean transform_offsets)  G_GNUC_MALLOC;

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
