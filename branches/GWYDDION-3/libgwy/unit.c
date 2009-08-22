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

#include <string.h>
#include <stdlib.h>
#include "libgwy/macros.h"
#include "libgwy/math.h"
#include "libgwy/serializable.h"
#include "libgwy/unit.h"

enum {
    CHANGED,
    LAST_SIGNAL
};

typedef struct {
    GQuark unit;
    gshort power;
    gshort traits;
} GwySimpleUnit;

typedef struct {
    const gchar *power10_times;
    const gchar *power10_open;
    const gchar *power_open;
    const gchar *power_close;
    const gchar *unit_times;
    const gchar *unit_division;
    const gchar *power_unit_separator;
} GwyUnitStyleSpec;

static void     gwy_unit_finalize         (GObject *object);
static void     gwy_unit_serializable_init(GwySerializableInterface *iface);
static gsize    gwy_unit_n_items          (GwySerializable *serializable);
static gsize    gwy_unit_itemize          (GwySerializable *serializable,
                                           GwySerializableItems *items);
static void     gwy_unit_done             (GwySerializable *serializable);
static GObject* gwy_unit_construct        (GwySerializableItems *items,
                                           GwyErrorList **error_list);
static GObject* gwy_unit_duplicate_impl   (GwySerializable *serializable);
static void     gwy_unit_assign_impl      (GwySerializable *destination,
                                           GwySerializable *source);
static gboolean     gwy_unit_parse             (GwyUnit *unit,
                                                   const gchar *string);
static GwyUnit*   gwy_unit_power_real        (GwyUnit *unit,
                                                   gint power,
                                                   GwyUnit *result);
static GwyUnit*   gwy_unit_canonicalize      (GwyUnit *unit);
static const gchar* gwy_unit_prefix            (gint power);
static GString*     gwy_unit_format            (GwyUnit *unit,
                                                   const GwyUnitStyleSpec *fs,
                                                   GString *string);

static const struct {
    const gchar *prefix;
    gint power10;
}
/* Canonical form must be always first, because this table is used for reverse
 * mapping too */
SI_prefixes[] = {
    { "k",     3  },
    { "c",    -2  },
    { "m",    -3  },
    { "M",     6  },
    { "µ",    -6  },
    /* People are extremely creative when it comes to \mu replacements... */
    { "μ",    -6  },
    { "~",    -6  },
    { "u",    -6  },
    { "\265", -6  },
    { "G",     9  },
    { "n",    -9  },
    { "T",     12 },
    { "p",    -12 },
    { "P",     15 },
    { "f",    -15 },
    { "E",     18 },
    { "a",    -18 },
    { "Z",     21 },
    { "z",    -21 },
    { "Y",     24 },
    { "y",    -24 },
};

/* TODO: silly units we should probably support specially: kg */

/* Units that can conflict with prefixes */
static const gchar *known_units[] = {
    "deg", "Pa", "cd", "mol", "cal", "px", "pt", "cps"
};

/* Unit formats */
static const GwyUnitStyleSpec format_style_plain = {
    "*", "10^", "^", NULL, " ", "/", " "
};
static const GwyUnitStyleSpec format_style_pango = {
    "×", "10<sup>", "<sup>", "</sup>", " ", "/", " "
};
static const GwyUnitStyleSpec format_style_TeX = {
    "\\times", "10^{", "^{", "}", "\\,", "/", "\\,"
};

static const GwyUnitStyleSpec *format_styles[] = {
    NULL,
    &format_style_plain,
    &format_style_pango,
    &format_style_TeX,
};

static guint unit_signals[LAST_SIGNAL];

G_DEFINE_TYPE_EXTENDED
    (GwyUnit, gwy_unit, G_TYPE_OBJECT, 0,
     GWY_IMPLEMENT_SERIALIZABLE(gwy_unit_serializable_init))

static void
gwy_unit_serializable_init(GwySerializableInterface *iface)
{
    iface->n_items   = gwy_unit_n_items;
    iface->itemize   = gwy_unit_itemize;
    iface->construct = gwy_unit_construct;
    iface->duplicate = gwy_unit_duplicate_impl;
    iface->assign    = gwy_unit_assign_impl;
}

static void
gwy_unit_class_init(GwyUnitClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = gwy_unit_finalize;

/**
 * GwyUnit::changed:
 * @gwysiunit: The #GwyUnit which received the signal.
 *
 * The ::changed signal is emitted whenever unit changes.
 */
    unit_signals[CHANGED]
        = g_signal_new("changed",
                       G_OBJECT_CLASS_TYPE(gobject_class),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET(GwyUnitClass, changed),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__VOID,
                       G_TYPE_NONE, 0);
}

static void
gwy_unit_init(G_GNUC_UNUSED GwyUnit *unit)
{
}

static void
gwy_unit_finalize(GObject *object)
{
    GwyUnit *unit = (GwyUnit*)object;

    if (unit->units)
        g_array_free(unit->units, TRUE);

    G_OBJECT_CLASS(gwy_unit_parent_class)->finalize(object);
}

static gsize
gwy_unit_n_items(G_GNUC_UNUSED GwySerializable *serializable)
{
    return 1;
}

static gsize
gwy_unit_itemize(GwySerializable *serializable,
                 GwySerializableItems *items)
{
}

static void
gwy_unit_done(GwySerializable *serializable)
{
    GwyUnit *unit = GWY_UNIT(serializable);

    GWY_FREE(unit->serialize_str);
}

