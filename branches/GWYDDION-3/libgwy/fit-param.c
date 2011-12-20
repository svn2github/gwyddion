/*
 *  $Id$
 *  Copyright (C) 2010 David Nečas (Yeti).
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
#include <glib/gi18n-lib.h>
#include "libgwy/macros.h"
#include "libgwy/math.h"
#include "libgwy/unit.h"
#include "libgwy/strfuncs.h"
#include "libgwy/serialize.h"
#include "libgwy/expr.h"
#include "libgwy/fit-param.h"
#include "libgwy/fit-func-builtin.h"
#include "libgwy/object-internal.h"

enum { POWER_MIN = -12, POWER_MAX = 12 };

enum { N_ITEMS = 4 };

enum {
    PROP_0,
    PROP_NAME,
    PROP_POWER_X,
    PROP_POWER_Y,
    PROP_ESTIMATE,
    N_PROPS
};

struct _GwyFitParamPrivate {
    gchar *name;
    gchar *estimate;
    gint power_x;
    gint power_y;
};

typedef struct _GwyFitParamPrivate FitParam;

static void     gwy_fit_param_finalize         (GObject *object);
static void     gwy_fit_param_set_property     (GObject *object,
                                                guint prop_id,
                                                const GValue *value,
                                                GParamSpec *pspec);
static void     gwy_fit_param_get_property     (GObject *object,
                                                guint prop_id,
                                                GValue *value,
                                                GParamSpec *pspec);
static void     gwy_fit_param_serializable_init(GwySerializableInterface *iface);
static gsize    gwy_fit_param_n_items          (GwySerializable *serializable);
static gsize    gwy_fit_param_itemize          (GwySerializable *serializable,
                                                GwySerializableItems *items);
static gboolean gwy_fit_param_construct        (GwySerializable *serializable,
                                                GwySerializableItems *items,
                                                GwyErrorList **error_list);
static GObject* gwy_fit_param_duplicate_impl   (GwySerializable *serializable);
static void     gwy_fit_param_assign_impl      (GwySerializable *destination,
                                                GwySerializable *source);

static const gunichar more[] = { '_', 0 };

static const GwySerializableItem serialize_items[N_ITEMS] = {
    /*0*/ { .name = "name",     .ctype = GWY_SERIALIZABLE_STRING, },
    /*1*/ { .name = "x-power",  .ctype = GWY_SERIALIZABLE_INT32,  },
    /*2*/ { .name = "y-power",  .ctype = GWY_SERIALIZABLE_INT32,  },
    /*3*/ { .name = "estimate", .ctype = GWY_SERIALIZABLE_STRING, },
};

static GParamSpec *properties[N_PROPS];

G_DEFINE_TYPE_EXTENDED
    (GwyFitParam, gwy_fit_param, G_TYPE_OBJECT, 0,
     GWY_IMPLEMENT_SERIALIZABLE(gwy_fit_param_serializable_init));

static void
gwy_fit_param_serializable_init(GwySerializableInterface *iface)
{
    iface->n_items   = gwy_fit_param_n_items;
    iface->itemize   = gwy_fit_param_itemize;
    iface->construct = gwy_fit_param_construct;
    iface->duplicate = gwy_fit_param_duplicate_impl;
    iface->assign    = gwy_fit_param_assign_impl;
}

