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

#include "libgwy/macros.h"
#include "libgwy/math.h"
#include "libgwyui/graph-internal.h"

static void calculate_one_scaling(gdouble srcfrom,
                                  gdouble srcto,
                                  gdouble destfrom,
                                  gdouble destlen,
                                  GwyGraphScaleType scale,
                                  gdouble *q,
                                  gdouble *off);

void
_gwy_graph_calculate_scaling(const GwyGraphArea *grapharea,
                             const cairo_rectangle_int_t *rect,
                             gdouble *qx, gdouble *offx,
                             gdouble *qy, gdouble *offy)
{
    GwyRange range;
    if (qx || offx) {
        gwy_graph_area_get_xrange(grapharea, &range);
        calculate_one_scaling(range.from, range.to, rect->x, rect->width,
                              gwy_graph_area_get_xscale(grapharea),
                              qx, offx);
    }
    if (qy || offy) {
        gwy_graph_area_get_yrange(grapharea, &range);
        calculate_one_scaling(range.to, range.from, rect->y, rect->height,
                              gwy_graph_area_get_yscale(grapharea),
                              qy, offy);
    }
}

static void
calculate_one_scaling(gdouble srcfrom, gdouble srcto,
                      gdouble destfrom, gdouble destlen,
                      GwyGraphScaleType scale,
                      gdouble *q, gdouble *off)
{
    if (scale == GWY_GRAPH_SCALE_SQRT) {
        srcfrom = gwy_ssqrt(srcfrom);
        srcto = gwy_ssqrt(srcto);
    }
    else if (scale == GWY_GRAPH_SCALE_LOG) {
        if (srcfrom > 0.0)
            srcfrom = log(srcfrom);
        else {
            g_warning("Logscale minimum is %g, fixing to e*DBLMIN", srcfrom);
            srcfrom = log(G_MINDOUBLE) + 1.0;
        }

        if (srcto > 0.0)
            srcto = log(srcto);
        else {
            g_warning("Logscale maximum is %g, fixing to DBLMAX/e", srcto);
            srcto = log(G_MAXDOUBLE) - 1.0;
        }
    }
    else {
        g_assert(scale == GWY_GRAPH_SCALE_LINEAR);
    }

    gdouble srclen = srcto - srcfrom;
    gdouble qq = destlen/srclen;
    GWY_MAYBE_SET(q, qq);
    GWY_MAYBE_SET(off, destfrom - srcfrom*qq);
}

void
_gwy_graph_data_range_union(GwyGraphDataRange *target,
                            const GwyGraphDataRange *operand)
{
    if (operand->anypresent) {
        if (target->anypresent) {
            target->full.from = fmin(target->full.from, operand->full.from);
            target->full.to = fmax(target->full.to, operand->full.to);
        }
        else {
            target->full.from = operand->full.from;
            target->full.to = operand->full.to;
            target->anypresent = TRUE;
        }
    }

    if (operand->pospresent) {
        if (target->pospresent) {
            target->positive.from = fmin(target->positive.from,
                                         operand->positive.from);
            target->positive.to = fmax(target->positive.to,
                                       operand->positive.to);
        }
        else {
            target->positive.from = operand->positive.from;
            target->positive.to = operand->positive.to;
            target->pospresent = TRUE;
        }
    }
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
