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
 * Line
 *
 ***************************************************************************/

G_GNUC_UNUSED
static void
print_line(const gchar *name, const GwyLine *line)
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

static void
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
        g_assert_cmpfloat(fabs(value - ref), <=, eps);
    }
}

void
test_line_props(void)
{
    GwyLine *line = gwy_line_new_sized(41, FALSE);
    guint res;
    gdouble real, off;
    g_object_get(line,
                 "res", &res,
                 "real", &real,
                 "offset", &off,
                 NULL);
    g_assert_cmpuint(res, ==, line->res);
    g_assert_cmpfloat(real, ==, line->real);
    g_assert_cmpfloat(off, ==, line->off);
    real = 5.0;
    off = -3;
    gwy_line_set_real(line, real);
    gwy_line_set_offset(line, off);
    g_assert_cmpfloat(real, ==, line->real);
    g_assert_cmpfloat(off, ==, line->off);
    g_object_unref(line);
}

void
test_line_data_changed(void)
{
    GwyLine *line = gwy_line_new();
    guint counter = 0;
    g_signal_connect_swapped(line, "data-changed",
                             G_CALLBACK(record_signal), &counter);
    gwy_line_data_changed(line);
    g_assert_cmpuint(counter, ==, 1);
    g_object_unref(line);
}

