/*
 *  $Id$
 *  Copyright (C) 2011-2013 David Nečas (Yeti).
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
 * Brick
 *
 ***************************************************************************/

void
brick_print(const gchar *name, const GwyBrick *brick)
{
    g_printerr("=====[ %s ]=====\n", name);
    g_printerr("size %u x %u x %u, real %g x %g x %g, offsets %g x %g x %g\n",
               brick->xres, brick->yres, brick->zres,
               brick->xreal, brick->yreal, brick->zreal,
               brick->xoff, brick->yoff, brick->zoff);
    for (guint l = 0; l < brick->zres; l++) {
        for (guint i = 0; i < brick->yres; i++) {
            g_printerr("[%02u][%02u]", l, i);
            for (guint j = 0; j < brick->xres; j++) {
                g_printerr(" % .4f", gwy_brick_index(brick, j, i, l));
            }
            g_printerr("\n");
        }
    }
}

void
brick_randomize(GwyBrick *brick,
                GRand *rng)
{
    gdouble *d = brick->data;
    for (guint n = brick->xres*brick->yres*brick->zres; n; n--, d++)
        *d = g_rand_double(rng);
    gwy_brick_invalidate(brick);
}

void
brick_assert_equal(const GwyBrick *result,
                   const GwyBrick *reference)
{
    g_assert(GWY_IS_BRICK(result));
    g_assert(GWY_IS_BRICK(reference));
    g_assert_cmpuint(result->xres, ==, reference->xres);
    g_assert_cmpuint(result->yres, ==, reference->yres);
    g_assert_cmpuint(result->zres, ==, reference->zres);
    compare_properties(G_OBJECT(result), G_OBJECT(reference));

    guint xres = result->xres, yres = result->yres, zres = result->zres;
    for (guint l = 0; l < zres; l++) {
        const gdouble *result_level
            = result->data + l*xres*yres;
        const gdouble *reference_level
            = reference->data + l*xres*yres;
        for (guint i = 0; i < yres; i++) {
            const gdouble *result_row = result_level + i*xres;
            const gdouble *reference_row = reference_level + i*xres;
            for (guint j = 0; j < xres; j++)
                g_assert_cmpfloat(result_row[j], ==, reference_row[j]);
        }
    }
}

static void
brick_assert_equal_object(GObject *object, GObject *reference)
{
    brick_assert_equal(GWY_BRICK(object), GWY_BRICK(reference));
}

void
brick_assert_numerically_equal(const GwyBrick *result,
                               const GwyBrick *reference,
                               gdouble eps)
{
    g_assert(GWY_IS_BRICK(result));
    g_assert(GWY_IS_BRICK(reference));
    g_assert_cmpuint(result->xres, ==, reference->xres);
    g_assert_cmpuint(result->yres, ==, reference->yres);
    g_assert_cmpuint(result->zres, ==, reference->zres);

    guint xres = result->xres, yres = result->yres, zres = result->zres;
    for (guint l = 0; l < zres; l++) {
        const gdouble *result_level
            = result->data + l*xres*yres;
        const gdouble *reference_level
            = reference->data + l*xres*yres;
        for (guint i = 0; i < yres; i++) {
            const gdouble *result_row = result_level + i*xres;
            const gdouble *reference_row = reference_level + i*xres;
            for (guint j = 0; j < xres; j++) {
                gdouble value = result_row[j], ref = reference_row[j];
                //g_printerr("[%u,%u,%u] %g %g\n", j, i, l, value, ref);
                gwy_assert_floatval(value, ref, eps);
            }
        }
    }
}

void
test_brick_units_assign(void)
{
    GwyBrick *brick = gwy_brick_new(), *brick2 = gwy_brick_new();
    GwyUnit *xunit = gwy_brick_get_xunit(brick);
    GwyUnit *yunit = gwy_brick_get_yunit(brick);
    GwyUnit *zunit = gwy_brick_get_zunit(brick);
    GwyUnit *wunit = gwy_brick_get_wunit(brick);
    guint count_x = 0;
    guint count_y = 0;
    guint count_z = 0;
    guint count_w = 0;

    g_signal_connect_swapped(xunit, "changed",
                             G_CALLBACK(record_signal), &count_x);
    g_signal_connect_swapped(yunit, "changed",
                             G_CALLBACK(record_signal), &count_y);
    g_signal_connect_swapped(zunit, "changed",
                             G_CALLBACK(record_signal), &count_z);
    g_signal_connect_swapped(wunit, "changed",
                             G_CALLBACK(record_signal), &count_w);

    gwy_unit_set_from_string(gwy_brick_get_xunit(brick), "m", NULL);
    g_assert_cmpuint(count_x, ==, 1);
    g_assert_cmpuint(count_y, ==, 0);
    g_assert_cmpuint(count_z, ==, 0);
    g_assert_cmpuint(count_w, ==, 0);

    gwy_unit_set_from_string(gwy_brick_get_yunit(brick), "A", NULL);
    g_assert_cmpuint(count_x, ==, 1);
    g_assert_cmpuint(count_y, ==, 1);
    g_assert_cmpuint(count_z, ==, 0);
    g_assert_cmpuint(count_w, ==, 0);

    gwy_unit_set_from_string(gwy_brick_get_zunit(brick), "s", NULL);
    g_assert_cmpuint(count_x, ==, 1);
    g_assert_cmpuint(count_y, ==, 1);
    g_assert_cmpuint(count_z, ==, 1);
    g_assert_cmpuint(count_w, ==, 0);

    gwy_unit_set_from_string(gwy_brick_get_wunit(brick), "kg", NULL);
    g_assert_cmpuint(count_x, ==, 1);
    g_assert_cmpuint(count_y, ==, 1);
    g_assert_cmpuint(count_z, ==, 1);
    g_assert_cmpuint(count_w, ==, 1);

    gwy_brick_assign(brick, brick2);
    g_assert_cmpuint(count_x, ==, 2);
    g_assert_cmpuint(count_y, ==, 2);
    g_assert_cmpuint(count_z, ==, 2);
    g_assert_cmpuint(count_w, ==, 2);
    g_assert(gwy_brick_get_xunit(brick) == xunit);
    g_assert(gwy_brick_get_yunit(brick) == yunit);
    g_assert(gwy_brick_get_zunit(brick) == zunit);
    g_assert(gwy_brick_get_wunit(brick) == wunit);

    // Try again to see if the signal counts change.
    gwy_brick_assign(brick, brick2);
    g_assert_cmpuint(count_x, ==, 2);
    g_assert_cmpuint(count_y, ==, 2);
    g_assert_cmpuint(count_z, ==, 2);
    g_assert_cmpuint(count_w, ==, 2);
    g_assert(gwy_brick_get_xunit(brick) == xunit);
    g_assert(gwy_brick_get_yunit(brick) == yunit);
    g_assert(gwy_brick_get_zunit(brick) == zunit);
    g_assert(gwy_brick_get_wunit(brick) == wunit);

    g_object_unref(brick2);
    g_object_unref(brick);
}

void
test_brick_get(void)
{
    enum { max_size = 15, niter = 40 };

    GRand *rng = g_rand_new_with_seed(42);

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size);
        guint zres = g_rand_int_range(rng, 1, max_size);
        GwyBrick *brick = gwy_brick_new_sized(xres, yres, zres, FALSE);
        brick_randomize(brick, rng);

        for (guint k = 0; k < zres; k++) {
            for (guint i = 0; i < yres; i++) {
                for (guint j = 0; j < xres; j++) {
                    g_assert_cmpfloat(brick->data[(k*yres + i)*xres + j],
                                      ==,
                                      gwy_brick_get(brick, j, i, k));
                }
            }
        }
        g_object_unref(brick);
    }

    g_rand_free(rng);
}

void
test_brick_set(void)
{
    enum { max_size = 15, niter = 40 };

    GRand *rng = g_rand_new_with_seed(42);

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size);
        guint zres = g_rand_int_range(rng, 1, max_size);
        GwyBrick *brick = gwy_brick_new_sized(xres, yres, zres, FALSE);

        for (guint k = 0; k < zres; k++) {
            for (guint i = 0; i < yres; i++) {
                for (guint j = 0; j < xres; j++)
                    gwy_brick_set(brick, j, i, k, (k*yres + i)*xres + j);
            }
        }

        for (guint k = 0; k < xres*yres*zres; k++) {
            g_assert_cmpfloat(brick->data[k], ==, k);
        }
        g_object_unref(brick);
    }

    g_rand_free(rng);
}

