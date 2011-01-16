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
    compare_properties(G_OBJECT(result), G_OBJECT(reference));

    for (guint i = 0; i < result->yres; i++) {
        gdouble *result_row = result->data + i*result->xres;
        gdouble *reference_row = reference->data + i*reference->xres;
        for (guint j = 0; j < result->xres; j++)
            g_assert_cmpfloat(result_row[j], ==, reference_row[j]);
    }
}

void
field_assert_numerically_equal(const GwyField *result,
                               const GwyField *reference,
                               gdouble eps)
{
    g_assert(GWY_IS_FIELD(result));
    g_assert(GWY_IS_FIELD(reference));
    g_assert_cmpuint(result->xres, ==, reference->xres);
    g_assert_cmpuint(result->yres, ==, reference->yres);

    guint xres = result->xres, yres = result->yres;
    for (guint i = 0; i < yres; i++) {
        for (guint j = 0; j < xres; j++) {
            gdouble value = result->data[i*xres + j],
                    ref = reference->data[i*xres + j];
            //g_printerr("[%u,%u] %g %g\n", j, i, value, ref);
            g_assert_cmpfloat(fabs(value - ref), <=, eps);
        }
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
test_field_data_changed(void)
{
    GwyField *field = gwy_field_new();
    guint counter = 0;
    g_signal_connect_swapped(field, "data-changed",
                             G_CALLBACK(record_signal), &counter);
    gwy_field_data_changed(field);
    g_assert_cmpuint(counter, ==, 1);
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
        GwyValueFormat *format = gwy_value_format_new();
        gwy_field_format_xy(field, GWY_VALUE_FORMAT_PLAIN, format);
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
        gwy_field_copy(reference, NULL, dest, 0, 0);
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
        GwyRectangle rectangle = { col, row, width, height };
        gwy_field_copy(source, &rectangle, dest, destcol, destrow);
        field_part_copy_dumb(source, col, row, width, height,
                             reference, destcol, destrow);
        test_field_assert_equal(dest, reference);
        g_object_unref(source);
        g_object_unref(dest);
        g_object_unref(reference);
    }

    GwyField *field = gwy_field_new_sized(37, 41, TRUE);
    field_randomize(field, rng);
    GwyField *dest = gwy_field_new_alike(field, FALSE);
    gwy_field_copy_full(field, dest);
    g_assert_cmpint(memcmp(field->data, dest->data,
                           field->xres*field->yres*sizeof(gdouble)), ==, 0);
    g_object_unref(dest);
    g_object_unref(field);

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
        GwyRectangle rectangle = { col, row, width, height };
        GwyField *part = gwy_field_new_part(source, &rectangle, TRUE);
        GwyField *reference = gwy_field_new_sized(width, height, FALSE);
        gwy_field_set_xreal(reference, source->xreal*width/xres);
        gwy_field_set_yreal(reference, source->yreal*height/yres);
        gwy_field_set_xoffset(reference, source->xreal*col/xres);
        gwy_field_set_yoffset(reference, source->yreal*row/yres);
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
field_flip_one(const gdouble *orig,
               const gdouble *reference,
               guint xres, guint yres,
               gboolean horizontally, gboolean vertically)
{
    GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
    gwy_assign(field->data, orig, xres*yres);
    gwy_field_flip(field, horizontally, vertically, FALSE);
    g_assert(field->xres == xres);
    g_assert(field->yres == yres);
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

    field_flip_one(orig1, orig1, xres1, yres1, FALSE, FALSE);
    field_flip_one(orig1, hflip1, xres1, yres1, TRUE, FALSE);
    field_flip_one(orig1, vflip1, xres1, yres1, FALSE, TRUE);
    field_flip_one(orig1, bflip1, xres1, yres1, TRUE, TRUE);

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

    field_flip_one(orig2, orig2, xres2, yres2, FALSE, FALSE);
    field_flip_one(orig2, hflip2, xres2, yres2, TRUE, FALSE);
    field_flip_one(orig2, vflip2, xres2, yres2, FALSE, TRUE);
    field_flip_one(orig2, bflip2, xres2, yres2, TRUE, TRUE);
}

static void
field_rotate_simple_one(const gdouble *orig,
                        const gdouble *reference,
                        guint xres, guint yres,
                        GwySimpleRotation rotation)
{
    GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
    gwy_assign(field->data, orig, xres*yres);
    GwyField *rotated = gwy_field_new_rotated_simple(field, rotation, FALSE);
    if (rotation % 180) {
        g_assert(field->xres == rotated->yres);
        g_assert(field->yres == rotated->xres);
    }
    else {
        g_assert(field->xres == rotated->xres);
        g_assert(field->yres == rotated->yres);
    }
    for (guint i = 0; i < xres*yres; i++) {
        g_assert_cmpfloat(rotated->data[i], ==, reference[i]);
    }
    g_object_unref(field);
    g_object_unref(rotated);
}

void
test_field_rotate_simple(void)
{
    const GwySimpleRotation rotations[] = {
        GWY_SIMPLE_ROTATE_NONE,
        GWY_SIMPLE_ROTATE_COUNTERCLOCKWISE,
        GWY_SIMPLE_ROTATE_UPSIDEDOWN,
        GWY_SIMPLE_ROTATE_CLOCKWISE,
    };
    enum { xres1 = 3, yres1 = 2, n1 = xres1*yres1 };
    const double data1[][n1] = {
        {
            1, 2, 3,
            4, 5, 6,
        },
        {
            3, 6,
            2, 5,
            1, 4,
        },
        {
            6, 5, 4,
            3, 2, 1,
        },
        {
            4, 1,
            5, 2,
            6, 3,
        },
    };

    for (guint i = 0; i < G_N_ELEMENTS(rotations); i++) {
        const gdouble *source = data1[i];
        for (guint j = 0; j < G_N_ELEMENTS(rotations); j++) {
            GwySimpleRotation rotation = rotations[j];
            const gdouble *reference = data1[(i + j) % G_N_ELEMENTS(rotations)];
            field_rotate_simple_one(source, reference,
                                    i % 2 ? yres1 : xres1,
                                    i % 2 ? xres1 : yres1,
                                    rotation);
        }
    }

    enum { xres2 = 5, yres2 = 1, n2 = xres2*yres2 };
    const double data2[][n2] = {
        {
            1, 2, 3, 4, 5,
        },
        {
            5,
            4,
            3,
            2,
            1,
        },
        {
            5, 4, 3, 2, 1,
        },
        {
            1,
            2,
            3,
            4,
            5,
        },
    };

    for (guint i = 0; i < G_N_ELEMENTS(rotations); i++) {
        const gdouble *source = data2[i];
        for (guint j = 0; j < G_N_ELEMENTS(rotations); j++) {
            GwySimpleRotation rotation = rotations[j];
            const gdouble *reference = data2[(i + j) % G_N_ELEMENTS(rotations)];
            field_rotate_simple_one(source, reference,
                                    i % 2 ? yres2 : xres2,
                                    i % 2 ? xres2 : yres2,
                                    rotation);
        }
    }
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
        area = gwy_field_surface_area(field, NULL, NULL, GWY_MASK_IGNORE);
        area_expected = planar_field_surface_area(field);
        g_assert_cmpfloat(fabs(area - area_expected)/area_expected, <=, 1e-9);

        gwy_field_set_xreal(field, 1.0);
        gwy_field_set_yreal(field, 1.0);
        area = gwy_field_surface_area(field, NULL, NULL, GWY_MASK_IGNORE);
        area_expected = planar_field_surface_area(field);
        g_assert_cmpfloat(fabs(area - area_expected)/area_expected, <=, 1e-9);

        field_randomize(field, rng);
        guint width = g_rand_int_range(rng, 1, xres+1);
        guint height = g_rand_int_range(rng, 1, yres+1);
        guint col = g_rand_int_range(rng, 0, xres-width+1);
        guint row = g_rand_int_range(rng, 0, yres-height+1);
        GwyRectangle rectangle = { col, row, width, height };

        GwyMaskField *mask = random_mask_field(xres, yres, rng);

        gwy_field_set_xreal(field, xres/sqrt(xres*yres));
        gwy_field_set_yreal(field, yres/sqrt(xres*yres));
        gdouble area_include, area_exclude, area_ignore;
        area_include = gwy_field_surface_area(field, &rectangle,
                                              mask, GWY_MASK_INCLUDE);
        area_exclude = gwy_field_surface_area(field, &rectangle,
                                              mask, GWY_MASK_EXCLUDE);
        area_ignore = gwy_field_surface_area(field, &rectangle,
                                             mask, GWY_MASK_IGNORE);
        g_assert_cmpfloat(fabs(area_include + area_exclude
                               - area_ignore)/area_ignore, <=, 1e-9);

        gwy_field_set_xreal(field, 1.0);
        gwy_field_set_yreal(field, 1.0);
        area_include = gwy_field_surface_area(field, &rectangle,
                                              mask, GWY_MASK_INCLUDE);
        area_exclude = gwy_field_surface_area(field, &rectangle,
                                              mask, GWY_MASK_EXCLUDE);
        area_ignore = gwy_field_surface_area(field, &rectangle,
                                             mask, GWY_MASK_IGNORE);
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
        mean = gwy_field_mean_full(field);
        mean_expected = 0.5*(alpha + beta);
        g_assert_cmpfloat(fabs(mean - mean_expected)/mean_expected, <=, 1e-9);

        field_randomize(field, rng);
        guint width = g_rand_int_range(rng, 1, xres+1);
        guint height = g_rand_int_range(rng, 1, yres+1);
        guint col = g_rand_int_range(rng, 0, xres-width+1);
        guint row = g_rand_int_range(rng, 0, yres-height+1);
        GwyRectangle rectangle = { col, row, width, height };

        GwyMaskField *mask = random_mask_field(xres, yres, rng);
        guint m = gwy_mask_field_part_count(mask, &rectangle, TRUE);
        guint n = gwy_mask_field_part_count(mask, &rectangle, FALSE);
        gdouble mean_include = gwy_field_mean(field, &rectangle,
                                              mask, GWY_MASK_INCLUDE);
        gdouble mean_exclude = gwy_field_mean(field, &rectangle,
                                              mask, GWY_MASK_EXCLUDE);
        gdouble mean_ignore = gwy_field_mean(field, &rectangle,
                                             mask, GWY_MASK_IGNORE);

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
        rms = gwy_field_rms_full(field);
        rms_expected = 0.5*sqrt((alpha*alpha*(1.0 - 1.0/yres/yres)
                                 + beta*beta*(1.0 - 1.0/xres/xres))/3.0);
        g_assert_cmpfloat(fabs(rms - rms_expected)/rms_expected, <=, 1e-9);

        field_randomize(field, rng);
        guint width = g_rand_int_range(rng, 1, xres+1);
        guint height = g_rand_int_range(rng, 1, yres+1);
        guint col = g_rand_int_range(rng, 0, xres-width+1);
        guint row = g_rand_int_range(rng, 0, yres-height+1);
        GwyRectangle rectangle = { col, row, width, height };

        GwyMaskField *mask = random_mask_field(xres, yres, rng);
        guint m = gwy_mask_field_part_count(mask, &rectangle, TRUE);
        guint n = gwy_mask_field_part_count(mask, &rectangle, FALSE);
        gdouble mean_include = gwy_field_mean(field, &rectangle,
                                              mask, GWY_MASK_INCLUDE);
        gdouble mean_exclude = gwy_field_mean(field, &rectangle,
                                              mask, GWY_MASK_EXCLUDE);
        gdouble rms_include = gwy_field_rms(field, &rectangle,
                                            mask, GWY_MASK_INCLUDE);
        gdouble rms_exclude = gwy_field_rms(field, &rectangle,
                                            mask, GWY_MASK_EXCLUDE);
        gdouble rms_ignore = gwy_field_rms(field, &rectangle,
                                           mask, GWY_MASK_IGNORE);

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
        gwy_field_statistics(field, NULL, NULL, GWY_MASK_IGNORE,
                             &mean, NULL, &rms, NULL, NULL);
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
        GwyRectangle rectangle = { col, row, width, height };

        GwyMaskField *mask = random_mask_field(xres, yres, rng);
        guint m = gwy_mask_field_part_count(mask, &rectangle, TRUE);
        guint n = gwy_mask_field_part_count(mask, &rectangle, FALSE);
        gdouble mean_include, rms_include;
        gwy_field_statistics(field, &rectangle, mask, GWY_MASK_INCLUDE,
                             &mean_include, NULL, &rms_include,
                             NULL, NULL);
        gdouble mean_exclude, rms_exclude;
        gwy_field_statistics(field, &rectangle, mask, GWY_MASK_EXCLUDE,
                             &mean_exclude, NULL, &rms_exclude,
                             NULL, NULL);
        gdouble mean_ignore, rms_ignore;
        gwy_field_statistics(field, &rectangle, mask, GWY_MASK_IGNORE,
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

void
test_field_level_plane(void)
{
    enum { max_size = 204 };
    GRand *rng = g_rand_new();
    g_rand_set_seed(rng, 42);
    gsize niter = 20;

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size);
        gdouble alpha = g_rand_double_range(rng, -100.0, 100.0);
        gdouble beta = g_rand_double_range(rng, -100.0, 100.0);
        GwyField *field = make_planar_field(xres, yres, alpha, beta);

        gdouble fa, fbx, fby, m1a, m1bx, m1by, m0a, m0bx, m0by;
        gdouble a_expected = 0.5*(alpha + beta);
        gdouble bx_expected = 0.5*beta*(1.0 - 1.0/xres);
        gdouble by_expected = 0.5*alpha*(1.0 - 1.0/yres);
        g_assert(gwy_field_fit_plane(field, NULL, NULL, GWY_MASK_IGNORE,
                                     &fa, &fbx, &fby));
        GwyMaskField *mask = random_mask_field(xres, yres, rng);
        g_assert(gwy_field_fit_plane(field, NULL, mask, GWY_MASK_EXCLUDE,
                                     &m0a, &m0bx, &m0by));
        g_assert(gwy_field_fit_plane(field, NULL, mask, GWY_MASK_INCLUDE,
                                     &m1a, &m1bx, &m1by));

        g_assert_cmpfloat(fabs((fa - a_expected)/a_expected), <=, 1e-13);
        g_assert_cmpfloat(fabs((fbx - bx_expected)/bx_expected), <=, 1e-13);
        g_assert_cmpfloat(fabs((fby - by_expected)/by_expected), <=, 1e-13);

        g_assert_cmpfloat(fabs((m0a - a_expected)/a_expected), <=, 1e-13);
        g_assert_cmpfloat(fabs((m0bx - bx_expected)/bx_expected), <=, 1e-13);
        g_assert_cmpfloat(fabs((m0by - by_expected)/by_expected), <=, 1e-13);

        g_assert_cmpfloat(fabs((m1a - a_expected)/a_expected), <=, 1e-13);
        g_assert_cmpfloat(fabs((m1bx - bx_expected)/bx_expected), <=, 1e-13);
        g_assert_cmpfloat(fabs((m1by - by_expected)/by_expected), <=, 1e-13);

        gwy_field_subtract_plane(field, a_expected, bx_expected, by_expected);
        g_assert(gwy_field_fit_plane(field, NULL, NULL, GWY_MASK_IGNORE,
                                     &fa, &fbx, &fby));
        g_assert_cmpfloat(fabs(fa), <=, 1e-13);
        g_assert_cmpfloat(fabs(fbx), <=, 1e-13);
        g_assert_cmpfloat(fabs(fby), <=, 1e-13);
        g_assert_cmpfloat(fabs(gwy_field_mean_full(field)), <=, 1e-13);
        g_assert_cmpfloat(fabs(gwy_field_rms_full(field)), <=, 1e-13);

        g_object_unref(mask);
        g_object_unref(field);
    }
    g_rand_free(rng);
}

void
test_field_level_poly(void)
{
    enum { max_size = 204 };
    const guint x_powers[] = { 0, 1, 0 };
    const guint y_powers[] = { 0, 0, 1 };

    GRand *rng = g_rand_new();
    g_rand_set_seed(rng, 42);
    gsize niter = 20;

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size);
        gdouble alpha = g_rand_double_range(rng, -100.0, 100.0);
        gdouble beta = g_rand_double_range(rng, -100.0, 100.0);
        GwyField *field = make_planar_field(xres, yres, alpha, beta);

        gdouble fc[3], m0[3], m1[3];
        gdouble a_expected = 0.5*(alpha + beta);
        gdouble bx_expected = 0.5*beta*(1.0 - 1.0/xres);
        gdouble by_expected = 0.5*alpha*(1.0 - 1.0/yres);
        g_assert(gwy_field_fit_poly(field, NULL, NULL, GWY_MASK_IGNORE,
                                    x_powers, y_powers, 3, fc));
        GwyMaskField *mask = random_mask_field(xres, yres, rng);
        g_assert(gwy_field_fit_poly(field, NULL, mask, GWY_MASK_EXCLUDE,
                                    x_powers, y_powers, 3, m0));
        g_assert(gwy_field_fit_poly(field, NULL, mask, GWY_MASK_INCLUDE,
                                    x_powers, y_powers, 3, m1));

        g_assert_cmpfloat(fabs((fc[0] - a_expected)/a_expected), <=, 1e-13);
        g_assert_cmpfloat(fabs((fc[1] - bx_expected)/bx_expected), <=, 1e-13);
        g_assert_cmpfloat(fabs((fc[2] - by_expected)/by_expected), <=, 1e-13);

        g_assert_cmpfloat(fabs((m0[0] - a_expected)/a_expected), <=, 1e-13);
        g_assert_cmpfloat(fabs((m0[1] - bx_expected)/bx_expected), <=, 1e-13);
        g_assert_cmpfloat(fabs((m0[2] - by_expected)/by_expected), <=, 1e-13);

        g_assert_cmpfloat(fabs((m1[0] - a_expected)/a_expected), <=, 1e-13);
        g_assert_cmpfloat(fabs((m1[1] - bx_expected)/bx_expected), <=, 1e-13);
        g_assert_cmpfloat(fabs((m1[2] - by_expected)/by_expected), <=, 1e-13);

        gdouble c[3] = { a_expected, bx_expected, by_expected };
        gwy_field_subtract_poly(field, x_powers, y_powers, 3, c);
        g_assert(gwy_field_fit_poly(field, NULL, NULL, GWY_MASK_IGNORE,
                                    x_powers, y_powers, 3, fc));
        g_assert_cmpfloat(fabs(fc[0]), <=, 1e-13);
        g_assert_cmpfloat(fabs(fc[1]), <=, 1e-13);
        g_assert_cmpfloat(fabs(fc[2]), <=, 1e-13);
        g_assert_cmpfloat(fabs(gwy_field_mean_full(field)), <=, 1e-13);
        g_assert_cmpfloat(fabs(gwy_field_rms_full(field)), <=, 1e-13);

        g_object_unref(mask);
        g_object_unref(field);
    }
    g_rand_free(rng);
}

