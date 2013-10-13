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
 * Field statistics
 *
 ***************************************************************************/

void
test_field_range(void)
{
    enum { max_size = 76 };
    GRand *rng = g_rand_new_with_seed(42);
    gsize niter = 50;

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size);
        GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
        gdouble *data = field->data;
        for (guint i = 0; i < yres; i++) {
            for (guint j = 0; j < xres; j++)
                data[i*xres + j] = g_rand_double_range(rng, -1.0, 1.0);
        }
        gwy_field_invalidate(field);

        gdouble min, max;
        gwy_field_min_max_full(field, &min, &max);
        g_assert_cmpfloat(min, <=, max);
        g_assert_cmpfloat(min, >=, -1.0);
        g_assert_cmpfloat(max, <=, 1.0);
        gdouble lower = -0.2;
        gdouble upper = 0.6;

        GwyMaskField *mask = gwy_mask_field_new_from_field(field, NULL,
                                                           lower, upper, FALSE);
        guint count_mask = gwy_mask_field_count(mask, NULL, TRUE);
        guint nabove, nbelow;
        guint total = gwy_field_count_above_below(field, NULL,
                                                  mask, GWY_MASK_INCLUDE,
                                                  upper, lower, TRUE,
                                                  &nabove, &nbelow);
        guint count_field = total - nabove - nbelow;
        g_assert_cmpuint(count_mask, ==, count_field);
        gwy_field_min_max(field, NULL, mask, GWY_MASK_INCLUDE, &min, &max);
        g_assert_cmpfloat(min, <=, max);
        g_assert_cmpfloat(min, >=, lower);
        g_assert_cmpfloat(max, <=, upper);

        guint nabove1, nbelow1, nabove2, nbelow2;
        total = gwy_field_count_above_below(field, NULL,
                                            NULL, GWY_MASK_IGNORE,
                                            upper, lower, FALSE,
                                            &nabove1, &nbelow1);
        g_assert_cmpfloat(total, ==, xres*yres);
        total = gwy_field_count_above_below(field, NULL,
                                            NULL, GWY_MASK_IGNORE,
                                            lower, upper, TRUE,
                                            &nabove2, &nbelow2);
        g_assert_cmpfloat(total, ==, xres*yres);
        g_assert_cmpuint(nabove1 + nbelow2, ==, total);
        g_assert_cmpuint(nabove2 + nbelow1, ==, total);

        g_object_unref(mask);
        g_object_unref(field);
    }

    g_rand_free(rng);
}

static void
sum_quarters(gdouble zul, gdouble zur, gdouble zlr, gdouble zll,
             guint wul, guint wur, guint wlr, guint wll,
             gpointer user_data)
{
    gdouble *s = (gdouble*)user_data;
    *s += zul*wul + zur*wur + zlr*wlr + zll*wll;
}

void
test_field_process_quarters_unmasked_border(void)
{
    enum { max_size = 15, niter = 100 };
    gdouble eps = 1e-12;
    GRand *rng = g_rand_new_with_seed(42);

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size);
        guint width = g_rand_int_range(rng, 1, xres+1);
        guint height = g_rand_int_range(rng, 1, yres+1);
        guint col = g_rand_int_range(rng, 0, xres-width+1);
        guint row = g_rand_int_range(rng, 0, yres-height+1);
        GwyFieldPart fpart = { col, row, width, height };
        GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
        field_randomize(field, rng);
        gdouble result = 0.0;
        gwy_field_process_quarters(field, &fpart, NULL, 0,
                                   TRUE, sum_quarters, &result);
        gdouble reference = 0.0;
        for (guint i = 0; i < height; i++) {
            for (guint j = 0; j < width; j++) {
                reference += 4.0*gwy_field_index(field, col + j, row + i);
            }
        }
        gwy_assert_floatval(result, reference, eps);

        g_object_unref(field);
    }
    g_rand_free(rng);
}

void
test_field_process_quarters_unmasked_noborder(void)
{
    enum { max_size = 15, niter = 100 };
    gdouble eps = 1e-12;
    GRand *rng = g_rand_new_with_seed(42);

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size);
        guint width = g_rand_int_range(rng, 1, xres+1);
        guint height = g_rand_int_range(rng, 1, yres+1);
        guint col = g_rand_int_range(rng, 0, xres-width+1);
        guint row = g_rand_int_range(rng, 0, yres-height+1);
        GwyFieldPart fpart = { col, row, width, height };
        GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
        field_randomize(field, rng);
        gdouble result = 0.0;
        gwy_field_process_quarters(field, &fpart, NULL, 0,
                                   FALSE, sum_quarters, &result);
        gdouble reference = 0.0;
        for (guint i = 0; i < height; i++) {
            for (guint j = 0; j < width; j++) {
                guint wh = (col + j > 0) + (col + j < xres-1);
                guint wv = (row + i > 0) + (row + i < yres-1);
                reference += wh*wv*gwy_field_index(field, col + j, row + i);
            }
        }
        gwy_assert_floatval(result, reference, eps);

        g_object_unref(field);
    }
    g_rand_free(rng);
}

