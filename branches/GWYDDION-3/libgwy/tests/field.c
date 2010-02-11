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
 * Field
 *
 ***************************************************************************/

void
field_randomize(GwyField *field,
                GRand *rng)
{
    gdouble *d = gwy_field_get_data(field);
    for (guint n = field->xres*field->yres; n; n--, d++)
        *d = g_rand_double(rng);
    gwy_field_invalidate(field);
}

static void
test_field_assert_equal(const GwyField *result,
                        const GwyField *reference)
{
    g_assert(GWY_IS_FIELD(result));
    g_assert(GWY_IS_FIELD(reference));
    g_assert_cmpuint(result->xres, ==, reference->xres);
    g_assert_cmpuint(result->yres, ==, reference->yres);

    for (guint i = 0; i < result->yres; i++) {
        gdouble *result_row = result->data + i*result->xres;
        gdouble *reference_row = reference->data + i*reference->xres;
        for (guint j = 0; j < result->xres; j++)
            g_assert_cmpfloat(result_row[j], ==, reference_row[j]);
    }
}

void
test_field_props(void)
{
    GwyField *field = gwy_field_new_sized(41, 37, FALSE);
    guint xres, yres;
    gdouble xreal, yreal, xoff, yoff;
    g_object_get(field,
                 "x-res", &xres,
                 "y-res", &yres,
                 "x-real", &xreal,
                 "y-real", &yreal,
                 "x-offset", &xoff,
                 "y-offset", &yoff,
                 NULL);
    g_assert_cmpuint(xres, ==, field->xres);
    g_assert_cmpuint(yres, ==, field->yres);
    g_assert_cmpfloat(xreal, ==, field->xreal);
    g_assert_cmpfloat(yreal, ==, field->yreal);
    g_assert_cmpfloat(xoff, ==, field->xoff);
    g_assert_cmpfloat(yoff, ==, field->yoff);
    xreal = 5.0;
    yreal = 7.0e-14;
    xoff = -3;
    yoff = 1e-15;
    gwy_field_set_xreal(field, xreal);
    gwy_field_set_yreal(field, yreal);
    gwy_field_set_xoffset(field, xoff);
    gwy_field_set_yoffset(field, yoff);
    g_assert_cmpfloat(xreal, ==, field->xreal);
    g_assert_cmpfloat(yreal, ==, field->yreal);
    g_assert_cmpfloat(xoff, ==, field->xoff);
    g_assert_cmpfloat(yoff, ==, field->yoff);
    g_object_unref(field);
}

void
test_field_units(void)
{
    enum { max_size = 411 };
    GRand *rng = g_rand_new();
    g_rand_set_seed(rng, 42);
    gsize niter = 50;

    for (guint iter = 0; iter < niter; iter++) {
        guint width = g_rand_int_range(rng, 1, max_size);
        guint height = g_rand_int_range(rng, 1, max_size);
        GwyField *field = gwy_field_new_sized(width, height, FALSE);
        g_object_set(field,
                     "x-real", -log(1.0 - g_rand_double(rng)),
                     "y-real", -log(1.0 - g_rand_double(rng)),
                     NULL);
        GString *prev = g_string_new(NULL), *next = g_string_new(NULL);
        GwyValueFormat *format = gwy_field_get_format_xy(field,
                                                         GWY_VALUE_FORMAT_PLAIN,
                                                         NULL);
        gdouble dx = gwy_field_dx(field);
        for (gint i = 0; i <= 10; i++) {
            GWY_SWAP(GString*, prev, next);
            g_string_assign(next,
                            gwy_value_format_print_number(format, (i - 5)*dx));
            if (i)
                g_assert_cmpstr(prev->str, !=, next->str);
        }
        gdouble dy = gwy_field_dy(field);
        for (gint i = 0; i <= 10; i++) {
            GWY_SWAP(GString*, prev, next);
            g_string_assign(next,
                            gwy_value_format_print_number(format, (i - 5)*dy));
            if (i)
                g_assert_cmpstr(prev->str, !=, next->str);
        }
        g_object_unref(format);
        g_object_unref(field);
        g_string_free(prev, TRUE);
        g_string_free(next, TRUE);
    }
    g_rand_free(rng);
}

