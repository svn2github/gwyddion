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
#include <stdlib.h>
#include <glib/gi18n-lib.h>
#include "libgwy/macros.h"
#include "libgwy/types.h"
#include "libgwy/strfuncs.h"
#include "libgwy/math.h"
#include "libgwy/unit.h"
#include "libgwy/expr.h"
#include "libgwy/serialize.h"
#include "libgwy/user-grain-value.h"
#include "libgwy/grain-value-builtin.h"
#include "libgwy/object-internal.h"

enum { POWER_MIN = -12, POWER_MAX = 12 };

enum { N_ITEMS = 9 };

struct _GwyUserGrainValuePrivate {
    gchar *group;
    gchar *formula;
    gchar *ident;
    gchar *symbol;
    gint power_x;
    gint power_y;
    gint power_z;
    GwyGrainValueSameUnits same_units;
    gboolean is_angle;
};

typedef struct _GwyUserGrainValuePrivate UserGrainValue;

static void         gwy_user_grain_value_finalize         (GObject *object);
static void         gwy_user_grain_value_serializable_init(GwySerializableInterface *iface);
static gsize        gwy_user_grain_value_n_items          (GwySerializable *serializable);
static gsize        gwy_user_grain_value_itemize          (GwySerializable *serializable,
                                                           GwySerializableItems *items);
static gboolean     gwy_user_grain_value_construct        (GwySerializable *serializable,
                                                           GwySerializableItems *items,
                                                           GwyErrorList **error_list);
static GObject*     gwy_user_grain_value_duplicate_impl   (GwySerializable *serializable);
static void         gwy_user_grain_value_assign_impl      (GwySerializable *destination,
                                                           GwySerializable *source);
static GwyResource* gwy_user_grain_value_copy             (GwyResource *resource);
static void         gwy_user_grain_value_changed          (GwyUserGrainValue *usergrainvalue);
static gboolean     validate                              (GwyUserGrainValue *usergrainvalue,
                                                           guint domain,
                                                           guint code,
                                                           GError **error);
static void         ensure_test_expr                      (void);
static gboolean     resolve_deps                          (GError **error,
                                                           guint domain,
                                                           guint code);
static gchar*       gwy_user_grain_value_dump             (GwyResource *resource);
static gboolean     gwy_user_grain_value_parse            (GwyResource *resource,
                                                           GwyStrLineIter *iter,
                                                           GError **error);

static const gunichar more[] = { '_', 0 };

static const GwySerializableItem serialize_items[N_ITEMS] = {
    /*0*/ { .name = "group",      .ctype = GWY_SERIALIZABLE_STRING,  },
    /*1*/ { .name = "formula",    .ctype = GWY_SERIALIZABLE_STRING,  },
    /*2*/ { .name = "ident",      .ctype = GWY_SERIALIZABLE_STRING,  },
    /*3*/ { .name = "symbol",     .ctype = GWY_SERIALIZABLE_STRING,  },
    /*4*/ { .name = "x-power",    .ctype = GWY_SERIALIZABLE_INT8,    },
    /*5*/ { .name = "y-power",    .ctype = GWY_SERIALIZABLE_INT8,    },
    /*6*/ { .name = "z-power",    .ctype = GWY_SERIALIZABLE_INT8,    },
    /*7*/ { .name = "same-units", .ctype = GWY_SERIALIZABLE_INT8,    },
    /*8*/ { .name = "is-angle",   .ctype = GWY_SERIALIZABLE_BOOLEAN, },
};

static guint test_expr_nidents = 0;
static guint *test_expr_indices = NULL;
static GwyExpr *test_expr = NULL;
G_LOCK_DEFINE_STATIC(test_expr);

G_DEFINE_TYPE_EXTENDED
    (GwyUserGrainValue, gwy_user_grain_value, GWY_TYPE_RESOURCE, 0,
     GWY_IMPLEMENT_SERIALIZABLE(gwy_user_grain_value_serializable_init));

static GwySerializableInterface *parent_serializable = NULL;

static void
gwy_user_grain_value_serializable_init(GwySerializableInterface *iface)
{
    parent_serializable = g_type_interface_peek_parent(iface);
    iface->n_items   = gwy_user_grain_value_n_items;
    iface->itemize   = gwy_user_grain_value_itemize;
    iface->construct = gwy_user_grain_value_construct;
    iface->duplicate = gwy_user_grain_value_duplicate_impl;
    iface->assign    = gwy_user_grain_value_assign_impl;
}