void
test_field_process_quarters_masked_border(void)
{
    enum { max_size = 15, niter = 100 };
    gdouble eps = 1e-12;
    GRand *rng = g_rand_new_with_seed(42);

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size);
        guint width = g_rand_int_range(rng, 1, xres+1);
        guint height = g_rand_int_range(rng, 1, yres+1);
        guint col = g_rand_int_range(rng, 0, xres-width+1);
        guint row = g_rand_int_range(rng, 0, yres-height+1);
        GwyFieldPart fpart = { col, row, width, height };
        GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
        field_randomize(field, rng);
        GwyMaskField *mask = random_mask_field_prob(xres, yres, rng, 0.5);
        gdouble result = 0.0, result0 = 0.0, result1 = 0.0;
        gwy_field_process_quarters(field, &fpart, mask, GWY_MASK_IGNORE,
                                   TRUE, sum_quarters, &result);
        gwy_field_process_quarters(field, &fpart, mask, GWY_MASK_EXCLUDE,
                                   TRUE, sum_quarters, &result0);
        gwy_field_process_quarters(field, &fpart, mask, GWY_MASK_INCLUDE,
                                   TRUE, sum_quarters, &result1);
        gdouble reference = 0.0, reference0 = 0.0, reference1 = 0.0;
        for (guint i = 0; i < height; i++) {
            for (guint j = 0; j < width; j++) {
                gdouble v = 4.0*gwy_field_index(field, col + j, row + i);
                reference += v;
                if (gwy_mask_field_get(mask, col + j, row + i))
                    reference1 += v;
                else
                    reference0 += v;
            }
        }
        gwy_assert_floatval(result, reference, eps);
        gwy_assert_floatval(result0, reference0, eps);
        gwy_assert_floatval(result1, reference1, eps);

        g_object_unref(mask);
        g_object_unref(field);
    }
    g_rand_free(rng);
}

void
test_field_process_quarters_masked_noborder(void)
{
    enum { max_size = 15, niter = 100 };
    gdouble eps = 1e-12;
    GRand *rng = g_rand_new_with_seed(42);

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size);
        guint width = g_rand_int_range(rng, 1, xres+1);
        guint height = g_rand_int_range(rng, 1, yres+1);
        guint col = g_rand_int_range(rng, 0, xres-width+1);
        guint row = g_rand_int_range(rng, 0, yres-height+1);
        GwyFieldPart fpart = { col, row, width, height };
        GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
        field_randomize(field, rng);
        GwyMaskField *mask = random_mask_field_prob(xres, yres, rng, 0.5);
        gdouble result = 0.0, result0 = 0.0, result1 = 0.0;
        gwy_field_process_quarters(field, &fpart, mask, GWY_MASK_IGNORE,
                                   FALSE, sum_quarters, &result);
        gwy_field_process_quarters(field, &fpart, mask, GWY_MASK_EXCLUDE,
                                   FALSE, sum_quarters, &result0);
        gwy_field_process_quarters(field, &fpart, mask, GWY_MASK_INCLUDE,
                                   FALSE, sum_quarters, &result1);
        gdouble reference = 0.0, reference0 = 0.0, reference1 = 0.0;
        for (guint i = 0; i < height; i++) {
            for (guint j = 0; j < width; j++) {
                guint wh = (col + j > 0) + (col + j < xres-1);
                guint wv = (row + i > 0) + (row + i < yres-1);
                gdouble v = wh*wv*gwy_field_index(field, col + j, row + i);
                reference += v;
                if (gwy_mask_field_get(mask, col + j, row + i))
                    reference1 += v;
                else
                    reference0 += v;
            }
        }
        gwy_assert_floatval(result, reference, eps);
        gwy_assert_floatval(result0, reference0, eps);
        gwy_assert_floatval(result1, reference1, eps);

        g_object_unref(mask);
        g_object_unref(field);
    }
    g_rand_free(rng);
}

static gdouble
planar_field_surface_area(const GwyField *field)
{
    guint xres = field->xres, yres = field->yres;
    gdouble z00 = field->data[0];
    gdouble zm0 = field->data[xres-1];
    gdouble z0m = field->data[(yres - 1)*xres];
    gdouble zmm = field->data[xres*yres - 1];
    g_assert(fabs(zmm - (z00 + (zm0 - z00) + (z0m - z00))) < 1e-9);
    gdouble dx = gwy_field_dx(field);
    gdouble dy = gwy_field_dy(field);
    gdouble xinner = dx*(xres - 1.0);
    gdouble yinner = dy*(yres - 1.0);
    gdouble xgrad = xinner ? (zm0 - z00)/xinner : 0.0;
    gdouble ygrad = yinner ? (z0m - z00)/yinner : 0.0;
    gdouble grad = hypot(xgrad, ygrad);
    gdouble area_inner = xinner*yinner*hypot(1.0, grad);
    gdouble area_lr = yinner*dx*hypot(1.0, ygrad);
    gdouble area_td = xinner*dy*hypot(1.0, xgrad);
    gdouble area_corner = dx*dy;

    return area_inner + area_lr + area_td + area_corner;
}

