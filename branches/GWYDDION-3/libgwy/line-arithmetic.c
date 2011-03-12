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

#include <string.h>
#include "libgwy/macros.h"
#include "libgwy/math.h"
#include "libgwy/line-arithmetic.h"
#include "libgwy/math-internal.h"
#include "libgwy/line-internal.h"

/**
 * gwy_line_is_incompatible:
 * @line1: A data line.
 * @line2: Another data line.
 * @check: Properties to check for compatibility.
 *
 * Checks whether two lines are compatible.
 *
 * Returns: Zero if all tested properties are compatible.  Flags corresponding
 *          to failed tests if lines are not compatible.
 **/
GwyLineCompatibilityFlags
gwy_line_is_incompatible(const GwyLine *line1,
                         const GwyLine *line2,
                         GwyLineCompatibilityFlags check)
{
    g_return_val_if_fail(GWY_IS_LINE(line1), check);
    g_return_val_if_fail(GWY_IS_LINE(line2), check);

    guint res1 = line1->res;
    guint res2 = line2->res;
    gdouble real1 = line1->real;
    gdouble real2 = line2->real;
    GwyLineCompatibilityFlags result = 0;

    /* Resolution */
    if (check & GWY_LINE_COMPATIBLE_RES) {
        if (res1 != res2)
            result |= GWY_LINE_COMPATIBLE_RES;
    }

    /* Real size */
    /* Keeps the conditions for real numbers in negative form to catch NaNs and
     * odd values as incompatible. */
    if (check & GWY_LINE_COMPATIBLE_REAL) {
        if (!(fabs(log(real1/real2)) <= COMPAT_EPSILON))
            result |= GWY_LINE_COMPATIBLE_REAL;
    }

    /* Measure */
    if (check & GWY_LINE_COMPATIBLE_DX) {
        if (!(fabs(log(real1/res1*res2/real2)) <= COMPAT_EPSILON))
            result |= GWY_LINE_COMPATIBLE_DX;
    }

    /* Lateral units */
    if (check & GWY_LINE_COMPATIBLE_LATERAL) {
        /* This can cause instantiation of line units as a side effect */
        GwyUnit *unit1 = gwy_line_get_unit_x(line1);
        GwyUnit *unit2 = gwy_line_get_unit_x(line2);
        if (!gwy_unit_equal(unit1, unit2))
            result |= GWY_LINE_COMPATIBLE_LATERAL;
    }

    /* Value units */
    if (check & GWY_LINE_COMPATIBLE_VALUE) {
        /* This can cause instantiation of line units as a side effect */
        GwyUnit *unit1 = gwy_line_get_unit_y(line1);
        GwyUnit *unit2 = gwy_line_get_unit_y(line2);
        if (!gwy_unit_equal(unit1, unit2))
            result |= GWY_LINE_COMPATIBLE_VALUE;
    }

    return result;
}

// FIXME: These may not belong here, but they do not worth a separate
// header.
/**
 * gwy_line_accumulate:
 * @line: A one-dimensional data line.
 * @unbiased: %TRUE to perform a transformation of a distribution to
 *            cumulative, %FALSE to simply sum the preceeding elements.
 *
 * Transforms a distribution in a line to cumulative distribution.
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
 * Transforms a cumulative distribution in a line to distribution.
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
 * gwy_line_clear:
 * @line: A one-dimensional data line.
 * @lpart: (allow-none):
 *         Segment in @line to clear.  Pass %NULL to clear entire @line.
 *
 * Fills a line with zeroes.
 **/
void
gwy_line_clear(GwyLine *line,
               const GwyLinePart *lpart)
{
    guint pos, len;
    if (!_gwy_line_check_part(line, lpart, &pos, &len))
        return;
    gwy_clear(line->data + pos, len);
}

/**
 * gwy_line_fill:
 * @line: A one-dimensional data line.
 * @lpart: (allow-none):
 *         Segment in @line to fill.  Pass %NULL to fill entire @line.
 * @value: Value to fill the segment with.
 *
 * Fills a line with the specified value.
 **/
void
gwy_line_fill(GwyLine *line,
              const GwyLinePart *lpart,
              gdouble value)
{
    guint pos, len;
    if (!_gwy_line_check_part(line, lpart, &pos, &len))
        return;

    gdouble *p = line->data + pos;
    for (guint j = len; j; j--)
        *(p++) = value;
}

/**
 * gwy_line_add:
 * @line: A one-dimensional data line.
 * @value: Value to add to data.
 *
 * Adds a value to all line items.
 **/
void
gwy_line_add(GwyLine *line,
             gdouble value)
{
    g_return_if_fail(GWY_IS_LINE(line));
    gdouble *p = line->data;
    for (guint i = line->res; i; i--, p++)
        *p += value;
}

/**
 * gwy_line_multiply:
 * @line: A one-dimensional data line.
 * @value: Value to multiply data with.
 *
 * Multiplies all line items with a value.
 **/
void
gwy_line_multiply(GwyLine *line,
                  gdouble value)
{
    g_return_if_fail(GWY_IS_LINE(line));
    gdouble *p = line->data;
    for (guint i = line->res; i; i--, p++)
        *p *= value;
}

