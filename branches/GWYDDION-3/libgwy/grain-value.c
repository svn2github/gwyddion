/*
 *  $Id$
 *  Copyright (C) 2011 David Nečas (Yeti).
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
#include "libgwy/grain-value.h"
#include "libgwy/object-internal.h"
#include "libgwy/grain-value-builtin.h"

enum {
    PROP_0,
    PROP_NAME,
    PROP_GROUP,
    PROP_IDENT,
    PROP_SYMBOL,
    PROP_RESOURCE,
    N_PROPS
};

struct _GwyGrainValuePrivate {
    gchar *name;

    gboolean is_valid;  // Set to %TRUE if the function actually exists.

    guint ngrains;
    gdouble *values;
    GwyUnit *unit;

    // Exactly one of builtin/resource is set
    const BuiltinGrainValue *builtin;
    GwyUserGrainValue *resource;

    // User values only
    GwyExpr *expr;
    gulong data_changed_id;
    gulong notify_name_id;
};

typedef struct _GwyGrainValuePrivate GrainValue;

static void gwy_grain_value_constructed (GObject *object);
static void gwy_grain_value_dispose     (GObject *object);
static void gwy_grain_value_finalize    (GObject *object);
static void gwy_grain_value_set_property(GObject *object,
                                         guint prop_id,
                                         const GValue *value,
                                         GParamSpec *pspec);
static void gwy_grain_value_get_property(GObject *object,
                                         guint prop_id,
                                         GValue *value,
                                         GParamSpec *pspec);
static void user_value_data_changed     (GwyGrainValue *grainvalue,
                                         GwyUserGrainValue *usergrainvalue);
static void user_value_notify_name      (GwyGrainValue *grainvalue,
                                         GParamSpec *pspec,
                                         GwyUserGrainValue *usergrainvalue);
static const gchar* get_group (const GwyGrainValue *grainvalue);
static const gchar* get_ident (const GwyGrainValue *grainvalue);
static const gchar* get_symbol(const GwyGrainValue *grainvalue);

static GParamSpec *properties[N_PROPS];

G_DEFINE_TYPE(GwyGrainValue, gwy_grain_value, G_TYPE_OBJECT);

static GObjectClass *parent_class = NULL;
static GHashTable *builtin_values = NULL;

static void
gwy_grain_value_class_init(GwyGrainValueClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    parent_class = G_OBJECT_CLASS(gwy_grain_value_parent_class);

    g_type_class_add_private(klass, sizeof(GrainValue));

    gobject_class->constructed = gwy_grain_value_constructed;
    gobject_class->dispose = gwy_grain_value_dispose;
    gobject_class->finalize = gwy_grain_value_finalize;
    gobject_class->get_property = gwy_grain_value_get_property;
    gobject_class->set_property = gwy_grain_value_set_property;

    properties[PROP_NAME]
        = g_param_spec_string("name",
                              "Name",
                              "Grain value name, either built-in or user.",
                              "Invalid",
                              G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY
                              | G_PARAM_STATIC_STRINGS);

    properties[PROP_GROUP]
        = g_param_spec_string("group",
                              "Group",
                              "Group the grain value belongs to.",
                              "Invalid",
                              G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    properties[PROP_IDENT]
        = g_param_spec_string("ident",
                              "Ident",
                              "Plain identifier usable in expressions.",
                              "invalid",
                              G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    properties[PROP_SYMBOL]
        = g_param_spec_string("symbol",
                              "Symbol",
                              "Presentational symbol that may contain "
                              "arbitrary UTF-8 and/or Pango markup.",
                              "invalid",
                              G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    properties[PROP_RESOURCE]
        = g_param_spec_object("resource",
                              "Resource",
                              "GwyUserGrainValue resource wrapped by this "
                              "function if any.",
                              GWY_TYPE_USER_GRAIN_VALUE,
                              G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    for (guint i = 1; i < N_PROPS; i++)
        g_object_class_install_property(gobject_class, i, properties[i]);

    builtin_values = _gwy_grain_value_setup_builtins();
}

static void
gwy_grain_value_init(GwyGrainValue *grainvalue)
{
    grainvalue->priv = G_TYPE_INSTANCE_GET_PRIVATE(grainvalue,
                                                   GWY_TYPE_GRAIN_VALUE,
                                                   GrainValue);
}

static void
gwy_grain_value_constructed(GObject *object)
{
    GwyGrainValue *grainvalue = GWY_GRAIN_VALUE(object);
    GrainValue *priv = grainvalue->priv;

    priv->builtin = g_hash_table_lookup(builtin_values, priv->name);
    if (priv->builtin)
        priv->is_valid = TRUE;
    else {
        priv->resource = gwy_user_grain_values_get(priv->name);
        if (priv->resource) {
            g_object_ref(priv->resource);
            priv->data_changed_id
                = g_signal_connect_swapped(priv->resource, "data-changed",
                                           G_CALLBACK(user_value_data_changed),
                                           grainvalue);
            priv->notify_name_id
                = g_signal_connect_swapped(priv->resource, "notify::name",
                                           G_CALLBACK(user_value_notify_name),
                                           grainvalue);
            priv->is_valid = TRUE;
        }
    }
    if (parent_class->constructed)
        parent_class->constructed(object);
}

static void
invalidate(GwyGrainValue *grainvalue)
{
    GrainValue *priv = grainvalue->priv;
    GWY_OBJECT_UNREF(priv->unit);
    GWY_FREE(priv->values);
    GWY_OBJECT_UNREF(priv->expr);
}

static void
gwy_grain_value_dispose(GObject *object)
{
    GwyGrainValue *grainvalue = GWY_GRAIN_VALUE(object);
    GrainValue *priv = grainvalue->priv;
    GWY_SIGNAL_HANDLER_DISCONNECT(priv->resource, priv->data_changed_id);
    GWY_SIGNAL_HANDLER_DISCONNECT(priv->resource, priv->notify_name_id);
    GWY_OBJECT_UNREF(priv->unit);
    GWY_OBJECT_UNREF(priv->resource);
    GWY_OBJECT_UNREF(priv->expr);
    parent_class->dispose(object);
}

static void
gwy_grain_value_finalize(GObject *object)
{
    GwyGrainValue *grainvalue = GWY_GRAIN_VALUE(object);
    GrainValue *priv = grainvalue->priv;
    GWY_FREE(priv->name);
    GWY_FREE(priv->values);
    parent_class->finalize(object);
}

static void
gwy_grain_value_set_property(GObject *object,
                             guint prop_id,
                             const GValue *value,
                             GParamSpec *pspec)
{
    GwyGrainValue *grainvalue = GWY_GRAIN_VALUE(object);
    GrainValue *priv = grainvalue->priv;

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
gwy_grain_value_get_property(GObject *object,
                             guint prop_id,
                             GValue *value,
                             GParamSpec *pspec)
{
    GwyGrainValue *grainvalue = GWY_GRAIN_VALUE(object);
    GrainValue *priv = grainvalue->priv;

    switch (prop_id) {
        case PROP_NAME:
        g_value_set_string(value, priv->name);
        break;

        case PROP_GROUP:
        g_value_set_string(value, get_group(grainvalue));
        break;

        case PROP_IDENT:
        g_value_set_string(value, get_ident(grainvalue));
        break;

        case PROP_SYMBOL:
        g_value_set_string(value, get_symbol(grainvalue));
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
 * gwy_grain_value_new:
 * @name: Grain value name.  It must correspond to either a builtin grain
 *        value or user grain value loaded from #GwyUserGrainValue resources.
 *
 * Creates a new grain value.
 *
 * Returns: (transfer full):
 *          A new grain value.  It can return %NULL if @group is invalid
 *          or no function of called @name is present in the group (this is not
 *          considered to be an error).
 **/