void
test_brick_props(void)
{
    GwyBrick *brick = gwy_brick_new_sized(21, 17, 14, FALSE);
    guint xres_changed = 0, yres_changed = 0, zres_changed = 0,
          xreal_changed = 0, yreal_changed = 0, zreal_changed = 0,
          xoff_changed = 0, yoff_changed = 0, zoff_changed = 0,
          name_changed = 0;
    g_signal_connect_swapped(brick, "notify::x-res",
                             G_CALLBACK(record_signal), &xres_changed);
    g_signal_connect_swapped(brick, "notify::y-res",
                             G_CALLBACK(record_signal), &yres_changed);
    g_signal_connect_swapped(brick, "notify::z-res",
                             G_CALLBACK(record_signal), &zres_changed);
    g_signal_connect_swapped(brick, "notify::x-real",
                             G_CALLBACK(record_signal), &xreal_changed);
    g_signal_connect_swapped(brick, "notify::y-real",
                             G_CALLBACK(record_signal), &yreal_changed);
    g_signal_connect_swapped(brick, "notify::z-real",
                             G_CALLBACK(record_signal), &zreal_changed);
    g_signal_connect_swapped(brick, "notify::x-offset",
                             G_CALLBACK(record_signal), &xoff_changed);
    g_signal_connect_swapped(brick, "notify::y-offset",
                             G_CALLBACK(record_signal), &yoff_changed);
    g_signal_connect_swapped(brick, "notify::z-offset",
                             G_CALLBACK(record_signal), &zoff_changed);
    g_signal_connect_swapped(brick, "notify::name",
                             G_CALLBACK(record_signal), &name_changed);

    guint xres, yres, zres;
    gdouble xreal, yreal, zreal, xoff, yoff, zoff;
    gchar *name;
    g_object_get(brick,
                 "x-res", &xres,
                 "y-res", &yres,
                 "z-res", &zres,
                 "x-real", &xreal,
                 "y-real", &yreal,
                 "z-real", &zreal,
                 "x-offset", &xoff,
                 "y-offset", &yoff,
                 "z-offset", &zoff,
                 "name", &name,
                 NULL);
    g_assert_cmpuint(xres, ==, brick->xres);
    g_assert_cmpuint(yres, ==, brick->yres);
    g_assert_cmpuint(zres, ==, brick->zres);
    g_assert_cmpfloat(xreal, ==, brick->xreal);
    g_assert_cmpfloat(yreal, ==, brick->yreal);
    g_assert_cmpfloat(zreal, ==, brick->zreal);
    g_assert_cmpfloat(xoff, ==, brick->xoff);
    g_assert_cmpfloat(yoff, ==, brick->yoff);
    g_assert_cmpfloat(zoff, ==, brick->zoff);
    g_assert(!name);
    g_assert_cmpuint(xres_changed, ==, 0);
    g_assert_cmpuint(yres_changed, ==, 0);
    g_assert_cmpuint(zres_changed, ==, 0);
    g_assert_cmpuint(xreal_changed, ==, 0);
    g_assert_cmpuint(yreal_changed, ==, 0);
    g_assert_cmpuint(zreal_changed, ==, 0);
    g_assert_cmpuint(xoff_changed, ==, 0);
    g_assert_cmpuint(yoff_changed, ==, 0);
    g_assert_cmpuint(zoff_changed, ==, 0);
    g_assert_cmpuint(name_changed, ==, 0);

    xreal = 5.0;
    yreal = 7.0e-14;
    zreal = 4.44e8;
    xoff = -3;
    yoff = 1e-15;
    zoff = -2e7;
    gwy_brick_set_xreal(brick, xreal);
    g_assert_cmpuint(xreal_changed, ==, 1);
    gwy_brick_set_yreal(brick, yreal);
    g_assert_cmpuint(yreal_changed, ==, 1);
    gwy_brick_set_zreal(brick, zreal);
    g_assert_cmpuint(zreal_changed, ==, 1);
    gwy_brick_set_xoffset(brick, xoff);
    g_assert_cmpuint(xoff_changed, ==, 1);
    gwy_brick_set_yoffset(brick, yoff);
    g_assert_cmpuint(yoff_changed, ==, 1);
    gwy_brick_set_zoffset(brick, zoff);
    g_assert_cmpuint(zoff_changed, ==, 1);
    gwy_brick_set_name(brick, "First");
    g_assert_cmpuint(name_changed, ==, 1);
    g_assert_cmpuint(xres_changed, ==, 0);
    g_assert_cmpuint(yres_changed, ==, 0);
    g_assert_cmpuint(zres_changed, ==, 0);
    g_assert_cmpfloat(xreal, ==, brick->xreal);
    g_assert_cmpfloat(yreal, ==, brick->yreal);
    g_assert_cmpfloat(zreal, ==, brick->zreal);
    g_assert_cmpfloat(xoff, ==, brick->xoff);
    g_assert_cmpfloat(yoff, ==, brick->yoff);
    g_assert_cmpfloat(zoff, ==, brick->zoff);
    g_assert_cmpstr(gwy_brick_get_name(brick), ==, "First");

    // Do it twice to excersise no-change behaviour.
    gwy_brick_set_xreal(brick, xreal);
    gwy_brick_set_yreal(brick, yreal);
    gwy_brick_set_zreal(brick, zreal);
    gwy_brick_set_xoffset(brick, xoff);
    gwy_brick_set_yoffset(brick, yoff);
    gwy_brick_set_zoffset(brick, zoff);
    gwy_brick_set_name(brick, "First");
    g_assert_cmpuint(xres_changed, ==, 0);
    g_assert_cmpuint(yres_changed, ==, 0);
    g_assert_cmpuint(zres_changed, ==, 0);
    g_assert_cmpuint(xreal_changed, ==, 1);
    g_assert_cmpuint(yreal_changed, ==, 1);
    g_assert_cmpuint(zreal_changed, ==, 1);
    g_assert_cmpuint(xoff_changed, ==, 1);
    g_assert_cmpuint(yoff_changed, ==, 1);
    g_assert_cmpuint(zoff_changed, ==, 1);
    g_assert_cmpuint(name_changed, ==, 1);
    g_assert_cmpfloat(xreal, ==, brick->xreal);
    g_assert_cmpfloat(yreal, ==, brick->yreal);
    g_assert_cmpfloat(zreal, ==, brick->zreal);
    g_assert_cmpfloat(xoff, ==, brick->xoff);
    g_assert_cmpfloat(yoff, ==, brick->yoff);
    g_assert_cmpfloat(zoff, ==, brick->zoff);
    g_assert_cmpstr(gwy_brick_get_name(brick), ==, "First");

    xreal = 1e16;
    yreal = 0.6;
    zreal = 1e-5;
    xoff = 0.0;
    yoff = 1e-5;
    zoff = -2.0;
    g_object_set(brick,
                 "x-real", xreal,
                 "y-real", yreal,
                 "z-real", zreal,
                 "x-offset", xoff,
                 "y-offset", yoff,
                 "z-offset", zoff,
                 "name", "Second",
                 NULL);
    g_assert_cmpuint(xres_changed, ==, 0);
    g_assert_cmpuint(yres_changed, ==, 0);
    g_assert_cmpuint(zres_changed, ==, 0);
    g_assert_cmpuint(xreal_changed, ==, 2);
    g_assert_cmpuint(yreal_changed, ==, 2);
    g_assert_cmpuint(zreal_changed, ==, 2);
    g_assert_cmpuint(xoff_changed, ==, 2);
    g_assert_cmpuint(yoff_changed, ==, 2);
    g_assert_cmpuint(zoff_changed, ==, 2);
    g_assert_cmpuint(name_changed, ==, 2);
    g_assert_cmpfloat(xreal, ==, brick->xreal);
    g_assert_cmpfloat(yreal, ==, brick->yreal);
    g_assert_cmpfloat(zreal, ==, brick->zreal);
    g_assert_cmpfloat(xoff, ==, brick->xoff);
    g_assert_cmpfloat(yoff, ==, brick->yoff);
    g_assert_cmpfloat(zoff, ==, brick->zoff);
    g_assert_cmpstr(gwy_brick_get_name(brick), ==, "Second");

    g_object_unref(brick);
}

void
test_brick_data_changed(void)
{
    // Plain emission
    GwyBrick *brick = gwy_brick_new();
    guint counter = 0;
    g_signal_connect_swapped(brick, "data-changed",
                             G_CALLBACK(record_signal), &counter);
    gwy_brick_data_changed(brick, NULL);
    g_assert_cmpuint(counter, ==, 1);
    g_object_unref(brick);

    // Specified part argument
    brick = gwy_brick_new_sized(10, 10, 10, FALSE);
    GwyBrickPart bpart = { 1, 2, 3, 4, 5, 6 };
    g_signal_connect_swapped(brick, "data-changed",
                             G_CALLBACK(brick_part_assert_equal), &bpart);
    gwy_brick_data_changed(brick, &bpart);
    g_object_unref(brick);

    // NULL part argument
    brick = gwy_brick_new_sized(2, 3, 4, FALSE);
    g_signal_connect_swapped(brick, "data-changed",
                             G_CALLBACK(brick_part_assert_equal), NULL);
    gwy_brick_data_changed(brick, NULL);
    g_object_unref(brick);
}

void
test_brick_units(void)
{
    enum { max_size = 30 };
    GRand *rng = g_rand_new_with_seed(42);
    gsize niter = 50;

    for (guint iter = 0; iter < niter; iter++) {
        guint width = g_rand_int_range(rng, 1, max_size);
        guint height = g_rand_int_range(rng, 1, max_size);
        guint depth = g_rand_int_range(rng, 1, max_size);
        GwyBrick *brick = gwy_brick_new_sized(width, height, depth, FALSE);
        g_object_set(brick,
                     "x-real", -log(1.0 - g_rand_double(rng)),
                     "y-real", -log(1.0 - g_rand_double(rng)),
                     "z-real", -log(1.0 - g_rand_double(rng)),
                     NULL);
        GString *prev = g_string_new(NULL), *next = g_string_new(NULL);
        GwyValueFormat *format = gwy_brick_format_xy(brick,
                                                     GWY_VALUE_FORMAT_PLAIN);
        gdouble dx = gwy_brick_dx(brick);
        for (gint i = 0; i <= 10; i++) {
            GWY_SWAP(GString*, prev, next);
            g_string_assign(next,
                            gwy_value_format_print_number(format, (i - 5)*dx));
            if (i)
                g_assert_cmpstr(prev->str, !=, next->str);
        }
        gdouble dy = gwy_brick_dy(brick);
        for (gint i = 0; i <= 10; i++) {
            GWY_SWAP(GString*, prev, next);
            g_string_assign(next,
                            gwy_value_format_print_number(format, (i - 5)*dy));
            if (i)
                g_assert_cmpstr(prev->str, !=, next->str);
        }
        g_object_unref(format);

        format = gwy_brick_format_z(brick, GWY_VALUE_FORMAT_PLAIN);
        gdouble dz = gwy_brick_dz(brick);
        for (gint i = 0; i <= 10; i++) {
            GWY_SWAP(GString*, prev, next);
            g_string_assign(next,
                            gwy_value_format_print_number(format, (i - 5)*dz));
            if (i)
                g_assert_cmpstr(prev->str, !=, next->str);
        }
        g_object_unref(format);
        g_object_unref(brick);
        g_string_free(prev, TRUE);
        g_string_free(next, TRUE);
    }
    g_rand_free(rng);
}

void
test_brick_serialize(void)
{
    enum { max_size = 25 };
    GRand *rng = g_rand_new_with_seed(42);
    gsize niter = g_test_slow() ? 50 : 20;

    for (guint iter = 0; iter < niter; iter++) {
        guint width = g_rand_int_range(rng, 1, max_size);
        guint height = g_rand_int_range(rng, 1, max_size/2);
        guint depth = g_rand_int_range(rng, 1, max_size/4);
        GwyBrick *original = gwy_brick_new_sized(width, height, depth, FALSE);
        brick_randomize(original, rng);
        GwyBrick *copy;

        if (g_rand_int(rng) % 5)
            unit_randomize(gwy_brick_get_xunit(original), rng);
        if (g_rand_int(rng) % 5)
            unit_randomize(gwy_brick_get_yunit(original), rng);
        if (g_rand_int(rng) % 5)
            unit_randomize(gwy_brick_get_zunit(original), rng);
        if (g_rand_int(rng) % 5)
            unit_randomize(gwy_brick_get_wunit(original), rng);

        if (g_rand_int(rng) % 3 == 0)
            gwy_brick_set_xoffset(original, g_rand_double(rng));
        if (g_rand_int(rng) % 3 == 0)
            gwy_brick_set_yoffset(original, g_rand_double(rng));
        if (g_rand_int(rng) % 3 == 0)
            gwy_brick_set_zoffset(original, g_rand_double(rng));

        serializable_duplicate(GWY_SERIALIZABLE(original),
                               brick_assert_equal_object);
        serializable_assign(GWY_SERIALIZABLE(original),
                            brick_assert_equal_object);
        copy = GWY_BRICK(serialize_and_back(G_OBJECT(original),
                                            brick_assert_equal_object));
        g_object_unref(copy);
        g_object_unref(original);
    }
    g_rand_free(rng);
}

