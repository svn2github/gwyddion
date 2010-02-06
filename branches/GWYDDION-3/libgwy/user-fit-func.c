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
#include <stdlib.h>
#include <glib/gi18n-lib.h>
#include "libgwy/macros.h"
#include "libgwy/strfuncs.h"
#include "libgwy/math.h"
#include "libgwy/unit.h"
#include "libgwy/expr.h"
#include "libgwy/serialize.h"
#include "libgwy/user-fit-func.h"
#include "libgwy/libgwy-aliases.h"
#include "libgwy/object-internal.h"
#include "libgwy/fit-func-builtin.h"

enum { N_ITEMS = 6 };

struct _GwyUserFitFuncPrivate {
    gchar *formula;
    gchar *filter;
    guint nparams;
    GwyFitParam *param;

    gchar **serialize_names;
    gint *serialize_x_powers;
    gint *serialize_y_powers;
    gchar **serialize_estimates;
};

typedef struct _GwyUserFitFuncPrivate UserFitFunc;

static void         gwy_user_fit_func_finalize         (GObject *object);
static void         gwy_user_fit_func_serializable_init(GwySerializableInterface *iface);
static gsize        gwy_user_fit_func_n_items          (GwySerializable *serializable);
static gsize        gwy_user_fit_func_itemize          (GwySerializable *serializable,
                                                        GwySerializableItems *items);
static void         gwy_user_fit_func_done             (GwySerializable *serializable);
static gboolean     gwy_user_fit_func_construct        (GwySerializable *serializable,
                                                        GwySerializableItems *items,
                                                        GwyErrorList **error_list);
static GObject*     gwy_user_fit_func_duplicate_impl   (GwySerializable *serializable);
static void         gwy_user_fit_func_assign_impl      (GwySerializable *destination,
                                                        GwySerializable *source);
static GwyResource* gwy_user_fit_func_copy             (GwyResource *resource);
static void         gwy_user_fit_func_changed          (GwyUserFitFunc *userfitfunc);
static void         sanitize_param                     (GwyFitParam *param);
static void         sanitize                           (GwyUserFitFunc *userfitfunc);
static gboolean     gwy_user_fit_func_validate         (GwyUserFitFunc *userfitfunc);
static gchar*       gwy_user_fit_func_dump             (GwyResource *resource);
static gboolean     gwy_user_fit_func_parse            (GwyResource *resource,
                                                        gchar *text,
                                                        GError **error);

static const GwyFitParam default_param = {
    .name = "a", .power_x = 0, .power_y = 1, .estimate = "0",
};

static const GwySerializableItem serialize_items[N_ITEMS] = {
    /*0*/ { .name = "formula",    .ctype = GWY_SERIALIZABLE_STRING,       },
    /*1*/ { .name = "filter",     .ctype = GWY_SERIALIZABLE_STRING,       },
    /*2*/ { .name = "param",      .ctype = GWY_SERIALIZABLE_STRING_ARRAY, },
    /*3*/ { .name = "x-power",    .ctype = GWY_SERIALIZABLE_INT32_ARRAY,  },
    /*4*/ { .name = "y-power",    .ctype = GWY_SERIALIZABLE_INT32_ARRAY,  },
    /*5*/ { .name = "estimate",   .ctype = GWY_SERIALIZABLE_STRING_ARRAY, },
};

static GwyExpr *test_expr = NULL;
G_LOCK_DEFINE_STATIC(test_expr);

G_DEFINE_TYPE_EXTENDED
    (GwyUserFitFunc, gwy_user_fit_func, GWY_TYPE_RESOURCE, 0,
     GWY_IMPLEMENT_SERIALIZABLE(gwy_user_fit_func_serializable_init))

GwySerializableInterface *gwy_user_fit_func_parent_serializable = NULL;

GType
gwy_fit_param_get_type(void)
{
    static GType param_type = 0;

    if (G_UNLIKELY(!param_type)) {
        param_type = g_boxed_type_register_static
                                           ("GwyFitParam",
                                            (GBoxedCopyFunc)gwy_fit_param_copy,
                                            (GBoxedFreeFunc)gwy_fit_param_free);
        // XXX: Make it serializable?
    }

    return param_type;
}

/**
 * gwy_fit_param_copy:
 * @fitparam: A user-defined function fitting parameter.
 *
 * Copies a user-defined function fitting parameter.
 *
 * Returns: A copy of @fitparam. The result must be freed using
 *          gwy_fit_param_free(), not g_free().
 **/
GwyFitParam*
gwy_fit_param_copy(const GwyFitParam *fitparam)
{
    g_return_val_if_fail(fitparam, NULL);
    GwyFitParam *newparam = g_slice_copy(sizeof(GwyFitParam), fitparam);
    newparam->name = g_strdup(newparam->name);
    newparam->estimate = g_strdup(newparam->estimate);
    return newparam;
}

/**
 * gwy_fit_param_free:
 * @fitparam: A user-defined function fitting parameter.
 *
 * Frees a user-defined function fitting parameter created
 * with gwy_fit_param_copy().
 **/
void
gwy_fit_param_free(GwyFitParam *fitparam)
{
    g_free(fitparam->name);
    g_free(fitparam->estimate);
    g_slice_free1(sizeof(GwyFitParam), fitparam);
}

