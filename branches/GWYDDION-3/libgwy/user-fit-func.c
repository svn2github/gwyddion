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
#include <glib/gi18n-lib.h>
#include "libgwy/macros.h"
#include "libgwy/strfuncs.h"
#include "libgwy/expr.h"
#include "libgwy/serialize.h"
#include "libgwy/user-fit-func.h"
#include "libgwy/libgwy-aliases.h"
#include "libgwy/object-internal.h"

enum { N_ITEMS = 6 };

struct _GwyUserFitFuncPrivate {
    gchar *expression;
    gchar *filter;
    guint nparams;
    GwyUserFitFuncParam *param;

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
static gboolean     gwy_user_fit_func_validate         (GwyUserFitFunc *userfitfunc);
static gchar*       gwy_user_fit_func_dump             (GwyResource *resource);
static gboolean     gwy_user_fit_func_parse            (GwyResource *resource,
                                                        gchar *text,
                                                        GError **error);

static const GwyUserFitFuncParam default_param[1] = {
    { .name = "a", .power_x = 0, .power_y = 1, .estimate = "", },
};

static const GwySerializableItem serialize_items[N_ITEMS] = {
    /*0*/ { .name = "expression", .ctype = GWY_SERIALIZABLE_STRING,       },
    /*1*/ { .name = "filter",     .ctype = GWY_SERIALIZABLE_STRING,       },
    /*2*/ { .name = "param",      .ctype = GWY_SERIALIZABLE_STRING_ARRAY, },
    /*3*/ { .name = "x-power",    .ctype = GWY_SERIALIZABLE_INT32_ARRAY,  },
    /*4*/ { .name = "y-power",    .ctype = GWY_SERIALIZABLE_INT32_ARRAY,  },
    /*5*/ { .name = "estimate",   .ctype = GWY_SERIALIZABLE_STRING_ARRAY, },
};

G_DEFINE_TYPE_EXTENDED
    (GwyUserFitFunc, gwy_user_fit_func, GWY_TYPE_RESOURCE, 0,
     GWY_IMPLEMENT_SERIALIZABLE(gwy_user_fit_func_serializable_init))

GwySerializableInterface *gwy_user_fit_func_parent_serializable = NULL;

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
              const GwyUserFitFuncParam *param)
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
        priv->param = g_new(GwyUserFitFuncParam, nparams);
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
    priv->expression = g_strdup("a");
    priv->filter = g_strdup("");
    assign_params(priv, G_N_ELEMENTS(default_param), default_param);
}

static void
gwy_user_fit_func_finalize(GObject *object)
{
    GwyUserFitFunc *userfitfunc = GWY_USER_FIT_FUNC(object);
    UserFitFunc *priv = userfitfunc->priv;
    GWY_FREE(priv->expression);
    GWY_FREE(priv->filter);
    free_params(priv);
    G_OBJECT_CLASS(gwy_user_fit_func_parent_class)->finalize(object);
}

static gsize
gwy_user_fit_func_n_items(G_GNUC_UNUSED GwySerializable *serializable)
{
    return N_ITEMS;
}