GwyGrainValue*
gwy_grain_value_new(const gchar *name)
{
    GwyGrainValue *grainvalue = g_object_new(GWY_TYPE_GRAIN_VALUE,
                                             "name", name,
                                             NULL);
    if (grainvalue->priv->is_valid)
        return grainvalue;

    g_object_unref(grainvalue);
    return NULL;
}

/**
 * gwy_grain_value_get_name:
 * @grainvalue: A grain value.
 *
 * Obtains the name of a grain value.
 *
 * This is the same name as was used in gwy_grain_value_new().  Except if it is
 * a user grain value that has been renamed meanwhile.
 *
 * Returns: The grain value name.
 **/
const gchar*
gwy_grain_value_get_name(const GwyGrainValue *grainvalue)
{
    g_return_val_if_fail(GWY_IS_GRAIN_VALUE(grainvalue), NULL);
    return grainvalue->priv->name;
}

/**
 * gwy_grain_value_get_group:
 * @grainvalue: A grain value.
 *
 * Obtains the group of a grain value.
 *
 * Returns: The grain value group.
 **/
const gchar*
gwy_grain_value_get_group(GwyGrainValue *grainvalue)
{
    g_return_val_if_fail(GWY_IS_GRAIN_VALUE(grainvalue), NULL);
    return get_group(grainvalue);
}

/**
 * gwy_grain_value_get_ident:
 * @grainvalue: A grain value.
 *
 * Obtains the identfier of a grain value.
 *
 * Returns: The grain value identifier.
 **/
