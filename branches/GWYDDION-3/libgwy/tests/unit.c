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

#include "testlibgwy.h"
#include <stdlib.h>

/***************************************************************************
 *
 * Units
 *
 ***************************************************************************/

typedef struct {
    const gchar *string;
    const gchar *reference;
    gint power10;
} UnitTestCase;

void
unit_randomize(GwyUnit *unit, GRand *rng)
{
    static const gchar const *some_units[] = {
        "m", "s", "A", "nN", "mV", "deg", "kg", "Hz"
    };

    gwy_unit_clear(unit);
    GwyUnit *op = gwy_unit_new();
    while (g_rand_int(rng) % 4) {
        guint i = g_rand_int_range(rng, 0, G_N_ELEMENTS(some_units));
        gwy_unit_set_from_string(op, some_units[i], NULL);
        if (g_rand_boolean(rng))
            gwy_unit_multiply(unit, unit, op);
        else
            gwy_unit_divide(unit, unit, op);
    }
    g_object_unref(op);
}

static void
unit_check_cases(const UnitTestCase *cases, guint n)
{
    for (guint i = 0; i < n; i++) {
        gint power10;
        GwyUnit *unit = gwy_unit_new_from_string(cases[i].string, &power10);
        g_assert(GWY_IS_UNIT(unit));
        g_assert_cmpint(power10, ==, cases[i].power10);
        GwyUnit *ref = gwy_unit_new_from_string(cases[i].reference, NULL);
        g_assert(gwy_unit_equal(unit, ref));
        g_object_unref(ref);
        g_object_unref(unit);
    }
}

void
test_unit_parse_power10(void)
{
    UnitTestCase cases[] = {
        { "",                NULL, 0,   },
        { "1",               NULL, 0,   },
        { "10",              NULL, 1,   },
        { "0.01",            NULL, -2,  },
        { "%",               NULL, -2,  },
        { "10%",             NULL, -1,  },
        { "‰",               NULL, -3,  },
        { "10‰",             NULL, -2,  },
        { "10^4",            NULL, 4,   },
        { "10^-6",           NULL, -6,  },
        { "10<sup>3</sup>",  NULL, 3,   },
        { "10<sup>-3</sup>", NULL, -3,  },
        { "10⁵",             NULL, 5,   },
        { "10⁻¹²",           NULL, -12, },
        { "×10^2",           NULL, 2,   },
        { "×10^-2",          NULL, -2,  },
        { "*10^2",           NULL, 2,   },
        { "*10^-2",          NULL, -2,  },
        { "×10^11",          NULL, 11,  },
        { "×10^-11",         NULL, -11, },
        { "*10^12",          NULL, 12,  },
        { "*10^-12",         NULL, -12, },
    };
    unit_check_cases(cases, G_N_ELEMENTS(cases));
}

void
test_unit_parse_meter(void)
{
    UnitTestCase cases[] = {
        { "m",   "m", 0,   },
        { "km",  "m", 3,   },
        { "mm",  "m", -3,  },
        { "cm",  "m", -2,  },
        { "dm",  "m", -1,  },
        { "Å",   "m", -10, },
        { "Å",   "m", -10, },
        { "nm",  "m", -9,  },
    };
    unit_check_cases(cases, G_N_ELEMENTS(cases));
}

void
test_unit_parse_micro(void)
{
    UnitTestCase cases[] = {
        { "µs",    "s", -6, },
        { "μs",    "s", -6, },
        { "us",    "s", -6, },
        { "~s",    "s", -6, },
        { "\265s", "s", -6, },
    };
    unit_check_cases(cases, G_N_ELEMENTS(cases));
}

