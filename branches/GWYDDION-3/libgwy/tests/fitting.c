/*
 *  $Id$
 *  Copyright (C) 2009 David Nečas (Yeti).
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
 * Fit Task
 *
 ***************************************************************************/

static gboolean
gaussian_point(gdouble x,
               gdouble *retval,
               gdouble xoff,
               gdouble yoff,
               gdouble b,
               gdouble a)
{
    x = (x - xoff)/b;
    *retval = yoff + a*exp(-x*x);
    return b != 0.0;
}

static gboolean
gaussian_vector(guint i,
                gpointer user_data,
                gdouble *retval,
                gdouble xoff,
                gdouble yoff,
                gdouble b,
                gdouble a)
{
    GwyXY *pts = (GwyXY*)user_data;
    gdouble x = (pts[i].x - xoff)/b;
    *retval = yoff + a*exp(-x*x) - pts[i].y;
    return b != 0.0;
}

static gboolean
gaussian_vfunc(guint i,
               gpointer user_data,
               gdouble *retval,
               const gdouble *params)
{
    GwyXY *pts = (GwyXY*)user_data;
    gdouble x = (pts[i].x - params[0])/params[2];
    *retval = params[1] + params[3]*exp(-x*x) - pts[i].y;
    return params[2] != 0.0;
}

static GwyXY*
make_gaussian_data(gdouble xoff,
                   gdouble yoff,
                   gdouble b,
                   gdouble a,
                   guint ndata,
                   guint seed)
{
    GRand *rng = g_rand_new();
    g_rand_set_seed(rng, seed);
    GwyXY *data = g_new(GwyXY, ndata);
    gdouble xmin = xoff - b*(2 + 3*g_rand_double(rng));
    gdouble xmax = xoff + b*(2 + 3*g_rand_double(rng));
    gdouble noise = 0.15*a;
    for (guint i = 0; i < ndata; i++) {
        data[i].x = g_rand_double_range(rng, xmin, xmax);
        gaussian_point(data[i].x, &data[i].y, xoff, yoff, b, a);
        data[i].y += g_rand_double_range(rng, -noise, noise);
    }
    g_rand_free(rng);
    return data;
}

static void
check_fit(GwyFitTask *fittask,
          const gdouble *param_good)
{
    GwyFitter *fitter = gwy_fit_task_get_fitter(fittask);
    guint nparam = gwy_fitter_get_n_params(fitter);
    gdouble res_init = gwy_fit_task_eval_residuum(fittask);
    g_assert_cmpfloat(res_init, >, 0.0);
    g_assert(gwy_fit_task_fit(fittask));
    gdouble res = gwy_fitter_residuum(fitter);
    g_assert_cmpfloat(res, >, 0.0);
    g_assert_cmpfloat(res, <, 0.01*res_init);
    gdouble param_final[nparam];
    g_assert(gwy_fitter_get_params(fitter, param_final));
    /* Conservative result check */
    gdouble eps = 0.2;
    g_assert_cmpfloat(fabs(param_final[0] - param_good[0]),
                      <=,
                      eps*fabs(param_good[0]));
    g_assert_cmpfloat(fabs(param_final[1] - param_good[1]),
                      <=,
                      eps*fabs(param_good[1]));
    g_assert_cmpfloat(fabs(param_final[2] - param_good[2]),
                      <=,
                      eps*fabs(param_good[2]));
    g_assert_cmpfloat(fabs(param_final[3] - param_good[3]),
                      <=,
                      eps*fabs(param_good[3]));
    /* Error estimate check */
    gdouble error[nparam];
    eps = 0.3;
    g_assert(gwy_fit_task_param_errors(fittask, TRUE, error));
    g_assert_cmpfloat(fabs((param_final[0] - param_good[0])/error[0]),
                      <=,
                      1.0 + eps);
    g_assert_cmpfloat(fabs((param_final[1] - param_good[1])/error[1]),
                      <=,
                      1.0 + eps);
    g_assert_cmpfloat(fabs((param_final[2] - param_good[2])/error[2]),
                      <=,
                      1.0 + eps);
    g_assert_cmpfloat(fabs((param_final[3] - param_good[3])/error[3]),
                      <=,
                      1.0 + eps);
}

void
test_fitter_props(void)
{
    GwyFitter *fitter = gwy_fitter_new();
    g_assert(GWY_IS_FITTER(fitter));

    g_object_set(fitter,
                 "n-params", 3,
                 "max-iters", 1234,
                 "successes-to-get-bored", 13,
                 "lambda-max", 4242.0,
                 "lambda-start", G_PI,
                 "lambda-increase", G_E,
                 "lambda-decrease", G_SQRT2,
                 "min-residuum-change", 0.0042,
                 "min-param-change", 0.000042,
                 NULL);
    g_assert_cmpuint(gwy_fitter_get_n_params(fitter), ==, 3);
    guint n_params, max_iters, successes_to_get_bored;
    gdouble lambda_max, lambda_start, lambda_increase, lambda_decrease,
            residuum_change_min, param_change_min;
    g_object_get(fitter,
                 "n-params", &n_params,
                 "max-iters", &max_iters,
                 "successes-to-get-bored", &successes_to_get_bored,
                 "lambda-max", &lambda_max,
                 "lambda-start", &lambda_start,
                 "lambda-increase", &lambda_increase,
                 "lambda-decrease", &lambda_decrease,
                 "min-residuum-change", &residuum_change_min,
                 "min-param-change", &param_change_min,
                 NULL);
    g_assert_cmpuint(n_params, ==, 3);
    g_assert_cmpuint(max_iters, ==, 1234);
    g_assert_cmpuint(successes_to_get_bored, ==, 13);
    g_assert_cmpfloat(lambda_max, ==, 4242.0);
    g_assert_cmpfloat(lambda_start, ==, G_PI);
    g_assert_cmpfloat(lambda_increase, ==, G_E);
    g_assert_cmpfloat(lambda_decrease, ==, G_SQRT2);
    g_assert_cmpfloat(residuum_change_min, ==, 0.0042);
    g_assert_cmpfloat(param_change_min, ==, 0.000042);
    g_object_unref(fitter);
}

