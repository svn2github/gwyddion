/*
 *  $Id$
 *  Copyright (C) 2013 David Nečas (Yeti).
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
 * Field correlations
 *
 ***************************************************************************/


static gdouble
find_max(const GwyField *field,
         guint *col, guint *row)
{
    gdouble max = -G_MAXDOUBLE;

    *col = *row = 0;
    for (guint k = 0; k < field->xres*field->yres; k++) {
        if (field->data[k] > max) {
            max = field->data[k];
            *col = k % field->xres;
            *row = k/field->xres;
        }
    }
    return max;
}

void
test_field_correlate_plain(void)
{
    enum { max_size = 54, niter = 40 };

    GRand *rng = g_rand_new_with_seed(42);

    for (guint iter = 0; iter < niter; iter++) {
        // With too small kernels we can get higher correlation score elsewhere
        // by a mere chance (at least without normalising).
        guint xres = g_rand_int_range(rng, 8, max_size);
        guint yres = g_rand_int_range(rng, 8, max_size);
        guint kxres = g_rand_int_range(rng, 4, xres/2+1);
        guint kyres = g_rand_int_range(rng, 4, yres/2+1);
        guint col = g_rand_int_range(rng, 0, xres-kxres+1);
        guint row = g_rand_int_range(rng, 0, yres-kyres+1);
        guint kx0 = (kxres - 1)/2;
        guint ky0 = (kyres - 1)/2;

        GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
        field_randomize(field, rng);
        gwy_field_normalize(field, NULL, NULL, GWY_MASK_IGNORE, 0.0, 1.0,
                            GWY_NORMALIZE_MEAN | GWY_NORMALIZE_RMS);
        GwyField *kernel = gwy_field_new_sized(kxres, kyres, FALSE);
        field_randomize(kernel, rng);
        gwy_field_normalize(kernel, NULL, NULL, GWY_MASK_IGNORE, 0.0, 1.0,
                            GWY_NORMALIZE_MEAN | GWY_NORMALIZE_RMS);
        gwy_field_copy(kernel, NULL, field, col, row);
        GwyField *score = gwy_field_new_alike(field, FALSE);

        gwy_field_correlate(field, NULL, score, kernel, NULL,
                            0,
                            GWY_EXTERIOR_BORDER_EXTEND, 0.0);

        guint scol, srow;
        gdouble maxscore = find_max(score, &scol, &srow);
        g_assert_cmpuint(scol, ==, col + kx0);
        g_assert_cmpuint(srow, ==, row + ky0);
        g_assert_cmpfloat(maxscore, >=, 0.999);
        g_assert_cmpfloat(maxscore, <, 1.001);

        g_object_unref(score);
        g_object_unref(kernel);
        g_object_unref(field);
    }
    g_rand_free(rng);
}

void
test_field_correlate_level(void)
{
    enum { max_size = 54, niter = 40 };

    GRand *rng = g_rand_new_with_seed(42);

    for (guint iter = 0; iter < niter; iter++) {
        // With too small kernels we can get higher correlation score elsewhere
        // by a mere chance (at least without normalising).
        guint xres = g_rand_int_range(rng, 8, max_size);
        guint yres = g_rand_int_range(rng, 8, max_size);
        guint kxres = g_rand_int_range(rng, 4, xres/2+1);
        guint kyres = g_rand_int_range(rng, 4, yres/2+1);
        guint col = g_rand_int_range(rng, 0, xres-kxres+1);
        guint row = g_rand_int_range(rng, 0, yres-kyres+1);
        guint kx0 = (kxres - 1)/2;
        guint ky0 = (kyres - 1)/2;

        GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
        field_randomize(field, rng);
        gwy_field_normalize(field, NULL, NULL, GWY_MASK_IGNORE, 0.0, 1.0,
                            GWY_NORMALIZE_MEAN | GWY_NORMALIZE_RMS);
        GwyField *kernel = gwy_field_new_sized(kxres, kyres, FALSE);
        field_randomize(kernel, rng);
        gwy_field_normalize(kernel, NULL, NULL, GWY_MASK_IGNORE, 0.0, 1.0,
                            GWY_NORMALIZE_MEAN | GWY_NORMALIZE_RMS);
        gwy_field_copy(kernel, NULL, field, col, row);
        gwy_field_add_full(field, 20.0*(g_rand_double(rng) - 0.5));
        GwyField *score = gwy_field_new_alike(field, FALSE);

        gwy_field_correlate(field, NULL, score, kernel, NULL,
                            GWY_CORRELATION_LEVEL,
                            GWY_EXTERIOR_BORDER_EXTEND, 0.0);

        guint scol, srow;
        gdouble maxscore = find_max(score, &scol, &srow);
        g_assert_cmpuint(scol, ==, col + kx0);
        g_assert_cmpuint(srow, ==, row + ky0);
        g_assert_cmpfloat(maxscore, >=, 0.999);
        g_assert_cmpfloat(maxscore, <, 1.001);

        g_object_unref(score);
        g_object_unref(kernel);
        g_object_unref(field);
    }
    g_rand_free(rng);
}

