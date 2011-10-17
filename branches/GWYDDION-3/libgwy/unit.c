/*
 *  $Id$
 *  Copyright (C) 2009 David Nečas (Yeti).
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

#include "config.h"
#include <string.h>
#include <stdlib.h>
#include "libgwy/macros.h"
#include "libgwy/math.h"
#include "libgwy/serialize.h"
#include "libgwy/strfuncs.h"
#include "libgwy/unit.h"

#ifdef ENABLE_PARSE_WARNINGS
#define gwy_unit_warning g_warning
#else
#define gwy_unit_warning(...) /* */
#endif

#define simple_unit_index(a, i) g_array_index((a), GwySimpleUnit, (i))

enum { N_ITEMS = 1 };

enum {
    CHANGED,
    N_SIGNALS
};

typedef void (*AppendPowerFunc)(GString *str, gint power);

typedef struct {
    GQuark unit;
    gshort power;
} GwySimpleUnit;

typedef struct {
    const gchar *power10_times;
    const gchar *power10_open;
    const gchar *power_open;
    const gchar *power_close;
    const gchar *unit_times;
    const gchar *unit_division;
    const gchar *number_unit_glue;
    AppendPowerFunc append_power;
} GwyUnitStyleSpec;

struct _GwyUnitPrivate {
    GArray *units;
    gchar *serialize_str;
};

typedef struct _GwyUnitPrivate Unit;

static void         gwy_unit_finalize         (GObject *object);
static void         gwy_unit_serializable_init(GwySerializableInterface *iface);
static gsize        gwy_unit_n_items          (GwySerializable *serializable);
static gsize        gwy_unit_itemize          (GwySerializable *serializable,
                                               GwySerializableItems *items);
static void         gwy_unit_done             (GwySerializable *serializable);
static gboolean     gwy_unit_construct        (GwySerializable *serializable,
                                               GwySerializableItems *items,
                                               GwyErrorList **error_list);
static GObject*     gwy_unit_duplicate_impl   (GwySerializable *serializable);
static void         gwy_unit_assign_impl      (GwySerializable *destination,
                                               GwySerializable *source);

static gdouble      find_number_format        (gdouble step,
                                               gdouble maximum,
                                               guint *precision);
static gboolean     parse                     (Unit *unit,
                                               const gchar *string,
                                               gint *power10);
static void         power_impl                (Unit *unit,
                                               const Unit *op,
                                               gint power);
static void         canonicalize              (Unit *unit);
static const gchar* get_prefix                (gint power);
static void         append_power_plain        (GString *str,
                                               gint power);
static void         append_power_unicode      (GString *str,
                                               gint power);
static void         format_unit               (const Unit *unit,
                                               const GwyUnitStyleSpec *fs,
                                               gint power10,
                                               gchar **glue_retval,
                                               gchar **units_retval,
                                               gboolean retain_empty_strings);

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
    /* People are extremely creative when it comes to \mu replacements...
     * NB: The two μ below are different symbols. */
    { "µ",    -6  },
    { "μ",    -6  },
    { "~",    -6  },
    { "u",    -6  },
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
    "*", "10^", "^", NULL, " ", "/", " ",
    &append_power_plain,
};

static const GwyUnitStyleSpec format_style_unicode = {
    "×", "10", "", NULL, " ", "/", " ",
    &append_power_unicode,
};

static const GwyUnitStyleSpec format_style_pango = {
    "×", "10<sup>", "<sup>", "</sup>", " ", "/", " ",
    &append_power_plain,
};

static const GwyUnitStyleSpec format_style_TeX = {
    "\\times", "10^{", "^{", "}", "\\,", "/", "\\,",
    &append_power_plain,
};

static const GwyUnitStyleSpec *format_styles[] = {
    NULL,
    &format_style_plain,
    &format_style_unicode,
    &format_style_pango,
    &format_style_TeX,
};

static const GwySerializableItem serialize_items[N_ITEMS] = {
    { .name = "unitstr", .ctype = GWY_SERIALIZABLE_STRING, },
};

static guint unit_signals[N_SIGNALS];

G_DEFINE_TYPE_EXTENDED
    (GwyUnit, gwy_unit, G_TYPE_OBJECT, 0,
     GWY_IMPLEMENT_SERIALIZABLE(gwy_unit_serializable_init));

static void
gwy_unit_serializable_init(GwySerializableInterface *iface)
{
    iface->n_items   = gwy_unit_n_items;
    iface->itemize   = gwy_unit_itemize;
    iface->done      = gwy_unit_done;
    iface->construct = gwy_unit_construct;
    iface->duplicate = gwy_unit_duplicate_impl;
    iface->assign    = gwy_unit_assign_impl;
}

