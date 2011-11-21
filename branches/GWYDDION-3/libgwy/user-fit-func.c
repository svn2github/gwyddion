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
#include <stdlib.h>
#include <glib/gi18n-lib.h>
#include "libgwy/macros.h"
#include "libgwy/strfuncs.h"
#include "libgwy/math.h"
#include "libgwy/unit.h"
#include "libgwy/expr.h"
#include "libgwy/serialize.h"
#include "libgwy/user-fit-func.h"
#include "libgwy/object-internal.h"
#include "libgwy/fit-func-builtin.h"

enum { N_ITEMS = 3 };

struct _GwyUserFitFuncPrivate {
    gchar *formula;
    gchar *filter;
    guint nparams;
    GwyFitParam **param;
};

typedef struct _GwyUserFitFuncPrivate UserFitFunc;

static void         gwy_user_fit_func_finalize         (GObject *object);
static void         gwy_user_fit_func_serializable_init(GwySerializableInterface *iface);
static gsize        gwy_user_fit_func_n_items          (GwySerializable *serializable);
static gsize        gwy_user_fit_func_itemize          (GwySerializable *serializable,
                                                        GwySerializableItems *items);
static gboolean     gwy_user_fit_func_construct        (GwySerializable *serializable,
                                                        GwySerializableItems *items,
                                                        GwyErrorList **error_list);
static GObject*     gwy_user_fit_func_duplicate_impl   (GwySerializable *serializable);
static void         gwy_user_fit_func_assign_impl      (GwySerializable *destination,
                                                        GwySerializable *source);
static GwyResource* gwy_user_fit_func_copy             (GwyResource *resource);
static void         gwy_user_fit_func_changed          (GwyUserFitFunc *userfitfunc);
static gboolean     validate                           (GwyUserFitFunc *userfitfunc,
                                                        guint domain,
                                                        guint code,
                                                        GError **error);
static gchar*       gwy_user_fit_func_dump             (GwyResource *resource);
static gboolean     gwy_user_fit_func_parse            (GwyResource *resource,
                                                        GwyStrLineIter *iter,
                                                        GError **error);

static const GwySerializableItem serialize_items[N_ITEMS] = {
    /*0*/ { .name = "formula",    .ctype = GWY_SERIALIZABLE_STRING,       },
    /*1*/ { .name = "filter",     .ctype = GWY_SERIALIZABLE_STRING,       },
    /*2*/ { .name = "param",      .ctype = GWY_SERIALIZABLE_OBJECT_ARRAY, },
};

static GwyExpr *test_expr = NULL;
G_LOCK_DEFINE_STATIC(test_expr);

G_DEFINE_TYPE_EXTENDED
    (GwyUserFitFunc, gwy_user_fit_func, GWY_TYPE_RESOURCE, 0,
     GWY_IMPLEMENT_SERIALIZABLE(gwy_user_fit_func_serializable_init));

static GwySerializableInterface *parent_serializable = NULL;

static void
gwy_user_fit_func_serializable_init(GwySerializableInterface *iface)
{
    parent_serializable = g_type_interface_peek_parent(iface);
    iface->n_items   = gwy_user_fit_func_n_items;
    iface->itemize   = gwy_user_fit_func_itemize;
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
    for (unsigned i = 0; i < priv->nparams; i++)
        GWY_OBJECT_UNREF(priv->param[i]);
    GWY_FREE(priv->param);
    priv->nparams = 0;
}

static void
assign_params(UserFitFunc *priv,
              guint nparams,
              GwyFitParam **param)
{
    if (priv->nparams == nparams) {
        for (guint i = 0; i < nparams; i++)
            gwy_fit_param_assign(priv->param[i], param[i]);
    }
    else {
        // The number of params differ, don't bother.
        free_params(priv);
        priv->param = g_new(GwyFitParam*, nparams);
        priv->nparams = nparams;
        for (guint i = 0; i < nparams; i++)
            priv->param[i] = gwy_fit_param_duplicate(param[i]);
    }
}

static GwyFitParam*
default_param(void)
{
    return gwy_fit_param_new_set("a", 0, 1, NULL);
}

static void
gwy_user_fit_func_init(GwyUserFitFunc *userfitfunc)
{
    userfitfunc->priv = G_TYPE_INSTANCE_GET_PRIVATE(userfitfunc,
                                                    GWY_TYPE_USER_FIT_FUNC,
                                                    UserFitFunc);
    UserFitFunc *priv = userfitfunc->priv;

    // Constant function, by default.
    priv->formula = g_strdup("a");
    priv->nparams = 1;
    priv->param = g_new(GwyFitParam*, 1);
    priv->param[0] = default_param();
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
    GwyUserFitFunc *userfitfunc = GWY_USER_FIT_FUNC(serializable);
    UserFitFunc *priv = userfitfunc->priv;
    gsize n = N_ITEMS+1;
    for (guint i = 0; i < priv->nparams; i++)
        n += gwy_serializable_n_items(GWY_SERIALIZABLE(priv->param[i]));
    n += parent_serializable->n_items(serializable);
    return n;
}

