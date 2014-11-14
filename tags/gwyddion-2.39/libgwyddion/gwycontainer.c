/*
 *  @(#) $Id$
 *  Copyright (C) 2003,2004,2014 David Necas (Yeti), Petr Klapetek.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwydebugobjects.h>
#include "gwycontainer.h"
#include "gwyserializable.h"

#define GWY_CONTAINER_TYPE_NAME "GwyContainer"

enum {
    ITEM_CHANGED,
    LAST_SIGNAL
};

typedef struct {
    GQuark key;
    GValue *value;
    gboolean changed;
} GwyKeyVal;

typedef struct {
    GwyContainer *container;
    const gchar *prefix;
    gsize prefix_length;
    guint count;
    GSList *keylist;
    gboolean closed_prefix;
    GHFunc func;
    gpointer user_data;
} PrefixData;

typedef struct {
    GwyContainer *container;
    gint nprefixes;
    const gchar **prefixes;
    gboolean *pfxclosed;
    gsize *pfxlengths;
} PrefixListData;

typedef struct {
    GwySerializeItem *items;
    gint i;
} SerializeData;

static void     gwy_container_serializable_init  (GwySerializableIface *iface);
static void     value_destroy_func               (gpointer data);
static void     gwy_container_finalize           (GObject *object);
static GValue*  gwy_container_get_value_of_type  (GwyContainer *container,
                                                  GQuark key,
                                                  GType type);
static GValue*  gwy_container_gis_value_of_type  (GwyContainer *container,
                                                  GQuark key,
                                                  GType type);
static gboolean gwy_container_try_set_one        (GwyContainer *container,
                                                  GQuark key,
                                                  GValue *value);
static void     gwy_container_try_setv           (GwyContainer *container,
                                                  gsize nvalues,
                                                  GwyKeyVal *values);
static void     gwy_container_try_set_valist     (GwyContainer *container,
                                                  va_list ap);
static void     gwy_container_set_by_name_valist (GwyContainer *container,
                                                  va_list ap);
static GByteArray* gwy_container_serialize       (GObject *object,
                                                  GByteArray *buffer);
static gsize    gwy_container_get_size           (GObject *object);
static void     hash_serialize_func              (gpointer hkey,
                                                  gpointer hvalue,
                                                  gpointer hdata);
static GObject* gwy_container_deserialize        (const guchar *buffer,
                                                  gsize size,
                                                  gsize *position);
static gboolean hash_remove_prefix_func          (gpointer hkey,
                                                  gpointer hvalue,
                                                  gpointer hdata);
static void     hash_foreach_func                (gpointer hkey,
                                                  gpointer hvalue,
                                                  gpointer hdata);
static void     keys_foreach_func                (gpointer hkey,
                                                  gpointer hvalue,
                                                  gpointer hdata);
static void     keys_by_name_foreach_func        (gpointer hkey,
                                                  gpointer hvalue,
                                                  gpointer hdata);
static GObject* gwy_container_duplicate_real     (GObject *object);
static void     gwy_container_clone_real         (GObject *source,
                                                  GObject *copy);
static gboolean hash_remove_all_func             (void);
static void     hash_duplicate_func              (gpointer hkey,
                                                  gpointer hvalue,
                                                  gpointer hdata);
static GwyContainer*
              gwy_container_duplicate_by_prefix_valist(GwyContainer *container,
                                                       va_list ap);
static void     hash_prefix_duplicate_func       (gpointer hkey,
                                                  gpointer hvalue,
                                                  gpointer hdata);
static void     hash_find_keys_func              (gpointer hkey,
                                                  gpointer hvalue,
                                                  gpointer hdata);

static int      pstring_compare_callback         (const void *p,
                                                  const void *q);
static guint    token_length                     (const gchar *text);
static gchar*   dequote_token                    (const gchar *tok,
                                                  gsize *len);

static guint container_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_EXTENDED
    (GwyContainer, gwy_container, G_TYPE_OBJECT, 0,
     GWY_IMPLEMENT_SERIALIZABLE(gwy_container_serializable_init))

static void
gwy_container_serializable_init(GwySerializableIface *iface)
{
    iface->serialize = gwy_container_serialize;
    iface->deserialize = gwy_container_deserialize;
    iface->get_size = gwy_container_get_size;
    iface->duplicate = gwy_container_duplicate_real;
    iface->clone = gwy_container_clone_real;
}

static void
gwy_container_class_init(GwyContainerClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = gwy_container_finalize;

    /**
    * GwyContainer::item-changed:
    * @gwycontainer: The #GwyContainer which received the signal.
    * @arg1: The quark key identifying the changed item.
    *
    * The ::item-changed signal is emitted whenever a container item is
    * changed.  The detail is the string key identifier.
    */
    container_signals[ITEM_CHANGED]
        = g_signal_new("item-changed",
                       G_OBJECT_CLASS_TYPE(gobject_class),
                       G_SIGNAL_RUN_FIRST | G_SIGNAL_DETAILED
                           | G_SIGNAL_NO_RECURSE,
                       G_STRUCT_OFFSET(GwyContainerClass, item_changed),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__UINT,
                       G_TYPE_NONE, 1,
                       G_TYPE_UINT);
}

static void
gwy_container_init(GwyContainer *container)
{
    gwy_debug_objects_creation((GObject*)container);
    container->values = g_hash_table_new_full(NULL, NULL,
                                              NULL, value_destroy_func);
}

static void
gwy_container_finalize(GObject *object)
{
    GwyContainer *container = (GwyContainer*)object;

    g_hash_table_destroy(container->values);

    G_OBJECT_CLASS(gwy_container_parent_class)->finalize(object);
}

/**
 * gwy_container_new:
 *
 * Creates a new #GwyContainer.
 *
 * Returns: The container, as a #GObject.
 **/
GwyContainer*
gwy_container_new(void)
{
    GwyContainer *container;

    container = g_object_new(GWY_TYPE_CONTAINER, NULL);

    return container;
}

static void
value_destroy_func(gpointer data)
{
    GValue *val = (GValue*)data;
#ifdef DEBUG
    GObject *obj = NULL;

    gwy_debug("unsetting value %p, holds object = %d (%s)",
              val, G_VALUE_HOLDS_OBJECT(val), G_VALUE_TYPE_NAME(val));
    if (G_VALUE_HOLDS_OBJECT(val)) {
        obj = G_OBJECT(g_value_peek_pointer(val));
        gwy_debug("%s refcount = %d", G_OBJECT_TYPE_NAME(obj), obj->ref_count);
    }
#endif
    g_value_unset(val);
    g_free(val);
#ifdef DEBUG
    if (obj)
        gwy_debug("refcount = %d", obj->ref_count);
#endif
}

/**
 * gwy_container_get_n_items:
 * @container: A container.
 *
 * Gets the number of items in a container.
 *
 * Returns: The number of items.
 **/
guint
gwy_container_get_n_items(GwyContainer *container)
{
    g_return_val_if_fail(GWY_IS_CONTAINER(container), 0);
    return g_hash_table_size(container->values);
}

/**
 * gwy_container_value_type_by_name:
 * @c: A container.
 * @n: A nul-terminated name (id).
 *
 * Gets the type of value in container @c identified by name @n.
 **/

/**
 * gwy_container_value_type:
 * @container: A container.
 * @key: A #GQuark key.
 *
 * Returns the type of value in @container identified by @key.
 *
 * Returns: The value type as #GType; 0 if there is no such value.
 **/
GType
gwy_container_value_type(GwyContainer *container, GQuark key)
{
    GValue *p;

    g_return_val_if_fail(GWY_IS_CONTAINER(container), 0);
    if (!key)
        return 0;

    p = (GValue*)g_hash_table_lookup(container->values,
                                     GUINT_TO_POINTER(key));

    return p ? G_VALUE_TYPE(p) : 0;
}

/**
 * gwy_container_contains_by_name:
 * @c: A container.
 * @n: A nul-terminated name (id).
 *
 * Expands to %TRUE if container @c contains a value identified by name @n.
 **/

/**
 * gwy_container_contains:
 * @container: A container.
 * @key: A #GQuark key.
 *
 * Returns %TRUE if @container contains a value identified by @key.
 *
 * Returns: Whether @container contains something identified by @key.
 **/
gboolean
gwy_container_contains(GwyContainer *container, GQuark key)
{
    g_return_val_if_fail(GWY_IS_CONTAINER(container), 0);
    return key
           && g_hash_table_lookup(container->values,
                                  GUINT_TO_POINTER(key)) != NULL;
}

/**
 * gwy_container_remove_by_name:
 * @c: A container.
 * @n: A nul-terminated name (id).
 *
 * Removes a value identified by name @n from container @c.
 *
 * Expands to %TRUE if there was such a value and was removed.
 **/

/**
 * gwy_container_remove:
 * @container: A container.
 * @key: A #GQuark key.
 *
 * Removes a value identified by @key from a container.
 *
 * Returns: %TRUE if there was such a value and was removed.
 **/
gboolean
gwy_container_remove(GwyContainer *container, GQuark key)
{
    GValue *value;

    g_return_val_if_fail(GWY_IS_CONTAINER(container), FALSE);
    if (!key)
        return FALSE;

    value = g_hash_table_lookup(container->values, GUINT_TO_POINTER(key));
    if (!value)
        return FALSE;
#ifdef DEBUG
    if (G_VALUE_HOLDS_OBJECT(value)) {
        gwy_debug("refcount = %d",
                  G_OBJECT(g_value_peek_pointer(value))->ref_count);
    }
#endif

#ifdef DEBUG
    gwy_debug("holds object = %d", G_VALUE_HOLDS_OBJECT(value));
    if (G_VALUE_HOLDS_OBJECT(value)) {
        gwy_debug("refcount = %d",
                  G_OBJECT(g_value_peek_pointer(value))->ref_count);
    }
#endif

    g_hash_table_remove(container->values, GUINT_TO_POINTER(key));
    g_signal_emit(container, container_signals[ITEM_CHANGED], key, key);

    return TRUE;
}

