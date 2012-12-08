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
#include "libgwyui/inventory-store.h"

enum {
    PROP_0,
    PROP_INVENTORY,
    N_PROPS
};

typedef struct _GwyInventoryStorePrivate InventoryStore;

struct _GwyInventoryStorePrivate {
    GwyInventory *inventory;
    guint stamp;

    // Cached inventory properties
    const GwyInventoryItemType *item_type;

    gulong item_updated_id;
    gulong item_inserted_id;
    gulong item_deleted_id;
    gulong items_reordered_id;
};

static void              gwy_inventory_store_finalize       (GObject *object);
static void              gwy_inventory_store_dispose        (GObject *object);
static void              gwy_inventory_store_tree_model_init(GtkTreeModelIface *iface);
static void              gwy_inventory_store_set_property   (GObject *object,
                                                             guint prop_id,
                                                             const GValue *value,
                                                             GParamSpec *pspec);
static void              gwy_inventory_store_get_property   (GObject *object,
                                                             guint prop_id,
                                                             GValue *value,
                                                             GParamSpec *pspec);
static GtkTreeModelFlags gwy_inventory_store_get_flags      (GtkTreeModel *model);
static gint              gwy_inventory_store_get_n_columns  (GtkTreeModel *model);
static GType             gwy_inventory_store_get_column_type(GtkTreeModel *model,
                                                             gint column);
static gboolean          gwy_inventory_store_get_tree_iter  (GtkTreeModel *model,
                                                             GtkTreeIter *iter,
                                                             GtkTreePath *path);
static GtkTreePath*      gwy_inventory_store_get_path       (GtkTreeModel *model,
                                                             GtkTreeIter *iter);
static void              gwy_inventory_store_get_value      (GtkTreeModel *model,
                                                             GtkTreeIter *iter,
                                                             gint column,
                                                             GValue *value);
static gboolean          gwy_inventory_store_iter_next      (GtkTreeModel *model,
                                                             GtkTreeIter *iter);
static gboolean          gwy_inventory_store_iter_children  (GtkTreeModel *model,
                                                             GtkTreeIter *iter,
                                                             GtkTreeIter *parent);
static gboolean          gwy_inventory_store_iter_has_child (GtkTreeModel *model,
                                                             GtkTreeIter *iter);
static gint              gwy_inventory_store_iter_n_children(GtkTreeModel *model,
                                                             GtkTreeIter *iter);
static gboolean          gwy_inventory_store_iter_nth_child (GtkTreeModel *model,
                                                             GtkTreeIter *iter,
                                                             GtkTreeIter *parent,
                                                             gint n);
static gboolean          gwy_inventory_store_iter_parent    (GtkTreeModel *model,
                                                             GtkTreeIter *iter,
                                                             GtkTreeIter *child);
static void              inventory_item_updated             (GwyInventoryStore *store,
                                                             guint i,
                                                             GwyInventory *inventory);
static void              inventory_item_inserted            (GwyInventoryStore *store,
                                                             guint i,
                                                             GwyInventory *inventory);
static void              inventory_item_deleted             (GwyInventoryStore *store,
                                                             guint i);
static void              inventory_items_reordered          (GwyInventoryStore *store,
                                                             gint *new_order);
static void              set_inventory                      (GwyInventoryStore *store,
                                                             GwyInventory *inventory);
static gboolean          check_item                         (guint n,
                                                             gpointer item,
                                                             gpointer user_data);

static GParamSpec *properties[N_PROPS];

G_DEFINE_TYPE_EXTENDED
    (GwyInventoryStore, gwy_inventory_store, G_TYPE_INITIALLY_UNOWNED, 0,
     GWY_IMPLEMENT_TREE_MODEL(gwy_inventory_store_tree_model_init))

static void
gwy_inventory_store_class_init(GwyInventoryStoreClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    g_type_class_add_private(klass, sizeof(InventoryStore));

    gobject_class->finalize = gwy_inventory_store_finalize;
    gobject_class->dispose = gwy_inventory_store_dispose;
    gobject_class->get_property = gwy_inventory_store_get_property;
    gobject_class->set_property = gwy_inventory_store_set_property;

    properties[PROP_INVENTORY]
        = g_param_spec_object("inventory",
                              "Inventory",
                              "Inventory object the inventory store wraps.",
                              GWY_TYPE_INVENTORY,
                              G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY
                              | G_PARAM_STATIC_STRINGS);

    for (guint i = 1; i < N_PROPS; i++)
        g_object_class_install_property(gobject_class, i, properties[i]);
}

