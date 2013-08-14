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
 * Field distributions
 *
 ***************************************************************************/

static void
fit_gaussian_psdf(const GwyLine *psdf,
                  gdouble *sigma,
                  gdouble *T)
{
    GwyFitFunc *fitfunc = gwy_fit_func_new("Gaussian");
    g_assert(GWY_IS_FIT_FUNC(fitfunc));

    guint n = psdf->res;
    GwyXY *xydata = g_new(GwyXY, n);
    for (guint i = 0; i < n; i++) {
        xydata[i].x = (i + 0.5)*gwy_line_dx(psdf);
        xydata[i].y = psdf->data[i];
    }
    gwy_fit_func_set_data(fitfunc, xydata, n);

    gboolean ok;
    gdouble params[4], errors[4];
    ok = gwy_fit_func_estimate(fitfunc, params);
    g_assert(ok);

    // Don't fit x0 and y0; they must be zeroes.
    guint x0_id = gwy_fit_func_param_number(fitfunc, "x₀");
    g_assert_cmpuint(x0_id, <, gwy_fit_func_n_params(fitfunc));
    params[x0_id] = 0.0;

    guint y0_id = gwy_fit_func_param_number(fitfunc, "y₀");
    g_assert_cmpuint(y0_id, <, gwy_fit_func_n_params(fitfunc));
    params[y0_id] = 0.0;

    GwyFitTask *fittask = gwy_fit_func_get_fit_task(fitfunc);
    g_assert(GWY_IS_FIT_TASK(fittask));
    gwy_fit_task_set_fixed_param(fittask, x0_id, TRUE);
    gwy_fit_task_set_fixed_param(fittask, y0_id, TRUE);

    GwyFitter *fitter = gwy_fit_task_get_fitter(fittask);
    g_assert(GWY_IS_FITTER(fitter));
    gwy_fitter_set_params(fitter, params);

    ok = gwy_fit_task_fit(fittask);
    g_assert(ok);
    gwy_fitter_get_params(fitter, params);
    // XXX: This seems to fail occasionally.  Which is especially suspicious
    // because we use stable random generators.
    ok = gwy_fit_task_param_errors(fittask, TRUE, errors);
    g_assert(ok);

    guint a_id = gwy_fit_func_param_number(fitfunc, "a");
    g_assert_cmpuint(a_id, <, gwy_fit_func_n_params(fitfunc));
    guint b_id = gwy_fit_func_param_number(fitfunc, "b");
    g_assert_cmpuint(b_id, <, gwy_fit_func_n_params(fitfunc));

    gdouble a = params[a_id], b = params[b_id];

    g_object_unref(fitfunc);
    g_free(xydata);

    *sigma = sqrt(a*b*sqrt(G_PI));
    *T = 2.0/b;
}

void
test_field_distributions_row_psdf_full(void)
{
    enum { size = 400 };
    gdouble dx = 50e-9;
    gdouble sigma = 20e-9;
    gdouble T = 300e-9;

    gwy_fft_load_wisdom();
    GRand *rng = g_rand_new_with_seed(42);

    GwyField *field = gwy_field_new_sized(size, size, FALSE);
    gwy_field_set_xreal(field, size*dx);
    gwy_field_set_yreal(field, size*dx);
    for (guint i = 0; i < size*size; i++)
        field->data[i] = 2.0*G_PI*sigma*T/dx*g_rand_double(rng);

    gwy_field_filter_gaussian(field, NULL, field, 0.5*T/dx, 0.5*T/dx,
                              GWY_EXTERIOR_PERIODIC, 0.0);

    GwyLine *psdf = gwy_field_row_psdf(field, NULL, NULL, GWY_MASK_IGNORE,
                                       GWY_WINDOWING_NONE, 1);

    // There is no independent method to verify the PSDF.  Try to fit it and
    // check if we find reasonable surface roughness parameters.
    gdouble sigma_fit, T_fit;
    fit_gaussian_psdf(psdf, &sigma_fit, &T_fit);
    gwy_assert_floatval(sigma_fit, sigma, 0.1*sigma);
    gwy_assert_floatval(T_fit, T, 0.1*T);

    g_object_unref(psdf);
    g_object_unref(field);
    g_rand_free(rng);
}

void
test_field_distributions_row_psdf_masked(void)
{
    enum { size = 400 };
    gdouble dx = 50e-9;
    gdouble sigma = 20e-9;
    gdouble T = 300e-9;

    gwy_fft_load_wisdom();
    GRand *rng = g_rand_new_with_seed(42);

    GwyField *field = gwy_field_new_sized(size, size, FALSE);
    gwy_field_set_xreal(field, size*dx);
    gwy_field_set_yreal(field, size*dx);
    for (guint i = 0; i < size*size; i++)
        field->data[i] = 2.0*G_PI*sigma*T/dx*g_rand_double(rng);

    gwy_field_filter_gaussian(field, NULL, field, 0.5*T/dx, 0.5*T/dx,
                              GWY_EXTERIOR_PERIODIC, 0.0);

    // Only 1/4 of pixels is used.
    GwyMaskField *mask = random_mask_field_prob(size, size, rng, 0.25);

    GwyLine *psdf = gwy_field_row_psdf(field, NULL, mask, GWY_MASK_INCLUDE,
                                       GWY_WINDOWING_NONE, 1);

    // There is no independent method to verify the PSDF.  Try to fit it and
    // check if we find reasonable surface roughness parameters.
    gdouble sigma_fit, T_fit;
    fit_gaussian_psdf(psdf, &sigma_fit, &T_fit);
    gwy_assert_floatval(sigma_fit, sigma, 0.1*sigma);
    gwy_assert_floatval(T_fit, T, 0.1*T);

    g_object_unref(psdf);
    g_object_unref(mask);
    g_object_unref(field);
    g_rand_free(rng);
}

void
test_field_distributions_row_psdf_rms(void)
{
    enum { max_size = 174, niter = 50 };
    GRand *rng = g_rand_new_with_seed(42);

    gwy_fft_load_wisdom();
    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 8, max_size);
        guint yres = g_rand_int_range(rng, 8, max_size);
        GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
        GwyWindowingType windowing = g_rand_int_range(rng,
                                                      0,
                                                      GWY_WINDOWING_KAISER25+1);

        field_randomize(field, rng);
        GwyLine *psdf = gwy_field_row_psdf(field, NULL, NULL, GWY_MASK_IGNORE,
                                           windowing, 0);
        gdouble rms_ref = sqrt(gwy_field_meansq_full(field));
        // sqrt(∫W dkx)
        gdouble i1 = gwy_line_mean_full(psdf)*psdf->res;
        psdf->data[0] = 0.0;
        gdouble i2 = gwy_line_mean_full(psdf)*psdf->res;
        gdouble rms_psdf = sqrt((i1 + i2)*gwy_line_dx(psdf));
        gwy_assert_floatval(rms_psdf, rms_ref, 2.0*rms_ref/xres);

        g_object_unref(psdf);
        g_object_unref(field);
    }

    g_rand_free(rng);
}

void
test_field_distributions_slope_simple_x(void)
{
    guint xres = 20, yres = 10;
    GwyField *field = gwy_field_new_sized(xres, yres, FALSE);

    // Construct a non-trivial field for which the discrete and continuous
    // are the same and at least some cases are known exactly.
    for (guint i = 0; i < yres; i++) {
        gdouble y = (gdouble)i/yres;
        for (guint j = 0; j < xres; j++) {
            gdouble x = (gdouble)j/xres;
            field->data[i*xres + j] = x*x + y*y;
        }
    }

    GwyLine *ddist, *cdist;
    gdouble real, integral;
    real = 2.0*(xres - 1)/xres;

    ddist = gwy_field_slope_dist(field, NULL, NULL, GWY_MASK_IGNORE,
                                 0.0, FALSE, FALSE, xres-1,
                                 0.0, real);
    g_assert_cmpuint(ddist->res, ==, xres-1);
    g_assert_cmpfloat(ddist->off, ==, 0.0);
    g_assert_cmpfloat(ddist->real, ==, real);
    integral = gwy_line_sum_full(ddist) * gwy_line_dx(ddist);
    gwy_assert_floatval(integral, 1.0, 1e-14);

    cdist = gwy_field_slope_dist(field, NULL, NULL, GWY_MASK_IGNORE,
                                 0.0, FALSE, TRUE, xres-1,
                                 0.0, real);
    g_assert_cmpuint(cdist->res, ==, xres-1);
    g_assert_cmpfloat(cdist->off, ==, 0.0);
    g_assert_cmpfloat(cdist->real, ==, real);
    integral = gwy_line_sum_full(cdist) * gwy_line_dx(cdist);
    gwy_assert_floatval(integral, 1.0, 1e-14);

    line_assert_numerically_equal(cdist, ddist, 1e-14);

    for (guint i = 1; i < cdist->res; i++) {
        gwy_assert_floatval(cdist->data[i-1], cdist->data[i], 1e-14);
    }

    g_object_unref(cdist);
    g_object_unref(ddist);
    g_object_unref(field);
}

void
test_field_distributions_slope_simple_y(void)
{
    guint xres = 20, yres = 10;
    GwyField *field = gwy_field_new_sized(xres, yres, FALSE);

    // Construct a non-trivial field for which the discrete and continuous
    // are the same and at least some cases are known exactly.
    for (guint i = 0; i < yres; i++) {
        gdouble y = (gdouble)i/yres;
        for (guint j = 0; j < xres; j++) {
            gdouble x = (gdouble)j/xres;
            field->data[i*xres + j] = x*x + y*y;
        }
    }

    GwyLine *ddist, *cdist;
    gdouble real, integral;
    real = 2.0*(yres - 1)/yres;

    ddist = gwy_field_slope_dist(field, NULL, NULL, GWY_MASK_IGNORE,
                                 G_PI/2.0, FALSE, FALSE, yres-1,
                                 0.0, real);
    g_assert_cmpuint(ddist->res, ==, yres-1);
    g_assert_cmpfloat(ddist->off, ==, 0.0);
    g_assert_cmpfloat(ddist->real, ==, real);
    integral = gwy_line_sum_full(ddist) * gwy_line_dx(ddist);
    gwy_assert_floatval(integral, 1.0, 1e-14);

    cdist = gwy_field_slope_dist(field, NULL, NULL, GWY_MASK_IGNORE,
                                 G_PI/2.0, FALSE, TRUE, yres-1,
                                 0.0, real);
    g_assert_cmpuint(cdist->res, ==, yres-1);
    g_assert_cmpfloat(cdist->off, ==, 0.0);
    g_assert_cmpfloat(cdist->real, ==, real);
    integral = gwy_line_sum_full(cdist) * gwy_line_dx(cdist);
    gwy_assert_floatval(integral, 1.0, 1e-14);

    line_assert_numerically_equal(cdist, ddist, 1e-14);

    for (guint i = 1; i < cdist->res; i++) {
        gwy_assert_floatval(cdist->data[i-1], cdist->data[i], 1e-14);
    }

    g_object_unref(cdist);
    g_object_unref(ddist);
    g_object_unref(field);
}

