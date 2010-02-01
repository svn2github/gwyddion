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

/*< private_header >*/

#ifndef __LIBGWY_FIT_FUNC_BUILTIN_H__
#define __LIBGWY_FIT_FUNC_BUILTIN_H__

G_BEGIN_DECLS

typedef struct {
    const gchar *name;
    gint power_x;
    gint power_y;
} FitFuncParam;

typedef gboolean (*BuiltinFunction)(gdouble x,
                                    const gdouble *param,
                                    gdouble *value);
typedef gboolean (*BuiltinEstimate)(const GwyXY *points,
                                    guint npoints,
                                    gdouble *param);
typedef GwyUnit* (*BuiltinDeriveUnits)(guint param,
                                       GwyUnit *unit_x,
                                       GwyUnit *unit_y);

typedef struct {
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
GHashTable* _gwy_fit_func_setup_builtins(void) G_GNUC_MALLOC;

G_GNUC_INTERNAL
const gchar* _gwy_fit_func_check_estimators(GwyExpr *expr);

G_GNUC_INTERNAL
GwyExpr* _gwy_fit_func_new_expr_with_constants(void) G_GNUC_MALLOC;

G_END_DECLS

#endif /*__GWY_FIT_FUNC_BUILTIN_H__*/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
