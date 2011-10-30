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

#include <stdarg.h>
#include <string.h>
#include "libgwy/macros.h"
#include "libgwy/strfuncs.h"
#include "libgwy/object-utils.h"

/**
 * GwyUserFuncData:
 *
 * Representation of a user function with data and destroy notifier.
 *
 * Usually, you define a structure with specific function type of @func and
 * typecase it to #GwyUserFuncData when passing it to
 * gwy_user_func_data_destroy().
 **/

/**
 * gwy_set_user_func:
 * @func: New function pointer.
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
        (*destroy_field)(data);

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
 * If @member_object is not %NULL a reference is taken (and conversely, the
 * reference to the previous member object is released).
 *
 * The purpose is to simplify bookkeeping in classes that have settable
 * member objects and (usually but not necessarily) need to connect to some
 * signals of these member objects.  Since this function both connects and
 * disconnects signals it must be always called with the same set of signals,
 * including callbacks and flags, for a specific member object.
 *
 * Example for a #GwyFoo class owning a #GwyGradient member object, assuming
 * the usual conventions:
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
        g_object_ref(member_object);

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