GwyField*
field_make_planar(guint xres, guint yres,
                  gdouble alpha, gdouble beta)
{
    GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
    gdouble *data = field->data;
    for (guint i = 0; i < yres; i++) {
        for (guint j = 0; j < xres; j++)
            data[i*xres + j] = alpha*(i + 0.5)/yres + beta*(j + 0.5)/xres;
    }
    gwy_field_invalidate(field);
    return field;
}

void
test_field_surface_area_planar(void)
{
    enum { max_size = 76 };
    GRand *rng = g_rand_new_with_seed(42);
    gsize niter = 50;

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size);
        gdouble alpha = g_rand_double_range(rng, -5.0, 5.0);
        gdouble beta = g_rand_double_range(rng, -5.0, 5.0);
        GwyField *field = field_make_planar(xres, yres, alpha, beta);
        gdouble area, area_expected;
        gwy_field_set_xreal(field, xres/sqrt(xres*yres));
        gwy_field_set_yreal(field, yres/sqrt(xres*yres));
        area = gwy_field_surface_area(field, NULL, NULL, GWY_MASK_IGNORE);
        area_expected = planar_field_surface_area(field);
        g_assert_cmpfloat(fabs(area - area_expected)/area_expected, <=, 1e-9);

        gwy_field_set_xreal(field, 1.0);
        gwy_field_set_yreal(field, 1.0);
        area = gwy_field_surface_area(field, NULL, NULL, GWY_MASK_IGNORE);
        area_expected = planar_field_surface_area(field);
        g_assert_cmpfloat(fabs(area - area_expected)/area_expected, <=, 1e-9);

        g_object_unref(field);
    }
    g_rand_free(rng);
}

void
test_field_surface_area_masked(void)
{
    enum { max_size = 76 };
    GRand *rng = g_rand_new_with_seed(42);
    gsize niter = 50;

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size);
        GwyField *field = gwy_field_new_sized(xres, yres, FALSE);

        field_randomize(field, rng);
        guint width = g_rand_int_range(rng, 1, xres+1);
        guint height = g_rand_int_range(rng, 1, yres+1);
        guint col = g_rand_int_range(rng, 0, xres-width+1);
        guint row = g_rand_int_range(rng, 0, yres-height+1);
        GwyFieldPart fpart = { col, row, width, height };

        GwyMaskField *mask = random_mask_field(xres, yres, rng);

        gwy_field_set_xreal(field, xres/sqrt(xres*yres));
        gwy_field_set_yreal(field, yres/sqrt(xres*yres));
        gdouble area_include, area_exclude, area_ignore;
        area_include = gwy_field_surface_area(field, &fpart,
                                              mask, GWY_MASK_INCLUDE);
        area_exclude = gwy_field_surface_area(field, &fpart,
                                              mask, GWY_MASK_EXCLUDE);
        area_ignore = gwy_field_surface_area(field, &fpart,
                                             mask, GWY_MASK_IGNORE);
        g_assert_cmpfloat(fabs(area_include + area_exclude
                               - area_ignore)/area_ignore, <=, 1e-9);

        gwy_field_set_xreal(field, 1.0);
        gwy_field_set_yreal(field, 1.0);
        area_include = gwy_field_surface_area(field, &fpart,
                                              mask, GWY_MASK_INCLUDE);
        area_exclude = gwy_field_surface_area(field, &fpart,
                                              mask, GWY_MASK_EXCLUDE);
        area_ignore = gwy_field_surface_area(field, &fpart,
                                             mask, GWY_MASK_IGNORE);
        g_assert_cmpfloat(fabs(area_include + area_exclude
                               - area_ignore)/area_ignore, <=, 1e-9);
        g_object_unref(mask);
        g_object_unref(field);
    }
    g_rand_free(rng);
}

// Screw the borders.
static gdouble
planar_field_volume(const GwyField *field)
{
    guint xres = field->xres, yres = field->yres;
    gdouble z00 = (gwy_field_index(field, 0, 0)
                   + gwy_field_index(field, 1, 0)
                   + gwy_field_index(field, 0, 1)
                   + gwy_field_index(field, 1, 1))/4.0;
    gdouble zm0 = (gwy_field_index(field, xres-2, 0)
                   + gwy_field_index(field, xres-1, 0)
                   + gwy_field_index(field, xres-2, 1)
                   + gwy_field_index(field, xres-1, 1))/4.0;
    gdouble z0m = (gwy_field_index(field, 0, yres-2)
                   + gwy_field_index(field, 1, yres-2)
                   + gwy_field_index(field, 0, yres-1)
                   + gwy_field_index(field, 1, yres-1))/4.0;
    gdouble zmm = (gwy_field_index(field, xres-2, yres-2)
                   + gwy_field_index(field, xres-1, yres-2)
                   + gwy_field_index(field, xres-2, yres-1)
                   + gwy_field_index(field, xres-1, yres-1))/4.0;
    gwy_assert_floatval(zmm, z00 + (zm0 - z00) + (z0m - z00), 1e-9);
    gdouble dx = gwy_field_dx(field), dy = gwy_field_dy(field);
    return (xres - 2)*(yres - 2)*dx*dy*(z00 + zm0 + z0m + zmm)/4.0;
}