void
test_unit_parse_power(void)
{
    UnitTestCase cases[] = {
        { "mA¹",             "A",    -3,  },
        { "mA<sup>1</sup>",  "A",    -3,  },
        { "mA1",             "A^1",  -3,  },
        { "mA²",             "A^2",  -6,  },
        { "mA<sup>2</sup>",  "A^2",  -6,  },
        { "mA2",             "A^2",  -6,  },
        { "mA\262",          "A^2",  -6,  },
        { "mA³",             "A^3",  -9,  },
        { "mA3",             "A^3",  -9,  },
        { "mA<sup>3</sup>",  "A^3",  -9,  },
        { "mA⁴",             "A^4",  -12, },
        { "mA<sup>4</sup>",  "A^4",  -12, },
        { "mA⁻¹",            "A^-1", 3,   },
        { "mA<sup>-1</sup>", "A^-1", 3,   },
        { "mA-1",            "A^-1", 3,   },
        { "mA⁻²",            "A^-2", 6,   },
        { "mA<sup>-2</sup>", "A^-2", 6,   },
        { "mA-2",            "A^-2", 6,   },
        { "mA⁻³",            "A^-3", 9,   },
        { "mA<sup>-3</sup>", "A^-3", 9,   },
        { "mA-3",            "A^-3", 9,   },
        { "mA⁻⁴",            "A^-4", 12,  },
        { "mA<sup>-4</sup>", "A^-4", 12,  },
    };
    unit_check_cases(cases, G_N_ELEMENTS(cases));
}

void
test_unit_parse_combine(void)
{
    UnitTestCase cases[] = {
        { "um s^-1",            "mm/ps",              -6, },
        { "mm/ps",              "μs<sup>-1</sup> cm", 9,  },
        { "μs<sup>-1</sup> cm", "um s^-1",            4,  },
        { "m^3 cm^-2/km",       NULL,                 1,  },
    };
    unit_check_cases(cases, G_N_ELEMENTS(cases));
}

void
test_wunitord_names(void)
{
    gint pw1, pw2, pw3, pw4;
    GwyUnit *u1 = gwy_unit_new_from_string("m", &pw1);
    GwyUnit *u2 = gwy_unit_new_from_string("Metre", &pw2);
    GwyUnit *u3 = gwy_unit_new_from_string("METER", &pw3);
    GwyUnit *u4 = gwy_unit_new_from_string("metres", &pw4);

    g_assert(gwy_unit_equal(u2, u1));
    g_assert(gwy_unit_equal(u3, u1));
    g_assert(gwy_unit_equal(u4, u1));
    g_assert_cmpint(pw1, ==, 0);
    g_assert_cmpint(pw2, ==, 0);
    g_assert_cmpint(pw3, ==, 0);
    g_assert_cmpint(pw4, ==, 0);
    g_object_unref(u4);
    g_object_unref(u3);
    g_object_unref(u2);
    g_object_unref(u1);

    gint pw5;
    GwyUnit *u5 = gwy_unit_new_from_string("furlong", &pw5);
    g_assert_cmpint(pw5, ==, 0);
    gchar *s = gwy_unit_to_string(u5, GWY_VALUE_FORMAT_PLAIN);
    g_assert_cmpstr(s, ==, "furlong");
    g_free(s);
    g_object_unref(u5);
}

void
test_unit_changed(void)
{
    GwyUnit *unit = gwy_unit_new();
    guint counter = 0;
    g_signal_connect_swapped(unit, "changed",
                             G_CALLBACK(record_signal), &counter);

    gwy_unit_set_from_string(unit, "", NULL);
    g_assert_cmpuint(counter, ==, 0);

    gwy_unit_set_from_string(unit, "m", NULL);
    g_assert_cmpuint(counter, ==, 1);

    gwy_unit_set_from_string(unit, "km", NULL);
    g_assert_cmpuint(counter, ==, 1);

    gwy_unit_set_from_string(unit, "m²/m", NULL);
    g_assert_cmpuint(counter, ==, 1);

    gwy_unit_set_from_string(unit, "s", NULL);
    g_assert_cmpuint(counter, ==, 2);

    gwy_unit_set_from_string(unit, NULL, NULL);
    g_assert_cmpuint(counter, ==, 3);

    gwy_unit_set_from_string(unit, NULL, NULL);
    g_assert_cmpuint(counter, ==, 3);

    gwy_unit_set_from_string(unit, "", NULL);
    g_assert_cmpuint(counter, ==, 3);

    gwy_unit_set_from_string(unit, "100", NULL);
    g_assert_cmpuint(counter, ==, 3);

    g_object_unref(unit);
}

