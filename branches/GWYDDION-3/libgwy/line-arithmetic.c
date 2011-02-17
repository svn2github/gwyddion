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
#include "libgwy/line-internal.h"

// For compatibility checks.
#define EPSILON 1e-6

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
gwy_line_is_incompatible(GwyLine *line1,
                         GwyLine *line2,
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
        if (!(fabs(log(real1/real2)) <= EPSILON))
            result |= GWY_LINE_COMPATIBLE_REAL;
    }

    /* Measure */
    if (check & GWY_LINE_COMPATIBLE_DX) {
        if (!(fabs(log(real1/res1*res2/real2)) <= EPSILON))
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

// FIXME: These two may not belong here, but they do not worth a separate
// header.
/**
 * gwy_line_accumulate:
 * @line: A one-dimensional data line.
 *
 * Transforms a distribution in a line to cummulative distribution.
 *
 * Each element becomes the sum of all previous elements in the line, including
 * self.
 **/
void
gwy_line_accumulate(GwyLine *line)
{
    g_return_if_fail(GWY_IS_LINE(line));
    gdouble *p = line->data;
    for (guint i = line->res-1; i; i--, p++)
        p[1] += p[0];
}

/**
 * gwy_line_distribute:
 * @line: A one-dimensional data line.
 *
 * Transforms a cummulative distribution in a line to distribution.
 *
 * Each element except the first is set to the difference beteen self and the
 * previous element.
 *
 * The first element is kept intact to make this method the exact inverse of
 * gwy_line_accumulate().  However, you might also wish to set it to zero
 * afterwards.
 **/
void
gwy_line_distribute(GwyLine *line)
{
    g_return_if_fail(GWY_IS_LINE(line));
    gdouble *p = line->data + line->res-2;
    for (guint i = line->res-1; i; i--, p--)
        p[1] -= p[0];
}

/**
 * gwy_line_clear_full:
 * @line: A one-dimensional data line.
 *
 * Fills an entire line with zeroes.
 **/
void
gwy_line_clear_full(GwyLine *line)
{
    g_return_if_fail(GWY_IS_LINE(line));
    gwy_clear(line->data, line->res);
}

/**
 * gwy_line_fill_full:
 * @line: A one-dimensional data line.
 * @value: Value to fill @line with.
 *
 * Fills an entire line with the specified value.
 **/
void
gwy_line_fill_full(GwyLine *line,
                   gdouble value)
{
    if (!value) {
        gwy_line_clear_full(line);
        return;
    }
    g_return_if_fail(GWY_IS_LINE(line));
    gwy_line_fill(line, 0, line->res, value);
}

/**
 * gwy_line_clear:
 * @line: A one-dimensional data line.
 * @pos: Position of the line part start.
 * @len: Part length (number of items).
 *
 * Fills a line with zeroes.
 **/
void
gwy_line_clear(GwyLine *line,
               guint pos,
               guint len)
{
    g_return_if_fail(GWY_IS_LINE(line));
    g_return_if_fail(pos + len <= line->res);

    if (!len)
        return;

    gwy_clear(line->data + pos, len);
}

/**
 * gwy_line_fill:
 * @line: A one-dimensional data line.
 * @pos: Position of the line part start.
 * @len: Part length (number of items).
 * @value: Value to fill the rectangle with.
 *
 * Fills a line with the specified value.
 **/
void
gwy_line_fill(GwyLine *line,
              guint pos,
              guint len,
              gdouble value)
{
    if (!value) {
        gwy_line_clear(line, pos, len);
        return;
    }

    g_return_if_fail(GWY_IS_LINE(line));
    g_return_if_fail(pos + len <= line->res);

    if (!len)
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
 * SECTION: line-arithmetic
 * @section_id: GwyLine-arithmetic
 * @title: GwyLine arithmetic
 * @short_description: Arithmetic operations with lines
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