void
test_field_serialize(void)
{
    enum { max_size = 55 };
    GRand *rng = g_rand_new();
    g_rand_set_seed(rng, 42);
    gsize niter = g_test_slow() ? 50 : 10;

    for (guint iter = 0; iter < niter; iter++) {
        guint width = g_rand_int_range(rng, 1, max_size);
        guint height = g_rand_int_range(rng, 1, max_size/4);
        GwyField *original = gwy_field_new_sized(width, height, FALSE);
        field_randomize(original, rng);
        GwyField *copy;

        copy = gwy_field_duplicate(original);
        test_field_assert_equal(copy, original);
        g_object_unref(copy);

        copy = gwy_field_new();
        gwy_field_assign(copy, original);
        test_field_assert_equal(copy, original);
        g_object_unref(copy);

        copy = GWY_FIELD(serialize_and_back(G_OBJECT(original)));
        test_field_assert_equal(copy, original);
        g_object_unref(copy);

        g_object_unref(original);
    }
    g_rand_free(rng);
}

void
test_field_set_size(void)
{
    GwyField *field = gwy_field_new_sized(13, 11, TRUE);
    guint xres_changed = 0, yres_changed = 0;

    g_signal_connect_swapped(field, "notify::x-res",
                             G_CALLBACK(record_signal), &xres_changed);
    g_signal_connect_swapped(field, "notify::y-res",
                             G_CALLBACK(record_signal), &yres_changed);

    gwy_field_set_size(field, 13, 11, TRUE);
    g_assert_cmpuint(field->xres, ==, 13);
    g_assert_cmpuint(field->yres, ==, 11);
    g_assert_cmpuint(xres_changed, ==, 0);
    g_assert_cmpuint(yres_changed, ==, 0);

    gwy_field_set_size(field, 13, 10, TRUE);
    g_assert_cmpuint(field->xres, ==, 13);
    g_assert_cmpuint(field->yres, ==, 10);
    g_assert_cmpuint(xres_changed, ==, 0);
    g_assert_cmpuint(yres_changed, ==, 1);

    gwy_field_set_size(field, 11, 10, TRUE);
    g_assert_cmpuint(field->xres, ==, 11);
    g_assert_cmpuint(field->yres, ==, 10);
    g_assert_cmpuint(xres_changed, ==, 1);
    g_assert_cmpuint(yres_changed, ==, 1);

    gwy_field_set_size(field, 15, 14, TRUE);
    g_assert_cmpuint(field->xres, ==, 15);
    g_assert_cmpuint(field->yres, ==, 14);
    g_assert_cmpuint(xres_changed, ==, 2);
    g_assert_cmpuint(yres_changed, ==, 2);

    g_object_unref(field);
}

static void
field_part_copy_dumb(const GwyField *src,
                     guint col,
                     guint row,
                     guint width,
                     guint height,
                     GwyField *dest,
                     guint destcol,
                     guint destrow)
{
    for (guint i = 0; i < height; i++) {
        if (row + i >= src->yres || destrow + i >= dest->yres)
            continue;
        for (guint j = 0; j < width; j++) {
            if (col + j >= src->xres || destcol + j >= dest->xres)
                continue;

            gdouble val = gwy_field_index(src, col + j, row + i);
            gwy_field_index(dest, destcol + j, destrow + i) = val;
        }
    }
}

