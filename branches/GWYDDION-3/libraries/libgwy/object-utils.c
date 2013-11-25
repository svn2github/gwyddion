/*
 *  $Id$
 *  Copyright (C) 2011-2013 David Nečas (Yeti).
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

#include <stdarg.h>
#include <string.h>
#include "libgwy/macros.h"
#include "libgwy/strfuncs.h"
#include "libgwy/serializable-boxed.h"
#include "libgwy/object-utils.h"

/**
 * gwy_set_user_func:
 * @func: (allow-none):
 *        New function pointer.
 * @data: New user data for @func.
 * @destroy: New destroy notifier for @data.
 * @func_field: Pointer to location storing the current function to
 *              be replaced by @func.
 * @data_field: Pointer to location storing the current data to
 *              be replaced by @data.
 * @destroy_field: Pointer to location storing the current data to
 *                 be replaced by @destroy.
 *
 * Sets user function with data, calling the destroy notifier if any is set.
 *
 * Returns: %TRUE if either the function or its data has changed.  If only the
 *          destroy notifier is changed it is called and updated but %FALSE is
 *          returned, assuming that a change of the destroy notifier does not
 *          influence the function.
 **/
gboolean
gwy_set_user_func(gpointer func,
                  gpointer data,
                  GDestroyNotify destroy,
                  gpointer func_field,
                  gpointer data_field,
                  GDestroyNotify *destroy_field)
{
    gpointer *pfunc = (gpointer*)func_field, *pdata = (gpointer*)data_field;

    if (*pfunc == func && *pdata == data) {
        if (*destroy_field == destroy)
            return FALSE;

        if (*destroy_field)
            (*destroy_field)(data);
        *destroy_field = destroy;
        // Since data and function have not changed we still return FALSE.
        // This can meaningfully happen if data is reference-counted.
        return FALSE;
    }

    if (*destroy_field)
        (*destroy_field)(*pdata);

    *pfunc = func;
    *pdata = data;
    *destroy_field = destroy;
    return TRUE;
}

/**
 * gwy_set_member_object:
 * @instance: An object instance.
 * @member_object: Another object to be owned by @instanced, or %NULL.
 * @expected_type: The type of @member_object.  It is checked and a critical
 *                 message is emitted if it does not conform.
 * @member_field: Pointer to location storing the current member object to
 *                be replaced by @member_object.
 * @...: List of quadruplets of the form signal name, #GCallback callback,
 *       #gulong pointer to location to hold the signal handler id, and
 *       #GConnectFlags connection flags.
 *
 * Replaces a member object of another object, handling signal connection and
 * disconnection.
 *
 * If @member_object is not %NULL a reference is taken, sinking any floating
 * objects (and conversely, the reference to the previous member object is
 * released).
 *
 * The purpose is to simplify bookkeeping in classes that have settable
 * member objects and (usually but not necessarily) need to connect to some
 * signals of these member objects.  Since this function both connects and
 * disconnects signals it must be always called with the same set of signals,
 * including callbacks and flags, for a specific member object.
 *
 * Example for a <type>GwyFoo</type> class owning a #GwyGradient member object,
 * assuming the usual conventions:
 * |[
 * typedef struct _GwyFooPrivate GwyFooPrivate;
 *
 * struct _GwyFooPrivate {
 *     GwyGradient *gradient;
 *     gulong gradient_data_changed_id;
 * };
 *
 * static gboolean
 * set_gradient(GwyFoo *foo)
 * {
 *     GwyFooPrivate *priv = G_TYPE_INSTANCE_GET_PRIVATE(foo, GWY_TYPE_FOO,
 *                                                       GwyFooPrivate);
 *     if (!gwy_set_member_object(foo, gradient, GWY_TYPE_GRADIENT,
 *                                &priv->gradient,
 *                                "data-changed", &foo_gradient_data_changed,
 *                                &priv->gradient_data_changed_id,
 *                                G_CONNECT_SWAPPED,
 *                                NULL))
 *         return FALSE;
 *
 *     // Do whatever else needs to be done if the gradient changes.
 *     return TRUE;
 * }
 * ]|
 * The gradient setter then usually only calls
 * <function>set_gradient()</function> and disposing of the member object again
 * only calls <function>set_gradient()</function> but with %NULL gradient.
 *
 * Returns: %TRUE if @member_field was changed.  %FALSE means the new
 *          member is identical to the current one and the function reduced to
 *          no-op (or that an assertion faled).
 **/