static gsize
gwy_user_fit_func_itemize(GwySerializable *serializable,
                          GwySerializableItems *items)
{
    g_return_val_if_fail(items->len - items->n >= N_ITEMS+1, 0);

    GwyUserFitFunc *userfitfunc = GWY_USER_FIT_FUNC(serializable);
    UserFitFunc *priv = userfitfunc->priv;
    GwySerializableItem *it = items->items + items->n;
    guint nn = 0;

    // Our own data
    *it = serialize_items[0];
    it->value.v_string = priv->formula;
    it++, items->n++, nn++;

    if (priv->filter) {
        *it = serialize_items[1];
        it->value.v_string = priv->filter;
        it++, items->n++, nn++;
    }

    *it = serialize_items[2];
    it->value.v_object_array = (GObject**)priv->param;
    it->array_size = priv->nparams;
    it++, items->n++, nn++;

    for (guint i = 0; i < priv->nparams; i++) {
        g_return_val_if_fail(items->len - items->n, 0);
        gwy_serializable_itemize(GWY_SERIALIZABLE(priv->param[i]), items);
    }

    return _gwy_itemize_chain_to_parent(serializable, GWY_TYPE_RESOURCE,
                                        parent_serializable, items, nn);
}

static gboolean
gwy_user_fit_func_construct(GwySerializable *serializable,
                            GwySerializableItems *items,
                            GwyErrorList **error_list)
{
    GwySerializableItem its[N_ITEMS];
    memcpy(its, serialize_items, sizeof(serialize_items));
    GwySerializableItems parent_items;
    if (gwy_deserialize_filter_items(its, N_ITEMS, items, &parent_items,
                                     "GwyUserFitFunc", error_list)) {
        if (!parent_serializable->construct(serializable, &parent_items,
                                            error_list))
            goto fail;
    }
    gsize n = its[2].array_size;

    // Our own data
    GwyUserFitFunc *userfitfunc = GWY_USER_FIT_FUNC(serializable);
    UserFitFunc *priv = userfitfunc->priv;

    if (!n || n > 256) {
        gwy_error_list_add(error_list, GWY_DESERIALIZE_ERROR,
                           GWY_DESERIALIZE_ERROR_INVALID,
                           _("Invalid number of user fitting function "
                             "parameters: %lu."),
                           (gulong)n);
        goto fail;
    }
    if (!_gwy_check_object_component(its + 2, userfitfunc,
                                     GWY_TYPE_FIT_PARAM, error_list))
        goto fail;

    GWY_TAKE_STRING(priv->formula, its[0].value.v_string);
    GWY_TAKE_STRING(priv->filter, its[1].value.v_string);

    free_params(priv);
    priv->param = (GwyFitParam**)its[2].value.v_object_array;
    priv->nparams = n;
    its[2].value.v_object_array = NULL;

    GError *err = NULL;
    if (validate(userfitfunc,
                 GWY_DESERIALIZE_ERROR, GWY_DESERIALIZE_ERROR_INVALID, &err))
        return TRUE;

    gwy_error_list_propagate(error_list, err);

fail:
    return FALSE;
}

static void
assign_info(UserFitFunc *dpriv,
            const UserFitFunc *spriv)
{
    _gwy_assign_string(&dpriv->formula, spriv->formula);
    _gwy_assign_string(&dpriv->filter, spriv->filter);
}