void
test_field_copy(void)
{
    enum { max_size = 19 };
    GRand *rng = g_rand_new();
    g_rand_set_seed(rng, 42);
    gsize niter = g_test_slow() ? 1000 : 200;

    for (gsize iter = 0; iter < niter; iter++) {
        guint sxres = g_rand_int_range(rng, 1, max_size);
        guint syres = g_rand_int_range(rng, 1, max_size/2);
        guint dxres = g_rand_int_range(rng, 1, max_size);
        guint dyres = g_rand_int_range(rng, 1, max_size/2);
        GwyField *source = gwy_field_new_sized(sxres, syres, FALSE);
        GwyField *dest = gwy_field_new_sized(dxres, dyres, FALSE);
        GwyField *reference = gwy_field_new_sized(dxres, dyres, FALSE);
        field_randomize(source, rng);
        field_randomize(reference, rng);
        gwy_field_copy(reference, dest);
        guint width = g_rand_int_range(rng, 0, MAX(sxres, dxres));
        guint height = g_rand_int_range(rng, 0, MAX(syres, dyres));
        guint col = g_rand_int_range(rng, 0, sxres);
        guint row = g_rand_int_range(rng, 0, syres);
        guint destcol = g_rand_int_range(rng, 0, dxres);
        guint destrow = g_rand_int_range(rng, 0, dyres);
        if (sxres == dxres && g_rand_int_range(rng, 0, 2) == 0) {
            // Check the fast path
            col = destcol = 0;
            width = sxres;
        }
        gwy_field_part_copy(source, col, row, width, height,
                            dest, destcol, destrow);
        field_part_copy_dumb(source, col, row, width, height,
                             reference, destcol, destrow);
        test_field_assert_equal(dest, reference);
        g_object_unref(source);
        g_object_unref(dest);
        g_object_unref(reference);
    }
    g_rand_free(rng);
}

void
test_field_new_part(void)
{
    enum { max_size = 23 };
    GRand *rng = g_rand_new();
    g_rand_set_seed(rng, 42);
    gsize niter = g_test_slow() ? 1000 : 200;

    for (gsize iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size/2);
        GwyField *source = gwy_field_new_sized(xres, yres, FALSE);
        field_randomize(source, rng);
        guint width = g_rand_int_range(rng, 1, xres+1);
        guint height = g_rand_int_range(rng, 1, yres+1);
        guint col = g_rand_int_range(rng, 0, xres-width+1);
        guint row = g_rand_int_range(rng, 0, yres-height+1);
        GwyField *part = gwy_field_new_part(source, col, row, width, height,
                                            TRUE);
        GwyField *reference = gwy_field_new_sized(width, height, FALSE);
        field_part_copy_dumb(source, col, row, width, height,
                             reference, 0, 0);
        test_field_assert_equal(part, reference);
        g_object_unref(source);
        g_object_unref(part);
        g_object_unref(reference);
    }
    g_rand_free(rng);
}

static void
test_field_flip_one(const gdouble *orig,
                    const gdouble *reference,
                    guint xres, guint yres,
                    gboolean horizontally, gboolean vertically)
{
    GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
    memcpy(field->data, orig, xres*yres*sizeof(gdouble));
    gwy_field_flip(field, horizontally, vertically, FALSE);
    for (guint i = 0; i < xres*yres; i++) {
        g_assert_cmpfloat(field->data[i], ==, reference[i]);
    }
    g_object_unref(field);
}

void
test_field_flip(void)
{
    enum { xres1 = 3, yres1 = 2 };
    const gdouble orig1[xres1*yres1] = {
        1, 2, 3,
        4, 5, 6,
    };
    const gdouble hflip1[xres1*yres1] = {
        3, 2, 1,
        6, 5, 4,
    };
    const gdouble vflip1[xres1*yres1] = {
        4, 5, 6,
        1, 2, 3,
    };
    const gdouble bflip1[xres1*yres1] = {
        6, 5, 4,
        3, 2, 1,
    };

    test_field_flip_one(orig1, orig1, xres1, yres1, FALSE, FALSE);
    test_field_flip_one(orig1, hflip1, xres1, yres1, TRUE, FALSE);
    test_field_flip_one(orig1, vflip1, xres1, yres1, FALSE, TRUE);
    test_field_flip_one(orig1, bflip1, xres1, yres1, TRUE, TRUE);

    enum { xres2 = 2, yres2 = 3 };
    const gdouble orig2[xres2*yres2] = {
        1, 2,
        3, 4,
        5, 6,
    };
    const gdouble hflip2[xres2*yres2] = {
        2, 1,
        4, 3,
        6, 5,
    };
    const gdouble vflip2[xres2*yres2] = {
        5, 6,
        3, 4,
        1, 2,
    };
    const gdouble bflip2[xres2*yres2] = {
        6, 5,
        4, 3,
        2, 1,
    };

    test_field_flip_one(orig2, orig2, xres2, yres2, FALSE, FALSE);
    test_field_flip_one(orig2, hflip2, xres2, yres2, TRUE, FALSE);
    test_field_flip_one(orig2, vflip2, xres2, yres2, FALSE, TRUE);
    test_field_flip_one(orig2, bflip2, xres2, yres2, TRUE, TRUE);
}

