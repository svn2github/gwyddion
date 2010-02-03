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
#include <stdio.h>

/***************************************************************************
 *
 * Mask Field
 *
 ***************************************************************************/

G_GNUC_UNUSED
static void
mask_field_dump(const GwyMaskField *maskfield, const gchar *name)
{
    printf("%s %s %p %ux%u (stride %u)\n",
           G_OBJECT_TYPE_NAME(maskfield), name, maskfield,
           maskfield->xres, maskfield->yres, maskfield->stride);
    for (guint i = 0; i < maskfield->yres; i++) {
        for (guint j = 0; j < maskfield->xres; j++)
            putchar(gwy_mask_field_get(maskfield, j, i) ? '#' : '.');
        putchar('\n');
    }
}

void
test_mask_field_props(void)
{
    GwyMaskField *maskfield = gwy_mask_field_new_sized(41, 37, FALSE);
    guint xres, yres, stride;
    g_object_get(maskfield,
                 "x-res", &xres,
                 "y-res", &yres,
                 "stride", &stride,
                 NULL);
    g_assert_cmpuint(xres, ==, maskfield->xres);
    g_assert_cmpuint(yres, ==, maskfield->yres);
    g_assert_cmpuint(stride, ==, maskfield->stride);
    g_object_unref(maskfield);
}

static void
mask_field_part_copy_dumb(const GwyMaskField *src,
                          guint col,
                          guint row,
                          guint width,
                          guint height,
                          GwyMaskField *dest,
                          guint destcol,
                          guint destrow)
{
    for (guint i = 0; i < height; i++) {
        if (row + i >= src->yres || destrow + i >= dest->yres)
            continue;
        for (guint j = 0; j < width; j++) {
            if (col + j >= src->xres || destcol + j >= dest->xres)
                continue;

            gboolean val = gwy_mask_field_get(src, col + j, row + i);
            gwy_mask_field_set(dest, destcol + j, destrow + i, val);
        }
    }
}

static guint32*
mask_field_random_pool_new(GRand *rng,
                           guint max_size)
{
    guint n = max_size*(max_size + 31)/32 * 4;
    guint32 *pool = g_new(guint32, n);
    for (guint i = 0; i < n; i++)
        pool[i] = g_rand_int(rng);
    return pool;
}

static void
mask_field_random_pool_free(guint32 *pool)
{
    g_free(pool);
}

static void
mask_field_randomize(GwyMaskField *maskfield,
                     const guint32 *pool,
                     guint max_size,
                     GRand *rng)
{
    guint required = maskfield->stride * maskfield->yres;
    guint n = max_size*(max_size + 31)/32 * 4;
    guint offset = g_rand_int_range(rng, 0, n - required);
    guint32 *data = gwy_mask_field_get_data(maskfield);
    memcpy(data, pool + offset, required*sizeof(guint32));
}

static void
test_mask_field_assert_equal(const GwyMaskField *result,
                             const GwyMaskField *reference)
{
    g_assert(GWY_IS_MASK_FIELD(result));
    g_assert(GWY_IS_MASK_FIELD(reference));
    g_assert(result->xres == reference->xres);
    g_assert(result->yres == reference->yres);
    g_assert(result->stride == reference->stride);

    // NB: The mask may be wrong for end == 0 because on x86 (at least) shift
    // by 32bits is the same as shift by 0 bits.
    guint end = result->xres % 32;
#if (G_BYTE_ORDER == G_LITTLE_ENDIAN)
    guint32 m = 0xffffffffu >> (32 - end);
#endif
#if (G_BYTE_ORDER == G_BIG_ENDIAN)
    guint32 m = 0xffffffffu << (32 - end);
#endif

    for (guint i = 0; i < result->yres; i++) {
        guint32 *result_row = result->data + i*result->stride;
        guint32 *reference_row = reference->data + i*reference->stride;
        for (guint j = 0; j < result->xres/32; j++)
            g_assert_cmpuint(result_row[j], ==, reference_row[j]);
        if (end) {
            guint j = result->xres/32;
            g_assert_cmpuint((result_row[j] & m), ==, (reference_row[j] & m));
        }
    }
}

