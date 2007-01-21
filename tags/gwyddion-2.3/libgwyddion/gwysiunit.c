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

#include "config.h"
#include "gwymacros.h"

#include <string.h>
#include <stdlib.h>

#include "gwymath.h"
#include "gwydebugobjects.h"
#include "gwyserializable.h"
#include "gwysiunit.h"

#define GWY_SI_UNIT_TYPE_NAME "GwySIUnit"

enum {
    VALUE_CHANGED,
    LAST_SIGNAL
};

/* FIXME: unused */
typedef struct {
    gdouble prefix_affinity;
    gint base_power10;
} GwyUnitTraits;

typedef struct {
    GQuark unit;
    gshort power;
    gshort traits;
} GwySimpleUnit;

typedef struct {
    const gchar *power10_prefix;
    const gchar *power_prefix;
    const gchar *power_suffix;
    const gchar *unit_times;
    const gchar *unit_division;
    const gchar *power_unit_separator;
} GwySIStyleSpec;

static void         gwy_si_unit_finalize          (GObject *object);
static void         gwy_si_unit_serializable_init (GwySerializableIface *iface);
static GByteArray*  gwy_si_unit_serialize         (GObject *obj,
                                                   GByteArray *buffer);
static gsize        gwy_si_unit_get_size          (GObject *obj);
static GObject*     gwy_si_unit_deserialize       (const guchar *buffer,
                                                   gsize size,
                                                   gsize *position);
static GObject*     gwy_si_unit_duplicate_real    (GObject *object);
static void         gwy_si_unit_clone_real        (GObject *source,
                                                   GObject *copy);
static gboolean     gwy_si_unit_parse             (GwySIUnit *siunit,
                                                   const gchar *string);
static GwySIUnit*   gwy_si_unit_power_real        (GwySIUnit *siunit,
                                                   gint power,
                                                   GwySIUnit *result);
static GwySIUnit*   gwy_si_unit_power_multiply    (GwySIUnit *siunit1,
                                                   gint power1,
                                                   GwySIUnit *siunit2,
                                                   gint power2,
                                                   GwySIUnit *result);
static GwySIUnit*   gwy_si_unit_canonicalize      (GwySIUnit *siunit);
static const gchar* gwy_si_unit_prefix            (gint power);
static GString*     gwy_si_unit_format            (GwySIUnit *siunit,
                                                   const GwySIStyleSpec *fs,
                                                   GString *string);

/* FIXME: unused */
static const GwyUnitTraits unit_traits[] = {
    { 1.0,   0 },    /* normal unit */
    { 1.0,   3 },    /* kilogram */
    { 0.0,   0 },    /* does not take prefixes */
};

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
static const GwySIStyleSpec format_style_plain = {
    "10^", "^", NULL, " ", "/", " "
};
static const GwySIStyleSpec format_style_markup = {
    "10<sup>", "<sup>", "</sup>", " ", "/", " "
};
static const GwySIStyleSpec format_style_vfmarkup = {
    "× 10<sup>", "<sup>", "</sup>", " ", "/", " "
};
static const GwySIStyleSpec format_style_TeX = {
    "10^{", "^{", "}", "\\,", "/", "\\,"
};
/* Unused */
static const GwySIStyleSpec format_style_backwoods = {
    "1e", NULL, NULL, " ", "/", " "
};

static const GwySIStyleSpec *format_styles[] = {
    NULL,
    &format_style_plain,
    &format_style_markup,
    &format_style_vfmarkup,
    &format_style_TeX,
};

static guint si_unit_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_EXTENDED
    (GwySIUnit, gwy_si_unit, G_TYPE_OBJECT, 0,
     GWY_IMPLEMENT_SERIALIZABLE(gwy_si_unit_serializable_init))

static void
gwy_si_unit_serializable_init(GwySerializableIface *iface)
{
    iface->serialize = gwy_si_unit_serialize;
    iface->deserialize = gwy_si_unit_deserialize;
    iface->get_size = gwy_si_unit_get_size;
    iface->duplicate = gwy_si_unit_duplicate_real;
    iface->clone = gwy_si_unit_clone_real;
}

