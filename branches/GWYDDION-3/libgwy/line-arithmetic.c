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
    gdouble *p = line->data + line->res-1;
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
 * SECTION: line-arithmetic
 * @section_id: GwyLine-arithmetic
 * @title: GwyLine arithmetic
 * @short_description: Arithmetic operations with lines
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
