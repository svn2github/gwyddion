/*
 *  $Id$
 *  Copyright (C) 2009 David Necas (Yeti).
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
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <glib/gi18n-lib.h>
#include "libgwy/macros.h"
#include "libgwy/strfuncs.h"
#include "libgwy/container.h"
#include "libgwy/serializable-boxed.h"
#include "libgwy/serialize.h"
#include "libgwy/libgwy-aliases.h"

enum {
    ITEM_CHANGED,
    N_SIGNALS
};

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

struct _GwyContainer {
    GObject g_object;
    GHashTable *values;
    gboolean in_construction;
};

static void     gwy_container_serializable_init(GwySerializableInterface *iface);
static gsize    gwy_container_n_items_impl     (GwySerializable *serializable);
static gsize    gwy_container_itemize          (GwySerializable *serializable,
                                                GwySerializableItems *items);
static void     gwy_container_finalize         (GObject *object);
static void     gwy_container_dispose          (GObject *object);
static GObject* gwy_container_construct        (GwySerializableItems *items,
                                                GwyErrorList **error_list);
static GObject* gwy_container_duplicate_impl   (GwySerializable *object);
static void     gwy_container_assign_impl      (GwySerializable *destination,
                                                GwySerializable *source);
static void     value_destroy                  (gpointer data);
static void     hash_object_dispose            (gpointer hkey,
                                                gpointer hvalue,
                                                gpointer hdata);
static GValue*  get_value_of_type              (GwyContainer *container,
                                                GQuark key,
                                                GType type);
static GValue*  gis_value_of_type              (GwyContainer *container,
                                                GQuark key,
                                                GType type);
static void     hash_count_items               (gpointer hkey,
                                                gpointer hvalue,
                                                gpointer hdata);
static void     hash_itemize                   (gpointer hkey,
                                                gpointer hvalue,
                                                gpointer hdata);
static gboolean hash_remove_prefix             (gpointer hkey,
                                                gpointer hvalue,
                                                gpointer hdata);
static void     hash_foreach                   (gpointer hkey,
                                                gpointer hvalue,
                                                gpointer hdata);
static void     keys_foreach                   (gpointer hkey,
                                                gpointer hvalue,
                                                gpointer hdata);
static void     keys_by_name_foreach           (gpointer hkey,
                                                gpointer hvalue,
                                                gpointer hdata);
static void     hash_duplicate                 (gpointer hkey,
                                                gpointer hvalue,
                                                gpointer hdata);
static void     hash_find_keys                 (gpointer hkey,
                                                gpointer hvalue,
                                                gpointer hdata);
static int      pstring_compare                (const void *p,
                                                const void *q);
static guint    token_length                   (const gchar *text);
static gchar*   dequote_token                  (const gchar *tok,
                                                gsize *len);

static guint container_signals[N_SIGNALS];

G_DEFINE_TYPE_EXTENDED
    (GwyContainer, gwy_container, G_TYPE_OBJECT, 0,
     GWY_IMPLEMENT_SERIALIZABLE(gwy_container_serializable_init))

static void
gwy_container_serializable_init(GwySerializableInterface *iface)
{
    iface->duplicate = gwy_container_duplicate_impl;
    iface->assign = gwy_container_assign_impl;
    /* A bit unfortunate naming: gwy_container_n_items() counts the number of
     * items. */
    iface->n_items = gwy_container_n_items_impl;
    iface->itemize = gwy_container_itemize;
    iface->construct = gwy_container_construct;
}

static void
gwy_container_class_init(GwyContainerClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->dispose = gwy_container_dispose;
    gobject_class->finalize = gwy_container_finalize;

    /**
     * GwyContainer::item-changed:
     * @gwycontainer: The #GwyContainer which received the signal.
     * @arg1: #GQuark key identifying the changed item.
     *
     * The ::item-changed signal is emitted whenever a container item is
     * changed.
     *
     * This signal is detailed and the detail is the string key identifier.
     **/
    container_signals[ITEM_CHANGED]
        = g_signal_new_class_handler("item-changed",
                                     G_OBJECT_CLASS_TYPE(klass),
                                     G_SIGNAL_RUN_FIRST | G_SIGNAL_DETAILED
                                     | G_SIGNAL_NO_RECURSE,
                                     NULL, NULL, NULL,
                                     g_cclosure_marshal_VOID__UINT,
                                     G_TYPE_NONE, 1,
                                     G_TYPE_UINT);
}

static void
gwy_container_init(GwyContainer *container)
{
    container->values = g_hash_table_new_full(NULL, NULL, NULL, value_destroy);
}

static void
gwy_container_finalize(GObject *object)
{
    g_hash_table_destroy(GWY_CONTAINER(object)->values);
    G_OBJECT_CLASS(gwy_container_parent_class)->finalize(object);
}

static void
gwy_container_dispose(GObject *object)
{
    g_hash_table_foreach(GWY_CONTAINER(object)->values,
                         hash_object_dispose, NULL);
    G_OBJECT_CLASS(gwy_container_parent_class)->dispose(object);
}

static void
hash_object_dispose(G_GNUC_UNUSED gpointer hkey,
                    gpointer hvalue,
                    G_GNUC_UNUSED gpointer hdata)
{
    GValue *value = (GValue*)hvalue;

    if (g_type_is_a(G_VALUE_TYPE(value), G_TYPE_OBJECT))
        g_value_reset(value);
}

static gsize
gwy_container_n_items_impl(GwySerializable *serializable)
{
    gsize n_items = 0;
    g_hash_table_foreach(GWY_CONTAINER(serializable)->values,
                         hash_count_items, &n_items);
    return n_items;
}

static void
hash_count_items(G_GNUC_UNUSED gpointer hkey,
                 gpointer hvalue,
                 gpointer hdata)
{
    const GValue *value = (GValue*)hvalue;
    gsize *n_items = (gsize*)hdata;

    (*n_items)++;
    GType type = G_VALUE_TYPE(value);
    if (g_type_is_a(type, G_TYPE_OBJECT)) {
        GObject *object = g_value_get_object(value);
        *n_items += gwy_serializable_n_items(GWY_SERIALIZABLE(object));
    }
    else if (g_type_is_a(type, G_TYPE_BOXED)) {
        *n_items += gwy_serializable_boxed_n_items(type);
    }
}

static gsize
gwy_container_itemize(GwySerializable *serializable,
                      GwySerializableItems *items)
{
    GwyContainer *container = GWY_CONTAINER(serializable);

    g_hash_table_foreach(container->values, hash_itemize, items);
    return g_hash_table_size(container->values);
}

static void
hash_itemize(gpointer hkey, gpointer hvalue, gpointer hdata)
{
    GQuark key = GPOINTER_TO_UINT(hkey);
    GValue *value = (GValue*)hvalue;
    GwySerializableItems *items = (GwySerializableItems*)hdata;
    GwySerializableItem *it;
    GType type = G_VALUE_TYPE(value);

    g_return_if_fail(items->len - items->n_items);
    it = items->items + items->n_items;
    it->name = g_quark_to_string(key);
    it->array_size = 0;
    items->n_items++;

    switch (type) {
        case G_TYPE_BOOLEAN:
        it->ctype = GWY_SERIALIZABLE_BOOLEAN;
        it->value.v_boolean = !!g_value_get_boolean(value);
        return;

        case G_TYPE_CHAR:
        it->ctype = GWY_SERIALIZABLE_INT8;
        it->value.v_int8 = g_value_get_char(value);
        return;

        case G_TYPE_INT:
        it->ctype = GWY_SERIALIZABLE_INT32;
        it->value.v_int32 = g_value_get_int(value);
        return;

        case G_TYPE_INT64:
        it->ctype = GWY_SERIALIZABLE_INT64;
        it->value.v_int64 = g_value_get_int64(value);
        return;

        case G_TYPE_DOUBLE:
        it->ctype = GWY_SERIALIZABLE_DOUBLE;
        it->value.v_double = g_value_get_double(value);
        return;

        case G_TYPE_STRING:
        it->ctype = GWY_SERIALIZABLE_STRING;
        it->value.v_string = (gchar*)g_value_get_string(value);
        return;
    }

    if (g_type_is_a(type, G_TYPE_OBJECT)) {
        it->ctype = GWY_SERIALIZABLE_OBJECT;
        it->value.v_object = g_value_get_object(value);
        gwy_serializable_itemize(GWY_SERIALIZABLE(it->value.v_object), items);
        return;
    }

    if (g_type_is_a(type, G_TYPE_BOXED)) {
        it->ctype = GWY_SERIALIZABLE_BOXED;
        it->value.v_boxed = g_value_get_boxed(value);
        gwy_serializable_boxed_itemize(type, it->value.v_boxed, items);
        return;
    }

    g_warning("Cannot pack GValue holding %s", g_type_name(type));
    return;
}

