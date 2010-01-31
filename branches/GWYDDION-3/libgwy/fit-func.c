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
#include "libgwy/strfuncs.h"
#include "libgwy/expr.h"
#include "libgwy/fit-func.h"
#include "libgwy/libgwy-aliases.h"
#include "libgwy/object-internal.h"
#include "libgwy/fit-func-builtin.h"

enum {
    PROP_0,
    PROP_NAME,
    N_PROPS
};

// NB: The order must match estimators[]
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
};

struct _GwyFitFuncPrivate {
    GwyFitTask *fittask;
    gchar *name;
    const GwyXY *points;
    guint npoints;
    gboolean is_builtin : 1;
    gboolean has_data : 1;

    // Exactly one of builtin/user is set
    const BuiltinFitFunc *builtin;

    GwyUserFitFunc *user;
    guint nparams;  // Cached namely for user-defined funcs, but set for both.
    guint *indices;
    GwyExpr *expr;
};

typedef struct _GwyFitFuncPrivate FitFunc;

static void gwy_fit_func_constructed (GObject *object);
static void gwy_fit_func_dispose     (GObject *object);
static void gwy_fit_func_finalize    (GObject *object);
static void gwy_fit_func_set_property(GObject *object,
                                      guint prop_id,
                                      const GValue *value,
                                      GParamSpec *pspec);
static void gwy_fit_func_get_property(GObject *object,
                                      guint prop_id,
                                      GValue *value,
                                      GParamSpec *pspec);

G_DEFINE_TYPE(GwyFitFunc, gwy_fit_func, G_TYPE_OBJECT)

// NB: The order is given by ESTIMATOR_FOO enum values
static const gchar* const estimators[] = {
    "xmin", "xmid", "xmax",
    "ymin", "ymax", "ymean",
    "yxmin", "yxmid", "yxmax",
    "xymax", "xymin",
};

static GHashTable *builtin_functions = NULL;

static void
gwy_fit_func_class_init(GwyFitFuncClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    g_type_class_add_private(klass, sizeof(FitFunc));

    gobject_class->constructed = gwy_fit_func_constructed;
    gobject_class->dispose = gwy_fit_func_dispose;
    gobject_class->finalize = gwy_fit_func_finalize;
    gobject_class->get_property = gwy_fit_func_get_property;
    gobject_class->set_property = gwy_fit_func_set_property;

    g_object_class_install_property
        (gobject_class,
         PROP_NAME,
         g_param_spec_string("name",
                             "Name",
                             "Function name, either built-in or user.",
                             "Constant",
                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY
                             | STATICP));

    builtin_functions = _gwy_fit_func_setup_builtins();
}

static void
gwy_fit_func_init(GwyFitFunc *fitfunc)
{
    fitfunc->priv = G_TYPE_INSTANCE_GET_PRIVATE(fitfunc,
                                                GWY_TYPE_FIT_FUNC,
                                                FitFunc);
}

static void
gwy_fit_func_constructed(GObject *object)
{
    GwyFitFunc *fitfunc = GWY_FIT_FUNC(object);
    FitFunc *priv = fitfunc->priv;

    const BuiltinFitFunc *builtin;
    builtin = g_hash_table_lookup(builtin_functions, priv->name);
    if (builtin) {
        priv->is_builtin = TRUE;
        priv->builtin = builtin;
        priv->nparams = builtin->nparams;
    }
    // TODO: User functions
    G_OBJECT_CLASS(gwy_fit_func_parent_class)->constructed(object);
}

static void
gwy_fit_func_dispose(GObject *object)
{
    GwyFitFunc *fitfunc = GWY_FIT_FUNC(object);
    FitFunc *priv = fitfunc->priv;
    GWY_OBJECT_UNREF(priv->fittask);
    GWY_OBJECT_UNREF(priv->user);
    GWY_OBJECT_UNREF(priv->expr);
    G_OBJECT_CLASS(gwy_fit_func_parent_class)->dispose(object);
}

static void
gwy_fit_func_finalize(GObject *object)
{
    GwyFitFunc *fitfunc = GWY_FIT_FUNC(object);
    FitFunc *priv = fitfunc->priv;
    GWY_FREE(priv->name);
    GWY_FREE(priv->indices);
    G_OBJECT_CLASS(gwy_fit_func_parent_class)->finalize(object);
}