void
test_fit_task_point(void)
{
    enum { nparam = 4, ndata = 100 };
    const gdouble param[nparam] = { 1e-5, 1e6, 1e-4, 2e5 };
    const gdouble param_init[nparam] = { 4e-5, -1e6, 2e-4, 4e5 };
    GwyFitTask *fittask = gwy_fit_task_new();
    GwyFitter *fitter = gwy_fit_task_get_fitter(fittask);
    GwyXY *data = make_gaussian_data(param[0], param[1], param[2], param[3],
                                     ndata, 42);
    gwy_fit_task_set_point_function
        (fittask, nparam, (GwyFitTaskPointFunc)gaussian_point);
    gwy_fit_task_set_point_data(fittask, data, ndata);
    gwy_fitter_set_params(fitter, param_init);
    check_fit(fittask, param);
    g_free(data);
    g_object_unref(fittask);
}

void
test_fit_task_fixed(void)
{
    enum { nparam = 4, ndata = 100 };
    const gdouble param[nparam] = { 1e-5, 1e6, 1e-4, 2e5 };
    const gdouble param_init[nparam] = { 4e-5, -1e6, 2e-4, 4e5 };
    GwyFitTask *fittask = gwy_fit_task_new();
    GwyFitter *fitter = gwy_fit_task_get_fitter(fittask);
    GwyXY *data = make_gaussian_data(param[0], param[1], param[2], param[3],
                                     ndata, 42);
    gwy_fit_task_set_point_function
        (fittask, nparam, (GwyFitTaskPointFunc)gaussian_point);
    gwy_fit_task_set_point_data(fittask, data, ndata);
    for (guint i = 0; i < nparam; i++) {
        gwy_fitter_set_params(fitter, param_init);
        gwy_fit_task_set_fixed_param(fittask, i, TRUE);
        gdouble res_init = gwy_fit_task_eval_residuum(fittask);
        g_assert_cmpfloat(res_init, >, 0.0);
        g_assert(gwy_fit_task_fit(fittask));
        gdouble res = gwy_fitter_residuum(fitter);
        g_assert_cmpfloat(res, >, 0.0);
        g_assert_cmpfloat(res, <, 0.1*res_init);
        /* Fixed params are not touched. */
        gdouble param_final[nparam];
        g_assert(gwy_fitter_get_params(fitter, param_final));
        g_assert_cmpfloat(param_final[i], ==, param_init[i]);
        gdouble error[nparam];
        g_assert(gwy_fit_task_param_errors(fittask, TRUE, error));
        g_assert_cmpfloat(error[i], ==, 0);
        gwy_fit_task_set_fixed_param(fittask, i, FALSE);
    }
    g_free(data);
    g_object_unref(fittask);
}

void
test_fit_task_vector(void)
{
    enum { nparam = 4, ndata = 100 };
    const gdouble param[nparam] = { 1e-5, 1e6, 1e-4, 2e5 };
    const gdouble param_init[nparam] = { 4e-5, -1e6, 2e-4, 4e5 };
    GwyFitTask *fittask = gwy_fit_task_new();
    GwyFitter *fitter = gwy_fit_task_get_fitter(fittask);
    GwyXY *data = make_gaussian_data(param[0], param[1], param[2], param[3],
                                     ndata, 42);
    gwy_fit_task_set_vector_function
        (fittask, nparam, (GwyFitTaskVectorFunc)gaussian_vector);
    gwy_fit_task_set_vector_data(fittask, data, ndata);
    gwy_fitter_set_params(fitter, param_init);
    check_fit(fittask, param);
    g_free(data);
    g_object_unref(fittask);
}

void
test_fit_task_vfunc(void)
{
    enum { nparam = 4, ndata = 100 };
    const gdouble param[nparam] = { 1e-5, 1e6, 1e-4, 2e5 };
    const gdouble param_init[nparam] = { 4e-5, -1e6, 2e-4, 4e5 };
    GwyFitTask *fittask = gwy_fit_task_new();
    GwyFitter *fitter = gwy_fit_task_get_fitter(fittask);
    GwyXY *data = make_gaussian_data(param[0], param[1], param[2], param[3],
                                     ndata, 42);
    gwy_fit_task_set_vector_vfunction
        (fittask, nparam, (GwyFitTaskVectorVFunc)gaussian_vfunc, NULL);
    gwy_fit_task_set_vector_data(fittask, data, ndata);
    gwy_fitter_set_params(fitter, param_init);
    check_fit(fittask, param);
    g_free(data);
    g_object_unref(fittask);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
