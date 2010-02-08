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

#include <gwyconfig.h>
#include <string.h>
#include <stdlib.h>
#include "libgwy/macros.h"
#include "libgwy/math.h"
#include "libgwy/serialize.h"
#include "libgwy/strfuncs.h"
#include "libgwy/types.h"
#include "libgwy/value-format.h"
#include "libgwy/libgwy-aliases.h"
#include "libgwy/object-internal.h"

enum {
    PROP_0,
    PROP_STYLE,
    PROP_BASE,
    PROP_PRECISION,
    PROP_GLUE,
    PROP_UNITS,
    N_PROPS
};

struct _GwyValueFormatPrivate {
    guint precision;
    gdouble base;
    gchar *glue;
    gchar *units;
    GString *value;
    GwyValueFormatStyle style;
};

typedef struct _GwyValueFormatPrivate ValueFormat;

static void gwy_value_format_finalize    (GObject *object);
static void gwy_value_format_set_property(GObject *object,
                                          guint prop_id,
                                          const GValue *value,
                                          GParamSpec *pspec);
static void gwy_value_format_get_property(GObject *object,
                                          guint prop_id,
                                          GValue *value,
                                          GParamSpec *pspec);
static void ensure_value                 (ValueFormat *format);
static void fix_utf8_minus               (GString *str);

G_DEFINE_TYPE(GwyValueFormat, gwy_value_format, G_TYPE_OBJECT)

static void
gwy_value_format_class_init(GwyValueFormatClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    g_type_class_add_private(klass, sizeof(ValueFormat));

    gobject_class->finalize = gwy_value_format_finalize;
    gobject_class->get_property = gwy_value_format_get_property;
    gobject_class->set_property = gwy_value_format_set_property;

    g_object_class_install_property
        (gobject_class,
         PROP_STYLE,
         g_param_spec_enum("style",
                           "Value format style",
                           "What output style is this format intended to be "
                           "used with.",
                           GWY_TYPE_VALUE_FORMAT_STYLE, GWY_VALUE_FORMAT_PLAIN,
                           G_PARAM_READWRITE | STATICP));

    g_object_class_install_property
        (gobject_class,
         PROP_BASE,
         g_param_spec_double("base",
                             "Base value",
                             "Factor to divide the formatted number with, "
                             "usualy a power of 1000.",
                             G_MINDOUBLE, G_MAXDOUBLE, 1.0,
                             G_PARAM_READWRITE | STATICP));

    g_object_class_install_property
        (gobject_class,
         PROP_PRECISION,
         g_param_spec_uint("precision",
                           "Precision",
                           "Number of digits after the decimal point.",
                           0, 1024, 3,
                           G_PARAM_READWRITE | STATICP));

    g_object_class_install_property
        (gobject_class,
         PROP_UNITS,
         g_param_spec_string("units",
                             "Units",
                             "Units appended to the formatted number, "
                             "including the corresponding prefix.  May also be "
                             "just a power of 10 or nothing.",
                             NULL,
                             G_PARAM_READWRITE | STATICP));

    g_object_class_install_property
        (gobject_class,
         PROP_GLUE,
         g_param_spec_string("glue",
                             "Glue between number and units",
                             "String put between the number and units when "
                             "a value is formatted.  Usually a kind of "
                             "multiplication symbol, space, may also be empty.",
                             NULL,
                             G_PARAM_READWRITE | STATICP));
}

static void
gwy_value_format_init(GwyValueFormat *format)
{
    format->priv = G_TYPE_INSTANCE_GET_PRIVATE(format, GWY_TYPE_VALUE_FORMAT,
                                               ValueFormat);
    format->priv->style = GWY_VALUE_FORMAT_PLAIN;
    format->priv->base = 1.0;
    format->priv->precision = 3;
}

static void
gwy_value_format_finalize(GObject *object)
{
    GwyValueFormat *format = (GwyValueFormat*)object;
    ValueFormat *priv = format->priv;
    GWY_FREE(priv->units);
    GWY_FREE(priv->glue);
    GWY_STRING_FREE(priv->value);
    G_OBJECT_CLASS(gwy_value_format_parent_class)->finalize(object);
}