static void
gwy_user_fit_func_serializable_init(GwySerializableInterface *iface)
{
    gwy_user_fit_func_parent_serializable = g_type_interface_peek_parent(iface);
    iface->n_items   = gwy_user_fit_func_n_items;
    iface->itemize   = gwy_user_fit_func_itemize;
    iface->done      = gwy_user_fit_func_done;
    iface->construct = gwy_user_fit_func_construct;
    iface->duplicate = gwy_user_fit_func_duplicate_impl;
    iface->assign    = gwy_user_fit_func_assign_impl;
}

static void
gwy_user_fit_func_class_init(GwyUserFitFuncClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GwyResourceClass *res_class = GWY_RESOURCE_CLASS(klass);

    g_type_class_add_private(klass, sizeof(UserFitFunc));

    gobject_class->finalize = gwy_user_fit_func_finalize;

    res_class->copy = gwy_user_fit_func_copy;
    res_class->dump = gwy_user_fit_func_dump;
    res_class->parse = gwy_user_fit_func_parse;

    gwy_resource_class_register(res_class, "userfitfuncs", NULL);
}

static void
free_params(UserFitFunc *priv)
{
    for (unsigned i = 0; i < priv->nparams; i++) {
        GWY_FREE(priv->param[i].name);
        GWY_FREE(priv->param[i].estimate);
    }
    GWY_FREE(priv->param);
    priv->nparams = 0;
}

static void
assign_params(UserFitFunc *priv,
              guint nparams,
              const GwyFitParam *param)
{
    if (priv->nparams == nparams) {
        for (guint i = 0; i < nparams; i++) {
            if (!gwy_strequal(priv->param[i].name, param[i].name)) {
                g_free(priv->param[i].name);
                priv->param[i].name = g_strdup(param[i].name);
            }
            priv->param[i].power_x = param[i].power_x;
            priv->param[i].power_y = param[i].power_y;
            if (!gwy_strequal(priv->param[i].estimate, param[i].estimate)) {
                g_free(priv->param[i].estimate);
                priv->param[i].estimate = g_strdup(param[i].estimate);
            }
        }
    }
    else {
        // The number of params differ, don't bother.
        free_params(priv);
        priv->param = g_new(GwyFitParam, nparams);
        for (guint i = 0; i < nparams; i++) {
            priv->param[i].name = g_strdup(param[i].name);
            priv->param[i].power_x = param[i].power_x;
            priv->param[i].power_y = param[i].power_y;
            priv->param[i].estimate = g_strdup(param[i].estimate);
        }
    }
}

static void
gwy_user_fit_func_init(GwyUserFitFunc *userfitfunc)
{
    userfitfunc->priv = G_TYPE_INSTANCE_GET_PRIVATE(userfitfunc,
                                                    GWY_TYPE_USER_FIT_FUNC,
                                                    UserFitFunc);
    UserFitFunc *priv = userfitfunc->priv;

    // Constant value, by default.
    priv->formula = g_strdup(default_param.name);
    priv->filter = g_strdup("");
    assign_params(priv, 1, &default_param);
}

static void
gwy_user_fit_func_finalize(GObject *object)
{
    GwyUserFitFunc *userfitfunc = GWY_USER_FIT_FUNC(object);
    UserFitFunc *priv = userfitfunc->priv;
    GWY_FREE(priv->formula);
    GWY_FREE(priv->filter);
    free_params(priv);
    G_OBJECT_CLASS(gwy_user_fit_func_parent_class)->finalize(object);
}

static gsize
gwy_user_fit_func_n_items(GwySerializable *serializable)
{
    return N_ITEMS+1 + gwy_user_fit_func_parent_serializable->n_items(serializable);
}

static void
serialize_params(UserFitFunc *priv)
{
    guint n = priv->nparams;
    priv->serialize_names = g_slice_alloc(n*sizeof(gchar*));
    priv->serialize_x_powers = g_slice_alloc(n*sizeof(gint));
    priv->serialize_y_powers = g_slice_alloc(n*sizeof(gint));
    priv->serialize_estimates = g_slice_alloc(n*sizeof(gchar*));
    for (guint i = 0; i < n; i++) {
        priv->serialize_names[i] = priv->param[i].name;
        priv->serialize_x_powers[i] = priv->param[i].power_x;
        priv->serialize_y_powers[i] = priv->param[i].power_y;
        priv->serialize_estimates[i] = priv->param[i].estimate;
    }
}

