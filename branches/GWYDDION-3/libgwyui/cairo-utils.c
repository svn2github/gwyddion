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
#include "libgwy/math.h"
#include "libgwyui/cairo-utils.h"

// TODO: Create a table with factors describing how to scale various
// geometrical shapes in order to get *visually* uniform sizes.  For instance,
// times has √2 larger arms than cross while diamon has only half of the
// area of a square.  The primitives should be kept as-is (unscaled) because
// straightforward interpretation of parameters can also be useful.  The
// hight level interface may just take a symbol name/enum and visual size and
// draw it because all the functions look the same.

/**
 * gwy_cairo_set_source_rgba:
 * @cr: A Cairo drawing context.
 * @rgba: An RGBA colour.
 *
 * Sets the source patter of a Cairo context to #GwyRGBA color.
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
 * @cr: A Cairo drawing context.
 * @x: Centre x-coordinate.
 * @y: Centre y-coordinate.
 * @xr: Horizontal half-axis length.
 * @yr: Vertical half-axis length.
 *
 * Adds a closed elliptical subpath to a Cairo context.
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
 * @cr: A Cairo drawing context.
 * @x: Centre x-coordinate.
 * @y: Centre y-coordinate.
 * @halfside: Length of the cross arm (from centre to end), i.e. half of the
 *            side of the smallest square containing the cross.
 *
 * Adds a cross-shaped subpath to a Cairo context.
 *
 * A new subpath is started, but it is not terminated.  Use
 * cairo_new_sub_path() or other subpath-terminating primitive afterwards if
 * necessary.  All parameters are in user coordinates.
 **/
void
gwy_cairo_cross(cairo_t *cr,
                gdouble x, gdouble y, gdouble halfside)
{
    cairo_move_to(cr, x - halfside, y);
    cairo_line_to(cr, x + halfside, y);
    cairo_move_to(cr, x, y - halfside);
    cairo_line_to(cr, x, y + halfside);
}

/**
 * gwy_cairo_times:
 * @cr: A Cairo drawing context.
 * @x: Centre x-coordinate.
 * @y: Centre y-coordinate.
 * @halfside: Half of the side of the smallest square containing the cross.
 *
 * Adds a times-shaped subpath to a Cairo context.
 *
 * A new subpath is started, but it is not terminated.  Use
 * cairo_new_sub_path() or other subpath-terminating primitive afterwards if
 * necessary.  All parameters are in user coordinates.
 **/
void
gwy_cairo_times(cairo_t *cr,
                gdouble x, gdouble y, gdouble halfside)
{
    cairo_move_to(cr, x - halfside, y - halfside);
    cairo_line_to(cr, x + halfside, y + halfside);
    cairo_move_to(cr, x + halfside, y - halfside);
    cairo_line_to(cr, x - halfside, y + halfside);
}

/**
 * gwy_cairo_square:
 * @cr: A Cairo drawing context.
 * @x: Centre x-coordinate.
 * @y: Centre y-coordinate.
 * @halfside: Half of the side of the square.
 *
 * Adds a square-shaped subpath to a Cairo context.
 *
 * A new subpath is started and closed.  All parameters are in user
 * coordinates.
 **/
void
gwy_cairo_square(cairo_t *cr,
                 gdouble x, gdouble y, gdouble halfside)
{
    cairo_move_to(cr, x - halfside, y - halfside);
    cairo_line_to(cr, x + halfside, y - halfside);
    cairo_line_to(cr, x + halfside, y + halfside);
    cairo_line_to(cr, x - halfside, y + halfside);
    cairo_close_path(cr);
}

/**
 * gwy_cairo_diamond:
 * @cr: A Cairo drawing context.
 * @x: Centre x-coordinate.
 * @y: Centre y-coordinate.
 * @halfside: Half of the side of the containing square.
 *
 * Adds a diamond-shaped subpath to a Cairo context.
 *
 * A new subpath is started and closed.  All parameters are in user
 * coordinates.
 **/
void
gwy_cairo_diamond(cairo_t *cr,
                  gdouble x, gdouble y, gdouble halfside)
{
    cairo_move_to(cr, x, y - halfside);
    cairo_line_to(cr, x + halfside, y);
    cairo_line_to(cr, x, y + halfside);
    cairo_line_to(cr, x - halfside, y);
    cairo_close_path(cr);
}