static void
gwy_user_grain_value_class_init(GwyUserGrainValueClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GwyResourceClass *res_class = GWY_RESOURCE_CLASS(klass);

    g_type_class_add_private(klass, sizeof(UserGrainValue));

    gobject_class->finalize = gwy_user_grain_value_finalize;

    res_class->copy = gwy_user_grain_value_copy;
    res_class->dump = gwy_user_grain_value_dump;
    res_class->parse = gwy_user_grain_value_parse;

    gwy_resource_class_register(res_class, "usergrainvalues",
                                N_("Grain value"), NULL);
}

static void
gwy_user_grain_value_init(GwyUserGrainValue *usergrainvalue)
{
    usergrainvalue->priv = G_TYPE_INSTANCE_GET_PRIVATE(usergrainvalue,
                                                       GWY_TYPE_USER_GRAIN_VALUE,
                                                       UserGrainValue);
    UserGrainValue *priv = usergrainvalue->priv;

    // Constant, by default.
    priv->formula = g_strdup("0");
    priv->group = g_strdup("User");
    priv->ident = g_strdup("");
}

static void
gwy_user_grain_value_finalize(GObject *object)
{
    GwyUserGrainValue *usergrainvalue = GWY_USER_GRAIN_VALUE(object);
    UserGrainValue *priv = usergrainvalue->priv;
    GWY_FREE(priv->formula);
    GWY_FREE(priv->group);
    GWY_FREE(priv->ident);
    GWY_FREE(priv->symbol);
    G_OBJECT_CLASS(gwy_user_grain_value_parent_class)->finalize(object);
}

static gsize
gwy_user_grain_value_n_items(GwySerializable *serializable)
{
    return N_ITEMS+1 + parent_serializable->n_items(serializable);
}

static gsize
gwy_user_grain_value_itemize(GwySerializable *serializable,
                             GwySerializableItems *items)
{
    g_return_val_if_fail(items->len - items->n >= N_ITEMS+1, 0);

    GwyUserGrainValue *usergrainvalue = GWY_USER_GRAIN_VALUE(serializable);
    UserGrainValue *priv = usergrainvalue->priv;
    GwySerializableItem *it = items->items + items->n;
    guint nn = 0;

    // Our own data
    *it = serialize_items[0];
    it->value.v_string = priv->group;
    it++, items->n++, nn++;

    *it = serialize_items[1];
    it->value.v_string = priv->formula;
    it++, items->n++, nn++;

    *it = serialize_items[2];
    it->value.v_string = priv->ident;
    it++, items->n++, nn++;

    if (priv->symbol) {
        *it = serialize_items[3];
        it->value.v_string = priv->symbol;
        it++, items->n++, nn++;
    }

    if (priv->power_x) {
        *it = serialize_items[4];
        it->value.v_int8 = priv->power_x;
        it++, items->n++, nn++;
    }

    if (priv->power_y) {
        *it = serialize_items[5];
        it->value.v_int8 = priv->power_y;
        it++, items->n++, nn++;
    }

    if (priv->power_z) {
        *it = serialize_items[6];
        it->value.v_int8 = priv->power_z;
        it++, items->n++, nn++;
    }

    if (priv->same_units) {
        *it = serialize_items[7];
        it->value.v_uint8 = priv->same_units;
        it++, items->n++, nn++;
    }

    if (priv->is_angle) {
        *it = serialize_items[8];
        it->value.v_boolean = priv->is_angle;
        it++, items->n++, nn++;
    }

    return _gwy_itemize_chain_to_parent(serializable, GWY_TYPE_RESOURCE,
                                        parent_serializable, items, nn);
}

