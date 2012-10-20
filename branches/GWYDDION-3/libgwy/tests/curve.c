/*
 *  $Id$
 *  Copyright (C) 2010 David Nečas (Yeti).
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
 * Curve
 *
 ***************************************************************************/

void
curve_randomize(GwyCurve *curve,
                GRand *rng)
{
    GwyXY *d = curve->data;
    for (guint n = curve->n; n; n--, d++) {
        d->x = g_rand_double(rng);
        d->y = g_rand_double(rng);
    }
    gwy_curve_sort(curve);
}

static void
curve_assert_equal(const GwyCurve *result,
                   const GwyCurve *reference)
{
    g_assert(GWY_IS_CURVE(result));
    g_assert(GWY_IS_CURVE(reference));
    g_assert_cmpuint(result->n, ==, reference->n);
    compare_properties(G_OBJECT(result), G_OBJECT(reference));

    for (guint i = 0; i < result->n; i++) {
        GwyXY resxy = result->data[i];
        GwyXY refxy = reference->data[i];
        g_assert_cmpfloat(resxy.x, ==, refxy.x);
        g_assert_cmpfloat(resxy.y, ==, refxy.y);
    }
}

static void
curve_assert_equal_object(GObject *object, GObject *reference)
{
    curve_assert_equal(GWY_CURVE(object), GWY_CURVE(reference));
}

/*
static void
curve_assert_similar(const GwyCurve *result,
                          const GwyCurve *reference)
{
    g_assert(GWY_IS_CURVE(result));
    g_assert(GWY_IS_CURVE(reference));
    g_assert_cmpuint(result->n, ==, reference->n);

    gdouble tolx = 1e-14*MAX(fabs(curve->data[curve->n-1].x),
                             fabs(curve->data[0].x));
    gdouble min, max;
    gwy_curve_min_max(curve, &min, &max);
    gdouble toly = 1e-14*MAX(fabs(max), fabs(min));

    for (guint i = 0; i < result->n; i++) {
        GwyXY resxy = result->data[i];
        GwyXY refxy = reference->data[i];
        g_assert_cmpfloat(fabs(resxy.x - refxy.x), <=, tolx);
        g_assert_cmpfloat(fabs(resxy.y - refxy.y), <=, toly);
    }
}
*/

void
test_curve_units_assign(void)
{
    GwyCurve *curve = gwy_curve_new(), *curve2 = gwy_curve_new();
    GwyUnit *unit_x = gwy_curve_get_unit_x(curve);
    GwyUnit *unit_y = gwy_curve_get_unit_y(curve);
    guint count_x = 0;
    guint count_y = 0;

    g_signal_connect_swapped(unit_x, "changed",
                             G_CALLBACK(record_signal), &count_x);
    g_signal_connect_swapped(unit_y, "changed",
                             G_CALLBACK(record_signal), &count_y);

    gwy_unit_set_from_string(gwy_curve_get_unit_x(curve), "m", NULL);
    g_assert_cmpuint(count_x, ==, 1);
    g_assert_cmpuint(count_y, ==, 0);

    gwy_unit_set_from_string(gwy_curve_get_unit_y(curve), "s", NULL);
    g_assert_cmpuint(count_x, ==, 1);
    g_assert_cmpuint(count_y, ==, 1);

    gwy_curve_assign(curve, curve2);
    g_assert_cmpuint(count_x, ==, 2);
    g_assert_cmpuint(count_y, ==, 2);
    g_assert(gwy_curve_get_unit_x(curve) == unit_x);
    g_assert(gwy_curve_get_unit_y(curve) == unit_y);

    // Try again to see if the signal counts change.
    gwy_curve_assign(curve, curve2);
    g_assert_cmpuint(count_x, ==, 2);
    g_assert_cmpuint(count_y, ==, 2);
    g_assert(gwy_curve_get_unit_x(curve) == unit_x);
    g_assert(gwy_curve_get_unit_y(curve) == unit_y);

    g_object_unref(curve2);
    g_object_unref(curve);
}

