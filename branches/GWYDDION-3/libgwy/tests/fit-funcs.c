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
 * Fit Func
 *
 ***************************************************************************/

void
test_fit_func_builtin_constant(void)
{
    enum { ndata = 100, expected_nparams = 1 };
    GRand *rng = g_rand_new();
    g_rand_set_seed(rng, 42);

    GwyFitFunc *fitfunc = gwy_fit_func_new("Constant", "builtin");
    g_assert(GWY_IS_FIT_FUNC(fitfunc));

    guint nparams = gwy_fit_func_get_n_params(fitfunc);
    g_assert_cmpuint(nparams, ==, expected_nparams);

    gdouble param0[nparams];
    for (guint i = 0; i < nparams; i++) {
        // Be very conservative with parameters as they can be exponents of
        // Gaussians and such.  Only use positive values, these are usually
        // safe.
        param0[i] = g_rand_double_range(rng, 0.2, 5.0);
    }

    GwyCurve *curve0 = gwy_curve_new_sized(ndata);
    // Do not use curve_randomize(), it generates unsafe data and also
    // generates y data we would throw away.
    for (guint j = 0; j < ndata; j++) {
        GwyXY *pt = curve0->data + j;
        pt->x = g_rand_double_range(rng, 0.1, 10.0);
        gboolean evaluate_ok = gwy_fit_func_evaluate(fitfunc, pt->x, param0,
                                                     &pt->y);
        g_assert(evaluate_ok);
    }

    GwyCurve *curve = gwy_curve_duplicate(curve0);
    // Use multiplicative randomizing to avoid negative values also here
    for (guint j = 0; j < ndata; j++)
        curve->data[j].y *= (1.0 + g_rand_double_range(rng, -0.1, 0.1));
    gwy_fit_func_set_data(fitfunc, curve->data, curve->n);

    gdouble param[nparams];
    gboolean estimate_ok = gwy_fit_func_estimate(fitfunc, param);
    g_assert(estimate_ok);

    GwyFitTask *fittask = gwy_fit_func_get_fit_task(fitfunc);
    g_assert(GWY_IS_FIT_TASK(fittask));

    gdouble res_init = gwy_fit_task_eval_residuum(fittask);
    g_assert_cmpfloat(res_init, >, 0.0);

    gboolean fitting_ok = gwy_fit_task_fit(fittask);
    g_assert(fitting_ok);

    GwyFitter *fitter = gwy_fit_task_get_fitter(fittask);
    g_assert_cmpuint(gwy_fitter_get_n_params(fitter), ==, expected_nparams);
    gdouble res = gwy_fitter_get_residuum(fitter);
    g_assert_cmpfloat(res, >, 0.0);
    g_assert_cmpfloat(res, <, res_init);
    g_assert(gwy_fitter_get_params(fitter, param));

    /* Conservative result check */
    gdouble eps = 0.3;
    for (guint i = 0; i < nparams; i++) {
        g_assert_cmpfloat(fabs(param[i] - param0[i]), <=, eps*fabs(param0[i]));
    }

    /* Error estimate check */
    gdouble error[nparams];
    g_assert(gwy_fit_task_get_param_errors(fittask, TRUE, error));
    for (guint i = 0; i < nparams; i++) {
        g_assert_cmpfloat(fabs((param[i] - param0[i])/error[i]), <=, 1.0 + eps);
    }

    g_object_unref(curve);
    g_object_unref(curve0);
    g_object_unref(fitfunc);
    g_rand_free(rng);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
