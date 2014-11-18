/*
 *  $Id$
 *  Copyright (C) 2009 David Nečas (Yeti).
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
#include "libgwy/dict.h"
#include "libgwy/serializable-boxed.h"
#include "libgwy/serialize.h"

enum {
    SGNL_ITEM_CHANGED,
    N_SIGNALS
};

typedef struct {
    GwyDict *dict;
    const gchar *prefix;
    gsize prefix_length;
    guint count;
    GSList *keylist;
    gboolean closed_prefix;
    GwyDictForeachFunc func;
    gpointer user_data;
} PrefixData;

struct _GwyDictPrivate {
    GHashTable *values;
    gboolean in_construction : 1;
};

typedef struct _GwyDictPrivate Dict;

static void     gwy_dict_serializable_init(GwySerializableInterface *iface);
static gsize    gwy_dict_n_items          (GwySerializable *serializable);
static gsize    gwy_dict_itemize          (GwySerializable *serializable,
                                           GwySerializableItems *items);
static void     gwy_dict_finalize         (GObject *object);
static void     gwy_dict_dispose          (GObject *object);
static gboolean gwy_dict_construct        (GwySerializable *serializable,
                                           GwySerializableItems *items,
                                           GwyErrorList **error_list);
static GObject* gwy_dict_duplicate_impl   (GwySerializable *object);
static void     gwy_dict_assign_impl      (GwySerializable *destination,
                                           GwySerializable *source);
static void     value_destroy             (gpointer data);
static GValue*  get_value_of_type         (const GwyDict *dict,
                                           GQuark key,
                                           GType type);
static GValue*  pick_value_of_type         (const GwyDict *dict,
                                           GQuark key,
                                           GType type);
static void     hash_itemize              (gpointer hkey,
                                           gpointer hvalue,
                                           gpointer hdata);
static gboolean hash_remove_prefix        (gpointer hkey,
                                           gpointer hvalue,
                                           gpointer hdata);
static void     hash_foreach              (gpointer hkey,
                                           gpointer hvalue,
                                           gpointer hdata);
static void     hash_duplicate            (gpointer hkey,
                                           gpointer hvalue,
                                           gpointer hdata);
static int      pstring_compare           (const void *p,
                                           const void *q);
static guint    token_length              (const gchar *text);
static gchar*   dequote_token             (const gchar *tok,
                                           gsize *len);

static guint signals[N_SIGNALS];

G_DEFINE_TYPE_EXTENDED
    (GwyDict, gwy_dict, G_TYPE_OBJECT, 0,
     GWY_IMPLEMENT_SERIALIZABLE(gwy_dict_serializable_init));

static void
gwy_dict_serializable_init(GwySerializableInterface *iface)
{
    iface->duplicate = gwy_dict_duplicate_impl;
    iface->assign = gwy_dict_assign_impl;
    iface->n_items = gwy_dict_n_items;
    iface->itemize = gwy_dict_itemize;
    iface->construct = gwy_dict_construct;
}

static void
gwy_dict_class_init(GwyDictClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    g_type_class_add_private(klass, sizeof(Dict));

    gobject_class->dispose = gwy_dict_dispose;
    gobject_class->finalize = gwy_dict_finalize;

    /**
     * GwyDict::item-changed:
     * @gwydict: The #GwyDict which received the signal.
     * @arg1: #GQuark key identifying the changed item.
     *
     * The ::item-changed signal is emitted whenever a dictionary item is
     * changed.
     *
     * This signal is detailed and the detail is the string key identifier.
     **/
    signals[SGNL_ITEM_CHANGED]
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
gwy_dict_init(GwyDict *dict)
{
    dict->priv = G_TYPE_INSTANCE_GET_PRIVATE(dict, GWY_TYPE_DICT,
                                                  Dict);
    dict->priv->values = g_hash_table_new_full(NULL, NULL, NULL,
                                                    value_destroy);
}

static void
gwy_dict_finalize(GObject *object)
{
    g_hash_table_destroy(GWY_DICT(object)->priv->values);
    G_OBJECT_CLASS(gwy_dict_parent_class)->finalize(object);
}

static void
gwy_dict_dispose(GObject *object)
{
    GHashTableIter iter;
    g_hash_table_iter_init(&iter, GWY_DICT(object)->priv->values);
    gpointer pvalue;
    while (g_hash_table_iter_next(&iter, NULL, &pvalue)) {
        GValue *value = (GValue*)pvalue;
        if (g_type_is_a(G_VALUE_TYPE(value), G_TYPE_OBJECT))
            g_value_reset(value);
    }
    G_OBJECT_CLASS(gwy_dict_parent_class)->dispose(object);
}

static gsize
gwy_dict_n_items(GwySerializable *serializable)
{
    gsize n_items = 0;
    GHashTableIter iter;
    g_hash_table_iter_init(&iter, GWY_DICT(serializable)->priv->values);
    gpointer pvalue;
    while (g_hash_table_iter_next(&iter, NULL, &pvalue)) {
        GValue *value = (GValue*)pvalue;
        GType type = G_VALUE_TYPE(value);
        n_items++;
        if (g_type_is_a(type, G_TYPE_OBJECT)) {
            GObject *object = g_value_get_object(value);
            n_items += gwy_serializable_n_items(GWY_SERIALIZABLE(object));
        }
        else if (g_type_is_a(type, G_TYPE_BOXED)) {
            n_items += gwy_serializable_boxed_n_items(type);
        }
    }
    return n_items;
}

static gsize
gwy_dict_itemize(GwySerializable *serializable,
                 GwySerializableItems *items)
{
    GwyDict *dict = GWY_DICT(serializable);
    g_hash_table_foreach(dict->priv->values, hash_itemize, items);
    return g_hash_table_size(dict->priv->values);
}

