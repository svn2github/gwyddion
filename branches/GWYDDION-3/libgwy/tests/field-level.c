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
 * Field levelling
 *
 ***************************************************************************/


void
test_field_level_plane(void)
{
    enum { max_size = 204 };
    GRand *rng = g_rand_new_with_seed(42);
    gsize niter = 20;

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size);
        gdouble alpha = g_rand_double_range(rng, -100.0, 100.0);
        gdouble beta = g_rand_double_range(rng, -100.0, 100.0);
        GwyField *field = field_make_planar(xres, yres, alpha, beta);

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

        gwy_assert_floatval(fa, a_expected, 1e-13*fabs(a_expected));
        gwy_assert_floatval(fbx, bx_expected, 1e-13*fabs(bx_expected));
        gwy_assert_floatval(fby, by_expected, 1e-13*fabs(by_expected));

        gwy_assert_floatval(m0a, a_expected, 1e-13*fabs(a_expected));
        gwy_assert_floatval(m0bx, bx_expected, 1e-13*fabs(bx_expected));
        gwy_assert_floatval(m0by, by_expected, 1e-13*fabs(by_expected));

        gwy_assert_floatval(m1a, a_expected, 1e-13*fabs(a_expected));
        gwy_assert_floatval(m1bx, bx_expected, 1e-13*fabs(bx_expected));
        gwy_assert_floatval(m1by, by_expected, 1e-13*fabs(by_expected));

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

    GRand *rng = g_rand_new_with_seed(42);
    gsize niter = 20;

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size);
        gdouble alpha = g_rand_double_range(rng, -100.0, 100.0);
        gdouble beta = g_rand_double_range(rng, -100.0, 100.0);
        GwyField *field = field_make_planar(xres, yres, alpha, beta);

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

        gwy_assert_floatval(fc[0], a_expected, 1e-13*fabs(a_expected));
        gwy_assert_floatval(fc[1], bx_expected, 1e-13*fabs(bx_expected));
        gwy_assert_floatval(fc[2], by_expected, 1e-13*fabs(by_expected));

        gwy_assert_floatval(m0[0], a_expected, 1e-13*fabs(a_expected));
        gwy_assert_floatval(m0[1], bx_expected, 1e-13*fabs(bx_expected));
        gwy_assert_floatval(m0[2], by_expected, 1e-13*fabs(by_expected));

        gwy_assert_floatval(m1[0], a_expected, 1e-13*fabs(a_expected));
        gwy_assert_floatval(m1[1], bx_expected, 1e-13*fabs(bx_expected));
        gwy_assert_floatval(m1[2], by_expected, 1e-13*fabs(by_expected));

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

    GRand *rng = g_rand_new_with_seed(42);
    gsize niter = 30;

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 100, max_size);
        guint yres = g_rand_int_range(rng, 100, max_size);
        gdouble alpha = g_rand_double_range(rng, -1.0, 1.0);
        gdouble beta = g_rand_double_range(rng, -1.0, 1.0);
        GwyField *plane = field_make_planar(xres, yres, alpha, beta);
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
        gwy_assert_floatval(fbx, bx_expected, 0.02*fabs(bx_expected));
        gwy_assert_floatval(fby, by_expected, 0.02*fabs(by_expected));
        gwy_assert_floatval(m0bx, bx_expected, 0.02*fabs(bx_expected));
        gwy_assert_floatval(m0by, by_expected, 0.02*fabs(by_expected));
        gwy_assert_floatval(m1bx, bx_expected, 0.02*fabs(bx_expected));
        gwy_assert_floatval(m1by, by_expected, 0.02*fabs(by_expected));

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
    GRand *rng = g_rand_new_with_seed(42);
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
        GwyLine *foundshifts1 = gwy_field_find_row_shifts(field,
                                                          NULL, GWY_MASK_IGNORE,
                                                          method, 1);
        gwy_line_multiply(foundshifts1, -1.0);
        gwy_line_accumulate(foundshifts1, FALSE);
        gwy_field_shift_rows(field, foundshifts1);
        g_object_unref(foundshifts1);

        g_assert_cmpfloat(gwy_field_rms_full(field), <=, 1e-12);

        GwyMaskField *mask = random_mask_field(xres, yres, rng);
        GwyLine *foundshifts2 = gwy_field_find_row_shifts(field,
                                                          mask, GWY_MASK_INCLUDE,
                                                          method, 1);
        gwy_line_multiply(foundshifts2, -1.0);
        gwy_line_accumulate(foundshifts2, FALSE);
        gwy_field_shift_rows(field, foundshifts2);
        g_object_unref(foundshifts2);

        g_assert_cmpfloat(gwy_field_rms_full(field), <=, 1e-11);

        g_object_unref(mask);
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

