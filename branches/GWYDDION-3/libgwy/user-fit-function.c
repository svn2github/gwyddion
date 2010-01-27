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
#include "libgwy/serialize.h"
#include "libgwy/user-fit-function.h"
#include "libgwy/libgwy-aliases.h"
#include "libgwy/object-internal.h"

enum { N_ITEMS = 6 };

struct _GwyUserFitFunctionPrivate {
    guint nparams;
    GwyUserFitFunctionParam *param;
    gchar *formula;
    gchar *filter;
};

typedef struct _GwyUserFitFunctionPrivate UserFitFunction;

static void         gwy_user_fit_function_serializable_init(GwySerializableInterface *iface);
static gsize        gwy_user_fit_function_n_items          (GwySerializable *serializable);
static gsize        gwy_user_fit_function_itemize          (GwySerializable *serializable,
                                                            GwySerializableItems *items);
static gboolean     gwy_user_fit_function_construct        (GwySerializable *serializable,
                                                            GwySerializableItems *items,
                                                            GwyErrorList **error_list);
static GObject*     gwy_user_fit_function_duplicate_impl   (GwySerializable *serializable);
static void         gwy_user_fit_function_assign_impl      (GwySerializable *destination,
                                                            GwySerializable *source);
static GwyResource* gwy_user_fit_function_copy             (GwyResource *resource);
static void         gwy_user_fit_function_changed          (GwyUserFitFunction *userfitfunction);
static gchar*       gwy_user_fit_function_dump             (GwyResource *resource);
static gboolean     gwy_user_fit_function_parse            (GwyResource *resource,
                                                            gchar *text,
                                                            GError **error);

static const GwySerializableItem serialize_items[N_ITEMS] = {
    /*0*/ { .name = "formula",  .ctype = GWY_SERIALIZABLE_STRING,       },
    /*1*/ { .name = "filter",   .ctype = GWY_SERIALIZABLE_STRING,       },
    /*2*/ { .name = "param",    .ctype = GWY_SERIALIZABLE_STRING_ARRAY, },
    /*3*/ { .name = "x-power",  .ctype = GWY_SERIALIZABLE_INT32_ARRAY,  },
    /*4*/ { .name = "y-power",  .ctype = GWY_SERIALIZABLE_INT32_ARRAY,  },
    /*5*/ { .name = "estimate", .ctype = GWY_SERIALIZABLE_STRING_ARRAY, },
};

G_DEFINE_TYPE_EXTENDED
    (GwyUserFitFunction, gwy_user_fit_function, GWY_TYPE_RESOURCE, 0,
     GWY_IMPLEMENT_SERIALIZABLE(gwy_user_fit_function_serializable_init))

GwySerializableInterface *gwy_user_fit_function_parent_serializable = NULL;

static void
gwy_user_fit_function_serializable_init(GwySerializableInterface *iface)
{
    gwy_user_fit_function_parent_serializable = g_type_interface_peek_parent(iface);
    iface->n_items   = gwy_user_fit_function_n_items;
    iface->itemize   = gwy_user_fit_function_itemize;
    iface->construct = gwy_user_fit_function_construct;
    iface->duplicate = gwy_user_fit_function_duplicate_impl;
    iface->assign    = gwy_user_fit_function_assign_impl;
}

static void
gwy_user_fit_function_class_init(GwyUserFitFunctionClass *klass)
{
    //GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GwyResourceClass *res_class = GWY_RESOURCE_CLASS(klass);

    g_type_class_add_private(klass, sizeof(UserFitFunction));

    res_class->copy = gwy_user_fit_function_copy;
    res_class->dump = gwy_user_fit_function_dump;
    res_class->parse = gwy_user_fit_function_parse;

    gwy_resource_class_register(res_class, "fitfunctions", NULL);
}

static void
gwy_user_fit_function_init(GwyUserFitFunction *userfitfunction)
{
    userfitfunction->priv = G_TYPE_INSTANCE_GET_PRIVATE(userfitfunction,
                                                        GWY_TYPE_USER_FIT_FUNCTION,
                                                        UserFitFunction);
    //TODO *userfitfunction->priv = opengl_default;
}

