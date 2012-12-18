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

#include <stdlib.h>
#include "testlibgwy.h"

/***************************************************************************
 *
 * Line
 *
 ***************************************************************************/

void
line_print(const gchar *name, const GwyLine *line)
{
    g_printerr("%s", name);
    for (guint i = 0; i < line->res; i++)
        g_printerr(" %g", line->data[i]);
    g_printerr("\n");
}

void
line_randomize(GwyLine *line,
               GRand *rng)
{
    gdouble *d = line->data;
    for (guint n = line->res; n; n--, d++)
        *d = g_rand_double(rng);
}

void
line_assert_equal(const GwyLine *result,
                  const GwyLine *reference)
{
    g_assert(GWY_IS_LINE(result));
    g_assert(GWY_IS_LINE(reference));
    g_assert_cmpuint(result->res, ==, reference->res);
    compare_properties(G_OBJECT(result), G_OBJECT(reference));

    for (guint j = 0; j < result->res; j++)
        g_assert_cmpfloat(result->data[j], ==, reference->data[j]);
}

static void
line_assert_equal_object(GObject *object, GObject *reference)
{
    line_assert_equal(GWY_LINE(object), GWY_LINE(reference));
}

void
line_assert_numerically_equal(const GwyLine *result,
                              const GwyLine *reference,
                              gdouble eps)
{
    g_assert(GWY_IS_LINE(result));
    g_assert(GWY_IS_LINE(reference));
    g_assert_cmpuint(result->res, ==, reference->res);

    for (guint j = 0; j < result->res; j++) {
        gdouble value = result->data[j], ref = reference->data[j];
        //g_printerr("[%u] %g %g\n", j, value, ref);
        gwy_assert_floatval(value, ref, eps);
    }
}

void
test_line_props(void)
{
    GwyLine *line = gwy_line_new_sized(41, FALSE);
    guint res_changed = 0,
          real_changed = 0,
          off_changed = 0,
          name_changed = 0;
    g_signal_connect_swapped(line, "notify::res",
                             G_CALLBACK(record_signal), &res_changed);
    g_signal_connect_swapped(line, "notify::real",
                             G_CALLBACK(record_signal), &real_changed);
    g_signal_connect_swapped(line, "notify::offset",
                             G_CALLBACK(record_signal), &off_changed);
    g_signal_connect_swapped(line, "notify::name",
                             G_CALLBACK(record_signal), &name_changed);

    guint res;
    gdouble real, off;
    gchar *name;
    g_object_get(line,
                 "res", &res,
                 "real", &real,
                 "offset", &off,
                 "name", &name,
                 NULL);
    g_assert_cmpuint(res, ==, line->res);
    g_assert_cmpfloat(real, ==, line->real);
    g_assert_cmpfloat(off, ==, line->off);
    g_assert(!name);
    g_assert_cmpuint(res_changed, ==, 0);
    g_assert_cmpuint(real_changed, ==, 0);
    g_assert_cmpuint(off_changed, ==, 0);
    g_assert_cmpuint(name_changed, ==, 0);

    real = 5.0;
    off = -3;
    gwy_line_set_real(line, real);
    g_assert_cmpuint(real_changed, ==, 1);
    gwy_line_set_offset(line, off);
    g_assert_cmpuint(off_changed, ==, 1);
    gwy_line_set_name(line, "First");
    g_assert_cmpuint(name_changed, ==, 1);
    g_assert_cmpuint(res_changed, ==, 0);
    g_assert_cmpfloat(real, ==, line->real);
    g_assert_cmpfloat(off, ==, line->off);
    g_assert_cmpstr(gwy_line_get_name(line), ==, "First");

    // Do it twice to excersise no-change behaviour.
    gwy_line_set_real(line, real);
    gwy_line_set_offset(line, off);
    gwy_line_set_name(line, "First");
    g_assert_cmpuint(res_changed, ==, 0);
    g_assert_cmpuint(real_changed, ==, 1);
    g_assert_cmpuint(off_changed, ==, 1);
    g_assert_cmpuint(name_changed, ==, 1);
    g_assert_cmpfloat(real, ==, line->real);
    g_assert_cmpfloat(off, ==, line->off);
    g_assert_cmpstr(gwy_line_get_name(line), ==, "First");

    real = 7.2;
    off = 1.3;
    g_object_set(line,
                 "real", real,
                 "offset", off,
                 "name", "Second",
                 NULL);
    g_assert_cmpuint(res_changed, ==, 0);
    g_assert_cmpuint(real_changed, ==, 2);
    g_assert_cmpuint(off_changed, ==, 2);
    g_assert_cmpuint(name_changed, ==, 2);
    g_assert_cmpfloat(real, ==, line->real);
    g_assert_cmpfloat(off, ==, line->off);
    g_assert_cmpstr(gwy_line_get_name(line), ==, "Second");

    g_object_unref(line);
}

void
test_line_data_changed(void)
{
    // Plain emission
    GwyLine *line = gwy_line_new();
    guint counter = 0;
    g_signal_connect_swapped(line, "data-changed",
                             G_CALLBACK(record_signal), &counter);
    gwy_line_data_changed(line, NULL);
    g_assert_cmpuint(counter, ==, 1);
    g_object_unref(line);

    // Specified part argument
    line = gwy_line_new_sized(8, FALSE);
    GwyLinePart lpart = { 1, 2 };
    g_signal_connect_swapped(line, "data-changed",
                             G_CALLBACK(line_part_assert_equal), &lpart);
    gwy_line_data_changed(line, &lpart);
    g_object_unref(line);

    // NULL part argument
    line = gwy_line_new_sized(2, FALSE);
    g_signal_connect_swapped(line, "data-changed",
                             G_CALLBACK(line_part_assert_equal), NULL);
    gwy_line_data_changed(line, NULL);
    g_object_unref(line);
}