/**
 * gwy_container_remove_by_prefix:
 * @container: A container.
 * @prefix: A nul-terminated id prefix.
 *
 * Removes a values whose key start with @prefix from container @container.
 *
 * @prefix can be %NULL, all values are then removed.
 *
 * Returns: The number of values removed.
 **/
guint
gwy_container_remove_by_prefix(GwyContainer *container, const gchar *prefix)
{
    PrefixData pfdata;
    GSList *l;

    g_return_val_if_fail(GWY_IS_CONTAINER(container), 0);

    pfdata.container = container;
    pfdata.prefix = prefix;
    pfdata.prefix_length = prefix ? strlen(pfdata.prefix) : 0;
    pfdata.count = 0;
    pfdata.keylist = NULL;
    pfdata.closed_prefix = !pfdata.prefix_length
                           || (prefix[pfdata.prefix_length - 1]
                               == GWY_CONTAINER_PATHSEP);
    g_hash_table_foreach_remove(container->values, hash_remove_prefix_func,
                                &pfdata);
    pfdata.keylist = g_slist_reverse(pfdata.keylist);
    for (l = pfdata.keylist; l; l = g_slist_next(l))
        g_signal_emit(container, container_signals[ITEM_CHANGED],
                      GPOINTER_TO_UINT(l->data), GPOINTER_TO_UINT(l->data));
    g_slist_free(pfdata.keylist);

    return pfdata.count;
}

static gboolean
hash_remove_prefix_func(gpointer hkey,
                        G_GNUC_UNUSED gpointer hvalue,
                        gpointer hdata)
{
    GQuark key = GPOINTER_TO_UINT(hkey);
    PrefixData *pfdata = (PrefixData*)hdata;
    const gchar *name;

    if (pfdata->prefix
        && (!(name = g_quark_to_string(key))
            || !g_str_has_prefix(name, pfdata->prefix)
            || (!pfdata->closed_prefix
                && name[pfdata->prefix_length] != '\0'
                && name[pfdata->prefix_length] != GWY_CONTAINER_PATHSEP)))
        return FALSE;

    pfdata->count++;
    pfdata->keylist = g_slist_prepend(pfdata->keylist,
                                      GUINT_TO_POINTER(hkey));
    return TRUE;
}

/**
 * gwy_container_foreach:
 * @container: A container.
 * @prefix: A nul-terminated id prefix.
 * @function: The function called on the items.
 * @user_data: The user data passed to @function.
 *
 * Calls @function on each @container item whose identifier starts with
 * @prefix.
 *
 * The function is called @function(#GQuark key, #GValue *value, user_data).
 *
 * Returns: The number of items @function was called on.
 **/
guint
gwy_container_foreach(GwyContainer *container,
                      const gchar *prefix,
                      GHFunc function,
                      gpointer user_data)
{
    PrefixData pfdata;

    g_return_val_if_fail(GWY_IS_CONTAINER(container), 0);
    g_return_val_if_fail(function, 0);

    pfdata.container = container;
    pfdata.prefix = prefix;
    pfdata.prefix_length = prefix ? strlen(pfdata.prefix) : 0;
    pfdata.closed_prefix = !pfdata.prefix_length
                           || (prefix[pfdata.prefix_length - 1]
                               == GWY_CONTAINER_PATHSEP);
    pfdata.count = 0;
    pfdata.keylist = NULL;
    pfdata.func = function;
    pfdata.user_data = user_data;
    g_hash_table_foreach(container->values, hash_foreach_func, &pfdata);

    return pfdata.count;
}

static void
hash_foreach_func(gpointer hkey, gpointer hvalue, gpointer hdata)
{
    GQuark key = GPOINTER_TO_UINT(hkey);
    GValue *value = (GValue*)hvalue;
    PrefixData *pfdata = (PrefixData*)hdata;
    const gchar *name;

    if (pfdata->prefix
        && (!(name = g_quark_to_string(key))
            || !g_str_has_prefix(name, pfdata->prefix)
            || (!pfdata->closed_prefix
                && name[pfdata->prefix_length] != '\0'
                && name[pfdata->prefix_length] != GWY_CONTAINER_PATHSEP)))
        return;

    pfdata->func(hkey, value, pfdata->user_data);
    pfdata->count++;
}

/**
 * gwy_container_keys:
 * @container: A container.
 *
 * Gets all quark keys of a container.
 *
 * Returns: A newly allocated array with quark keys of all @container items,
 *          in no particular order.  The number of items can be obtained
 *          with gwy_container_get_n_items().  If there are no items, %NULL
 *          is returned.
 *
 * Since: 2.7
 **/
GQuark*
gwy_container_keys(GwyContainer *container)
{
    GArray *array;
    GQuark *keys;
    guint n;

    g_return_val_if_fail(GWY_IS_CONTAINER(container), NULL);
    n = g_hash_table_size(container->values);
    if (!n)
        return NULL;

    array = g_array_sized_new(FALSE, FALSE, sizeof(GQuark), n);
    g_hash_table_foreach(container->values, keys_foreach_func, array);
    keys = (GQuark*)array->data;
    g_array_free(array, FALSE);

    return keys;
}

static void
keys_foreach_func(gpointer hkey,
                  G_GNUC_UNUSED gpointer hvalue,
                  gpointer hdata)
{
    GQuark key = GPOINTER_TO_UINT(hkey);
    GArray *array = (GArray*)hdata;

    g_array_append_val(array, key);
}

/**
 * gwy_container_keys_by_name:
 * @container: A container.
 *
 * Gets all string keys of a container.
 *
 * Returns: A newly allocated array with string keys of all @container items,
 *          in no particular order.  The number of items can be obtained
 *          with gwy_container_get_n_items().  If there are no items, %NULL
 *          is returned.  Unlike the array the strings are owned by GLib and
 *          must not be freed.
 *
 * Since: 2.7
 **/
const gchar**
gwy_container_keys_by_name(GwyContainer *container)
{
    GPtrArray *array;
    const gchar **keys;
    guint n;

    g_return_val_if_fail(GWY_IS_CONTAINER(container), NULL);
    n = g_hash_table_size(container->values);
    if (!n)
        return NULL;

    array = g_ptr_array_sized_new(n);
    g_hash_table_foreach(container->values, keys_by_name_foreach_func, array);
    keys = (const gchar**)array->pdata;
    g_ptr_array_free(array, FALSE);

    return keys;
}

static void
keys_by_name_foreach_func(gpointer hkey,
                          G_GNUC_UNUSED gpointer hvalue,
                          gpointer hdata)
{
    GQuark key = GPOINTER_TO_UINT(hkey);
    GPtrArray *array = (GPtrArray*)hdata;

    g_ptr_array_add(array, (gpointer)g_quark_to_string(key));
}

/**
 * gwy_container_rename_by_name:
 * @c: A container.
 * @n: A nul-terminated name (id).
 * @nn: A nul-terminated name (id).
 * @f: Whether to delete existing value at @newkey.
 *
 * Makes a value in container @c identified by name @n to be identified by
 * new name @nn.
 *
 * See gwy_container_rename() for details.
 **/

/**
 * gwy_container_rename:
 * @container: A container.
 * @key: The current key.
 * @newkey: A new key for the value.
 * @force: Whether to replace existing value at @newkey.
 *
 * Makes a value in @container identified by @key to be identified by @newkey.
 *
 * When @force is %TRUE existing value at @newkey is removed from @container.
 * When it's %FALSE, an existing value @newkey inhibits the rename and %FALSE
 * is returned.
 *
 * Returns: Whether the rename succeeded.
 **/
gboolean
gwy_container_rename(GwyContainer *container,
                     GQuark key,
                     GQuark newkey,
                     gboolean force)
{
    GValue *value;

    g_return_val_if_fail(GWY_IS_CONTAINER(container), FALSE);
    g_return_val_if_fail(key, FALSE);
    if (key == newkey)
        return TRUE;

    value = g_hash_table_lookup(container->values, GUINT_TO_POINTER(key));
    if (!value)
        return FALSE;

    if (g_hash_table_lookup(container->values, GUINT_TO_POINTER(newkey))) {
        if (!force)
            return FALSE;

        g_hash_table_remove(container->values, GUINT_TO_POINTER(newkey));
    }

    g_hash_table_insert(container->values, GUINT_TO_POINTER(newkey), value);
    g_hash_table_steal(container->values, GUINT_TO_POINTER(key));
    g_signal_emit(container, container_signals[ITEM_CHANGED], key, key);
    g_signal_emit(container, container_signals[ITEM_CHANGED], newkey, newkey);

    return TRUE;
}

/**
 * gwy_container_get_value_of_type:
 * @container: A container.
 * @key: A #GQuark key.
 * @type: Value type to get.  Can be %NULL to not check value type.
 *
 * Low level function to get a value from a container.
 *
 * Causes a warning when no such value exists, or it's of a wrong type.
 * Use gwy_container_gis_value_of_type() to get value that may not exist.
 *
 * Returns: The value identified by @key; %NULL on failure.
 **/
