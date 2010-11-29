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

static const FitFuncParam const_param[] = {
   { "a",             0, 1, },
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
const_estimate(const GwyXY *pts,
               guint npoints,
               gdouble *param)
{
    gdouble s = 0.0;

    for (guint i = 0; i < npoints; i++)
        s += pts[i].y;
    param[0] = s/npoints;
    return TRUE;
}

static const BuiltinFitFunc const_builtin = {
    .formula = "<i>a</i>",
    .nparams = G_N_ELEMENTS(const_param),
    .param = const_param,
    .function = const_function,
    .estimate = const_estimate,
};

static const FitFuncParam exp_param[] = {
   { "y<sub>0</sub>", 0, 1, },
   { "a",             0, 1, },
   { "b",             1, 0, },
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
exp_estimate(const GwyXY *pts,
             guint npoints,
             gdouble *param)
{
    guint imin = 0, imax = 0;
    gdouble s = pts[0].y;

    for (guint i = 1; i < npoints; i++) {
        if (pts[i].y > pts[imax].y)
            imax = i;
        if (pts[i].y < pts[imin].y)
            imin = i;
        s += pts[i].y;
    }

    if (pts[imax].y == pts[imin].y) {
        param[0] = 0.0;
        param[1] = pts[imin].y;
        param[2] = 10*(pts[npoints-1].x - pts[0].x);
        return FALSE;
    }

    s /= npoints;
    if (2.0*s < pts[imax].y + pts[imin].y)
        s -= pts[imin].y;
    else
        s -= pts[imax].y;
    s *= pts[imax].x - pts[imin].x;

    param[2] = s/(pts[imax].y - pts[imin].y);
    param[1] = (pts[imax].y - pts[imin].y)/(exp(pts[imax].x/param[2])
                                            - exp(pts[imin].x/param[2]));
    param[0] = pts[imin].y - param[1]*exp(pts[imin].x/param[2]);

    return TRUE;
}

static const BuiltinFitFunc exp_builtin = {
    .formula = "= <i>y</i><sub>0</sub> + <i>a</i> exp(<i>x</i>/<i>b</i>)",
    .nparams = G_N_ELEMENTS(exp_param),
    .param = exp_param,
    .function = exp_function,
    .estimate = exp_estimate,
};

#define add_builtin(name, func) \
    g_hash_table_insert(builtins, (gpointer)name, (gpointer)&func)

GHashTable*
_gwy_fit_func_setup_builtins(void)
{
    GHashTable *builtins;

    builtins = g_hash_table_new(g_str_hash, g_str_equal);
    add_builtin("Constant", const_builtin);
    add_builtin("Exponential", exp_builtin);
    return builtins;
}


/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
