/*
 *  $Id$
 *  Copyright (C) 2009-2011 David Nečas (Yeti).
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
#include <stdio.h>
#include "testlibgwy.h"

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
test_mask_field_stride(void)
{
    for (guint i = 1; i <= 133; i++) {
        GwyMaskField *maskfield = gwy_mask_field_new_sized(i, 1, FALSE);
        g_assert_cmpuint(32*maskfield->stride, >=, i);
        g_assert_cmpuint(maskfield->stride % 2, ==, 0);
        g_object_unref(maskfield);
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

void
test_mask_field_data_changed(void)
{
    // Plain emission
    GwyMaskField *field = gwy_mask_field_new();
    guint counter = 0;
    g_signal_connect_swapped(field, "data-changed",
                             G_CALLBACK(record_signal), &counter);
    gwy_mask_field_data_changed(field, NULL);
    g_assert_cmpuint(counter, ==, 1);
    g_object_unref(field);

    // Specified part argument
    field = gwy_mask_field_new_sized(8, 8, FALSE);
    GwyFieldPart fpart = { 1, 2, 3, 4 };
    g_signal_connect_swapped(field, "data-changed",
                             G_CALLBACK(field_part_assert_equal), &fpart);
    gwy_mask_field_data_changed(field, &fpart);
    g_object_unref(field);

    // NULL part argument
    field = gwy_mask_field_new_sized(2, 3, FALSE);
    g_signal_connect_swapped(field, "data-changed",
                             G_CALLBACK(field_part_assert_equal), NULL);
    gwy_mask_field_data_changed(field, NULL);
    g_object_unref(field);
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
    guint32 *data = maskfield->data;
    memcpy(data, pool + offset, required*sizeof(guint32));
    gwy_mask_field_invalidate(maskfield);
}

static void
mask_field_assert_equal(const GwyMaskField *result,
                        const GwyMaskField *reference)
{
    g_assert(GWY_IS_MASK_FIELD(result));
    g_assert(GWY_IS_MASK_FIELD(reference));
    g_assert(result->xres == reference->xres);
    g_assert(result->yres == reference->yres);
    g_assert(result->stride == reference->stride);
    compare_properties(G_OBJECT(result), G_OBJECT(reference));

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
            g_assert_cmphex(result_row[j], ==, reference_row[j]);
        if (end) {
            guint j = result->xres/32;
            g_assert_cmphex((result_row[j] & m), ==, (reference_row[j] & m));
        }
    }
}

static void
mask_field_assert_equal_object(GObject *object, GObject *reference)
{
    mask_field_assert_equal(GWY_MASK_FIELD(object), GWY_MASK_FIELD(reference));
}

void
test_mask_field_copy(void)
{
    enum { max_size = 600 };
    GRand *rng = g_rand_new_with_seed(42);
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
        gwy_mask_field_copy_full(reference, dest);
        guint width = g_rand_int_range(rng, 0, MAX(sxres, dxres));
        guint height = g_rand_int_range(rng, 0, MAX(syres, dyres));
        guint col = g_rand_int_range(rng, 0, sxres);
        guint row = g_rand_int_range(rng, 0, syres);
        guint destcol = g_rand_int_range(rng, 0, dxres);
        guint destrow = g_rand_int_range(rng, 0, dyres);
        GwyFieldPart fpart = { col, row, width, height };
        gwy_mask_field_copy(source, &fpart, dest, destcol, destrow);
        mask_field_part_copy_dumb(source, col, row, width, height,
                                  reference, destcol, destrow);
        mask_field_assert_equal(dest, reference);
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
    enum { max_size = 433 };
    GRand *rng = g_rand_new_with_seed(42);
    guint32 *pool = mask_field_random_pool_new(rng, max_size);
    gsize niter = g_test_slow() ? 10000 : 1000;

    for (gsize iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size/4);
        GwyMaskField *source = gwy_mask_field_new_sized(xres, yres, FALSE);
        mask_field_randomize(source, pool, max_size, rng);
        guint width = g_rand_int_range(rng, 1, xres+1);
        guint height = g_rand_int_range(rng, 1, yres+1);
        guint col = g_rand_int_range(rng, 0, xres-width+1);
        guint row = g_rand_int_range(rng, 0, yres-height+1);
        GwyFieldPart fpart = { col, row, width, height };
        GwyMaskField *part = gwy_mask_field_new_part(source, &fpart);
        GwyMaskField *reference = gwy_mask_field_new_sized(width, height,
                                                           FALSE);
        mask_field_part_copy_dumb(source, col, row, width, height,
                                  reference, 0, 0);
        mask_field_assert_equal(part, reference);
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
    GRand *rng = g_rand_new_with_seed(42);
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
            gwy_mask_field_copy_full(source, reference);
            gwy_mask_field_copy_full(source, dest);
            mask_field_logical_dumb(reference, operand, NULL, op);
            gwy_mask_field_logical(dest, operand, NULL, op);
            mask_field_assert_equal(dest, reference);

            gwy_mask_field_copy_full(source, reference);
            gwy_mask_field_copy_full(source, dest);
            mask_field_logical_dumb(reference, operand, mask, op);
            gwy_mask_field_logical(dest, operand, mask, op);
            mask_field_assert_equal(dest, reference);
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
mask_field_part_logical_dumb(GwyMaskField *maskfield,
                             guint col, guint row, guint width, guint height,
                             const GwyMaskField *operand,
                             guint opcol, guint oprow,
                             GwyLogicalOperator op)
{
    gboolean results[4] = { op & 8, op & 4, op & 2, op & 1 };

    for (guint i = 0; i < height; i++) {
        if (i + row >= maskfield->yres || i + oprow >= operand->yres)
            continue;
        for (guint j = 0; j < width; j++) {
            if (j + col >= maskfield->xres || j + opcol >= operand->xres)
                continue;
            guint a = !!gwy_mask_field_get(maskfield, j + col, i + row);
            guint b = !!gwy_mask_field_get(operand, j + opcol, i + oprow);
            guint res = results[2*a + b];
            gwy_mask_field_set(maskfield, col + j, row + i, res);
        }
    }
}

void
test_mask_field_logical_part(void)
{
    enum { max_size = 333 };
    GRand *rng = g_rand_new_with_seed(42);
    guint32 *pool = mask_field_random_pool_new(rng, max_size);
    gsize niter = g_test_slow() ? 5000 : 1000;

    for (guint iter = 0; iter < niter; iter++) {
        guint sxres = g_rand_int_range(rng, 1, max_size);
        guint syres = g_rand_int_range(rng, 1, max_size/4);
        guint oxres = g_rand_int_range(rng, 1, max_size);
        guint oyres = g_rand_int_range(rng, 1, max_size/4);
        GwyMaskField *source = gwy_mask_field_new_sized(sxres, syres, FALSE);
        GwyMaskField *operand = gwy_mask_field_new_sized(oxres, oyres, FALSE);
        GwyMaskField *dest = gwy_mask_field_new_sized(sxres, syres, FALSE);
        GwyMaskField *reference = gwy_mask_field_new_sized(sxres, syres, FALSE);
        guint width = g_rand_int_range(rng, 0, MAX(sxres, oxres));
        guint height = g_rand_int_range(rng, 0, MAX(syres, oyres));
        guint col = g_rand_int_range(rng, 0, sxres);
        guint row = g_rand_int_range(rng, 0, syres);
        guint opcol = g_rand_int_range(rng, 0, oxres);
        guint oprow = g_rand_int_range(rng, 0, oyres);

        mask_field_randomize(source, pool, max_size, rng);
        mask_field_randomize(operand, pool, max_size, rng);
        for (GwyLogicalOperator op = GWY_LOGICAL_ZERO;
             op <= GWY_LOGICAL_ONE;
             op++) {
            gwy_mask_field_copy_full(source, dest);
            gwy_mask_field_copy_full(source, reference);
            GwyFieldPart fpart = { col, row, width, height };
            gwy_mask_field_part_logical(dest, &fpart,
                                        operand, opcol, oprow, op);
            mask_field_part_logical_dumb(reference, col, row, width, height,
                                         operand, opcol, oprow, op);
            mask_field_assert_equal(dest, reference);
        }

        g_object_unref(source);
        g_object_unref(dest);
        g_object_unref(operand);
        g_object_unref(reference);
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
    GRand *rng = g_rand_new_with_seed(42);
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
        gwy_mask_field_copy_full(reference, dest);

        GwyFieldPart fpart = { col, row, width, height };
        gwy_mask_field_fill(dest, &fpart, FALSE);
        mask_field_part_fill_dumb(reference,  col, row, width, height, FALSE);
        mask_field_assert_equal(dest, reference);

        mask_field_randomize(reference, pool, max_size, rng);
        gwy_mask_field_copy_full(reference, dest);

        gwy_mask_field_fill(dest, &fpart, TRUE);
        mask_field_part_fill_dumb(reference,  col, row, width, height, TRUE);
        mask_field_assert_equal(dest, reference);

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
    GRand *rng = g_rand_new_with_seed(42);
    guint32 *pool = mask_field_random_pool_new(rng, max_size);
    gsize niter = g_test_slow() ? 50 : 10;

    for (guint iter = 0; iter < niter; iter++) {
        guint width = g_rand_int_range(rng, 1, max_size);
        guint height = g_rand_int_range(rng, 1, max_size/4);
        GwyMaskField *original = gwy_mask_field_new_sized(width, height, FALSE);
        mask_field_randomize(original, pool, max_size, rng);
        GwyMaskField *copy;

        serializable_duplicate(GWY_SERIALIZABLE(original),
                               mask_field_assert_equal_object);
        serializable_assign(GWY_SERIALIZABLE(original),
                            mask_field_assert_equal_object);
        copy = GWY_MASK_FIELD(serialize_and_back(G_OBJECT(original),
                                                 mask_field_assert_equal_object));
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

static void
mask_field_check_part_good(guint xres, guint yres,
                      const GwyFieldPart *fpart,
                      guint expected_col, guint expected_row,
                      guint expected_width, guint expected_height)
{
    GwyMaskField *mask_field = gwy_mask_field_new_sized(xres, yres, FALSE);
    guint col, row, width, height;

    g_assert(gwy_mask_field_check_part(mask_field, fpart, &col, &row, &width, &height));
    g_assert_cmpuint(col, ==, expected_col);
    g_assert_cmpuint(row, ==, expected_row);
    g_assert_cmpuint(width, ==, expected_width);
    g_assert_cmpuint(height, ==, expected_height);
    g_object_unref(mask_field);
}

void
test_mask_field_check_part_good(void)
{
    mask_field_check_part_good(17, 25, &(GwyFieldPart){ 0, 0, 17, 25 },
                          0, 0, 17, 25);
    mask_field_check_part_good(17, 25, NULL,
                          0, 0, 17, 25);
    mask_field_check_part_good(17, 25, &(GwyFieldPart){ 0, 0, 3, 24 },
                          0, 0, 3, 24);
    mask_field_check_part_good(17, 25, &(GwyFieldPart){ 16, 20, 1, 4 },
                          16, 20, 1, 4);
}

static void
mask_field_check_part_empty(guint xres, guint yres,
                       const GwyFieldPart *fpart)
{
    GwyMaskField *mask_field = gwy_mask_field_new_sized(xres, yres, FALSE);
    guint col, row, width, height;

    g_assert(!gwy_mask_field_check_part(mask_field, fpart, &col, &row, &width, &height));
    g_object_unref(mask_field);
}

void
test_mask_field_check_part_empty(void)
{
    mask_field_check_part_empty(17, 25, &(GwyFieldPart){ 0, 0, 0, 0 });
    mask_field_check_part_empty(17, 25, &(GwyFieldPart){ 17, 25, 0, 0 });
    mask_field_check_part_empty(17, 25, &(GwyFieldPart){ 1000, 1000, 0, 0 });
}

static void
mask_field_check_part_bad(guint xres, guint yres,
                     const GwyFieldPart *fpart)
{
    if (g_test_trap_fork(0,
                         G_TEST_TRAP_SILENCE_STDOUT
                         | G_TEST_TRAP_SILENCE_STDERR)) {
        GwyMaskField *mask_field = gwy_mask_field_new_sized(xres, yres, FALSE);
        guint col, row, width, height;
        gwy_mask_field_check_part(mask_field, fpart, &col, &row, &width, &height);
        exit(0);
    }
    g_test_trap_assert_failed();
    g_test_trap_assert_stderr("*CRITICAL*");
}

void
test_mask_field_check_part_bad(void)
{
    mask_field_check_part_bad(17, 25, &(GwyFieldPart){ 0, 0, 18, 1 });
    mask_field_check_part_bad(17, 25, &(GwyFieldPart){ 0, 0, 1, 26 });
    mask_field_check_part_bad(17, 25, &(GwyFieldPart){ 17, 0, 1, 1 });
    mask_field_check_part_bad(17, 25, &(GwyFieldPart){ 0, 25, 1, 1 });
}

void
test_mask_field_grain_numbers(void)
{
    enum { max_size = 83 };
    GRand *rng = g_rand_new_with_seed(42);
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
    GRand *rng = g_rand_new_with_seed(42);
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
        GwyFieldPart fpart = { col, row, width, height };

        g_assert_cmpuint(gwy_mask_field_part_count(field, &fpart,
                                                   FALSE),
                         ==, mask_field_count_dumb(field, NULL,
                                                   col, row, width, height,
                                                   FALSE));
        g_assert_cmpuint(gwy_mask_field_part_count(field, &fpart,
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

void
test_mask_field_count_rows(void)
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
        mask_field_randomize(field, pool, max_size, rng);

        guint width = g_rand_int_range(rng, 1, xres+1);
        guint height = g_rand_int_range(rng, 1, yres+1);
        guint col = g_rand_int_range(rng, 0, xres-width+1);
        guint row = g_rand_int_range(rng, 0, yres-height+1);
        GwyFieldPart fpart = { col, row, width, height };
        guint *counts = g_new(guint, height);

        gwy_mask_field_count_rows(field, &fpart, TRUE, counts);
        for (guint i = 0; i < height; i++) {
            g_assert_cmpuint(counts[i],
                             ==, mask_field_count_dumb(field, NULL,
                                                       col, row + i, width, 1,
                                                       TRUE));
        }

        gwy_mask_field_count_rows(field, &fpart, FALSE, counts);
        for (guint i = 0; i < height; i++) {
            g_assert_cmpuint(counts[i],
                             ==, mask_field_count_dumb(field, NULL,
                                                       col, row + i, width, 1,
                                                       FALSE));
        }

        g_free(counts);
        g_object_unref(field);
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
    //GwyMaskField *field = gwy_mask_field_new_sized(width, height, FALSE);
    // XXX: Bad for testing, but good for visualization:
    GwyMaskField *field = gwy_mask_field_new_sized(width, height, TRUE);
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
    mask_field_assert_equal(orig, grow);
    g_object_unref(orig);

    orig = mask_field_from_string(orig_str);
    gwy_mask_field_grow(orig, TRUE);
    mask_field_assert_equal(orig, keep);
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
    mask_field_assert_equal(orig, shrink);
    g_object_unref(orig);

    orig = mask_field_from_string(orig_str);
    gwy_mask_field_shrink(orig, TRUE);
    mask_field_assert_equal(orig, bord);
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

static void
test_mask_field_flip_one(const gchar *orig_str,
                         gboolean horizontally, gboolean vertically,
                         const gchar *reference_str)
{
    GwyMaskField *field = mask_field_from_string(orig_str);
    gwy_mask_field_flip(field, horizontally, vertically);
    GwyMaskField *reference = mask_field_from_string(reference_str);
    mask_field_assert_equal(field, reference);
    g_object_unref(field);
    g_object_unref(reference);
}

void
test_mask_field_flip(void)
{
    // Even/odd
    const gchar *orig1_str =
        "####\n"
        "#   \n"
        "#   \n";
    const gchar *hflip1_str =
        "####\n"
        "   #\n"
        "   #\n";
    const gchar *vflip1_str =
        "#   \n"
        "#   \n"
        "####\n";
    const gchar *bflip1_str =
        "   #\n"
        "   #\n"
        "####\n";

    test_mask_field_flip_one(orig1_str, FALSE, FALSE, orig1_str);
    test_mask_field_flip_one(orig1_str, TRUE, FALSE, hflip1_str);
    test_mask_field_flip_one(orig1_str, FALSE, TRUE, vflip1_str);
    test_mask_field_flip_one(orig1_str, TRUE, TRUE, bflip1_str);

    // Odd/even
    const gchar *orig2_str =
        "###\n"
        "#  \n"
        "#  \n"
        "#  \n";
    const gchar *hflip2_str =
        "###\n"
        "  #\n"
        "  #\n"
        "  #\n";
    const gchar *vflip2_str =
        "#  \n"
        "#  \n"
        "#  \n"
        "###\n";
    const gchar *bflip2_str =
        "  #\n"
        "  #\n"
        "  #\n"
        "###\n";

    test_mask_field_flip_one(orig2_str, FALSE, FALSE, orig2_str);
    test_mask_field_flip_one(orig2_str, TRUE, FALSE, hflip2_str);
    test_mask_field_flip_one(orig2_str, FALSE, TRUE, vflip2_str);
    test_mask_field_flip_one(orig2_str, TRUE, TRUE, bflip2_str);

    enum { max_size = 161 };
    GRand *rng = g_rand_new_with_seed(42);
    guint32 *pool = mask_field_random_pool_new(rng, max_size);
    gsize niter = g_test_slow() ? 500 : 100;

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size/4);
        GwyMaskField *source = gwy_mask_field_new_sized(xres, yres, FALSE);
        mask_field_randomize(source, pool, max_size, rng);
        GwyMaskField *dest = gwy_mask_field_duplicate(source);

        gwy_mask_field_flip(dest, FALSE, FALSE);
        mask_field_assert_equal(source, dest);

        gwy_mask_field_flip(dest, TRUE, FALSE);
        gwy_mask_field_flip(dest, TRUE, FALSE);
        mask_field_assert_equal(source, dest);

        gwy_mask_field_flip(dest, FALSE, TRUE);
        gwy_mask_field_flip(dest, FALSE, TRUE);
        mask_field_assert_equal(source, dest);

        gwy_mask_field_flip(dest, TRUE, TRUE);
        gwy_mask_field_flip(dest, TRUE, TRUE);
        mask_field_assert_equal(source, dest);

        gwy_mask_field_flip(dest, TRUE, FALSE);
        gwy_mask_field_flip(dest, FALSE, TRUE);
        gwy_mask_field_flip(dest, TRUE, FALSE);
        gwy_mask_field_flip(dest, FALSE, TRUE);
        mask_field_assert_equal(source, dest);

        g_object_unref(dest);
        g_object_unref(source);
    }
    mask_field_random_pool_free(pool);
    g_rand_free(rng);
}

static void
test_mask_field_transpose_one(const gchar *orig_str,
                              const gchar *reference_str)
{
    GwyMaskField *field = mask_field_from_string(orig_str);
    GwyMaskField *transposed = gwy_mask_field_new_transposed(field, NULL);
    g_object_unref(field);
    GwyMaskField *reference = mask_field_from_string(reference_str);
    mask_field_assert_equal(transposed, reference);
    g_object_unref(transposed);
    g_object_unref(reference);
}

void
test_mask_field_transpose(void)
{
    const gchar *orig1_str =
        "####\n"
        "#   \n"
        "#   \n";
    const gchar *trans1_str =
        "###\n"
        "#  \n"
        "#  \n"
        "#  \n";

    test_mask_field_transpose_one(orig1_str, trans1_str);
    test_mask_field_transpose_one(trans1_str, orig1_str);

    const gchar *orig2_str =
        "#################################\n"
        "#                                \n"
        "#                                \n"
        "# # # # # # # # # # # # # # # # #\n";
    const gchar *trans2_str =
        "####\n"
        "#   \n"
        "#  #\n"
        "#   \n"
        "#  #\n"
        "#   \n"
        "#  #\n"
        "#   \n"
        "#  #\n"
        "#   \n"
        "#  #\n"
        "#   \n"
        "#  #\n"
        "#   \n"
        "#  #\n"
        "#   \n"
        "#  #\n"
        "#   \n"
        "#  #\n"
        "#   \n"
        "#  #\n"
        "#   \n"
        "#  #\n"
        "#   \n"
        "#  #\n"
        "#   \n"
        "#  #\n"
        "#   \n"
        "#  #\n"
        "#   \n"
        "#  #\n"
        "#   \n"
        "#  #\n";

    test_mask_field_transpose_one(orig2_str, trans2_str);
    test_mask_field_transpose_one(trans2_str, orig2_str);

    enum { max_size = 149 };
    GRand *rng = g_rand_new_with_seed(42);
    guint32 *pool = mask_field_random_pool_new(rng, max_size);
    gsize niter = g_test_slow() ? 100000 : 2000;

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size/4);
        GwyMaskField *source = gwy_mask_field_new_sized(xres, yres, FALSE);
        mask_field_randomize(source, pool, max_size, rng);
        guint width = g_rand_int_range(rng, 1, xres+1);
        guint height = g_rand_int_range(rng, 1, yres+1);
        guint col = g_rand_int_range(rng, 0, xres-width+1);
        guint row = g_rand_int_range(rng, 0, yres-height+1);
        GwyMaskField *dest = gwy_mask_field_duplicate(source);
        GwyFieldPart fpart = { col, row, width, height };
        GwyMaskField *part = gwy_mask_field_new_transposed(source, &fpart);
        fpart = (GwyFieldPart){0, 0, height, width};
        gwy_mask_field_transpose(part, &fpart, dest, col, row);
        mask_field_assert_equal(source, dest);

        g_object_unref(part);
        g_object_unref(dest);
        g_object_unref(source);
    }
    mask_field_random_pool_free(pool);
    g_rand_free(rng);
}

static void
test_mask_field_rotate_one(const gchar *orig_str,
                           GwySimpleRotation rotation,
                           const gchar *reference_str)
{
    GwyMaskField *field = mask_field_from_string(orig_str);
    GwyMaskField *rotated = gwy_mask_field_new_rotated_simple(field, rotation);
    g_object_unref(field);
    GwyMaskField *reference = mask_field_from_string(reference_str);
    mask_field_assert_equal(rotated, reference);
    g_object_unref(rotated);
    g_object_unref(reference);
}

void
test_mask_field_rotate(void)
{
    // Even/odd
    const gchar *orig1_str =
        "####\n"
        "#   \n"
        "#   \n";
    const gchar *r90d1_str =
        "#  \n"
        "#  \n"
        "#  \n"
        "###\n";
    const gchar *r180d1_str =
        "   #\n"
        "   #\n"
        "####\n";
    const gchar *r270d1_str =
        "###\n"
        "  #\n"
        "  #\n"
        "  #\n";

    test_mask_field_rotate_one(orig1_str, 0, orig1_str);
    test_mask_field_rotate_one(orig1_str, 90, r90d1_str);
    test_mask_field_rotate_one(orig1_str, 180, r180d1_str);
    test_mask_field_rotate_one(orig1_str, 270, r270d1_str);

    test_mask_field_rotate_one(r90d1_str, 0, r90d1_str);
    test_mask_field_rotate_one(r90d1_str, 90, r180d1_str);
    test_mask_field_rotate_one(r90d1_str, 180, r270d1_str);
    test_mask_field_rotate_one(r90d1_str, 270, orig1_str);

    test_mask_field_rotate_one(r180d1_str, 0, r180d1_str);
    test_mask_field_rotate_one(r180d1_str, 90, r270d1_str);
    test_mask_field_rotate_one(r180d1_str, 180, orig1_str);
    test_mask_field_rotate_one(r180d1_str, 270, r90d1_str);

    test_mask_field_rotate_one(r270d1_str, 0, r270d1_str);
    test_mask_field_rotate_one(r270d1_str, 90, orig1_str);
    test_mask_field_rotate_one(r270d1_str, 180, r90d1_str);
    test_mask_field_rotate_one(r270d1_str, 270, r180d1_str);

    enum { max_size = 161 };
    GRand *rng = g_rand_new_with_seed(42);
    guint32 *pool = mask_field_random_pool_new(rng, max_size);
    gsize niter = g_test_slow() ? 5000 : 1000;

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size/4);
        GwyMaskField *source = gwy_mask_field_new_sized(xres, yres, FALSE);
        mask_field_randomize(source, pool, max_size, rng);
        GwyMaskField *rotccw = gwy_mask_field_new_rotated_simple(source, 90);
        GwyMaskField *rotcw = gwy_mask_field_new_rotated_simple(source, 270);
        gwy_mask_field_flip(rotcw, TRUE, TRUE);
        mask_field_assert_equal(rotcw, rotccw);
        g_object_unref(rotcw);
        g_object_unref(rotccw);
        g_object_unref(source);
    }
    mask_field_random_pool_free(pool);
    g_rand_free(rng);
}

void
test_mask_field_fill_ellipse(void)
{
    enum { max_size = 214 };
    GRand *rng = g_rand_new_with_seed(42);
    guint32 *pool = mask_field_random_pool_new(rng, max_size);
    gsize niter = g_test_slow() ? 10000 : 2000;

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size);
        GwyMaskField *ellipse = gwy_mask_field_new_sized(xres, yres, FALSE);
        GwyMaskField *reference = gwy_mask_field_new_sized(xres, yres, FALSE);
        gwy_mask_field_fill_ellipse(ellipse, NULL, TRUE, TRUE);
        // Fill with the other value and invert
        gwy_mask_field_fill_ellipse(reference, NULL, TRUE, FALSE);
        gwy_mask_field_logical(reference, NULL, NULL, GWY_LOGICAL_NA);
        mask_field_assert_equal(ellipse, reference);
        // Check symmetry
        gwy_mask_field_flip(reference, TRUE, FALSE);
        mask_field_assert_equal(ellipse, reference);
        gwy_mask_field_flip(reference, FALSE, TRUE);
        mask_field_assert_equal(ellipse, reference);
        g_object_unref(reference);
        g_object_unref(ellipse);
    }
    mask_field_random_pool_free(pool);
    g_rand_free(rng);
}

GwyMaskField*
random_mask_field(guint xres, guint yres, GRand *rng)
{
    GwyMaskField *field = gwy_mask_field_new_sized(xres, yres, FALSE);
    for (guint i = 0; i < yres*field->stride; i++)
        field->data[i] = g_rand_int(rng);
    return field;
}

GwyMaskField*
random_mask_field_prob(guint xres, guint yres, GRand *rng,
                       gdouble probability)
{
    GwyMaskField *field = gwy_mask_field_new_sized(xres, yres, FALSE);
    for (guint i = 0; i < field->yres; i++) {
        GwyMaskIter iter;
        gwy_mask_field_iter_init(field, iter, 0, i);
        for (guint j = 0; j < field->xres; j++) {
            gboolean bit = g_rand_double(rng) < probability;
            gwy_mask_iter_set(iter, bit);
            gwy_mask_iter_next(iter);
        }
    }
    return field;
}

// FIXME: This is just a weak consistency check.  How to verify the sizes
// independently?
void
test_mask_field_grain_sizes(void)
{
    enum { max_size = 208, niter = 3000 };
    GRand *rng = g_rand_new_with_seed(42);

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size);
        gdouble prob = g_rand_double(rng);
        GwyMaskField *field = random_mask_field_prob(xres, yres, rng, prob);
        guint ngrains;

        gwy_mask_field_number_grains(field, &ngrains);
        const guint *grain_sizes = gwy_mask_field_grain_sizes(field);

        guint total = 0;
        for (guint i = 0; i <= ngrains; i++) {
            g_assert(!i || grain_sizes[i]);
            total += grain_sizes[i];
        }
        g_assert_cmpuint(total, ==, xres*yres);

        guint *grain_sizes_prev = g_memdup(grain_sizes,
                                           (ngrains + 1)*sizeof(guint));
        gwy_mask_field_invalidate(field);
        grain_sizes = gwy_mask_field_grain_sizes(field);

        for (guint i = 0; i <= ngrains; i++) {
            g_assert_cmpuint(grain_sizes[i], ==, grain_sizes_prev[i]);
        }

        g_free(grain_sizes_prev);
        g_object_unref(field);
    }
    g_rand_free(rng);
}

void
test_mask_field_grain_bounding_boxes(void)
{
    enum { max_size = 208, niter = 3000 };
    GRand *rng = g_rand_new_with_seed(42);

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size);
        gdouble prob = g_rand_double(rng);
        GwyMaskField *field = random_mask_field_prob(xres, yres, rng, prob);
        guint ngrains;

        const guint *grains = gwy_mask_field_number_grains(field, &ngrains);
        const GwyFieldPart *grain_bboxes = gwy_mask_field_grain_bounding_boxes(field);

        for (guint i = 0; i <= ngrains; i++) {
            const GwyFieldPart *bbox = grain_bboxes + i;

            if (!bbox->width) {
                g_assert_cmpuint(i, ==, 0);
                g_assert_cmpuint(bbox->height, ==, 0);
                g_assert_cmpuint(bbox->col, ==, 0);
                g_assert_cmpuint(bbox->row, ==, 0);
            }
            else {
                g_assert_cmpuint(bbox->height, >, 0);
                g_assert_cmpuint(bbox->width, <=, xres);
                g_assert_cmpuint(bbox->height, <=, yres);
                g_assert_cmpuint(bbox->col, <, xres);
                g_assert_cmpuint(bbox->row, <, yres);
                g_assert_cmpuint(bbox->col + bbox->width, <=, xres);
                g_assert_cmpuint(bbox->row + bbox->height, <=, yres);
            }
        }

        guint *found_edge = g_new0(guint, ngrains+1);

        for (guint i = 0; i < yres; i++) {
            for (guint j = 0; j < xres; j++) {
                guint gno = grains[i*xres + j];
                const GwyFieldPart *bbox = grain_bboxes + gno;

                if (bbox->width) {
                    g_assert_cmpuint(j, >=, bbox->col);
                    g_assert_cmpuint(j, <, bbox->col + bbox->width);
                    g_assert_cmpuint(i, >=, bbox->row);
                    g_assert_cmpuint(i, <, bbox->row + bbox->height);
                    if (j == bbox->col)
                        found_edge[gno] |= 1 << 0;
                    if (i == bbox->row)
                        found_edge[gno] |= 1 << 1;
                    if (j == bbox->col + bbox->width-1)
                        found_edge[gno] |= 1 << 2;
                    if (i == bbox->row + bbox->height-1)
                        found_edge[gno] |= 1 << 3;
                }
            }
        }

        for (guint i = 0; i <= ngrains; i++) {
            const GwyFieldPart *bbox = grain_bboxes + i;

            if (bbox->width) {
                g_assert_cmphex(found_edge[i], ==, 0xf);
            }
        }

        g_free(found_edge);
        g_object_unref(field);
    }
    g_rand_free(rng);
}

// Undef macros to test the exported functions.
#undef gwy_mask_field_get
#undef gwy_mask_field_set

void
test_mask_field_get(void)
{
    enum { max_size = 55, niter = 40 };

    GRand *rng = g_rand_new_with_seed(42);
    guint32 *pool = mask_field_random_pool_new(rng, max_size);

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size);
        GwyMaskField *field = gwy_mask_field_new_sized(xres, yres, FALSE);
        mask_field_randomize(field, pool, max_size, rng);

        for (guint i = 0; i < yres; i++) {
            GwyMaskIter miter;
            gwy_mask_field_iter_init(field, miter, 0, i);
            for (guint j = 0; j < xres; j++) {
                g_assert_cmpuint(!gwy_mask_iter_get(miter),
                                 ==,
                                 !gwy_mask_field_get(field, j, i));
                gwy_mask_iter_next(miter);
            }
        }
        g_object_unref(field);
    }

    mask_field_random_pool_free(pool);
    g_rand_free(rng);
}

void
test_mask_field_set(void)
{
    enum { max_size = 55, niter = 40 };

    GRand *rng = g_rand_new_with_seed(42);

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size);
        GwyMaskField *field = gwy_mask_field_new_sized(xres, yres, FALSE);

        for (guint i = 0; i < yres; i++) {
            for (guint j = 0; j < xres; j++)
                gwy_mask_field_set(field, j, i, (i*j + i/3 + j/5) % 2);
        }

        for (guint i = 0; i < yres; i++) {
            GwyMaskIter miter;
            gwy_mask_field_iter_init(field, miter, 0, i);
            for (guint j = 0; j < xres; j++) {
                g_assert_cmpuint(!gwy_mask_iter_get(miter),
                                 ==,
                                 !((i*j + i/3 + j/5) % 2));
                gwy_mask_iter_next(miter);
            }
        }
        g_object_unref(field);
    }

    g_rand_free(rng);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
