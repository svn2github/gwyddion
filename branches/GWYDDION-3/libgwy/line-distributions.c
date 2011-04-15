/*
 *  $Id$
 *  Copyright (C) 2009-2011 David Nečas (Yeti).
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
#include "libgwy/line-distributions.h"

/**
 * gwy_line_accumulate:
 * @line: A one-dimensional data line.
 * @unbiased: %TRUE to perform a transformation of a distribution to
 *            cumulative, %FALSE to simply sum the preceeding elements.
 *
 * Transforms a density in a line to cumulative distribution.
 *
 * If @unbiased is %FALSE each element becomes the sum of all previous elements
 * in the line, including self.  The first element is kept intact. This is
 * useful for accumulating differences such as those calculated with
 * gwy_field_find_row_shifts().
 *
 * For statistical distributions, @unbiased should be usually %TRUE.  In this
 * case each element becomes the sum of all previous elements in the line plus
 * half of self.  This is based on treating the pixels as bins, estimating the
 * distribution within each bin as uniform and then taking samples at bin
 * centres (as is usual).
 **/
void
gwy_line_accumulate(GwyLine *line,
                    gboolean unbiased)
{
    g_return_if_fail(GWY_IS_LINE(line));
    gdouble *p = line->data;
    if (unbiased) {
        gdouble prev = p[0];
        p[0] *= 0.5;
        for (guint i = line->res-1; i; i--, p++) {
            gdouble curr = p[1];
            p[1] = p[0] + 0.5*(prev + curr);
            prev = curr;
        }
    }
    else {
        for (guint i = line->res-1; i; i--, p++)
            p[1] += p[0];
    }
}

/**
 * gwy_line_distribute:
 * @line: A one-dimensional data line.
 * @unbiased: %TRUE to perform a transformation of a cumulative distribution to
 *            non-cumulative, %FALSE to simply subtract the following elements.
 *
 * Transforms a cumulative distribution in a line to density.
 *
 * This method performs the exact inverse of gwy_line_accumulate() provided
 * that the transformation is of the same type, i.e. the same value of
 * @unbiased is passed.  See gwy_line_accumulate() for discussion of the two
 * transformation types.
 *
 * The first element is kept intact if @unbiased is %FALSE.  In some cases you
 * may wish to set it to zero afterwards.
 **/
void
gwy_line_distribute(GwyLine *line,
                    gboolean unbiased)
{
    g_return_if_fail(GWY_IS_LINE(line));
    gdouble *p = line->data + line->res-2;
    for (guint i = line->res-1; i; i--, p--)
        p[1] -= p[0];
    if (unbiased) {
        p = line->data;
        for (guint i = line->res-1; i; i--, p++)
            p[1] -= p[0];
        p = line->data;
        for (guint i = line->res; i; i--, p++)
            *p *= 2.0;
    }
}

/**
 * gwy_line_add_dist_uniform:
 * @line: A one-dimensional data line.
 * @from: Left endpoint of the contribution.
 * @to: Right endpoint of the contribution.
 * @weight: Total weight of the contribution to add.
 *
 * Adds a uniform contribution to a line containing a distribution.
 *
 * The line elements are considered to be equally-sized bins, with the left
 * edge of the first bin at @off and the right edge of the last bin at
 * @off+@real.
 *
 * The added contribution is uniform in [@from, @to] with integral equal to
 * @weight.  If part of the contribution lies outside the line range the
 * corresponding part of the weight will not be added to @line.
 **/