static GValue*
gwy_container_get_value_of_type(GwyContainer *container,
                                GQuark key,
                                GType type)
{
    GValue *p;

    g_return_val_if_fail(GWY_IS_CONTAINER(container), NULL);
    g_return_val_if_fail(key, NULL);
    p = (GValue*)g_hash_table_lookup(container->values, GUINT_TO_POINTER(key));
    if (!p) {
        g_warning("%s: no value for key %u (%s)",
                  GWY_CONTAINER_TYPE_NAME, key, g_quark_to_string(key));
        return NULL;
    }
    if (type && !G_VALUE_HOLDS(p, type)) {
        g_warning("%s: trying to get %s as %s, key %u (%s)",
                  GWY_CONTAINER_TYPE_NAME,
                  G_VALUE_TYPE_NAME(p), g_type_name(type),
                  key, g_quark_to_string(key));
        return NULL;
    }

    return p;
}

/**
 * gwy_container_gis_value_of_type:
 * @container: A container.
 * @key: A #GQuark key.
 * @type: Value type to get.  Can be %NULL to not check value type.
 *
 * Low level function to get a value from a container.
 *
 * Causes a warning when value is of a wrong type.
 *
 * Returns: The value identified by @key, or %NULL.
 **/
static GValue*
gwy_container_gis_value_of_type(GwyContainer *container,
                                GQuark key,
                                GType type)
{
    GValue *p;

    if (!key)
        return NULL;
    g_return_val_if_fail(GWY_IS_CONTAINER(container), NULL);

    p = (GValue*)g_hash_table_lookup(container->values, GUINT_TO_POINTER(key));
    if (!p)
        return NULL;
    if (type && !G_VALUE_HOLDS(p, type)) {
        g_warning("%s: trying to get %s as %s, key %u (%s)",
                  GWY_CONTAINER_TYPE_NAME,
                  G_VALUE_TYPE_NAME(p), g_type_name(type),
                  key, g_quark_to_string(key));
        return NULL;
    }

    return p;
}

/**
 * gwy_container_get_value_by_name:
 * @c: A container.
 * @n: A nul-terminated name (id).
 *
 * Gets the value in container @c identified by name @n.
 **/

/**
 * gwy_container_get_value:
 * @container: A container.
 * @key: A #GQuark key.
 *
 * Returns the value in @container identified by @key.
 *
 * Returns: The value as a #GValue.
 **/
GValue
gwy_container_get_value(GwyContainer *container, GQuark key)
{
    GValue gvalue;
    GValue *p;

    gwy_clear(&gvalue, 1);
    p = gwy_container_get_value_of_type(container, key, 0);
    if (G_LIKELY(p)) {
        g_value_init(&gvalue, G_VALUE_TYPE(p));
        g_value_copy(p, &gvalue);
    }

    return gvalue;
}

/**
 * gwy_container_gis_value_by_name:
 * @c: A container.
 * @n: A nul-terminated name (id).
 * @v: Pointer to a #GValue to update. If item does not exist,
 *     it is left untouched.
 *
 * Get-if-set a generic value from a container.
 *
 * Expands to %TRUE if @value was actually updated, %FALSE when there is no
 * such value in the container.
 **/

/**
 * gwy_container_gis_value:
 * @container: A container.
 * @key: A #GQuark key.
 * @value: Pointer to a #GValue to update. If item does not exist,
 *         it is left untouched.
 *
 * Get-if-set a generic value from a container.
 *
 * Returns: %TRUE if @v was actually updated, %FALSE when there is no
 *          such value in the container.
 **/
gboolean
gwy_container_gis_value(GwyContainer *container,
                        GQuark key,
                        GValue *value)
{
    GValue *p;

    if (!(p = gwy_container_gis_value_of_type(container, key, 0)))
        return FALSE;

    if (G_VALUE_TYPE(value))
        g_value_unset(value);
    g_value_init(value, G_VALUE_TYPE(p));
    g_value_copy(p, value);

    return TRUE;
}

/**
 * gwy_container_get_boolean_by_name:
 * @c: A container.
 * @n: A nul-terminated name (id).
 *
 * Gets the boolean in container @c identified by name @n.
 **/

/**
 * gwy_container_get_boolean:
 * @container: A container.
 * @key: A #GQuark key.
 *
 * Returns the boolean in @container identified by @key.
 *
 * Returns: The boolean as #gboolean.
 **/
gboolean
gwy_container_get_boolean(GwyContainer *container, GQuark key)
{
    GValue *p;

    p = gwy_container_get_value_of_type(container, key, G_TYPE_BOOLEAN);
    return G_LIKELY(p) ? !!g_value_get_boolean(p) : FALSE;
}

/**
 * gwy_container_gis_boolean_by_name:
 * @c: A container.
 * @n: A nul-terminated name (id).
 * @v: Pointer to the boolean to update.
 *
 * Get-if-set a boolean from a container.
 *
 * Expands to %TRUE if @value was actually updated, %FALSE when there is no
 * such boolean in the container.
 **/

/**
 * gwy_container_gis_boolean:
 * @container: A container.
 * @key: A #GQuark key.
 * @value: Pointer to the boolean to update.
 *
 * Get-if-set a boolean from a container.
 *
 * Returns: %TRUE if @v was actually updated, %FALSE when there is no
 *          such boolean in the container.
 **/
gboolean
gwy_container_gis_boolean(GwyContainer *container,
                          GQuark key,
                          gboolean *value)
{
    GValue *p;

    if ((p = gwy_container_gis_value_of_type(container, key, G_TYPE_BOOLEAN))) {
        *value = !!g_value_get_boolean(p);
        return TRUE;
    }
    return FALSE;
}

/**
 * gwy_container_get_uchar_by_name:
 * @c: A container.
 * @n: A nul-terminated name (id).
 *
 * Gets the unsigned character in container @c identified by name @n.
 **/

/**
 * gwy_container_get_uchar:
 * @container: A container.
 * @key: A #GQuark key.
 *
 * Returns the unsigned character in @container identified by @key.
 *
 * Returns: The character as #guchar.
 **/
guchar
gwy_container_get_uchar(GwyContainer *container, GQuark key)
{
    GValue *p;

    p = gwy_container_get_value_of_type(container, key, G_TYPE_UCHAR);
    return G_LIKELY(p) ? g_value_get_uchar(p) : 0;
}

/**
 * gwy_container_gis_uchar_by_name:
 * @c: A container.
 * @n: A nul-terminated name (id).
 * @v: Pointer to the unsigned char to update.
 *
 * Get-if-set an unsigned char from a container.
 *
 * Expands to %TRUE if @value was actually updated, %FALSE when there is no
 * such unsigned char in the container.
 **/

/**
 * gwy_container_gis_uchar:
 * @container: A container.
 * @key: A #GQuark key.
 * @value: Pointer to the unsigned char to update.
 *
 * Get-if-set an unsigned char from a container.
 *
 * Returns: %TRUE if @v was actually updated, %FALSE when there is no
 *          such unsigned char in the container.
 **/
gboolean
gwy_container_gis_uchar(GwyContainer *container,
                        GQuark key,
                        guchar *value)
{
    GValue *p;

    if ((p = gwy_container_gis_value_of_type(container, key, G_TYPE_UCHAR))) {
        *value = g_value_get_uchar(p);
        return TRUE;
    }
    return FALSE;
}

/**
 * gwy_container_get_int32_by_name:
 * @c: A container.
 * @n: A nul-terminated name (id).
 *
 * Gets the 32bit integer in container @c identified by name @n.
 **/

/**
 * gwy_container_get_int32:
 * @container: A container.
 * @key: A #GQuark key.
 *
 * Returns the 32bit integer in @container identified by @key.
 *
 * Returns: The integer as #guint32.
 **/
gint32
gwy_container_get_int32(GwyContainer *container, GQuark key)
{
    GValue *p;

    p = gwy_container_get_value_of_type(container, key, G_TYPE_INT);
    return G_LIKELY(p) ? g_value_get_int(p) : 0;
}

/**
 * gwy_container_gis_int32_by_name:
 * @c: A container.
 * @n: A nul-terminated name (id).
 * @v: Pointer to the 32bit integer to update.
 *
 * Get-if-set a 32bit integer from a container.
 *
 * Expands to %TRUE if @value was actually updated, %FALSE when there is no
 * such 32bit integer in the container.
 **/

/**
 * gwy_container_gis_int32:
 * @container: A container.
 * @key: A #GQuark key.
 * @value: Pointer to the 32bit integer to update.
 *
 * Get-if-set a 32bit integer from a container.
 *
 * Returns: %TRUE if @v was actually updated, %FALSE when there is no
 *          such 32bit integer in the container.
 **/
gboolean
gwy_container_gis_int32(GwyContainer *container,
                        GQuark key,
                        gint32 *value)
{
    GValue *p;

    if ((p = gwy_container_gis_value_of_type(container, key, G_TYPE_INT))) {
        *value = g_value_get_int(p);
        return TRUE;
    }
    return FALSE;
}

/**
 * gwy_container_get_enum_by_name:
 * @c: A container.
 * @n: A nul-terminated name (id).
 *
 * Gets the enum in container @c identified by name @n.
 *
 * Note enums are treated as 32bit integers.
 **/

/**
 * gwy_container_get_enum:
 * @container: A container.
 * @key: A #GQuark key.
 *
 * Returns the enum in @container identified by @key.
 *
 * Note enums are treated as 32bit integers.
 *
 * Returns: The enum as #gint.
 **/
guint
gwy_container_get_enum(GwyContainer *container, GQuark key)
{
    return gwy_container_get_int32(container, key);
}

/**
 * gwy_container_gis_enum_by_name:
 * @c: A container.
 * @n: A nul-terminated name (id).
 * @v: Pointer to the enum to update.
 *
 * Get-if-set an enum from a container.
 *
 * Note enums are treated as 32bit integers.
 *
 * Expands to %TRUE if @value was actually updated, %FALSE when there is no
 * such enum in the container.
 **/