void
test_unit_arithmetic_simple(void)
{
    GwyUnit *u1, *u2, *u3, *u4, *u5, *u6, *u7, *u8, *u9, *u0;

    u1 = gwy_unit_new_from_string("kg m s^-2", NULL);
    u2 = gwy_unit_new_from_string("s/kg", NULL);
    u3 = gwy_unit_new_from_string("m/s", NULL);

    u4 = gwy_unit_new();
    gwy_unit_multiply(u4, u1, u2);
    g_assert(gwy_unit_equal(u3, u4));

    u5 = gwy_unit_new();
    gwy_unit_power(u5, u1, -1);
    u6 = gwy_unit_new();
    gwy_unit_power_multiply(u6, u5, 2, u2, -2);
    u7 = gwy_unit_new();
    gwy_unit_power(u7, u3, -2);
    g_assert(gwy_unit_equal(u6, u7));

    u8 = gwy_unit_new();
    gwy_unit_nth_root(u8, u6, 2);
    gwy_unit_power(u8, u8, -1);
    g_assert(gwy_unit_equal(u8, u3));

    gwy_unit_divide(u8, u8, u3);
    u0 = gwy_unit_new();
    g_assert(gwy_unit_equal(u8, u0));

    u9 = gwy_unit_new();
    gwy_unit_power(u9, u3, 4);
    gwy_unit_power_multiply(u9, u9, 1, u1, -3);
    gwy_unit_power_multiply(u9, u2, 3, u9, -1);
    gwy_unit_multiply(u9, u9, u3);
    g_assert(gwy_unit_equal(u9, u0));

    g_object_unref(u1);
    g_object_unref(u2);
    g_object_unref(u3);
    g_object_unref(u4);
    g_object_unref(u5);
    g_object_unref(u6);
    g_object_unref(u7);
    g_object_unref(u8);
    g_object_unref(u9);
    g_object_unref(u0);
}

void
test_unit_arithmetic_null(void)
{
    GwyUnit *u1, *u2, *u3, *u4, *u5, *u6, *u7, *u8;

    u1 = gwy_unit_new_from_string("m", NULL);
    u2 = gwy_unit_new_from_string("m^2", NULL);

    u3 = gwy_unit_new();
    g_assert(gwy_unit_is_empty(u3));
    g_assert(gwy_unit_equal(NULL, NULL));
    g_assert(gwy_unit_equal(u3, NULL));
    g_assert(gwy_unit_equal(NULL, u3));

    u4 = gwy_unit_new_from_string("m", NULL);
    gwy_unit_power(u4, NULL, 3);
    g_assert(gwy_unit_is_empty(u4));

    u5 = gwy_unit_new();
    gwy_unit_power_multiply(u5, NULL, -1, u1, 2);
    g_assert(!gwy_unit_is_empty(u5));
    g_assert(gwy_unit_equal(u5, u2));

    u6 = gwy_unit_new();
    gwy_unit_power_multiply(u6, u1, 2, NULL, -1);
    g_assert(!gwy_unit_is_empty(u6));
    g_assert(gwy_unit_equal(u6, u2));

    u7 = gwy_unit_new_from_string("m", NULL);
    g_assert(gwy_unit_nth_root(u7, NULL, 2));
    g_assert(gwy_unit_is_empty(u7));

    u8 = gwy_unit_new_from_string("m/s", NULL);
    gwy_unit_clear(u8);
    g_assert(gwy_unit_is_empty(u8));

    gwy_unit_clear(NULL);

    g_object_unref(u1);
    g_object_unref(u2);
    g_object_unref(u3);
    g_object_unref(u4);
    g_object_unref(u5);
    g_object_unref(u6);
    g_object_unref(u7);
    g_object_unref(u8);
}