// FIXME: The masked versions *may* fail if the mask is `unlucky' and does not
// contain enough contiguous pieces and/or they are located at the edges.
void
test_field_level_inclination(void)
{
    enum { max_size = 172 };

    GRand *rng = g_rand_new();
    g_rand_set_seed(rng, 42);
    gsize niter = 30;

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 100, max_size);
        guint yres = g_rand_int_range(rng, 100, max_size);
        gdouble alpha = g_rand_double_range(rng, -1.0, 1.0);
        gdouble beta = g_rand_double_range(rng, -1.0, 1.0);
        GwyField *plane = make_planar_field(xres, yres, alpha, beta);
        gdouble phi = g_rand_double_range(rng, 0, G_PI);
        gdouble width = g_rand_double_range(rng, 0.25, 1.5);
        gdouble height = g_rand_double_range(rng, -2.0, 2.0);
        GwyField *steps = gwy_field_new_sized(xres, yres, FALSE);

        for (guint i = 0; i < yres; i++) {
            gdouble y = 2*i/(yres - 1.0) - 1.0;
            for (guint j = 0; j < xres; j++) {
                gdouble x = 2*j/(xres - 1.0) - 1.0;
                gdouble z = height*floor((x*cos(phi) - y*sin(phi))/width);
                steps->data[i*xres + j] = z;
            }
        }

        GwyField *field = gwy_field_duplicate(steps);
        gwy_field_add_field(plane, NULL, field, 0, 0, 1.0);

        gdouble fbx, fby, m0bx, m0by, m1bx, m1by;
        gdouble a_expected = 0.5*(alpha + beta);
        gdouble bx_expected = 0.5*beta*(1.0 - 1.0/xres);
        gdouble by_expected = 0.5*alpha*(1.0 - 1.0/yres);
        g_assert(gwy_field_inclination(field, NULL, NULL, GWY_MASK_IGNORE,
                                       20, &fbx, &fby));
        GwyMaskField *mask = random_mask_field(xres, yres, rng);
        g_assert(gwy_field_inclination(field, NULL, mask, GWY_MASK_EXCLUDE,
                                       20, &m0bx, &m0by));
        g_assert(gwy_field_inclination(field, NULL, mask, GWY_MASK_INCLUDE,
                                       20, &m1bx, &m1by));
        // The required precision is quite low as the method is supposed to
        // recover the plane only approximately.
        g_assert_cmpfloat(fabs((fbx - bx_expected)/bx_expected), <=, 0.02);
        g_assert_cmpfloat(fabs((fby - by_expected)/by_expected), <=, 0.02);
        g_assert_cmpfloat(fabs((m0bx - bx_expected)/bx_expected), <=, 0.02);
        g_assert_cmpfloat(fabs((m0by - by_expected)/by_expected), <=, 0.02);
        g_assert_cmpfloat(fabs((m1bx - bx_expected)/bx_expected), <=, 0.02);
        g_assert_cmpfloat(fabs((m1by - by_expected)/by_expected), <=, 0.02);

        gwy_field_subtract_plane(field, a_expected, fbx, fby);
        g_assert(gwy_field_inclination(field, NULL, NULL, GWY_MASK_IGNORE,
                                       20, &fbx, &fby));
        g_assert(gwy_field_inclination(field, NULL, mask, GWY_MASK_EXCLUDE,
                                       20, &m0bx, &m0by));
        g_assert(gwy_field_inclination(field, NULL, mask, GWY_MASK_INCLUDE,
                                       20, &m1bx, &m1by));
        g_assert_cmpfloat(fabs(fbx), <=, 0.02);
        g_assert_cmpfloat(fabs(fby), <=, 0.02);
        g_assert_cmpfloat(fabs(m0bx), <=, 0.02);
        g_assert_cmpfloat(fabs(m0by), <=, 0.02);
        g_assert_cmpfloat(fabs(m1bx), <=, 0.02);
        g_assert_cmpfloat(fabs(m1by), <=, 0.02);

        g_object_unref(mask);
        g_object_unref(field);
        g_object_unref(steps);
        g_object_unref(plane);
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

        g_assert_cmpfloat(gwy_field_rms_full(field), <=, 1e-12);

        GwyMaskField *mask = random_mask_field(xres, yres, rng);
        gwy_field_find_row_shifts(field, mask, GWY_MASK_INCLUDE,
                                  method, 1, foundshifts);
        gwy_line_multiply(foundshifts, -1.0);
        gwy_line_accumulate(foundshifts);
        gwy_field_shift_rows(field, foundshifts);

        g_assert_cmpfloat(gwy_field_rms_full(field), <=, 1e-11);

        g_object_unref(mask);
        g_object_unref(foundshifts);
        g_object_unref(shifts);
        g_object_unref(field);
    }
    g_rand_free(rng);
}