void
test_line_units(void)
{
    enum { max_size = 411 };
    GRand *rng = g_rand_new();
    g_rand_set_seed(rng, 42);
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
test_line_serialize(void)
{
    enum { max_size = 55 };
    GRand *rng = g_rand_new();
    g_rand_set_seed(rng, 42);
    gsize niter = g_test_slow() ? 50 : 10;

    for (guint iter = 0; iter < niter; iter++) {
        guint width = g_rand_int_range(rng, 1, max_size);
        GwyLine *original = gwy_line_new_sized(width, FALSE);
        line_randomize(original, rng);
        GwyLine *copy;

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
    GRand *rng = g_rand_new();
    g_rand_set_seed(rng, 42);
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
    GRand *rng = g_rand_new();
    g_rand_set_seed(rng, 42);
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
    GRand *rng = g_rand_new();
    g_rand_set_seed(rng, 42);

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
    GRand *rng = g_rand_new();
    g_rand_set_seed(rng, 42);

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
                                              GWY_LINE_COMPATIBLE_RES),
                     ==, GWY_LINE_COMPATIBLE_RES);
    g_assert_cmpuint(gwy_line_is_incompatible(line2, line1,
                                              GWY_LINE_COMPATIBLE_RES),
                     ==, GWY_LINE_COMPATIBLE_RES);

    g_assert_cmpuint(gwy_line_is_incompatible(line2, line3,
                                              GWY_LINE_COMPATIBLE_RES),
                     ==, 0);
    g_assert_cmpuint(gwy_line_is_incompatible(line3, line2,
                                              GWY_LINE_COMPATIBLE_RES),
                     ==, 0);

    g_assert_cmpuint(gwy_line_is_incompatible(line1, line2,
                                              GWY_LINE_COMPATIBLE_DX),
                     ==, GWY_LINE_COMPATIBLE_DX);
    g_assert_cmpuint(gwy_line_is_incompatible(line2, line1,
                                              GWY_LINE_COMPATIBLE_DX),
                     ==, GWY_LINE_COMPATIBLE_DX);

    g_assert_cmpuint(gwy_line_is_incompatible(line2, line3,
                                              GWY_LINE_COMPATIBLE_DX),
                     ==, 0);
    g_assert_cmpuint(gwy_line_is_incompatible(line3, line2,
                                              GWY_LINE_COMPATIBLE_DX),
                     ==, 0);

    g_assert_cmpuint(gwy_line_is_incompatible(line1, line2,
                                              GWY_LINE_COMPATIBLE_REAL),
                     ==, 0);
    g_assert_cmpuint(gwy_line_is_incompatible(line2, line1,
                                              GWY_LINE_COMPATIBLE_REAL),
                     ==, 0);
    g_assert_cmpuint(gwy_line_is_incompatible(line1, line3,
                                              GWY_LINE_COMPATIBLE_REAL),
                     ==, 0);
    g_assert_cmpuint(gwy_line_is_incompatible(line3, line1,
                                              GWY_LINE_COMPATIBLE_REAL),
                     ==, 0);
    g_assert_cmpuint(gwy_line_is_incompatible(line2, line3,
                                              GWY_LINE_COMPATIBLE_REAL),
                     ==, 0);
    g_assert_cmpuint(gwy_line_is_incompatible(line3, line2,
                                              GWY_LINE_COMPATIBLE_REAL),
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
                                               GWY_LINE_COMPATIBLE_REAL),
                     ==, 0);
    g_assert_cmpuint(gwy_line_is_incompatible(line2, line1,
                                               GWY_LINE_COMPATIBLE_REAL),
                     ==, 0);

    g_assert_cmpuint(gwy_line_is_incompatible(line2, line3,
                                               GWY_LINE_COMPATIBLE_REAL),
                     ==, GWY_LINE_COMPATIBLE_REAL);
    g_assert_cmpuint(gwy_line_is_incompatible(line3, line2,
                                               GWY_LINE_COMPATIBLE_REAL),
                     ==, GWY_LINE_COMPATIBLE_REAL);

    g_assert_cmpuint(gwy_line_is_incompatible(line1, line3,
                                               GWY_LINE_COMPATIBLE_REAL),
                     ==, GWY_LINE_COMPATIBLE_REAL);
    g_assert_cmpuint(gwy_line_is_incompatible(line3, line1,
                                               GWY_LINE_COMPATIBLE_REAL),
                     ==, GWY_LINE_COMPATIBLE_REAL);

    g_assert_cmpuint(gwy_line_is_incompatible(line1, line2,
                                              GWY_LINE_COMPATIBLE_DX),
                     ==, GWY_LINE_COMPATIBLE_DX);
    g_assert_cmpuint(gwy_line_is_incompatible(line2, line1,
                                              GWY_LINE_COMPATIBLE_DX),
                     ==, GWY_LINE_COMPATIBLE_DX);

    g_assert_cmpuint(gwy_line_is_incompatible(line2, line3,
                                              GWY_LINE_COMPATIBLE_DX),
                     ==, GWY_LINE_COMPATIBLE_DX);
    g_assert_cmpuint(gwy_line_is_incompatible(line3, line2,
                                              GWY_LINE_COMPATIBLE_DX),
                     ==, GWY_LINE_COMPATIBLE_DX);

    g_assert_cmpuint(gwy_line_is_incompatible(line1, line3,
                                              GWY_LINE_COMPATIBLE_DX),
                     ==, 0);
    g_assert_cmpuint(gwy_line_is_incompatible(line3, line1,
                                              GWY_LINE_COMPATIBLE_DX),
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
                                              GWY_LINE_COMPATIBLE_LATERAL),
                     ==, GWY_LINE_COMPATIBLE_LATERAL);
    g_assert_cmpuint(gwy_line_is_incompatible(line2, line1,
                                              GWY_LINE_COMPATIBLE_LATERAL),
                     ==, GWY_LINE_COMPATIBLE_LATERAL);

    g_assert_cmpuint(gwy_line_is_incompatible(line2, line3,
                                              GWY_LINE_COMPATIBLE_LATERAL),
                     ==, 0);
    g_assert_cmpuint(gwy_line_is_incompatible(line3, line2,
                                              GWY_LINE_COMPATIBLE_LATERAL),
                     ==, 0);

    g_assert_cmpuint(gwy_line_is_incompatible(line1, line3,
                                              GWY_LINE_COMPATIBLE_LATERAL),
                     ==, GWY_LINE_COMPATIBLE_LATERAL);
    g_assert_cmpuint(gwy_line_is_incompatible(line3, line1,
                                              GWY_LINE_COMPATIBLE_LATERAL),
                     ==, GWY_LINE_COMPATIBLE_LATERAL);

    g_assert_cmpuint(gwy_line_is_incompatible(line1, line2,
                                              GWY_LINE_COMPATIBLE_VALUE),
                     ==, 0);
    g_assert_cmpuint(gwy_line_is_incompatible(line2, line1,
                                              GWY_LINE_COMPATIBLE_VALUE),
                     ==, 0);

    g_assert_cmpuint(gwy_line_is_incompatible(line2, line3,
                                              GWY_LINE_COMPATIBLE_VALUE),
                     ==, GWY_LINE_COMPATIBLE_VALUE);
    g_assert_cmpuint(gwy_line_is_incompatible(line3, line2,
                                              GWY_LINE_COMPATIBLE_VALUE),
                     ==, GWY_LINE_COMPATIBLE_VALUE);

    g_assert_cmpuint(gwy_line_is_incompatible(line1, line3,
                                              GWY_LINE_COMPATIBLE_VALUE),
                     ==, GWY_LINE_COMPATIBLE_VALUE);
    g_assert_cmpuint(gwy_line_is_incompatible(line3, line1,
                                              GWY_LINE_COMPATIBLE_VALUE),
                     ==, GWY_LINE_COMPATIBLE_VALUE);

    g_object_unref(line1);
    g_object_unref(line2);
    g_object_unref(line3);
}