void
test_brick_serialize_failure_xres0(void)
{
    GOutputStream *stream = g_memory_output_stream_new(NULL, 0,
                                                       g_realloc, g_free);
    GDataOutputStream *datastream = g_data_output_stream_new(stream);
    g_data_output_stream_set_byte_order(datastream,
                                        G_DATA_STREAM_BYTE_ORDER_LITTLE_ENDIAN);

    data_stream_put_string0(datastream, "GwyBrick", NULL, NULL);
    g_data_output_stream_put_uint64(datastream, 0, NULL, NULL);
    data_stream_put_string0(datastream, "xres", NULL, NULL);
    g_data_output_stream_put_byte(datastream, GWY_SERIALIZABLE_INT32,
                                  NULL, NULL);
    g_data_output_stream_put_uint32(datastream, 0, NULL, NULL);
    data_stream_put_string0(datastream, "yres", NULL, NULL);
    g_data_output_stream_put_byte(datastream, GWY_SERIALIZABLE_INT32,
                                  NULL, NULL);
    g_data_output_stream_put_uint32(datastream, 2, NULL, NULL);
    data_stream_put_string0(datastream, "zres", NULL, NULL);
    g_data_output_stream_put_byte(datastream, GWY_SERIALIZABLE_INT32,
                                  NULL, NULL);
    g_data_output_stream_put_uint32(datastream, 1, NULL, NULL);
    data_stream_put_string0(datastream, "xreal", NULL, NULL);
    g_data_output_stream_put_byte(datastream, GWY_SERIALIZABLE_DOUBLE,
                                  NULL, NULL);
    data_stream_put_double(datastream, 1.0, NULL, NULL);
    data_stream_put_string0(datastream, "yreal", NULL, NULL);
    g_data_output_stream_put_byte(datastream, GWY_SERIALIZABLE_DOUBLE,
                                  NULL, NULL);
    data_stream_put_double(datastream, 1.0, NULL, NULL);
    data_stream_put_string0(datastream, "zreal", NULL, NULL);
    g_data_output_stream_put_byte(datastream, GWY_SERIALIZABLE_DOUBLE,
                                  NULL, NULL);
    data_stream_put_double(datastream, 1.0, NULL, NULL);

    GwyErrorList *error_list = NULL;
    gwy_error_list_add(&error_list,
                       GWY_DESERIALIZE_ERROR, GWY_DESERIALIZE_ERROR_INVALID,
                       "Dimension %u×%u×%u of ‘GwyBrick’ is invalid.", 0, 2, 1);

    deserialize_assert_failure(G_MEMORY_OUTPUT_STREAM(stream), error_list);
    gwy_error_list_clear(&error_list);
    g_object_unref(datastream);
    g_object_unref(stream);
}

void
test_brick_serialize_failure_yres0(void)
{
    GOutputStream *stream = g_memory_output_stream_new(NULL, 0,
                                                       g_realloc, g_free);
    GDataOutputStream *datastream = g_data_output_stream_new(stream);
    g_data_output_stream_set_byte_order(datastream,
                                        G_DATA_STREAM_BYTE_ORDER_LITTLE_ENDIAN);

    data_stream_put_string0(datastream, "GwyBrick", NULL, NULL);
    g_data_output_stream_put_uint64(datastream, 0, NULL, NULL);
    data_stream_put_string0(datastream, "xres", NULL, NULL);
    g_data_output_stream_put_byte(datastream, GWY_SERIALIZABLE_INT32,
                                  NULL, NULL);
    g_data_output_stream_put_uint32(datastream, 3, NULL, NULL);
    data_stream_put_string0(datastream, "yres", NULL, NULL);
    g_data_output_stream_put_byte(datastream, GWY_SERIALIZABLE_INT32,
                                  NULL, NULL);
    g_data_output_stream_put_uint32(datastream, 0, NULL, NULL);
    data_stream_put_string0(datastream, "zres", NULL, NULL);
    g_data_output_stream_put_byte(datastream, GWY_SERIALIZABLE_INT32,
                                  NULL, NULL);
    g_data_output_stream_put_uint32(datastream, 1, NULL, NULL);
    data_stream_put_string0(datastream, "xreal", NULL, NULL);
    g_data_output_stream_put_byte(datastream, GWY_SERIALIZABLE_DOUBLE,
                                  NULL, NULL);
    data_stream_put_double(datastream, 1.0, NULL, NULL);
    data_stream_put_string0(datastream, "yreal", NULL, NULL);
    g_data_output_stream_put_byte(datastream, GWY_SERIALIZABLE_DOUBLE,
                                  NULL, NULL);
    data_stream_put_double(datastream, 1.0, NULL, NULL);
    data_stream_put_string0(datastream, "zreal", NULL, NULL);
    g_data_output_stream_put_byte(datastream, GWY_SERIALIZABLE_DOUBLE,
                                  NULL, NULL);
    data_stream_put_double(datastream, 1.0, NULL, NULL);

    GwyErrorList *error_list = NULL;
    gwy_error_list_add(&error_list,
                       GWY_DESERIALIZE_ERROR, GWY_DESERIALIZE_ERROR_INVALID,
                       "Dimension %u×%u×%u of ‘GwyBrick’ is invalid.", 3, 0, 1);

    deserialize_assert_failure(G_MEMORY_OUTPUT_STREAM(stream), error_list);
    gwy_error_list_clear(&error_list);
    g_object_unref(datastream);
    g_object_unref(stream);
}

void
test_brick_serialize_failure_zres0(void)
{
    GOutputStream *stream = g_memory_output_stream_new(NULL, 0,
                                                       g_realloc, g_free);
    GDataOutputStream *datastream = g_data_output_stream_new(stream);
    g_data_output_stream_set_byte_order(datastream,
                                        G_DATA_STREAM_BYTE_ORDER_LITTLE_ENDIAN);

    data_stream_put_string0(datastream, "GwyBrick", NULL, NULL);
    g_data_output_stream_put_uint64(datastream, 0, NULL, NULL);
    data_stream_put_string0(datastream, "xres", NULL, NULL);
    g_data_output_stream_put_byte(datastream, GWY_SERIALIZABLE_INT32,
                                  NULL, NULL);
    g_data_output_stream_put_uint32(datastream, 3, NULL, NULL);
    data_stream_put_string0(datastream, "yres", NULL, NULL);
    g_data_output_stream_put_byte(datastream, GWY_SERIALIZABLE_INT32,
                                  NULL, NULL);
    g_data_output_stream_put_uint32(datastream, 2, NULL, NULL);
    data_stream_put_string0(datastream, "zres", NULL, NULL);
    g_data_output_stream_put_byte(datastream, GWY_SERIALIZABLE_INT32,
                                  NULL, NULL);
    g_data_output_stream_put_uint32(datastream, 0, NULL, NULL);
    data_stream_put_string0(datastream, "xreal", NULL, NULL);
    g_data_output_stream_put_byte(datastream, GWY_SERIALIZABLE_DOUBLE,
                                  NULL, NULL);
    data_stream_put_double(datastream, 1.0, NULL, NULL);
    data_stream_put_string0(datastream, "yreal", NULL, NULL);
    g_data_output_stream_put_byte(datastream, GWY_SERIALIZABLE_DOUBLE,
                                  NULL, NULL);
    data_stream_put_double(datastream, 1.0, NULL, NULL);
    data_stream_put_string0(datastream, "zreal", NULL, NULL);
    g_data_output_stream_put_byte(datastream, GWY_SERIALIZABLE_DOUBLE,
                                  NULL, NULL);
    data_stream_put_double(datastream, 1.0, NULL, NULL);

    GwyErrorList *error_list = NULL;
    gwy_error_list_add(&error_list,
                       GWY_DESERIALIZE_ERROR, GWY_DESERIALIZE_ERROR_INVALID,
                       "Dimension %u×%u×%u of ‘GwyBrick’ is invalid.", 3, 2, 0);

    deserialize_assert_failure(G_MEMORY_OUTPUT_STREAM(stream), error_list);
    gwy_error_list_clear(&error_list);
    g_object_unref(datastream);
    g_object_unref(stream);
}

void
test_brick_serialize_failure_size(void)
{
    GOutputStream *stream = g_memory_output_stream_new(NULL, 0,
                                                       g_realloc, g_free);
    GDataOutputStream *datastream = g_data_output_stream_new(stream);
    g_data_output_stream_set_byte_order(datastream,
                                        G_DATA_STREAM_BYTE_ORDER_LITTLE_ENDIAN);

    data_stream_put_string0(datastream, "GwyBrick", NULL, NULL);
    g_data_output_stream_put_uint64(datastream, 0, NULL, NULL);
    data_stream_put_string0(datastream, "xres", NULL, NULL);
    g_data_output_stream_put_byte(datastream, GWY_SERIALIZABLE_INT32,
                                  NULL, NULL);
    g_data_output_stream_put_uint32(datastream, 3, NULL, NULL);
    data_stream_put_string0(datastream, "yres", NULL, NULL);
    g_data_output_stream_put_byte(datastream, GWY_SERIALIZABLE_INT32,
                                  NULL, NULL);
    g_data_output_stream_put_uint32(datastream, 2, NULL, NULL);
    data_stream_put_string0(datastream, "zres", NULL, NULL);
    g_data_output_stream_put_byte(datastream, GWY_SERIALIZABLE_INT32,
                                  NULL, NULL);
    g_data_output_stream_put_uint32(datastream, 1, NULL, NULL);
    data_stream_put_string0(datastream, "xreal", NULL, NULL);
    g_data_output_stream_put_byte(datastream, GWY_SERIALIZABLE_DOUBLE,
                                  NULL, NULL);
    data_stream_put_double(datastream, 1.0, NULL, NULL);
    data_stream_put_string0(datastream, "yreal", NULL, NULL);
    g_data_output_stream_put_byte(datastream, GWY_SERIALIZABLE_DOUBLE,
                                  NULL, NULL);
    data_stream_put_double(datastream, 1.0, NULL, NULL);
    data_stream_put_string0(datastream, "zreal", NULL, NULL);
    g_data_output_stream_put_byte(datastream, GWY_SERIALIZABLE_DOUBLE,
                                  NULL, NULL);
    data_stream_put_double(datastream, 1.0, NULL, NULL);
    guint len = 5;
    data_stream_put_string0(datastream, "data", NULL, NULL);
    g_data_output_stream_put_byte(datastream, GWY_SERIALIZABLE_DOUBLE_ARRAY,
                                  NULL, NULL);
    g_data_output_stream_put_uint64(datastream, len, NULL, NULL);
    for (guint i = 0; i < len; i++)
        data_stream_put_double(datastream, i, NULL, NULL);

    GwyErrorList *error_list = NULL;
    gwy_error_list_add(&error_list,
                       GWY_DESERIALIZE_ERROR, GWY_DESERIALIZE_ERROR_INVALID,
                       "GwyBrick dimensions %u×%u×%u do not match "
                       "data size %lu.",
                       3, 2, 1, (gulong)len);

    deserialize_assert_failure(G_MEMORY_OUTPUT_STREAM(stream), error_list);
    gwy_error_list_clear(&error_list);
    g_object_unref(datastream);
    g_object_unref(stream);
}