void
gwy_line_add_dist_uniform(GwyLine *line,
                          gdouble from, gdouble to,
                          gdouble weight)
{
    g_return_if_fail(GWY_IS_LINE(line));

    guint n = line->res;
    gdouble binsize = line->real/n,
            binfrom = (from - line->off)/binsize,
            binto = (to - line->off)/binsize;

    if (binfrom > n || binto < 0.0)
        return;

    gboolean leftext = binfrom < 0.0, rightext = binto >= n;
    guint ifrom = leftext ? 0 : floor(binfrom);
    guint ito = rightext ? n : floor(binto);
    gdouble len = to - from;
    gdouble wbin = binsize*weight/len;

    guint i = ifrom;
    if (!leftext) {
        if (!rightext && ito == ifrom) {
            // Entire distribution is contained in bin @i.
            line->data[ifrom] += weight;
            return;
        }
        // Distribution starts in bin @i.
        gdouble xlen = ((i + 1.0)*binsize + line->off - from)/len;
        line->data[i] += weight*xlen;
        i++;
    }

    // Note if @rightext is TRUE then @ito points after the last element
    // but if @rightext is FALSE then @ito points to the last element.
    while (i < ito) {
        // Open-ended contribution to bin @i.
        line->data[i] += wbin;
        i++;
    }

    if (!rightext) {
        // Distribution ends in bin @i.
        gdouble xlen = (to - i*binsize - line->off)/len;
        line->data[i] += weight*xlen;
    }
}

/**
 * gwy_line_add_dist_left_triangular:
 * @line: A one-dimensional data line.
 * @from: Left endpoint of the contribution.
 * @to: Right endpoint of the contribution.
 * @weight: Total weight of the contribution to add.
 *
 * Adds a left-triangular contribution to a line containing a distribution.
 *
 * The line elements are considered to be equally-sized bins, with the left
 * edge of the first bin at @off and the right edge of the last bin at
 * @off+@real.
 *
 * The added contribution is triangular in [@from, @to] with 0 in @from and the
 * maximum in @to and integral equal to @weight.  If part of the contribution
 * lies outside the line range the corresponding part of the weight will not be
 * added to @line.
 **/
void
gwy_line_add_dist_left_triangular(GwyLine *line,
                                  gdouble from, gdouble to,
                                  gdouble weight)
{
    g_return_if_fail(GWY_IS_LINE(line));

    guint n = line->res;
    gdouble binsize = line->real/n,
            binfrom = (from - line->off)/binsize,
            binto = (to - line->off)/binsize;

    if (binfrom > n || binto < 0.0)
        return;

    gboolean leftext = binfrom < 0.0, rightext = binto >= n;
    guint ifrom = leftext ? 0 : floor(binfrom);
    guint ito = rightext ? n : floor(binto);
    gdouble len = to - from;
    gdouble wbin = 2.0*binsize*weight/(len*len);

    guint i = ifrom;
    if (!leftext) {
        if (!rightext && ito == ifrom) {
            // Entire distribution is contained in bin @i.
            line->data[ifrom] += weight;
            return;
        }
        // Distribution starts in bin @i.
        gdouble xlen = ((i + 1.0)*binsize + line->off - from)/len;
        line->data[i] += weight*xlen*xlen;
        i++;
    }

    // Note if @rightext is TRUE then @ito points after the last element
    // but if @rightext is FALSE then @ito points to the last element.
    while (i < ito) {
        // Open-ended contribution to bin @i.
        line->data[i] += wbin*((i + 0.5)*binsize + line->off - from);
        i++;
    }

    if (!rightext) {
        // Distribution ends in bin @i.
        gdouble xlen = (to - i*binsize - line->off)/len;
        line->data[i] += weight*(2.0 - xlen)*xlen;
    }
}

/**
 * gwy_line_add_dist_right_triangular:
 * @line: A one-dimensional data line.
 * @from: Left endpoint of the contribution.
 * @to: Right endpoint of the contribution.
 * @weight: Total weight of the contribution to add.
 *
 * Adds a right-triangular contribution to a line containing a distribution.
 *
 * The line elements are considered to be equally-sized bins, with the left
 * edge of the first bin at @off and the right edge of the last bin at
 * @off+@real.
 *
 * The added contribution is triangular in [@from, @to] with the maximum in
 * @from and 0 in @to and integral equal to @weight.  If part of the
 * contribution lies outside the line range the corresponding part of the
 * weight will not be added to @line.
 **/