void
test_unit_arithmetic_nth_root(void)
{
    GwyUnit *u1, *u2, *u3, *u4, *u5, *u6;

    u1 = gwy_unit_new_from_string("m^6/s^3", NULL);
    u2 = gwy_unit_new();
    g_assert(gwy_unit_nth_root(u2, u1, 3));
    u3 = gwy_unit_new_from_string("m^2/s", NULL);
    g_assert(gwy_unit_equal(u2, u3));

    g_assert(gwy_unit_nth_root(u2, u1, -3));
    u4 = gwy_unit_new_from_string("s/m^2", NULL);
    g_assert(gwy_unit_equal(u2, u4));

    u5 = gwy_unit_duplicate(u2);
    g_assert(!gwy_unit_nth_root(u2, u1, 2));
    g_assert(gwy_unit_equal(u2, u5));

    g_assert(gwy_unit_nth_root(u2, u1, 1));
    g_assert(gwy_unit_equal(u2, u5));

    g_assert(gwy_unit_nth_root(u2, u1, -1));
    gwy_unit_power(u5, u1, -1);
    g_assert(gwy_unit_equal(u2, u5));

    u6 = gwy_unit_new();
    g_assert(gwy_unit_nth_root(u2, u6, 1));
    g_assert(gwy_unit_equal(u2, u6));
    g_assert(gwy_unit_nth_root(u2, u6, -2));
    g_assert(gwy_unit_equal(u2, u6));
    g_assert(gwy_unit_nth_root(u2, u6, 17));
    g_assert(gwy_unit_equal(u2, u6));
    g_assert(gwy_unit_nth_root(u2, NULL, -5));
    g_assert(gwy_unit_equal(u2, u6));

    g_object_unref(u1);
    g_object_unref(u2);
    g_object_unref(u3);
    g_object_unref(u4);
    g_object_unref(u5);
    g_object_unref(u6);
}

void
test_unit_changed_arithmetic(void)
{
    GwyUnit *u1 = gwy_unit_new_from_string("m", NULL);
    GwyUnit *u2 = gwy_unit_new_from_string("m²", NULL);
    GwyUnit *u3 = gwy_unit_new_from_string("m/s", NULL);
    GwyUnit *u4 = gwy_unit_new_from_string("s", NULL);
    GwyUnit *u5 = gwy_unit_new_from_string("m", NULL);
    GwyUnit *u6 = gwy_unit_new_from_string("m/s²", NULL);
    GwyUnit *u7 = gwy_unit_new();

    guint counter = 0;
    g_signal_connect_swapped(u1, "changed",
                             G_CALLBACK(record_signal), &counter);

    gwy_unit_assign(u1, u5);
    g_assert_cmpuint(counter, ==, 0);

    gwy_unit_multiply(u1, u3, u4);
    g_assert_cmpuint(counter, ==, 0);

    gwy_unit_divide(u1, u2, u5);
    g_assert_cmpuint(counter, ==, 0);

    gwy_unit_power(u1, u5, 1);
    g_assert_cmpuint(counter, ==, 0);

    gwy_unit_nth_root(u1, u2, 2);
    g_assert_cmpuint(counter, ==, 0);

    gwy_unit_power_multiply(u1, u5, 1, u7, 0);
    g_assert_cmpuint(counter, ==, 0);

    gwy_unit_power_multiply(u1, u5, 1, u7, -3);
    g_assert_cmpuint(counter, ==, 0);

    gwy_unit_power_multiply(u1, u7, 0, u5, 1);
    g_assert_cmpuint(counter, ==, 0);

    gwy_unit_power_multiply(u1, u7, 2, u5, 1);
    g_assert_cmpuint(counter, ==, 0);

    gwy_unit_power_multiply(u1, u5, 2, u5, -1);
    g_assert_cmpuint(counter, ==, 0);

    gwy_unit_power_multiply(u1, u1, 2, u1, -1);
    g_assert_cmpuint(counter, ==, 0);

    gwy_unit_power_multiply(u1, u3, 2, u6, -1);
    g_assert_cmpuint(counter, ==, 0);

    g_object_unref(u7);
    g_object_unref(u6);
    g_object_unref(u5);
    g_object_unref(u4);
    g_object_unref(u3);
    g_object_unref(u2);
    g_object_unref(u1);
}