void
test_brick_set_size(void)
{
    GwyBrick *brick = gwy_brick_new_sized(13, 11, 8, TRUE);
    guint xres_changed = 0, yres_changed = 0, zres_changed = 0;

    g_signal_connect_swapped(brick, "notify::x-res",
                             G_CALLBACK(record_signal), &xres_changed);
    g_signal_connect_swapped(brick, "notify::y-res",
                             G_CALLBACK(record_signal), &yres_changed);
    g_signal_connect_swapped(brick, "notify::z-res",
                             G_CALLBACK(record_signal), &zres_changed);

    gwy_brick_set_size(brick, 13, 11, 8, TRUE);
    g_assert_cmpuint(brick->xres, ==, 13);
    g_assert_cmpuint(brick->yres, ==, 11);
    g_assert_cmpuint(brick->zres, ==, 8);
    g_assert_cmpuint(xres_changed, ==, 0);
    g_assert_cmpuint(yres_changed, ==, 0);
    g_assert_cmpuint(zres_changed, ==, 0);

    gwy_brick_set_size(brick, 13, 10, 8, TRUE);
    g_assert_cmpuint(brick->xres, ==, 13);
    g_assert_cmpuint(brick->yres, ==, 10);
    g_assert_cmpuint(brick->zres, ==, 8);
    g_assert_cmpuint(xres_changed, ==, 0);
    g_assert_cmpuint(yres_changed, ==, 1);
    g_assert_cmpuint(zres_changed, ==, 0);

    gwy_brick_set_size(brick, 11, 10, 8, TRUE);
    g_assert_cmpuint(brick->xres, ==, 11);
    g_assert_cmpuint(brick->yres, ==, 10);
    g_assert_cmpuint(brick->zres, ==, 8);
    g_assert_cmpuint(xres_changed, ==, 1);
    g_assert_cmpuint(yres_changed, ==, 1);
    g_assert_cmpuint(zres_changed, ==, 0);

    gwy_brick_set_size(brick, 11, 10, 9, TRUE);
    g_assert_cmpuint(brick->xres, ==, 11);
    g_assert_cmpuint(brick->yres, ==, 10);
    g_assert_cmpuint(brick->zres, ==, 9);
    g_assert_cmpuint(xres_changed, ==, 1);
    g_assert_cmpuint(yres_changed, ==, 1);
    g_assert_cmpuint(zres_changed, ==, 1);

    gwy_brick_set_size(brick, 15, 14, 4, TRUE);
    g_assert_cmpuint(brick->xres, ==, 15);
    g_assert_cmpuint(brick->yres, ==, 14);
    g_assert_cmpuint(brick->zres, ==, 4);
    g_assert_cmpuint(xres_changed, ==, 2);
    g_assert_cmpuint(yres_changed, ==, 2);
    g_assert_cmpuint(zres_changed, ==, 2);

    g_object_unref(brick);
}

static void
brick_part_copy_dumb(const GwyBrick *src,
                     guint col,
                     guint row,
                     guint level,
                     guint width,
                     guint height,
                     guint depth,
                     GwyBrick *dest,
                     guint destcol,
                     guint destrow,
                     guint destlevel)
{
    for (guint l = 0; l < depth; l++) {
        if (level + l >= src->zres || destlevel + l >= dest->zres)
            continue;
        for (guint i = 0; i < height; i++) {
            if (row + i >= src->yres || destrow + i >= dest->yres)
                continue;
            for (guint j = 0; j < width; j++) {
                if (col + j >= src->xres || destcol + j >= dest->xres)
                    continue;

                gdouble val = gwy_brick_index(src, col + j, row + i, level + l);
                gwy_brick_index(dest, destcol + j, destrow + i, destlevel + l)
                    = val;
            }
        }
    }
}

void
test_brick_copy(void)
{
    enum { max_size = 19 };
    GRand *rng = g_rand_new();
    g_rand_set_seed(rng, 42);
    gsize niter = g_test_slow() ? 1000 : 200;

    for (gsize iter = 0; iter < niter; iter++) {
        guint sxres = g_rand_int_range(rng, 1, max_size);
        guint syres = g_rand_int_range(rng, 1, max_size/2);
        guint szres = g_rand_int_range(rng, 1, max_size/3);
        guint dxres = g_rand_int_range(rng, 1, max_size);
        guint dyres = g_rand_int_range(rng, 1, max_size/2);
        guint dzres = g_rand_int_range(rng, 1, max_size/3);
        GwyBrick *source = gwy_brick_new_sized(sxres, syres, szres, FALSE);
        GwyBrick *dest = gwy_brick_new_sized(dxres, dyres, dzres, FALSE);
        GwyBrick *reference = gwy_brick_new_sized(dxres, dyres, dzres, FALSE);
        brick_randomize(source, rng);
        brick_randomize(reference, rng);
        gwy_brick_copy(reference, NULL, dest, 0, 0, 0);
        guint width = g_rand_int_range(rng, 0, MAX(sxres, dxres));
        guint height = g_rand_int_range(rng, 0, MAX(syres, dyres));
        guint depth = g_rand_int_range(rng, 0, MAX(szres, dzres));
        guint col = g_rand_int_range(rng, 0, sxres);
        guint row = g_rand_int_range(rng, 0, syres);
        guint level = g_rand_int_range(rng, 0, szres);
        guint destcol = g_rand_int_range(rng, 0, dxres);
        guint destrow = g_rand_int_range(rng, 0, dyres);
        guint destlevel = g_rand_int_range(rng, 0, dzres);
        if (sxres == dxres && syres == dyres
            && g_rand_int_range(rng, 0, 2) == 0) {
            // Check the fast path
            col = destcol = 0;
            row = destrow = 0;
            depth = szres;
        }
        GwyBrickPart bpart = { col, row, level, width, height, depth };
        gwy_brick_copy(source, &bpart, dest, destcol, destrow, destlevel);
        brick_part_copy_dumb(source, col, row, level, width, height, depth,
                             reference, destcol, destrow, destlevel);
        brick_assert_equal(dest, reference);
        g_object_unref(source);
        g_object_unref(dest);
        g_object_unref(reference);
    }

    GwyBrick *brick = gwy_brick_new_sized(17, 4, 12, TRUE);
    brick_randomize(brick, rng);
    GwyBrick *dest = gwy_brick_new_alike(brick, FALSE);
    gwy_brick_copy_full(brick, dest);
    g_assert_cmpint(memcmp(brick->data, dest->data,
                           brick->xres*brick->yres*brick->zres*sizeof(gdouble)),
                    ==, 0);
    g_object_unref(dest);
    g_object_unref(brick);

    g_rand_free(rng);
}

void
test_brick_new_part(void)
{
    enum { max_size = 18 };
    GRand *rng = g_rand_new_with_seed(42);
    gsize niter = g_test_slow() ? 1000 : 200;

    for (gsize iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size/2);
        guint zres = g_rand_int_range(rng, 1, max_size/3);
        GwyBrick *source = gwy_brick_new_sized(xres, yres, zres, FALSE);
        brick_randomize(source, rng);
        guint width = g_rand_int_range(rng, 1, xres+1);
        guint height = g_rand_int_range(rng, 1, yres+1);
        guint depth = g_rand_int_range(rng, 1, zres+1);
        guint col = g_rand_int_range(rng, 0, xres-width+1);
        guint row = g_rand_int_range(rng, 0, yres-height+1);
        guint level = g_rand_int_range(rng, 0, zres-depth+1);
        GwyBrickPart bpart = { col, row, level, width, height, depth };
        GwyBrick *part = gwy_brick_new_part(source, &bpart, TRUE);
        GwyBrick *reference = gwy_brick_new_sized(width, height, depth, FALSE);
        gwy_brick_set_xreal(reference, source->xreal*width/xres);
        gwy_brick_set_yreal(reference, source->yreal*height/yres);
        gwy_brick_set_zreal(reference, source->zreal*depth/zres);
        gwy_brick_set_xoffset(reference, source->xreal*col/xres);
        gwy_brick_set_yoffset(reference, source->yreal*row/yres);
        gwy_brick_set_zoffset(reference, source->zreal*level/zres);
        brick_part_copy_dumb(source, col, row, level, width, height, depth,
                             reference, 0, 0, 0);
        brick_assert_equal(part, reference);
        g_object_unref(source);
        g_object_unref(part);
        g_object_unref(reference);
    }
    g_rand_free(rng);
}

void
test_brick_compatibility_res(void)
{
    GwyBrick *brick1 = gwy_brick_new_sized(2, 3, 4, FALSE);
    GwyBrick *brick2 = gwy_brick_new_sized(2, 2, 4, FALSE);
    GwyBrick *brick3 = gwy_brick_new_sized(3, 2, 4, FALSE);
    GwyBrick *brick4 = gwy_brick_new_sized(3, 2, 1, FALSE);

    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick2,
                                               GWY_BRICK_COMPAT_XRES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick2,
                                               GWY_BRICK_COMPAT_YRES),
                     ==, GWY_BRICK_COMPAT_YRES);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick2,
                                               GWY_BRICK_COMPAT_RES),
                     ==, GWY_BRICK_COMPAT_YRES);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick1,
                                               GWY_BRICK_COMPAT_XRES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick1,
                                               GWY_BRICK_COMPAT_YRES),
                     ==, GWY_BRICK_COMPAT_YRES);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick1,
                                               GWY_BRICK_COMPAT_RES),
                     ==, GWY_BRICK_COMPAT_YRES);

    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick3,
                                               GWY_BRICK_COMPAT_XRES),
                     ==, GWY_BRICK_COMPAT_XRES);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick3,
                                               GWY_BRICK_COMPAT_YRES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick3,
                                               GWY_BRICK_COMPAT_RES),
                     ==, GWY_BRICK_COMPAT_XRES);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick2,
                                               GWY_BRICK_COMPAT_XRES),
                     ==, GWY_BRICK_COMPAT_XRES);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick2,
                                               GWY_BRICK_COMPAT_YRES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick2,
                                               GWY_BRICK_COMPAT_RES),
                     ==, GWY_BRICK_COMPAT_XRES);

    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick3,
                                               GWY_BRICK_COMPAT_XRES),
                     ==, GWY_BRICK_COMPAT_XRES);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick3,
                                               GWY_BRICK_COMPAT_YRES),
                     ==, GWY_BRICK_COMPAT_YRES);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick3,
                                               GWY_BRICK_COMPAT_RES),
                     ==, GWY_BRICK_COMPAT_XRES | GWY_BRICK_COMPAT_YRES);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick1,
                                               GWY_BRICK_COMPAT_XRES),
                     ==, GWY_BRICK_COMPAT_XRES);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick1,
                                               GWY_BRICK_COMPAT_YRES),
                     ==, GWY_BRICK_COMPAT_YRES);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick1,
                                               GWY_BRICK_COMPAT_RES),
                     ==, GWY_BRICK_COMPAT_XRES | GWY_BRICK_COMPAT_YRES);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick3,
                                               GWY_BRICK_COMPAT_ZRES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick1,
                                               GWY_BRICK_COMPAT_ZRES),
                     ==, 0);

    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick4,
                                               GWY_BRICK_COMPAT_ZRES),
                     ==, GWY_BRICK_COMPAT_ZRES);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick4, brick3,
                                               GWY_BRICK_COMPAT_ZRES),
                     ==, GWY_BRICK_COMPAT_ZRES);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick4,
                                               GWY_BRICK_COMPAT_RES),
                     ==, GWY_BRICK_COMPAT_ZRES);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick4, brick3,
                                               GWY_BRICK_COMPAT_RES),
                     ==, GWY_BRICK_COMPAT_ZRES);

    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick2,
                                               GWY_BRICK_COMPAT_DX),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick2,
                                               GWY_BRICK_COMPAT_DY),
                     ==, GWY_BRICK_COMPAT_DY);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick2,
                                               GWY_BRICK_COMPAT_DXDY),
                     ==, GWY_BRICK_COMPAT_DY);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick1,
                                               GWY_BRICK_COMPAT_DX),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick1,
                                               GWY_BRICK_COMPAT_DY),
                     ==, GWY_BRICK_COMPAT_DY);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick1,
                                               GWY_BRICK_COMPAT_DXDY),
                     ==, GWY_BRICK_COMPAT_DY);

    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick3,
                                               GWY_BRICK_COMPAT_DX),
                     ==, GWY_BRICK_COMPAT_DX);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick3,
                                               GWY_BRICK_COMPAT_DY),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick3,
                                               GWY_BRICK_COMPAT_DXDY),
                     ==, GWY_BRICK_COMPAT_DX);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick2,
                                               GWY_BRICK_COMPAT_DX),
                     ==, GWY_BRICK_COMPAT_DX);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick2,
                                               GWY_BRICK_COMPAT_DY),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick2,
                                               GWY_BRICK_COMPAT_DXDY),
                     ==, GWY_BRICK_COMPAT_DX);

    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick3,
                                               GWY_BRICK_COMPAT_DX),
                     ==, GWY_BRICK_COMPAT_DX);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick3,
                                               GWY_BRICK_COMPAT_DY),
                     ==, GWY_BRICK_COMPAT_DY);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick3,
                                               GWY_BRICK_COMPAT_DXDY),
                     ==, GWY_BRICK_COMPAT_DXDY);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick1,
                                               GWY_BRICK_COMPAT_DX),
                     ==, GWY_BRICK_COMPAT_DX);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick1,
                                               GWY_BRICK_COMPAT_DY),
                     ==, GWY_BRICK_COMPAT_DY);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick1,
                                               GWY_BRICK_COMPAT_DXDY),
                     ==, GWY_BRICK_COMPAT_DXDY);

    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick2,
                                               GWY_BRICK_COMPAT_REAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick1,
                                               GWY_BRICK_COMPAT_REAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick3,
                                               GWY_BRICK_COMPAT_REAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick1,
                                               GWY_BRICK_COMPAT_REAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick3,
                                               GWY_BRICK_COMPAT_REAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick2,
                                               GWY_BRICK_COMPAT_REAL),
                     ==, 0);

    g_object_unref(brick1);
    g_object_unref(brick2);
    g_object_unref(brick3);
    g_object_unref(brick4);
}

