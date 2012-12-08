/*
 *  $Id$
 *  Copyright (C) 2012 David Nečas (Yeti).
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

#include "libgwy/macros.h"
#include "libgwy/strfuncs.h"
#include "libgwy/object-utils.h"
#include "libgwyui/widget-utils.h"
#include "libgwyui/array-store.h"

enum {
    PROP_0,
    PROP_ARRAY,
    N_PROPS
};

enum {
    COLUMN_ID,
    COLUMN_ITEM,
    N_COLUMNS
};

typedef struct _GwyArrayStorePrivate ArrayStore;

struct _GwyArrayStorePrivate {
    GwyArray *array;
    guint stamp;
    guint cached_size;

    gulong item_updated_id;
    gulong item_inserted_id;
    gulong item_deleted_id;
};

static void              gwy_array_store_finalize       (GObject *object);
static void              gwy_array_store_dispose        (GObject *object);
static void              gwy_array_store_tree_model_init(GtkTreeModelIface *iface);
static void              gwy_array_store_set_property   (GObject *object,
                                                         guint prop_id,
                                                         const GValue *value,
                                                         GParamSpec *pspec);
static void              gwy_array_store_get_property   (GObject *object,
                                                         guint prop_id,
                                                         GValue *value,
                                                         GParamSpec *pspec);
static GtkTreeModelFlags gwy_array_store_get_flags      (GtkTreeModel *model);
static gint              gwy_array_store_get_n_columns  (GtkTreeModel *model);
static GType             gwy_array_store_get_column_type(GtkTreeModel *model,
                                                         gint column);
static gboolean          gwy_array_store_get_tree_iter  (GtkTreeModel *model,
                                                         GtkTreeIter *iter,
                                                         GtkTreePath *path);
static GtkTreePath*      gwy_array_store_get_path       (GtkTreeModel *model,
                                                         GtkTreeIter *iter);
static void              gwy_array_store_get_value      (GtkTreeModel *model,
                                                         GtkTreeIter *iter,
                                                         gint column,
                                                         GValue *value);
static gboolean          gwy_array_store_iter_next      (GtkTreeModel *model,
                                                         GtkTreeIter *iter);
static gboolean          gwy_array_store_iter_children  (GtkTreeModel *model,
                                                         GtkTreeIter *iter,
                                                         GtkTreeIter *parent);
static gboolean          gwy_array_store_iter_has_child (GtkTreeModel *model,
                                                         GtkTreeIter *iter);
static gint              gwy_array_store_iter_n_children(GtkTreeModel *model,
                                                         GtkTreeIter *iter);
static gboolean          gwy_array_store_iter_nth_child (GtkTreeModel *model,
                                                         GtkTreeIter *iter,
                                                         GtkTreeIter *parent,
                                                         gint n);
static gboolean          gwy_array_store_iter_parent    (GtkTreeModel *model,
                                                         GtkTreeIter *iter,
                                                         GtkTreeIter *child);
static void              array_item_updated             (GwyArrayStore *store,
                                                         guint i,
                                                         GwyArray *array);
static void              array_item_inserted            (GwyArrayStore *store,
                                                         guint i,
                                                         GwyArray *array);
static void              array_item_deleted             (GwyArrayStore *store,
                                                         guint i,
                                                         GwyArray *array);
static void              set_array                      (GwyArrayStore *store,
                                                         GwyArray *array);

static GParamSpec *properties[N_PROPS];

G_DEFINE_TYPE_EXTENDED
    (GwyArrayStore, gwy_array_store, G_TYPE_INITIALLY_UNOWNED, 0,
     GWY_IMPLEMENT_TREE_MODEL(gwy_array_store_tree_model_init))

static void
gwy_array_store_class_init(GwyArrayStoreClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    g_type_class_add_private(klass, sizeof(ArrayStore));

    gobject_class->finalize = gwy_array_store_finalize;
    gobject_class->dispose = gwy_array_store_dispose;
    gobject_class->get_property = gwy_array_store_get_property;
    gobject_class->set_property = gwy_array_store_set_property;

    properties[PROP_ARRAY]
        = g_param_spec_object("array",
                              "Array",
                              "Array object the array store wraps.",
                              GWY_TYPE_ARRAY,
                              G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY
                              | G_PARAM_STATIC_STRINGS);

    for (guint i = 1; i < N_PROPS; i++)
        g_object_class_install_property(gobject_class, i, properties[i]);
}

static void
gwy_array_store_tree_model_init(GtkTreeModelIface *iface)
{
    iface->get_flags = gwy_array_store_get_flags;
    iface->get_n_columns = gwy_array_store_get_n_columns;
    iface->get_column_type = gwy_array_store_get_column_type;
    iface->get_iter = gwy_array_store_get_tree_iter;
    iface->get_path = gwy_array_store_get_path;
    iface->get_value = gwy_array_store_get_value;
    iface->iter_next = gwy_array_store_iter_next;
    iface->iter_children = gwy_array_store_iter_children;
    iface->iter_has_child = gwy_array_store_iter_has_child;
    iface->iter_n_children = gwy_array_store_iter_n_children;
    iface->iter_nth_child = gwy_array_store_iter_nth_child;
    iface->iter_parent = gwy_array_store_iter_parent;
}

static void
gwy_array_store_init(GwyArrayStore *store)
{
    store->priv = G_TYPE_INSTANCE_GET_PRIVATE(store, GWY_TYPE_ARRAY_STORE,
                                              ArrayStore);
    store->priv->stamp = g_random_int();
}

static void
gwy_array_store_finalize(GObject *object)
{
    ArrayStore *priv = GWY_ARRAY_STORE(object)->priv;
    GWY_OBJECT_UNREF(priv->array);
    G_OBJECT_CLASS(gwy_array_store_parent_class)->finalize(object);
}

static void
gwy_array_store_dispose(GObject *object)
{
    ArrayStore *priv = GWY_ARRAY_STORE(object)->priv;
    GWY_SIGNAL_HANDLER_DISCONNECT(priv->array, priv->item_updated_id);
    GWY_SIGNAL_HANDLER_DISCONNECT(priv->array, priv->item_inserted_id);
    GWY_SIGNAL_HANDLER_DISCONNECT(priv->array, priv->item_deleted_id);
    G_OBJECT_CLASS(gwy_array_store_parent_class)->dispose(object);
}

static void
gwy_array_store_set_property(GObject *object,
                                 guint prop_id,
                                 const GValue *value,
                                 GParamSpec *pspec)
{
    GwyArrayStore *store = GWY_ARRAY_STORE(object);

    switch (prop_id) {
        case PROP_ARRAY:
        set_array(store, g_value_get_object(value));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_array_store_get_property(GObject *object,
                                 guint prop_id,
                                 GValue *value,
                                 GParamSpec *pspec)
{
    ArrayStore *priv = GWY_ARRAY_STORE(object)->priv;

    switch (prop_id) {
        case PROP_ARRAY:
        g_value_set_object(value, priv->array);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static GtkTreeModelFlags
gwy_array_store_get_flags(G_GNUC_UNUSED GtkTreeModel *model)
{
    return GTK_TREE_MODEL_LIST_ONLY;
}

static gint
gwy_array_store_get_n_columns(G_GNUC_UNUSED GtkTreeModel *model)
{
    return N_COLUMNS;
}

static GType
gwy_array_store_get_column_type(G_GNUC_UNUSED GtkTreeModel *model,
                                gint column)
{
    if (column == COLUMN_ID)
        return G_TYPE_UINT;
    if (column == COLUMN_ITEM)
        return G_TYPE_POINTER;
    g_return_val_if_reached(G_TYPE_NONE);
}

static gboolean
gwy_array_store_get_tree_iter(GtkTreeModel *model,
                              GtkTreeIter *iter,
                              GtkTreePath *path)
{
    g_return_val_if_fail(gtk_tree_path_get_depth(path) > 0, FALSE);
    ArrayStore *priv = GWY_ARRAY_STORE(model)->priv;
    guint i = gtk_tree_path_get_indices(path)[0];
    if (i >= priv->cached_size)
        return FALSE;

    iter->stamp = priv->stamp;
    iter->user_data2 = GUINT_TO_POINTER(i);
    return TRUE;
}

static GtkTreePath*
gwy_array_store_get_path(GtkTreeModel *model,
                         GtkTreeIter *iter)
{
    ArrayStore *priv = GWY_ARRAY_STORE(model)->priv;
    g_assert((guint)iter->stamp == priv->stamp);
    GtkTreePath *path = gtk_tree_path_new();
    gtk_tree_path_append_index(path, GPOINTER_TO_UINT(iter->user_data2));
    return path;
}

static void
gwy_array_store_get_value(GtkTreeModel *model,
                          GtkTreeIter *iter,
                          gint column,
                          GValue *value)
{
    ArrayStore *priv = GWY_ARRAY_STORE(model)->priv;
    g_assert((guint)iter->stamp == priv->stamp);
    guint i = GPOINTER_TO_UINT(iter->user_data2);
    if (column == COLUMN_ID) {
        g_value_init(value, G_TYPE_UINT);
        g_value_set_uint(value, i);
    }
    else if (column == COLUMN_ITEM) {
        g_value_init(value, G_TYPE_POINTER);
        g_value_set_pointer(value, gwy_array_get(priv->array, i));
    }
    else {
        g_critical("Invalid column id.");
    }
}

static gboolean
gwy_array_store_iter_next(GtkTreeModel *model,
                          GtkTreeIter *iter)
{
    ArrayStore *priv = GWY_ARRAY_STORE(model)->priv;
    g_assert((guint)iter->stamp == priv->stamp);
    guint i = GPOINTER_TO_UINT(iter->user_data2) + 1;
    iter->user_data2 = GUINT_TO_POINTER(i);
    return i < priv->cached_size;
}

static gboolean
gwy_array_store_iter_children(GtkTreeModel *model,
                              GtkTreeIter *iter,
                              GtkTreeIter *parent)
{
    if (parent)
        return FALSE;

    ArrayStore *priv = GWY_ARRAY_STORE(model)->priv;
    g_assert((guint)parent->stamp == priv->stamp);
    if (!priv->cached_size)
        return FALSE;

    iter->stamp = priv->stamp;
    iter->user_data2 = GUINT_TO_POINTER(0);
    return TRUE;
}

static gboolean
gwy_array_store_iter_has_child(G_GNUC_UNUSED GtkTreeModel *model,
                               G_GNUC_UNUSED GtkTreeIter *iter)
{
    return FALSE;
}

static gint
gwy_array_store_iter_n_children(GtkTreeModel *model,
                                GtkTreeIter *iter)
{
    if (iter)
        return 0;

    ArrayStore *priv = GWY_ARRAY_STORE(model)->priv;
    g_assert((guint)iter->stamp == priv->stamp);
    return priv->cached_size;
}

static gboolean
gwy_array_store_iter_nth_child(GtkTreeModel *model,
                               GtkTreeIter *iter,
                               GtkTreeIter *parent,
                               gint n)
{
    if (parent)
        return FALSE;

    ArrayStore *priv = GWY_ARRAY_STORE(model)->priv;
    g_assert((guint)parent->stamp == priv->stamp);
    if ((guint)n >= priv->cached_size)
        return FALSE;

    iter->stamp = priv->stamp;
    iter->user_data2 = GUINT_TO_POINTER(n);
    return TRUE;
}

static gboolean
gwy_array_store_iter_parent(G_GNUC_UNUSED GtkTreeModel *model,
                            G_GNUC_UNUSED GtkTreeIter *iter,
                            G_GNUC_UNUSED GtkTreeIter *child)
{
    return FALSE;
}

static void
array_item_updated(GwyArrayStore *store,
                   guint i,
                   G_GNUC_UNUSED GwyArray *array)
{
    GtkTreeIter iter = {
        .stamp = store->priv->stamp,
        .user_data2 = GUINT_TO_POINTER(i),
    };
    GtkTreePath *path = gtk_tree_path_new();
    gtk_tree_path_append_index(path, i);
    gtk_tree_model_row_changed(GTK_TREE_MODEL(store), path, &iter);
    gtk_tree_path_free(path);
}

static void
array_item_inserted(GwyArrayStore *store,
                    guint i,
                    G_GNUC_UNUSED GwyArray *array)
{
    ArrayStore *priv = store->priv;
    priv->stamp++;
    priv->cached_size++;
    GtkTreeIter iter = {
        .stamp = priv->stamp,
        .user_data2 = GUINT_TO_POINTER(i),
    };
    GtkTreePath *path = gtk_tree_path_new();
    gtk_tree_path_append_index(path, i);
    gtk_tree_model_row_inserted(GTK_TREE_MODEL(store), path, &iter);
    gtk_tree_path_free(path);
}

static void
array_item_deleted(GwyArrayStore *store,
                   guint i,
                   G_GNUC_UNUSED GwyArray *array)
{
    ArrayStore *priv = store->priv;
    priv->stamp++;
    priv->cached_size--;
    GtkTreePath *path = gtk_tree_path_new();
    gtk_tree_path_append_index(path, i);
    gtk_tree_model_row_deleted(GTK_TREE_MODEL(store), path);
    gtk_tree_path_free(path);
}

/**
 * gwy_array_store_new:
 * @array: (transfer full):
 *         An array.
 *
 * Creates a new tree model wrapper around an array.
 *
 * Returns: The newly created array store.
 **/