void
test_field_range(void)
{
    enum { max_size = 76 };
    GRand *rng = g_rand_new();
    g_rand_set_seed(rng, 42);
    gsize niter = 50;

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size);
        GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
        gdouble *data = gwy_field_get_data(field);
        for (guint i = 0; i < yres; i++) {
            for (guint j = 0; j < xres; j++)
                data[i*xres + j] = g_rand_double_range(rng, -1.0, 1.0);
        }

        gdouble min, max;
        gwy_field_min_max(field, &min, &max);
        g_assert_cmpfloat(min, <=, max);
        g_assert_cmpfloat(min, >=, -1.0);
        g_assert_cmpfloat(max, <=, 1.0);
        gdouble lower = -0.2;
        gdouble upper = 0.6;

        GwyMaskField *mask = gwy_mask_field_new_from_field(field,
                                                           0, 0, xres, yres,
                                                           lower, upper, FALSE);
        guint count_mask = gwy_mask_field_count(mask, NULL, TRUE);
        guint nabove, nbelow;
        guint total = gwy_field_part_count_in_range(field, mask,
                                                    GWY_MASK_INCLUDE,
                                                    0, 0, xres, yres,
                                                    upper, lower, TRUE,
                                                    &nabove, &nbelow);
        guint count_field = total - nabove - nbelow;
        g_assert_cmpuint(count_mask, ==, count_field);
        gwy_field_part_min_max(field, mask, GWY_MASK_INCLUDE,
                               0, 0, xres, yres, &min, &max);
        g_assert_cmpfloat(min, <=, max);
        g_assert_cmpfloat(min, >=, lower);
        g_assert_cmpfloat(max, <=, upper);

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

static GwyField*
make_planar_field(guint xres, guint yres,
                  gdouble alpha, gdouble beta)
{
    GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
    gdouble *data = gwy_field_get_data(field);
    for (guint i = 0; i < yres; i++) {
        for (guint j = 0; j < xres; j++)
            data[i*xres + j] = alpha*(i + 0.5)/yres + beta*(j + 0.5)/xres;
    }
    return field;
}

void
test_field_surface_area(void)
{
    enum { max_size = 76 };
    GRand *rng = g_rand_new();
    g_rand_set_seed(rng, 42);
    gsize niter = 50;

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size);
        gdouble alpha = g_rand_double_range(rng, -5.0, 5.0);
        gdouble beta = g_rand_double_range(rng, -5.0, 5.0);
        GwyField *field = make_planar_field(xres, yres, alpha, beta);
        gdouble area, area_expected;
        gwy_field_set_xreal(field, xres/sqrt(xres*yres));
        gwy_field_set_yreal(field, yres/sqrt(xres*yres));
        area = gwy_field_surface_area(field);
        area_expected = planar_field_surface_area(field);
        g_assert_cmpfloat(fabs(area - area_expected)/area_expected, <=, 1e-9);

        gwy_field_set_xreal(field, 1.0);
        gwy_field_set_yreal(field, 1.0);
        gwy_field_invalidate(field);
        area = gwy_field_surface_area(field);
        area_expected = planar_field_surface_area(field);
        g_assert_cmpfloat(fabs(area - area_expected)/area_expected, <=, 1e-9);

        field_randomize(field, rng);
        guint width = g_rand_int_range(rng, 1, xres+1);
        guint height = g_rand_int_range(rng, 1, yres+1);
        guint col = g_rand_int_range(rng, 0, xres-width+1);
        guint row = g_rand_int_range(rng, 0, yres-height+1);

        GwyMaskField *mask = random_mask_field(xres, yres, rng);
        gdouble area_include
            = gwy_field_part_surface_area(field, mask, GWY_MASK_INCLUDE,
                                          col, row, width, height);
        gdouble area_exclude
            = gwy_field_part_surface_area(field, mask, GWY_MASK_EXCLUDE,
                                          col, row, width, height);
        gdouble area_ignore
            = gwy_field_part_surface_area(field, mask, GWY_MASK_IGNORE,
                                          col, row, width, height);
        g_assert_cmpfloat(fabs(area_include + area_exclude
                               - area_ignore)/area_ignore, <=, 1e-9);

        g_object_unref(mask);
        g_object_unref(field);
    }
    g_rand_free(rng);
}