void
test_brick_compatibility_real(void)
{
    GwyBrick *brick1 = gwy_brick_new_sized(2, 2, 2, FALSE);
    GwyBrick *brick2 = gwy_brick_new_sized(2, 2, 2, FALSE);
    GwyBrick *brick3 = gwy_brick_new_sized(2, 2, 2, FALSE);
    GwyBrick *brick4 = gwy_brick_new_sized(2, 2, 2, FALSE);

    gwy_brick_set_yreal(brick1, 1.5);
    gwy_brick_set_xreal(brick3, 1.5);
    gwy_brick_set_zreal(brick4, 1.5);

    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick2,
                                               GWY_BRICK_COMPAT_XREAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick2,
                                               GWY_BRICK_COMPAT_YREAL),
                     ==, GWY_BRICK_COMPAT_YREAL);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick2,
                                               GWY_BRICK_COMPAT_REAL),
                     ==, GWY_BRICK_COMPAT_YREAL);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick1,
                                               GWY_BRICK_COMPAT_XREAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick1,
                                               GWY_BRICK_COMPAT_YREAL),
                     ==, GWY_BRICK_COMPAT_YREAL);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick1,
                                               GWY_BRICK_COMPAT_REAL),
                     ==, GWY_BRICK_COMPAT_YREAL);

    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick3,
                                               GWY_BRICK_COMPAT_XREAL),
                     ==, GWY_BRICK_COMPAT_XREAL);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick3,
                                               GWY_BRICK_COMPAT_YREAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick3,
                                               GWY_BRICK_COMPAT_REAL),
                     ==, GWY_BRICK_COMPAT_XREAL);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick2,
                                               GWY_BRICK_COMPAT_XREAL),
                     ==, GWY_BRICK_COMPAT_XREAL);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick2,
                                               GWY_BRICK_COMPAT_YREAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick2,
                                               GWY_BRICK_COMPAT_REAL),
                     ==, GWY_BRICK_COMPAT_XREAL);

    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick3,
                                               GWY_BRICK_COMPAT_XREAL),
                     ==, GWY_BRICK_COMPAT_XREAL);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick3,
                                               GWY_BRICK_COMPAT_YREAL),
                     ==, GWY_BRICK_COMPAT_YREAL);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick3,
                                               GWY_BRICK_COMPAT_REAL),
                     ==, GWY_BRICK_COMPAT_XREAL | GWY_BRICK_COMPAT_YREAL);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick1,
                                               GWY_BRICK_COMPAT_XREAL),
                     ==, GWY_BRICK_COMPAT_XREAL);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick1,
                                               GWY_BRICK_COMPAT_YREAL),
                     ==, GWY_BRICK_COMPAT_YREAL);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick1,
                                               GWY_BRICK_COMPAT_REAL),
                     ==, GWY_BRICK_COMPAT_XREAL | GWY_BRICK_COMPAT_YREAL);

    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick2,
                                               GWY_BRICK_COMPAT_DX),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick2,
                                               GWY_BRICK_COMPAT_DY),
                     ==, GWY_BRICK_COMPAT_DY);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick2,
                                               GWY_BRICK_COMPAT_DXDY),
                     ==, GWY_BRICK_COMPAT_DY);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick1,
                                               GWY_BRICK_COMPAT_DX),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick1,
                                               GWY_BRICK_COMPAT_DY),
                     ==, GWY_BRICK_COMPAT_DY);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick1,
                                               GWY_BRICK_COMPAT_DXDY),
                     ==, GWY_BRICK_COMPAT_DY);

    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick3,
                                               GWY_BRICK_COMPAT_DX),
                     ==, GWY_BRICK_COMPAT_DX);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick3,
                                               GWY_BRICK_COMPAT_DY),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick3,
                                               GWY_BRICK_COMPAT_DXDY),
                     ==, GWY_BRICK_COMPAT_DX);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick2,
                                               GWY_BRICK_COMPAT_DX),
                     ==, GWY_BRICK_COMPAT_DX);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick2,
                                               GWY_BRICK_COMPAT_DY),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick2,
                                               GWY_BRICK_COMPAT_DXDY),
                     ==, GWY_BRICK_COMPAT_DX);

    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick3,
                                               GWY_BRICK_COMPAT_DX),
                     ==, GWY_BRICK_COMPAT_DX);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick3,
                                               GWY_BRICK_COMPAT_DY),
                     ==, GWY_BRICK_COMPAT_DY);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick3,
                                               GWY_BRICK_COMPAT_DXDY),
                     ==, GWY_BRICK_COMPAT_DXDY);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick1,
                                               GWY_BRICK_COMPAT_DX),
                     ==, GWY_BRICK_COMPAT_DX);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick1,
                                               GWY_BRICK_COMPAT_DY),
                     ==, GWY_BRICK_COMPAT_DY);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick1,
                                               GWY_BRICK_COMPAT_DXDY),
                     ==, GWY_BRICK_COMPAT_DXDY);

    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick4,
                                               GWY_BRICK_COMPAT_DZ),
                     ==, GWY_BRICK_COMPAT_DZ);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick4,
                                               GWY_BRICK_COMPAT_DXDY),
                     ==, GWY_BRICK_COMPAT_DY);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick4,
                                               GWY_BRICK_COMPAT_DXDYDZ),
                     ==, GWY_BRICK_COMPAT_DY | GWY_BRICK_COMPAT_DZ);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick4, brick1,
                                               GWY_BRICK_COMPAT_DZ),
                     ==, GWY_BRICK_COMPAT_DZ);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick4, brick1,
                                               GWY_BRICK_COMPAT_DXDY),
                     ==, GWY_BRICK_COMPAT_DY);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick4, brick1,
                                               GWY_BRICK_COMPAT_DXDYDZ),
                     ==, GWY_BRICK_COMPAT_DY | GWY_BRICK_COMPAT_DZ);

    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick4,
                                               GWY_BRICK_COMPAT_DZ),
                     ==, GWY_BRICK_COMPAT_DZ);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick4,
                                               GWY_BRICK_COMPAT_DXDY),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick4,
                                               GWY_BRICK_COMPAT_DXDYDZ),
                     ==, GWY_BRICK_COMPAT_DZ);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick4, brick2,
                                               GWY_BRICK_COMPAT_DZ),
                     ==, GWY_BRICK_COMPAT_DZ);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick4, brick2,
                                               GWY_BRICK_COMPAT_DXDY),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick4, brick2,
                                               GWY_BRICK_COMPAT_DXDYDZ),
                     ==, GWY_BRICK_COMPAT_DZ);

    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick2,
                                               GWY_BRICK_COMPAT_RES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick1,
                                               GWY_BRICK_COMPAT_RES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick3,
                                               GWY_BRICK_COMPAT_RES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick1,
                                               GWY_BRICK_COMPAT_RES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick3,
                                               GWY_BRICK_COMPAT_RES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick2,
                                               GWY_BRICK_COMPAT_RES),
                     ==, 0);

    g_object_unref(brick1);
    g_object_unref(brick2);
    g_object_unref(brick3);
    g_object_unref(brick4);
}

void
test_brick_compatibility_units(void)
{
    enum { N = 5 };
    static const GwyBrickCompatFlags incompat[N] = {
        0,
        GWY_BRICK_COMPAT_X,
        GWY_BRICK_COMPAT_Y,
        GWY_BRICK_COMPAT_Z,
        GWY_BRICK_COMPAT_VALUE,
    };
    GwyBrick *bricks[N];
    for (guint i = 0; i < N; i++)
        bricks[i] = gwy_brick_new();

    gwy_unit_set_from_string(gwy_brick_get_xunit(bricks[1]), "m", NULL);
    gwy_unit_set_from_string(gwy_brick_get_yunit(bricks[2]), "m", NULL);
    gwy_unit_set_from_string(gwy_brick_get_zunit(bricks[3]), "m", NULL);
    gwy_unit_set_from_string(gwy_brick_get_wunit(bricks[4]), "m", NULL);

    for (guint tocheck = 0; tocheck <= GWY_BRICK_COMPAT_ALL; tocheck++) {
        for (guint i = 0; i < N; i++) {
            for (guint j = 0; j < N; j++) {
                GwyBrickCompatFlags expected = ((i == j) ? 0
                                                : incompat[i] | incompat[j]);
                g_assert_cmpuint(gwy_brick_is_incompatible(bricks[i], bricks[j],
                                                           tocheck),
                                 ==, expected & tocheck);
            }
        }
    }

    for (guint i = 0; i < N; i++)
        g_object_unref(bricks[i]);
}

