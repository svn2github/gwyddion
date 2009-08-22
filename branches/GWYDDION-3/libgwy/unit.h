/*
 *  $Id$
 *  Copyright (C) 2009 David Necas (Yeti).
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

G_BEGIN_DECLS

typedef enum {
    GWY_UNIT_FORMAT_NONE = 0,
    GWY_UNIT_FORMAT_PLAIN,
    GWY_UNIT_FORMAT_PANGO,
    GWY_UNIT_FORMAT_TEX
} GwyUnitFormatStyle;

typedef struct {
    gdouble magnitude;
    gchar *units;
    gint precision;
} GwyUnitFormat;

#define GWY_UNIT_INIT_FORMAT { 0.0, NULL, 0 }

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

    GArray *units;
    gchar *serialize_str;
    gpointer reserved1;
};

struct _GwyUnitClass {
    GObjectClass g_object_class;
};

#define gwy_unit_duplicate(unit) \
        (GWY_UNIT(gwy_serializable_duplicate(GWY_SERIALIZABLE(unit))))
#define gwy_unit_assign(dest, src) \
        (gwy_serializable_assign(GWY_SERIALIZABLE(dest), GWY_SERIALIZABLE(src))

GType    gwy_unit_get_type              (void) G_GNUC_CONST;

GwyUnit* gwy_unit_new                   (void);
GwyUnit* gwy_unit_new_from_string       (const gchar *unit_string,
                                         gint *power10);
void     gwy_unit_set_from_string       (GwyUnit *unit,
                                         const gchar *unit_string,
                                         gint *power10);
gchar*   gwy_unit_to_string             (GwyUnit *unit,
                                         GwyUnitFormatStyle style);
GwyUnit* gwy_unit_multiply              (GwyUnit *unit,
                                         GwyUnit *op1,
                                         GwyUnit *op2);
GwyUnit* gwy_unit_divide                (GwyUnit *unit,
                                         GwyUnit *op1,
                                         GwyUnit *op2);
GwyUnit* gwy_unit_power                 (GwyUnit *unit,
                                         GwyUnit *op,
                                         gint power);
GwyUnit* gwy_unit_nth_root              (GwyUnit *unit,
                                         GwyUnit *op,
                                         gint ipower);
GwyUnit* gwy_unit_power_multiply        (GwyUnit *unit,
                                         GwyUnit *op1,
                                         gint power1,
                                         GwyUnit *op2,
                                         gint power2);
gboolean gwy_unit_equal                 (GwyUnit *unit,
                                         GwyUnit *op);
void     gwy_unit_format_for_power10    (GwyUnit *unit,
                                         GwyUnitFormatStyle style,
                                         gboolean standalone,
                                         gint power10,
                                         GwyUnitFormat *format);
void     gwy_unit_format_with_resolution(GwyUnit *unit,
                                         GwyUnitFormatStyle style,
                                         gboolean standalone,
                                         gdouble maximum,
                                         gdouble resolution,
                                         GwyUnitFormat *format);
void     gwy_unit_format_with_digits    (GwyUnit *unit,
                                         GwyUnitFormatStyle style,
                                         gboolean standalone,
                                         gdouble maximum,
                                         gint sdigits,
                                         GwyUnitFormat *format);
void     gwy_unit_clear_format          (GwyUnitFormat *format);
gint     gwy_unit_format_snprint        (gchar *string,
                                         gsize size,
                                         const GwyUnitFormat *format,
                                         gdouble value);
gchar*   gwy_unit_format_strdup_print   (const GwyUnitFormat *format,
                                         gdouble value);
void     gwy_unit_format_gstring_print  (GString *string,
                                         const GwyUnitFormat *format,
                                         gdouble value);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