static gsize
gwy_user_fit_func_itemize(GwySerializable *serializable,
                          GwySerializableItems *items)
{
    g_return_val_if_fail(items->len - items->n >= N_ITEMS+1, 0);

    GwyUserFitFunc *userfitfunc = GWY_USER_FIT_FUNC(serializable);
    UserFitFunc *priv = userfitfunc->priv;
    GwySerializableItem *it = items->items + items->n;

    // Our own data
    *it = serialize_items[0];
    it->value.v_string = priv->formula;
    it++, items->n++;

    if (priv->filter) {
        *it = serialize_items[1];
        it->value.v_string = priv->filter;
        it++, items->n++;
    }

    serialize_params(priv);

    *it = serialize_items[2];
    it->value.v_string_array = priv->serialize_names;
    it->array_size = priv->nparams;
    it++, items->n++;

    *it = serialize_items[3];
    it->value.v_int32_array = priv->serialize_x_powers;
    it->array_size = priv->nparams;
    it++, items->n++;

    *it = serialize_items[4];
    it->value.v_int32_array = priv->serialize_y_powers;
    it->array_size = priv->nparams;
    it++, items->n++;

    *it = serialize_items[5];
    it->value.v_string_array = priv->serialize_estimates;
    it->array_size = priv->nparams;
    it++, items->n++;

    // Chain to parent
    g_return_val_if_fail(items->len - items->n, 0);
    it->ctype = GWY_SERIALIZABLE_PARENT;
    it->name = g_type_name(GWY_TYPE_RESOURCE);
    it->array_size = 0;
    it->value.v_type = GWY_TYPE_RESOURCE;
    it++, items->n++;

    guint n;
    if ((n = gwy_user_fit_func_parent_serializable->itemize(serializable, items)))
        return N_ITEMS+1 + n;
    return 0;
}

static void
gwy_user_fit_func_done(GwySerializable *serializable)
{
    GwyUserFitFunc *userfitfunc = GWY_USER_FIT_FUNC(serializable);
    UserFitFunc *priv = userfitfunc->priv;
    guint n = priv->nparams;

    g_slice_free1(n*sizeof(gchar*), priv->serialize_names);
    g_slice_free1(n*sizeof(gint), priv->serialize_x_powers);
    g_slice_free1(n*sizeof(gint), priv->serialize_y_powers);
    g_slice_free1(n*sizeof(gchar*), priv->serialize_estimates);
}

static gboolean
gwy_user_fit_func_construct(GwySerializable *serializable,
                            GwySerializableItems *items,
                            GwyErrorList **error_list)
{
    GwySerializableItem its[N_ITEMS];
    memcpy(its, serialize_items, sizeof(serialize_items));
    gsize np = gwy_deserialize_filter_items(its, N_ITEMS, items,
                                            "GwyUserFitFunc",
                                            error_list);
    guint n = its[2].array_size;

    // Chain to parent
    if (np < items->n) {
        np++;
        GwySerializableItems parent_items = {
            items->len - np, items->n - np, items->items + np
        };
        if (!gwy_user_fit_func_parent_serializable->construct(serializable,
                                                              &parent_items,
                                                              error_list))
            goto fail;
    }

    // Our own data
    GwyUserFitFunc *userfitfunc = GWY_USER_FIT_FUNC(serializable);
    UserFitFunc *priv = userfitfunc->priv;

    if (!n || n > 256) {
        gwy_error_list_add(error_list, GWY_DESERIALIZE_ERROR,
                           GWY_DESERIALIZE_ERROR_INVALID,
                           _("Invalid number of user fitting function "
                             "parameters: %lu."),
                           (gulong)its[0].array_size);
        goto fail;
    }
    if (its[3].array_size != n
        || its[4].array_size != n
        || its[5].array_size != n) {
        gwy_error_list_add(error_list, GWY_DESERIALIZE_ERROR,
                           GWY_DESERIALIZE_ERROR_INVALID,
                           _("The number of parameter attributes do not match "
                             "the number of parameters."));
        goto fail;
    }

    priv->param = g_new(GwyFitParam, n);
    priv->nparams = n;
    for (guint i = 0; i < n; i++) {
        GwyFitParam *param = priv->param + i;
        param->name = its[2].value.v_string_array[i];
        param->power_x = its[3].value.v_int32_array[i];
        param->power_y = its[4].value.v_int32_array[i];
        param->estimate = its[5].value.v_string_array[i];
        its[2].value.v_string_array[i] = NULL;
        its[5].value.v_string_array[i] = NULL;
    }
    g_free(its[2].value.v_string_array);
    g_free(its[3].value.v_int32_array);
    g_free(its[4].value.v_int32_array);
    g_free(its[5].value.v_string_array);

    sanitize(userfitfunc);
    gboolean ok = gwy_user_fit_func_validate(userfitfunc);
    if (!ok) {
        gwy_error_list_add(error_list, GWY_DESERIALIZE_ERROR,
                           GWY_DESERIALIZE_ERROR_INVALID,
                           _("Invalid formula or parameters."));
    }

    return ok;

fail:
    for (guint i = 0; i < n; i++) {
        g_free(its[2].value.v_string_array[i]);
        g_free(its[5].value.v_string_array[i]);
    }
    g_free(its[2].value.v_string_array);
    g_free(its[3].value.v_int32_array);
    g_free(its[4].value.v_int32_array);
    g_free(its[5].value.v_string_array);
    return FALSE;
}

static void
assign_info(UserFitFunc *dpriv,
            const UserFitFunc *spriv)
{
    GWY_FREE(dpriv->formula);
    dpriv->formula = g_strdup(spriv->formula);
    GWY_FREE(dpriv->filter);
    dpriv->filter = g_strdup(spriv->filter);
}