void
test_field_level_row_mean(void)
{
    test_field_row_level_one(GWY_ROW_SHIFT_MEAN);
}

void
test_field_level_row_median(void)
{
    test_field_row_level_one(GWY_ROW_SHIFT_MEDIAN);
}

void
test_field_level_row_mean_diff(void)
{
    test_field_row_level_one(GWY_ROW_SHIFT_MEAN_DIFF);
}

void
test_field_level_row_median_diff(void)
{
    test_field_row_level_one(GWY_ROW_SHIFT_MEDIAN_DIFF);
}

void
test_field_compatibility_res(void)
{
    GwyField *field1 = gwy_field_new_sized(2, 3, FALSE);
    GwyField *field2 = gwy_field_new_sized(2, 2, FALSE);
    GwyField *field3 = gwy_field_new_sized(3, 2, FALSE);

    g_assert_cmpuint(gwy_field_is_incompatible(field1, field2,
                                               GWY_FIELD_COMPATIBLE_XRES),
                     ==, 0);
    g_assert_cmpuint(gwy_field_is_incompatible(field1, field2,
                                               GWY_FIELD_COMPATIBLE_YRES),
                     ==, GWY_FIELD_COMPATIBLE_YRES);
    g_assert_cmpuint(gwy_field_is_incompatible(field1, field2,
                                               GWY_FIELD_COMPATIBLE_RES),
                     ==, GWY_FIELD_COMPATIBLE_YRES);
    g_assert_cmpuint(gwy_field_is_incompatible(field2, field1,
                                               GWY_FIELD_COMPATIBLE_XRES),
                     ==, 0);
    g_assert_cmpuint(gwy_field_is_incompatible(field2, field1,
                                               GWY_FIELD_COMPATIBLE_YRES),
                     ==, GWY_FIELD_COMPATIBLE_YRES);
    g_assert_cmpuint(gwy_field_is_incompatible(field2, field1,
                                               GWY_FIELD_COMPATIBLE_RES),
                     ==, GWY_FIELD_COMPATIBLE_YRES);

    g_assert_cmpuint(gwy_field_is_incompatible(field2, field3,
                                               GWY_FIELD_COMPATIBLE_XRES),
                     ==, GWY_FIELD_COMPATIBLE_XRES);
    g_assert_cmpuint(gwy_field_is_incompatible(field2, field3,
                                               GWY_FIELD_COMPATIBLE_YRES),
                     ==, 0);
    g_assert_cmpuint(gwy_field_is_incompatible(field2, field3,
                                               GWY_FIELD_COMPATIBLE_RES),
                     ==, GWY_FIELD_COMPATIBLE_XRES);
    g_assert_cmpuint(gwy_field_is_incompatible(field3, field2,
                                               GWY_FIELD_COMPATIBLE_XRES),
                     ==, GWY_FIELD_COMPATIBLE_XRES);
    g_assert_cmpuint(gwy_field_is_incompatible(field3, field2,
                                               GWY_FIELD_COMPATIBLE_YRES),
                     ==, 0);
    g_assert_cmpuint(gwy_field_is_incompatible(field3, field2,
                                               GWY_FIELD_COMPATIBLE_RES),
                     ==, GWY_FIELD_COMPATIBLE_XRES);

    g_assert_cmpuint(gwy_field_is_incompatible(field1, field3,
                                               GWY_FIELD_COMPATIBLE_XRES),
                     ==, GWY_FIELD_COMPATIBLE_XRES);
    g_assert_cmpuint(gwy_field_is_incompatible(field1, field3,
                                               GWY_FIELD_COMPATIBLE_YRES),
                     ==, GWY_FIELD_COMPATIBLE_YRES);
    g_assert_cmpuint(gwy_field_is_incompatible(field1, field3,
                                               GWY_FIELD_COMPATIBLE_RES),
                     ==, GWY_FIELD_COMPATIBLE_RES);
    g_assert_cmpuint(gwy_field_is_incompatible(field3, field1,
                                               GWY_FIELD_COMPATIBLE_XRES),
                     ==, GWY_FIELD_COMPATIBLE_XRES);
    g_assert_cmpuint(gwy_field_is_incompatible(field3, field1,
                                               GWY_FIELD_COMPATIBLE_YRES),
                     ==, GWY_FIELD_COMPATIBLE_YRES);
    g_assert_cmpuint(gwy_field_is_incompatible(field3, field1,
                                               GWY_FIELD_COMPATIBLE_RES),
                     ==, GWY_FIELD_COMPATIBLE_RES);

    g_assert_cmpuint(gwy_field_is_incompatible(field1, field2,
                                               GWY_FIELD_COMPATIBLE_DX),
                     ==, 0);
    g_assert_cmpuint(gwy_field_is_incompatible(field1, field2,
                                               GWY_FIELD_COMPATIBLE_DY),
                     ==, GWY_FIELD_COMPATIBLE_DY);
    g_assert_cmpuint(gwy_field_is_incompatible(field1, field2,
                                               GWY_FIELD_COMPATIBLE_DXDY),
                     ==, GWY_FIELD_COMPATIBLE_DY);
    g_assert_cmpuint(gwy_field_is_incompatible(field2, field1,
                                               GWY_FIELD_COMPATIBLE_DX),
                     ==, 0);
    g_assert_cmpuint(gwy_field_is_incompatible(field2, field1,
                                               GWY_FIELD_COMPATIBLE_DY),
                     ==, GWY_FIELD_COMPATIBLE_DY);
    g_assert_cmpuint(gwy_field_is_incompatible(field2, field1,
                                               GWY_FIELD_COMPATIBLE_DXDY),
                     ==, GWY_FIELD_COMPATIBLE_DY);

    g_assert_cmpuint(gwy_field_is_incompatible(field2, field3,
                                               GWY_FIELD_COMPATIBLE_DX),
                     ==, GWY_FIELD_COMPATIBLE_DX);
    g_assert_cmpuint(gwy_field_is_incompatible(field2, field3,
                                               GWY_FIELD_COMPATIBLE_DY),
                     ==, 0);
    g_assert_cmpuint(gwy_field_is_incompatible(field2, field3,
                                               GWY_FIELD_COMPATIBLE_DXDY),
                     ==, GWY_FIELD_COMPATIBLE_DX);
    g_assert_cmpuint(gwy_field_is_incompatible(field3, field2,
                                               GWY_FIELD_COMPATIBLE_DX),
                     ==, GWY_FIELD_COMPATIBLE_DX);
    g_assert_cmpuint(gwy_field_is_incompatible(field3, field2,
                                               GWY_FIELD_COMPATIBLE_DY),
                     ==, 0);
    g_assert_cmpuint(gwy_field_is_incompatible(field3, field2,
                                               GWY_FIELD_COMPATIBLE_DXDY),
                     ==, GWY_FIELD_COMPATIBLE_DX);

    g_assert_cmpuint(gwy_field_is_incompatible(field1, field3,
                                               GWY_FIELD_COMPATIBLE_DX),
                     ==, GWY_FIELD_COMPATIBLE_DX);
    g_assert_cmpuint(gwy_field_is_incompatible(field1, field3,
                                               GWY_FIELD_COMPATIBLE_DY),
                     ==, GWY_FIELD_COMPATIBLE_DY);
    g_assert_cmpuint(gwy_field_is_incompatible(field1, field3,
                                               GWY_FIELD_COMPATIBLE_DXDY),
                     ==, GWY_FIELD_COMPATIBLE_DXDY);
    g_assert_cmpuint(gwy_field_is_incompatible(field3, field1,
                                               GWY_FIELD_COMPATIBLE_DX),
                     ==, GWY_FIELD_COMPATIBLE_DX);
    g_assert_cmpuint(gwy_field_is_incompatible(field3, field1,
                                               GWY_FIELD_COMPATIBLE_DY),
                     ==, GWY_FIELD_COMPATIBLE_DY);
    g_assert_cmpuint(gwy_field_is_incompatible(field3, field1,
                                               GWY_FIELD_COMPATIBLE_DXDY),
                     ==, GWY_FIELD_COMPATIBLE_DXDY);

    g_assert_cmpuint(gwy_field_is_incompatible(field1, field2,
                                               GWY_FIELD_COMPATIBLE_REAL),
                     ==, 0);
    g_assert_cmpuint(gwy_field_is_incompatible(field2, field1,
                                               GWY_FIELD_COMPATIBLE_REAL),
                     ==, 0);
    g_assert_cmpuint(gwy_field_is_incompatible(field1, field3,
                                               GWY_FIELD_COMPATIBLE_REAL),
                     ==, 0);
    g_assert_cmpuint(gwy_field_is_incompatible(field3, field1,
                                               GWY_FIELD_COMPATIBLE_REAL),
                     ==, 0);
    g_assert_cmpuint(gwy_field_is_incompatible(field2, field3,
                                               GWY_FIELD_COMPATIBLE_REAL),
                     ==, 0);
    g_assert_cmpuint(gwy_field_is_incompatible(field3, field2,
                                               GWY_FIELD_COMPATIBLE_REAL),
                     ==, 0);

    g_object_unref(field1);
    g_object_unref(field2);
    g_object_unref(field3);
}

