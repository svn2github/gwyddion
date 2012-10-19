/*
 *  $Id$
 *  Copyright (C) 2009,2012 David Nečas (Yeti).
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

#ifndef __LIBGWY_RGBA_H__
#define __LIBGWY_RGBA_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define GWY_TYPE_RGBA                         (gwy_rgba_get_type())

typedef struct {
    gdouble r;
    gdouble g;
    gdouble b;
    gdouble a;
} GwyRGBA;

GType          gwy_rgba_get_type        (void)                G_GNUC_CONST;
GwyRGBA*       gwy_rgba_copy            (const GwyRGBA *rgba) G_GNUC_MALLOC;
void           gwy_rgba_free            (GwyRGBA *rgba);
gboolean       gwy_rgba_fix             (GwyRGBA *rgba);
void           gwy_rgba_interpolate     (const GwyRGBA *src1,
                                         const GwyRGBA *src2,
                                         gdouble x,
                                         GwyRGBA *rgba);
void           gwy_rgba_preset_color    (GwyRGBA *rgba,
                                         guint i);
const GwyRGBA* gwy_rgba_get_preset_color(guint i)             G_GNUC_CONST;
guint          gwy_rgba_n_preset_colors (void)                G_GNUC_CONST;

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