static GObject*
gwy_unit_construct(GwySerializableItems *items,
                   GwyErrorList **error_list)
{
}

static GObject*
gwy_unit_duplicate_impl(GwySerializable *serializable)
{
    GwyUnit *unit = GWY_UNIT(serializable);

    GwyUnit *duplicate = g_object_newv(GWY_TYPE_UNIT, 0, NULL);
    duplicate->units = g_array_sized_new(FALSE, FALSE, sizeof(GwySimpleUnit),
                                         unit->units->len);
    g_array_append_vals(duplicate->units,
                        unit->units->data, unit->units->len);
}

static void
gwy_unit_assign_impl(GwySerializable *destination,
                     GwySerializable *source)
{
    GwyUnit *unit = GWY_UNIT(destination);
    GwyUnit *src = GWY_UNIT(source);

    if (gwy_unit_equal(unit, src))
        return;

    g_array_set_size(unit->units, 0);
    g_array_append_vals(unit->units, src->units->data, src->units->len);
    g_signal_emit(unit, unit_signals[CHANGED], 0);
}


#if 0
static GByteArray*
gwy_unit_serialize(GObject *obj,
                      GByteArray *buffer)
{
    GwyUnit *unit;
    GByteArray *retval;

    g_return_val_if_fail(GWY_IS_UNIT(obj), NULL);

    unit = GWY_UNIT(obj);
    {
        gchar *unitstr = gwy_unit_get_string(unit,
                                                GWY_UNIT_FORMAT_PLAIN);
        GwySerializeSpec spec[] = {
            { 's', "unitstr", &unitstr, NULL, },
        };
        gwy_debug("unitstr = <%s>", unitstr);
        retval = gwy_serialize_pack_object_struct(buffer,
                                                  GWY_UNIT_TYPE_NAME,
                                                  G_N_ELEMENTS(spec), spec);
        g_free(unitstr);
        return retval;
    }
}

static gsize
gwy_unit_get_size(GObject *obj)
{
    GwyUnit *unit;
    gsize size;

    g_return_val_if_fail(GWY_IS_UNIT(obj), 0);

    unit = GWY_UNIT(obj);
    size = gwy_serialize_get_struct_size(GWY_UNIT_TYPE_NAME, 0, NULL);
    /* Just estimate */
    size += 20*unit->units->len;

    return size;
}

static GObject*
gwy_unit_deserialize(const guchar *buffer,
                        gsize size,
                        gsize *position)
{
    gchar *unitstr = NULL;
    GwySerializeSpec spec[] = {
        { 's', "unitstr", &unitstr, NULL, },
    };
    GwyUnit *unit;

    g_return_val_if_fail(buffer, NULL);

    if (!gwy_serialize_unpack_object_struct(buffer, size, position,
                                            GWY_UNIT_TYPE_NAME,
                                            G_N_ELEMENTS(spec), spec)) {
        return NULL;
    }

    if (unitstr && !*unitstr) {
        g_free(unitstr);
        unitstr = NULL;
    }
    unit = gwy_unit_new(unitstr);
    g_free(unitstr);

    return (GObject*)unit;
}


static GObject*
gwy_unit_duplicate_real(GObject *object)
{
    GwyUnit *unit, *duplicate;

    g_return_val_if_fail(GWY_IS_UNIT(object), NULL);
    unit = GWY_UNIT(object);
    duplicate = gwy_unit_new_parse("", NULL);
    g_array_append_vals(duplicate->units,
                        unit->units->data, unit->units->len);

    return (GObject*)duplicate;
}

static void
gwy_unit_clone_real(GObject *source, GObject *copy)
{
    GwyUnit *unit, *clone;

    g_return_if_fail(GWY_IS_UNIT(source));
    g_return_if_fail(GWY_IS_UNIT(copy));

    unit = GWY_UNIT(source);
    clone = GWY_UNIT(copy);
    if (gwy_unit_equal(unit, clone))
        return;

    g_array_set_size(clone->units, 0);
    g_array_append_vals(clone->units,
                        unit->units->data, unit->units->len);
    g_signal_emit(copy, unit_signals[VALUE_CHANGED], 0);
}

/**
 * gwy_unit_new:
 * @unit_string: Unit string (it can be %NULL for an empty unit).
 *
 * Creates a new unit from string representation.
 *
 * Unit string represents unit with no prefixes
 * (e. g. "m", "N", "A", etc.)
 *
 * Returns: A new unit.
 **/
GwyUnit*
gwy_unit_new(const char *unit_string)
{
    return gwy_unit_new_parse(unit_string, NULL);
}

/**
 * gwy_unit_new_parse:
 * @unit_string: Unit string (it can be %NULL for an empty unit).
 * @power10: Where power of 10 should be stored (or %NULL).
 *
 * Creates a new unit from string representation.
 *
 * This is a more powerful version of gwy_unit_new(): @unit_string may
 * be a relatively complex unit, with prefixes, like "pA/s" or "km^2".
 * Beside conversion to a base unit like "A/s" or "m^2" it also computes
 * the power of 10 one has to multiply the base unit with to get an equivalent
 * of @unit_string.
 *
 * For example, for <literal>"pA/s"</literal> it will store -12 to @power10
 * because 1 pA/s is 1e-12 A/s, for <literal>"km^2"</literal> it will store 6
 * to @power10 because 1 km^2 is 1e6 m^2.
 *
 * Returns: A new unit.
 **/
