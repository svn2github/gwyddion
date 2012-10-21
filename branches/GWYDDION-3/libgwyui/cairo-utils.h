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

#ifndef __LIBGWYUI_UTILS_H__
#define __LIBGWYUI_UTILS_H__

#include <libgwy/rgba.h>
#include <cairo/cairo.h>

G_BEGIN_DECLS

void gwy_cairo_set_source_rgba(cairo_t *cr,
                               const GwyRGBA *rgba);
void gwy_cairo_ellipse        (cairo_t *cr,
                               gdouble x,
                               gdouble y,
                               gdouble xr,
                               gdouble yr);
void gwy_cairo_cross          (cairo_t *cr,
                               gdouble x,
                               gdouble y,
                               gdouble halfside);
void gwy_cairo_times          (cairo_t *cr,
                               gdouble x,
                               gdouble y,
                               gdouble halfside);
void gwy_cairo_asterisk       (cairo_t *cr,
                               gdouble x,
                               gdouble y,
                               gdouble halfside);
void gwy_cairo_square         (cairo_t *cr,
                               gdouble x,
                               gdouble y,
                               gdouble halfside);
void gwy_cairo_diamond        (cairo_t *cr,
                               gdouble x,
                               gdouble y,
                               gdouble halfside);
void gwy_cairo_triangle_up    (cairo_t *cr,
                               gdouble x,
                               gdouble y,
                               gdouble halfside);
void gwy_cairo_triangle_down  (cairo_t *cr,
                               gdouble x,
                               gdouble y,
                               gdouble halfside);
void gwy_cairo_triangle_left  (cairo_t *cr,
                               gdouble x,
                               gdouble y,
                               gdouble halfside);
void gwy_cairo_triangle_right (cairo_t *cr,
                               gdouble x,
                               gdouble y,
                               gdouble halfside);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