static void
gwy_fit_param_class_init(GwyFitParamClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    g_type_class_add_private(klass, sizeof(FitParam));

    gobject_class->finalize = gwy_fit_param_finalize;
    gobject_class->get_property = gwy_fit_param_get_property;
    gobject_class->set_property = gwy_fit_param_set_property;

    properties[PROP_NAME]
        = g_param_spec_string("name",
                              "Name",
                              "Parameter identifier",
                              "a",
                              G_PARAM_CONSTRUCT_ONLY
                              | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_POWER_X]
        = g_param_spec_int("power-x",
                           "Power of X",
                           "Power of the abscissa contained in the parameter.",
                           POWER_MIN, POWER_MAX, 0,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_POWER_Y]
        = g_param_spec_int("power-y",
                           "Power of Y",
                           "Power of the ordinate contained in the parameter.",
                           POWER_MIN, POWER_MAX, 0,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_ESTIMATE]
        = g_param_spec_string("estimate",
                              "Estimate",
                              "Expression used as the initial parameter "
                              "estimate.  It can contain the estimator "
                              "variables.  No estimate expression means the "
                              "initial parameter estimate will be zero.",
                              NULL,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    for (guint i = 1; i < N_PROPS; i++)
        g_object_class_install_property(gobject_class, i, properties[i]);

}

static void
gwy_fit_param_init(GwyFitParam *fitparam)
{
    fitparam->priv = G_TYPE_INSTANCE_GET_PRIVATE(fitparam, GWY_TYPE_FIT_PARAM,
                                                 FitParam);
}

static void
gwy_fit_param_finalize(GObject *object)
{
    GwyFitParam *fitparam = (GwyFitParam*)object;
    FitParam *priv = fitparam->priv;
    GWY_FREE(priv->name);
    GWY_FREE(priv->estimate);
    G_OBJECT_CLASS(gwy_fit_param_parent_class)->finalize(object);
}

static void
gwy_fit_param_set_property(GObject *object,
                           guint prop_id,
                           const GValue *value,
                           GParamSpec *pspec)
{
    GwyFitParam *fitparam = GWY_FIT_PARAM(object);
    FitParam *priv = fitparam->priv;

    switch (prop_id) {
        case PROP_NAME:
        // Constr only, no need to free previous value.
        priv->name = g_value_dup_string(value);
        break;

        case PROP_POWER_X:
        priv->power_x = g_value_get_int(value);
        break;

        case PROP_POWER_Y:
        priv->power_y = g_value_get_int(value);
        break;

        case PROP_ESTIMATE:
        gwy_fit_param_set_estimate(fitparam, g_value_get_string(value));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_fit_param_get_property(GObject *object,
                              guint prop_id,
                              GValue *value,
                              GParamSpec *pspec)
{
    GwyFitParam *fitparam = GWY_FIT_PARAM(object);
    FitParam *priv = fitparam->priv;

    switch (prop_id) {
        case PROP_NAME:
        g_value_set_string(value, priv->name);
        break;

        case PROP_POWER_X:
        g_value_set_int(value, priv->power_x);
        break;

        case PROP_POWER_Y:
        g_value_set_int(value, priv->power_y);
        break;

        case PROP_ESTIMATE:
        g_value_set_string(value, priv->estimate);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static gsize
gwy_fit_param_n_items(G_GNUC_UNUSED GwySerializable *serializable)
{
    return N_ITEMS;
}

static gsize
gwy_fit_param_itemize(GwySerializable *serializable,
                      GwySerializableItems *items)
{
    GwyFitParam *fitparam = GWY_FIT_PARAM(serializable);
    FitParam *priv = fitparam->priv;
    GwySerializableItem it;
    guint n = 0;

    g_return_val_if_fail(items->len - items->n >= N_ITEMS, 0);

    it = serialize_items[0];
    it.value.v_string = priv->name;
    items->items[items->n++] = it;
    n++;

    if (priv->power_x) {
        it = serialize_items[1];
        it.value.v_int32 = priv->power_x;
        items->items[items->n++] = it;
        n++;
    }

    if (priv->power_y) {
        it = serialize_items[2];
        it.value.v_int32 = priv->power_y;
        items->items[items->n++] = it;
        n++;
    }

    if (priv->estimate) {
        it = serialize_items[3];
        it.value.v_string = priv->estimate;
        items->items[items->n++] = it;
        n++;
    }

    return n;
}

static gboolean
gwy_fit_param_construct(GwySerializable *serializable,
                        GwySerializableItems *items,
                        GwyErrorList **error_list)
{
    GwyFitParam *fitparam = GWY_FIT_PARAM(serializable);
    FitParam *priv = fitparam->priv;

    GwySerializableItem its[N_ITEMS];
    memcpy(its, serialize_items, sizeof(serialize_items));
    gwy_deserialize_filter_items(its, N_ITEMS, items, NULL,
                                 "GwyFitParam", error_list);

    if (!its[0].value.v_string
        || !gwy_utf8_strisident(its[0].value.v_string, more, NULL)) {
        gwy_error_list_add(error_list, GWY_DESERIALIZE_ERROR,
                           GWY_DESERIALIZE_ERROR_INVALID,
                           _("Fiting parameter name is missing or not valid "
                             "UTF-8."));
        return FALSE;
    }

    GWY_TAKE_STRING(priv->name, its[0].value.v_string);
    // FIXME
    priv->power_x = CLAMP(its[1].value.v_int32, POWER_MIN, POWER_MAX);
    priv->power_y = CLAMP(its[2].value.v_int32, POWER_MIN, POWER_MAX);
    GWY_TAKE_STRING(priv->estimate, its[3].value.v_string);
    if (!gwy_fit_param_check_estimate(priv->estimate, NULL)) {
        gwy_error_list_add(error_list, GWY_DESERIALIZE_ERROR,
                           GWY_DESERIALIZE_ERROR_INVALID,
                           _("Fiting parameter %s estimate expression is "
                             "invalid."),
                           priv->name);
        return FALSE;
    }

    return TRUE;
}

static GObject*
gwy_fit_param_duplicate_impl(GwySerializable *serializable)
{
    GwyFitParam *fitparam = GWY_FIT_PARAM(serializable);
    FitParam *priv = fitparam->priv;
    GwyFitParam *duplicate = gwy_fit_param_new_set(priv->name,
                                                   priv->power_x, priv->power_y,
                                                   priv->estimate);
    return G_OBJECT(duplicate);
}

static void
gwy_fit_param_assign_impl(GwySerializable *destination,
                          GwySerializable *source)
{
    GwyFitParam *dest = GWY_FIT_PARAM(destination);
    GwyFitParam *src = GWY_FIT_PARAM(source);
    FitParam *spriv = src->priv, *dpriv = dest->priv;

    GParamSpec *notify[N_PROPS];
    guint nn = 0;
    // Names are construct-only and they cannot be assigned.
    if (dpriv->power_x != spriv->power_x) {
        notify[nn++] = properties[PROP_POWER_X];
        dpriv->power_x = spriv->power_x;
    }
    if (dpriv->power_y != spriv->power_y) {
        notify[nn++] = properties[PROP_POWER_Y];
        dpriv->power_y = spriv->power_y;
    }
    if (_gwy_assign_string(&dpriv->estimate, spriv->estimate))
        notify[nn++] = properties[PROP_ESTIMATE];

    _gwy_notify_properties_by_pspec(G_OBJECT(dest), notify, nn);
}

/**
 * gwy_fit_param_new:
 * @name: Parameter name.  It must be a valid identifier.
 *
 * Creates a new fitting parameter.
 *
 * Returns: A new fiting parameter.
 **/
GwyFitParam*
gwy_fit_param_new(const gchar *name)
{
    g_return_val_if_fail(name && gwy_utf8_strisident(name, more, NULL), NULL);
    return g_object_new(GWY_TYPE_FIT_PARAM, "name", name, NULL);
}

/**
 * gwy_fit_param_new_set:
 * @name: Parameter name.  It must be a valid identifier.
 * @power_x: Power of the abscissa contained in the parameter.
 * @power_y: Power of the ordinate contained in the parameter.
 * @estimate: Initial parameter estimate (a formula that can contain the
 *            estimator variables).
 *
 * Creates a new fiting parameter with specified properties.
 *
 * Returns: A new fiting parameter.
 **/
GwyFitParam*
gwy_fit_param_new_set(const gchar *name,
                      gint power_x,
                      gint power_y,
                      const gchar *estimate)
{
    if (!gwy_fit_param_check_estimate(estimate, NULL)) {
        g_critical("Invalid parameter estimate expression.");
        return NULL;
    }

    GwyFitParam *fitparam = gwy_fit_param_new(name);
    g_return_val_if_fail(fitparam, NULL);
    FitParam *priv = fitparam->priv;

    priv->power_x = CLAMP(power_x, POWER_MIN, POWER_MAX);
    priv->power_y = CLAMP(power_y, POWER_MIN, POWER_MAX);
    priv->estimate = g_strdup(estimate);

    return fitparam;
}

/**
 * gwy_fit_param_get_name:
 * @fitparam: A user-defined function fitting parameter.
 *
 * Gets the name of a user-defined fitting function parameter.
 *
 * Returns: The parameter name as a string owned by @fitparam.
 **/
const gchar*
gwy_fit_param_get_name(const GwyFitParam *fitparam)
{
    g_return_val_if_fail(GWY_IS_FIT_PARAM(fitparam), NULL);
    return fitparam->priv->name;
}

/**
 * gwy_fit_param_get_power_x:
 * @fitparam: A user-defined function fitting parameter.
 *
 * Gets the power of abscissa in a user-defined fitting function parameter.
 *
 * Returns: The abscissa power.
 **/
gint
gwy_fit_param_get_power_x(const GwyFitParam *fitparam)
{
    g_return_val_if_fail(GWY_IS_FIT_PARAM(fitparam), 0);
    return fitparam->priv->power_x;
}

/**
 * gwy_fit_param_set_power_x:
 * @fitparam: A user-defined function fitting parameter.
 * @power_x: Power of the abscissa contained in the parameter.
 *
 * Sets the power of abscissa in a user-defined fitting function parameter.
 **/
void
gwy_fit_param_set_power_x(GwyFitParam *fitparam,
                          gint power_x)
{
    g_return_if_fail(GWY_IS_FIT_PARAM(fitparam));
    g_return_if_fail(power_x >= POWER_MIN && power_x <= POWER_MAX);
    FitParam *priv = fitparam->priv;
    if (power_x == priv->power_x)
        return;
    priv->power_x = power_x;
    g_object_notify_by_pspec(G_OBJECT(fitparam), properties[PROP_POWER_X]);
}

/**
 * gwy_fit_param_get_power_y:
 * @fitparam: A user-defined function fitting parameter.
 *
 * Gets the power of ordinate in a user-defined fitting function parameter.
 *
 * Returns: The ordinate power.
 **/
gint
gwy_fit_param_get_power_y(const GwyFitParam *fitparam)
{
    g_return_val_if_fail(GWY_IS_FIT_PARAM(fitparam), 0);
    return fitparam->priv->power_y;
}

/**
 * gwy_fit_param_set_power_y:
 * @fitparam: A user-defined function fitting parameter.
 * @power_y: Power of the ordinate contained in the parameter.
 *
 * Sets the power of ordinate in a user-defined fitting function parameter.
 **/
void
gwy_fit_param_set_power_y(GwyFitParam *fitparam,
                          gint power_y)
{
    g_return_if_fail(GWY_IS_FIT_PARAM(fitparam));
    g_return_if_fail(power_y >= POWER_MIN && power_y <= POWER_MAX);
    FitParam *priv = fitparam->priv;
    if (power_y == priv->power_y)
        return;
    priv->power_y = power_y;
    g_object_notify_by_pspec(G_OBJECT(fitparam), properties[PROP_POWER_Y]);
}

/**
 * gwy_fit_param_get_estimate:
 * @fitparam: A user-defined function fitting parameter.
 *
 * Gets the initial estimate expression of a user-defined fitting function
 * parameter.
 *
 * Returns: The expression as a string owned by @fitparam.  Note it can be
 *          %NULL if no estimate is defined (0.0 is then expected to be used as
 *          the initial estimate unless you know better).
 **/
const gchar*
gwy_fit_param_get_estimate(const GwyFitParam *fitparam)
{
    g_return_val_if_fail(GWY_IS_FIT_PARAM(fitparam), NULL);
    return fitparam->priv->estimate;
}

/**
 * gwy_fit_param_set_estimate:
 * @fitparam: A user-defined function fitting parameter.
 * @estimate: Initial parameter estimate (a formula that can contain the
 *            estimator variables).  It can be %NULL for the default zero
 *            estimate.
 *
 * Sets the initial estimate expression of a user-defined fitting function
 * parameter.
 *
 * The expression must be valid.  If you obtain the expression from an
 * untrusted source, e.g. some file or the user, check it first with
 * gwy_fit_param_check_estimate().
 **/
void
gwy_fit_param_set_estimate(GwyFitParam *fitparam,
                           const gchar *estimate)
{
    g_return_if_fail(GWY_IS_FIT_PARAM(fitparam));
    if (!gwy_fit_param_check_estimate(estimate, NULL)) {
        g_critical("Invalid parameter estimate expression.");
        return;
    }
    FitParam *priv = fitparam->priv;
    if (_gwy_assign_string(&priv->estimate, estimate))
        g_object_notify_by_pspec(G_OBJECT(fitparam), properties[PROP_ESTIMATE]);
}

static GwyExpr*
make_estimator_expr(void)
{
    GwyExpr *expr = _gwy_fit_func_new_expr_with_constants();
    guint n;
    const gchar* const *estimators = _gwy_fit_func_estimators(&n);
    while (n--)
        gwy_expr_define_constant(expr, estimators[n], 0.0, NULL);
    return expr;
}

/**
 * gwy_fit_param_check_estimate:
 * @estimate: Initial parameter estimate.
 * @error: Return location for the error, or %NULL.  The error can be from
 *         either %GWY_FIT_PARAM_ERROR or %GWY_EXPR_ERROR domain.
 *
 * Validates the initial estimate expression of a user-defined fitting function
 * parameter.
 *
 * Returns: %TRUE if the expression is valid, i.e. it compiles and does not
 *          reference unknown quantities.  %FALSE if @estimate is invalid.
 **/
gboolean
gwy_fit_param_check_estimate(const gchar *estimate,
                             GError **error)
{
    static GwyExpr *test_expr = NULL;
    G_LOCK_DEFINE_STATIC(test_expr);

    if (!estimate)
        return TRUE;

    G_LOCK(test_expr);
    if (!test_expr)
        test_expr = make_estimator_expr();

    gboolean ok = TRUE;
    if (!gwy_expr_compile(test_expr, estimate, error))
        ok = FALSE;
    else {
        const gchar **vars = NULL;
        guint n = gwy_expr_get_variables(test_expr, &vars);
        if (n > 1) {
            gchar *list = g_strjoinv(", ", (gchar**)vars + 1);
            g_set_error(error, GWY_FIT_PARAM_ERROR,
                        GWY_FIT_PARAM_ERROR_VARIABLE,
                        _("Estimate expression contains unknown variables: "
                          "%s."),
                        list);
            g_free(list);
            ok = FALSE;
        }
    }
    G_UNLOCK(test_expr);

    return ok;
}

/**
 * gwy_fit_param_error_quark:
 *
 * Returns error domain for user-defined fitting function parameter
 * manipulation.
 *
 * See and use %GWY_FIT_PARAM_ERROR.
 *
 * Returns: The error domain.
 **/
GQuark
gwy_fit_param_error_quark(void)
{
    static GQuark error_domain = 0;

    if (!error_domain)
        error_domain = g_quark_from_static_string("gwy-fit-param-error-quark");

    return error_domain;
}


/**
 * SECTION: fit-param
 * @title: GwyFitParam
 * @short_description: User-defined fitting function parameter
 *
 * #GwyFitParam represents one parameter of a user-defined fitting function.
 * Its name is set upon construction and can never change.
 **/

/**
 * GwyFitParam:
 *
 * Object representing the user-defined fitting function parameters.
 *
 * The #GwyFitParam struct contains private data only and should be accessed
 * using the functions below.
 **/

/**
 * GwyFitParamClass:
 *
 * Class of user-defined fitting function parameters.
 **/

/**
 * gwy_fit_param_duplicate:
 * @fitparam: A user-defined function fitting parameter.
 *
 * Duplicates a user-defined function fitting parameter.
 *
 * This is a convenience wrapper of gwy_serializable_duplicate().
 **/

/**
 * gwy_fit_param_assign:
 * @dest: Destination user-defined function fitting parameter.
 * @src: Source user-defined function fitting parameter.
 *
 * Copies the value of a user-defined function fitting parameter.
 *
 * This is a convenience wrapper of gwy_serializable_assign().
 **/

/**
 * GWY_FIT_PARAM_ERROR:
 *
 * Error domain for user-defined fitting function parameter manipulation.
 * Errors in this domain will be from the #GwyFitParamError enumeration. See
 * #GError for information on error domains.
 **/

/**
 * GwyFitParamError:
 * @GWY_FIT_PARAM_ERROR_VARIABLE: Formula for initial parameter estimate
 *                                contains unknown quantities.
 *
 * Error codes returned by user-defined fitting function parameter
 * manipulation.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