static GObject*
gwy_user_fit_func_duplicate_impl(GwySerializable *serializable)
{
    GwyUserFitFunc *userfitfunc = GWY_USER_FIT_FUNC(serializable);
    UserFitFunc *priv = userfitfunc->priv;
    GwyUserFitFunc *duplicate = g_object_newv(GWY_TYPE_USER_FIT_FUNC, 0, NULL);
    UserFitFunc *dpriv = duplicate->priv;

    gwy_user_fit_func_parent_serializable->assign(GWY_SERIALIZABLE(duplicate),
                                                  serializable);
    // FIXME: This is not very efficient
    assign_params(dpriv, priv->nparams, priv->param);
    assign_info(dpriv, priv);

    return G_OBJECT(duplicate);
}

static void
gwy_user_fit_func_assign_impl(GwySerializable *destination,
                              GwySerializable *source)
{
    GwyUserFitFunc *dest = GWY_USER_FIT_FUNC(destination);
    GwyUserFitFunc *src = GWY_USER_FIT_FUNC(source);

    g_object_freeze_notify(G_OBJECT(dest));
    gwy_user_fit_func_parent_serializable->assign(destination, source);

    const UserFitFunc *spriv = src->priv;
    UserFitFunc *dpriv = dest->priv;
    assign_params(dpriv, spriv->nparams, spriv->param);
    assign_info(dpriv, spriv);

    g_object_thaw_notify(G_OBJECT(dest));
    gwy_user_fit_func_changed(dest);
}

static GwyResource*
gwy_user_fit_func_copy(GwyResource *resource)
{
    return GWY_RESOURCE(gwy_user_fit_func_duplicate_impl(GWY_SERIALIZABLE(resource)));
}

static void
gwy_user_fit_func_changed(GwyUserFitFunc *userfitfunc)
{
    gwy_resource_data_changed(GWY_RESOURCE(userfitfunc));
}

// Get the function and params to a physically sane state.
static void
sanitize_param(GwyFitParam *param)
{
    gchar *end;

    if (!param->name)
        param->name = g_strdup(default_param.name);
    else if (!g_utf8_validate(param->name, -1, (const gchar**)&end))
        *end = '\0';

    if (!param->estimate)
        param->estimate = g_strdup(default_param.estimate);
    else if (!g_utf8_validate(param->estimate, -1, (const gchar**)&end))
        *end = '\0';

    param->power_x = CLAMP(param->power_x, -12, 12);
    param->power_y = CLAMP(param->power_y, -12, 12);
}

static void
sanitize(GwyUserFitFunc *userfitfunc)
{
    UserFitFunc *priv = userfitfunc->priv;
    gchar *end;

    if (!priv->formula)
        priv->formula = g_strdup("0");
    else if (!g_utf8_validate(priv->formula, -1, (const gchar**)&end))
        *end = '\0';

    if (!priv->filter && !g_utf8_validate(priv->filter, -1, (const gchar**)&end))
        *end = '\0';

    if (!priv->nparams) {
        free_params(priv);
        assign_params(priv, 1, &default_param);
    }
    for (guint i = 0; i < priv->nparams; i++)
        sanitize_param(priv->param + i);
}

// Verify if the state is consistent logically.
// We assume gwy_user_fit_func_sanitize() has been done.
static gboolean
gwy_user_fit_func_validate(GwyUserFitFunc *userfitfunc)
{
    static const gunichar more[] = { '_', 0 };

    UserFitFunc *priv = userfitfunc->priv;
    // Parameters
    guint n = priv->nparams;
    for (guint i = 0; i < n; i++) {
        if (!gwy_utf8_strisident(priv->param[i].name, more, NULL))
            return FALSE;
        for (guint j = i+1; j < n; j++) {
            if (gwy_strequal(priv->param[i].name, priv->param[j].name))
                return FALSE;
        }
    }

    G_LOCK(test_expr);
    gboolean ok = FALSE;

    // Fomula
    if (!test_expr)
        test_expr = _gwy_fit_func_new_expr_with_constants();
    if (!gwy_expr_compile(test_expr, priv->formula, NULL))
        goto fail;
    if (gwy_user_fit_func_resolve_params(userfitfunc, test_expr, NULL, NULL))
        goto fail;

    // Filter
    if (priv->filter && strlen(priv->filter)) {
        if (!gwy_expr_compile(test_expr, priv->filter, NULL))
            goto fail;
        const gchar *names[1] = { "x" };
        guint indices[1];
        if (gwy_expr_resolve_variables(test_expr, 1, names, indices))
            goto fail;
    }

    // Estimators
    for (guint i = 0; i < n; i++) {
        if (!gwy_expr_compile(test_expr, priv->param[i].estimate, NULL))
            goto fail;
        if (_gwy_fit_func_check_estimators(test_expr))
            goto fail;
    }
    ok = TRUE;

fail:
    G_UNLOCK(test_expr);
    return ok;
}

/**
 * gwy_user_fit_func_new:
 *
 * Creates a new user fitting function.
 *
 * Returns: A new free-standing user fitting function.
 **/
GwyUserFitFunc*
gwy_user_fit_func_new(void)
{
    return g_object_newv(GWY_TYPE_USER_FIT_FUNC, 0, NULL);
}