static void
serialize_params(UserFitFunc *priv)
{
    guint n = priv->nparams;
    priv->serialize_names = g_new(gchar*, n);
    priv->serialize_x_powers = g_new(gint, n);
    priv->serialize_y_powers = g_new(gint, n);
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
    it->value.v_string = priv->expression;
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

    GWY_FREE(priv->serialize_names);
    GWY_FREE(priv->serialize_x_powers);
    GWY_FREE(priv->serialize_y_powers);
    GWY_FREE(priv->serialize_estimates);
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

    guint n = its[2].array_size;
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

    priv->param = g_new(GwyUserFitFuncParam, n);
    priv->nparams = n;
    for (guint i = 0; i < n; i++) {
        GwyUserFitFuncParam *param = priv->param + i;
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

    gboolean ok = gwy_user_fit_func_validate(userfitfunc);
    if (!ok) {
        gwy_error_list_add(error_list, GWY_DESERIALIZE_ERROR,
                           GWY_DESERIALIZE_ERROR_INVALID,
                           _("Invalid expression or parameters."));
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
    GWY_FREE(dpriv->expression);
    dpriv->expression = g_strdup(spriv->expression);
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

static gboolean
gwy_user_fit_func_validate(GwyUserFitFunc *userfitfunc)
{
    static GwyExpr *test_expr = NULL;
    G_LOCK_DEFINE_STATIC(test_expr);

    UserFitFunc *priv = userfitfunc->priv;
    guint n = priv->nparams;
    for (guint i = 0; i < n; i++) {
        for (guint j = i+1; j < n; j++) {
            if (gwy_strequal(priv->param[i].name, priv->param[j].name))
                return FALSE;
        }
    }

    G_LOCK(test_expr);
    if (!test_expr) {
        test_expr = gwy_expr_new();
        gwy_expr_define_constant(test_expr, "pi", G_PI, NULL);
        gwy_expr_define_constant(test_expr, "π", G_PI, NULL);
    }
    if (!gwy_expr_compile(test_expr, priv->expression, NULL)) {
        G_UNLOCK(test_expr);
        return FALSE;
    }
    if (!gwy_user_fit_func_resolve_params(userfitfunc, test_expr, "x", NULL)) {
        G_UNLOCK(test_expr);
        return FALSE;
    }
    // TODO: Validate estimators, filter
    G_UNLOCK(test_expr);

    return TRUE;
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

const gchar*
gwy_user_fit_func_get_expression(GwyUserFitFunc *userfitfunc)
{
    g_return_val_if_fail(GWY_IS_USER_FIT_FUNC(userfitfunc), NULL);
    return userfitfunc->priv->expression;
}

const GwyUserFitFuncParam*
gwy_user_fit_func_get_params(GwyUserFitFunc *userfitfunc,
                             guint *nparams)
{
    g_return_val_if_fail(GWY_IS_USER_FIT_FUNC(userfitfunc), NULL);
    UserFitFunc *priv = userfitfunc->priv;
    GWY_MAYBE_SET(nparams, priv->nparams);
    return priv->param;
}

/**
 * gwy_user_fit_func_resolve_params:
 * @userfitfunc: A user fitting function.
 * @expr: An expression, presumably with compiled @userfitfunc's formula.
 * @independent_name: Name of independent variable (abscissa).
 * @indices: Array to store the map from the parameter number to the
 *           expression variable number.  The abscissa goes last after all
 *           parameters.
 *
 * Resolves the mapping between paramters and expression variables for a
 * user fitting function.
 *
 * See gwy_expr_resolve() for some discussion.
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
    g_return_val_if_fail(independent_name, G_MAXUINT);
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

static gchar*
gwy_user_fit_func_dump(GwyResource *resource)
{
    GwyUserFitFunc *userfitfunc = GWY_USER_FIT_FUNC(resource);
    /*
    UserFitFunc *priv = userfitfunc->priv;
    gchar *amb, *dif, *spec, *emi;

    amb = gwy_resource_dump_data_line((gdouble*)&priv->ambient, 4);
    dif = gwy_resource_dump_data_line((gdouble*)&priv->diffuse, 4);
    spec = gwy_resource_dump_data_line((gdouble*)&priv->specular, 4);
    emi = gwy_resource_dump_data_line((gdouble*)&priv->emission, 4);

    gchar buffer[G_ASCII_DTOSTR_BUF_SIZE];
    g_ascii_formatd(buffer, sizeof(buffer), "%g", priv->shininess);

    gchar *retval = g_strjoin("\n", amb, dif, spec, emi, buffer, "", NULL);
    g_free(amb);
    g_free(dif);
    g_free(spec);
    g_free(emi);

    return retval;
    */
    return g_strdup("");
}

/*
static gboolean
parse_component(gchar **text, GwyRGBA *color)
{
    GwyResourceLineType ltype;
    do {
        gchar *line = gwy_str_next_line(text);
        if (!line)
            return FALSE;
        ltype = gwy_resource_parse_data_line(line, 4, (gdouble*)color);
        if (ltype == GWY_RESOURCE_LINE_OK)
            return TRUE;
    } while (ltype == GWY_RESOURCE_LINE_EMPTY);
    return FALSE;
}
*/

static gboolean
gwy_user_fit_func_parse(GwyResource *resource,
                      gchar *text,
                      G_GNUC_UNUSED GError **error)
{
    GwyUserFitFunc *userfitfunc = GWY_USER_FIT_FUNC(resource);

    /*
    if (!parse_component(&text, &userfitfunc->priv->ambient)
        || !parse_component(&text, &userfitfunc->priv->diffuse)
        || !parse_component(&text, &userfitfunc->priv->specular)
        || !parse_component(&text, &userfitfunc->priv->emission))
        return FALSE;

    gchar *end;
    userfitfunc->priv->shininess = g_ascii_strtod(text, &end);
    if (end == text)
        return FALSE;

    gwy_user_fit_func_sanitize(userfitfunc);
    */
    return TRUE;
}

#define __LIBGWY_USER_FIT_FUNC_C__
#include "libgwy/libgwy-aliases.c"

/************************** Documentation ****************************/

/**
 * SECTION: user-fit-func
 * @title: GwyUserFitFunc
 * @short_description: User-defined fitting function
 *
 * #GwyUserFitFunc represents a user-defined fitting function.
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

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
