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

#include <libgwy/line.h>
#include <libgwy/field.h>

G_BEGIN_DECLS

void gwy_field_row_convolve   (GwyField *field,
                               const GwyRectangle* rectangle,
                               const GwyLine *kernel);
void gwy_field_row_convolve_fft(GwyField *field,
                                const GwyRectangle *rectangle,
                                const GwyLine *kernel);
void gwy_field_col_convolve   (GwyField *field,
                               const GwyRectangle* rectangle,
                               const GwyLine *kernel);
void gwy_field_filter_gaussian(GwyField *field,
                               const GwyRectangle* rectangle,
                               gdouble hsigma,
                               gdouble vsigma);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