static GObject*
gwy_container_construct(GwySerializableItems *items,
                        GwyErrorList **error_list)
{
    GwyContainer *container = gwy_container_new();

    for (gsize i = 0; i < items->n_items; i++) {
        GwySerializableItem *it = items->items + i;
        GQuark key = g_quark_from_string(it->name);
        switch (it->ctype) {
            case GWY_SERIALIZABLE_BOOLEAN:
            gwy_container_set_boolean(container, key, it->value.v_boolean);
            break;

            case GWY_SERIALIZABLE_INT8:
            gwy_container_set_char(container, key, it->value.v_int8);
            break;

            case GWY_SERIALIZABLE_INT32:
            gwy_container_set_int32(container, key, it->value.v_int32);
            break;

            case GWY_SERIALIZABLE_INT64:
            gwy_container_set_int64(container, key, it->value.v_int64);
            break;

            case GWY_SERIALIZABLE_DOUBLE:
            gwy_container_set_double(container, key, it->value.v_double);
            break;

            case GWY_SERIALIZABLE_STRING:
            gwy_container_take_string(container, key, it->value.v_string);
            it->value.v_string = NULL;
            break;

            case GWY_SERIALIZABLE_OBJECT:
            if (it->value.v_object) {
                gwy_container_take_object(container, key, it->value.v_object);
                it->value.v_object = NULL;
            }
            break;

            case GWY_SERIALIZABLE_BOXED:
            if (it->value.v_boxed) {
                /* array_size means boxed type */
                gwy_container_set_boxed(container, key, it->array_size,
                                        it->value.v_object);
                g_boxed_free(it->array_size, it->value.v_object);
                it->value.v_boxed = NULL;
            }
            break;

            default:
            gwy_error_list_add(error_list, GWY_DESERIALIZE_ERROR,
                               GWY_DESERIALIZE_ERROR_INVALID,
                               _("GwyContainer cannot store data of type "
                                 "0x%02x."), it->ctype);
            break;
        }
    }

    return G_OBJECT(container);
}

static GObject*
gwy_container_duplicate_impl(GwySerializable *object)
{
    GwyContainer *duplicate = gwy_container_new();
    duplicate->in_construction = TRUE;
    g_hash_table_foreach(GWY_CONTAINER(object)->values,
                         hash_duplicate, duplicate);
    duplicate->in_construction = FALSE;

    return (GObject*)duplicate;
}

static void
gwy_container_assign_impl(GwySerializable *destination,
                          GwySerializable *source)
{
    GwyContainer *container = GWY_CONTAINER(destination);
    GwyContainer *src = GWY_CONTAINER(source);

    /* FIXME: Does not emit item-changed!? */
    g_hash_table_remove_all(container->values);
    g_hash_table_foreach(src->values, hash_duplicate, container);
}

/**
 * gwy_container_new:
 *
 * Creates a new data container.
 *
 * Returns: A new data container.
 **/
GwyContainer*
gwy_container_new(void)
{
    GwyContainer *container;

    container = g_object_new(GWY_TYPE_CONTAINER, NULL);

    return container;
}

static void
value_destroy(gpointer data)
{
    GValue *val = (GValue*)data;
    g_value_unset(val);
    g_slice_free1(sizeof(GValue), val);
}

/**
 * gwy_container_n_items:
 * @container: A container.
 *
 * Gets the number of items in a container.
 *
 * Returns: The number of items.
 **/
guint
gwy_container_n_items(GwyContainer *container)
{
    g_return_val_if_fail(GWY_IS_CONTAINER(container), 0);
    return g_hash_table_size(container->values);
}

/**
 * gwy_container_item_type_n:
 * @c: A container.
 * @n: String item key.
 *
 * Gets the type of value in container @c identified by name @n.
 **/

/**
 * gwy_container_item_type:
 * @container: A container.
 * @key: Quark item key.
 *
 * Returns the type of value in @container identified by @key.
 *
 * Returns: The value type as #GType; 0 if there is no such value.
 **/
GType
gwy_container_item_type(GwyContainer *container, GQuark key)
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
 * gwy_container_contains_n:
 * @c: A container.
 * @n: String item key.
 *
 * Expands to %TRUE if container @c contains a value identified by name @n.
 **/

/**
 * gwy_container_contains:
 * @container: A container.
 * @key: Quark item key.
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
 * gwy_container_remove_n:
 * @c: A container.
 * @n: String item key.
 *
 * Removes a value identified by name @n from container @c.
 *
 * Expands to %TRUE if there was such a value and was removed.
 **/

/**
 * gwy_container_remove:
 * @container: A container.
 * @key: Quark item key.
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

    g_hash_table_remove(container->values, GUINT_TO_POINTER(key));
    g_signal_emit(container, container_signals[ITEM_CHANGED], key, key);

    return TRUE;
}

/**
 * gwy_container_remove_prefix:
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
gwy_container_remove_prefix(GwyContainer *container, const gchar *prefix)
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
    g_hash_table_foreach_remove(container->values, hash_remove_prefix,
                                &pfdata);
    pfdata.keylist = g_slist_reverse(pfdata.keylist);
    for (l = pfdata.keylist; l; l = g_slist_next(l))
        g_signal_emit(container, container_signals[ITEM_CHANGED],
                      GPOINTER_TO_UINT(l->data), GPOINTER_TO_UINT(l->data));
    g_slist_free(pfdata.keylist);

    return pfdata.count;
}

static gboolean
hash_remove_prefix(gpointer hkey,
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
    g_hash_table_foreach(container->values, hash_foreach, &pfdata);

    return pfdata.count;
}

static void
hash_foreach(gpointer hkey, gpointer hvalue, gpointer hdata)
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
 *          with gwy_container_n_items().  If there are no items, %NULL
 *          is returned.
 **/
GQuark*
gwy_container_keys(GwyContainer *container)
{
    g_return_val_if_fail(GWY_IS_CONTAINER(container), NULL);
    guint n = g_hash_table_size(container->values);
    if (!n)
        return NULL;

    GArray *array = g_array_sized_new(FALSE, FALSE, sizeof(GQuark), n);
    g_hash_table_foreach(container->values, keys_foreach, array);
    return (GQuark*)g_array_free(array, FALSE);
}

static void
keys_foreach(gpointer hkey,
             G_GNUC_UNUSED gpointer hvalue,
             gpointer hdata)
{
    GQuark key = GPOINTER_TO_UINT(hkey);
    GArray *array = (GArray*)hdata;

    g_array_append_val(array, key);
}

/**
 * gwy_container_keys_n:
 * @container: A container.
 *
 * Gets all string keys of a container.
 *
 * Returns: A newly allocated, %NULL-terminated array with string keys of all
 *          @container items, in no particular order.  The number of items can
 *          be obtained with gwy_container_n_items().  If there are no items,
 *          %NULL is returned.  The array must be freed by caller, however,
 *          the strings are owned by GLib and must not be freed.
 **/
const gchar**
gwy_container_keys_n(GwyContainer *container)
{
    g_return_val_if_fail(GWY_IS_CONTAINER(container), NULL);
    guint n = g_hash_table_size(container->values);
    if (!n)
        return NULL;

    GPtrArray *array = g_ptr_array_sized_new(n);
    g_hash_table_foreach(container->values, keys_by_name_foreach, array);
    return (const gchar**)g_ptr_array_free(array, FALSE);
}

static void
keys_by_name_foreach(gpointer hkey,
                     G_GNUC_UNUSED gpointer hvalue,
                     gpointer hdata)
{
    GQuark key = GPOINTER_TO_UINT(hkey);
    GPtrArray *array = (GPtrArray*)hdata;

    g_ptr_array_add(array, (gpointer)g_quark_to_string(key));
}