void
test_field_compatibility_real(void)
{
    GwyField *field1 = gwy_field_new_sized(2, 2, FALSE);
    GwyField *field2 = gwy_field_new_sized(2, 2, FALSE);
    GwyField *field3 = gwy_field_new_sized(2, 2, FALSE);

    gwy_field_set_yreal(field1, 1.5);
    gwy_field_set_xreal(field3, 1.5);

    g_assert_cmpuint(gwy_field_is_incompatible(field1, field2,
                                               GWY_FIELD_COMPATIBLE_XREAL),
                     ==, 0);
    g_assert_cmpuint(gwy_field_is_incompatible(field1, field2,
                                               GWY_FIELD_COMPATIBLE_YREAL),
                     ==, GWY_FIELD_COMPATIBLE_YREAL);
    g_assert_cmpuint(gwy_field_is_incompatible(field1, field2,
                                               GWY_FIELD_COMPATIBLE_REAL),
                     ==, GWY_FIELD_COMPATIBLE_YREAL);
    g_assert_cmpuint(gwy_field_is_incompatible(field2, field1,
                                               GWY_FIELD_COMPATIBLE_XREAL),
                     ==, 0);
    g_assert_cmpuint(gwy_field_is_incompatible(field2, field1,
                                               GWY_FIELD_COMPATIBLE_YREAL),
                     ==, GWY_FIELD_COMPATIBLE_YREAL);
    g_assert_cmpuint(gwy_field_is_incompatible(field2, field1,
                                               GWY_FIELD_COMPATIBLE_REAL),
                     ==, GWY_FIELD_COMPATIBLE_YREAL);

    g_assert_cmpuint(gwy_field_is_incompatible(field2, field3,
                                               GWY_FIELD_COMPATIBLE_XREAL),
                     ==, GWY_FIELD_COMPATIBLE_XREAL);
    g_assert_cmpuint(gwy_field_is_incompatible(field2, field3,
                                               GWY_FIELD_COMPATIBLE_YREAL),
                     ==, 0);
    g_assert_cmpuint(gwy_field_is_incompatible(field2, field3,
                                               GWY_FIELD_COMPATIBLE_REAL),
                     ==, GWY_FIELD_COMPATIBLE_XREAL);
    g_assert_cmpuint(gwy_field_is_incompatible(field3, field2,
                                               GWY_FIELD_COMPATIBLE_XREAL),
                     ==, GWY_FIELD_COMPATIBLE_XREAL);
    g_assert_cmpuint(gwy_field_is_incompatible(field3, field2,
                                               GWY_FIELD_COMPATIBLE_YREAL),
                     ==, 0);
    g_assert_cmpuint(gwy_field_is_incompatible(field3, field2,
                                               GWY_FIELD_COMPATIBLE_REAL),
                     ==, GWY_FIELD_COMPATIBLE_XREAL);

    g_assert_cmpuint(gwy_field_is_incompatible(field1, field3,
                                               GWY_FIELD_COMPATIBLE_XREAL),
                     ==, GWY_FIELD_COMPATIBLE_XREAL);
    g_assert_cmpuint(gwy_field_is_incompatible(field1, field3,
                                               GWY_FIELD_COMPATIBLE_YREAL),
                     ==, GWY_FIELD_COMPATIBLE_YREAL);
    g_assert_cmpuint(gwy_field_is_incompatible(field1, field3,
                                               GWY_FIELD_COMPATIBLE_REAL),
                     ==, GWY_FIELD_COMPATIBLE_REAL);
    g_assert_cmpuint(gwy_field_is_incompatible(field3, field1,
                                               GWY_FIELD_COMPATIBLE_XREAL),
                     ==, GWY_FIELD_COMPATIBLE_XREAL);
    g_assert_cmpuint(gwy_field_is_incompatible(field3, field1,
                                               GWY_FIELD_COMPATIBLE_YREAL),
                     ==, GWY_FIELD_COMPATIBLE_YREAL);
    g_assert_cmpuint(gwy_field_is_incompatible(field3, field1,
                                               GWY_FIELD_COMPATIBLE_REAL),
                     ==, GWY_FIELD_COMPATIBLE_REAL);

    g_assert_cmpuint(gwy_field_is_incompatible(field1, field2,
                                               GWY_FIELD_COMPATIBLE_DX),
                     ==, 0);
    g_assert_cmpuint(gwy_field_is_incompatible(field1, field2,
                                               GWY_FIELD_COMPATIBLE_DY),
                     ==, GWY_FIELD_COMPATIBLE_DY);
    g_assert_cmpuint(gwy_field_is_incompatible(field1, field2,
                                               GWY_FIELD_COMPATIBLE_DXDY),
                     ==, GWY_FIELD_COMPATIBLE_DY);
    g_assert_cmpuint(gwy_field_is_incompatible(field2, field1,
                                               GWY_FIELD_COMPATIBLE_DX),
                     ==, 0);
    g_assert_cmpuint(gwy_field_is_incompatible(field2, field1,
                                               GWY_FIELD_COMPATIBLE_DY),
                     ==, GWY_FIELD_COMPATIBLE_DY);
    g_assert_cmpuint(gwy_field_is_incompatible(field2, field1,
                                               GWY_FIELD_COMPATIBLE_DXDY),
                     ==, GWY_FIELD_COMPATIBLE_DY);

    g_assert_cmpuint(gwy_field_is_incompatible(field2, field3,
                                               GWY_FIELD_COMPATIBLE_DX),
                     ==, GWY_FIELD_COMPATIBLE_DX);
    g_assert_cmpuint(gwy_field_is_incompatible(field2, field3,
                                               GWY_FIELD_COMPATIBLE_DY),
                     ==, 0);
    g_assert_cmpuint(gwy_field_is_incompatible(field2, field3,
                                               GWY_FIELD_COMPATIBLE_DXDY),
                     ==, GWY_FIELD_COMPATIBLE_DX);
    g_assert_cmpuint(gwy_field_is_incompatible(field3, field2,
                                               GWY_FIELD_COMPATIBLE_DX),
                     ==, GWY_FIELD_COMPATIBLE_DX);
    g_assert_cmpuint(gwy_field_is_incompatible(field3, field2,
                                               GWY_FIELD_COMPATIBLE_DY),
                     ==, 0);
    g_assert_cmpuint(gwy_field_is_incompatible(field3, field2,
                                               GWY_FIELD_COMPATIBLE_DXDY),
                     ==, GWY_FIELD_COMPATIBLE_DX);

    g_assert_cmpuint(gwy_field_is_incompatible(field1, field3,
                                               GWY_FIELD_COMPATIBLE_DX),
                     ==, GWY_FIELD_COMPATIBLE_DX);
    g_assert_cmpuint(gwy_field_is_incompatible(field1, field3,
                                               GWY_FIELD_COMPATIBLE_DY),
                     ==, GWY_FIELD_COMPATIBLE_DY);
    g_assert_cmpuint(gwy_field_is_incompatible(field1, field3,
                                               GWY_FIELD_COMPATIBLE_DXDY),
                     ==, GWY_FIELD_COMPATIBLE_DXDY);
    g_assert_cmpuint(gwy_field_is_incompatible(field3, field1,
                                               GWY_FIELD_COMPATIBLE_DX),
                     ==, GWY_FIELD_COMPATIBLE_DX);
    g_assert_cmpuint(gwy_field_is_incompatible(field3, field1,
                                               GWY_FIELD_COMPATIBLE_DY),
                     ==, GWY_FIELD_COMPATIBLE_DY);
    g_assert_cmpuint(gwy_field_is_incompatible(field3, field1,
                                               GWY_FIELD_COMPATIBLE_DXDY),
                     ==, GWY_FIELD_COMPATIBLE_DXDY);

    g_assert_cmpuint(gwy_field_is_incompatible(field1, field2,
                                               GWY_FIELD_COMPATIBLE_RES),
                     ==, 0);
    g_assert_cmpuint(gwy_field_is_incompatible(field2, field1,
                                               GWY_FIELD_COMPATIBLE_RES),
                     ==, 0);
    g_assert_cmpuint(gwy_field_is_incompatible(field1, field3,
                                               GWY_FIELD_COMPATIBLE_RES),
                     ==, 0);
    g_assert_cmpuint(gwy_field_is_incompatible(field3, field1,
                                               GWY_FIELD_COMPATIBLE_RES),
                     ==, 0);
    g_assert_cmpuint(gwy_field_is_incompatible(field2, field3,
                                               GWY_FIELD_COMPATIBLE_RES),
                     ==, 0);
    g_assert_cmpuint(gwy_field_is_incompatible(field3, field2,
                                               GWY_FIELD_COMPATIBLE_RES),
                     ==, 0);

    g_object_unref(field1);
    g_object_unref(field2);
    g_object_unref(field3);
}

void
test_field_compatibility_units(void)
{
    GwyField *field1 = gwy_field_new();
    GwyField *field2 = gwy_field_new();
    GwyField *field3 = gwy_field_new();

    gwy_unit_set_from_string(gwy_field_get_unit_xy(field1), "m", NULL);
    gwy_unit_set_from_string(gwy_field_get_unit_z(field3), "m", NULL);

    g_assert_cmpuint(gwy_field_is_incompatible(field1, field2,
                                               GWY_FIELD_COMPATIBLE_LATERAL),
                     ==, GWY_FIELD_COMPATIBLE_LATERAL);
    g_assert_cmpuint(gwy_field_is_incompatible(field2, field1,
                                               GWY_FIELD_COMPATIBLE_LATERAL),
                     ==, GWY_FIELD_COMPATIBLE_LATERAL);

    g_assert_cmpuint(gwy_field_is_incompatible(field2, field3,
                                               GWY_FIELD_COMPATIBLE_LATERAL),
                     ==, 0);
    g_assert_cmpuint(gwy_field_is_incompatible(field3, field2,
                                               GWY_FIELD_COMPATIBLE_LATERAL),
                     ==, 0);

    g_assert_cmpuint(gwy_field_is_incompatible(field1, field3,
                                               GWY_FIELD_COMPATIBLE_LATERAL),
                     ==, GWY_FIELD_COMPATIBLE_LATERAL);
    g_assert_cmpuint(gwy_field_is_incompatible(field3, field1,
                                               GWY_FIELD_COMPATIBLE_LATERAL),
                     ==, GWY_FIELD_COMPATIBLE_LATERAL);

    g_assert_cmpuint(gwy_field_is_incompatible(field1, field2,
                                               GWY_FIELD_COMPATIBLE_VALUE),
                     ==, 0);
    g_assert_cmpuint(gwy_field_is_incompatible(field2, field1,
                                               GWY_FIELD_COMPATIBLE_VALUE),
                     ==, 0);

    g_assert_cmpuint(gwy_field_is_incompatible(field2, field3,
                                               GWY_FIELD_COMPATIBLE_VALUE),
                     ==, GWY_FIELD_COMPATIBLE_VALUE);
    g_assert_cmpuint(gwy_field_is_incompatible(field3, field2,
                                               GWY_FIELD_COMPATIBLE_VALUE),
                     ==, GWY_FIELD_COMPATIBLE_VALUE);

    g_assert_cmpuint(gwy_field_is_incompatible(field1, field3,
                                               GWY_FIELD_COMPATIBLE_VALUE),
                     ==, GWY_FIELD_COMPATIBLE_VALUE);
    g_assert_cmpuint(gwy_field_is_incompatible(field3, field1,
                                               GWY_FIELD_COMPATIBLE_VALUE),
                     ==, GWY_FIELD_COMPATIBLE_VALUE);

    g_object_unref(field1);
    g_object_unref(field2);
    g_object_unref(field3);
}

void
test_field_arithmetic_cache(void)
{
    enum { xres = 2, yres = 2 };
    const gdouble data[xres*yres] = {
        -1, 0,
        1, 2,
    };
    gdouble min, max;

    GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
    gwy_assign(field->data, data, xres*yres);
    gwy_field_min_max_full(field, &min, &max);
    g_assert_cmpfloat(min, ==, -1.0);
    g_assert_cmpfloat(max, ==, 2.0);
    g_assert_cmpfloat(gwy_field_mean_full(field), ==, 0.5);
    g_assert_cmpfloat(fabs(gwy_field_rms_full(field) - 0.5*sqrt(5.0)), <, 1e-15);
    gwy_field_add(field, NULL, NULL, GWY_MASK_IGNORE, 0.0);
    gwy_field_min_max_full(field, &min, &max);
    g_assert_cmpfloat(min, ==, -1.0);
    g_assert_cmpfloat(max, ==, 2.0);
    g_assert_cmpfloat(gwy_field_mean_full(field), ==, 0.5);
    g_assert_cmpfloat(fabs(gwy_field_rms_full(field) - 0.5*sqrt(5.0)), <, 1e-15);
    gwy_field_add(field, NULL, NULL, GWY_MASK_IGNORE, -1.0);
    gwy_field_min_max_full(field, &min, &max);
    g_assert_cmpfloat(min, ==, -2.0);
    g_assert_cmpfloat(max, ==, 1.0);
    g_assert_cmpfloat(gwy_field_mean_full(field), ==, -0.5);
    g_assert_cmpfloat(fabs(gwy_field_rms_full(field) - 0.5*sqrt(5.0)), <, 1e-15);
    gwy_field_multiply(field, NULL, NULL, GWY_MASK_IGNORE, 1.0);
    gwy_field_min_max_full(field, &min, &max);
    g_assert_cmpfloat(min, ==, -2.0);
    g_assert_cmpfloat(max, ==, 1.0);
    g_assert_cmpfloat(gwy_field_mean_full(field), ==, -0.5);
    g_assert_cmpfloat(fabs(gwy_field_rms_full(field) - 0.5*sqrt(5.0)), <, 1e-15);
    gwy_field_multiply(field, NULL, NULL, GWY_MASK_IGNORE, -2.0);
    gwy_field_min_max_full(field, &min, &max);
    g_assert_cmpfloat(min, ==, -2.0);
    g_assert_cmpfloat(max, ==, 4.0);
    g_assert_cmpfloat(gwy_field_mean_full(field), ==, 1.0);
    g_assert_cmpfloat(fabs(gwy_field_rms_full(field) - sqrt(5.0)), <, 1e-15);
    g_object_unref(field);
}

