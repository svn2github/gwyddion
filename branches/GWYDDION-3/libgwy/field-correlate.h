/*
 *  $Id$
 *  Copyright (C) 2010-2011 David Necas (Yeti).
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

#ifndef __LIBGWY_FIELD_CORRELATE_H__
#define __LIBGWY_FIELD_CORRELATE_H__

#include <libgwy/interpolation.h>
#include <libgwy/field.h>

G_BEGIN_DECLS

typedef enum {
    GWY_CORRELATION_LEVEL      = 1 << 0,
    GWY_CORRELATION_NORMALIZE  = 1 << 1,
    GWY_CORRELATION_PHASE_ONLY = 1 << 2,
} GwyCorrelationFlags;

typedef enum {
    GWY_CROSSCORRELATION_LEVEL      = 1 << 0,
    GWY_CROSSCORRELATION_NORMALIZE  = 1 << 1,
} GwyCrosscorrelationFlags;

void      gwy_field_correlate      (const GwyField *field,
                                    const GwyRectangle* rectangle,
                                    GwyField *score,
                                    const GwyField *kernel,
                                    const GwyMaskField *kmask,
                                    GwyCorrelationFlags flags,
                                    GwyExteriorType exterior,
                                    gdouble fill_value);
void      gwy_field_crosscorrelate (const GwyField *field,
                                    const GwyField *reference,
                                    const GwyRectangle* rectangle,
                                    GwyField *score,
                                    GwyField *xoff,
                                    GwyField *yoff,
                                    const GwyMaskField *kernel,
                                    guint colsearch,
                                    guint rowsearch,
                                    GwyCrosscorrelationFlags flags,
                                    GwyExteriorType exterior,
                                    gdouble fill_value);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