void
test_curve_get(void)
{
    enum { max_size = 255, niter = 40 };

    GRand *rng = g_rand_new_with_seed(42);

    for (guint iter = 0; iter < niter; iter++) {
        guint res = g_rand_int_range(rng, 1, max_size);
        GwyCurve *curve = gwy_curve_new_sized(res);
        curve_randomize(curve, rng);

        for (guint i = 0; i < res; i++) {
            GwyXY pt = gwy_curve_get(curve, i);
            g_assert_cmpfloat(curve->data[i].x, ==, pt.x);
            g_assert_cmpfloat(curve->data[i].y, ==, pt.y);
        }
        g_object_unref(curve);
    }

    g_rand_free(rng);
}

void
test_curve_set(void)
{
    enum { max_size = 255, niter = 40 };

    GRand *rng = g_rand_new_with_seed(42);

    for (guint iter = 0; iter < niter; iter++) {
        guint res = g_rand_int_range(rng, 1, max_size);
        GwyCurve *curve = gwy_curve_new_sized(res);

        for (guint i = 0; i < res; i++) {
            GwyXY pt = { i, G_PI/(i + 1) };
            gwy_curve_set(curve, i, pt);
        }

        for (guint k = 0; k < res; k++) {
            GwyXY pt = gwy_curve_get(curve, k);
            g_assert_cmpfloat(pt.x, ==, k);
            g_assert_cmpfloat(pt.y, ==, G_PI/(k + 1));
        }
        g_object_unref(curve);
    }

    g_rand_free(rng);
}

void
test_curve_serialize(void)
{
    enum { max_size = 55 };
    GRand *rng = g_rand_new_with_seed(42);
    gsize niter = g_test_slow() ? 50 : 10;

    for (guint iter = 0; iter < niter; iter++) {
        guint res = g_rand_int_range(rng, 1, max_size);
        GwyCurve *original = gwy_curve_new_sized(res);
        curve_randomize(original, rng);
        GwyCurve *copy;

        serializable_duplicate(GWY_SERIALIZABLE(original),
                               curve_assert_equal_object);
        serializable_assign(GWY_SERIALIZABLE(original),
                            curve_assert_equal_object);
        copy = GWY_CURVE(serialize_and_back(G_OBJECT(original),
                                            curve_assert_equal_object));
        g_object_unref(copy);

        g_object_unref(original);
    }
    g_rand_free(rng);
}

void
test_curve_serialize_failure_odd(void)
{
    GOutputStream *stream = g_memory_output_stream_new(NULL, 0,
                                                       g_realloc, g_free);
    GDataOutputStream *datastream = g_data_output_stream_new(stream);
    g_data_output_stream_set_byte_order(datastream,
                                        G_DATA_STREAM_BYTE_ORDER_LITTLE_ENDIAN);

    guint len = 5;
    data_stream_put_string0(datastream, "GwyCurve", NULL, NULL);
    g_data_output_stream_put_uint64(datastream, 0, NULL, NULL);
    data_stream_put_string0(datastream, "data", NULL, NULL);
    g_data_output_stream_put_byte(datastream, GWY_SERIALIZABLE_DOUBLE_ARRAY,
                                  NULL, NULL);
    g_data_output_stream_put_uint64(datastream, len, NULL, NULL);
    for (guint i = 0; i < len; i++)
        data_stream_put_double(datastream, i, NULL, NULL);

    GwyErrorList *error_list = NULL;
    gwy_error_list_add(&error_list,
                       GWY_DESERIALIZE_ERROR, GWY_DESERIALIZE_ERROR_INVALID,
                       "Curve data length is %lu which is not a multiple of 2.",
                       (gulong)len);

    deserialize_assert_failure(G_MEMORY_OUTPUT_STREAM(stream), error_list);
    gwy_error_list_clear(&error_list);
    g_object_unref(datastream);
    g_object_unref(stream);
}