static void
gwy_si_unit_class_init(GwySIUnitClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = gwy_si_unit_finalize;

/**
 * GwySIUnit::value-changed:
 * @gwysiunit: The #GwySIUnit which received the signal.
 *
 * The ::value-changed signal is emitted whenever SI unit changes.
 */
    si_unit_signals[VALUE_CHANGED]
        = g_signal_new("value-changed",
                       G_OBJECT_CLASS_TYPE(gobject_class),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET(GwySIUnitClass, value_changed),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__VOID,
                       G_TYPE_NONE, 0);
}

static void
gwy_si_unit_init(GwySIUnit *si_unit)
{
    gwy_debug_objects_creation((GObject*)si_unit);
}

static void
gwy_si_unit_finalize(GObject *object)
{
    GwySIUnit *si_unit = (GwySIUnit*)object;

    if (si_unit->units)
        g_array_free(si_unit->units, TRUE);

    G_OBJECT_CLASS(gwy_si_unit_parent_class)->finalize(object);
}

static GByteArray*
gwy_si_unit_serialize(GObject *obj,
                      GByteArray *buffer)
{
    GwySIUnit *si_unit;
    GByteArray *retval;

    g_return_val_if_fail(GWY_IS_SI_UNIT(obj), NULL);

    si_unit = GWY_SI_UNIT(obj);
    {
        gchar *unitstr = gwy_si_unit_get_string(si_unit,
                                                GWY_SI_UNIT_FORMAT_PLAIN);
        GwySerializeSpec spec[] = {
            { 's', "unitstr", &unitstr, NULL, },
        };
        gwy_debug("unitstr = <%s>", unitstr);
        retval = gwy_serialize_pack_object_struct(buffer,
                                                  GWY_SI_UNIT_TYPE_NAME,
                                                  G_N_ELEMENTS(spec), spec);
        g_free(unitstr);
        return retval;
    }
}

static gsize
gwy_si_unit_get_size(GObject *obj)
{
    GwySIUnit *si_unit;
    gsize size;

    g_return_val_if_fail(GWY_IS_SI_UNIT(obj), 0);

    si_unit = GWY_SI_UNIT(obj);
    size = gwy_serialize_get_struct_size(GWY_SI_UNIT_TYPE_NAME, 0, NULL);
    /* Just estimate */
    size += 20*si_unit->units->len;

    return size;
}

static GObject*
gwy_si_unit_deserialize(const guchar *buffer,
                        gsize size,
                        gsize *position)
{
    gchar *unitstr = NULL;
    GwySerializeSpec spec[] = {
        { 's', "unitstr", &unitstr, NULL, },
    };
    GwySIUnit *si_unit;

    g_return_val_if_fail(buffer, NULL);

    if (!gwy_serialize_unpack_object_struct(buffer, size, position,
                                            GWY_SI_UNIT_TYPE_NAME,
                                            G_N_ELEMENTS(spec), spec)) {
        return NULL;
    }

    if (unitstr && !*unitstr) {
        g_free(unitstr);
        unitstr = NULL;
    }
    si_unit = gwy_si_unit_new(unitstr);
    g_free(unitstr);

    return (GObject*)si_unit;
}


static GObject*
gwy_si_unit_duplicate_real(GObject *object)
{
    GwySIUnit *si_unit, *duplicate;

    g_return_val_if_fail(GWY_IS_SI_UNIT(object), NULL);
    si_unit = GWY_SI_UNIT(object);
    duplicate = gwy_si_unit_new_parse("", NULL);
    duplicate->power10 = si_unit->power10;
    g_array_append_vals(duplicate->units,
                        si_unit->units->data, si_unit->units->len);

    return (GObject*)duplicate;
}