void
test_brick_compatibility_field_res(void)
{
    GwyBrick *brick = gwy_brick_new_sized(2, 3, 4, FALSE);
    GwyField *field1 = gwy_field_new_sized(2, 3, FALSE);
    GwyField *field2 = gwy_field_new_sized(3, 2, FALSE);
    GwyField *field3 = gwy_field_new_sized(3, 3, FALSE);
    GwyField *field4 = gwy_field_new_sized(2, 2, FALSE);

    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field1, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_XRES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field1, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_YRES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field1, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_RES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field2, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_XRES),
                     ==, GWY_FIELD_COMPAT_XRES);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field2, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_YRES),
                     ==, GWY_FIELD_COMPAT_YRES);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field2, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_RES),
                     ==, GWY_FIELD_COMPAT_RES);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field3, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_XRES),
                     ==, GWY_FIELD_COMPAT_XRES);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field3, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_YRES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field3, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_RES),
                     ==, GWY_FIELD_COMPAT_XRES);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field4, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_XRES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field4, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_YRES),
                     ==, GWY_FIELD_COMPAT_YRES);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field4, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_RES),
                     ==, GWY_FIELD_COMPAT_YRES);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field1, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_XREAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field1, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_YREAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field1, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_REAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field2, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_XREAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field2, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_YREAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field2, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_REAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field3, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_XREAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field3, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_YREAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field3, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_REAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field4, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_XREAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field4, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_YREAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field4, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_REAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field1, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_DX),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field1, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_DY),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field1, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_DXDY),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field2, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_DX),
                     ==, GWY_FIELD_COMPAT_DX);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field2, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_DY),
                     ==, GWY_FIELD_COMPAT_DY);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field2, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_DXDY),
                     ==, GWY_FIELD_COMPAT_DXDY);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field3, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_DX),
                     ==, GWY_FIELD_COMPAT_DX);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field3, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_DY),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field3, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_DXDY),
                     ==, GWY_FIELD_COMPAT_DX);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field4, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_DX),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field4, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_DY),
                     ==, GWY_FIELD_COMPAT_DY);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field4, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_DXDY),
                     ==, GWY_FIELD_COMPAT_DY);

    g_object_unref(field1);
    g_object_unref(field2);
    g_object_unref(field3);
    g_object_unref(field4);
    g_object_unref(brick);
}

void
test_brick_compatibility_field_real(void)
{
    GwyBrick *brick = gwy_brick_new_sized(2, 2, 4, FALSE);
    GwyField *field1 = gwy_field_new_sized(2, 2, FALSE);
    GwyField *field2 = gwy_field_new_sized(2, 2, FALSE);
    GwyField *field3 = gwy_field_new_sized(2, 2, FALSE);
    GwyField *field4 = gwy_field_new_sized(2, 2, FALSE);

    gwy_field_set_xreal(field2, 2.0);
    gwy_field_set_yreal(field2, 2.0);
    gwy_field_set_xreal(field3, 2.0);
    gwy_field_set_yreal(field4, 2.0);

    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field1, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_XRES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field1, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_YRES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field1, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_RES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field2, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_XRES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field2, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_YRES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field2, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_RES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field3, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_XRES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field3, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_YRES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field3, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_RES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field4, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_XRES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field4, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_YRES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field4, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_RES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field1, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_XREAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field1, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_YREAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field1, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_REAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field2, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_XREAL),
                     ==, GWY_FIELD_COMPAT_XREAL);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field2, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_YREAL),
                     ==, GWY_FIELD_COMPAT_YREAL);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field2, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_REAL),
                     ==, GWY_FIELD_COMPAT_REAL);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field3, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_XREAL),
                     ==, GWY_FIELD_COMPAT_XREAL);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field3, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_YREAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field3, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_REAL),
                     ==, GWY_FIELD_COMPAT_XREAL);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field4, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_XREAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field4, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_YREAL),
                     ==, GWY_FIELD_COMPAT_YREAL);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field4, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_REAL),
                     ==, GWY_FIELD_COMPAT_YREAL);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field1, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_DX),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field1, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_DY),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field1, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_DXDY),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field2, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_DX),
                     ==, GWY_FIELD_COMPAT_DX);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field2, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_DY),
                     ==, GWY_FIELD_COMPAT_DY);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field2, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_DXDY),
                     ==, GWY_FIELD_COMPAT_DXDY);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field3, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_DX),
                     ==, GWY_FIELD_COMPAT_DX);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field3, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_DY),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field3, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_DXDY),
                     ==, GWY_FIELD_COMPAT_DX);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field4, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_DX),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field4, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_DY),
                     ==, GWY_FIELD_COMPAT_DY);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field4, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_DXDY),
                     ==, GWY_FIELD_COMPAT_DY);

    g_object_unref(field1);
    g_object_unref(field2);
    g_object_unref(field3);
    g_object_unref(field4);
    g_object_unref(brick);
}

void
test_brick_compatibility_field_units(void)
{
    GwyBrick *brick = gwy_brick_new_sized(2, 2, 2, FALSE);
    GwyField *field1 = gwy_field_new_sized(2, 2, FALSE);
    GwyField *field2 = gwy_field_new_sized(2, 2, FALSE);
    GwyField *field3 = gwy_field_new_sized(2, 2, FALSE);
    GwyField *field4 = gwy_field_new_sized(2, 2, FALSE);

    gwy_unit_set_from_string(gwy_brick_get_xunit(brick), "m", NULL);
    gwy_unit_set_from_string(gwy_brick_get_yunit(brick), "N", NULL);
    gwy_unit_set_from_string(gwy_brick_get_zunit(brick), "s", NULL);
    gwy_unit_set_from_string(gwy_brick_get_wunit(brick), "A", NULL);
    gwy_unit_set_from_string(gwy_field_get_xunit(field1), "m", NULL);
    gwy_unit_set_from_string(gwy_field_get_yunit(field1), "N", NULL);
    gwy_unit_set_from_string(gwy_field_get_zunit(field1), "A", NULL);
    gwy_unit_set_from_string(gwy_field_get_xunit(field3), "m", NULL);
    gwy_unit_set_from_string(gwy_field_get_yunit(field3), "N", NULL);
    gwy_unit_set_from_string(gwy_field_get_zunit(field4), "A", NULL);

    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field1, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_LATERAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field1, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_VALUE),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field1, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_UNITS),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field2, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_LATERAL),
                     ==, GWY_FIELD_COMPAT_LATERAL);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field2, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_VALUE),
                     ==, GWY_FIELD_COMPAT_VALUE);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field2, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_UNITS),
                     ==, GWY_FIELD_COMPAT_UNITS);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field3, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_LATERAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field3, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_VALUE),
                     ==, GWY_FIELD_COMPAT_VALUE);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field3, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_UNITS),
                     ==, GWY_FIELD_COMPAT_VALUE);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field4, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_LATERAL),
                     ==, GWY_FIELD_COMPAT_LATERAL);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field4, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_VALUE),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field4, GWY_DIMEN_X, GWY_DIMEN_Y,
                            GWY_FIELD_COMPAT_UNITS),
                     ==, GWY_FIELD_COMPAT_LATERAL);

    g_object_unref(field1);
    g_object_unref(field2);
    g_object_unref(field3);
    g_object_unref(field4);
    g_object_unref(brick);
}

void
test_brick_compatibility_line_res(void)
{
    GwyBrick *brick = gwy_brick_new_sized(2, 3, 4, FALSE);
    GwyLine *line1 = gwy_line_new_sized(4, FALSE);
    GwyLine *line2 = gwy_line_new_sized(5, FALSE);

    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                           (brick, line1, GWY_DIMEN_Z,
                            GWY_LINE_COMPAT_RES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                           (brick, line2, GWY_DIMEN_Z,
                            GWY_LINE_COMPAT_RES),
                     ==, GWY_LINE_COMPAT_RES);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                           (brick, line1, GWY_DIMEN_Z,
                            GWY_LINE_COMPAT_REAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                           (brick, line2, GWY_DIMEN_Z,
                            GWY_LINE_COMPAT_REAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                           (brick, line1, GWY_DIMEN_Z,
                            GWY_LINE_COMPAT_DX),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                           (brick, line2, GWY_DIMEN_Z,
                            GWY_LINE_COMPAT_DX),
                     ==, GWY_LINE_COMPAT_DX);

    g_object_unref(line1);
    g_object_unref(line2);
    g_object_unref(brick);
}

void
test_brick_compatibility_line_real(void)
{
    GwyBrick *brick = gwy_brick_new_sized(2, 3, 4, FALSE);
    GwyLine *line1 = gwy_line_new_sized(4, FALSE);
    GwyLine *line2 = gwy_line_new_sized(4, FALSE);

    gwy_line_set_real(line2, 2.0);

    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                           (brick, line1, GWY_DIMEN_Z,
                            GWY_LINE_COMPAT_RES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                           (brick, line2, GWY_DIMEN_Z,
                            GWY_LINE_COMPAT_RES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                           (brick, line1, GWY_DIMEN_Z,
                            GWY_LINE_COMPAT_REAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                           (brick, line2, GWY_DIMEN_Z,
                            GWY_LINE_COMPAT_REAL),
                     ==, GWY_LINE_COMPAT_REAL);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                           (brick, line1, GWY_DIMEN_Z,
                            GWY_LINE_COMPAT_DX),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                           (brick, line2, GWY_DIMEN_Z,
                            GWY_LINE_COMPAT_DX),
                     ==, GWY_LINE_COMPAT_DX);

    g_object_unref(line1);
    g_object_unref(line2);
    g_object_unref(brick);
}

void
test_brick_compatibility_line_units(void)
{
    GwyBrick *brick = gwy_brick_new_sized(2, 3, 4, FALSE);
    GwyLine *line1 = gwy_line_new_sized(4, FALSE);
    GwyLine *line2 = gwy_line_new_sized(4, FALSE);
    GwyLine *line3 = gwy_line_new_sized(4, FALSE);
    GwyLine *line4 = gwy_line_new_sized(4, FALSE);

    gwy_unit_set_from_string(gwy_brick_get_xunit(brick), "m", NULL);
    gwy_unit_set_from_string(gwy_brick_get_yunit(brick), "m", NULL);
    gwy_unit_set_from_string(gwy_brick_get_zunit(brick), "s", NULL);
    gwy_unit_set_from_string(gwy_brick_get_wunit(brick), "A", NULL);
    gwy_unit_set_from_string(gwy_line_get_xunit(line1), "s", NULL);
    gwy_unit_set_from_string(gwy_line_get_yunit(line1), "A", NULL);
    gwy_unit_set_from_string(gwy_line_get_xunit(line3), "s", NULL);
    gwy_unit_set_from_string(gwy_line_get_yunit(line4), "A", NULL);

    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                           (brick, line1, GWY_DIMEN_Z,
                            GWY_LINE_COMPAT_LATERAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                           (brick, line1, GWY_DIMEN_Z,
                            GWY_LINE_COMPAT_VALUE),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                           (brick, line1, GWY_DIMEN_Z,
                            GWY_LINE_COMPAT_UNITS),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                           (brick, line2, GWY_DIMEN_Z,
                            GWY_LINE_COMPAT_LATERAL),
                     ==, GWY_LINE_COMPAT_LATERAL);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                           (brick, line2, GWY_DIMEN_Z,
                            GWY_LINE_COMPAT_VALUE),
                     ==, GWY_LINE_COMPAT_VALUE);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                           (brick, line2, GWY_DIMEN_Z,
                            GWY_LINE_COMPAT_UNITS),
                     ==, GWY_LINE_COMPAT_UNITS);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                           (brick, line3, GWY_DIMEN_Z,
                            GWY_LINE_COMPAT_LATERAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                           (brick, line3, GWY_DIMEN_Z,
                            GWY_LINE_COMPAT_VALUE),
                     ==, GWY_LINE_COMPAT_VALUE);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                           (brick, line3, GWY_DIMEN_Z,
                            GWY_LINE_COMPAT_UNITS),
                     ==, GWY_LINE_COMPAT_VALUE);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                           (brick, line4, GWY_DIMEN_Z,
                            GWY_LINE_COMPAT_LATERAL),
                     ==, GWY_LINE_COMPAT_LATERAL);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                           (brick, line4, GWY_DIMEN_Z,
                            GWY_LINE_COMPAT_VALUE),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                           (brick, line4, GWY_DIMEN_Z,
                            GWY_LINE_COMPAT_UNITS),
                     ==, GWY_LINE_COMPAT_LATERAL);

    g_object_unref(line1);
    g_object_unref(line2);
    g_object_unref(line3);
    g_object_unref(line4);
    g_object_unref(brick);
}

