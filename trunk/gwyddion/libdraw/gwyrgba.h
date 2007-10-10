/*
 *  @(#) $Id$
 *  Copyright (C) 2004 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 */

#ifndef __GWY_RGBA_H__
#define __GWY_RGBA_H__

#include <gdk/gdkgc.h>
#include <libgwyddion/gwycontainer.h>

G_BEGIN_DECLS

#define GWY_TYPE_RGBA                         (gwy_rgba_get_type())

typedef struct {
    gdouble r;
    gdouble g;
    gdouble b;
    gdouble a;
} GwyRGBA;

GType         gwy_rgba_get_type                 (void) G_GNUC_CONST;
GwyRGBA*      gwy_rgba_copy                     (const GwyRGBA *rgba);
void          gwy_rgba_free                     (GwyRGBA *rgba);
void          gwy_rgba_to_gdk_color             (const GwyRGBA *rgba,
                                                 GdkColor *gdkcolor);
guint16       gwy_rgba_to_gdk_alpha             (const GwyRGBA *rgba);
void          gwy_rgba_from_gdk_color           (GwyRGBA *rgba,
                                                 const GdkColor *gdkcolor);
void          gwy_rgba_from_gdk_color_and_alpha (GwyRGBA *rgba,
                                                 const GdkColor *gdkcolor,
                                                 guint16 gdkalpha);
void          gwy_rgba_interpolate              (const GwyRGBA *src1,
                                                 const GwyRGBA *src2,
                                                 gdouble x,
                                                 GwyRGBA *rgba);
gboolean      gwy_rgba_get_from_container       (GwyRGBA *rgba,
                                                 GwyContainer *container,
                                                 const gchar *prefix);
void          gwy_rgba_store_to_container       (const GwyRGBA *rgba,
                                                 GwyContainer *container,
                                                 const gchar *prefix);
gboolean      gwy_rgba_remove_from_container    (GwyContainer *container,
                                                 const gchar *prefix);
void          gwy_rgba_set_gdk_gc_fg            (const GwyRGBA *rgba,
                                                 GdkGC *gc);
void          gwy_rgba_set_gdk_gc_bg            (const GwyRGBA *rgba,
                                                 GdkGC *gc);

G_END_DECLS

#endif /* __GWY_RGBA_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
