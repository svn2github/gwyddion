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

#ifndef __LIBGWY_VALUE_FORMAT_H__
#define __LIBGWY_VALUE_FORMAT_H__

#include <glib-object.h>

G_BEGIN_DECLS

typedef enum {
    GWY_VALUE_FORMAT_NONE = 0,
    GWY_VALUE_FORMAT_PLAIN,
    GWY_VALUE_FORMAT_UNICODE,
    GWY_VALUE_FORMAT_PANGO,
    GWY_VALUE_FORMAT_TEX
} GwyValueFormatStyle;

#define GWY_TYPE_VALUE_FORMAT \
    (gwy_value_format_get_type())
#define GWY_VALUE_FORMAT(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_VALUE_FORMAT, GwyValueFormat))
#define GWY_VALUE_FORMAT_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_VALUE_FORMAT, GwyValueFormatClass))
#define GWY_IS_VALUE_FORMAT(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_VALUE_FORMAT))
#define GWY_IS_VALUE_FORMAT_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_VALUE_FORMAT))
#define GWY_VALUE_FORMAT_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_VALUE_FORMAT, GwyValueFormatClass))

typedef struct _GwyValueFormat      GwyValueFormat;
typedef struct _GwyValueFormatClass GwyValueFormatClass;

struct _GwyValueFormat {
    GObject g_object;
    struct _GwyValueFormatPrivate *priv;
};

struct _GwyValueFormatClass {
    /*<private>*/
    GObjectClass g_object_class;
};

GType           gwy_value_format_get_type    (void)                      G_GNUC_CONST;
GwyValueFormat* gwy_value_format_new         (void)                      G_GNUC_MALLOC;
GwyValueFormat* gwy_value_format_new_set     (GwyValueFormatStyle style,
                                              gint power10,
                                              guint precision,
                                              const gchar *glue,
                                              const gchar *units)        G_GNUC_MALLOC;
const gchar*    gwy_value_format_print       (GwyValueFormat *format,
                                              gdouble value);
const gchar*    gwy_value_format_print_number(GwyValueFormat *format,
                                              gdouble value);
const gchar*    gwy_value_format_get_units   (GwyValueFormat *format)    G_GNUC_PURE;
void            gwy_value_format_set_units   (GwyValueFormat *format,
                                              const gchar *units);
const gchar*    gwy_value_format_get_glue    (GwyValueFormat *format)    G_GNUC_PURE;
void            gwy_value_format_set_glue    (GwyValueFormat *format,
                                              const gchar *glue);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