void
test_curve_props(void)
{
    static const GwyXY data[] = { { 1, 2 }, { 3, 4 } };
    GwyCurve *curve = gwy_curve_new_from_data(data, G_N_ELEMENTS(data));
    guint n = 0;
    g_object_get(curve,
                 "n-points", &n,
                 NULL);
    g_assert_cmpuint(n, ==, curve->n);
    g_assert_cmpuint(n, ==, G_N_ELEMENTS(data));
    g_object_unref(curve);
}

void
test_curve_data_changed(void)
{
    GwyCurve *curve = gwy_curve_new();
    guint counter = 0;
    g_signal_connect_swapped(curve, "data-changed",
                             G_CALLBACK(record_signal), &counter);
    gwy_curve_data_changed(curve);
    g_assert_cmpuint(counter, ==, 1);
    g_object_unref(curve);
}

void
test_curve_regularize(void)
{
    enum { max_size = 27 };
    GRand *rng = g_rand_new_with_seed(42);
    gsize niter = g_test_slow() ? 50 : 10;

    for (guint iter = 0; iter < niter; iter++) {
        guint res = g_rand_int_range(rng, 2, max_size);
        gdouble real = g_rand_double_range(rng, 1, G_E)
                       * exp(g_rand_double_range(rng, -8, 8));
        gdouble off = g_rand_double_range(rng, -5, 5)*real;
        GwyLine *line = gwy_line_new_sized(res, FALSE);
        line_randomize(line, rng);
        gwy_line_set_real(line, real);
        gwy_line_set_offset(line, off);

        GwyCurve *curve = gwy_curve_new_from_line(line);
        g_assert_cmpuint(curve->n, ==, line->res);
        g_assert(gwy_unit_equal(gwy_curve_get_unit_x(curve),
                                gwy_line_get_unit_x(line)));
        g_assert(gwy_unit_equal(gwy_curve_get_unit_y(curve),
                                gwy_line_get_unit_y(line)));

        GwyLine *newline = gwy_curve_regularize_full(curve, 0);
        g_assert_cmpuint(newline->res, ==, curve->n);
        g_assert_cmpfloat(fabs(newline->real - line->real),
                          <=,
                          1e-14*line->real);
        g_assert_cmpfloat(fabs(newline->off - line->off),
                          <=,
                          1e-14*fabs(line->off));
        g_assert(gwy_unit_equal(gwy_line_get_unit_x(newline),
                                gwy_curve_get_unit_x(curve)));
        g_assert(gwy_unit_equal(gwy_line_get_unit_y(newline),
                                gwy_curve_get_unit_y(curve)));
        line_assert_numerically_equal(newline, line, 1e-14);

        g_object_unref(newline);
        g_object_unref(curve);

        curve = gwy_curve_new();
        gwy_curve_set_from_line(curve, line);
        g_assert_cmpuint(curve->n, ==, line->res);
        g_assert(gwy_unit_equal(gwy_curve_get_unit_x(curve),
                                gwy_line_get_unit_x(line)));
        g_assert(gwy_unit_equal(gwy_curve_get_unit_y(curve),
                                gwy_line_get_unit_y(line)));

        newline = gwy_curve_regularize_full(curve, 0);
        g_assert_cmpuint(newline->res, ==, curve->n);
        g_assert_cmpfloat(fabs(newline->real - line->real),
                          <=,
                          1e-14*line->real);
        g_assert_cmpfloat(fabs(newline->off - line->off),
                          <=,
                          1e-14*fabs(line->off));
        g_assert(gwy_unit_equal(gwy_line_get_unit_x(newline),
                                gwy_curve_get_unit_x(curve)));
        g_assert(gwy_unit_equal(gwy_line_get_unit_y(newline),
                                gwy_curve_get_unit_y(curve)));
        line_assert_numerically_equal(newline, line, 1e-14);

        g_object_unref(newline);
        g_object_unref(curve);
        g_object_unref(line);
    }

    g_rand_free(rng);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
