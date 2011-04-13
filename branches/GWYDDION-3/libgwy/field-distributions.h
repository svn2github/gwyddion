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

#ifndef __LIBGWY_FIELD_DISTRIBUTIONS_H__
#define __LIBGWY_FIELD_DISTRIBUTIONS_H__

#include <libgwy/line.h>
#include <libgwy/field.h>
#include <libgwy/mask-field.h>

G_BEGIN_DECLS

// FIXME: Not used for anything.
typedef enum {
    GWY_ORIENTATION_HORIZONTAL,
    GWY_ORIENTATION_VERTICAL,
} GwyOrientation;

GwyLine* gwy_field_value_dist(const GwyField *field,
                              const GwyFieldPart *fpart,
                              const GwyMaskField *mask,
                              GwyMaskingType masking,
                              gboolean cumulative,
                              guint npoints,
                              gdouble min,
                              gdouble max)                   G_GNUC_MALLOC;
GwyLine* gwy_field_slope_dist(const GwyField *field,
                              const GwyFieldPart *fpart,
                              const GwyMaskField *mask,
                              GwyMaskingType masking,
                              gdouble angle,
                              gboolean cumulative,
                              gboolean continuous,
                              guint npoints,
                              gdouble min,
                              gdouble max)                   G_GNUC_MALLOC;
GwyLine* gwy_field_row_psdf  (const GwyField *field,
                              const GwyFieldPart *fpart,
                              const GwyMaskField *mask,
                              GwyMaskingType masking,
                              GwyWindowingType windowing,
                              guint level)                   G_GNUC_MALLOC;
GwyLine* gwy_field_row_acf   (const GwyField *field,
                              const GwyFieldPart *fpart,
                              const GwyMaskField *mask,
                              GwyMaskingType masking,
                              guint level)                   G_GNUC_MALLOC;
GwyLine* gwy_field_row_hhcf  (const GwyField *field,
                              const GwyFieldPart *fpart,
                              const GwyMaskField *mask,
                              GwyMaskingType masking,
                              guint level)                   G_GNUC_MALLOC;

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
