/*
 *  $Id$
 *  Copyright (C) 2009-2012 David Nečas (Yeti).
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
#include "libgwy/object-internal.h"
#include "libgwy/field-internal.h"
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
GwyLineCompatFlags
gwy_line_is_incompatible(const GwyLine *line1,
                         const GwyLine *line2,
                         GwyLineCompatFlags check)
{
    g_return_val_if_fail(GWY_IS_LINE(line1), check);
    g_return_val_if_fail(GWY_IS_LINE(line2), check);

    guint res1 = line1->res;
    guint res2 = line2->res;
    gdouble real1 = line1->real;
    gdouble real2 = line2->real;
    GwyLineCompatFlags result = 0;

    /* Resolution */
    if (check & GWY_LINE_COMPAT_RES) {
        if (res1 != res2)
            result |= GWY_LINE_COMPAT_RES;
    }

    /* Real size */
    /* Keeps the conditions for real numbers in negative form to catch NaNs and
     * odd values as incompatible. */
    if (check & GWY_LINE_COMPAT_REAL) {
        if (!(fabs(log(real1/real2)) <= COMPAT_EPSILON))
            result |= GWY_LINE_COMPAT_REAL;
    }

    /* Measure */
    if (check & GWY_LINE_COMPAT_DX) {
        if (!(fabs(log(real1/res1*res2/real2)) <= COMPAT_EPSILON))
            result |= GWY_LINE_COMPAT_DX;
    }

    const Line *priv1 = line1->priv, *priv2 = line2->priv;

    /* Lateral units */
    if (check & GWY_LINE_COMPAT_X) {
        if (!gwy_unit_equal(priv1->unit_x, priv2->unit_x))
            result |= GWY_LINE_COMPAT_X;
    }

    /* Value units */
    if (check & GWY_LINE_COMPAT_VALUE) {
        if (!gwy_unit_equal(priv1->unit_y, priv2->unit_y))
            result |= GWY_LINE_COMPAT_VALUE;
    }

    return result;
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
    if (!gwy_line_check_part(line, lpart, &pos, &len))
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
    if (!gwy_line_check_part(line, lpart, &pos, &len))
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
 * gwy_line_add_line:
 * @src: Source one-dimensional data line.
 * @srcpart: Area in line @src to add.  Pass %NULL to add entire @src.
 * @dest: Destination one-dimensional data line.
 * @destpos: Destination position in @dest.
 * @factor: Value to multiply @src data with before adding.
 *
 * Adds data from one line to another.
 *
 * The segment of added data is defined by @srcpart and the values are
 * added to @dest starting from @destpos.
 *
 * There are no limitations on the positions and dimensions.  Only the part of
 * the segment that is corresponds to data inside @src and @dest is added.
 * This can also mean @dest is not modified at all.
 *
 * If @src is equal to @dest the segments may <emphasis>not</emphasis> overlap.
 **/
void
gwy_line_add_line(const GwyLine *src,
                  const GwyLinePart *srcpart,
                  GwyLine *dest,
                  guint destpos,
                  gdouble factor)
{
    guint pos, len;
    if (!gwy_line_limit_parts(src, srcpart, dest, destpos, &pos, &len))
        return;
    if (!factor)
        return;

    const gdouble *srow = src->data + pos;
    gdouble *drow = dest->data + destpos;
    for (guint i = len; i; i--, srow++, drow++)
        *drow += factor*(*srow);
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
 * Returns: (transfer full):
 *          A newly created field.
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

    _gwy_assign_units(&field->priv->unit_x, row->priv->unit_x);
    _gwy_assign_units(&field->priv->unit_y, column->priv->unit_x);
    if (!gwy_unit_is_empty(row->priv->unit_y)
        || !gwy_unit_is_empty(column->priv->unit_y))
        gwy_unit_multiply(gwy_field_get_unit_z(field),
                          row->priv->unit_y, column->priv->unit_y);

    return field;
}

/**
 * SECTION: line-arithmetic
 * @section_id: GwyLine-arithmetic
 * @title: GwyLine arithmetic
 * @short_description: Arithmetic operations with lines
 **/

/**
 * GwyLineCompatFlags:
 * @GWY_LINE_COMPAT_RES: Resolution (size).
 * @GWY_LINE_COMPAT_REAL: Physical dimension.
 * @GWY_LINE_COMPAT_DX: Pixel size.
 * @GWY_LINE_COMPAT_X: Lateral (@x) units.
 * @GWY_LINE_COMPAT_LATERAL: Alias for %GWY_LINE_COMPAT_X.
 * @GWY_LINE_COMPAT_VALUE: Value units.
 * @GWY_LINE_COMPAT_UNITS: All units.
 * @GWY_LINE_COMPAT_ALL: All defined properties.
 *
 * Line properties that can be checked for compatibility.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