void
field_volume_planar_one(GwyFieldVolumeMethod method)
{
    enum { max_size = 76 };
    GRand *rng = g_rand_new_with_seed(42);
    gsize niter = 50;

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 3, max_size);
        guint yres = g_rand_int_range(rng, 3, max_size);
        gdouble alpha = g_rand_double_range(rng, -5.0, 5.0);
        gdouble beta = g_rand_double_range(rng, -5.0, 5.0);
        GwyField *field = field_make_planar(xres, yres, alpha, beta);
        GwyFieldPart fpart = { 1, 1, xres-2, yres-2 };
        gdouble volume, volume_expected;
        gwy_field_set_xreal(field, xres/sqrt(xres*yres));
        gwy_field_set_yreal(field, yres/sqrt(xres*yres));
        volume = gwy_field_volume(field, &fpart, NULL, GWY_MASK_IGNORE, method);
        volume_expected = planar_field_volume(field);
        gwy_assert_floatval(volume, volume_expected, 1e-9*fabs(volume_expected));

        gwy_field_set_xreal(field, 1.0);
        gwy_field_set_yreal(field, 1.0);
        volume = gwy_field_volume(field, &fpart, NULL, GWY_MASK_IGNORE, method);
        volume_expected = planar_field_volume(field);
        gwy_assert_floatval(volume, volume_expected, 1e-9*fabs(volume_expected));

        g_object_unref(field);
    }
    g_rand_free(rng);
}

void
test_field_volume_planar_gwyddion2(void)
{
    field_volume_planar_one(GWY_FIELD_VOLUME_GWYDDION2);
}

void
test_field_volume_planar_triangular(void)
{
    field_volume_planar_one(GWY_FIELD_VOLUME_TRIANGULAR);
}

void
test_field_volume_planar_bilinear(void)
{
    field_volume_planar_one(GWY_FIELD_VOLUME_BILINEAR);
}

void
test_field_volume_planar_biquadratic(void)
{
    field_volume_planar_one(GWY_FIELD_VOLUME_BIQUADRATIC);
}

void
field_volume_masked_one(GwyFieldVolumeMethod method)
{
    enum { max_size = 76 };
    GRand *rng = g_rand_new_with_seed(42);
    gsize niter = 50;

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size);
        GwyField *field = gwy_field_new_sized(xres, yres, FALSE);

        field_randomize(field, rng);
        guint width = g_rand_int_range(rng, 1, xres+1);
        guint height = g_rand_int_range(rng, 1, yres+1);
        guint col = g_rand_int_range(rng, 0, xres-width+1);
        guint row = g_rand_int_range(rng, 0, yres-height+1);
        GwyFieldPart fpart = { col, row, width, height };

        GwyMaskField *mask = random_mask_field(xres, yres, rng);

        gwy_field_set_xreal(field, xres/sqrt(xres*yres));
        gwy_field_set_yreal(field, yres/sqrt(xres*yres));
        gdouble volume_include, volume_exclude, volume_ignore;
        volume_include = gwy_field_volume(field, &fpart,
                                          mask, GWY_MASK_INCLUDE, method);
        volume_exclude = gwy_field_volume(field, &fpart,
                                          mask, GWY_MASK_EXCLUDE, method);
        volume_ignore = gwy_field_volume(field, &fpart,
                                         mask, GWY_MASK_IGNORE, method);
        gwy_assert_floatval(volume_include + volume_exclude, volume_ignore,
                            1e-9*volume_ignore);

        gwy_field_set_xreal(field, 1.0);
        gwy_field_set_yreal(field, 1.0);
        volume_include = gwy_field_volume(field, &fpart,
                                          mask, GWY_MASK_INCLUDE, method);
        volume_exclude = gwy_field_volume(field, &fpart,
                                          mask, GWY_MASK_EXCLUDE, method);
        volume_ignore = gwy_field_volume(field, &fpart,
                                         mask, GWY_MASK_IGNORE, method);
        gwy_assert_floatval(volume_include + volume_exclude, volume_ignore,
                            1e-9*volume_ignore);
        g_object_unref(mask);
        g_object_unref(field);
    }
    g_rand_free(rng);
}

void
test_field_volume_masked_gwyddion2(void)
{
    field_volume_masked_one(GWY_FIELD_VOLUME_GWYDDION2);
}

void
test_field_volume_masked_triangular(void)
{
    field_volume_masked_one(GWY_FIELD_VOLUME_TRIANGULAR);
}

void
test_field_volume_masked_bilinear(void)
{
    field_volume_masked_one(GWY_FIELD_VOLUME_BILINEAR);
}

void
test_field_volume_masked_biquadratic(void)
{
    field_volume_masked_one(GWY_FIELD_VOLUME_BIQUADRATIC);
}