GwyUnit*
gwy_unit_new_parse(const char *unit_string,
                      gint *power10)
{
    GwyUnit *unit;

    gwy_debug("");
    unit = g_object_new(GWY_TYPE_UNIT, NULL);
    unit->units = g_array_new(FALSE, FALSE, sizeof(GwySimpleUnit));
    gwy_unit_parse(unit, unit_string);
    if (power10)
        *power10 = unit->power10;

    return unit;
}

/**
 * gwy_unit_set_from_string:
 * @unit: An unit.
 * @unit_string: Unit string to set @unit from (it can be %NULL for an empty
 *               unit).
 *
 * Sets string that represents unit.
 *
 * It must be base unit with no prefixes (e. g. "m", "N", "A", etc.).
 **/
void
gwy_unit_set_from_string(GwyUnit *unit,
                            const gchar *unit_string)
{
    g_return_if_fail(GWY_IS_UNIT(unit));
    gwy_unit_set_from_string_parse(unit, unit_string, NULL);
}

/**
 * gwy_unit_set_from_string_parse:
 * @unit: An unit.
 * @unit_string: Unit string to set @unit from (it can be %NULL for an empty
 *               unit).
 * @power10: Where power of 10 should be stored (or %NULL).
 *
 * Changes an unit according to string representation.
 *
 * This is a more powerful version of gwy_unit_set_from_string(), please
 * see gwy_unit_new_parse() for some discussion.
 **/
void
gwy_unit_set_from_string_parse(GwyUnit *unit,
                                  const gchar *unit_string,
                                  gint *power10)
{
    g_return_if_fail(GWY_IS_UNIT(unit));

    gwy_unit_parse(unit, unit_string);
    if (power10)
        *power10 = unit->power10;

    g_signal_emit(unit, unit_signals[VALUE_CHANGED], 0);
}

static inline const GwyUnitStyleSpec*
gwy_unit_find_style_spec(GwyUnitFormatStyle style)
{
    if ((guint)style > GWY_UNIT_FORMAT_TEX) {
        g_warning("Invalid format style");
        style = GWY_UNIT_FORMAT_PLAIN;
    }

    return format_styles[style];
}

/**
 * gwy_unit_get_string:
 * @unit: An unit.
 * @style: Unit format style.
 *
 * Obtains string representing a unit.
 *
 * Returns: A newly allocated string that represents the base unit (with no
 *          prefixes).
 **/
gchar*
gwy_unit_get_string(GwyUnit *unit,
                       GwyUnitFormatStyle style)
{
    GString *string;
    gchar *s;

    g_return_val_if_fail(GWY_IS_UNIT(unit), NULL);

    string = gwy_unit_format(unit, gwy_unit_find_style_spec(style),
                                NULL);
    s = string->str;
    g_string_free(string, FALSE);

    return s;
}

/**
 * gwy_unit_get_format_for_power10:
 * @unit: An unit.
 * @style: Unit format style.
 * @power10: Power of 10, in the same sense as gwy_unit_new_parse()
 *           returns it.
 * @format: A value format to set-up, may be %NULL, a new value format is
 *          allocated then.
 *
 * Finds format for representing a specific power-of-10 multiple of a unit.
 *
 * The values should be then printed as value/@format->magnitude
 * [@format->units] with @format->precision decimal places.
 *
 * This function does not change the precision field of @format.
 *
 * Returns: The value format.  If @format was %NULL, a newly allocated format
 *          is returned, otherwise (modified) @format itself is returned.
 **/
GwyUnitFormat*
gwy_unit_get_format_for_power10(GwyUnit *unit,
                                   GwyUnitFormatStyle style,
                                   gint power10,
                                   GwyUnitFormat *format)
{
    const GwyUnitStyleSpec *spec;

    g_return_val_if_fail(GWY_IS_UNIT(unit), NULL);

    spec = gwy_unit_find_style_spec(style);
    if (!format)
        format = (GwyUnitFormat*)g_new0(GwyUnitFormat, 1);

    unit->power10 = power10;
    format->magnitude = pow10(power10);
    format->units_gstring = gwy_unit_format(unit, spec,
                                               format->units_gstring);
    format->units = format->units_gstring->str;

    return format;
}

/**
 * gwy_unit_get_format:
 * @unit: An unit.
 * @style: Unit format style.
 * @value: Value the format should be suitable for.
 * @format: A value format to set-up, may be %NULL, a new value format is
 *          allocated then.
 *
 * Finds a good format for representing a value.
 *
 * The values should be then printed as value/@format->magnitude
 * [@format->units] with @format->precision decimal places.
 *
 * Returns: The value format.  If @format was %NULL, a newly allocated format
 *          is returned, otherwise (modified) @format itself is returned.
 **/
GwyUnitFormat*
gwy_unit_get_format(GwyUnit *unit,
                       GwyUnitFormatStyle style,
                       gdouble value,
                       GwyUnitFormat *format)
{
    const GwyUnitStyleSpec *spec;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_UNIT(unit), NULL);

    spec = gwy_unit_find_style_spec(style);
    if (!format)
        format = (GwyUnitFormat*)g_new0(GwyUnitFormat, 1);

    value = fabs(value);
    if (!value) {
        format->magnitude = 1;
        format->precision = 2;
    }
    else
        format->magnitude = gwy_math_humanize_numbers(value/36, value,
                                                      &format->precision);
    unit->power10 = GWY_ROUND(log10(format->magnitude));
    format->units_gstring = gwy_unit_format(unit, spec,
                                               format->units_gstring);
    format->units = format->units_gstring->str;

    return format;
}