void
test_unit_arithmetic_commutativity(void)
{
    static const gchar *bases[] = { "1/m", "", "m", "m^2" };
    enum { nbases = G_N_ELEMENTS(bases) };

    GwyUnit *iref = gwy_unit_new(), *jref = gwy_unit_new(),
            *iunit = gwy_unit_new(), *junit = gwy_unit_new(),
            *result1 = gwy_unit_new(), *result2 = gwy_unit_new();

    for (unsigned int i = 0; i < nbases; i++) {
        gwy_unit_set_from_string(iref, bases[i], NULL);
        for (unsigned int j = 0; j < nbases; j++) {
            gwy_unit_set_from_string(jref, bases[j], NULL);
            for (int ipower = -2; ipower <= 2; ipower++) {
                for (int jpower = -2; jpower <= 2; jpower++) {
                    gwy_unit_assign(iunit, iref);
                    gwy_unit_assign(junit, jref);
                    gwy_unit_power_multiply(result1,
                                            iunit, ipower, junit, jpower);
                    gwy_unit_power_multiply(result2,
                                            junit, jpower, iunit, ipower);
                    g_assert(gwy_unit_equal(result1, result2));
                    g_assert(gwy_unit_equal(iunit, iref));
                    g_assert(gwy_unit_equal(junit, jref));

                    gwy_unit_assign(iunit, iref);
                    gwy_unit_assign(junit, jref);
                    gwy_unit_power_multiply(iunit,
                                            iunit, ipower, junit, jpower);
                    g_assert(gwy_unit_equal(iunit, result1));
                    g_assert(gwy_unit_equal(junit, jref));

                    gwy_unit_assign(iunit, iref);
                    gwy_unit_assign(junit, jref);
                    gwy_unit_power_multiply(iunit,
                                            junit, jpower, iunit, ipower);
                    g_assert(gwy_unit_equal(iunit, result2));
                    g_assert(gwy_unit_equal(junit, jref));
                }
            }
        }
    }

    g_object_unref(result2);
    g_object_unref(result1);
    g_object_unref(junit);
    g_object_unref(iunit);
    g_object_unref(jref);
    g_object_unref(iref);
}

static void
unit_assert_equal(const GwyUnit *unit, const GwyUnit *otherunit)
{
    g_assert(gwy_unit_equal(unit, otherunit));
}

void
test_unit_swap(void)
{
    GwyUnit *u1 = gwy_unit_new_from_string("µm", NULL);
    GwyUnit *u2 = gwy_unit_new_from_string("A/s", NULL);
    GwyUnit *ref1 = gwy_unit_duplicate(u1);
    GwyUnit *ref2 = gwy_unit_duplicate(u2);
    guint changed1 = 0, changed2 = 0, changedref1 = 0, changedref2 = 0;

    g_signal_connect_swapped(u1, "changed",
                             G_CALLBACK(record_signal), &changed1);
    g_signal_connect_swapped(u2, "changed",
                             G_CALLBACK(record_signal), &changed2);
    g_signal_connect_swapped(ref1, "changed",
                             G_CALLBACK(record_signal), &changedref1);
    g_signal_connect_swapped(ref2, "changed",
                             G_CALLBACK(record_signal), &changedref2);
    g_signal_connect(u1, "changed", G_CALLBACK(unit_assert_equal), ref2);
    g_signal_connect(u2, "changed", G_CALLBACK(unit_assert_equal), ref1);

    gwy_unit_swap(u1, u2);
    g_assert_cmpuint(changed1, ==, 1);
    g_assert_cmpuint(changed2, ==, 1);

    gwy_unit_swap(u1, ref2);
    g_assert_cmpuint(changed1, ==, 1);
    g_assert_cmpuint(changedref2, ==, 0);

    gwy_unit_swap(u2, ref1);
    g_assert_cmpuint(changed2, ==, 1);
    g_assert_cmpuint(changedref1, ==, 0);

    g_object_unref(ref2);
    g_object_unref(ref1);
    g_object_unref(u2);
    g_object_unref(u1);
}

static void
unit_serialize_one(GwyUnit *unit1)
{
    GwyUnit *unit2;
    GOutputStream *stream;
    GMemoryOutputStream *memstream;
    GError *error = NULL;
    GwyErrorList *error_list = NULL;
    gsize len;
    gboolean ok;

    stream = g_memory_output_stream_new(malloc(100), 100, NULL, &free);
    memstream = G_MEMORY_OUTPUT_STREAM(stream);
    ok = gwy_serialize_gio(GWY_SERIALIZABLE(unit1), stream, &error);
    g_assert(ok);
    g_assert_no_error(error);
    len = g_memory_output_stream_get_data_size(memstream);
    unit2 = (GwyUnit*)(gwy_deserialize_memory
                       (g_memory_output_stream_get_data(memstream),
                        g_memory_output_stream_get_data_size(memstream),
                        &len, &error_list));
    g_assert_cmpuint(len, ==, g_memory_output_stream_get_data_size(memstream));
    g_assert_cmpuint(g_slist_length(error_list), ==, 0);
    g_assert(GWY_IS_UNIT(unit2));
    g_assert(gwy_unit_equal(unit2, unit1));

    g_object_unref(stream);
    g_object_unref(unit2);
}