static void
hash_itemize(gpointer hkey, gpointer hvalue, gpointer hdata)
{
    GQuark key = GPOINTER_TO_UINT(hkey);
    GValue *value = (GValue*)hvalue;
    GwySerializableItems *items = (GwySerializableItems*)hdata;
    GwySerializableItem *it;
    GType type = G_VALUE_TYPE(value);

    g_return_if_fail(items->len - items->n);
    it = items->items + items->n;
    it->name = g_quark_to_string(key);
    it->array_size = 0;
    items->n++;

    switch (type) {
        case G_TYPE_BOOLEAN:
        it->ctype = GWY_SERIALIZABLE_BOOLEAN;
        it->value.v_boolean = !!g_value_get_boolean(value);
        return;

        case G_TYPE_CHAR:
        it->ctype = GWY_SERIALIZABLE_INT8;
        it->value.v_int8 = g_value_get_schar(value);
        return;

        case G_TYPE_UCHAR:
        it->ctype = GWY_SERIALIZABLE_INT8;
        it->value.v_uint8 = g_value_get_uchar(value);
        return;

        case G_TYPE_INT:
        it->ctype = GWY_SERIALIZABLE_INT32;
        it->value.v_int32 = g_value_get_int(value);
        return;

        case G_TYPE_UINT:
        it->ctype = GWY_SERIALIZABLE_INT32;
        it->value.v_uint32 = g_value_get_uint(value);
        return;

        case G_TYPE_INT64:
        it->ctype = GWY_SERIALIZABLE_INT64;
        it->value.v_int64 = g_value_get_int64(value);
        return;

        case G_TYPE_UINT64:
        it->ctype = GWY_SERIALIZABLE_INT64;
        it->value.v_uint64 = g_value_get_uint64(value);
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

static gboolean
gwy_dict_construct(GwySerializable *serializable,
                   GwySerializableItems *items,
                   GwyErrorList **error_list)
{
    GwyDict *dict = GWY_DICT(serializable);

    for (gsize i = 0; i < items->n; i++) {
        GwySerializableItem *it = items->items + i;
        GQuark key = g_quark_from_string(it->name);
        switch (it->ctype) {
            case GWY_SERIALIZABLE_BOOLEAN:
            gwy_dict_set_boolean(dict, key, it->value.v_boolean);
            break;

            case GWY_SERIALIZABLE_INT8:
            gwy_dict_set_schar(dict, key, it->value.v_int8);
            break;

            case GWY_SERIALIZABLE_INT32:
            gwy_dict_set_int32(dict, key, it->value.v_int32);
            break;

            case GWY_SERIALIZABLE_INT64:
            gwy_dict_set_int64(dict, key, it->value.v_int64);
            break;

            case GWY_SERIALIZABLE_DOUBLE:
            gwy_dict_set_double(dict, key, it->value.v_double);
            break;

            case GWY_SERIALIZABLE_STRING:
            gwy_dict_take_string(dict, key, it->value.v_string);
            it->value.v_string = NULL;
            break;

            case GWY_SERIALIZABLE_OBJECT:
            if (it->value.v_object) {
                gwy_dict_take_object(dict, key, it->value.v_object);
                it->value.v_object = NULL;
            }
            break;

            case GWY_SERIALIZABLE_BOXED:
            if (it->value.v_boxed) {
                /* array_size means boxed type */
                gwy_dict_set_boxed(dict, key, it->array_size,
                                        it->value.v_object);
                g_boxed_free(it->array_size, it->value.v_object);
                it->value.v_boxed = NULL;
            }
            break;

            default:
            gwy_error_list_add(error_list, GWY_DESERIALIZE_ERROR,
                               GWY_DESERIALIZE_ERROR_INVALID,
                               // TRANSLATORS: Error message.
                               _("GwyDict cannot store data of type "
                                 "0x%02x."), it->ctype);
            break;
        }
    }

    return TRUE;
}

static GObject*
gwy_dict_duplicate_impl(GwySerializable *object)
{
    GwyDict *duplicate = gwy_dict_new();
    duplicate->priv->in_construction = TRUE;
    g_hash_table_foreach(GWY_DICT(object)->priv->values,
                         hash_duplicate, duplicate);
    duplicate->priv->in_construction = FALSE;

    return (GObject*)duplicate;
}

static void
gwy_dict_assign_impl(GwySerializable *destination,
                     GwySerializable *source)
{
    GwyDict *dict = GWY_DICT(destination);
    GwyDict *src = GWY_DICT(source);

    /* FIXME: Does not emit item-changed!? */
    g_hash_table_remove_all(dict->priv->values);
    g_hash_table_foreach(src->priv->values, hash_duplicate, dict);
}

/**
 * gwy_dict_new: (constructor)
 *
 * Creates a new dictionary.
 *
 * Returns: A new dictionary.
 **/
GwyDict*
gwy_dict_new(void)
{
    GwyDict *dict;

    dict = g_object_newv(GWY_TYPE_DICT, 0, NULL);

    return dict;
}

static void
value_destroy(gpointer data)
{
    GValue *val = (GValue*)data;
    g_value_unset(val);
    g_slice_free1(sizeof(GValue), val);
}

/**
 * gwy_dict_size:
 * @dict: A dictionary.
 *
 * Gets the number of items in a dictionary.
 *
 * Returns: The number of items.
 **/
guint
gwy_dict_size(const GwyDict *dict)
{
    g_return_val_if_fail(GWY_IS_DICT(dict), 0);
    return g_hash_table_size(dict->priv->values);
}

/**
 * gwy_dict_item_type:
 * @dict: A dictionary.
 * @key: Quark item key.
 *
 * Gets the type of value in a dictionary identified by quark key.
 *
 * Returns: The value type as #GType; 0 if there is no such value.
 **/
GType
gwy_dict_item_type(const GwyDict *dict, GQuark key)
{
    g_return_val_if_fail(GWY_IS_DICT(dict), 0);
    if (!key)
        return 0;

    GValue *p = (GValue*)g_hash_table_lookup(dict->priv->values,
                                             GUINT_TO_POINTER(key));

    return p ? G_VALUE_TYPE(p) : 0;
}

/**
 * gwy_dict_item_type_n:
 * @dict: A dictionary.
 * @name: String item key.
 *
 * Gets the type of value in a dictionary identified by string key.
 *
 * Returns: The value type as #GType; 0 if there is no such value.
 **/
GType
gwy_dict_item_type_n(const GwyDict *dict, const gchar *name)
{
    return gwy_dict_item_type(dict, g_quark_try_string(name));
}

/**
 * gwy_dict_contains:
 * @dict: A dictionary.
 * @key: Quark item key.
 *
 * Tests whether a dictionary contains the value identified by a quark key.
 *
 * Returns: Whether @dictionary contains something identified by @key.
 **/
gboolean
gwy_dict_contains(const GwyDict *dict, GQuark key)
{
    g_return_val_if_fail(GWY_IS_DICT(dict), 0);
    return key && g_hash_table_lookup(dict->priv->values,
                                      GUINT_TO_POINTER(key)) != NULL;
}

/**
 * gwy_dict_contains_n:
 * @dict: A dictionary.
 * @name: String item key.
 *
 * Tests whether a dictionary contains the value identified by a string key.
 *
 * Returns: Whether @dictionary contains something identified by @key.
 **/
gboolean
gwy_dict_contains_n(const GwyDict *dict, const gchar *name)
{
    return gwy_dict_contains(dict, g_quark_try_string(name));
}

/**
 * gwy_dict_remove:
 * @dict: A dictionary.
 * @key: Quark item key.
 *
 * Removes a value identified by quark key from a dictionary.
 *
 * Returns: %TRUE if there was such a value and was removed.
 **/
gboolean
gwy_dict_remove(GwyDict *dict, GQuark key)
{
    GValue *value;

    g_return_val_if_fail(GWY_IS_DICT(dict), FALSE);
    if (!key)
        return FALSE;

    value = g_hash_table_lookup(dict->priv->values, GUINT_TO_POINTER(key));
    if (!value)
        return FALSE;

    g_hash_table_remove(dict->priv->values, GUINT_TO_POINTER(key));
    g_signal_emit(dict, signals[SGNL_ITEM_CHANGED], key, key);

    return TRUE;
}

/**
 * gwy_dict_remove_n:
 * @dict: A dictionary.
 * @name: String item key.
 *
 * Removes a value identified by string key from a dictionary.
 *
 * Returns: %TRUE if there was such a value and was removed.
 **/
gboolean
gwy_dict_remove_n(GwyDict *dict, const gchar *name)
{
    return gwy_dict_remove(dict, g_quark_try_string(name));
}

/**
 * gwy_dict_remove_prefix:
 * @dict: A dictionary.
 * @prefix: A nul-terminated id prefix.
 *
 * Removes a values whose key start with given prefix from a dictionary.
 *
 * Prefix @prefix can be also %NULL, all values are removed then.
 *
 * Returns: The number of values removed.
 **/
guint
gwy_dict_remove_prefix(GwyDict *dict, const gchar *prefix)
{
    g_return_val_if_fail(GWY_IS_DICT(dict), 0);

    PrefixData pfdata;
    pfdata.dict = dict;
    pfdata.prefix = prefix;
    pfdata.prefix_length = prefix ? strlen(pfdata.prefix) : 0;
    pfdata.count = 0;
    pfdata.keylist = NULL;
    pfdata.closed_prefix = !pfdata.prefix_length
                           || (prefix[pfdata.prefix_length - 1]
                               == GWY_DICT_PATHSEP);
    g_hash_table_foreach_remove(dict->priv->values, hash_remove_prefix,
                                &pfdata);
    pfdata.keylist = g_slist_reverse(pfdata.keylist);
    for (GSList *l = pfdata.keylist; l; l = g_slist_next(l)) {
        g_signal_emit(dict, signals[SGNL_ITEM_CHANGED],
                      GPOINTER_TO_UINT(l->data), GPOINTER_TO_UINT(l->data));
    }
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
                && name[pfdata->prefix_length] != GWY_DICT_PATHSEP)))
        return FALSE;

    pfdata->count++;
    pfdata->keylist = g_slist_prepend(pfdata->keylist,
                                      GUINT_TO_POINTER(hkey));
    return TRUE;
}

/**
 * gwy_dict_foreach:
 * @dict: A dictionary.
 * @prefix: A nul-terminated id prefix.
 * @function: (scope call):
 *            The function called on the items.
 * @user_data: User data passed to @function.
 *
 * Calls a function for each dictionary item whose identifier starts with
 * given prefix.
 *
 * Returns: The number of items @function was called on.
 **/
guint
gwy_dict_foreach(GwyDict *dict,
                 const gchar *prefix,
                 GwyDictForeachFunc function,
                 gpointer user_data)
{
    g_return_val_if_fail(GWY_IS_DICT(dict), 0);
    g_return_val_if_fail(function, 0);

    PrefixData pfdata;
    pfdata.dict = dict;
    pfdata.prefix = prefix;
    pfdata.prefix_length = prefix ? strlen(pfdata.prefix) : 0;
    pfdata.closed_prefix = !pfdata.prefix_length
                           || (prefix[pfdata.prefix_length - 1]
                               == GWY_DICT_PATHSEP);
    pfdata.count = 0;
    pfdata.keylist = NULL;
    pfdata.func = function;
    pfdata.user_data = user_data;
    g_hash_table_foreach(dict->priv->values, hash_foreach, &pfdata);

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
                && name[pfdata->prefix_length] != GWY_DICT_PATHSEP)))
        return;

    pfdata->func(key, value, pfdata->user_data);
    pfdata->count++;
}

/**
 * gwy_dict_keys:
 * @dict: A dictionary.
 *
 * Gets all quark keys of a dictionary.
 *
 * Returns: (array zero-terminated=1):
 *          A newly allocated 0-terminated array with quark keys of all
 *          @dict items, in no particular order.  The number of items can
 *          be obtained with gwy_dict_size().
 **/
GQuark*
gwy_dict_keys(const GwyDict *dict)
{
    g_return_val_if_fail(GWY_IS_DICT(dict), NULL);
    guint i = 0, n = g_hash_table_size(dict->priv->values);
    GHashTableIter iter;
    g_hash_table_iter_init(&iter, dict->priv->values);
    GQuark *quarks = g_new(GQuark, n+1);
    gpointer key;
    while (g_hash_table_iter_next(&iter, &key, NULL))
        quarks[i++] = GPOINTER_TO_UINT(key);
    quarks[i] = 0;
    return quarks;
}

/**
 * gwy_dict_keys_n:
 * @dict: A dictionary.
 *
 * Gets all string keys of a dictionary.
 *
 * Returns: (transfer container) (array zero-terminated=1):
 *          A newly allocated %NULL-terminated array with string keys of all
 *          @dict items, in no particular order.  The number of items can
 *          be obtained with gwy_dict_size().  The array must be freed by
 *          caller, however, the strings are owned by GLib and must not be
 *          freed.
 **/
