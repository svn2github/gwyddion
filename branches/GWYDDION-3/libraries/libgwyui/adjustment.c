/*
 *  $Id$
 *  Copyright (C) 2013 David Nečas (Yeti).
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
#include "libgwy/math.h"
#include "libgwy/macros.h"
#include "libgwy/object-utils.h"
#include "libgwyui/adjustment.h"

enum {
    PROP_0,
    PROP_DEFAULT,
    N_PROPS
};

typedef struct _GwyAdjustmentPrivate Adjustment;

struct _GwyAdjustmentPrivate {
    gdouble defaultval;

    // Property-based.
    GBinding *binding;
};

static void     gwy_adjustment_dispose     (GObject *object);
static void     gwy_adjustment_set_property(GObject *object,
                                            guint prop_id,
                                            const GValue *value,
                                            GParamSpec *pspec);
static void     gwy_adjustment_get_property(GObject *object,
                                            guint prop_id,
                                            GValue *value,
                                            GParamSpec *pspec);
static gboolean set_default                (GwyAdjustment *adj,
                                            gdouble value);

static GParamSpec *properties[N_PROPS];

G_DEFINE_TYPE(GwyAdjustment, gwy_adjustment, GTK_TYPE_ADJUSTMENT);

static void
gwy_adjustment_class_init(GwyAdjustmentClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    g_type_class_add_private(klass, sizeof(Adjustment));

    gobject_class->get_property = gwy_adjustment_get_property;
    gobject_class->set_property = gwy_adjustment_set_property;
    gobject_class->dispose = gwy_adjustment_dispose;

    properties[PROP_DEFAULT]
        = g_param_spec_double("default",
                              "default",
                              "Default value of the adjustment.",
                              -G_MAXDOUBLE, G_MAXDOUBLE, 0.0,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    for (guint i = 1; i < N_PROPS; i++)
        g_object_class_install_property(gobject_class, i, properties[i]);
}

static void
gwy_adjustment_init(GwyAdjustment *adjustment)
{
    adjustment->priv = G_TYPE_INSTANCE_GET_PRIVATE(adjustment,
                                                   GWY_TYPE_ADJUSTMENT,
                                                   Adjustment);
}

static void
gwy_adjustment_dispose(GObject *object)
{
    Adjustment *priv = GWY_ADJUSTMENT(object)->priv;
    // Clean up in case the binding survives us somehow.
    if (priv->binding) {
        g_object_remove_weak_pointer(G_OBJECT(priv->binding),
                                     (gpointer*)&priv->binding);
        GWY_OBJECT_UNREF(priv->binding);
    }
    G_OBJECT_CLASS(gwy_adjustment_parent_class)->dispose(object);
}

static void
gwy_adjustment_set_property(GObject *object,
                            guint prop_id,
                            const GValue *value,
                            GParamSpec *pspec)
{
    GwyAdjustment *adj = GWY_ADJUSTMENT(object);

    switch (prop_id) {
        case PROP_DEFAULT:
        set_default(adj, g_value_get_double(value));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_adjustment_get_property(GObject *object,
                            guint prop_id,
                            GValue *value,
                            GParamSpec *pspec)
{
    GwyAdjustment *adj = GWY_ADJUSTMENT(object);
    Adjustment *priv = adj->priv;

    switch (prop_id) {
        case PROP_DEFAULT:
        g_value_set_double(value, priv->defaultval);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

/**
 * gwy_adjustment_new: (constructor)
 *
 * Creates a new adjustable bounded value.
 *
 * Returns: A newly created adjustment.
 **/
GwyAdjustment*
gwy_adjustment_new(void)
{
    return g_object_newv(GWY_TYPE_ADJUSTMENT, 0, NULL);
}

/**
 * gwy_adjustment_new_set: (constructor)
 * @value: Initial value.
 * @defaultval: Default value.
 * @lower: Minimum value.
 * @upper: Maximum value.
 * @step_increment: Step increment.
 * @page_increment: Page increment.
 *
 * Creates a new adjustable bounded value with given ranges and value.
 *
 * Returns: A newly created adjustment.
 **/
GwyAdjustment*
gwy_adjustment_new_set(gdouble value,
                       gdouble defaultval,
                       gdouble lower,
                       gdouble upper,
                       gdouble step_increment,
                       gdouble page_increment)
{
    GwyAdjustment *adj = gwy_adjustment_new();
    gtk_adjustment_configure(GTK_ADJUSTMENT(adj),
                             value, lower, upper,
                             step_increment, page_increment, 0.0);
    adj->priv->defaultval = defaultval;
    return adj;
}