void
test_unit_serialize(void)
{
    GwyUnit *unit;

    /* Trivial unit */
    unit = gwy_unit_new();
    unit_serialize_one(unit);
    g_object_unref(unit);

    /* Less trivial unit */
    unit = gwy_unit_new_from_string("kg m s^-2/nA", NULL);
    unit_serialize_one(unit);
    g_object_unref(unit);
}

void
test_unit_assign(void)
{
    GwyUnit *unit1 = gwy_unit_new_from_string("kg µm ms^-2/nA", NULL);
    GwyUnit *unit2 = gwy_unit_new();
    GwyUnit *unit3 = gwy_unit_new();

    g_assert(!gwy_unit_equal(unit1, unit3));
    gwy_unit_assign(unit3, unit1);
    g_assert(gwy_unit_equal(unit1, unit3));
    gwy_unit_assign(unit2, unit1);
    g_assert(gwy_unit_equal(unit2, unit3));

    g_object_unref(unit1);
    g_object_unref(unit2);
    g_object_unref(unit3);
}

void
test_unit_garbage(void)
{
    enum { niter = 10000 };
    static const gchar *tokens[] = {
        "^", "+", "-", "/", "*", "<sup>", "</sup>", "10",
    };
    static const gchar characters[] = G_CSET_a_2_z G_CSET_A_2_Z G_CSET_DIGITS;
    GwyUnit *unit = gwy_unit_new();

    GString *garbage = g_string_new(NULL);
    GRand *rng = g_rand_new_with_seed(42);

    /* No checks.  The goal is not to crash... */
    for (gsize i = 0; i < niter; i++) {
        gsize ntoks = g_rand_int_range(rng, 0, 5) + g_rand_int_range(rng, 0, 7);

        g_string_truncate(garbage, 0);
        for (gsize j = 0; j < ntoks; j++) {
            gsize what = g_rand_int_range(rng, 0, G_N_ELEMENTS(tokens) + 10);

            if (g_rand_int_range(rng, 0, 3))
                g_string_append_c(garbage, ' ');

            if (what == G_N_ELEMENTS(tokens))
                g_string_append_c(garbage, g_rand_int_range(rng, 1, 0x100));
            else if (what < G_N_ELEMENTS(tokens))
                g_string_append(garbage, tokens[what]);
            else {
                what = g_rand_int_range(rng, 0, sizeof(characters));
                g_string_append_printf(garbage, "%c", characters[what]);
            }
        }
        gwy_unit_set_from_string(unit, garbage->str, NULL);
    }

    g_string_free(garbage, TRUE);
    g_rand_free(rng);
    g_object_unref(unit);
}

void
test_unit_serialize_complex(void)
{
    enum { niter = 1000 };
    GRand *rng = g_rand_new_with_seed(42);

    for (guint i = 0; i < niter; i++) {
        GwyUnit *unit = gwy_unit_new();
        unit_randomize(unit, rng);
        unit_serialize_one(unit);
        g_object_unref(unit);
    }

    g_rand_free(rng);
}