void
test_field_distributions_slope_simple_oblique(void)
{
    guint xres = 20, yres = 10;
    GwyField *field = gwy_field_new_sized(xres, yres, FALSE);

    // Construct a non-trivial field for which the discrete and continuous
    // are the same and at least some cases are known exactly.
    for (guint i = 0; i < yres; i++) {
        gdouble y = (gdouble)i/yres;
        for (guint j = 0; j < xres; j++) {
            gdouble x = (gdouble)j/xres;
            field->data[i*xres + j] = x*x + y*y;
        }
    }

    GwyLine *ddist, *cdist;
    gdouble integral;

    ddist = gwy_field_slope_dist(field, NULL, NULL, GWY_MASK_IGNORE,
                                 G_PI/2.0, FALSE, FALSE, 0,
                                 0.0, 0.0);
    integral = gwy_line_sum_full(ddist) * gwy_line_dx(ddist);
    gwy_assert_floatval(integral, 1.0, 1e-14);

    cdist = gwy_field_slope_dist(field, NULL, NULL, GWY_MASK_IGNORE,
                                 G_PI/2.0, FALSE, TRUE, 0,
                                 0.0, 0.0);
    integral = gwy_line_sum_full(cdist) * gwy_line_dx(cdist);
    gwy_assert_floatval(integral, 1.0, 1e-14);

    line_assert_numerically_equal(cdist, ddist, 1e-14);

    // FIXME: The expected values are a bit hard to calculate.

    g_object_unref(cdist);
    g_object_unref(ddist);
    g_object_unref(field);
}

void
test_field_distributions_slope_nonsquare(void)
{
    guint res = 2;
    gdouble aspect = 2.0;
    gdouble expected_max = 4/hypot(1.0, aspect);
    GwyField *field = gwy_field_new_sized(res, res, TRUE);
    field->data[res*res - 1] = 1.0;

    GwyLine *dist_wide, *dist_tall;
    gdouble integral;

    gwy_field_set_xreal(field, aspect);
    gwy_field_set_yreal(field, 1.0);
    dist_wide = gwy_field_slope_dist(field, NULL, NULL, GWY_MASK_IGNORE,
                                     atan(1.0/aspect), FALSE, FALSE, 0,
                                     0.0, 0.0);
    integral = gwy_line_sum_full(dist_wide) * gwy_line_dx(dist_wide);
    gwy_assert_floatval(integral, 1.0, 1e-14);
    g_assert_cmpfloat(fabs(dist_wide->off), <=, 1e-14);
    gwy_assert_floatval(dist_wide->real, expected_max, 1e-14);

    gwy_field_set_xreal(field, 1.0);
    gwy_field_set_yreal(field, aspect);
    dist_tall = gwy_field_slope_dist(field, NULL, NULL, GWY_MASK_IGNORE,
                                     atan(aspect), FALSE, FALSE, 0,
                                     0.0, 0.0);
    integral = gwy_line_sum_full(dist_tall) * gwy_line_dx(dist_tall);
    gwy_assert_floatval(integral, 1.0, 1e-14);
    g_assert_cmpfloat(fabs(dist_tall->off), <=, 1e-14);
    gwy_assert_floatval(dist_tall->real, expected_max, 1e-14);

    line_assert_numerically_equal(dist_wide, dist_tall, 1e-14);

    g_object_unref(dist_tall);
    g_object_unref(dist_wide);
    g_object_unref(field);
}

void
test_field_distributions_value_discr_full(void)
{
    enum { max_size = 134, niter = 200 };
    GRand *rng = g_rand_new_with_seed(42);

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 5, max_size);
        guint yres = g_rand_int_range(rng, 5, max_size);
        GwyField *field = gwy_field_new_sized(xres, yres, FALSE);

        field_randomize(field, rng);

        gdouble min, max;
        gwy_field_min_max_full(field, &min, &max);
        GwyLine *vdist = gwy_field_value_dist(field,
                                              NULL, NULL, GWY_MASK_IGNORE,
                                              FALSE, FALSE, 0, 0.0, 0.0);
        g_assert_cmpfloat(vdist->off, <=, min);
        g_assert_cmpfloat(vdist->off + vdist->real, >=, max);
        g_assert_cmpfloat(vdist->data[0], >, 0.0);
        g_assert_cmpfloat(vdist->data[vdist->res-1], >, 0.0);
        gdouble m = gwy_line_mean_full(vdist);
        gdouble s = gwy_line_sum_full(vdist);
        gdouble integral = gwy_line_dx(vdist)*s;
        // This is 5σ error bar.  Quite large, but we do not want to ever get
        // outside of it.
        gdouble eps = 5.0*s*sqrt(m/(xres*yres*vdist->res)*(1.0 - m/vdist->res));
        gwy_assert_floatval(integral, 1.0, 1e-12);
        for (guint i = 0; i < vdist->res; i++) {
            g_assert_cmpfloat(vdist->data[i], >=, 0.0);
            g_assert_cmpfloat(fabs(vdist->data[i] - m), <=, eps);
        }

        GwyMaskField *mask = gwy_mask_field_new_sized(xres, yres, TRUE);
        GwyLine *vdist0 = gwy_field_value_dist(field,
                                               NULL, mask, GWY_MASK_EXCLUDE,
                                               FALSE, FALSE, 0, 0.0, 0.0);
        gwy_mask_field_fill(mask, NULL, TRUE);
        GwyLine *vdist1 = gwy_field_value_dist(field,
                                               NULL, mask, GWY_MASK_INCLUDE,
                                               FALSE, FALSE, 0, 0.0, 0.0);
        line_assert_equal(vdist0, vdist);
        line_assert_equal(vdist1, vdist);

        g_object_unref(vdist);
        g_object_unref(vdist1);
        g_object_unref(vdist0);
        g_object_unref(mask);
        g_object_unref(field);
    }

    g_rand_free(rng);
}

void
test_field_distributions_value_discr_range(void)
{
    enum { max_size = 134, niter = 200 };
    GRand *rng = g_rand_new_with_seed(42);

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 5, max_size);
        guint yres = g_rand_int_range(rng, 5, max_size);
        GwyField *field = gwy_field_new_sized(xres, yres, FALSE);

        field_randomize(field, rng);

        gdouble min, max;
        gwy_field_min_max_full(field, &min, &max);
        GwyLine *vdist = gwy_field_value_dist(field,
                                              NULL, NULL, GWY_MASK_IGNORE,
                                              FALSE, FALSE, 0, min, max);
        g_assert_cmpfloat(vdist->off, <=, min);
        g_assert_cmpfloat(vdist->off + vdist->real, >=, max);
        g_assert_cmpfloat(vdist->data[0], >, 0.0);
        g_assert_cmpfloat(vdist->data[vdist->res-1], >, 0.0);
        gdouble m = gwy_line_mean_full(vdist);
        gdouble s = gwy_line_sum_full(vdist);
        gdouble integral = gwy_line_dx(vdist)*s;
        // This is 5σ error bar.  Quite large, but we do not want to ever get
        // outside of it.
        gdouble eps = 5.0*s*sqrt(m/(xres*yres*vdist->res)*(1.0 - m/vdist->res));
        gwy_assert_floatval(integral, 1.0, 1e-12);
        for (guint i = 0; i < vdist->res; i++) {
            g_assert_cmpfloat(vdist->data[i], >=, 0.0);
            g_assert_cmpfloat(fabs(vdist->data[i] - m), <=, eps);
        }

        guint ncut1 = vdist->res/3, ncut2 = 2*vdist->res/3;
        gdouble cutoff1 = vdist->off + gwy_line_dx(vdist)*ncut1,
                cutoff2 = vdist->off + gwy_line_dx(vdist)*ncut2;
        // XXX: If a value coincides *exactly* (at least within rounding
        // errors) with a cut-off it can end up in either part of the
        // distribution.
        GwyLine *pvdist0 = gwy_field_value_dist(field, NULL, NULL, 0,
                                                FALSE, FALSE,
                                                ncut1, min, cutoff1);
        GwyLine *pvdist1 = gwy_field_value_dist(field, NULL, NULL, 0,
                                                FALSE, FALSE,
                                                ncut2-ncut1, cutoff1, cutoff2);
        GwyLine *pvdist2 = gwy_field_value_dist(field, NULL, NULL, 0,
                                                FALSE, FALSE,
                                                vdist->res-ncut2, cutoff2, max);

        gwy_assert_floatval(gwy_line_dx(pvdist0), gwy_line_dx(vdist), 1e-15);
        gwy_assert_floatval(gwy_line_dx(pvdist1), gwy_line_dx(vdist), 1e-15);
        gwy_assert_floatval(gwy_line_dx(pvdist2), gwy_line_dx(vdist), 1e-15);
        g_assert_cmpfloat(pvdist0->off, ==, min);
        gwy_assert_floatval(pvdist0->off + pvdist0->real, cutoff1, 1e-15);
        g_assert_cmpfloat(pvdist1->off, ==, cutoff1);
        gwy_assert_floatval(pvdist1->off + pvdist1->real, cutoff2, 1e-15);
        g_assert_cmpfloat(pvdist2->off, ==, cutoff2);
        gwy_assert_floatval(pvdist2->off + pvdist2->real, max, 1e-15);
        gdouble ps0 = gwy_line_sum_full(pvdist0),
                ps1 = gwy_line_sum_full(pvdist1),
                ps2 = gwy_line_sum_full(pvdist2);
        gdouble pintegral = (ps0 + ps1 + ps2)*gwy_line_dx(vdist);
        gwy_assert_floatval(pintegral, 1.0, 1e-12);

        g_object_unref(pvdist2);
        g_object_unref(pvdist1);
        g_object_unref(pvdist0);
        g_object_unref(vdist);
        g_object_unref(field);
    }

    g_rand_free(rng);
}