// FIXME: This appears to be exactly as error prone as the library functions it
// should be testing.
static void
summarize_lines_one(GwyBrickLineSummary quantity,
                    GwyDimenType coldim, GwyDimenType rowdim,
                    gdouble (*line_stat_function)(const GwyLine *line))
{
    enum { max_size = 19 };
    GRand *rng = g_rand_new();
    g_rand_set_seed(rng, 42);
    gsize niter = g_test_slow() ? 100 : 30;

    for (gsize iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size/2);
        guint zres = g_rand_int_range(rng, 2, max_size/2);
        GwyBrick *brick = gwy_brick_new_sized(xres, yres, zres, FALSE);
        brick_randomize(brick, rng);
        guint width = g_rand_int_range(rng, 1, xres+1);
        guint height = g_rand_int_range(rng, 1, yres+1);
        guint depth = g_rand_int_range(rng, 2, zres+1);
        guint col = g_rand_int_range(rng, 0, xres-width+1);
        guint row = g_rand_int_range(rng, 0, yres-height+1);
        guint level = g_rand_int_range(rng, 0, zres-depth+1);
        GwyBrickPart bpart = { col, row, level, width, height, depth };
        guint fxres, fyres, fwidth, fheight, lres, loff, fcol, frow;
        if (coldim == GWY_DIMEN_X && rowdim == GWY_DIMEN_Y) {
            fxres = xres;
            fyres = yres;
            fwidth = width;
            fheight = height;
            lres = depth;
            loff = level;
            fcol = col;
            frow = row;
        }
        else if (coldim == GWY_DIMEN_Y && rowdim == GWY_DIMEN_X) {
            fxres = yres;
            fyres = xres;
            fwidth = height;
            fheight = width;
            lres = depth;
            loff = level;
            fcol = row;
            frow = col;
        }
        else if (coldim == GWY_DIMEN_X && rowdim == GWY_DIMEN_Z) {
            fxres = xres;
            fyres = zres;
            fwidth = width;
            fheight = depth;
            lres = height;
            loff = row;
            fcol = col;
            frow = level;
        }
        else if (coldim == GWY_DIMEN_Z && rowdim == GWY_DIMEN_X) {
            fxres = zres;
            fyres = xres;
            fwidth = depth;
            fheight = width;
            lres = height;
            loff = row;
            fcol = level;
            frow = col;
        }
        else if (coldim == GWY_DIMEN_Y && rowdim == GWY_DIMEN_Z) {
            fxres = yres;
            fyres = zres;
            fwidth = height;
            fheight = depth;
            lres = width;
            loff = col;
            fcol = row;
            frow = level;
        }
        else if (coldim == GWY_DIMEN_Z && rowdim == GWY_DIMEN_Y) {
            fxres = zres;
            fyres = yres;
            fwidth = depth;
            fheight = height;
            lres = width;
            loff = col;
            fcol = level;
            frow = row;
        }
        else {
            g_assert_not_reached();
        }
        GwyField *fullfield = gwy_field_new_sized(fwidth, fheight, FALSE);
        GwyField *partfield = gwy_field_new_sized(fxres, fyres, TRUE);
        GwyField *linefield = gwy_field_new_sized(fwidth, fheight, FALSE);
        gdouble eps = (quantity == GWY_BRICK_LINE_RMS) ? 1e-13 : 1e-15;

        gwy_brick_summarize_lines(brick, &bpart, fullfield,
                                  coldim, rowdim, quantity);
        gwy_brick_summarize_lines(brick, &bpart, partfield,
                                  coldim, rowdim, quantity);

        GwyLine *line = gwy_line_new_sized(lres, FALSE);
        GwyLinePart lpart = { loff, lres };
        for (guint i = 0; i < fheight; i++) {
            for (guint j = 0; j < fwidth; j++) {
                gwy_brick_extract_line(brick, line, &lpart,
                                       coldim, rowdim, j + fcol, i + frow,
                                       FALSE);
                linefield->data[i*fwidth + j] = line_stat_function(line);
            }
        }

        for (guint i = 0; i < fyres; i++) {
            for (guint j = 0; j < fxres; j++) {
                if (i < frow || i >= frow + fheight
                    || j < fcol || j > fcol + fwidth)
                    g_assert_cmpfloat(partfield->data[i*fxres + j], ==, 0.0);
            }
        }

        for (guint i = 0; i < fheight; i++) {
            for (guint j = 0; j < fwidth; j++) {
                gdouble vfull = fullfield->data[i*fwidth + j],
                        vpart = partfield->data[(i + frow)*fxres + (j + fcol)],
                        vline = linefield->data[i*fwidth + j];
                g_assert_cmpfloat(vpart, ==, vfull);
                gwy_assert_floatval(vfull, vline, eps);
            }
        }

        g_object_unref(line);
        g_object_unref(brick);
        g_object_unref(linefield);
        g_object_unref(partfield);
        g_object_unref(fullfield);
    }
    g_rand_free(rng);
}

static gdouble
line_min(const GwyLine *line)
{
    gdouble min, max;
    gwy_line_min_max_full(line, &min, &max);
    return min;
}

static gdouble
line_max(const GwyLine *line)
{
    gdouble min, max;
    gwy_line_min_max_full(line, &min, &max);
    return max;
}

static gdouble
line_range(const GwyLine *line)
{
    gdouble min, max;
    gwy_line_min_max_full(line, &min, &max);
    return max - min;
}

void
test_brick_summarize_lines_minimum(void)
{
    for (GwyDimenType coldim = GWY_DIMEN_X; coldim <= GWY_DIMEN_Z; coldim++) {
        for (GwyDimenType rowdim = GWY_DIMEN_X; rowdim <= GWY_DIMEN_Z; rowdim++) {
            if (rowdim == coldim)
                continue;
            summarize_lines_one(GWY_BRICK_LINE_MINIMUM,
                                coldim, rowdim, &line_min);
        }
    }
}

void
test_brick_summarize_lines_maximum(void)
{
    for (GwyDimenType coldim = GWY_DIMEN_X; coldim <= GWY_DIMEN_Z; coldim++) {
        for (GwyDimenType rowdim = GWY_DIMEN_X; rowdim <= GWY_DIMEN_Z; rowdim++) {
            if (rowdim == coldim)
                continue;
            summarize_lines_one(GWY_BRICK_LINE_MAXIMUM,
                                coldim, rowdim, &line_max);
        }
    }
}

void
test_brick_summarize_lines_range(void)
{
    for (GwyDimenType coldim = GWY_DIMEN_X; coldim <= GWY_DIMEN_Z; coldim++) {
        for (GwyDimenType rowdim = GWY_DIMEN_X; rowdim <= GWY_DIMEN_Z; rowdim++) {
            if (rowdim == coldim)
                continue;
            summarize_lines_one(GWY_BRICK_LINE_RANGE,
                                coldim, rowdim, &line_range);
        }
    }
}

void
test_brick_summarize_lines_mean(void)
{
    for (GwyDimenType coldim = GWY_DIMEN_X; coldim <= GWY_DIMEN_Z; coldim++) {
        for (GwyDimenType rowdim = GWY_DIMEN_X; rowdim <= GWY_DIMEN_Z; rowdim++) {
            if (rowdim == coldim)
                continue;
            summarize_lines_one(GWY_BRICK_LINE_MEAN,
                                coldim, rowdim, &gwy_line_mean_full);
        }
    }
}

void
test_brick_summarize_lines_rms(void)
{
    for (GwyDimenType coldim = GWY_DIMEN_X; coldim <= GWY_DIMEN_Z; coldim++) {
        for (GwyDimenType rowdim = GWY_DIMEN_X; rowdim <= GWY_DIMEN_Z; rowdim++) {
            if (rowdim == coldim)
                continue;
            summarize_lines_one(GWY_BRICK_LINE_RMS,
                                coldim, rowdim, &gwy_line_rms_full);
        }
    }
}

static void
extract_col_line(const GwyBrick *brick,
                 GwyLine *target,
                 guint col, guint row, guint level,
                 guint len)
{
    g_assert(target->res == len || col + len <= target->res);
    guint linepos = (target->res == len ? 0 : col);

    for (guint i = 0; i < len; i++)
        gwy_line_set(target,
                     linepos + i,
                     gwy_brick_get(brick, col + i, row, level));

    gwy_line_set_real(target, gwy_brick_dx(brick)*target->res);
    gwy_unit_assign(gwy_line_get_xunit(target), gwy_brick_get_xunit(brick));
    gwy_unit_assign(gwy_line_get_yunit(target), gwy_brick_get_wunit(brick));
}

static void
extract_row_line(const GwyBrick *brick,
                 GwyLine *target,
                 guint col, guint row, guint level,
                 guint len)
{
    g_assert(target->res == len || row + len <= target->res);
    guint linepos = (target->res == len ? 0 : row);

    for (guint i = 0; i < len; i++)
        gwy_line_set(target,
                     linepos + i,
                     gwy_brick_get(brick, col, row + i, level));

    gwy_line_set_real(target, gwy_brick_dy(brick)*target->res);
    gwy_unit_assign(gwy_line_get_xunit(target), gwy_brick_get_yunit(brick));
    gwy_unit_assign(gwy_line_get_yunit(target), gwy_brick_get_wunit(brick));
}