void
test_mask_field_copy(void)
{
    enum { max_size = 600 };
    GRand *rng = g_rand_new();
    g_rand_set_seed(rng, 42);
    guint32 *pool = mask_field_random_pool_new(rng, max_size);
    gsize niter = g_test_slow() ? 100000 : 10000;

    for (gsize iter = 0; iter < niter; iter++) {
        guint sxres = g_rand_int_range(rng, 1, max_size);
        guint syres = g_rand_int_range(rng, 1, max_size/4);
        guint dxres = g_rand_int_range(rng, 1, max_size);
        guint dyres = g_rand_int_range(rng, 1, max_size/4);
        GwyMaskField *source = gwy_mask_field_new_sized(sxres, syres, FALSE);
        GwyMaskField *dest = gwy_mask_field_new_sized(dxres, dyres, FALSE);
        GwyMaskField *reference = gwy_mask_field_new_sized(dxres, dyres, FALSE);
        mask_field_randomize(source, pool, max_size, rng);
        mask_field_randomize(reference, pool, max_size, rng);
        gwy_mask_field_copy(reference, dest);
        guint width = g_rand_int_range(rng, 0, MAX(sxres, dxres));
        guint height = g_rand_int_range(rng, 0, MAX(syres, dyres));
        guint col = g_rand_int_range(rng, 0, sxres);
        guint row = g_rand_int_range(rng, 0, syres);
        guint destcol = g_rand_int_range(rng, 0, dxres);
        guint destrow = g_rand_int_range(rng, 0, dyres);
        gwy_mask_field_part_copy(source, col, row, width, height,
                                 dest, destcol, destrow);
        mask_field_part_copy_dumb(source, col, row, width, height,
                                  reference, destcol, destrow);
        test_mask_field_assert_equal(dest, reference);
        g_object_unref(source);
        g_object_unref(dest);
        g_object_unref(reference);
    }
    mask_field_random_pool_free(pool);
    g_rand_free(rng);
}

void
test_mask_field_new_part(void)
{
    enum { max_size = 233 };
    GRand *rng = g_rand_new();
    g_rand_set_seed(rng, 42);
    guint32 *pool = mask_field_random_pool_new(rng, max_size);
    gsize niter = g_test_slow() ? 1000 : 200;

    for (gsize iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size/4);
        GwyMaskField *source = gwy_mask_field_new_sized(xres, yres, FALSE);
        mask_field_randomize(source, pool, max_size, rng);
        guint width = g_rand_int_range(rng, 1, xres+1);
        guint height = g_rand_int_range(rng, 1, yres+1);
        guint col = g_rand_int_range(rng, 0, xres-width+1);
        guint row = g_rand_int_range(rng, 0, yres-height+1);
        GwyMaskField *part = gwy_mask_field_new_part(source,
                                                     col, row, width, height);
        GwyMaskField *reference = gwy_mask_field_new_sized(width, height,
                                                           FALSE);
        mask_field_part_copy_dumb(source, col, row, width, height,
                                  reference, 0, 0);
        test_mask_field_assert_equal(part, reference);
        g_object_unref(source);
        g_object_unref(part);
        g_object_unref(reference);
    }
    mask_field_random_pool_free(pool);
    g_rand_free(rng);
}

static void
mask_field_logical_dumb(GwyMaskField *maskfield,
                        const GwyMaskField *operand,
                        const GwyMaskField *mask,
                        GwyLogicalOperator op)
{
    gboolean results[4] = { op & 8, op & 4, op & 2, op & 1 };

    for (guint i = 0; i < maskfield->yres; i++) {
        for (guint j = 0; j < maskfield->xres; j++) {
            if (!mask || gwy_mask_field_get(mask, j, i)) {
                guint a = !!gwy_mask_field_get(maskfield, j, i);
                guint b = !!gwy_mask_field_get(operand, j, i);
                guint res = results[2*a + b];
                gwy_mask_field_set(maskfield, j, i, res);
            }
        }
    }
}