/**
 * gwy_adjustment_new_for_property:
 * @object: (transfer none):
 *          An object.
 * @propname: Name of property to create adjustment for.  The property must be
 *            of integral or double type.
 *
 * Creates a new adjustment with ranges, default and current value given by
 * the property of an object.
 *
 * Changes to the property value are reflected by changes in the adjustment
 * value and vice versa.  The adjustment does not take a reference to @object.
 * If @object ceases to exist before the adjustment does the adjustment will
 * remain configured the same way, just the value will not be synchronised with
 * anything.
 *
 * You can limit the adjustment range further than what is permitted by the
 * property after the construction.  Extending it is not allowed.
 *
 * Returns: A newly created adjustment.
 **/
GwyAdjustment*
gwy_adjustment_new_for_property(GObject *object,
                                const gchar *propname)
{
    GwyAdjustment *adj = gwy_adjustment_new();
    g_return_val_if_fail(G_IS_OBJECT(object), adj);
    Adjustment *priv = adj->priv;

    g_return_val_if_fail(propname, adj);
    GObjectClass *klass = G_OBJECT_GET_CLASS(object);
    GParamSpec *property = g_object_class_find_property(klass, propname);
    g_return_val_if_fail(property, adj);

    gdouble lower, upper, defaultval;
    gboolean is_integral = TRUE;

    // Try the types by descending probability.
    if (G_IS_PARAM_SPEC_DOUBLE(property)) {
        GParamSpecDouble *pspec = G_PARAM_SPEC_DOUBLE(property);
        lower = pspec->minimum;
        upper = pspec->maximum;
        defaultval = pspec->default_value;
        is_integral = FALSE;
    }
    else if (G_IS_PARAM_SPEC_UINT(property)) {
        GParamSpecUInt *pspec = G_PARAM_SPEC_UINT(property);
        lower = pspec->minimum;
        upper = pspec->maximum;
        defaultval = pspec->default_value;
    }
    else if (G_IS_PARAM_SPEC_INT(property)) {
        GParamSpecInt *pspec = G_PARAM_SPEC_INT(property);
        lower = pspec->minimum;
        upper = pspec->maximum;
        defaultval = pspec->default_value;
    }
    else if (G_IS_PARAM_SPEC_LONG(property)) {
        GParamSpecLong *pspec = G_PARAM_SPEC_LONG(property);
        lower = pspec->minimum;
        upper = pspec->maximum;
        defaultval = pspec->default_value;
    }
    else if (G_IS_PARAM_SPEC_ULONG(property)) {
        GParamSpecULong *pspec = G_PARAM_SPEC_ULONG(property);
        lower = pspec->minimum;
        upper = pspec->maximum;
        defaultval = pspec->default_value;
    }
    else if (G_IS_PARAM_SPEC_FLOAT(property)) {
        GParamSpecFloat *pspec = G_PARAM_SPEC_FLOAT(property);
        lower = pspec->minimum;
        upper = pspec->maximum;
        defaultval = pspec->default_value;
        is_integral = FALSE;
    }
    else {
        g_critical("Cannot create adjustment for value type %s.",
                   g_type_name(property->value_type));
        return adj;
    }

    // Construct a harmless value for the gtk_adjustment_configure() call.
    gdouble step_increment, page_increment, value;
    // XXX: This needs more elaboration.
    if (is_integral) {
        value = gwy_round(0.5*(lower + upper));
        step_increment = 1.0;
        page_increment = MIN(upper - lower, 10.0);
    }
    else {
        value = 0.5*(lower + upper);
        step_increment = MAX((upper - lower)/100000.0, fabs(lower));
        step_increment = gwy_powi(10.0, gwy_round(log10(step_increment)));
        page_increment = sqrt(step_increment*(upper - lower));
        page_increment = gwy_powi(10.0, gwy_round(log10(page_increment)));
    }

    gtk_adjustment_configure(GTK_ADJUSTMENT(adj),
                             value, lower, upper,
                             step_increment, page_increment, 0.0);
    priv->defaultval = defaultval;
    // This takes care of value synchronisation from the property to us.
    priv->binding = g_object_bind_property(object, propname,
                                           adj, "value",
                                           G_BINDING_BIDIRECTIONAL
                                           | G_BINDING_SYNC_CREATE);
    g_object_add_weak_pointer(G_OBJECT(priv->binding),
                              (gpointer*)&priv->binding);

    return adj;
}

