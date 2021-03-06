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

#ifndef __LIBGWY_UNIT_H__
#define __LIBGWY_UNIT_H__

#include <libgwy/serializable.h>
#include <libgwy/value-format.h>

G_BEGIN_DECLS

#define GWY_TYPE_UNIT \
    (gwy_unit_get_type())
#define GWY_UNIT(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_UNIT, GwyUnit))
#define GWY_UNIT_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_UNIT, GwyUnitClass))
#define GWY_IS_UNIT(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_UNIT))
#define GWY_IS_UNIT_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_UNIT))
#define GWY_UNIT_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_UNIT, GwyUnitClass))

typedef struct _GwyUnit      GwyUnit;
typedef struct _GwyUnitClass GwyUnitClass;

struct _GwyUnit {
    GObject g_object;
    struct _GwyUnitPrivate *priv;
};

struct _GwyUnitClass {
    /*<private>*/
    GObjectClass g_object_class;
};

#define gwy_unit_duplicate(unit) \
    (GWY_UNIT(gwy_serializable_duplicate(GWY_SERIALIZABLE(unit))))
#define gwy_unit_assign(dest, src) \
    (gwy_serializable_assign(GWY_SERIALIZABLE(dest), GWY_SERIALIZABLE(src)))

GType           gwy_unit_get_type              (void)                      G_GNUC_CONST;
GwyUnit*        gwy_unit_new                   (void)                      G_GNUC_MALLOC;
GwyUnit*        gwy_unit_new_from_string       (const gchar *unit_string,
                                                gint *power10)             G_GNUC_MALLOC;
void            gwy_unit_set_from_string       (GwyUnit *unit,
                                                const gchar *unit_string,
                                                gint *power10);
gchar*          gwy_unit_to_string             (const GwyUnit *unit,
                                                GwyValueFormatStyle style) G_GNUC_MALLOC;
void            gwy_unit_multiply              (GwyUnit *unit,
                                                const GwyUnit *op1,
                                                const GwyUnit *op2);
void            gwy_unit_divide                (GwyUnit *unit,
                                                const GwyUnit *op1,
                                                const GwyUnit *op2);
void            gwy_unit_power                 (GwyUnit *unit,
                                                const GwyUnit *op,
                                                gint power);
gboolean        gwy_unit_nth_root              (GwyUnit *unit,
                                                const GwyUnit *op,
                                                gint ipower);
void            gwy_unit_power_multiply        (GwyUnit *unit,
                                                const GwyUnit *op1,
                                                gint power1,
                                                const GwyUnit *op2,
                                                gint power2);
gboolean        gwy_unit_equal                 (const GwyUnit *unit,
                                                const GwyUnit *op)         G_GNUC_PURE;
gboolean        gwy_unit_is_empty              (const GwyUnit *unit)       G_GNUC_PURE;
void            gwy_unit_clear                 (GwyUnit *unit);
void            gwy_unit_swap                  (GwyUnit *unit,
                                                GwyUnit *otherunit);
GwyValueFormat* gwy_unit_format_for_power10    (const GwyUnit *unit,
                                                GwyValueFormatStyle style,
                                                gint power10)              G_GNUC_MALLOC;
GwyValueFormat* gwy_unit_format_with_resolution(const GwyUnit *unit,
                                                GwyValueFormatStyle style,
                                                gdouble maximum,
                                                gdouble resolution)        G_GNUC_MALLOC;
GwyValueFormat* gwy_unit_format_with_digits    (const GwyUnit *unit,
                                                GwyValueFormatStyle style,
                                                gdouble value,
                                                guint sdigits)             G_GNUC_MALLOC;

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
