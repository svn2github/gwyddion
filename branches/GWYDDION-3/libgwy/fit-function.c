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
#include "libgwy/fit-function.h"
#include "libgwy/libgwy-aliases.h"

enum { N_ITEMS = 1 };

// NB: Below we assume this is plain old data that can be bit-wise assigned.
// If this ceases to hold, the code must be reviewed.
struct _GwyFitFunctionPrivate {
};

typedef struct _GwyFitFunctionPrivate FitFunction;

static void         gwy_fit_function_serializable_init(GwySerializableInterface *iface);
static gsize        gwy_fit_function_n_items          (GwySerializable *serializable);
static gsize        gwy_fit_function_itemize          (GwySerializable *serializable,
                                                       GwySerializableItems *items);
static gboolean     gwy_fit_function_construct        (GwySerializable *serializable,
                                                       GwySerializableItems *items,
                                                       GwyErrorList **error_list);
static GObject*     gwy_fit_function_duplicate_impl   (GwySerializable *serializable);
static void         gwy_fit_function_assign_impl      (GwySerializable *destination,
                                                       GwySerializable *source);
static GwyResource* gwy_fit_function_copy             (GwyResource *resource);
static void         gwy_fit_function_changed          (GwyFitFunction *fit_function);
static void         gwy_fit_function_setup_inventory  (GwyInventory *inventory);
static gchar*       gwy_fit_function_dump             (GwyResource *resource);
static gboolean     gwy_fit_function_parse            (GwyResource *resource,
                                                       gchar *text,
                                                       GError **error);

static const GwySerializableItem serialize_items[N_ITEMS] = {
    /*0*/ { .name = "formula",   .ctype = GWY_SERIALIZABLE_STRING,  },
};

G_DEFINE_TYPE_EXTENDED
    (GwyFitFunction, gwy_fit_function, GWY_TYPE_RESOURCE, 0,
     GWY_IMPLEMENT_SERIALIZABLE(gwy_fit_function_serializable_init))

GwySerializableInterface *gwy_fit_function_parent_serializable = NULL;

static void
gwy_fit_function_serializable_init(GwySerializableInterface *iface)
{
    gwy_fit_function_parent_serializable = g_type_interface_peek_parent(iface);
    iface->n_items   = gwy_fit_function_n_items;
    iface->itemize   = gwy_fit_function_itemize;
    iface->construct = gwy_fit_function_construct;
    iface->duplicate = gwy_fit_function_duplicate_impl;
    iface->assign    = gwy_fit_function_assign_impl;
}

static void
gwy_fit_function_class_init(GwyFitFunctionClass *klass)
{
    GwyResourceClass *res_class = GWY_RESOURCE_CLASS(klass);

    g_type_class_add_private(klass, sizeof(FitFunction));

    res_class->setup_inventory = gwy_fit_function_setup_inventory;
    res_class->copy = gwy_fit_function_copy;
    res_class->dump = gwy_fit_function_dump;
    res_class->parse = gwy_fit_function_parse;

    gwy_resource_class_register(res_class, "fitfunctions", NULL);
}

static void
gwy_fit_function_init(GwyFitFunction *fit_function)
{
    fit_function->priv = G_TYPE_INSTANCE_GET_PRIVATE(fit_function,
                                                    GWY_TYPE_FIT_FUNCTION,
                                                    FitFunction);
    //TODO *fit_function->priv = opengl_default;
}

static gsize
gwy_fit_function_n_items(G_GNUC_UNUSED GwySerializable *serializable)
{
    return N_ITEMS;
}

static gsize
gwy_fit_function_itemize(GwySerializable *serializable,
                        GwySerializableItems *items)
{

    GwyFitFunction *fit_function = GWY_FIT_FUNCTION(serializable);
    FitFunction *function = fit_function->priv;
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
    if ((n = gwy_fit_function_parent_serializable->itemize(serializable, items)))
        return N_ITEMS+1 + n;
    return 0;
}

static gboolean
gwy_fit_function_construct(GwySerializable *serializable,
                          GwySerializableItems *items,
                          GwyErrorList **error_list)
{
    GwySerializableItem its[N_ITEMS];
    memcpy(its, serialize_items, sizeof(serialize_items));
    gsize np = gwy_deserialize_filter_items(its, N_ITEMS, items,
                                            "GwyFitFunction",
                                            error_list);
    // Chain to parent
    if (np < items->n) {
        np++;
        GwySerializableItems parent_items = {
            items->len - np, items->n - np, items->items + np
        };
        if (!gwy_fit_function_parent_serializable->construct(serializable,
                                                            &parent_items,
                                                            error_list))
            goto fail;
    }

    // Our own data
    GwyFitFunction *fit_function = GWY_FIT_FUNCTION(serializable);
    FitFunction *function = fit_function->priv;

    //gwy_fit_function_sanitize(fit_function);

    return TRUE;

fail:
    return FALSE;
}

static GObject*
gwy_fit_function_duplicate_impl(GwySerializable *serializable)
{
    GwyFitFunction *fit_function = GWY_FIT_FUNCTION(serializable);
    GwyFitFunction *duplicate = g_object_newv(GWY_TYPE_FIT_FUNCTION, 0, NULL);

    gwy_fit_function_parent_serializable->assign(GWY_SERIALIZABLE(duplicate),
                                                 serializable);
    duplicate->priv = fit_function->priv;

    return G_OBJECT(duplicate);
}

