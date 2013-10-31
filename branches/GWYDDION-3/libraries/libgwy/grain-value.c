/*
 *  $Id$
 *  Copyright (C) 2011-2012 David Nečas (Yeti).
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
#include "libgwy/mask-field-grains.h"
#include "libgwy/grain-value.h"
#include "libgwy/object-internal.h"
#include "libgwy/field-internal.h"
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
static BuiltinGrainValueTable builtin_table = { NULL, 0, NULL, NULL };

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

    _gwy_grain_value_setup_builtins(&builtin_table);
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

    priv->builtin = g_hash_table_lookup(builtin_table.table, priv->name);
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
gwy_grain_value_dispose(GObject *object)
{
    GwyGrainValue *grainvalue = GWY_GRAIN_VALUE(object);
    GrainValue *priv = grainvalue->priv;
    GWY_SIGNAL_HANDLER_DISCONNECT(priv->resource, priv->data_changed_id);
    GWY_SIGNAL_HANDLER_DISCONNECT(priv->resource, priv->notify_name_id);
    GWY_OBJECT_UNREF(priv->unit);
    GWY_OBJECT_UNREF(priv->resource);
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
 * Returns: (transfer full) (allow-none):
 *          A new grain value.  It can return %NULL if no grain value called
 *          @name exists (this is not considered to be an error).
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
gwy_grain_value_get_group(const GwyGrainValue *grainvalue)
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
 * The zeroth item in the returned array does not contain any meaningful value
 * and is present for consistent indexing by grain numbers (that start from 1).
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

// XXX: Nothing to do here?
static void
user_value_data_changed(G_GNUC_UNUSED GwyGrainValue *grainvalue,
                        G_GNUC_UNUSED GwyUserGrainValue *usergrainvalue)
{
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
 * Reports whether a grain value needs the same lateral and/or value units.
 *
 * See #GwyGrainValueSameUnits for a discussion of what grain quantities
 * require which unit combinations.
 *
 * Returns: Combination of units the grain value requires to be the same.
 **/
GwyGrainValueSameUnits
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
 * gwy_grain_value_is_angle:
 * @grainvalue: A grain value.
 *
 * Reports whether a grain value represents an angle.
 *
 * Grain values that represent angles are calculated in radians and reported
 * as unitless.  To present them to the user they usually should be converted
 * to degrees.  Using this method you can tell apart angular values from other
 * unitless values.
 *
 * Returns: %TRUE if the grain value represents an angle.
 **/
