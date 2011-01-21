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

#include "libgwy/macros.h"
#include "libgwy/math.h"
#include "libgwy/line-arithmetic.h"
#include "libgwy/line-internal.h"

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
