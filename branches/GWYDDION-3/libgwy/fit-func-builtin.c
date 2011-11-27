/*
 *  $Id$
 *  Copyright (C) 2010,2011 David Nečas (Yeti).
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

#include <glib/gi18n-lib.h>
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
    .group = NC_("function group", "Elementary"),
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
    .group = NC_("function group", "Elementary"),
    .formula = "<i>y</i>₀ + <i>a</i> exp(<i>x</i>/<i>b</i>)",
    .nparams = G_N_ELEMENTS(exp_param),
    .param = exp_param,
    .function = exp_function,
    .estimate = exp_estimate,
};

/****************************************************************************
 *
 * Two-sided exponential
 *
 ****************************************************************************/

static const FitFuncParam peak_param[] = {
   { "x₀", 1, 0, },
   { "y₀", 0, 1, },
   { "a",  0, 1, },
   { "b",  1, 0, },
};

static gboolean
two_exp_function(gdouble x,
                 const gdouble *param,
                 gdouble *v)
{
    gdouble x0 = param[0], y0_ = param[1], a = param[2], b = param[3];
    gdouble u = (x - x0)/b;
    *v = a*exp(-fabs(u)) + y0_;
    return b != 0;
}

static gboolean
two_exp_estimate(G_GNUC_UNUSED const GwyXY *pts,
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
    param[3] = 1.44*estim[ESTIMATOR_HWPEAK];
    return TRUE;
}

static const BuiltinFitFunc two_exp_builtin = {
    .group = NC_("function group", "Elementary"),
    .formula = "<i>y</i>₀ + <i>a</i> exp(-|<i>x</i> - <i>x</i>₀|/<i>b</i>)",
    .nparams = G_N_ELEMENTS(peak_param),
    .param = peak_param,
    .function = two_exp_function,
    .estimate = two_exp_estimate,
};

/****************************************************************************
 *
 * Gaussian
 *
 ****************************************************************************/

static gboolean
gauss_function(gdouble x,
               const gdouble *param,
               gdouble *v)
{
    gdouble x0 = param[0], y0_ = param[1], a = param[2], b = param[3];
    gdouble u = (x - x0)/b;
    *v = a*exp(-u*u) + y0_;
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
    .group = NC_("function group", "Elementary"),
    .formula = "<i>y</i>₀ + <i>a</i> exp[−(<i>x</i> − <i>x</i>₀)²/<i>b</i>²]",
    .nparams = G_N_ELEMENTS(peak_param),
    .param = peak_param,
    .function = gauss_function,
    .estimate = gauss_estimate,
};

/****************************************************************************
 *
 * Lorentzian
 *
 ****************************************************************************/

static gboolean
lorentz_function(gdouble x,
                 const gdouble *param,
                 gdouble *v)
{
    gdouble x0 = param[0], y0_ = param[1], a = param[2], b = param[3];
    gdouble u = (x - x0)/b;
    *v = a/(1.0 + u*u) + y0_;
    return b != 0;
}

static gboolean
lorentz_estimate(G_GNUC_UNUSED const GwyXY *pts,
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
    param[3] = estim[ESTIMATOR_HWPEAK];
    return TRUE;
}

static const BuiltinFitFunc lorentz_builtin = {
    .group = NC_("function group", "Elementary"),
    .formula = "<i>y</i>₀ + <i>ab</i>²/((<i>x</i> - <i>x</i>₀)² + <i>b</i>²)",
    .nparams = G_N_ELEMENTS(peak_param),
    .param = peak_param,
    .function = lorentz_function,
    .estimate = lorentz_estimate,
};

/****************************************************************************
 *
 * ACF Exponential
 *
 ****************************************************************************/

static const FitFuncParam acf_param[] = {
   { "σ", 0, 0, },
   { "T", 1, 0, },
};