/**
 * gwy_user_fit_func_get_formula:
 * @userfitfunc: A user fitting function.
 *
 * Gets the formula of a user fitting function.
 *
 * Returns: The formula as a string owned by @userfitfunc.
 **/
const gchar*
gwy_user_fit_func_get_formula(GwyUserFitFunc *userfitfunc)
{
    g_return_val_if_fail(GWY_IS_USER_FIT_FUNC(userfitfunc), NULL);
    return userfitfunc->priv->formula;
}

/**
 * gwy_user_fit_func_set_formula:
 * @userfitfunc: A user fitting function.
 * @formula: New fitting function formula.
 * @error: Return location for the error, or %NULL.  The error can be from
 *         either %GWY_USER_FIT_FUNC_ERROR or %GWY_EXPR_ERROR domain.
 *
 * Sets the formula of a user fitting function.
 *
 * The formula is validated and possibly rejected (the function then returns
 * %FALSE).  If it is accepted the parameters are rebuilt to correspond to the
 * new formula.  Same-named parameters in the old and new formula are assumed
 * to be the same parameter and so their properties are retained.  Parameters
 * not present in the old formula are defined with default properties.
 *
 * Returns: %TRUE if the formula was been changed to @formula, %FALSE if
 *          @formula is invalid and hence it was not set as the new formula.
 **/
gboolean
gwy_user_fit_func_set_formula(GwyUserFitFunc *userfitfunc,
                              const gchar *formula,
                              GError **error)
{
    g_return_val_if_fail(GWY_IS_USER_FIT_FUNC(userfitfunc), FALSE);
    g_return_val_if_fail(formula, FALSE);
    UserFitFunc *priv = userfitfunc->priv;

    if (gwy_strequal(formula, priv->formula))
        return TRUE;

    G_LOCK(test_expr);
    if (!gwy_expr_compile(test_expr, formula, error)) {
        G_UNLOCK(test_expr);
        return FALSE;
    }
    const gchar **names;
    guint n = gwy_expr_get_variables(test_expr, &names);
    // This is usualy still two parameters too large as the vars contain "x".
    GwyFitParam *newparam = g_new0(GwyFitParam, n);
    guint np = 0;
    for (guint i = 1; i < n; i++) {
        if (gwy_strequal(names[i], "x"))
            continue;

        guint j = 0;
        while (j < priv->nparams) {
            if (gwy_strequal(names[i], priv->param[j].name))
                break;
            j++;
        }
        if (j == priv->nparams) {
            newparam[np].name = g_strdup(names[i]);
            newparam[np].estimate = g_strdup(default_param.estimate);
        }
        else {
            newparam[np] = priv->param[j];
            priv->param[j].name = NULL;
            priv->param[j].estimate = NULL;
        }
        np++;
    }
    // Must not release it eariler while we still use names[].
    G_UNLOCK(test_expr);

    if (!np) {
        g_set_error(error, GWY_USER_FIT_FUNC_ERROR,
                    GWY_USER_FIT_FUNC_ERROR_NO_PARAM,
                    _("Fitting function has no parameters."));
        g_free(newparam);
        return FALSE;
    }

    free_params(priv);
    priv->param = newparam;
    priv->nparams = np;
    GWY_FREE(priv->formula);
    priv->formula = g_strdup(formula);
    gwy_user_fit_func_changed(userfitfunc);

    return TRUE;

}

/**
 * gwy_user_fit_func_n_params:
 * @userfitfunc: A user fitting function.
 *
 * Gets the number of parameters of a user fitting function.
 *
 * Returns: The number of parameters.
 **/
guint
gwy_user_fit_func_n_params(GwyUserFitFunc *userfitfunc)
{
    g_return_val_if_fail(GWY_IS_USER_FIT_FUNC(userfitfunc), 0);
    UserFitFunc *priv = userfitfunc->priv;
    return priv->nparams;
}

/**
 * gwy_user_fit_func_param:
 * @userfitfunc: A user fitting function.
 * @name: Parameter name.
 *
 * Obtains one user fitting function parameter.
 *
 * Returns: The parameter, as a pointer to @userfitfunc-owned structure.  It
 *          must not be modified and it remains valid as long as @userfitfunc
 *          does not change.
 **/
const GwyFitParam*
gwy_user_fit_func_param(GwyUserFitFunc *userfitfunc,
                        const gchar *name)
{
    g_return_val_if_fail(GWY_IS_USER_FIT_FUNC(userfitfunc), NULL);
    g_return_val_if_fail(name, NULL);
    UserFitFunc *priv = userfitfunc->priv;
    for (guint i = 0; i < priv->nparams; i++) {
        if (gwy_strequal(name, priv->param[i].name))
            return priv->param + i;
    }
    return NULL;
}

/**
 * gwy_user_fit_func_nth_param:
 * @userfitfunc: A user fitting function.
 * @i: Parameter number.
 *
 * Obtains one user fitting function parameter given by index.
 *
 * Returns: The parameter, as a pointer to @userfitfunc-owned structure.  It
 *          must not be modified and it remains valid as long as @userfitfunc
 *          does not change.
 **/