void
test_field_distributions_value_cont_range(void)
{
    enum { max_size = 76, niter = 100 };
    GRand *rng = g_rand_new_with_seed(42);

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 5, max_size);
        guint yres = g_rand_int_range(rng, 5, max_size);
        GwyField *field = gwy_field_new_sized(xres, yres, FALSE);

        field_randomize(field, rng);

        gdouble min, max;
        gwy_field_min_max_full(field, &min, &max);
        GwyLine *vdist = gwy_field_value_dist(field,
                                              NULL, NULL, GWY_MASK_IGNORE,
                                              FALSE, TRUE, 0, min, max);
        g_assert_cmpfloat(vdist->off, <=, min);
        g_assert_cmpfloat(vdist->off + vdist->real, >=, max);
        g_assert_cmpfloat(vdist->data[0], >, 0.0);
        g_assert_cmpfloat(vdist->data[vdist->res-1], >, 0.0);
        gdouble s = gwy_line_sum_full(vdist);
        gdouble integral = gwy_line_dx(vdist)*s;
        gwy_assert_floatval(integral, 1.0, 1e-12);
        for (guint i = 0; i < vdist->res; i++) {
            g_assert_cmpfloat(vdist->data[i], >=, 0.0);
            // FIXME: What is a reliable test for expected values?
        }

        guint ncut1 = vdist->res/3, ncut2 = 2*vdist->res/3;
        gdouble cutoff1 = vdist->off + gwy_line_dx(vdist)*ncut1,
                cutoff2 = vdist->off + gwy_line_dx(vdist)*ncut2;
        // XXX: If a value coincides *exactly* (at least within rounding
        // errors) with a cut-off it can end up in either part of the
        // distribution.
        GwyLine *pvdist0 = gwy_field_value_dist(field, NULL, NULL, 0,
                                                FALSE, TRUE,
                                                ncut1, min, cutoff1);
        GwyLine *pvdist1 = gwy_field_value_dist(field, NULL, NULL, 0,
                                                FALSE, TRUE,
                                                ncut2-ncut1, cutoff1, cutoff2);
        GwyLine *pvdist2 = gwy_field_value_dist(field, NULL, NULL, 0,
                                                FALSE, TRUE,
                                                vdist->res-ncut2, cutoff2, max);

        gwy_assert_floatval(gwy_line_dx(pvdist0), gwy_line_dx(vdist), 1e-15);
        gwy_assert_floatval(gwy_line_dx(pvdist1), gwy_line_dx(vdist), 1e-15);
        gwy_assert_floatval(gwy_line_dx(pvdist2), gwy_line_dx(vdist), 1e-15);
        g_assert_cmpfloat(pvdist0->off, ==, min);
        gwy_assert_floatval(pvdist0->off + pvdist0->real, cutoff1, 1e-15);
        g_assert_cmpfloat(pvdist1->off, ==, cutoff1);
        gwy_assert_floatval(pvdist1->off + pvdist1->real, cutoff2, 1e-15);
        g_assert_cmpfloat(pvdist2->off, ==, cutoff2);
        gwy_assert_floatval(pvdist2->off + pvdist2->real, max, 1e-15);
        gdouble ps0 = gwy_line_sum_full(pvdist0),
                ps1 = gwy_line_sum_full(pvdist1),
                ps2 = gwy_line_sum_full(pvdist2);
        gdouble pintegral = (ps0 + ps1 + ps2)*gwy_line_dx(vdist);
        gwy_assert_floatval(pintegral, 1.0, 1e-12);

        g_object_unref(pvdist2);
        g_object_unref(pvdist1);
        g_object_unref(pvdist0);
        g_object_unref(vdist);
        g_object_unref(field);
    }

    g_rand_free(rng);
}

static void
level_for_cf(GwyField *field,
             const GwyMaskField *mask,
             GwyMaskingType masking)
{
    for (guint i = 0; i < field->yres; i++) {
        GwyFieldPart fpart = { 0, i, field->xres, 1 };
        gdouble mean = gwy_field_mean(field, &fpart, mask, masking);
        if (!isnan(mean))
            gwy_field_add(field, &fpart, NULL, GWY_MASK_IGNORE, -mean);
    }
}

static GwyLine*
cf_dumb(const GwyField *field,
        const GwyFieldPart *fpart,
        const GwyMaskField *mask,
        GwyMaskingType masking,
        gboolean level,
        gboolean do_hhcf,
        GwyLine *weights)
{
    GwyField *part = gwy_field_new_part(field, fpart, FALSE);
    GwyLine *cf = gwy_line_new_sized(part->xres, TRUE);

    GwyMaskField *mpart = NULL;
    if (mask && masking != GWY_MASK_IGNORE)
        mpart = gwy_mask_field_new_part(mask, fpart);

    if (level)
        level_for_cf(part, mpart, masking);

    gwy_line_set_size(weights, part->xres, TRUE);
    weights->real = gwy_field_dx(field)*weights->res;
    weights->off = -0.5*gwy_field_dx(field);

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

    return cf;
}

void
test_field_distributions_row_acf_full(void)
{
    enum { max_size = 134 };
    GRand *rng = g_rand_new_with_seed(42);
    gsize niter = g_test_slow() ? 100 : 15;

    gwy_fft_load_wisdom();
    for (guint lvl = 0; lvl <= 1; lvl++) {
        for (guint iter = 0; iter < niter; iter++) {
            guint xres = g_rand_int_range(rng, 2, max_size);
            guint yres = g_rand_int_range(rng, 2, max_size);
            GwyField *field = gwy_field_new_sized(xres, yres, FALSE);

            field_randomize(field, rng);

            GwyLine *weights = gwy_line_new(), *dumb_weights = gwy_line_new();
            GwyLine *acf = gwy_field_row_acf(field,
                                             NULL, NULL, GWY_MASK_IGNORE, lvl,
                                             weights);
            GwyMaskField *mask = gwy_mask_field_new_sized(xres, yres, TRUE);
            GwyLine *weights0 = gwy_line_new();
            GwyLine *acf0 = gwy_field_row_acf(field, NULL,
                                              mask, GWY_MASK_EXCLUDE, lvl,
                                              weights0);
            gwy_mask_field_fill(mask, NULL, TRUE);
            GwyLine *weights1 = gwy_line_new();
            GwyLine *acf1 = gwy_field_row_acf(field, NULL,
                                              mask, GWY_MASK_INCLUDE, lvl,
                                              weights1);
            GwyLine *dumb_acf = cf_dumb(field, NULL,
                                        NULL, GWY_MASK_IGNORE, lvl, FALSE,
                                        dumb_weights);

            line_assert_numerically_equal(acf, dumb_acf, 1e-13);
            line_assert_numerically_equal(acf0, dumb_acf, 1e-13);
            line_assert_numerically_equal(acf1, dumb_acf, 1e-13);
            line_assert_equal(weights, dumb_weights);
            line_assert_equal(weights0, dumb_weights);
            line_assert_equal(weights1, dumb_weights);

            g_object_unref(acf);
            g_object_unref(acf1);
            g_object_unref(acf0);
            g_object_unref(dumb_acf);
            g_object_unref(weights);
            g_object_unref(weights1);
            g_object_unref(weights0);
            g_object_unref(dumb_weights);
            g_object_unref(mask);
            g_object_unref(field);
        }
    }

    g_rand_free(rng);
}

void
test_field_distributions_row_acf_masked(void)
{
    enum { max_size = 134 };
    GRand *rng = g_rand_new_with_seed(42);
    gsize niter = g_test_slow() ? 100 : 15;

    gwy_fft_load_wisdom();
    for (guint lvl = 0; lvl <= 1; lvl++) {
        for (guint iter = 0; iter < niter; iter++) {
            guint xres = g_rand_int_range(rng, 2, max_size);
            guint yres = g_rand_int_range(rng, 2, max_size);
            GwyField *field = gwy_field_new_sized(xres, yres, FALSE);

            field_randomize(field, rng);

            GwyMaskField *mask = random_mask_field(xres, yres, rng);
            GwyLine *weights = gwy_line_new(), *dumb_weights = gwy_line_new(),
                    *weights0 = gwy_line_new(), *dumb_weights0 = gwy_line_new(),
                    *weights1 = gwy_line_new(), *dumb_weights1 = gwy_line_new();
            GwyLine *acf = gwy_field_row_acf(field, NULL,
                                             mask, GWY_MASK_IGNORE, lvl,
                                             weights);
            GwyLine *acf0 = gwy_field_row_acf(field, NULL,
                                              mask, GWY_MASK_EXCLUDE, lvl,
                                              weights0);
            GwyLine *acf1 = gwy_field_row_acf(field, NULL,
                                              mask, GWY_MASK_INCLUDE, lvl,
                                              weights1);
            GwyLine *dumb_acf = cf_dumb(field, NULL,
                                        mask, GWY_MASK_IGNORE, lvl, FALSE,
                                        dumb_weights);
            GwyLine *dumb_acf0 = cf_dumb(field, NULL,
                                         mask, GWY_MASK_EXCLUDE, lvl, FALSE,
                                         dumb_weights0);
            GwyLine *dumb_acf1 = cf_dumb(field, NULL,
                                         mask, GWY_MASK_INCLUDE, lvl, FALSE,
                                         dumb_weights1);

            line_assert_numerically_equal(acf, dumb_acf, 1e-13);
            line_assert_numerically_equal(acf0, dumb_acf0, 1e-13);
            line_assert_numerically_equal(acf1, dumb_acf1, 1e-13);
            line_assert_equal(weights, dumb_weights);
            line_assert_equal(weights0, dumb_weights0);
            line_assert_equal(weights1, dumb_weights1);

            g_object_unref(acf);
            g_object_unref(acf0);
            g_object_unref(acf1);
            g_object_unref(dumb_acf);
            g_object_unref(dumb_acf0);
            g_object_unref(dumb_acf1);
            g_object_unref(weights);
            g_object_unref(weights0);
            g_object_unref(weights1);
            g_object_unref(dumb_weights);
            g_object_unref(dumb_weights0);
            g_object_unref(dumb_weights1);
            g_object_unref(mask);
            g_object_unref(field);
        }
    }

    g_rand_free(rng);
}

