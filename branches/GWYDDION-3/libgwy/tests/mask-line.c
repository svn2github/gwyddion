/*
 *  $Id$
 *  Copyright (C) 2011 David Nečas (Yeti).
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
#include <stdio.h>

/***************************************************************************
 *
 * Mask Line
 *
 ***************************************************************************/

G_GNUC_UNUSED
static void
mask_line_dump(const GwyMaskLine *maskline, const gchar *name)
{
    printf("%s %s %p %u\n",
           G_OBJECT_TYPE_NAME(maskline), name, maskline,
           maskline->res);
    for (guint j = 0; j < maskline->res; j++)
        putchar(gwy_mask_line_get(maskline, j) ? '#' : '.');
    putchar('\n');
}

void
test_mask_line_props(void)
{
    GwyMaskLine *maskline = gwy_mask_line_new_sized(41, FALSE);
    guint res;
    g_object_get(maskline,
                 "res", &res,
                 NULL);
    g_assert_cmpuint(res, ==, maskline->res);
    g_object_unref(maskline);
}

void
test_mask_line_data_changed(void)
{
    // Plain emission
    GwyMaskLine *line = gwy_mask_line_new();
    guint counter = 0;
    g_signal_connect_swapped(line, "data-changed",
                             G_CALLBACK(record_signal), &counter);
    gwy_mask_line_data_changed(line, NULL);
    g_assert_cmpuint(counter, ==, 1);
    g_object_unref(line);

    // Specified part argument
    line = gwy_mask_line_new_sized(8, FALSE);
    GwyLinePart lpart = { 1, 2 };
    g_signal_connect_swapped(line, "data-changed",
                             G_CALLBACK(line_part_assert_equal), &lpart);
    gwy_mask_line_data_changed(line, &lpart);
    g_object_unref(line);

    // NULL part argument
    line = gwy_mask_line_new_sized(2, FALSE);
    g_signal_connect_swapped(line, "data-changed",
                             G_CALLBACK(line_part_assert_equal), NULL);
    gwy_mask_line_data_changed(line, NULL);
    g_object_unref(line);
}

static void
mask_line_part_copy_dumb(const GwyMaskLine *src,
                         guint pos,
                         guint len,
                         GwyMaskLine *dest,
                         guint destpos)
{
    for (guint j = 0; j < len; j++) {
        if (pos + j >= src->res || destpos + j >= dest->res)
            continue;

        gboolean val = gwy_mask_line_get(src, pos + j);
        gwy_mask_line_set(dest, destpos + j, val);
    }
}

static guint32*
mask_line_random_pool_new(GRand *rng,
                          guint max_size)
{
    guint n = (max_size + 31)/32 * 4;
    guint32 *pool = g_new(guint32, n);
    for (guint i = 0; i < n; i++)
        pool[i] = g_rand_int(rng);
    return pool;
}

static void
mask_line_random_pool_free(guint32 *pool)
{
    g_free(pool);
}

static void
mask_line_randomize(GwyMaskLine *maskline,
                    const guint32 *pool,
                    guint max_size,
                    GRand *rng)
{
    guint required = (maskline->res + 31)/32;
    guint n = (max_size + 31)/32 * 4;
    guint offset = g_rand_int_range(rng, 0, n - required);
    guint32 *data = maskline->data;
    memcpy(data, pool + offset, required*sizeof(guint32));
    gwy_mask_line_invalidate(maskline);
}

