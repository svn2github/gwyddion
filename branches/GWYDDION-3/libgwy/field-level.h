/*
 *  $Id$
 *  Copyright (C) 2009-2010 David Nečas (Yeti).
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

#ifndef __LIBGWY_FIELD_LEVEL_H__
#define __LIBGWY_FIELD_LEVEL_H__

#include <libgwy/line.h>
#include <libgwy/field.h>
#include <libgwy/mask-field.h>

G_BEGIN_DECLS

typedef enum {
    GWY_ROW_SHIFT_MEAN,
    GWY_ROW_SHIFT_MEDIAN,
    GWY_ROW_SHIFT_MEAN_DIFF,
    GWY_ROW_SHIFT_MEDIAN_DIFF,
} GwyRowShiftMethod;

gboolean gwy_field_fit_plane      (const GwyField *field,
                                   const GwyFieldPart *fpart,
                                   const GwyMaskField *mask,
                                   GwyMaskingType masking,
                                   gdouble *a,
                                   gdouble *bx,
                                   gdouble *by);
void     gwy_field_subtract_plane (GwyField *field,
                                   gdouble a,
                                   gdouble bx,
                                   gdouble by);
gboolean gwy_field_inclination    (const GwyField *field,
                                   const GwyFieldPart *fpart,
                                   const GwyMaskField *mask,
                                   GwyMaskingType masking,
                                   gdouble damping,
                                   gdouble *bx,
                                   gdouble *by);
gboolean gwy_field_fit_poly       (const GwyField *field,
                                   const GwyFieldPart *fpart,
                                   const GwyMaskField *mask,
                                   GwyMaskingType masking,
                                   const guint *xpowers,
                                   const guint *ypowers,
                                   guint nterms,
                                   gdouble *coeffs);
void     gwy_field_subtract_poly  (GwyField *field,
                                   const guint *xpowers,
                                   const guint *ypowers,
                                   guint nterms,
                                   const gdouble *coeffs);
void     gwy_field_shift_rows     (GwyField *field,
                                   const GwyLine *shifts);
GwyLine* gwy_field_find_row_shifts(const GwyField *field,
                                   const GwyMaskField *mask,
                                   GwyMaskingType masking,
                                   GwyRowShiftMethod method,
                                   guint min_freedom);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