void
test_field_distributions_row_acf_partial(void)
{
    enum { max_size = 134 };
    GRand *rng = g_rand_new_with_seed(42);
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
            GwyFieldPart fpart = { col, row, width, height };

            GwyMaskField *mask = random_mask_field(xres, yres, rng);
            GwyLine *weights = gwy_line_new(), *dumb_weights = gwy_line_new(),
                    *weights0 = gwy_line_new(), *dumb_weights0 = gwy_line_new(),
                    *weights1 = gwy_line_new(), *dumb_weights1 = gwy_line_new();
            GwyLine *acf = gwy_field_row_acf(field, &fpart,
                                             mask, GWY_MASK_IGNORE, lvl,
                                             weights);
            GwyLine *acf0 = gwy_field_row_acf(field, &fpart,
                                              mask, GWY_MASK_EXCLUDE, lvl,
                                              weights0);
            GwyLine *acf1 = gwy_field_row_acf(field, &fpart,
                                              mask, GWY_MASK_INCLUDE, lvl,
                                              weights1);
            GwyLine *dumb_acf = cf_dumb(field, &fpart,
                                        mask, GWY_MASK_IGNORE, lvl, FALSE,
                                        dumb_weights);
            GwyLine *dumb_acf0 = cf_dumb(field, &fpart,
                                         mask, GWY_MASK_EXCLUDE, lvl, FALSE,
                                         dumb_weights0);
            GwyLine *dumb_acf1 = cf_dumb(field, &fpart,
                                         mask, GWY_MASK_INCLUDE, lvl, FALSE,
                                         dumb_weights1);

            line_assert_numerically_equal(acf, dumb_acf, 1e-13);
            line_assert_numerically_equal(acf0, dumb_acf0, 1e-13);
            line_assert_numerically_equal(acf1, dumb_acf1, 1e-13);
            line_assert_equal(weights, dumb_weights);
            line_assert_equal(weights0, dumb_weights0);
            line_assert_equal(weights1, dumb_weights1);

            g_object_unref(acf);
            g_object_unref(acf0);
            g_object_unref(acf1);
            g_object_unref(dumb_acf);
            g_object_unref(dumb_acf0);
            g_object_unref(dumb_acf1);
            g_object_unref(weights);
            g_object_unref(weights0);
            g_object_unref(weights1);
            g_object_unref(dumb_weights);
            g_object_unref(dumb_weights0);
            g_object_unref(dumb_weights1);
            g_object_unref(mask);
            g_object_unref(field);
        }
    }

    g_rand_free(rng);
}

static void
extract_grain_with_data(const GwyMaskField *mask,
                        GwyMaskField *mask_target,
                        const GwyField *field,
                        GwyField *field_target,
                        guint grain_id,
                        guint border)
{
    const GwyFieldPart *bboxes = gwy_mask_field_grain_bounding_boxes(mask);
    const GwyFieldPart *fpart = bboxes + grain_id;
    gwy_mask_field_set_size(mask_target, fpart->width, fpart->height, FALSE);
    gwy_mask_field_extract_grain(mask, mask_target, grain_id, border);
    gwy_field_extend(field, fpart, field_target, border, border, border, border,
                     GWY_EXTERIOR_MIRROR_EXTEND, NAN, TRUE);
    g_assert_cmpfloat(fabs(gwy_field_dx(field_target) - gwy_field_dx(field)),
                      <=, 1e-16);
    g_assert_cmpfloat(fabs(gwy_field_dy(field_target) - gwy_field_dy(field)),
                      <=, 1e-16);
}

void
test_field_distributions_row_acf_grain(void)
{
    enum { max_size = 40, niter = 15 };
    GRand *rng = g_rand_new_with_seed(42);
    GwyField *grainfield = gwy_field_new();
    GwyMaskField *grainmask = gwy_mask_field_new();
    GwyLine *weights = gwy_line_new(), *dumb_weights = gwy_line_new();

    gwy_fft_load_wisdom();
    for (guint lvl = 0; lvl <= 1; lvl++) {
        for (guint iter = 0; iter < niter; iter++) {
            guint xres = g_rand_int_range(rng, 8, max_size);
            guint yres = g_rand_int_range(rng, 8, max_size);
            GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
            field_randomize(field, rng);
            GwyMaskField *mask = random_mask_field(xres, yres, rng);
            guint ngrains = gwy_mask_field_n_grains(mask);

            for (guint gno = 1; gno <= ngrains; gno++) {
                extract_grain_with_data(mask, grainmask, field, grainfield,
                                        gno, 0);

                GwyLine *acf = gwy_field_grain_row_acf(field, mask, gno, lvl,
                                                       weights);
                GwyLine *dumb_acf = cf_dumb(grainfield, NULL,
                                            grainmask, GWY_MASK_INCLUDE,
                                            lvl, FALSE,
                                            dumb_weights);

                line_assert_numerically_equal(acf, dumb_acf, 1e-13);
                line_assert_equal(weights, dumb_weights);

                g_object_unref(dumb_acf);
                g_object_unref(acf);
            }
            g_object_unref(mask);
            g_object_unref(field);
        }
    }
    g_object_unref(dumb_weights);
    g_object_unref(weights);
    g_object_unref(grainmask);
    g_object_unref(grainfield);

    g_rand_free(rng);
}

void
test_field_distributions_row_hhcf_full(void)
{
    enum { max_size = 134 };
    GRand *rng = g_rand_new_with_seed(42);
    gsize niter = g_test_slow() ? 100 : 15;

    gwy_fft_load_wisdom();
    for (guint lvl = 0; lvl <= 1; lvl++) {
        for (guint iter = 0; iter < niter; iter++) {
            guint xres = g_rand_int_range(rng, 2, max_size);
            guint yres = g_rand_int_range(rng, 2, max_size);
            GwyField *field = gwy_field_new_sized(xres, yres, FALSE);

            field_randomize(field, rng);

            GwyLine *weights = gwy_line_new(), *dumb_weights = gwy_line_new();
            GwyLine *hhcf = gwy_field_row_hhcf(field, NULL,
                                               NULL, GWY_MASK_IGNORE, lvl,
                                               weights);
            GwyMaskField *mask = gwy_mask_field_new_sized(xres, yres, TRUE);
            GwyLine *weights0 = gwy_line_new();
            GwyLine *hhcf0 = gwy_field_row_hhcf(field, NULL,
                                                mask, GWY_MASK_EXCLUDE, lvl,
                                                weights0);
            gwy_mask_field_fill(mask, NULL, TRUE);
            GwyLine *weights1 = gwy_line_new();
            GwyLine *hhcf1 = gwy_field_row_hhcf(field, NULL,
                                                mask, GWY_MASK_INCLUDE, lvl,
                                                weights1);
            GwyLine *dumb_hhcf = cf_dumb(field, NULL,
                                         NULL, GWY_MASK_IGNORE, lvl, TRUE,
                                         dumb_weights);

            line_assert_numerically_equal(hhcf, dumb_hhcf, 1e-13);
            line_assert_numerically_equal(hhcf0, dumb_hhcf, 1e-13);
            line_assert_numerically_equal(hhcf1, dumb_hhcf, 1e-13);
            line_assert_equal(weights, dumb_weights);
            line_assert_equal(weights0, dumb_weights);
            line_assert_equal(weights1, dumb_weights);

            g_object_unref(hhcf);
            g_object_unref(hhcf1);
            g_object_unref(hhcf0);
            g_object_unref(weights);
            g_object_unref(weights1);
            g_object_unref(weights0);
            g_object_unref(dumb_weights);
            g_object_unref(dumb_hhcf);
            g_object_unref(mask);
            g_object_unref(field);
        }
    }

    g_rand_free(rng);
}

void
test_field_distributions_row_hhcf_masked(void)
{
    enum { max_size = 134 };
    GRand *rng = g_rand_new_with_seed(42);
    gsize niter = g_test_slow() ? 100 : 15;

    gwy_fft_load_wisdom();
    for (guint lvl = 0; lvl <= 1; lvl++) {
        for (guint iter = 0; iter < niter; iter++) {
            guint xres = g_rand_int_range(rng, 2, max_size);
            guint yres = g_rand_int_range(rng, 2, max_size);
            GwyField *field = gwy_field_new_sized(xres, yres, FALSE);

            field_randomize(field, rng);

            GwyMaskField *mask = random_mask_field(xres, yres, rng);
            GwyLine *weights = gwy_line_new(), *dumb_weights = gwy_line_new(),
                    *weights0 = gwy_line_new(), *dumb_weights0 = gwy_line_new(),
                    *weights1 = gwy_line_new(), *dumb_weights1 = gwy_line_new();
            GwyLine *hhcf = gwy_field_row_hhcf(field, NULL,
                                               mask, GWY_MASK_IGNORE, lvl,
                                               weights);
            GwyLine *hhcf0 = gwy_field_row_hhcf(field, NULL,
                                                mask, GWY_MASK_EXCLUDE, lvl,
                                                weights0);
            GwyLine *hhcf1 = gwy_field_row_hhcf(field, NULL,
                                                mask, GWY_MASK_INCLUDE, lvl,
                                                weights1);
            GwyLine *dumb_hhcf = cf_dumb(field, NULL,
                                         mask, GWY_MASK_IGNORE, lvl, TRUE,
                                         dumb_weights);
            GwyLine *dumb_hhcf0 = cf_dumb(field, NULL,
                                          mask, GWY_MASK_EXCLUDE, lvl, TRUE,
                                          dumb_weights0);
            GwyLine *dumb_hhcf1 = cf_dumb(field, NULL,
                                          mask, GWY_MASK_INCLUDE, lvl, TRUE,
                                          dumb_weights1);

            line_assert_numerically_equal(hhcf, dumb_hhcf, 1e-13);
            line_assert_numerically_equal(hhcf0, dumb_hhcf0, 1e-13);
            line_assert_numerically_equal(hhcf1, dumb_hhcf1, 1e-13);
            line_assert_equal(weights, dumb_weights);
            line_assert_equal(weights0, dumb_weights0);
            line_assert_equal(weights1, dumb_weights1);

            g_object_unref(hhcf);
            g_object_unref(hhcf0);
            g_object_unref(hhcf1);
            g_object_unref(dumb_hhcf);
            g_object_unref(dumb_hhcf0);
            g_object_unref(dumb_hhcf1);
            g_object_unref(weights);
            g_object_unref(weights0);
            g_object_unref(weights1);
            g_object_unref(dumb_weights);
            g_object_unref(dumb_weights0);
            g_object_unref(dumb_weights1);
            g_object_unref(mask);
            g_object_unref(field);
        }
    }

    g_rand_free(rng);
}