void
test_field_mean(void)
{
    enum { max_size = 76 };
    GRand *rng = g_rand_new();
    g_rand_set_seed(rng, 42);
    gsize niter = 50;

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size);
        gdouble alpha = g_rand_double_range(rng, -5.0, 5.0);
        gdouble beta = g_rand_double_range(rng, -5.0, 5.0);
        GwyField *field = make_planar_field(xres, yres, alpha, beta);

        gdouble mean, mean_expected;
        mean = gwy_field_mean(field);
        mean_expected = 0.5*(alpha + beta);
        g_assert_cmpfloat(fabs(mean - mean_expected)/mean_expected, <=, 1e-9);

        field_randomize(field, rng);
        guint width = g_rand_int_range(rng, 1, xres+1);
        guint height = g_rand_int_range(rng, 1, yres+1);
        guint col = g_rand_int_range(rng, 0, xres-width+1);
        guint row = g_rand_int_range(rng, 0, yres-height+1);

        GwyMaskField *mask = random_mask_field(xres, yres, rng);
        guint m = gwy_mask_field_part_count(mask, col, row, width, height,
                                            TRUE);
        guint n = gwy_mask_field_part_count(mask, col, row, width, height,
                                            FALSE);
        gdouble mean_include
            = gwy_field_part_mean(field, mask, GWY_MASK_INCLUDE,
                                  col, row, width, height);
        gdouble mean_exclude
            = gwy_field_part_mean(field, mask, GWY_MASK_EXCLUDE,
                                  col, row, width, height);
        gdouble mean_ignore
            = gwy_field_part_mean(field, mask, GWY_MASK_IGNORE,
                                  col, row, width, height);

        if (isnan(mean_include)) {
            g_assert_cmpuint(m, ==, 0);
            g_assert_cmpfloat(fabs(mean_exclude
                                    - mean_ignore)/mean_ignore, <=, 1e-9);
        }
        else if (isnan(mean_exclude)) {
            g_assert_cmpuint(n, ==, 0);
            g_assert_cmpfloat(fabs(mean_include
                                    - mean_ignore)/mean_ignore, <=, 1e-9);
        }
        else {
            // μ = (Mμ₁ + Nμ₂)/(M+N)
            g_assert_cmpfloat(fabs((m*mean_include + n*mean_exclude)/(m+n)
                                   - mean_ignore)/mean_ignore, <=, 1e-9);
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
    GRand *rng = g_rand_new();
    g_rand_set_seed(rng, 42);
    gsize niter = 50;

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size);
        gdouble alpha = g_rand_double_range(rng, -5.0, 5.0);
        gdouble beta = g_rand_double_range(rng, -5.0, 5.0);
        GwyField *field = make_planar_field(xres, yres, alpha, beta);

        gdouble rms, rms_expected;
        rms = gwy_field_rms(field);
        rms_expected = 0.5*sqrt((alpha*alpha*(1.0 - 1.0/yres/yres)
                                 + beta*beta*(1.0 - 1.0/xres/xres))/3.0);
        g_assert_cmpfloat(fabs(rms - rms_expected)/rms_expected, <=, 1e-9);

        field_randomize(field, rng);
        guint width = g_rand_int_range(rng, 1, xres+1);
        guint height = g_rand_int_range(rng, 1, yres+1);
        guint col = g_rand_int_range(rng, 0, xres-width+1);
        guint row = g_rand_int_range(rng, 0, yres-height+1);

        GwyMaskField *mask = random_mask_field(xres, yres, rng);
        guint m = gwy_mask_field_part_count(mask, col, row, width, height,
                                            TRUE);
        guint n = gwy_mask_field_part_count(mask, col, row, width, height,
                                            FALSE);
        gdouble mean_include
            = gwy_field_part_mean(field, mask, GWY_MASK_INCLUDE,
                                  col, row, width, height);
        gdouble mean_exclude
            = gwy_field_part_mean(field, mask, GWY_MASK_EXCLUDE,
                                  col, row, width, height);
        gdouble rms_include
            = gwy_field_part_rms(field, mask, GWY_MASK_INCLUDE,
                                 col, row, width, height);
        gdouble rms_exclude
            = gwy_field_part_rms(field, mask, GWY_MASK_EXCLUDE,
                                 col, row, width, height);
        gdouble rms_ignore
            = gwy_field_part_rms(field, mask, GWY_MASK_IGNORE,
                                 col, row, width, height);

        if (isnan(mean_include)) {
            g_assert_cmpuint(m, ==, 0);
            g_assert_cmpfloat(fabs(rms_exclude
                                   - rms_ignore)/rms_ignore, <=, 1e-9);
        }
        else if (isnan(mean_exclude)) {
            g_assert_cmpuint(n, ==, 0);
            g_assert_cmpfloat(fabs(rms_include
                                   - rms_ignore)/rms_ignore, <=, 1e-9);
        }
        else {
            // σ² = [Mσ₁² + Nσ₂² + MN/(M+N)*(μ₁-μ₂)²]/(M+N)
            gdouble mean_diff = mean_include - mean_exclude;
            g_assert_cmpfloat(fabs(sqrt((m*rms_include*rms_include
                                         + n*rms_exclude*rms_exclude
                                         + m*n*mean_diff*mean_diff/(m+n))/(m+n))
                                   - rms_ignore)/rms_ignore, <=, 1e-9);
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
    GRand *rng = g_rand_new();
    g_rand_set_seed(rng, 42);
    gsize niter = 50;

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size);
        gdouble alpha = g_rand_double_range(rng, -5.0, 5.0);
        gdouble beta = g_rand_double_range(rng, -5.0, 5.0);
        GwyField *field = make_planar_field(xres, yres, alpha, beta);

        // TODO: Other characteristics.
        gdouble rms, mean, rms_expected, mean_expected;
        gwy_field_statistics(field, &mean, NULL, &rms, NULL, NULL);
        mean_expected = 0.5*(alpha + beta);
        rms_expected = 0.5*sqrt((alpha*alpha*(1.0 - 1.0/yres/yres)
                                 + beta*beta*(1.0 - 1.0/xres/xres))/3.0);
        g_assert_cmpfloat(fabs(mean - mean_expected)/mean_expected, <=, 1e-9);
        g_assert_cmpfloat(fabs(rms - rms_expected)/rms_expected, <=, 1e-9);

        field_randomize(field, rng);
        guint width = g_rand_int_range(rng, 1, xres+1);
        guint height = g_rand_int_range(rng, 1, yres+1);
        guint col = g_rand_int_range(rng, 0, xres-width+1);
        guint row = g_rand_int_range(rng, 0, yres-height+1);

        GwyMaskField *mask = random_mask_field(xres, yres, rng);
        guint m = gwy_mask_field_part_count(mask, col, row, width, height,
                                            TRUE);
        guint n = gwy_mask_field_part_count(mask, col, row, width, height,
                                            FALSE);
        gdouble mean_include, rms_include;
        gwy_field_part_statistics(field, mask, GWY_MASK_INCLUDE,
                                  col, row, width, height,
                                  &mean_include, NULL, &rms_include,
                                  NULL, NULL);
        gdouble mean_exclude, rms_exclude;
        gwy_field_part_statistics(field, mask, GWY_MASK_EXCLUDE,
                                  col, row, width, height,
                                  &mean_exclude, NULL, &rms_exclude,
                                  NULL, NULL);
        gdouble mean_ignore, rms_ignore;
        gwy_field_part_statistics(field, mask, GWY_MASK_IGNORE,
                                  col, row, width, height,
                                  &mean_ignore, NULL, &rms_ignore,
                                  NULL, NULL);

        if (isnan(mean_include)) {
            g_assert_cmpuint(m, ==, 0);
            g_assert_cmpfloat(fabs(rms_exclude
                                   - rms_ignore)/rms_ignore, <=, 1e-9);
            g_assert_cmpfloat(fabs(mean_exclude
                                    - mean_ignore)/mean_ignore, <=, 1e-9);
        }
        else if (isnan(mean_exclude)) {
            g_assert_cmpuint(n, ==, 0);
            g_assert_cmpfloat(fabs(rms_include
                                   - rms_ignore)/rms_ignore, <=, 1e-9);
            g_assert_cmpfloat(fabs(mean_include
                                    - mean_ignore)/mean_ignore, <=, 1e-9);
        }
        else {
            // μ = (Mμ₁ + Nμ₂)/(M+N)
            g_assert_cmpfloat(fabs((m*mean_include + n*mean_exclude)/(m+n)
                                   - mean_ignore)/mean_ignore, <=, 1e-9);
            // σ² = [Mσ₁² + Nσ₂² + MN/(M+N)*(μ₁-μ₂)²]/(M+N)
            gdouble mean_diff = mean_include - mean_exclude;
            g_assert_cmpfloat(fabs(sqrt((m*rms_include*rms_include
                                         + n*rms_exclude*rms_exclude
                                         + m*n*mean_diff*mean_diff/(m+n))/(m+n))
                                   - rms_ignore)/rms_ignore, <=, 1e-9);
        }

        g_object_unref(mask);
        g_object_unref(field);
    }
    g_rand_free(rng);
}

