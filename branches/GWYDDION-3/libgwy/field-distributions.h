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

#ifndef __LIBGWY_FIELD_DISTRIBUTIONS_H__
#define __LIBGWY_FIELD_DISTRIBUTIONS_H__

#include <libgwy/line.h>
#include <libgwy/field.h>
#include <libgwy/mask-field.h>

G_BEGIN_DECLS

typedef enum {
    GWY_ORIENTATION_HORIZONTAL,
    GWY_ORIENTATION_VERTICAL,
} GwyOrientation;

GwyLine* gwy_field_part_value_dist(GwyField *field,
                                   const GwyMaskField *mask,
                                   GwyMaskingType masking,
                                   guint col,
                                   guint row,
                                   guint width,
                                   guint height,
                                   gboolean cumulative,
                                   guint npoints)              G_GNUC_MALLOC;
GwyLine* gwy_field_part_slope_dist(GwyField *field,
                                   const GwyMaskField *mask,
                                   GwyMaskingType masking,
                                   guint col,
                                   guint row,
                                   guint width,
                                   guint height,
                                   GwyOrientation orientation,
                                   gboolean cumulative,
                                   guint npoints)              G_GNUC_MALLOC;

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