static gboolean
gwy_user_grain_value_construct(GwySerializable *serializable,
                               GwySerializableItems *items,
                               GwyErrorList **error_list)
{
    GwySerializableItem its[N_ITEMS];
    memcpy(its, serialize_items, sizeof(serialize_items));
    GwySerializableItems parent_items;

    if (!gwy_deserialize_filter_items(its, N_ITEMS, items, &parent_items,
                                      "GwyUserGrainValue", error_list)
        || !parent_serializable->construct(serializable, &parent_items,
                                           error_list))
        goto fail;

    // Our own data
    GwyUserGrainValue *usergrainvalue = GWY_USER_GRAIN_VALUE(serializable);
    UserGrainValue *priv = usergrainvalue->priv;

    GWY_TAKE_STRING(priv->group, its[0].value.v_string);
    GWY_TAKE_STRING(priv->formula, its[1].value.v_string);
    GWY_TAKE_STRING(priv->ident, its[2].value.v_string);
    GWY_TAKE_STRING(priv->symbol, its[3].value.v_string);

    // FIXME
    priv->power_x = CLAMP(its[4].value.v_int8, POWER_MIN, POWER_MAX);
    priv->power_y = CLAMP(its[5].value.v_int8, POWER_MIN, POWER_MAX);
    priv->power_z = CLAMP(its[6].value.v_int8, POWER_MIN, POWER_MAX);
    priv->same_units = MIN(its[7].value.v_uint8, GWY_GRAIN_VALUE_SAME_UNITS_ALL);
    priv->is_angle = !!its[8].value.v_boolean;

    GError *err = NULL;
    if (validate(usergrainvalue,
                 GWY_DESERIALIZE_ERROR, GWY_DESERIALIZE_ERROR_INVALID, &err))
        return TRUE;

    gwy_error_list_propagate(error_list, err);

fail:
    return FALSE;
}

static void
assign_info(UserGrainValue *dpriv,
            const UserGrainValue *spriv)
{
    gwy_assign_string(&dpriv->formula, spriv->formula);
    gwy_assign_string(&dpriv->group, spriv->group);
    gwy_assign_string(&dpriv->ident, spriv->ident);
    gwy_assign_string(&dpriv->symbol, spriv->symbol);
    dpriv->power_x = spriv->power_x;
    dpriv->power_y = spriv->power_y;
    dpriv->power_z = spriv->power_z;
    dpriv->same_units = spriv->same_units;
    dpriv->is_angle = spriv->is_angle;
}

static GObject*
gwy_user_grain_value_duplicate_impl(GwySerializable *serializable)
{
    GwyUserGrainValue *usergrainvalue = GWY_USER_GRAIN_VALUE(serializable);
    UserGrainValue *priv = usergrainvalue->priv;
    GwyUserGrainValue *duplicate = g_object_newv(GWY_TYPE_USER_GRAIN_VALUE,
                                                 0, NULL);
    UserGrainValue *dpriv = duplicate->priv;

    parent_serializable->assign(GWY_SERIALIZABLE(duplicate), serializable);
    assign_info(dpriv, priv);

    return G_OBJECT(duplicate);
}

static void
gwy_user_grain_value_assign_impl(GwySerializable *destination,
                                 GwySerializable *source)
{
    GwyUserGrainValue *dest = GWY_USER_GRAIN_VALUE(destination);
    GwyUserGrainValue *src = GWY_USER_GRAIN_VALUE(source);

    g_object_freeze_notify(G_OBJECT(dest));
    parent_serializable->assign(destination, source);

    const UserGrainValue *spriv = src->priv;
    UserGrainValue *dpriv = dest->priv;
    assign_info(dpriv, spriv);

    g_object_thaw_notify(G_OBJECT(dest));
    gwy_user_grain_value_changed(dest);
}

static GwyResource*
gwy_user_grain_value_copy(GwyResource *resource)
{
    return GWY_RESOURCE(gwy_user_grain_value_duplicate_impl(GWY_SERIALIZABLE(resource)));
}

static void
gwy_user_grain_value_changed(GwyUserGrainValue *usergrainvalue)
{
    gwy_resource_data_changed(GWY_RESOURCE(usergrainvalue));
}