/**
 * gwy_unit_get_format_with_resolution:
 * @unit: A unit.
 * @style: Unit format style.
 * @maximum: The maximum value to be represented.
 * @resolution: The smallest step (approximately) that should make a visible
 *              difference in the representation.
 * @format: A value format to set-up, may be %NULL, a new value format is
 *          allocated then.
 *
 * Finds a good format for representing a range of values with given resolution.
 *
 * The values should be then printed as value/@format->magnitude
 * [@format->units] with @format->precision decimal places.
 *
 * Returns: The value format.  If @format was %NULL, a newly allocated format
 *          is returned, otherwise (modified) @format itself is returned.
 **/
GwyUnitFormat*
gwy_unit_get_format_with_resolution(GwyUnit *unit,
                                       GwyUnitFormatStyle style,
                                       gdouble maximum,
                                       gdouble resolution,
                                       GwyUnitFormat *format)
{
    const GwyUnitStyleSpec *spec;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_UNIT(unit), NULL);

    spec = gwy_unit_find_style_spec(style);
    if (!format)
        format = (GwyUnitFormat*)g_new0(GwyUnitFormat, 1);

    maximum = fabs(maximum);
    resolution = fabs(resolution);
    if (!maximum) {
        format->magnitude = 1;
        format->precision = 2;
    }
    else
        format->magnitude = gwy_math_humanize_numbers(resolution, maximum,
                                                      &format->precision);
    unit->power10 = GWY_ROUND(log10(format->magnitude));
    format->units_gstring = gwy_unit_format(unit, spec,
                                               format->units_gstring);
    format->units = format->units_gstring->str;

    return format;
}

/**
 * gwy_unit_get_format_with_digits:
 * @unit: A unit.
 * @style: Unit format style.
 * @maximum: The maximum value to be represented.
 * @sdigits: The number of significant digits the value should have.
 * @format: A value format to set-up, may be %NULL, a new value format is
 *          allocated then.
 *
 * Finds a good format for representing a values with given number of
 * significant digits.
 *
 * The values should be then printed as value/@format->magnitude
 * [@format->units] with @format->precision decimal places.
 *
 * Returns: The value format.  If @format was %NULL, a newly allocated format
 *          is returned, otherwise (modified) @format itself is returned.
 **/
GwyUnitFormat*
gwy_unit_get_format_with_digits(GwyUnit *unit,
                                   GwyUnitFormatStyle style,
                                   gdouble maximum,
                                   gint sdigits,
                                   GwyUnitFormat *format)
{
    const GwyUnitStyleSpec *spec;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_UNIT(unit), NULL);

    spec = gwy_unit_find_style_spec(style);
    if (!format)
        format = (GwyUnitFormat*)g_new0(GwyUnitFormat, 1);

    maximum = fabs(maximum);
    if (!maximum) {
        format->magnitude = 1;
        format->precision = sdigits;
    }
    else
        format->magnitude
            = gwy_math_humanize_numbers(maximum/pow10(sdigits),
                                        maximum, &format->precision);
    unit->power10 = GWY_ROUND(log10(format->magnitude));
    format->units_gstring = gwy_unit_format(unit, spec,
                                               format->units_gstring);
    format->units = format->units_gstring->str;

    return format;
}

/**
 * unit_format_free:
 * @format: A value format to free.
 *
 * Frees a value format structure.
 **/
void
gwy_unit_format_free(GwyUnitFormat *format)
{
    if (format->units_gstring)
        g_string_free(format->units_gstring, TRUE);
    g_free(format);
}

/**
 * unit_format_set_units:
 * @format: A value format to set units of.
 * @units: The units string.
 *
 * Sets the units field of a value format structure.
 *
 * This function keeps the @units and @units_gstring fields consistent.
 **/
void
gwy_unit_format_set_units(GwyUnitFormat *format,
                                   const gchar *units)
{
    if (!format->units_gstring)
        format->units_gstring = g_string_new(units);
    else
        g_string_assign(format->units_gstring, units);

    format->units = format->units_gstring->str;
}

#endif

/**
 * gwy_unit_equal:
 * @unit: Physical units.
 * @op: Physical units to compare @unit to.
 *
 * Tests whether two physical units are equal.
 *
 * Returns: %TRUE iff units are equal.
 **/
gboolean
gwy_unit_equal(GwyUnit *unit, GwyUnit *op)
{
    if (op == unit)
        return TRUE;

    if (op->units->len != unit->units->len)
        return FALSE;

    for (guint i = 0; i < unit->units->len; i++) {
        const GwySimpleUnit *u = &g_array_index(unit->units, GwySimpleUnit, i);
        guint j;

        for (j = 0; j < op->units->len; j++) {
            if (g_array_index(op->units, GwySimpleUnit, j).unit == u->unit) {
                if (g_array_index(op->units, GwySimpleUnit, j).power != u->power)
                    return FALSE;
                break;
            }
        }
        if (j == op->units->len)
            return FALSE;
    }

    return TRUE;
}

#if 0