void
test_mask_field_logical(void)
{
    enum { max_size = 333 };
    GRand *rng = g_rand_new();
    g_rand_set_seed(rng, 42);
    guint32 *pool = mask_field_random_pool_new(rng, max_size);
    gsize niter = g_test_slow() ? 500 : 100;

    for (guint iter = 0; iter < niter; iter++) {
        guint width = g_rand_int_range(rng, 1, max_size);
        guint height = g_rand_int_range(rng, 1, max_size/4);
        GwyMaskField *source = gwy_mask_field_new_sized(width, height, FALSE);
        GwyMaskField *operand = gwy_mask_field_new_sized(width, height, FALSE);
        GwyMaskField *mask = gwy_mask_field_new_sized(width, height, FALSE);
        GwyMaskField *reference = gwy_mask_field_new_sized(width, height,
                                                           FALSE);
        GwyMaskField *dest = gwy_mask_field_new_sized(width, height,
                                                           FALSE);

        mask_field_randomize(source, pool, max_size, rng);
        mask_field_randomize(operand, pool, max_size, rng);
        mask_field_randomize(mask, pool, max_size, rng);

        for (GwyLogicalOperator op = GWY_LOGICAL_ZERO;
             op <= GWY_LOGICAL_ONE;
             op++) {
            gwy_mask_field_copy(source, reference);
            gwy_mask_field_copy(source, dest);
            mask_field_logical_dumb(reference, operand, NULL, op);
            gwy_mask_field_logical(dest, operand, NULL, op);
            test_mask_field_assert_equal(dest, reference);

            gwy_mask_field_copy(source, reference);
            gwy_mask_field_copy(source, dest);
            mask_field_logical_dumb(reference, operand, mask, op);
            gwy_mask_field_logical(dest, operand, mask, op);
            test_mask_field_assert_equal(dest, reference);
        }

        g_object_unref(source);
        g_object_unref(operand);
        g_object_unref(mask);
        g_object_unref(reference);
        g_object_unref(dest);
    }
    mask_field_random_pool_free(pool);
    g_rand_free(rng);
}

static void
mask_field_part_fill_dumb(GwyMaskField *maskfield,
                          guint col,
                          guint row,
                          guint width,
                          guint height,
                          gboolean value)
{
    for (guint i = row; i < row + height; i++) {
        for (guint j = col; j < col + width; j++)
            gwy_mask_field_set(maskfield, j, i, value);
    }
}

void
test_mask_field_fill(void)
{
    enum { max_size = 333 };
    GRand *rng = g_rand_new();
    g_rand_set_seed(rng, 42);
    guint32 *pool = mask_field_random_pool_new(rng, max_size);
    gsize niter = g_test_slow() ? 100 : 20;

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size/4);
        GwyMaskField *dest = gwy_mask_field_new_sized(xres, yres, FALSE);
        GwyMaskField *reference = gwy_mask_field_new_sized(xres, yres, FALSE);
        guint width = g_rand_int_range(rng, 0, xres+1);
        guint height = g_rand_int_range(rng, 0, yres+1);
        guint col = g_rand_int_range(rng, 0, xres-width+1);
        guint row = g_rand_int_range(rng, 0, yres-height+1);

        mask_field_randomize(reference, pool, max_size, rng);
        gwy_mask_field_copy(reference, dest);

        gwy_mask_field_part_fill(dest, col, row, width, height, FALSE);
        mask_field_part_fill_dumb(reference,  col, row, width, height, FALSE);
        test_mask_field_assert_equal(dest, reference);

        mask_field_randomize(reference, pool, max_size, rng);
        gwy_mask_field_copy(reference, dest);

        gwy_mask_field_part_fill(dest, col, row, width, height, TRUE);
        mask_field_part_fill_dumb(reference,  col, row, width, height, TRUE);
        test_mask_field_assert_equal(dest, reference);

        g_object_unref(dest);
        g_object_unref(reference);
    }
    mask_field_random_pool_free(pool);
    g_rand_free(rng);
}