gboolean
gwy_grain_value_is_angle(const GwyGrainValue *grainvalue)
{
    g_return_val_if_fail(GWY_IS_GRAIN_VALUE(grainvalue), FALSE);
    GrainValue *priv = grainvalue->priv;
    if (priv->builtin)
        return priv->builtin->is_angle;
    else
        return gwy_user_grain_value_get_is_angle(priv->resource);
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

static void
add_deps(GwyGrainValue *grainvalue,
         GwyExpr *expr,
         GwyGrainValue **builtins,
         guint *indices)
{
    GwyUserGrainValue *usergrainvalue = grainvalue->priv->resource;
    g_return_if_fail(usergrainvalue);
    const gchar *formula = gwy_user_grain_value_get_formula(usergrainvalue);
    guint n = builtin_table.n;
    if (!gwy_expr_compile(expr, formula, NULL)) {
        g_critical("Cannot compile grain value expresion for %s.",
                   gwy_resource_get_name(GWY_RESOURCE(usergrainvalue)));
        return;
    }
    if (gwy_expr_resolve_variables(expr, n, builtin_table.idents, indices)) {
        g_critical("Cannot resolve variables in grain value expression for %s.",
                   gwy_resource_get_name(GWY_RESOURCE(usergrainvalue)));
        return;
    }
    for (guint i = 0; i < n; i++) {
        if (indices[i] && !builtins[i])
            builtins[i] = gwy_grain_value_new(builtin_table.names[i]);
    }
}

static void
calc_derived(GwyGrainValue *grainvalue,
             GwyExpr *expr,
             GwyGrainValue **builtins,
             guint *indices,
             const gdouble **vectors,
             guint ngrains)
{
    GwyUserGrainValue *usergrainvalue = grainvalue->priv->resource;
    g_return_if_fail(usergrainvalue);
    const gchar *formula = gwy_user_grain_value_get_formula(usergrainvalue);
    guint n = builtin_table.n;
    if (!gwy_expr_compile(expr, formula, NULL)) {
        g_critical("Cannot compile grain value expresion for %s.",
                   gwy_resource_get_name(GWY_RESOURCE(usergrainvalue)));
        return;
    }
    if (gwy_expr_resolve_variables(expr, n, builtin_table.idents, indices)) {
        g_critical("Cannot resolve variables in grain value expression for %s.",
                   gwy_resource_get_name(GWY_RESOURCE(usergrainvalue)));
        return;
    }

    for (guint i = 0; i < n; i++) {
        if (indices[i])
            vectors[indices[i]] = builtins[i]->priv->values;
    }
    gwy_expr_vector_execute(expr, ngrains+1, vectors, grainvalue->priv->values);
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
    g_return_if_fail(GWY_IS_FIELD(field));
    g_return_if_fail(GWY_IS_MASK_FIELD(mask));
    g_return_if_fail(field->xres == mask->xres);
    g_return_if_fail(field->yres == mask->yres);
    if (!nvalues)
        return;
    g_return_if_fail(grainvalues);
    for (guint i = 0; i < nvalues; i++)
        g_return_if_fail(GWY_IS_GRAIN_VALUE(grainvalues[i]));

    guint ngrains = gwy_mask_field_n_grains(mask);
    guint n = builtin_table.n;
    gsize tablesize = n*sizeof(GwyGrainValue*);
    GwyGrainValue **builtins = g_slice_alloc0(tablesize);
    const gdouble **vectors = NULL;
    guint *indices = NULL;
    GwyExpr *expr = NULL;

    for (guint i = 0; i < nvalues; i++) {
        GwyGrainValue *grainvalue = grainvalues[i];
        GrainValue *priv = grainvalue->priv;
        if (priv->builtin) {
            BuiltinGrainValueId id = priv->builtin->id;
            if (!builtins[id])
                builtins[id] = g_object_ref(grainvalue);
        }
        else {
            if (!expr) {
                indices = g_slice_alloc(n*sizeof(guint));
                vectors = g_slice_alloc((n + 1)*sizeof(gdouble*));
                expr = gwy_expr_new();
            }
            add_deps(grainvalue, expr, builtins, indices);
        }
    }

    GwyGrainValue **compact_builtins = g_slice_alloc0(tablesize);
    guint bcount = 0;
    for (guint i = 0; i < n; i++) {
        if ((compact_builtins[bcount] = builtins[i]))
            bcount++;
    }
    _gwy_grain_value_evaluate_builtins(field, mask, compact_builtins, bcount);

    const GwyUnit *unitx = field->priv->xunit;
    const GwyUnit *unity = field->priv->yunit;
    const GwyUnit *unitz = field->priv->zunit;
    for (guint i = 0; i < nvalues; i++) {
        GwyGrainValue *grainvalue = grainvalues[i];
        GrainValue *priv = grainvalue->priv;
        if (priv->builtin) {
            BuiltinGrainValueId id = priv->builtin->id;
            if (builtins[id] != grainvalue)
                _gwy_grain_value_assign(grainvalue, builtins[id]);
        }
        else {
            _gwy_grain_value_set_size(grainvalue, ngrains);
            calc_derived(grainvalue, expr, builtins, indices, vectors, ngrains);

            GwyUserGrainValue *usergrainvalue = grainvalue->priv->resource;
            gint powerx = gwy_user_grain_value_get_power_x(usergrainvalue);
            gint powery = gwy_user_grain_value_get_power_y(usergrainvalue);
            gint powerz = gwy_user_grain_value_get_power_z(usergrainvalue);
            if (!priv->unit)
                priv->unit = gwy_unit_new();
            gwy_unit_power_multiply(priv->unit, unitx, powerx, unity, powery);
            gwy_unit_power_multiply(priv->unit, priv->unit, 1, unitz, powerz);
        }
    }

    for (guint i = 0; i < bcount; i++)
        g_object_unref(compact_builtins[i]);
    if (expr) {
        g_object_unref(expr);
        g_slice_free1(n*sizeof(guint), indices);
        g_slice_free1((n + 1)*sizeof(gdouble*), vectors);
    }
    g_slice_free1(tablesize, compact_builtins);
    g_slice_free1(tablesize, builtins);
}

/**
 * gwy_grain_value_evaluate:
 * @grainvalue: A grain value.
 * @field: A two-dimensional data field.
 * @mask: A two-dimensional mask field representing the grains.
 *
 * Evaluates a single grain value given a surface and mask representing the
 * grains on this surface.
 *
 * The sizes of @field and @mask must match.
 *
 * Evaluation of grain values using a #GwyGrainValue method, instead of
 * gwy_field_evaluate_grains() which is a #GwyField method, may be sometimes
 * convenient.  Note, however, that evaluation of multiple grain values is
 * considerably more efficient using gwy_field_evaluate_grains(), especially if
 * the values are related because this method ensures that common calculations
 * are performed only once.
 **/
void
gwy_grain_value_evaluate(GwyGrainValue *grainvalue,
                         const GwyField *field,
                         const GwyMaskField *mask)
{
    g_return_if_fail(GWY_IS_GRAIN_VALUE(grainvalue));

    if (grainvalue->priv->builtin) {
        g_return_if_fail(GWY_IS_FIELD(field));
        g_return_if_fail(GWY_IS_MASK_FIELD(mask));
        g_return_if_fail(field->xres == mask->xres);
        g_return_if_fail(field->yres == mask->yres);
        _gwy_grain_value_evaluate_builtins(field, mask, &grainvalue, 1);
    }
    else
        gwy_field_evaluate_grains(field, mask, &grainvalue, 1);
}

GwyExpr*
_gwy_grain_value_new_expr_with_constants(void)
{
    GwyExpr *expr = gwy_expr_new();
    gwy_expr_define_constant(expr, "pi", G_PI, NULL);
    gwy_expr_define_constant(expr, "π", G_PI, NULL);
    return expr;
}

void
_gwy_grain_value_set_size(GwyGrainValue *grainvalue, guint ngrains)
{
    GrainValue *priv = grainvalue->priv;
    if (priv->ngrains != ngrains) {
        GWY_FREE(priv->values);
        priv->values = g_new(gdouble, ngrains+1);
        priv->ngrains = ngrains;
    }
}

void
_gwy_grain_value_assign(GwyGrainValue *dest,
                        const GwyGrainValue *source)
{
    GrainValue *dpriv = dest->priv;
    const GrainValue *spriv = source->priv;
    g_return_if_fail(gwy_strequal(dpriv->name, spriv->name));

    _gwy_grain_value_set_size(dest, spriv->ngrains);
    gwy_assign(dpriv->values, spriv->values, spriv->ngrains+1);
    _gwy_assign_unit(&dpriv->unit, spriv->unit);
}

/**
 * gwy_grain_value_list_builtins:
 *
 * Obtains the list of all built-in grain value names.
 *
 * The list is valid permanently. Neither the list nor its elements may be
 * modified or freed.
 *
 * Returns: (transfer none) (array zero-terminated=1):
 *          A %NULL-terminated array with grain value names.
 **/
const gchar* const*
gwy_grain_value_list_builtins(void)
{
    gpointer klass = g_type_class_ref(GWY_TYPE_GRAIN_VALUE);
    const gchar* const *retval = builtin_table.names;
    g_type_class_unref(klass);
    return retval;
}

const gchar* const*
_gwy_grain_value_list_builtin_idents(void)
{
    gpointer klass = g_type_class_ref(GWY_TYPE_GRAIN_VALUE);
    const gchar* const *retval = builtin_table.idents;
    g_type_class_unref(klass);
    return retval;
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
 * A #GwyGrainValue can wrap either a built-in grain value (see
 * <link linkend='libgwy-builtin-grain-value'>Builtin grain values</link>
 * for a list) or user grain value resources #GwyUserGrainValue.
 *
 * After evaluation using gwy_field_evaluate_grains() it holds the results,
 * i.e. the obtained grain data and their units that can be accessed using
 * gwy_grain_value_data() and gwy_grain_value_unit().  The grain value objects
 * take no references to the mask or field and changes in the mask, field or
 * even definition of the grain value (in the case of user grain values) do not
 * cause automatic re-evaluation.  So to re-evaluate, you need to use
 * gwy_field_evaluate_grains() again.  On the other hand this independence mean
 * grain values remains valid after the mask and field are destroyed.
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
