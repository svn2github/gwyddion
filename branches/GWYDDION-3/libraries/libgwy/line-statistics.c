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

#include <string.h>
#include "libgwy/macros.h"
#include "libgwy/math.h"
#include "libgwy/line-statistics.h"
#include "libgwy/line-internal.h"

/**
 * gwy_line_min_max_full:
 * @line: A one-dimensional data line.
 * @min: Location to store the minimum to, or %NULL.
 * @max: Location to store the maximum to, or %NULL.
 *
 * Finds the minimum and maximum value in a line.
 **/
void
gwy_line_min_max_full(const GwyLine *line,
                      gdouble *min,
                      gdouble *max)
{
    g_return_if_fail(GWY_IS_LINE(line));
    if (!min && !max)
        return;
    const gdouble *d = line->data;
    gdouble min1 = *d, max1 = *d;
    d++;
    for (guint i = line->res-1; i; i--, d++) {
        if (min1 > *d)
            min1 = *d;
        if (max1 < *d)
            max1 = *d;
    }
    GWY_MAYBE_SET(min, min1);
    GWY_MAYBE_SET(max, max1);
}

/**
 * gwy_line_sum_full:
 * @line: A one-dimensional data line.
 *
 * Calculates the sum of line values.
 *
 * Returns: The sum of all values in the data line.
 **/
gdouble
gwy_line_sum_full(const GwyLine *line)
{
    g_return_val_if_fail(GWY_IS_LINE(line), 0.0);
    const gdouble *d = line->data;
    gdouble sum = 0.0;
    for (guint i = line->res; i; i--, d++)
        sum += *d;
    return sum;
}

/**
 * gwy_line_mean_full:
 * @line: A one-dimensional data line.
 *
 * Calculates the mean value of an entire line.
 *
 * Returns: The mean value.
 **/
gdouble
gwy_line_mean_full(const GwyLine *line)
{
    g_return_val_if_fail(GWY_IS_LINE(line), NAN);
    const gdouble *d = line->data;
    gdouble mean = 0.0;
    for (guint i = line->res; i; i--, d++)
        mean += *d;
    return mean/line->res;
}

/**
 * gwy_line_mean:
 * @line: A one-dimensional data line.
 * @lpart: (allow-none):
 *         Segment in @line to fill.  Pass %NULL to process entire @line.
 * @mask: (allow-none):
 *        A one dimensional mask line determing which values to take into
 *        account.
 * @masking: Masking mode to use.
 *
 * Calculates the mean value of a line.
 *
 * Returns: The mean value.  The mean value of no data is NaN.
 **/
gdouble
gwy_line_mean(const GwyLine *line,
              const GwyLinePart *lpart,
              const GwyMaskLine *mask,
              GwyMasking masking)
{
    guint pos, len, maskpos;
    if (!gwy_line_check_mask(line, lpart, mask, &masking, &pos, &len, &maskpos))
        return NAN;

    const gdouble *d = line->data + pos;
    gdouble mean = 0.0;
    guint n = 0;

    if (masking == GWY_MASK_IGNORE) {
        for (guint j = len; j; j--, d++)
            mean += *d;
        n = len;
    }
    else {
        const gboolean invert = (masking == GWY_MASK_EXCLUDE);
        GwyMaskIter iter;
        gwy_mask_line_iter_init(mask, iter, maskpos);
        for (guint j = len; j; j--, d++) {
            if (!gwy_mask_iter_get(iter) == invert) {
                mean += *d;
                n++;
            }
            gwy_mask_iter_next(iter);
        }
    }

    if (!n)
        return NAN;
    return mean/n;
}

/**
 * gwy_line_median_full:
 * @line: A one-dimensional data line.
 *
 * Calculates the median value of a line.
 *
 * Returns: The median value.
 **/
gdouble
gwy_line_median_full(const GwyLine *line)
{
    g_return_val_if_fail(GWY_IS_LINE(line), NAN);
    gsize bufsize = line->res*sizeof(gdouble);
    gdouble *buffer = g_slice_alloc(bufsize);
    gwy_assign(buffer, line->data, line->res);
    gdouble median = gwy_math_median(buffer, line->res);
    g_slice_free1(bufsize, buffer);
    return median;
}

