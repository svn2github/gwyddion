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

#ifndef __LIBGWY_SURFACE_H__
#define __LIBGWY_SURFACE_H__

#include <libgwy/serializable.h>
#include <libgwy/unit.h>
#include <libgwy/math.h>
#include <libgwy/field.h>

G_BEGIN_DECLS

typedef enum {
    GWY_SURFACE_REGULARIZE_PREVIEW,
} GwySurfaceRegularizeType;

#define GWY_TYPE_SURFACE \
    (gwy_surface_get_type())
#define GWY_SURFACE(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_SURFACE, GwySurface))
#define GWY_SURFACE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_SURFACE, GwySurfaceClass))
#define GWY_IS_SURFACE(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_SURFACE))
#define GWY_IS_SURFACE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_SURFACE))
#define GWY_SURFACE_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_SURFACE, GwySurfaceClass))

typedef struct _GwySurface      GwySurface;
typedef struct _GwySurfaceClass GwySurfaceClass;

struct _GwySurface {
    GObject g_object;
    struct _GwySurfacePrivate *priv;
    /*<public>*/
    guint n;
    GwyXYZ *data;
};

struct _GwySurfaceClass {
    /*<private>*/
    GObjectClass g_object_class;
};

#define gwy_surface_duplicate(surface) \
        (GWY_SURFACE(gwy_serializable_duplicate(GWY_SERIALIZABLE(surface))))
#define gwy_surface_assign(dest, src) \
        (gwy_serializable_assign(GWY_SERIALIZABLE(dest), GWY_SERIALIZABLE(src)))

GType           gwy_surface_get_type       (void)                      G_GNUC_CONST;
GwySurface*     gwy_surface_new            (void)                      G_GNUC_MALLOC;
GwySurface*     gwy_surface_new_sized      (guint n)                   G_GNUC_MALLOC;
GwySurface*     gwy_surface_new_from_data  (const GwyXYZ *points,
                                            guint n)                   G_GNUC_MALLOC;
GwySurface*     gwy_surface_new_alike      (const GwySurface *model)   G_GNUC_MALLOC;
GwySurface*     gwy_surface_new_part       (const GwySurface *surface,
                                            gdouble xfrom,
                                            gdouble xto,
                                            gdouble yfrom,
                                            gdouble yto)               G_GNUC_MALLOC;
GwySurface*     gwy_surface_new_from_field (const GwyField *field)     G_GNUC_MALLOC;
void            gwy_surface_data_changed   (GwySurface *surface);
void            gwy_surface_copy           (const GwySurface *src,
                                            GwySurface *dest);
void            gwy_surface_invalidate     (GwySurface *surface);
void            gwy_surface_set_from_field (GwySurface *surface,
                                            const GwyField *field);
GwyField*       gwy_surface_regularize_full(const GwySurface *surface,
                                            GwySurfaceRegularizeType method,
                                            guint xres,
                                            guint yres)                G_GNUC_MALLOC;
GwyField*       gwy_surface_regularize     (const GwySurface *surface,
                                            GwySurfaceRegularizeType method,
                                            gdouble xfrom,
                                            gdouble xto,
                                            gdouble yfrom,
                                            gdouble yto,
                                            guint xres,
                                            guint yres)                G_GNUC_MALLOC;
GwyUnit*        gwy_surface_get_unit_xy    (GwySurface *surface)       G_GNUC_PURE;
GwyUnit*        gwy_surface_get_unit_z     (GwySurface *surface)       G_GNUC_PURE;
GwyValueFormat* gwy_surface_format_xy      (GwySurface *surface,
                                            GwyValueFormatStyle style) G_GNUC_MALLOC;
GwyValueFormat* gwy_surface_format_z       (GwySurface *surface,
                                            GwyValueFormatStyle style) G_GNUC_MALLOC;

#define gwy_surface_index(surface, pos) \
    ((surface)->data[pos])

GwyXYZ gwy_surface_get(const GwySurface *surface,
                       guint pos);
void   gwy_surface_set(const GwySurface *surface,
                       guint pos,
                       GwyXYZ point);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