/**
 * gwy_container_rename_n:
 * @c: A container.
 * @n: String item key.
 * @nn: String item key.
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
 * get_value_of_type:
 * @container: A container.
 * @key: Quark item key.
 * @type: Value type to get.  Can be %NULL to not check value type.
 *
 * Low level function to get a value from a container.
 *
 * Causes a warning when no such value exists, or it's of a wrong type.
 * Use gis_value_of_type() to get value that may not exist.
 *
 * Returns: The value identified by @key; %NULL on failure.
 **/
static GValue*
get_value_of_type(GwyContainer *container,
                  GQuark key,
                  GType type)
{
    GValue *p;

    g_return_val_if_fail(GWY_IS_CONTAINER(container), NULL);
    g_return_val_if_fail(key, NULL);
    p = (GValue*)g_hash_table_lookup(container->values, GUINT_TO_POINTER(key));
    if (!p) {
        g_warning("No value for key %u (%s)", key, g_quark_to_string(key));
        return NULL;
    }
    if (type && !G_VALUE_HOLDS(p, type)) {
        g_warning("Trying to get %s as %s, key %u (%s)",
                  G_VALUE_TYPE_NAME(p), g_type_name(type),
                  key, g_quark_to_string(key));
        return NULL;
    }

    return p;
}

/**
 * gis_value_of_type:
 * @container: A container.
 * @key: Quark item key.
 * @type: Value type to get.  Can be %NULL to not check value type.
 *
 * Low level function to get a value from a container.
 *
 * Causes a warning when value is of a wrong type.
 *
 * Returns: The value identified by @key, or %NULL.
 **/
static GValue*
gis_value_of_type(GwyContainer *container,
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
        g_warning("Trying to get %s as %s, key %u (%s)",
                  G_VALUE_TYPE_NAME(p), g_type_name(type),
                  key, g_quark_to_string(key));
        return NULL;
    }

    return p;
}

/**
 * gwy_container_get_value_n:
 * @c: A container.
 * @n: String item key.
 * @v: #GValue to update, it can be either zero-initialized or containing
 *     a value. If item does not exist, it is left untouched.
 *
 * Gets a generic value from a container using a string identifier.
 *
 * Expands to %TRUE if @v was actually updated, %FALSE when there is no
 * such data in the container.
 **/

/**
 * gwy_container_get_value:
 * @container: A container.
 * @key: Quark item key.
 * @value: #GValue to update, it can be either zero-initialized or containing
 *         a value. If item does not exist, it is left untouched.
 *
 * Gets a generic value from a container using a quark identifier.
 *
 * Returns: %TRUE if @value was actually updated, %FALSE when there is no
 *          such data in the container.
 **/
gboolean
gwy_container_get_value(GwyContainer *container,
                        GQuark key,
                        GValue *value)
{
    GValue *p;

    if (!(p = get_value_of_type(container, key, 0)))
        return FALSE;

    if (G_VALUE_TYPE(value))
        g_value_unset(value);
    g_value_init(value, G_VALUE_TYPE(p));
    g_value_copy(p, value);

    return TRUE;
}

/**
 * gwy_container_get_boolean_n:
 * @c: A container.
 * @n: String item key.
 *
 * Gets a boolean from a container using a string identifier.
 **/

/**
 * gwy_container_get_boolean:
 * @container: A container.
 * @key: Quark item key.
 *
 * Gets a boolean from a container using a quark identifier.
 *
 * Returns: The boolean value.
 **/
gboolean
gwy_container_get_boolean(GwyContainer *container, GQuark key)
{
    GValue *p;

    p = get_value_of_type(container, key, G_TYPE_BOOLEAN);
    return G_LIKELY(p) ? !!g_value_get_boolean(p) : FALSE;
}

/**
 * gwy_container_gis_boolean_n:
 * @c: A container.
 * @n: String item key.
 * @v: Pointer to the boolean to update.
 *
 * Updates a boolean from a container using a string identifier.
 *
 * Expands to %TRUE if @v was actually updated, %FALSE when there is no
 * such boolean in the container.
 **/

/**
 * gwy_container_gis_boolean:
 * @container: A container.
 * @key: Quark item key.
 * @value: Pointer to the boolean to update.
 *
 * Updates a boolean from a container using a quark identifier.
 *
 * Returns: %TRUE if @value was actually updated, %FALSE when there is no
 *          such boolean in the container.
 **/
gboolean
gwy_container_gis_boolean(GwyContainer *container,
                          GQuark key,
                          gboolean *value)
{
    GValue *p;

    if ((p = gis_value_of_type(container, key, G_TYPE_BOOLEAN))) {
        *value = !!g_value_get_boolean(p);
        return TRUE;
    }
    return FALSE;
}

/**
 * gwy_container_get_char_n:
 * @c: A container.
 * @n: String item key.
 *
 * Gets a character from a container using a string identifier.
 **/

/**
 * gwy_container_get_char:
 * @container: A container.
 * @key: Quark item key.
 *
 * Gets a character from a container using a quark identifier.
 *
 * Returns: The character as #gchar.
 **/
gchar
gwy_container_get_char(GwyContainer *container, GQuark key)
{
    GValue *p;

    p = get_value_of_type(container, key, G_TYPE_CHAR);
    return G_LIKELY(p) ? g_value_get_char(p) : 0;
}

/**
 * gwy_container_gis_char_n:
 * @c: A container.
 * @n: String item key.
 * @v: Pointer to the character to update.
 *
 * Updates a character from a container using a string identifier.
 *
 * Expands to %TRUE if @v was actually updated, %FALSE when there is no
 * such character in the container.
 **/

/**
 * gwy_container_gis_char:
 * @container: A container.
 * @key: Quark item key.
 * @value: Pointer to the character to update.
 *
 * Updates a character from a container using a quark identifier.
 *
 * Returns: %TRUE if @value was actually updated, %FALSE when there is no
 *          such character in the container.
 **/
gboolean
gwy_container_gis_char(GwyContainer *container,
                       GQuark key,
                       gchar *value)
{
    GValue *p;

    if ((p = gis_value_of_type(container, key, G_TYPE_CHAR))) {
        *value = g_value_get_char(p);
        return TRUE;
    }
    return FALSE;
}

/**
 * gwy_container_get_int32_n:
 * @c: A container.
 * @n: String item key.
 *
 * Gets a 32bit integer from a container using a string identifier.
 **/

/**
 * gwy_container_get_int32:
 * @container: A container.
 * @key: Quark item key.
 *
 * Gets a 32bit integer from a container using a quark identifier.
 *
 * Returns: The integer as #gint32.
 **/
gint32
gwy_container_get_int32(GwyContainer *container, GQuark key)
{
    GValue *p;

    p = get_value_of_type(container, key, G_TYPE_INT);
    return G_LIKELY(p) ? g_value_get_int(p) : 0;
}

/**
 * gwy_container_gis_int32_n:
 * @c: A container.
 * @n: String item key.
 * @v: Pointer to the 32bit integer to update.
 *
 * Updates a 32bit integer from a container using a string identifier.
 *
 * Expands to %TRUE if @v was actually updated, %FALSE when there is no
 * such 32bit integer in the container.
 **/

/**
 * gwy_container_gis_int32:
 * @container: A container.
 * @key: Quark item key.
 * @value: Pointer to the 32bit integer to update.
 *
 * Updates a 32bit integer from a container using a quark identifier.
 *
 * Returns: %TRUE if @value was actually updated, %FALSE when there is no
 *          such 32bit integer in the container.
 **/
gboolean
gwy_container_gis_int32(GwyContainer *container,
                        GQuark key,
                        gint32 *value)
{
    GValue *p;

    if ((p = gis_value_of_type(container, key, G_TYPE_INT))) {
        *value = g_value_get_int(p);
        return TRUE;
    }
    return FALSE;
}

/**
 * gwy_container_get_enum_n:
 * @c: A container.
 * @n: String item key.
 *
 * Gets an enum value from a container using a string identifier.
 *
 * Note enums are treated as 32bit integers.
 **/

/**
 * gwy_container_get_enum:
 * @container: A container.
 * @key: Quark item key.
 *
 * Gets an enum value from a container using a quark identifier.
 *
 * Note enums are treated as 32bit integers.
 *
 * Returns: The enum as #guint.
 **/
guint
gwy_container_get_enum(GwyContainer *container, GQuark key)
{
    return gwy_container_get_int32(container, key);
}