static gboolean
validate(GwyUserGrainValue *usergrainvalue,
         guint domain, guint code,
         GError **error)
{
    UserGrainValue *priv = usergrainvalue->priv;

    // Group physical sanity
    if (!priv->group || !*priv->group) {
        g_set_error(error, domain, code,
                    // TRANSLATORS: Error message.
                    _("Grain value has no group."));
        return FALSE;
    }
    if (!g_utf8_validate(priv->group, -1, NULL)) {
        g_set_error(error, domain, code,
                    // TRANSLATORS: Error message.
                    _("Grain value group is not valid UTF-8."));
        return FALSE;
    }

    // Identifier and symbol physical sanity.
    if (!priv->ident || !*priv->ident) {
        g_set_error(error, domain, code,
                    // TRANSLATORS: Error message.
                    _("Grain value has no identifier."));
        return FALSE;
    }
    if (!gwy_ascii_strisident(priv->ident, "_", NULL)) {
        g_set_error(error, domain, code,
                    // TRANSLATORS: Error message.
                    _("Grain value identifier is not a valid identifier."));
        return FALSE;
    }
    if (priv->symbol && !g_utf8_validate(priv->symbol, -1, NULL)) {
        // XXX: In principle, we should validate the possible Pango markup.
        // But it would induce a weird Pango dependence here.
        g_set_error(error, domain, code,
                    // TRANSLATORS: Error message.
                    _("Grain value symbol is not valid UTF-8."));
        return FALSE;
    }

    // Formula physical sanity
    if (!priv->formula || !*priv->formula) {
        g_set_error(error, domain, code,
                    // TRANSLATORS: Error message.
                    _("Grain value has no formula."));
        return FALSE;
    }
    if (!g_utf8_validate(priv->formula, -1, NULL)) {
        g_set_error(error, domain, code,
                    // TRANSLATORS: Error message.
                    _("Grain value formula is not valid UTF-8."));
        return FALSE;
    }

    // Formula
    gboolean ok = TRUE;
    G_LOCK(test_expr);
    ensure_test_expr();
    if (ok && !gwy_expr_compile(test_expr, priv->formula, NULL)) {
        g_set_error(error, domain, code,
                    // TRANSLATORS: Error message.
                    _("Grain value formula is invalid."));
        ok = FALSE;
    }
    if (ok && !resolve_deps(error, domain, code))
        ok = FALSE;

    G_UNLOCK(test_expr);
    return ok;
}

/**
 * gwy_user_grain_value_new: (constructor)
 *
 * Creates a new user grain value.
 *
 * Returns: (transfer full):
 *          A new free-standing user grain value.
 **/
GwyUserGrainValue*
gwy_user_grain_value_new(void)
{
    return g_object_newv(GWY_TYPE_USER_GRAIN_VALUE, 0, NULL);
}

/**
 * gwy_user_grain_value_get_formula:
 * @usergrainvalue: A user grain value.
 *
 * Gets the formula of a user grain value.
 *
 * Returns: The formula as a string owned by @usergrainvalue.
 **/
const gchar*
gwy_user_grain_value_get_formula(const GwyUserGrainValue *usergrainvalue)
{
    g_return_val_if_fail(GWY_IS_USER_GRAIN_VALUE(usergrainvalue), NULL);
    return usergrainvalue->priv->formula;
}

/**
 * gwy_user_grain_value_set_formula:
 * @usergrainvalue: A user grain value.
 * @formula: New grain value formula.
 * @error: Return location for the error, or %NULL.  The error can be from
 *         either %GWY_USER_GRAIN_VALUE_ERROR or %GWY_EXPR_ERROR domain.
 *
 * Sets the formula of a user grain value.
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
gwy_user_grain_value_set_formula(GwyUserGrainValue *usergrainvalue,
                                 const gchar *formula,
                                 GError **error)
{
    g_return_val_if_fail(GWY_IS_USER_GRAIN_VALUE(usergrainvalue), FALSE);
    g_return_val_if_fail(formula, FALSE);
    UserGrainValue *priv = usergrainvalue->priv;

    if (gwy_strequal(formula, priv->formula))
        return TRUE;

    G_LOCK(test_expr);
    ensure_test_expr();
    gboolean ok = (gwy_expr_compile(test_expr, formula, error)
                   && resolve_deps(error, GWY_USER_GRAIN_VALUE_ERROR,
                                   GWY_USER_GRAIN_VALUE_ERROR_DEPENDS));
    G_UNLOCK(test_expr);

    if (ok) {
        gwy_assign_string(&priv->formula, formula);
        gwy_user_grain_value_changed(usergrainvalue);
    }

    return ok;
}

// Must be called with locked test_expr.
static void
ensure_test_expr(void)
{
    if (test_expr)
        return;

    const gchar* const *idents = _gwy_grain_value_list_builtin_idents();
    test_expr = _gwy_grain_value_new_expr_with_constants();
    test_expr_nidents = g_strv_length((gchar**)idents);
    test_expr_indices = g_slice_alloc((test_expr_nidents + 1)*sizeof(guint));
}

// Must be called with locked test_expr.
static gboolean
resolve_deps(GError **error, guint domain, guint code)
{
    const gchar* const *idents = _gwy_grain_value_list_builtin_idents();
    if (!gwy_expr_resolve_variables(test_expr, test_expr_nidents, idents,
                                    test_expr_indices))
        return TRUE;

    g_set_error(error, domain, code,
                // TRANSLATORS: Error message.
                _("Grain value formula contains unknown variables."));
    return FALSE;
}

/**
 * gwy_user_grain_value_get_group:
 * @usergrainvalue: A user grain value resource.
 *
 * Obtains the group of a user grain value.
 *
 * Returns: The user grain value group.
 **/