/**
 * gwy_container_gis_enum:
 * @container: A container.
 * @key: A #GQuark key.
 * @value: Pointer to the enum to update.
 *
 * Get-if-set an enum from a container.
 *
 * Note enums are treated as 32bit integers.
 *
 * Returns: %TRUE if @v was actually updated, %FALSE when there is no
 *          such enum in the container.
 **/
/* FIXME: this is probably wrong.  It's here to localize the problem with
 * enum/int/int32 exchanging in a one place. */
gboolean
gwy_container_gis_enum(GwyContainer *container,
                       GQuark key,
                       guint *value)
{
    gint32 value32;

    if (gwy_container_gis_int32(container, key, &value32)) {
        *value = value32;
        return TRUE;
    }
    return FALSE;
}

/**
 * gwy_container_get_int64_by_name:
 * @c: A container.
 * @n: A nul-terminated name (id).
 *
 * Gets the 64bit integer in container @c identified by name @n.
 **/

/**
 * gwy_container_get_int64:
 * @container: A container.
 * @key: A #GQuark key.
 *
 * Returns the 64bit integer in @container identified by @key.
 *
 * Returns: The 64bit integer as #guint64.
 **/
gint64
gwy_container_get_int64(GwyContainer *container, GQuark key)
{
    GValue *p;

    p = gwy_container_get_value_of_type(container, key, G_TYPE_INT64);
    return G_LIKELY(p) ? g_value_get_int64(p) : 0;
}

/**
 * gwy_container_gis_int64_by_name:
 * @c: A container.
 * @n: A nul-terminated name (id).
 * @v: Pointer to the 64bit integer to update.
 *
 * Get-if-set a 64bit integer from a container.
 *
 * Expands to %TRUE if @value was actually updated, %FALSE when there is no
 * such 64bit integer in the container.
 **/

/**
 * gwy_container_gis_int64:
 * @container: A container.
 * @key: A #GQuark key.
 * @value: Pointer to the 64bit integer to update.
 *
 * Get-if-set a 64bit integer from a container.
 *
 * Returns: %TRUE if @v was actually updated, %FALSE when there is no
 *          such 64bit integer in the container.
 **/
gboolean
gwy_container_gis_int64(GwyContainer *container,
                        GQuark key,
                        gint64 *value)
{
    GValue *p;

    if ((p = gwy_container_gis_value_of_type(container, key, G_TYPE_INT64))) {
        *value = g_value_get_int64(p);
        return TRUE;
    }
    return FALSE;
}

/**
 * gwy_container_get_double_by_name:
 * @c: A container.
 * @n: A nul-terminated name (id).
 *
 * Gets the double in container @c identified by name @n.
 **/

/**
 * gwy_container_get_double:
 * @container: A container.
 * @key: A #GQuark key.
 *
 * Returns the double in @container identified by @key.
 *
 * Returns: The double as #gdouble.
 **/
gdouble
gwy_container_get_double(GwyContainer *container, GQuark key)
{
    GValue *p;

    p = gwy_container_get_value_of_type(container, key, G_TYPE_DOUBLE);
    return G_LIKELY(p) ? g_value_get_double(p) : 0.0;
}

/**
 * gwy_container_gis_double_by_name:
 * @c: A container.
 * @n: A nul-terminated name (id).
 * @v: Pointer to the double to update.
 *
 * Get-if-set a double from a container.
 *
 * Expands to %TRUE if @value was actually updated, %FALSE when there is no
 * such double in the container.
 **/

/**
 * gwy_container_gis_double:
 * @container: A container.
 * @key: A #GQuark key.
 * @value: Pointer to the double to update.
 *
 * Get-if-set a double from a container.
 *
 * Returns: %TRUE if @v was actually updated, %FALSE when there is no
 *          such double in the container.
 **/
gboolean
gwy_container_gis_double(GwyContainer *container,
                         GQuark key,
                         gdouble *value)
{
    GValue *p;

    if ((p = gwy_container_gis_value_of_type(container, key, G_TYPE_DOUBLE))) {
        *value = g_value_get_double(p);
        return TRUE;
    }
    return FALSE;
}

/**
 * gwy_container_get_string_by_name:
 * @c: A container.
 * @n: A nul-terminated name (id).
 *
 * Gets the string in container @c identified by name @n.
 *
 * The returned string must be treated as constant and never freed or modified.
 **/

/**
 * gwy_container_get_string:
 * @container: A container.
 * @key: A #GQuark key.
 *
 * Returns the string in @container identified by @key.
 *
 * The returned string must be treated as constant and never freed or modified.
 *
 * Returns: The string.
 **/
const guchar*
gwy_container_get_string(GwyContainer *container, GQuark key)
{
    GValue *p;

    p = gwy_container_get_value_of_type(container, key, G_TYPE_STRING);
    return G_LIKELY(p) ? g_value_get_string(p) : NULL;
}

/**
 * gwy_container_gis_string_by_name:
 * @c: A container.
 * @n: A nul-terminated name (id).
 * @v: Pointer to the string pointer to update.
 *
 * Get-if-set a string from a container.
 *
 * The string eventually stored in @v must be treated as constant and
 * never freed or modified.
 *
 * Expands to %TRUE if @value was actually updated, %FALSE when there is no
 * such string in the container.
 **/

/**
 * gwy_container_gis_string:
 * @container: A container.
 * @key: A #GQuark key.
 * @value: Pointer to the string pointer to update.
 *
 * Get-if-set a string from a container.
 *
 * The string eventually stored in @value must be treated as constant and
 * never freed or modified.
 *
 * Returns: %TRUE if @v was actually updated, %FALSE when there is no
 *          such string in the container.
 **/
gboolean
gwy_container_gis_string(GwyContainer *container,
                         GQuark key,
                         const guchar **value)
{
    GValue *p;

    if ((p = gwy_container_gis_value_of_type(container, key, G_TYPE_STRING))) {
        *value = g_value_get_string(p);
        return TRUE;
    }
    return FALSE;
}

/**
 * gwy_container_get_object_by_name:
 * @c: A container.
 * @n: A nul-terminated name (id).
 *
 * Gets the object in container @c identified by name @n.
 *
 * The returned object doesn't have its reference count increased, use
 * g_object_ref() if you want to access it even when @container may cease
 * to exist.
 **/

/**
 * gwy_container_get_object:
 * @container: A container.
 * @key: A #GQuark key.
 *
 * Returns the object in @container identified by @key.
 *
 * The returned object doesn't have its reference count increased, use
 * g_object_ref() if you want to access it even when @container may cease
 * to exist.
 *
 * Returns: The object as #gpointer.
 **/
gpointer
gwy_container_get_object(GwyContainer *container, GQuark key)
{
    GValue *p;

    p = gwy_container_get_value_of_type(container, key, G_TYPE_OBJECT);
    return G_LIKELY(p) ? (gpointer)g_value_get_object(p) : NULL;
}

/**
 * gwy_container_gis_object_by_name:
 * @c: A container.
 * @n: A nul-terminated name (id).
 * @v: Pointer to the object pointer to update.
 *
 * Get-if-set an object from a container.
 *
 * The object eventually stored in @value doesn't have its reference count
 * increased, use g_object_ref() if you want to access it even when
 * @container may cease to exist.
 *
 * Expands to %TRUE if @value was actually updated, %FALSE when there is no
 * such object in the container.
 **/

/**
 * gwy_container_gis_object:
 * @container: A container.
 * @key: A #GQuark key.
 * @value: Pointer to the object pointer to update.
 *
 * Get-if-set an object from a container.
 *
 * The object eventually stored in @value doesn't have its reference count
 * increased, use g_object_ref() if you want to access it even when
 * @container may cease to exist.
 *
 * Returns: %TRUE if @v was actually updated, %FALSE when there is no
 *          such object in the container.
 **/
gboolean
gwy_container_gis_object(GwyContainer *container,
                         GQuark key,
                         gpointer value)
{
    GValue *p;

    if ((p = gwy_container_gis_value_of_type(container, key, G_TYPE_OBJECT))) {
        *(GObject**)value = g_value_get_object(p);
        return TRUE;
    }
    return FALSE;
}

static gboolean
gwy_container_values_equal(GValue *value1,
                           GValue *value2)
{
    GType type;

    g_return_val_if_fail(value1 || value2, TRUE);
    if (!value1 || !value2)
        return FALSE;

    type = G_VALUE_TYPE(value1);
    if (type != G_VALUE_TYPE(value2))
        return FALSE;

    switch (type) {
        case G_TYPE_BOOLEAN:
        return !g_value_get_boolean(value1) == !g_value_get_boolean(value2);

        case G_TYPE_UCHAR:
        return g_value_get_uchar(value1) == g_value_get_uchar(value2);

        case G_TYPE_INT:
        return g_value_get_int(value1) == g_value_get_int(value2);

        case G_TYPE_INT64:
        return g_value_get_int64(value1) == g_value_get_int64(value2);

        case G_TYPE_DOUBLE:
        return g_value_get_double(value1) == g_value_get_double(value2);

        case G_TYPE_STRING:
        return gwy_strequal(g_value_get_string(value1),
                            g_value_get_string(value2));

        /* objects must be identical, so compare addresses */
        case G_TYPE_OBJECT:
        return g_value_get_object(value1) == g_value_get_object(value2);
        break;
    }

    g_return_val_if_reached(FALSE);
}