static void
field_level_rows_one(guint level)
{
    enum { max_size = 64 };
    GRand *rng = g_rand_new_with_seed(42);
    gsize niter = 30;

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size);
        GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
        field_randomize(field, rng);
        gdouble a = g_rand_double_range(rng, -G_PI, G_PI);
        gdouble b = g_rand_double_range(rng, -G_PI, G_PI);
        gdouble c = g_rand_double_range(rng, -G_PI, G_PI);
        gdouble d = g_rand_double_range(rng, -G_PI, G_PI);

        for (guint i = 0; i < yres; i++) {
            gdouble y = (i + 0.5)/yres - 0.5;
            for (guint j = 0; j < xres; j++) {
                gdouble x = (j + 0.5)/xres - 0.5;
                gwy_field_index(field, j, i) += sin(a + b*x + c*y + d*x*y);
            }
        }

        gwy_field_level_rows(field, level);
        for (guint i = 0; i < yres; i++) {
            gdouble s[4] = { 0.0, 0.0, 0.0, 0.0 };
            for (guint j = 0; j < xres; j++) {
                gdouble x = (j + 0.5)/xres - 0.5;
                gdouble z = gwy_field_index(field, j, i);
                s[0] += z;
                s[1] += z*x;
                s[2] += z*x*x;
                s[3] += z*x*x*x;
            }
            for (guint k = 0; k < 4; k++)
                s[k] /= xres;
            for (guint k = 0; k < level; k++) {
                gwy_assert_floatval(s[k], 0.0, 1e-14);
            }
            // In principle, this can fail.  But with negligible probability.
            for (guint k = level; k < MIN(4, xres); k++) {
                g_assert_cmpfloat(fabs(s[k]), >=, 1e-14);
            }
        }

        g_object_unref(field);
    }
    g_rand_free(rng);
}

void
test_field_level_rows_0(void)
{
    field_level_rows_one(0);
}

void
test_field_level_rows_1(void)
{
    field_level_rows_one(1);
}

void
test_field_level_rows_2(void)
{
    field_level_rows_one(2);
}

void
test_field_level_rows_3(void)
{
    field_level_rows_one(3);
}

static void
field_laplace_check_unmodif(const GwyField *field,
                            const GwyField *reference,
                            const guint *grains,
                            guint grain_id)
{
    guint n = field->xres * field->yres;

    for (guint k = 0; k < n; k++) {
        if ((grains[k] == grain_id) || (grain_id == G_MAXUINT && grains[k]))
            continue;
        g_assert_cmpfloat(field->data[k], ==, reference->data[k]);
    }
}

static void
field_laplace_check_local_error(const GwyField *field,
                                const guint *grains,
                                guint grain_id,
                                gdouble maxerr)
{
    guint xres = field->xres, yres = field->yres;

    for (guint k = 0; k < xres*yres; k++) {
        if ((grains[k] == grain_id) || (grain_id == G_MAXUINT && grains[k])) {
            guint n = 0, i = k/xres, j = k % xres;
            gdouble z = 0;
            if (i) {
                z += field->data[k-xres];
                n++;
            }
            if (j) {
                z += field->data[k-1];
                n++;
            }
            if (j+1 < xres) {
                z += field->data[k+1];
                n++;
            }
            if (i+1 < yres) {
                z += field->data[k+xres];
                n++;
            }
            z /= n;
            g_assert_cmpfloat(fabs(field->data[k] - z), <=, maxerr);
        }
    }
}

static void
field_laplace_check_absolute_error(const GwyField *field,
                                   const GwyField *reference,
                                   const guint *grains,
                                   guint grain_id,
                                   gdouble maxerr)
{
    guint xres = field->xres, yres = field->yres;

    for (guint k = 0; k < xres*yres; k++) {
        if ((grains[k] == grain_id) || (grain_id == G_MAXUINT && grains[k])) {
            g_assert_cmpfloat(fabs(reference->data[k] - field->data[k]),
                              <=, maxerr);
        }
    }
}

static void
field_laplace_invalidate_grain(const GwyField *field,
                               const guint *grains,
                               guint grain_id)
{
    guint xres = field->xres, yres = field->yres;

    for (guint k = 0; k < xres*yres; k++) {
        if ((grains[k] == grain_id) || (grain_id == G_MAXUINT && grains[k]))
            field->data[k] = NAN;
    }
}

void
test_field_level_laplace_random(void)
{
    enum { max_size = 214, niter = 600 };
    GRand *rng = g_rand_new_with_seed(42);

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size);
        gdouble prob = cbrt(g_rand_double(rng));
        GwyMaskField *mask = random_mask_field_prob(xres, yres, rng, prob);
        guint ngrains = gwy_mask_field_n_grains(mask);
        const guint *grains = gwy_mask_field_grain_numbers(mask);
        if (!ngrains) {
            g_object_unref(mask);
            continue;
        }
        GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
        field_randomize(field, rng);
        GwyField *reference = gwy_field_duplicate(field);

        const guint *sizes = gwy_mask_field_grain_sizes(mask);
        guint grain_id;
        if (!g_rand_int_range(rng, 0, 40))
            grain_id = G_MAXUINT;
        else {
            grain_id = g_rand_int_range(rng, 0, ngrains+1);
            for (guint i = 0; i < 10; i++) {
                if (grain_id && sizes[grain_id] > 1)
                    break;
                grain_id = g_rand_int_range(rng, 0, ngrains+1);
            }
        }
        field_laplace_invalidate_grain(field, grains, grain_id);
        gwy_field_laplace_solve(field, mask, grain_id);

        field_laplace_check_unmodif(field, reference, grains, grain_id);
        field_laplace_check_local_error(field, grains, grain_id, 1e-4);

        g_object_unref(mask);
        g_object_unref(field);
        g_object_unref(reference);
    }
    g_rand_free(rng);
}