static void
gwy_unit_class_init(GwyUnitClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    g_type_class_add_private(klass, sizeof(Unit));

    gobject_class->finalize = gwy_unit_finalize;

    /**
     * GwyUnit::changed:
     * @gwyunit: The #GwyUnit which received the signal.
     *
     * The ::changed signal is emitted whenever unit changes.
     **/
    unit_signals[CHANGED]
        = g_signal_new_class_handler("changed",
                                     G_OBJECT_CLASS_TYPE(klass),
                                     G_SIGNAL_RUN_FIRST,
                                     NULL, NULL, NULL,
                                     g_cclosure_marshal_VOID__VOID,
                                     G_TYPE_NONE, 0);
}

static void
gwy_unit_init(GwyUnit *unit)
{
    unit->priv = G_TYPE_INSTANCE_GET_PRIVATE(unit, GWY_TYPE_UNIT, Unit);
    unit->priv->units = g_array_new(FALSE, FALSE, sizeof(GwySimpleUnit));
}

static void
gwy_unit_finalize(GObject *object)
{
    GwyUnit *unit = GWY_UNIT(object);
    g_array_free(unit->priv->units, TRUE);
    G_OBJECT_CLASS(gwy_unit_parent_class)->finalize(object);
}

static gsize
gwy_unit_n_items(G_GNUC_UNUSED GwySerializable *serializable)
{
    return N_ITEMS;
}

static gsize
gwy_unit_itemize(GwySerializable *serializable,
                 GwySerializableItems *items)
{
    GwyUnit *unit = GWY_UNIT(serializable);

    unit->priv->serialize_str = gwy_unit_to_string(unit,
                                                   GWY_VALUE_FORMAT_PLAIN);
    if (!*unit->priv->serialize_str) {
        GWY_FREE(unit->priv->serialize_str);
        return 0;
    }

    g_return_val_if_fail(items->len - items->n >= N_ITEMS, 0);

    GwySerializableItem *it = items->items + items->n;

    *it = serialize_items[0];
    it->value.v_string = unit->priv->serialize_str;
    it++, items->n++;

    return N_ITEMS;
}

static void
gwy_unit_done(GwySerializable *serializable)
{
    GwyUnit *unit = GWY_UNIT(serializable);
    GWY_FREE(unit->priv->serialize_str);
}

static gboolean
gwy_unit_construct(GwySerializable *serializable,
                   GwySerializableItems *items,
                   GwyErrorList **error_list)
{
    GwySerializableItem item = serialize_items[0];
    gwy_deserialize_filter_items(&item, N_ITEMS, items, NULL,
                                 "GwyUnit", error_list);

    GwyUnit *unit = GWY_UNIT(serializable);
    gwy_unit_set_from_string(unit, item.value.v_string, NULL);
    GWY_FREE(item.value.v_string);

    return TRUE;
}

static GObject*
gwy_unit_duplicate_impl(GwySerializable *serializable)
{
    GwyUnit *unit = GWY_UNIT(serializable);
    GArray *units = unit->priv->units;

    GwyUnit *duplicate = g_object_newv(GWY_TYPE_UNIT, 0, NULL);
    g_array_append_vals(duplicate->priv->units, units->data, units->len);

    return G_OBJECT(duplicate);
}

static void
gwy_unit_assign_impl(GwySerializable *destination,
                     GwySerializable *source)
{
    GwyUnit *dest = GWY_UNIT(destination);
    GwyUnit *src = GWY_UNIT(source);

    if (gwy_unit_equal(dest, src))
        return;

    g_array_set_size(dest->priv->units, 0);
    g_array_append_vals(dest->priv->units,
                        src->priv->units->data, src->priv->units->len);
    g_signal_emit(dest, unit_signals[CHANGED], 0);
}

/**
 * gwy_unit_new:
 *
 * Creates a new physical unit.
 *
 * Returns: A new physical unit.
 **/
GwyUnit*
gwy_unit_new(void)
{
    return g_object_newv(GWY_TYPE_UNIT, 0, NULL);
}

/**
 * gwy_unit_new_from_string:
 * @unit_string: Unit string.   It can be %NULL for an empty unit.
 * @power10: Location to store the power of 10, or %NULL to ignore.
 *
 * Creates a new physical unit from a string representation.
 *
 * Relatively complex notations are recognised in @unit_string, namely
 * prefixes, powers and inverse powers such as "pA/s" or "km^2".  Beside
 * conversion to the base unit such as "A/s" or "m^2" this constructor also
 * computes the power of 10 one has to multiply the base unit with to get an
 * equivalent of @unit_string.
 *
 * For example, for <literal>"pA/s"</literal> it will store -12 to @power10
 * because 1 pA/s is 10<superscript>-12</superscript> A/s, for
 * <literal>"km^2"</literal> it will store 6 to @power10 because 1
 * km<superscript>2</superscript> is 10<superscript>6</superscript>
 * m<superscript>2</superscript>.
 *
 * Returns: A new physical unit.
 **/
