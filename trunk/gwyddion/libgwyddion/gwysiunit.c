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

#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwydebugobjects.h>
#include <libgwyddion/gwyserializable.h>
#include <libgwyddion/gwysiunit.h>

#define GWY_SI_UNIT_TYPE_NAME "GwySIUnit"

static void        gwy_si_unit_class_init        (GwySIUnitClass *klass);
static void        gwy_si_unit_init              (GwySIUnit *si_unit);
static void        gwy_si_unit_finalize          (GObject *object);
static void        gwy_si_unit_serializable_init (GwySerializableIface *iface);
static GByteArray* gwy_si_unit_serialize         (GObject *obj,
                                                  GByteArray *buffer);
static GObject*    gwy_si_unit_deserialize       (const guchar *buffer,
                                                 gsize size,
                                                 gsize *position);
static GObject*    gwy_si_unit_duplicate         (GObject *object);


static GObjectClass *parent_class = NULL;

GType
gwy_si_unit_get_type(void)
{
    static GType gwy_si_unit_type = 0;

    if (!gwy_si_unit_type) {
        static const GTypeInfo gwy_si_unit_info = {
            sizeof(GwySIUnitClass),
            NULL,
            NULL,
            (GClassInitFunc)gwy_si_unit_class_init,
            NULL,
            NULL,
            sizeof(GwySIUnit),
            0,
            (GInstanceInitFunc)gwy_si_unit_init,
            NULL,
        };

        GInterfaceInfo gwy_serializable_info = {
            (GInterfaceInitFunc)gwy_si_unit_serializable_init,
            NULL,
            NULL
        };

        gwy_debug("");
        gwy_si_unit_type = g_type_register_static(G_TYPE_OBJECT,
                                                  GWY_SI_UNIT_TYPE_NAME,
                                                  &gwy_si_unit_info,
                                                  0);
        g_type_add_interface_static(gwy_si_unit_type,
                                    GWY_TYPE_SERIALIZABLE,
                                    &gwy_serializable_info);
    }

    return gwy_si_unit_type;
}

static void
gwy_si_unit_serializable_init(GwySerializableIface *iface)
{
    gwy_debug("");
    /* initialize stuff */
    iface->serialize = gwy_si_unit_serialize;
    iface->deserialize = gwy_si_unit_deserialize;
    iface->duplicate = gwy_si_unit_duplicate;
}


static void
gwy_si_unit_class_init(GwySIUnitClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gwy_debug("");

    parent_class = g_type_class_peek_parent(klass);

    gobject_class->finalize = gwy_si_unit_finalize;
}

static void
gwy_si_unit_init(GwySIUnit *si_unit)
{
    gwy_debug("");
    gwy_debug_objects_creation((GObject*)si_unit);
    si_unit->unitstr = NULL;
}

static void
gwy_si_unit_finalize(GObject *object)
{
    GwySIUnit *si_unit = (GwySIUnit*)object;

    gwy_debug("");
    g_free(si_unit->unitstr);

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

static GByteArray*
gwy_si_unit_serialize(GObject *obj,
                      GByteArray *buffer)
{
    GwySIUnit *si_unit;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_SI_UNIT(obj), NULL);

    si_unit = GWY_SI_UNIT(obj);
    {
        const gchar *unitstr = si_unit->unitstr ? si_unit->unitstr : "";
        GwySerializeSpec spec[] = {
            { 's', "unitstr", &unitstr, NULL, },
        };
        gwy_debug("unitstr = <%s>", unitstr);
        return gwy_serialize_pack_object_struct(buffer,
                                                GWY_SI_UNIT_TYPE_NAME,
                                                G_N_ELEMENTS(spec), spec);
    }
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
    GObject *si_unit;

    gwy_debug("");
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

    return si_unit;
}


static GObject*
gwy_si_unit_duplicate(GObject *object)
{
    GwySIUnit *si_unit;
    GObject *duplicate;

    g_return_val_if_fail(GWY_IS_SI_UNIT(object), NULL);
    si_unit = GWY_SI_UNIT(object);
    duplicate = gwy_si_unit_new(si_unit->unitstr);

    return duplicate;
}

/**
 * gwy_si_unit_new:
 * @unit_string: unit string
 *
 * Unit string represents unit with no prefixes
 * (e. g. "m", "N", "A", etc.)
 *
 * Returns: a new GwySiUnit with a given string
 **/