gboolean
gwy_set_member_object(gpointer instance,
                      gpointer member_object,
                      GType expected_type,
                      gpointer member_field,
                      ...)
{
    gpointer *pmember = (gpointer*)member_field;
    gpointer old_member = *pmember;

    if (old_member == member_object)
        return FALSE;

    g_return_val_if_fail(!member_object
                         || G_TYPE_CHECK_INSTANCE_TYPE(member_object,
                                                       expected_type),
                         FALSE);

    if (member_object)
        g_object_ref_sink(member_object);

    if (old_member) {
        va_list ap;
        va_start(ap, member_field);
        for (const gchar *signal_name = va_arg(ap, const gchar*);
             signal_name;
             signal_name = va_arg(ap, const gchar*)) {
            G_GNUC_UNUSED GCallback handler = va_arg(ap, GCallback);
            gulong *handler_id = va_arg(ap, gulong*);
            G_GNUC_UNUSED GConnectFlags flags = va_arg(ap, GConnectFlags);
            g_signal_handler_disconnect(old_member, *handler_id);
            *handler_id = 0;
        }
        va_end(ap);
    }

    *pmember = member_object;

    if (member_object) {
        va_list ap;
        va_start(ap, member_field);
        for (const gchar *signal_name = va_arg(ap, const gchar*);
             signal_name;
             signal_name = va_arg(ap, const gchar*)) {
            GCallback handler = va_arg(ap, GCallback);
            gulong *handler_id = va_arg(ap, gulong*);
            GConnectFlags flags = va_arg(ap, GConnectFlags);
            *handler_id = g_signal_connect_data(member_object, signal_name,
                                                handler, instance, NULL,
                                                flags);
        }
        va_end(ap);
    }

    if (old_member)
        g_object_unref(old_member);

    return TRUE;
}

/**
 * gwy_assign_string:
 * @target: Pointer to target string, typically a struct field.
 * @newvalue: New value of the string, may be %NULL.
 *
 * Assigns a string, checking for equality and handling %NULL<!-- -->s.
 *
 * This function simplifies handling of string value setters.
 *
 * The new value is duplicated and the old string is freed in a safe manner
 * (it is possible to pass a pointer somewhere within the old value as the
 * new value, for instance).  Any of the old and new value can be %NULL.  If
 * both values are equal (including both unset), the function returns %FALSE.
 *
 * Returns: %TRUE if the target string has changed.
 **/
gboolean
gwy_assign_string(gchar **target,
                  const gchar *newvalue)
{
    if (*target && newvalue) {
        if (!gwy_strequal(*target, newvalue)) {
            gchar *old = *target;
            *target = g_strdup(newvalue);
            g_free(old);
            return TRUE;
        }
    }
    else if (*target) {
        g_free(*target);
        *target = NULL;
        return TRUE;
    }
    else if (newvalue) {
        *target = g_strdup(newvalue);
        return TRUE;
    }
    return FALSE;
}

/**
 * gwy_assign_boxed:
 * @target: Pointer to target boxed, typically a struct field.
 * @newvalue: New value of the boxed, may be %NULL.
 *
 * Assigns a serialisable boxed value, checking for equality and handling
 * %NULL<!-- -->s.
 *
 * This function simplifies handling of serialisable boxed value setters,
 * using gwy_serializable_boxed_equal() and gwy_serializable_boxed_assign()
 * for correct comparison and value assignment.
 *
 * Any of the old and new value can be %NULL.  If both values are equal
 * (including both unset), the function returns %FALSE.
 *
 * Returns: %TRUE if the target boxed has changed.
 **/
gboolean
gwy_assign_boxed(gpointer *target,
                 gconstpointer newvalue,
                 GType boxed_type)
{
    if (*target && newvalue) {
        if (!gwy_serializable_boxed_equal(boxed_type, *target, newvalue)) {
            gwy_serializable_boxed_assign(boxed_type, *target, newvalue);
            return TRUE;
        }
    }
    else if (*target) {
        g_boxed_free(boxed_type, *target);
        *target = NULL;
        return TRUE;
    }
    else if (newvalue) {
        *target = g_boxed_copy(boxed_type, newvalue);
        return TRUE;
    }
    return FALSE;
}

