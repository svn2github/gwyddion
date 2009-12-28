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

#ifndef __LIBGWY_FIELD_LEVEL_H__
#define __LIBGWY_FIELD_LEVEL_H__

#include <libgwy/field.h>
#include <libgwy/mask-field.h>

G_BEGIN_DECLS

gboolean gwy_field_part_fit_plane  (const GwyField *field,
                                    const GwyMaskField *mask,
                                    GwyMaskingType masking,
                                    guint col,
                                    guint row,
                                    guint width,
                                    guint height,
                                    gdouble *a,
                                    gdouble *bx,
                                    gdouble *by);
void     gwy_field_subtract_plane  (GwyField *field,
                                    gdouble a,
                                    gdouble bx,
                                    gdouble by);
gboolean gwy_field_part_inclination(const GwyField *field,
                                    const GwyMaskField *mask,
                                    GwyMaskingType masking,
                                    guint col,
                                    guint row,
                                    guint width,
                                    guint height,
                                    gdouble damping,
                                    gdouble *bx,
                                    gdouble *by);
gboolean gwy_field_part_fit_poly   (const GwyField *field,
                                    const GwyMaskField *mask,
                                    GwyMaskingType masking,
                                    guint col,
                                    guint row,
                                    guint width,
                                    guint height,
                                    const guint *xpowers,
                                    const guint *ypowers,
                                    guint nterms,
                                    gdouble *coeffs);
void     gwy_field_subtract_poly   (GwyField *field,
                                    const guint *xpowers,
                                    const guint *ypowers,
                                    guint nterms,
                                    const gdouble *coeffs);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