static void
gwy_fit_function_assign_impl(GwySerializable *destination,
                            GwySerializable *source)
{
    GwyFitFunction *dest = GWY_FIT_FUNCTION(destination);
    GwyFitFunction *src = GWY_FIT_FUNCTION(source);
    gboolean emit_changed = FALSE;

    g_object_freeze_notify(G_OBJECT(dest));
    gwy_fit_function_parent_serializable->assign(destination, source);
    /*
    if (memcmp(dest->priv, src->priv, sizeof(FitFunction)) != 0) {
        dest->priv = src->priv;
        emit_changed = TRUE;
    }
    */
    g_object_thaw_notify(G_OBJECT(dest));
    if (emit_changed)
        gwy_fit_function_changed(dest);
}

static GwyResource*
gwy_fit_function_copy(GwyResource *resource)
{
    return GWY_RESOURCE(gwy_fit_function_duplicate_impl(GWY_SERIALIZABLE(resource)));
}

static void
gwy_fit_function_changed(GwyFitFunction *fit_function)
{
    gwy_resource_data_changed(GWY_RESOURCE(fit_function));
}

static void
gwy_fit_function_setup_inventory(GwyInventory *inventory)
{
    gwy_inventory_set_default_name(inventory, GWY_FIT_FUNCTION_DEFAULT);
    GwyFitFunction *fit_function;
    // Constant
    fit_function = g_object_new(GWY_TYPE_FIT_FUNCTION,
                                "is-modifiable", FALSE,
                                "name", GWY_FIT_FUNCTION_DEFAULT,
                                NULL);
    gwy_inventory_insert(inventory, fit_function);
    g_object_unref(fit_function);
}

/**
 * gwy_fit_function_new:
 *
 * Creates a new GL function.
 *
 * Returns: A new free-standing GL function.
 **/
GwyFitFunction*
gwy_fit_function_new(void)
{
    return g_object_newv(GWY_TYPE_FIT_FUNCTION, 0, NULL);
}

gdouble
gwy_fit_function_get_value(GwyFitFunction *fitfunction,
                           gdouble x,
                           const gdouble *params,
                           gboolean *fres)
{
    return 0.0;
}

const gchar*
gwy_fit_function_get_formula(GwyFitFunction *fitfunction)
{
    return "";
}

guint
gwy_fit_function_get_nparams(GwyFitFunction *fitfunction)
{
    return 0;
}

const gchar*
gwy_fit_function_get_param_name(GwyFitFunction *fitfunction,
                                                guint param)
{
    return "";
}

GwyUnit*
gwy_fit_function_get_param_units(GwyFitFunction *fitfunction,
                                                 guint param,
                                                 GwyUnit *unit_x,
                                                 GwyUnit *unit_y)
{
    return NULL;
}

gboolean
gwy_fit_function_estimate(GwyFitFunction *fitfunction,
                                          GwyXY *points,
                                          guint npoints,
                                          gdouble *params)
{
    return FALSE;
}

GwyFitTask*
gwy_fit_function_get_fit_task(GwyFitFunction *fitfunction)
{
    return NULL;
}

static gchar*
gwy_fit_function_dump(GwyResource *resource)
{
    GwyFitFunction *fit_function = GWY_FIT_FUNCTION(resource);
    /*
    FitFunction *function = fit_function->priv;
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
gwy_fit_function_parse(GwyResource *resource,
                      gchar *text,
                      G_GNUC_UNUSED GError **error)
{
    GwyFitFunction *fit_function = GWY_FIT_FUNCTION(resource);

    /*
    if (!parse_component(&text, &fit_function->priv->ambient)
        || !parse_component(&text, &fit_function->priv->diffuse)
        || !parse_component(&text, &fit_function->priv->specular)
        || !parse_component(&text, &fit_function->priv->emission))
        return FALSE;

    gchar *end;
    fit_function->priv->shininess = g_ascii_strtod(text, &end);
    if (end == text)
        return FALSE;

    gwy_fit_function_sanitize(fit_function);
    */
    return TRUE;
}

#define __LIBGWY_FIT_FUNCTION_C__
#include "libgwy/libgwy-aliases.c"

/************************** Documentation ****************************/

/**
 * SECTION: fit-function
 * @title: GwyFitFunction
 * @short_description: User fitting function
 *
 * #GwyFitFunction represents a named fitting function with formula, named
 * parameters, capability to estimate parameters values or derive their units
 * from the units of fitted data.
 **/

/**
 * GwyFitFunction:
 *
 * Object represnting a user fitting function.
 *
 * The #GwyFitFunction struct contains private data only and should be accessed
 * using the functions below.
 **/

/**
 * GwyFitFunctionClass:
 *
 * Class of user fitting functions.
 *
 * #GwyFitFunctionClass does not contain any public members.
 **/

/**
 * GWY_FIT_FUNCTION_DEFAULT:
 *
 * Name of the default user fitting function.
 *
 * It is guaranteed to always exist.
 *
 * Note this is not the same as user's default fitting function which
 * corresponds to the default item in gwy_fit_functions() inventory and it
 * change over time.
 **/

/**
 * gwy_fit_function_duplicate:
 * @fit_function: A user fitting function.
 *
 * Duplicates a user fitting function.
 *
 * This is a convenience wrapper of gwy_serializable_duplicate().
 **/

/**
 * gwy_fit_function_assign:
 * @dest: Destination user fitting function.
 * @src: Source user fitting function.
 *
 * Copies the value of a user fitting function.
 *
 * This is a convenience wrapper of gwy_serializable_assign().
 **/

/**
 * gwy_fit_functions:
 *
 * Gets inventory with all the user fitting functions.
 *
 * Returns: User fitting function inventory.
 **/

/**
 * gwy_fit_functions_get:
 * @name: User fitting function name.  May be %NULL to get the default
 *        function.
 *
 * Convenience function to get a user fitting function from gwy_fit_functions()
 * by name.
 *
 * Returns: User fitting function identified by @name or the default user
 *          fitting function if @name does not exist.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