void
test_mask_field_serialize(void)
{
    enum { max_size = 333 };
    GRand *rng = g_rand_new();
    g_rand_set_seed(rng, 42);
    guint32 *pool = mask_field_random_pool_new(rng, max_size);
    gsize niter = g_test_slow() ? 50 : 10;

    for (guint iter = 0; iter < niter; iter++) {
        guint width = g_rand_int_range(rng, 1, max_size);
        guint height = g_rand_int_range(rng, 1, max_size/4);
        GwyMaskField *original = gwy_mask_field_new_sized(width, height, FALSE);
        mask_field_randomize(original, pool, max_size, rng);
        GwyMaskField *copy;

        copy = gwy_mask_field_duplicate(original);
        test_mask_field_assert_equal(copy, original);
        g_object_unref(copy);

        copy = gwy_mask_field_new();
        gwy_mask_field_assign(copy, original);
        test_mask_field_assert_equal(copy, original);
        g_object_unref(copy);

        copy = GWY_MASK_FIELD(serialize_and_back(G_OBJECT(original)));
        test_mask_field_assert_equal(copy, original);
        g_object_unref(copy);

        g_object_unref(original);
    }
    mask_field_random_pool_free(pool);
    g_rand_free(rng);
}

void
test_mask_field_set_size(void)
{
    GwyMaskField *maskfield = gwy_mask_field_new_sized(13, 11, TRUE);
    guint xres_changed = 0, yres_changed = 0;

    g_signal_connect_swapped(maskfield, "notify::x-res",
                             G_CALLBACK(record_signal), &xres_changed);
    g_signal_connect_swapped(maskfield, "notify::y-res",
                             G_CALLBACK(record_signal), &yres_changed);

    gwy_mask_field_set_size(maskfield, 13, 11, TRUE);
    g_assert_cmpuint(maskfield->xres, ==, 13);
    g_assert_cmpuint(maskfield->yres, ==, 11);
    g_assert_cmpuint(xres_changed, ==, 0);
    g_assert_cmpuint(yres_changed, ==, 0);

    gwy_mask_field_set_size(maskfield, 13, 10, TRUE);
    g_assert_cmpuint(maskfield->xres, ==, 13);
    g_assert_cmpuint(maskfield->yres, ==, 10);
    g_assert_cmpuint(xres_changed, ==, 0);
    g_assert_cmpuint(yres_changed, ==, 1);

    gwy_mask_field_set_size(maskfield, 11, 10, TRUE);
    g_assert_cmpuint(maskfield->xres, ==, 11);
    g_assert_cmpuint(maskfield->yres, ==, 10);
    g_assert_cmpuint(xres_changed, ==, 1);
    g_assert_cmpuint(yres_changed, ==, 1);

    gwy_mask_field_set_size(maskfield, 15, 14, TRUE);
    g_assert_cmpuint(maskfield->xres, ==, 15);
    g_assert_cmpuint(maskfield->yres, ==, 14);
    g_assert_cmpuint(xres_changed, ==, 2);
    g_assert_cmpuint(yres_changed, ==, 2);

    g_object_unref(maskfield);
}

void
test_mask_field_grain_no(void)
{
    enum { max_size = 83 };
    GRand *rng = g_rand_new();
    g_rand_set_seed(rng, 42);
    guint32 *pool = mask_field_random_pool_new(rng, max_size);
    gsize niter = g_test_slow() ? 50 : 10;

    for (guint iter = 0; iter < niter; iter++) {
        guint width = g_rand_int_range(rng, 1, max_size);
        guint height = g_rand_int_range(rng, 1, max_size);
        GwyMaskField *maskfield = gwy_mask_field_new_sized(width, height,
                                                           FALSE);
        mask_field_randomize(maskfield, pool, max_size, rng);

        guint ngrains;
        const guint32 *grains = gwy_mask_field_number_grains(maskfield,
                                                             &ngrains);
        guint *counts = g_new0(guint, ngrains+1);
        for (guint i = 0; i < height; i++) {
            for (guint j = 0; j < width; j++) {
                guint id_up = i ? grains[(i - 1)*width + j] : 0;
                guint id_left = j ? grains[i*width + j - 1] : 0;
                guint id = grains[i*width + j];
                guint id_right = j+1 < width ? grains[i*width + j + 1] : 0;
                guint id_down = i+1 < height ? grains[(i + 1)*width + j] : 0;

                g_assert(id_up <= ngrains);
                g_assert(id_left <= ngrains);
                g_assert(id <= ngrains);
                g_assert(id_right <= ngrains);
                g_assert(id_down <= ngrains);
                g_assert(id == id_up || !id || !id_up);
                g_assert(id == id_left || !id || !id_left);
                g_assert(id == id_right || !id || !id_right);
                g_assert(id == id_down || !id || !id_down);
                counts[id]++;
            }
        }

        for (guint id = 1; id <= ngrains; id++)
            g_assert(counts[id]);
        g_free(counts);
        // XXX: The hard part is to reliably check grain are also contiguous.
        g_object_unref(maskfield);
    }
    mask_field_random_pool_free(pool);
    g_rand_free(rng);
}