GObject*
gwy_si_unit_new(const char *unit_string)
{
    GwySIUnit *siunit;

    gwy_debug("");
    siunit = g_object_new(GWY_TYPE_SI_UNIT, NULL); 
    if (unit_string == NULL || strcmp(unit_string, "") == 0)
        siunit->unitstr = NULL;
    else
        siunit->unitstr = g_strdup(unit_string);

    return (GObject*)siunit;
}

/**
 * gwy_si_unit_set_unit_string:
 * @siunit: GwySiUnit
 * @unit_string: unit string to be set
 *
 * Sets string that represents unit. It is the
 * unit with no prefixes (e. g. "m", "N", "A", etc.)
 **/
void
gwy_si_unit_set_unit_string(GwySIUnit *siunit, char *unit_string)
{
    gwy_debug("");

    g_free(siunit->unitstr);
    siunit->unitstr = g_strdup(unit_string);
}

/**
 * gwy_si_unit_get_unit_string:
 * @siunit: GwySiUnit
 *
 *
 *
 * Returns: string that represents unit (with no prefixes)
 **/
gchar*
gwy_si_unit_get_unit_string(GwySIUnit *siunit)
{
    gwy_debug("");
    return g_strdup(siunit->unitstr);
}

static void
format_regular(GwySIUnit *siunit, gdouble value, GwySIValueFormat *format)
{
   gwy_debug("");
   format->magnitude = pow(10, 3*ROUND(((gint)(log10(fabs(value))))/3.0) - 3);
   format->units = (gchar*)g_malloc((strlen(siunit->unitstr)+1)*sizeof(gchar));
   format->units = g_strconcat(gwy_math_SI_prefix(format->magnitude), siunit->unitstr, NULL);
}

/**
 * gwy_si_unit_get_format:
 * @siunit: GwySiUnit
 * @value: input value
 * @format: returned number representation parameters
 *
 * Finds reasonable representation for a number.
 * This means that number @value should
 * be written as @value / @number->magnitude [@number->units].
 *
 * Returns: The value format.  If @format was %NULL, a newly allocated format
 *          is returned, otherwise (modified) @format itself is returned.
 **/
GwySIValueFormat*
gwy_si_unit_get_format(GwySIUnit *siunit,
                       gdouble value,
                       GwySIValueFormat *format)
{
    char num[23];

    gwy_debug("");
    if (format==NULL) {
        format = (GwySIValueFormat *)g_new(GwySIValueFormat, 1);
        format->units = NULL;
    }

    if (format->units)
        g_free(format->units);
   
    if (siunit->unitstr!=NULL && (strlen(siunit->unitstr)<2 && strlen(siunit->unitstr)!=0))
    {
        format_regular(siunit, value, format);
    }
    else
    {
        if (value==0) format->magnitude = 1;
//        else format->magnitude = pow(10, (gint)(log10(fabs(value))));
	else format->magnitude = pow(10, 3*ROUND(((gint)(log10(fabs(value))))/3.0));

       if (siunit->unitstr!=NULL)
            format->units = (gchar*)g_malloc((strlen(siunit->unitstr)+23)*sizeof(gchar));
        else
            format->units = (gchar*)g_malloc(23*sizeof(gchar));
        
        if (fabs((gint)(3*ROUND(((gint)(log10(fabs(value))))/3.0))) > 2)
        {
            sprintf(num, "× 10<sup>%d</sup> ", (gint)(3*ROUND(((gint)(log10(fabs(value))))/3.0)));
            if (siunit->unitstr==NULL || strlen(siunit->unitstr)==0) 
            {
                format->units = strcpy(format->units, num);
            }
            else format->units = g_strconcat(num, siunit->unitstr, NULL);
         }
        else
        {
            if (siunit->unitstr==NULL)
            {
                format->units = strcpy(format->units, " ");
            }
            else
                format->units = strcpy(format->units, gwy_si_unit_get_unit_string(siunit));
        }

    }
    format->precision = 2;
    gwy_debug("unitstr = <%s>, units = <%s>", siunit->unitstr, format->units);
    return format;
}


