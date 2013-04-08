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

#include "libgwy/macros.h"
#include "libgwy/object-utils.h"
#include "libgwyui/widget-utils.h"
#include "libgwyui/list-store.h"

enum {
    PROP_0,
    PROP_BACKEND,
    N_PROPS
};

enum {
    COLUMN_I,
    COLUMN_ITEM,
    N_COLUMNS
};

typedef struct _GwyListStorePrivate ListStore;

struct _GwyListStorePrivate {
    GwyListable *backend;
    guint stamp;
    guint cached_size;

    gulong item_updated_id;
    gulong item_inserted_id;
    gulong item_deleted_id;
    gulong items_reordered_id;
};

static void              gwy_list_store_finalize       (GObject *object);
static void              gwy_list_store_dispose        (GObject *object);
static void              gwy_list_store_tree_model_init(GtkTreeModelIface *iface);
static void              gwy_list_store_set_property   (GObject *object,
                                                        guint prop_id,
                                                        const GValue *value,
                                                        GParamSpec *pspec);
static void              gwy_list_store_get_property   (GObject *object,
                                                        guint prop_id,
                                                        GValue *value,
                                                        GParamSpec *pspec);
static GtkTreeModelFlags gwy_list_store_get_flags      (GtkTreeModel *model);
static gint              gwy_list_store_get_n_columns  (GtkTreeModel *model);
static GType             gwy_list_store_get_column_type(GtkTreeModel *model,
                                                        gint column);
static gboolean          gwy_list_store_get_tree_iter  (GtkTreeModel *model,
                                                        GtkTreeIter *iter,
                                                        GtkTreePath *path);
static GtkTreePath*      gwy_list_store_get_path       (GtkTreeModel *model,
                                                        GtkTreeIter *iter);
static void              gwy_list_store_get_value      (GtkTreeModel *model,
                                                        GtkTreeIter *iter,
                                                        gint column,
                                                        GValue *value);
static gboolean          gwy_list_store_iter_next      (GtkTreeModel *model,
                                                        GtkTreeIter *iter);
static gboolean          gwy_list_store_iter_children  (GtkTreeModel *model,
                                                        GtkTreeIter *iter,
                                                        GtkTreeIter *parent);
static gboolean          gwy_list_store_iter_has_child (GtkTreeModel *model,
                                                        GtkTreeIter *iter);
static gint              gwy_list_store_iter_n_children(GtkTreeModel *model,
                                                        GtkTreeIter *iter);
static gboolean          gwy_list_store_iter_nth_child (GtkTreeModel *model,
                                                        GtkTreeIter *iter,
                                                        GtkTreeIter *parent,
                                                        gint n);
static gboolean          gwy_list_store_iter_parent    (GtkTreeModel *model,
                                                        GtkTreeIter *iter,
                                                        GtkTreeIter *child);
static void              list_item_updated             (GwyListStore *store,
                                                        guint i,
                                                        GwyListable *listable);
static void              list_item_inserted            (GwyListStore *store,
                                                        guint i,
                                                        GwyListable *listable);
static void              list_item_deleted             (GwyListStore *store,
                                                        guint i,
                                                        GwyListable *listable);
static void              list_items_reordered          (GwyListStore *store,
                                                        gint *new_order,
                                                        GwyListable *listable);
static void              set_backend                   (GwyListStore *store,
                                                        GwyListable *listable);

static GParamSpec *properties[N_PROPS];

G_DEFINE_TYPE_EXTENDED
    (GwyListStore, gwy_list_store, G_TYPE_INITIALLY_UNOWNED, 0,
     GWY_IMPLEMENT_TREE_MODEL(gwy_list_store_tree_model_init))

static void
gwy_list_store_class_init(GwyListStoreClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    g_type_class_add_private(klass, sizeof(ListStore));

    gobject_class->finalize = gwy_list_store_finalize;
    gobject_class->dispose = gwy_list_store_dispose;
    gobject_class->get_property = gwy_list_store_get_property;
    gobject_class->set_property = gwy_list_store_set_property;

    properties[PROP_BACKEND]
        = g_param_spec_object("backend",
                              "Backend",
                              "Listable object providing the data.",
                              GWY_TYPE_LISTABLE,
                              G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY
                              | G_PARAM_STATIC_STRINGS);

    for (guint i = 1; i < N_PROPS; i++)
        g_object_class_install_property(gobject_class, i, properties[i]);
}