/**
 * gwy_container_gis_enum_n:
 * @c: A container.
 * @n: String item key.
 * @v: Pointer to the enum to update.
 *
 * Updates an enum from a container using a string identifier.
 *
 * Note enums are treated as 32bit integers.
 *
 * Expands to %TRUE if @v was actually updated, %FALSE when there is no
 * such enum in the container.
 **/

/**
 * gwy_container_gis_enum:
 * @container: A container.
 * @key: Quark item key.
 * @value: Pointer to the enum to update.
 *
 * Updates an enum from a container using a quark identifier.
 *
 * Note enums are treated as 32bit integers.
 *
 * Returns: %TRUE if @value was actually updated, %FALSE when there is no
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
 * gwy_container_get_int64_n:
 * @c: A container.
 * @n: String item key.
 *
 * Gets a 64bit integer from a container using a string identifier.
 **/

/**
 * gwy_container_get_int64:
 * @container: A container.
 * @key: Quark item key.
 *
 * Gets a 64bit integer from a container using a quark identifier.
 *
 * Returns: The integer as #gint64.
 **/
gint64
gwy_container_get_int64(GwyContainer *container, GQuark key)
{
    GValue *p;

    p = get_value_of_type(container, key, G_TYPE_INT64);
    return G_LIKELY(p) ? g_value_get_int64(p) : 0;
}

/**
 * gwy_container_gis_int64_n:
 * @c: A container.
 * @n: String item key.
 * @v: Pointer to the 64bit integer to update.
 *
 * Updates a 64bit integer from a container using a string identifier.
 *
 * Expands to %TRUE if @v was actually updated, %FALSE when there is no
 * such 64bit integer in the container.
 **/

/**
 * gwy_container_gis_int64:
 * @container: A container.
 * @key: Quark item key.
 * @value: Pointer to the 64bit integer to update.
 *
 * Updates a 64bit integer from a container using a quark identifier.
 *
 * Returns: %TRUE if @value was actually updated, %FALSE when there is no
 *          such 64bit integer in the container.
 **/
gboolean
gwy_container_gis_int64(GwyContainer *container,
                        GQuark key,
                        gint64 *value)
{
    GValue *p;

    if ((p = gis_value_of_type(container, key, G_TYPE_INT64))) {
        *value = g_value_get_int64(p);
        return TRUE;
    }
    return FALSE;
}

/**
 * gwy_container_get_double_n:
 * @c: A container.
 * @n: String item key.
 *
 * Gets a double from a container using a string identifier.
 **/

/**
 * gwy_container_get_double:
 * @container: A container.
 * @key: Quark item key.
 *
 * Gets a double from a container using a quark identifier.
 *
 * Returns: The double as #gdouble.
 **/
gdouble
gwy_container_get_double(GwyContainer *container, GQuark key)
{
    GValue *p;

    p = get_value_of_type(container, key, G_TYPE_DOUBLE);
    return G_LIKELY(p) ? g_value_get_double(p) : 0.0;
}

/**
 * gwy_container_gis_double_n:
 * @c: A container.
 * @n: String item key.
 * @v: Pointer to the double to update.
 *
 * Updates a double from a container using a string identifier.
 *
 * Expands to %TRUE if @v was actually updated, %FALSE when there is no
 * such double in the container.
 **/

/**
 * gwy_container_gis_double:
 * @container: A container.
 * @key: Quark item key.
 * @value: Pointer to the double to update.
 *
 * Updates a double from a container using a quark identifier.
 *
 * Returns: %TRUE if @value was actually updated, %FALSE when there is no
 *          such double in the container.
 **/
gboolean
gwy_container_gis_double(GwyContainer *container,
                         GQuark key,
                         gdouble *value)
{
    GValue *p;

    if ((p = gis_value_of_type(container, key, G_TYPE_DOUBLE))) {
        *value = g_value_get_double(p);
        return TRUE;
    }
    return FALSE;
}

/**
 * gwy_container_get_string_n:
 * @c: A container.
 * @n: String item key.
 *
 * Gets a string from a container using a string identifier.
 *
 * The returned string must be treated as constant and never freed or modified.
 **/

/**
 * gwy_container_get_string:
 * @container: A container.
 * @key: Quark item key.
 *
 * Gets a string from a container using a quark identifier.
 *
 * The returned string must be treated as constant and never freed or modified.
 *
 * Returns: The string.
 **/
const gchar*
gwy_container_get_string(GwyContainer *container, GQuark key)
{
    GValue *p;

    p = get_value_of_type(container, key, G_TYPE_STRING);
    return G_LIKELY(p) ? g_value_get_string(p) : NULL;
}

/**
 * gwy_container_gis_string_n:
 * @c: A container.
 * @n: String item key.
 * @v: Pointer to the string pointer to update.
 *
 * Updates a string from a container using a string identifier.
 *
 * The string stored in @v if this function succeeds must be treated as
 * constant and never freed or modified.
 *
 * Expands to %TRUE if @v was actually updated, %FALSE when there is no
 * such string in the container.
 **/

/**
 * gwy_container_gis_string:
 * @container: A container.
 * @key: Quark item key.
 * @value: Pointer to the string pointer to update.
 *
 * Updates a string from a container using a quark identifier.
 *
 * The string stored in @value if this function succeeds must be treated as
 * constant and never freed or modified.
 *
 * Returns: %TRUE if @value was actually updated, %FALSE when there is no
 *          such string in the container.
 **/
gboolean
gwy_container_gis_string(GwyContainer *container,
                         GQuark key,
                         const gchar **value)
{
    GValue *p;

    if ((p = gis_value_of_type(container, key, G_TYPE_STRING))) {
        *value = g_value_get_string(p);
        return TRUE;
    }
    return FALSE;
}

/**
 * gwy_container_get_object_n:
 * @c: A container.
 * @n: String item key.
 *
 * Gets an object from a container using a string identifier.
 *
 * The returned object does not have its reference count increased, use
 * g_object_ref() if you want to access it even when @container may cease
 * to exist.
 **/

/**
 * gwy_container_get_object:
 * @container: A container.
 * @key: Quark item key.
 *
 * Gets an object from a container using a quark identifier.
 *
 * The returned object does not have its reference count increased, use
 * g_object_ref() if you want to access it even when @container may cease
 * to exist.
 *
 * Returns: The object as #gpointer.
 **/
gpointer
gwy_container_get_object(GwyContainer *container, GQuark key)
{
    GValue *p;

    p = get_value_of_type(container, key, G_TYPE_OBJECT);
    return G_LIKELY(p) ? (gpointer)g_value_get_object(p) : NULL;
}

/**
 * gwy_container_gis_object_n:
 * @c: A container.
 * @n: String item key.
 * @v: Pointer to the object pointer to update.
 *
 * Updates an object from a container using a string identifier.
 *
 * The object stored in @v if this function succeeds does not have its
 * reference count increased, use g_object_ref() if you want to access it even
 * when @container may cease to exist.
 *
 * Expands to %TRUE if @v was actually updated, %FALSE when there is no
 * such object in the container.
 **/

/**
 * gwy_container_gis_object:
 * @container: A container.
 * @key: Quark item key.
 * @value: Pointer to the object pointer to update.
 *
 * Updates an object from a container using a quark identifier.
 *
 * The object stored in @value if this function succeeds does not have its
 * reference count increased, use g_object_ref() if you want to access it even
 * when @container may cease to exist.
 * |[
 * GwyUnit *unit = NULL;
 * if (gwy_container_gis_object(container, key, &unit)) {
 *     // Add our own reference.
 *     g_object_ref(unit);
 *     ...
 * }
 * ]|
 *
 * Returns: %TRUE if @value was actually updated, %FALSE when there is no
 *          such object in the container.
 **/
gboolean
gwy_container_gis_object(GwyContainer *container,
                         GQuark key,
                         gpointer value)
{
    GValue *p;

    if ((p = gis_value_of_type(container, key, G_TYPE_OBJECT))) {
        *(GObject**)value = g_value_get_object(p);
        return TRUE;
    }
    return FALSE;
}

/**
 * gwy_container_get_boxed_n:
 * @c: A container.
 * @n: String item key.
 * @t: Serializable boxed type.  You can pass the concrete type
 *     (recommended) or %G_TYPE_BOXED to get any boxed type (if you know
 *     what you are doing).
 *
 * Gets a boxed type from a container using a string identifier.
 *
 * The returned boxed type is owned by @container.  Use g_boxed_copy() to
 * create a private copy if you need a modifiable/long-lasting data.
 **/

