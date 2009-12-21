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
            putchar(gwy_mask_field_get(maskfield, j, i) ? '1' : '0');
        putchar('\n');
    }
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
    guint n = max_size*(max_size + 31)/32 * 8;
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
    guint n = max_size*(max_size + 31)/32 * 8;
    guint offset = g_rand_int_range(rng, 0, n - required);
    guint32 *data = gwy_mask_field_get_data(maskfield);
    memcpy(data, pool + offset, required*sizeof(guint32));
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
        guint n = reference->stride * reference->yres;
        guint width = g_rand_int_range(rng, 0, MAX(sxres, dxres));
        guint height = g_rand_int_range(rng, 0, MAX(syres, dyres));
        guint row = g_rand_int_range(rng, 0, sxres);
        guint col = g_rand_int_range(rng, 0, syres);
        guint destrow = g_rand_int_range(rng, 0, dxres);
        guint destcol = g_rand_int_range(rng, 0, dyres);
        gwy_mask_field_part_copy(source, col, row, width, height,
                                 dest, destcol, destrow);
        mask_field_part_copy_dumb(source, col, row, width, height,
                                  reference, destcol, destrow);
        g_assert_cmpint(memcmp(dest->data, reference->data, n*sizeof(guint32)),
                        ==, 0);
        g_object_unref(source);
        g_object_unref(dest);
        g_object_unref(reference);
    }
    mask_field_random_pool_free(pool);
    g_rand_free(rng);
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

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