void
test_field_correlate_scale_rms(void)
{
    enum { max_size = 54, niter = 40 };

    GRand *rng = g_rand_new_with_seed(42);

    for (guint iter = 0; iter < niter; iter++) {
        // With too small kernels we can get higher correlation score elsewhere
        // by a mere chance (at least without normalising).
        guint xres = g_rand_int_range(rng, 8, max_size);
        guint yres = g_rand_int_range(rng, 8, max_size);
        guint kxres = g_rand_int_range(rng, 4, xres/2+1);
        guint kyres = g_rand_int_range(rng, 4, yres/2+1);
        guint col = g_rand_int_range(rng, 0, xres-kxres+1);
        guint row = g_rand_int_range(rng, 0, yres-kyres+1);
        guint kx0 = (kxres - 1)/2;
        guint ky0 = (kyres - 1)/2;

        GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
        field_randomize(field, rng);
        gwy_field_normalize(field, NULL, NULL, GWY_MASK_IGNORE, 0.0, 1.0,
                            GWY_NORMALIZE_MEAN | GWY_NORMALIZE_RMS);
        GwyField *kernel = gwy_field_new_sized(kxres, kyres, FALSE);
        field_randomize(kernel, rng);
        gwy_field_normalize(kernel, NULL, NULL, GWY_MASK_IGNORE, 0.0, 1.0,
                            GWY_NORMALIZE_MEAN | GWY_NORMALIZE_RMS);
        gwy_field_copy(kernel, NULL, field, col, row);
        gwy_field_multiply_full(field, -log(g_rand_double(rng)));
        GwyField *score = gwy_field_new_alike(field, FALSE);

        gwy_field_correlate(field, NULL, score, kernel, NULL,
                            GWY_CORRELATION_NORMALIZE,
                            GWY_EXTERIOR_BORDER_EXTEND, 0.0);

        guint scol, srow;
        gdouble maxscore = find_max(score, &scol, &srow);
        g_assert_cmpuint(scol, ==, col + kx0);
        g_assert_cmpuint(srow, ==, row + ky0);
        g_assert_cmpfloat(maxscore, >=, 0.999);
        g_assert_cmpfloat(maxscore, <, 1.001);

        g_object_unref(score);
        g_object_unref(kernel);
        g_object_unref(field);
    }
    g_rand_free(rng);
}