void
test_line_units(void)
{
    enum { max_size = 411 };
    GRand *rng = g_rand_new_with_seed(42);
    gsize niter = 50;

    for (guint iter = 0; iter < niter; iter++) {
        guint res = g_rand_int_range(rng, 1, max_size);
        GwyLine *line = gwy_line_new_sized(res, FALSE);
        g_object_set(line, "real", -log(1.0 - g_rand_double(rng)), NULL);
        GString *prev = g_string_new(NULL), *next = g_string_new(NULL);
        GwyValueFormat *format = gwy_line_format_x(line,
                                                   GWY_VALUE_FORMAT_PLAIN);
        gdouble dx = gwy_line_dx(line);
        for (gint i = 0; i <= 10; i++) {
            GWY_SWAP(GString*, prev, next);
            g_string_assign(next,
                            gwy_value_format_print_number(format, (i - 5)*dx));
            if (i)
                g_assert_cmpstr(prev->str, !=, next->str);
        }
        g_object_unref(format);
        g_object_unref(line);
        g_string_free(prev, TRUE);
        g_string_free(next, TRUE);
    }
    g_rand_free(rng);
}

void
test_line_units_assign(void)
{
    GwyLine *line = gwy_line_new(), *line2 = gwy_line_new();
    GwyUnit *unit_x = gwy_line_get_unit_x(line);
    GwyUnit *unit_y = gwy_line_get_unit_y(line);
    guint count_x = 0;
    guint count_y = 0;

    g_signal_connect_swapped(unit_x, "changed",
                             G_CALLBACK(record_signal), &count_x);
    g_signal_connect_swapped(unit_y, "changed",
                             G_CALLBACK(record_signal), &count_y);

    gwy_unit_set_from_string(gwy_line_get_unit_x(line), "m", NULL);
    g_assert_cmpuint(count_x, ==, 1);
    g_assert_cmpuint(count_y, ==, 0);

    gwy_unit_set_from_string(gwy_line_get_unit_y(line), "s", NULL);
    g_assert_cmpuint(count_x, ==, 1);
    g_assert_cmpuint(count_y, ==, 1);

    gwy_line_assign(line, line2);
    g_assert_cmpuint(count_x, ==, 2);
    g_assert_cmpuint(count_y, ==, 2);
    g_assert(gwy_line_get_unit_x(line) == unit_x);
    g_assert(gwy_line_get_unit_y(line) == unit_y);

    // Try again to see if the signal counts change.
    gwy_line_assign(line, line2);
    g_assert_cmpuint(count_x, ==, 2);
    g_assert_cmpuint(count_y, ==, 2);
    g_assert(gwy_line_get_unit_x(line) == unit_x);
    g_assert(gwy_line_get_unit_y(line) == unit_y);

    g_object_unref(line2);
    g_object_unref(line);
}

void
test_line_serialize(void)
{
    enum { max_size = 55 };
    GRand *rng = g_rand_new_with_seed(42);
    gsize niter = g_test_slow() ? 50 : 20;

    for (guint iter = 0; iter < niter; iter++) {
        guint width = g_rand_int_range(rng, 1, max_size);
        GwyLine *original = gwy_line_new_sized(width, FALSE);
        line_randomize(original, rng);
        GwyLine *copy;

        if (g_rand_int(rng) % 5)
            unit_randomize(gwy_line_get_unit_x(original), rng);
        if (g_rand_int(rng) % 5)
            unit_randomize(gwy_line_get_unit_y(original), rng);

        serializable_duplicate(GWY_SERIALIZABLE(original),
                               line_assert_equal_object);
        serializable_assign(GWY_SERIALIZABLE(original),
                            line_assert_equal_object);
        copy = GWY_LINE(serialize_and_back(G_OBJECT(original),
                                           line_assert_equal_object));
        g_object_unref(copy);

        g_object_unref(original);
    }
    g_rand_free(rng);
}

void
test_line_serialize_failure_nodata(void)
{
    GOutputStream *stream = g_memory_output_stream_new(NULL, 0,
                                                       g_realloc, g_free);
    GDataOutputStream *datastream = g_data_output_stream_new(stream);
    g_data_output_stream_set_byte_order(datastream,
                                        G_DATA_STREAM_BYTE_ORDER_LITTLE_ENDIAN);

    data_stream_put_string0(datastream, "GwyLine", NULL, NULL);
    g_data_output_stream_put_uint64(datastream, 0, NULL, NULL);
    data_stream_put_string0(datastream, "real", NULL, NULL);
    g_data_output_stream_put_byte(datastream, GWY_SERIALIZABLE_DOUBLE,
                                  NULL, NULL);
    data_stream_put_double(datastream, 1.0, NULL, NULL);

    GwyErrorList *error_list = NULL;
    gwy_error_list_add(&error_list,
                       GWY_DESERIALIZE_ERROR, GWY_DESERIALIZE_ERROR_INVALID,
                       "Dimension %u of ‘GwyLine’ is invalid.", 0);

    deserialize_assert_failure(G_MEMORY_OUTPUT_STREAM(stream), error_list);
    gwy_error_list_clear(&error_list);
    g_object_unref(datastream);
    g_object_unref(stream);
}