void
test_field_mean(void)
{
    enum { max_size = 76 };
    GRand *rng = g_rand_new_with_seed(42);
    gsize niter = 50;

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size);
        gdouble alpha = g_rand_double_range(rng, -5.0, 5.0);
        gdouble beta = g_rand_double_range(rng, -5.0, 5.0);
        GwyField *field = field_make_planar(xres, yres, alpha, beta);

        gdouble mean, mean_expected;
        mean = gwy_field_mean_full(field);
        mean_expected = 0.5*(alpha + beta);
        gwy_assert_floatval(mean, mean_expected, 1e-9*fabs(mean_expected));

        field_randomize(field, rng);
        guint width = g_rand_int_range(rng, 1, xres+1);
        guint height = g_rand_int_range(rng, 1, yres+1);
        guint col = g_rand_int_range(rng, 0, xres-width+1);
        guint row = g_rand_int_range(rng, 0, yres-height+1);
        GwyFieldPart fpart = { col, row, width, height };

        GwyMaskField *mask = random_mask_field(xres, yres, rng);
        guint m = gwy_mask_field_part_count(mask, &fpart, TRUE);
        guint n = gwy_mask_field_part_count(mask, &fpart, FALSE);
        gdouble mean_include = gwy_field_mean(field, &fpart,
                                              mask, GWY_MASK_INCLUDE);
        gdouble mean_exclude = gwy_field_mean(field, &fpart,
                                              mask, GWY_MASK_EXCLUDE);
        gdouble mean_ignore = gwy_field_mean(field, &fpart,
                                             mask, GWY_MASK_IGNORE);

        if (isnan(mean_include)) {
            g_assert_cmpuint(m, ==, 0);
            gwy_assert_floatval(mean_exclude, mean_ignore,
                                1e-9*fabs(mean_ignore));
        }
        else if (isnan(mean_exclude)) {
            g_assert_cmpuint(n, ==, 0);
            gwy_assert_floatval(mean_include, mean_ignore,
                                1e-9*fabs(mean_ignore));
        }
        else {
            // μ = (Mμ₁ + Nμ₂)/(M+N)
            gwy_assert_floatval((m*mean_include + n*mean_exclude)/(m+n),
                                mean_ignore, 1e-9*fabs(mean_ignore));
        }

        g_object_unref(mask);
        g_object_unref(field);
    }
    g_rand_free(rng);
}

void
test_field_rms(void)
{
    enum { max_size = 76 };
    GRand *rng = g_rand_new_with_seed(42);
    gsize niter = 50;

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size);
        gdouble alpha = g_rand_double_range(rng, -5.0, 5.0);
        gdouble beta = g_rand_double_range(rng, -5.0, 5.0);
        GwyField *field = field_make_planar(xres, yres, alpha, beta);

        gdouble rms, rms_expected;
        rms = gwy_field_rms_full(field);
        rms_expected = 0.5*sqrt((alpha*alpha*(1.0 - 1.0/yres/yres)
                                 + beta*beta*(1.0 - 1.0/xres/xres))/3.0);
        gwy_assert_floatval(rms, rms_expected, 1e-9*rms_expected);

        field_randomize(field, rng);
        guint width = g_rand_int_range(rng, 1, xres+1);
        guint height = g_rand_int_range(rng, 1, yres+1);
        guint col = g_rand_int_range(rng, 0, xres-width+1);
        guint row = g_rand_int_range(rng, 0, yres-height+1);
        GwyFieldPart fpart = { col, row, width, height };

        GwyMaskField *mask = random_mask_field(xres, yres, rng);
        guint m = gwy_mask_field_part_count(mask, &fpart, TRUE);
        guint n = gwy_mask_field_part_count(mask, &fpart, FALSE);
        gdouble mean_include = gwy_field_mean(field, &fpart,
                                              mask, GWY_MASK_INCLUDE);
        gdouble mean_exclude = gwy_field_mean(field, &fpart,
                                              mask, GWY_MASK_EXCLUDE);
        gdouble rms_include = gwy_field_rms(field, &fpart,
                                            mask, GWY_MASK_INCLUDE);
        gdouble rms_exclude = gwy_field_rms(field, &fpart,
                                            mask, GWY_MASK_EXCLUDE);
        gdouble rms_ignore = gwy_field_rms(field, &fpart,
                                           mask, GWY_MASK_IGNORE);

        if (isnan(mean_include)) {
            g_assert_cmpuint(m, ==, 0);
            gwy_assert_floatval(rms_exclude, rms_ignore, 1e-9*rms_ignore);
        }
        else if (isnan(mean_exclude)) {
            g_assert_cmpuint(n, ==, 0);
            gwy_assert_floatval(rms_include, rms_ignore, 1e-9*rms_ignore);
        }
        else {
            // σ² = [Mσ₁² + Nσ₂² + MN/(M+N)*(μ₁-μ₂)²]/(M+N)
            gdouble mean_diff = mean_include - mean_exclude;
            gwy_assert_floatval(sqrt((m*rms_include*rms_include
                                      + n*rms_exclude*rms_exclude
                                      + m*n*mean_diff*mean_diff/(m+n))/(m+n)),
                                rms_ignore, 1e-9*rms_ignore);
        }

        g_object_unref(mask);
        g_object_unref(field);
    }
    g_rand_free(rng);
}

