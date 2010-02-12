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

static void
test_fit_func_builtin_one(const gchar *name,
                          guint expected_nparams,
                          const gchar* const *param_names)
{
    enum { ndata = 100 };
    GRand *rng = g_rand_new();
    g_rand_set_seed(rng, 42);

    GwyFitFunc *fitfunc = gwy_fit_func_new(name, "builtin");
    g_assert(GWY_IS_FIT_FUNC(fitfunc));

    guint nparams = gwy_fit_func_get_n_params(fitfunc);
    g_assert_cmpuint(nparams, ==, expected_nparams);

    for (guint i = 0; i < nparams; i++) {
        const gchar *pname = gwy_fit_func_get_param_name(fitfunc, i);
        guint j;
        for (j = 0; j < nparams; j++) {
            if (gwy_strequal(pname, param_names[j]))
                break;
        }
        if (j == nparams)
            g_error("Function %s has an unexpected parameter %s.",
                    name, pname);
    }

    for (guint i = 0; i < nparams; i++) {
        guint j;
        for (j = 0; j < nparams; j++) {
            const gchar *pname = gwy_fit_func_get_param_name(fitfunc, j);
            if (gwy_strequal(pname, param_names[i]))
                break;
        }
        if (j == nparams)
            g_error("Function %s lacks an expected parameter %s.",
                    name, param_names[i]);
    }

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
    eps = 1.0;
    g_assert(gwy_fit_task_get_param_errors(fittask, TRUE, error));
    for (guint i = 0; i < nparams; i++) {
        g_assert_cmpfloat(error[i], >=, 0.0);
        g_assert_cmpfloat(fabs(param[i] - param0[i]), <=, (1.0 + eps)*error[i]);
    }

    g_object_unref(curve);
    g_object_unref(curve0);
    g_object_unref(fitfunc);
    g_rand_free(rng);
}

void
test_fit_func_builtin_constant(void)
{
    const gchar *pnames[] = { "a" };
    test_fit_func_builtin_one("Constant", G_N_ELEMENTS(pnames), pnames);
}

void
test_fit_func_builtin_exponential(void)
{
    const gchar *pnames[] = { "a", "b", "y<sub>0</sub>" };
    test_fit_func_builtin_one("Exponential", G_N_ELEMENTS(pnames), pnames);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