static void
mask_line_assert_equal(const GwyMaskLine *result,
                       const GwyMaskLine *reference)
{
    g_assert(GWY_IS_MASK_LINE(result));
    g_assert(GWY_IS_MASK_LINE(reference));
    g_assert(result->res == reference->res);
    compare_properties(G_OBJECT(result), G_OBJECT(reference));

    // NB: The mask may be wrong for end == 0 because on x86 (at least) shift
    // by 32bits is the same as shift by 0 bits.
    guint end = result->res % 32;
#if (G_BYTE_ORDER == G_LITTLE_ENDIAN)
    guint32 m = 0xffffffffu >> (32 - end);
#endif
#if (G_BYTE_ORDER == G_BIG_ENDIAN)
    guint32 m = 0xffffffffu << (32 - end);
#endif

    for (guint j = 0; j < result->res/32; j++)
        g_assert_cmphex(result->data[j], ==, reference->data[j]);
    if (end) {
        guint j = result->res/32;
        g_assert_cmphex((result->data[j] & m), ==, (reference->data[j] & m));
    }
}

static void
mask_line_assert_equal_object(GObject *object, GObject *reference)
{
    mask_line_assert_equal(GWY_MASK_LINE(object), GWY_MASK_LINE(reference));
}

void
test_mask_line_copy(void)
{
    enum { max_size = 600 };
    GRand *rng = g_rand_new_with_seed(42);
    guint32 *pool = mask_line_random_pool_new(rng, max_size);
    gsize niter = g_test_slow() ? 100000 : 10000;

    for (gsize iter = 0; iter < niter; iter++) {
        guint sres = g_rand_int_range(rng, 1, max_size);
        guint dres = g_rand_int_range(rng, 1, max_size);
        GwyMaskLine *source = gwy_mask_line_new_sized(sres, FALSE);
        GwyMaskLine *dest = gwy_mask_line_new_sized(dres, FALSE);
        GwyMaskLine *reference = gwy_mask_line_new_sized(dres, FALSE);
        mask_line_randomize(source, pool, max_size, rng);
        mask_line_randomize(reference, pool, max_size, rng);
        gwy_mask_line_assign(dest, reference);
        guint len = g_rand_int_range(rng, 0, MAX(sres, dres));
        guint pos = g_rand_int_range(rng, 0, sres);
        guint destpos = g_rand_int_range(rng, 0, dres);
        GwyLinePart lpart = { pos, len };
        gwy_mask_line_copy(source, &lpart, dest, destpos);
        mask_line_part_copy_dumb(source, pos, len, reference, destpos);
        mask_line_assert_equal(dest, reference);
        g_object_unref(source);
        g_object_unref(dest);
        g_object_unref(reference);
    }
    mask_line_random_pool_free(pool);
    g_rand_free(rng);
}

void
test_mask_line_new_part(void)
{
    enum { max_size = 433 };
    GRand *rng = g_rand_new_with_seed(42);
    guint32 *pool = mask_line_random_pool_new(rng, max_size);
    gsize niter = g_test_slow() ? 10000 : 1000;

    for (gsize iter = 0; iter < niter; iter++) {
        guint res = g_rand_int_range(rng, 1, max_size);
        GwyMaskLine *source = gwy_mask_line_new_sized(res, FALSE);
        mask_line_randomize(source, pool, max_size, rng);
        guint len = g_rand_int_range(rng, 1, res+1);
        guint pos = g_rand_int_range(rng, 0, res-len+1);
        GwyLinePart lpart = { pos, len };
        GwyMaskLine *part = gwy_mask_line_new_part(source, &lpart);
        GwyMaskLine *reference = gwy_mask_line_new_sized(len, FALSE);
        mask_line_part_copy_dumb(source, pos, len, reference, 0);
        mask_line_assert_equal(part, reference);
        g_object_unref(source);
        g_object_unref(part);
        g_object_unref(reference);
    }
    mask_line_random_pool_free(pool);
    g_rand_free(rng);
}