const gchar**
gwy_dict_keys_n(const GwyDict *dict)
{
    g_return_val_if_fail(GWY_IS_DICT(dict), NULL);
    guint i = 0, n = g_hash_table_size(dict->priv->values);
    GHashTableIter iter;
    g_hash_table_iter_init(&iter, dict->priv->values);
    const gchar **strings = g_new(const gchar*, n+1);
    gpointer key;
    while (g_hash_table_iter_next(&iter, &key, NULL))
        strings[i++] = g_quark_to_string(GPOINTER_TO_UINT(key));
    strings[i] = NULL;
    return strings;
}

/**
 * gwy_dict_rename:
 * @dict: A dictionary.
 * @key: The current key.
 * @newkey: A new key for the value.
 * @force: Whether to replace existing value at @newkey.
 *
 * Makes a value in a dictionary identified by quark key to be identified by
 * another key.
 *
 * When @force is %TRUE existing value at @newkey is removed from @dict.
 * When it's %FALSE, an existing value @newkey inhibits the rename and %FALSE
 * is returned.
 *
 * Returns: Whether the renaming succeeded.
 **/
gboolean
gwy_dict_rename(GwyDict *dict,
                GQuark key,
                GQuark newkey,
                gboolean force)
{
    g_return_val_if_fail(GWY_IS_DICT(dict), FALSE);
    g_return_val_if_fail(key, FALSE);
    if (key == newkey)
        return TRUE;

    GHashTable *values = dict->priv->values;
    GValue *value = g_hash_table_lookup(values, GUINT_TO_POINTER(key));

    if (!value)
        return FALSE;

    if (g_hash_table_lookup(values, GUINT_TO_POINTER(newkey))) {
        if (!force)
            return FALSE;

        g_hash_table_remove(values, GUINT_TO_POINTER(newkey));
    }

    g_hash_table_insert(values, GUINT_TO_POINTER(newkey), value);
    g_hash_table_steal(values, GUINT_TO_POINTER(key));
    g_signal_emit(dict, signals[SGNL_ITEM_CHANGED], key, key);
    g_signal_emit(dict, signals[SGNL_ITEM_CHANGED], newkey, newkey);

    return TRUE;
}

/**
 * gwy_dict_rename_n:
 * @dict: A dictionary.
 * @name: String item key.
 * @newname: New string item key.
 * @force: Whether to delete existing value at @newkey.
 *
 * Makes a value in a dictionary identified by given name be identified by a
 * another name.
 *
 * See gwy_dict_rename() for details.
 *
 * Returns: Whether the renaming succeeded.
 **/
gboolean
gwy_dict_rename_n(GwyDict *dict,
                  const gchar *name, const gchar *newname,
                  gboolean force)
{
    return gwy_dict_rename(dict,
                           g_quark_try_string(name),
                           g_quark_from_string(newname),
                           force);
}

/**
 * get_value_of_type:
 * @dict: A dictionary.
 * @key: Quark item key.
 * @type: Value type to get.  Can be %NULL to not check value type.
 *
 * Low level function to get a value from a dictionary.
 *
 * Causes a warning when no such value exists, or it's of a wrong type.
 * Use pick_value_of_type() to get value that may not exist.
 *
 * Returns: The value identified by @key; %NULL on failure.
 **/