const gchar*
gwy_user_grain_value_get_group(const GwyUserGrainValue *usergrainvalue)
{
    g_return_val_if_fail(GWY_IS_USER_GRAIN_VALUE(usergrainvalue), NULL);
    return usergrainvalue->priv->group;
}

/**
 * gwy_user_grain_value_set_group:
 * @usergrainvalue: A user grain value resource.
 * @group: Group name.
 *
 * Sets the group of a user grain value.
 **/
void
gwy_user_grain_value_set_group(GwyUserGrainValue *usergrainvalue,
                               const gchar *group)
{
    g_return_if_fail(GWY_IS_USER_GRAIN_VALUE(usergrainvalue));
    g_return_if_fail(group);
    UserGrainValue *priv = usergrainvalue->priv;
    if (gwy_assign_string(&priv->group, group))
        gwy_user_grain_value_changed(usergrainvalue);
}

/**
 * gwy_user_grain_value_get_symbol:
 * @usergrainvalue: A user grain value resource.
 *
 * Obtains the symbol of a user grain value.
 *
 * Returns: The user grain value symbol.
 **/
const gchar*
gwy_user_grain_value_get_symbol(const GwyUserGrainValue *usergrainvalue)
{
    g_return_val_if_fail(GWY_IS_USER_GRAIN_VALUE(usergrainvalue), NULL);
    UserGrainValue *priv = usergrainvalue->priv;
    if (priv->symbol)
        return priv->symbol;
    return priv->ident;
}

/**
 * gwy_user_grain_value_set_symbol:
 * @usergrainvalue: A user grain value resource.
 * @symbol: Grain value symbol.
 *
 * Sets the symbol of a user grain value.
 **/
void
gwy_user_grain_value_set_symbol(GwyUserGrainValue *usergrainvalue,
                                const gchar *symbol)
{
    g_return_if_fail(GWY_IS_USER_GRAIN_VALUE(usergrainvalue));
    g_return_if_fail(symbol);
    UserGrainValue *priv = usergrainvalue->priv;
    if (gwy_assign_string(&priv->symbol, symbol))
        gwy_user_grain_value_changed(usergrainvalue);
}

/**
 * gwy_user_grain_value_get_ident:
 * @usergrainvalue: A user grain value resource.
 *
 * Obtains the identifier of a user grain value.
 *
 * Returns: The user grain value identifier.
 **/
const gchar*
gwy_user_grain_value_get_ident(const GwyUserGrainValue *usergrainvalue)
{
    g_return_val_if_fail(GWY_IS_USER_GRAIN_VALUE(usergrainvalue), NULL);
    return usergrainvalue->priv->ident;
}

/**
 * gwy_user_grain_value_set_ident:
 * @usergrainvalue: A user grain value resource.
 * @ident: Identifier.
 *
 * Sets the identifier of a user grain value.
 **/
void
gwy_user_grain_value_set_ident(GwyUserGrainValue *usergrainvalue,
                               const gchar *ident)
{
    g_return_if_fail(GWY_IS_USER_GRAIN_VALUE(usergrainvalue));
    g_return_if_fail(ident && gwy_utf8_strisident(ident, more, NULL));
    // XXX: Uniqueness?  Or maybe not here?
    UserGrainValue *priv = usergrainvalue->priv;
    if (gwy_assign_string(&priv->ident, ident))
        gwy_user_grain_value_changed(usergrainvalue);
}

/**
 * gwy_user_grain_value_get_power_x:
 * @usergrainvalue: A user grain value resource.
 *
 * Gets the power of field horizontal lateral units entering a grain value.
 *
 * Returns: The power of horizontal lateral units.
 **/
gint
gwy_user_grain_value_get_power_x(const GwyUserGrainValue *usergrainvalue)
{
    g_return_val_if_fail(GWY_IS_USER_GRAIN_VALUE(usergrainvalue), 0);
    return usergrainvalue->priv->power_x;
}