void
test_unit_format_power10(void)
{
    GwyUnit *unit = gwy_unit_new();
    GwyValueFormat *format;
    gdouble base;

    format = gwy_unit_format_for_power10(unit, GWY_VALUE_FORMAT_PLAIN, 0);
    g_object_get(format, "base", &base, NULL);
    g_assert_cmpfloat(base, ==, 1.0);
    g_assert(!gwy_value_format_get_units(format));
    g_object_unref(format);

    format = gwy_unit_format_for_power10(unit, GWY_VALUE_FORMAT_PLAIN, 3);
    g_object_get(format, "base", &base, NULL);
    g_assert_cmpfloat(fabs(base - 1000.0), <, 1e-13);
    g_assert_cmpstr(gwy_value_format_get_units(format), ==, "10^3");
    g_object_unref(format);

    format = gwy_unit_format_for_power10(unit, GWY_VALUE_FORMAT_UNICODE, 6);
    g_object_get(format, "base", &base, NULL);
    g_assert_cmpfloat(fabs(base - 1000000.0), <, 1e-10);
    g_assert_cmpstr(gwy_value_format_get_units(format), ==, "10⁶");
    g_object_unref(format);

    format = gwy_unit_format_for_power10(unit, GWY_VALUE_FORMAT_UNICODE, -6);
    g_object_get(format, "base", &base, NULL);
    g_assert_cmpfloat(fabs(base - 0.000001), <, 1e-22);
    g_assert_cmpstr(gwy_value_format_get_units(format), ==, "10⁻⁶");
    g_object_unref(format);

    gwy_unit_set_from_string(unit, "m", NULL);

    format = gwy_unit_format_for_power10(unit, GWY_VALUE_FORMAT_PLAIN, 0);
    g_object_get(format, "base", &base, NULL);
    g_assert_cmpfloat(base, ==, 1.0);
    g_assert_cmpstr(gwy_value_format_get_units(format), ==, "m");
    g_object_unref(format);

    format = gwy_unit_format_for_power10(unit, GWY_VALUE_FORMAT_PLAIN, 3);
    g_object_get(format, "base", &base, NULL);
    g_assert_cmpfloat(fabs(base - 1000.0), <, 1e-13);
    g_assert_cmpstr(gwy_value_format_get_units(format), ==, "km");
    g_object_unref(format);

    format = gwy_unit_format_for_power10(unit, GWY_VALUE_FORMAT_PLAIN, -3);
    g_object_get(format, "base", &base, NULL);
    g_assert_cmpfloat(fabs(base - 0.001), <, 1e-19);
    g_assert_cmpstr(gwy_value_format_get_units(format), ==, "mm");
    g_object_unref(format);

    gwy_unit_set_from_string(unit, "m s^-1", NULL);

    format = gwy_unit_format_for_power10(unit, GWY_VALUE_FORMAT_PLAIN, 0);
    g_object_get(format, "base", &base, NULL);
    g_assert_cmpfloat(base, ==, 1.0);
    g_assert_cmpstr(gwy_value_format_get_units(format), ==, "m/s");
    g_object_unref(format);

    format = gwy_unit_format_for_power10(unit, GWY_VALUE_FORMAT_PLAIN, 3);
    g_object_get(format, "base", &base, NULL);
    g_assert_cmpfloat(fabs(base - 1000.0), <, 1e-13);
    g_assert_cmpstr(gwy_value_format_get_units(format), ==, "km/s");
    g_object_unref(format);

    format = gwy_unit_format_for_power10(unit, GWY_VALUE_FORMAT_PLAIN, -3);
    g_object_get(format, "base", &base, NULL);
    g_assert_cmpfloat(fabs(base - 0.001), <, 1e-19);
    g_assert_cmpstr(gwy_value_format_get_units(format), ==, "mm/s");
    g_object_unref(format);

    g_object_unref(unit);
}