static gboolean
gwy_container_try_set_one(GwyContainer *container,
                          GQuark key,
                          GValue *value)
{
    GValue *old;
    gboolean changed;

    g_return_val_if_fail(GWY_IS_CONTAINER(container), FALSE);
    g_return_val_if_fail(key, FALSE);
    g_return_val_if_fail(G_IS_VALUE(value), FALSE);

    /* Allow only some sane types to be stored, at least for now */
    if (G_VALUE_HOLDS_OBJECT(value)) {
        GObject *obj = g_value_peek_pointer(value);

        g_return_val_if_fail(GWY_IS_SERIALIZABLE(obj), FALSE);
    }
    else {
        GType type = G_VALUE_TYPE(value);

        g_return_val_if_fail(G_TYPE_FUNDAMENTAL(type)
                             && type != G_TYPE_BOXED
                             && type != G_TYPE_POINTER
                             && type != G_TYPE_PARAM,
                             FALSE);
    }

    old = (GValue*)g_hash_table_lookup(container->values, GUINT_TO_POINTER(key));
    if (old) {
        g_assert(G_IS_VALUE(old));
        changed = !gwy_container_values_equal(value, old);
        g_value_unset(old);
    }
    else {
        /* old is actually new here, but who cares... */
        old = g_new0(GValue, 1);
        g_hash_table_insert(container->values, GUINT_TO_POINTER(key), old);
        changed = TRUE;
    }
    g_value_init(old, G_VALUE_TYPE(value));
    g_value_copy(value, old);

    if (changed && !container->in_construction)
        g_signal_emit(container, container_signals[ITEM_CHANGED], key, key);

    return TRUE;
}

static void
gwy_container_try_setv(GwyContainer *container,
                       gsize nvalues,
                       GwyKeyVal *values)
{
    gsize i;

    for (i = 0; i < nvalues; i++)
        values[i].changed = gwy_container_try_set_one(container,
                                                      values[i].key,
                                                      values[i].value);
}

static void
gwy_container_try_set_valist(GwyContainer *container,
                             va_list ap)
{
    GwyKeyVal *values;
    gsize n, i;
    GQuark key;

    n = 16;
    values = g_new(GwyKeyVal, n);
    i = 0;
    key = va_arg(ap, GQuark);
    while (key) {
        if (i == n) {
            n += 16;
            values = g_renew(GwyKeyVal, values, n);
        }
        values[i].value = va_arg(ap, GValue*);
        values[i].key = key;
        values[i].changed = FALSE;
        i++;
        key = va_arg(ap, GQuark);
    }
    gwy_container_try_setv(container, i, values);
    g_free(values);
}

/**
 * gwy_container_set_value:
 * @container: A container.
 * @...: A zero-terminated list of #GQuark keys and #GValue values.
 *
 * Inserts or updates several values in @container.
 **/
void
gwy_container_set_value(GwyContainer *container,
                        ...)
{
    va_list ap;

    va_start(ap, container);
    gwy_container_try_set_valist(container, ap);
    va_end(ap);
}

static void
gwy_container_set_by_name_valist(GwyContainer *container,
                                 va_list ap)
{
    GwyKeyVal *values;
    gsize n, i;
    GQuark key;
    guchar *name;

    n = 16;
    values = g_new(GwyKeyVal, n);
    i = 0;
    name = va_arg(ap, guchar*);
    while (name) {
        key = g_quark_from_string(name);
        if (i == n) {
            n += 16;
            values = g_renew(GwyKeyVal, values, n);
        }
        values[i].value = va_arg(ap, GValue*);
        values[i].key = key;
        values[i].changed = FALSE;
        i++;
        name = va_arg(ap, guchar*);
    }
    gwy_container_try_setv(container, i, values);
    g_free(values);
}

/**
 * gwy_container_set_value_by_name:
 * @container: A container.
 * @...: A %NULL-terminated list of string keys and #GValue values.
 *
 * Inserts or updates several values in @container.
 **/
void
gwy_container_set_value_by_name(GwyContainer *container,
                                ...)
{
    va_list ap;

    va_start(ap, container);
    gwy_container_set_by_name_valist(container, ap);
    va_end(ap);
}

/**
 * gwy_container_set_boolean_by_name:
 * @c: A container.
 * @n: A nul-terminated name (id).
 * @v: A boolean.
 *
 * Stores a boolean into container @c, identified by name @n.
 **/

/**
 * gwy_container_set_boolean:
 * @container: A container.
 * @key: A #GQuark key.
 * @value: A boolean.
 *
 * Stores a boolean into @container, identified by @key.
 **/
void
gwy_container_set_boolean(GwyContainer *container,
                          GQuark key,
                          gboolean value)
{
    GValue gvalue;

    gwy_clear(&gvalue, 1);
    g_value_init(&gvalue, G_TYPE_BOOLEAN);
    g_value_set_boolean(&gvalue, !!value);
    gwy_container_try_set_one(container, key, &gvalue);
}

/**
 * gwy_container_set_uchar_by_name:
 * @c: A container.
 * @n: A nul-terminated name (id).
 * @v: An unsigned character.
 *
 * Stores an unsigned character into container @c, identified by name @n.
 **/

/**
 * gwy_container_set_uchar:
 * @container: A container.
 * @key: A #GQuark key.
 * @value: An unsigned character.
 *
 * Stores an unsigned character into @container, identified by @key.
 **/
void
gwy_container_set_uchar(GwyContainer *container,
                        GQuark key,
                        guchar value)
{
    GValue gvalue;

    gwy_clear(&gvalue, 1);
    g_value_init(&gvalue, G_TYPE_UCHAR);
    g_value_set_uchar(&gvalue, value);
    gwy_container_try_set_one(container, key, &gvalue);
}

/**
 * gwy_container_set_int32_by_name:
 * @c: A container.
 * @n: A nul-terminated name (id).
 * @v: A 32bit integer.
 *
 * Stores a 32bit integer into container @c, identified by name @n.
 **/

/**
 * gwy_container_set_int32:
 * @container: A container.
 * @key: A #GQuark key.
 * @value: A 32bit integer.
 *
 * Stores a 32bit integer into @container, identified by @key.
 **/
void
gwy_container_set_int32(GwyContainer *container,
                        GQuark key,
                        gint32 value)
{
    GValue gvalue;

    gwy_clear(&gvalue, 1);
    g_value_init(&gvalue, G_TYPE_INT);
    g_value_set_int(&gvalue, value);
    gwy_container_try_set_one(container, key, &gvalue);
}

/**
 * gwy_container_set_enum_by_name:
 * @c: A container.
 * @n: A nul-terminated name (id).
 * @v: An enum.
 *
 * Stores an enum into container @c, identified by name @n.
 *
 * Note enums are treated as 32bit integers.
 **/

/**
 * gwy_container_set_enum:
 * @container: A container.
 * @key: A #GQuark key.
 * @value: An enum integer.
 *
 * Stores an enum into @container, identified by @key.
 *
 * Note enums are treated as 32bit integers.
 **/
void
gwy_container_set_enum(GwyContainer *container,
                       GQuark key,
                       guint value)
{
    gint32 value32 = value;

    gwy_container_set_int32(container, key, value32);
}

/**
 * gwy_container_set_int64_by_name:
 * @c: A container.
 * @n: A nul-terminated name (id).
 * @v: A 64bit integer.
 *
 * Stores a 64bit integer into container @c, identified by name @n.
 **/

/**
 * gwy_container_set_int64:
 * @container: A container.
 * @key: A #GQuark key.
 * @value: A 64bit integer.
 *
 * Stores a 64bit integer into @container, identified by @key.
 **/
void
gwy_container_set_int64(GwyContainer *container,
                        GQuark key,
                        gint64 value)
{
    GValue gvalue;

    gwy_clear(&gvalue, 1);
    g_value_init(&gvalue, G_TYPE_INT64);
    g_value_set_int64(&gvalue, value);
    gwy_container_try_set_one(container, key, &gvalue);
}

/**
 * gwy_container_set_double_by_name:
 * @c: A container.
 * @n: A nul-terminated name (id).
 * @v: A double integer.
 *
 * Stores a double into container @c, identified by name @n.
 **/

/**
 * gwy_container_set_double:
 * @container: A container.
 * @key: A #GQuark key.
 * @value: A double.
 *
 * Stores a double into @container, identified by @key.
 **/
void
gwy_container_set_double(GwyContainer *container,
                         GQuark key,
                         gdouble value)
{
    GValue gvalue;

    gwy_clear(&gvalue, 1);
    g_value_init(&gvalue, G_TYPE_DOUBLE);
    g_value_set_double(&gvalue, value);
    gwy_container_try_set_one(container, key, &gvalue);
}

/**
 * gwy_container_set_string_by_name:
 * @c: A container.
 * @n: A nul-terminated name (id).
 * @v: A nul-terminated string.
 *
 * Stores a string into container @c, identified by name @n.
 *
 * The container takes ownership of the string, so it can't be used on
 * static strings, use g_strdup() to duplicate them first.
 **/

/**
 * gwy_container_set_string:
 * @container: A container.
 * @key: A #GQuark key.
 * @value: A nul-terminated string.
 *
 * Stores a string into @container, identified by @key.
 *
 * The container takes ownership of the string, so it can't be used on
 * static strings, use g_strdup() to duplicate them first.
 **/
void
gwy_container_set_string(GwyContainer *container,
                         GQuark key,
                         const guchar *value)
{
    gboolean changed = FALSE;
    GValue *gvalue;
    gpointer pkey;

    g_return_if_fail(GWY_IS_CONTAINER(container));
    g_return_if_fail(key);
    g_return_if_fail(value);

    pkey = GUINT_TO_POINTER(key);
    gvalue = (GValue*)g_hash_table_lookup(container->values, pkey);
    if (gvalue) {
        g_assert(G_IS_VALUE(gvalue));
        if (!G_VALUE_HOLDS_STRING(gvalue)) {
            g_value_unset(gvalue);
            g_value_init(gvalue, G_TYPE_STRING);
            changed = TRUE;
        }
        else if (!gwy_strequal(g_value_get_string(gvalue), value))
            changed = TRUE;
    }
    else {
        gvalue = g_new0(GValue, 1);
        g_value_init(gvalue, G_TYPE_STRING);
        g_hash_table_insert(container->values, pkey, gvalue);
        changed = TRUE;
    }
    /* XXX: We MUST do this.  There is code relying on @value being consumed
     * and kept alive, not just immediately freed by this function. */
    g_value_take_string(gvalue, (gchar*)value);

    if (changed && !container->in_construction)
        g_signal_emit(container, container_signals[ITEM_CHANGED], key, key);
}

