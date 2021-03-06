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

#ifndef __LIBGWY_GRADIENT_H__
#define __LIBGWY_GRADIENT_H__

#include <libgwy/rgba.h>
#include <libgwy/serializable.h>
#include <libgwy/resource.h>

G_BEGIN_DECLS

#define GWY_GRADIENT_DEFAULT "Gray"

#define GWY_TYPE_GRADIENT \
    (gwy_gradient_get_type())
#define GWY_GRADIENT(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_GRADIENT, GwyGradient))
#define GWY_GRADIENT_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_GRADIENT, GwyGradientClass))
#define GWY_IS_GRADIENT(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_GRADIENT))
#define GWY_IS_GRADIENT_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_GRADIENT))
#define GWY_GRADIENT_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_GRADIENT, GwyGradientClass))

typedef struct {
    gdouble x;
    GwyRGBA color;
} GwyGradientPoint;

typedef struct _GwyGradient      GwyGradient;
typedef struct _GwyGradientClass GwyGradientClass;

struct _GwyGradient {
    GwyResource resource;
    struct _GwyGradientPrivate *priv;
};

struct _GwyGradientClass {
    /*<private>*/
    GwyResourceClass resource_class;
};

#define gwy_gradient_duplicate(gradient) \
    (GWY_GRADIENT(gwy_serializable_duplicate(GWY_SERIALIZABLE(gradient))))
#define gwy_gradient_assign(dest, src) \
    (gwy_serializable_assign(GWY_SERIALIZABLE(dest), GWY_SERIALIZABLE(src)))

#define GWY_TYPE_GRADIENT_POINT (gwy_gradient_point_get_type())

GType             gwy_gradient_point_get_type(void)                          G_GNUC_CONST;
GwyGradientPoint* gwy_gradient_point_copy    (const GwyGradientPoint *point) G_GNUC_MALLOC;
void              gwy_gradient_point_free    (GwyGradientPoint *point);

GType                   gwy_gradient_get_type        (void)                            G_GNUC_CONST;
GwyGradient*            gwy_gradient_new             (void)                            G_GNUC_MALLOC;
void                    gwy_gradient_color           (const GwyGradient *gradient,
                                                      gdouble x,
                                                      GwyRGBA *color);
guint                   gwy_gradient_n_points        (const GwyGradient *gradient)     G_GNUC_PURE;
GwyGradientPoint        gwy_gradient_get             (const GwyGradient *gradient,
                                                      guint n)                         G_GNUC_PURE;
void                    gwy_gradient_set             (GwyGradient *gradient,
                                                      guint n,
                                                      const GwyGradientPoint *point);
void                    gwy_gradient_set_color       (GwyGradient *gradient,
                                                      guint n,
                                                      const GwyRGBA *color);
void                    gwy_gradient_insert          (GwyGradient *gradient,
                                                      guint n,
                                                      const GwyGradientPoint *point);
guint                   gwy_gradient_insert_sorted   (GwyGradient *gradient,
                                                      const GwyGradientPoint *point);
void                    gwy_gradient_delete          (GwyGradient *gradient,
                                                      guint n);
const GwyGradientPoint* gwy_gradient_get_data        (const GwyGradient *gradient,
                                                      guint *npoints);
void                    gwy_gradient_set_data        (GwyGradient *gradient,
                                                      guint npoints,
                                                      const GwyGradientPoint *points);
void                    gwy_gradient_set_from_samples(GwyGradient *gradient,
                                                      guint nsamples,
                                                      const guchar *samples,
                                                      gdouble threshold);

#define gwy_gradients() \
    (gwy_resource_type_get_inventory(GWY_TYPE_GRADIENT))
#define gwy_gradients_get(name) \
    ((GwyGradient*)gwy_inventory_get_or_default(gwy_gradients(), (name)))

/* FIXME: The jury is still out on the pixel format.  Generally, we want to use
 * the Cairo format but avoiding GdkPixbuf althogether might not be possible
 * or wise. */
// const guchar*     gwy_gradient_get_samples           (GwyGradient *gradient,
//                                                       gint *nsamples);
// guchar*           gwy_gradient_sample                (GwyGradient *gradient,
//                                                       gint nsamples,
//                                                       guchar *samples);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