static gboolean
gwy_unit_parse(GwyUnit *unit,
                  const gchar *string)
{
    GwySimpleUnit unit;
    gdouble q;
    const gchar *end;
    gchar *p, *e;
    gint n, i, pfpower;
    GString *buf;
    gboolean dividing = FALSE;

    g_array_set_size(unit->units, 0);
    unit->power10 = 0;

    if (!string || !*string)
        return TRUE;

    /* give up when it looks too wild */
    end = strpbrk(string,
                  "\177\001\002\003\004\005\006\007"
                  "\010\011\012\013\014\015\016\017"
                  "\020\021\022\023\024\025\026\027"
                  "\030\031\032\033\034\035\036\037"
                  "!#$&()*,:;=?@\\[]_`|{}");
    if (end) {
        g_warning("Invalid character 0x%02x", *end);
        return FALSE;
    }

    /* may start with a multiplier, but it must be a power of 10 */
    q = g_ascii_strtod(string, (gchar**)&end);
    if (end != string) {
        string = end;
        unit->power10 = GWY_ROUND(log10(q));
        if (q <= 0 || fabs(log(q/pow10(unit->power10))) > 1e-13) {
            g_warning("Bad multiplier %g", q);
            unit->power10 = 0;
        }
        else if (g_str_has_prefix(string, "<sup>")) {
            string += strlen("<sup>");
            n = strtol(string, (gchar**)&end, 10);
            if (end == string)
                g_warning("Bad exponent %s", string);
            else if (!g_str_has_prefix(end, "</sup>"))
                g_warning("Expected </sup> after exponent");
            else
                unit->power10 *= n;
            string = end;
        }
        else if (string[0] == '^') {
            string++;
            n = strtol(string, (gchar**)&end, 10);
            if (end == string)
                g_warning("Bad exponent %s", string);
            else
                unit->power10 *= n;
            string = end;
        }
    }
    while (g_ascii_isspace(*string))
        string++;

    buf = g_string_new("");

    /* the rest are units */
    while (*string) {
        /* units are separated with whitespace and maybe a division sign */
        end = string;
        do {
            end = strpbrk(end, " /");
            if (!end || end == string || *end != '/' || *(end-1) != '<')
                break;
            end++;
        } while (TRUE);
        if (!end)
            end = string + strlen(string);

        g_string_set_size(buf, 0);
        g_string_append_len(buf, string, end - string);

        /* fix sloppy notations */
        if (buf->str[0] == '\272') {
            if (!buf->str[1])
                g_string_assign(buf, "deg");
            else {
                g_string_erase(buf, 0, 1);
                g_string_prepend(buf, "°");
            }
        }
        else if (gwy_strequal(buf->str, "°"))
            g_string_assign(buf, "deg");
        else if ((buf->str[0] == '\305' && !buf->str[1])
                 || gwy_strequal(buf->str, "Å")
                 || gwy_strequal(buf->str, "AA")
                 || gwy_strequal(buf->str, "Ang"))
            g_string_assign(buf, "Å");

        /* get prefix, but be careful not to split mol to mili-ol */
        pfpower = 0;
        for (i = 0; i < G_N_ELEMENTS(known_units); i++) {
            if (g_str_has_prefix(buf->str, known_units[i])
                && !g_ascii_isalpha(buf->str[strlen(known_units[i])]))
                break;
        }
        if (i == G_N_ELEMENTS(known_units) && strlen(buf->str) > 1) {
            for (i = 0; i < G_N_ELEMENTS(SI_prefixes); i++) {
                const gchar *pfx = SI_prefixes[i].prefix;

                if (g_str_has_prefix(buf->str, pfx)
                    && g_ascii_isalpha(buf->str[strlen(pfx)])) {
                    pfpower = SI_prefixes[i].power10;
                    g_string_erase(buf, 0, strlen(pfx));
                    break;
                }
            }
        }

        /* get unit power */
        unit.power = 1;
        if ((p = strstr(buf->str + 1, "<sup>"))) {
            unit.power = strtol(p + strlen("<sup>"), &e, 10);
            if (e == p + strlen("<sup>")
                || !g_str_has_prefix(e, "</sup>")) {
                g_warning("Bad power %s", p);
                unit.power = 1;
            }
            else if (!unit.power || abs(unit.power) > 12) {
                g_warning("Bad power %d", unit.power);
                unit.power = 1;
            }
            g_string_truncate(buf, p - buf->str);
        }
        else if ((p = strchr(buf->str + 1, '^'))) {
            unit.power = strtol(p + 1, &e, 10);
            if (e == p + 1 || *e) {
                g_warning("Bad power %s", p);
                unit.power = 1;
            }
            else if (!unit.power || abs(unit.power) > 12) {
                g_warning("Bad power %d", unit.power);
                unit.power = 1;
            }
            g_string_truncate(buf, p - buf->str);
        }
        else if (buf->len) {
            /* Are we really desperate?  Yes, we are! */
            i = buf->len;
            while (i && (g_ascii_isdigit(buf->str[i-1])
                         || buf->str[i-1] == '-'))
                i--;
            if (i != buf->len) {
                unit.power = strtol(buf->str + i, NULL, 10);
                if (!unit.power || abs(unit.power) > 12) {
                    g_warning("Bad power %d", unit.power);
                    unit.power = 1;
                }
                g_string_truncate(buf, i);
            }
        }

        /* handle some ugly, but quite common units */
        if (gwy_strequal(buf->str, "Å")) {
            pfpower -= 10;
            g_string_assign(buf, "m");
        }
        else if (gwy_strequal(buf->str, "%")) {
            pfpower -= 2;
            g_string_assign(buf, "");
        }

        /* elementary sanity */
        if (!g_utf8_validate(buf->str, -1, (const gchar**)&p)) {
            g_warning("Unit string is not valid UTF-8");
            g_string_truncate(buf, p - buf->str);
        }
        if (!buf->len) {
            /* maybe it's just percentage.  cross fingers and proceed. */
            if (dividing)
                unit.power = -unit.power;
            unit->power10 += unit.power * pfpower;
        }
        else if (!g_ascii_isalpha(buf->str[0]) && (guchar)buf->str[0] < 128)
            g_warning("Invalid base unit: %s", buf->str);
        else {
            /* append it */
            unit.unit = g_quark_from_string(buf->str);
            if (dividing)
                unit.power = -unit.power;
            gwy_debug("<%s:%u> %d\n", buf->str, unit.unit, unit.power);
            unit->power10 += unit.power * pfpower;
            g_array_append_val(unit->units, unit);
        }

        /* TODO: scan known obscure units */
        unit.traits = 0;

        /* get to the next token, looking for division */
        while (g_ascii_isspace(*end))
            end++;
        if (*end == '/') {
            if (dividing)
                g_warning("Cannot group multiple divisions");
            dividing = TRUE;
            end++;
            while (g_ascii_isspace(*end))
                end++;
        }
        string = end;
    }

    gwy_unit_canonicalize(unit);

    return TRUE;
}