/**
 * gwy_container_set_const_string_by_name:
 * @c: A container.
 * @n: A nul-terminated name (id).
 * @v: A nul-terminated string.
 *
 * Stores a string into container @c, identified by name @n.
 *
 * The container makes a copy of the string, so it can be used on static
 * strings.
 *
 * Since: 2.38
 **/

/**
 * gwy_container_set_const_string:
 * @container: A container.
 * @key: A #GQuark key.
 * @value: A nul-terminated string.
 *
 * Stores a string into @container, identified by @key.
 *
 * The container makes a copy of the string, so it can be used on static
 * strings.
 *
 * Since: 2.38
 **/
void
gwy_container_set_const_string(GwyContainer *container,
                               GQuark key,
                               const guchar *value)
{
    GValue gvalue;

    g_return_if_fail(value);
    gwy_clear(&gvalue, 1);
    g_value_init(&gvalue, G_TYPE_STRING);
    g_value_take_string(&gvalue, (gchar*)value);
    /* This is OK because gwy_container_try_set_one() makes a copy. */
    gwy_container_try_set_one(container, key, &gvalue);
}

/**
 * gwy_container_set_object_by_name:
 * @c: A container.
 * @n: A nul-terminated name (id).
 * @v: An object to store into container.
 *
 * Stores an object into container @c, identified by name @n.
 *
 * See gwy_container_set_object() for details.
 **/

/**
 * gwy_container_set_object:
 * @container: A container.
 * @key: A #GQuark key.
 * @value: An object to store into container.
 *
 * Stores an object into @container, identified by @key.
 *
 * The container claims ownership on the object, i.e. its reference count is
 * incremented.
 *
 * The object must implement #GwySerializable interface to allow serialization
 * of the container.
 **/
void
gwy_container_set_object(GwyContainer *container,
                         GQuark key,
                         gpointer value)
{
    GValue gvalue;

    g_return_if_fail(G_IS_OBJECT(value));
    gwy_clear(&gvalue, 1);
    g_value_init(&gvalue, G_TYPE_OBJECT);
    g_value_set_object(&gvalue, value);  /* this increases refcount too */
    gwy_container_try_set_one(container, key, &gvalue);
    g_object_unref(value);
}

static GByteArray*
gwy_container_serialize(GObject *object,
                        GByteArray *buffer)
{
    GwyContainer *container;
    gsize nitems = 0;
    SerializeData sdata;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_CONTAINER(object), buffer);
    container = GWY_CONTAINER(object);

    nitems = g_hash_table_size(container->values);
    sdata.items = g_new0(GwySerializeItem, nitems);
    sdata.i = 0;
    g_hash_table_foreach(container->values, hash_serialize_func, &sdata);
    buffer = gwy_serialize_object_items(buffer, GWY_CONTAINER_TYPE_NAME,
                                        sdata.i, sdata.items);
    g_free(sdata.items);

    return buffer;
}

static gsize
gwy_container_get_size(GObject *object)
{
    GwyContainer *container;
    gsize nitems = 0, size;
    SerializeData sdata;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_CONTAINER(object), 0);
    container = GWY_CONTAINER(object);

    nitems = g_hash_table_size(container->values);
    sdata.items = g_new0(GwySerializeItem, nitems);
    sdata.i = 0;
    g_hash_table_foreach(container->values, hash_serialize_func, &sdata);
    size = gwy_serialize_get_items_size(GWY_CONTAINER_TYPE_NAME,
                                        sdata.i, sdata.items);
    g_free(sdata.items);

    return size;
}

static void
hash_serialize_func(gpointer hkey, gpointer hvalue, gpointer hdata)
{
    GQuark key = GPOINTER_TO_UINT(hkey);
    GValue *value = (GValue*)hvalue;
    SerializeData *sdata = (SerializeData*)hdata;
    GwySerializeItem *it;
    GType type = G_VALUE_TYPE(value);
    gsize i;

    i = sdata->i;
    it = sdata->items + i;
    it->name = g_quark_to_string(key);
    switch (type) {
        case G_TYPE_BOOLEAN:
        it->ctype = 'b';
        it->value.v_boolean = !!g_value_get_boolean(value);
        break;

        case G_TYPE_UCHAR:
        it->ctype = 'c';
        it->value.v_char = g_value_get_uchar(value);
        break;

        case G_TYPE_INT:
        it->ctype = 'i';
        it->value.v_int32 = g_value_get_int(value);
        break;

        case G_TYPE_INT64:
        it->ctype = 'q';
        it->value.v_int64 = g_value_get_int64(value);
        break;

        case G_TYPE_DOUBLE:
        it->ctype = 'd';
        it->value.v_double = g_value_get_double(value);
        break;

        case G_TYPE_STRING:
        it->ctype = 's';
        it->value.v_string = (guchar*)g_value_get_string(value);
        break;

        case G_TYPE_OBJECT:
        it->ctype = 'o';
        it->value.v_object = g_value_get_object(value);
        break;

        default:
        g_warning("Cannot pack GValue holding %s", g_type_name(type));
        return;
        break;
    }

    sdata->i++;
}

static GObject*
gwy_container_deserialize(const guchar *buffer,
                          gsize size,
                          gsize *position)
{
    GwySerializeItem *items, *it;
    GwyContainer *container;
    GQuark key;
    gsize i, nitems = 0;

    gwy_debug("");
    g_return_val_if_fail(buffer, NULL);
    items = gwy_deserialize_object_hash(buffer, size, position,
                                        GWY_CONTAINER_TYPE_NAME, &nitems);
    g_return_val_if_fail(items, NULL);

    container = (GwyContainer*)gwy_container_new();
    for (i = 0; i < nitems; i++) {
        it = items + i;
        gwy_debug("value: #%" G_GSIZE_FORMAT ": <%s> of <%c>",
                  i, it->name, it->ctype);
        key = g_quark_from_string(it->name);
        switch (it->ctype) {
            case 'b':
            gwy_container_set_boolean(container, key, it->value.v_boolean);
            break;

            case 'c':
            gwy_container_set_uchar(container, key, it->value.v_char);
            break;

            case 'i':
            gwy_container_set_int32(container, key, it->value.v_int32);
            break;

            case 'q':
            gwy_container_set_int64(container, key, it->value.v_int64);
            break;

            case 'd':
            gwy_container_set_double(container, key, it->value.v_double);
            break;

            case 's':
            gwy_container_set_string(container, key, it->value.v_string);
            break;

            case 'o':
            if (it->value.v_object) {
                gwy_container_set_object(container, key, it->value.v_object);
                g_object_unref(it->value.v_object);
            }
            break;

            default:
            g_critical("Container doesn't support type <%c>", it->ctype);
            break;
        }
    }
    g_free(items);

    return (GObject*)container;
}

static GObject*
gwy_container_duplicate_real(GObject *object)
{
    GwyContainer *duplicate;

    duplicate = gwy_container_new();
    /* don't emit signals when no one can be connected */
    duplicate->in_construction = TRUE;
    g_hash_table_foreach(GWY_CONTAINER(object)->values,
                         hash_duplicate_func, duplicate);
    duplicate->in_construction = FALSE;

    return (GObject*)duplicate;
}

static void
gwy_container_clone_real(GObject *source,
                         GObject *copy)
{
    GwyContainer *container, *clone;

    g_return_if_fail(GWY_IS_CONTAINER(source));
    g_return_if_fail(GWY_IS_CONTAINER(copy));
    container = GWY_CONTAINER(source);
    clone = GWY_CONTAINER(copy);

    g_hash_table_foreach_remove(container->values,
                                (GHRFunc)hash_remove_all_func, NULL);
    g_hash_table_foreach(container->values, hash_duplicate_func, clone);
}

static gboolean
hash_remove_all_func(void)
{
    return TRUE;
}

static void
hash_duplicate_func(gpointer hkey, gpointer hvalue, gpointer hdata)
{
    GQuark key = GPOINTER_TO_UINT(hkey);
    GValue *value = (GValue*)hvalue;
    GwyContainer *duplicate = (GwyContainer*)hdata;
    GType type = G_VALUE_TYPE(value);
    GObject *object;

    switch (type) {
        case G_TYPE_OBJECT:
        /* objects have to be handled separately since we want a deep copy */
        object = gwy_serializable_duplicate(g_value_get_object(value));
        gwy_container_set_object(duplicate, key, object);
        g_object_unref(object);
        break;

        case G_TYPE_STRING:
        gwy_container_set_string(duplicate, key, g_value_dup_string(value));
        break;

        case G_TYPE_BOOLEAN:
        case G_TYPE_UCHAR:
        case G_TYPE_INT:
        case G_TYPE_INT64:
        case G_TYPE_DOUBLE:
        gwy_container_set_value(duplicate, key, value, NULL);
        break;

        default:
        g_warning("Cannot properly duplicate %s", g_type_name(type));
        gwy_container_set_value(duplicate, key, value, NULL);
        break;
    }
}

/**
 * gwy_container_duplicate_by_prefix:
 * @container: A container.
 * @...: A %NULL-terminated list of string keys.
 *
 * Duplicates a container keeping only values under given prefixes.
 *
 * Like gwy_container_duplicate(), this method creates a deep copy, that is
 * contained object are physically duplicated too, not just referenced again.
 *
 * Returns: A newly created container.
 **/