GwyUnit*
gwy_unit_new_from_string(const char *unit_string,
                         gint *power10)
{
    GwyUnit *unit = g_object_newv(GWY_TYPE_UNIT, 0, NULL);
    parse(unit->priv, unit_string, power10);

    return unit;
}

/**
 * gwy_unit_set_from_string:
 * @unit: A physical unit.
 * @unit_string: Unit string.  It can be %NULL for an empty unit.
 * @power10: Location to store the power of 10, or %NULL to ignore.
 *
 * Sets a physical unit from a string representation.
 *
 * See gwy_unit_new_from_string().
 **/
void
gwy_unit_set_from_string(GwyUnit *unit,
                         const gchar *unit_string,
                         gint *power10)
{
    g_return_if_fail(GWY_IS_UNIT(unit));
    parse(unit->priv, unit_string, power10);
    g_signal_emit(unit, unit_signals[CHANGED], 0);
}

static inline const GwyUnitStyleSpec*
find_style_spec(GwyValueFormatStyle style)
{
    if ((guint)style > GWY_VALUE_FORMAT_TEX) {
        g_warning("Invalid format style %u", style);
        style = GWY_VALUE_FORMAT_PLAIN;
    }

    return format_styles[style];
}

/**
 * gwy_unit_to_string:
 * @unit: A physical unit.
 * @style: Output format style.
 *
 * Obtains string representing a physical unit.
 *
 * Returns: A newly allocated string that represents the base unit (with no
 *          prefixes).
 **/
gchar*
gwy_unit_to_string(const GwyUnit *unit,
                   GwyValueFormatStyle style)
{
    g_return_val_if_fail(GWY_IS_UNIT(unit), NULL);
    gchar *units;
    format_unit(unit->priv, find_style_spec(style), 0, NULL, &units, TRUE);
    return units;
}

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
gwy_unit_equal(const GwyUnit *unit, const GwyUnit *op)
{
    g_return_val_if_fail(GWY_IS_UNIT(unit), FALSE);
    g_return_val_if_fail(GWY_IS_UNIT(op), FALSE);

    if (op == unit)
        return TRUE;

    Unit *priv = unit->priv, *privop = op->priv;

    if (privop->units->len != priv->units->len)
        return FALSE;

    for (guint i = 0; i < priv->units->len; i++) {
        const GwySimpleUnit *u = &simple_unit_index(priv->units, i);
        guint j;

        for (j = 0; j < privop->units->len; j++) {
            if (simple_unit_index(privop->units, j).unit == u->unit) {
                if (simple_unit_index(privop->units, j).power != u->power)
                    return FALSE;
                break;
            }
        }
        if (j == privop->units->len)
            return FALSE;
    }

    return TRUE;
}