void
test_field_distributions_row_hhcf_partial(void)
{
    enum { max_size = 134 };
    GRand *rng = g_rand_new_with_seed(42);
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
            GwyFieldPart fpart = { col, row, width, height };

            GwyMaskField *mask = random_mask_field(xres, yres, rng);
            GwyLine *weights = gwy_line_new(), *dumb_weights = gwy_line_new(),
                    *weights0 = gwy_line_new(), *dumb_weights0 = gwy_line_new(),
                    *weights1 = gwy_line_new(), *dumb_weights1 = gwy_line_new();
            GwyLine *hhcf = gwy_field_row_hhcf(field, &fpart,
                                               mask, GWY_MASK_IGNORE, lvl,
                                               weights);
            GwyLine *hhcf0 = gwy_field_row_hhcf(field, &fpart,
                                                mask, GWY_MASK_EXCLUDE, lvl,
                                                weights0);
            GwyLine *hhcf1 = gwy_field_row_hhcf(field, &fpart,
                                                mask, GWY_MASK_INCLUDE, lvl,
                                                weights1);
            GwyLine *dumb_hhcf = cf_dumb(field, &fpart,
                                         mask, GWY_MASK_IGNORE, lvl, TRUE,
                                         dumb_weights);
            GwyLine *dumb_hhcf0 = cf_dumb(field, &fpart,
                                          mask, GWY_MASK_EXCLUDE, lvl, TRUE,
                                          dumb_weights0);
            GwyLine *dumb_hhcf1 = cf_dumb(field, &fpart,
                                          mask, GWY_MASK_INCLUDE, lvl, TRUE,
                                          dumb_weights1);

            line_assert_numerically_equal(hhcf, dumb_hhcf, 1e-13);
            line_assert_numerically_equal(hhcf0, dumb_hhcf0, 1e-13);
            line_assert_numerically_equal(hhcf1, dumb_hhcf1, 1e-13);
            line_assert_equal(weights, dumb_weights);
            line_assert_equal(weights0, dumb_weights0);
            line_assert_equal(weights1, dumb_weights1);

            g_object_unref(hhcf);
            g_object_unref(hhcf0);
            g_object_unref(hhcf1);
            g_object_unref(dumb_hhcf);
            g_object_unref(dumb_hhcf0);
            g_object_unref(dumb_hhcf1);
            g_object_unref(weights);
            g_object_unref(weights0);
            g_object_unref(weights1);
            g_object_unref(dumb_weights);
            g_object_unref(dumb_weights0);
            g_object_unref(dumb_weights1);
            g_object_unref(mask);
            g_object_unref(field);
        }
    }

    g_rand_free(rng);
}

void
test_field_distributions_row_asg_full(void)
{
    enum { size = 134 };
    GRand *rng = g_rand_new_with_seed(42);

    gwy_fft_load_wisdom();
    for (guint lvl = 0; lvl <= 1; lvl++) {
        for (gint iscale = -12; iscale <= 12; iscale++) {
            GwyField *field = gwy_field_new_sized(size, size, FALSE);
            gdouble scale = exp10(0.5*iscale);

            field_randomize(field, rng);
            gwy_field_multiply_full(field, scale);
            gdouble area = gwy_field_surface_area(field, NULL, NULL, 0);
            gdouble ex_1 = area/(field->xreal*field->yreal) - 1.0;

            GwyLine *asg = gwy_field_row_asg(field, NULL,
                                             NULL, GWY_MASK_IGNORE, lvl);
            GwyMaskField *mask = gwy_mask_field_new_sized(size, size, TRUE);
            GwyLine *asg0 = gwy_field_row_asg(field, NULL,
                                              mask, GWY_MASK_EXCLUDE, lvl);
            gwy_mask_field_fill(mask, NULL, TRUE);
            GwyLine *asg1 = gwy_field_row_asg(field, NULL,
                                              mask, GWY_MASK_INCLUDE, lvl);

            g_assert_cmpuint(asg->res, ==, field->xres-1);
            line_assert_equal(asg0, asg);
            line_assert_equal(asg1, asg);
            // The estimate is rough because surface_area() uses a different
            // definition of surface area than the ASG.
            gwy_assert_floatval(0.75*asg->data[0], ex_1, 0.12*ex_1);

            g_object_unref(asg);
            g_object_unref(asg1);
            g_object_unref(asg0);
            g_object_unref(mask);
            g_object_unref(field);
        }
    }

    g_rand_free(rng);
}

void
test_field_distributions_minkowski_volume(void)
{
    enum { max_size = 30, niter = 400 };
    GRand *rng = g_rand_new_with_seed(42);

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 2, max_size);
        guint yres = g_rand_int_range(rng, 2, max_size);
        GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
        field_randomize(field, rng);

        guint width = g_rand_int_range(rng, 1, xres+1);
        guint height = g_rand_int_range(rng, 1, yres+1);
        guint col = g_rand_int_range(rng, 0, xres-width+1);
        guint row = g_rand_int_range(rng, 0, yres-height+1);
        GwyFieldPart fpart = { col, row, width, height };

        GwyMaskField *mask = random_mask_field(width, height, rng);
        GwyMaskingType masking = (GwyMaskingType)g_rand_int_range(rng, 0, 3);

        GwyLine *volumedist = gwy_field_minkowski(field, &fpart,
                                                  mask, masking,
                                                  GWY_MINKOWSKI_VOLUME,
                                                  0, 0.0, 0.0);
        if (volumedist->res == 1) {
            guint count = gwy_mask_field_count(mask, NULL,
                                               masking == GWY_MASK_INCLUDE);
            g_assert_cmpuint(count, ==, 0);
            g_object_unref(volumedist);
            g_object_unref(mask);
            g_object_unref(field);
            continue;
        }

        for (guint i = 0; i < volumedist->res; i++) {
            gdouble threshold = (volumedist->off
                                 + (i + 0.5)*gwy_line_dx(volumedist));
            guint nabove, n;
            n = gwy_field_count_above_below(field, &fpart, mask, masking,
                                            threshold, threshold, FALSE,
                                            &nabove, NULL);
            gdouble fraction = (gdouble)nabove/n;
            if (n == 1) {
                g_assert_cmpuint(volumedist->res, ==, 3);
                g_assert_cmpfloat(volumedist->data[0], ==, 1.0);
                // Depends on rounding, permit both.
                g_assert(volumedist->data[1] == 0.0
                         || volumedist->data[1] == 1.0);
                g_assert_cmpfloat(volumedist->data[2], ==, 0.0);
                break;
            }
            g_assert_cmpfloat(fabs(volumedist->data[i] - fraction), <=, 1e-14);
        }

        g_object_unref(volumedist);
        g_object_unref(mask);
        g_object_unref(field);
    }
    g_rand_free(rng);
}

static guint
count_black_white_edges_dumb(const GwyField *field,
                             const GwyFieldPart *fpart,
                             const GwyMaskField *mask,
                             GwyMaskingType masking,
                             gdouble threshold,
                             guint *bw_edge_count)
{
    guint count = 0, count_bw = 0;

    for (guint i = 0; i < fpart->height; i++) {
        for (guint j = 0; j < fpart->width; j++) {
            if (i+1 < fpart->height) {
                gdouble z1 = gwy_field_index(field,
                                             fpart->col + j, fpart->row + i),
                        z2 = gwy_field_index(field,
                                             fpart->col + j, fpart->row + i+1);
                gdouble zmin = fmin(z1, z2), zmax = fmax(z1, z2);

                if (masking == GWY_MASK_IGNORE
                    || (masking == GWY_MASK_INCLUDE
                        && gwy_mask_field_get(mask, j, i)
                        && gwy_mask_field_get(mask, j, i+1))
                    || (masking == GWY_MASK_EXCLUDE
                        && !gwy_mask_field_get(mask, j, i)
                        && !gwy_mask_field_get(mask, j, i+1))) {
                    count++;
                    if (zmin <= threshold && zmax > threshold)
                        count_bw++;
                }
            }
            if (j+1 < fpart->width) {
                gdouble z1 = gwy_field_index(field,
                                             fpart->col + j, fpart->row + i),
                        z2 = gwy_field_index(field,
                                             fpart->col + j+1, fpart->row + i);
                gdouble zmin = fmin(z1, z2), zmax = fmax(z1, z2);

                if (masking == GWY_MASK_IGNORE
                    || (masking == GWY_MASK_INCLUDE
                        && gwy_mask_field_get(mask, j, i)
                        && gwy_mask_field_get(mask, j+1, i))
                    || (masking == GWY_MASK_EXCLUDE
                        && !gwy_mask_field_get(mask, j, i)
                        && !gwy_mask_field_get(mask, j+1, i))) {
                    count++;
                    if (zmin <= threshold && zmax > threshold)
                        count_bw++;
                }
            }
        }
    }

    *bw_edge_count = count_bw;
    return count;
}

void
test_field_distributions_minkowski_boundary(void)
{
    enum { max_size = 30, niter = 400 };
    GRand *rng = g_rand_new_with_seed(42);

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 2, max_size);
        guint yres = g_rand_int_range(rng, 2, max_size);
        GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
        field_randomize(field, rng);

        guint width = g_rand_int_range(rng, 1, xres+1);
        guint height = g_rand_int_range(rng, 1, yres+1);
        guint col = g_rand_int_range(rng, 0, xres-width+1);
        guint row = g_rand_int_range(rng, 0, yres-height+1);
        GwyFieldPart fpart = { col, row, width, height };

        GwyMaskField *mask = random_mask_field(width, height, rng);
        GwyMaskingType masking = (GwyMaskingType)g_rand_int_range(rng, 0, 3);

        GwyLine *boundarydist = gwy_field_minkowski(field, &fpart,
                                                    mask, masking,
                                                    GWY_MINKOWSKI_BOUNDARY,
                                                    0, 0.0, 0.0);
        if (boundarydist->res == 1) {
            if (masking == GWY_MASK_IGNORE) {
                g_assert_cmpuint(width, ==, 1);
                g_assert_cmpuint(height, ==, 1);
            }
            else {
                gboolean include = (masking == GWY_MASK_INCLUDE);
                guint count = gwy_mask_field_count(mask, NULL, include);
                if (!include)
                    gwy_mask_field_logical(mask, NULL, NULL, GWY_LOGICAL_NA);
                guint ngrains = gwy_mask_field_n_grains(mask);
                g_assert_cmpuint(count, ==, ngrains);
            }
            g_object_unref(boundarydist);
            g_object_unref(mask);
            g_object_unref(field);
            continue;
        }

        for (guint i = 0; i < boundarydist->res; i++) {
            gdouble threshold = (boundarydist->off
                                 + (i + 0.5)*gwy_line_dx(boundarydist));
            guint nbw, n;
            n = count_black_white_edges_dumb(field, &fpart, mask, masking,
                                             threshold, &nbw);
            gdouble fraction = (gdouble)nbw/n;
            g_assert_cmpfloat(fabs(boundarydist->data[i] - fraction), <=, 1e-14);
        }

        g_object_unref(boundarydist);
        g_object_unref(mask);
        g_object_unref(field);
    }
    g_rand_free(rng);
}

