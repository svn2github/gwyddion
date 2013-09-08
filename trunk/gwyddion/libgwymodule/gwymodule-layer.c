/*
 *  @(#) $Id$
 *  Copyright (C) 2003,2004 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "config.h"
#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwymodule/gwymodule-layer.h>
#include "gwymoduleinternal.h"

/* Auxiliary structure to pass both user callback function and data to
 * g_hash_table_foreach() lambda argument in gwy_layer_func_foreach() */
typedef struct {
    GFunc function;
    gpointer user_data;
} LayerFuncForeachData;

static GHashTable *layer_funcs = NULL;

/**
 * gwy_layer_func_register:
 * @type: Layer type in GObject type system.  That is the return value of
 *        gwy_layer_foo_get_type().
 *
 * Registeres a layer function (layer type).
 *
 * Returns: Normally %TRUE; %FALSE on failure.
 **/
gboolean
gwy_layer_func_register(GType type)
{
    const gchar *name;
    gpointer klass;

    g_return_val_if_fail(type, FALSE);
    klass = g_type_class_ref(type);
    name = g_type_name(type);
    gwy_debug("layer type = %s", name);

    if (!layer_funcs) {
        gwy_debug("Initializing...");
        layer_funcs = g_hash_table_new_full(g_str_hash, g_str_equal,
                                            NULL, NULL);
    }

    if (g_hash_table_lookup(layer_funcs, name)) {
        g_warning("Duplicate type %s, keeping only first", name);
        g_type_class_unref(klass);
        return FALSE;
    }
    g_hash_table_insert(layer_funcs, (gpointer)name, GUINT_TO_POINTER(type));
    if (!_gwy_module_add_registered_function(GWY_MODULE_PREFIX_LAYER, name)) {
        g_hash_table_remove(layer_funcs, name);
        g_type_class_unref(klass);
        return FALSE;
    }

    return TRUE;
}

static void
gwy_layer_func_user_cb(gpointer key,
                       G_GNUC_UNUSED gpointer value,
                       gpointer user_data)
{
    LayerFuncForeachData *lffd = (LayerFuncForeachData*)user_data;

    lffd->function(key, lffd->user_data);
}

/**
 * gwy_layer_func_foreach:
 * @function: Function to run for each layer function.  It will get function
 *            name (constant string owned by module system) as its first
 *            argument, @user_data as the second argument.
 * @user_data: Data to pass to @function.
 *
 * Calls a function for each layer function.
 **/
void
gwy_layer_func_foreach(GFunc function,
                       gpointer user_data)
{
    LayerFuncForeachData lffd;

    if (!layer_funcs)
        return;

    lffd.user_data = user_data;
    lffd.function = function;
    g_hash_table_foreach(layer_funcs, gwy_layer_func_user_cb, &lffd);
}

gboolean
_gwy_layer_func_remove(const gchar *name)
{
    GType type;

    gwy_debug("%s", name);
    type = GPOINTER_TO_UINT(g_hash_table_lookup(layer_funcs, name));
    if (!type) {
        g_warning("Cannot remove function %s", name);
        return FALSE;
    }

    g_type_class_unref(g_type_class_peek(type));
    g_hash_table_remove(layer_funcs, name);
    return TRUE;
}

/************************** Documentation ****************************/

/**
 * SECTION:gwymodule-layer
 * @title: gwymodule-layer
 * @short_description: GwyDataView layer modules
 *
 * Layer modules implement #GwyDataView layers, corresponding to different
 * kinds of selections.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