/**
 * gwy_cairo_triangle_up:
 * @cr: A Cairo drawing context.
 * @x: Centre x-coordinate.
 * @y: Centre y-coordinate.
 * @halfside: Half of the side of the containing square.
 *
 * Adds an upward pointing triangle-shaped subpath to a Cairo context.
 *
 * A new subpath is started and closed.  All parameters are in user
 * coordinates.
 **/
void
gwy_cairo_triangle_up(cairo_t *cr,
                      gdouble x, gdouble y, gdouble halfside)
{
    cairo_move_to(cr, x, y - halfside);
    cairo_line_to(cr, x - halfside, y + halfside);
    cairo_line_to(cr, x + halfside, y + halfside);
    cairo_close_path(cr);
}

/**
 * gwy_cairo_triangle_down:
 * @cr: A Cairo drawing context.
 * @x: Centre x-coordinate.
 * @y: Centre y-coordinate.
 * @halfside: Half of the side of the containing square.
 *
 * Adds a downward pointing triangle-shaped subpath to a Cairo context.
 *
 * A new subpath is started and closed.  All parameters are in user
 * coordinates.
 **/
void
gwy_cairo_triangle_down(cairo_t *cr,
                        gdouble x, gdouble y, gdouble halfside)
{
    cairo_move_to(cr, x, y + halfside);
    cairo_line_to(cr, x + halfside, y - halfside);
    cairo_line_to(cr, x - halfside, y - halfside);
    cairo_close_path(cr);
}

/**
 * gwy_cairo_triangle_left:
 * @cr: A Cairo drawing context.
 * @x: Centre x-coordinate.
 * @y: Centre y-coordinate.
 * @halfside: Half of the side of the containing square.
 *
 * Adds a leftward pointing triangle-shaped subpath to a Cairo context.
 *
 * A new subpath is started and closed.  All parameters are in user
 * coordinates.
 **/
void
gwy_cairo_triangle_left(cairo_t *cr,
                        gdouble x, gdouble y, gdouble halfside)
{
    cairo_move_to(cr, x - halfside, y);
    cairo_line_to(cr, x + halfside, y + halfside);
    cairo_line_to(cr, x + halfside, y - halfside);
    cairo_close_path(cr);
}

/**
 * gwy_cairo_triangle_right:
 * @cr: A Cairo drawing context.
 * @x: Centre x-coordinate.
 * @y: Centre y-coordinate.
 * @halfside: Half of the side of the containing square.
 *
 * Adds a rightward pointing triangle-shaped subpath to a Cairo context.
 *
 * A new subpath is started and closed.  All parameters are in user
 * coordinates.
 **/
void
gwy_cairo_triangle_right(cairo_t *cr,
                         gdouble x, gdouble y, gdouble halfside)
{
    cairo_move_to(cr, x + halfside, y);
    cairo_line_to(cr, x - halfside, y - halfside);
    cairo_line_to(cr, x - halfside, y + halfside);
    cairo_close_path(cr);
}

/**
 * gwy_cairo_asterisk:
 * @cr: A Cairo drawing context.
 * @x: Centre x-coordinate.
 * @y: Centre y-coordinate.
 * @halfside: Half of the side of the containing square.
 *
 * Adds a rightward pointing triangle-shaped subpath to a Cairo context.
 *
 * A new subpath is started, but it is not terminated.  Use
 * cairo_new_sub_path() or other subpath-terminating primitive afterwards if
 * necessary.  All parameters are in user coordinates.
 **/
void
gwy_cairo_asterisk(cairo_t *cr,
                   gdouble x, gdouble y, gdouble halfside)
{
    gdouble qside = 0.5*halfside, dside = GWY_SQRT3*qside;
    cairo_move_to(cr, x, y - halfside);
    cairo_line_to(cr, x, y + halfside);
    cairo_move_to(cr, x + dside, y + qside);
    cairo_line_to(cr, x - dside, y - qside);
    cairo_move_to(cr, x - dside, y + qside);
    cairo_line_to(cr, x + dside, y - qside);
}

/**
 * SECTION: cairo-utils
 * @section_id: libgwyui-Cairo-utils
 * @title: Cairo drawing utils
 * @short_description: Auxiliary and impedance matching functions for Cairo
 *                     drawing
 *
 * Drawing primitives for simple geometrical shapes such as gwy_cairo_cross()
 * or gwy_cairo_trangle_up() are namely useful for drawing markes and symbols
 * on graphs because they are visually centered on given coordinates.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