void
test_field_distributions_minkowski_black(void)
{
    enum { max_size = 30, niter = 400 };
    GRand *rng = g_rand_new_with_seed(42);

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 2, max_size);
        guint yres = g_rand_int_range(rng, 2, max_size);
        GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
        field_randomize(field, rng);

        guint width = g_rand_int_range(rng, 1, xres+1);
        guint height = g_rand_int_range(rng, 1, yres+1);
        guint col = g_rand_int_range(rng, 0, xres-width+1);
        guint row = g_rand_int_range(rng, 0, yres-height+1);
        GwyFieldPart fpart = { col, row, width, height };

        GwyMaskField *mask = random_mask_field(width, height, rng);
        GwyMaskingType masking = (GwyMaskingType)g_rand_int_range(rng, 0, 3);

        GwyLine *blackdist = gwy_field_minkowski(field, &fpart,
                                                 mask, masking,
                                                 GWY_MINKOWSKI_BLACK,
                                                 0, 0.0, 0.0);
        if (blackdist->res == 1) {
            guint count = gwy_mask_field_count(mask, NULL,
                                               masking == GWY_MASK_INCLUDE);
            g_assert_cmpuint(count, ==, 0);
            goto next;
        }

        guint n = width*height;
        if (masking != GWY_MASK_IGNORE)
            n = gwy_mask_field_count(mask, NULL, masking == GWY_MASK_INCLUDE);

        if (n == 1) {
            g_assert_cmpuint(blackdist->res, ==, 3);
            g_assert_cmpfloat(blackdist->data[0], ==, 0.0);
            // Depends on rounding, permit both.
            g_assert(blackdist->data[1] == 0.0 || blackdist->data[1] == 1.0);
            g_assert_cmpfloat(blackdist->data[2], ==, 1.0);
            goto next;
        }

        for (guint i = 0; i < blackdist->res; i++) {
            gdouble threshold = (blackdist->off
                                 + (i + 0.5)*gwy_line_dx(blackdist));
            GwyMaskField *grains = gwy_mask_field_new_from_field(field, &fpart,
                                                                 -G_MAXDOUBLE,
                                                                 threshold,
                                                                 FALSE);
            if (masking == GWY_MASK_INCLUDE)
                gwy_mask_field_logical(grains, mask, NULL, GWY_LOGICAL_AND);
            else if (masking == GWY_MASK_EXCLUDE)
                gwy_mask_field_logical(grains, mask, NULL, GWY_LOGICAL_NIMPL);

            guint ng = gwy_mask_field_n_grains(grains);
            gdouble fraction = (gdouble)ng/n;
            g_assert_cmpfloat(fabs(blackdist->data[i] - fraction), <=, 1e-14);
            g_object_unref(grains);
        }

next:
        g_object_unref(blackdist);
        g_object_unref(mask);
        g_object_unref(field);
    }
    g_rand_free(rng);
}

void
test_field_distributions_minkowski_white(void)
{
    enum { max_size = 30, niter = 400 };
    GRand *rng = g_rand_new_with_seed(42);

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 2, max_size);
        guint yres = g_rand_int_range(rng, 2, max_size);
        GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
        field_randomize(field, rng);

        guint width = g_rand_int_range(rng, 1, xres+1);
        guint height = g_rand_int_range(rng, 1, yres+1);
        guint col = g_rand_int_range(rng, 0, xres-width+1);
        guint row = g_rand_int_range(rng, 0, yres-height+1);
        GwyFieldPart fpart = { col, row, width, height };

        GwyMaskField *mask = random_mask_field(width, height, rng);
        GwyMaskingType masking = (GwyMaskingType)g_rand_int_range(rng, 0, 3);

        GwyLine *whitedist = gwy_field_minkowski(field, &fpart,
                                                 mask, masking,
                                                 GWY_MINKOWSKI_WHITE,
                                                 0, 0.0, 0.0);
        if (whitedist->res == 1) {
            guint count = gwy_mask_field_count(mask, NULL,
                                               masking == GWY_MASK_INCLUDE);
            g_assert_cmpuint(count, ==, 0);
            goto next;
        }

        guint n = width*height;
        if (masking != GWY_MASK_IGNORE)
            n = gwy_mask_field_count(mask, NULL, masking == GWY_MASK_INCLUDE);

        if (n == 1) {
            g_assert_cmpuint(whitedist->res, ==, 3);
            g_assert_cmpfloat(whitedist->data[0], ==, 1.0);
            // Depends on rounding, permit both.
            g_assert(whitedist->data[1] == 0.0 || whitedist->data[1] == 1.0);
            g_assert_cmpfloat(whitedist->data[2], ==, 0.0);
            goto next;
        }

        for (guint i = 0; i < whitedist->res; i++) {
            gdouble threshold = (whitedist->off
                                 + (i + 0.5)*gwy_line_dx(whitedist));
            GwyMaskField *grains = gwy_mask_field_new_from_field(field, &fpart,
                                                                 threshold,
                                                                 G_MAXDOUBLE,
                                                                 FALSE);
            if (masking == GWY_MASK_INCLUDE)
                gwy_mask_field_logical(grains, mask, NULL, GWY_LOGICAL_AND);
            else if (masking == GWY_MASK_EXCLUDE)
                gwy_mask_field_logical(grains, mask, NULL, GWY_LOGICAL_NIMPL);

            guint ng = gwy_mask_field_n_grains(grains);
            gdouble fraction = (gdouble)ng/n;
            g_assert_cmpfloat(fabs(whitedist->data[i] - fraction), <=, 1e-14);
            g_object_unref(grains);
        }

next:
        g_object_unref(whitedist);
        g_object_unref(mask);
        g_object_unref(field);
    }
    g_rand_free(rng);
}

void
test_field_distributions_minkowski_connectivity(void)
{
    enum { max_size = 30, niter = 400 };
    GRand *rng = g_rand_new_with_seed(42);

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 2, max_size);
        guint yres = g_rand_int_range(rng, 2, max_size);
        GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
        field_randomize(field, rng);

        guint width = g_rand_int_range(rng, 1, xres+1);
        guint height = g_rand_int_range(rng, 1, yres+1);
        guint col = g_rand_int_range(rng, 0, xres-width+1);
        guint row = g_rand_int_range(rng, 0, yres-height+1);
        GwyFieldPart fpart = { col, row, width, height };

        GwyMaskField *mask = random_mask_field(width, height, rng);
        GwyMaskingType masking = (GwyMaskingType)g_rand_int_range(rng, 0, 3);

        GwyLine *conndist = gwy_field_minkowski(field, &fpart,
                                                mask, masking,
                                                GWY_MINKOWSKI_CONNECTIVITY,
                                                0, 0.0, 0.0);
        if (conndist->res == 1) {
            guint count = gwy_mask_field_count(mask, NULL,
                                               masking == GWY_MASK_INCLUDE);
            g_assert_cmpuint(count, ==, 0);
            goto next;
        }

        guint n = width*height;
        if (masking != GWY_MASK_IGNORE)
            n = gwy_mask_field_count(mask, NULL, masking == GWY_MASK_INCLUDE);

        if (n == 1) {
            g_assert_cmpuint(conndist->res, ==, 3);
            g_assert_cmpfloat(conndist->data[0], ==, 1.0);
            // Depends on rounding, permit both.
            g_assert(conndist->data[1] == -1.0
                     || conndist->data[1] == 0.0
                     || conndist->data[1] == 1.0);
            g_assert_cmpfloat(conndist->data[2], ==, -1.0);
            goto next;
        }

        for (guint i = 0; i < conndist->res; i++) {
            gdouble threshold = (conndist->off
                                 + (i + 0.5)*gwy_line_dx(conndist));
            GwyMaskField *wgrains, *bgrains;
            wgrains = gwy_mask_field_new_from_field(field, &fpart,
                                                    threshold, G_MAXDOUBLE,
                                                    FALSE);
            bgrains = gwy_mask_field_new_from_field(field, &fpart,
                                                    -G_MAXDOUBLE, threshold,
                                                    FALSE);
            if (masking == GWY_MASK_INCLUDE) {
                gwy_mask_field_logical(wgrains, mask, NULL, GWY_LOGICAL_AND);
                gwy_mask_field_logical(bgrains, mask, NULL, GWY_LOGICAL_AND);
            }
            else if (masking == GWY_MASK_EXCLUDE) {
                gwy_mask_field_logical(wgrains, mask, NULL, GWY_LOGICAL_NIMPL);
                gwy_mask_field_logical(bgrains, mask, NULL, GWY_LOGICAL_NIMPL);
            }

            guint ngw = gwy_mask_field_n_grains(wgrains);
            guint ngb = gwy_mask_field_n_grains(bgrains);
            gdouble fraction = (gdouble)ngw/n - (gdouble)ngb/n;
            gwy_assert_floatval(conndist->data[i], fraction, 1e-14);
            g_object_unref(wgrains);
            g_object_unref(bgrains);
        }

next:
        g_object_unref(conndist);
        g_object_unref(mask);
        g_object_unref(field);
    }
    g_rand_free(rng);
}

static gdouble
dumb_acf_2d(const GwyField *field, gint tx, gint ty)
{
    guint xres = field->xres, yres = field->yres;
    const gdouble *data = field->data;
    if (ty < 0) {
        tx = -tx;
        ty = -ty;
    }
    gboolean neg = tx < 0;
    tx = ABS(tx);

    gdouble g = 0.0;
    for (guint i = 0; i < yres - ty; i++) {
        if (neg) {
            for (guint j = 0; j < xres - tx; j++)
                g += data[i*xres + j + tx] * data[(i + ty)*xres + j];
        }
        else {
            for (guint j = 0; j < xres - tx; j++)
                g += data[i*xres + j] * data[(i + ty)*xres + j + tx];
        }
    }
    return g/(xres - tx)/(yres - ty);
}