const GwyFitParam*
gwy_user_fit_func_nth_param(GwyUserFitFunc *userfitfunc,
                            guint i)
{
    g_return_val_if_fail(GWY_IS_USER_FIT_FUNC(userfitfunc), NULL);
    UserFitFunc *priv = userfitfunc->priv;
    g_return_val_if_fail(i < priv->nparams, NULL);
    return priv->param + i;
}

/**
 * gwy_user_fit_func_update_param:
 * @userfitfunc: A user fitting function.
 * @param: Parameter data.  All entries are copied so the caller retains its
 *         ownership of all @param's data.  It is safe to pass @param
 *         containing strings obtained from gwy_user_fit_func_param() or
 *         gwy_user_fit_func_nth_param().  However, as this function may
 *         invalidate them (e.g. by freeing them) it is not safe to continue
 *         using these parameter structs afterwards.
 *
 * Updates one user fitting function parameter.
 *
 * The parameter to update is identified by the name in the @param struct.
 * This means the name must be euqal to the name of an existing parameter.  It
 * is not possible to rename parameters as it does not make any sense;
 * parameter names are determined by the function formula.
 *
 * Returns: The parameter, as a pointer to @userfitfunc-owned structure.  It
 *          must not be modified and it remains valid as long as @userfitfunc
 *          does not change.
 **/
void
gwy_user_fit_func_update_param(GwyUserFitFunc *userfitfunc,
                               const GwyFitParam *param)
{
    g_return_if_fail(GWY_IS_USER_FIT_FUNC(userfitfunc));
    g_return_if_fail(param && param->name && param->estimate);

    UserFitFunc *priv = userfitfunc->priv;
    GwyFitParam *myparam = NULL;

    for (guint i = 0; i < priv->nparams; i++) {
        myparam = priv->param + i;
        if (param->name == myparam->name
            || gwy_strequal(param->name, myparam->name))
            break;
    }
    if (!myparam) {
        g_warning("Fitting function has no parameter called %s.", param->name);
        return;
    }

    myparam->power_x = param->power_x;
    myparam->power_y = param->power_y;
    sanitize_param(myparam);

    // We must be careful not to free strings before we use them
    if (param->estimate != myparam->estimate
        && !gwy_strequal(param->estimate, myparam->estimate)
        && gwy_user_fit_func_check_estimate(param->estimate, NULL)) {

        gchar *old = myparam->estimate;
        myparam->estimate = g_strdup(param->estimate);
        g_free(old);
    }

    gwy_user_fit_func_changed(userfitfunc);
}

// FIXME: This is illogical.  Why should the user pass the compiled expr?
/**
 * gwy_user_fit_func_resolve_params:
 * @userfitfunc: A user fitting function.
 * @expr: An expression, presumably with compiled @userfitfunc's formula.
 * @independent_name: Name of independent variable (abscissa), pass %NULL for
 *                    the default "x".
 * @indices: Array to store the map from the parameter number to the
 *           @expr variable number.  The abscissa goes last after all
 *           parameters.
 *
 * Resolves the mapping between paramters and formula variables for a
 * user fitting function.
 *
 * See gwy_expr_resolve_variables() for some discussion.
 *
 * Returns: The number of unresolved identifiers in @expr.
 **/
guint
gwy_user_fit_func_resolve_params(GwyUserFitFunc *userfitfunc,
                                 GwyExpr *expr,
                                 const gchar *independent_name,
                                 guint *indices)
{
    g_return_val_if_fail(GWY_IS_USER_FIT_FUNC(userfitfunc), G_MAXUINT);
    g_return_val_if_fail(GWY_IS_EXPR(expr), G_MAXUINT);
    if (!independent_name)
        independent_name = "x";
    UserFitFunc *priv = userfitfunc->priv;

    guint n = priv->nparams;
    const gchar *names[n+1];
    for (guint i = 0; i < n; i++)
        names[i] = priv->param[i].name;
    names[n] = independent_name;
    if (!indices)
        indices = g_newa(guint, n+1);

    return gwy_expr_resolve_variables(expr, n+1, names, indices);
}

/**
 * gwy_user_fit_func_check_estimate:
 * @estimate: Initial parameter estimate (a formula that can contain the
 *            estimator variables).
 * @error: Return location for the error, or %NULL.  The error can be from
 *         either %GWY_USER_FIT_FUNC_ERROR or %GWY_EXPR_ERROR domain.
 *
 * Check validity of a parameter estimation formula.
 *
 * Returns: %TRUE if @estimate is a valid parameter estimation formula.
 **/
gboolean
gwy_user_fit_func_check_estimate(const gchar *estimate,
                                 GError **error)
{
    g_return_val_if_fail(estimate, FALSE);

    G_LOCK(test_expr);
    if (!gwy_expr_compile(test_expr, estimate, error)) {
        G_UNLOCK(test_expr);
        return FALSE;
    }

    const gchar *name = _gwy_fit_func_check_estimators(test_expr);
    if (name) {
        if (*name)
            g_set_error(error, GWY_USER_FIT_FUNC_ERROR,
                        GWY_USER_FIT_FUNC_ERROR_ESTIMATE,
                        _("Estimate expression contains unknown variable %s."),
                        name);
        else {
            g_critical("_gwy_fit_func_check_estimators() returned fatal error "
                       "for a successfully compiled estimator.");
        }
    }
    G_UNLOCK(test_expr);

    return name == NULL;
}