/**
 * gwy_override_class_properties:
 * @oclass: An object class.
 * @properties: Array of properties, indexed by the property id, to store
 *              the #GParamSpec<!-- -->s of the overriden properties.
 * @...: List of couples of the form property name, property id.
 *
 * Overrides a set of object class properties.
 *
 * The param spec of each property is looked up and stored into @properties
 * at position corresponding to its id.  This is useful if you keep the array
 * of param specs for the class around.
 **/
void
gwy_override_class_properties(GObjectClass *oclass,
                              GParamSpec **properties,
                              ...)
{
    g_return_if_fail(G_IS_OBJECT_CLASS(oclass));
    g_return_if_fail(properties);

    va_list ap;

    va_start(ap, properties);
    for (const gchar *prop_name = va_arg(ap, const gchar*);
         prop_name;
         prop_name = va_arg(ap, const gchar*)) {
        guint prop_id = va_arg(ap, guint);
        g_object_class_override_property(oclass, prop_id, prop_name);
    }
    va_end(ap);


    guint nprops = 0;
    GParamSpec **pspecs = g_object_class_list_properties(oclass, &nprops);

    va_start(ap, properties);
    for (const gchar *prop_name = va_arg(ap, const gchar*);
         prop_name;
         prop_name = va_arg(ap, const gchar*)) {
        guint prop_id = va_arg(ap, guint);
        gboolean ok = FALSE;
        for (guint i = 0; !ok && i < nprops; i++) {
            if (gwy_strequal(prop_name, pspecs[i]->name)) {
                properties[prop_id] = pspecs[i];
                ok = TRUE;
            }
        }
        if (!ok) {
            g_critical("Cannot find property %s after overriding "
                       "it in class %s.",
                       G_OBJECT_CLASS_NAME(oclass), prop_name);
        }
    }
    va_end(ap);

    g_free(pspecs);
}

static void
gather_children_recursively(GArray *array,
                            GType type,
                            gboolean concrete)
{
    guint n;
    GType *children = g_type_children(type, &n);

    for (guint i = 0; i < n; i++) {
        GType ctype = children[i];
        if (!concrete || !G_TYPE_IS_ABSTRACT(ctype))
            g_array_append_val(array, ctype);

        gather_children_recursively(array, ctype, concrete);
    }

    g_free(children);
}

/**
 * gwy_all_type_children:
 * @type: A #GType, presumably derivable and instantiatable.
 * @concrete: %TRUE to gather only concrete types (i.e. those we can actually
 *            create instances of), %FALSE to gather all types, even abstract.
 * @n: (out) (allow-none):
 *     Optional return location for the number of items in the returned array.
 *
 * Lists all children of given type, no matter how deeply derived.
 *
 * Returns: (array length=n) (transfer full):
 *          A newly allocated and zero-terminated array of type IDs, listing
 *          the child types. It must be freed with g_free().
 **/
GType*
gwy_all_type_children(GType type,
                      gboolean concrete,
                      guint *n)
{
    GArray *result = g_array_new(FALSE, FALSE, sizeof(GType));
    gather_children_recursively(result, type, concrete);
    GWY_MAYBE_SET(n, result->len);
    return (GType*)g_array_free(result, FALSE);
}