static GValue*
get_value_of_type(const GwyDict *dict,
                  GQuark key,
                  GType type)
{
    GValue *p;

    g_return_val_if_fail(GWY_IS_DICT(dict), NULL);
    g_return_val_if_fail(key, NULL);
    p = (GValue*)g_hash_table_lookup(dict->priv->values,
                                     GUINT_TO_POINTER(key));
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
 * pick_value_of_type:
 * @dict: A dictionary.
 * @key: Quark item key.
 * @type: Value type to get.  Can be %NULL to not check value type.
 *
 * Low level function to get a value from a dictionary.
 *
 * Causes a warning when value is of a wrong type.
 *
 * Returns: The value identified by @key, or %NULL.
 **/
static GValue*
pick_value_of_type(const GwyDict *dict,
                   GQuark key,
                   GType type)
{
    g_return_val_if_fail(GWY_IS_DICT(dict), NULL);
    if (!key)
        return NULL;

    GValue *p = (GValue*)g_hash_table_lookup(dict->priv->values,
                                             GUINT_TO_POINTER(key));
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
 * gwy_dict_get_value:
 * @dict: A dictionary.
 * @key: Quark item key.
 * @value: (inout):
 *         #GValue to update, it can be either zero-initialised or containing
 *         a value. If item does not exist, it is left untouched.
 *
 * Gets a generic value from a dictionary using a quark key.
 *
 * Returns: %TRUE if @value was actually updated, %FALSE when there is no
 *          such data in the dict.
 **/
gboolean
gwy_dict_get_value(const GwyDict *dict,
                   GQuark key,
                   GValue *value)
{
    GValue *p;

    if (!(p = get_value_of_type(dict, key, 0)))
        return FALSE;

    if (G_VALUE_TYPE(value))
        g_value_unset(value);
    g_value_init(value, G_VALUE_TYPE(p));
    g_value_copy(p, value);

    return TRUE;
}

/**
 * gwy_dict_get_value_n:
 * @dict: A dictionary.
 * @name: String item key.
 * @value: #GValue to update, it can be either zero-initialised or containing
 *         a value. If item does not exist, it is left untouched.
 *
 * Gets a generic value from a dictionary using a string key.
 *
 * Returns: %TRUE if @value was actually updated, %FALSE when there is no
 *          such data in the dict.
 **/
gboolean
gwy_dict_get_value_n(const GwyDict *dict,
                     const gchar *name,
                     GValue *value)
{
    return gwy_dict_get_value(dict, g_quark_try_string(name), value);
}

/**
 * gwy_dict_get_boolean:
 * @dict: A dictionary.
 * @key: Quark item key.
 *
 * Gets a boolean from a dictionary using a quark key.
 *
 * Returns: The boolean value.
 **/
gboolean
gwy_dict_get_boolean(const GwyDict *dict, GQuark key)
{
    GValue *p = get_value_of_type(dict, key, G_TYPE_BOOLEAN);
    return G_LIKELY(p) ? !!g_value_get_boolean(p) : FALSE;
}

/**
 * gwy_dict_get_boolean_n:
 * @dict: A dictionary.
 * @name: String item key.
 *
 * Gets a boolean from a dictionary using a string key.
 *
 * Returns: The boolean value.
 **/
gboolean
gwy_dict_get_boolean_n(const GwyDict *dict, const gchar *name)
{
    return gwy_dict_get_boolean(dict, g_quark_try_string(name));
}

/**
 * gwy_dict_pick_boolean:
 * @dict: A dictionary.
 * @key: Quark item key.
 * @value: (inout):
 *         Pointer to the boolean to update.
 *
 * Updates a boolean from a dictionary using a quark key.
 *
 * Returns: %TRUE if @value was actually updated, %FALSE when there is no
 *          such boolean in the dict.
 **/
gboolean
gwy_dict_pick_boolean(const GwyDict *dict,
                      GQuark key,
                      gboolean *value)
{
    GValue *p;

    if ((p = pick_value_of_type(dict, key, G_TYPE_BOOLEAN))) {
        *value = !!g_value_get_boolean(p);
        return TRUE;
    }
    return FALSE;
}

/**
 * gwy_dict_pick_boolean_n:
 * @dict: A dictionary.
 * @name: String item key.
 * @value: (inout):
 *         Pointer to the boolean to update.
 *
 * Updates a boolean from a dictionary using a string key.
 *
 * Returns: %TRUE if @value was actually updated, %FALSE when there is no
 *          such boolean in the dict.
 **/
gboolean
gwy_dict_pick_boolean_n(const GwyDict *dict,
                        const gchar *name,
                        gboolean *value)
{
    return gwy_dict_pick_boolean(dict, g_quark_try_string(name), value);
}

/**
 * gwy_dict_get_schar:
 * @dict: A dictionary.
 * @key: Quark item key.
 *
 * Gets a character from a dictionary using a quark key.
 *
 * Returns: The character as #gchar.
 **/
gint8
gwy_dict_get_schar(const GwyDict *dict, GQuark key)
{
    GValue *p = get_value_of_type(dict, key, G_TYPE_CHAR);
    return G_LIKELY(p) ? g_value_get_schar(p) : 0;
}

/**
 * gwy_dict_get_schar_n:
 * @dict: A dictionary.
 * @name: String item key.
 *
 * Gets a character from a dictionary using a string key.
 *
 * Returns: The character as #gchar.
 **/
gint8
gwy_dict_get_schar_n(const GwyDict *dict, const gchar *name)
{
    return gwy_dict_get_schar(dict, g_quark_try_string(name));
}

/**
 * gwy_dict_pick_schar:
 * @dict: A dictionary.
 * @key: Quark item key.
 * @value: (inout):
 *         Pointer to the character to update.
 *
 * Updates a character from a dictionary using a quark key.
 *
 * Returns: %TRUE if @value was actually updated, %FALSE when there is no
 *          such character in the dict.
 **/
gboolean
gwy_dict_pick_schar(const GwyDict *dict,
                    GQuark key,
                    gint8 *value)
{
    GValue *p;

    if ((p = pick_value_of_type(dict, key, G_TYPE_CHAR))) {
        *value = g_value_get_schar(p);
        return TRUE;
    }
    return FALSE;
}

/**
 * gwy_dict_pick_schar_n:
 * @dict: A dictionary.
 * @name: String item key.
 * @value: (inout):
 *         Pointer to the character to update.
 *
 * Updates a character from a dictionary using a string key.
 *
 * Returns: %TRUE if @value was actually updated, %FALSE when there is no
 *          such character in the dict.
 **/
gboolean
gwy_dict_pick_schar_n(const GwyDict *dict,
                      const gchar *name,
                      gint8 *value)
{
    return gwy_dict_pick_schar(dict, g_quark_try_string(name), value);
}

/**
 * gwy_dict_get_int32:
 * @dict: A dictionary.
 * @key: Quark item key.
 *
 * Gets a 32bit integer from a dictionary using a quark key.
 *
 * Returns: The integer as #gint32.
 **/
gint32
gwy_dict_get_int32(const GwyDict *dict, GQuark key)
{
    GValue *p = get_value_of_type(dict, key, G_TYPE_INT);
    return G_LIKELY(p) ? g_value_get_int(p) : 0;
}

/**
 * gwy_dict_get_int32_n:
 * @dict: A dictionary.
 * @name: String item key.
 *
 * Gets a 32bit integer from a dictionary using a string key.
 *
 * Returns: The integer as #gint32.
 **/
gint32
gwy_dict_get_int32_n(const GwyDict *dict,
                     const gchar *name)
{
    return gwy_dict_get_int32(dict, g_quark_try_string(name));
}

/**
 * gwy_dict_pick_int32:
 * @dict: A dictionary.
 * @key: Quark item key.
 * @value: (inout):
 *         Pointer to the 32bit integer to update.
 *
 * Updates a 32bit integer from a dictionary using a quark key.
 *
 * Returns: %TRUE if @value was actually updated, %FALSE when there is no
 *          such 32bit integer in the dict.
 **/
gboolean
gwy_dict_pick_int32(const GwyDict *dict,
                    GQuark key,
                    gint32 *value)
{
    GValue *p;

    if ((p = pick_value_of_type(dict, key, G_TYPE_INT))) {
        *value = g_value_get_int(p);
        return TRUE;
    }
    return FALSE;
}

/**
 * gwy_dict_pick_int32_n:
 * @dict: A dictionary.
 * @name: String item key.
 * @value: (inout):
 *         Pointer to the 32bit integer to update.
 *
 * Updates a 32bit integer from a dictionary using a string key.
 *
 * Returns: %TRUE if @value was actually updated, %FALSE when there is no
 *          such 32bit integer in the dict.
 **/
gboolean
gwy_dict_pick_int32_n(const GwyDict *dict,
                      const gchar *name,
                      gint32 *value)
{
    return gwy_dict_pick_int32(dict, g_quark_try_string(name), value);
}

/**
 * gwy_dict_get_enum:
 * @dict: A dictionary.
 * @key: Quark item key.
 *
 * Gets an enum value from a dictionary using a quark key.
 *
 * Note enums are treated as 32bit integers.
 *
 * Returns: The enum as #guint.
 **/
guint32
gwy_dict_get_enum(const GwyDict *dict, GQuark key)
{
    return gwy_dict_get_int32(dict, key);
}

/**
 * gwy_dict_get_enum_n:
 * @dict: A dictionary.
 * @name: String item key.
 *
 * Gets an enum value from a dictionary using a string key.
 *
 * Note enums are treated as 32bit integers.
 *
 * Returns: The enum as #guint.
 **/
guint32
gwy_dict_get_enum_n(const GwyDict *dict, const gchar *name)
{
    return gwy_dict_get_int32(dict, g_quark_try_string(name));
}

/**
 * gwy_dict_pick_enum:
 * @dict: A dictionary.
 * @key: Quark item key.
 * @value: (inout):
 *         Pointer to the enum to update.
 *
 * Updates an enum from a dictionary using a quark key.
 *
 * Note enums are treated as 32bit integers.
 *
 * Returns: %TRUE if @value was actually updated, %FALSE when there is no
 *          such enum in the dict.
 **/
gboolean
gwy_dict_pick_enum(const GwyDict *dict,
                   GQuark key,
                   guint32 *value)
{
    gint32 value32;

    if (gwy_dict_pick_int32(dict, key, &value32)) {
        *value = value32;
        return TRUE;
    }
    return FALSE;
}

/**
 * gwy_dict_pick_enum_n:
 * @dict: A dictionary.
 * @name: String item key.
 * @value: (inout):
 *         Pointer to the enum to update.
 *
 * Updates an enum from a dictionary using a string key.
 *
 * Note enums are treated as 32bit integers.
 *
 * Returns: %TRUE if @value was actually updated, %FALSE when there is no
 *          such enum in the dict.
 **/
gboolean
gwy_dict_pick_enum_n(const GwyDict *dict,
                     const gchar *name,
                     guint32 *value)
{
    return gwy_dict_pick_enum(dict, g_quark_try_string(name), value);
}

/**
 * gwy_dict_get_int64:
 * @dict: A dictionary.
 * @key: Quark item key.
 *
 * Gets a 64bit integer from a dictionary using a quark key.
 *
 * Returns: The integer as #gint64.
 **/
gint64
gwy_dict_get_int64(const GwyDict *dict, GQuark key)
{
    GValue *p = get_value_of_type(dict, key, G_TYPE_INT64);
    return G_LIKELY(p) ? g_value_get_int64(p) : 0;
}

/**
 * gwy_dict_get_int64_n:
 * @dict: A dictionary.
 * @name: String item key.
 *
 * Gets a 64bit integer from a dictionary using a string key.
 *
 * Returns: The integer as #gint64.
 **/
gint64
gwy_dict_get_int64_n(const GwyDict *dict, const gchar *name)
{
    return gwy_dict_get_int64(dict, g_quark_try_string(name));
}

/**
 * gwy_dict_pick_int64:
 * @dict: A dictionary.
 * @key: Quark item key.
 * @value: (inout):
 *         Pointer to the 64bit integer to update.
 *
 * Updates a 64bit integer from a dictionary using a quark key.
 *
 * Returns: %TRUE if @value was actually updated, %FALSE when there is no
 *          such 64bit integer in the dict.
 **/
gboolean
gwy_dict_pick_int64(const GwyDict *dict,
                    GQuark key,
                    gint64 *value)
{
    GValue *p;

    if ((p = pick_value_of_type(dict, key, G_TYPE_INT64))) {
        *value = g_value_get_int64(p);
        return TRUE;
    }
    return FALSE;
}

/**
 * gwy_dict_pick_int64_n:
 * @dict: A dictionary.
 * @name: String item key.
 * @value: (inout):
 *         Pointer to the 64bit integer to update.
 *
 * Updates a 64bit integer from a dictionary using a string key.
 *
 * Returns: %TRUE if @value was actually updated, %FALSE when there is no
 *          such 64bit integer in the dict.
 **/
gboolean
gwy_dict_pick_int64_n(const GwyDict *dict,
                      const gchar *name,
                      gint64 *value)
{
    return gwy_dict_pick_int64(dict, g_quark_try_string(name), value);
}

/**
 * gwy_dict_get_double:
 * @dict: A dictionary.
 * @key: Quark item key.
 *
 * Gets a double from a dictionary using a quark key.
 *
 * Returns: The double as #gdouble.
 **/
gdouble
gwy_dict_get_double(const GwyDict *dict, GQuark key)
{
    GValue *p = get_value_of_type(dict, key, G_TYPE_DOUBLE);
    return G_LIKELY(p) ? g_value_get_double(p) : 0.0;
}

/**
 * gwy_dict_get_double_n:
 * @dict: A dictionary.
 * @name: String item key.
 *
 * Gets a double from a dictionary using a string key.
 *
 * Returns: The double as #gdouble.
 **/
gdouble
gwy_dict_get_double_n(const GwyDict *dict, const gchar *name)
{
    return gwy_dict_get_double(dict, g_quark_try_string(name));
}

/**
 * gwy_dict_pick_double:
 * @dict: A dictionary.
 * @key: Quark item key.
 * @value: (inout):
 *         Pointer to the double to update.
 *
 * Updates a double from a dictionary using a quark key.
 *
 * Returns: %TRUE if @value was actually updated, %FALSE when there is no
 *          such double in the dict.
 **/
gboolean
gwy_dict_pick_double(const GwyDict *dict,
                     GQuark key,
                     gdouble *value)
{
    GValue *p;

    if ((p = pick_value_of_type(dict, key, G_TYPE_DOUBLE))) {
        *value = g_value_get_double(p);
        return TRUE;
    }
    return FALSE;
}

/**
 * gwy_dict_pick_double_n:
 * @dict: A dictionary.
 * @name: String item key.
 * @value: (inout):
 *         Pointer to the double to update.
 *
 * Updates a double from a dictionary using a string key.
 *
 * Returns: %TRUE if @value was actually updated, %FALSE when there is no
 *          such double in the dict.
 **/
gboolean
gwy_dict_pick_double_n(const GwyDict *dict,
                       const gchar *name,
                       gdouble *value)
{
    return gwy_dict_pick_double(dict, g_quark_try_string(name), value);
}

/**
 * gwy_dict_get_string:
 * @dict: A dictionary.
 * @key: Quark item key.
 *
 * Gets a string from a dictionary using a quark key.
 *
 * The returned string must be treated as constant and never freed or modified.
 *
 * Returns: The string.
 **/
const gchar*
gwy_dict_get_string(const GwyDict *dict, GQuark key)
{
    GValue *p = get_value_of_type(dict, key, G_TYPE_STRING);
    return G_LIKELY(p) ? g_value_get_string(p) : NULL;
}

/**
 * gwy_dict_get_string_n:
 * @dict: A dictionary.
 * @name: String item key.
 *
 * Gets a string from a dictionary using a string key.
 *
 * The returned string must be treated as constant and never freed or modified.
 *
 * Returns: The string.
 **/
const gchar*
gwy_dict_get_string_n(const GwyDict *dict, const gchar *name)
{
    return gwy_dict_get_string(dict, g_quark_try_string(name));
}

/**
 * gwy_dict_pick_string:
 * @dict: A dictionary.
 * @key: Quark item key.
 * @value: (inout):
 *         Pointer to the string pointer to update.
 *
 * Updates a string from a dictionary using a quark key.
 *
 * The string stored in @value if this function succeeds must be treated as
 * constant and never freed or modified.
 *
 * Returns: %TRUE if @value was actually updated, %FALSE when there is no
 *          such string in the dict.
 **/
gboolean
gwy_dict_pick_string(const GwyDict *dict,
                     GQuark key,
                     const gchar **value)
{
    GValue *p;

    if ((p = pick_value_of_type(dict, key, G_TYPE_STRING))) {
        *value = g_value_get_string(p);
        return TRUE;
    }
    return FALSE;
}

/**
 * gwy_dict_pick_string_n:
 * @dict: A dictionary.
 * @name: String item key.
 * @value: (inout):
 *         Pointer to the string pointer to update.
 *
 * Updates a string from a dictionary using a string key.
 *
 * The string stored in @v if this function succeeds must be treated as
 * constant and never freed or modified.
 *
 * Returns: %TRUE if @value was actually updated, %FALSE when there is no
 *          such string in the dict.
 **/
gboolean
gwy_dict_pick_string_n(const GwyDict *dict,
                       const gchar *name,
                       const gchar **value)
{
    return gwy_dict_pick_string(dict, g_quark_try_string(name), value);
}

/**
 * gwy_dict_get_object:
 * @dict: A dictionary.
 * @key: Quark item key.
 *
 * Gets an object from a dictionary using a quark key.
 *
 * The returned object does not have its reference count increased, use
 * g_object_ref() if you want to access it even when @dict may cease
 * to exist.
 *
 * Returns: (transfer none):
 *          The object as #gpointer.
 **/
gpointer
gwy_dict_get_object(const GwyDict *dict, GQuark key)
{
    GValue *p = get_value_of_type(dict, key, G_TYPE_OBJECT);
    return G_LIKELY(p) ? (gpointer)g_value_get_object(p) : NULL;
}

/**
 * gwy_dict_get_object_n:
 * @dict: A dictionary.
 * @name: String item key.
 *
 * Gets an object from a dictionary using a string key.
 *
 * The returned object does not have its reference count increased, use
 * g_object_ref() if you want to access it even when @dict may cease
 * to exist.
 *
 * Returns: (transfer none):
 *          The object as #gpointer.
 **/
gpointer
gwy_dict_get_object_n(const GwyDict *dict, const gchar *name)
{
    return gwy_dict_get_object(dict, g_quark_try_string(name));
}

/**
 * gwy_dict_pick_object:
 * @dict: A dictionary.
 * @key: Quark item key.
 * @value: Pointer to the object pointer to update.
 *
 * Updates an object from a dictionary using a quark key.
 *
 * The object stored in @value if this function succeeds does not have its
 * reference count increased, use g_object_ref() if you want to access it even
 * when @dict may cease to exist.
 * |[
 * GwyUnit *unit = NULL;
 * if (gwy_dict_pick_object(dict, key, &unit)) {
 *     // Add our own reference.
 *     g_object_ref(unit);
 *     ...
 * }
 * ]|
 *
 * Returns: %TRUE if @value was actually updated, %FALSE when there is no
 *          such object in the dict.
 **/
gboolean
gwy_dict_pick_object(const GwyDict *dict,
                     GQuark key,
                     gpointer value)
{
    GValue *p;

    if ((p = pick_value_of_type(dict, key, G_TYPE_OBJECT))) {
        *(GObject**)value = g_value_get_object(p);
        return TRUE;
    }
    return FALSE;
}

/**
 * gwy_dict_pick_object_n:
 * @dict: A dictionary.
 * @name: String item key.
 * @value: Pointer to the object pointer to update.
 *
 * Updates an object from a dictionary using a string key.
 *
 * The object stored in @v if this function succeeds does not have its
 * reference count increased, use g_object_ref() if you want to access it even
 * when @dict may cease to exist.
 *
 * Returns: %TRUE if @value was actually updated, %FALSE when there is no
 *          such object in the dict.
 **/
gboolean
gwy_dict_pick_object_n(const GwyDict *dict,
                       const gchar *name,
                       gpointer value)
{
    return gwy_dict_pick_object(dict, g_quark_try_string(name), value);
}

/**
 * gwy_dict_get_boxed:
 * @dict: A dictionary.
 * @key: Quark item key.
 * @type: Serializable boxed type.  You can pass the concrete type
 *        (recommended) or %G_TYPE_BOXED to get any boxed type (if you know
 *        what you are doing).
 *
 * Gets a boxed type from a dictionary using a quark key.
 *
 * The returned boxed type is owned by @dict.  Use g_boxed_copy() to
 * create a private copy if you need a modifiable/long-lasting data.
 * |[
 * const GwyRGBA *color = gwy_dict_get_boxed(dict, key, GWY_TYPE_RGBA);
 * ]|
 *
 * Returns: (transfer none):
 *          Boxed type pointer.
 **/
gconstpointer
gwy_dict_get_boxed(const GwyDict *dict, GQuark key, GType type)
{
    GValue *p = get_value_of_type(dict, key, type);
    return G_LIKELY(p) ? (gpointer)g_value_get_boxed(p) : NULL;
}

/**
 * gwy_dict_get_boxed_n:
 * @dict: A dictionary.
 * @name: String item key.
 * @type: Serializable boxed type.  You can pass the concrete type
 *        (recommended) or %G_TYPE_BOXED to get any boxed type (if you know
 *        what you are doing).
 *
 * Gets a boxed type from a dictionary using a string key.
 *
 * The returned boxed type is owned by @dict.  Use g_boxed_copy() to
 * create a private copy if you need a modifiable/long-lasting data.
 * |[
 * const GwyRGBA *color = gwy_dict_get_boxed_n(dict, "name", GWY_TYPE_RGBA);
 * ]|
 *
 * Returns: (transfer none):
 *          Boxed type pointer.
 **/
gconstpointer
gwy_dict_get_boxed_n(const GwyDict *dict, const gchar *name, GType type)
{
    return gwy_dict_get_boxed(dict, g_quark_try_string(name), type);
}

/**
 * gwy_dict_pick_boxed:
 * @dict: A dictionary.
 * @key: Quark item key.
 * @type: Serializable boxed type.  You can pass the concrete type
 *        (recommended) or %G_TYPE_BOXED to get any boxed type (if you know
 *        what you are doing).
 * @value: Pointer to the boxed type to update.
 *
 * Updates a boxed type from a dictionary using a quark key.
 *
 * The value is transferred using gwy_serializable_boxed_assign(), i.e. it is
 * assigned by value.
 * |[
 * GwyRGBA color = { 0, 0, 0, 0 };
 * if (gwy_dict_pick_boxed(dict, key, GWY_TYPE_RGBA, &color)) {
 *     ...
 * }
 * ]|
 *
 * Returns: %TRUE if @value was actually updated, %FALSE when there is no
 *          such boxed in the dict.
 **/
gboolean
gwy_dict_pick_boxed(const GwyDict *dict,
                    GQuark key,
                    GType type,
                    gpointer value)
{
    GValue *p;

    if ((p = pick_value_of_type(dict, key, type))) {
        gwy_serializable_boxed_assign(G_VALUE_TYPE(p),
                                      value, g_value_get_boxed(p));
        return TRUE;
    }
    return FALSE;
}

/**
 * gwy_dict_pick_boxed_n:
 * @dict: A dictionary.
 * @name: String item key.
 * @type: Serializable boxed type.  You can pass the concrete type
 *        (recommended) or %G_TYPE_BOXED to get any boxed type (if you know
 *        what you are doing).
 * @value: Pointer to the boxed type to update.
 *
 * Updates a boxed type from a dictionary using a string key.
 *
 * The value is transferred using gwy_serializable_boxed_assign(), i.e. it is
 * assigned by value.
 *
 * Returns: %TRUE if @value was actually updated, %FALSE when there is no
 *          such boxed in the dict.
 **/
gboolean
gwy_dict_pick_boxed_n(const GwyDict *dict,
                      const gchar *name,
                      GType type,
                      gpointer value)
{
    return gwy_dict_pick_boxed(dict, g_quark_try_string(name), type, value);
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
        return g_value_get_schar(value1) == g_value_get_schar(value2);

        case G_TYPE_UCHAR:
        return g_value_get_uchar(value1) == g_value_get_uchar(value2);

        case G_TYPE_INT:
        return g_value_get_int(value1) == g_value_get_int(value2);

        case G_TYPE_UINT:
        return g_value_get_uint(value1) == g_value_get_uint(value2);

        case G_TYPE_INT64:
        return g_value_get_int64(value1) == g_value_get_int64(value2);

        case G_TYPE_UINT64:
        return g_value_get_uint64(value1) == g_value_get_uint64(value2);

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
        return gwy_serializable_boxed_equal(type,
                                            g_value_get_boxed(value1),
                                            g_value_get_boxed(value2));
    }

    g_return_val_if_reached(FALSE);
}

/**
 * gwy_dict_set_value:
 * @dict: A dictionary.
 * @key: Quark item key.
 * @value: #GValue with the value to set.
 *
 * Inserts or updates a value identified by quark key in a dictionary.
 *
 * The value is copied by the dict.
 **/
void
gwy_dict_set_value(GwyDict *dict, GQuark key, const GValue *value)
{
    g_return_if_fail(GWY_IS_DICT(dict));
    g_return_if_fail(key);
    g_return_if_fail(value);

    GHashTable *values = dict->priv->values;
    GValue *gvalue = g_hash_table_lookup(values, GUINT_TO_POINTER(key));
    GType newtype = G_VALUE_TYPE(value);
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
            g_hash_table_insert(values, GUINT_TO_POINTER(key), newvalue);
            // g_value_unset(gvalue); done by hash value destroy function
        }
    }
    else {
        gvalue = g_slice_new0(GValue);
        g_value_init(gvalue, newtype);
        g_value_copy(value, gvalue);
        g_hash_table_insert(values, GUINT_TO_POINTER(key), gvalue);
    }
    if (!dict->priv->in_construction)
        g_signal_emit(dict, signals[SGNL_ITEM_CHANGED], key, key);
}

