/*
 *  $Id$
 *  Copyright (C) 2005,2009 David Necas (Yeti).
 *  Copyright (C) 2003 Petr Klapetek.
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

#ifndef __GWY_INTERPOLATION_H__
#define __GWY_INTERPOLATION_H__

#include <glib.h>

G_BEGIN_DECLS

typedef enum {
    /* Keep compatibility, Gwyddion 2.x had NONE = 0 here */
    GWY_INTERPOLATION_ROUND   = 1,
    GWY_INTERPOLATION_LINEAR  = 2,
    GWY_INTERPOLATION_KEYS    = 3,
    GWY_INTERPOLATION_BSPLINE = 4,
    GWY_INTERPOLATION_OMOMS   = 5,
    GWY_INTERPOLATION_NNA     = 6,
    GWY_INTERPOLATION_SCHAUM  = 7
} GwyInterpolationType;

typedef enum {
    GWY_EXTERIOR_UNDEFINED,
    GWY_EXTERIOR_BORDER_EXTEND,
    GWY_EXTERIOR_MIRROR_EXTEND,
    GWY_EXTERIOR_PERIODIC,
    GWY_EXTERIOR_FIXED_VALUE
} GwyExteriorType;

gdouble  gwy_interpolate_values                   (gdouble x,
                                                   const gdouble *data,
                                                   GwyInterpolationType interpolation);
gdouble  gwy_interpolate_1d                       (gdouble x,
                                                   const gdouble *coeff,
                                                   GwyInterpolationType interpolation);
gdouble  gwy_interpolate_2d                       (gdouble x,
                                                   gdouble y,
                                                   gint rowstride,
                                                   const gdouble *coeff,
                                                   GwyInterpolationType interpolation);
gboolean gwy_interpolation_has_interpolating_basis(GwyInterpolationType interpolation);
gint     gwy_interpolation_get_support_size       (GwyInterpolationType interpolation);
void     gwy_interpolation_resolve_coeffs_1d      (gint n,
                                                   gdouble *data,
                                                   GwyInterpolationType interpolation);
void     gwy_interpolation_resolve_coeffs_2d      (gint width,
                                                   gint height,
                                                   gint rowstride,
                                                   gdouble *data,
                                                   GwyInterpolationType interpolation);
void     gwy_interpolation_resample_block_1d      (gint length,
                                                   gdouble *data,
                                                   gint newlength,
                                                   gdouble *newdata,
                                                   GwyInterpolationType interpolation,
                                                   gboolean preserve);
void     gwy_interpolation_resample_block_2d      (gint width,
                                                   gint height,
                                                   gint rowstride,
                                                   gdouble *data,
                                                   gint newwidth,
                                                   gint newheight,
                                                   gint newrowstride,
                                                   gdouble *newdata,
                                                   GwyInterpolationType interpolation,
                                                   gboolean preserve);
void     gwy_interpolation_shift_block_1d         (gint length,
                                                   gdouble *data,
                                                   gdouble offset,
                                                   gdouble *newdata,
                                                   GwyInterpolationType interpolation,
                                                   GwyExteriorType exterior,
                                                   gdouble fill_value,
                                                   gboolean preserve);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