void
gwy_line_add_dist_right_triangular(GwyLine *line,
                                   gdouble from, gdouble to,
                                   gdouble weight)
{
    g_return_if_fail(GWY_IS_LINE(line));

    guint n = line->res;
    gdouble binsize = line->real/n,
            binfrom = (from - line->off)/binsize,
            binto = (to - line->off)/binsize;

    if (binfrom > n || binto < 0.0)
        return;

    gboolean leftext = binfrom < 0.0, rightext = binto >= n;
    guint ifrom = leftext ? 0 : floor(binfrom);
    guint ito = rightext ? n : floor(binto);
    gdouble len = to - from;
    gdouble wbin = 2.0*binsize*weight/(len*len);

    guint i = ifrom;
    if (!leftext) {
        if (!rightext && ito == ifrom) {
            // Entire distribution is contained in bin @i.
            line->data[ifrom] += weight;
            return;
        }
        // Distribution starts in bin @i.
        gdouble xlen = ((i + 1.0)*binsize + line->off - from)/len;
        line->data[i] += weight*(2.0 - xlen)*xlen;
        i++;
    }

    // Note if @rightext is TRUE then @ito points after the last element
    // but if @rightext is FALSE then @ito points to the last element.
    while (i < ito) {
        // Open-ended contribution to bin @i.
        line->data[i] += wbin*(to - (i + 0.5)*binsize - line->off);
        i++;
    }

    if (!rightext) {
        // Distribution ends in bin @i.
        gdouble xlen = (to - i*binsize - line->off)/len;
        line->data[i] += weight*xlen*xlen;
    }
}

/**
 * gwy_line_add_dist_delta:
 * @line: A one-dimensional data line.
 * @value: Position of the contribution.
 * @weight: Total weight of the contribution to add.
 *
 * Adds a delta-contribution to a line containing a distribution.
 *
 * The line elements are considered to be equally-sized bins, with the left
 * edge of the first bin at @off and the right edge of the last bin at
 * @off+@real.
 *
 * The added contribution is a δ-function at @value.
 **/
void
gwy_line_add_dist_delta(GwyLine *line,
                        gdouble value,
                        gdouble weight)
{
    g_return_if_fail(GWY_IS_LINE(line));

    guint n = line->res;
    gdouble binsize = line->real/n,
            binvalue = (value - line->off)/binsize;
    guint ivalue;

    if (binvalue > n || binvalue < 0.0)
        return;

    if (binvalue == n)
        ivalue = n-1;
    else
        ivalue = floor(binvalue);

    line->data[ivalue] += weight;
}

/**
 * gwy_line_add_dist_trapezoidal:
 * @line: A one-dimensional data line.
 * @from: Left endpoint of the contribution.
 * @mid1: Left midpoint vertex of the trapezoid.
 * @mid2: Left midpoint vertex of the trapezoid.
 * @to: Right endpoint of the contribution.
 * @weight: Total weight of the contribution to add.
 *
 * Adds a trapezoidal contribution to a line containing a distribution.
 *
 * The line elements are considered to be equally-sized bins, with the left
 * edge of the first bin at @off and the right edge of the last bin at
 * @off+@real.
 *
 * The added contribution is trapezoidal in [@from, @to], with 0 in the
 * boundary points @from and @to, constant within [@mid1, @mid2] and triangular
 * in intervals [@from, @mid1] and [@mid2, @to].  If part of the contribution
 * lies outside the line range the corresponding part of the weight will not be
 * added to @line.
 **/
void
gwy_line_add_dist_trapezoidal(GwyLine *line,
                              gdouble from, gdouble mid1,
                              gdouble mid2, gdouble to,
                              gdouble weight)
{
    if (to - from < 1e-12*(fabs(from) + fabs(to))) {
        gwy_line_add_dist_delta(line, 0.5*(from + to), weight);
        return;
    }

    // If @to > @from at least one of the subintervals must be also non-zero.
    gdouble w = weight/((to - from) + (mid2 - mid1));
    if (mid1 > from)
        gwy_line_add_dist_left_triangular(line, from, mid1, w*(mid1 - from));
    if (mid2 > mid1)
        gwy_line_add_dist_uniform(line, mid1, mid2, 2.0*w*(mid2 - mid1));
    if (to > mid2)
        gwy_line_add_dist_right_triangular(line, mid2, to, w*(to - mid2));
}

/**
 * SECTION: line-distributions
 * @section_id: GwyLine-distributions
 * @title: GwyLine as a distribution
 * @short_description: Using lines as histograms and distributions
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