static gsize
gwy_user_fit_function_n_items(G_GNUC_UNUSED GwySerializable *serializable)
{
    return N_ITEMS;
}

static gsize
gwy_user_fit_function_itemize(GwySerializable *serializable,
                              GwySerializableItems *items)
{

    GwyUserFitFunction *userfitfunction = GWY_USER_FIT_FUNCTION(serializable);
    UserFitFunction *function = userfitfunction->priv;
    GwySerializableItem it;

    // Our own data

    // Chain to parent
    g_return_val_if_fail(items->len - items->n, 0);
    it.ctype = GWY_SERIALIZABLE_PARENT;
    it.name = g_type_name(GWY_TYPE_RESOURCE);
    it.array_size = 0;
    it.value.v_type = GWY_TYPE_RESOURCE;
    items->items[items->n++] = it;

    guint n;
    if ((n = gwy_user_fit_function_parent_serializable->itemize(serializable, items)))
        return N_ITEMS+1 + n;
    return 0;
}

static gboolean
gwy_user_fit_function_construct(GwySerializable *serializable,
                                GwySerializableItems *items,
                                GwyErrorList **error_list)
{
    GwySerializableItem its[N_ITEMS];
    memcpy(its, serialize_items, sizeof(serialize_items));
    gsize np = gwy_deserialize_filter_items(its, N_ITEMS, items,
                                            "GwyUserFitFunction",
                                            error_list);
    // Chain to parent
    if (np < items->n) {
        np++;
        GwySerializableItems parent_items = {
            items->len - np, items->n - np, items->items + np
        };
        if (!gwy_user_fit_function_parent_serializable->construct(serializable,
                                                            &parent_items,
                                                            error_list))
            goto fail;
    }

    // Our own data
    GwyUserFitFunction *userfitfunction = GWY_USER_FIT_FUNCTION(serializable);
    UserFitFunction *function = userfitfunction->priv;

    //gwy_user_fit_function_sanitize(userfitfunction);

    return TRUE;

fail:
    return FALSE;
}

static GObject*
gwy_user_fit_function_duplicate_impl(GwySerializable *serializable)
{
    GwyUserFitFunction *userfitfunction = GWY_USER_FIT_FUNCTION(serializable);
    GwyUserFitFunction *duplicate = g_object_newv(GWY_TYPE_USER_FIT_FUNCTION, 0, NULL);

    gwy_user_fit_function_parent_serializable->assign(GWY_SERIALIZABLE(duplicate),
                                                 serializable);
    duplicate->priv = userfitfunction->priv;

    return G_OBJECT(duplicate);
}

static void
gwy_user_fit_function_assign_impl(GwySerializable *destination,
                            GwySerializable *source)
{
    GwyUserFitFunction *dest = GWY_USER_FIT_FUNCTION(destination);
    GwyUserFitFunction *src = GWY_USER_FIT_FUNCTION(source);
    gboolean emit_changed = FALSE;

    g_object_freeze_notify(G_OBJECT(dest));
    gwy_user_fit_function_parent_serializable->assign(destination, source);
    /*
    if (memcmp(dest->priv, src->priv, sizeof(UserFitFunction)) != 0) {
        dest->priv = src->priv;
        emit_changed = TRUE;
    }
    */
    g_object_thaw_notify(G_OBJECT(dest));
    if (emit_changed)
        gwy_user_fit_function_changed(dest);
}

static GwyResource*
gwy_user_fit_function_copy(GwyResource *resource)
{
    return GWY_RESOURCE(gwy_user_fit_function_duplicate_impl(GWY_SERIALIZABLE(resource)));
}

static void
gwy_user_fit_function_changed(GwyUserFitFunction *userfitfunction)
{
    gwy_resource_data_changed(GWY_RESOURCE(userfitfunction));
}

/**
 * gwy_user_fit_function_new:
 *
 * Creates a new GL function.
 *
 * Returns: A new free-standing GL function.
 **/