/**
 * gwy_dict_set_value_n:
 * @dict: A dictionary.
 * @name: String item key.
 * @value: #GValue with the value to set.
 *
 * Inserts or updates a value identified by string key in a dictionary.
 *
 * The value is copied by the dict.
 **/
void
gwy_dict_set_value_n(GwyDict *dict, const gchar *name, const GValue *value)
{
    gwy_dict_set_value(dict, g_quark_from_string(name), value);
}

#define dict_set_template(dict,key,value,n,N) \
    g_return_if_fail(GWY_IS_DICT(dict)); \
    g_return_if_fail(key); \
    GHashTable *values = dict->priv->values; \
    GValue *gvalue = g_hash_table_lookup(values, GUINT_TO_POINTER(key)); \
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
        g_hash_table_insert(values, GUINT_TO_POINTER(key), gvalue); \
    } \
    g_value_set_##n(gvalue, value); \
    if (!dict->priv->in_construction) \
        g_signal_emit(dict, signals[SGNL_ITEM_CHANGED], key, key)

/**
 * gwy_dict_set_boolean:
 * @dict: A dictionary.
 * @key: Quark item key.
 * @value: A boolean.
 *
 * Stores a boolean identified by a quark into a dictionary.
 **/
void
gwy_dict_set_boolean(GwyDict *dict,
                     GQuark key,
                     gboolean value)
{
    value = !!value;
    dict_set_template(dict, key, value, boolean, BOOLEAN);
}