static void
gwy_si_unit_clone_real(GObject *source, GObject *copy)
{
    GwySIUnit *si_unit, *clone;

    g_return_if_fail(GWY_IS_SI_UNIT(source));
    g_return_if_fail(GWY_IS_SI_UNIT(copy));

    si_unit = GWY_SI_UNIT(source);
    clone = GWY_SI_UNIT(copy);
    if (gwy_si_unit_equal(si_unit, clone))
        return;

    g_array_set_size(clone->units, 0);
    g_array_append_vals(clone->units,
                        si_unit->units->data, si_unit->units->len);
    clone->power10 = si_unit->power10;
    g_signal_emit(copy, si_unit_signals[VALUE_CHANGED], 0);
}

/**
 * gwy_si_unit_new:
 * @unit_string: Unit string (it can be %NULL for an empty unit).
 *
 * Creates a new SI unit from string representation.
 *
 * Unit string represents unit with no prefixes
 * (e. g. "m", "N", "A", etc.)
 *
 * Returns: A new SI unit.
 **/
GwySIUnit*
gwy_si_unit_new(const char *unit_string)
{
    return gwy_si_unit_new_parse(unit_string, NULL);
}

/**
 * gwy_si_unit_new_parse:
 * @unit_string: Unit string (it can be %NULL for an empty unit).
 * @power10: Where power of 10 should be stored (or %NULL).
 *
 * Creates a new SI unit from string representation.
 *
 * This is a more powerful version of gwy_si_unit_new(): @unit_string may
 * be a relatively complex unit, with prefixes, like "pA/s" or "km^2".
 * Beside conversion to a base SI unit like "A/s" or "m^2" it also computes
 * the power of 10 one has to multiply the base unit with to get an equivalent
 * of @unit_string.
 *
 * For example, for <literal>"pA/s"</literal> it will store -12 to @power10
 * because 1 pA/s is 1e-12 A/s, for <literal>"km^2"</literal> it will store 6
 * to @power10 because 1 km^2 is 1e6 m^2.
 *
 * Returns: A new SI unit.
 **/
GwySIUnit*
gwy_si_unit_new_parse(const char *unit_string,
                      gint *power10)
{
    GwySIUnit *siunit;

    gwy_debug("");
    siunit = g_object_new(GWY_TYPE_SI_UNIT, NULL);
    siunit->units = g_array_new(FALSE, FALSE, sizeof(GwySimpleUnit));
    gwy_si_unit_parse(siunit, unit_string);
    if (power10)
        *power10 = siunit->power10;

    return siunit;
}

/**
 * gwy_si_unit_set_from_string:
 * @siunit: An SI unit.
 * @unit_string: Unit string to set @siunit from (it can be %NULL for an empty
 *               unit).
 *
 * Sets string that represents unit.
 *
 * It must be base unit with no prefixes (e. g. "m", "N", "A", etc.).
 **/
void
gwy_si_unit_set_from_string(GwySIUnit *siunit,
                            const gchar *unit_string)
{
    g_return_if_fail(GWY_IS_SI_UNIT(siunit));
    gwy_si_unit_set_from_string_parse(siunit, unit_string, NULL);
}

/**
 * gwy_si_unit_set_from_string_parse:
 * @siunit: An SI unit.
 * @unit_string: Unit string to set @siunit from (it can be %NULL for an empty
 *               unit).
 * @power10: Where power of 10 should be stored (or %NULL).
 *
 * Changes an SI unit according to string representation.
 *
 * This is a more powerful version of gwy_si_unit_set_from_string(), please
 * see gwy_si_unit_new_parse() for some discussion.
 **/
void
gwy_si_unit_set_from_string_parse(GwySIUnit *siunit,
                                  const gchar *unit_string,
                                  gint *power10)
{
    g_return_if_fail(GWY_IS_SI_UNIT(siunit));

    gwy_si_unit_parse(siunit, unit_string);
    if (power10)
        *power10 = siunit->power10;

    g_signal_emit(siunit, si_unit_signals[VALUE_CHANGED], 0);
}

static inline const GwySIStyleSpec*
gwy_si_unit_find_style_spec(GwySIUnitFormatStyle style)
{
    if ((guint)style > GWY_SI_UNIT_FORMAT_TEX) {
        g_warning("Invalid format style");
        style = GWY_SI_UNIT_FORMAT_PLAIN;
    }

    return format_styles[style];
}