static void
gwy_list_store_tree_model_init(GtkTreeModelIface *iface)
{
    iface->get_flags = gwy_list_store_get_flags;
    iface->get_n_columns = gwy_list_store_get_n_columns;
    iface->get_column_type = gwy_list_store_get_column_type;
    iface->get_iter = gwy_list_store_get_tree_iter;
    iface->get_path = gwy_list_store_get_path;
    iface->get_value = gwy_list_store_get_value;
    iface->iter_next = gwy_list_store_iter_next;
    iface->iter_children = gwy_list_store_iter_children;
    iface->iter_has_child = gwy_list_store_iter_has_child;
    iface->iter_n_children = gwy_list_store_iter_n_children;
    iface->iter_nth_child = gwy_list_store_iter_nth_child;
    iface->iter_parent = gwy_list_store_iter_parent;
}

static void
gwy_list_store_init(GwyListStore *store)
{
    store->priv = G_TYPE_INSTANCE_GET_PRIVATE(store, GWY_TYPE_LIST_STORE,
                                              ListStore);
}

static void
gwy_list_store_finalize(GObject *object)
{
    ListStore *priv = GWY_LIST_STORE(object)->priv;
    GWY_OBJECT_UNREF(priv->backend);
    G_OBJECT_CLASS(gwy_list_store_parent_class)->finalize(object);
}

static void
gwy_list_store_dispose(GObject *object)
{
    ListStore *priv = GWY_LIST_STORE(object)->priv;
    GWY_SIGNAL_HANDLER_DISCONNECT(priv->backend, priv->item_updated_id);
    GWY_SIGNAL_HANDLER_DISCONNECT(priv->backend, priv->item_inserted_id);
    GWY_SIGNAL_HANDLER_DISCONNECT(priv->backend, priv->item_deleted_id);
    GWY_SIGNAL_HANDLER_DISCONNECT(priv->backend, priv->items_reordered_id);
    G_OBJECT_CLASS(gwy_list_store_parent_class)->dispose(object);
}

