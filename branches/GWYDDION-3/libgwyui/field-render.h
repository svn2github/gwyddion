/*
 *  $Id$
 *  Copyright (C) 2011 David Nečas (Yeti).
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

#ifndef __LIBGWYUI_FIELD_RENDER_H__
#define __LIBGWYUI_FIELD_RENDER_H__

#include <libgwy/field.h>
#include <libgwy/gradient.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <cairo/cairo.h>

G_BEGIN_DECLS

void gwy_field_render_pixbuf(const GwyField *field,
                             GdkPixbuf *pixbuf,
                             GwyGradient *gradient,
                             gdouble xfrom,
                             gdouble yfrom,
                             gdouble xto,
                             gdouble yto,
                             gdouble min,
                             gdouble max);
void gwy_field_render_cairo (const GwyField *field,
                             cairo_surface_t *surface,
                             GwyGradient *gradient,
                             gdouble xfrom,
                             gdouble yfrom,
                             gdouble xto,
                             gdouble yto,
                             gdouble min,
                             gdouble max);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