GwyContainer*
gwy_container_duplicate_by_prefix(GwyContainer *container,
                                  ...)
{
    GwyContainer *duplicate;
    va_list ap;

    g_return_val_if_fail(GWY_IS_CONTAINER(container), NULL);

    va_start(ap, container);
    duplicate = gwy_container_duplicate_by_prefix_valist(container, ap);
    va_end(ap);

    return duplicate;
}

static GwyContainer*
gwy_container_duplicate_by_prefix_valist(GwyContainer *container,
                                         va_list ap)
{
    GwyContainer *duplicate;
    PrefixListData pfxlist;
    const gchar *prefix;
    gsize n;

    n = 16;
    pfxlist.prefixes = g_new(const gchar*, n);
    pfxlist.nprefixes = 0;
    prefix = va_arg(ap, const gchar*);
    while (prefix) {
        if (pfxlist.nprefixes == n) {
            n += 16;
            pfxlist.prefixes = g_renew(const gchar*, pfxlist.prefixes, n);
        }
        pfxlist.prefixes[pfxlist.nprefixes] = prefix;
        pfxlist.nprefixes++;
        prefix = va_arg(ap, const gchar*);
    }
    pfxlist.pfxlengths = g_new(gsize, n);
    pfxlist.pfxclosed = g_new(gboolean, n);
    for (n = 0; n < pfxlist.nprefixes; n++) {
        pfxlist.pfxlengths[n] = strlen(pfxlist.prefixes[n]);
        pfxlist.pfxclosed[n] = !pfxlist.pfxlengths[n]
                               || pfxlist.prefixes[n][pfxlist.pfxlengths[n] - 1]
                                  == GWY_CONTAINER_PATHSEP;
    }

    /* don't emit signals when no one can be connected */
    duplicate = (GwyContainer*)gwy_container_new();
    duplicate->in_construction = TRUE;
    pfxlist.container = duplicate;
    g_hash_table_foreach(container->values,
                         hash_prefix_duplicate_func, &pfxlist);
    duplicate->in_construction = FALSE;

    g_free(pfxlist.prefixes);
    g_free(pfxlist.pfxclosed);
    g_free(pfxlist.pfxlengths);

    return duplicate;
}

static void
hash_prefix_duplicate_func(gpointer hkey, gpointer hvalue, gpointer hdata)
{
    GQuark key = GPOINTER_TO_UINT(hkey);
    GValue *value = (GValue*)hvalue;
    PrefixListData *pfxlist = (PrefixListData*)hdata;
    GwyContainer *duplicate;
    GType type = G_VALUE_TYPE(value);
    GObject *object;
    const gchar *name;
    gsize n;
    gboolean ok = FALSE;

    duplicate = pfxlist->container;
    name = g_quark_to_string(key);
    if (!name)
        return;

    for (n = 0; n < pfxlist->nprefixes; n++) {
        if (strncmp(name, pfxlist->prefixes[n], pfxlist->pfxlengths[n]) == 0
            && (pfxlist->pfxclosed[n]
                || name[pfxlist->pfxlengths[n]] == '\0'
                || name[pfxlist->pfxlengths[n]] == GWY_CONTAINER_PATHSEP)) {
            ok = TRUE;
            break;
        }
    }
    if (!ok)
        return;

    switch (type) {
        case G_TYPE_OBJECT:
        /* objects have to be handled separately since we want a deep copy */
        object = gwy_serializable_duplicate(g_value_get_object(value));
        gwy_container_set_object(duplicate, key, object);
        g_object_unref(object);
        break;

        case G_TYPE_STRING:
        gwy_container_set_string(duplicate, key, g_value_dup_string(value));
        break;

        case G_TYPE_BOOLEAN:
        case G_TYPE_UCHAR:
        case G_TYPE_INT:
        case G_TYPE_INT64:
        case G_TYPE_DOUBLE:
        gwy_container_set_value(duplicate, key, value, NULL);
        break;

        default:
        g_warning("Cannot properly duplicate %s", g_type_name(type));
        gwy_container_set_value(duplicate, key, value, NULL);
        break;
    }
}

static void
hash_text_serialize_func(gpointer hkey, gpointer hvalue, gpointer hdata)
{
    static const guchar hexdigits[] = "0123456789abcdef";
    static gchar buf[G_ASCII_DTOSTR_BUF_SIZE];
    GQuark key = GPOINTER_TO_UINT(hkey);
    GValue *value = (GValue*)hvalue;
    GPtrArray *pa = (GPtrArray*)hdata;
    GType type = G_VALUE_TYPE(value);
    gchar *k, *v, *s;
    GByteArray *b;
    gsize i, j;
    guchar c;

    k = g_strescape(g_quark_to_string(key), NULL);
    v = NULL;
    switch (type) {
        case G_TYPE_BOOLEAN:
        v = g_strdup_printf("\"%s\" boolean %s",
                            k, g_value_get_boolean(value) ? "True" : "False");
        break;

        case G_TYPE_UCHAR:
        c = g_value_get_uchar(value);
        if (g_ascii_isprint(c) && !g_ascii_isspace(c))
            v = g_strdup_printf("\"%s\" char %c", k, c);
        else
            v = g_strdup_printf("\"%s\" char 0x%02x", k, c);
        break;

        case G_TYPE_INT:
        v = g_strdup_printf("\"%s\" int32 %d", k, g_value_get_int(value));
        break;

        case G_TYPE_INT64:
        /* FIXME: this may fail */
        v = g_strdup_printf("\"%s\" int64 %" G_GINT64_FORMAT,
                            k, g_value_get_int64(value));
        break;

        case G_TYPE_DOUBLE:
        g_ascii_dtostr(buf, G_ASCII_DTOSTR_BUF_SIZE, g_value_get_double(value));
        v = g_strdup_printf("\"%s\" double %s", k, buf);
        break;

        case G_TYPE_STRING:
        s = g_strescape(g_value_get_string(value), NULL);
        v = g_strdup_printf("\"%s\" string \"%s\"", k, s);
        g_free(s);
        break;

        case G_TYPE_OBJECT:
        g_warning("Forced to serialize object %s to text",
                  G_OBJECT_TYPE_NAME(g_value_get_object(value)));
        b = gwy_serializable_serialize(g_value_get_object(value), NULL);
        g_assert(b);
        j = strlen(k);
        v = g_new(gchar, 1 + j + 2 + sizeof("object") + 2*b->len + 1);
        v[0] = '"';
        memcpy(v+1, k, j);
        memcpy(v+j+1, "\" object ", sizeof("\" object ") - 1);
        for (i = 0; i < b->len; i++) {
            v[3 + j + sizeof("object") + 2*i] = hexdigits[b->data[i] >> 4];
            v[4 + j + sizeof("object") + 2*i] = hexdigits[b->data[i] & 0xf];
        }
        v[3 + j + sizeof("object") + 2*b->len] = '\0';
        g_byte_array_free(b, TRUE);
        break;

        default:
        g_critical("Cannot pack GValue holding %s", g_type_name(type));
        break;
    }
    if (v)
        g_ptr_array_add(pa, v);
    g_free(k);
}

/**
 * gwy_container_transfer:
 * @source: Source container.
 * @dest: Destination container. It may be the same container as @source, but
 *        @source_prefix and @dest_prefix may not overlap then.
 * @source_prefix: Prefix in @source to take values from.
 * @dest_prefix: Prefix in @dest to put values to.
 * @force: %TRUE to replace existing values in @dest.
 *
 * Copies a items from one place in container to another place.
 *
 * The copies are shallow, objects are not physically duplicated, only
 * referenced in @dest.
 *
 * Returns: The number of actually transferred items.
 **/
gint
gwy_container_transfer(GwyContainer *source,
                       GwyContainer *dest,
                       const gchar *source_prefix,
                       const gchar *dest_prefix,
                       gboolean force)
{
    PrefixData pfdata;
    GValue *val, *copy;
    GString *key;
    GQuark quark;
    guint dpflen;
    GSList *l;

    g_return_val_if_fail(GWY_IS_CONTAINER(source), 0);
    g_return_val_if_fail(GWY_IS_CONTAINER(dest), 0);
    g_return_val_if_fail(source_prefix, 0);
    g_return_val_if_fail(dest_prefix, 0);
    if (source == dest) {
        if (gwy_strequal(source_prefix, dest_prefix))
            return 0;

        g_return_val_if_fail(!g_str_has_prefix(source_prefix, dest_prefix), 0);
        g_return_val_if_fail(!g_str_has_prefix(dest_prefix, source_prefix), 0);
    }

    pfdata.container = source;
    pfdata.prefix = source_prefix;
    pfdata.prefix_length = strlen(pfdata.prefix);
    pfdata.count = 0;
    pfdata.keylist = NULL;
    pfdata.closed_prefix = !pfdata.prefix_length
                           || (source_prefix[pfdata.prefix_length - 1]
                               == GWY_CONTAINER_PATHSEP);
    g_hash_table_foreach(source->values, hash_find_keys_func, &pfdata);
    if (!pfdata.keylist)
        return 0;

    pfdata.keylist = g_slist_reverse(pfdata.keylist);
    key = g_string_new(dest_prefix);
    dpflen = strlen(dest_prefix);
    if (dest_prefix[dpflen - 1] == GWY_CONTAINER_PATHSEP)
        dpflen--;
    if (pfdata.closed_prefix)
        pfdata.prefix_length--;

    pfdata.count = 0;
    for (l = pfdata.keylist; l; l = g_slist_next(l)) {
        val = (GValue*)g_hash_table_lookup(source->values, l->data);
        if (!val) {
            g_critical("Container contents changed during "
                       "gwy_container_transfer().");
            break;
        }
        g_string_truncate(key, dpflen);
        g_string_append(key,
                        g_quark_to_string(GPOINTER_TO_UINT(l->data))
                        + pfdata.prefix_length);
        quark = g_quark_from_string(key->str);

        if (!force && g_hash_table_lookup(dest->values, GUINT_TO_POINTER(quark)))
            continue;

        copy = g_new0(GValue, 1);
        g_value_init(copy, G_VALUE_TYPE(val));
        g_value_copy(val, copy);

        g_hash_table_insert(dest->values, GUINT_TO_POINTER(quark), copy);
        g_signal_emit(dest, container_signals[ITEM_CHANGED], quark, quark);
        pfdata.count++;
    }
    g_slist_free(pfdata.keylist);
    g_string_free(key, TRUE);

    return pfdata.count;
}

