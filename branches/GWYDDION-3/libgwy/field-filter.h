/*
 *  $Id$
 *  Copyright (C) 2010-2011 David Nečas (Yeti).
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

#ifndef __LIBGWY_FIELD_FILTER_H__
#define __LIBGWY_FIELD_FILTER_H__

#include <libgwy/interpolation.h>
#include <libgwy/line.h>
#include <libgwy/field.h>

G_BEGIN_DECLS

typedef enum {
    GWY_FILTER_LAPLACE,
    GWY_FILTER_LAPLACE_SCHARR,
    GWY_FILTER_HSOBEL,
    GWY_FILTER_VSOBEL,
    GWY_FILTER_SOBEL,
    GWY_FILTER_HPREWITT,
    GWY_FILTER_VPREWITT,
    GWY_FILTER_PREWITT,
    GWY_FILTER_HSCHARR,
    GWY_FILTER_VSCHARR,
    GWY_FILTER_SCHARR,
    GWY_FILTER_DECHECKER,
    GWY_FILTER_KUWAHARA,
    GWY_FILTER_STEP,
    GWY_FILTER_NONLINEARITY,
} GwyFilterType;

GwyField* gwy_field_new_extended   (const GwyField *field,
                                    const GwyFieldPart *fpart,
                                    guint left,
                                    guint right,
                                    guint up,
                                    guint down,
                                    GwyExteriorType exterior,
                                    gdouble fill_value,
                                    gboolean keep_offsets)      G_GNUC_MALLOC;
void      gwy_field_extend         (const GwyField *field,
                                    const GwyFieldPart *fpart,
                                    GwyField *target,
                                    guint left,
                                    guint right,
                                    guint up,
                                    guint down,
                                    GwyExteriorType exterior,
                                    gdouble fill_value,
                                    gboolean keep_offsets);
void      gwy_field_row_convolve   (const GwyField *field,
                                    const GwyFieldPart *fpart,
                                    GwyField *target,
                                    const GwyLine *kernel,
                                    GwyExteriorType exterior,
                                    gdouble fill_value);
void      gwy_field_convolve       (const GwyField *field,
                                    const GwyFieldPart *fpart,
                                    GwyField *target,
                                    const GwyField *kernel,
                                    GwyExteriorType exterior,
                                    gdouble fill_value);
void      gwy_field_filter_gaussian(const GwyField *field,
                                    const GwyFieldPart *fpart,
                                    GwyField *target,
                                    gdouble hsigma,
                                    gdouble vsigma,
                                    GwyExteriorType exterior,
                                    gdouble fill_value);
void      gwy_field_filter_standard(const GwyField *field,
                                    const GwyFieldPart *fpart,
                                    GwyField *target,
                                    GwyFilterType filter,
                                    GwyExteriorType exterior,
                                    gdouble fill_value);
void      gwy_field_filter_median  (const GwyField *field,
                                    const GwyFieldPart *fpart,
                                    GwyField *target,
                                    const GwyMaskField *kernel,
                                    GwyExteriorType exterior,
                                    gdouble fill_value);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