void
test_field_distributions_acf_full(void)
{
    enum { max_size = 74, niter = 20, nvalue = 100 };
    GRand *rng = g_rand_new_with_seed(42);

    gwy_fft_load_wisdom();
    for (guint lvl = 0; lvl <= 1; lvl++) {
        for (guint iter = 0; iter < niter; iter++) {
            guint xres = g_rand_int_range(rng, 2, max_size);
            guint yres = g_rand_int_range(rng, 2, max_size);
            guint xrange = g_rand_int_range(rng, 0, xres);
            guint yrange = g_rand_int_range(rng, 0, yres);
            GwyField *field = gwy_field_new_sized(xres, yres, FALSE);

            field_randomize(field, rng);

            GwyField *acf = gwy_field_acf(field, NULL, xrange, yrange,
                                          lvl);
            GwyField *acf_full = gwy_field_acf(field, NULL, xres-1, yres-1,
                                               lvl);

            if (lvl == 1)
                gwy_field_add_full(field, -gwy_field_mean_full(field));

            gint xr = xrange, yr = yrange;
            for (guint iv = 0; iv < nvalue; iv++) {
                gint i = g_rand_int_range(rng, -yr, yr+1);
                gint j = g_rand_int_range(rng, -xr, xr+1);
                gdouble v = acf->data[(yr + i)*acf->xres + xr + j];
                gdouble vref = dumb_acf_2d(field, j, i);
                gwy_assert_floatval(v, vref, 1e-13);
            }

            xr = xres-1;
            yr = yres-1;
            for (guint iv = 0; iv < nvalue; iv++) {
                gint i = g_rand_int_range(rng, -yr, yr+1);
                gint j = g_rand_int_range(rng, -xr, xr+1);
                gdouble v = acf_full->data[(yr + i)*acf_full->xres + xr + j];
                gdouble vref = dumb_acf_2d(field, j, i);
                gwy_assert_floatval(v, vref, 1e-13);
            }

            g_object_unref(acf_full);
            g_object_unref(acf);
            g_object_unref(field);
        }
    }

    g_rand_free(rng);
}

void
test_field_distributions_acf_partial(void)
{
    enum { max_size = 74, niter = 30, nvalue = 100 };
    GRand *rng = g_rand_new_with_seed(42);

    gwy_fft_load_wisdom();
    for (guint lvl = 0; lvl <= 1; lvl++) {
        for (guint iter = 0; iter < niter; iter++) {
            guint xres = g_rand_int_range(rng, 2, max_size);
            guint yres = g_rand_int_range(rng, 2, max_size);
            guint width = g_rand_int_range(rng, 1, xres+1);
            guint height = g_rand_int_range(rng, 1, yres+1);
            guint col = g_rand_int_range(rng, 0, xres-width+1);
            guint row = g_rand_int_range(rng, 0, yres-height+1);
            guint xrange = g_rand_int_range(rng, 0, width);
            guint yrange = g_rand_int_range(rng, 0, height);
            GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
            GwyFieldPart fpart = { col, row, width, height };

            field_randomize(field, rng);

            GwyField *acf = gwy_field_acf(field, &fpart, xrange, yrange,
                                          lvl);
            GwyField *acf_full = gwy_field_acf(field, &fpart, width-1, height-1,
                                               lvl);

            GwyField *subfield = gwy_field_new_part(field, &fpart, FALSE);
            if (lvl == 1)
                gwy_field_add_full(subfield, -gwy_field_mean_full(subfield));

            gint xr = xrange, yr = yrange;
            for (guint iv = 0; iv < nvalue; iv++) {
                gint i = g_rand_int_range(rng, -yr, yr+1);
                gint j = g_rand_int_range(rng, -xr, xr+1);
                gdouble v = acf->data[(yr + i)*acf->xres + xr + j];
                gdouble vref = dumb_acf_2d(subfield, j, i);
                gwy_assert_floatval(v, vref, 1e-13);
            }

            xr = width-1;
            yr = height-1;
            for (guint iv = 0; iv < nvalue; iv++) {
                gint i = g_rand_int_range(rng, -yr, yr+1);
                gint j = g_rand_int_range(rng, -xr, xr+1);
                gdouble v = acf_full->data[(yr + i)*acf_full->xres + xr + j];
                gdouble vref = dumb_acf_2d(subfield, j, i);
                gwy_assert_floatval(v, vref, 1e-13);
            }

            g_object_unref(acf_full);
            g_object_unref(acf);
            g_object_unref(subfield);
            g_object_unref(field);
        }
    }

    g_rand_free(rng);
}

static gdouble
dumb_hhcf_2d(const GwyField *field, gint tx, gint ty)
{
    guint xres = field->xres, yres = field->yres;
    const gdouble *data = field->data;
    if (ty < 0) {
        tx = -tx;
        ty = -ty;
    }
    gboolean neg = tx < 0;
    tx = ABS(tx);

    gdouble g = 0.0;
    for (guint i = 0; i < yres - ty; i++) {
        if (neg) {
            for (guint j = 0; j < xres - tx; j++) {
                gdouble d = data[i*xres + j + tx] - data[(i + ty)*xres + j];
                g += d*d;
            }
        }
        else {
            for (guint j = 0; j < xres - tx; j++) {
                gdouble d = data[i*xres + j] - data[(i + ty)*xres + j + tx];
                g += d*d;
            }
        }
    }
    return g/(xres - tx)/(yres - ty);
}

void
test_field_distributions_hhcf_full(void)
{
    enum { max_size = 74, niter = 20, nvalue = 100 };
    GRand *rng = g_rand_new_with_seed(42);

    gwy_fft_load_wisdom();
    for (guint lvl = 0; lvl <= 1; lvl++) {
        for (guint iter = 0; iter < niter; iter++) {
            guint xres = g_rand_int_range(rng, 2, max_size);
            guint yres = g_rand_int_range(rng, 2, max_size);
            guint xrange = g_rand_int_range(rng, 0, xres);
            guint yrange = g_rand_int_range(rng, 0, yres);
            GwyField *field = gwy_field_new_sized(xres, yres, FALSE);

            field_randomize(field, rng);
            gwy_field_filter_gaussian(field, NULL, field, 2.0, 2.0,
                                      GWY_EXTERIOR_PERIODIC, 0.0);

            GwyField *hhcf = gwy_field_hhcf(field, NULL, xrange, yrange,
                                            lvl);
            GwyField *hhcf_full = gwy_field_hhcf(field, NULL, xres-1, yres-1,
                                                 lvl);

            if (lvl == 1)
                gwy_field_add_full(field, -gwy_field_mean_full(field));

            gint xr = xrange, yr = yrange;
            for (guint iv = 0; iv < nvalue; iv++) {
                gint i = g_rand_int_range(rng, -yr, yr+1);
                gint j = g_rand_int_range(rng, -xr, xr+1);
                gdouble v = hhcf->data[(yr + i)*hhcf->xres + xr + j];
                gdouble vref = dumb_hhcf_2d(field, j, i);
                gwy_assert_floatval(v, vref, 1e-13);
            }

            xr = xres-1;
            yr = yres-1;
            for (guint iv = 0; iv < nvalue; iv++) {
                gint i = g_rand_int_range(rng, -yr, yr+1);
                gint j = g_rand_int_range(rng, -xr, xr+1);
                gdouble v = hhcf_full->data[(yr + i)*hhcf_full->xres + xr + j];
                gdouble vref = dumb_hhcf_2d(field, j, i);
                gwy_assert_floatval(v, vref, 1e-13);
            }

            g_object_unref(hhcf_full);
            g_object_unref(hhcf);
            g_object_unref(field);
        }
    }

    g_rand_free(rng);
}

void
test_field_distributions_hhcf_partial(void)
{
    enum { max_size = 74, niter = 30, nvalue = 100 };
    GRand *rng = g_rand_new_with_seed(42);

    gwy_fft_load_wisdom();
    for (guint lvl = 0; lvl <= 1; lvl++) {
        for (guint iter = 0; iter < niter; iter++) {
            guint xres = g_rand_int_range(rng, 2, max_size);
            guint yres = g_rand_int_range(rng, 2, max_size);
            guint width = g_rand_int_range(rng, 1, xres+1);
            guint height = g_rand_int_range(rng, 1, yres+1);
            guint col = g_rand_int_range(rng, 0, xres-width+1);
            guint row = g_rand_int_range(rng, 0, yres-height+1);
            guint xrange = g_rand_int_range(rng, 0, width);
            guint yrange = g_rand_int_range(rng, 0, height);
            GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
            GwyFieldPart fpart = { col, row, width, height };

            field_randomize(field, rng);

            GwyField *hhcf = gwy_field_hhcf(field, &fpart, xrange, yrange,
                                          lvl);
            GwyField *hhcf_full = gwy_field_hhcf(field, &fpart, width-1, height-1,
                                               lvl);

            GwyField *subfield = gwy_field_new_part(field, &fpart, FALSE);
            if (lvl == 1)
                gwy_field_add_full(subfield, -gwy_field_mean_full(subfield));

            gint xr = xrange, yr = yrange;
            for (guint iv = 0; iv < nvalue; iv++) {
                gint i = g_rand_int_range(rng, -yr, yr+1);
                gint j = g_rand_int_range(rng, -xr, xr+1);
                gdouble v = hhcf->data[(yr + i)*hhcf->xres + xr + j];
                gdouble vref = dumb_hhcf_2d(subfield, j, i);
                gwy_assert_floatval(v, vref, 1e-13);
            }

            xr = width-1;
            yr = height-1;
            for (guint iv = 0; iv < nvalue; iv++) {
                gint i = g_rand_int_range(rng, -yr, yr+1);
                gint j = g_rand_int_range(rng, -xr, xr+1);
                gdouble v = hhcf_full->data[(yr + i)*hhcf_full->xres + xr + j];
                gdouble vref = dumb_hhcf_2d(subfield, j, i);
                gwy_assert_floatval(v, vref, 1e-13);
            }

            g_object_unref(hhcf_full);
            g_object_unref(hhcf);
            g_object_unref(subfield);
            g_object_unref(field);
        }
    }

    g_rand_free(rng);
}