static inline guint
count_set_bits_l1(guint32 x)
{
    guint count = 0;
    while (x) {
        if (x & 1)
            count++;
        x >>= 1;
    }
    return count;
}

static inline guint
count_set_bits_l1c(guint32 x)
{
    guint count = 0;
    while (x) {
        count += x & 1;
        x >>= 1;
    }
    return count;
}

static inline guint
count_set_bits_l2c(guint32 x)
{
    static const guint8 table[4] = { 0, 1, 1, 2 };
    guint count = 0;
    while (x) {
        count += table[x & 0x3];
        x >>= 2;
    }
    return count;
}

static inline guint
count_set_bits_l4c(guint32 x)
{
    static const guint8 table[16] = {
        0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4
    };
    guint count = 0;
    while (x) {
        count += table[x & 0xf];
        x >>= 4;
    }
    return count;
}

static inline guint
count_set_bits_l8c(guint32 x)
{
    static const guint8 table[256] = {
        0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
        1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
        1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
        1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
        3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
        4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8,
    };
    guint count = 0;
    while (x) {
        count += table[x & 0xff];
        x >>= 8;
    }
    return count;
}

static inline guint
count_set_bits_l2h(guint32 x)
{
    static const guint16 table[4] = { 0, 1, 1, 2 };
    guint count = 0;
    while (x) {
        count += table[x & 0x3];
        x >>= 2;
    }
    return count;
}

static inline guint
count_set_bits_l4h(guint32 x)
{
    static const guint16 table[16] = {
        0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4
    };
    guint count = 0;
    while (x) {
        count += table[x & 0xf];
        x >>= 4;
    }
    return count;
}

static inline guint
count_set_bits_l8h(guint32 x)
{
    static const guint16 table[256] = {
        0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
        1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
        1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
        1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
        3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
        4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8,
    };
    guint count = 0;
    while (x) {
        count += table[x & 0xff];
        x >>= 8;
    }
    return count;
}

static inline guint
count_set_bits_lu8h(guint32 x)
{
    static const guint16 table[256] = {
        0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
        1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
        1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
        1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
        3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
        4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8,
    };
    guint count = 0;
    if (!x)
        return count;
    count += table[x & 0xff];
    x >>= 8;
    if (!x)
        return count;
    count += table[x & 0xff];
    x >>= 8;
    if (!x)
        return count;
    count += table[x & 0xff];
    x >>= 8;
    if (!x)
        return count;
    count += table[x & 0xff];
    return count;
}

static inline guint
count_set_bits_u8h(guint32 x)
{
    static const guint16 table[256] = {
        0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
        1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
        1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
        1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
        3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
        4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8,
    };
    guint count = 0;
    count += table[x & 0xff];
    x >>= 8;
    count += table[x & 0xff];
    x >>= 8;
    count += table[x & 0xff];
    x >>= 8;
    count += table[x & 0xff];
    return count;
}


static inline guint
count_set_bits_l2w(guint32 x)
{
    static const guint table[4] = { 0, 1, 1, 2 };
    guint count = 0;
    while (x) {
        count += table[x & 0x3];
        x >>= 2;
    }
    return count;
}