/**
 * gwy_si_unit_get_string:
 * @siunit: An SI unit.
 * @style: Unit format style.
 *
 * Obtains string representing a SI unit.
 *
 * Returns: A newly allocated string that represents the base unit (with no
 *          prefixes).
 **/
gchar*
gwy_si_unit_get_string(GwySIUnit *siunit,
                       GwySIUnitFormatStyle style)
{
    GString *string;
    gchar *s;

    g_return_val_if_fail(GWY_IS_SI_UNIT(siunit), NULL);
    siunit->power10 = 0;

    string = gwy_si_unit_format(siunit, gwy_si_unit_find_style_spec(style),
                                NULL);
    s = string->str;
    g_string_free(string, FALSE);

    return s;
}

/**
 * gwy_si_unit_get_format_for_power10:
 * @siunit: An SI unit.
 * @style: Unit format style.
 * @power10: Power of 10, in the same sense as gwy_si_unit_new_parse()
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
GwySIValueFormat*
gwy_si_unit_get_format_for_power10(GwySIUnit *siunit,
                                   GwySIUnitFormatStyle style,
                                   gint power10,
                                   GwySIValueFormat *format)
{
    const GwySIStyleSpec *spec;

    g_return_val_if_fail(GWY_IS_SI_UNIT(siunit), NULL);

    spec = gwy_si_unit_find_style_spec(style);
    if (!format)
        format = (GwySIValueFormat*)g_new0(GwySIValueFormat, 1);

    siunit->power10 = power10;
    format->magnitude = pow10(power10);
    format->units_gstring = gwy_si_unit_format(siunit, spec,
                                               format->units_gstring);
    format->units = format->units_gstring->str;

    return format;
}

/**
 * gwy_si_unit_get_format:
 * @siunit: An SI unit.
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
GwySIValueFormat*
gwy_si_unit_get_format(GwySIUnit *siunit,
                       GwySIUnitFormatStyle style,
                       gdouble value,
                       GwySIValueFormat *format)
{
    const GwySIStyleSpec *spec;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_SI_UNIT(siunit), NULL);

    spec = gwy_si_unit_find_style_spec(style);
    if (!format)
        format = (GwySIValueFormat*)g_new0(GwySIValueFormat, 1);

    value = fabs(value);
    if (!value) {
        format->magnitude = 1;
        format->precision = 2;
    }
    else
        format->magnitude = gwy_math_humanize_numbers(value/36, value,
                                                      &format->precision);
    siunit->power10 = ROUND(log10(format->magnitude));
    format->units_gstring = gwy_si_unit_format(siunit, spec,
                                               format->units_gstring);
    format->units = format->units_gstring->str;

    return format;
}

/**
 * gwy_si_unit_get_format_with_resolution:
 * @siunit: A SI unit.
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
GwySIValueFormat*
gwy_si_unit_get_format_with_resolution(GwySIUnit *siunit,
                                       GwySIUnitFormatStyle style,
                                       gdouble maximum,
                                       gdouble resolution,
                                       GwySIValueFormat *format)
{
    const GwySIStyleSpec *spec;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_SI_UNIT(siunit), NULL);

    spec = gwy_si_unit_find_style_spec(style);
    if (!format)
        format = (GwySIValueFormat*)g_new0(GwySIValueFormat, 1);

    maximum = fabs(maximum);
    resolution = fabs(resolution);
    if (!maximum) {
        format->magnitude = 1;
        format->precision = 2;
    }
    else
        format->magnitude = gwy_math_humanize_numbers(resolution, maximum,
                                                      &format->precision);
    siunit->power10 = ROUND(log10(format->magnitude));
    format->units_gstring = gwy_si_unit_format(siunit, spec,
                                               format->units_gstring);
    format->units = format->units_gstring->str;

    return format;
}

/**
 * gwy_si_unit_get_format_with_digits:
 * @siunit: A SI unit.
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
GwySIValueFormat*
gwy_si_unit_get_format_with_digits(GwySIUnit *siunit,
                                   GwySIUnitFormatStyle style,
                                   gdouble maximum,
                                   gint sdigits,
                                   GwySIValueFormat *format)
{
    const GwySIStyleSpec *spec;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_SI_UNIT(siunit), NULL);

    spec = gwy_si_unit_find_style_spec(style);
    if (!format)
        format = (GwySIValueFormat*)g_new0(GwySIValueFormat, 1);

    maximum = fabs(maximum);
    if (!maximum) {
        format->magnitude = 1;
        format->precision = sdigits;
    }
    else
        format->magnitude
            = gwy_math_humanize_numbers(maximum/pow10(sdigits),
                                        maximum, &format->precision);
    siunit->power10 = ROUND(log10(format->magnitude));
    format->units_gstring = gwy_si_unit_format(siunit, spec,
                                               format->units_gstring);
    format->units = format->units_gstring->str;

    return format;
}

/**
 * gwy_si_unit_value_format_free:
 * @format: A value format to free.
 *
 * Frees a value format structure.
 **/