static void
level_for_cf(GwyField *field,
             const GwyMaskField *mask,
             GwyMaskingType masking)
{
    for (guint i = 0; i < field->yres; i++) {
        GwyRectangle rectangle = { 0, i, field->xres, 1 };
        gdouble mean = gwy_field_mean(field, &rectangle, mask, masking);
        if (!isnan(mean))
            gwy_field_add(field, &rectangle, NULL, GWY_MASK_IGNORE, -mean);
    }
}

static GwyLine*
cf_dumb(const GwyField *field,
        const GwyRectangle *rectangle,
        const GwyMaskField *mask,
        GwyMaskingType masking,
        gboolean level,
        gboolean do_hhcf)
{
    GwyField *part = gwy_field_new_part(field, rectangle, FALSE);
    GwyLine *cf = gwy_line_new_sized(part->xres, TRUE);
    GwyLine *weights = gwy_line_new_sized(part->xres, TRUE);

    GwyMaskField *mpart = NULL;
    if (mask && masking != GWY_MASK_IGNORE)
        mpart = gwy_mask_field_new_part(mask, rectangle);

    if (level)
        level_for_cf(part, mpart, masking);

    for (guint i = 0; i < part->yres; i++) {
        const gdouble *row = part->data + i*part->xres;
        for (guint k = 0; k < part->xres; k++) {
            for (guint j = 0; j < part->xres - k; j++) {
                gboolean use_this_pixel = TRUE;
                if (mpart) {
                    guint m1 = gwy_mask_field_get(mpart, j, i);
                    guint m2 = gwy_mask_field_get(mpart, j + k, i);
                    if (masking == GWY_MASK_INCLUDE)
                        use_this_pixel = m1 && m2;
                    else
                        use_this_pixel = !m1 && !m2;
                }
                if (use_this_pixel) {
                    gdouble z1 = row[j];
                    gdouble z2 = row[j + k];
                    cf->data[k] += do_hhcf ? (z2 - z1)*(z2 - z1) : z1*z2;
                    weights->data[k]++;
                }
            }
        }
    }
    GWY_OBJECT_UNREF(mpart);
    for (guint k = 0; k < part->xres; k++)
        cf->data[k] = weights->data[k] ? cf->data[k]/weights->data[k] : 0.0;
    g_object_unref(part);
    g_object_unref(weights);

    return cf;
}

void
test_field_distributions_acf_full(void)
{
    enum { max_size = 134 };
    GRand *rng = g_rand_new();
    g_rand_set_seed(rng, 42);
    gsize niter = g_test_slow() ? 100 : 15;

    gwy_fft_load_wisdom();
    for (guint lvl = 0; lvl <= 1; lvl++) {
        for (guint iter = 0; iter < niter; iter++) {
            guint xres = g_rand_int_range(rng, 2, max_size);
            guint yres = g_rand_int_range(rng, 2, max_size);
            GwyField *field = gwy_field_new_sized(xres, yres, FALSE);

            field_randomize(field, rng);

            GwyLine *acf = gwy_field_row_acf(field,
                                             NULL, NULL, GWY_MASK_IGNORE, lvl);
            GwyMaskField *mask = gwy_mask_field_new_sized(xres, yres, TRUE);
            GwyLine *acf0 = gwy_field_row_acf(field, NULL,
                                              mask, GWY_MASK_EXCLUDE, lvl);
            gwy_mask_field_fill(mask, NULL, TRUE);
            GwyLine *acf1 = gwy_field_row_acf(field, NULL,
                                              mask, GWY_MASK_INCLUDE, lvl);
            GwyLine *dumb_acf = cf_dumb(field, NULL,
                                        NULL, GWY_MASK_IGNORE, lvl, FALSE);

            line_assert_numerically_equal(acf, dumb_acf, 1e-13);
            line_assert_numerically_equal(acf0, dumb_acf, 1e-13);
            line_assert_numerically_equal(acf1, dumb_acf, 1e-13);

            g_object_unref(acf);
            g_object_unref(acf1);
            g_object_unref(acf0);
            g_object_unref(dumb_acf);
            g_object_unref(mask);
            g_object_unref(field);
        }
    }

    g_rand_free(rng);
}

void
test_field_distributions_acf_masked(void)
{
    enum { max_size = 134 };
    GRand *rng = g_rand_new();
    g_rand_set_seed(rng, 42);
    gsize niter = g_test_slow() ? 100 : 15;

    gwy_fft_load_wisdom();
    for (guint lvl = 0; lvl <= 1; lvl++) {
        for (guint iter = 0; iter < niter; iter++) {
            guint xres = g_rand_int_range(rng, 2, max_size);
            guint yres = g_rand_int_range(rng, 2, max_size);
            GwyField *field = gwy_field_new_sized(xres, yres, FALSE);

            field_randomize(field, rng);

            GwyMaskField *mask = random_mask_field(xres, yres, rng);
            GwyLine *acf = gwy_field_row_acf(field, NULL,
                                             mask, GWY_MASK_IGNORE, lvl);
            GwyLine *acf0 = gwy_field_row_acf(field, NULL,
                                              mask, GWY_MASK_EXCLUDE, lvl);
            GwyLine *acf1 = gwy_field_row_acf(field, NULL,
                                              mask, GWY_MASK_INCLUDE, lvl);
            GwyLine *dumb_acf = cf_dumb(field, NULL,
                                        mask, GWY_MASK_IGNORE, lvl, FALSE);
            GwyLine *dumb_acf0 = cf_dumb(field, NULL,
                                         mask, GWY_MASK_EXCLUDE, lvl, FALSE);
            GwyLine *dumb_acf1 = cf_dumb(field, NULL,
                                         mask, GWY_MASK_INCLUDE, lvl, FALSE);

            line_assert_numerically_equal(acf, dumb_acf, 1e-13);
            line_assert_numerically_equal(acf0, dumb_acf0, 1e-13);
            line_assert_numerically_equal(acf1, dumb_acf1, 1e-13);

            g_object_unref(acf);
            g_object_unref(acf0);
            g_object_unref(acf1);
            g_object_unref(dumb_acf);
            g_object_unref(dumb_acf0);
            g_object_unref(dumb_acf1);
            g_object_unref(mask);
            g_object_unref(field);
        }
    }

    g_rand_free(rng);
}

void
test_field_distributions_acf_partial(void)
{
    enum { max_size = 134 };
    GRand *rng = g_rand_new();
    g_rand_set_seed(rng, 42);
    gsize niter = g_test_slow() ? 100 : 15;

    gwy_fft_load_wisdom();
    for (guint lvl = 0; lvl <= 1; lvl++) {
        for (guint iter = 0; iter < niter; iter++) {
            guint xres = g_rand_int_range(rng, 2, max_size);
            guint yres = g_rand_int_range(rng, 2, max_size);
            GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
            field_randomize(field, rng);

            guint width = g_rand_int_range(rng, 1, xres+1);
            guint height = g_rand_int_range(rng, 1, yres+1);
            guint col = g_rand_int_range(rng, 0, xres-width+1);
            guint row = g_rand_int_range(rng, 0, yres-height+1);
            GwyRectangle rectangle = { col, row, width, height };

            GwyMaskField *mask = random_mask_field(xres, yres, rng);
            GwyLine *acf = gwy_field_row_acf(field, &rectangle,
                                             mask, GWY_MASK_IGNORE, lvl);
            GwyLine *acf0 = gwy_field_row_acf(field, &rectangle,
                                              mask, GWY_MASK_EXCLUDE, lvl);
            GwyLine *acf1 = gwy_field_row_acf(field, &rectangle,
                                              mask, GWY_MASK_INCLUDE, lvl);
            GwyLine *dumb_acf = cf_dumb(field, &rectangle,
                                        mask, GWY_MASK_IGNORE, lvl, FALSE);
            GwyLine *dumb_acf0 = cf_dumb(field, &rectangle,
                                         mask, GWY_MASK_EXCLUDE, lvl, FALSE);
            GwyLine *dumb_acf1 = cf_dumb(field, &rectangle,
                                         mask, GWY_MASK_INCLUDE, lvl, FALSE);

            line_assert_numerically_equal(acf, dumb_acf, 1e-13);
            line_assert_numerically_equal(acf0, dumb_acf0, 1e-13);
            line_assert_numerically_equal(acf1, dumb_acf1, 1e-13);

            g_object_unref(acf);
            g_object_unref(acf0);
            g_object_unref(acf1);
            g_object_unref(dumb_acf);
            g_object_unref(dumb_acf0);
            g_object_unref(dumb_acf1);
            g_object_unref(mask);
            g_object_unref(field);
        }
    }

    g_rand_free(rng);
}

void
test_field_distributions_hhcf_full(void)
{
    enum { max_size = 134 };
    GRand *rng = g_rand_new();
    g_rand_set_seed(rng, 42);
    gsize niter = g_test_slow() ? 100 : 15;

    gwy_fft_load_wisdom();
    for (guint lvl = 0; lvl <= 1; lvl++) {
        for (guint iter = 0; iter < niter; iter++) {
            guint xres = g_rand_int_range(rng, 2, max_size);
            guint yres = g_rand_int_range(rng, 2, max_size);
            GwyField *field = gwy_field_new_sized(xres, yres, FALSE);

            field_randomize(field, rng);

            GwyLine *hhcf = gwy_field_row_hhcf(field, NULL,
                                               NULL, GWY_MASK_IGNORE, lvl);
            GwyMaskField *mask = gwy_mask_field_new_sized(xres, yres, TRUE);
            GwyLine *hhcf0 = gwy_field_row_hhcf(field, NULL,
                                                mask, GWY_MASK_EXCLUDE, lvl);
            gwy_mask_field_fill(mask, NULL, TRUE);
            GwyLine *hhcf1 = gwy_field_row_hhcf(field, NULL,
                                                mask, GWY_MASK_INCLUDE, lvl);
            GwyLine *dumb_hhcf = cf_dumb(field, NULL,
                                         NULL, GWY_MASK_IGNORE, lvl, TRUE);

            line_assert_numerically_equal(hhcf, dumb_hhcf, 1e-13);
            line_assert_numerically_equal(hhcf0, dumb_hhcf, 1e-13);
            line_assert_numerically_equal(hhcf1, dumb_hhcf, 1e-13);

            g_object_unref(hhcf);
            g_object_unref(hhcf1);
            g_object_unref(hhcf0);
            g_object_unref(dumb_hhcf);
            g_object_unref(mask);
            g_object_unref(field);
        }
    }

    g_rand_free(rng);
}