/**
 * gwy_unit_multiply:
 * @siunit1: An unit.
 * @siunit2: An unit.
 * @result: An unit to set to product of @siunit1 and @siunit2.  It is
 *          safe to pass one of @siunit1, @siunit2. It can be %NULL
 *          too, a new unit is created then and returned.
 *
 * Multiplies two units.
 *
 * Returns: When @result is %NULL, a newly created unit that has to be
 *          dereferenced when no longer used later.  Otherwise @result itself
 *          is simply returned, its reference count is NOT increased.
 **/
GwyUnit*
gwy_unit_multiply(GwyUnit *siunit1,
                     GwyUnit *siunit2,
                     GwyUnit *result)
{
    return gwy_unit_power_multiply(siunit1, 1, siunit2, 1, result);
}

/**
 * gwy_unit_divide:
 * @siunit1: An unit.
 * @siunit2: An unit.
 * @result: An unit to set to quotient of @siunit1 and @siunit2.  It is
 *          safe to pass one of @siunit1, @siunit2. It can be %NULL
 *          too, a new unit is created then and returned.
 *
 * Divides two units.
 *
 * Returns: When @result is %NULL, a newly created unit that has to be
 *          dereferenced when no longer used later.  Otherwise @result itself
 *          is simply returned, its reference count is NOT increased.
 **/
GwyUnit*
gwy_unit_divide(GwyUnit *siunit1,
                   GwyUnit *siunit2,
                   GwyUnit *result)
{
    return gwy_unit_power_multiply(siunit1, 1, siunit2, -1, result);
}

/**
 * gwy_unit_power:
 * @unit: An unit.
 * @power: Power to raise @unit to.
 * @result: An unit to set to power of @unit.  It is safe to pass
 *          @unit itself.  It can be %NULL too, a new unit is created
 *          then and returned.
 *
 * Computes a power of an unit.
 *
 * Returns: When @result is %NULL, a newly created unit that has to be
 *          dereferenced when no longer used later.  Otherwise @result itself
 *          is simply returned, its reference count is NOT increased.
 **/
GwyUnit*
gwy_unit_power(GwyUnit *unit,
                  gint power,
                  GwyUnit *result)
{
    g_return_val_if_fail(GWY_IS_UNIT(unit), NULL);
    g_return_val_if_fail(!result || GWY_IS_UNIT(result), NULL);

    if (!result)
        result = gwy_unit_new(NULL);

    gwy_unit_power_real(unit, power, result);
    g_signal_emit(result, unit_signals[VALUE_CHANGED], 0);

    return result;
}

static GwyUnit*
gwy_unit_power_real(GwyUnit *unit,
                       gint power,
                       GwyUnit *result)
{
    GArray *units;
    GwySimpleUnit *unit;
    gint j;

    units = g_array_new(FALSE, FALSE, sizeof(GwySimpleUnit));
    result->power10 = power*unit->power10;

    if (power) {
        g_array_append_vals(units, unit->units->data, unit->units->len);
        for (j = 0; j < units->len; j++) {
            unit = &g_array_index(units, GwySimpleUnit, j);
            unit->power *= power;
        }
    }

    g_array_set_size(result->units, 0);
    g_array_append_vals(result->units, units->data, units->len);
    g_array_free(units, TRUE);

    return result;
}

/**
 * gwy_unit_nth_root:
 * @unit: An unit.
 * @ipower: The root to take: 2 means a quadratic root, 3 means cubic root,
 *          etc.
 * @result: An unit to set to power of @unit.  It is safe to pass
 *          @unit itself.  It can be %NULL too, a new unit is created
 *          then and returned.
 *
 * Calulates n-th root of an unit.
 *
 * This operation fails if the result would have fractional powers that
 * are not representable by #GwyUnit.
 *
 * Returns: On success: When @result is %NULL, a newly created unit that
 *          has to be dereferenced when no longer used later, otherwise
 *          @result itself is simply returned, its reference count is NOT
 *          increased. On failure %NULL is always returned.
 *
 * Since: 2.5
 **/