void
gwy_si_unit_value_format_free(GwySIValueFormat *format)
{
    if (format->units_gstring)
        g_string_free(format->units_gstring, TRUE);
    g_free(format);
}

/**
 * gwy_si_unit_value_format_set_units:
 * @format: A value format to set units of.
 * @units: The units string.
 *
 * Sets the units field of a value format structure.
 *
 * This function keeps the @units and @units_gstring fields consistent.
 **/
void
gwy_si_unit_value_format_set_units(GwySIValueFormat *format,
                                   const gchar *units)
{
    if (!format->units_gstring)
        format->units_gstring = g_string_new(units);
    else
        g_string_assign(format->units_gstring, units);

    format->units = format->units_gstring->str;
}

/**
 * gwy_si_unit_equal:
 * @siunit1: First unit.
 * @siunit2: Second unit.
 *
 * Checks whether two SI units are equal.
 *
 * Returns: %TRUE if units are equal.
 **/
gboolean
gwy_si_unit_equal(GwySIUnit *siunit1, GwySIUnit *siunit2)
{
    guint i, j;

    if (siunit2 == siunit1)
        return TRUE;

    if (siunit2->units->len != siunit1->units->len)
        return FALSE;

    for (i = 0; i < siunit1->units->len; i++) {
        GwySimpleUnit *unit = &g_array_index(siunit1->units, GwySimpleUnit, i);

        for (j = 0; j < siunit2->units->len; j++) {
            if (g_array_index(siunit2->units, GwySimpleUnit, j).unit
                == unit->unit) {
                if (g_array_index(siunit2->units, GwySimpleUnit, j).power
                    != unit->power)
                    return FALSE;
                break;
            }
        }
        if (j == siunit2->units->len)
            return FALSE;
    }

    return TRUE;
}