GwyUserFitFunction*
gwy_user_fit_function_new(void)
{
    return g_object_newv(GWY_TYPE_USER_FIT_FUNCTION, 0, NULL);
}

const gchar*
gwy_user_fit_function_get_expression(GwyUserFitFunction *userfitfunction)
{
    g_return_val_if_fail(GWY_IS_USER_FIT_FUNCTION(userfitfunction), NULL);
}

const GwyUserFitFunctionParam*
gwy_user_fit_function_get_params(GwyUserFitFunction *userfitfunction,
                                 guint *nparams)
{
    g_return_val_if_fail(GWY_IS_USER_FIT_FUNCTION(userfitfunction), NULL);
    UserFitFunction *priv = userfitfunction->priv;
    GWY_MAYBE_SET(nparams, priv->nparams);
    return priv->param;
}

static gchar*
gwy_user_fit_function_dump(GwyResource *resource)
{
    GwyUserFitFunction *userfitfunction = GWY_USER_FIT_FUNCTION(resource);
    /*
    UserFitFunction *function = userfitfunction->priv;
    gchar *amb, *dif, *spec, *emi;

    amb = gwy_resource_dump_data_line((gdouble*)&function->ambient, 4);
    dif = gwy_resource_dump_data_line((gdouble*)&function->diffuse, 4);
    spec = gwy_resource_dump_data_line((gdouble*)&function->specular, 4);
    emi = gwy_resource_dump_data_line((gdouble*)&function->emission, 4);

    gchar buffer[G_ASCII_DTOSTR_BUF_SIZE];
    g_ascii_formatd(buffer, sizeof(buffer), "%g", function->shininess);

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
gwy_user_fit_function_parse(GwyResource *resource,
                      gchar *text,
                      G_GNUC_UNUSED GError **error)
{
    GwyUserFitFunction *userfitfunction = GWY_USER_FIT_FUNCTION(resource);

    /*
    if (!parse_component(&text, &userfitfunction->priv->ambient)
        || !parse_component(&text, &userfitfunction->priv->diffuse)
        || !parse_component(&text, &userfitfunction->priv->specular)
        || !parse_component(&text, &userfitfunction->priv->emission))
        return FALSE;

    gchar *end;
    userfitfunction->priv->shininess = g_ascii_strtod(text, &end);
    if (end == text)
        return FALSE;

    gwy_user_fit_function_sanitize(userfitfunction);
    */
    return TRUE;
}

#define __LIBGWY_USER_FIT_FUNCTION_C__
#include "libgwy/libgwy-aliases.c"

/************************** Documentation ****************************/

/**
 * SECTION: user-fit-function
 * @title: GwyUserFitFunction
 * @short_description: User-defined fitting function
 *
 * #GwyUserFitFunction represents a user-defined fitting function.
 **/

/**
 * GwyUserFitFunction:
 *
 * Object represnting a user fitting function.
 *
 * The #GwyUserFitFunction struct contains private data only and should be
 * accessed using the functions below.
 **/

/**
 * GwyUserFitFunctionClass:
 *
 * Class of user fitting functions.
 *
 * #GwyUserFitFunctionClass does not contain any public members.
 **/

/**
 * gwy_user_fit_function_duplicate:
 * @userfitfunction: A user fitting function.
 *
 * Duplicates a user fitting function.
 *
 * This is a convenience wrapper of gwy_serializable_duplicate().
 **/

/**
 * gwy_user_fit_function_assign:
 * @dest: Destination user fitting function.
 * @src: Source user fitting function.
 *
 * Copies the value of a user fitting function.
 *
 * This is a convenience wrapper of gwy_serializable_assign().
 **/

/**
 * gwy_user_fit_functions:
 *
 * Gets inventory with all the user fitting functions.
 *
 * Returns: User fitting function inventory.
 **/

/**
 * gwy_user_fit_functions_get:
 * @name: User fitting function name.  May be %NULL to get the default
 *        function.
 *
 * Convenience function to get a user fitting function from
 * gwy_user_fit_functions() by name.
 *
 * Returns: User fitting function identified by @name or the default user
 *          fitting function if @name does not exist.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
