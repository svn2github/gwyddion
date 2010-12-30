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

#ifndef __LIBGWY_FIELD_STATISTICS_H__
#define __LIBGWY_FIELD_STATISTICS_H__

#include <libgwy/field.h>
#include <libgwy/mask-field.h>

G_BEGIN_DECLS

void    gwy_field_min_max       (const GwyField *field,
                                 const GwyRectangle *rectangle,
                                 const GwyMaskField *mask,
                                 GwyMaskingType masking,
                                 gdouble *min,
                                 gdouble *max);
void    gwy_field_min_max_full  (const GwyField *field,
                                 gdouble *min,
                                 gdouble *max);
gdouble gwy_field_mean          (const GwyField *field,
                                 const GwyRectangle *rectangle,
                                 const GwyMaskField *mask,
                                 GwyMaskingType masking);
gdouble gwy_field_mean_full     (const GwyField *field);
gdouble gwy_field_median        (const GwyField *field,
                                 const GwyRectangle *rectangle,
                                 const GwyMaskField *mask,
                                 GwyMaskingType masking);
gdouble gwy_field_median_full   (const GwyField *field);
gdouble gwy_field_rms           (const GwyField *field,
                                 const GwyRectangle *rectangle,
                                 const GwyMaskField *mask,
                                 GwyMaskingType masking);
gdouble gwy_field_rms_full      (const GwyField *field);
void    gwy_field_statistics    (const GwyField *field,
                                 const GwyRectangle *rectangle,
                                 const GwyMaskField *mask,
                                 GwyMaskingType masking,
                                 gdouble *mean,
                                 gdouble *ra,
                                 gdouble *rms,
                                 gdouble *skew,
                                 gdouble *kurtosis);
gdouble gwy_field_surface_area  (const GwyField *field,
                                 const GwyRectangle *rectangle,
                                 const GwyMaskField *mask,
                                 GwyMaskingType masking);
guint   gwy_field_count_in_range(const GwyField *field,
                                 const GwyRectangle *rectangle,
                                 const GwyMaskField *mask,
                                 GwyMaskingType masking,
                                 gdouble lower,
                                 gdouble upper,
                                 gboolean strict,
                                 guint *nabove,
                                 guint *nbelow);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