static GParamSpec*
replicate_param_spec(GParamSpec *sourcepspec)
{
    GType type = G_PARAM_SPEC_TYPE(sourcepspec),
          value_type = sourcepspec->value_type;
    const gchar *name = sourcepspec->name;
    const gchar *nick = g_param_spec_get_nick(sourcepspec);
    const gchar *blurb = g_param_spec_get_blurb(sourcepspec);
    gchar *pname = NULL, *pnick = NULL, *pblurb = NULL;
    GParamFlags flags = sourcepspec->flags;
    GParamSpec *pspec = NULL;

    if (!(flags & G_PARAM_STATIC_NAME))
        name = pname = g_strdup(name);
    if (!(flags & G_PARAM_STATIC_NICK))
        nick = pnick = g_strdup(nick);
    if (!(flags & G_PARAM_STATIC_BLURB))
        blurb = pblurb = g_strdup(blurb);

    // Require exact type matches for replication.  Otherwise it would not
    // be replication...
    if (type == G_TYPE_PARAM_DOUBLE) {
        GParamSpecDouble *sspec = G_PARAM_SPEC_DOUBLE(sourcepspec);
        pspec = g_param_spec_double(name, nick, blurb,
                                    sspec->minimum, sspec->maximum,
                                    sspec->default_value,
                                    flags);
        G_PARAM_SPEC_DOUBLE(pspec)->epsilon = sspec->epsilon;
    }
    else if (type == G_TYPE_PARAM_BOOLEAN) {
        GParamSpecBoolean *sspec = G_PARAM_SPEC_BOOLEAN(sourcepspec);
        pspec = g_param_spec_boolean(name, nick, blurb,
                                     sspec->default_value,
                                     flags);
    }
    else if (type == G_TYPE_PARAM_INT) {
        GParamSpecInt *sspec = G_PARAM_SPEC_INT(sourcepspec);
        pspec = g_param_spec_int(name, nick, blurb,
                                 sspec->minimum, sspec->maximum,
                                 sspec->default_value,
                                 flags);
    }
    else if (type == G_TYPE_PARAM_UINT) {
        GParamSpecUInt *sspec = G_PARAM_SPEC_UINT(sourcepspec);
        pspec = g_param_spec_uint(name, nick, blurb,
                                  sspec->minimum, sspec->maximum,
                                  sspec->default_value,
                                  flags);
    }
    else if (type == G_TYPE_PARAM_ENUM) {
        GParamSpecEnum *sspec = G_PARAM_SPEC_ENUM(sourcepspec);
        pspec = g_param_spec_enum(name, nick, blurb,
                                  value_type, sspec->default_value,
                                  flags);
    }
    else if (type == G_TYPE_PARAM_FLAGS) {
        GParamSpecFlags *sspec = G_PARAM_SPEC_FLAGS(sourcepspec);
        pspec = g_param_spec_flags(name, nick, blurb,
                                   value_type, sspec->default_value,
                                   flags);
    }
    else if (type == G_TYPE_PARAM_STRING) {
        GParamSpecString *sspec = G_PARAM_SPEC_STRING(sourcepspec);
        pspec = g_param_spec_string(name, nick, blurb,
                                    sspec->default_value,
                                    flags);
    }
    else if (type == G_TYPE_PARAM_BOXED) {
        pspec = g_param_spec_boxed(name, nick, blurb, value_type, flags);
    }
    else if (type == G_TYPE_PARAM_OBJECT) {
        pspec = g_param_spec_object(name, nick, blurb, value_type, flags);
    }
    else {
        g_critical("Cannot replicate param spec of type %s.",
                   G_PARAM_SPEC_TYPE_NAME(sourcepspec));
    }

    GWY_FREE(pname);
    GWY_FREE(pnick);
    GWY_FREE(pblurb);

    return pspec;
}

/**
 * gwy_replicate_class_properties:
 * @klass: Target object class.
 * @fromtype: Type of source object class.
 * @properties: Optional array where to store the created #GParamSpec objects,
 *              starting from index @id.  Pass %NULL to ignore.
 * @id: Property id of the first property to install, and index of the item in
 *      @properties corresponding to the first property (if given).
 * @...: First property name, second property name, ... Terminated with %NULL.
 *
 * Replicates properties from one class to another.
 *
 * Returns: Property id that follows all the assigned properties.  Equal to
 *          @id + number of property names (except in case of catastrophic
 *          failure).
 **/
guint
gwy_replicate_class_properties(GObjectClass *klass,
                               GType fromtype,
                               GParamSpec **properties,
                               guint id,
                               ...)
{
    g_return_val_if_fail(G_IS_OBJECT_CLASS(klass), id);
    g_return_val_if_fail(g_type_is_a(fromtype, G_TYPE_OBJECT), id);

    GObjectClass *sourceclass = g_type_class_ref(fromtype);
    g_return_val_if_fail(G_IS_OBJECT_CLASS(sourceclass), id);

    va_list ap;
    va_start(ap, id);
    const gchar *name;
    while ((name = va_arg(ap, const gchar*))) {
        GParamSpec *sourcepspec = g_object_class_find_property(sourceclass,
                                                               name);
        if (!sourcepspec) {
            g_critical("Class %s has no property called %s.",
                       G_OBJECT_CLASS_NAME(sourceclass), name);
            continue;
        }
        GParamSpec *pspec = replicate_param_spec(sourcepspec);
        if (!pspec)
            continue;

        g_object_class_install_property(klass, id, pspec);
        if (properties)
            properties[id] = pspec;

        id++;
    }
    va_end(ap);

    // Hopefully this will not wipe out the params again.  But nothing bad
    // should happen because now @klass references them.
    g_type_class_unref(sourceclass);

    return id;
}

