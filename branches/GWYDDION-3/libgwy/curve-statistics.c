/*
 *  $Id$
 *  Copyright (C) 2010 David Necas (Yeti).
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
#include "libgwy/curve-statistics.h"
#include "libgwy/curve-internal.h"

/**
 * gwy_curve_min_max_full:
 * @curve: A curve.
 * @min: (allow-none):
 *       Location to store the minimum value to, or %NULL.
 * @max: (allow-none):
 *       Location to store the maximum value to, or %NULL.
 *
 * Finds the minimum and maximum value in an entire curve.
 *
 * If the curve is empty @min is set to a huge positive value and @max to
 * a huge negative value.
 **/
void
gwy_curve_min_max_full(const GwyCurve *curve,
                       gdouble *pmin,
                       gdouble *pmax)
{
    g_return_if_fail(GWY_IS_CURVE(curve));
    if (!pmin && !pmax)
        return;

    gdouble min = HUGE_VAL;
    gdouble max = -HUGE_VAL;
    const GwyXY *p = curve->data;
    for (guint i = curve->n; i; i--, p++) {
        if (p->y < min)
            min = p->y;
        if (p->y > max)
            max = p->y;
    }
    GWY_MAYBE_SET(pmin, min);
    GWY_MAYBE_SET(pmax, max);
}

/**
 * gwy_curve_range_full:
 * @curve: A curve.
 * @min: (allow-none):
 *       Location to store the minimum abscissa to, or %NULL.
 * @max: (allow-none):
 *       Location to store the maximum abscissa to, or %NULL.
 *
 * Finds the minimum and maximum abscissa of an entire curve.
 *
 * Since the curve is sorted they can be also found by looking at x-coordinates
 * of the frist and last point.  If the curve is empty @min and @max are set
 * to NaN.
 **/
void
gwy_curve_range_full(const GwyCurve *curve,
                     gdouble *min,
                     gdouble *max)
{
    g_return_if_fail(GWY_IS_CURVE(curve));
    GWY_MAYBE_SET(min, curve->n ? curve->data[0].x : NAN);
    GWY_MAYBE_SET(max, curve->n ? curve->data[curve->n-1].x : NAN);
}

/**
 * gwy_curve_mean_full:
 * @curve: A curve.
 *
 * Calculates the mean value of an entire curve.
 *
 * Returns: The mean value.  The mean value of an empty curve is NaN.
 **/
gdouble
gwy_curve_mean_full(const GwyCurve *curve)
{
    g_return_val_if_fail(GWY_IS_CURVE(curve), NAN);
    if (G_UNLIKELY(!curve->n))
        return NAN;

    gdouble s = 0.0;
    const GwyXY *p = curve->data;
    for (guint i = curve->n; i; i--, p++)
        s += p->y;

    return s/curve->n;
}

/**
 * gwy_curve_median_dx_full:
 * @curve: A curve.
 *
 * Calculates the median of abscissa differences between neighbour points of an
 * entire curve.
 *
 * Returns: The median abscissa difference.  NaN is returned for an empty
 *          curve and zero for a single-point curve.
 **/
gdouble
gwy_curve_median_dx_full(const GwyCurve *curve)
{
    g_return_val_if_fail(GWY_IS_CURVE(curve), NAN);
    if (G_UNLIKELY(!curve->n))
        return NAN;
    if (G_UNLIKELY(curve->n == 1))
        return 0.0;

    const GwyXY *p = curve->data;
    guint last = curve->n - 1;
    gdouble *xd = g_slice_alloc(last*sizeof(gdouble));
    for (guint i = 0; i < last; i++)
        xd[i] = p[i+1].x - p[i].x;
    gdouble mdx = gwy_math_median(xd, last);
    g_slice_free1(last*sizeof(gdouble), xd);
    return mdx;
}


/**
 * SECTION: curve-statistics
 * @section_id: GwyCurve-statistics
 * @title: GwyCurve statistics
 * @short_description: Statistical characteristics of curves
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