void
test_field_meansq(void)
{
    enum { max_size = 76 };
    GRand *rng = g_rand_new_with_seed(42);
    gsize niter = 50;

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size);
        gdouble alpha = g_rand_double_range(rng, -5.0, 5.0);
        gdouble beta = g_rand_double_range(rng, -5.0, 5.0);
        GwyField *field = field_make_planar(xres, yres, alpha, beta);

        gdouble meansq, meansq_expected;
        meansq = gwy_field_meansq(field, NULL, NULL, 0);
        meansq_expected = (alpha*alpha/3.0 + beta*beta/3.0 + alpha*beta/2.0
                           - alpha*alpha/(12.0*yres*yres)
                           - beta*beta/(12.0*xres*xres));
        gwy_assert_floatval(meansq, meansq_expected, 1e-9*meansq_expected);

        field_randomize(field, rng);
        guint width = g_rand_int_range(rng, 1, xres+1);
        guint height = g_rand_int_range(rng, 1, yres+1);
        guint col = g_rand_int_range(rng, 0, xres-width+1);
        guint row = g_rand_int_range(rng, 0, yres-height+1);
        GwyFieldPart fpart = { col, row, width, height };

        GwyMaskField *mask = random_mask_field(xres, yres, rng);
        guint m = gwy_mask_field_part_count(mask, &fpart, TRUE);
        guint n = gwy_mask_field_part_count(mask, &fpart, FALSE);
        gdouble meansq_include = gwy_field_meansq(field, &fpart,
                                                  mask, GWY_MASK_INCLUDE);
        gdouble meansq_exclude = gwy_field_meansq(field, &fpart,
                                                  mask, GWY_MASK_EXCLUDE);
        gdouble meansq_ignore = gwy_field_meansq(field, &fpart,
                                                 mask, GWY_MASK_IGNORE);

        if (m == 0) {
            gwy_assert_floatval(meansq_exclude, meansq_ignore,
                                1e-9*meansq_ignore);
        }
        else if (n == 0) {
            gwy_assert_floatval(meansq_include, meansq_ignore,
                                1e-9*meansq_ignore);
        }
        else {
            // s = [Ms₁ + Ns₂]/(M+N)
            gwy_assert_floatval((m*meansq_include + n*meansq_exclude)/(m+n),
                                meansq_ignore, 1e-9*fabs(meansq_ignore));
        }

        g_object_unref(mask);
        g_object_unref(field);
    }
    g_rand_free(rng);
}

void
test_field_statistics(void)
{
    enum { max_size = 67 };
    GRand *rng = g_rand_new_with_seed(42);
    gsize niter = 50;

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size);
        gdouble alpha = g_rand_double_range(rng, -5.0, 5.0);
        gdouble beta = g_rand_double_range(rng, -5.0, 5.0);
        GwyField *field = field_make_planar(xres, yres, alpha, beta);

        // TODO: Other characteristics.
        gdouble rms, mean, skewness, rms_expected, mean_expected;
        gwy_field_statistics(field, NULL, NULL, GWY_MASK_IGNORE,
                             &mean, NULL, &rms, &skewness, NULL);
        mean_expected = 0.5*(alpha + beta);
        rms_expected = 0.5*sqrt((alpha*alpha*(1.0 - 1.0/yres/yres)
                                 + beta*beta*(1.0 - 1.0/xres/xres))/3.0);
        gwy_assert_floatval(mean, mean_expected, 1e-9*fabs(mean_expected));
        gwy_assert_floatval(rms, rms_expected, 1e-9*rms_expected);
        gwy_assert_floatval(skewness, 0.0, 1e-9);

        field_randomize(field, rng);
        guint width = g_rand_int_range(rng, 1, xres+1);
        guint height = g_rand_int_range(rng, 1, yres+1);
        guint col = g_rand_int_range(rng, 0, xres-width+1);
        guint row = g_rand_int_range(rng, 0, yres-height+1);
        GwyFieldPart fpart = { col, row, width, height };

        GwyMaskField *mask = random_mask_field(xres, yres, rng);
        guint m = gwy_mask_field_part_count(mask, &fpart, TRUE);
        guint n = gwy_mask_field_part_count(mask, &fpart, FALSE);
        gdouble mean_include, rms_include;
        gwy_field_statistics(field, &fpart, mask, GWY_MASK_INCLUDE,
                             &mean_include, NULL, &rms_include,
                             NULL, NULL);
        gdouble mean_exclude, rms_exclude;
        gwy_field_statistics(field, &fpart, mask, GWY_MASK_EXCLUDE,
                             &mean_exclude, NULL, &rms_exclude,
                             NULL, NULL);
        gdouble mean_ignore, rms_ignore;
        gwy_field_statistics(field, &fpart, mask, GWY_MASK_IGNORE,
                             &mean_ignore, NULL, &rms_ignore,
                             NULL, NULL);

        if (isnan(mean_include)) {
            g_assert_cmpuint(m, ==, 0);
            gwy_assert_floatval(mean_exclude, mean_ignore,
                                1e-9*fabs(mean_ignore));
            gwy_assert_floatval(rms_exclude, rms_ignore, 1e-9*rms_ignore);
        }
        else if (isnan(mean_exclude)) {
            g_assert_cmpuint(n, ==, 0);
            gwy_assert_floatval(mean_include, mean_ignore,
                                1e-9*fabs(mean_ignore));
            gwy_assert_floatval(rms_include, rms_ignore, 1e-9*rms_ignore);
        }
        else {
            // μ = (Mμ₁ + Nμ₂)/(M+N)
            gwy_assert_floatval((m*mean_include + n*mean_exclude)/(m+n),
                                mean_ignore, 1e-9*fabs(mean_ignore));
            // σ² = [Mσ₁² + Nσ₂² + MN/(M+N)*(μ₁-μ₂)²]/(M+N)
            gdouble mean_diff = mean_include - mean_exclude;
            gwy_assert_floatval(sqrt((m*rms_include*rms_include
                                      + n*rms_exclude*rms_exclude
                                      + m*n*mean_diff*mean_diff/(m+n))/(m+n)),
                                rms_ignore, 1e-9*rms_ignore);
        }

        g_object_unref(mask);
        g_object_unref(field);
    }
    g_rand_free(rng);
}