/**
 * gwy_container_get_boxed:
 * @container: A container.
 * @key: Quark item key.
 * @type: Serializable boxed type.  You can pass the concrete type
 *        (recommended) or %G_TYPE_BOXED to get any boxed type (if you know
 *        what you are doing).
 *
 * Gets a boxed type from a container using a quark identifier.
 *
 * The returned boxed type is owned by @container.  Use g_boxed_copy() to
 * create a private copy if you need a modifiable/long-lasting data.
 * |[
 * GwyRGBA *color;
 * color = gwy_container_get_boxed(container, key);
 * ]|
 *
 * Returns: Boxed type pointer.
 **/
gconstpointer
gwy_container_get_boxed(GwyContainer *container, GQuark key, GType type)
{
    GValue *p;

    p = get_value_of_type(container, key, type);
    return G_LIKELY(p) ? (gpointer)g_value_get_boxed(p) : NULL;
}

/**
 * gwy_container_gis_boxed_n:
 * @c: A container.
 * @n: String item key.
 * @t: Serializable boxed type.  You can pass the concrete type
 *     (recommended) or %G_TYPE_BOXED to get any boxed type (if you know
 *     what you are doing).
 * @v: Pointer to the boxed type to update.
 *
 * Updates a boxed type from a container using a string identifier.
 *
 * The value is transferred using gwy_serializable_boxed_assign(), i.e. it is
 * assigned by value.
 *
 * Expands to %TRUE if @v was actually updated, %FALSE when there is no
 * such boxed in the container.
 **/

/**
 * gwy_container_gis_boxed:
 * @container: A container.
 * @key: Quark item key.
 * @type: Serializable boxed type.  You can pass the concrete type
 *        (recommended) or %G_TYPE_BOXED to get any boxed type (if you know
 *        what you are doing).
 * @value: Pointer to the boxed type to update.
 *
 * Updates a boxed type from a container using a quark identifier.
 *
 * The value is transferred using gwy_serializable_boxed_assign(), i.e. it is
 * assigned by value.
 * |[
 * GwyRGBA color = { 0, 0, 0, 0 };
 * if (gwy_container_gis_boxed(container, key, &color)) {
 *     ...
 * }
 * ]|
 *
 * Returns: %TRUE if @value was actually updated, %FALSE when there is no
 *          such boxed in the container.
 **/
gboolean
gwy_container_gis_boxed(GwyContainer *container,
                        GQuark key,
                        GType type,
                        gpointer value)
{
    GValue *p;

    if ((p = gis_value_of_type(container, key, type))) {
        gwy_serializable_boxed_assign(G_VALUE_TYPE(p),
                                      value, g_value_get_boxed(p));
        return TRUE;
    }
    return FALSE;
}

static gboolean
values_are_equal(const GValue *value1,
                 const GValue *value2)
{
    if (!value1 || !value2)
        return FALSE;

    GType type = G_VALUE_TYPE(value1);
    if (type != G_VALUE_TYPE(value2))
        return FALSE;

    switch (type) {
        case G_TYPE_BOOLEAN:
        return !g_value_get_boolean(value1) == !g_value_get_boolean(value2);

        case G_TYPE_CHAR:
        return g_value_get_char(value1) == g_value_get_char(value2);

        case G_TYPE_INT:
        return g_value_get_int(value1) == g_value_get_int(value2);

        case G_TYPE_INT64:
        return g_value_get_int64(value1) == g_value_get_int64(value2);

        case G_TYPE_DOUBLE:
        return g_value_get_double(value1) == g_value_get_double(value2);

        case G_TYPE_STRING:
        return gwy_strequal(g_value_get_string(value1),
                            g_value_get_string(value2));
    }

    if (g_type_is_a(type, G_TYPE_OBJECT)) {
        // Objects must be identical, so compare addresses.
        return g_value_get_object(value1) == g_value_get_object(value2);
    }
    if (g_type_is_a(type, G_TYPE_BOXED)) {
        // Boxed values are never equal.
        return FALSE;
    }

    g_return_val_if_reached(FALSE);
}

/**
 * gwy_container_set_value_n:
 * @container: A container.
 * @n: String item key.
 * @value: #GValue with the value to set.
 *
 * Inserts or updates a value identified by name in a data container.
 *
 * The value is copied by the container.
 **/

/**
 * gwy_container_set_value:
 * @container: A container.
 * @key: Quark item key.
 * @value: #GValue with the value to set.
 *
 * Inserts or updates a value identified by quark in a data container.
 *
 * The value is copied by the container.
 **/
void
gwy_container_set_value(GwyContainer *container,
                        GQuark key,
                        const GValue *value)
{
    g_return_if_fail(GWY_IS_CONTAINER(container));
    g_return_if_fail(key);
    g_return_if_fail(value);
    GValue *gvalue = g_hash_table_lookup(container->values,
                                         GUINT_TO_POINTER(key));
    GType newtype = G_VALUE_TYPE(gvalue);
    if (gvalue) {
        GType type = G_VALUE_TYPE(gvalue);
        if (type == newtype) {
            if (values_are_equal(gvalue, value))
                return;
            g_value_copy(value, gvalue);
        }
        else {
            // Be careful not to free something before using it.
            GValue *newvalue = g_slice_new0(GValue);
            g_value_init(newvalue, newtype);
            g_value_copy(value, newvalue);
            g_hash_table_insert(container->values, GUINT_TO_POINTER(key),
                                newvalue);
            // g_value_unset(gvalue); done by hash value destroy function
        }
    }
    else {
        gvalue = g_slice_new0(GValue);
        g_value_init(gvalue, newtype);
        g_value_copy(value, gvalue);
        g_hash_table_insert(container->values, GUINT_TO_POINTER(key), gvalue);
    }
    if (!container->in_construction)
        g_signal_emit(container, container_signals[ITEM_CHANGED], key, key);
}