/**
 * gwy_genum_value_nick:
 * @enumtype: Enum value type.
 * @value: A value from the enumerated type.  Or possibly another value; the
 *         functions tries to return a result useful for debugging in such
 *         case.
 *
 * Finds the nick of an enum value.
 *
 * This is a debugging function.  Location to a static storage is returned so
 * you cannot use multiple return values in a single printf() call, for
 * instance.  This function is, of course, also thread-unsafe.
 *
 * Returns: Nick of the value.
 **/
const gchar*
gwy_genum_value_nick(GType enumtype,
                     gint value)
{
    static GString *result = NULL;
    if (!result)
        result = g_string_new(NULL);

    g_return_val_if_fail(G_TYPE_IS_ENUM(enumtype), "<INVALID-TYPE>");
    GEnumClass *klass = g_type_class_ref(enumtype);
    GEnumValue *enumvalue = g_enum_get_value(klass, value);
    if (enumvalue)
        g_string_assign(result, enumvalue->value_nick);
    else
        g_string_printf(result, "<INVALID-VALUE %d>", value);
    g_type_class_unref(klass);
    return result->str;
}

/**
 * gwy_gflags_value_nick:
 * @flagstype: Flags value type.
 * @value: Any combination of flags from the flags type.  Or possibly another
 *         value; the functions tries to return a result useful for debugging
 *         in such case.
 *
 * Constructs the string representation of the nicks of a flags value.
 *
 * This is a debugging function.  Location to a static storage is returned so
 * you cannot use multiple return values in a single printf() call, for
 * instance.  This function is, of course, also thread-unsafe.
 *
 * Returns: Nicks of the flags, joined together with "|".
 **/
const gchar*
gwy_gflags_value_nick(GType flagstype,
                      guint value)
{
    static GString *result = NULL;
    if (!result)
        result = g_string_new(NULL);

    g_return_val_if_fail(G_TYPE_IS_FLAGS(flagstype), "<INVALID-TYPE>");
    GFlagsClass *klass = g_type_class_ref(flagstype);
    if (!value)
        return "0";

    g_string_truncate(result, 0);
    GFlagsValue *flagsvalue = g_flags_get_first_value(klass, value);
    while (flagsvalue) {
        if (result->len)
            g_string_append_c(result, '|');
        g_string_append(result, flagsvalue->value_nick);
        value &= ~flagsvalue->value;
        flagsvalue = g_flags_get_first_value(klass, value);
    }
    if (value) {
        if (result->len)
            g_string_append_c(result, '|');
        g_string_append_printf(result, "<INVALID-FLAGS 0x%x>", value);
    }
    g_type_class_unref(klass);
    return result->str;
}

/**
 * SECTION: object-utils
 * @title: Object utils
 * @short_description: GObject utility functions
 **/

/**
 * GWY_OBJECT_UNREF:
 * @obj: Pointer to #GObject or %NULL (must be an l-value).
 *
 * Unreferences an object if it exists.
 *
 * This is an idempotent wrapper of g_object_unref(): if @obj is not %NULL
 * g_object_unref() is called on it and @obj is set to %NULL.
 *
 * If the object reference count is greater than one ensure it is referenced
 * elsewhere, otherwise it leaks memory.
 *
 * This macro may evaluate its arguments several times.
 * This macro is usable as a single statement.
 **/

/**
 * GWY_SIGNAL_HANDLER_DISCONNECT:
 * @obj: Pointer to #GObject or %NULL.
 * @hid: Id of a signal handler connected to @obj, or 0 (must be an l-value).
 *
 * Disconnects a signal handler if it exists.
 *
 * This is an idempotent wrapper of g_signal_handler_disconnect(): if @hid is
 * nonzero and @obj is not %NULL, the signal handler identified by
 * @hid is disconnected and @hid is set to 0.
 *
 * This macro may evaluate its arguments several times.
 * This macro is usable as a single statement.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