static void
field_level_laplace_function_one(void (*function)(GwyField *field, GRand *rng),
                                 gdouble maxerr)
{
    enum { max_size = 250, niter = 50 };
    GRand *rng = g_rand_new_with_seed(42);

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 12, max_size);
        guint yres = g_rand_int_range(rng, 12, max_size);
        GwyMaskField *mask = gwy_mask_field_new_sized(xres, yres, TRUE);
        // Ensure Dirichlet boundary conditions.
        GwyFieldPart fpart = { 1, 1, xres-2, yres-2 };
        gwy_mask_field_fill(mask, &fpart, TRUE);
        guint grain_id = 1;

        GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
        function(field, rng);
        GwyField *reference = gwy_field_duplicate(field);

        gwy_field_fill(field, &fpart, NULL, GWY_MASK_IGNORE, NAN);
        gwy_field_laplace_solve(field, mask, grain_id);

        const guint *grains = gwy_mask_field_grain_numbers(mask);
        field_laplace_check_unmodif(field, reference, grains, grain_id);
        field_laplace_check_absolute_error(field, reference, grains, grain_id,
                                           maxerr);

        g_object_unref(mask);
        g_object_unref(field);
        g_object_unref(reference);
    }
    g_rand_free(rng);
}

static void
field_fill_linear(GwyField *field,
                  GRand *rng)
{
    gdouble xoff = g_rand_double(rng) - 0.5, yoff = g_rand_double(rng) - 0.5;
    gdouble bx = g_rand_double(rng) - 0.5, by = g_rand_double(rng) - 0.5;
    guint xres = field->xres, yres = field->yres;
    gdouble q = 2.0/MAX(xres - 1, yres - 1);

    for (guint i = 0; i < yres; i++) {
        gdouble y = q*i - 1.0 - yoff;
        for (guint j = 0; j < xres; j++) {
            gdouble x = q*j - 1.0 - xoff;
            field->data[i*xres + j] = bx*x + by*y;
        }
    }
    gwy_field_invalidate(field);
}

void
test_field_level_laplace_linear(void)
{
    field_level_laplace_function_one(field_fill_linear, 2e-3);
}

static void
field_fill_xy(GwyField *field,
              GRand *rng)
{
    gdouble xoff = g_rand_double(rng) - 0.5, yoff = g_rand_double(rng) - 0.5;
    guint xres = field->xres, yres = field->yres;
    gdouble q = 2.0/MAX(xres - 1, yres - 1);

    for (guint i = 0; i < yres; i++) {
        gdouble y = q*i - 1.0 - yoff;
        for (guint j = 0; j < xres; j++) {
            gdouble x = q*j - 1.0 - xoff;
            field->data[i*xres + j] = x*y;
        }
    }
    gwy_field_invalidate(field);
}

void
test_field_level_laplace_xy(void)
{
    field_level_laplace_function_one(field_fill_xy, 2e-3);
}

static void
field_fill_x2_y2(GwyField *field,
                 GRand *rng)
{
    gdouble xoff = g_rand_double(rng) - 0.5, yoff = g_rand_double(rng) - 0.5;
    guint xres = field->xres, yres = field->yres;
    gdouble q = 2.0/MAX(xres - 1, yres - 1);

    for (guint i = 0; i < yres; i++) {
        gdouble y = q*i - 1.0 - yoff;
        for (guint j = 0; j < xres; j++) {
            gdouble x = q*j - 1.0 - xoff;
            field->data[i*xres + j] = x*x - y*y;
        }
    }
    gwy_field_invalidate(field);
}

void
test_field_level_laplace_x2_y2(void)
{
    field_level_laplace_function_one(field_fill_x2_y2, 2e-3);
}

static void
field_fill_expx_cosy(GwyField *field,
                     GRand *rng)
{
    gdouble xoff = g_rand_double(rng) - 0.5, yoff = g_rand_double(rng) - 0.5;
    guint xres = field->xres, yres = field->yres;
    gdouble q = 2.0/MAX(xres - 1, yres - 1);

    for (guint i = 0; i < yres; i++) {
        gdouble y = q*i - 1.0 - yoff;
        for (guint j = 0; j < xres; j++) {
            gdouble x = q*j - 1.0 - xoff;
            field->data[i*xres + j] = exp(x) * cos(y);
        }
    }
    gwy_field_invalidate(field);
}

void
test_field_level_laplace_expx_cosy(void)
{
    field_level_laplace_function_one(field_fill_expx_cosy, 2e-3);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
