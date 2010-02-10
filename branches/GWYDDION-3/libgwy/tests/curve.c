/*
 *  $Id$
 *  Copyright (C) 2010 David Necas (Yeti).
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
test_curve_assert_equal(const GwyCurve *result,
                        const GwyCurve *reference)
{
    g_assert(GWY_IS_CURVE(result));
    g_assert(GWY_IS_CURVE(reference));
    g_assert_cmpuint(result->n, ==, reference->n);

    for (guint i = 0; i < result->n; i++) {
        GwyXY resxy = result->data[i];
        GwyXY refxy = reference->data[i];
        g_assert_cmpfloat(resxy.x, ==, refxy.x);
        g_assert_cmpfloat(resxy.y, ==, refxy.y);
    }
}

/*
static void
test_curve_assert_similar(const GwyCurve *result,
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
test_curve_serialize(void)
{
    enum { max_size = 55 };
    GRand *rng = g_rand_new();
    g_rand_set_seed(rng, 42);
    gsize niter = g_test_slow() ? 50 : 10;

    for (guint iter = 0; iter < niter; iter++) {
        guint res = g_rand_int_range(rng, 1, max_size);
        GwyCurve *original = gwy_curve_new_sized(res);
        curve_randomize(original, rng);
        GwyCurve *copy;

        copy = gwy_curve_duplicate(original);
        test_curve_assert_equal(copy, original);
        g_object_unref(copy);

        copy = gwy_curve_new();
        gwy_curve_assign(copy, original);
        test_curve_assert_equal(copy, original);
        g_object_unref(copy);

        copy = GWY_CURVE(serialize_and_back(G_OBJECT(original)));
        test_curve_assert_equal(copy, original);
        g_object_unref(copy);

        g_object_unref(original);
    }
    g_rand_free(rng);
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
test_curve_line_convert(void)
{
    enum { max_size = 27 };
    GRand *rng = g_rand_new();
    g_rand_set_seed(rng, 42);
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
        GwyLine *newline = gwy_line_new_from_curve(curve, 0);

        g_assert_cmpfloat((newline->real - line->real), <=, 1e-14*line->real);
        g_assert_cmpfloat((newline->off - line->off), <=, 1e-14*fabs(line->off));

        g_object_unref(newline);
        g_object_unref(line);
        g_object_unref(curve);
    }
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