GwyArrayStore*
gwy_array_store_new(GwyArray *array)
{
    g_return_val_if_fail(GWY_IS_ARRAY(array), NULL);
    return (GwyArrayStore*)g_object_new(GWY_TYPE_ARRAY_STORE,
                                        "array", array,
                                        NULL);
}

static void
set_array(GwyArrayStore *store,
          GwyArray *array)
{
    g_object_ref(array);
    ArrayStore *priv = store->priv;
    priv->array = array;
    priv->cached_size = gwy_array_size(array);

    priv->item_updated_id
        = g_signal_connect_swapped(array, "item-updated",
                                   G_CALLBACK(array_item_updated), store);
    priv->item_inserted_id
        = g_signal_connect(array, "item-inserted",
                           G_CALLBACK(array_item_inserted), store);
    priv->item_deleted_id
        = g_signal_connect(array, "item-deleted",
                           G_CALLBACK(array_item_deleted), store);
}

/**
 * gwy_array_store_get_array:
 * @store: An array store.
 *
 * Gets the array an array store wraps.
 *
 * Returns: (transfer none):
 *          The underlying array (its reference count is not increased).
 **/
GwyArray*
gwy_array_store_get_array(const GwyArrayStore *store)
{
    g_return_val_if_fail(GWY_IS_ARRAY_STORE(store), NULL);
    return store->priv->array;
}

