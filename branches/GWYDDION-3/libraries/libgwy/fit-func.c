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

#include <string.h>
#include "libgwy/macros.h"
#include "libgwy/strfuncs.h"
#include "libgwy/expr.h"
#include "libgwy/fit-func.h"
#include "libgwy/object-internal.h"
#include "libgwy/fit-func-builtin.h"

enum {
    PROP_0,
    PROP_NAME,
    PROP_GROUP,
    PROP_FIT_TASK,
    PROP_RESOURCE,
    N_PROPS
};

struct _GwyFitFuncPrivate {
    gchar *name;
    GwyFitTask *fittask;
    const GwyXY *points;
    guint npoints;

    gboolean is_valid;  // Set to %TRUE if the function actually exists.

    // Exactly one of builtin/resource is set
    const BuiltinFitFunc *builtin;
    GwyUserFitFunc *resource;

    // User functions only
    guint *indices;
    GwyExpr *expr;
    GwyExpr *estimate;
    gulong data_changed_id;
    gulong notify_name_id;
};

typedef struct _GwyFitFuncPrivate FitFunc;

static void     gwy_fit_func_constructed (GObject *object);
static void     gwy_fit_func_dispose     (GObject *object);
static void     gwy_fit_func_finalize    (GObject *object);
static void     gwy_fit_func_set_property(GObject *object,
                                          guint prop_id,
                                          const GValue *value,
                                          GParamSpec *pspec);
static void     gwy_fit_func_get_property(GObject *object,
                                          guint prop_id,
                                          GValue *value,
                                          GParamSpec *pspec);
static void     evaluate_estimators      (FitFunc *priv,
                                          gdouble *estim);
static void     user_func_data_changed   (GwyFitFunc *fitfunc,
                                          GwyUserFitFunc *userfitfunc);
static void     user_func_notify_name    (GwyFitFunc *fitfunc,
                                          GParamSpec *pspec,
                                          GwyUserFitFunc *userfitfunc);
static gboolean evaluate                 (GwyFitFunc *fitfunc,
                                          gdouble x,
                                          const gdouble *params,
                                          gdouble *retval);
static guint        get_nparams          (const GwyFitFunc *fitfunc);
static const gchar* get_group            (const GwyFitFunc *fitfunc);

static GParamSpec *properties[N_PROPS];

G_DEFINE_TYPE(GwyFitFunc, gwy_fit_func, G_TYPE_OBJECT);

// The order is given by ESTIMATOR_FOO enum values
static const gchar* const estimators[N_ESTIMATORS] = {
    "xmin", "xmid", "xmax",
    "ymin", "ymax", "ymean",
    "yxmin", "yxmid", "yxmax",
    "xymax", "xymin",
    "xpeak", "apeak", "hwpeak", "y0peak",
    "integr",
};

static GObjectClass *parent_class = NULL;
static BuiltinFitFuncTable builtin_table = { NULL, 0, NULL };