/**
 * gwy_dict_set_boolean_n:
 * @dict: A dictionary.
 * @name: String item key.
 * @value: A boolean.
 *
 * Stores a boolean identified by a string into a dictionary.
 **/
void
gwy_dict_set_boolean_n(GwyDict *dict, const gchar *name, gboolean value)
{
    gwy_dict_set_boolean(dict, g_quark_from_string(name), value);
}

/**
 * gwy_dict_set_schar:
 * @dict: A dictionary.
 * @key: Quark item key.
 * @value: A character.
 *
 * Stores a character identified by a quark into a dictionary.
 **/
void
gwy_dict_set_schar(GwyDict *dict, GQuark key, gint8 value)
{
    dict_set_template(dict, key, value, schar, CHAR);
}

/**
 * gwy_dict_set_schar_n:
 * @dict: A dictionary.
 * @name: String item key.
 * @value: A character.
 *
 * Stores a character identified by a string into a dictionary.
 **/
void
gwy_dict_set_schar_n(GwyDict *dict, const gchar *name, gint8 value)
{
    gwy_dict_set_schar(dict, g_quark_from_string(name), value);
}

/**
 * gwy_dict_set_int32:
 * @dict: A dictionary.
 * @key: Quark item key.
 * @value: A 32bit integer.
 *
 * Stores a 32bit integer identified by a quark into a dictionary.
 **/
void
gwy_dict_set_int32(GwyDict *dict, GQuark key, gint32 value)
{
    dict_set_template(dict, key, value, int, INT);
}

/**
 * gwy_dict_set_int32_n:
 * @dict: A dictionary.
 * @name: String item key.
 * @value: A 32bit integer.
 *
 * Stores a 32bit integer identified by a string into a dictionary.
 **/
void
gwy_dict_set_int32_n(GwyDict *dict, const gchar *name, gint32 value)
{
    gwy_dict_set_int32(dict, g_quark_from_string(name), value);
}

/**
 * gwy_dict_set_enum:
 * @dict: A dictionary.
 * @key: Quark item key.
 * @value: An enum integer.
 *
 * Stores an enum value identified by a quark into a dictionary.
 *
 * Note enums are treated as 32bit integers.
 **/
void
gwy_dict_set_enum(GwyDict *dict, GQuark key, guint32 value)
{
    gint32 value32 = value;
    dict_set_template(dict, key, value32, int, INT);
}

/**
 * gwy_dict_set_enum_n:
 * @dict: A dictionary.
 * @name: String item key.
 * @value: An enum.
 *
 * Stores an enum value identified by a string into a dictionary.
 *
 * Note enums are treated as 32bit integers.
 **/
void
gwy_dict_set_enum_n(GwyDict *dict, const gchar *name, guint32 value)
{
    gwy_dict_set_enum(dict, g_quark_from_string(name), value);
}

/**
 * gwy_dict_set_int64:
 * @dict: A dictionary.
 * @key: Quark item key.
 * @value: A 64bit integer.
 *
 * Stores a 64bit integer identified by a quark into a dictionary.
 **/
void
gwy_dict_set_int64(GwyDict *dict, GQuark key, gint64 value)
{
    dict_set_template(dict, key, value, int64, INT64);
}

/**
 * gwy_dict_set_int64_n:
 * @dict: A dictionary.
 * @name: String item key.
 * @value: A 64bit integer.
 *
 * Stores a 64bit integer identified by a string into a dictionary.
 **/
void
gwy_dict_set_int64_n(GwyDict *dict, const gchar *name, gint64 value)
{
    gwy_dict_set_int64(dict, g_quark_from_string(name), value);
}

/**
 * gwy_dict_set_double:
 * @dict: A dictionary.
 * @key: Quark item key.
 * @value: A double.
 *
 * Stores a double identified by a quark into a dictionary.
 **/
void
gwy_dict_set_double(GwyDict *dict, GQuark key, gdouble value)
{
    dict_set_template(dict, key, value, double, DOUBLE);
}

/**
 * gwy_dict_set_double_n:
 * @dict: A dictionary.
 * @name: String item key.
 * @value: A double integer.
 *
 * Stores a double identified by a string into a dictionary.
 **/
void
gwy_dict_set_double_n(GwyDict *dict, const gchar *name, gdouble value)
{
    gwy_dict_set_double(dict, g_quark_from_string(name), value);
}

/**
 * gwy_dict_set_string:
 * @dict: A dictionary.
 * @key: Quark item key.
 * @value: A nul-terminated string.
 *
 * Copies a string identified by a quark into a dictionary.
 *
 * The dictionary makes a copy of the string so, this method can be used on
 * static strings and strings that may be modified or freed.
 **/