static inline guint
count_set_bits_l4w(guint32 x)
{
    static const guint table[16] = {
        0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4
    };
    guint count = 0;
    while (x) {
        count += table[x & 0xf];
        x >>= 4;
    }
    return count;
}

static inline guint
count_set_bits_l8w(guint32 x)
{
    static const guint table[256] = {
        0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
        1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
        1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
        1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
        3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
        4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8,
    };
    guint count = 0;
    while (x) {
        count += table[x & 0xff];
        x >>= 8;
    }
    return count;
}

#ifdef __GNUC__
#define count_set_bits_gcc __builtin_popcount
#endif

#define COUNT_BITS_LOOP(name) \
    do { \
        g_timer_start(timer); \
        guint count = 0; \
        for (guint iter = 0; iter < niter; iter++) { \
            guint32 *p = pool; \
            for (guint i = n; i; i--) \
                count += count_set_bits_##name(*(p++)); \
        } \
        gdouble t = g_timer_elapsed(timer, NULL); \
        g_printerr("@@ %s: time=%g, count=%u\n", #name, t, count); \
    } while (0)

// XXX: This is a benchmark, not a regular test.  Not sure how to apply
// g_test_minimized_result() to it.
void
test_mask_field_count_benchmark(void)
{
    GRand *rng = g_rand_new();
    g_rand_set_seed(rng, 42);
    guint n = 65536, niter = 2000;
    guint32 *pool = g_new(guint32, n);
    for (guint i = 0; i < n; i++)
        pool[i] = g_rand_int(rng);
    GTimer *timer = g_timer_new();

    g_printerr("\n");
    COUNT_BITS_LOOP(l1);
    COUNT_BITS_LOOP(l1c);
    COUNT_BITS_LOOP(l2c);
    COUNT_BITS_LOOP(l2h);
    COUNT_BITS_LOOP(l2w);
    COUNT_BITS_LOOP(l4c);
    COUNT_BITS_LOOP(l4h);
    COUNT_BITS_LOOP(l4w);
    COUNT_BITS_LOOP(l8c);
    COUNT_BITS_LOOP(l8h);
    COUNT_BITS_LOOP(lu8h);
    COUNT_BITS_LOOP(u8h);
    COUNT_BITS_LOOP(l8w);
#ifdef __GNUC__
    COUNT_BITS_LOOP(gcc);
#endif

    g_timer_destroy(timer);
    g_free(pool);
    g_rand_free(rng);
}

static guint
mask_field_count_dumb(GwyMaskField *field,
                      const GwyMaskField *mask,
                      guint col, guint row,
                      guint width, guint height,
                      gboolean value)
{
    guint count = 0;
    for (guint i = 0; i < field->yres; i++) {
        if (i < row || i >= row + height)
            continue;
        for (guint j = 0; j < field->xres; j++) {
            if (j < col || j >= col + width)
                continue;
            if (!mask || gwy_mask_field_get(mask, j, i)) {
                if (!!gwy_mask_field_get(field, j, i) == value)
                    count++;
            }
        }
    }
    return count;
}

void
test_mask_field_count(void)
{
    enum { max_size = 333 };
    GRand *rng = g_rand_new();
    g_rand_set_seed(rng, 42);
    guint32 *pool = mask_field_random_pool_new(rng, max_size);
    gsize niter = g_test_slow() ? 500 : 100;

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size/4);
        GwyMaskField *field = gwy_mask_field_new_sized(xres, yres, FALSE);
        GwyMaskField *mask = gwy_mask_field_new_sized(xres, yres, FALSE);

        mask_field_randomize(field, pool, max_size, rng);
        mask_field_randomize(mask, pool, max_size, rng);

        g_assert_cmpuint(gwy_mask_field_count(field, NULL, FALSE),
                         ==, mask_field_count_dumb(field, NULL,
                                                   0, 0, xres, yres, FALSE));
        g_assert_cmpuint(gwy_mask_field_count(field, NULL, TRUE),
                         ==, mask_field_count_dumb(field, NULL,
                                                   0, 0, xres, yres, TRUE));
        g_assert_cmpuint(gwy_mask_field_count(field, mask, FALSE),
                         ==, mask_field_count_dumb(field, mask,
                                                   0, 0, xres, yres, FALSE));
        g_assert_cmpuint(gwy_mask_field_count(field, mask, TRUE),
                         ==, mask_field_count_dumb(field, mask,
                                                   0, 0, xres, yres, TRUE));

        guint width = g_rand_int_range(rng, 1, xres+1);
        guint height = g_rand_int_range(rng, 1, yres+1);
        guint col = g_rand_int_range(rng, 0, xres-width+1);
        guint row = g_rand_int_range(rng, 0, yres-height+1);

        g_assert_cmpuint(gwy_mask_field_part_count(field,
                                                   col, row, width, height,
                                                   FALSE),
                         ==, mask_field_count_dumb(field, NULL,
                                                   col, row, width, height,
                                                   FALSE));
        g_assert_cmpuint(gwy_mask_field_part_count(field,
                                                   col, row, width, height,
                                                   TRUE),
                         ==, mask_field_count_dumb(field, NULL,
                                                   col, row, width, height,
                                                   TRUE));

        g_object_unref(field);
        g_object_unref(mask);
    }
    mask_field_random_pool_free(pool);
    g_rand_free(rng);
}