void
test_mask_line_serialize(void)
{
    enum { max_size = 333 };
    GRand *rng = g_rand_new_with_seed(42);
    guint32 *pool = mask_line_random_pool_new(rng, max_size);
    gsize niter = g_test_slow() ? 50 : 10;

    for (guint iter = 0; iter < niter; iter++) {
        guint len = g_rand_int_range(rng, 1, max_size);
        GwyMaskLine *original = gwy_mask_line_new_sized(len, FALSE);
        mask_line_randomize(original, pool, max_size, rng);
        GwyMaskLine *copy;

        serializable_duplicate(GWY_SERIALIZABLE(original),
                               mask_line_assert_equal_object);
        serializable_assign(GWY_SERIALIZABLE(original),
                            mask_line_assert_equal_object);
        copy = GWY_MASK_LINE(serialize_and_back(G_OBJECT(original),
                                                mask_line_assert_equal_object));
        g_object_unref(copy);

        g_object_unref(original);
    }
    mask_line_random_pool_free(pool);
    g_rand_free(rng);
}

void
test_mask_line_set_size(void)
{
    GwyMaskLine *maskline = gwy_mask_line_new_sized(13, TRUE);
    guint res_changed = 0;

    g_signal_connect_swapped(maskline, "notify::res",
                             G_CALLBACK(record_signal), &res_changed);

    gwy_mask_line_set_size(maskline, 13, TRUE);
    g_assert_cmpuint(maskline->res, ==, 13);
    g_assert_cmpuint(res_changed, ==, 0);

    gwy_mask_line_set_size(maskline, 11, TRUE);
    g_assert_cmpuint(maskline->res, ==, 11);
    g_assert_cmpuint(res_changed, ==, 1);

    g_object_unref(maskline);
}

static guint
mask_line_count_dumb(GwyMaskLine *line,
                     const GwyMaskLine *mask,
                     guint pos, guint len,
                     gboolean value)
{
    guint count = 0;
    for (guint j = 0; j < line->res; j++) {
        if (j < pos || j >= pos + len)
            continue;
        if (!mask || gwy_mask_line_get(mask, j)) {
            if (!!gwy_mask_line_get(line, j) == value)
                count++;
        }
    }
    return count;
}

void
test_mask_line_count(void)
{
    enum { max_size = 333 };
    GRand *rng = g_rand_new_with_seed(42);
    guint32 *pool = mask_line_random_pool_new(rng, max_size);
    gsize niter = g_test_slow() ? 500 : 100;

    for (guint iter = 0; iter < niter; iter++) {
        guint res = g_rand_int_range(rng, 1, max_size);
        GwyMaskLine *line = gwy_mask_line_new_sized(res, FALSE);
        GwyMaskLine *mask = gwy_mask_line_new_sized(res, FALSE);

        mask_line_randomize(line, pool, max_size, rng);
        mask_line_randomize(mask, pool, max_size, rng);

        g_assert_cmpuint(gwy_mask_line_count(line, NULL, FALSE),
                         ==, mask_line_count_dumb(line, NULL, 0, res, FALSE));
        g_assert_cmpuint(gwy_mask_line_count(line, NULL, TRUE),
                         ==, mask_line_count_dumb(line, NULL, 0, res, TRUE));
        g_assert_cmpuint(gwy_mask_line_count(line, mask, FALSE),
                         ==, mask_line_count_dumb(line, mask, 0, res, FALSE));
        g_assert_cmpuint(gwy_mask_line_count(line, mask, TRUE),
                         ==, mask_line_count_dumb(line, mask, 0, res, TRUE));

        guint len = g_rand_int_range(rng, 1, res+1);
        guint pos = g_rand_int_range(rng, 0, res-len+1);

        GwyLinePart lpart = { pos, len };
        g_assert_cmpuint(gwy_mask_line_part_count(line, &lpart, FALSE),
                         ==, mask_line_count_dumb(line, NULL, pos, len, FALSE));
        g_assert_cmpuint(gwy_mask_line_part_count(line, &lpart, TRUE),
                         ==, mask_line_count_dumb(line, NULL, pos, len, TRUE));

        g_object_unref(line);
        g_object_unref(mask);
    }
    mask_line_random_pool_free(pool);
    g_rand_free(rng);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