static void
test_field_row_level_one(GwyRowShiftMethod method)
{
    enum { max_size = 260 };
    GRand *rng = g_rand_new();
    g_rand_set_seed(rng, 42);
    enum { niter = 10 };

    for (guint iter = 0; iter < niter; iter++) {
        // User large widths to esnure resonably probability; several
        // algorithms require the mask on pixels in consecutive rows which
        // means on 1/4 of pixels will be used on average.
        guint xres = g_rand_int_range(rng, 128, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size/8);
        GwyField *field = gwy_field_new_sized(xres, yres, TRUE);
        GwyLine *shifts = gwy_line_new_sized(yres, TRUE);
        line_randomize(shifts, rng);
        gwy_field_shift_rows(field, shifts);
        GwyLine *foundshifts = gwy_line_new_sized(yres, FALSE);
        gwy_field_find_row_shifts(field, NULL, GWY_MASK_IGNORE,
                                  method, 1, foundshifts);
        gwy_line_multiply(foundshifts, -1.0);
        gwy_line_accumulate(foundshifts);
        gwy_field_shift_rows(field, foundshifts);

        g_assert_cmpfloat(gwy_field_rms(field), <=, 1e-12);

        GwyMaskField *mask = random_mask_field(xres, yres, rng);
        gwy_field_find_row_shifts(field, mask, GWY_MASK_INCLUDE,
                                  method, 1, foundshifts);
        gwy_line_multiply(foundshifts, -1.0);
        gwy_line_accumulate(foundshifts);
        gwy_field_shift_rows(field, foundshifts);

        g_assert_cmpfloat(gwy_field_rms(field), <=, 1e-11);

        g_object_unref(mask);
        g_object_unref(foundshifts);
        g_object_unref(shifts);
        g_object_unref(field);
    }
    g_rand_free(rng);
}

void
test_field_row_level_mean(void)
{
    test_field_row_level_one(GWY_ROW_SHIFT_MEAN);
}

void
test_field_row_level_median(void)
{
    test_field_row_level_one(GWY_ROW_SHIFT_MEDIAN);
}

void
test_field_row_level_mean_diff(void)
{
    test_field_row_level_one(GWY_ROW_SHIFT_MEAN_DIFF);
}

void
test_field_row_level_median_diff(void)
{
    test_field_row_level_one(GWY_ROW_SHIFT_MEDIAN_DIFF);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