/**
 * gwy_line_outer_product:
 * @column: A one-dimensional data line that forms the column vector.
 * @row: A one-dimensional data line that forms the row vector.
 *
 * Performs outer product of two data lines.
 *
 * The horizontal size and physical dimensions of the result correspond to
 * @row while the vertical dimensions to @column.  The value units of the
 * result are equal to the product of the value units of the two lines.
 *
 * Returns: A newly created field.
 **/
GwyField*
gwy_line_outer_product(const GwyLine *column,
                       const GwyLine *row)
{
    g_return_val_if_fail(GWY_IS_LINE(column), NULL);
    g_return_val_if_fail(GWY_IS_LINE(row), NULL);

    guint xres = row->res, yres = column->res;
    GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
    for (guint i = 0; i < yres; i++) {
        for (guint j = 0; j < xres; j++)
            field->data[i*xres + j] = row->data[j] * column->data[i];
    }

    field->xreal = row->real;
    field->yreal = column->real;
    field->xoff = row->off;
    field->yoff = column->off;

    if (row->priv->unit_x || column->priv->unit_x) {
        if (!row->priv->unit_x
            || !column->priv->unit_x
            || !gwy_unit_equal(row->priv->unit_x, column->priv->unit_x)) {
            g_warning("Multiplied lines have different x-units.");
        }
        gwy_unit_assign(gwy_field_get_unit_xy(field), row->priv->unit_x);
    }
    if (row->priv->unit_y || column->priv->unit_y) {
        GwyUnit *zunit = gwy_field_get_unit_z(field);
        if (!row->priv->unit_y)
            gwy_unit_assign(zunit, column->priv->unit_y);
        else if (!column->priv->unit_y)
            gwy_unit_assign(zunit, row->priv->unit_y);
        else
            gwy_unit_multiply(zunit, column->priv->unit_x, row->priv->unit_y);
    }

    return field;
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
    gdouble binsize = n/line->real,
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
        gdouble xlen = (i*binsize - from)/len;
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
        gdouble xlen = (to - i*binsize)/len;
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
    gdouble binsize = n/line->real,
            binfrom = (from - line->off)/binsize,
            binto = (to - line->off)/binsize;

    if (binfrom > n || binto < 0.0)
        return;

    gboolean leftext = binfrom < 0.0, rightext = binto >= n;
    guint ifrom = leftext ? 0 : floor(binfrom);
    guint ito = rightext ? n : floor(binto);
    gdouble len = to - from;
    gdouble wbin = binsize*weight/(len*len);

    guint i = ifrom;
    if (!leftext) {
        if (!rightext && ito == ifrom) {
            // Entire distribution is contained in bin @i.
            line->data[ifrom] += weight;
            return;
        }
        // Distribution starts in bin @i.
        gdouble xlen = (i*binsize - from)/len;
        line->data[i] += weight*xlen*xlen;
        i++;
    }

    // Note if @rightext is TRUE then @ito points after the last element
    // but if @rightext is FALSE then @ito points to the last element.
    while (i < ito) {
        // Open-ended contribution to bin @i.
        line->data[i] += wbin*((2.0*i + 1.0)*binsize - from);
        i++;
    }

    if (!rightext) {
        // Distribution ends in bin @i.
        gdouble xlen = (to - i*binsize)/len;
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
    gdouble binsize = n/line->real,
            binfrom = (from - line->off)/binsize,
            binto = (to - line->off)/binsize;

    if (binfrom > n || binto < 0.0)
        return;

    gboolean leftext = binfrom < 0.0, rightext = binto >= n;
    guint ifrom = leftext ? 0 : floor(binfrom);
    guint ito = rightext ? n : floor(binto);
    gdouble len = to - from;
    gdouble wbin = binsize*weight/(len*len);

    guint i = ifrom;
    if (!leftext) {
        if (!rightext && ito == ifrom) {
            // Entire distribution is contained in bin @i.
            line->data[ifrom] += weight;
            return;
        }
        // Distribution starts in bin @i.
        gdouble xlen = (i*binsize - from)/len;
        line->data[i] += weight*(2.0 - xlen)*xlen;
        i++;
    }

    // Note if @rightext is TRUE then @ito points after the last element
    // but if @rightext is FALSE then @ito points to the last element.
    while (i < ito) {
        // Open-ended contribution to bin @i.
        line->data[i] += wbin*(to - (2.0*i + 1.0)*binsize);
        i++;
    }

    if (!rightext) {
        // Distribution ends in bin @i.
        gdouble xlen = (to - i*binsize)/len;
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
    gdouble binsize = n/line->real,
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
 * SECTION: line-arithmetic
 * @section_id: GwyLine-arithmetic
 * @title: GwyLine arithmetic
 * @short_description: Arithmetic operations with lines
 **/

/**
 * GwyLineCompatibilityFlags:
 * @GWY_LINE_COMPATIBLE_RES: Resolution (size).
 * @GWY_LINE_COMPATIBLE_REAL: Physical dimension.
 * @GWY_LINE_COMPATIBLE_DX: Pixel size.
 * @GWY_LINE_COMPATIBLE_LATERAL: Lateral units.
 * @GWY_LINE_COMPATIBLE_VALUE: Value units.
 * @GWY_LINE_COMPATIBLE_ALL: All defined properties.
 *
 * Line properties that can be checked for compatibility.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
