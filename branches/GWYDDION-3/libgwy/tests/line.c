/*
 *  $Id$
 *  Copyright (C) 2009 David Necas (Yeti).
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

void
line_randomize(GwyLine *line,
               GRand *rng)
{
    gdouble *d = line->data;
    for (guint n = line->res; n; n--, d++)
        *d = g_rand_double(rng);
}

static void
test_line_assert_equal(const GwyLine *result,
                        const GwyLine *reference)
{
    g_assert(GWY_IS_LINE(result));
    g_assert(GWY_IS_LINE(reference));
    g_assert_cmpuint(result->res, ==, reference->res);

    for (guint j = 0; j < result->res; j++)
        g_assert_cmpfloat(result->data[j], ==, reference->data[j]);
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
    GwyField *line = gwy_line_new();
    guint counter = 0;
    g_signal_connect_swapped(line, "data-changed",
                             G_CALLBACK(record_signal), &counter);
    gwy_line_data_changed(line);
    g_assert_cmpuint(counter, ==, 1);
    g_object_unref(line);
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

        copy = gwy_line_duplicate(original);
        test_line_assert_equal(copy, original);
        g_object_unref(copy);

        copy = gwy_line_new();
        gwy_line_assign(copy, original);
        test_line_assert_equal(copy, original);
        g_object_unref(copy);

        copy = GWY_LINE(serialize_and_back(G_OBJECT(original)));
        test_line_assert_equal(copy, original);
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
        gwy_line_copy(reference, dest);
        guint len = g_rand_int_range(rng, 0, MAX(sres, dres));
        guint pos = g_rand_int_range(rng, 0, sres);
        guint destpos = g_rand_int_range(rng, 0, dres);
        gwy_line_part_copy(source, pos, len, dest, destpos);
        line_part_copy_dumb(source, pos, len, reference, destpos);
        test_line_assert_equal(dest, reference);
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
        GwyLine *part = gwy_line_new_part(source, pos, len, TRUE);
        GwyLine *reference = gwy_line_new_sized(len, FALSE);
        line_part_copy_dumb(source, pos, len, reference, 0);
        test_line_assert_equal(part, reference);
        g_object_unref(source);
        g_object_unref(part);
        g_object_unref(reference);
    }
    g_rand_free(rng);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