static gboolean
gwy_si_unit_parse(GwySIUnit *siunit,
                  const gchar *string)
{
    GwySimpleUnit unit;
    gdouble q;
    const gchar *end;
    gchar *p, *e;
    gint n, i, pfpower;
    GString *buf;
    gboolean dividing = FALSE;

    g_array_set_size(siunit->units, 0);
    siunit->power10 = 0;

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
        siunit->power10 = ROUND(log10(q));
        if (q <= 0 || fabs(log(q/pow10(siunit->power10))) > 1e-14) {
            g_warning("Bad multiplier %g", q);
            siunit->power10 = 0;
        }
        else if (g_str_has_prefix(string, "<sup>")) {
            string += strlen("<sup>");
            n = strtol(string, (gchar**)&end, 10);
            if (end == string)
                g_warning("Bad exponent %s", string);
            else if (!g_str_has_prefix(end, "</sup>"))
                g_warning("Expected </sup> after exponent");
            else
                siunit->power10 *= n;
            string = end;
        }
        else if (string[0] == '^') {
            string++;
            n = strtol(string, (gchar**)&end, 10);
            if (end == string)
                g_warning("Bad exponent %s", string);
            else
                siunit->power10 *= n;
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
        else if (buf->str[0] == '\305' && !buf->str[1])
            g_string_assign(buf, "Å");
        else if (gwy_strequal(buf->str, "Å"))
            g_string_assign(buf, "Å");
        else if (gwy_strequal(buf->str, "AA"))
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
            siunit->power10 += unit.power * pfpower;
        }
        else if (!g_ascii_isalpha(buf->str[0]) && (guchar)buf->str[0] < 128)
            g_warning("Invalid base unit: %s", buf->str);
        else {
            /* append it */
            unit.unit = g_quark_from_string(buf->str);
            if (dividing)
                unit.power = -unit.power;
            gwy_debug("<%s:%u> %d\n", buf->str, unit.unit, unit.power);
            siunit->power10 += unit.power * pfpower;
            g_array_append_val(siunit->units, unit);
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

    gwy_si_unit_canonicalize(siunit);

    return TRUE;
}

/**
 * gwy_si_unit_multiply:
 * @siunit1: An SI unit.
 * @siunit2: An SI unit.
 * @result:  An SI unit to set to product of @siunit1 and @siunit2.  It is
 *           safe to pass one of @siunit1, @siunit2. It can be %NULL
 *           too, a new SI unit is created then and returned.
 *
 * Multiplies two SI units.
 *
 * Returns: When @result is %NULL, a newly created SI unit that has to be
 *          dereferenced when no longer used later.  Otherwise @result itself
 *          is simply returned, its reference count is NOT increased.
 **/
GwySIUnit*
gwy_si_unit_multiply(GwySIUnit *siunit1,
                     GwySIUnit *siunit2,
                     GwySIUnit *result)
{
    g_return_val_if_fail(GWY_IS_SI_UNIT(siunit1), NULL);
    g_return_val_if_fail(GWY_IS_SI_UNIT(siunit2), NULL);
    g_return_val_if_fail(!result || GWY_IS_SI_UNIT(result), NULL);

    if (!result)
        result = gwy_si_unit_new(NULL);

    return gwy_si_unit_power_multiply(siunit1, 1, siunit2, 1, result);
}

/**
 * gwy_si_unit_divide:
 * @siunit1: An SI unit.
 * @siunit2: An SI unit.
 * @result:  An SI unit to set to quotient of @siunit1 and @siunit2.  It is
 *           safe to pass one of @siunit1, @siunit2. It can be %NULL
 *           too, a new SI unit is created then and returned.
 *
 * Divides two SI units.
 *
 * Returns: When @result is %NULL, a newly created SI unit that has to be
 *          dereferenced when no longer used later.  Otherwise @result itself
 *          is simply returned, its reference count is NOT increased.
 **/
GwySIUnit*
gwy_si_unit_divide(GwySIUnit *siunit1,
                   GwySIUnit *siunit2,
                   GwySIUnit *result)
{
    g_return_val_if_fail(GWY_IS_SI_UNIT(siunit1), NULL);
    g_return_val_if_fail(GWY_IS_SI_UNIT(siunit2), NULL);
    g_return_val_if_fail(!result || GWY_IS_SI_UNIT(result), NULL);

    if (!result)
        result = gwy_si_unit_new(NULL);

    return gwy_si_unit_power_multiply(siunit1, 1, siunit2, -1, result);
}

/**
 * gwy_si_unit_power:
 * @siunit: An SI unit.
 * @power: Power to power @siunit to.
 * @result:  An SI unit to set to power of @siunit.  It is safe to pass
 *           @siunit itself.  It can be %NULL too, a new SI unit is created
 *           then and returned.
 *
 * Computes a power of an SI unit.
 *
 * Returns: When @result is %NULL, a newly created SI unit that has to be
 *          dereferenced when no longer used later.  Otherwise @result itself
 *          is simply returned, its reference count is NOT increased.
 **/
GwySIUnit*
gwy_si_unit_power(GwySIUnit *siunit,
                  gint power,
                  GwySIUnit *result)
{
    g_return_val_if_fail(GWY_IS_SI_UNIT(siunit), NULL);
    g_return_val_if_fail(!result || GWY_IS_SI_UNIT(result), NULL);

    if (!result)
        result = gwy_si_unit_new(NULL);

    gwy_si_unit_power_real(siunit, power, result);
    g_signal_emit(result, si_unit_signals[VALUE_CHANGED], 0);

    return result;
}

static GwySIUnit*
gwy_si_unit_power_real(GwySIUnit *siunit,
                       gint power,
                       GwySIUnit *result)
{
    GArray *units;
    GwySimpleUnit *unit;
    gint j;

    units = g_array_new(FALSE, FALSE, sizeof(GwySimpleUnit));
    result->power10 = power*siunit->power10;

    if (power) {
        g_array_append_vals(units, siunit->units->data, siunit->units->len);
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

static GwySIUnit*
gwy_si_unit_power_multiply(GwySIUnit *siunit1,
                           gint power1,
                           GwySIUnit *siunit2,
                           gint power2,
                           GwySIUnit *result)
{
    GwySimpleUnit *unit, *unit2;
    gint i, j;

    if (siunit1->units->len < siunit2->units->len || !power1) {
        GWY_SWAP(GwySIUnit*, siunit1, siunit2);
        GWY_SWAP(gint, power1, power2);
    }
    gwy_si_unit_power_real(siunit1, power1, result);
    if (!power2) {
        gwy_si_unit_canonicalize(result);
        return result;
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
    gwy_si_unit_canonicalize(result);
    g_signal_emit(result, si_unit_signals[VALUE_CHANGED], 0);

    return result;
}

static GwySIUnit*
gwy_si_unit_canonicalize(GwySIUnit *siunit)
{
    GwySimpleUnit *dst, *src;
    gint i, j;

    /* consolidate multiple occurences of the same unit */
    i = 0;
    while (i < siunit->units->len) {
        src = &g_array_index(siunit->units, GwySimpleUnit, i);

        for (j = 0; j < i; j++) {
            dst = &g_array_index(siunit->units, GwySimpleUnit, j);
            if (src->unit == dst->unit) {
                dst->power += src->power;
                g_array_remove_index(siunit->units, i);
                break;
            }
        }

        if (j == i)
            i++;
    }

    /* remove units with zero power */
    i = 0;
    while (i < siunit->units->len) {
        if (g_array_index(siunit->units, GwySimpleUnit, i).power)
            i++;
        else {
            g_array_remove_index(siunit->units, i);
        }
    }

    return siunit;
}

static GString*
gwy_si_unit_format(GwySIUnit *siunit,
                   const GwySIStyleSpec *fs,
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
    if (siunit->units->len > 1) {
        for (i = 0; i < siunit->units->len; i++) {
            unit = &g_array_index(siunit->units, GwySimpleUnit, i);
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
    if (siunit->power10) {
        for (i = 0; i < siunit->units->len; i++) {
            if (i == move_me_to_end)
                continue;
            unit = &g_array_index(siunit->units, GwySimpleUnit, i);
            if (siunit->power10 % (3*abs(unit->power)) == 0) {
                prefix_bearer = i;
                break;
            }
        }
    }
    if (siunit->power10 && prefix_bearer < 0 && move_me_to_end >= 0) {
        unit = &g_array_index(siunit->units, GwySimpleUnit, move_me_to_end);
        if (siunit->power10 % (3*abs(unit->power)) == 0)
            prefix_bearer = move_me_to_end;
    }
    /* check whether we are not out of prefix range */
    if (prefix_bearer >= 0) {
        unit = &g_array_index(siunit->units, GwySimpleUnit, prefix_bearer);
        prefix = gwy_si_unit_prefix(siunit->power10/unit->power);
        if (!prefix)
            prefix_bearer = -1;
    }

    /* if we were unable to place the prefix, we must add a power of 10 */
    if (siunit->power10 && prefix_bearer < 0) {
        switch (siunit->power10) {
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
            if (fs->power10_prefix)
                g_string_append(string, fs->power10_prefix);
            g_string_append_printf(string, "%d", siunit->power10);
            if (fs->power_suffix)
                g_string_append(string, fs->power_suffix);
            break;
        }
        if (fs->power_unit_separator && siunit->units->len)
            g_string_append(string, fs->power_unit_separator);
    }

    /* append units */
    for (i = 0; i < siunit->units->len; i++) {
        if (i == move_me_to_end)
            continue;
        if (i > 1 || (i && move_me_to_end)) {
            g_string_append(string, fs->unit_times);
        }
        unit = &g_array_index(siunit->units, GwySimpleUnit, i);
        if (i == prefix_bearer)
            g_string_append(string, prefix);
        g_string_append(string, g_quark_to_string(unit->unit));
        if (unit->power != 1) {
            if (fs->power_prefix)
                g_string_append(string, fs->power_prefix);
            g_string_append_printf(string, "%d", unit->power);
            if (fs->power_suffix)
                g_string_append(string, fs->power_suffix);
        }
    }
    if (move_me_to_end >= 0) {
        g_string_append(string, fs->unit_division);
        unit = &g_array_index(siunit->units, GwySimpleUnit, move_me_to_end);
        if (move_me_to_end == prefix_bearer)
            g_string_append(string, prefix);
        g_string_append(string, g_quark_to_string(unit->unit));
        if (unit->power != -1) {
            if (fs->power_prefix)
                g_string_append(string, fs->power_prefix);
            g_string_append_printf(string, "%d", -unit->power);
            if (fs->power_suffix)
                g_string_append(string, fs->power_suffix);
        }
    }

    return string;
}

static const gchar*
gwy_si_unit_prefix(gint power)
{
    gint i;

    for (i = 0; i < G_N_ELEMENTS(SI_prefixes); i++) {
        if (SI_prefixes[i].power10 == power)
            return SI_prefixes[i].prefix;
    }
    return NULL;
}

/************************** Documentation ****************************/

/**
 * SECTION:gwysiunit
 * @title: GwySIUnit
 * @short_description: SI unit representation, physical quantitiy formatting
 *
 * #GwySIUnit object represents a physical SI unit (or any other unit), it can
 * be created from a unit string with gwy_si_unit_new().
 *
 * GwySIUnit is also responsible for prefixes selection and generally
 * formatting of physical quantities (see also gwymath for pure number
 * formatting functions).  There are several functions computing value format
 * (as a #GwySIValueFormat structure) with given resolution --
 * gwy_si_unit_get_format_with_resolution(), or number of significant digits --
 * gwy_si_unit_get_format_with_digits().
 **/

/**
 * GwySIUnit:
 *
 * The #GwySIUnit struct contains private data only and should be accessed
 * using the functions below.
 **/

/**
 * GwySIUnitFormatStyle:
 * @GWY_SI_UNIT_FORMAT_NONE: No units.  This value is unused by #GwySIUnit
 *                           itself and must not be requested as a format
 *                           style.
 * @GWY_SI_UNIT_FORMAT_PLAIN: Plain style, as one would use on a text terminal.
 * @GWY_SI_UNIT_FORMAT_MARKUP: Pango markup, for units usable standalone.
 * @GWY_SI_UNIT_FORMAT_VFMARKUP: Pango markup, for units directly after value.
 *                               Differs from %GWY_SI_UNIT_FORMAT_MARKUP by
 *                               multiplication sign at the start of units
 *                               (where appropriate).
 * @GWY_SI_UNIT_FORMAT_TEX: Representation that can be typeset by TeX.
 *
 * Physical quantity formatting style.
 **/

/**
 * GwySIValueFormat:
 * @magnitude: Number to divide a quantity by (a power of 1000).
 * @precision: Number of decimal places to format a quantity to.
 * @units: Units to put after quantity divided by @magnitude.  This is actually
 *         an alias to @units_gstring->str.
 * @units_gstring: #GString used to represent @units internally.
 *
 * A physical quantity formatting information.
 *
 * The @magnitude and @precision fields can be directly modified if necessary.
 * Units must be always set with gwy_si_unit_value_format_set_units() to update
 * the internal representation properly.
 */

/**
 * gwy_si_unit_duplicate:
 * @siunit: An SI unit to duplicate.
 *
 * Convenience macro doing gwy_serializable_duplicate() with all the necessary
 * typecasting.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
