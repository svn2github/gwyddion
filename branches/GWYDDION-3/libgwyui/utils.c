/*
 *  $Id$
 *  Copyright (C) 2012 David Nečas (Yeti).
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
#include "libgwyui/utils.h"

/**
 * gwy_cairo_set_source_rgba:
 * @cr: A cairo drawing context.
 * @rgba: An RGBA colour.
 *
 * Sets the source patter of a cairo context to #GwyRGBA color.
 *
 * This is a convenience function that is exactly equivalent to
 * |[
 * cairo_set_source_rgba(cr, rgba->r, rgba->g, rgba->b, rgba->a);
 * ]|
 **/
void
gwy_cairo_set_source_rgba(cairo_t *cr,
                          const GwyRGBA *rgba)
{
    cairo_set_source_rgba(cr, rgba->r, rgba->g, rgba->b, rgba->a);
}

/**
 * gwy_cairo_ellipse:
 * @cr: A cairo drawing context.
 * @x: Centre x-coordinate.
 * @y: Centre y-coordinate.
 * @xr: Horizontal half-axis length.
 * @yr: Vertical half-axis length.
 *
 * Adds a closed elliptical subpath to a cairo context.
 *
 * A new subpath is started and closed.  The ellipse is approximated using
 * Bezier curves, on the other hand, it does not rely on transformations.  All
 * parameters are in user coordinates.
 **/
void
gwy_cairo_ellipse(cairo_t *cr,
                  gdouble x, gdouble y, gdouble xr, gdouble yr)
{
    const gdouble q = 0.552;

    cairo_move_to(cr, x + xr, y);
    cairo_curve_to(cr, x + xr, y + q*yr, x + q*xr, y + yr, x, y + yr);
    cairo_curve_to(cr, x - q*xr, y + yr, x - xr, y + q*yr, x - xr, y);
    cairo_curve_to(cr, x - xr, y - q*yr, x - q*xr, y - yr, x, y - yr);
    cairo_curve_to(cr, x + q*xr, y - yr, x + xr, y - q*yr, x + xr, y);
    cairo_close_path(cr);
}

/**
 * gwy_cairo_cross:
 * @cr: A cairo drawing context.
 * @x: Centre x-coordinate.
 * @y: Centre y-coordinate.
 * @ticklen: Length of the cross arm (from centre to end).
 *
 * Adds a cross-shaped subpath to a cairo context.
 *
 * A new subpath is started, but it is not terminated.  Use cairo_new_subpath()
 * or other subpath-terminating primitive afterwards if necessary.  All
 * parameters are in user coordinates.
 **/
void
gwy_cairo_cross(cairo_t *cr,
                gdouble x, gdouble y, gdouble ticklen)
{
    cairo_move_to(cr, x - ticklen, y);
    cairo_line_to(cr, x + ticklen, y);
    cairo_move_to(cr, x, y - ticklen);
    cairo_line_to(cr, x, y + ticklen);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