static void
gwy_list_store_set_property(GObject *object,
                            guint prop_id,
                            const GValue *value,
                            GParamSpec *pspec)
{
    GwyListStore *store = GWY_LIST_STORE(object);

    switch (prop_id) {
        case PROP_BACKEND:
        set_backend(store, g_value_get_object(value));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_list_store_get_property(GObject *object,
                            guint prop_id,
                            GValue *value,
                            GParamSpec *pspec)
{
    ListStore *priv = GWY_LIST_STORE(object)->priv;

    switch (prop_id) {
        case PROP_BACKEND:
        g_value_set_object(value, priv->backend);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static GtkTreeModelFlags
gwy_list_store_get_flags(G_GNUC_UNUSED GtkTreeModel *model)
{
    return GTK_TREE_MODEL_LIST_ONLY;
}

static gint
gwy_list_store_get_n_columns(G_GNUC_UNUSED GtkTreeModel *model)
{
    return N_COLUMNS;
}

static GType
gwy_list_store_get_column_type(G_GNUC_UNUSED GtkTreeModel *model,
                               gint column)
{
    if (column == COLUMN_I)
        return G_TYPE_UINT;
    if (column == COLUMN_ITEM)
        return G_TYPE_POINTER;
    g_return_val_if_reached(G_TYPE_NONE);
}

static gboolean
gwy_list_store_get_tree_iter(GtkTreeModel *model,
                             GtkTreeIter *iter,
                             GtkTreePath *path)
{
    g_return_val_if_fail(gtk_tree_path_get_depth(path) > 0, FALSE);
    ListStore *priv = GWY_LIST_STORE(model)->priv;
    guint i = gtk_tree_path_get_indices(path)[0];
    if (i >= priv->cached_size)
        return FALSE;

    iter->stamp = priv->stamp;
    iter->user_data2 = GUINT_TO_POINTER(i);
    return TRUE;
}

static GtkTreePath*
gwy_list_store_get_path(GtkTreeModel *model,
                        GtkTreeIter *iter)
{
    ListStore *priv = GWY_LIST_STORE(model)->priv;
    g_assert((guint)iter->stamp == priv->stamp);
    GtkTreePath *path = gtk_tree_path_new();
    gtk_tree_path_append_index(path, GPOINTER_TO_UINT(iter->user_data2));
    return path;
}

static void
gwy_list_store_get_value(GtkTreeModel *model,
                         GtkTreeIter *iter,
                         gint column,
                         GValue *value)
{
    ListStore *priv = GWY_LIST_STORE(model)->priv;
    g_assert((guint)iter->stamp == priv->stamp);
    guint i = GPOINTER_TO_UINT(iter->user_data2);
    if (column == COLUMN_I) {
        g_value_init(value, G_TYPE_UINT);
        g_value_set_uint(value, i);
    }
    else if (column == COLUMN_ITEM) {
        g_value_init(value, G_TYPE_POINTER);
        g_value_set_pointer(value, gwy_listable_get(priv->backend, i));
    }
    else {
        g_critical("Invalid column id.");
    }
}

static gboolean
gwy_list_store_iter_next(GtkTreeModel *model,
                         GtkTreeIter *iter)
{
    ListStore *priv = GWY_LIST_STORE(model)->priv;
    g_assert((guint)iter->stamp == priv->stamp);
    guint i = GPOINTER_TO_UINT(iter->user_data2) + 1;
    iter->user_data2 = GUINT_TO_POINTER(i);
    return i < priv->cached_size;
}

static gboolean
gwy_list_store_iter_children(GtkTreeModel *model,
                             GtkTreeIter *iter,
                             GtkTreeIter *parent)
{
    if (parent)
        return FALSE;

    ListStore *priv = GWY_LIST_STORE(model)->priv;
    if (!priv->cached_size)
        return FALSE;

    iter->stamp = priv->stamp;
    iter->user_data2 = GUINT_TO_POINTER(0);
    return TRUE;
}

static gboolean
gwy_list_store_iter_has_child(G_GNUC_UNUSED GtkTreeModel *model,
                              G_GNUC_UNUSED GtkTreeIter *iter)
{
    return FALSE;
}

static gint
gwy_list_store_iter_n_children(GtkTreeModel *model,
                               GtkTreeIter *iter)
{
    if (iter)
        return 0;

    ListStore *priv = GWY_LIST_STORE(model)->priv;
    g_assert((guint)iter->stamp == priv->stamp);
    return priv->cached_size;
}

static gboolean
gwy_list_store_iter_nth_child(GtkTreeModel *model,
                              GtkTreeIter *iter,
                              GtkTreeIter *parent,
                              gint n)
{
    if (parent)
        return FALSE;

    ListStore *priv = GWY_LIST_STORE(model)->priv;
    if ((guint)n >= priv->cached_size)
        return FALSE;

    iter->stamp = priv->stamp;
    iter->user_data2 = GUINT_TO_POINTER(n);
    return TRUE;
}

static gboolean
gwy_list_store_iter_parent(G_GNUC_UNUSED GtkTreeModel *model,
                           G_GNUC_UNUSED GtkTreeIter *iter,
                           G_GNUC_UNUSED GtkTreeIter *child)
{
    return FALSE;
}

static void
list_item_updated(GwyListStore *store,
                  guint i,
                  G_GNUC_UNUSED GwyListable *listable)
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
list_item_inserted(GwyListStore *store,
                   guint i,
                   G_GNUC_UNUSED GwyListable *listable)
{
    ListStore *priv = store->priv;
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
list_item_deleted(GwyListStore *store,
                  guint i,
                  G_GNUC_UNUSED GwyListable *listable)
{
    ListStore *priv = store->priv;
    priv->stamp++;
    priv->cached_size--;
    GtkTreePath *path = gtk_tree_path_new();
    gtk_tree_path_append_index(path, i);
    gtk_tree_model_row_deleted(GTK_TREE_MODEL(store), path);
    gtk_tree_path_free(path);
}

static void
list_items_reordered(GwyListStore *store,
                     gint *new_order,
                     G_GNUC_UNUSED GwyListable *listable)
{
    ListStore *priv = store->priv;
    priv->stamp++;
    GtkTreePath *path = gtk_tree_path_new();
    gtk_tree_model_rows_reordered(GTK_TREE_MODEL(store), path, NULL, new_order);
    gtk_tree_path_free(path);
}

/**
 * gwy_list_store_new:
 * @backend: (transfer full):
 *           A listable object that will provide the data.
 *
 * Creates a new tree model wrapper around a listable object.
 *
 * Returns: The newly created list store.
 **/
GwyListStore*
gwy_list_store_new(GwyListable *backend)
{
    g_return_val_if_fail(GWY_IS_LISTABLE(backend), NULL);
    return (GwyListStore*)g_object_new(GWY_TYPE_LIST_STORE,
                                       "backend", backend,
                                       NULL);
}

static void
set_backend(GwyListStore *store,
            GwyListable *listable)
{
    g_object_ref(listable);
    ListStore *priv = store->priv;
    priv->backend = listable;
    priv->cached_size = gwy_listable_size(listable);

    priv->item_updated_id
        = g_signal_connect_swapped(listable, "item-updated",
                                   G_CALLBACK(list_item_updated), store);
    priv->item_inserted_id
        = g_signal_connect_swapped(listable, "item-inserted",
                                   G_CALLBACK(list_item_inserted), store);
    priv->item_deleted_id
        = g_signal_connect_swapped(listable, "item-deleted",
                                   G_CALLBACK(list_item_deleted), store);
    priv->items_reordered_id
        = g_signal_connect_swapped(listable, "items-reordered",
                                   G_CALLBACK(list_items_reordered), store);
}

/**
 * gwy_list_store_get_backend:
 * @store: A list store wrapper.
 *
 * Gets the list a list store wraps.
 *
 * Returns: (transfer none):
 *          The underlying listable object (its reference count is not
 *          increased).
 **/
GwyListable*
gwy_list_store_get_backend(const GwyListStore *store)
{
    g_return_val_if_fail(GWY_IS_LIST_STORE(store), NULL);
    return store->priv->backend;
}

/**
 * gwy_list_store_get_iter:
 * @store: A list store wrapper.
 * @i: Item position in the list.
 * @iter: Tree iterator to set to point to item.
 *
 * Initializes a tree iterator to row the corresponding to a list item.
 *
 * This is essentially just a shorthand for
 * <literal>gtk_tree_model_iter_nth_child(@model, @iter, %NULL, @i);</literal>
 * without the redundant argument.
 *
 * Returns: %TRUE if @iter is valid, that is the item exists, %FALSE if @iter
 *          was not set.
 **/
gboolean
gwy_list_store_get_iter(const GwyListStore *store,
                        guint i,
                        GtkTreeIter *iter)
{
    g_return_val_if_fail(GWY_IS_LIST_STORE(store), FALSE);
    g_return_val_if_fail(iter, FALSE);

    ListStore *priv = store->priv;
    if (i >= priv->cached_size)
        return FALSE;

    iter->stamp = priv->stamp;
    iter->user_data2 = GUINT_TO_POINTER(i);
    return TRUE;
}

/**
 * gwy_list_store_iter_is_valid:
 * @store: A list store wrapper.
 * @iter: Tree iterator to check.
 *
 * Checks if the given iter is a valid iter for this list store.
 *
 * Returns: %TRUE if the iter is valid, %FALSE if the iter is invalid.
 **/
gboolean
gwy_list_store_iter_is_valid(const GwyListStore *store,
                              GtkTreeIter *iter)
{
    g_return_val_if_fail(GWY_IS_LIST_STORE(store), FALSE);
    if (!iter)
        return FALSE;

    ListStore *priv = store->priv;
    if ((guint)iter->stamp != priv->stamp)
        return FALSE;

    g_assert(GPOINTER_TO_UINT(iter->user_data2) < priv->cached_size);
    return TRUE;
}

/**
 * SECTION: list-store
 * @title: GwyListStore
 * @short_description: Tree model wrapper of #GwyList.
 *
 * #GwyListStore is a simple adaptor class that wraps #GwyList in #GtkTreeModel
 * interface.  It is list-only and it does not have persistent iterators.  It
 * offers no methods to manipulate items, this should be done on the underlying
 * list.
 *
 * The backend is set upon construction and remains the same during the entire
 * life time of the wrapper.
 *
 * The backend list class must implement two #GwyList methods: gwy_list_get()
 * and gwy_list_size().  In addition it may define signals "item-inserted",
 * "item-deleted" and "item-updated" with one #guint argument representing the
 * item position.  Furthermore it may define signal "items-reordered" with one
 * #gpointer argument representing the new sort order – see #GtkTreeModel for a
 * description of the sort order argument.
 *
 * A #GwyListStore has always exactly two columns:
 * <variablelist>
 *   <varlistentry>
 *     <term><literal>0</literal>, type %G_TYPE_UINT</term>
 *     <listitem>Index of the item in the underlying list.</listitem>
 *   </varlistentry>
 *   <varlistentry>
 *     <term><literal>1</literal>, type %G_TYPE_POINTER</term>
 *     <listitem>Direct pointer to the item in the underlying list.</listitem>
 *   </varlistentry>
 * </variablelist>
 **/

/**
 * GwyListStore:
 *
 * Object representing a tree model wrapper of a list.
 *
 * The #GwyListStore struct contains private data only and should be
 * accessed using the functions below.
 **/

/**
 * GwyListStoreClass:
 *
 * Class of list tree model wrappers.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