void
gwy_dict_set_string(GwyDict *dict, GQuark key, const gchar *value)
{
    g_return_if_fail(GWY_IS_DICT(dict));
    g_return_if_fail(key);
    GHashTable *values = dict->priv->values;
    GValue *gvalue = g_hash_table_lookup(values, GUINT_TO_POINTER(key));
    if (gvalue) {
        GType type = G_VALUE_TYPE(gvalue);
        if (type == G_TYPE_STRING) {
            const gchar *old = g_value_get_string(gvalue);
            if (old && gwy_strequal(old, value))
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
        g_hash_table_insert(values, GUINT_TO_POINTER(key), gvalue);
    }
    g_value_set_string(gvalue, value);
    if (!dict->priv->in_construction)
        g_signal_emit(dict, signals[SGNL_ITEM_CHANGED], key, key);
}

/**
 * gwy_dict_set_string_n:
 * @dict: A dictionary.
 * @name: String item key.
 * @value: A nul-terminated string.
 *
 * Copies a string identified by a string into a dictionary.
 *
 * The dictionary makes a copy of the string so, this method can be used on
 * static strings and strings that may be modified or freed.
 **/
void
gwy_dict_set_string_n(GwyDict *dict, const gchar *name, const gchar *value)
{
    gwy_dict_set_string(dict, g_quark_from_string(name), value);
}

/**
 * gwy_dict_take_string:
 * @dict: A dictionary.
 * @key: Quark item key.
 * @value: (out) (transfer full):
 *         A nul-terminated string.
 *
 * Stores a string identified by a quark into a dictionary.
 *
 * The dictionary takes ownership of the string so, this method cannot be used
 * with static strings, use g_strdup() to duplicate them first.  The string
 * becomes fully owned by @dict and you must not touch it any more.
 * In fact, it may be already freed when this function returns.
 **/
void
gwy_dict_take_string(GwyDict *dict, GQuark key, gchar *value)
{
    g_return_if_fail(GWY_IS_DICT(dict));
    g_return_if_fail(key);
    GHashTable *values = dict->priv->values;
    GValue *gvalue = g_hash_table_lookup(values, GUINT_TO_POINTER(key));
    if (gvalue) {
        GType type = G_VALUE_TYPE(gvalue);
        if (type == G_TYPE_STRING) {
            const gchar *old = g_value_get_string(gvalue);
            if (old && gwy_strequal(old, value)) {
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
        g_hash_table_insert(values, GUINT_TO_POINTER(key), gvalue);
    }
    g_value_take_string(gvalue, value);
    if (!dict->priv->in_construction)
        g_signal_emit(dict, signals[SGNL_ITEM_CHANGED], key, key);
}

/**
 * gwy_dict_take_string_n:
 * @dict: A dictionary.
 * @name: String item key.
 * @value: A nul-terminated string.
 *
 * Stores a string identified by a string into a dictionary.
 *
 * The dictionary takes ownership of the string so, this method cannot be used
 * with static strings, use g_strdup() to duplicate them first.  The string
 * becomes fully owned by @dict and you must not touch it any more.
 * In fact, it may be already freed when this function returns.
 **/
void
gwy_dict_take_string_n(GwyDict *dict, const gchar *name, gchar *value)
{
    gwy_dict_take_string(dict, g_quark_from_string(name), value);
}

/**
 * gwy_dict_set_object:
 * @dict: A dictionary.
 * @key: Quark item key.
 * @value: (out) (type GObject.Object) (transfer none):
 *         Object to store into the dict.
 *
 * Stores an object identified by a quark into a dictionary.
 *
 * The dictionary adds its own reference on the object.
 *
 * The object must implement #GwySerializable interface to allow serialisation
 * and other operations with the dict.
 * |[
 * GwyUnit *unit = gwy_si_unit_new_from_string("m");
 * gwy_dict_set_object(dict, key, unit);
 * // Release our own reference.
 * g_object_unref(unit);
 * ]|
 **/
void
gwy_dict_set_object(GwyDict *dict, GQuark key, gpointer value)
{
    g_return_if_fail(GWY_IS_DICT(dict));
    g_return_if_fail(key);
    GHashTable *values = dict->priv->values;
    GValue *gvalue = g_hash_table_lookup(values, GUINT_TO_POINTER(key));
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
        g_hash_table_insert(values, GUINT_TO_POINTER(key), gvalue);
    }
    g_value_set_object(gvalue, value);
    g_object_unref(value);
    if (!dict->priv->in_construction)
        g_signal_emit(dict, signals[SGNL_ITEM_CHANGED], key, key);
}

/**
 * gwy_dict_set_object_n:
 * @dict: A dictionary.
 * @name: String item key.
 * @value: An object to store into dict.
 *
 * Stores an object identified by a string into a dictionary.
 *
 * See gwy_dict_set_object() for details.
 **/
void
gwy_dict_set_object_n(GwyDict *dict, const gchar *name, gpointer value)
{
    gwy_dict_set_object(dict, g_quark_from_string(name), value);
}

/**
 * gwy_dict_take_object:
 * @dict: A dictionary.
 * @key: Quark item key.
 * @value: (out) (type GObject.Object) (transfer full):
 *         Object to store into the dict.
 *
 * Stores an object identified by a quark into a dictionary.
 *
 * The dictionary takes the ownership on the object from the caler, i.e. its
 * reference count is not incremented.
 *
 * The object must implement #GwySerializable interface to allow serialisation
 * and other operations with the dict.
 * |[
 * GwyUnit *unit = gwy_si_unit_new_from_string("m");
 * // Pass our reference to dict.
 * gwy_dict_take_object(dict, key, unit);
 * ]|
 **/
void
gwy_dict_take_object(GwyDict *dict,
                     GQuark key,
                     gpointer value)
{
    g_return_if_fail(GWY_IS_DICT(dict));
    g_return_if_fail(key);
    GHashTable *values = dict->priv->values;
    GValue *gvalue = g_hash_table_lookup(values, GUINT_TO_POINTER(key));
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
        g_hash_table_insert(values, GUINT_TO_POINTER(key), gvalue);
    }
    g_value_take_object(gvalue, value);
    if (!dict->priv->in_construction)
        g_signal_emit(dict, signals[SGNL_ITEM_CHANGED], key, key);
}

/**
 * gwy_dict_take_object_n:
 * @dict: A dictionary.
 * @name: String item key.
 * @value: An object to store into dict.
 *
 * Stores an object identified by a string into a dictionary.
 *
 * See gwy_dict_take_object() for details.
 **/
void
gwy_dict_take_object_n(GwyDict *dict, const gchar *name, gpointer value)
{
    gwy_dict_take_object(dict, g_quark_from_string(name), value);
}

/**
 * gwy_dict_set_boxed:
 * @dict: A dictionary.
 * @key: Quark item key.
 * @type: Serializable boxed type.
 * @value: Pointer to boxed struct of type @type to store into dict.
 *
 * Stores a boxed type identified by a quark into a dictionary.
 *
 * The dictionary stores the boxed type by value, i.e. a copy is made.
 *
 * The boxed type must implement the
 * <link linkend="libgwy4-serializable-boxed">serializable boxed</link>.
 * to allow serialisation and other operations with the dict.
 * |[
 * GwyRGBA color = { 0, 0, 0, 0 };
 * gwy_dict_set_boxed(dict, key, GWY_TYPE_RGBA, &color);
 * ]|
 **/
void
gwy_dict_set_boxed(GwyDict *dict,
                   GQuark key,
                   GType boxtype,
                   gpointer value)
{
    g_return_if_fail(GWY_IS_DICT(dict));
    g_return_if_fail(key);
    GHashTable *values = dict->priv->values;
    GValue *gvalue = g_hash_table_lookup(values, GUINT_TO_POINTER(key));
    if (gvalue) {
        GType type = G_VALUE_TYPE(gvalue);
        if (type == boxtype) {
            gpointer boxed = g_value_get_boxed(gvalue);
            if (gwy_serializable_boxed_equal(type, boxed, value))
                return;
            gwy_serializable_boxed_assign(type, boxed, value);
        }
        else {
            // Be careful not to free something before using it.
            GValue *newvalue = g_slice_new0(GValue);
            g_value_init(newvalue, boxtype);
            g_value_copy(value, newvalue);
            g_hash_table_insert(values, GUINT_TO_POINTER(key), newvalue);
            // g_value_unset(gvalue); done by hash value destroy function
        }
    }
    else {
        gvalue = g_slice_new0(GValue);
        g_value_init(gvalue, boxtype);
        g_value_set_boxed(gvalue, value);
        g_hash_table_insert(values, GUINT_TO_POINTER(key), gvalue);
    }
    if (!dict->priv->in_construction)
        g_signal_emit(dict, signals[SGNL_ITEM_CHANGED], key, key);
}

/**
 * gwy_dict_set_boxed_n:
 * @dict: A dictionary.
 * @name: String item key.
 * @type: Serializable boxed type.
 * @value: Pointer to boxed struct of type @t to store into dict.
 *
 * Stores a boxed type identified by a string into a dictionary.
 *
 * See gwy_dict_set_boxed() for details.
 **/
void
gwy_dict_set_boxed_n(GwyDict *dict,
                     const gchar *name,
                     GType type,
                     gpointer value)
{
    gwy_dict_set_boxed(dict, g_quark_from_string(name), type, value);
}

static void
set_copied_value(GwyDict *dict,
                 GQuark key,
                 const GValue *value)
{
    GType type = G_VALUE_TYPE(value);

    switch (type) {
        case G_TYPE_BOOLEAN:
        case G_TYPE_CHAR:
        case G_TYPE_UCHAR:
        case G_TYPE_UINT:
        case G_TYPE_INT:
        case G_TYPE_INT64:
        case G_TYPE_UINT64:
        case G_TYPE_DOUBLE:
        case G_TYPE_STRING:
        gwy_dict_set_value(dict, key, value);
        return;
    }

    // Objects have to be handled separately since we want a deep copy.
    if (g_type_is_a(type, G_TYPE_OBJECT)) {
        GObject *object = g_value_get_object(value);
        object = gwy_serializable_duplicate(GWY_SERIALIZABLE(object));
        gwy_dict_take_object(dict, key, object);
        return;
    }

    // Boxed are set by value.
    if (g_type_is_a(type, G_TYPE_BOXED)) {
        gwy_dict_set_value(dict, key, value);
        return;
    }

    g_warning("Cannot properly copy value of type %s", g_type_name(type));
    gwy_dict_set_value(dict, key, value);
}

static void
hash_duplicate(gpointer hkey, gpointer hvalue, gpointer hdata)
{
    GQuark key = GPOINTER_TO_UINT(hkey);
    const GValue *value = (GValue*)hvalue;
    GwyDict *duplicate = (GwyDict*)hdata;

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
        c = (guchar)g_value_get_schar(value);
        if (g_ascii_isprint(c) && !g_ascii_isspace(c))
            v = g_strdup_printf("\"%s\" char %c", k, c);
        else
            v = g_strdup_printf("\"%s\" char 0x%02x", k, c);
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

        case G_TYPE_UINT:
        v = g_strdup_printf("\"%s\" int32 %u", k, g_value_get_uint(value));
        break;

        case G_TYPE_INT64:
        /* FIXME: this may fail */
        v = g_strdup_printf("\"%s\" int64 %" G_GINT64_FORMAT,
                            k, g_value_get_int64(value));
        break;

        case G_TYPE_UINT64:
        /* FIXME: this may fail */
        v = g_strdup_printf("\"%s\" int64 %" G_GUINT64_FORMAT,
                            k, g_value_get_uint64(value));
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
 * gwy_dict_transfer:
 * @source: Source dict.
 * @dest: Destination dictionary. It may be the same dictionary as @source, but
 *        @source_prefix and @dest_prefix may not overlap then.
 * @source_prefix: Prefix in @source to take values from.
 * @dest_prefix: Prefix in @dest to put values to.
 * @deep: %TRUE to perform a deep copy, %FALSE to perform a shallow copy.
 *        This option pertains only to contained objects, strings are always
 *        duplicated.
 * @force: %TRUE to replace existing values in @dest.
 *
 * Copies a items from one place in a dictionary to another place and/or another
 * dict.
 *
 * Returns: The number of actually transferred items.
 **/
guint
gwy_dict_transfer(GwyDict *source,
                  GwyDict *dest,
                  const gchar *source_prefix,
                  const gchar *dest_prefix,
                  gboolean deep,
                  gboolean force)
{
    g_return_val_if_fail(GWY_IS_DICT(source), 0);
    g_return_val_if_fail(GWY_IS_DICT(dest), 0);
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
    guint prefix_length = strlen(source_prefix);
    gboolean closed_prefix = (!prefix_length
                              || (source_prefix[prefix_length-1]
                                  == GWY_DICT_PATHSEP));
    GSList *keylist = NULL;
    GHashTableIter iter;
    g_hash_table_iter_init(&iter, source->priv->values);
    gpointer pkey;
    while (g_hash_table_iter_next(&iter, &pkey, NULL)) {
        GQuark quark = GPOINTER_TO_UINT(pkey);
        const gchar *name = g_quark_to_string(quark);
        if (g_str_has_prefix(name, source_prefix)
            && (closed_prefix || name[prefix_length] == GWY_DICT_PATHSEP))
            keylist = g_slist_prepend(keylist, pkey);
    }
    if (!keylist)
        return 0;

    keylist = g_slist_reverse(keylist);
    GString *key = g_string_new(dest_prefix);
    guint dpflen = strlen(dest_prefix);
    if (dpflen && dest_prefix[dpflen-1] == GWY_DICT_PATHSEP)
        dpflen--;
    if (closed_prefix && prefix_length)
        prefix_length--;

    /* Transfer the items */
    guint count = 0;
    for (GSList *l = keylist; l; l = g_slist_next(l)) {
        GValue *val = (GValue*)g_hash_table_lookup(source->priv->values,
                                                   l->data);
        if (G_UNLIKELY(!val)) {
            g_critical("Source dict contents changed during "
                       "gwy_dict_transfer().");
            break;
        }

        g_string_truncate(key, dpflen);
        g_string_append(key,
                        g_quark_to_string(GPOINTER_TO_UINT(l->data))
                        + prefix_length);

        GQuark quark = g_quark_from_string(key->str);
        GValue *copy = (GValue*)g_hash_table_lookup(dest->priv->values,
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
            g_hash_table_insert(dest->priv->values,
                                GUINT_TO_POINTER(quark), copy);
        g_signal_emit(dest, signals[SGNL_ITEM_CHANGED], quark, quark);
        count++;
    }
    g_slist_free(keylist);
    g_string_free(key, TRUE);

    return count;
}

static int
pstring_compare(const void *p, const void *q)
{
    return strcmp(*(gchar**)p, *(gchar**)q);
}

/**
 * gwy_dict_dump_to_text:
 * @dict: A dictionary.
 *
 * Creates a text representation of a dictionary contents.
 *
 * Only simple data types are supported as serialisation of compound objects is
 * not controllable.
 *
 * Returns: (transfer full) (array zero-terminated=1):
 *          A pointer array, each item containing string with one dict
 *          item representation (name, type, value).  The array is sorted by
 *          name.
 **/
gchar**
gwy_dict_dump_to_text(const GwyDict *dict)
{
    g_return_val_if_fail(GWY_IS_DICT(dict), NULL);

    GPtrArray *pa = g_ptr_array_new();
    g_hash_table_foreach(dict->priv->values, hash_text_serialize, pa);
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
 * gwy_dict_new_from_text: (constructor)
 * @text: Text containing dictionary contents as dumped by
 *        gwy_dict_dump_to_text().
 *
 * Constructs a dictionary from its text dump.
 *
 * Returns: The restored dictionary as a newly created object.
 **/
GwyDict*
gwy_dict_new_from_text(const gchar *text)
{
    GwyDict *dict;
    const gchar *tok, *type;
    gchar *name = NULL;
    gsize len, namelen, typelen;
    GQuark key;

    dict = gwy_dict_new();
    dict->priv->in_construction = TRUE;

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
                gwy_dict_set_boolean(dict, key, TRUE);
            else if (len == 5 && g_str_has_prefix(tok, "False"))
                gwy_dict_set_boolean(dict, key, FALSE);
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
            gwy_dict_set_schar(dict, key, (gint8)c);
        }
        /* FIXME char */
        else if (typelen+1 == sizeof("uchar")
                 && g_str_has_prefix(type, "uchar")) {
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
            gwy_dict_set_schar(dict, key, (gint8)c);
        }
        /* int32 */
        else if (typelen+1 == sizeof("int32")
                 && g_str_has_prefix(type, "int32")) {
            gwy_dict_set_int32(dict, key, strtol(tok, NULL, 0));
        }
        /* FIXME uint32 */
        else if (typelen+1 == sizeof("uint32")
                 && g_str_has_prefix(type, "uint32")) {
            gwy_dict_set_int32(dict, key, strtol(tok, NULL, 0));
        }
        /* int64 */
        else if (typelen+1 == sizeof("int64")
                 && g_str_has_prefix(type, "int64")) {
            gwy_dict_set_int64(dict, key,
                               g_ascii_strtoull(tok, NULL, 0));
        }
        /* FIXME uint64 */
        else if (typelen+1 == sizeof("uint64")
                 && g_str_has_prefix(type, "uint64")) {
            gwy_dict_set_int64(dict, key,
                               g_ascii_strtoull(tok, NULL, 0));
        }
        /* double */
        else if (typelen+1 == sizeof("double")
                 && g_str_has_prefix(type, "double")) {
            gwy_dict_set_double(dict, key, g_ascii_strtod(tok, NULL));
        }
        /* string */
        else if (typelen+1 == sizeof("string")
                 && g_str_has_prefix(type, "string")) {
            gchar *s;
            gsize vallen;

            vallen = len;
            s = dequote_token(tok, &vallen);
            gwy_dict_take_string(dict, key, s);
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
    dict->priv->in_construction = FALSE;

    return dict;
}

/**
 * SECTION: dict
 * @title: GwyDict
 * @short_description: Data dictionary with items identified by strings or quarks
 * @see_also: #GHashTable, #GwyInventory
 *
 * #GwyDict is a general-purpose dict, it can hold atomic types,
 * strings and objects. However, objects must implement the #GwySerializable
 * interface, because the dictionary itself is serialisable.
 *
 * A new dictionary can be created with gwy_dict_new(), items can be stored
 * with functions gwy_dict_set_int32(), gwy_dict_set_double(), etc.
 * read with gwy_dict_get_int32(), gwy_dict_get_double(), etc. and
 * removed with gwy_dict_remove() or gwy_dict_remove_prefix(). A
 * presence of a value can be tested with gwy_dict_contains(), convenience
 * functions for reading (updating) a value only if it is present such as
 * gwy_dict_pick_double(), are available too.
 *
 * When non-atomic items are stored, #GwyDict can either takes ownership
 * of them or make a copy. For strings, this means strings stored with
 * gwy_dict_take_string() must be freeable when the dictionary no longer
 * needs them and nothing else must modify or free them after storing them to
 * the dict. Objects are not duplicated whether taken or copied, in this
 * case the difference is only whether the dictionary takes the caller's
 * reference or its adds its own.
 *
 * Items in a #GwyDict can be identified by a #GQuark or the corresponding
 * string.  While #GQuarks are atomic values and allow faster access, they are
 * less convenient for casual usage -- each quark-key function such as
 * gwy_dict_set_double() thus has a string-key counterpart
 * <literal>_n</literal> appended to the name: gwy_dict_set_double_n().
 *
 * An important difference between #GwyDict and ordinary #GHashTable is
 * that the dictionary emits signal #GwyDict::item-changed whenever an item
 * changes.
 **/

/**
 * GwyDict:
 *
 * Object representing hash table-like dictionary.
 *
 * The #GwyDict struct contains private data only and should be accessed
 * using the functions below.
 **/

/**
 * GwyDictClass:
 *
 * Class of dictionaries.
 **/

/**
 * GWY_DICT_PATHSEP:
 *
 * Path separator to be used for hierarchical structures in dictionaries.
 **/

/**
 * GWY_DICT_PATHSEP_STR:
 *
 * Path separator to be used for hierarchical structures in dictionaries,
 * as a string.
 **/

/**
 * gwy_dict_duplicate:
 * @dict: A dictionary.
 *
 * This is a convenience wrapper of gwy_serializable_duplicate().
 **/

/**
 * gwy_dict_assign:
 * @dest: Destination dictionary.
 * @src: Source dictionary.
 *
 * Copies the contents of a dictionary.
 *
 * This is a convenience wrapper of gwy_serializable_assign().
 *
 * See also gwy_dict_transfer() for a more powerful method of copying data
 * between containers.
 **/

/**
 * GwyDictForeachFunc:
 * @key: Item key.
 * @value: Item value.  It must not be modified.
 * @user_data: Data passed to gwy_dict_foreach().
 *
 * Type of function passed to gwy_dict_foreach().
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