static void
hash_find_keys_func(gpointer hkey,
                    G_GNUC_UNUSED gpointer hvalue,
                    gpointer hdata)
{
    GQuark key = GPOINTER_TO_UINT(hkey);
    PrefixData *pfdata = (PrefixData*)hdata;
    const gchar *name;

    if (pfdata->prefix
        && (!(name = g_quark_to_string(key))
            || !g_str_has_prefix(name, pfdata->prefix)
            || (!pfdata->closed_prefix
                && name[pfdata->prefix_length] != GWY_CONTAINER_PATHSEP)))
        return;

    pfdata->count++;
    pfdata->keylist = g_slist_prepend(pfdata->keylist,
                                      GUINT_TO_POINTER(hkey));
}

static int
pstring_compare_callback(const void *p, const void *q)
{
    return strcmp(*(gchar**)p, *(gchar**)q);
}

/**
 * gwy_container_serialize_to_text:
 * @container: A container.
 *
 * Creates a text representation of @container contents.
 *
 * Note only simple data types are supported as serialization of compound
 * objects is not controllable.
 *
 * Returns: A pointer array, each item containing string with one container
 * item representation (name, type, value).  The array is sorted by name.
 **/
GPtrArray*
gwy_container_serialize_to_text(GwyContainer *container)
{
    GPtrArray *pa;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_CONTAINER(container), NULL);

    pa = g_ptr_array_new();
    g_hash_table_foreach(container->values, hash_text_serialize_func, pa);
    g_ptr_array_sort(pa, pstring_compare_callback);

    return pa;
}

static guint
token_length(const gchar *text)
{
    guint i;

    g_assert(!g_ascii_isspace(*text));
    if (*text == '"') {
        /* quoted string */
        for (i = 1; text[i] != '"'; i++) {
            if (text[i] == '\\') {
               i++;
               if (!text[i])
                   return (guint)-1;
            }
        }
        i++;
    }
    else {
        /* normal token */
        for (i = 0; text[i] && !g_ascii_isspace(text[i]); i++)
            ;
    }

    return i;
}

static gchar*
dequote_token(const gchar *tok, gsize *len)
{
    gchar *s, *t;

    if (*len < 2 || *tok != '"')
        return g_strndup(tok, *len);

    g_assert(tok[*len-1] == '"');
    s = g_strndup(tok+1, *len-2);
    t = g_strcompress(s);
    g_free(s);

    return t;
}

/**
 * gwy_container_deserialize_from_text:
 * @text: Text containing serialized container contents as dumped by
 *        gwy_container_serialize_to_text().
 *
 * Restores a container from is text representation.
 *
 * Returns: The restored container, or %NULL on failure.
 **/
GwyContainer*
gwy_container_deserialize_from_text(const gchar *text)
{
    GwyContainer *container;
    const gchar *tok, *type;
    gchar *name = NULL;
    gsize len, namelen, typelen;
    GQuark key;

    container = GWY_CONTAINER(gwy_container_new());
    container->in_construction = TRUE;

    for (tok = text; g_ascii_isspace(*tok); tok++)
        ;
    while ((len = token_length(tok))) {
        /* name */
        if (len == (guint)-1)
            goto fail;
        namelen = len;
        name = dequote_token(tok, &namelen);
        key = g_quark_from_string(name);
        gwy_debug("got name <%s>", name);

        /* type */
        for (tok = tok + len; g_ascii_isspace(*tok); tok++)
            ;
        if (!(len = token_length(tok)) || len == (guint)-1)
            goto fail;
        type = tok;
        typelen = len;
        gwy_debug("got type <%.*s>", (guint)typelen, type);

        /* value */
        for (tok = tok + len; g_ascii_isspace(*tok); tok++)
            ;
        if (!(len = token_length(tok)) || len == (guint)-1)
            goto fail;
        /* boolean */
        if (typelen+1 == sizeof("boolean")
            && g_str_has_prefix(type, "boolean")) {
            if (len == 4 && g_str_has_prefix(tok, "True"))
                gwy_container_set_boolean(container, key, TRUE);
            else if (len == 5 && g_str_has_prefix(tok, "False"))
                gwy_container_set_boolean(container, key, FALSE);
            else
                goto fail;
        }
        /* char */
        else if (typelen+1 == sizeof("char")
                 && g_str_has_prefix(type, "char")) {
            guint c;

            if (len == 1)
                c = *tok;
            else {
                if (len != 4)
                    goto fail;
                sscanf(tok+2, "%x", &c);
            }
            gwy_container_set_uchar(container, key, (guchar)c);
        }
        /* int32 */
        else if (typelen+1 == sizeof("int32")
                 && g_str_has_prefix(type, "int32")) {
            gwy_container_set_int32(container, key, strtol(tok, NULL, 0));
        }
        /* int64 */
        else if (typelen+1 == sizeof("int64")
                 && g_str_has_prefix(type, "int64")) {
            gwy_container_set_int64(container, key,
                                    g_ascii_strtoull(tok, NULL, 0));
        }
        /* double */
        else if (typelen+1 == sizeof("double")
                 && g_str_has_prefix(type, "double")) {
            gwy_container_set_double(container, key, g_ascii_strtod(tok, NULL));
        }
        /* string */
        else if (typelen+1 == sizeof("string")
                 && g_str_has_prefix(type, "string")) {
            gchar *s;
            gsize vallen;

            vallen = len;
            s = dequote_token(tok, &vallen);
            gwy_container_set_string(container, key, s);
        }
        /* object */
        else if (typelen+1 == sizeof("object")
                 && g_str_has_prefix(type, "object")) {
            guchar *buf;
            GObject *object;
            gsize i, pos = 0;

            if (len % 2)
                goto fail;

            buf = g_new(guchar, len/2);
            for (i = 0; i < len/2; i++) {
                gint hi = g_ascii_xdigit_value(tok[2*i]);
                gint low = g_ascii_xdigit_value(tok[2*i + 1]);

                if (hi == -1 || low == -1) {
                    g_free(buf);
                    tok += 2*i;   /* for warning */
                    goto fail;
                }
                buf[i] = (hi << 4) | low;
            }
            object = gwy_serializable_deserialize(buf, len/2, &pos);
            g_free(buf);
            if (object) {
                gwy_container_set_object(container, key, object);
                g_object_unref(object);
            }
            else
                g_warning("cannot deserialize object %.*s",
                          (guint)namelen, name);
        }
        /* UFO */
        else {
            tok = type;  /* for warning */
            goto fail;
        }
        g_free(name);
        name = NULL;

        /* skip space */
        for (tok = tok + len; g_ascii_isspace(*tok); tok++)
            ;
    }
    container->in_construction = FALSE;

    return container;

fail:
    g_free(name);
    g_warning("parsing failed at <%.18s...>", tok);
    g_object_unref(container);
    return NULL;
}

/************************** Documentation ****************************/

/**
 * SECTION:gwycontainer
 * @title: GwyContainer
 * @short_description: A container with items identified by a GQuark
 * @see_also: #GwyInventory
 *
 * #GwyContainer is a general-purpose container, it can hold atomic types,
 * strings and objects. However, objects must implement the #GwySerializable
 * interface, because the container itself is serializable.
 *
 * A new container can be created with gwy_container_new(), items can be stored
 * with function like gwy_container_set_double(), read with
 * gwy_container_get_double(), and removed with gwy_container_remove() or
 * gwy_container_remove_by_prefix(). A presence of a value can be tested with
 * gwy_container_contains(), convenience functions for reading (updating) a
 * value only if it is present like gwy_container_gis_double(), are available
 * too.
 *
 * #GwyContainer takes ownership of stored non-atomic items. For strings, this
 * means you cannot store static strings (use g_strdup() to duplicate them),
 * and must not free stored dynamic strings, as the container will free them
 * itself when they are removed or when the container is finalized. For
 * objects, this means it takes a reference on the object (released when the
 * object is removed or the container is finalized), so you usually want to
 * g_object_unref() objects after storing them to a container.
 *
 * Items in a #GwyContainer can be identified by a #GQuark or the corresponding
 * string.  While #GQuark's are atomic values and allow faster acces, they are
 * less convenient for casual usage -- each #GQuark-key function like
 * gwy_container_set_double() thus has a string-key counterpart
 * gwy_container_set_double_by_name().
 **/

/**
 * GwyContainer:
 *
 * The #GwyContainer struct contains private data only and should be accessed
 * using the functions below.
 **/

/**
 * GWY_CONTAINER_PATHSEP:
 *
 * Path separator to be used for hierarchical structures in the container.
 **/

/**
 * GWY_CONTAINER_PATHSEP_STR:
 *
 * Path separator to be used for hierarchical structures in the container,
 * as a string.
 **/

/**
 * gwy_container_duplicate:
 * @container: A container to duplicate.
 *
 * Convenience macro doing gwy_serializable_duplicate() with all the necessary
 * typecasting.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