static void
gwy_fit_func_class_init(GwyFitFuncClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    parent_class = G_OBJECT_CLASS(gwy_fit_func_parent_class);

    g_type_class_add_private(klass, sizeof(FitFunc));

    gobject_class->constructed = gwy_fit_func_constructed;
    gobject_class->dispose = gwy_fit_func_dispose;
    gobject_class->finalize = gwy_fit_func_finalize;
    gobject_class->get_property = gwy_fit_func_get_property;
    gobject_class->set_property = gwy_fit_func_set_property;

    properties[PROP_GROUP]
        = g_param_spec_string("group",
                              "Group",
                              "Group the function belongs to.",
                              "builtin",
                              G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    properties[PROP_NAME]
        = g_param_spec_string("name",
                              "Name",
                              "Function name, either built-in or user.",
                              "Constant",
                              G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY
                              | G_PARAM_STATIC_STRINGS);

    properties[PROP_FIT_TASK]
        = g_param_spec_object("fit-task",
                              "Fit Task",
                              "GwyFitTask instance used by this function. "
                              "May be NULL if no fitting has been done yet.",
                              GWY_TYPE_FIT_TASK,
                              G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    properties[PROP_RESOURCE]
        = g_param_spec_object("resource",
                              "Resource",
                              "GwyUserFitFunc resource wrapped by this "
                              "function if any.",
                              GWY_TYPE_USER_FIT_FUNC,
                              G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    for (guint i = 1; i < N_PROPS; i++)
        g_object_class_install_property(gobject_class, i, properties[i]);

    _gwy_fit_func_setup_builtins(&builtin_table);
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

    priv->builtin = g_hash_table_lookup(builtin_table.table, priv->name);
    if (priv->builtin)
        priv->is_valid = TRUE;
    else {
        priv->resource = gwy_user_fit_funcs_get(priv->name);
        if (priv->resource) {
            g_object_ref(priv->resource);
            priv->data_changed_id
                = g_signal_connect_swapped(priv->resource, "data-changed",
                                           G_CALLBACK(user_func_data_changed),
                                           fitfunc);
            priv->notify_name_id
                = g_signal_connect_swapped(priv->resource, "notify::name",
                                           G_CALLBACK(user_func_notify_name),
                                           fitfunc);
            priv->is_valid = TRUE;
        }
    }
    if (parent_class->constructed)
        parent_class->constructed(object);
}

static void
gwy_fit_func_dispose(GObject *object)
{
    GwyFitFunc *fitfunc = GWY_FIT_FUNC(object);
    FitFunc *priv = fitfunc->priv;
    GWY_SIGNAL_HANDLER_DISCONNECT(priv->resource, priv->data_changed_id);
    GWY_SIGNAL_HANDLER_DISCONNECT(priv->resource, priv->notify_name_id);
    GWY_OBJECT_UNREF(priv->fittask);
    GWY_OBJECT_UNREF(priv->resource);
    GWY_OBJECT_UNREF(priv->expr);
    GWY_OBJECT_UNREF(priv->estimate);
    parent_class->dispose(object);
}

static void
gwy_fit_func_finalize(GObject *object)
{
    GwyFitFunc *fitfunc = GWY_FIT_FUNC(object);
    FitFunc *priv = fitfunc->priv;
    GWY_FREE(priv->name);
    GWY_FREE(priv->indices);
    parent_class->finalize(object);
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

        case PROP_GROUP:
        g_value_set_string(value, get_group(fitfunc));
        break;

        case PROP_FIT_TASK:
        g_value_set_object(value, gwy_fit_func_get_fit_task(fitfunc));
        break;

        case PROP_RESOURCE:
        g_value_set_object(value, priv->resource);
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
 * Returns: (transfer full) (allow-none):
 *          A new fitting function.  It can return %NULL if no function called
 *          @name exists (this is not considered to be an error).
 **/
GwyFitFunc*
gwy_fit_func_new(const gchar *name)
{
    GwyFitFunc *fitfunc = g_object_new(GWY_TYPE_FIT_FUNC,
                                       "name", name,
                                       NULL);
    if (fitfunc->priv->is_valid)
        return fitfunc;

    g_object_unref(fitfunc);
    return NULL;
}

/**
 * gwy_fit_func_evaluate:
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
gwy_fit_func_evaluate(GwyFitFunc *fitfunc,
                      gdouble x,
                      const gdouble *params,
                      gdouble *value)
{
    g_return_val_if_fail(GWY_IS_FIT_FUNC(fitfunc), FALSE);
    FitFunc *priv = fitfunc->priv;
    g_return_val_if_fail(priv->is_valid, FALSE);
    return evaluate(fitfunc, x, params, value);
}

/**
 * gwy_fit_func_formula:
 * @fitfunc: A fitting function.
 *
 * Obtains the formula of a fitting function.
 *
 * Returns: The formula represented in Pango markup.  The returned string is
 *          owned by @fitfunc.
 **/
const gchar*
gwy_fit_func_formula(const GwyFitFunc *fitfunc)
{
    g_return_val_if_fail(GWY_IS_FIT_FUNC(fitfunc), NULL);
    FitFunc *priv = fitfunc->priv;
    g_return_val_if_fail(priv->is_valid, NULL);
    if (priv->builtin)
        return priv->builtin->formula;
    else
        return gwy_user_fit_func_get_formula(priv->resource);
}

/**
 * gwy_fit_func_get_name:
 * @fitfunc: A fitting function.
 *
 * Obtains the name of a fitting function.
 *
 * This is the same name as was used in gwy_fit_func_new().  Except if it is
 * a user fitting function that has been renamed meanwhile.
 *
 * Returns: The function name. The returned string is owned by @fitfunc.
 **/
const gchar*
gwy_fit_func_get_name(const GwyFitFunc *fitfunc)
{
    g_return_val_if_fail(GWY_IS_FIT_FUNC(fitfunc), NULL);
    FitFunc *priv = fitfunc->priv;
    g_return_val_if_fail(priv->is_valid, NULL);
    return priv->name;
}

/**
 * gwy_fit_func_get_group:
 * @fitfunc: A fitting function.
 *
 * Obtains the group of a fitting function.
 *
 * This is always the same group as was used in gwy_fit_func_new().
 *
 * Returns: The function group. The returned string is owned by @fitfunc.
 **/
const gchar*
gwy_fit_func_get_group(const GwyFitFunc *fitfunc)
{
    g_return_val_if_fail(GWY_IS_FIT_FUNC(fitfunc), NULL);
    return get_group(fitfunc);
}

/**
 * gwy_fit_func_n_params:
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
gwy_fit_func_n_params(const GwyFitFunc *fitfunc)
{
    g_return_val_if_fail(GWY_IS_FIT_FUNC(fitfunc), 0);
    return get_nparams(fitfunc);
}

/**
 * gwy_fit_func_param_name:
 * @fitfunc: A fitting function.
 * @i: Parameter number.
 *
 * Obtains the name of a fitting function parameter.
 *
 * Returns: The name of @i-th parameter.  The returned string is owned by
 *          @fitfunc (or possibly its #GwyUserFitFunc).
 **/
const gchar*
gwy_fit_func_param_name(const GwyFitFunc *fitfunc,
                        guint i)
{
    g_return_val_if_fail(GWY_IS_FIT_FUNC(fitfunc), NULL);
    FitFunc *priv = fitfunc->priv;
    g_return_val_if_fail(priv->is_valid, NULL);
    g_return_val_if_fail(i < get_nparams(fitfunc), NULL);

    if (priv->builtin) {
        const BuiltinFitFunc *builtin = priv->builtin;
        return builtin->param[i].name;
    }
    const GwyFitParam *p = gwy_user_fit_func_nth_param(priv->resource, i);
    return gwy_fit_param_get_name(p);
}

/**
 * gwy_fit_func_param_number:
 * @fitfunc: A fitting function.
 * @name: Parameter name.
 *
 * Finds a fitting function parameter by name.
 *
 * Returns: The parameter number.  If the paramter is no found a number larger
 *          than the number of parameters is returned, specificallly
 *          %G_MAXUINT.
 **/
guint
gwy_fit_func_param_number(const GwyFitFunc *fitfunc,
                          const gchar *name)
{
    g_return_val_if_fail(GWY_IS_FIT_FUNC(fitfunc), G_MAXUINT);
    g_return_val_if_fail(name, G_MAXUINT);
    FitFunc *priv = fitfunc->priv;
    g_return_val_if_fail(priv->is_valid, G_MAXUINT);

    guint nparams = get_nparams(fitfunc);
    if (priv->builtin) {
        const BuiltinFitFunc *builtin = priv->builtin;
        for (guint i = 0; i < nparams; i++) {
            if (gwy_strequal(builtin->param[i].name, name))
                return i;
        }
    }
    else {
        for (guint i = 0; i < nparams; i++) {
            const GwyFitParam *p = gwy_user_fit_func_nth_param(priv->resource,
                                                               i);
            if (gwy_strequal(gwy_fit_param_get_name(p), name))
                return i;
        }
    }
    return G_MAXUINT;
}

/**
 * gwy_fit_func_param_unit:
 * @fitfunc: A fitting function.
 * @i: Parameter number.
 * @xunit: Unit of abscissa.
 * @yunit: Unit of ordinate.
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
 * Returns: (transfer full):
 *          A newly created @GwyUnit with the units of the @i-th parameter.
 **/
GwyUnit*
gwy_fit_func_param_unit(GwyFitFunc *fitfunc,
                        guint i,
                        const GwyUnit *xunit,
                        const GwyUnit *yunit)
{
    g_return_val_if_fail(GWY_IS_FIT_FUNC(fitfunc), NULL);
    FitFunc *priv = fitfunc->priv;
    g_return_val_if_fail(priv->is_valid, NULL);
    g_return_val_if_fail(i < get_nparams(fitfunc), NULL);

    gint power_x, power_y;
    if (priv->builtin) {
        const BuiltinFitFunc *builtin = priv->builtin;
        if (builtin->derive_units)
            return builtin->derive_units(i, xunit, yunit);
        power_x = builtin->param[i].power_x;
        power_y = builtin->param[i].power_y;
    }
    else {
        const GwyFitParam *p = gwy_user_fit_func_nth_param(priv->resource, i);
        g_assert(p);
        power_x = gwy_fit_param_get_power_x(p);
        power_y = gwy_fit_param_get_power_y(p);
    }
    GwyUnit *unit = gwy_unit_new();
    gwy_unit_power_multiply(unit, xunit, power_x, yunit, power_y);
    return unit;
}

/**
 * gwy_fit_func_estimate:
 * @fitfunc: A fitting function.
 * @params: Array to fill with estimated parameter values.
 *
 * Estimates parameter values of a fitting function.
 *
 * The estimate is based on the data previously set with
 * gwy_fit_func_set_data().  The estimates are set as parameter values of the
 * underlying #GwyFitter.
 *
 * The initial estimate method depends on the function used.  There is no
 * absolute guarantee of quality.  However, if the data points approximately
 * match the fitted function the fit will typically converge from the returned
 * estimate.
 *
 * The parameters are filled also on failure to produce a reasonable estimate
 * for given data, though just with some neutral values that should not give
 * raise to NaNs and infinities.
 *
 * Returns: %TRUE if the estimate was succesful, i.e. the estimator thinks the
 *          data do not seem completely incompatible with this function and
 *          the estimated parameters should be reasonable.
 **/
gboolean
gwy_fit_func_estimate(GwyFitFunc *fitfunc,
                      gdouble *params)
{
    g_return_val_if_fail(GWY_IS_FIT_FUNC(fitfunc), FALSE);
    g_return_val_if_fail(params, FALSE);
    FitFunc *priv = fitfunc->priv;
    g_return_val_if_fail(priv->is_valid, FALSE);
    guint nparams = get_nparams(fitfunc);
    gwy_clear(params, nparams);
    g_return_val_if_fail(priv->npoints, FALSE);
    GwyFitTask *fittask = gwy_fit_func_get_fit_task(fitfunc);
    GwyFitter *fitter = gwy_fit_task_get_fitter(fittask);
    gdouble estim[N_ESTIMATORS];
    evaluate_estimators(priv, estim);

    if (priv->builtin) {
        const BuiltinFitFunc *builtin = priv->builtin;
        g_return_val_if_fail(builtin->estimate, FALSE);
        gboolean ok = builtin->estimate(priv->points, priv->npoints, estim,
                                        params);
        gwy_fitter_set_params(fitter, params);
        return ok;
    }

    if (!priv->estimate)
        priv->estimate = _gwy_fit_func_new_expr_with_constants();
    for (guint i = 0; i < N_ESTIMATORS; i++) {
        if (!gwy_expr_define_constant(priv->estimate, estimators[i], estim[i],
                                      NULL))
            g_critical("Cannot define %s as a GwyExpr constant.", estimators[i]);
    }
    for (guint i = 0; i < nparams; i++) {
        const GwyFitParam *p = gwy_user_fit_func_nth_param(priv->resource, i);
        const gchar *estimate = gwy_fit_param_get_estimate(p);
        if (estimate
            && !gwy_expr_evaluate(priv->estimate, estimate, params + i, NULL))
            g_critical("Parameter %u estimator does not compile.", i);
    }
    gwy_fitter_set_params(fitter, params);
    return TRUE;
}

static void
evaluate_estimators(FitFunc *priv, gdouble *estim)
{
    const GwyXY *pts = priv->points;
    guint n = priv->npoints;

    estim[ESTIMATOR_XMIN]
        = estim[ESTIMATOR_XMAX]
        = estim[ESTIMATOR_XMID]
        = estim[ESTIMATOR_XYMIN]
        = estim[ESTIMATOR_XYMAX]
        = estim[ESTIMATOR_XPEAK]
        = pts[0].x;

    estim[ESTIMATOR_HWPEAK]
        = estim[ESTIMATOR_INTEGR]
        = 0.0;

    estim[ESTIMATOR_YMIN]
        = estim[ESTIMATOR_YMAX]
        = estim[ESTIMATOR_YMEAN]
        = estim[ESTIMATOR_YXMIN]
        = estim[ESTIMATOR_YXMID]
        = estim[ESTIMATOR_YXMAX]
        = estim[ESTIMATOR_APEAK]
        = estim[ESTIMATOR_Y0PEAK]
        = pts[0].y;

    for (guint i = 1; i < n; i++) {
        gdouble x = pts[i].x, y = pts[i].y;
        if (x < estim[ESTIMATOR_XMIN]) {
            estim[ESTIMATOR_XMIN] = x;
            estim[ESTIMATOR_YXMIN] = y;
        }
        if (x > estim[ESTIMATOR_XMAX]) {
            estim[ESTIMATOR_XMAX] = x;
            estim[ESTIMATOR_YXMAX] = y;
        }
        if (y < estim[ESTIMATOR_YMIN]) {
            estim[ESTIMATOR_YMIN] = y;
            estim[ESTIMATOR_XYMIN] = x;
        }
        if (y > estim[ESTIMATOR_YMAX]) {
            estim[ESTIMATOR_YMAX] = y;
            estim[ESTIMATOR_XYMAX] = x;
        }
        estim[ESTIMATOR_YMEAN] += y;
    }
    estim[ESTIMATOR_YMEAN] /= n;

    // Integral.
    if (n >= 2) {
        estim[ESTIMATOR_INTEGR] += (pts[1].x - pts[0].x)*pts[0].y;
        for (guint i = 1; i+1 < n; i++)
            estim[ESTIMATOR_INTEGR] += (pts[i+1].x - pts[i-1].x)*pts[i].y;
        estim[ESTIMATOR_INTEGR] += (pts[n-1].x - pts[n-2].x)*pts[n-1].y;
        estim[ESTIMATOR_INTEGR] *= 0.5;
    }

    // The middle point
    gdouble xmid = (estim[ESTIMATOR_XMIN] + estim[ESTIMATOR_XMAX])/2;
    gdouble mindist = fabs(estim[ESTIMATOR_XMID] - xmid);
    for (guint i = 1; i < n; i++) {
        gdouble x = pts[i].x, y = pts[i].y;
        if (fabs(x - xmid) < mindist) {
            estim[ESTIMATOR_XMID] = x;
            estim[ESTIMATOR_YXMID] = y;
            mindist = fabs(x - xmid);
        }
    }

    // Peak.
    gdouble xymax2l = estim[ESTIMATOR_XMIN], xymax2r = estim[ESTIMATOR_XMAX],
            xymin2l = estim[ESTIMATOR_XMIN], xymin2r = estim[ESTIMATOR_XMAX];
    gdouble h = 0.5*(estim[ESTIMATOR_YMIN] + estim[ESTIMATOR_YMAX]);
    for (guint i = 0; i < n; i++) {
        gdouble x = pts[i].x, y = pts[i].y;
        if (y <= h && x < estim[ESTIMATOR_XYMAX] && x > xymax2l)
            xymax2l = x;
        if (y <= h && x > estim[ESTIMATOR_XYMAX] && x < xymax2r)
            xymax2r = x;
        if (y >= h && x < estim[ESTIMATOR_XYMIN] && x > xymin2l)
            xymin2l = x;
        if (y >= h && x > estim[ESTIMATOR_XYMIN] && x < xymin2r)
            xymin2r = x;
    }
    // Choose between upward and downward peak based on which is narrower,
    // preferring upward slightly.
    if (0.6*(xymax2r - xymax2l) <= xymin2r - xymin2l) {
        estim[ESTIMATOR_XPEAK] = estim[ESTIMATOR_XYMAX];
        estim[ESTIMATOR_APEAK] = estim[ESTIMATOR_YMAX] - estim[ESTIMATOR_YMIN];
        estim[ESTIMATOR_HWPEAK] = 0.5*(xymax2r - xymax2l);
        estim[ESTIMATOR_Y0PEAK] = estim[ESTIMATOR_YMIN];
    }
    else {
        estim[ESTIMATOR_XPEAK] = estim[ESTIMATOR_XYMIN];
        estim[ESTIMATOR_APEAK] = estim[ESTIMATOR_YMIN] - estim[ESTIMATOR_YMAX];
        estim[ESTIMATOR_HWPEAK] = 0.5*(xymin2r - xymin2l);
        estim[ESTIMATOR_Y0PEAK] = estim[ESTIMATOR_YMAX];
    }
}

static void
user_func_data_changed(GwyFitFunc *fitfunc,
                       G_GNUC_UNUSED GwyUserFitFunc *userfitfunc)
{
    // Just invalidate stuff, construct_expr() will create it again if
    // necessary.
    FitFunc *priv = fitfunc->priv;
    GWY_FREE(priv->indices);
    GWY_OBJECT_UNREF(priv->expr);
}

// This does not feel right, or at least not useful.  But if the name changes
// we should emit notify::name so just do it.
static void
user_func_notify_name(GwyFitFunc *fitfunc,
                      G_GNUC_UNUSED GParamSpec *pspec,
                      GwyUserFitFunc *userfitfunc)
{
    FitFunc *priv = fitfunc->priv;
    GwyResource *resource = GWY_RESOURCE(userfitfunc);
    const gchar *name = gwy_resource_get_name(resource);
    if (!gwy_strequal(name, priv->name)) {
        g_free(priv->name);
        priv->name = g_strdup(name);
        g_object_notify_by_pspec(G_OBJECT(fitfunc), properties[PROP_NAME]);
    }
}

static void
construct_expr(GwyFitFunc *fitfunc)
{
    FitFunc *priv = fitfunc->priv;
    priv->expr = _gwy_fit_func_new_expr_with_constants();
    if (!gwy_expr_compile(priv->expr,
                          gwy_user_fit_func_get_formula(priv->resource),
                          NULL)) {
        g_critical("Cannot compile user fitting function formula.");
        return;
    }

    priv->indices = g_new(guint, get_nparams(fitfunc)+1);
    if (gwy_user_fit_func_resolve_params(priv->resource, priv->expr,
                                         priv->indices)) {
        g_critical("Cannot resolve variables in user fitting function "
                   "formula.");
        return;
    }
}

static gboolean
evaluate(GwyFitFunc *fitfunc,
         gdouble x,
         const gdouble *params,
         gdouble *retval)
{
    FitFunc *priv = fitfunc->priv;
    if (priv->builtin) {
        const BuiltinFitFunc *builtin = priv->builtin;
        return builtin->function(x, params, retval);
    }

    if (G_UNLIKELY(!priv->expr))
        construct_expr(fitfunc);

    guint nparams = get_nparams(fitfunc);
    gdouble variables[nparams+2];
    for (guint j = 0; j < nparams; j++)
        variables[priv->indices[j]] = params[j];
    variables[priv->indices[nparams]] = x;
    *retval = gwy_expr_execute(priv->expr, variables);
    return TRUE;
}

static gboolean
fit_func_vfunc(guint i,
               gpointer user_data,
               gdouble *retval,
               const gdouble *params)
{
    GwyFitFunc *fitfunc = (GwyFitFunc*)user_data;
    FitFunc *priv = fitfunc->priv;
    g_return_val_if_fail(i < priv->npoints, FALSE);
    gdouble x = priv->points[i].x, y = priv->points[i].y;
    gboolean ok = evaluate(fitfunc, x, params, retval);
    *retval -= y;
    return ok;
}

static void
update_fit_task(GwyFitFunc *fitfunc)
{
    FitFunc *priv = fitfunc->priv;
    if (!priv->fittask) {
        priv->fittask = gwy_fit_task_new();
        gwy_fit_task_set_vector_vfuncs(priv->fittask, get_nparams(fitfunc),
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
 * Returns: (transfer none):
 *          The fit task object of a fitting function.  The returned
 *          object is owned by @fitfunc and no reference is added.
 **/
GwyFitTask*
gwy_fit_func_get_fit_task(GwyFitFunc *fitfunc)
{
    g_return_val_if_fail(GWY_IS_FIT_FUNC(fitfunc), NULL);
    FitFunc *priv = fitfunc->priv;
    g_return_val_if_fail(priv->is_valid, NULL);
    if (!priv->fittask)
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
    // FIXME: If we permit changing the function after construction, this is no
    // longer correct.
    g_return_if_fail(priv->is_valid);
    priv->points = points;
    priv->npoints = npoints;
    if (priv->fittask)
        update_fit_task(fitfunc);
}

static const gchar*
get_group(const GwyFitFunc *fitfunc)
{
    FitFunc *priv = fitfunc->priv;
    if (priv->builtin)
        return priv->builtin->group;
    else
        return gwy_user_fit_func_get_group(priv->resource);
}

static guint
get_nparams(const GwyFitFunc *fitfunc)
{
    FitFunc *priv = fitfunc->priv;
    if (priv->builtin)
        return priv->builtin->nparams;
    else
        return gwy_user_fit_func_n_params(priv->resource);
}

/**
 * gwy_fit_func_get_resource:
 * @fitfunc: A fitting function.
 *
 * Obtains the user-defined fitting function resource of a fitting function.
 *
 * This method can be called both with built-in and user-defined fitting
 * functions, in fact, it can be used to determine if a function is built-in.
 *
 * Returns: (transfer none) (allow-none):
 *          The user fitting function resource corresponding to @fitfunc
 *          (no reference added), or %NULL if the function is built-in.
 **/
GwyUserFitFunc*
gwy_fit_func_get_resource(const GwyFitFunc *fitfunc)
{
    g_return_val_if_fail(GWY_IS_FIT_FUNC(fitfunc), NULL);
    FitFunc *priv = fitfunc->priv;
    g_return_val_if_fail(priv->is_valid, NULL);
    return priv->builtin ? NULL : priv->resource;
}

/**
 * gwy_fit_func_is_valid:
 * @fitfunc: A fitting function.
 *
 * Reports whether a fitting function is valid.
 *
 * Since invalid fitting function resources are not loaded at all and the
 * constructor gwy_fit_func_new() returns %NULL if a non-existent fitting
 * function is requested, an invalid fitting function object can only be
 * created directly using g_object_new() with a non-existent fitting function
 * name.  So this method is mainly intended for bindings where such situation
 * can arise more easily than in C.
 *
 * Returns: %TRUE if the fitting function is valid and can be used for fitting,
 *          etc.
 **/
gboolean
gwy_fit_func_is_valid(const GwyFitFunc *fitfunc)
{
    g_return_val_if_fail(GWY_IS_FIT_FUNC(fitfunc), FALSE);
    return fitfunc->priv->is_valid;
}

const gchar* const*
_gwy_fit_func_estimators(guint *n)
{
    GWY_MAYBE_SET(n, N_ESTIMATORS);
    return estimators;
}

GwyExpr*
_gwy_fit_func_new_expr_with_constants(void)
{
    GwyExpr *expr = gwy_expr_new();
    gwy_expr_define_constant(expr, "pi", G_PI, NULL);
    gwy_expr_define_constant(expr, "π", G_PI, NULL);
    return expr;
}

/**
 * gwy_fit_func_list_builtins:
 *
 * Obtains the list of all built-in fitting function names.
 *
 * The list is valid permanently. Neither the list nor its elements may be
 * modified or freed.
 *
 * Returns: (transfer container) (array zero-terminated=1):
 *          A %NULL-terminated array with function names.
 **/
const gchar* const*
gwy_fit_func_list_builtins(void)
{
    gpointer klass = g_type_class_ref(GWY_TYPE_FIT_FUNC);
    const gchar* const *retval = builtin_table.names;
    g_type_class_unref(klass);
    return retval;
}

/************************** Documentation ****************************/

/**
 * SECTION: fit-func
 * @title: GwyFitFunc
 * @short_description: Fitting function
 *
 * #GwyFitFunc represents a named fitting function with formula, named
 * parameters, capability to estimate parameters values or derive their units
 * from the units of fitted data.
 *
 * It can wrap either a built-in fitting function (see
 * <link linkend='libgwy4-builtin-fit-func'>Builtin fit funcs</link>
 * for a list) or user function resources
 * #GwyUserFitFunc.
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
