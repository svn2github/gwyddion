/*
 *  $Id$
 *  Copyright (C) 2009-2013 David Nečas (Yeti).
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

#ifndef __LIBGWY_LINE_STATISTICS_H__
#define __LIBGWY_LINE_STATISTICS_H__

#include <libgwy/line.h>

G_BEGIN_DECLS

void    gwy_line_min_max_full(const GwyLine *line,
                              gdouble *min,
                              gdouble *max);
gdouble gwy_line_sum_full    (const GwyLine *line);
gdouble gwy_line_mean_full   (const GwyLine *line);
gdouble gwy_line_median_full (const GwyLine *line);
gdouble gwy_line_rms_full    (const GwyLine *line);
gdouble gwy_line_nrms_full   (const GwyLine *line);
gdouble gwy_line_length_full (const GwyLine *line);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