static GwyMaskField*
mask_field_from_string(const gchar *str)
{
    guint width = 0, height = 0;
    const gchar *prev = str, *s = str;
    while (*s) {
        if (*s == '\n') {
            height++;
            if (!width)
                width = s - str;
            else
                g_assert(s - prev == width);
            prev = s+1;
        }
        else {
            g_assert(*s == '0' || *s == '1'
                     || *s == ' ' || *s == '@'
                     || *s == '.' || *s == '#');
        }
        s++;
    }
    if (s != prev) {
        if (!width)
            width = s - str;
        else
            g_assert(s - prev == width);
    }
    GwyMaskField *field = gwy_mask_field_new_sized(width, height, FALSE);
    GwyMaskIter iter;
    s = str;
    for (guint i = 0; i < height; i++, s++) {
        gwy_mask_field_iter_init(field, iter, 0, i);
        for (guint j = 0; j < width; j++, s++) {
            gboolean one = (*s == '1' || *s == '@' || *s == '#');
            gwy_mask_iter_set(iter, one);
            gwy_mask_iter_next(iter);
        }
        g_assert(*s == '\n' || *s == '\0');
    }
    return field;
}

void
test_mask_field_grow_one(const gchar *orig_str,
                         const gchar *grow_str,
                         const gchar *keep_str)
{
    GwyMaskField *grow = mask_field_from_string(grow_str);
    GwyMaskField *keep = mask_field_from_string(keep_str);
    GwyMaskField *orig;

    orig = mask_field_from_string(orig_str);
    gwy_mask_field_grow(orig, FALSE);
    test_mask_field_assert_equal(orig, grow);
    g_object_unref(orig);

    orig = mask_field_from_string(orig_str);
    gwy_mask_field_grow(orig, TRUE);
    test_mask_field_assert_equal(orig, keep);
    g_object_unref(orig);

    g_object_unref(keep);
    g_object_unref(grow);
}