void
record_signal(guint *counter)
{
    (*counter)++;
}

void
test_line_set_size(void)
{
    GwyLine *line = gwy_line_new_sized(13, TRUE);
    guint res_changed = 0;

    g_signal_connect_swapped(line, "notify::res",
                             G_CALLBACK(record_signal), &res_changed);

    gwy_line_set_size(line, 13, TRUE);
    g_assert_cmpuint(line->res, ==, 13);
    g_assert_cmpuint(res_changed, ==, 0);

    gwy_line_set_size(line, 10, TRUE);
    g_assert_cmpuint(line->res, ==, 10);
    g_assert_cmpuint(res_changed, ==, 1);

    g_object_unref(line);
}

static void
line_check_part_good(guint res,
                     const GwyLinePart *lpart,
                     guint expected_pos, guint expected_len)
{
    GwyLine *line = gwy_line_new_sized(res, FALSE);
    guint pos, len;

    g_assert(gwy_line_check_part(line, lpart, &pos, &len));
    g_assert_cmpuint(pos, ==, expected_pos);
    g_assert_cmpuint(len, ==, expected_len);
    g_object_unref(line);
}

void
test_line_check_part_good(void)
{
    line_check_part_good(25, &(GwyLinePart){ 0, 25 }, 0, 25);
    line_check_part_good(25, NULL, 0, 25);
    line_check_part_good(25, &(GwyLinePart){ 0, 24 }, 0, 24);
    line_check_part_good(25, &(GwyLinePart){ 20, 4 }, 20, 4);
}

static void
line_check_part_empty(guint res,
                      const GwyLinePart *lpart)
{
    GwyLine *line = gwy_line_new_sized(res, FALSE);
    guint pos, len;

    g_assert(!gwy_line_check_part(line, lpart, &pos, &len));
    g_object_unref(line);
}

void
test_line_check_part_empty(void)
{
    line_check_part_empty(25, &(GwyLinePart){ 0, 0 });
    line_check_part_empty(25, &(GwyLinePart){ 25, 0 });
    line_check_part_empty(25, &(GwyLinePart){ 1000, 0 });
}

static void
line_check_part_bad(guint res,
                    const GwyLinePart *lpart)
{
    if (g_test_trap_fork(0,
                         G_TEST_TRAP_SILENCE_STDOUT
                         | G_TEST_TRAP_SILENCE_STDERR)) {
        GwyLine *line = gwy_line_new_sized(res, FALSE);
        guint pos, len;
        gwy_line_check_part(line, lpart, &pos, &len);
        exit(0);
    }
    g_test_trap_assert_failed();
    g_test_trap_assert_stderr("*CRITICAL*");
}

void
test_line_check_part_bad(void)
{
    line_check_part_bad(25, &(GwyLinePart){ 0, 26 });
    line_check_part_bad(25, &(GwyLinePart){ 25, 1 });
}

void
test_line_get(void)
{
    enum { max_size = 255, niter = 40 };

    GRand *rng = g_rand_new_with_seed(42);

    for (guint iter = 0; iter < niter; iter++) {
        guint res = g_rand_int_range(rng, 1, max_size);
        GwyLine *line = gwy_line_new_sized(res, FALSE);
        line_randomize(line, rng);

        for (guint i = 0; i < res; i++) {
            g_assert_cmpfloat(line->data[i], ==, gwy_line_get(line, i));
        }
        g_object_unref(line);
    }

    g_rand_free(rng);
}

void
test_line_set(void)
{
    enum { max_size = 255, niter = 40 };

    GRand *rng = g_rand_new_with_seed(42);

    for (guint iter = 0; iter < niter; iter++) {
        guint res = g_rand_int_range(rng, 1, max_size);
        GwyLine *line = gwy_line_new_sized(res, FALSE);

        for (guint i = 0; i < res; i++)
            gwy_line_set(line, i, i);

        for (guint k = 0; k < res; k++) {
            g_assert_cmpfloat(line->data[k], ==, k);
        }
        g_object_unref(line);
    }

    g_rand_free(rng);
}

static void
line_part_copy_dumb(const GwyLine *src,
                    guint pos,
                    guint len,
                    GwyLine *dest,
                    guint destpos)
{
    for (guint j = 0; j < len; j++) {
        if (pos + j >= src->res || destpos + j >= dest->res)
            continue;
        gdouble val = gwy_line_index(src, pos + j);
        gwy_line_index(dest, destpos + j) = val;
    }
}