GwyUnit*
gwy_unit_nth_root(GwyUnit *unit,
                     gint ipower,
                     GwyUnit *result)
{
    GArray *units;
    GwySimpleUnit *unit;
    gint j;

    g_return_val_if_fail(GWY_IS_UNIT(unit), NULL);
    g_return_val_if_fail(!result || GWY_IS_UNIT(result), NULL);
    g_return_val_if_fail(ipower > 0, NULL);

    /* Check applicability */
    for (j = 0; j < unit->units->len; j++) {
        unit = &g_array_index(unit->units, GwySimpleUnit, j);
        if (unit->power % ipower != 0)
            return NULL;
    }

    units = g_array_new(FALSE, FALSE, sizeof(GwySimpleUnit));
    /* XXX: Applicability not required */
    result->power10 = unit->power10/ipower;

    if (!result)
        result = gwy_unit_new(NULL);

    g_array_append_vals(units, unit->units->data, unit->units->len);
    for (j = 0; j < units->len; j++) {
        unit = &g_array_index(units, GwySimpleUnit, j);
        unit->power /= ipower;
    }

    g_array_set_size(result->units, 0);
    g_array_append_vals(result->units, units->data, units->len);
    g_array_free(units, TRUE);

    g_signal_emit(result, unit_signals[VALUE_CHANGED], 0);

    return result;
}

/**
 * gwy_unit_power_multiply:
 * @siunit1: An unit.
 * @power1: Power to raise @siunit1 to.
 * @siunit2: An unit.
 * @power2: Power to raise @siunit2 to.
 * @result: An unit to set to @siunit1^@power1*@siunit2^@power2.
 *          It is safe to pass @siunit1 or @siunit2.  It can be %NULL too,
 *          a new unit is created then and returned.
 *
 * Computes the product of two units raised to arbitrary powers.
 *
 * This is the most complex unit arithmetic function.  It can be easily
 * chained when more than two units are to be multiplied.
 *
 * Returns: When @result is %NULL, a newly created unit that has to be
 *          dereferenced when no longer used later.  Otherwise @result itself
 *          is simply returned, its reference count is NOT increased.
 *
 * Since: 2.4
 **/
GwyUnit*
gwy_unit_power_multiply(GwyUnit *siunit1,
                           gint power1,
                           GwyUnit *siunit2,
                           gint power2,
                           GwyUnit *result)
{
    GwyUnit *op2 = NULL;
    GwySimpleUnit *unit, *unit2;
    gint i, j;

    g_return_val_if_fail(GWY_IS_UNIT(siunit1), NULL);
    g_return_val_if_fail(GWY_IS_UNIT(siunit2), NULL);
    g_return_val_if_fail(!result || GWY_IS_UNIT(result), NULL);

    if (!result)
        result = gwy_unit_new(NULL);

    /* Try to avoid hard work by making siunit2 the simplier one */
    if (siunit1->units->len < siunit2->units->len
        || (power2 && !power1)
        || (siunit2 == result && siunit1 != result)) {
        GWY_SWAP(GwyUnit*, siunit1, siunit2);
        GWY_SWAP(gint, power1, power2);
    }
    gwy_unit_power_real(siunit1, power1, result);
    if (!power2) {
        gwy_unit_canonicalize(result);
        return result;
    }

    /* When the second operand is the same object as the result, we have to
     * operate on a temporary copy */
    if (siunit2 == result) {
        op2 = gwy_unit_duplicate(siunit2);
        siunit2 = op2;
    }

    result->power10 += power2*siunit2->power10;
    for (i = 0; i < siunit2->units->len; i++) {
        unit2 = &g_array_index(siunit2->units, GwySimpleUnit, i);

        for (j = 0; j < result->units->len; j++) {
            unit = &g_array_index(result->units, GwySimpleUnit, j);
            gwy_debug("[%d] %u == [%d] %u",
                      i, unit2->unit, j, unit->unit);
            if (unit2->unit == unit->unit) {
                unit->power += power2*unit2->power;
                break;
            }
        }
        if (j == result->units->len) {
            g_array_append_val(result->units, *unit2);
            unit = &g_array_index(result->units, GwySimpleUnit,
                                  result->units->len - 1);
            unit->power *= power2;
        }
    }
    gwy_unit_canonicalize(result);
    gwy_object_unref(op2);
    g_signal_emit(result, unit_signals[VALUE_CHANGED], 0);

    return result;
}

static GwyUnit*
gwy_unit_canonicalize(GwyUnit *unit)
{
    GwySimpleUnit *dst, *src;
    gint i, j;

    /* consolidate multiple occurences of the same unit */
    i = 0;
    while (i < unit->units->len) {
        src = &g_array_index(unit->units, GwySimpleUnit, i);

        for (j = 0; j < i; j++) {
            dst = &g_array_index(unit->units, GwySimpleUnit, j);
            if (src->unit == dst->unit) {
                dst->power += src->power;
                g_array_remove_index(unit->units, i);
                break;
            }
        }

        if (j == i)
            i++;
    }

    /* remove units with zero power */
    i = 0;
    while (i < unit->units->len) {
        if (g_array_index(unit->units, GwySimpleUnit, i).power)
            i++;
        else {
            g_array_remove_index(unit->units, i);
        }
    }

    return unit;
}