void
test_mask_field_grow(void)
{
    const gchar *orig1_str =
        "##   #   #\n"
        " ##   ##  \n"
        "   ###  # \n"
        "#    #   #\n";
    const gchar *grow1_str =
        "### ######\n"
        "##########\n"
        "##########\n"
        "## #### ##\n";
    const gchar *keep1_str =
        "### ## # #\n"
        "###   ## #\n"
        " # ###  # \n"
        "#  ####  #\n";
    test_mask_field_grow_one(orig1_str, grow1_str, keep1_str);

    const gchar *orig2_str =
        "                                          ######                  \n"
        "        ######    #        #     #        #    #                  \n"
        "                  #         #   #         ## # ##                #\n"
        "                  #          # #             #                   #\n"
        "              #####           #         ######                   #\n"
        " ###                         # #                                  \n"
        " # # #                      #   #         ######                 #\n"
        " ###                                                              \n"
        "                  #####                                           \n";
    const gchar *grow2_str =
        "        ######    #        #     #       ########                 \n"
        "       ########  ###      ###   ###      ########                #\n"
        "        ######   ###       ### ###       #########              ##\n"
        "              ######        #####       #########               ##\n"
        " ###         #######         ###       ########                 ##\n"
        "######        #####         #####       ########                 #\n"
        "#######                    ### ###       ########               ##\n"
        "######            #####     #   #         ######                 #\n"
        " ###             #######                                          \n";
    const gchar *keep2_str =
        "        ######    #        #     #       ########                 \n"
        "       ########  ###      ##     ##      #### ###                #\n"
        "        ######   ###        #   #        ### # ###              ##\n"
        "              ######         # #        #   ### #               ##\n"
        " ###         #######          #        ########                 ##\n"
        "#####         #####          # #        ##     #                  \n"
        "#### ##                    ##   ##        #######               ##\n"
        "#####             #####     #   #         ######                 #\n"
        " ###             #######                                          \n";
    test_mask_field_grow_one(orig2_str, grow2_str, keep2_str);
}

void
test_mask_field_shrink_one(const gchar *orig_str,
                           const gchar *shrink_str,
                           const gchar *bord_str)
{
    GwyMaskField *shrink = mask_field_from_string(shrink_str);
    GwyMaskField *bord = mask_field_from_string(bord_str);
    GwyMaskField *orig;

    orig = mask_field_from_string(orig_str);
    gwy_mask_field_shrink(orig, FALSE);
    test_mask_field_assert_equal(orig, shrink);
    g_object_unref(orig);

    orig = mask_field_from_string(orig_str);
    gwy_mask_field_shrink(orig, TRUE);
    test_mask_field_assert_equal(orig, bord);
    g_object_unref(orig);

    g_object_unref(bord);
    g_object_unref(shrink);
}

void
test_mask_field_shrink(void)
{
    const gchar *orig1_str =
        "### ######\n"
        "##########\n"
        "##########\n"
        "## #### ##\n";
    const gchar *shrink1_str =
        "##   #####\n"
        "### ######\n"
        "## #### ##\n"
        "#   ##   #\n";
    const gchar *bord1_str =
        "          \n"
        " ## ##### \n"
        " # #### # \n"
        "          \n";
    test_mask_field_shrink_one(orig1_str, shrink1_str, bord1_str);

    const gchar *orig2_str =
        "        ######    #        #     #       ########                 \n"
        "       ########  ###      ###   ###      ########                #\n"
        "        ######   ###       ### ###       #########              ##\n"
        "              ######        #####       #########               ##\n"
        " ###         #######         ###       ########                 ##\n"
        "######        #####         #####       ########                 #\n"
        "#######                    ### ###       ########               ##\n"
        "######            #####     #   #         ######                 #\n"
        " ###             #######                                          \n";
    const gchar *shrink2_str =
        "         ####                             ######                  \n"
        "        ######    #        #     #        ######                  \n"
        "                  #         #   #         #######                #\n"
        "                 ##          # #         ######                  #\n"
        "              #####           #         ######                   #\n"
        " ###                         # #         ######                   \n"
        "######                      #   #         ######                 #\n"
        " ###                                                              \n"
        "  #               #####                                           \n";
    const gchar *bord2_str =
        "                                                                  \n"
        "        ######    #        #     #        ######                  \n"
        "                  #         #   #         #######                 \n"
        "                 ##          # #         ######                   \n"
        "              #####           #         ######                    \n"
        " ###                         # #         ######                   \n"
        " #####                      #   #         ######                  \n"
        " ###                                                              \n"
        "                                                                  \n";
    test_mask_field_shrink_one(orig2_str, shrink2_str, bord2_str);
}

GwyMaskField*
random_mask_field(guint xres, guint yres, GRand *rng)
{
    GwyMaskField *field = gwy_mask_field_new_sized(xres, yres, FALSE);
    for (guint i = 0; i < yres*field->stride; i++)
        field->data[i] = g_rand_int(rng);
    return field;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
