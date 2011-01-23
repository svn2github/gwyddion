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
} GwyFilterType;

typedef enum {
    GWY_CORRELATION_LEVEL      = 1 << 0,
    GWY_CORRELATION_NORMALIZE  = 1 << 1,
    GWY_CORRELATION_PHASE_ONLY = 1 << 2,
} GwyCorrelationFlags;

GwyField* gwy_field_extend         (const GwyField *field,
                                    const GwyRectangle *rectangle,
                                    guint left,
                                    guint right,
                                    guint up,
                                    guint down,
                                    GwyExteriorType exterior,
                                    gdouble fill_value)            G_GNUC_MALLOC;
void      gwy_field_row_convolve   (const GwyField *field,
                                    const GwyRectangle* rectangle,
                                    GwyField *target,
                                    const GwyLine *kernel,
                                    GwyExteriorType exterior,
                                    gdouble fill_value);
void      gwy_field_convolve       (const GwyField *field,
                                    const GwyRectangle* rectangle,
                                    GwyField *target,
                                    const GwyField *kernel,
                                    GwyExteriorType exterior,
                                    gdouble fill_value);
void      gwy_field_filter_gaussian(const GwyField *field,
                                    const GwyRectangle* rectangle,
                                    GwyField *target,
                                    gdouble hsigma,
                                    gdouble vsigma,
                                    GwyExteriorType exterior,
                                    gdouble fill_value);
void      gwy_field_filter_standard(const GwyField *field,
                                    const GwyRectangle* rectangle,
                                    GwyField *target,
                                    GwyFilterType filter,
                                    GwyExteriorType exterior,
                                    gdouble fill_value);
void      gwy_field_filter_median  (const GwyField *field,
                                    const GwyRectangle* rectangle,
                                    GwyField *target,
                                    const GwyMaskField *kernel,
                                    GwyExteriorType exterior,
                                    gdouble fill_value);
void      gwy_field_correlate      (const GwyField *field,
                                    const GwyRectangle* rectangle,
                                    GwyField *target,
                                    const GwyField *kernel,
                                    const GwyMaskField *kmask,
                                    GwyCorrelationFlags flags,
                                    GwyExteriorType exterior,
                                    gdouble fill_value);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