void
test_line_copy(void)
{
    enum { max_size = 19 };
    GRand *rng = g_rand_new_with_seed(42);
    gsize niter = g_test_slow() ? 1000 : 200;

    for (gsize iter = 0; iter < niter; iter++) {
        guint sres = g_rand_int_range(rng, 1, max_size);
        guint dres = g_rand_int_range(rng, 1, max_size);
        GwyLine *source = gwy_line_new_sized(sres, FALSE);
        GwyLine *dest = gwy_line_new_sized(dres, FALSE);
        GwyLine *reference = gwy_line_new_sized(dres, FALSE);
        line_randomize(source, rng);
        line_randomize(reference, rng);
        gwy_line_copy_full(reference, dest);
        guint len = g_rand_int_range(rng, 0, MAX(sres, dres));
        guint pos = g_rand_int_range(rng, 0, sres);
        guint destpos = g_rand_int_range(rng, 0, dres);
        GwyLinePart lpart = { pos, len };
        gwy_line_copy(source, &lpart, dest, destpos);
        line_part_copy_dumb(source, pos, len, reference, destpos);
        line_assert_equal(dest, reference);
        g_object_unref(source);
        g_object_unref(dest);
        g_object_unref(reference);
    }
    g_rand_free(rng);
}

void
test_line_new_part(void)
{
    enum { max_size = 23 };
    GRand *rng = g_rand_new_with_seed(42);
    gsize niter = g_test_slow() ? 1000 : 200;

    for (gsize iter = 0; iter < niter; iter++) {
        guint res = g_rand_int_range(rng, 1, max_size);
        GwyLine *source = gwy_line_new_sized(res, FALSE);
        line_randomize(source, rng);
        guint len = g_rand_int_range(rng, 1, res+1);
        guint pos = g_rand_int_range(rng, 0, res-len+1);
        GwyLinePart lpart = { pos, len };
        GwyLine *part = gwy_line_new_part(source, &lpart, TRUE);
        GwyLine *reference = gwy_line_new_sized(len, FALSE);
        gwy_line_set_real(reference, source->real*len/res);
        gwy_line_set_offset(reference, source->real*pos/res);
        line_part_copy_dumb(source, pos, len, reference, 0);
        line_assert_equal(part, reference);
        g_object_unref(source);
        g_object_unref(part);
        g_object_unref(reference);
    }
    g_rand_free(rng);
}

void
test_line_outer_product(void)
{
    enum { xres = 2, yres = 3 };
    static const gdouble xdata[xres] = { 1, 2 };
    static const gdouble ydata[yres] = { 3, 4, 5 };
    static const gdouble expected[xres*yres] = {
        3, 6,
        4, 8,
        5, 10,
    };

    GwyLine *xline = gwy_line_new_sized(xres, FALSE);
    gwy_assign(xline->data, xdata, xres);

    GwyLine *yline = gwy_line_new_sized(yres, FALSE);
    gwy_assign(yline->data, ydata, yres);

    GwyField *field = gwy_line_outer_product(yline, xline);
    GwyField *reference = gwy_field_new_sized(xres, yres, FALSE);
    gwy_assign(reference->data, expected, xres*yres);
    field_assert_equal(field, reference);

    g_object_unref(reference);
    g_object_unref(field);
    g_object_unref(yline);
    g_object_unref(xline);
}

void
test_line_distribute_simple(void)
{
    GRand *rng = g_rand_new_with_seed(42);

    for (guint res = 1; res <= 13; res++) {
        GwyLine *source = gwy_line_new_sized(res, FALSE);
        line_randomize(source, rng);

        GwyLine *line = gwy_line_duplicate(source);
        gwy_line_accumulate(line, FALSE);
        gwy_line_distribute(line, FALSE);
        line_assert_numerically_equal(line, source, 1e-14);

        g_object_unref(line);
        g_object_unref(source);
    }
    g_rand_free(rng);
}

void
test_line_distribute_unbiased(void)
{
    GRand *rng = g_rand_new_with_seed(42);

    for (guint res = 1; res <= 13; res++) {
        GwyLine *source = gwy_line_new_sized(res, FALSE);
        line_randomize(source, rng);

        GwyLine *line = gwy_line_duplicate(source);
        gwy_line_accumulate(line, TRUE);
        gwy_line_distribute(line, TRUE);
        line_assert_numerically_equal(line, source, 1e-14);

        g_object_unref(line);
        g_object_unref(source);
    }
    g_rand_free(rng);
}

void
test_line_compatibility_res(void)
{
    GwyLine *line1 = gwy_line_new_sized(2, FALSE);
    GwyLine *line2 = gwy_line_new_sized(3, FALSE);
    GwyLine *line3 = gwy_line_new_sized(3, FALSE);

    g_assert_cmpuint(gwy_line_is_incompatible(line1, line2,
                                              GWY_LINE_COMPAT_RES),
                     ==, GWY_LINE_COMPAT_RES);
    g_assert_cmpuint(gwy_line_is_incompatible(line2, line1,
                                              GWY_LINE_COMPAT_RES),
                     ==, GWY_LINE_COMPAT_RES);

    g_assert_cmpuint(gwy_line_is_incompatible(line2, line3,
                                              GWY_LINE_COMPAT_RES),
                     ==, 0);
    g_assert_cmpuint(gwy_line_is_incompatible(line3, line2,
                                              GWY_LINE_COMPAT_RES),
                     ==, 0);

    g_assert_cmpuint(gwy_line_is_incompatible(line1, line2,
                                              GWY_LINE_COMPAT_DX),
                     ==, GWY_LINE_COMPAT_DX);
    g_assert_cmpuint(gwy_line_is_incompatible(line2, line1,
                                              GWY_LINE_COMPAT_DX),
                     ==, GWY_LINE_COMPAT_DX);

    g_assert_cmpuint(gwy_line_is_incompatible(line2, line3,
                                              GWY_LINE_COMPAT_DX),
                     ==, 0);
    g_assert_cmpuint(gwy_line_is_incompatible(line3, line2,
                                              GWY_LINE_COMPAT_DX),
                     ==, 0);

    g_assert_cmpuint(gwy_line_is_incompatible(line1, line2,
                                              GWY_LINE_COMPAT_REAL),
                     ==, 0);
    g_assert_cmpuint(gwy_line_is_incompatible(line2, line1,
                                              GWY_LINE_COMPAT_REAL),
                     ==, 0);
    g_assert_cmpuint(gwy_line_is_incompatible(line1, line3,
                                              GWY_LINE_COMPAT_REAL),
                     ==, 0);
    g_assert_cmpuint(gwy_line_is_incompatible(line3, line1,
                                              GWY_LINE_COMPAT_REAL),
                     ==, 0);
    g_assert_cmpuint(gwy_line_is_incompatible(line2, line3,
                                              GWY_LINE_COMPAT_REAL),
                     ==, 0);
    g_assert_cmpuint(gwy_line_is_incompatible(line3, line2,
                                              GWY_LINE_COMPAT_REAL),
                     ==, 0);

    g_object_unref(line1);
    g_object_unref(line2);
    g_object_unref(line3);
}