static GString*
gwy_unit_format(GwyUnit *unit,
                   const GwyUnitStyleSpec *fs,
                   gint power10,
                   GString *string)
{
    const gchar *prefix = "No GCC, this can't be used uninitialized";
    GwySimpleUnit *unit;
    gint i, prefix_bearer, move_me_to_end;

    if (!string)
        string = g_string_new("");
    else
        g_string_truncate(string, 0);

    /* if there is a single unit with negative exponent, move it to the end
     * TODO: we may want more sophistication here */
    move_me_to_end = -1;
    if (unit->units->len > 1) {
        for (i = 0; i < unit->units->len; i++) {
            unit = &g_array_index(unit->units, GwySimpleUnit, i);
            if (unit->power < 0) {
                if (move_me_to_end >= 0) {
                    move_me_to_end = -1;
                    break;
                }
                move_me_to_end = i;
            }
        }
    }

    /* find a victim to prepend a prefix to.  mwhahaha */
    prefix_bearer = -1;
    if (power10) {
        for (i = 0; i < unit->units->len; i++) {
            if (i == move_me_to_end)
                continue;
            unit = &g_array_index(unit->units, GwySimpleUnit, i);
            if (power10 % (3*abs(unit->power)) == 0) {
                prefix_bearer = i;
                break;
            }
        }
    }
    if (power10 && prefix_bearer < 0 && move_me_to_end >= 0) {
        unit = &g_array_index(unit->units, GwySimpleUnit, move_me_to_end);
        if (power10 % (3*abs(unit->power)) == 0)
            prefix_bearer = move_me_to_end;
    }
    /* check whether we are not out of prefix range */
    if (prefix_bearer >= 0) {
        unit = &g_array_index(unit->units, GwySimpleUnit, prefix_bearer);
        prefix = gwy_unit_prefix(power10/unit->power);
        if (!prefix)
            prefix_bearer = -1;
    }

    /* if we were unable to place the prefix, we must add a power of 10 */
    if (power10 && prefix_bearer < 0) {
        switch (power10) {
            case -1:
            g_string_append(string, "0.1");
            break;

            case 1:
            g_string_append(string, "10");
            break;

            case 2:
            g_string_append(string, "100");
            break;

            default:
            if (fs->power10_open)
                g_string_append(string, fs->power10_open);
            g_string_append_printf(string, "%d", unit->power10);
            if (fs->power_close)
                g_string_append(string, fs->power_close);
            break;
        }
        if (fs->power_unit_separator && unit->units->len)
            g_string_append(string, fs->power_unit_separator);
    }

    /* append units */
    for (i = 0; i < unit->units->len; i++) {
        if (i == move_me_to_end)
            continue;
        if (i > 1 || (i && move_me_to_end)) {
            g_string_append(string, fs->unit_times);
        }
        unit = &g_array_index(unit->units, GwySimpleUnit, i);
        if (i == prefix_bearer)
            g_string_append(string, prefix);
        g_string_append(string, g_quark_to_string(unit->unit));
        if (unit->power != 1) {
            if (fs->power_open)
                g_string_append(string, fs->power_open);
            g_string_append_printf(string, "%d", unit->power);
            if (fs->power_close)
                g_string_append(string, fs->power_close);
        }
    }
    if (move_me_to_end >= 0) {
        g_string_append(string, fs->unit_division);
        unit = &g_array_index(unit->units, GwySimpleUnit, move_me_to_end);
        if (move_me_to_end == prefix_bearer)
            g_string_append(string, prefix);
        g_string_append(string, g_quark_to_string(unit->unit));
        if (unit->power != -1) {
            if (fs->power_open)
                g_string_append(string, fs->power_open);
            g_string_append_printf(string, "%d", -unit->power);
            if (fs->power_close)
                g_string_append(string, fs->power_close);
        }
    }

    return string;
}

static const gchar*
gwy_unit_prefix(gint power)
{
    gint i;

    for (i = 0; i < G_N_ELEMENTS(SI_prefixes); i++) {
        if (SI_prefixes[i].power10 == power)
            return SI_prefixes[i].prefix;
    }
    return NULL;
}

#endif

/************************** Documentation ****************************/

/**
 * SECTION: unit
 * @title: GwyUnit
 * @short_description: Physical unit representation, quantitiy formatting
 *
 * #GwyUnit represents physical units.  Units can be compared and also divided,
 * multiplied, etc. to give new units.
 *
 * It provides also methods for formatting of physical quantities.  There are
 * several methods computing value format (as a #GwyUnitFormat structure) with
 * given resolution -- gwy_unit_get_format_with_resolution(), or number of
 * significant digits -- gwy_unit_get_format_with_digits().
 **/

/**
 * GwyUnit:
 *
 * Representation of physical units.
 *
 * The #GwyUnit struct contains private data only and should be accessed
 * using the functions below.
 **/

/**
 * GwyUnitFormatStyle:
 * @GWY_UNIT_FORMAT_NONE: No units.  This value is unused by #GwyUnit
 *                        itself and must not be requested as a format
 *                        style.
 * @GWY_UNIT_FORMAT_PLAIN: Plain style, as one would use on a text terminal,
 *                         suitable for output to text files.
 * @GWY_UNIT_FORMAT_PANGO: Pango markup, for use with Gtk+ functions such as
 *                         gtk_label_set_markup().
 * @GWY_UNIT_FORMAT_TEX: Representation that can be typeset by TeX.
 *
 * Physical quantity formatting style.
 **/

/**
 * GwyUnitFormat:
 * @magnitude: Number to divide a quantity by (generally, a power of 1000).
 * @precision: Number of decimal places to format a quantity to.
 * @units: Units to put after quantity divided by @magnitude.  This is actually
 *         an alias to @units_gstring->str.
 * @units_gstring: #GString used to represent @units internally.
 *
 * A physical quantity formatting information.
 *
 * The @magnitude and @precision fields can be directly modified if necessary.
 * Units must be always set with gwy_unit_format_set_units() to update
 * the internal representation properly.
 */

/**
 * gwy_unit_duplicate:
 * @unit: An unit to duplicate.
 *
 * Convenience macro doing gwy_serializable_duplicate() with all the necessary
 * typecasting.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
