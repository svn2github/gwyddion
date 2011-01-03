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

#ifndef __LIBGWY_FIELD_ARITHMETIC_H__
#define __LIBGWY_FIELD_ARITHMETIC_H__

#include <libgwy/math.h>
#include <libgwy/field.h>
#include <libgwy/mask-field.h>

G_BEGIN_DECLS

typedef enum {
    GWY_FIELD_COMPATIBLE_XRES    = 1 << 0,
    GWY_FIELD_COMPATIBLE_YRES    = 1 << 1,
    GWY_FIELD_COMPATIBLE_RES     = GWY_FIELD_COMPATIBLE_XRES | GWY_FIELD_COMPATIBLE_YRES,
    GWY_FIELD_COMPATIBLE_XREAL   = 1 << 2,
    GWY_FIELD_COMPATIBLE_YREAL   = 1 << 3,
    GWY_FIELD_COMPATIBLE_REAL    = GWY_FIELD_COMPATIBLE_XREAL | GWY_FIELD_COMPATIBLE_YREAL,
    GWY_FIELD_COMPATIBLE_DX      = 1 << 4,
    GWY_FIELD_COMPATIBLE_DY      = 1 << 5,
    GWY_FIELD_COMPATIBLE_DXDY    = GWY_FIELD_COMPATIBLE_DX | GWY_FIELD_COMPATIBLE_DY,
    GWY_FIELD_COMPATIBLE_LATERAL = 1 << 6,
    GWY_FIELD_COMPATIBLE_VALUE   = 1 << 7,
    GWY_FIELD_COMPATIBLE_ALL     = 0x007fu
} GwyFieldCompatibilityFlags;

GwyFieldCompatibilityFlags gwy_field_is_incompatible(GwyField *field1,
                                                     GwyField *field2,
                                                     GwyFieldCompatibilityFlags check);
void                       gwy_field_add            (GwyField *field,
                                                     const GwyRectangle *rectangle,
                                                     const GwyMaskField *mask,
                                                     GwyMaskingType masking,
                                                     gdouble value);
void                       gwy_field_multiply       (GwyField *field,
                                                     const GwyRectangle *rectangle,
                                                     const GwyMaskField *mask,
                                                     GwyMaskingType masking,
                                                     gdouble value);
void                       gwy_field_apply_func     (GwyField *field,
                                                     const GwyRectangle *rectangle,
                                                     const GwyMaskField *mask,
                                                     GwyMaskingType masking,
                                                     GwyRealFunc func,
                                                     gpointer user_data);
void                       gwy_field_add_field      (const GwyField *src,
                                                     const GwyRectangle *srcrectangle,
                                                     GwyField *dest,
                                                     guint destcol,
                                                     guint destrow,
                                                     gdouble factor);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