void
test_line_compatibility_real(void)
{
    GwyLine *line1 = gwy_line_new_sized(2, FALSE);
    GwyLine *line2 = gwy_line_new_sized(3, FALSE);
    GwyLine *line3 = gwy_line_new_sized(3, FALSE);

    gwy_line_set_real(line3, 1.5);

    g_assert_cmpuint(gwy_line_is_incompatible(line1, line2,
                                               GWY_LINE_COMPAT_REAL),
                     ==, 0);
    g_assert_cmpuint(gwy_line_is_incompatible(line2, line1,
                                               GWY_LINE_COMPAT_REAL),
                     ==, 0);

    g_assert_cmpuint(gwy_line_is_incompatible(line2, line3,
                                               GWY_LINE_COMPAT_REAL),
                     ==, GWY_LINE_COMPAT_REAL);
    g_assert_cmpuint(gwy_line_is_incompatible(line3, line2,
                                               GWY_LINE_COMPAT_REAL),
                     ==, GWY_LINE_COMPAT_REAL);

    g_assert_cmpuint(gwy_line_is_incompatible(line1, line3,
                                               GWY_LINE_COMPAT_REAL),
                     ==, GWY_LINE_COMPAT_REAL);
    g_assert_cmpuint(gwy_line_is_incompatible(line3, line1,
                                               GWY_LINE_COMPAT_REAL),
                     ==, GWY_LINE_COMPAT_REAL);

    g_assert_cmpuint(gwy_line_is_incompatible(line1, line2,
                                              GWY_LINE_COMPAT_DX),
                     ==, GWY_LINE_COMPAT_DX);
    g_assert_cmpuint(gwy_line_is_incompatible(line2, line1,
                                              GWY_LINE_COMPAT_DX),
                     ==, GWY_LINE_COMPAT_DX);

    g_assert_cmpuint(gwy_line_is_incompatible(line2, line3,
                                              GWY_LINE_COMPAT_DX),
                     ==, GWY_LINE_COMPAT_DX);
    g_assert_cmpuint(gwy_line_is_incompatible(line3, line2,
                                              GWY_LINE_COMPAT_DX),
                     ==, GWY_LINE_COMPAT_DX);

    g_assert_cmpuint(gwy_line_is_incompatible(line1, line3,
                                              GWY_LINE_COMPAT_DX),
                     ==, 0);
    g_assert_cmpuint(gwy_line_is_incompatible(line3, line1,
                                              GWY_LINE_COMPAT_DX),
                     ==, 0);

    g_object_unref(line1);
    g_object_unref(line2);
    g_object_unref(line3);
}

void
test_line_compatibility_units(void)
{
    GwyLine *line1 = gwy_line_new();
    GwyLine *line2 = gwy_line_new();
    GwyLine *line3 = gwy_line_new();

    gwy_unit_set_from_string(gwy_line_get_unit_x(line1), "m", NULL);
    gwy_unit_set_from_string(gwy_line_get_unit_y(line3), "m", NULL);

    g_assert_cmpuint(gwy_line_is_incompatible(line1, line2,
                                              GWY_LINE_COMPAT_LATERAL),
                     ==, GWY_LINE_COMPAT_LATERAL);
    g_assert_cmpuint(gwy_line_is_incompatible(line2, line1,
                                              GWY_LINE_COMPAT_LATERAL),
                     ==, GWY_LINE_COMPAT_LATERAL);

    g_assert_cmpuint(gwy_line_is_incompatible(line2, line3,
                                              GWY_LINE_COMPAT_LATERAL),
                     ==, 0);
    g_assert_cmpuint(gwy_line_is_incompatible(line3, line2,
                                              GWY_LINE_COMPAT_LATERAL),
                     ==, 0);

    g_assert_cmpuint(gwy_line_is_incompatible(line1, line3,
                                              GWY_LINE_COMPAT_LATERAL),
                     ==, GWY_LINE_COMPAT_LATERAL);
    g_assert_cmpuint(gwy_line_is_incompatible(line3, line1,
                                              GWY_LINE_COMPAT_LATERAL),
                     ==, GWY_LINE_COMPAT_LATERAL);

    g_assert_cmpuint(gwy_line_is_incompatible(line1, line2,
                                              GWY_LINE_COMPAT_VALUE),
                     ==, 0);
    g_assert_cmpuint(gwy_line_is_incompatible(line2, line1,
                                              GWY_LINE_COMPAT_VALUE),
                     ==, 0);

    g_assert_cmpuint(gwy_line_is_incompatible(line2, line3,
                                              GWY_LINE_COMPAT_VALUE),
                     ==, GWY_LINE_COMPAT_VALUE);
    g_assert_cmpuint(gwy_line_is_incompatible(line3, line2,
                                              GWY_LINE_COMPAT_VALUE),
                     ==, GWY_LINE_COMPAT_VALUE);

    g_assert_cmpuint(gwy_line_is_incompatible(line1, line3,
                                              GWY_LINE_COMPAT_VALUE),
                     ==, GWY_LINE_COMPAT_VALUE);
    g_assert_cmpuint(gwy_line_is_incompatible(line3, line1,
                                              GWY_LINE_COMPAT_VALUE),
                     ==, GWY_LINE_COMPAT_VALUE);

    g_object_unref(line1);
    g_object_unref(line2);
    g_object_unref(line3);
}