void
test_field_distributions_hhcf_masked(void)
{
    enum { max_size = 134 };
    GRand *rng = g_rand_new();
    g_rand_set_seed(rng, 42);
    gsize niter = g_test_slow() ? 100 : 15;

    gwy_fft_load_wisdom();
    for (guint lvl = 0; lvl <= 1; lvl++) {
        for (guint iter = 0; iter < niter; iter++) {
            guint xres = g_rand_int_range(rng, 2, max_size);
            guint yres = g_rand_int_range(rng, 2, max_size);
            GwyField *field = gwy_field_new_sized(xres, yres, FALSE);

            field_randomize(field, rng);

            GwyMaskField *mask = random_mask_field(xres, yres, rng);
            GwyLine *hhcf = gwy_field_row_hhcf(field, NULL,
                                               mask, GWY_MASK_IGNORE, lvl);
            GwyLine *hhcf0 = gwy_field_row_hhcf(field, NULL,
                                                mask, GWY_MASK_EXCLUDE, lvl);
            GwyLine *hhcf1 = gwy_field_row_hhcf(field, NULL,
                                                mask, GWY_MASK_INCLUDE, lvl);
            GwyLine *dumb_hhcf = cf_dumb(field, NULL,
                                         mask, GWY_MASK_IGNORE, lvl, TRUE);
            GwyLine *dumb_hhcf0 = cf_dumb(field, NULL,
                                          mask, GWY_MASK_EXCLUDE, lvl, TRUE);
            GwyLine *dumb_hhcf1 = cf_dumb(field, NULL,
                                          mask, GWY_MASK_INCLUDE, lvl, TRUE);

            line_assert_numerically_equal(hhcf, dumb_hhcf, 1e-13);
            line_assert_numerically_equal(hhcf0, dumb_hhcf0, 1e-13);
            line_assert_numerically_equal(hhcf1, dumb_hhcf1, 1e-13);

            g_object_unref(hhcf);
            g_object_unref(hhcf0);
            g_object_unref(hhcf1);
            g_object_unref(dumb_hhcf);
            g_object_unref(dumb_hhcf0);
            g_object_unref(dumb_hhcf1);
            g_object_unref(mask);
            g_object_unref(field);
        }
    }

    g_rand_free(rng);
}

void
test_field_distributions_hhcf_partial(void)
{
    enum { max_size = 134 };
    GRand *rng = g_rand_new();
    g_rand_set_seed(rng, 42);
    gsize niter = g_test_slow() ? 60 : 15;

    gwy_fft_load_wisdom();
    for (guint lvl = 0; lvl <= 1; lvl++) {
        for (guint iter = 0; iter < niter; iter++) {
            guint xres = g_rand_int_range(rng, 2, max_size);
            guint yres = g_rand_int_range(rng, 2, max_size);
            GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
            field_randomize(field, rng);

            guint width = g_rand_int_range(rng, 1, xres+1);
            guint height = g_rand_int_range(rng, 1, yres+1);
            guint col = g_rand_int_range(rng, 0, xres-width+1);
            guint row = g_rand_int_range(rng, 0, yres-height+1);
            GwyRectangle rectangle = { col, row, width, height };

            GwyMaskField *mask = random_mask_field(xres, yres, rng);
            GwyLine *hhcf = gwy_field_row_hhcf(field, &rectangle,
                                               mask, GWY_MASK_IGNORE, lvl);
            GwyLine *hhcf0 = gwy_field_row_hhcf(field, &rectangle,
                                                mask, GWY_MASK_EXCLUDE, lvl);
            GwyLine *hhcf1 = gwy_field_row_hhcf(field, &rectangle,
                                                mask, GWY_MASK_INCLUDE, lvl);
            GwyLine *dumb_hhcf = cf_dumb(field, &rectangle,
                                         mask, GWY_MASK_IGNORE, lvl, TRUE);
            GwyLine *dumb_hhcf0 = cf_dumb(field, &rectangle,
                                          mask, GWY_MASK_EXCLUDE, lvl, TRUE);
            GwyLine *dumb_hhcf1 = cf_dumb(field, &rectangle,
                                          mask, GWY_MASK_INCLUDE, lvl, TRUE);

            line_assert_numerically_equal(hhcf, dumb_hhcf, 1e-13);
            line_assert_numerically_equal(hhcf0, dumb_hhcf0, 1e-13);
            line_assert_numerically_equal(hhcf1, dumb_hhcf1, 1e-13);

            g_object_unref(hhcf);
            g_object_unref(hhcf0);
            g_object_unref(hhcf1);
            g_object_unref(dumb_hhcf);
            g_object_unref(dumb_hhcf0);
            g_object_unref(dumb_hhcf1);
            g_object_unref(mask);
            g_object_unref(field);
        }
    }

    g_rand_free(rng);
}

G_GNUC_UNUSED
static void
print_row(const gchar *name, const gdouble *data, gsize size)
{
    g_printerr("%s", name);
    while (size--)
        g_printerr(" %g", *(data++));
    g_printerr("\n");
}

G_GNUC_UNUSED
static void
print_field(const gchar *name, const GwyField *field)
{
    g_printerr("=====[ %s ]=====\n", name);
    for (guint i = 0; i < field->yres; i++) {
        g_printerr("[%02u]", i);
        for (guint j = 0; j < field->xres; j++) {
            g_printerr(" % .4f", field->data[i*field->xres + j]);
        }
        g_printerr("\n");
    }
}

static gdouble
exterior_value_dumb(const gdouble *data,
                    guint size,
                    guint stride,
                    gint pos,
                    GwyExteriorType exterior,
                    gdouble fill_value)
{
    // Interior
    if (pos >= 0 && (guint)pos < size)
        return data[stride*pos];

    if (exterior == GWY_EXTERIOR_UNDEFINED)
        return NAN;

    if (exterior == GWY_EXTERIOR_FIXED_VALUE)
        return fill_value;

    if (exterior == GWY_EXTERIOR_BORDER_EXTEND) {
        pos = CLAMP(pos, 0, (gint)size-1);
        return data[stride*pos];
    }

    if (exterior == GWY_EXTERIOR_PERIODIC) {
        pos = (pos + 10000*size) % size;
        return data[stride*pos];
    }

    if (exterior == GWY_EXTERIOR_MIRROR_EXTEND) {
        guint p = (pos + 10000*2*size) % (2*size);
        return data[stride*(p < size ? p : 2*size-1 - p)];
    }

    g_return_val_if_reached(NAN);
}

static gdouble
exterior_value_dumb_2d(GwyField *field,
                       gint xpos, gint ypos,
                       GwyExteriorType exterior,
                       gdouble fill_value)
{
    // Interior
    if (xpos >= 0 && xpos < (gint)field->xres
        && ypos >= 0 && ypos < (gint)field->yres)
        return field->data[xpos + ypos*field->xres];

    if (exterior == GWY_EXTERIOR_UNDEFINED)
        return NAN;

    if (exterior == GWY_EXTERIOR_FIXED_VALUE)
        return fill_value;

    if (exterior == GWY_EXTERIOR_BORDER_EXTEND) {
        xpos = CLAMP(xpos, 0, (gint)field->xres-1);
        ypos = CLAMP(ypos, 0, (gint)field->yres-1);
        return field->data[xpos + ypos*field->xres];
    }

    if (exterior == GWY_EXTERIOR_PERIODIC) {
        xpos = (xpos + 10000*field->xres) % field->xres;
        ypos = (ypos + 10000*field->yres) % field->yres;
        return field->data[xpos + ypos*field->xres];
    }

    if (exterior == GWY_EXTERIOR_MIRROR_EXTEND) {
        xpos = (xpos + 10000*2*field->xres) % (2*field->xres);
        if (xpos >= (gint)field->xres)
            xpos = 2*field->xres-1 - xpos;
        ypos = (ypos + 10000*2*field->yres) % (2*field->yres);
        if (ypos >= (gint)field->yres)
            ypos = 2*field->yres-1 - ypos;
        return field->data[xpos + ypos*field->xres];
    }

    g_return_val_if_reached(NAN);
}

static void
field_extend_one(GwyExteriorType exterior)
{
    enum { max_size = 35 };

    GRand *rng = g_rand_new();
    g_rand_set_seed(rng, 42);
    gsize niter = g_test_slow() ? 300 : 100;

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size);
        guint left = g_rand_int_range(rng, 0, 2*xres);
        guint right = g_rand_int_range(rng, 0, 2*xres);
        guint up = g_rand_int_range(rng, 0, 2*yres);
        guint down = g_rand_int_range(rng, 0, 2*yres);
        guint width = g_rand_int_range(rng, 1, xres+1);
        guint height = g_rand_int_range(rng, 1, yres+1);
        guint col = g_rand_int_range(rng, 0, xres-width+1);
        guint row = g_rand_int_range(rng, 0, yres-height+1);

        GwyField *source = gwy_field_new_sized(xres, yres, FALSE);
        field_randomize(source, rng);

        GwyRectangle rectangle = { col, row, width, height };
        GwyField *field = gwy_field_extend(source, &rectangle,
                                           left, right, up, down,
                                           exterior, G_E);

        g_assert_cmpuint(field->xres, ==, width + left + right);
        g_assert_cmpuint(field->yres, ==, height + up + down);

        for (guint i = 0; i < field->yres; i++) {
            gint ypos = (gint)i + (gint)row - (gint)up;
            for (guint j = 0; j < field->xres; j++) {
                gint xpos = (gint)j + (gint)col - (gint)left;

                if (exterior == GWY_EXTERIOR_UNDEFINED
                    && (xpos != CLAMP(xpos, 0, (gint)source->xres-1)
                        || ypos != CLAMP(ypos, 0, (gint)source->yres-1)))
                    continue;

                gdouble value = field->data[i*field->xres + j];
                gdouble reference = exterior_value_dumb_2d(source, xpos, ypos,
                                                           exterior, G_E);
                g_assert_cmpfloat(value, ==, reference);
            }
        }

        g_object_unref(field);
        g_object_unref(source);
    }
    g_rand_free(rng);
}

void
test_field_extend_undef(void)
{
    field_extend_one(GWY_EXTERIOR_UNDEFINED);
}

void
test_field_extend_fixed(void)
{
    field_extend_one(GWY_EXTERIOR_FIXED_VALUE);
}

void
test_field_extend_mirror(void)
{
    field_extend_one(GWY_EXTERIOR_MIRROR_EXTEND);
}

void
test_field_extend_border(void)
{
    field_extend_one(GWY_EXTERIOR_BORDER_EXTEND);
}

void
test_field_extend_periodic(void)
{
    field_extend_one(GWY_EXTERIOR_PERIODIC);
}

// The behaviour for interiors should not depend on the exterior type
static void
field_convolve_row_interior_one(GwyExteriorType exterior)
{
    enum { yres = 3 };
    GRand *rng = g_rand_new();
    g_rand_set_seed(rng, 42);

    for (guint res = 1; res <= 30; res++) {
        GwyField *source = gwy_field_new_sized(res, yres, FALSE);
        field_randomize(source, rng);

        GwyField *field = gwy_field_new_alike(source, FALSE);
        for (guint kres = 1; kres <= res; kres++) {
            guint k0 = (kres - 1)/2;
            GwyLine *kernel = gwy_line_new_sized(kres, TRUE);

            GwyRectangle rectangle = { k0, 0, res - kres/2 - k0, yres };
            for (guint pos = 0; pos < kres; pos++) {
                gwy_line_clear_full(kernel);
                kernel->data[pos] = 1.0;

                g_assert_cmpuint(rectangle.col + rectangle.width, <=, res);
                gwy_field_copy_full(source, field);
                gwy_field_row_convolve(field, &rectangle, field, kernel,
                                       exterior, G_E);

                // Everything outside @rectangle must be equal to @source.
                // Data inside @rectangle must be equal to @source data shifted
                // to the right by kres-1 - pos - (kres-1)/2.
                guint shift = (kres-1 - pos) - k0;
                for (guint i = 0; i < yres; i++) {
                    const gdouble *srow = source->data + i*res;
                    const gdouble *drow = field->data + i*res;
                    for (guint j = 0; j < res; j++) {
                        // Outside
                        if (j < rectangle.col
                            || j >= rectangle.col + rectangle.width)
                            g_assert_cmpfloat(drow[j], ==, srow[j]);
                        // Inside
                        else {
                            g_assert_cmpfloat(fabs(drow[j] - srow[j + shift]),
                                              <, 1e-14);
                        }
                    }
                }
            }
            g_object_unref(kernel);
        }
        g_object_unref(field);
        g_object_unref(source);
    }
    g_rand_free(rng);
}

