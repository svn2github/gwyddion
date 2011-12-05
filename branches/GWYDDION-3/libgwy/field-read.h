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

#ifndef __LIBGWY_FIELD_READ_H__
#define __LIBGWY_FIELD_READ_H__

#include <libgwy/curve.h>
#include <libgwy/field.h>
#include <libgwy/math.h>
#include <libgwy/interpolation.h>

G_BEGIN_DECLS

gdouble   gwy_field_value               (const GwyField *field,
                                         gint col,
                                         gint row,
                                         GwyExteriorType exterior,
                                         gdouble fill_value)                  G_GNUC_PURE;
gdouble   gwy_field_value_interpolated  (const GwyField *field,
                                         gdouble x,
                                         gdouble y,
                                         GwyInterpolationType interpolation,
                                         GwyExteriorType exterior,
                                         gdouble fill_value)                  G_GNUC_PURE;
gdouble   gwy_field_value_averaged      (const GwyField *field,
                                         const GwyMaskField *mask,
                                         GwyMaskingType masking,
                                         gint col,
                                         gint row,
                                         guint ax,
                                         guint ay,
                                         gboolean elliptical,
                                         GwyExteriorType exterior,
                                         gdouble fill_value)                  G_GNUC_PURE;
guint     gwy_field_slope               (const GwyField *field,
                                         const GwyMaskField *mask,
                                         GwyMaskingType masking,
                                         gint col,
                                         gint row,
                                         guint ax,
                                         guint ay,
                                         gboolean elliptical,
                                         GwyExteriorType exterior,
                                         gdouble fill_value,
                                         gdouble *a,
                                         gdouble *bx,
                                         gdouble *by);
guint     gwy_field_curvature           (const GwyField *field,
                                         const GwyMaskField *mask,
                                         GwyMaskingType masking,
                                         gint col,
                                         gint row,
                                         guint ax,
                                         guint ay,
                                         gboolean elliptical,
                                         GwyExteriorType exterior,
                                         gdouble fill_value,
                                         GwyCurvatureParams *curvature);
GwyCurve* gwy_field_profile             (GwyField *field,
                                         gdouble xfrom,
                                         gdouble yfrom,
                                         gdouble xto,
                                         gdouble yto,
                                         guint res,
                                         gdouble thickness,
                                         guint averaging,
                                         GwyInterpolationType interpolation,
                                         GwyExteriorType exterior,
                                         gdouble fill_value)                  G_GNUC_MALLOC;
GwyField* gwy_field_interpolation_coeffs(GwyField *field,
                                         GwyInterpolationType interpolation);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