/**
 * gwy_line_rms_full:
 * @line: A one-dimensional data line.
 *
 * Calculates the root mean square difference from mean of an entire line.
 *
 * Returns: The root mean square of differences from the mean value.
 **/
gdouble
gwy_line_rms_full(const GwyLine *line)
{
    g_return_val_if_fail(GWY_IS_LINE(line), 0.0);
    const gdouble *d = line->data;
    gdouble rms = 0.0, avg = 0.0;
    for (guint i = line->res; i; i--, d++)
        avg += *d;
    avg /= line->res;
    d = line->data;
    for (guint i = line->res; i; i--, d++)
        rms += (*d - avg)*(*d - avg);
    return sqrt(rms/line->res);
}

/**
 * gwy_line_rms:
 * @line: A one-dimensional data line.
 * @lpart: (allow-none):
 *         Segment in @line to fill.  Pass %NULL to process entire @line.
 * @mask: (allow-none):
 *        A one dimensional mask line determing which values to take into
 *        account.
 * @masking: Masking mode to use.
 *
 * Calculates the root mean square of a line.
 *
 * Returns: The root mean square of differences from the mean value.  The rms
 *          of no data is zero.
 **/
gdouble
gwy_line_rms(const GwyLine *line,
             const GwyLinePart *lpart,
             const GwyMaskLine *mask,
             GwyMasking masking)
{
    guint pos, len, maskpos;
    if (!gwy_line_check_mask(line, lpart, mask, &masking, &pos, &len, &maskpos))
        return NAN;

    const gdouble *d = line->data + pos;
    gdouble rms = 0.0;
    guint n = 0;
    gdouble avg = gwy_line_mean(line, lpart, mask, masking);
    if (isnan(avg))
        return 0.0;

    if (masking == GWY_MASK_IGNORE) {
        for (guint j = len; j; j--, d++) {
            gdouble v = *d - avg;
            rms += v*v;
        }
        n = len;
    }
    else {
        const gboolean invert = (masking == GWY_MASK_EXCLUDE);
        GwyMaskIter iter;
        gwy_mask_line_iter_init(mask, iter, maskpos);
        for (guint j = len; j; j--, d++) {
            if (!gwy_mask_iter_get(iter) == invert) {
                gdouble v = *d - avg;
                rms += v*v;
                n++;
            }
            gwy_mask_iter_next(iter);
        }
    }

    return sqrt(rms/n);
}

/**
 * gwy_line_nrms_full:
 * @line: A one-dimensional data line.
 *
 * Calculates the root mean square difference from neighbour value of a line.
 *
 * If the line does not have at least two points, zero is returned.
 *
 * Returns: The root mean square of differences from neighbour values.
 **/
gdouble
gwy_line_nrms_full(const GwyLine *line)
{
    g_return_val_if_fail(GWY_IS_LINE(line), 0.0);
    if (line->res < 2)
        return 0.0;
    const gdouble *d = line->data;
    gdouble rms = 0.0, prev = *(d++);
    for (guint i = line->res-1; i; i--, d++) {
        rms += (*d - prev)*(*d - prev);
        prev = *d;
    }
    return sqrt(rms/(line->res - 1.0));
}

/**
 * gwy_line_length_full:
 * @line: A one-dimensional data line.
 *
 * Calculates the non-projected length of a line.
 *
 * Returns: The line length.
 **/
gdouble
gwy_line_length_full(const GwyLine *line)
{
    g_return_val_if_fail(GWY_IS_LINE(line), 0.0);
    const gdouble *d = line->data;
    gdouble q = gwy_line_dx(line);
    gdouble length = q;
    for (guint i = line->res-1; i; i--, d++)
        length += hypot(q, d[1] - d[0]);
    return length;
}

/**
 * SECTION: line-statistics
 * @section_id: GwyLine-statistics
 * @title: GwyLine statistics
 * @short_description: Statistical characteristics of lines
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