const gchar*
gwy_grain_value_get_ident(const GwyGrainValue *grainvalue)
{
    g_return_val_if_fail(GWY_IS_GRAIN_VALUE(grainvalue), NULL);
    return get_ident(grainvalue);
}

/**
 * gwy_grain_value_get_symbol:
 * @grainvalue: A grain value.
 *
 * Obtains the symbol of a grain value.
 *
 * Returns: The grain value symbol.
 **/
const gchar*
gwy_grain_value_get_symbol(const GwyGrainValue *grainvalue)
{
    g_return_val_if_fail(GWY_IS_GRAIN_VALUE(grainvalue), NULL);
    return get_symbol(grainvalue);
}

/**
 * gwy_grain_value_unit:
 * @grainvalue: A grain value.
 *
 * Obtains the unit of a grain value.
 *
 * The unit is derived from field units when the grain values are evaulated.
 * If no evaulation has been done yet this function returns %NULL.  It also
 * returns %NULL if the unit is not meaningful, for instance if the grain
 * value has @GWY_GRAIN_VALUE_SAME_UNITS flag set but it was evaluated for a
 * field that had different lateral and value units.
 *
 * Returns: (transfer none) (allow-none):
 *          A @GwyUnit with the grain value units.
 **/
const GwyUnit*
gwy_grain_value_unit(const GwyGrainValue *grainvalue)
{
    g_return_val_if_fail(GWY_IS_GRAIN_VALUE(grainvalue), NULL);
    return grainvalue->priv->unit;
}

/**
 * gwy_grain_value_data:
 * @grainvalue: A grain value.
 * @ngrains: (out) (allow-none):
 *           Location to store the number of grains.
 *
 * Obtains the data of individual grains for a grain value.
 *
 * Returns: (transfer none) (allow-none) (array length=ngrains):
 *          An array of @ngrains+1 items containing the grain values from the
 *          last evaluation.  %NULL can be returned if @grainvalue has not been
 *          used for any evaluation yet or the underlying resource has changed.
 **/
const gdouble*
gwy_grain_value_data(const GwyGrainValue *grainvalue,
                     guint *ngrains)
{
    g_return_val_if_fail(GWY_IS_GRAIN_VALUE(grainvalue), NULL);
    GWY_MAYBE_SET(ngrains, grainvalue->priv->ngrains);
    return grainvalue->priv->values;
}

static void
user_value_data_changed(GwyGrainValue *grainvalue,
                        G_GNUC_UNUSED GwyUserGrainValue *usergrainvalue)
{
    // Just invalidate stuff, construct_expr() will create it again if
    // necessary.
    GrainValue *priv = grainvalue->priv;
    GWY_OBJECT_UNREF(priv->expr);
}

// This does not feel right, or at least not useful.  But if the name changes
// we should emit notify::name so just do it.
static void
user_value_notify_name(GwyGrainValue *grainvalue,
                        G_GNUC_UNUSED GParamSpec *pspec,
                        GwyUserGrainValue *usergrainvalue)
{
    GrainValue *priv = grainvalue->priv;
    GwyResource *resource = GWY_RESOURCE(usergrainvalue);
    const gchar *name = gwy_resource_get_name(resource);
    if (!gwy_strequal(name, priv->name)) {
        g_free(priv->name);
        priv->name = g_strdup(name);
        g_object_notify_by_pspec(G_OBJECT(grainvalue), properties[PROP_NAME]);
    }
}

static void
construct_expr(GrainValue *priv)
{
    priv->expr = _gwy_grain_value_new_expr_with_constants();
    if (!gwy_expr_compile(priv->expr,
                          gwy_user_grain_value_get_formula(priv->resource),
                          NULL)) {
        g_critical("Cannot compile user grain value formula.");
        return;
    }

    /* TODO: At some point we need to calculate the dependences.  Now?
    priv->indices = g_new(guint, priv->nparams+1);
    if (gwy_user_grain_value_resolve_params(priv->resource, priv->expr, NULL,
                                            priv->indices)) {
        g_critical("Cannot resolve variables in user grain value "
                   "formula.");
        return;
    }
    */
}

static const gchar*
get_group(const GwyGrainValue *grainvalue)
{
    GrainValue *priv = grainvalue->priv;
    if (priv->builtin)
        return priv->builtin->group;
    else
        return gwy_user_grain_value_get_group(priv->resource);
}

static const gchar*
get_ident(const GwyGrainValue *grainvalue)
{
    GrainValue *priv = grainvalue->priv;
    if (priv->builtin)
        return priv->builtin->ident;
    else
        return gwy_user_grain_value_get_ident(priv->resource);
}