#define container_set_template(container,key,value,n,N) \
    g_return_if_fail(GWY_IS_CONTAINER(container)); \
    g_return_if_fail(key); \
    GValue *gvalue = g_hash_table_lookup(container->values, \
                                         GUINT_TO_POINTER(key)); \
    if (gvalue) { \
        GType type = G_VALUE_TYPE(gvalue); \
        if (type == G_TYPE_##N) { \
            if (g_value_get_##n(gvalue) == value) \
                return; \
        } \
        else { \
            g_value_unset(gvalue); \
            g_value_init(gvalue, G_TYPE_##N); \
        } \
    } \
    else { \
        gvalue = g_slice_new0(GValue); \
        g_value_init(gvalue, G_TYPE_##N); \
        g_hash_table_insert(container->values, GUINT_TO_POINTER(key), gvalue); \
    } \
    g_value_set_##n(gvalue, value); \
    if (!container->in_construction) \
        g_signal_emit(container, container_signals[ITEM_CHANGED], key, key)

/**
 * gwy_container_set_boolean_n:
 * @c: A container.
 * @n: String item key.
 * @v: A boolean.
 *
 * Stores a boolean identified by a string into a container.
 **/

/**
 * gwy_container_set_boolean:
 * @container: A container.
 * @key: Quark item key.
 * @value: A boolean.
 *
 * Stores a boolean identified by a quark into a container.
 **/
void
gwy_container_set_boolean(GwyContainer *container,
                          GQuark key,
                          gboolean value)
{
    value = !!value;
    container_set_template(container, key, value, boolean, BOOLEAN);
}

/**
 * gwy_container_set_char_n:
 * @c: A container.
 * @n: String item key.
 * @v: A character.
 *
 * Stores a character identified by a string into a container.
 **/

/**
 * gwy_container_set_char:
 * @container: A container.
 * @key: Quark item key.
 * @value: A character.
 *
 * Stores a character identified by a quark into a container.
 **/
void
gwy_container_set_char(GwyContainer *container,
                       GQuark key,
                       gchar value)
{
    container_set_template(container, key, value, char, CHAR);
}

/**
 * gwy_container_set_int32_n:
 * @c: A container.
 * @n: String item key.
 * @v: A 32bit integer.
 *
 * Stores a 32bit integer identified by a string into a container.
 **/

/**
 * gwy_container_set_int32:
 * @container: A container.
 * @key: Quark item key.
 * @value: A 32bit integer.
 *
 * Stores a 32bit integer identified by a quark into a container.
 **/
void
gwy_container_set_int32(GwyContainer *container,
                        GQuark key,
                        gint32 value)
{
    container_set_template(container, key, value, int, INT);
}

/**
 * gwy_container_set_enum_n:
 * @c: A container.
 * @n: String item key.
 * @v: An enum.
 *
 * Stores an enum value identified by a string into a container.
 *
 * Note enums are treated as 32bit integers.
 **/

/**
 * gwy_container_set_enum:
 * @container: A container.
 * @key: Quark item key.
 * @value: An enum integer.
 *
 * Stores an enum value identified by a quark into a container.
 *
 * Note enums are treated as 32bit integers.
 **/
void
gwy_container_set_enum(GwyContainer *container,
                       GQuark key,
                       guint value)
{
    gint32 value32 = value;
    container_set_template(container, key, value32, int, INT);
}

/**
 * gwy_container_set_int64_n:
 * @c: A container.
 * @n: String item key.
 * @v: A 64bit integer.
 *
 * Stores a 64bit integer identified by a string into a container.
 **/

/**
 * gwy_container_set_int64:
 * @container: A container.
 * @key: Quark item key.
 * @value: A 64bit integer.
 *
 * Stores a 64bit integer identified by a quark into a container.
 **/
void
gwy_container_set_int64(GwyContainer *container,
                        GQuark key,
                        gint64 value)
{
    container_set_template(container, key, value, int64, INT64);
}

/**
 * gwy_container_set_double_n:
 * @c: A container.
 * @n: String item key.
 * @v: A double integer.
 *
 * Stores a double identified by a string into a container.
 **/

/**
 * gwy_container_set_double:
 * @container: A container.
 * @key: Quark item key.
 * @value: A double.
 *
 * Stores a double identified by a quark into a container.
 **/
void
gwy_container_set_double(GwyContainer *container,
                         GQuark key,
                         gdouble value)
{
    container_set_template(container, key, value, double, DOUBLE);
}

/**
 * gwy_container_set_string_n:
 * @c: A container.
 * @n: String item key.
 * @v: A nul-terminated string.
 *
 * Copies a string identified by a string into a container.
 *
 * The container makes a copy of the string so, this method can be used on
 * static strings and strings that may be modified or freed.
 **/

/**
 * gwy_container_set_string:
 * @container: A container.
 * @key: Quark item key.
 * @value: A nul-terminated string.
 *
 * Copies a string identified by a quark into a container.
 *
 * The container makes a copy of the string so, this method can be used on
 * static strings and strings that may be modified or freed.
 **/
void
gwy_container_set_string(GwyContainer *container,
                         GQuark key,
                         const gchar *value)
{
    g_return_if_fail(GWY_IS_CONTAINER(container));
    g_return_if_fail(key);
    GValue *gvalue = g_hash_table_lookup(container->values,
                                         GUINT_TO_POINTER(key));
    if (gvalue) {
        GType type = G_VALUE_TYPE(gvalue);
        if (type == G_TYPE_STRING) {
            if (gwy_strequal(g_value_get_string(gvalue), value))
                return;
        }
        else {
            g_value_unset(gvalue);
            g_value_init(gvalue, G_TYPE_STRING);
        }
    }
    else {
        gvalue = g_slice_new0(GValue);
        g_value_init(gvalue, G_TYPE_STRING);
        g_hash_table_insert(container->values, GUINT_TO_POINTER(key), gvalue);
    }
    g_value_set_string(gvalue, value);
    if (!container->in_construction)
        g_signal_emit(container, container_signals[ITEM_CHANGED], key, key);
}

/**
 * gwy_container_take_string_n:
 * @c: A container.
 * @n: String item key.
 * @v: A nul-terminated string.
 *
 * Stores a string identified by a string into a container.
 *
 * The container takes ownership of the string so, this method cannot be used
 * with static strings, use g_strdup() to duplicate them first.  The string
 * becomes fully owned by @container and you must not touch it any more.
 * In fact, it may be already freed when this function returns.
 **/

/**
 * gwy_container_take_string:
 * @container: A container.
 * @key: Quark item key.
 * @value: A nul-terminated string.
 *
 * Stores a string identified by a quark into a container.
 *
 * The container takes ownership of the string so, this method cannot be used
 * with static strings, use g_strdup() to duplicate them first.  The string
 * becomes fully owned by @container and you must not touch it any more.
 * In fact, it may be already freed when this function returns.
 **/
void
gwy_container_take_string(GwyContainer *container,
                          GQuark key,
                          gchar *value)
{
    g_return_if_fail(GWY_IS_CONTAINER(container));
    g_return_if_fail(key);
    GValue *gvalue = g_hash_table_lookup(container->values,
                                         GUINT_TO_POINTER(key));
    if (gvalue) {
        GType type = G_VALUE_TYPE(gvalue);
        if (type == G_TYPE_STRING) {
            if (gwy_strequal(g_value_get_string(gvalue), value)) {
                g_free(value);
                return;
            }
        }
        else {
            g_value_unset(gvalue);
            g_value_init(gvalue, G_TYPE_STRING);
        }
    }
    else {
        gvalue = g_slice_new0(GValue);
        g_value_init(gvalue, G_TYPE_STRING);
        g_hash_table_insert(container->values, GUINT_TO_POINTER(key), gvalue);
    }
    g_value_take_string(gvalue, value);
    if (!container->in_construction)
        g_signal_emit(container, container_signals[ITEM_CHANGED], key, key);
}

/**
 * gwy_container_set_object_n:
 * @c: A container.
 * @n: String item key.
 * @v: An object to store into container.
 *
 * Stores an object identified by a string into a container.
 *
 * See gwy_container_set_object() for details.
 **/

/**
 * gwy_container_set_object:
 * @container: A container.
 * @key: Quark item key.
 * @value: Object to store into the container.
 *
 * Stores an object identified by a quark into a container.
 *
 * The container adds its own reference on the object.
 *
 * The object must implement #GwySerializable interface to allow serialization
 * and other operations with the container.
 * |[
 * GwyUnit *unit = gwy_si_unit_new_from_string("m");
 * gwy_container_set_object(container, key, unit);
 * // Release our own reference.
 * g_object_unref(unit);
 * ]|
 **/
void
gwy_container_set_object(GwyContainer *container,
                         GQuark key,
                         gpointer value)
{
    g_return_if_fail(GWY_IS_CONTAINER(container));
    g_return_if_fail(key);
    GValue *gvalue = g_hash_table_lookup(container->values,
                                         GUINT_TO_POINTER(key));
    GType objtype = G_OBJECT_TYPE(value);
    // Take a reference unconditionally.  The caller's reference may be the
    // one released by g_value_unset() and very bad things would happen if
    // it was the last one.
    g_object_ref(value);
    if (gvalue) {
        GType type = G_VALUE_TYPE(gvalue);
        if (type == objtype) {
            if (g_value_get_object(gvalue) == value) {
                g_object_unref(value);
                return;
            }
        }
        else {
            g_value_unset(gvalue);
            g_value_init(gvalue, objtype);
        }
    }
    else {
        gvalue = g_slice_new0(GValue);
        g_value_init(gvalue, objtype);
        g_hash_table_insert(container->values, GUINT_TO_POINTER(key), gvalue);
    }
    g_value_set_object(gvalue, value);
    g_object_unref(value);
    if (!container->in_construction)
        g_signal_emit(container, container_signals[ITEM_CHANGED], key, key);
}

/**
 * gwy_container_take_object_n:
 * @c: A container.
 * @n: String item key.
 * @v: An object to store into container.
 *
 * Stores an object identified by a string into a container.
 *
 * See gwy_container_take_object() for details.
 **/

/**
 * gwy_container_take_object:
 * @container: A container.
 * @key: Quark item key.
 * @value: Object to store into the container.
 *
 * Stores an object identified by a quark into a container.
 *
 * The container takes the ownership on the object from the caler, i.e. its
 * reference count is not incremented.
 *
 * The object must implement #GwySerializable interface to allow serialization
 * and other operations with the container.
 * |[
 * GwyUnit *unit = gwy_si_unit_new_from_string("m");
 * // Pass our reference to container.
 * gwy_container_take_object(container, key, unit);
 * ]|
 **/
void
gwy_container_take_object(GwyContainer *container,
                          GQuark key,
                          gpointer value)
{
    g_return_if_fail(GWY_IS_CONTAINER(container));
    g_return_if_fail(key);
    GValue *gvalue = g_hash_table_lookup(container->values,
                                         GUINT_TO_POINTER(key));
    GType objtype = G_OBJECT_TYPE(value);
    if (gvalue) {
        GType type = G_VALUE_TYPE(gvalue);
        if (type == objtype) {
            if (g_value_get_object(gvalue) == value) {
                // If the caller did not lie this is not our own reference.
                g_object_unref(value);
                return;
            }
        }
        else {
            // This should be OK because we already own the caller's reference
            // to value so, whatever references may be released now, they are
            // not last.
            g_value_unset(gvalue);
            g_value_init(gvalue, objtype);
        }
    }
    else {
        gvalue = g_slice_new0(GValue);
        g_value_init(gvalue, objtype);
        g_hash_table_insert(container->values, GUINT_TO_POINTER(key), gvalue);
    }
    g_value_take_object(gvalue, value);
    if (!container->in_construction)
        g_signal_emit(container, container_signals[ITEM_CHANGED], key, key);
}

/**
 * gwy_container_set_boxed_n:
 * @c: A container.
 * @n: String item key.
 * @t: Serializable boxed type.
 * @v: Pointer to boxed struct of type @t to store into container.
 *
 * Stores a boxed type identified by a string into a container.
 *
 * See gwy_container_set_boxed() for details.
 **/

/**
 * gwy_container_set_boxed:
 * @container: A container.
 * @key: Quark item key.
 * @type: Serializable boxed type.
 * @value: Pointer to boxed struct of type @type to store into container.
 *
 * Stores a boxed type identified by a quark into a container.
 *
 * The container stored the boxed type by value, i.e. a copy is made.
 *
 * The boxed type must implement the
 * <link linkend="libgwy-serializable-boxed">serializable boxed</link>.
 * to allow serialization and other operations with the container.
 * |[
 * GwyRGBA color = { 0, 0, 0, 0 };
 * gwy_container_set_boxed(container, key, GWY_TYPE_RGBA, &color);
 * ]|
 **/
void
gwy_container_set_boxed(GwyContainer *container,
                        GQuark key,
                        GType boxtype,
                        gpointer value)
{
    g_return_if_fail(GWY_IS_CONTAINER(container));
    g_return_if_fail(key);
    GValue *gvalue = g_hash_table_lookup(container->values,
                                         GUINT_TO_POINTER(key));
    if (gvalue) {
        GType type = G_VALUE_TYPE(gvalue);
        if (type == boxtype) {
            gwy_serializable_boxed_assign(type,
                                          g_value_get_boxed(gvalue), value);
            // Unlike for other types, we always emit item-changed.
            // FIXME: Extend serializable boxed with comparison operator?
        }
        else {
            // Be careful not to free something before using it.
            GValue *newvalue = g_slice_new0(GValue);
            g_value_init(newvalue, boxtype);
            g_value_copy(value, newvalue);
            g_hash_table_insert(container->values, GUINT_TO_POINTER(key),
                                newvalue);
            // g_value_unset(gvalue); done by hash value destroy function
        }
    }
    else {
        gvalue = g_slice_new0(GValue);
        g_value_init(gvalue, boxtype);
        g_value_set_boxed(gvalue, value);
        g_hash_table_insert(container->values, GUINT_TO_POINTER(key), gvalue);
    }
    if (!container->in_construction)
        g_signal_emit(container, container_signals[ITEM_CHANGED], key, key);
}

static void
set_copied_value(GwyContainer *container,
                 GQuark key,
                 const GValue *value)
{
    GType type = G_VALUE_TYPE(value);

    switch (type) {
        case G_TYPE_BOOLEAN:
        case G_TYPE_CHAR:
        case G_TYPE_INT:
        case G_TYPE_INT64:
        case G_TYPE_DOUBLE:
        case G_TYPE_STRING:
        gwy_container_set_value(container, key, value);
        return;
    }

    // Objects have to be handled separately since we want a deep copy.
    if (g_type_is_a(type, G_TYPE_OBJECT)) {
        GObject *object = g_value_get_object(value);
        object = gwy_serializable_duplicate(GWY_SERIALIZABLE(object));
        gwy_container_take_object(container, key, object);
        return;
    }

    // Boxed are set by value.
    if (g_type_is_a(type, G_TYPE_BOXED)) {
        gwy_container_set_value(container, key, value);
        return;
    }

    g_warning("Cannot properly copy value of type %s", g_type_name(type));
    gwy_container_set_value(container, key, value);
}

static void
hash_duplicate(gpointer hkey, gpointer hvalue, gpointer hdata)
{
    GQuark key = GPOINTER_TO_UINT(hkey);
    const GValue *value = (GValue*)hvalue;
    GwyContainer *duplicate = (GwyContainer*)hdata;

    set_copied_value(duplicate, key, value);
}

static void
hash_text_serialize(gpointer hkey, gpointer hvalue, gpointer hdata)
{
    static gchar buf[G_ASCII_DTOSTR_BUF_SIZE];
    GQuark key = GPOINTER_TO_UINT(hkey);
    GValue *value = (GValue*)hvalue;
    GPtrArray *pa = (GPtrArray*)hdata;
    GType type = G_VALUE_TYPE(value);
    gchar *k, *v, *s;
    guchar c;

    k = g_strescape(g_quark_to_string(key), NULL);
    v = NULL;
    switch (type) {
        case G_TYPE_BOOLEAN:
        v = g_strdup_printf("\"%s\" boolean %s",
                            k, g_value_get_boolean(value) ? "True" : "False");
        break;

        case G_TYPE_CHAR:
        c = (guchar)g_value_get_char(value);
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

        default:
        g_warning("Cannot dump item at \"%s\" holding %s to text",
                  k, g_type_name(type));
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
 * @deep: %TRUE to perform a deep copy, %FALSE to perform a shallow copy.
 *        This option pertains only to contained objects, strings are always
 *        duplicated.
 * @force: %TRUE to replace existing values in @dest.
 *
 * Copies a items from one place in a container to another place and/or another
 * container.
 *
 * Returns: The number of actually transferred items.
 **/
guint
gwy_container_transfer(GwyContainer *source,
                       GwyContainer *dest,
                       const gchar *source_prefix,
                       const gchar *dest_prefix,
                       gboolean deep,
                       gboolean force)
{
    PrefixData pfdata;
    GString *key;
    guint dpflen;

    g_return_val_if_fail(GWY_IS_CONTAINER(source), 0);
    g_return_val_if_fail(GWY_IS_CONTAINER(dest), 0);
    if (!source_prefix)
        source_prefix = "";
    if (!dest_prefix)
        dest_prefix = "";
    if (source == dest) {
        if (gwy_strequal(source_prefix, dest_prefix))
            return 0;

        g_return_val_if_fail(!g_str_has_prefix(source_prefix, dest_prefix), 0);
        g_return_val_if_fail(!g_str_has_prefix(dest_prefix, source_prefix), 0);
    }

    /* Build a list of item keys we need to tansfer. */
    pfdata.container = source;
    pfdata.prefix = source_prefix;
    pfdata.prefix_length = strlen(pfdata.prefix);
    pfdata.count = 0;
    pfdata.keylist = NULL;
    pfdata.closed_prefix = !pfdata.prefix_length
                           || (source_prefix[pfdata.prefix_length - 1]
                               == GWY_CONTAINER_PATHSEP);
    g_hash_table_foreach(source->values, hash_find_keys, &pfdata);
    if (!pfdata.keylist)
        return 0;

    pfdata.keylist = g_slist_reverse(pfdata.keylist);
    key = g_string_new(dest_prefix);
    dpflen = strlen(dest_prefix);
    if (dpflen && dest_prefix[dpflen - 1] == GWY_CONTAINER_PATHSEP)
        dpflen--;
    if (pfdata.closed_prefix && pfdata.prefix_length)
        pfdata.prefix_length--;

    /* Transfer the items */
    pfdata.count = 0;
    for (GSList *l = pfdata.keylist; l; l = g_slist_next(l)) {
        GValue *val = (GValue*)g_hash_table_lookup(source->values, l->data);
        if (G_UNLIKELY(!val)) {
            g_critical("Source container contents changed during "
                       "gwy_container_transfer().");
            break;
        }

        g_string_truncate(key, dpflen);
        g_string_append(key,
                        g_quark_to_string(GPOINTER_TO_UINT(l->data))
                        + pfdata.prefix_length);

        GQuark quark = g_quark_from_string(key->str);
        GValue *copy = (GValue*)g_hash_table_lookup(dest->values,
                                                    GUINT_TO_POINTER(quark));
        gboolean exists = (copy != NULL);
        if (exists && (!force || values_are_equal(val, copy)))
            continue;

        /* FIXME: Hopefully the rules for what can be copied where ensure that
         * we do not free the very same value we are copying. */
        if (exists)
            g_value_unset(copy);
        else
            copy = g_slice_new0(GValue);
        GType type = G_VALUE_TYPE(val);
        g_value_init(copy, type);

        /* This g_value_copy() makes a real copy of everything, including
         * strings, but it only references objects while deep copy requires
         * duplication. */
        if (deep && g_type_is_a(type, G_TYPE_OBJECT)) {
            GwySerializable *serializable;
            serializable = GWY_SERIALIZABLE(g_value_get_object(val));
            if (G_UNLIKELY(!serializable)) {
                g_critical("Cannot duplicate a nonserializable object.");
                g_value_copy(val, copy);
            }
            else
                g_value_take_object(copy,
                                    gwy_serializable_duplicate(serializable));
        }
        else
            g_value_copy(val, copy);

        if (!exists)
            g_hash_table_insert(dest->values, GUINT_TO_POINTER(quark), copy);
        g_signal_emit(dest, container_signals[ITEM_CHANGED], quark, quark);
        pfdata.count++;
    }
    g_slist_free(pfdata.keylist);
    g_string_free(key, TRUE);

    return pfdata.count;
}

static void
hash_find_keys(gpointer hkey,
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
pstring_compare(const void *p, const void *q)
{
    return strcmp(*(gchar**)p, *(gchar**)q);
}

/**
 * gwy_container_dump_to_text:
 * @container: A container.
 *
 * Creates a text representation of a container contents.
 *
 * Only simple data types are supported as serialization of compound objects is
 * not controllable.
 *
 * Returns: A pointer array, each item containing string with one container
 * item representation (name, type, value).  The array is sorted by name.
 **/
gchar**
gwy_container_dump_to_text(GwyContainer *container)
{
    g_return_val_if_fail(GWY_IS_CONTAINER(container), NULL);

    GPtrArray *pa = g_ptr_array_new();
    g_hash_table_foreach(container->values, hash_text_serialize, pa);
    g_ptr_array_sort(pa, pstring_compare);
    g_ptr_array_add(pa, NULL);

    return (gchar**)g_ptr_array_free(pa, FALSE);
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
 * gwy_container_new_from_text:
 * @text: Text containing container contents as dumped by
 *        gwy_container_dump_to_text().
 *
 * Constructs a container from its text dump.
 *
 * Returns: The restored container as a newly created object.
 **/
GwyContainer*
gwy_container_new_from_text(const gchar *text)
{
    GwyContainer *container;
    const gchar *tok, *type;
    gchar *name = NULL;
    gsize len, namelen, typelen;
    GQuark key;

    container = gwy_container_new();
    container->in_construction = TRUE;

    for (tok = text; g_ascii_isspace(*tok); tok++)
        ;
    while ((len = token_length(tok))) {
        /* name */
        if (len == (guint)-1) {
            g_warning("Empty item name.");
            if (!(tok = strchr(tok, '\n')))
                break;

            len = 1;
            goto next;
        }
        namelen = len;
        name = dequote_token(tok, &namelen);
        key = g_quark_from_string(name);

        /* type */
        for (tok = tok + len; g_ascii_isspace(*tok); tok++)
            ;
        if (!(len = token_length(tok)) || len == (guint)-1) {
            g_warning("Empty item name.");
            if (!(tok = strchr(tok, '\n'))) {
                GWY_FREE(name);
                break;
            }

            len = 1;
            goto next;
        }
        type = tok;
        typelen = len;

        /* value */
        for (tok = tok + len; g_ascii_isspace(*tok); tok++)
            ;
        if (!(len = token_length(tok)) || len == (guint)-1) {
            g_warning("Empty item value.");
            if (!(tok = strchr(tok, '\n'))) {
                GWY_FREE(name);
                break;
            }

            len = 1;
            goto next;
        }
        /* boolean */
        if (typelen+1 == sizeof("boolean")
            && g_str_has_prefix(type, "boolean")) {
            if (len == 4 && g_str_has_prefix(tok, "True"))
                gwy_container_set_boolean(container, key, TRUE);
            else if (len == 5 && g_str_has_prefix(tok, "False"))
                gwy_container_set_boolean(container, key, FALSE);
            else {
                g_warning("Cannot interpret ‘%s’ as a boolean value.", tok);
            }
        }
        /* char */
        else if (typelen+1 == sizeof("char")
                 && g_str_has_prefix(type, "char")) {
            guint c;

            if (len == 1)
                c = *tok;
            else {
                if (len != 4) {
                    g_warning("Cannot interpret ‘%s’ as a char value.", tok);
                    goto next;
                }
                sscanf(tok+2, "%x", &c);
            }
            gwy_container_set_char(container, key, (gchar)c);
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
            gwy_container_take_string(container, key, s);
        }
        /* UFO */
        else {
            g_warning("Unknown item type ‘%s’", type);
        }

next:
        GWY_FREE(name);

        /* skip space */
        for (tok = tok + len; g_ascii_isspace(*tok); tok++)
            ;
    }
    container->in_construction = FALSE;

    return container;
}

#define __LIBGWY_CONTAINER_C__
#include "libgwy/libgwy-aliases.c"

/**
 * SECTION: container
 * @title: GwyContainer
 * @short_description: Data container with items identified by strings or quarks
 * @see_also: #GHashTable, #GwyInventory
 *
 * #GwyContainer is a general-purpose container, it can hold atomic types,
 * strings and objects. However, objects must implement the #GwySerializable
 * interface, because the container itself is serializable.
 *
 * A new container can be created with gwy_container_new(), items can be stored
 * with functions gwy_container_set_int32(), gwy_container_set_double(), etc.
 * read with gwy_container_get_int32(), gwy_container_get_double(), etc. and
 * removed with gwy_container_remove() or gwy_container_remove_prefix(). A
 * presence of a value can be tested with gwy_container_contains(), convenience
 * functions for reading (updating) a value only if it is present such as
 * gwy_container_gis_double(), are available too.
 *
 * When non-atomic items are stored, #GwyContainer can either takes ownership
 * of them or make a copy. For strings, this means strings stored with
 * gwy_container_take_string() must be freeable when the container no longer
 * needs them and nothing else must modify or free them after storing them to
 * the container. Objects are not duplicated whether taken or copied, in this
 * case the difference is only whether the container takes the caller's
 * reference or its adds its own.
 *
 * Items in a #GwyContainer can be identified by a #GQuark or the corresponding
 * string.  While #GQuarks are atomic values and allow faster access, they are
 * less convenient for casual usage -- each quark-key function such as
 * gwy_container_set_double() thus has a string-key counterpart
 * <literal>_n</literal> appended to the name: gwy_container_set_double_n().
 *
 * An important difference between #GwyContainer and ordinary #GHashTable is
 * that the container emits signal #GwyContainer::item-changed whenever an item
 * changes.
 **/

/**
 * GwyContainer:
 *
 * Object representing hash table-like data container.
 *
 * The #GwyContainer struct contains private data only and should be accessed
 * using the functions below.
 **/

/**
 * GwyContainerClass:
 * @g_object_class: Parent class.
 *
 * Class of data containers.
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
 * @container: A data container.
 *
 * This is a convenience wrapper of gwy_serializable_duplicate().
 **/

/**
 * gwy_container_assign:
 * @dest: Destination data container.
 * @src: Source data container.
 *
 * Copies the contents of a data container.
 *
 * This is a convenience wrapper of gwy_serializable_assign().
 *
 * See also gwy_container_transfer() for a more powerful method of copying data
 * between containers.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