static void
gwy_inventory_store_tree_model_init(GtkTreeModelIface *iface)
{
    iface->get_flags = gwy_inventory_store_get_flags;
    iface->get_n_columns = gwy_inventory_store_get_n_columns;
    iface->get_column_type = gwy_inventory_store_get_column_type;
    iface->get_iter = gwy_inventory_store_get_tree_iter;
    iface->get_path = gwy_inventory_store_get_path;
    iface->get_value = gwy_inventory_store_get_value;
    iface->iter_next = gwy_inventory_store_iter_next;
    iface->iter_children = gwy_inventory_store_iter_children;
    iface->iter_has_child = gwy_inventory_store_iter_has_child;
    iface->iter_n_children = gwy_inventory_store_iter_n_children;
    iface->iter_nth_child = gwy_inventory_store_iter_nth_child;
    iface->iter_parent = gwy_inventory_store_iter_parent;
}

static void
gwy_inventory_store_init(GwyInventoryStore *store)
{
    store->priv = G_TYPE_INSTANCE_GET_PRIVATE(store, GWY_TYPE_INVENTORY_STORE,
                                              InventoryStore);
    store->priv->stamp = g_random_int();
}

static void
gwy_inventory_store_finalize(GObject *object)
{
    InventoryStore *priv = GWY_INVENTORY_STORE(object)->priv;
    GWY_OBJECT_UNREF(priv->inventory);
    G_OBJECT_CLASS(gwy_inventory_store_parent_class)->finalize(object);
}

static void
gwy_inventory_store_dispose(GObject *object)
{
    InventoryStore *priv = GWY_INVENTORY_STORE(object)->priv;
    GWY_SIGNAL_HANDLER_DISCONNECT(priv->inventory, priv->item_updated_id);
    GWY_SIGNAL_HANDLER_DISCONNECT(priv->inventory, priv->item_inserted_id);
    GWY_SIGNAL_HANDLER_DISCONNECT(priv->inventory, priv->item_deleted_id);
    GWY_SIGNAL_HANDLER_DISCONNECT(priv->inventory,
                                  priv->items_reordered_id);
    G_OBJECT_CLASS(gwy_inventory_store_parent_class)->dispose(object);
}