void
test_field_correlate_normalize(void)
{
    enum { max_size = 54, niter = 40 };

    GRand *rng = g_rand_new_with_seed(42);

    for (guint iter = 0; iter < niter; iter++) {
        // With too small kernels we can get higher correlation score elsewhere
        // by a mere chance (at least without normalising).
        guint xres = g_rand_int_range(rng, 8, max_size);
        guint yres = g_rand_int_range(rng, 8, max_size);
        guint kxres = g_rand_int_range(rng, 4, xres/2+1);
        guint kyres = g_rand_int_range(rng, 4, yres/2+1);
        guint col = g_rand_int_range(rng, 0, xres-kxres+1);
        guint row = g_rand_int_range(rng, 0, yres-kyres+1);
        guint kx0 = (kxres - 1)/2;
        guint ky0 = (kyres - 1)/2;

        GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
        field_randomize(field, rng);
        gwy_field_normalize(field, NULL, NULL, GWY_MASK_IGNORE, 0.0, 1.0,
                            GWY_NORMALIZE_MEAN | GWY_NORMALIZE_RMS);
        GwyField *kernel = gwy_field_new_sized(kxres, kyres, FALSE);
        field_randomize(kernel, rng);
        gwy_field_normalize(kernel, NULL, NULL, GWY_MASK_IGNORE, 0.0, 1.0,
                            GWY_NORMALIZE_MEAN | GWY_NORMALIZE_RMS);
        gwy_field_copy(kernel, NULL, field, col, row);
        gwy_field_addmul_full(field,
                              -log(g_rand_double(rng)),
                              20.0*(g_rand_double(rng) - 0.5));
        GwyField *score = gwy_field_new_alike(field, FALSE);

        gwy_field_correlate(field, NULL, score, kernel, NULL,
                            GWY_CORRELATION_LEVEL | GWY_CORRELATION_NORMALIZE,
                            GWY_EXTERIOR_BORDER_EXTEND, 0.0);

        guint scol, srow;
        gdouble maxscore = find_max(score, &scol, &srow);
        g_assert_cmpuint(scol, ==, col + kx0);
        g_assert_cmpuint(srow, ==, row + ky0);
        g_assert_cmpfloat(maxscore, >=, 0.999);
        g_assert_cmpfloat(maxscore, <, 1.001);

        g_object_unref(score);
        g_object_unref(kernel);
        g_object_unref(field);
    }
    g_rand_free(rng);
}

static void
field_assert_local_extrema(const GwyField *field,
                           const GwyFieldPart *fpart,
                           const GwyMaskField *mask,
                           GwyMaskingType masking,
                           const guint *expected_indices,
                           guint expected_n,
                           gboolean maxima)
{
    guint *indices = g_new(guint, expected_n+1);

    for (guint ni = 0; ni <= expected_n+1; ni++) {
        // Make failure visible.
        for (guint j = 0; j < expected_n+1; j++)
            indices[j] = G_MAXUINT;

        guint nex = MIN(ni, expected_n);
        guint n = gwy_field_local_extrema(field, fpart, mask, masking,
                                          indices, ni, maxima);
        g_assert_cmpuint(n, ==, nex);
        for (guint j = 0; j < n; j++) {
            //g_printerr("[%u] %u %u\n", j, indices[j] % field->xres, indices[j]/field->xres);
            g_assert_cmpuint(indices[j], ==, expected_indices[j]);
        }
    }

    g_free(indices);
}

static GwyField*
field_local_extrema_create1(gint sign)
{
    enum { xres = 30, yres = 20 };
    GwyField *field = field_make_planar(xres, yres, sign*0.1, sign*0.2);

    gwy_field_index(field, 1, 1) = sign*3.0;
    gwy_field_index(field, 6, 4) = sign*3.0;
    gwy_field_index(field, 6, 5) = sign*3.0;
    gwy_field_index(field, 24, 9) = sign*3.0;
    gwy_field_index(field, 24, 10) = sign*3.0;
    gwy_field_index(field, 25, 10) = sign*3.0;
    gwy_field_index(field, 24, 11) = sign*3.0;

    return field;
}

void
test_field_local_extrema_max_full(void)
{
    GwyField *field = field_local_extrema_create1(1);
    guint indices[] = {
        10*field->xres + 24,
        10*field->xres + 25,
        11*field->xres + 24,
        9*field->xres + 24,
        5*field->xres + 6,
        4*field->xres + 6,
        1*field->xres + 1,
        field->xres*field->yres - 1,
    };
    field_assert_local_extrema(field, NULL, NULL, GWY_MASK_IGNORE,
                               indices, G_N_ELEMENTS(indices), TRUE);
    g_object_unref(field);
}

void
test_field_local_extrema_min_full(void)
{
    GwyField *field = field_local_extrema_create1(-1);
    guint indices[] = {
        10*field->xres + 24,
        10*field->xres + 25,
        11*field->xres + 24,
        9*field->xres + 24,
        5*field->xres + 6,
        4*field->xres + 6,
        1*field->xres + 1,
        field->xres*field->yres - 1,
    };
    field_assert_local_extrema(field, NULL, NULL, GWY_MASK_IGNORE,
                               indices, G_N_ELEMENTS(indices), FALSE);
    g_object_unref(field);
}