/**
 * gwy_si_unit_get_format_with_resolution:
 * @siunit: A SI unit.
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
                                       gdouble maximum,
                                       gdouble resolution,
                                       GwySIValueFormat *format)
{
    gint prec;
    char num[23];
    gwy_debug("");
    g_return_val_if_fail(GWY_IS_SI_UNIT(siunit), NULL);

    if (format==NULL) {
        format = (GwySIValueFormat *)g_new(GwySIValueFormat, 1);
        format->units = NULL;
    }

    if (format->units)
    {
        g_free(format->units);
    }

    if (maximum == 0) format->magnitude = 1;
    else format->magnitude = gwy_math_humanize_numbers(resolution, maximum, &prec);
    format->precision = prec;

    if (siunit->unitstr!=NULL && (strlen(siunit->unitstr)<2 && strlen(siunit->unitstr)!=0))
    {
        format->units = (gchar*)g_malloc((strlen(siunit->unitstr)+strlen(gwy_math_SI_prefix(format->magnitude)))*sizeof(gchar));
        format->units = g_strconcat(gwy_math_SI_prefix(format->magnitude), siunit->unitstr, NULL);
    }
    else
    {
        if (siunit->unitstr!=NULL)
            format->units = (gchar*)g_malloc((strlen(siunit->unitstr)+23)*sizeof(gchar));
        else
            format->units = (gchar*)g_malloc(23*sizeof(gchar));
            
        if ((gint)(log10(fabs(format->magnitude))) != 0)
        {
            sprintf(num, "× 10<sup>%d</sup> ", (gint)(log10(fabs(format->magnitude))));
            if (siunit->unitstr==NULL || strlen(siunit->unitstr)==0) 
                format->units = strcpy(format->units, num);
            else format->units = g_strconcat(num, siunit->unitstr, NULL);
        }
        else
        {
            if (siunit->unitstr==NULL)
                format->units = strcpy(format->units, " ");
            else
                format->units = strcpy(format->units, gwy_si_unit_get_unit_string(siunit));
        }

    }

    gwy_debug("unitstr = <%s>, units = <%s>", siunit->unitstr, format->units);
    return format;
}

/**
 * gwy_si_unit_get_format_with_digits:
 * @siunit: A SI unit.
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
                                   gdouble maximum,
                                   gint sdigits,
                                   GwySIValueFormat *format)
{
    char num[23];
    gdouble realmag;

    gwy_debug("");
    if (format==NULL) {
        format = (GwySIValueFormat *)g_new(GwySIValueFormat, 1);
        format->units = NULL;
    }

    if (format->units)
    {
        g_free(format->units);
    }

    if (siunit->unitstr!=NULL && (strlen(siunit->unitstr)<2 && strlen(siunit->unitstr)!=0))
    {
        format->magnitude = pow(10, 3*ROUND(((gint)(log10(fabs(maximum))))/3.0));
        realmag = pow(10, (gint)(log10(fabs(maximum)))-1);

        if (ROUND((gdouble)format->magnitude/realmag)==10.0)
        {
            if (maximum/format->magnitude >= 1)
               format->precision = sdigits-1;
            else
               format->precision = sdigits;
        }
        else if (ROUND((gdouble)format->magnitude/realmag)==100.0)
        {
            if (maximum/format->magnitude >= 0.1)
                format->precision = sdigits;
            else
                format->precision = sdigits+1;
        }
         else if (ROUND((gdouble)format->magnitude/realmag)==1.0)
        {
            if (maximum/format->magnitude >= 10)
                format->precision = sdigits-2;
            else
                format->precision = sdigits-1;
        }
        else format->precision = sdigits+1;

        if (format->precision < 0) format->precision = 0;

        format->units = (gchar*)g_malloc((strlen(siunit->unitstr)+2)*sizeof(gchar));
        format->units = g_strconcat(gwy_math_SI_prefix(format->magnitude), siunit->unitstr, NULL);
    }
    else
    {
        format->magnitude = pow(10, (gint)(log10(fabs(maximum)))-1);

        if (siunit->unitstr!=NULL)
            format->units = (gchar*)g_malloc((strlen(siunit->unitstr)+23)*sizeof(gchar));
        else
            format->units = (gchar*)g_malloc(23*sizeof(gchar));
        
        if ((gint)(log10(fabs(format->magnitude))) != 0)
        {
            sprintf(num, "× 10<sup>%d</sup> ", (gint)(log10(fabs(format->magnitude))));
            if (siunit->unitstr==NULL || strlen(siunit->unitstr)==0) 
                format->units = strcpy(format->units, num);
            else format->units = g_strconcat(num, siunit->unitstr, NULL);
        }
        else
        {
            if (siunit->unitstr==NULL)
            {
                format->units = strcpy(format->units, " ");
            }
            else
                format->units = strcpy(format->units, gwy_si_unit_get_unit_string(siunit));
        }
        format->precision = 2;

    }
    gwy_debug("unitstr = <%s>, units = <%s>", siunit->unitstr, format->units);
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
    g_free(format->units);
    g_free(format);
}


/************************** Parser ***********************************/

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
    gint power10;
    GArray *units;
} GwySIUnit2;