static const gchar*
get_symbol(const GwyGrainValue *grainvalue)
{
    GrainValue *priv = grainvalue->priv;
    if (priv->builtin) {
        if (priv->builtin->symbol)
            return priv->builtin->symbol;
        return priv->builtin->ident;
    }
    else
        return gwy_user_grain_value_get_symbol(priv->resource);
}

/**
 * gwy_grain_value_get_resource:
 * @grainvalue: A grain value.
 *
 * Obtains the user-defined grain value resource of a grain value.
 *
 * This method can be called both with built-in and user-defined grain values
 * functions, in fact, it can be used to determine if a grain value is
 * user-defined.
 *
 * Returns: (transfer none) (allow-none):
 *          The user grain value resource corresponding to @grainvalue
 *          (no reference added), or %NULL if the grain value is not defined by
 *          a user resource.
 **/
GwyUserGrainValue*
gwy_grain_value_get_resource(const GwyGrainValue *grainvalue)
{
    g_return_val_if_fail(GWY_IS_GRAIN_VALUE(grainvalue), NULL);
    GrainValue *priv = grainvalue->priv;
    return priv->builtin ? NULL : priv->resource;
}

/**
 * gwy_grain_value_needs_same_units:
 * @grainvalue: A grain value.
 *
 * Reports whether a grain value needs the same lateral and value units.
 *
 * Some grain values, such as surface area, are meaningful only if height is
 * the same physical quantity as lateral dimensions.  For these grain values
 * this function returns %TRUE.
 *
 * Returns: %TRUE if the grain value needs the same lateral and value units.
 **/
gboolean
gwy_grain_value_needs_same_units(const GwyGrainValue *grainvalue)
{
    g_return_val_if_fail(GWY_IS_GRAIN_VALUE(grainvalue), FALSE);
    GrainValue *priv = grainvalue->priv;
    if (priv->builtin)
        return priv->builtin->same_units;
    else
        return gwy_user_grain_value_get_same_units(priv->resource);
}

/**
 * gwy_grain_value_is_valid:
 * @grainvalue: A grain value.
 *
 * Reports whether a grain value is valid.
 *
 * Since invalid grain value resources are not loaded at all and the
 * constructor gwy_grain_value_new() returns %NULL if a non-existent grain
 * value is requested, an invalid grain value object can only be created
 * directly using g_object_new() with a non-existent grain value name.  So this
 * method is mainly intended for bindings where such situation can arise more
 * easily than in C.
 *
 * Returns: %TRUE if the grain value is valid and can be used for evaluation,
 *          etc.
 **/
gboolean
gwy_grain_value_is_valid(const GwyGrainValue *grainvalue)
{
    g_return_val_if_fail(GWY_IS_GRAIN_VALUE(grainvalue), FALSE);
    return grainvalue->priv->is_valid;
}

/**
 * gwy_field_evaluate_grains:
 * @field: A two-dimensional data field.
 * @mask: A two-dimensional mask field representing the grains.
 * @grainvalues: (array length=nvalues):
 *               Grain values to calculate for the grains.
 * @nvalues: Number of items in @grainvalues.
 *
 * Calculates a set of grain values given a surface and mask representing the
 * grains on this surface.
 *
 * The sizes of @field and @mask must match.
 **/
void
gwy_field_evaluate_grains(const GwyField *field,
                          const GwyMaskField *mask,
                          GwyGrainValue **grainvalues,
                          guint nvalues)
{
    // TODO
}

GwyExpr*
_gwy_grain_value_new_expr_with_constants(void)
{
    GwyExpr *expr = gwy_expr_new();
    gwy_expr_define_constant(expr, "pi", G_PI, NULL);
    gwy_expr_define_constant(expr, "π", G_PI, NULL);
    return expr;
}

/************************** Documentation ****************************/

/**
 * SECTION: grain-value
 * @title: GwyGrainValue
 * @short_description: Grain value
 *
 * #GwyGrainValue represents a named quantity that can be evaluated for
 * individual grains, with formula and capability to derive units from the
 * units of the data field.
 *
 * It can wrap either a built-in grain value or user grain value resources
 * #GwyUserGrainValue.
 *
 * FIXME: Built-in grain values should be listed here.  Once we implement some.
 **/

/**
 * GwyGrainValue:
 *
 * Object represnting a grain value.
 *
 * The #GwyGrainValue struct contains private data only and should be accessed
 * using the functions below.
 **/

/**
 * GwyGrainValueClass:
 *
 * Class of grain values.
 *
 * #GwyGrainValueClass does not contain any public members.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