void
test_line_add_dist_uniform(void)
{
    guint res = 12;
    gdouble xmin = -1.0, xmax = 2.0, binsize = (xmax - xmin)/res;

    GwyLine *line = gwy_line_new_sized(res, FALSE);
    gwy_line_set_real(line, xmax - xmin);
    gwy_line_set_offset(line, xmin);

    // Exactly full
    gwy_line_clear(line, NULL);
    gwy_line_add_dist_uniform(line, xmin, xmax, G_PI);
    //print_line("full", line);
    g_assert_cmpfloat(fabs(gwy_line_sum_full(line) - G_PI), <=, 1e-14);
    for (guint i = 0; i < res; i++) {
        gdouble expected = G_PI/res;
        g_assert_cmpfloat(fabs(line->data[i] - expected), <=, 1e-14);
    }

    // Right half
    gwy_line_clear(line, NULL);
    gwy_line_add_dist_uniform(line, xmin - (xmax - xmin), xmax, G_PI);
    //print_line("right", line);
    g_assert_cmpfloat(fabs(gwy_line_sum_full(line) - 0.5*G_PI), <=, 1e-14);
    for (guint i = 0; i < res; i++) {
        gdouble expected = 0.5*G_PI/res;
        g_assert_cmpfloat(fabs(line->data[i] - expected), <=, 1e-14);
    }

    // Left half
    gwy_line_clear(line, NULL);
    gwy_line_add_dist_uniform(line, xmin, xmax + (xmax - xmin), G_PI);
    //print_line("left", line);
    g_assert_cmpfloat(fabs(gwy_line_sum_full(line) - 0.5*G_PI), <=, 1e-14);
    for (guint i = 0; i < res; i++) {
        gdouble expected = 0.5*G_PI/res;
        g_assert_cmpfloat(fabs(line->data[i] - expected), <=, 1e-14);
    }

    // Inside
    gwy_line_clear(line, NULL);
    gwy_line_add_dist_uniform(line, 0.0, 1.0, G_PI);
    //print_line("inside", line);
    g_assert_cmpfloat(fabs(gwy_line_sum_full(line) - G_PI), <=, 1e-14);
    for (guint i = 0; i < res; i++) {
        gdouble expected = (i < res/3 || i >= 2*res/3) ? 0.0 : 3*G_PI/res;
        g_assert_cmpfloat(fabs(line->data[i] - expected), <=, 1e-14);
    }

    // Within one bin
    gwy_line_clear(line, NULL);
    gwy_line_add_dist_uniform(line, 0.01, 0.02, G_PI);
    //print_line("one-bin", line);
    g_assert_cmpfloat(fabs(gwy_line_sum_full(line) - G_PI), <=, 1e-14);
    for (guint i = 0; i < res; i++) {
        gdouble expected = (i == res/3) ? G_PI : 0.0;
        g_assert_cmpfloat(fabs(line->data[i] - expected), <=, 1e-14);
    }

    // Partial bins
    gwy_line_clear(line, NULL);
    gwy_line_add_dist_uniform(line, xmin + 0.75*binsize, xmax - 0.75*binsize,
                              G_PI);
    //print_line("partial", line);
    g_assert_cmpfloat(fabs(gwy_line_sum_full(line) - G_PI), <=, 1e-14);
    for (guint i = 0; i < res; i++) {
        gdouble wb = G_PI/(res - 1.5);
        gdouble expected = (i == 0 || i == res-1) ? 0.25*wb : wb;
        g_assert_cmpfloat(fabs(line->data[i] - expected), <=, 1e-14);
    }

    // Ends in the first.
    gwy_line_clear(line, NULL);
    gwy_line_add_dist_uniform(line, xmin - binsize, xmin + binsize/3, G_PI);
    //print_line("first-end", line);
    g_assert_cmpfloat(fabs(gwy_line_sum_full(line) - G_PI/4), <=, 1e-14);
    for (guint i = 0; i < res; i++) {
        gdouble expected = (i == 0) ? G_PI/4 : 0.0;
        g_assert_cmpfloat(fabs(line->data[i] - expected), <=, 1e-14);
    }

    // Begins in the last.
    gwy_line_clear(line, NULL);
    gwy_line_add_dist_uniform(line, xmax - binsize/3, xmax + binsize, G_PI);
    //print_line("last-begining", line);
    g_assert_cmpfloat(fabs(gwy_line_sum_full(line) - G_PI/4), <=, 1e-14);
    for (guint i = 0; i < res; i++) {
        gdouble expected = (i == res-1) ? G_PI/4 : 0.0;
        g_assert_cmpfloat(fabs(line->data[i] - expected), <=, 1e-14);
    }

    g_object_unref(line);
}