static gboolean
parse(Unit *unit,
      const gchar *string,
      gint *ppower10)
{
    gdouble q;
    const gchar *end;
    gchar *p, *e, *utf8string = NULL;
    guint n, i;
    gint pfpower, power10;
    gboolean dividing = FALSE;

    g_array_set_size(unit->units, 0);
    power10 = 0;
    GWY_MAYBE_SET(ppower10, power10);

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
        gwy_unit_warning("Invalid character 0x%02x", *end);
        return FALSE;
    }

    /* If the string is not UTF-8, assume it's Latin 1.  This is what people
     * usually have in various files. */
    if (!g_utf8_validate(string, -1, NULL))
        string = utf8string = g_convert(string, -1, "UTF-8", "ISO-8859-1",
                                        NULL, NULL, NULL);

    /* may start with a multiplier, but it must be a power of 10 */
    while (g_ascii_isspace(string[0]))
        string++;

    if (string[0] == '*')
        string++;
    else if (strncmp(string, "×", sizeof("×")-1) == 0)
        string += sizeof("×")-1;

    q = g_ascii_strtod(string, (gchar**)&end);
    if (end != string) {
        string = end;
        power10 = gwy_round(log10(q));
        if (q <= 0 || fabs(log(q/gwy_powi(10.0, power10))) > 1e-13) {
            gwy_unit_warning("Bad multiplier %g", q);
            power10 = 0;
        }
        else if (g_str_has_prefix(string, "<sup>")) {
            string += strlen("<sup>");
            n = strtol(string, (gchar**)&end, 10);
            if (end == string)
                gwy_unit_warning("Bad exponent %s", string);
            else if (!g_str_has_prefix(end, "</sup>"))
                gwy_unit_warning("Expected </sup> after exponent");
            else
                power10 *= n;
            string = end;
        }
        else if (string[0] == '^') {
            string++;
            n = strtol(string, (gchar**)&end, 10);
            if (end == string)
                gwy_unit_warning("Bad exponent %s", string);
            else
                power10 *= n;
            string = end;
        }
    }
    while (g_ascii_isspace(*string))
        string++;

    GString *buf = g_string_new(NULL);

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

        if (end == string)
            break;

        g_string_set_size(buf, 0);
        g_string_append_len(buf, string, end - string);

        /* fix sloppy notations */
        if (gwy_stramong(buf->str, "°", "degree", "degrees", NULL))
            g_string_assign(buf, "deg");
        else if (gwy_stramong(buf->str, "micro m", NULL))
            g_string_assign(buf, "µm");
        else if (gwy_stramong(buf->str, "Å", "AA", "Ang", NULL))
            g_string_assign(buf, "Å");
        else if (gwy_stramong(buf->str, "a.u.", "a. u.", "counts", NULL))
            g_string_assign(buf, "");

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
        GwySimpleUnit u;
        u.power = 1;
        if ((p = strstr(buf->str + 1, "²"))) {
            u.power = 2;
            g_string_truncate(buf, p - buf->str);
        }
        else if ((p = strstr(buf->str + 1, "<sup>"))) {
            u.power = strtol(p + strlen("<sup>"), &e, 10);
            if (e == p + strlen("<sup>")
                || !g_str_has_prefix(e, "</sup>")) {
                gwy_unit_warning("Bad power %s", p);
                u.power = 1;
            }
            else if (!u.power || abs(u.power) > 12) {
                gwy_unit_warning("Bad power %d", u.power);
                u.power = 1;
            }
            g_string_truncate(buf, p - buf->str);
        }
        else if ((p = strchr(buf->str + 1, '^'))) {
            u.power = strtol(p + 1, &e, 10);
            if (e == p + 1 || *e) {
                gwy_unit_warning("Bad power %s", p);
                u.power = 1;
            }
            else if (!u.power || abs(u.power) > 12) {
                gwy_unit_warning("Bad power %d", u.power);
                u.power = 1;
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
                u.power = strtol(buf->str + i, NULL, 10);
                if (!u.power || abs(u.power) > 12) {
                    gwy_unit_warning("Bad power %d", u.power);
                    u.power = 1;
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
        else if (gwy_strequal(buf->str, "‰")) {
            pfpower -= 3;
            g_string_assign(buf, "");
        }

        /* elementary sanity */
        if (!g_utf8_validate(buf->str, -1, (const gchar**)&p)) {
            gwy_unit_warning("Unit string is not valid UTF-8");
            g_string_truncate(buf, p - buf->str);
        }
        if (!buf->len) {
            /* maybe it's just percentage.  cross fingers and proceed. */
            if (dividing)
                u.power = -u.power;
            power10 += u.power * pfpower;
        }
        else if (!g_ascii_isalpha(buf->str[0]) && (guchar)buf->str[0] < 128)
            gwy_unit_warning("Invalid base unit: %s", buf->str);
        else {
            /* append it */
            u.unit = g_quark_from_string(buf->str);
            if (dividing)
                u.power = -u.power;
            power10 += u.power * pfpower;
            g_array_append_val(unit->units, u);
        }

        /* TODO: scan known obscure units, implement traits */

        /* get to the next token, looking for division */
        while (g_ascii_isspace(*end))
            end++;
        if (*end == '/') {
            if (dividing) {
                gwy_unit_warning("Cannot group multiple divisions");
            }
            dividing = TRUE;
            end++;
            while (g_ascii_isspace(*end))
                end++;
        }
        string = end;
    }

    canonicalize(unit);
    g_string_free(buf, TRUE);
    g_free(utf8string);
    GWY_MAYBE_SET(ppower10, power10);

    return TRUE;
}

/**
 * gwy_unit_multiply:
 * @unit: A physical unit.
 *        It is set to the product of units @op1 and @op2.
 *        It may be one of @op1, @op2.
 * @op1: One multiplication operand.
 * @op2: Other multiplication operand.
 *
 * Multiplies two physical units.
 **/
void
gwy_unit_multiply(GwyUnit *unit,
                  const GwyUnit *op1,
                  const GwyUnit *op2)
{
    gwy_unit_power_multiply(unit, op1, 1, op2, 1);
}

/**
 * gwy_unit_divide:
 * @unit: A physical unit.
 *        It is set to the ratio of units @op1 and @op2.
 *        It may be one of @op1, @op2.
 * @op1: Numerator operand.
 * @op2: Denominator operand.
 *
 * Divides two physical units.
 **/
void
gwy_unit_divide(GwyUnit *unit,
                const GwyUnit *op1,
                const GwyUnit *op2)
{
    return gwy_unit_power_multiply(unit, op1, 1, op2, -1);
}

/**
 * gwy_unit_power:
 * @unit: A physical unit.
 *        It is set to unit @op raised to power @power.
 *        It may be @op itself.
 * @op: Base operand.
 * @power: Power to raise @op to.
 *
 * Computes the power of a physical unit.
 **/
void
gwy_unit_power(GwyUnit *unit,
               const GwyUnit *op,
               gint power)
{
    g_return_if_fail(GWY_IS_UNIT(unit));
    g_return_if_fail(GWY_IS_UNIT(op));

    power_impl(unit->priv, op->priv, power);
    g_signal_emit(unit, unit_signals[CHANGED], 0);
}

static void
power_impl(Unit *unit,
           const Unit *op,
           gint power)
{
    /* Perform power in-place */
    if (unit == op) {
        if (power) {
            for (guint j = 0; j < unit->units->len; j++) {
                GwySimpleUnit *u = &simple_unit_index(unit->units, j);
                u->power *= power;
            }
        }
        else
            g_array_set_size(unit->units, 0);
        return;
    }

    GArray *units = g_array_new(FALSE, FALSE, sizeof(GwySimpleUnit));

    if (power) {
        g_array_append_vals(units, op->units->data, op->units->len);
        for (guint j = 0; j < units->len; j++) {
            GwySimpleUnit *u = &simple_unit_index(units, j);
            u->power *= power;
        }
    }

    g_array_set_size(unit->units, 0);
    g_array_append_vals(unit->units, units->data, units->len);
    g_array_free(units, TRUE);
}

static void
multiply_impl(Unit *unit, Unit *op, gint power)
{
    for (guint i = 0; i < op->units->len; i++) {
        GwySimpleUnit *u2 = &simple_unit_index(op->units, i);

        guint j;
        for (j = 0; j < unit->units->len; j++) {
            GwySimpleUnit *u = &simple_unit_index(unit->units, j);
            if (u2->unit == u->unit) {
                u->power += power*u2->power;
                break;
            }
        }
        if (j == unit->units->len) {
            g_array_append_val(unit->units, *u2);
            GwySimpleUnit *u = &simple_unit_index(unit->units,
                                                  unit->units->len - 1);
            u->power *= power;
        }
    }
}

/**
 * gwy_unit_nth_root:
 * @unit: A physical unit.
 *        It is set to unit @op raised to power 1/@ipower or kept intact.
 *        It may be @op itself.
 * @op: Base operand.
 * @ipower: Root to take (a non-zero integer); 2 means a quadratic root, 3
 *          means cubic root, etc.
 *
 * Calulates the n-th root of a physical unit, if possible.
 *
 * This operation fails if the result would have fractional powers that
 * are not representable by #GwyUnit.  The function returns %FALSE in such case
 * and leaves @unit untouched.
 *
 * Returns: %TRUE if the root was successfully calculated, %FALSE on failure.
 **/
gboolean
gwy_unit_nth_root(GwyUnit *unit,
                  const GwyUnit *op,
                  gint ipower)
{
    g_return_val_if_fail(GWY_IS_UNIT(unit), FALSE);
    g_return_val_if_fail(GWY_IS_UNIT(op), FALSE);
    g_return_val_if_fail(ipower, FALSE);

    Unit *privop = op->priv;

    /* Check applicability */
    for (guint j = 0; j < privop->units->len; j++) {
        GwySimpleUnit *u = &simple_unit_index(privop->units, j);
        if (u->power % ipower != 0)
            return FALSE;
    }

    Unit *priv = unit->priv;
    GArray *units = g_array_new(FALSE, FALSE, sizeof(GwySimpleUnit));

    g_array_append_vals(units, privop->units->data, privop->units->len);
    for (guint j = 0; j < units->len; j++) {
        GwySimpleUnit *u = &simple_unit_index(units, j);
        u->power /= ipower;
    }

    g_array_set_size(priv->units, 0);
    g_array_append_vals(priv->units, units->data, units->len);
    g_array_free(units, TRUE);

    g_signal_emit(unit, unit_signals[CHANGED], 0);

    return TRUE;
}

/**
 * gwy_unit_power_multiply:
 * @unit: A physical unit.
 *        It is set to the product of units @op1 and @op2 raised to powers
 *        @power1 and @power2, respectively.
 *        It may be one of @op1, @op2.
 * @op1: First operand.
 * @power1: Power to raise @op1 to.
 * @op2: Second operand.
 * @power2: Power to raise @op2 to.
 *
 * Computes the product of two physical units raised to arbitrary powers.
 *
 * This is the most complex unit arithmetic function.
 **/
void
gwy_unit_power_multiply(GwyUnit *unit,
                        const GwyUnit *op1,
                        gint power1,
                        const GwyUnit *op2,
                        gint power2)
{
    g_return_if_fail(GWY_IS_UNIT(unit));
    g_return_if_fail(GWY_IS_UNIT(op1));
    g_return_if_fail(GWY_IS_UNIT(op2));

    /* Ensure @unit is different from at least one of @op1, @op2. */
    if (unit == op1 && unit == op2)
        return gwy_unit_power(unit, unit, power1 + power2);

    /* Try to avoid hard work by making @op2 the simplier argument, but
     * primarily make it different from @unit. */
    if (op2 == unit) {
        GWY_SWAP(const GwyUnit*, op1, op2);
        GWY_SWAP(gint, power1, power2);
    }
    else if (op1 != unit && (op1->priv->units->len < op2->priv->units->len
                             || (power2 && !power1))) {
        GWY_SWAP(const GwyUnit*, op1, op2);
        GWY_SWAP(gint, power1, power2);
    }
    g_assert(op2 != unit);

    power_impl(unit->priv, op1->priv, power1);
    if (power2)
        multiply_impl(unit->priv, op2->priv, power2);
    canonicalize(unit->priv);
    g_signal_emit(unit, unit_signals[CHANGED], 0);
}

static void
canonicalize(Unit *unit)
{
    guint i, j;

    /* consolidate multiple occurences of the same unit */
    i = 0;
    while (i < unit->units->len) {
        GwySimpleUnit *src = &simple_unit_index(unit->units, i);
        for (j = 0; j < i; j++) {
            GwySimpleUnit *dst = &simple_unit_index(unit->units, j);
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
        if (simple_unit_index(unit->units, i).power)
            i++;
        else
            g_array_remove_index(unit->units, i);
    }
}

/**
 * gwy_unit_format_for_power10:
 * @unit: A physical unit.
 * @style: Output format style.
 * @power10: Power of 10, in the same sense as gwy_unit_new_from_string()
 *           returns it.
 *
 * Finds format for representing a specific power-of-10 multiple of a physical
 * unit.
 *
 * The precision of the returned format is left at the default value (3).
 *
 * Returns: (transfer full):
 *          A newly created value format.
 **/
GwyValueFormat*
gwy_unit_format_for_power10(const GwyUnit *unit,
                            GwyValueFormatStyle style,
                            gint power10)
{
    g_return_val_if_fail(GWY_IS_UNIT(unit), NULL);

    gchar *glue, *units;
    format_unit(unit->priv, find_style_spec(style), power10,
                &glue, &units, FALSE);
    GwyValueFormat *format = g_object_new(GWY_TYPE_VALUE_FORMAT,
                                          "style", style,
                                          "base", gwy_powi(10.0, power10),
                                          "glue", glue,
                                          "units", units,
                                          NULL);
    g_free(glue);
    g_free(units);

    return format;
}

/**
 * gwy_unit_format_with_resolution:
 * @unit: A physical unit.
 * @style: Output format style.
 * @maximum: Maximum value to be represented.
 * @resolution: Smallest step (approximately) that should make a visible
 *              difference in the representation.
 *
 * Finds a format for representing a range of values with given resolution.
 *
 * Returns: (transfer full):
 *          A newly created value format.
 **/
GwyValueFormat*
gwy_unit_format_with_resolution(const GwyUnit *unit,
                                GwyValueFormatStyle style,
                                gdouble maximum,
                                gdouble resolution)
{
    g_return_val_if_fail(GWY_IS_UNIT(unit), NULL);

    maximum = fabs(maximum);
    resolution = fabs(resolution);
    guint precision = 2;
    gdouble base = 1.0;

    if (maximum)
        base = find_number_format(resolution, maximum, &precision);

    gchar *glue, *units;
    format_unit(unit->priv, find_style_spec(style), gwy_round(log10(base)),
                &glue, &units, FALSE);
    GwyValueFormat *format = g_object_new(GWY_TYPE_VALUE_FORMAT,
                                          "style", style,
                                          "base", base,
                                          "precision", precision,
                                          "glue", glue,
                                          "units", units,
                                          NULL);
    g_free(glue);
    g_free(units);

    return format;
}

/**
 * gwy_unit_format_with_digits:
 * @unit: A physical unit.
 * @style: Output format style.
 * @maximum: Maximum value to be represented.
 * @sdigits: Number of significant digits the value should have.
 *
 * Finds a format for representing a values with given number of significant
 * digits.
 *
 * Returns: (transfer full):
 *          A newly created value format.
 **/
GwyValueFormat*
gwy_unit_format_with_digits(const GwyUnit *unit,
                            GwyValueFormatStyle style,
                            gdouble maximum,
                            gint sdigits)
{
    g_return_val_if_fail(GWY_IS_UNIT(unit), NULL);

    maximum = fabs(maximum);
    guint precision = sdigits;
    gdouble base = 1.0;
    if (maximum)
        base = find_number_format(maximum/gwy_powi(10.0, sdigits), maximum,
                                  &precision);

    gchar *glue, *units;
    format_unit(unit->priv, find_style_spec(style), gwy_round(log10(base)),
                &glue, &units, FALSE);
    GwyValueFormat *format = g_object_new(GWY_TYPE_VALUE_FORMAT,
                                          "style", style,
                                          "base", base,
                                          "precision", precision,
                                          "glue", glue,
                                          "units", units,
                                          NULL);
    g_free(glue);
    g_free(units);

    return format;
}

/**
 * find_number_format:
 * @unit: The smallest possible step.
 * @maximum: The maximum possible value.
 * @precision: A location to store printf() precession, if not %NULL.
 *
 * Finds a human-friendly format for a range of numbers.
 *
 * Returns: The magnitude i.e., a power of 1000.
 **/
static gdouble
find_number_format(gdouble step,
                   gdouble maximum,
                   guint *precision)
{
    const gdouble eps = 1e-12;
    gdouble lm, lu, mag, q;

    g_return_val_if_fail(step >= 0.0, 0.0);
    g_return_val_if_fail(maximum >= 0.0, 0.0);

    if (G_UNLIKELY(step == 0.0 || maximum == 0.0)) {
        if (step > 0.0)
            maximum = step;
        else if (maximum > 0.0)
            step = maximum;
        else {
            if (precision)
                *precision = 1;
            return 1.0;
        }
    }

    lm = log10(maximum) + eps;
    lu = log10(step) - eps;
    mag = 3.0*floor(lm/3.0);
    q = 3.0*ceil(lu/3.0);
    if (q > mag)
        q = 3.0*ceil((lu - 1.0)/3.0);
    if (lu > -0.5 && lm < 3.1) {
        while (lu > mag+2)
            mag += 3.0;
    }
    else if (lm <= 0.5 && lm > -1.5) {
        mag = 0.0;
    }
    else {
        while (q > mag)
            mag += 3.0;
    }

    if (precision) {
        /* eps was good for mag, but here it gives us one digit too much */
        *precision = MAX(0, ceil(mag - lu - 2*eps));
        *precision = MIN(*precision, 16);
    }

    return gwy_powi(10.0, mag);
}

static void
append_power_plain(GString *str,
                   gint power)
{
    g_string_append_printf(str, "%d", power);
}

static void
append_power_unicode(GString *str,
                     gint power)
{
    gchar buf[32];

    g_snprintf(buf, sizeof(buf), "%d", power);
    for (guint i = 0; buf[i]; i++) {
        if (buf[i] == '0' || (buf[i] >= '4' && buf[i] <= '9'))
            g_string_append_unichar(str, 0x2070 + buf[i] - '0');
        else if (buf[i] == '1')
            g_string_append_len(str, "¹", sizeof("¹")-1);
        else if (buf[i] == '2')
            g_string_append_len(str, "²", sizeof("²")-1);
        else if (buf[i] == '3')
            g_string_append_len(str, "³", sizeof("³")-1);
        else if (buf[i] == '-')
            g_string_append_len(str, "⁻", sizeof("⁻")-1);
        else {
            g_warning("Weird digits in number %s\n", buf);
            g_string_append_c(str, buf[i]);
        }
    }
}

static void
format_unit(const Unit *unit,
            const GwyUnitStyleSpec *fs,
            gint power10,
            gchar **glue_retval,
            gchar **units_retval,
            gboolean retain_empty_strings)
{
    /* if there is a single unit with negative exponent, move it to the end
     * TODO: we may want more sophistication here */
    gint move_me_to_end = -1;
    if (unit->units->len > 1) {
        for (guint i = 0; i < unit->units->len; i++) {
            const GwySimpleUnit *u = &simple_unit_index(unit->units, i);
            if (u->power < 0) {
                if (move_me_to_end >= 0) {
                    move_me_to_end = -1;
                    break;
                }
                move_me_to_end = i;
            }
        }
    }

    /* find a victim to prepend a prefix to.  mwhahaha */
    gint prefix_bearer = -1;
    if (power10) {
        for (guint i = 0; i < unit->units->len; i++) {
            if (i == (guint)move_me_to_end)
                continue;
            const GwySimpleUnit *u = &simple_unit_index(unit->units, i);
            if (power10 % (3*abs(u->power)) == 0) {
                prefix_bearer = i;
                break;
            }
        }
    }
    if (power10 && prefix_bearer < 0 && move_me_to_end >= 0) {
        const GwySimpleUnit *u = &simple_unit_index(unit->units, move_me_to_end);
        if (power10 % (3*abs(u->power)) == 0)
            prefix_bearer = move_me_to_end;
    }

    /* check whether we are not out of prefix range */
    const gchar *prefix = "";
    if (prefix_bearer >= 0) {
        const GwySimpleUnit *u = &simple_unit_index(unit->units, prefix_bearer);
        prefix = get_prefix(power10/u->power);
        if (!prefix)
            prefix_bearer = -1;
    }

    GString *ustring = g_string_new(NULL);
    GString *gstring = g_string_new(NULL);

    /* if we were unable to place the prefix, we must add a power of 10 */
    if (power10 && prefix_bearer < 0) {
        g_string_append(gstring, fs->power10_times);
        switch (power10) {
            case -1:
            g_string_append(ustring, "0.1");
            break;

            case 1:
            g_string_append(ustring, "10");
            break;

            case 2:
            g_string_append(ustring, "100");
            break;

            default:
            if (fs->power10_open)
                g_string_append(ustring, fs->power10_open);
            fs->append_power(ustring, power10);
            if (fs->power_close)
                g_string_append(ustring, fs->power_close);
            break;
        }
        if (fs->number_unit_glue && unit->units->len)
            g_string_append(ustring, fs->number_unit_glue);
    }
    else {
        if (unit->units->len)
            g_string_append(gstring, fs->number_unit_glue);
    }

    /* append units */
    for (guint i = 0; i < unit->units->len; i++) {
        if (i == (guint)move_me_to_end)
            continue;
        if (i > 1 || (i && move_me_to_end)) {
            g_string_append(ustring, fs->unit_times);
        }
        const GwySimpleUnit *u = &simple_unit_index(unit->units, i);
        if (i == (guint)prefix_bearer)
            g_string_append(ustring, prefix);
        g_string_append(ustring, g_quark_to_string(u->unit));
        if (u->power != 1) {
            if (fs->power_open)
                g_string_append(ustring, fs->power_open);
            fs->append_power(ustring, u->power);
            if (fs->power_close)
                g_string_append(ustring, fs->power_close);
        }
    }
    if (move_me_to_end >= 0) {
        g_string_append(ustring, fs->unit_division);
        const GwySimpleUnit *u = &simple_unit_index(unit->units, move_me_to_end);
        if (move_me_to_end == prefix_bearer)
            g_string_append(ustring, prefix);
        g_string_append(ustring, g_quark_to_string(u->unit));
        if (u->power != -1) {
            if (fs->power_open)
                g_string_append(ustring, fs->power_open);
            fs->append_power(ustring, -u->power);
            if (fs->power_close)
                g_string_append(ustring, fs->power_close);
        }
    }

    if (glue_retval) {
        if (retain_empty_strings || gstring->len)
            *glue_retval = g_string_free(gstring, FALSE);
        else {
            *glue_retval = NULL;
            g_string_free(gstring, TRUE);
        }
    }
    else
        g_string_free(gstring, TRUE);

    if (units_retval) {
        if (retain_empty_strings || ustring->len)
            *units_retval = g_string_free(ustring, FALSE);
        else {
            *units_retval = NULL;
            g_string_free(ustring, TRUE);
        }
    }
    else
        g_string_free(ustring, TRUE);
}

static const gchar*
get_prefix(gint power)
{
    for (guint i = 0; i < G_N_ELEMENTS(SI_prefixes); i++) {
        if (SI_prefixes[i].power10 == power)
            return SI_prefixes[i].prefix;
    }
    return NULL;
}


/**
 * SECTION: unit
 * @title: GwyUnit
 * @short_description: Physical unit representation
 * @see_also: #GwyValueFormat
 *
 * #GwyUnit represents physical units.  Units can be compared and also divided,
 * multiplied, etc. to give new units.
 *
 * It provides also methods for formatting of physical quantities.  There are
 * several methods available for constructing a value format with given
 * resolution -- gwy_unit_format_with_resolution(), or number of
 * significant digits -- gwy_unit_format_with_digits().  These methods
 * either create or update a #GwyValueFormat object that can be subsequently
 * used for the formatting.
 *
 * <refsect2 id='GwyUnit-ownership'>
 * <title>Units and Ownership</title>
 * <para>#GwyUnit is an object.  Beside the type of units it represents, it has
 * an identity: it is possible to connect to its signals or attach data to the
 * specific instance with g_object_set_data().</para>
 * <para>On the other hand, one often thinks about it and attempts to use it as
 * a mere value.  Hence it is almost never a good idea to make one unit object
 * owned by several other objects (the motivation for that could be keeping
 * their units synchronised).  In addition, this relation would be lost in the
 * serialised representation.  To prevent multiple ownership, objects that
 * typically own #GwyUnits, such as #GwyField or #GwyLine, do not provide
 * methods to set the units objects.  Instead of</para>
 * |[
 * gwy_field_set_unit_z(field, unit);
 * ]|
 * <para>you can modify the unit object with assignment:</para>
 * |[
 * gwy_unit_assign(gwy_field_get_unit_z(field), unit);
 * ]|
 * <para>or other operations, e.g. if you have a string, using:</para>
 * |[
 * gwy_unit_set_from_string(gwy_field_get_unit_z(field), "nm", NULL);
 * ]|
 * </refsect2>
 **/

/**
 * GwyUnit:
 *
 * Object representing physical units.
 *
 * The #GwyUnit struct contains private data only and should be accessed
 * using the functions below.
 **/

/**
 * GwyUnitClass:
 *
 * Class of physical units objects.
 **/

/**
 * gwy_unit_duplicate:
 * @unit: A physical unit.
 *
 * Duplicates a physical unit.
 *
 * This is a convenience wrapper of gwy_serializable_duplicate().
 **/

/**
 * gwy_unit_assign:
 * @dest: Destination physical unit.
 * @src: Source physical unit.
 *
 * Copies the value of a physical unit.
 *
 * This is a convenience wrapper of gwy_serializable_assign().
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