void
test_field_local_extrema_max_part(void)
{
    GwyField *field = field_local_extrema_create1(1);
    GwyFieldPart fpart = { 1, 1, 24, 9 };
    guint indices[] = {
        9*field->xres + 24,
        5*field->xres + 6,
        4*field->xres + 6,
        1*field->xres + 1,
    };
    field_assert_local_extrema(field, &fpart, NULL, GWY_MASK_IGNORE,
                               indices, G_N_ELEMENTS(indices), TRUE);
    g_object_unref(field);
}

void
test_field_local_extrema_min_part(void)
{
    GwyField *field = field_local_extrema_create1(-1);
    GwyFieldPart fpart = { 1, 1, 24, 9 };
    guint indices[] = {
        9*field->xres + 24,
        5*field->xres + 6,
        4*field->xres + 6,
        1*field->xres + 1,
    };
    field_assert_local_extrema(field, &fpart, NULL, GWY_MASK_IGNORE,
                               indices, G_N_ELEMENTS(indices), FALSE);
    g_object_unref(field);
}

void
test_field_local_extrema_max_masked(void)
{
    GwyField *field = field_local_extrema_create1(1);
    GRand *rng = g_rand_new_with_seed(42);
    GwyMaskField *mask = random_mask_field(field->xres, field->yres, rng);
    gwy_mask_field_set(mask, 24, 10, FALSE);
    gwy_mask_field_set(mask, 25, 10, TRUE);
    gwy_mask_field_set(mask, 24, 11, FALSE);
    gwy_mask_field_set(mask, 24, 9, TRUE);
    gwy_mask_field_set(mask, 6, 5, TRUE);
    gwy_mask_field_set(mask, 6, 4, FALSE);
    gwy_mask_field_set(mask, 1, 1, FALSE);
    gwy_mask_field_set(mask, field->xres-1, field->yres-1, TRUE);
    guint indices_include[] = {
        10*field->xres + 25,
        9*field->xres + 24,
        5*field->xres + 6,
        field->xres*field->yres - 1,
    };
    guint indices_exclude[] = {
        10*field->xres + 24,
        11*field->xres + 24,
        4*field->xres + 6,
        1*field->xres + 1,
    };
    field_assert_local_extrema(field, NULL, mask, GWY_MASK_INCLUDE,
                               indices_include, G_N_ELEMENTS(indices_include),
                               TRUE);
    field_assert_local_extrema(field, NULL, mask, GWY_MASK_EXCLUDE,
                               indices_exclude, G_N_ELEMENTS(indices_exclude),
                               TRUE);
    g_rand_free(rng);
    g_object_unref(mask);
    g_object_unref(field);
}

void
test_field_local_extrema_min_masked(void)
{
    GwyField *field = field_local_extrema_create1(-1);
    GRand *rng = g_rand_new_with_seed(42);
    GwyMaskField *mask = random_mask_field(field->xres, field->yres, rng);
    gwy_mask_field_set(mask, 24, 10, FALSE);
    gwy_mask_field_set(mask, 25, 10, TRUE);
    gwy_mask_field_set(mask, 24, 11, FALSE);
    gwy_mask_field_set(mask, 24, 9, TRUE);
    gwy_mask_field_set(mask, 6, 5, TRUE);
    gwy_mask_field_set(mask, 6, 4, FALSE);
    gwy_mask_field_set(mask, 1, 1, FALSE);
    gwy_mask_field_set(mask, field->xres-1, field->yres-1, TRUE);
    guint indices_include[] = {
        10*field->xres + 25,
        9*field->xres + 24,
        5*field->xres + 6,
        field->xres*field->yres - 1,
    };
    guint indices_exclude[] = {
        10*field->xres + 24,
        11*field->xres + 24,
        4*field->xres + 6,
        1*field->xres + 1,
    };
    field_assert_local_extrema(field, NULL, mask, GWY_MASK_INCLUDE,
                               indices_include, G_N_ELEMENTS(indices_include),
                               FALSE);
    field_assert_local_extrema(field, NULL, mask, GWY_MASK_EXCLUDE,
                               indices_exclude, G_N_ELEMENTS(indices_exclude),
                               FALSE);
    g_rand_free(rng);
    g_object_unref(mask);
    g_object_unref(field);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