void
test_unit_format_digits(void)
{
    GwyUnit *unit = gwy_unit_new();
    GwyValueFormat *format;

    format = gwy_unit_format_with_digits(unit, GWY_VALUE_FORMAT_PLAIN, 0.11,
                                         2);
    g_assert_cmpfloat(gwy_value_format_get_base(format), ==, 1.0);
    g_assert_cmpuint(gwy_value_format_get_precision(format), ==, 2);
    g_assert(!gwy_value_format_get_units(format));
    g_object_unref(format);

    format = gwy_unit_format_with_digits(unit, GWY_VALUE_FORMAT_PLAIN, 10.1,
                                         2);
    g_assert_cmpfloat(gwy_value_format_get_base(format), ==, 1.0);
    g_assert_cmpuint(gwy_value_format_get_precision(format), ==, 0);
    g_assert(!gwy_value_format_get_units(format));
    g_object_unref(format);

    // XXX: This is an exception.  The extra digit is probably less annoying
    // than a power of 10 appended.
    format = gwy_unit_format_with_digits(unit, GWY_VALUE_FORMAT_PLAIN, 100.1,
                                         2);
    g_assert_cmpfloat(gwy_value_format_get_base(format), ==, 1.0);
    g_assert_cmpuint(gwy_value_format_get_precision(format), ==, 0);
    g_assert(!gwy_value_format_get_units(format));
    g_object_unref(format);

    format = gwy_unit_format_with_digits(unit, GWY_VALUE_FORMAT_PLAIN, 1000.1,
                                         2);
    g_assert_cmpfloat(gwy_value_format_get_base(format), ==, 1000.0);
    g_assert_cmpuint(gwy_value_format_get_precision(format), ==, 1);
    g_assert_cmpstr(gwy_value_format_get_units(format), ==, "10^3");
    g_object_unref(format);

    format = gwy_unit_format_with_digits(unit, GWY_VALUE_FORMAT_PLAIN, 10000.1,
                                         2);
    g_assert_cmpfloat(gwy_value_format_get_base(format), ==, 1000.0);
    g_assert_cmpuint(gwy_value_format_get_precision(format), ==, 0);
    g_assert_cmpstr(gwy_value_format_get_units(format), ==, "10^3");
    g_object_unref(format);

    format = gwy_unit_format_with_digits(unit, GWY_VALUE_FORMAT_PLAIN, 0.11,
                                         3);
    g_assert_cmpfloat(gwy_value_format_get_base(format), ==, 1.0);
    g_assert_cmpuint(gwy_value_format_get_precision(format), ==, 3);
    g_assert(!gwy_value_format_get_units(format));
    g_object_unref(format);

    format = gwy_unit_format_with_digits(unit, GWY_VALUE_FORMAT_PLAIN, 1.1,
                                         3);
    g_assert_cmpfloat(gwy_value_format_get_base(format), ==, 1.0);
    g_assert_cmpuint(gwy_value_format_get_precision(format), ==, 2);
    g_assert(!gwy_value_format_get_units(format));
    g_object_unref(format);

    format = gwy_unit_format_with_digits(unit, GWY_VALUE_FORMAT_PLAIN, 10.1,
                                         3);
    g_assert_cmpfloat(gwy_value_format_get_base(format), ==, 1.0);
    g_assert_cmpuint(gwy_value_format_get_precision(format), ==, 1);
    g_assert(!gwy_value_format_get_units(format));
    g_object_unref(format);

    format = gwy_unit_format_with_digits(unit, GWY_VALUE_FORMAT_PLAIN, 100.1,
                                         3);
    g_assert_cmpfloat(gwy_value_format_get_base(format), ==, 1.0);
    g_assert_cmpuint(gwy_value_format_get_precision(format), ==, 0);
    g_assert(!gwy_value_format_get_units(format));
    g_object_unref(format);

    format = gwy_unit_format_with_digits(unit, GWY_VALUE_FORMAT_PLAIN, 1000.1,
                                         3);
    g_assert_cmpfloat(gwy_value_format_get_base(format), ==, 1000.0);
    g_assert_cmpuint(gwy_value_format_get_precision(format), ==, 2);
    g_assert_cmpstr(gwy_value_format_get_units(format), ==, "10^3");
    g_object_unref(format);

    format = gwy_unit_format_with_digits(unit, GWY_VALUE_FORMAT_PLAIN, 10000.1,
                                         3);
    g_assert_cmpfloat(gwy_value_format_get_base(format), ==, 1000.0);
    g_assert_cmpuint(gwy_value_format_get_precision(format), ==, 1);
    g_assert_cmpstr(gwy_value_format_get_units(format), ==, "10^3");
    g_object_unref(format);

    g_object_unref(unit);
}

void
test_unit_format_abnormal(void)
{
    GwyUnit *unit = gwy_unit_new();
    GwyValueFormat *format;

    format = gwy_unit_format_with_resolution(unit, GWY_VALUE_FORMAT_PLAIN,
                                             NAN, 1.0);
    g_assert_cmpfloat(gwy_value_format_get_base(format), ==, 1.0);
    g_assert_cmpuint(gwy_value_format_get_precision(format), ==, 0);
    g_object_unref(format);

    format = gwy_unit_format_with_resolution(unit, GWY_VALUE_FORMAT_PLAIN,
                                             1.0, NAN);
    g_assert_cmpfloat(gwy_value_format_get_base(format), ==, 1.0);
    g_assert_cmpuint(gwy_value_format_get_precision(format), ==, 0);
    g_object_unref(format);

    format = gwy_unit_format_with_digits(unit, GWY_VALUE_FORMAT_PLAIN, NAN, 1);
    g_assert_cmpfloat(gwy_value_format_get_base(format), ==, 1.0);
    g_assert_cmpuint(gwy_value_format_get_precision(format), ==, 0);
    g_object_unref(format);

    g_object_unref(unit);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