static gchar*
gwy_user_fit_func_dump(GwyResource *resource)
{
    GwyUserFitFunc *userfitfunc = GWY_USER_FIT_FUNC(resource);
    UserFitFunc *priv = userfitfunc->priv;

    GString *text = g_string_new(NULL);
    g_string_append_printf(text, "formula %s\n", priv->formula);
    if (priv->filter)
        g_string_append_printf(text, "filter %s\n", priv->filter);

    for (unsigned i = 0; i < priv->nparams; i++) {
        GwyFitParam *param = priv->param + i;
        g_string_append_printf(text, "param %s\n", param->name);
        g_string_append_printf(text, "power_x %d\n", param->power_x);
        g_string_append_printf(text, "power_y %d\n", param->power_y);
        g_string_append_printf(text, "estimate %s\n", param->estimate);
    }
    return g_string_free(text, FALSE);;
}

static gboolean
gwy_user_fit_func_parse(GwyResource *resource,
                        gchar *text,
                        GError **error)
{
    GwyUserFitFunc *userfitfunc = GWY_USER_FIT_FUNC(resource);
    UserFitFunc *priv = userfitfunc->priv;
    GwyFitParam param;
    GArray *params = NULL;

    gwy_memclear(&param, 1);
    for (gchar *line = gwy_str_next_line(&text);
         line;
         line = gwy_str_next_line(&text)) {
        gchar *key, *value;
        GwyResourceLineType type = gwy_resource_parse_param_line(line,
                                                                 &key, &value);
        if (type == GWY_RESOURCE_LINE_EMPTY)
            continue;
        if (type != GWY_RESOURCE_LINE_OK)
            break;

        if (gwy_strequal(key, "param")) {
            if (params) {
                g_array_append_val(params, param);
                gwy_memclear(&param, 1);
                param.name = g_strdup(value);
            }
            else
                params = g_array_new(FALSE, FALSE, sizeof(GwyFitParam));
        }
        else if (params) {
            if (gwy_strequal(key, "power_x"))
                param.power_x = strtol(value, NULL, 10);
            else if (gwy_strequal(key, "power_y"))
                param.power_y = strtol(value, NULL, 10);
            else if (gwy_strequal(key, "estimate")) {
                GWY_FREE(param.estimate);
                param.estimate = g_strdup(value);
            }
            else
                break;
        }
        else {
            if (gwy_strequal(key, "formula")) {
                g_free(priv->formula);
                priv->formula = g_strdup(value);
            }
            else if (gwy_strequal(key, "filter")) {
                GWY_FREE(priv->filter);
                priv->filter = g_strdup(value);
            }
            else
                break;
        }
    }
    if (param.name)
        g_array_append_val(params, param);

    // TODO
    if (!params)
        return FALSE;

    free_params(priv);
    priv->nparams = params->len;
    priv->param = (GwyFitParam*)g_array_free(params, FALSE);

    sanitize(userfitfunc);
    if (!gwy_user_fit_func_validate(userfitfunc)) {
        priv->nparams = 0;
        GWY_FREE(priv->param);
        // TODO
        return FALSE;
    }

    return TRUE;
}

/**
 * gwy_user_fit_func_error_quark:
 *
 * Returns error domain for user-defined fitting function manipulation.
 *
 * See and use %GWY_USER_FIT_FUNC_ERROR.
 *
 * Returns: The error domain.
 **/
GQuark
gwy_user_fit_func_error_quark(void)
{
    static GQuark error_domain = 0;

    if (!error_domain)
        error_domain = g_quark_from_static_string("gwy-user-fit-func-error-quark");

    return error_domain;
}

#define __LIBGWY_USER_FIT_FUNC_C__
#include "libgwy/libgwy-aliases.c"

/************************** Documentation ****************************/

