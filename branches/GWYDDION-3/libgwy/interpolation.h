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

#ifdef __LIBGWY_ALIASES_H__
#error Public headers must be included before the aliasing header.
#endif

#ifndef __LIBGWY_INTERPOLATION_H__
#define __LIBGWY_INTERPOLATION_H__

#include <glib.h>

G_BEGIN_DECLS

typedef enum {
    /* Keep compatibility, Gwyddion 2.x had NONE = 0 here */
    GWY_INTERPOLATION_BSPLINE1 = 1,
    GWY_INTERPOLATION_ROUND    = GWY_INTERPOLATION_BSPLINE1,
    GWY_INTERPOLATION_BSPLINE2 = 2,
    GWY_INTERPOLATION_LINEAR   = GWY_INTERPOLATION_BSPLINE2,
    GWY_INTERPOLATION_KEYS     = 3,
    GWY_INTERPOLATION_BSPLINE4 = 4,
    GWY_INTERPOLATION_BSPLINE  = GWY_INTERPOLATION_BSPLINE4,
    GWY_INTERPOLATION_OMOMS4   = 5,
    GWY_INTERPOLATION_OMOMS    = GWY_INTERPOLATION_OMOMS4,
    GWY_INTERPOLATION_NNA      = 6,
    GWY_INTERPOLATION_SCHAUM4  = 7,
    GWY_INTERPOLATION_SCHAUM   = GWY_INTERPOLATION_SCHAUM4,
    GWY_INTERPOLATION_BSPLINE6 = 8,
} GwyInterpolationType;

typedef enum {
    GWY_EXTERIOR_UNDEFINED,
    GWY_EXTERIOR_BORDER_EXTEND,
    GWY_EXTERIOR_MIRROR_EXTEND,
    GWY_EXTERIOR_PERIODIC,
    GWY_EXTERIOR_FIXED_VALUE
} GwyExteriorType;

gdouble  gwy_interpolate_1d                       (gdouble x,
                                                   const gdouble *coeff,
                                                   GwyInterpolationType interpolation) G_GNUC_PURE;
gdouble  gwy_interpolate_2d                       (gdouble x,
                                                   gdouble y,
                                                   guint rowstride,
                                                   const gdouble *coeff,
                                                   GwyInterpolationType interpolation) G_GNUC_PURE;
gboolean gwy_interpolation_has_interpolating_basis(GwyInterpolationType interpolation) G_GNUC_CONST;
guint    gwy_interpolation_get_support_size       (GwyInterpolationType interpolation) G_GNUC_CONST;
void     gwy_interpolation_resolve_coeffs_1d      (guint n,
                                                   gdouble *data,
                                                   GwyInterpolationType interpolation);
void     gwy_interpolation_resolve_coeffs_2d      (guint width,
                                                   guint height,
                                                   guint rowstride,
                                                   gdouble *data,
                                                   GwyInterpolationType interpolation);
void     gwy_interpolation_resample_block_1d      (guint length,
                                                   gdouble *data,
                                                   guint newlength,
                                                   gdouble *newdata,
                                                   GwyInterpolationType interpolation,
                                                   gboolean preserve);
void     gwy_interpolation_resample_block_2d      (guint width,
                                                   guint height,
                                                   guint rowstride,
                                                   gdouble *data,
                                                   guint newwidth,
                                                   guint newheight,
                                                   guint newrowstride,
                                                   gdouble *newdata,
                                                   GwyInterpolationType interpolation,
                                                   gboolean preserve);
void     gwy_interpolation_shift_block_1d         (guint length,
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