/**
 * gwy_array_store_get_iter:
 * @store: An array store.
 * @i: Item position in the array.
 * @iter: Tree iterator to set to point to item.
 *
 * Initializes a tree iterator to row the corresponding to an array item.
 *
 * This is essentially just a shorthand for
 * <literal>gtk_tree_model_iter_nth_child(@model, @iter, %NULL, @i);</literal>
 * without the redundant argument.
 *
 * Returns: %TRUE if @iter is valid, that is the item exists, %FALSE if @iter
 *          was not set.
 **/
gboolean
gwy_array_store_get_iter(const GwyArrayStore *store,
                         guint i,
                         GtkTreeIter *iter)
{
    g_return_val_if_fail(GWY_IS_ARRAY_STORE(store), FALSE);
    g_return_val_if_fail(iter, FALSE);

    ArrayStore *priv = store->priv;
    if (i >= priv->cached_size)
        return FALSE;

    iter->stamp = priv->stamp;
    iter->user_data2 = GUINT_TO_POINTER(i);
    return TRUE;
}

/**
 * gwy_array_store_iter_is_valid:
 * @store: An array store.
 * @iter: Tree iterator to check.
 *
 * Checks if the given iter is a valid iter for this array store.
 *
 * Returns: %TRUE if the iter is valid, %FALSE if the iter is invalid.
 **/
