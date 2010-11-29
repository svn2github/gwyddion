/*
 *  $Id$
 *  Copyright (C) 2010 David Necas (Yeti).
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

#ifndef __LIBGWY_CURVE_H__
#define __LIBGWY_CURVE_H__

#include <libgwy/serializable.h>
#include <libgwy/unit.h>
#include <libgwy/math.h>

G_BEGIN_DECLS

#define GWY_TYPE_CURVE \
    (gwy_curve_get_type())
#define GWY_CURVE(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_CURVE, GwyCurve))
#define GWY_CURVE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_CURVE, GwyCurveClass))
#define GWY_IS_CURVE(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_CURVE))
#define GWY_IS_CURVE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_CURVE))
#define GWY_CURVE_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_CURVE, GwyCurveClass))

typedef struct _GwyCurve      GwyCurve;
typedef struct _GwyCurveClass GwyCurveClass;

struct _GwyCurve {
    GObject g_object;
    struct _GwyCurvePrivate *priv;
    /*<public>*/
    guint n;
    GwyXY *data;
};

struct _GwyCurveClass {
    /*<private>*/
    GObjectClass g_object_class;
};

G_END_DECLS

#include <libgwy/line.h>

G_BEGIN_DECLS

#define gwy_curve_duplicate(curve) \
        (GWY_CURVE(gwy_serializable_duplicate(GWY_SERIALIZABLE(curve))))
#define gwy_curve_assign(dest, src) \
        (gwy_serializable_assign(GWY_SERIALIZABLE(dest), GWY_SERIALIZABLE(src)))

GType           gwy_curve_get_type     (void)                      G_GNUC_CONST;
GwyCurve*       gwy_curve_new          (void)                      G_GNUC_MALLOC;
GwyCurve*       gwy_curve_new_sized    (guint n)                   G_GNUC_MALLOC;
GwyCurve*       gwy_curve_new_from_data(const GwyXY *points,
                                        guint n)                   G_GNUC_MALLOC;
GwyCurve*       gwy_curve_new_alike    (const GwyCurve *model)     G_GNUC_MALLOC;
GwyCurve*       gwy_curve_new_part     (const GwyCurve *curve,
                                        gdouble from,
                                        gdouble to)                G_GNUC_MALLOC;
GwyCurve*       gwy_curve_new_from_line(const GwyLine *line)       G_GNUC_MALLOC;
void            gwy_curve_data_changed (GwyCurve *curve);
void            gwy_curve_copy         (const GwyCurve *src,
                                        GwyCurve *dest);
void            gwy_curve_sort         (GwyCurve *curve);
void            gwy_curve_set_from_line(GwyCurve *curve,
                                        const GwyLine *line);
GwyUnit*        gwy_curve_get_unit_x   (GwyCurve *curve)           G_GNUC_PURE;
GwyUnit*        gwy_curve_get_unit_y   (GwyCurve *curve)           G_GNUC_PURE;
GwyValueFormat* gwy_curve_get_format_x (GwyCurve *curve,
                                        GwyValueFormatStyle style,
                                        GwyValueFormat *format);
GwyValueFormat* gwy_curve_get_format_y (GwyCurve *curve,
                                        GwyValueFormatStyle style,
                                        GwyValueFormat *format);

#define gwy_curve_index(curve, pos) \
    ((curve)->data[pos])

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