static GObject*
gwy_user_fit_func_duplicate_impl(GwySerializable *serializable)
{
    GwyUserFitFunc *userfitfunc = GWY_USER_FIT_FUNC(serializable);
    UserFitFunc *priv = userfitfunc->priv;
    GwyUserFitFunc *duplicate = g_object_newv(GWY_TYPE_USER_FIT_FUNC, 0, NULL);
    UserFitFunc *dpriv = duplicate->priv;

    parent_serializable->assign(GWY_SERIALIZABLE(duplicate), serializable);
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
    parent_serializable->assign(destination, source);

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

static gboolean
validate(GwyUserFitFunc *userfitfunc,
         guint domain, guint code,
         GError **error)
{
    UserFitFunc *priv = userfitfunc->priv;

    // Formula physical sanity
    if (!priv->formula) {
        g_set_error(error, domain, code,
                    _("Fitting function has no formula."));
        return FALSE;
    }
    if (!g_utf8_validate(priv->formula, -1, NULL)) {
        g_set_error(error, domain, code,
                    _("Fitting function formula is not valid UTF-8."));
        return FALSE;
    }

    // Filter physical sanity
    if (priv->filter && !g_utf8_validate(priv->filter, -1, NULL)) {
        g_set_error(error, domain, code,
                    _("Fitting function filter is not valid UTF-8."));
        return FALSE;
    }

    // Params physical sanity
    if (!priv->nparams) {
        g_set_error(error, domain, code,
                    _("Fitting function has no parameters."));
        return FALSE;
    }
    guint n = priv->nparams;
    for (guint i = 0; i < n; i++) {
        if (!priv->param[i]) {
            g_set_error(error, domain, code,
                        _("Fitting function parameter %u is missing."),
                        i+1);
            return FALSE;
        }
    }

    // Parameters
    for (guint i = 0; i < n; i++) {
        const gchar *namei = gwy_fit_param_get_name(priv->param[i]);
        for (guint j = i+1; j < n; j++) {
            const gchar *namej = gwy_fit_param_get_name(priv->param[j]);
            if (gwy_strequal(namei, namej)) {
                g_set_error(error, domain, code,
                            _("Fitting function has duplicate parameters."));
                return FALSE;
            }
        }
    }

    G_LOCK(test_expr);
    gboolean ok = FALSE;

    // Formula
    if (!test_expr)
        test_expr = _gwy_fit_func_new_expr_with_constants();
    if (!gwy_expr_compile(test_expr, priv->formula, NULL)) {
        g_set_error(error, domain, code,
                    _("Fitting function formula is invalid."));
        goto fail;
    }
    if (gwy_user_fit_func_resolve_params(userfitfunc, test_expr, NULL, NULL)) {
        g_set_error(error, domain, code,
                    _("Fitting function parameters do not match the formula."));
        goto fail;
    }

    // Filter
    if (priv->filter && strlen(priv->filter)) {
        if (!gwy_expr_compile(test_expr, priv->filter, NULL)) {
            g_set_error(error, domain, code,
                        _("Fitting function filter is invalid."));
            goto fail;
        }
        const gchar *names[1] = { "x" };
        guint indices[1];
        if (gwy_expr_resolve_variables(test_expr, 1, names, indices)) {
            g_set_error(error, domain, code,
                        _("Fitting function filter is invalid."));
            goto fail;
        }
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
 * Returns: (transfer full):
 *          A new free-standing user fitting function.
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
    if (!test_expr)
        test_expr = _gwy_fit_func_new_expr_with_constants();

    if (!gwy_expr_compile(test_expr, formula, error)) {
        G_UNLOCK(test_expr);
        return FALSE;
    }
    const gchar **names;
    guint n = gwy_expr_get_variables(test_expr, &names);
    // This is usualy still two parameters too large as the vars contain "x".
    GwyFitParam **newparam = g_new0(GwyFitParam*, n);
    guint np = 0;
    for (guint i = 1; i < n; i++) {
        if (gwy_strequal(names[i], "x"))
            continue;

        guint j = 0;
        while (j < priv->nparams) {
            if (gwy_strequal(names[i], gwy_fit_param_get_name(priv->param[j])))
                break;
            j++;
        }
        if (j == priv->nparams)
            newparam[np] = gwy_fit_param_new(names[i]);
        else
            newparam[np] = g_object_ref(priv->param[j]);
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
    _gwy_assign_string(&priv->formula, formula);
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
 * Returns: (transfer none):
 *          The parameter, owned by @userfitfunc.  You can modify
 *          to change the parameter properties.  If the formula changes
 *          the parameter object may become invalid.
 **/
GwyFitParam*
gwy_user_fit_func_param(GwyUserFitFunc *userfitfunc,
                        const gchar *name)
{
    g_return_val_if_fail(GWY_IS_USER_FIT_FUNC(userfitfunc), NULL);
    g_return_val_if_fail(name, NULL);
    UserFitFunc *priv = userfitfunc->priv;
    for (guint i = 0; i < priv->nparams; i++) {
        if (gwy_strequal(name, gwy_fit_param_get_name(priv->param[i])))
            return priv->param[i];
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
 * Returns: (transfer none):
 *          The parameter, owned by @userfitfunc.  You can modify
 *          to change the parameter properties.  If the formula changes
 *          the parameter object may become invalid.
 **/
GwyFitParam*
gwy_user_fit_func_nth_param(GwyUserFitFunc *userfitfunc,
                            guint i)
{
    g_return_val_if_fail(GWY_IS_USER_FIT_FUNC(userfitfunc), NULL);
    UserFitFunc *priv = userfitfunc->priv;
    g_return_val_if_fail(i < priv->nparams, NULL);
    return priv->param[i];
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
        names[i] = gwy_fit_param_get_name(priv->param[i]);
    names[n] = independent_name;
    if (!indices)
        indices = g_newa(guint, n+1);

    return gwy_expr_resolve_variables(expr, n+1, names, indices);
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
        GwyFitParam *param = priv->param[i];
        g_string_append_printf(text, "param %s\n",
                               gwy_fit_param_get_name(param));
        g_string_append_printf(text, "power_x %d\n",
                               gwy_fit_param_get_power_x(param));
        g_string_append_printf(text, "power_y %d\n",
                               gwy_fit_param_get_power_y(param));
        const gchar *s = gwy_fit_param_get_estimate(param);
        if (s)
            g_string_append_printf(text, "estimate %s\n", s);
    }
    return g_string_free(text, FALSE);;
}

static gboolean
gwy_user_fit_func_parse(GwyResource *resource,
                        GwyStrLineIter *iter,
                        GError **error)
{
    GwyUserFitFunc *userfitfunc = GWY_USER_FIT_FUNC(resource);
    UserFitFunc *priv = userfitfunc->priv;
    GwyFitParam *param = NULL;
    GPtrArray *params = NULL;

    while (TRUE) {
        GwyResourceLineType line;
        gchar *key, *value;

        line = gwy_resource_parse_param_line(iter, &key, &value, error);
        if (line == GWY_RESOURCE_LINE_NONE)
            break;
        if (line != GWY_RESOURCE_LINE_OK)
            goto fail;

        if (gwy_strequal(key, "param")) {
            param = gwy_fit_param_new(value);
            if (param) {
                if (!params)
                    params = g_ptr_array_new();
                g_ptr_array_add(params, param);
            }
            else {
                g_set_error(error, GWY_RESOURCE_ERROR, GWY_RESOURCE_ERROR_DATA,
                            _("Parameter %u has invalid name."),
                            (guint)params->len+1);
                goto fail;
            }
        }
        else if (params) {
            if (param) {
                if (gwy_strequal(key, "power_x"))
                    gwy_fit_param_set_power_x(param, strtol(value, NULL, 10));
                else if (gwy_strequal(key, "power_y"))
                    gwy_fit_param_set_power_y(param, strtol(value, NULL, 10));
                else if (gwy_strequal(key, "estimate")
                         && gwy_fit_param_check_estimate(value, NULL))
                    gwy_fit_param_set_estimate(param, value);
                else
                   g_warning("Ignoring unknown GwyUserFitFunc key ‘%s’.", key);
            }
        }
        else {
            if (gwy_strequal(key, "formula"))
                _gwy_assign_string(&priv->formula, value);
            else if (gwy_strequal(key, "filter"))
                _gwy_assign_string(&priv->filter, value);
            else
                g_warning("Ignoring unknown GwyUserFitFunc key ‘%s’.", key);
        }
    }

    free_params(priv);
    priv->nparams = params->len;
    priv->param = (GwyFitParam**)g_ptr_array_free(params, FALSE);

    if (!validate(userfitfunc,
                  GWY_RESOURCE_ERROR, GWY_RESOURCE_ERROR_DATA, error))
        return FALSE;

    return TRUE;

fail:
    if (params) {
        for (guint i = 0; i < params->len; i++)
            GWY_OBJECT_UNREF(g_ptr_array_index(params, i));
        g_ptr_array_free(params, TRUE);
    }

    return FALSE;
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
 * other properties can be manipulated after obtaining the parameter with
 * gwy_user_fit_func_param() or gwy_user_fit_func_nth_param().
 *
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
 *   <varlistentry>
 *     <term>@xpeak</term>
 *     <listitem>Peak position, i.e. the abscissa (@x) of peak
 *               centre.</listitem>
 *   </varlistentry>
 *   <varlistentry>
 *     <term>@apeak</term>
 *     <listitem>Peak height, i.e. the difference between peak extreme value
 *               (@y) and <quote>background</quote>.  It is positive for upward
 *               peaks and negative for downward peaks.</listitem>
 *   </varlistentry>
 *   <varlistentry>
 *     <term>@hwpeak</term>
 *     <listitem>Peak half-width at half-maximum (or half-minimum for downward
                 peaks).</listitem>
 *   </varlistentry>
 *   <varlistentry>
 *     <term>@y0peak</term>
 *     <listitem>Peak background value (@y).  It is usually equal to @ymin for
 *               upward peaks and to @ymax for downward peak.</listitem>
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
 *
 * Error codes returned by user-defined fitting function manipulation.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