static const GwyUnitTraits unit_traits[] = {
    { 1.0,   0 },    /* normal unit */
    { 1.0,   3 },    /* kilogram */
    { 0.0,   0 },    /* does not take prefixes */
};

static const struct {
    const gchar *prefix;
    gint power10;
}
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

GwySIUnit2*
gwy_si_unit_parse(const gchar *string)
{
    GwySIUnit2 *siunit;
    GwySimpleUnit unit;
    gdouble q;
    const gchar *end;
    gchar *p, *e;
    gint n, i, pfpower;
    GString *buf;
    gboolean dividing = FALSE;

    siunit = g_new0(GwySIUnit2, 1);
    siunit->units = g_array_new(FALSE, FALSE, sizeof(GwySimpleUnit));

    /* give up when it looks too wild */
    end = strpbrk(string,
                  "\177\001\002\003\004\005\006\007"
                  "\010\011\012\013\014\015\016\017"
                  "\020\021\022\023\024\025\026\027"
                  "\030\031\032\033\034\035\036\037"
                  "!#$&()*,:;=?@\\[]_`|{}");
    if (end) {
        g_warning("Invalid character 0x%02x", *end);
        return siunit;
    }

    /* may start with a multiplier */
    q = g_ascii_strtod(string, (gchar**)&end);
    if (end != string) {
        string = end;
        siunit->power10 = ROUND(log(q)/G_LN10);
        if (q <= 0 || fabs(log(q/exp(G_LN10*siunit->power10))) > 1e-14) {
            g_warning("Bad multiplier %g", q);
            siunit->power10 = 0;
        }
        else if (g_str_has_prefix(string, "<sup>")) {
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
            n = strtol(string, (gchar**)&end, 10);
            if (end == string)
                g_warning("Bad exponent %s", string);
            else
                siunit->power10 *= n;
            string = end;
        }
    }

    buf = g_string_new("");

    /* the rest are units */
    while (*string) {
        end = strpbrk(string, " /");
        if (!end)
            end = string + strlen(string);

        g_string_set_size(buf, 0);
        g_string_append_len(buf, string, end - string);

        /* fix sloppy notations */
        if (strcmp(buf->str, "\272")) {
            g_string_assign(buf, "deg");
        }

        /* TODO: scan known obscure units */
        unit.traits = 0;

        /* get prefix */
        pfpower = 0;
        if (strlen(buf->str) > 1) {
            for (i = 0; i < G_N_ELEMENTS(SI_prefixes); i++) {
                const gchar *pfx = SI_prefixes[i].prefix;

                if (g_str_has_prefix(buf->str, pfx)
                    && g_ascii_isalpha(buf->str[strlen(pfx)])) {
                    pfpower = SI_prefixes[i].power10;
                    g_string_erase(buf, 0, strlen(pfx));
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
            unit.power = strtol(p + strlen("<sup>"), &e, 10);
            if (e == p + strlen("<sup>") || *e) {
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

        /* elementary sanity */
        if (!g_utf8_validate(buf->str, -1, (const gchar**)&p)) {
            g_warning("Unit string is not valid UTF-8");
            g_string_truncate(buf, p - buf->str);
        }
        if (!buf->len)
            g_warning("Base unit cannot be empty: %s", string);
        if (!g_ascii_isalpha(buf->str[0]) && (guchar)buf->str[0] < 128)
            g_warning("Invalid base unit: %s", buf->str);
        else {
            /* append it */
            unit.unit = g_quark_from_string(buf->str);
            if (dividing)
                unit.power = -unit.power;
            siunit->power10 += unit.power * pfpower;
            g_array_append_val(siunit->units, unit);
        }

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

    return siunit;
}

/************************** Documentation ****************************/

/**
 * GwySIUnit:
 *
 * The #GwySIUnit struct contains private data only and should be accessed
 * using the functions below.
 **/

/**
 * GwySIValueFormat:
 * @magnitude: Number to divide a quantity by (a power of 1000).
 * @precision: Number of decimal places to format a quantity to.
 * @units: Units to put after quantity divided by @magnitude.
 *
 * A physical quantity formatting information.
 */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
