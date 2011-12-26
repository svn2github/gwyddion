/*
 *  $Id$
 *  Copyright (C) 2010-2011 David Nečas (Yeti).
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

/*< private_header >*/

#ifndef __LIBGWY_FIT_FUNC_BUILTIN_H__
#define __LIBGWY_FIT_FUNC_BUILTIN_H__

G_BEGIN_DECLS

// The order must match estimators[] in fit-func.c
enum {
    ESTIMATOR_XMIN,
    ESTIMATOR_XMID,
    ESTIMATOR_XMAX,
    ESTIMATOR_YMIN,
    ESTIMATOR_YMAX,
    ESTIMATOR_YMEAN,
    ESTIMATOR_YXMIN,
    ESTIMATOR_YXMID,
    ESTIMATOR_YXMAX,
    ESTIMATOR_XYMAX,
    ESTIMATOR_XYMIN,
    ESTIMATOR_XPEAK,
    ESTIMATOR_APEAK,
    ESTIMATOR_HWPEAK,
    ESTIMATOR_Y0PEAK,
    ESTIMATOR_INTEGR,
    N_ESTIMATORS
};

typedef struct {
    const gchar *name;
    gint power_x;
    gint power_y;
} FitFuncParam;

typedef struct {
    GHashTable *table;
    guint n;
    const gchar **names;
} BuiltinFitFuncTable;

typedef gboolean (*BuiltinFunction)(gdouble x,
                                    const gdouble *param,
                                    gdouble *value);
typedef gboolean (*BuiltinEstimate)(const GwyXY *points,
                                    guint npoints,
                                    const gdouble *estim,
                                    gdouble *param);
typedef GwyUnit* (*BuiltinDeriveUnits)(guint param,
                                       const GwyUnit *unit_x,
                                       const GwyUnit *unit_y);

typedef struct {
    const gchar *group;
    const gchar *formula;
    guint nparams;
    const FitFuncParam *param;
    BuiltinFunction function;
    BuiltinEstimate estimate;
    BuiltinDeriveUnits derive_units;
    // TODO: Specialized estimators, filters, weights, differentiation, unit
    // derivation, ...
} BuiltinFitFunc;

G_GNUC_INTERNAL
const gchar* const* _gwy_fit_func_estimators(guint *n) G_GNUC_PURE;

G_GNUC_INTERNAL
GwyExpr* _gwy_fit_func_new_expr_with_constants(void) G_GNUC_MALLOC;

G_GNUC_INTERNAL
void _gwy_fit_func_setup_builtins(BuiltinFitFuncTable *table);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
