/*
 *  $Id$
 *  Copyright (C) 2009,2011 David Nečas (Yeti).
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

void
test_unit_parse(void)
{
    gint pw1, pw2, pw3, pw4, pw5, pw6, pw7, pw8, pw9;
    gint pw11, pw12, pw13, pw14, pw15, pw16, pw17, pw18;

    /* Simple notations */
    GwyUnit *u1 = gwy_unit_new_from_string("m", &pw1);
    GwyUnit *u2 = gwy_unit_new_from_string("km", &pw2);
    GwyUnit *u3 = gwy_unit_new_from_string("Å", &pw3);

    g_assert(gwy_unit_equal(u1, u2));
    g_assert(gwy_unit_equal(u2, u3));
    g_assert(gwy_unit_equal(u3, u1));
    g_assert_cmpint(pw1, ==, 0);
    g_assert_cmpint(pw2, ==, 3);
    g_assert_cmpint(pw3, ==, -10);

    g_object_unref(u1);
    g_object_unref(u2);
    g_object_unref(u3);

    /* Powers and comparison */
    GwyUnit *u4 = gwy_unit_new_from_string("um s^-1", &pw4);
    GwyUnit *u5 = gwy_unit_new_from_string("mm/ps", &pw5);
    GwyUnit *u6 = gwy_unit_new_from_string("μs<sup>-1</sup> cm", &pw6);

    g_assert(gwy_unit_equal(u4, u5));
    g_assert(gwy_unit_equal(u5, u6));
    g_assert(gwy_unit_equal(u6, u4));
    g_assert_cmpint(pw4, ==, -6);
    g_assert_cmpint(pw5, ==, 9);
    g_assert_cmpint(pw6, ==, 4);

    g_object_unref(u4);
    g_object_unref(u5);
    g_object_unref(u6);

    /* Cancellation */
    GwyUnit *u7 = gwy_unit_new_from_string(NULL, &pw7);
    GwyUnit *u8 = gwy_unit_new_from_string("10%", &pw8);
    GwyUnit *u9 = gwy_unit_new_from_string("m^3 cm^-2/km", &pw9);

    g_assert(gwy_unit_equal(u7, u8));
    g_assert(gwy_unit_equal(u8, u9));
    g_assert(gwy_unit_equal(u9, u7));
    g_assert_cmpint(pw7, ==, 0);
    g_assert_cmpint(pw8, ==, -1);
    g_assert_cmpint(pw9, ==, 1);

    g_object_unref(u7);
    g_object_unref(u8);
    g_object_unref(u9);

    /* Silly notations: micron */
    GwyUnit *u11 = gwy_unit_new_from_string("μs", &pw11);
    GwyUnit *u12 = gwy_unit_new_from_string("µs", &pw12);
    GwyUnit *u13 = gwy_unit_new_from_string("us", &pw13);
    GwyUnit *u14 = gwy_unit_new_from_string("~s", &pw14);
    GwyUnit *u15 = gwy_unit_new_from_string("\265s", &pw15);

    g_assert(gwy_unit_equal(u11, u12));
    g_assert(gwy_unit_equal(u12, u13));
    g_assert(gwy_unit_equal(u13, u14));
    g_assert(gwy_unit_equal(u14, u15));
    g_assert(gwy_unit_equal(u15, u11));
    g_assert_cmpint(pw11, ==, -6);
    g_assert_cmpint(pw12, ==, -6);
    g_assert_cmpint(pw13, ==, -6);
    g_assert_cmpint(pw14, ==, -6);
    g_assert_cmpint(pw15, ==, -6);

    g_object_unref(u11);
    g_object_unref(u12);
    g_object_unref(u13);
    g_object_unref(u14);
    g_object_unref(u15);

    /* Silly notations: squares */
    GwyUnit *u16 = gwy_unit_new_from_string("m²", &pw16);
    GwyUnit *u17 = gwy_unit_new_from_string("m m", &pw17);
    GwyUnit *u18 = gwy_unit_new_from_string("m\262", &pw18);

    g_assert(gwy_unit_equal(u16, u17));
    g_assert(gwy_unit_equal(u17, u18));
    g_assert(gwy_unit_equal(u18, u16));
    g_assert_cmpint(pw16, ==, 0);
    g_assert_cmpint(pw17, ==, 0);
    g_assert_cmpint(pw18, ==, 0);

    g_object_unref(u16);
    g_object_unref(u17);
    g_object_unref(u18);
}

void
test_unit_word_names(void)
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
test_unit_serialize_one(GwyUnit *unit1)
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
    test_unit_serialize_one(unit);
    g_object_unref(unit);

    /* Less trivial unit */
    unit = gwy_unit_new_from_string("kg m s^-2/nA", NULL);
    test_unit_serialize_one(unit);
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
    static const gchar *tokens[] = {
        "^", "+", "-", "/", "*", "<sup>", "</sup>", "10",
    };
    static const gchar characters[] = G_CSET_a_2_z G_CSET_A_2_Z G_CSET_DIGITS;
    GwyUnit *unit = gwy_unit_new();

    gsize n = 10000;
    GString *garbage = g_string_new(NULL);
    GRand *rng = g_rand_new_with_seed(42);

    /* No checks.  The goal is not to crash... */
    for (gsize i = 0; i < n; i++) {
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

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