/**
 * SECTION: user-fit-func
 * @title: GwyUserFitFunc
 * @short_description: User-defined fitting function
 *
 * #GwyUserFitFunc is a user fitting function definition.  It servers only for
 * user-defined fitting function representation and manipulation, it does not
 * have any internal state and it does not perform any fitting itself, see
 * #GwyFitFunc for that.
 *
 * #GwyUserFitFunc holds, of course, namely the fitting formula that can be
 * obtained with gwy_user_fit_func_get_formula() and modified with
 * gwy_user_fit_func_set_formula().
 *
 * Beside the function #GwyUserFitFunc holds also auxiliary information about
 * the fitting parameters such as unit powers or initial estimations.
 * Parameters and their names are determined by the function formula but their
 * other properties can be manipulated with gwy_user_fit_func_update_param().
 * To change both the formula and parameters, the formula must be changed first
 * with gwy_user_fit_func_set_formula() which causes update of the parameter
 * list.  Then the parameter properties can be updated.  This ensures the
 * object never gets into an inconsistent state.
 *
 * Parameter estimations can be based on the following quantities:
 * <variablelist>
 *   <varlistentry>
 *     <term>@xmin</term>
 *     <listitem>Minimum abscissa (@x) value.</listitem>
 *   </varlistentry>
 *   <varlistentry>
 *     <term>@xmid</term>
 *     <listitem>Central abscissa (@x) value, equal to
 *               (@xmin+@max)/2.</listitem>
 *   </varlistentry>
 *   <varlistentry>
 *     <term>@xmax</term>
 *     <listitem>Maximum abscissa (@x) value.</listitem>
 *   </varlistentry>
 *   <varlistentry>
 *     <term>@ymin</term>
 *     <listitem>Minimum ordinate (@y) value.</listitem>
 *   </varlistentry>
 *   <varlistentry>
 *     <term>@ymax</term>
 *     <listitem>Maximum ordinate (@y) value.</listitem>
 *   </varlistentry>
 *   <varlistentry>
 *     <term>@xmean</term>
 *     <listitem>Average ordinate (@y) value.</listitem>
 *   </varlistentry>
 *   <varlistentry>
 *     <term>@xymin</term>
 *     <listitem>Position of the minimum, i.e. the abscissa (@x) value at which
 *               @ymin occurs.  If it occurs at multiple data points, an
 *               arbitrary one is substituted, usually the first.</listitem>
 *   </varlistentry>
 *   <varlistentry>
 *     <term>@xymax</term>
 *     <listitem>Position of the maximum, i.e. the abscissa (@x) value at which
 *               @ymax occurs.  If it occurs at multiple data points, an
 *               arbitrary one is substituted, usually the first.</listitem>
 *   </varlistentry>
 *   <varlistentry>
 *     <term>@yxmin</term>
 *     <listitem>First value, i.e. the ordinate (@y) value at @xmin.</listitem>
 *   </varlistentry>
 *   <varlistentry>
 *     <term>@yxmid</term>
 *     <listitem>Central value, i.e. the ordinate (@y) value at
 *               @xmid.</listitem>
 *   </varlistentry>
 *   <varlistentry>
 *     <term>@yxmax</term>
 *     <listitem>Last value, i.e. the ordinate (@y) value at @xmax.</listitem>
 *   </varlistentry>
 * </variablelist>
 *
 * Since the data fitted in different situations can differ by many orders of
 * mangnitude, a rough estimate based on the data ranges is often much better
 * than constant-value estimates.  For instance setting the centre of a
 * Gaussian to @xmid and its half-width to (@xmax-@xmin)/10 with no regard to
 * the @y values will provide the user at least something to tune, and possibly
 * even an estimate from which the fit can converge.
 **/

/**
 * GwyUserFitFunc:
 *
 * Object represnting a user fitting function.
 *
 * The #GwyUserFitFunc struct contains private data only and should be
 * accessed using the functions below.
 **/

/**
 * GwyUserFitFuncClass:
 *
 * Class of user fitting functions.
 *
 * #GwyUserFitFuncClass does not contain any public members.
 **/

/**
 * GwyFitParam:
 * @name: Name, it must be a valid identifier (UTF-8 letters such as α are
 *        permitted).
 * @estimate: Initial parameter estimate (a formula that can contain the
 *            estimator variables).
 * @power_x: Power of the abscissa contained in the parameter.
 * @power_y: Power of the ordinate contained in the parameter.
 *
 * One parameter of a user fitting function.
 *
 * See gwy_fit_func_get_param_units() for a discussion of @power_x, @power_y
 * and choosing good parametrisation with respect to units.
 **/

/**
 * GWY_TYPE_FIT_PARAM:
 *
 * The #GType for a boxed type holding a #GwyFitParam.
 **/

/**
 * gwy_user_fit_func_duplicate:
 * @userfitfunc: A user fitting function.
 *
 * Duplicates a user fitting function.
 *
 * This is a convenience wrapper of gwy_serializable_duplicate().
 **/

/**
 * gwy_user_fit_func_assign:
 * @dest: Destination user fitting function.
 * @src: Source user fitting function.
 *
 * Copies the value of a user fitting function.
 *
 * This is a convenience wrapper of gwy_serializable_assign().
 **/

/**
 * gwy_user_fit_funcs:
 *
 * Gets inventory with all the user fitting functions.
 *
 * Returns: User fitting function inventory.
 **/

/**
 * gwy_user_fit_funcs_get:
 * @name: User fitting function name.  May be %NULL to get the default
 *        function.
 *
 * Convenience function to get a user fitting function from
 * gwy_user_fit_funcs() by name.
 *
 * Returns: User fitting function identified by @name or the default user
 *          fitting function if @name does not exist.
 **/

/**
 * GWY_USER_FIT_FUNC_ERROR:
 *
 * Error domain for user-defined fitting function manipulation.  Errors in this
 * domain will be from the #GwyUserFitFuncError enumeration. See #GError for
 * information on error domains.
 **/

/**
 * GwyUserFitFuncError:
 * @GWY_USER_FIT_FUNC_ERROR_NO_PARAM: Function formula does not contain any
 *                                    parameters.
 * @GWY_USER_FIT_FUNC_ERROR_ESTIMATE: Estimate formula contains unknown
 *                                    quantities.
 *
 * Error codes returned by user-defined fitting function manipulation.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