gboolean
gwy_array_store_iter_is_valid(const GwyArrayStore *store,
                              GtkTreeIter *iter)
{
    g_return_val_if_fail(GWY_IS_ARRAY_STORE(store), FALSE);
    if (!iter)
        return FALSE;

    ArrayStore *priv = store->priv;
    if ((guint)iter->stamp != priv->stamp)
        return FALSE;

    g_assert(GPOINTER_TO_UINT(iter->user_data2) < priv->cached_size);
    return TRUE;
}

/**
 * SECTION: array-store
 * @title: GwyArrayStore
 * @short_description: Tree model wrapper of #GwyArray.
 *
 * #GwyArrayStore is a simple adaptor class that wraps #GwyArray in
 * #GtkTreeModel interface.  It is list-only and it does not have persistent
 * iterators.  It offers no methods to manipulate items, this should be done on
 * the underlying array.
 *
 * A #GwyArrayStore has always exactly two columns:
 * <variablelist>
 *   <varlistentry>
 *     <term><literal>0</literal>, type %G_TYPE_UINT</term>
 *     <listitem>Index of the item in the underlying array.</listitem>
 *   </varlistentry>
 *   <varlistentry>
 *     <term><literal>1</literal>, type %G_TYPE_POINTER</term>
 *     <listitem>Direct pointer to the item in the underlying array.</listitem>
 *   </varlistentry>
 * </variablelist>
 **/

/**
 * GwyArrayStore:
 *
 * Object representing a tree model wrapper of an array.
 *
 * The #GwyArrayStore struct contains private data only and should be
 * accessed using the functions below.
 **/

/**
 * GwyArrayStoreClass:
 *
 * Class of array tree model wrappers.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