/**
 * gwy_user_grain_value_set_power_x:
 * @usergrainvalue: A user grain value resource.
 * @power_x: Power of horizontal lateral units which must be between -12 and 12.
 *
 * Sets the power of field horizontal lateral units entering a grain value.
 **/
void
gwy_user_grain_value_set_power_x(GwyUserGrainValue *usergrainvalue,
                                 gint power_x)
{
    g_return_if_fail(GWY_IS_USER_GRAIN_VALUE(usergrainvalue));
    g_return_if_fail(ABS(power_x) <= 12);
    UserGrainValue *priv = usergrainvalue->priv;
    if (power_x != priv->power_x) {
        priv->power_x = power_x;
        gwy_user_grain_value_changed(usergrainvalue);
    }
}

/**
 * gwy_user_grain_value_get_power_y:
 * @usergrainvalue: A user grain value resource.
 *
 * Gets the power of field vertical lateral units entering a grain value.
 *
 * Returns: The power of vertical lateral units.
 **/
gint
gwy_user_grain_value_get_power_y(const GwyUserGrainValue *usergrainvalue)
{
    g_return_val_if_fail(GWY_IS_USER_GRAIN_VALUE(usergrainvalue), 0);
    return usergrainvalue->priv->power_y;
}

/**
 * gwy_user_grain_value_set_power_y:
 * @usergrainvalue: A user grain value resource.
 * @power_y: Power of vertical lateral units which must be between -12 and 12.
 *
 * Sets the power of field vertical lateral units entering a grain value.
 **/
void
gwy_user_grain_value_set_power_y(GwyUserGrainValue *usergrainvalue,
                                 gint power_y)
{
    g_return_if_fail(GWY_IS_USER_GRAIN_VALUE(usergrainvalue));
    g_return_if_fail(ABS(power_y) <= 12);
    UserGrainValue *priv = usergrainvalue->priv;
    if (power_y != priv->power_y) {
        priv->power_y = power_y;
        gwy_user_grain_value_changed(usergrainvalue);
    }
}

/**
 * gwy_user_grain_value_get_power_z:
 * @usergrainvalue: A user grain value resource.
 *
 * Gets the power of field falue units entering a grain value.
 *
 * Returns: The power of value units.
 **/
gint
gwy_user_grain_value_get_power_z(const GwyUserGrainValue *usergrainvalue)
{
    g_return_val_if_fail(GWY_IS_USER_GRAIN_VALUE(usergrainvalue), 0);
    return usergrainvalue->priv->power_z;
}

/**
 * gwy_user_grain_value_set_power_z:
 * @usergrainvalue: A user grain value resource.
 * @power_z: Power of value units which must be between -12 and 12.
 *
 * Sets the power of field value units entering a grain value.
 **/
void
gwy_user_grain_value_set_power_z(GwyUserGrainValue *usergrainvalue,
                                  gint power_z)
{
    g_return_if_fail(GWY_IS_USER_GRAIN_VALUE(usergrainvalue));
    g_return_if_fail(ABS(power_z) <= 12);
    UserGrainValue *priv = usergrainvalue->priv;
    if (power_z != priv->power_z) {
        priv->power_z = power_z;
        gwy_user_grain_value_changed(usergrainvalue);
    }
}

/**
 * gwy_user_grain_value_get_same_units:
 * @usergrainvalue: A user grain value resource.
 *
 * Gets what units a user grain value needs to be the same.
 *
 * Returns: Combination of units @usergrainvalue requires to be the same.
 **/
GwyGrainValueSameUnits
gwy_user_grain_value_get_same_units(const GwyUserGrainValue *usergrainvalue)
{
    g_return_val_if_fail(GWY_IS_USER_GRAIN_VALUE(usergrainvalue), 0);
    return usergrainvalue->priv->same_units;
}

/**
 * gwy_user_grain_value_set_same_units:
 * @usergrainvalue: A user grain value resource.
 * @same_units: Combination of units @usergrainvalue should require to be the
 *              same.
 *
 * Sets whether a user grain value needs the same lateral and/or value units.
 *
 * See #GwyGrainValueSameUnits for a discussion of what grain quantities should
 * require which unit combinations.
 **/