/**
 * gwy_adjustment_set_default:
 * @adjustment: An adjustment.
 * @defaultval: Value which should become the default.
 *
 * Sets the default value of an a adjustment.
 *
 * It is not allowed to change the default if the adjustment is backed by an
 * object property.
 *
 * Generally, the default value should be within the adjustment range (at least
 * when it is used).
 **/
void
gwy_adjustment_set_default(GwyAdjustment *adj,
                           gdouble defaultval)
{
    g_return_if_fail(GWY_IS_ADJUSTMENT(adj));
    if (!set_default(adj, defaultval))
        return;

    g_object_notify_by_pspec(G_OBJECT(adj), properties[PROP_DEFAULT]);
}

/**
 * gwy_adjustment_get_default:
 * @adjustment: An adjustment.
 *
 * Gets the default value of an adjustment.
 *
 * Returns: The default value of the adjustment.
 **/
gdouble
gwy_adjustment_get_default(const GwyAdjustment *adj)
{
    g_return_val_if_fail(GWY_IS_ADJUSTMENT(adj), 0.0);
    return adj->priv->defaultval;
}

/**
 * gwy_adjustment_reset:
 * @adjustment: An adjustment.
 *
 * Resets the value of an adjustment to the default.
 *
 * Calling gwy_adjustment_reset() if the default value is not within
 * [@lower,@upper] is a bad idea.
 **/
void
gwy_adjustment_reset(GwyAdjustment *adj)
{
    g_return_if_fail(GWY_IS_ADJUSTMENT(adj));
    GtkAdjustment *gtkadj = GTK_ADJUSTMENT(adj);
    Adjustment *priv = adj->priv;
    // Ensure the new value is within the range.
    gdouble dval = priv->defaultval,
            lower = gtk_adjustment_get_lower(gtkadj),
            upper = gtk_adjustment_get_upper(gtkadj),
            value = gtk_adjustment_get_value(gtkadj);
    dval = CLAMP(dval, lower, upper);
    if (dval == value)
        return;

    gtk_adjustment_set_value(gtkadj, dval);
}

/**
 * gwy_adjustment_get_object:
 * @adjustment: An adjustment.
 *
 * Gets the object whose property is backing an adjustment.
 *
 * Returns: (allow-none) (transfer none):
 *          The object passed to gwy_adjustment_new_for_property().  Hence,
 *          possibly %NULL.
 **/
GObject*
gwy_adjustment_get_object(const GwyAdjustment *adj)
{
    g_return_val_if_fail(GWY_IS_ADJUSTMENT(adj), NULL);
    Adjustment *priv = adj->priv;
    return priv->binding ? g_binding_get_source(priv->binding) : NULL;
}

/**
 * gwy_adjustment_get_property_name:
 * @adjustment: An adjustment.
 *
 * Gets the name of property which is backing an adjustment.
 *
 * Returns: (allow-none):
 *          The property name of object passed to
 *          gwy_adjustment_new_for_property().
 *          Hence, possibly %NULL.
 **/
const gchar*
gwy_adjustment_get_property_name(const GwyAdjustment *adj)
{
    g_return_val_if_fail(GWY_IS_ADJUSTMENT(adj), NULL);
    Adjustment *priv = adj->priv;
    return priv->binding ? g_binding_get_source_property(priv->binding) : NULL;
}

static gboolean
set_default(GwyAdjustment *adj,
            gdouble defaultval)
{
    Adjustment *priv = adj->priv;
    g_return_val_if_fail(!priv->binding, FALSE);
    if (defaultval == priv->defaultval)
        return FALSE;

    priv->defaultval = defaultval;
    return TRUE;
}

/**
 * SECTION: adjustment
 * @title: GwyAdjustment
 * @short_description: Adjustable bounded value with a default.
 *
 * #GwyAdjustment extends #GtkAdjustment in one aspect: It has a default value.
 * Thus it is possible to reset it to default generically with
 * gwy_adjustment_reset().
 *
 * #GwyAdjustment can also be bound to the value of a numeric property of an
 * arbitrary object by creating it with gwy_adjustment_new_for_property().
 **/

/**
 * GwyAdjustment:
 *
 * Object representing an adjustable bounded value.
 *
 * The #GwyAdjustment struct contains private data only and should be accessed
 * using the functions below.
 **/

/**
 * GwyAdjustmentClass:
 *
 * Class of objects representing adjustable bounded values.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