void
test_line_add_dist_left_triangular(void)
{
    guint res = 12;
    gdouble xmin = -1.0, xmax = 2.0, binsize = (xmax - xmin)/res;

    GwyLine *line = gwy_line_new_sized(res, FALSE);
    gwy_line_set_real(line, xmax - xmin);
    gwy_line_set_offset(line, xmin);

    // Exactly full
    gwy_line_clear(line, NULL);
    gwy_line_add_dist_left_triangular(line, xmin, xmax, G_PI);
    //print_line("full", line);
    g_assert_cmpfloat(fabs(gwy_line_sum_full(line) - G_PI), <=, 1e-14);
    for (guint i = 0; i < res; i++) {
        gdouble expected = (2*i + 1)*G_PI/(res*res);
        g_assert_cmpfloat(fabs(line->data[i] - expected), <=, 1e-14);
    }

    // Right half
    gwy_line_clear(line, NULL);
    gwy_line_add_dist_left_triangular(line, xmin - (xmax - xmin), xmax, G_PI);
    //print_line("right", line);
    g_assert_cmpfloat(fabs(gwy_line_sum_full(line) - 0.75*G_PI), <=, 1e-14);
    for (guint i = 0; i < res; i++) {
        gdouble expected = (2*i + 1 + 2*res)*G_PI/(4*res*res);
        g_assert_cmpfloat(fabs(line->data[i] - expected), <=, 1e-14);
    }

    // Left half
    gwy_line_clear(line, NULL);
    gwy_line_add_dist_left_triangular(line, xmin, xmax + (xmax - xmin), G_PI);
    //print_line("left", line);
    g_assert_cmpfloat(fabs(gwy_line_sum_full(line) - 0.25*G_PI), <=, 1e-14);
    for (guint i = 0; i < res; i++) {
        gdouble expected = (2*i + 1)*G_PI/(4*res*res);
        g_assert_cmpfloat(fabs(line->data[i] - expected), <=, 1e-14);
    }

    // Inside
    gwy_line_clear(line, NULL);
    gwy_line_add_dist_left_triangular(line, 0.0, 1.0, G_PI);
    //print_line("inside", line);
    g_assert_cmpfloat(fabs(gwy_line_sum_full(line) - G_PI), <=, 1e-14);
    for (guint i = 0; i < res; i++) {
        gdouble expected = ((i < res/3 || i >= 2*res/3)
                            ? 0.0
                            : 9*(2*(i - res/3) + 1)*G_PI/(res*res));
        g_assert_cmpfloat(fabs(line->data[i] - expected), <=, 1e-14);
    }

    // Within one bin
    gwy_line_clear(line, NULL);
    gwy_line_add_dist_left_triangular(line, 0.01, 0.02, G_PI);
    //print_line("one-bin", line);
    g_assert_cmpfloat(fabs(gwy_line_sum_full(line) - G_PI), <=, 1e-14);
    for (guint i = 0; i < res; i++) {
        gdouble expected = (i == res/3) ? G_PI : 0.0;
        g_assert_cmpfloat(fabs(line->data[i] - expected), <=, 1e-14);
    }

    // Partial bins
    gwy_line_clear(line, NULL);
    gwy_line_add_dist_left_triangular(line,
                                      xmin + 0.75*binsize, xmax - 0.75*binsize,
                                      G_PI);
    //print_line("partial", line);
    g_assert_cmpfloat(fabs(gwy_line_sum_full(line) - G_PI), <=, 1e-14);
    for (guint i = 0; i < res; i++) {
        gdouble wb = 4*G_PI/(2*res*res - 6*res + 4.5);
        gdouble expected;
        if (i == 0)
            expected = wb/32.0;
        else if (i == res-1)
            expected = 0.25*(res - 13.0/8.0)*wb;
        else
            expected = (i - 0.25)*wb;
        g_assert_cmpfloat(fabs(line->data[i] - expected), <=, 1e-14);
    }

    // Ends in the first.
    gwy_line_clear(line, NULL);
    gwy_line_add_dist_left_triangular(line,
                                      xmin - binsize, xmin + binsize/3, G_PI);
    //print_line("first-end", line);
    g_assert_cmpfloat(fabs(gwy_line_sum_full(line) - 7*G_PI/16), <=, 1e-14);
    for (guint i = 0; i < res; i++) {
        gdouble expected = (i == 0) ? 7*G_PI/16 : 0.0;
        g_assert_cmpfloat(fabs(line->data[i] - expected), <=, 1e-14);
    }

    // Begins in the last.
    gwy_line_clear(line, NULL);
    gwy_line_add_dist_left_triangular(line,
                                      xmax - binsize/3, xmax + binsize, G_PI);
    //print_line("last-begining", line);
    g_assert_cmpfloat(fabs(gwy_line_sum_full(line) - G_PI/16), <=, 1e-14);
    for (guint i = 0; i < res; i++) {
        gdouble expected = (i == res-1) ? G_PI/16 : 0.0;
        g_assert_cmpfloat(fabs(line->data[i] - expected), <=, 1e-14);
    }

    g_object_unref(line);
}

