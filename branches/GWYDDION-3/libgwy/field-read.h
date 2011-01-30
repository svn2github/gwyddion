/*
 *  $Id$
 *  Copyright (C) 2011 David Necas (Yeti).
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

#ifndef __LIBGWY_FIELD_READING_H__
#define __LIBGWY_FIELD_READING_H__

#include <libgwy/field.h>
#include <libgwy/interpolation.h>

G_BEGIN_DECLS

gdouble   gwy_field_get                 (const GwyField *field,
                                         gint col,
                                         gint row,
                                         GwyExteriorType exterior,
                                         gdouble fill_value)                  G_GNUC_PURE;
gdouble   gwy_field_get_interpolated    (const GwyField *field,
                                         gdouble x,
                                         gdouble y,
                                         GwyInterpolationType interpolation,
                                         GwyExteriorType exterior,
                                         gdouble fill_value)                  G_GNUC_PURE;
GwyField* gwy_field_interpolation_coeffs(GwyField *field,
                                         GwyInterpolationType interpolation);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
