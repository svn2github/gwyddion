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

#include <string.h>
#include "libgwy/macros.h"
#include "libgwy/math.h"
#include "libgwy/line-statistics.h"
#include "libgwy/libgwy-aliases.h"
#include "libgwy/math-internal.h"
#include "libgwy/line-internal.h"

/**
 * gwy_line_min_max:
 * @line: A one-dimensional data line.
 * @min: Location to store the minimum to, or %NULL.
 * @max: Location to store the maximum to, or %NULL.
 *
 * Finds the minimum and maximum value in a line.
 **/
void
gwy_line_min_max(const GwyLine *line,
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
 * gwy_line_mean:
 * @line: A one-dimensional data line.
 *
 * Calculates the mean value of a line.
 *
 * Returns: The mean value.
 **/
gdouble
gwy_line_mean(const GwyLine *line)
{
    g_return_val_if_fail(GWY_IS_LINE(line), NAN);
    const gdouble *d = line->data;
    gdouble mean = 0.0;
    for (guint i = line->res; i; i--, d++)
        mean += *d;
    return mean/line->res;
}

/**
 * gwy_line_median:
 * @line: A one-dimensional data line.
 *
 * Calculates the median value of a line.
 *
 * Returns: The median value.
 **/
gdouble
gwy_line_median(const GwyLine *line)
{
    g_return_val_if_fail(GWY_IS_LINE(line), NAN);
    gsize bufsize = line->res*sizeof(gdouble);
    gdouble *buffer = g_slice_alloc(bufsize);
    ASSIGN(buffer, line->data, line->res);
    gdouble median = gwy_math_median(buffer, line->res);
    g_slice_free1(bufsize, buffer);
    return median;
}

/**
 * gwy_line_rms:
 * @line: A one-dimensional data line.
 *
 * Calculates the rooms mean square value of a line.
 *
 * Returns: The root mean square of differences from the mean value.
 **/
gdouble
gwy_line_rms(const GwyLine *line)
{
    g_return_val_if_fail(GWY_IS_LINE(line), 0.0);
    const gdouble *d = line->data;
    gdouble rms = 0.0, avg = 0.0;
    for (guint i = line->res; i; i--, d++) {
        avg += *d;
        rms += (*d)*(*d);
    }
    rms /= line->res;
    avg /= line->res;
    rms -= avg*avg;
    rms = sqrt(MAX(rms, 0.0));
    return rms;
}

/**
 * gwy_line_length:
 * @line: A one-dimensional data line.
 *
 * Calculates the non-projected length of a line.
 *
 * Returns: The line length.
 **/
gdouble
gwy_line_length(const GwyLine *line)
{
    g_return_val_if_fail(GWY_IS_LINE(line), 0.0);
    const gdouble *d = line->data;
    gdouble q = gwy_line_dx(line);
    gdouble length = q;
    for (guint i = line->res-1; i; i--, d++)
        length += hypot(q, d[1] - d[0]);
    return length;
}

#define __LIBGWY_LINE_STATISTICS_C__
#include "libgwy/libgwy-aliases.c"

/**
 * SECTION: line-statistics
 * @title: GwyLine statistics
 * @short_description: Statistical characteristics of lines
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