static void
field_convolve_row_exterior_one(GwyExteriorType exterior)
{
    enum { yres = 3 };
    GRand *rng = g_rand_new();
    g_rand_set_seed(rng, 42);

    for (guint res = 1; res <= 30; res++) {
        guint niter = g_test_slow() ? MAX(4, 3*res*res) : MAX(4, res*res/2);

        GwyField *source = gwy_field_new_sized(res, yres, FALSE);
        field_randomize(source, rng);

        GwyField *field = gwy_field_new_alike(source, FALSE);
        for (guint iter = 0; iter < niter; iter++) {
            guint kres = g_rand_int_range(rng, 1, 2*res);
            guint k0 = (kres - 1)/2;
            GwyLine *kernel = gwy_line_new_sized(kres, TRUE);
            guint width = g_rand_int_range(rng, 0, res+1);
            guint col = g_rand_int_range(rng, 0, res-width+1);
            guint pos = g_rand_int_range(rng, 0, kres);
            GwyRectangle rectangle = { col, 0, width, yres };

            gwy_line_clear_full(kernel);
            kernel->data[pos] = 1.0;
            gwy_field_copy_full(source, field);
            gwy_field_row_convolve(field, &rectangle, field, kernel,
                                   exterior, G_E);

            // Everything outside @rectangle must be equal to @source.
            // Data inside @rectangle must be equal to @source data shifted
            // to the right by kres-1 - pos - (kres-1)/2, possibly extended.
            gint shift = ((gint)kres-1 - (gint)pos) - (gint)k0;
            for (guint i = 0; i < yres; i++) {
                const gdouble *srow = source->data + i*res;
                const gdouble *drow = field->data + i*res;
                for (guint j = 0; j < res; j++) {
                    // Outside
                    if (j < rectangle.col
                        || j >= rectangle.col + rectangle.width)
                        g_assert_cmpfloat(drow[j], ==, srow[j]);
                    // Inside
                    else {
                        gdouble refval = exterior_value_dumb(srow, res, 1,
                                                             (gint)j + shift,
                                                             exterior, G_E);
                        g_assert_cmpfloat(fabs(drow[j] - refval), <, 1e-14);
                    }
                }
            }
            g_object_unref(kernel);
        }
        g_object_unref(field);
        g_object_unref(source);
    }
    g_rand_free(rng);
}

void
test_field_convolve_row_interior_undef_direct(void)
{
    gwy_tune_algorithms("convolution-method", "direct");
    field_convolve_row_interior_one(GWY_EXTERIOR_UNDEFINED);
    gwy_tune_algorithms("convolution-method", "auto");
}

void
test_field_convolve_row_interior_mirror_direct(void)
{
    gwy_tune_algorithms("convolution-method", "direct");
    field_convolve_row_interior_one(GWY_EXTERIOR_MIRROR_EXTEND);
    gwy_tune_algorithms("convolution-method", "auto");
}

void
test_field_convolve_row_interior_mirror_fft(void)
{
    gwy_tune_algorithms("convolution-method", "fft");
    gwy_fft_load_wisdom();
    field_convolve_row_interior_one(GWY_EXTERIOR_MIRROR_EXTEND);
    gwy_tune_algorithms("convolution-method", "auto");
}

void
test_field_convolve_row_interior_border_direct(void)
{
    gwy_tune_algorithms("convolution-method", "direct");
    field_convolve_row_interior_one(GWY_EXTERIOR_BORDER_EXTEND);
    gwy_tune_algorithms("convolution-method", "auto");
}

void
test_field_convolve_row_interior_border_fft(void)
{
    gwy_tune_algorithms("convolution-method", "fft");
    gwy_fft_load_wisdom();
    field_convolve_row_interior_one(GWY_EXTERIOR_BORDER_EXTEND);
    gwy_tune_algorithms("convolution-method", "auto");
}

void
test_field_convolve_row_interior_periodic_direct(void)
{
    gwy_tune_algorithms("convolution-method", "direct");
    field_convolve_row_interior_one(GWY_EXTERIOR_PERIODIC);
    gwy_tune_algorithms("convolution-method", "auto");
}

void
test_field_convolve_row_interior_periodic_fft(void)
{
    gwy_tune_algorithms("convolution-method", "fft");
    gwy_fft_load_wisdom();
    field_convolve_row_interior_one(GWY_EXTERIOR_PERIODIC);
    gwy_tune_algorithms("convolution-method", "auto");
}

void
test_field_convolve_row_interior_fixed_direct(void)
{
    gwy_tune_algorithms("convolution-method", "direct");
    field_convolve_row_interior_one(GWY_EXTERIOR_FIXED_VALUE);
    gwy_tune_algorithms("convolution-method", "auto");
}

void
test_field_convolve_row_interior_fixed_fft(void)
{
    gwy_tune_algorithms("convolution-method", "fft");
    gwy_fft_load_wisdom();
    field_convolve_row_interior_one(GWY_EXTERIOR_FIXED_VALUE);
    gwy_tune_algorithms("convolution-method", "auto");
}

void
test_field_convolve_row_exterior_mirror_direct(void)
{
    gwy_tune_algorithms("convolution-method", "direct");
    field_convolve_row_exterior_one(GWY_EXTERIOR_MIRROR_EXTEND);
    gwy_tune_algorithms("convolution-method", "auto");
}

void
test_field_convolve_row_exterior_mirror_fft(void)
{
    gwy_tune_algorithms("convolution-method", "fft");
    gwy_fft_load_wisdom();
    field_convolve_row_exterior_one(GWY_EXTERIOR_MIRROR_EXTEND);
    gwy_tune_algorithms("convolution-method", "auto");
}

void
test_field_convolve_row_exterior_border_direct(void)
{
    gwy_tune_algorithms("convolution-method", "direct");
    field_convolve_row_exterior_one(GWY_EXTERIOR_BORDER_EXTEND);
    gwy_tune_algorithms("convolution-method", "auto");
}

void
test_field_convolve_row_exterior_border_fft(void)
{
    gwy_tune_algorithms("convolution-method", "fft");
    gwy_fft_load_wisdom();
    field_convolve_row_exterior_one(GWY_EXTERIOR_BORDER_EXTEND);
    gwy_tune_algorithms("convolution-method", "auto");
}

void
test_field_convolve_row_exterior_periodic_direct(void)
{
    gwy_tune_algorithms("convolution-method", "direct");
    field_convolve_row_exterior_one(GWY_EXTERIOR_PERIODIC);
    gwy_tune_algorithms("convolution-method", "auto");
}

void
test_field_convolve_row_exterior_periodic_fft(void)
{
    gwy_tune_algorithms("convolution-method", "fft");
    gwy_fft_load_wisdom();
    field_convolve_row_exterior_one(GWY_EXTERIOR_PERIODIC);
    gwy_tune_algorithms("convolution-method", "auto");
}

void
test_field_convolve_row_exterior_fixed_direct(void)
{
    gwy_tune_algorithms("convolution-method", "direct");
    field_convolve_row_exterior_one(GWY_EXTERIOR_FIXED_VALUE);
    gwy_tune_algorithms("convolution-method", "auto");
}

void
test_field_convolve_row_exterior_fixed_fft(void)
{
    gwy_tune_algorithms("convolution-method", "fft");
    gwy_fft_load_wisdom();
    field_convolve_row_exterior_one(GWY_EXTERIOR_FIXED_VALUE);
    gwy_tune_algorithms("convolution-method", "auto");
}

static void
field_convolve_simple_one(const GwyField *source,
                          const GwyField *kernel,
                          const GwyField *reference,
                          GwyExteriorType exterior)
{
    GwyField *field = gwy_field_new_alike(source, FALSE);
    gwy_field_convolve(source, NULL, field, kernel, exterior, G_E);
    field_assert_numerically_equal(field, reference, 1e-14);
    g_object_unref(field);
}

static GwyField*
field_make_deltafunction(guint xres, guint yres, guint xpos, guint ypos)
{
    GwyField *field = gwy_field_new_sized(xres, yres, TRUE);
    field->data[ypos*xres + xpos] = 1.0;
    gwy_field_invalidate(field);
    return field;
}

static void
field_convolve_field_simple_odd_odd(void)
{
    GwyField *field0 = field_make_deltafunction(3, 3, 0, 0);
    GwyField *field1 = field_make_deltafunction(3, 3, 1, 0);
    GwyField *field2 = field_make_deltafunction(3, 3, 2, 0);
    GwyField *field3 = field_make_deltafunction(3, 3, 0, 1);
    GwyField *field4 = field_make_deltafunction(3, 3, 1, 1);
    GwyField *field5 = field_make_deltafunction(3, 3, 2, 1);
    GwyField *field6 = field_make_deltafunction(3, 3, 0, 2);
    GwyField *field7 = field_make_deltafunction(3, 3, 1, 2);
    GwyField *field8 = field_make_deltafunction(3, 3, 2, 2);

    field_convolve_simple_one(field0, field0, field8, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field0, field1, field6, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field0, field2, field7, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field0, field3, field2, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field0, field4, field0, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field0, field5, field1, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field0, field6, field5, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field0, field7, field3, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field0, field8, field4, GWY_EXTERIOR_PERIODIC);

    field_convolve_simple_one(field1, field0, field6, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field1, field1, field7, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field1, field2, field8, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field1, field3, field0, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field1, field4, field1, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field1, field5, field2, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field1, field6, field3, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field1, field7, field4, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field1, field8, field5, GWY_EXTERIOR_PERIODIC);

    field_convolve_simple_one(field2, field0, field7, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field2, field1, field8, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field2, field2, field6, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field2, field3, field1, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field2, field4, field2, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field2, field5, field0, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field2, field6, field4, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field2, field7, field5, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field2, field8, field3, GWY_EXTERIOR_PERIODIC);

    field_convolve_simple_one(field3, field0, field2, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field3, field1, field0, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field3, field2, field1, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field3, field3, field5, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field3, field4, field3, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field3, field5, field4, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field3, field6, field8, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field3, field7, field6, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field3, field8, field7, GWY_EXTERIOR_PERIODIC);

    field_convolve_simple_one(field4, field0, field0, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field4, field1, field1, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field4, field2, field2, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field4, field3, field3, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field4, field4, field4, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field4, field5, field5, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field4, field6, field6, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field4, field7, field7, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field4, field8, field8, GWY_EXTERIOR_PERIODIC);

    field_convolve_simple_one(field5, field0, field1, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field5, field1, field2, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field5, field2, field0, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field5, field3, field4, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field5, field4, field5, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field5, field5, field3, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field5, field6, field7, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field5, field7, field8, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field5, field8, field6, GWY_EXTERIOR_PERIODIC);

    field_convolve_simple_one(field6, field0, field5, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field6, field1, field3, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field6, field2, field4, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field6, field3, field8, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field6, field4, field6, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field6, field5, field7, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field6, field6, field2, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field6, field7, field0, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field6, field8, field1, GWY_EXTERIOR_PERIODIC);

    field_convolve_simple_one(field7, field0, field3, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field7, field1, field4, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field7, field2, field5, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field7, field3, field6, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field7, field4, field7, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field7, field5, field8, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field7, field6, field0, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field7, field7, field1, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field7, field8, field2, GWY_EXTERIOR_PERIODIC);

    field_convolve_simple_one(field8, field0, field4, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field8, field1, field5, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field8, field2, field3, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field8, field3, field7, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field8, field4, field8, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field8, field5, field6, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field8, field6, field1, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field8, field7, field2, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field8, field8, field0, GWY_EXTERIOR_PERIODIC);

    g_object_unref(field8);
    g_object_unref(field7);
    g_object_unref(field6);
    g_object_unref(field5);
    g_object_unref(field4);
    g_object_unref(field3);
    g_object_unref(field2);
    g_object_unref(field1);
    g_object_unref(field0);
}

