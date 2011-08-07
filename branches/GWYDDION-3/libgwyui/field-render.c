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

#include "libgwyui/field-render.h"

void
gwy_field_render(const GwyField *field,
                 GdkPixbuf *pixbuf,
                 GwyGradient *gradient,
                 gdouble xfrom, gdouble yfrom,
                 gdouble xto, gdouble yto,
                 gdouble min, gdouble max)
{
    g_return_if_fail(GWY_IS_FIELD(field));
    g_return_if_fail(GDK_IS_PIXBUF(pixbuf));
    g_return_if_fail(GWY_IS_GRADIENT(gradient));

    if (min == max)
        max = G_MAXDOUBLE;
}

/**
 * SECTION: field-render
 * @section_id: GwyField-render
 * @title: GwyField rendering
 * @short_description: Rendering of fields to raster images
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