static void
gwy_fit_func_set_property(GObject *object,
                          guint prop_id,
                          const GValue *value,
                          GParamSpec *pspec)
{
    GwyFitFunc *fitfunc = GWY_FIT_FUNC(object);
    FitFunc *priv = fitfunc->priv;

    switch (prop_id) {
        case PROP_NAME:
        priv->name = g_value_dup_string(value);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_fit_func_get_property(GObject *object,
                          guint prop_id,
                          GValue *value,
                          GParamSpec *pspec)
{
    GwyFitFunc *fitfunc = GWY_FIT_FUNC(object);
    FitFunc *priv = fitfunc->priv;

    switch (prop_id) {
        case PROP_NAME:
        g_value_set_string(value, priv->name);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

/**
 * gwy_fit_func_new:
 * @name: Function name.  It must correspond to either a builtin function or
 *        user function loaded from #GwyUserFitFunc resources.
 *
 * Creates a new fitting function.
 *
 * Returns: A new fitting function.
 **/
GwyFitFunc*
gwy_fit_func_new(const gchar *name)
{
    return g_object_new(GWY_TYPE_FIT_FUNC, "name", name, NULL);
}

/**
 * gwy_fit_func_get_value:
 * @fitfunc: A fitting function.
 * @x: Abscissa value to calculate the function value in.
 * @params: Array of length @nparams holding the parameters.
 * @value: Location to store the value to.
 *
 * Evaluates a fitting function in a single point.
 *
 * Returns: %TRUE if the evaluation was successful, %FALSE on error (typically
 *          a domain error).
 **/
gboolean
gwy_fit_func_get_value(GwyFitFunc *fitfunc,
                       gdouble x,
                       const gdouble *params,
                       gdouble *value)
{
    g_return_val_if_fail(GWY_IS_FIT_FUNC(fitfunc), 0.0);
    FitFunc *priv = fitfunc->priv;
    if (priv->is_builtin)
        return priv->builtin->function(x, params, value);
    g_warning("Non built-in functions are not implemented.");
    return 0.0;
}

/**
 * gwy_fit_func_get_formula:
 * @fitfunc: A fitting function.
 *
 * Obtains the formula of a fitting function.
 *
 * Returns: The formula represented in Pango markup.  The returned string is
 *          owned by @fitfunc.
 **/
const gchar*
gwy_fit_func_get_formula(GwyFitFunc *fitfunc)
{
    g_return_val_if_fail(GWY_IS_FIT_FUNC(fitfunc), NULL);
    FitFunc *priv = fitfunc->priv;
    if (priv->is_builtin)
        return priv->builtin->formula;
    g_warning("Non built-in functions are not implemented.");
    return "";
}

/**
 * gwy_fit_func_get_n_params:
 * @fitfunc: A fitting function.
 *
 * Gets the number of parameters of a fitting function.
 *
 * This information can also be obtained from @fitfunc's fit task.
 * However, this method does not instantiate the fit task which is preferable
 * if you only need to query the functions properties.
 *
 * Returns: The number of function parameters.
 **/
guint
gwy_fit_func_get_n_params(GwyFitFunc *fitfunc)
{
    g_return_val_if_fail(GWY_IS_FIT_FUNC(fitfunc), 0);
    FitFunc *priv = fitfunc->priv;
    if (priv->is_builtin)
        return priv->builtin->nparams;
    g_warning("Non built-in functions are not implemented.");
    return 0;
}

/**
 * gwy_fit_func_get_param_name:
 * @fitfunc: A fitting function.
 * @i: Parameter number.
 *
 * Obtains the name of a fitting function parameter.
 *
 * Returns: The name of @i-th parameter.  The returned string is owned by
 *          @fitfunc.
 **/
const gchar*
gwy_fit_func_get_param_name(GwyFitFunc *fitfunc,
                            guint i)
{
    g_return_val_if_fail(GWY_IS_FIT_FUNC(fitfunc), NULL);
    FitFunc *priv = fitfunc->priv;
    if (priv->is_builtin) {
        const BuiltinFitFunc *builtin = priv->builtin;
        g_return_val_if_fail(i < builtin->nparams, NULL);
        return builtin->param[i].name;
    }
    g_warning("Non built-in functions are not implemented.");
    return "";
}

/**
 * gwy_fit_func_get_param_units:
 * @fitfunc: A fitting function.
 * @i: Parameter number.
 * @unit_x: Unit of abscissa.
 * @unit_y: Unit of ordinate.
 *
 * Derives the unit of a fitting parameter from the units of the fitted data.
 *
 * The derivation is possible only if the parameters are chosen sensibly, i.e.
 * if their units are integer powers of abscissa and ordinate units.  In some
 * cases this requires certain forethought.
 *
 * For instance the units of parameter @B in function
 * @A+@B*@x<superscript>@C</superscript> depend on parameter @C, moreover, they
 * can be any irrational power of @x's units.  If it does not create
 * numerical difficulties it is better to parametrise the function as
 * @a*(1+(@x/@b)<superscript>@c</superscript>), where @a=@A,
 * @b=(@A/@B)<superscript>1/@c</superscript>, and @c=@C.  It can be easily seen
 * that all three new parameters @a, @b, @c have units that are fixed integer
 * powers of the abscissa and ordinate units, namely (0, 1), (1, 0) and (0, 0).
 *
 * Returns: A newly created @GwyUnit with the units of the @i-th parameter.
 **/
GwyUnit*
gwy_fit_func_get_param_units(GwyFitFunc *fitfunc,
                             guint i,
                             GwyUnit *unit_x,
                             GwyUnit *unit_y)
{
    g_return_val_if_fail(GWY_IS_FIT_FUNC(fitfunc), NULL);
    FitFunc *priv = fitfunc->priv;
    if (priv->is_builtin) {
        const BuiltinFitFunc *builtin = priv->builtin;
        g_return_val_if_fail(i < builtin->nparams, NULL);
        if (builtin->derive_units)
            return builtin->derive_units(i, unit_x, unit_y);
        else
            return gwy_unit_power_multiply(NULL,
                                           unit_x, builtin->param[i].power_x,
                                           unit_y, builtin->param[i].power_y);
    }
    g_warning("Non built-in functions are not implemented.");
    return gwy_unit_new();
}

/**
 * gwy_fit_func_estimate:
 * @fitfunc: A fitting function.
 * @params: Array to fill with estimated parameter values.
 *
 * Estimates parameter values of a fitting function.
 *
 * The estimate is based on the data previously set with
 * gwy_fit_func_set_data().
 *
 * The initial estimate method depends on the function used.  There is no
 * absolute guarantee of quality, however if the data points approximately
 * match the fitted function the fit will typically converge from the returned
 * estimate.
 *
 * The parameters are filled also on failure, though just with some neutral
 * values that should not give raise to NaNs and infinities.
 **/
gboolean
gwy_fit_func_estimate(GwyFitFunc *fitfunc,
                      gdouble *params)
{
    g_return_val_if_fail(GWY_IS_FIT_FUNC(fitfunc), FALSE);
    g_return_val_if_fail(params, FALSE);
    FitFunc *priv = fitfunc->priv;
    g_return_val_if_fail(priv->has_data, FALSE);
    // TODO
    return FALSE;
}

static void
construct_expr(FitFunc *priv)
{
    priv->expr = gwy_expr_new();
    if (!gwy_expr_compile(priv->expr,
                          gwy_user_fit_func_get_expression(priv->user),
                          NULL)) {
        g_critical("Cannot compile user fitting function expression.");
        return;
    }

    guint n;
    gwy_user_fit_func_get_params(priv->user, &n);
    priv->indices = g_new(guint, n+1);
    if (gwy_user_fit_func_resolve_params(priv->user, priv->expr, "x",
                                         priv->indices)) {
        g_critical("Cannot resolve variables in user fitting function "
                   "expression.");
        return;
    }
}

static gboolean
fit_func_vfunc(guint i,
               gpointer user_data,
               gdouble *retval,
               const gdouble *params)
{
    FitFunc *priv = ((GwyFitFunc*)user_data)->priv;
    g_return_val_if_fail(i < priv->npoints, FALSE);
    gdouble x = priv->points[i].x, y = priv->points[i].y;

    if (priv->is_builtin) {
        const BuiltinFitFunc *builtin = priv->builtin;
        gdouble v;
        gboolean ok = builtin->function(x, params, &v);
        *retval = v - y;
        return ok;
    }

    if (!priv->expr)
        construct_expr(priv);

    gdouble variables[priv->nparams+2];
    for (guint j = 0; j < priv->nparams; j++)
        variables[priv->indices[j]] = params[j];
    variables[priv->indices[priv->nparams]] = x;
    *retval = gwy_expr_execute(priv->expr, variables) - y;
    return TRUE;
}

static void
update_fit_task(GwyFitFunc *fitfunc)
{
    FitFunc *priv = fitfunc->priv;
    if (!priv->has_data) {
        if (priv->fittask)
            gwy_fit_task_set_vector_data(priv->fittask, fitfunc, 0);
        return;
    }

    if (!priv->fittask) {
        guint nparams = gwy_fit_func_get_n_params(fitfunc);
        priv->fittask = gwy_fit_task_new();
        gwy_fit_task_set_vector_vfunction(priv->fittask, nparams,
                                          fit_func_vfunc, NULL);
    }
    gwy_fit_task_set_vector_data(priv->fittask, fitfunc, priv->npoints);
}

/**
 * gwy_fit_func_get_fit_task:
 * @fitfunc: A fitting function.
 *
 * Obtains the fit task of a fitting function.
 *
 * You can use the parameter, error, residuum, correlation, etc. methods of
 * the returned fit task freely.  However, the function and data is set by
 * @fitfunc and must not be changed.
 *
 * Returns: The fit task object of a fitting function, it can be %NULL if
 *          gwy_fit_func_set_data() has not been called yet.  The returned
 *          object is owned by @fitfunc and no reference is added.
 **/
GwyFitTask*
gwy_fit_func_get_fit_task(GwyFitFunc *fitfunc)
{
    g_return_val_if_fail(GWY_IS_FIT_FUNC(fitfunc), NULL);
    FitFunc *priv = fitfunc->priv;
    if (priv->has_data)
        update_fit_task(fitfunc);
    return priv->fittask;
}

/**
 * gwy_fit_func_set_data:
 * @fitfunc: A fitting function.
 * @points: Point data, x-values are abscissas y-values are the data to fit.
 *          The data must exist during the lifetime of @fitfunc (or until
 *          another data is set) as @fitfunc does not make a copy.
 * @npoints: Number of data points.
 *
 * Sets the data to fit.
 **/
void
gwy_fit_func_set_data(GwyFitFunc *fitfunc,
                      const GwyXY *points,
                      guint npoints)
{
    g_return_if_fail(GWY_IS_FIT_FUNC(fitfunc));
    g_return_if_fail(points || !npoints);
    FitFunc *priv = fitfunc->priv;
    priv->points = points;
    priv->npoints = npoints;
    priv->has_data = npoints > 0;
    if (priv->fittask)
        update_fit_task(fitfunc);
}

/**
 * gwy_fit_func_get_user:
 * @fitfunc: A fitting function.
 *
 * Obtains the user-defined fitting function resource of a fitting function.
 *
 * This method can be called both with built-in and user-defined fitting
 * functions, in fact, is can be used to determine if a function is built-in.
 *
 * Returns: The user fitting function resource corresponding to @fitfunc
 *          (no reference added), or %NULL if the function is built-in.
 **/
GwyUserFitFunc*
gwy_fit_func_get_user(GwyFitFunc *fitfunc)
{
    g_return_val_if_fail(GWY_IS_FIT_FUNC(fitfunc), NULL);
    FitFunc *priv = fitfunc->priv;
    return priv->is_builtin ? NULL : priv->user;
}

/**
 * _gwy_fit_func_check_estimators:
 * @expr: Compiled estimation expression.
 *
 * Check if all variables in an expression correspond to known estimators.
 *
 * Returns: %NULL on success, the name of the first unknown variable on
 *          failure, empty string on total failure (e.g. an uncompiled
 *          expression was passed).
 **/
const gchar*
_gwy_fit_func_check_estimators(GwyExpr *expr)
{
    const gchar **names;
    guint n = gwy_expr_get_variables(expr, &names);

    if (!n)
        return "";

    for (guint i = 1; i < n; i++) {
        guint j;
        for (j = 0; j < G_N_ELEMENTS(estimators); j++) {
            if (gwy_strequal(names[i], estimators[j]))
                break;
        }
        if (j == G_N_ELEMENTS(estimators))
            return names[i];
    }
    return NULL;
}

#define __LIBGWY_FIT_FUNC_C__
#include "libgwy/libgwy-aliases.c"

/************************** Documentation ****************************/

/**
 * SECTION: fit-func
 * @title: GwyFitFunc
 * @short_description: Fitting function
 *
 * #GwyFitFunc represents a named fitting function with formula, named
 * parameters, capability to estimate parameters values or derive their units
 * from the units of fitted data.
 **/

/**
 * GwyFitFunc:
 *
 * Object represnting a fitting function.
 *
 * The #GwyFitFunc struct contains private data only and should be accessed
 * using the functions below.
 **/

/**
 * GwyFitFuncClass:
 *
 * Class of fitting functions.
 *
 * #GwyFitFuncClass does not contain any public members.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