static void
gwy_value_format_set_property(GObject *object,
                              guint prop_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
    GwyValueFormat *format = GWY_VALUE_FORMAT(object);
    ValueFormat *priv = format->priv;

    switch (prop_id) {
        case PROP_STYLE:
        priv->style = g_value_get_enum(value);
        break;

        case PROP_BASE:
        priv->base = g_value_get_double(value);
        break;

        case PROP_PRECISION:
        priv->precision = g_value_get_uint(value);
        break;

        case PROP_GLUE:
        gwy_value_format_set_glue(format, g_value_get_string(value));
        break;

        case PROP_UNITS:
        gwy_value_format_set_units(format, g_value_get_string(value));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_value_format_get_property(GObject *object,
                              guint prop_id,
                              GValue *value,
                              GParamSpec *pspec)
{
    GwyValueFormat *format = GWY_VALUE_FORMAT(object);
    ValueFormat *priv = format->priv;

    switch (prop_id) {
        case PROP_STYLE:
        g_value_set_enum(value, priv->style);
        break;

        case PROP_BASE:
        g_value_set_double(value, priv->base);
        break;

        case PROP_PRECISION:
        g_value_set_uint(value, priv->precision);
        break;

        case PROP_GLUE:
        g_value_set_string(value, priv->glue);
        break;

        case PROP_UNITS:
        g_value_set_string(value, priv->units);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

/**
 * gwy_value_format_new:
 *
 * Creates a new value format.
 *
 * Returns: A new value format.
 **/
GwyValueFormat*
gwy_value_format_new(void)
{
    return g_object_newv(GWY_TYPE_VALUE_FORMAT, 0, NULL);
}

/**
 * gwy_value_format_new_set:
 * @style: Output format style.
 * @power10: Power of 10 to use for base.
 * @precision: Number of digits after decimal point.
 * @glue: String between value and units.
 * @units: Units.
 *
 * Creates a new value format with specified properties.
 *
 * Returns: A new value format.
 **/
GwyValueFormat*
gwy_value_format_new_set(GwyValueFormatStyle style,
                         gint power10,
                         guint precision,
                         const gchar *glue,
                         const gchar *units)
{
    GwyValueFormat *format = g_object_newv(GWY_TYPE_VALUE_FORMAT, 0, NULL);
    ValueFormat *priv = format->priv;

    priv->style = style;
    priv->base = gwy_exp10(power10);
    priv->precision = precision;
    priv->glue = g_strdup(glue);
    priv->units = g_strdup(units);

    return format;
}

/**
 * gwy_value_format_print:
 * @format: Value format.
 * @value: Value to format using @format.
 *
 * Formats a value using given format, with units appended.
 *
 * Returns: The formatted value as a string owned by @format.  It remains valid
 *          until the next formatting performed with @format call or until
 *          @format is destroyed.
 **/
const gchar*
gwy_value_format_print(GwyValueFormat *format,
                       gdouble value)
{
    g_return_val_if_fail(GWY_IS_VALUE_FORMAT(format), NULL);
    ValueFormat *priv = format->priv;

    ensure_value(priv);
    g_string_printf(priv->value, "%.*f%s%s",
                    priv->precision, value/priv->base,
                    priv->glue ? priv->glue : "",
                    priv->units ? priv->units : "");

    if (priv->style == GWY_VALUE_FORMAT_PANGO)
        fix_utf8_minus(priv->value);

    return priv->value->str;
}

/**
 * gwy_value_format_print_number:
 * @format: Value format.
 * @value: Value to format using @format.
 *
 * Formats a value using given format.
 *
 * Neither the units nor glue are included in the formatted value.  This is
 * useful if the units are displayed elsewhere, e.g. in a table header.
 *
 * Returns: The formatted value as a string owned by @format.  It remains valid
 *          until the next formatting performed with @format call or until
 *          @format is destroyed.
 **/
const gchar*
gwy_value_format_print_number(GwyValueFormat *format,
                              gdouble value)
{
    g_return_val_if_fail(GWY_IS_VALUE_FORMAT(format), NULL);
    ValueFormat *priv = format->priv;

    ensure_value(priv);
    g_string_printf(priv->value, "%.*f",
                    priv->precision, value/priv->base);

    if (priv->style == GWY_VALUE_FORMAT_PANGO)
        fix_utf8_minus(priv->value);

    return priv->value->str;
}

/**
 * gwy_value_format_get_units:
 * @format: Value format.
 *
 * Gets the units of a value format.
 *
 * Returns: The units as a string owned by @format.  It can be %NULL.
 **/
const gchar*
gwy_value_format_get_units(GwyValueFormat *format)
{
    g_return_val_if_fail(GWY_IS_VALUE_FORMAT(format), NULL);
    return format->priv->units;
}

/**
 * gwy_value_format_set_units:
 * @format: Value format.
 * @units: New units.  Empty units can be represented as %NULL, in fact, this
 *         is preferable to empty string.
 *
 * Sets the units of a value format.
 **/
void
gwy_value_format_set_units(GwyValueFormat *format,
                           const gchar *units)
{
    g_return_if_fail(GWY_IS_VALUE_FORMAT(format));
    ValueFormat *priv = format->priv;
    if (_gwy_assign_string(&priv->units, units)) {
        // TODO: Emit something
    }
}

/**
 * gwy_value_format_get_glue:
 * @format: Value format.
 *
 * Gets the string between number and units of a value format.
 *
 * Returns: The glue as a string owned by @format.  It can be %NULL.
 **/
const gchar*
gwy_value_format_get_glue(GwyValueFormat *format)
{
    g_return_val_if_fail(GWY_IS_VALUE_FORMAT(format), NULL);
    return format->priv->glue;
}

/**
 * gwy_value_format_set_glue:
 * @format: Value format.
 * @glue: New glue.  Empty glue can be represented as %NULL, in fact, this
 *        is preferable to empty string.
 *
 * Sets the string between number and units of a value format.
 **/
void
gwy_value_format_set_glue(GwyValueFormat *format,
                          const gchar *glue)
{
    g_return_if_fail(GWY_IS_VALUE_FORMAT(format));
    ValueFormat *priv = format->priv;
    if (_gwy_assign_string(&priv->glue, glue)) {
        // TODO: Emit something
    }
}

static void
ensure_value(ValueFormat *format)
{
    if (!format->value)
        format->value = g_string_new(NULL);
}

static void
fix_utf8_minus(GString *str)
{
    static const gchar minus_utf8[] = { 0xe2, 0x88, 0x92 };

    gchar *minus;
    guint lastpos = 0;

    while ((minus = strchr(str->str + lastpos, '-'))) {
        *minus = minus_utf8[0];
        lastpos = minus - str->str + 1;
        g_string_insert_len(str, lastpos,
                            minus_utf8 + 1, G_N_ELEMENTS(minus_utf8) - 1);
        lastpos += 2;
    }
}

#define __LIBGWY_VALUE_FORMAT_C__
#include "libgwy/libgwy-aliases.c"

/**
 * SECTION: value-format
 * @title: GwyValueFormat
 * @short_description: Physical quantity formatting
 * @see_also: #GwyUnit
 *
 * #GwyValueFormat is an auxiliary object describing how to format values
 * of a physical quantity within some range.
 *
 * The formatting functions gwy_value_format_print() and
 * gwy_value_format_print_number() produce strings and hence provide convenient
 * method to format numbers once the value format has been constructed.
 *
 * |[
 * GwyUnit *unit = gwy_unit_new_from_string("m", NULL);
 * GwyValueFormat *format = gwy_unit_format_with_resolution(unit,
 *                                                          GWY_VALUE_FORMAT_PLAIN,
 *                                                          1e-6, 1e-9, NULL);
 * g_print("The length is: %s\n", gwy_value_format_print(format, 3.456e-7));
 * g_object_unref(format);
 * g_object_unref(unit);
 * ]|
 *
 * The format constructors of #GwyUnit create relatively conservative formats.
 * Setting up the format manually gives somewhat wider possibilities.  It is,
 * for instance, possible to print angles in degrees even though the values are
 * in radians.
 *
 * |[
 * GwyValueFormat *format = gwy_value_format_new_set(GWY_VALUE_FORMAT_PLAIN,
 *                                                   0, 3, " ", "deg");
 * g_object_set(format, "base", G_PI/180.0, NULL);
 * g_print("The angle is: %s\n", gwy_value_format_print(format, G_PI/6));
 * g_object_unref(format);
 * ]|
 **/

/**
 * GwyValueFormat:
 *
 * Object representing the format of a physical quantity values.
 *
 * The #GwyValueFormat struct contains private data only and should be accessed
 * using the functions below.
 **/

/**
 * GwyValueFormatClass:
 *
 * Class of formats of physical quantity values.
 **/

/**
 * GwyValueFormatStyle:
 * @GWY_VALUE_FORMAT_NONE: No units.  This value is unused by #GwyValueFormat
 *                         itself and must not be requested as a format
 *                         style.
 * @GWY_VALUE_FORMAT_PLAIN: Plain style using only ASCII characters, as one
 *                          would use on a text terminal, suitable for output
 *                          to text files.
 * @GWY_VALUE_FORMAT_UNICODE: Plain style making use of Unicode characters,
 *                            e.g. for superscripts.
 * @GWY_VALUE_FORMAT_PANGO: Pango markup, for use with Gtk+ functions such as
 *                          gtk_label_set_markup().
 * @GWY_VALUE_FORMAT_TEX: Representation that can be typeset by TeX.
 *
 * Physical quantity formatting style.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
