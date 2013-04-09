/*
 *  $Id$
 *  Copyright (C) 2013 David Nečas (Yeti).
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

/*< private_header >*/

#ifndef __LIBGWY_GRAPH_INTERNAL_H__
#define __LIBGWY_GRAPH_INTERNAL_H__

#include "libgwyui/graph-area.h"
#include "libgwyui/graph-curve.h"

G_BEGIN_DECLS

#define within_range(from,len,x,tol) \
    (fabs((x) - (from) - 0.5*(len)) <= 0.5*(len) + (tol))
#define range_type(from,len,x,tol) \
    (((x) + (tol) < (from) ? -1 : ((x) - (tol) > (from) + (len) ? 1 : 0)))

typedef struct {
    GwyRange full;
    GwyRange positive;
    gboolean cached : 1;
    gboolean anypresent : 1;
    gboolean pospresent : 1;
} GwyGraphDataRange;

G_GNUC_INTERNAL
void _gwy_graph_calculate_scaling(const GwyGraphArea *grapharea,
                                  const cairo_rectangle_int_t *rect,
                                  gdouble *qx,
                                  gdouble *offx,
                                  gdouble *qy,
                                  gdouble *offy);

G_GNUC_INTERNAL
void _gwy_graph_data_range_union(GwyGraphDataRange *target,
                                 const GwyGraphDataRange *operand);

G_GNUC_UNUSED
static inline gdouble
_gwy_graph_scale_data(gdouble v, GwyGraphScaleType scale)
{
    if (scale == GWY_GRAPH_SCALE_SQRT)
        return gwy_ssqrt(v);
    if (scale == GWY_GRAPH_SCALE_LOG)
        return log(v);
    g_assert(scale == GWY_GRAPH_SCALE_LINEAR);
    return v;
}

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