void
test_line_add_dist_right_triangular(void)
{
    guint res = 12;
    gdouble xmin = -1.0, xmax = 2.0, binsize = (xmax - xmin)/res;

    GwyLine *line = gwy_line_new_sized(res, FALSE);
    gwy_line_set_real(line, xmax - xmin);
    gwy_line_set_offset(line, xmin);

    // Exactly full
    gwy_line_clear(line, NULL);
    gwy_line_add_dist_right_triangular(line, xmin, xmax, G_PI);
    //print_line("full", line);
    g_assert_cmpfloat(fabs(gwy_line_sum_full(line) - G_PI), <=, 1e-14);
    for (guint i = 0; i < res; i++) {
        gdouble expected = (2*(res - i) - 1)*G_PI/(res*res);
        g_assert_cmpfloat(fabs(line->data[i] - expected), <=, 1e-14);
    }

    // Right half
    gwy_line_clear(line, NULL);
    gwy_line_add_dist_right_triangular(line, xmin - (xmax - xmin), xmax, G_PI);
    //print_line("right", line);
    g_assert_cmpfloat(fabs(gwy_line_sum_full(line) - 0.25*G_PI), <=, 1e-14);
    for (guint i = 0; i < res; i++) {
        gdouble expected = (2*(res - i) - 1)*G_PI/(4*res*res);
        g_assert_cmpfloat(fabs(line->data[i] - expected), <=, 1e-14);
    }

    // Left half
    gwy_line_clear(line, NULL);
    gwy_line_add_dist_right_triangular(line, xmin, xmax + (xmax - xmin), G_PI);
    //print_line("right", line);
    g_assert_cmpfloat(fabs(gwy_line_sum_full(line) - 0.75*G_PI), <=, 1e-14);
    for (guint i = 0; i < res; i++) {
        gdouble expected = (2*(res - i) - 1 + 2*res)*G_PI/(4*res*res);
        g_assert_cmpfloat(fabs(line->data[i] - expected), <=, 1e-14);
    }

    // Inside
    gwy_line_clear(line, NULL);
    gwy_line_add_dist_right_triangular(line, 0.0, 1.0, G_PI);
    //print_line("inside", line);
    g_assert_cmpfloat(fabs(gwy_line_sum_full(line) - G_PI), <=, 1e-14);
    for (guint i = 0; i < res; i++) {
        gdouble expected = ((i < res/3 || i >= 2*res/3)
                            ? 0.0
                            : 9*(2*(2*res/3 - 1 - i) + 1)*G_PI/(res*res));
        g_assert_cmpfloat(fabs(line->data[i] - expected), <=, 1e-14);
    }

    // Within one bin
    gwy_line_clear(line, NULL);
    gwy_line_add_dist_right_triangular(line, 0.01, 0.02, G_PI);
    //print_line("one-bin", line);
    g_assert_cmpfloat(fabs(gwy_line_sum_full(line) - G_PI), <=, 1e-14);
    for (guint i = 0; i < res; i++) {
        gdouble expected = (i == res/3) ? G_PI : 0.0;
        g_assert_cmpfloat(fabs(line->data[i] - expected), <=, 1e-14);
    }

    // Partial bins
    gwy_line_clear(line, NULL);
    gwy_line_add_dist_right_triangular(line,
                                       xmin + 0.75*binsize, xmax - 0.75*binsize,
                                       G_PI);
    //print_line("partial", line);
    g_assert_cmpfloat(fabs(gwy_line_sum_full(line) - G_PI), <=, 1e-14);
    for (guint i = 0; i < res; i++) {
        gdouble wb = 4*G_PI/(2*res*res - 6*res + 4.5);
        gdouble expected;
        if (i == 0)
            expected = 0.25*(res - 13.0/8.0)*wb;
        else if (i == res-1)
            expected = wb/32.0;
        else
            expected = (res-1 - i - 0.25)*wb;
        g_assert_cmpfloat(fabs(line->data[i] - expected), <=, 1e-14);
    }

    // Ends in the first.
    gwy_line_clear(line, NULL);
    gwy_line_add_dist_right_triangular(line,
                                       xmin - binsize, xmin + binsize/3, G_PI);
    //print_line("first-end", line);
    g_assert_cmpfloat(fabs(gwy_line_sum_full(line) - G_PI/16), <=, 1e-14);
    for (guint i = 0; i < res; i++) {
        gdouble expected = (i == 0) ? G_PI/16 : 0.0;
        g_assert_cmpfloat(fabs(line->data[i] - expected), <=, 1e-14);
    }

    // Begins in the last.
    gwy_line_clear(line, NULL);
    gwy_line_add_dist_right_triangular(line,
                                       xmax - binsize/3, xmax + binsize, G_PI);
    //print_line("last-begining", line);
    g_assert_cmpfloat(fabs(gwy_line_sum_full(line) - 7*G_PI/16), <=, 1e-14);
    for (guint i = 0; i < res; i++) {
        gdouble expected = (i == res-1) ? 7*G_PI/16 : 0.0;
        g_assert_cmpfloat(fabs(line->data[i] - expected), <=, 1e-14);
    }

    g_object_unref(line);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