void
test_line_add_dist_uniform(void)
{
    guint res = 12;
    gdouble xmin = -1.0, xmax = 2.0;

    GwyLine *line = gwy_line_new_sized(res, FALSE);
    gwy_line_set_real(line, xmax - xmin);
    gwy_line_set_offset(line, xmin);

    // Exactly full
    gwy_line_clear(line, NULL);
    gwy_line_add_dist_uniform(line, xmin, xmax, G_PI);
    //print_line("full", line);
    g_assert_cmpfloat(fabs(gwy_line_sum_full(line) - G_PI), <=, 1e-14);
    for (guint i = 0; i < res; i++)
        g_assert_cmpfloat(fabs(line->data[i] - G_PI/res), <=, 1e-14);

    // Right half
    gwy_line_clear(line, NULL);
    gwy_line_add_dist_uniform(line, xmin - (xmax - xmin), xmax, G_PI);
    //print_line("right", line);
    g_assert_cmpfloat(fabs(gwy_line_sum_full(line) - 0.5*G_PI), <=, 1e-14);
    for (guint i = 0; i < res; i++)
        g_assert_cmpfloat(fabs(line->data[i] - 0.5*G_PI/res), <=, 1e-14);

    // Left half
    gwy_line_clear(line, NULL);
    gwy_line_add_dist_uniform(line, xmin, xmax + (xmax - xmin), G_PI);
    //print_line("left", line);
    g_assert_cmpfloat(fabs(gwy_line_sum_full(line) - 0.5*G_PI), <=, 1e-14);
    for (guint i = 0; i < res; i++)
        g_assert_cmpfloat(fabs(line->data[i] - 0.5*G_PI/res), <=, 1e-14);

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
    gwy_line_add_dist_uniform(line,
                              xmin + 0.75*(xmax - xmin)/res,
                              xmax - 0.75*(xmax - xmin)/res,
                              G_PI);
    //print_line("partial", line);
    g_assert_cmpfloat(fabs(gwy_line_sum_full(line) - G_PI), <=, 1e-14);
    for (guint i = 0; i < res; i++) {
        gdouble wb = G_PI/(res - 1.5);
        gdouble expected = (i == 0 || i == res-1) ? 0.25*wb : wb;
        g_assert_cmpfloat(fabs(line->data[i] - expected), <=, 1e-14);
    }

    g_object_unref(line);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