void
gwy_user_grain_value_set_same_units(GwyUserGrainValue *usergrainvalue,
                                    GwyGrainValueSameUnits same_units)
{
    g_return_if_fail(GWY_IS_USER_GRAIN_VALUE(usergrainvalue));
    g_return_if_fail(gwy_grain_value_same_units_is_valid(same_units));
    UserGrainValue *priv = usergrainvalue->priv;
    if (same_units != priv->same_units) {
        priv->same_units = same_units;
        gwy_user_grain_value_changed(usergrainvalue);
    }
}

/**
 * gwy_user_grain_value_get_is_angle:
 * @usergrainvalue: A user grain value resource.
 *
 * Gets whether a user grain value represents an angle.
 *
 * Returns: %TRUE if the grain value represents an angle.
 **/
gboolean
gwy_user_grain_value_get_is_angle(const GwyUserGrainValue *usergrainvalue)
{
    g_return_val_if_fail(GWY_IS_USER_GRAIN_VALUE(usergrainvalue), FALSE);
    return usergrainvalue->priv->is_angle;
}

/**
 * gwy_user_grain_value_set_is_angle:
 * @usergrainvalue: A user grain value resource.
 * @is_angle: %TRUE if the grain value should require same lateral and value
 *              units, %FALSE if it should not.
 *
 * Sets whether a user grain value represents an angle.
 *
 * Grain values that represent angles are calculated in radians and reported
 * as unitless.  To present them to the user they usually should be converted
 * to degrees.  To tell them apart from other unitless values they should have
 * @is_angle set to %TRUE.
 **/
void
gwy_user_grain_value_set_is_angle(GwyUserGrainValue *usergrainvalue,
                                  gboolean is_angle)
{
    g_return_if_fail(GWY_IS_USER_GRAIN_VALUE(usergrainvalue));
    UserGrainValue *priv = usergrainvalue->priv;
    is_angle = !!is_angle;
    if (is_angle != priv->is_angle) {
        priv->is_angle = is_angle;
        gwy_user_grain_value_changed(usergrainvalue);
    }
}

static gchar*
gwy_user_grain_value_dump(GwyResource *resource)
{
    GwyUserGrainValue *usergrainvalue = GWY_USER_GRAIN_VALUE(resource);
    UserGrainValue *priv = usergrainvalue->priv;

    GString *text = g_string_new(NULL);
    g_string_append_printf(text, "formula %s\n", priv->formula);
    g_string_append_printf(text, "group %s\n", priv->group);
    g_string_append_printf(text, "ident %s\n", priv->ident);
    if (priv->symbol)
        g_string_append_printf(text, "symbol %s\n", priv->symbol);
    if (priv->power_x)
        g_string_append_printf(text, "power_x %d\n", priv->power_x);
    if (priv->power_y)
        g_string_append_printf(text, "power_y %d\n", priv->power_y);
    if (priv->power_z)
        g_string_append_printf(text, "power_z %d\n", priv->power_z);
    if (priv->same_units)
        g_string_append_printf(text, "same_units %u\n", (guint)priv->same_units);
    if (priv->is_angle)
        g_string_append(text, "is_angle 1\n");

    return g_string_free(text, FALSE);;
}

static gboolean
gwy_user_grain_value_parse(GwyResource *resource,
                           GwyStrLineIter *iter,
                           GError **error)
{
    GwyUserGrainValue *usergrainvalue = GWY_USER_GRAIN_VALUE(resource);
    UserGrainValue *priv = usergrainvalue->priv;

    while (TRUE) {
        GwyResourceLineType line;
        gchar *key, *value;

        line = gwy_resource_parse_param_line(iter, &key, &value, error);
        if (line == GWY_RESOURCE_LINE_NONE)
            break;
        if (line != GWY_RESOURCE_LINE_OK)
            return FALSE;

        if (gwy_strequal(key, "power_x"))
            priv->power_x = strtol(value, NULL, 10);
        else if (gwy_strequal(key, "power_y"))
            priv->power_y = strtol(value, NULL, 10);
        else if (gwy_strequal(key, "power_z"))
            priv->power_z = strtol(value, NULL, 10);
        else if (gwy_strequal(key, "same_units"))
            priv->same_units = strtol(value, NULL, 10);
        else if (gwy_strequal(key, "is_angle"))
            priv->is_angle = !!strtol(value, NULL, 10);
        else if (gwy_strequal(key, "formula"))
            gwy_assign_string(&priv->formula, value);
        else if (gwy_strequal(key, "group"))
            gwy_assign_string(&priv->group, value);
        else if (gwy_strequal(key, "ident"))
            gwy_assign_string(&priv->ident, value);
        else if (gwy_strequal(key, "symbol"))
            gwy_assign_string(&priv->symbol, value);
        else
            g_warning("Ignoring unknown GwyUserGrainValue key ‘%s’.", key);
    }

    if (!validate(usergrainvalue,
                  GWY_RESOURCE_ERROR, GWY_RESOURCE_ERROR_DATA, error))
        return FALSE;

    return TRUE;
}

