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

#include "testlibgwy.h"

/***************************************************************************
 *
 * Value formatting
 *
 ***************************************************************************/

void
test_value_format_unit(void)
{
    GwyUnit *unit = gwy_unit_new_from_string("m", NULL);
    GwyValueFormat *format;

    /* Base cases */
    format = gwy_unit_format_with_resolution(unit, GWY_VALUE_FORMAT_PLAIN,
                                             1e-6, 1e-9);
    g_assert_cmpstr(gwy_value_format_print(format, 1.23456e-7),
                    ==, "0.123 µm");
    g_assert_cmpstr(gwy_value_format_print_number(format, 1.23456e-7),
                    ==, "0.123");
    g_object_unref(format);

    format = gwy_unit_format_with_resolution(unit, GWY_VALUE_FORMAT_PLAIN,
                                             1e-7, 1e-10);
    g_assert_cmpstr(gwy_value_format_print(format, 1.23456e-7),
                    ==, "123.5 nm");
    g_assert_cmpstr(gwy_value_format_print_number(format, 1.23456e-7),
                    ==, "123.5");
    g_object_unref(format);

    format = gwy_unit_format_with_resolution(unit, GWY_VALUE_FORMAT_PLAIN,
                                             1e-7, 1e-9);
    g_assert_cmpstr(gwy_value_format_print(format, 1.23456e-7),
                    ==, "123 nm");
    g_object_unref(format);

    /* Near-base cases, ensure values differing by step are distinguishable */
    format = gwy_unit_format_with_resolution(unit, GWY_VALUE_FORMAT_PLAIN,
                                             1e-7, 1.01e-10);
    g_assert_cmpstr(gwy_value_format_print(format, 1.23456e-7),
                    ==, "123.5 nm");
    g_object_unref(format);

    format = gwy_unit_format_with_resolution(unit, GWY_VALUE_FORMAT_PLAIN,
                                             1e-7, 0.99e-10);
    g_assert_cmpstr(gwy_value_format_print(format, 1.23456e-7),
                    ==, "123.46 nm");
    g_object_unref(format);

    g_object_unref(unit);
}

void
test_value_format_props(void)
{
    GwyValueFormat *format;

    /* Fancy formatting with base not a power of 10 */
    format = gwy_value_format_new();
    g_object_set(format,
                 "style", GWY_VALUE_FORMAT_PLAIN,
                 "base", G_PI/180.0,
                 "precision", 1,
                 "glue", " ",
                 "units", "deg",
                 NULL);
    g_assert_cmpstr(gwy_value_format_print(format, G_PI/6.0), ==, "30.0 deg");

    GwyValueFormatStyle style;
    gdouble base;
    guint precision;
    gchar *glue, *units;
    g_object_get(format,
                 "style", &style,
                 "base", &base,
                 "precision", &precision,
                 "glue", &glue,
                 "units", &units,
                 NULL);
    g_assert_cmpuint(style, ==, GWY_VALUE_FORMAT_PLAIN);
    g_assert_cmpuint(base, ==, G_PI/180.0);
    g_assert_cmpuint(precision, ==, 1);
    g_assert_cmpstr(glue, ==, " ");
    g_assert_cmpstr(units, ==, "deg");

    g_free(glue);
    g_free(units);

    g_object_unref(format);

    format = gwy_value_format_new_set(GWY_VALUE_FORMAT_PANGO,
                                      -3, 3, " ", "ms");
    g_assert_cmpuint(gwy_value_format_get_precision(format), ==, 3);
    g_assert_cmpfloat(gwy_value_format_get_base(format), ==, 1e-3);
    g_assert_cmpstr(gwy_value_format_get_glue(format), ==, " ");
    g_assert_cmpstr(gwy_value_format_get_units(format), ==, "ms");
    g_assert_cmpstr(gwy_value_format_print(format, -1.2e-3),
                    ==, "−1.200 ms");
    g_object_unref(format);


    format = gwy_value_format_new();
    gwy_value_format_set_precision(format, 5);
    gwy_value_format_set_base(format, 1e2);
    gwy_value_format_set_units(format, "A");
    g_object_get(format,
                 "base", &base,
                 "precision", &precision,
                 NULL);
    g_assert_cmpfloat(base, ==, 1e2);
    g_assert_cmpuint(precision, ==, 5);
    g_assert_cmpstr(gwy_value_format_print(format, 1234.5),
                    ==, "12.34500A");

    gwy_value_format_set_precision(format, 1);
    gwy_value_format_set_power10(format, 0);
    g_object_get(format,
                 "base", &base,
                 "precision", &precision,
                 NULL);
    g_assert_cmpfloat(base, ==, 1.0);
    g_assert_cmpuint(precision, ==, 1);
    g_assert_cmpstr(gwy_value_format_print(format, 1234.5),
                    ==, "1234.5A");
    g_object_unref(format);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
