/*
 *  $Id$
 *  Copyright (C) 2010 David Necas (Yeti).
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

#include <string.h>
#include "libgwy/macros.h"
#include "libgwy/math.h"
#include "libgwy/fit-func.h"
#include "libgwy/fit-func-builtin.h"

/****************************************************************************
 *
 * Constant
 *
 ****************************************************************************/

static const FitFuncParam const_param[] = {
   { "a", 0, 1, },
};

static gboolean
const_function(G_GNUC_UNUSED gdouble x,
               const gdouble *param,
               gdouble *v)
{
    *v = *param;
    return TRUE;
}

static gboolean
const_estimate(G_GNUC_UNUSED const GwyXY *pts,
               G_GNUC_UNUSED guint npoints,
               const gdouble *estim,
               gdouble *param)
{
    param[0] = estim[ESTIMATOR_YMEAN];
    return TRUE;
}

static const BuiltinFitFunc const_builtin = {
    .formula = "<i>a</i>",
    .nparams = G_N_ELEMENTS(const_param),
    .param = const_param,
    .function = const_function,
    .estimate = const_estimate,
};

/****************************************************************************
 *
 * Exponential
 *
 ****************************************************************************/

static const FitFuncParam exp_param[] = {
   { "y₀", 0, 1, },
   { "a",  0, 1, },
   { "b",  1, 0, },
};

static gboolean
exp_function(gdouble x,
             const gdouble *param,
             gdouble *v)
{
    gdouble y0_ = param[0], a = param[1], b = param[2];
    *v = a*exp(x/b) + y0_;
    return b != 0;
}

static gboolean
exp_estimate(G_GNUC_UNUSED const GwyXY *pts,
             G_GNUC_UNUSED guint npoints,
             const gdouble *estim,
             gdouble *param)
{
    gdouble ymin = estim[ESTIMATOR_YMIN], ymax = estim[ESTIMATOR_YMAX];
    if (ymin == ymax) {
        param[0] = 0.0;
        param[1] = ymin;
        param[2] = 10*(estim[ESTIMATOR_XMAX]- estim[ESTIMATOR_XMIN]);
        return FALSE;
    }

    gdouble s = estim[ESTIMATOR_YMEAN];
    s -= (2.0*s < ymin + ymax) ? ymin : ymax;

    gdouble xymin = estim[ESTIMATOR_XYMIN], xymax = estim[ESTIMATOR_XYMAX];
    s *= xymax - xymin;

    param[2] = s/(ymax - ymin);
    param[1] = (ymax - ymin)/(exp(xymax/param[2]) - exp(xymin/param[2]));
    param[0] = ymin - param[1]*exp(xymin/param[2]);
    return TRUE;
}

static const BuiltinFitFunc exp_builtin = {
    .formula = "= <i>y</i>₀ + <i>a</i> exp(<i>x</i>/<i>b</i>)",
    .nparams = G_N_ELEMENTS(exp_param),
    .param = exp_param,
    .function = exp_function,
    .estimate = exp_estimate,
};

/****************************************************************************
 *
 * Gaussian
 *
 ****************************************************************************/

static const FitFuncParam gauss_param[] = {
   { "x₀", 1, 0, },
   { "y₀", 0, 1, },
   { "a",  0, 1, },
   { "b",  1, 0, },
};

static gboolean
gauss_function(gdouble x,
               const gdouble *param,
               gdouble *v)
{
    gdouble x0 = param[0], y0_ = param[1], a = param[2], b = param[3];
    gdouble t = (x - x0)/b;
    *v = a*exp(-t*t) + y0_;
    return b != 0;
}

static gboolean
gauss_estimate(G_GNUC_UNUSED const GwyXY *pts,
               G_GNUC_UNUSED guint npoints,
               const gdouble *estim,
               gdouble *param)
{
    if (!estim[ESTIMATOR_HWPEAK]) {
        param[0] = estim[ESTIMATOR_XMID];
        param[1] = estim[ESTIMATOR_YMIN];
        param[2] = 0.0;
        param[3] = 3*(estim[ESTIMATOR_XMAX]- estim[ESTIMATOR_XMIN]);
        return FALSE;
    }

    param[0] = estim[ESTIMATOR_XPEAK];
    param[1] = estim[ESTIMATOR_Y0PEAK];
    param[2] = estim[ESTIMATOR_APEAK];
    param[3] = 1.2*estim[ESTIMATOR_HWPEAK];
    return TRUE;
}

static const BuiltinFitFunc gauss_builtin = {
    .formula = "= <i>y</i>₀ + <i>a</i> exp[−(<i>x</i> − <i>x</i>₀)²/b²]",
    .nparams = G_N_ELEMENTS(gauss_param),
    .param = gauss_param,
    .function = gauss_function,
    .estimate = gauss_estimate,
};

/****************************************************************************
 *
 * Main
 *
 ****************************************************************************/

#define add_builtin(name, func) \
    g_hash_table_insert(builtins, (gpointer)name, (gpointer)&func)

GHashTable*
_gwy_fit_func_setup_builtins(void)
{
    GHashTable *builtins;

    builtins = g_hash_table_new(g_str_hash, g_str_equal);
    add_builtin("Constant", const_builtin);
    add_builtin("Exponential", exp_builtin);
    add_builtin("Gaussian", gauss_builtin);
    return builtins;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
