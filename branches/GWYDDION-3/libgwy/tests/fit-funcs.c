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
fit_func_one(const gchar *name,
             const gchar *group,
             guint expected_nparams,
             const gchar* const *param_names)
{
    enum { ndata = 500 };
    GRand *rng = g_rand_new();
    g_rand_set_seed(rng, 42);

    GwyFitFunc *fitfunc = gwy_fit_func_new(name, group);
    g_assert(GWY_IS_FIT_FUNC(fitfunc));
    gchar *fname, *fgroup;
    g_object_get(fitfunc, "name", &fname, "group", &fgroup, NULL);
    g_assert_cmpstr(fname, ==, name);
    g_assert_cmpstr(fgroup, ==, group);
    g_free(fname);
    g_free(fgroup);

    guint nparams = gwy_fit_func_n_params(fitfunc);
    g_assert_cmpuint(nparams, ==, expected_nparams);

    for (guint i = 0; i < nparams; i++) {
        const gchar *pname = gwy_fit_func_param_name(fitfunc, i);
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
            const gchar *pname = gwy_fit_func_param_name(fitfunc, j);
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
        if (g_test_verbose())
            g_printerr("True value %s = %g\n",
                       gwy_fit_func_param_name(fitfunc, i), param0[i]);
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
    for (guint j = 0; j < ndata; j++) {
        curve->data[j].y *= (1.0 + g_rand_double_range(rng, -0.1, 0.1));
        //g_printerr("%g %g\n", curve->data[j].x, curve->data[j].y);
    }
    gwy_fit_func_set_data(fitfunc, curve->data, curve->n);

    gdouble param[nparams];
    gboolean estimate_ok = gwy_fit_func_estimate(fitfunc, param);
    g_assert(estimate_ok);
    if (g_test_verbose()) {
        for (guint i = 0; i < nparams; i++)
            g_printerr("Estimate %s = %g\n",
                       gwy_fit_func_param_name(fitfunc, i), param[i]);
    }

    GwyFitTask *fittask = gwy_fit_func_get_fit_task(fitfunc);
    g_assert(GWY_IS_FIT_TASK(fittask));

    gdouble res_init = gwy_fit_task_eval_residuum(fittask);
    g_assert_cmpfloat(res_init, >, 0.0);

    gboolean fitting_ok = gwy_fit_task_fit(fittask);
    g_assert(fitting_ok);

    GwyFitter *fitter = gwy_fit_task_get_fitter(fittask);
    g_assert_cmpuint(gwy_fitter_get_n_params(fitter), ==, expected_nparams);
    gdouble res = gwy_fitter_residuum(fitter);
    g_assert_cmpfloat(res, >, 0.0);
    // Cannot use a sharp inequality as Constant has too good `estimate'
    g_assert_cmpfloat(res, <=, res_init);
    g_assert(gwy_fitter_get_params(fitter, param));
    if (g_test_verbose()) {
        for (guint i = 0; i < nparams; i++)
            g_printerr("Fitting result %s = %g\n",
                       gwy_fit_func_param_name(fitfunc, i), param[i]);
    }

    /* Conservative result check */
    gdouble eps = 0.3;
    for (guint i = 0; i < nparams; i++) {
        g_assert_cmpfloat(fabs(param[i] - param0[i]), <=, eps*fabs(param0[i]));
    }

    /* Error estimate check */
    gdouble error[nparams];
    eps = 0.5;
    g_assert(gwy_fit_task_param_errors(fittask, TRUE, error));
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
    const gchar *param_names[] = { "a" };
    fit_func_one("Constant", "builtin",
                 G_N_ELEMENTS(param_names), param_names);
}

void
test_fit_func_builtin_exponential(void)
{
    const gchar *param_names[] = { "a", "b", "y<sub>0</sub>" };
    fit_func_one("Exponential", "builtin",
                 G_N_ELEMENTS(param_names), param_names);
}

void
test_fit_func_builtin_gaussian(void)
{
    const gchar *param_names[] = { "a", "b", "x<sub>0</sub>", "y<sub>0</sub>" };
    fit_func_one("Gaussian", "builtin",
                 G_N_ELEMENTS(param_names), param_names);
}

void
test_fit_func_user(void)
{
    const gchar *param_names[] = { "a", "b" };
    GwyInventory *userfitfuncs = gwy_user_fit_funcs();
    g_assert(GWY_IS_INVENTORY(userfitfuncs));
    if (gwy_inventory_get(userfitfuncs, "Linear"))
        gwy_inventory_delete(userfitfuncs, "Linear");
    GwyUserFitFunc *linear = gwy_user_fit_func_new();
    g_assert(GWY_IS_USER_FIT_FUNC(linear));
    GwyResource *resource = GWY_RESOURCE(linear);
    gwy_resource_set_name(resource, "Linear");
    gwy_inventory_insert(userfitfuncs, linear);
    g_object_unref(linear);
    g_assert(gwy_user_fit_func_set_formula(linear, "a+b*x", NULL));
    GwyFitParam *a = gwy_user_fit_func_param(linear, "a");
    GwyFitParam *b = gwy_user_fit_func_param(linear, "b");
    g_assert(GWY_IS_FIT_PARAM(a));
    g_assert(GWY_IS_FIT_PARAM(b));
    gwy_fit_param_set_estimate(a, "ymean");
    gwy_fit_param_set_power_x(a, 0);
    gwy_fit_param_set_power_y(a, 1);
    gwy_fit_param_set_estimate(b, "(yxmax-yxmin)/(xmax-xmin)");
    gwy_fit_param_set_power_x(b, -1);
    gwy_fit_param_set_power_y(b, 1);
    fit_func_one("Linear", "userfitfunc",
                 G_N_ELEMENTS(param_names), param_names);

    GwyFitFunc *fitfunc = gwy_fit_func_new("Linear", "userfitfunc");
    g_assert(GWY_IS_FIT_FUNC(fitfunc));
    GwyUserFitFunc *userfitfunc = gwy_fit_func_get_user(fitfunc);
    g_assert(userfitfunc == linear);
    g_object_unref(fitfunc);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