static void
extract_level_line(const GwyBrick *brick,
                   GwyLine *target,
                   guint col, guint row, guint level,
                   guint len)
{
    g_assert(target->res == len || level + len <= target->res);
    guint linepos = (target->res == len ? 0 : level);

    for (guint i = 0; i < len; i++)
        gwy_line_set(target,
                     linepos + i,
                     gwy_brick_get(brick, col, row, level + i));

    gwy_line_set_real(target, gwy_brick_dz(brick)*target->res);
    gwy_unit_assign(gwy_line_get_xunit(target), gwy_brick_get_zunit(brick));
    gwy_unit_assign(gwy_line_get_yunit(target), gwy_brick_get_wunit(brick));
}

static void
choose_line_sizes(GwyLine *line1, GwyLine *line2,
                  guint len, guint res,
                  GRand *rng)
{
    if (g_rand_boolean(rng)) {
        gwy_line_set_size(line1, len, FALSE);
        gwy_line_set_size(line2, len, FALSE);
    }
    else {
        gwy_line_set_size(line1, res, TRUE);
        gwy_line_set_size(line2, res, TRUE);
    }
}

void
test_brick_extract_line(void)
{
    enum { max_size = 13, niter = 500 };
    GRand *rng = g_rand_new();
    g_rand_set_seed(rng, 114);

    for (gsize iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size);
        guint zres = g_rand_int_range(rng, 1, max_size);
        GwyBrick *brick = gwy_brick_new_sized(xres, yres, zres, FALSE);
        brick_randomize(brick, rng);
        guint col = g_rand_int_range(rng, 0, xres);
        guint row = g_rand_int_range(rng, 0, yres);
        guint level = g_rand_int_range(rng, 0, zres);
        guint width = g_rand_int_range(rng, 1, xres+1 - col);
        guint height = g_rand_int_range(rng, 1, yres+1 - row);
        guint depth = g_rand_int_range(rng, 1, zres+1 - level);
        GwyLine *target = gwy_line_new(), *reference = gwy_line_new();

        /* Column-wise extraction */
        choose_line_sizes(target, reference, width, xres, rng);
        extract_col_line(brick, reference, col, row, level, width);
        gwy_brick_extract_line(brick, target,
                               &(GwyLinePart){ col, width },
                               GWY_DIMEN_Y, GWY_DIMEN_Z,
                               row, level,
                               FALSE);
        line_assert_equal(target, reference);
        gwy_brick_extract_line(brick, target,
                               &(GwyLinePart){ col, width },
                               GWY_DIMEN_Z, GWY_DIMEN_Y,
                               level, row,
                               FALSE);
        line_assert_equal(target, reference);

        /* Row-wise extraction */
        choose_line_sizes(target, reference, height, yres, rng);
        extract_row_line(brick, reference, col, row, level, height);
        gwy_brick_extract_line(brick, target,
                               &(GwyLinePart){ row, height },
                               GWY_DIMEN_X, GWY_DIMEN_Z,
                               col, level,
                               FALSE);
        line_assert_equal(target, reference);
        gwy_brick_extract_line(brick, target,
                               &(GwyLinePart){ row, height },
                               GWY_DIMEN_Z, GWY_DIMEN_X,
                               level, col,
                               FALSE);
        line_assert_equal(target, reference);

        /* Level-wise extraction */
        choose_line_sizes(target, reference, depth, zres, rng);
        extract_level_line(brick, reference, col, row, level, depth);
        gwy_brick_extract_line(brick, target,
                               &(GwyLinePart){ level, depth },
                               GWY_DIMEN_X, GWY_DIMEN_Y,
                               col, row,
                               FALSE);
        line_assert_equal(target, reference);
        gwy_brick_extract_line(brick, target,
                               &(GwyLinePart){ level, depth },
                               GWY_DIMEN_Y, GWY_DIMEN_X,
                               row, col,
                               FALSE);
        line_assert_equal(target, reference);

        g_object_unref(brick);
        g_object_unref(target);
        g_object_unref(reference);
    }

    g_rand_free(rng);
}

static void
extract_col_row_field(const GwyBrick *brick,
                      GwyField *target,
                      guint col, guint row, guint level,
                      guint width, guint height)
{
    g_assert(target->xres == width || col + width <= target->xres);
    g_assert(target->yres == height || row + height <= target->yres);
    guint fieldcol = (target->xres == width ? 0 : col);
    guint fieldrow = (target->yres == height ? 0 : row);

    for (guint i = 0; i < height; i++) {
        for (guint j = 0; j < width; j++)
            gwy_field_set(target,
                          fieldcol + j, fieldrow + i,
                          gwy_brick_get(brick, col + j, row + i, level));
    }

    gwy_field_set_xreal(target, gwy_brick_dx(brick)*target->xres);
    gwy_field_set_yreal(target, gwy_brick_dy(brick)*target->yres);
    gwy_unit_assign(gwy_field_get_xunit(target), gwy_brick_get_xunit(brick));
    gwy_unit_assign(gwy_field_get_yunit(target), gwy_brick_get_yunit(brick));
    gwy_unit_assign(gwy_field_get_zunit(target), gwy_brick_get_wunit(brick));
}

static void
extract_col_level_field(const GwyBrick *brick,
                        GwyField *target,
                        guint col, guint row, guint level,
                        guint width, guint height)
{
    g_assert(target->xres == width || col + width <= target->xres);
    g_assert(target->yres == height || level + height <= target->yres);
    guint fieldcol = (target->xres == width ? 0 : col);
    guint fieldrow = (target->yres == height ? 0 : level);

    for (guint i = 0; i < height; i++) {
        for (guint j = 0; j < width; j++)
            gwy_field_set(target,
                          fieldcol + j, fieldrow + i,
                          gwy_brick_get(brick, col + j, row, level + i));
    }

    gwy_field_set_xreal(target, gwy_brick_dx(brick)*target->xres);
    gwy_field_set_yreal(target, gwy_brick_dz(brick)*target->yres);
    gwy_unit_assign(gwy_field_get_xunit(target), gwy_brick_get_xunit(brick));
    gwy_unit_assign(gwy_field_get_yunit(target), gwy_brick_get_zunit(brick));
    gwy_unit_assign(gwy_field_get_zunit(target), gwy_brick_get_wunit(brick));
}

static void
extract_row_level_field(const GwyBrick *brick,
                        GwyField *target,
                        guint col, guint row, guint level,
                        guint width, guint height)
{
    g_assert(target->xres == width || row + width <= target->xres);
    g_assert(target->yres == height || level + height <= target->yres);
    guint fieldcol = (target->xres == width ? 0 : row);
    guint fieldrow = (target->yres == height ? 0 : level);

    for (guint i = 0; i < height; i++) {
        for (guint j = 0; j < width; j++)
            gwy_field_set(target,
                          fieldcol + j, fieldrow + i,
                          gwy_brick_get(brick, col, row + j, level + i));
    }

    gwy_field_set_xreal(target, gwy_brick_dy(brick)*target->xres);
    gwy_field_set_yreal(target, gwy_brick_dz(brick)*target->yres);
    gwy_unit_assign(gwy_field_get_xunit(target), gwy_brick_get_yunit(brick));
    gwy_unit_assign(gwy_field_get_yunit(target), gwy_brick_get_zunit(brick));
    gwy_unit_assign(gwy_field_get_zunit(target), gwy_brick_get_wunit(brick));
}

static void
choose_field_sizes(GwyField *field1, GwyField *field2,
                   guint width, guint height, guint xres, guint yres,
                   GRand *rng)
{
    if (g_rand_boolean(rng)) {
        gwy_field_set_size(field1, width, height, FALSE);
        gwy_field_set_size(field2, width, height, FALSE);
    }
    else {
        gwy_field_set_size(field1, xres, yres, TRUE);
        gwy_field_set_size(field2, xres, yres, TRUE);
    }
}

void
test_brick_extract_plane(void)
{
    enum { max_size = 13, niter = 500 };
    GRand *rng = g_rand_new();
    g_rand_set_seed(rng, 114);

    for (gsize iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size);
        guint zres = g_rand_int_range(rng, 1, max_size);
        GwyBrick *brick = gwy_brick_new_sized(xres, yres, zres, FALSE);
        brick_randomize(brick, rng);
        guint col = g_rand_int_range(rng, 0, xres);
        guint row = g_rand_int_range(rng, 0, yres);
        guint level = g_rand_int_range(rng, 0, zres);
        guint width = g_rand_int_range(rng, 1, xres+1 - col);
        guint height = g_rand_int_range(rng, 1, yres+1 - row);
        guint depth = g_rand_int_range(rng, 1, zres+1 - level);
        GwyField *target = gwy_field_new(), *reference = gwy_field_new();

        /* Level plane extraction */
        choose_field_sizes(target, reference, width, height, xres, yres, rng);
        extract_col_row_field(brick, reference, col, row, level, width, height);
        gwy_brick_extract_plane(brick, target,
                                &(GwyFieldPart){ col, row, width, height },
                                GWY_DIMEN_X, GWY_DIMEN_Y,
                                level,
                                FALSE);
        field_assert_equal(target, reference);
        gwy_field_transform_congruent(reference, GWY_PLANE_MIRROR_DIAGONALLY);
        gwy_field_set_size(target, reference->xres, reference->yres, TRUE);
        gwy_brick_extract_plane(brick, target,
                                &(GwyFieldPart){ row, col, height, width },
                                GWY_DIMEN_Y, GWY_DIMEN_X,
                                level,
                                FALSE);
        field_assert_equal(target, reference);

        /* Row plane extraction */
        choose_field_sizes(target, reference, width, depth, xres, zres, rng);
        extract_col_level_field(brick, reference, col, row, level, width, depth);
        gwy_brick_extract_plane(brick, target,
                                &(GwyFieldPart){ col, level, width, depth },
                                GWY_DIMEN_X, GWY_DIMEN_Z,
                                row,
                                FALSE);
        field_assert_equal(target, reference);
        gwy_field_transform_congruent(reference, GWY_PLANE_MIRROR_DIAGONALLY);
        gwy_field_set_size(target, reference->xres, reference->yres, TRUE);
        gwy_brick_extract_plane(brick, target,
                                &(GwyFieldPart){ level, col, depth, width },
                                GWY_DIMEN_Z, GWY_DIMEN_X,
                                row,
                                FALSE);
        field_assert_equal(target, reference);

        /* Column plane extraction */
        choose_field_sizes(target, reference, height, depth, yres, zres, rng);
        extract_row_level_field(brick, reference, col, row, level, height, depth);
        gwy_brick_extract_plane(brick, target,
                                &(GwyFieldPart){ row, level, height, depth },
                                GWY_DIMEN_Y, GWY_DIMEN_Z,
                                col,
                                FALSE);
        field_assert_equal(target, reference);
        gwy_field_transform_congruent(reference, GWY_PLANE_MIRROR_DIAGONALLY);
        gwy_field_set_size(target, reference->xres, reference->yres, TRUE);
        gwy_brick_extract_plane(brick, target,
                                &(GwyFieldPart){ level, row, depth, height },
                                GWY_DIMEN_Z, GWY_DIMEN_Y,
                                col,
                                FALSE);
        field_assert_equal(target, reference);

        g_object_unref(brick);
        g_object_unref(target);
        g_object_unref(reference);
    }

    g_rand_free(rng);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