static void
field_convolve_field_simple_even_even(void)
{
    GwyField *field0 = field_make_deltafunction(2, 2, 0, 0);
    GwyField *field1 = field_make_deltafunction(2, 2, 1, 0);
    GwyField *field2 = field_make_deltafunction(2, 2, 0, 1);
    GwyField *field3 = field_make_deltafunction(2, 2, 1, 1);

    field_convolve_simple_one(field0, field0, field3, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field0, field1, field2, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field0, field2, field1, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field0, field3, field0, GWY_EXTERIOR_PERIODIC);

    field_convolve_simple_one(field1, field0, field2, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field1, field1, field3, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field1, field2, field0, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field1, field3, field1, GWY_EXTERIOR_PERIODIC);

    field_convolve_simple_one(field2, field0, field1, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field2, field1, field0, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field2, field2, field3, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field2, field3, field2, GWY_EXTERIOR_PERIODIC);

    field_convolve_simple_one(field3, field0, field0, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field3, field1, field1, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field3, field2, field2, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field3, field3, field3, GWY_EXTERIOR_PERIODIC);

    g_object_unref(field3);
    g_object_unref(field2);
    g_object_unref(field1);
    g_object_unref(field0);
}

static void
field_convolve_field_simple_odd_even(void)
{
    GwyField *field0 = field_make_deltafunction(3, 3, 0, 0);
    GwyField *field1 = field_make_deltafunction(3, 3, 1, 0);
    GwyField *field2 = field_make_deltafunction(3, 3, 2, 0);
    GwyField *field3 = field_make_deltafunction(3, 3, 0, 1);
    GwyField *field4 = field_make_deltafunction(3, 3, 1, 1);
    GwyField *field5 = field_make_deltafunction(3, 3, 2, 1);
    GwyField *field6 = field_make_deltafunction(3, 3, 0, 2);
    GwyField *field7 = field_make_deltafunction(3, 3, 1, 2);
    GwyField *field8 = field_make_deltafunction(3, 3, 2, 2);
    GwyField *kernel0 = field_make_deltafunction(2, 2, 0, 0);
    GwyField *kernel1 = field_make_deltafunction(2, 2, 1, 0);
    GwyField *kernel2 = field_make_deltafunction(2, 2, 0, 1);
    GwyField *kernel3 = field_make_deltafunction(2, 2, 1, 1);

    field_convolve_simple_one(field0, kernel0, field8, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field0, kernel1, field6, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field0, kernel2, field2, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field0, kernel3, field0, GWY_EXTERIOR_PERIODIC);

    field_convolve_simple_one(field1, kernel0, field6, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field1, kernel1, field7, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field1, kernel2, field0, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field1, kernel3, field1, GWY_EXTERIOR_PERIODIC);

    field_convolve_simple_one(field2, kernel0, field7, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field2, kernel1, field8, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field2, kernel2, field1, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field2, kernel3, field2, GWY_EXTERIOR_PERIODIC);

    field_convolve_simple_one(field3, kernel0, field2, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field3, kernel1, field0, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field3, kernel2, field5, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field3, kernel3, field3, GWY_EXTERIOR_PERIODIC);

    field_convolve_simple_one(field4, kernel0, field0, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field4, kernel1, field1, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field4, kernel2, field3, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field4, kernel3, field4, GWY_EXTERIOR_PERIODIC);

    field_convolve_simple_one(field5, kernel0, field1, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field5, kernel1, field2, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field5, kernel2, field4, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field5, kernel3, field5, GWY_EXTERIOR_PERIODIC);

    field_convolve_simple_one(field6, kernel0, field5, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field6, kernel1, field3, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field6, kernel2, field8, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field6, kernel3, field6, GWY_EXTERIOR_PERIODIC);

    field_convolve_simple_one(field7, kernel0, field3, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field7, kernel1, field4, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field7, kernel2, field6, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field7, kernel3, field7, GWY_EXTERIOR_PERIODIC);

    field_convolve_simple_one(field8, kernel0, field4, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field8, kernel1, field5, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field8, kernel2, field7, GWY_EXTERIOR_PERIODIC);
    field_convolve_simple_one(field8, kernel3, field8, GWY_EXTERIOR_PERIODIC);

    g_object_unref(kernel3);
    g_object_unref(kernel2);
    g_object_unref(kernel1);
    g_object_unref(kernel0);
    g_object_unref(field8);
    g_object_unref(field7);
    g_object_unref(field6);
    g_object_unref(field5);
    g_object_unref(field4);
    g_object_unref(field3);
    g_object_unref(field2);
    g_object_unref(field1);
    g_object_unref(field0);
}

void
test_field_convolve_field_simple_odd_odd_direct(void)
{
    gwy_tune_algorithms("convolution-method", "direct");
    field_convolve_field_simple_odd_odd();
    gwy_tune_algorithms("convolution-method", "auto");
}

void
test_field_convolve_field_simple_odd_odd_fft(void)
{
    gwy_tune_algorithms("convolution-method", "fft");
    field_convolve_field_simple_odd_odd();
    gwy_tune_algorithms("convolution-method", "auto");
}

void
test_field_convolve_field_simple_even_even_direct(void)
{
    gwy_tune_algorithms("convolution-method", "direct");
    field_convolve_field_simple_even_even();
    gwy_tune_algorithms("convolution-method", "auto");
}

void
test_field_convolve_field_simple_even_even_fft(void)
{
    gwy_tune_algorithms("convolution-method", "fft");
    field_convolve_field_simple_even_even();
    gwy_tune_algorithms("convolution-method", "auto");
}

void
test_field_convolve_field_simple_odd_even_direct(void)
{
    gwy_tune_algorithms("convolution-method", "direct");
    field_convolve_field_simple_odd_even();
    gwy_tune_algorithms("convolution-method", "auto");
}

void
test_field_convolve_field_simple_odd_even_fft(void)
{
    gwy_tune_algorithms("convolution-method", "fft");
    field_convolve_field_simple_odd_even();
    gwy_tune_algorithms("convolution-method", "auto");
}

static void
field_convolve_field_one(GwyExteriorType exterior)
{
    enum { max_size = 30, niter = 200 };

    GRand *rng = g_rand_new();
    g_rand_set_seed(rng, 46);

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size);
        guint width = g_rand_int_range(rng, 1, xres+1);
        guint height = g_rand_int_range(rng, 1, yres+1);
        guint col = g_rand_int_range(rng, 0, xres-width+1);
        guint row = g_rand_int_range(rng, 0, yres-height+1);
        guint kxres = g_rand_int_range(rng, 1, max_size);
        guint kyres = g_rand_int_range(rng, 1, max_size);
        guint xpos = g_rand_int_range(rng, 0, kxres);
        guint ypos = g_rand_int_range(rng, 0, kyres);
        guint kx0 = (kxres - 1)/2;
        guint ky0 = (kyres - 1)/2;

        GwyField *source = gwy_field_new_sized(xres, yres, FALSE);
        field_randomize(source, rng);
        GwyField *field = gwy_field_new_alike(source, FALSE);
        gwy_field_copy_full(source, field);
        GwyField *kernel = gwy_field_new_sized(kxres, kyres, TRUE);
        kernel->data[xpos + ypos*kxres] = 1.0;
        gwy_field_invalidate(kernel);
        GwyRectangle rectangle = { col, row, width, height };
        gwy_field_convolve(field, &rectangle, field, kernel, exterior, G_E);

        /*
        print_field("source", source);
        print_field("kernel", kernel);
        g_printerr("col=%u, row=%u, width=%u, height=%u\n", col, row, width, height);
        print_field("result", field);
        */

        // Everything outside @rectangle must be equal to @source.
        // Data inside @rectangle must be equal to @source data shifted
        // down by kyres-1 - ypos - (ykres-1)/2
        // and to the right by kxres-1 - xpos - (kxres-1)/2
        gint xshift = ((gint)kxres-1 - (gint)xpos) - (gint)kx0;
        gint yshift = ((gint)kyres-1 - (gint)ypos) - (gint)ky0;
        for (guint i = 0; i < yres; i++) {
            const gdouble *srow = source->data + i*xres;
            const gdouble *drow = field->data + i*xres;
            for (guint j = 0; j < xres; j++) {
                // Outside
                if (j < rectangle.col
                    || j >= rectangle.col + rectangle.width
                    || i < rectangle.row
                    || i >= rectangle.row + rectangle.height) {
                    g_assert_cmpfloat(drow[j], ==, srow[j]);
                }
                // Inside
                else {
                    if (exterior == GWY_EXTERIOR_UNDEFINED)
                        continue;
                    gdouble refval = exterior_value_dumb_2d(source,
                                                            (gint)j + xshift,
                                                            (gint)i + yshift,
                                                            exterior, G_E);
                    g_assert_cmpfloat(fabs(drow[j] - refval), <, 1e-14);
                }
            }
        }
        g_object_unref(kernel);
        g_object_unref(field);
        g_object_unref(source);
    }
    g_rand_free(rng);
}

void
test_field_convolve_field_undef_direct(void)
{
    gwy_tune_algorithms("convolution-method", "direct");
    field_convolve_field_one(GWY_EXTERIOR_UNDEFINED);
    gwy_tune_algorithms("convolution-method", "auto");
}

void
test_field_convolve_field_mirror_direct(void)
{
    gwy_tune_algorithms("convolution-method", "direct");
    field_convolve_field_one(GWY_EXTERIOR_MIRROR_EXTEND);
    gwy_tune_algorithms("convolution-method", "auto");
}

void
test_field_convolve_field_mirror_fft(void)
{
    gwy_tune_algorithms("convolution-method", "fft");
    field_convolve_field_one(GWY_EXTERIOR_MIRROR_EXTEND);
    gwy_tune_algorithms("convolution-method", "auto");
}

void
test_field_convolve_field_border_direct(void)
{
    gwy_tune_algorithms("convolution-method", "direct");
    field_convolve_field_one(GWY_EXTERIOR_BORDER_EXTEND);
    gwy_tune_algorithms("convolution-method", "auto");
}

void
test_field_convolve_field_border_fft(void)
{
    gwy_tune_algorithms("convolution-method", "fft");
    field_convolve_field_one(GWY_EXTERIOR_BORDER_EXTEND);
    gwy_tune_algorithms("convolution-method", "auto");
}

void
test_field_convolve_field_periodic_direct(void)
{
    gwy_tune_algorithms("convolution-method", "direct");
    field_convolve_field_one(GWY_EXTERIOR_PERIODIC);
    gwy_tune_algorithms("convolution-method", "auto");
}

void
test_field_convolve_field_periodic_fft(void)
{
    gwy_tune_algorithms("convolution-method", "fft");
    field_convolve_field_one(GWY_EXTERIOR_PERIODIC);
    gwy_tune_algorithms("convolution-method", "auto");
}

void
test_field_convolve_field_fixed_direct(void)
{
    gwy_tune_algorithms("convolution-method", "direct");
    field_convolve_field_one(GWY_EXTERIOR_FIXED_VALUE);
    gwy_tune_algorithms("convolution-method", "auto");
}

void
test_field_convolve_field_fixed_fft(void)
{
    gwy_tune_algorithms("convolution-method", "fft");
    field_convolve_field_one(GWY_EXTERIOR_FIXED_VALUE);
    gwy_tune_algorithms("convolution-method", "auto");
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