static void
field_median_one(GwyMaskingType masking)
{
    enum { max_size = 75 };
    GRand *rng = g_rand_new_with_seed(42);
    gsize niter = 50;

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size);
        GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
        field_randomize(field, rng);

        guint width = g_rand_int_range(rng, 1, xres+1);
        guint height = g_rand_int_range(rng, 1, yres+1);
        guint col = g_rand_int_range(rng, 0, xres-width+1);
        guint row = g_rand_int_range(rng, 0, yres-height+1);
        GwyFieldPart fpart = { col, row, width, height };

        GwyMaskField *mask = random_mask_field(xres, yres, rng);
        guint count = width*height;
        if (masking == GWY_MASK_INCLUDE)
            count = gwy_mask_field_part_count(mask, &fpart, TRUE);
        else if (masking == GWY_MASK_EXCLUDE)
            count = gwy_mask_field_part_count(mask, &fpart, FALSE);

        gdouble median = gwy_field_median(field, &fpart, mask, masking);
        guint nabove, nbelow;
        gwy_field_count_above_below(field, &fpart, mask, masking,
                                    median, median, FALSE, &nabove, &nbelow);

        if (isnan(median)) {
            g_assert_cmpuint(count, ==, 0);
            g_assert_cmpuint(nabove, ==, 0);
            g_assert_cmpuint(nbelow, ==, 0);
        }
        else {
            // XXX: Here we assert the part does not contain two identical
            // values.  While it is extremely rare it is not impossible.
            g_assert_cmpuint(nabove + nbelow, ==, count + 1);
            g_assert_cmpuint(nabove, <=, nbelow);
            g_assert_cmpuint(nabove + 1, >=, nbelow);
        }

        g_object_unref(mask);
        g_object_unref(field);
    }
    g_rand_free(rng);
}

void
test_field_median_include(void)
{
    field_median_one(GWY_MASK_INCLUDE);
}

void
test_field_median_exclude(void)
{
    field_median_one(GWY_MASK_EXCLUDE);
}

void
test_field_median_ignore(void)
{
    field_median_one(GWY_MASK_IGNORE);
}

static void
field_entropy_one(GwyMaskingType masking,
                  guint type,
                  gboolean outliers)
{
    enum { max_size = 178 };
    GRand *rng = g_rand_new_with_seed(42);
    GwyRand *zrng = gwy_rand_new_with_seed(42);
    gsize niter = outliers ? 20 : 50;

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 8, max_size);
        guint yres = g_rand_int_range(rng, 8, max_size);
        GwyField *field = gwy_field_new_sized(xres, yres, FALSE);

        guint width = g_rand_int_range(rng, 1, xres+1);
        guint height = g_rand_int_range(rng, 1, yres+1);
        guint col = g_rand_int_range(rng, 0, xres-width+1);
        guint row = g_rand_int_range(rng, 0, yres-height+1);
        GwyFieldPart fpart = { col, row, width, height };

        GwyMaskField *mask = random_mask_field(xres, yres, rng);
        guint count = width*height;
        if (masking == GWY_MASK_INCLUDE)
            count = gwy_mask_field_part_count(mask, &fpart, TRUE);
        else if (masking == GWY_MASK_EXCLUDE)
            count = gwy_mask_field_part_count(mask, &fpart, FALSE);

        if (count < 16 || (outliers && count < 216)) {
            g_object_unref(field);
            g_object_unref(mask);
            iter = MIN(iter-1, iter);
            continue;
        }

        gdouble param = exp(10.0*g_rand_double(rng) - 5.0);

        if (type == 0) {
            for (guint i = 0; i < xres*yres; i++)
                field->data[i] = param*gwy_rand_double(zrng);
        }
        else if (type == 1) {
            for (guint i = 0; i < xres*yres; i++)
                field->data[i] = param*gwy_rand_normal(zrng);
        }
        else if (type == 2) {
            for (guint i = 0; i < xres*yres; i++)
                field->data[i] = param*gwy_rand_exp_positive(zrng);
        }

        guint nout = outliers ? (guint)sqrt(sqrt(count) + 2.0) : 0;
        for (guint k = 0; k < nout; k++) {
            guint i = g_rand_int_range(rng, row, row+height);
            guint j = g_rand_int_range(rng, col, col+width);
            field->data[i*xres + j] = 1e30*(g_rand_double(rng) - 0.5);
        }

        gdouble entropy = gwy_field_entropy(field, &fpart, mask, masking);

        gdouble expected = 0.0;
        if (type == 0)
            expected = log(param);
        else if (type == 1)
            expected = 0.5 + log(GWY_SQRT_PI*G_SQRT2*param);
        else if (type == 2)
            expected = 1.0 + log(param);

        // Note this is not an absolute criterion, depending on what numbers
        // we get from the generator, the difference may be larger.
        gdouble eps = 3.5/sqrt(count);
        if (type == 0)
            eps = 12.0/count;
        if (nout) {
            gdouble r = (gdouble)nout/count;
            eps += 2.0*r*(fmax(fabs(expected), 1.0)
                          + fabs(log(param/sqrt(count))));
        }
        gwy_assert_floatval(expected, entropy, eps);

        g_object_unref(mask);
        g_object_unref(field);
    }
    gwy_rand_free(zrng);
    g_rand_free(rng);
}