static void
gwy_inventory_store_set_property(GObject *object,
                                 guint prop_id,
                                 const GValue *value,
                                 GParamSpec *pspec)
{
    GwyInventoryStore *store = GWY_INVENTORY_STORE(object);

    switch (prop_id) {
        case PROP_INVENTORY:
        set_inventory(store, g_value_get_object(value));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_inventory_store_get_property(GObject *object,
                                 guint prop_id,
                                 GValue *value,
                                 GParamSpec *pspec)
{
    InventoryStore *priv = GWY_INVENTORY_STORE(object)->priv;

    switch (prop_id) {
        case PROP_INVENTORY:
        g_value_set_object(value, priv->inventory);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static GtkTreeModelFlags
gwy_inventory_store_get_flags(G_GNUC_UNUSED GtkTreeModel *model)
{
    return GTK_TREE_MODEL_ITERS_PERSIST | GTK_TREE_MODEL_LIST_ONLY;
}

static gint
gwy_inventory_store_get_n_columns(GtkTreeModel *model)
{
    InventoryStore *priv = GWY_INVENTORY_STORE(model)->priv;
    if (!priv->item_type->get_traits)
        return 1;

    guint n;
    priv->item_type->get_traits(&n);
    return n+1;    // +1 for the "item" column
}

static GType
gwy_inventory_store_get_column_type(GtkTreeModel *model,
                                    gint column)
{
    InventoryStore *priv = GWY_INVENTORY_STORE(model)->priv;
    // Zeroth column is "item"
    if (!column)
        return G_TYPE_POINTER;
    return priv->item_type->get_traits(NULL)[column-1];
}

static inline void
update_iter(InventoryStore *priv,
            GtkTreeIter *iter)
{
    if ((guint)iter->stamp == priv->stamp)
        return;

    const gchar *name = priv->item_type->get_name(iter->user_data);
    guint i = gwy_inventory_position(priv->inventory, name);
    iter->stamp = priv->stamp;
    iter->user_data2 = GUINT_TO_POINTER(i);
}

static gboolean
gwy_inventory_store_get_tree_iter(GtkTreeModel *model,
                                  GtkTreeIter *iter,
                                  GtkTreePath *path)
{
    g_return_val_if_fail(gtk_tree_path_get_depth(path) > 0, FALSE);
    InventoryStore *priv = GWY_INVENTORY_STORE(model)->priv;
    guint i = gtk_tree_path_get_indices(path)[0];
    if (i >= gwy_inventory_size(priv->inventory))
        return FALSE;

    /* GwyInventoryStore has presistent iters, because it uses item pointers
     * themselves as @user_data and an item pointer is valid as long as the
     * item exists.
     *
     * This always works but needs a round trip item -> name -> position ->
     * position+1 -> item to get next iter.  So we also store item position
     * in @user_data2 and use that directly if the inventory has not changed.
     * If it has changed, we can update it using the slower method.
     *
     * To sum it up:
     * @stamp: Corresponds to store's @stamp, but does not have to match.
     * @user_data: Direct pointer to item.
     * @user_data2: Position in inventory.
     */
    iter->stamp = priv->stamp;
    iter->user_data = gwy_inventory_get_nth(priv->inventory, i);
    iter->user_data2 = GUINT_TO_POINTER(i);

    return TRUE;
}

static GtkTreePath*
gwy_inventory_store_get_path(GtkTreeModel *model,
                             GtkTreeIter *iter)
{
    InventoryStore *priv = GWY_INVENTORY_STORE(model)->priv;
    update_iter(priv, iter);
    GtkTreePath *path = gtk_tree_path_new();
    gtk_tree_path_append_index(path, GPOINTER_TO_UINT(iter->user_data2));
    return path;
}

static void
gwy_inventory_store_get_value(GtkTreeModel *model,
                              GtkTreeIter *iter,
                              gint column,
                              GValue *value)
{
    InventoryStore *priv = GWY_INVENTORY_STORE(model)->priv;
    if (!column) {
        g_value_init(value, G_TYPE_POINTER);
        g_value_set_pointer(value, iter->user_data);
        return;
    }
    priv->item_type->get_trait_value(iter->user_data, column-1, value);
}

static gboolean
gwy_inventory_store_iter_next(GtkTreeModel *model,
                              GtkTreeIter *iter)
{
    InventoryStore *priv = GWY_INVENTORY_STORE(model)->priv;
    update_iter(priv, iter);
    guint i = GPOINTER_TO_UINT(iter->user_data2) + 1;
    iter->user_data = gwy_inventory_get_nth(priv->inventory, i);
    iter->user_data2 = GUINT_TO_POINTER(i);
    return iter->user_data != NULL;
}

static gboolean
gwy_inventory_store_iter_children(GtkTreeModel *model,
                                  GtkTreeIter *iter,
                                  GtkTreeIter *parent)
{
    if (parent)
        return FALSE;

    InventoryStore *priv = GWY_INVENTORY_STORE(model)->priv;
    if (!gwy_inventory_size(priv->inventory))
        return FALSE;

    iter->stamp = priv->stamp;
    iter->user_data = gwy_inventory_get_nth(priv->inventory, 0);
    iter->user_data2 = GUINT_TO_POINTER(0);
    return TRUE;
}

static gboolean
gwy_inventory_store_iter_has_child(G_GNUC_UNUSED GtkTreeModel *model,
                                   G_GNUC_UNUSED GtkTreeIter *iter)
{
    return FALSE;
}

static gint
gwy_inventory_store_iter_n_children(GtkTreeModel *model,
                                    GtkTreeIter *iter)
{
    if (iter)
        return 0;

    InventoryStore *priv = GWY_INVENTORY_STORE(model)->priv;
    return gwy_inventory_size(priv->inventory);
}

static gboolean
gwy_inventory_store_iter_nth_child(GtkTreeModel *model,
                                   GtkTreeIter *iter,
                                   GtkTreeIter *parent,
                                   gint n)
{
    if (parent)
        return FALSE;

    InventoryStore *priv = GWY_INVENTORY_STORE(model)->priv;
    if ((guint)n >= gwy_inventory_size(priv->inventory))
        return FALSE;

    iter->stamp = priv->stamp;
    iter->user_data = gwy_inventory_get_nth(priv->inventory, n);
    iter->user_data2 = GUINT_TO_POINTER(n);
    return TRUE;
}

static gboolean
gwy_inventory_store_iter_parent(G_GNUC_UNUSED GtkTreeModel *model,
                                G_GNUC_UNUSED GtkTreeIter *iter,
                                G_GNUC_UNUSED GtkTreeIter *child)
{
    return FALSE;
}

static void
inventory_item_updated(GwyInventoryStore *store,
                       guint i,
                       GwyInventory *inventory)
{
    GtkTreeIter iter = {
        .user_data = gwy_inventory_get_nth(inventory, i),
        .stamp = store->priv->stamp,
    };
    GtkTreePath *path = gtk_tree_path_new();
    gtk_tree_path_append_index(path, i);
    gtk_tree_model_row_changed(GTK_TREE_MODEL(store), path, &iter);
    gtk_tree_path_free(path);
}

static void
inventory_item_inserted(GwyInventoryStore *store,
                        guint i,
                        GwyInventory *inventory)
{
    InventoryStore *priv = store->priv;
    priv->stamp++;
    GtkTreeIter iter = {
        .stamp = priv->stamp,
        .user_data = gwy_inventory_get_nth(inventory, i),
        .user_data2 = GUINT_TO_POINTER(i),
    };
    GtkTreePath *path = gtk_tree_path_new();
    gtk_tree_path_append_index(path, i);
    gtk_tree_model_row_inserted(GTK_TREE_MODEL(store), path, &iter);
    gtk_tree_path_free(path);
}

static void
inventory_item_deleted(GwyInventoryStore *store,
                       guint i)
{
    store->priv->stamp++;
    GtkTreePath *path = gtk_tree_path_new();
    gtk_tree_path_append_index(path, i);
    gtk_tree_model_row_deleted(GTK_TREE_MODEL(store), path);
    gtk_tree_path_free(path);
}

static void
inventory_items_reordered(GwyInventoryStore *store,
                          gint *new_order)
{
    store->priv->stamp++;
    GtkTreePath *path = gtk_tree_path_new();
    gtk_tree_model_rows_reordered(GTK_TREE_MODEL(store), path, NULL, new_order);
    gtk_tree_path_free(path);
}

/**
 * gwy_inventory_store_new:
 * @inventory: (transfer full):
 *             An inventory.
 *
 * Creates a new tree model wrapper around an inventory.
 *
 * Returns: The newly created inventory store.
 **/
GwyInventoryStore*
gwy_inventory_store_new(GwyInventory *inventory)
{
    g_return_val_if_fail(GWY_IS_INVENTORY(inventory), NULL);
    return (GwyInventoryStore*)g_object_new(GWY_TYPE_INVENTORY_STORE,
                                            "inventory", inventory,
                                            NULL);
}

static void
set_inventory(GwyInventoryStore *store,
              GwyInventory *inventory)
{
    const GwyInventoryItemType *item_type
        = gwy_inventory_get_item_type(inventory);

    g_return_if_fail(item_type->get_name);
    g_return_if_fail(item_type->get_traits);
    g_return_if_fail(item_type->get_trait_value);

    g_object_ref(inventory);
    InventoryStore *priv = store->priv;
    priv->inventory = inventory;
    priv->item_type = item_type;

    priv->item_updated_id
        = g_signal_connect_swapped(inventory, "item-updated",
                                   G_CALLBACK(inventory_item_updated), store);
    priv->item_inserted_id
        = g_signal_connect(inventory, "item-inserted",
                           G_CALLBACK(inventory_item_inserted), store);
    priv->item_deleted_id
        = g_signal_connect(inventory, "item-deleted",
                           G_CALLBACK(inventory_item_deleted), store);
    priv->items_reordered_id
        = g_signal_connect(inventory, "items-reordered",
                         G_CALLBACK(inventory_items_reordered), store);
}

/**
 * gwy_inventory_store_get_inventory:
 * @store: An inventory store.
 *
 * Gets the inventory a inventory store wraps.
 *
 * Returns: (transfer none):
 *          The underlying inventory (its reference count is not increased).
 **/
GwyInventory*
gwy_inventory_store_get_inventory(const GwyInventoryStore *store)
{
    g_return_val_if_fail(GWY_IS_INVENTORY_STORE(store), NULL);
    return store->priv->inventory;
}

/**
 * gwy_inventory_store_find_column:
 * @store: An inventory store.
 * @name: Trait (column) name.
 *
 * Finds the tree model column corresponding to given trait name in a inventory
 * store.
 *
 * The underlying inventory must support trait names, except for @name
 * <literal>"item"</literal> which always works (and always maps to 0).
 *
 * Returns: The model column number if a trait called @name exists.
 *          A negative value is returned if no such column exists.
 **/
gint
gwy_inventory_store_find_column(const GwyInventoryStore *store,
                                const gchar *name)
{
    g_return_val_if_fail(GWY_IS_INVENTORY_STORE(store), -1);
    if (gwy_strequal(name, "item"))
        return 0;

    InventoryStore *priv = store->priv;
    const GwyInventoryItemType *item_type = priv->item_type;
    const gchar* (*method)(guint) = item_type->get_trait_name;
    g_return_val_if_fail(method, -1);

    guint n;
    item_type->get_traits(&n);
    for (guint i = 0; i < n; i++) {
        if (gwy_strequal(name, method(i)))
            return i+1;
    }
    return -1;
}

/**
 * gwy_inventory_store_get_iter:
 * @store: An inventory store.
 * @name: Item name.
 * @iter: Tree iterator to set to point to item named @name.
 *
 * Initializes a tree iterator to row the corresponding to a inventory item.
 *
 * Returns: %TRUE if @iter is valid, that is the item exists, %FALSE if @iter
 *          was not set.
 **/
gboolean
gwy_inventory_store_get_iter(const GwyInventoryStore *store,
                             const gchar *name,
                             GtkTreeIter *iter)
{
    g_return_val_if_fail(GWY_IS_INVENTORY_STORE(store), FALSE);
    g_return_val_if_fail(iter, FALSE);

    InventoryStore *priv = store->priv;
    guint i = gwy_inventory_position(priv->inventory, name);
    if (i == G_MAXUINT)
        return FALSE;

    iter->stamp = priv->stamp;
    iter->user_data = gwy_inventory_get_nth(priv->inventory, i);
    g_assert(iter->user_data);
    iter->user_data2 = GUINT_TO_POINTER(i);
    return TRUE;
}

/**
 * gwy_inventory_store_iter_is_valid:
 * @store: An inventory store.
 * @iter: Tree iterator to check.
 *
 * Checks if the given iter is a valid iter for this inventory store.
 *
 * <warning>This function is slow. Only use it for debugging and/or testing
 * purposes.</warning>
 *
 * Returns: %TRUE if the iter is valid, %FALSE if the iter is invalid.
 **/
gboolean
gwy_inventory_store_iter_is_valid(const GwyInventoryStore *store,
                                  GtkTreeIter *iter)
{
    g_return_val_if_fail(GWY_IS_INVENTORY_STORE(store), FALSE);
    if (!iter || !iter->user_data)
        return FALSE;

    /* Make a copy because we use the iter as a scratch pad */
    GtkTreeIter copy = *iter;
    InventoryStore *priv = store->priv;
    if (!gwy_inventory_find(priv->inventory, check_item, &copy))
        return FALSE;

    /* Iters with different stamps are valid if just @user_data matches item
     * pointer.  But if stamps match, @user_data2 must match item position */
    return ((guint)iter->stamp != priv->stamp
            || iter->user_data2 == copy.user_data2);
}

static gboolean
check_item(guint n,
           gpointer item,
           gpointer user_data)
{
    GtkTreeIter *iter = (GtkTreeIter*)user_data;

    if (iter->user_data != item)
        return FALSE;

    iter->user_data2 = GUINT_TO_POINTER(n);
    return TRUE;
}

/**
 * SECTION: inventory-store
 * @title: GwyInventoryStore
 * @short_description: Tree model wrapper of #GwyInventory.
 *
 * #GwyInventoryStore is a simple adaptor class that wraps #GwyInventory in
 * #GtkTreeModel interface.  It is list-only and has persistent iterators.  It
 * offers no methods to manipulate items, this should be done on the underlying
 * inventory.
 *
 * #GwyInventoryStore maps inventory item traits to virtual #GtkTreeModel
 * columns.  The zeroth column is always of type %G_TYPE_POINTER and contains
 * the item itself.  It exists automatically, even when the underlying does not
 * provide any traits functions.  Columns from 1 onward are formed by item
 * traits.  You can obtain column id of a named item trait with
 * gwy_inventory_store_find_column().
 **/

/**
 * GwyInventoryStore:
 *
 * Object representing a tree model wrapper of an inventory.
 *
 * The #GwyInventoryStore struct contains private data only and should be
 * accessed using the functions below.
 **/

/**
 * GwyInventoryStoreClass:
 *
 * Class of inventory tree model wrappers.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