/**
 * gwy_user_grain_value_error_quark:
 *
 * Provides error domain for user-defined grain value manipulation.
 *
 * See and use %GWY_USER_GRAIN_VALUE_ERROR.
 *
 * Returns: The error domain.
 **/
G_DEFINE_QUARK(gwy-user-grain-value-error-quark, gwy_user_grain_value_error);

/************************** Documentation ****************************/

/**
 * SECTION: user-grain-value
 * @title: GwyUserGrainValue
 * @short_description: User-defined grain value
 **/

/**
 * GwyUserGrainValue:
 *
 * Object represnting a user grain value.
 *
 * The #GwyUserGrainValue struct contains private data only and should be
 * accessed using the functions below.
 **/

/**
 * GwyUserGrainValueClass:
 *
 * Class of user grain values.
 *
 * #GwyUserGrainValueClass does not contain any public members.
 **/

/**
 * gwy_user_grain_value_duplicate:
 * @usergrainvalue: A user grain value.
 *
 * Duplicates a user grain value.
 *
 * This is a convenience wrapper of gwy_serializable_duplicate().
 **/

/**
 * gwy_user_grain_value_assign:
 * @dest: Destination user grain value.
 * @src: Source user grain value.
 *
 * Copies the value of a user grain value.
 *
 * This is a convenience wrapper of gwy_serializable_assign().
 **/

/**
 * gwy_user_grain_values:
 *
 * Gets inventory with all the user grain values.
 *
 * Returns: User grain value inventory.
 **/

/**
 * gwy_user_grain_values_get:
 * @name: User grain value name.  May be %NULL to get the default
 *        function.
 *
 * Convenience function to get a user grain value from
 * gwy_user_grain_values() by name.
 *
 * Returns: User grain value identified by @name or the default user
 *          grain value if @name does not exist.
 **/

/**
 * GWY_USER_GRAIN_VALUE_ERROR:
 *
 * Error domain for user-defined grain value manipulation.  Errors in this
 * domain will be from the #GwyUserGrainValueError enumeration. See #GError for
 * information on error domains.
 **/

/**
 * GwyUserGrainValueError:
 * @GWY_USER_GRAIN_VALUE_ERROR_DEPENDS: The grain value has unresolved/invalid
 *                                      dependences on other values.
 *
 * Error codes returned by user-defined grain value manipulation.
 **/

/**
 * GwyGrainValueSameUnits:
 * @GWY_GRAIN_VALUE_SAME_UNITS_NONE: No requirements.
 * @GWY_GRAIN_VALUE_SAME_UNITS_LATERAL: Both lateral coordinates must be the
 *                                      same physical quantity.
 * @GWY_GRAIN_VALUE_SAME_UNITS_ALL: Both lateral coordinates and the value must
 *                                  be the same physical quantity.
 *
 * Type of grain value units requirement.
 *
 * Grain values that represent a single coordinate, for instance mean value or
 * @x-position of the centre, or are plain products of coordinates, such as
 * volume, are meaningful for all combinations of field units.  Hence they do
 * note have any special requirements which corresponds to
 * %GWY_GRAIN_VALUE_SAME_UNITS_NONE.
 *
 * However, some grain values, such as radii or angles in the plane, are
 * meaningful only if both lateral coordinates are the same physical quantity.
 * These grain values require %GWY_GRAIN_VALUE_SAME_UNITS_LATERAL.
 *
 * Other grain values, such as surface area, are meaningful only if height is
 * also the same physical quantity as lateral dimensions.  These grain values
 * require %GWY_GRAIN_VALUE_SAME_UNITS_ALL.
 *
 * Note a value may represent a single coordinate but cannot be meaningfully
 * calculated for arbitrary unit combinations, for instance @x-coordinate of
 * inscribed disc centre.  Such value requires whatever is necessary for the
 * <emphasis>calculation</emphasis>.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