void
test_field_entropy_uniform_ignore(void)
{
    field_entropy_one(GWY_MASK_IGNORE, 0, FALSE);
}

void
test_field_entropy_uniform_include(void)
{
    field_entropy_one(GWY_MASK_INCLUDE, 0, FALSE);
}

void
test_field_entropy_uniform_exclude(void)
{
    field_entropy_one(GWY_MASK_IGNORE, 0, FALSE);
}

void
test_field_entropy_normal_ignore(void)
{
    field_entropy_one(GWY_MASK_EXCLUDE, 1, FALSE);
}

void
test_field_entropy_normal_include(void)
{
    field_entropy_one(GWY_MASK_INCLUDE, 1, FALSE);
}

void
test_field_entropy_normal_exclude(void)
{
    field_entropy_one(GWY_MASK_IGNORE, 1, FALSE);
}

void
test_field_entropy_exponential_ignore(void)
{
    field_entropy_one(GWY_MASK_EXCLUDE, 2, FALSE);
}

void
test_field_entropy_exponential_include(void)
{
    field_entropy_one(GWY_MASK_INCLUDE, 2, FALSE);
}

void
test_field_entropy_exponential_exclude(void)
{
    field_entropy_one(GWY_MASK_EXCLUDE, 2, FALSE);
}

void
test_field_entropy_outliers_ignore(void)
{
    field_entropy_one(GWY_MASK_IGNORE, 0, TRUE);
}

void
test_field_entropy_outliers_include(void)
{
    field_entropy_one(GWY_MASK_INCLUDE, 0, TRUE);
}

void
test_field_entropy_outliers_exclude(void)
{
    field_entropy_one(GWY_MASK_EXCLUDE, 0, TRUE);
}

void
test_field_entropy_tiny(void)
{
    GwyField *field = gwy_field_new_sized(2, 2, FALSE);
    GwyMaskField *mask = gwy_mask_field_new_sized(2, 2, TRUE);
    field->data[0] = 0.0;
    field->data[1] = 0.0;
    field->data[2] = 1.0;
    field->data[3] = 1.0;

    gdouble entropy;

    entropy = gwy_field_entropy(field, NULL, NULL, GWY_MASK_IGNORE);
    gwy_assert_floatval(entropy, 0.0, 1e-16);

    entropy = gwy_field_entropy(field, &(GwyFieldPart){ 0, 0, 1, 2 },
                                NULL, GWY_MASK_IGNORE);
    gwy_assert_floatval(entropy, 0.0, 1e-16);

    entropy = gwy_field_entropy(field, &(GwyFieldPart){ 0, 0, 2, 1 },
                                NULL, GWY_MASK_IGNORE);
    g_assert(isinf(entropy));

    gwy_mask_field_set(mask, 0, 0, TRUE);
    gwy_mask_field_set(mask, 1, 1, TRUE);
    entropy = gwy_field_entropy(field, NULL, mask, GWY_MASK_INCLUDE);
    gwy_assert_floatval(entropy, 0.0, 1e-16);

    entropy = gwy_field_entropy(field, NULL, mask, GWY_MASK_EXCLUDE);
    gwy_assert_floatval(entropy, 0.0, 1e-16);

    gwy_mask_field_set(mask, 0, 0, FALSE);
    entropy = gwy_field_entropy(field, NULL, mask, GWY_MASK_INCLUDE);
    g_assert(isinf(entropy));

    entropy = gwy_field_entropy(field, NULL, mask, GWY_MASK_EXCLUDE);
    gwy_assert_floatval(entropy, log(3.0)/2.0 - 5.0/6.0*log(2.0), 1e-16);

    gwy_mask_field_set(mask, 1, 1, FALSE);
    entropy = gwy_field_entropy(field, NULL, mask, GWY_MASK_INCLUDE);
    g_assert(isnan(entropy));

    g_object_unref(mask);
    g_object_unref(field);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