void
test_field_distributions_psdf_rms(void)
{
    enum { max_size = 74, niter = 50 };
    GRand *rng = g_rand_new_with_seed(42);

    gwy_fft_load_wisdom();
    for (guint lvl = 0; lvl <= 1; lvl++) {
        for (guint iter = 0; iter < niter; iter++) {
            guint xres = g_rand_int_range(rng, 8, max_size);
            guint yres = g_rand_int_range(rng, 8, max_size);
            guint width = g_rand_int_range(rng, 6, xres+1);
            guint height = g_rand_int_range(rng, 6, yres+1);
            guint col = g_rand_int_range(rng, 0, xres-width+1);
            guint row = g_rand_int_range(rng, 0, yres-height+1);
            GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
            GwyFieldPart fpart = { col, row, width, height };
            gwy_field_set_xreal(field, g_rand_double_range(rng, 0.1, 10.0));
            gwy_field_set_yreal(field, g_rand_double_range(rng, 0.1, 10.0));
            GwyWindowingType windowing = g_rand_int_range(rng,
                                                          0,
                                                          GWY_WINDOWING_KAISER25+1);

            field_randomize(field, rng);
            GwyField *psdf = gwy_field_psdf(field, &fpart, windowing, lvl);
            gdouble rms_ref = (lvl
                               ? gwy_field_rms(field, &fpart, NULL, 0)
                               : sqrt(gwy_field_meansq(field, &fpart,
                                                       NULL, 0)));
            // sqrt(∫∫W dkx kdy)
            GwyFieldPart intpart = { 0, 0, width, height };
            gdouble rms_psdf = sqrt(gwy_field_mean(psdf, &intpart, NULL, 0)
                                    *psdf->xreal*width/psdf->xres
                                    *psdf->yreal*height/psdf->yres);
            gwy_assert_floatval(rms_psdf, rms_ref, 1e-10*rms_ref);
            g_object_unref(psdf);
            g_object_unref(field);
        }
    }

    g_rand_free(rng);
}

static gdouble
angavg_test_func_1(gdouble x, G_GNUC_UNUSED gpointer user_data)
{
    return cos(x);
}

static gdouble
angavg_test_func_2(gdouble x, G_GNUC_UNUSED gpointer user_data)
{
    return x*x/(1.0 + x);
}

static gdouble
angavg_test_func_3(gdouble x, G_GNUC_UNUSED gpointer user_data)
{
    return exp(-0.5*x*x);
}

void
test_field_distributions_angular_average_full(void)
{
    enum { max_size = 74, niter = 100 };
    static const GwyRealFunc funcs[] = {
        &angavg_test_func_1, &angavg_test_func_2, &angavg_test_func_3,
    };
    GRand *rng = g_rand_new_with_seed(42);

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 12, max_size);
        guint yres = g_rand_int_range(rng, 12, max_size);
        guint npoints = g_rand_int_range(rng, (xres+yres)/4, xres+yres);
        gdouble xreal = g_rand_double(rng) + 0.2;
        gdouble yreal = g_rand_double(rng) + 0.2;
        gdouble xoff = 2.0*g_rand_double(rng) - 1.5;
        gdouble yoff = 2.0*g_rand_double(rng) - 1.5;
        GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
        g_object_set(field,
                     "x-real", xreal, "y-real", yreal,
                     "x-offset", xoff, "y-offset", yoff,
                     NULL);
        guint fid = g_rand_int_range(rng, 0, G_N_ELEMENTS(funcs));
        GwyRealFunc func = funcs[fid];

        for (guint i = 0; i < yres; i++) {
            gdouble y = (i + 0.5)*gwy_field_dy(field) + field->yoff;
            for (guint j = 0; j < xres; j++) {
                gdouble x = (j + 0.5)*gwy_field_dx(field) + field->xoff;
                field->data[i*xres + j] = func(hypot(x, y), NULL);
            }
        }

        GwyCurve *aavg = gwy_field_angular_average(field, NULL,
                                                   NULL, GWY_MASK_IGNORE, 0);
        GwyCurve *aavgn = gwy_field_angular_average(field, NULL,
                                                    NULL, GWY_MASK_IGNORE,
                                                    npoints);

        g_assert_cmpuint(aavg->n, >=, MIN(xres, yres)/2);
        g_assert_cmpuint(aavg->n, <=, 4*(xres + yres));
        for (guint i = 0; i < aavg->n; i++) {
            gwy_assert_floatval(aavg->data[i].y, func(aavg->data[i].x, NULL),
                                0.02);
        }

        // For non-masked data we should get reasonably close to the requested
        // npoints.
        g_assert_cmpuint(aavgn->n, >=, MAX(5*npoints/6, 1));
        g_assert_cmpuint(aavgn->n, <=, npoints+2);
        for (guint i = 0; i < aavgn->n; i++) {
            gwy_assert_floatval(aavgn->data[i].y, func(aavgn->data[i].x, NULL),
                                0.06);
        }

        g_object_unref(aavgn);
        g_object_unref(aavg);
        g_object_unref(field);
    }

    g_rand_free(rng);
}

void
test_field_distributions_angular_average_partial(void)
{
    enum { max_size = 74, niter = 100 };
    static const GwyRealFunc funcs[] = {
        &angavg_test_func_1, &angavg_test_func_2, &angavg_test_func_3,
    };
    GRand *rng = g_rand_new_with_seed(42);

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 12, max_size);
        guint yres = g_rand_int_range(rng, 12, max_size);
        guint width = g_rand_int_range(rng, 5, xres+1);
        guint height = g_rand_int_range(rng, 5, yres+1);
        guint col = g_rand_int_range(rng, 0, xres-width+1);
        guint row = g_rand_int_range(rng, 0, yres-height+1);
        guint npoints = g_rand_int_range(rng, (width+height)/4, width+height);
        gdouble xreal = g_rand_double(rng) + 0.2;
        gdouble yreal = g_rand_double(rng) + 0.2;
        gdouble xoff = 2.0*g_rand_double(rng) - 1.5;
        gdouble yoff = 2.0*g_rand_double(rng) - 1.5;
        GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
        g_object_set(field,
                     "x-real", xreal, "y-real", yreal,
                     "x-offset", xoff, "y-offset", yoff,
                     NULL);
        GwyFieldPart fpart = { col, row, width, height };
        guint fid = g_rand_int_range(rng, 0, G_N_ELEMENTS(funcs));
        GwyRealFunc func = funcs[fid];

        gwy_field_fill_full(field, NAN);
        for (guint i = row; i < row + height; i++) {
            gdouble y = (i + 0.5)*gwy_field_dy(field) + field->yoff;
            for (guint j = col; j < col + width; j++) {
                gdouble x = (j + 0.5)*gwy_field_dx(field) + field->xoff;
                field->data[i*xres + j] = func(hypot(x, y), NULL);
            }
        }

        GwyCurve *aavg = gwy_field_angular_average(field, &fpart,
                                                   NULL, GWY_MASK_IGNORE, 0);
        GwyCurve *aavgn = gwy_field_angular_average(field, &fpart,
                                                    NULL, GWY_MASK_IGNORE,
                                                    npoints);

        g_assert_cmpuint(aavg->n, >=, MIN(width, height)/2);
        g_assert_cmpuint(aavg->n, <=, 4*(width + height));
        for (guint i = 0; i < aavg->n; i++) {
            g_assert(isfinite(aavg->data[i].y));
            gwy_assert_floatval(aavg->data[i].y, func(aavg->data[i].x, NULL),
                                0.03);
        }

        // For non-masked data we should get reasonably close to the requested
        // npoints.
        g_assert_cmpuint(aavgn->n, >=, MAX(5*npoints/6, 1));
        g_assert_cmpuint(aavgn->n, <=, npoints+2);
        for (guint i = 0; i < aavgn->n; i++) {
            g_assert(isfinite(aavgn->data[i].y));
            gwy_assert_floatval(aavgn->data[i].y, func(aavgn->data[i].x, NULL),
                                0.08);
        }

        g_object_unref(aavgn);
        g_object_unref(aavg);
        g_object_unref(field);
    }

    g_rand_free(rng);
}

void
test_field_distributions_angular_average_masked(void)
{
    enum { max_size = 74, niter = 100 };
    static const GwyRealFunc funcs[] = {
        &angavg_test_func_1, &angavg_test_func_2, &angavg_test_func_3,
    };
    GRand *rng = g_rand_new_with_seed(42);

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 12, max_size);
        guint yres = g_rand_int_range(rng, 12, max_size);
        guint npoints = g_rand_int_range(rng, (xres+yres)/4, xres+yres);
        gdouble xreal = g_rand_double(rng) + 0.2;
        gdouble yreal = g_rand_double(rng) + 0.2;
        gdouble xoff = 2.0*g_rand_double(rng) - 1.5;
        gdouble yoff = 2.0*g_rand_double(rng) - 1.5;
        GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
        g_object_set(field,
                     "x-real", xreal, "y-real", yreal,
                     "x-offset", xoff, "y-offset", yoff,
                     NULL);
        guint fid = g_rand_int_range(rng, 0, G_N_ELEMENTS(funcs));
        GwyRealFunc func = funcs[fid];
        GwyMaskField *mask = random_mask_field(xres, yres, rng);

        gwy_field_fill_full(field, NAN);
        for (guint i = 0; i < yres; i++) {
            gdouble y = (i + 0.5)*gwy_field_dy(field) + field->yoff;
            for (guint j = 0; j < xres; j++) {
                if (gwy_mask_field_get(mask, j, i)) {
                    gdouble x = (j + 0.5)*gwy_field_dx(field) + field->xoff;
                    field->data[i*xres + j] = func(hypot(x, y), NULL);
                }
            }
        }

        GwyCurve *aavg = gwy_field_angular_average(field, NULL,
                                                   mask, GWY_MASK_INCLUDE, 0);
        GwyCurve *aavgn = gwy_field_angular_average(field, NULL,
                                                    mask, GWY_MASK_INCLUDE,
                                                    npoints);

        g_assert_cmpuint(aavg->n, >=, MIN(xres, yres)/2);
        g_assert_cmpuint(aavg->n, <=, 4*(xres + yres));
        for (guint i = 0; i < aavg->n; i++) {
            g_assert(isfinite(aavg->data[i].y));
            gwy_assert_floatval(aavg->data[i].y, func(aavg->data[i].x, NULL),
                                0.03);
        }

        g_assert_cmpuint(aavgn->n, >=, MAX(npoints/3, 1));
        g_assert_cmpuint(aavgn->n, <=, npoints+2);
        for (guint i = 0; i < aavgn->n; i++) {
            g_assert(isfinite(aavgn->data[i].y));
            gwy_assert_floatval(aavgn->data[i].y, func(aavgn->data[i].x, NULL),
                                0.08);
        }

        g_object_unref(mask);
        g_object_unref(aavgn);
        g_object_unref(aavg);
        g_object_unref(field);
    }

    g_rand_free(rng);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
