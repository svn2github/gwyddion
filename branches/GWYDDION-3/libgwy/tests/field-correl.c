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

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