static GwyUnit*
acf_derive_units(guint param,
                 const GwyUnit *unit_x,
                 const GwyUnit *unit_y)
{
    GwyUnit *unit = gwy_unit_new();

    if (param == 0)
        gwy_unit_nth_root(unit, unit_y, 2);
    else if (param == 1)
        gwy_unit_assign(unit, unit_x);
    else {
        g_assert_not_reached();
    }

    return unit;
}

static gboolean
acf_exp_function(gdouble x,
                 const gdouble *param,
                 gdouble *v)
{
    gdouble sigma = param[0], T = param[1];
    *v = sigma*sigma*exp(-x/T);
    return T != 0;
}

static gboolean
acf_exp_estimate(const GwyXY *pts,
                 guint npoints,
                 const gdouble *estim,
                 gdouble *param)
{
    gdouble ymax = estim[ESTIMATOR_YMAX], ymin = estim[ESTIMATOR_YMIN];
    if (ymax <= 0.0) {
        param[0] = ymax - ymin;
        param[1] = 0.1*(estim[ESTIMATOR_XMAX]- estim[ESTIMATOR_XMIN]);
        return FALSE;
    }

    gdouble s = 0.5*pts[0].x*pts[0].y/ymax;
    for (guint i = 1; i+1 < npoints; i++)
        s += (pts[i+1].x - pts[i-1].x)*pts[i].y/ymax;

    param[0] = ymax;
    param[1] = s;
    return s > 0.0;
}

static const BuiltinFitFunc acf_exp_builtin = {
    .group = NC_("function group", "Autocorrelation"),
    .formula = "<i>σ</i>² exp(<i>x</i>/<i>T</i>)",
    .nparams = G_N_ELEMENTS(acf_param),
    .param = acf_param,
    .function = acf_exp_function,
    .estimate = acf_exp_estimate,
    .derive_units = acf_derive_units,
};

/****************************************************************************
 *
 * ACF Gaussian
 *
 ****************************************************************************/

static gboolean
acf_gauss_function(gdouble x,
                   const gdouble *param,
                   gdouble *v)
{
    gdouble sigma = param[0], T = param[1], u = x/T;
    *v = sigma*sigma*exp(-u*u);
    return T != 0;
}

static gboolean
acf_gauss_estimate(const GwyXY *pts,
                   guint npoints,
                   const gdouble *estim,
                   gdouble *param)
{
    gdouble ymax = estim[ESTIMATOR_YMAX], ymin = estim[ESTIMATOR_YMIN];
    if (ymax <= 0.0) {
        param[0] = ymax - ymin;
        param[1] = 0.1*(estim[ESTIMATOR_XMAX]- estim[ESTIMATOR_XMIN]);
        return FALSE;
    }

    gdouble s = 0.5*pts[0].x*pts[0].y/ymax;
    for (guint i = 1; i+1 < npoints; i++)
        s += (pts[i+1].x - pts[i-1].x)*pts[i].y/ymax;

    param[0] = ymax;
    param[1] = 0.8*s;
    return s > 0.0;
}

static const BuiltinFitFunc acf_gauss_builtin = {
    .group = NC_("function group", "Autocorrelation"),
    .formula = "<i>σ</i>² exp(−<i>x</i>²/<i>b</i>²)",
    .nparams = G_N_ELEMENTS(acf_param),
    .param = acf_param,
    .function = acf_gauss_function,
    .estimate = acf_gauss_estimate,
    .derive_units = acf_derive_units,
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
    add_builtin(NC_("function", "Constant"), const_builtin);
    add_builtin(NC_("function", "Exponential"), exp_builtin);
    add_builtin(NC_("function", "Exponential (two-side)"), two_exp_builtin);
    add_builtin(NC_("function", "Gaussian"), gauss_builtin);
    add_builtin(NC_("function", "Lorentzian"), lorentz_builtin);
    add_builtin(NC_("function", "ACF Exponential"), acf_exp_builtin);
    add_builtin(NC_("function", "ACF Gaussian"), acf_gauss_builtin);
    return builtins;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
