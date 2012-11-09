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

#include <glib/gi18n-lib.h>
#include "libgwy/object-utils.h"
#include "libgwyui/resource-list.h"

enum {
    PROP_0,
    PROP_STORE,
    PROP_ACTIVE,
    PROP_ONLY_PREFERRED,
    N_PROPS,
};

struct _GwyResourceListPrivate {
    GwyInventoryStore *store;
    GtkTreeModelFilter *filter;
    GwyResource *active_resource;
    gulong resource_notify_id;
    gboolean only_preferred;
    guint height;
};

typedef struct _GwyResourceListPrivate ResourceList;

static void     gwy_resource_list_constructed (GObject *object);
static void     gwy_resource_list_dispose     (GObject *object);
static void     gwy_resource_list_finalize    (GObject *object);
static void     gwy_resource_list_set_property(GObject *object,
                                               guint prop_id,
                                               const GValue *value,
                                               GParamSpec *pspec);
static void     gwy_resource_list_get_property(GObject *object,
                                               guint prop_id,
                                               GValue *value,
                                               GParamSpec *pspec);
static void     render_name                   (GtkTreeViewColumn *column,
                                               GtkCellRenderer *renderer,
                                               GtkTreeModel *model,
                                               GtkTreeIter *iter,
                                               gpointer user_data);
static void     selection_changed             (GwyResourceList *list,
                                               GtkTreeSelection *selection);
static void     set_active_resource           (GwyResourceList *list,
                                               GwyResource *resource);
static void     resource_notify               (GwyResourceList *list,
                                               GParamSpec *param,
                                               GwyResource *resource);
static gboolean set_only_preferred            (GwyResourceList *list,
                                               gboolean setting);
static gboolean resource_is_visible           (GtkTreeModel *model,
                                               GtkTreeIter *iter,
                                               gpointer user_data);
static void     preferred_toggled             (GwyResourceList *list,
                                               const gchar *stringpath);

static GParamSpec *properties[N_PROPS];

G_DEFINE_TYPE(GwyResourceList, gwy_resource_list, GTK_TYPE_TREE_VIEW);

static void
gwy_resource_list_class_init(GwyResourceListClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    g_type_class_add_private(klass, sizeof(ResourceList));

    gobject_class->dispose = gwy_resource_list_dispose;
    gobject_class->finalize = gwy_resource_list_finalize;
    gobject_class->constructed = gwy_resource_list_constructed;
    gobject_class->get_property = gwy_resource_list_get_property;
    gobject_class->set_property = gwy_resource_list_set_property;

    properties[PROP_STORE]
        = g_param_spec_object("store",
                              "Store",
                              "Inventory store of the resource list.  It may "
                              "differ from the tree model because of "
                              "filtering.",
                              GWY_TYPE_INVENTORY_STORE,
                              G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    properties[PROP_ACTIVE]
        = g_param_spec_string("active",
                              "Active",
                              "Name of the currently active (selected) "
                              "resource.  An attempt to set this property "
                              "to a name not corresponding to any resource "
                              "leaves it unchanged.",
                              NULL,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_ONLY_PREFERRED]
        = g_param_spec_boolean("only-preferred",
                               "Only preferred",
                               "Whether to show only preferred resources. "
                               "Settings this to %TRUE is usually meaningful "
                               "only for lists that do not have the "
                               "‘preferred’ column.",
                               FALSE,
                               G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    for (guint i = 1; i < N_PROPS; i++)
        g_object_class_install_property(gobject_class, i, properties[i]);
}

static void
gwy_resource_list_init(GwyResourceList *list)
{
    list->priv = G_TYPE_INSTANCE_GET_PRIVATE(list, GWY_TYPE_RESOURCE_LIST,
                                             ResourceList);
    ResourceList *priv = list->priv;
    gint w, h;
    gtk_icon_size_lookup(GTK_ICON_SIZE_SMALL_TOOLBAR, &w, &h);
    priv->height = h;
    gtk_tree_view_set_fixed_height_mode(GTK_TREE_VIEW(list), TRUE);
}

static void
gwy_resource_list_constructed(GObject *object)
{
    G_OBJECT_CLASS(gwy_resource_list_parent_class)->constructed(object);

    GwyResourceList *list = GWY_RESOURCE_LIST(object);
    ResourceList *priv = list->priv;
    GType type = GWY_RESOURCE_LIST_GET_CLASS(object)->resource_type;
    g_assert(g_type_is_a(type, GWY_TYPE_RESOURCE));
    GwyInventory *inventory = gwy_resource_type_get_inventory(type);
    priv->store = gwy_inventory_store_new(inventory);
    GtkTreeModel *model = GTK_TREE_MODEL(priv->store);
    priv->filter = GTK_TREE_MODEL_FILTER(gtk_tree_model_filter_new(model,
                                                                   NULL));
    gtk_tree_model_filter_set_visible_func(priv->filter,
                                           resource_is_visible, list, NULL);
    GtkTreeView *treeview = GTK_TREE_VIEW(object);
    gtk_tree_view_set_model(treeview, model);
    GtkTreeSelection *selection = gtk_tree_view_get_selection(treeview);
    g_signal_connect_swapped(selection, "changed",
                             G_CALLBACK(selection_changed), list);
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_BROWSE);
}

static void
gwy_resource_list_dispose(GObject *object)
{
    G_OBJECT_CLASS(gwy_resource_list_parent_class)->dispose(object);
}

static void
gwy_resource_list_finalize(GObject *object)
{
    ResourceList *priv = GWY_RESOURCE_LIST(object)->priv;
    GWY_OBJECT_UNREF(priv->filter);
    GWY_OBJECT_UNREF(priv->store);
    G_OBJECT_CLASS(gwy_resource_list_parent_class)->finalize(object);
}

static void
gwy_resource_list_set_property(GObject *object,
                               guint prop_id,
                               const GValue *value,
                               GParamSpec *pspec)
{
    GwyResourceList *list = GWY_RESOURCE_LIST(object);

    switch (prop_id) {
        case PROP_ONLY_PREFERRED:
        set_only_preferred(list, g_value_get_boolean(value));
        break;

        case PROP_ACTIVE:
        gwy_resource_list_set_active(list, g_value_get_string(value));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_resource_list_get_property(GObject *object,
                               guint prop_id,
                               GValue *value,
                               GParamSpec *pspec)
{
    GwyResourceList *list = GWY_RESOURCE_LIST(object);
    ResourceList *priv = list->priv;

    switch (prop_id) {
        case PROP_STORE:
        g_value_set_object(value, priv->store);
        break;

        case PROP_ONLY_PREFERRED:
        g_value_set_boolean(value, priv->only_preferred);
        break;

        case PROP_ACTIVE:
        g_value_set_string(value, gwy_resource_list_get_active(list));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

/**
 * gwy_resource_list_get_store:
 * @list: A resource list.
 *
 * Gets the inventory store that a resource list displays.
 *
 * Returns: (transfer none):
 *          Inventory store displayed by @list.
 **/
GwyInventoryStore*
gwy_resource_list_get_store(const GwyResourceList *list)
{
    g_return_val_if_fail(GWY_IS_RESOURCE_LIST(list), NULL);
    return list->priv->store;
}

/**
 * gwy_resource_list_set_active:
 * @list: A resource list.
 * @name: (allow-none):
 *        Name of resource to make active.
 *
 * Sets the name of resource currently selected in a resource list.
 *
 * If you request a name that does not exist, nothing is set and the function
 * returns %FALSE.
 *
 * Returns: %TRUE if @name corresponded to the name of a valid resource.
 *          Not necessarily whether the active resource has changed because the
 *          corresponding resource might have been already selected.
 **/
gboolean
gwy_resource_list_set_active(GwyResourceList *list,
                             const gchar *name)
{
    g_return_val_if_fail(GWY_IS_RESOURCE_LIST(list), FALSE);
    ResourceList *priv = list->priv;
    GtkTreeIter iter, filter_iter;
    if (!gwy_inventory_store_get_iter(priv->store, name, &iter))
        return FALSE;
    if (!name && !priv->active_resource)
        return FALSE;
    if (name
        && priv->active_resource
        && gwy_strequal(name, gwy_resource_get_name(priv->active_resource)))
        return TRUE;

    GtkTreeView *treeview = GTK_TREE_VIEW(list);
    gtk_tree_model_filter_convert_child_iter_to_iter(priv->filter,
                                                     &filter_iter, &iter);
    GtkTreeSelection *selection = gtk_tree_view_get_selection(treeview);
    gtk_tree_selection_select_iter(selection, &filter_iter);
    return TRUE;
}

/**
 * gwy_resource_list_get_active:
 * @list: A resource list.
 *
 * Gets the name of resource currently selected in a resource list.
 *
 * If you change the selection mode to %GTK_SELECTION_MULTIPLE this function
 * attempts to do something meaningful by returning the resource under cursor.
 *
 * Returns: (allow-none):
 *          Name of the currently selected resource.  On rare occassions, it
 *          may return %NULL which means no resource is selected.
 **/
const gchar*
gwy_resource_list_get_active(const GwyResourceList *list)
{
    g_return_val_if_fail(GWY_IS_RESOURCE_LIST(list), NULL);
    ResourceList *priv = list->priv;
    if (priv->active_resource)
        return gwy_resource_get_name(priv->active_resource);
    return NULL;
}

/**
 * gwy_resource_list_append_column_name:
 * @list: A resource list.
 *
 * Appends a standard column with resource names to a resource list.
 **/
void
gwy_resource_list_append_column_name(GwyResourceList *list)
{
    g_return_if_fail(GWY_IS_RESOURCE_LIST(list));
    ResourceList *priv = list->priv;

    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    gtk_cell_renderer_set_fixed_size(renderer, -1, priv->height);
    gint i = gwy_inventory_store_find_column(priv->store, "name");
    g_assert(i > 0);
    GtkTreeViewColumn *column
        = gtk_tree_view_column_new_with_attributes(_("Name"), renderer,
                                                   "text", i,
                                                   NULL);
    GwyInventory *inventory = gwy_inventory_store_get_inventory(priv->store);
    gtk_tree_view_column_set_cell_data_func(column, renderer, render_name,
                                            inventory, NULL);
    g_object_set(renderer, "weight-set", TRUE, NULL);
    GtkTreeView *treeview = GTK_TREE_VIEW(list);
    gtk_tree_view_append_column(treeview, column);

    gtk_tree_view_set_search_column(treeview, i);
    gtk_tree_view_set_enable_search(treeview, TRUE);
}

/**
 * gwy_resource_list_append_column_preferred:
 * @list: A resource list.
 *
 * Appends a standard column with ‘preferred’ checkboxes to a resource list.
 **/
void
gwy_resource_list_append_column_preferred(GwyResourceList *list)
{
    g_return_if_fail(GWY_IS_RESOURCE_LIST(list));
    ResourceList *priv = list->priv;

    GtkCellRenderer *renderer = gtk_cell_renderer_toggle_new();
    gtk_cell_renderer_set_fixed_size(renderer, -1, priv->height);
    gint i = gwy_inventory_store_find_column(priv->store, "preferred");
    g_assert(i > 0);
    const gchar *title = C_("resource-property", "Preferred");
    GtkTreeViewColumn *column
        = gtk_tree_view_column_new_with_attributes(title, renderer,
                                                   "active", i,
                                                   NULL);
    g_object_set(renderer, "activatable", TRUE, NULL);
    g_signal_connect_swapped(renderer, "toggled",
                             G_CALLBACK(preferred_toggled), list);
    gtk_tree_view_append_column(GTK_TREE_VIEW(list), column);
}

/**
 * gwy_resource_list_set_only_preferred:
 * @list: A resource list.
 * @onlypreferred: %TRUE to display only preferred resources, %FALSE to display
 *                 all.
 *
 * Sets whether only preferred resources are displayed in a resource list.
 **/
void
gwy_resource_list_set_only_preferred(GwyResourceList *list,
                                     gboolean onlypreferred)
{
    g_return_if_fail(GWY_IS_RESOURCE_LIST(list));
    if (!set_only_preferred(list, onlypreferred))
        return;

    g_object_notify_by_pspec(G_OBJECT(list), properties[PROP_ONLY_PREFERRED]);
}

/**
 * gwy_resource_list_get_only_preferred:
 * @list: A resource list.
 *
 * Gets whether only preferred resources are displayed in a resource list.
 *
 * Returns: %TRUE if only preferred resources are displayed, %FALSE if all are
 *          displayed.
 **/
gboolean
gwy_resource_list_get_only_preferred(GwyResourceList *list)
{
    g_return_val_if_fail(GWY_IS_RESOURCE_LIST(list), FALSE);
    return list->priv->only_preferred;
}

static void
render_name(G_GNUC_UNUSED GtkTreeViewColumn *column,
            GtkCellRenderer *renderer,
            GtkTreeModel *model,
            GtkTreeIter *iter,
            gpointer user_data)
{
    GwyInventory *inventory = (GwyInventory*)user_data;
    gpointer item;
    gtk_tree_model_get(model, iter, 0, &item, -1);
    gpointer defitem = gwy_inventory_get_default(inventory);
    PangoWeight w = (item == defitem) ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL;
    g_object_set(renderer, "weight", w, NULL);
}

static void
selection_changed(GwyResourceList *list,
                  GtkTreeSelection *selection)
{
    GtkTreeView *treeview = GTK_TREE_VIEW(list);
    GtkSelectionMode mode = gtk_tree_selection_get_mode(selection);
    GtkTreeModel *model = gtk_tree_view_get_model(treeview);
    GtkTreeIter iter;
    GwyResource *resource = NULL;

    if (mode == GTK_SELECTION_SINGLE
        || mode == GTK_SELECTION_BROWSE
        || mode == GTK_SELECTION_NONE) {
        if (gtk_tree_selection_get_selected(selection, NULL, &iter))
            gtk_tree_model_get(model, &iter, 0, &resource, -1);
    }
    else {
        GtkTreePath *path;
        gtk_tree_view_get_cursor(treeview, &path, NULL);
        if (path) {
            gtk_tree_model_get_iter(model, &iter, path);
            gtk_tree_model_get(model, &iter, 0, &resource, -1);
            gtk_tree_path_free(path);
        }
    }
    set_active_resource(list, resource);
}

static void
set_active_resource(GwyResourceList *list,
                    GwyResource *resource)
{
    GType type = GWY_RESOURCE_LIST_GET_CLASS(list)->resource_type;
    ResourceList *priv = list->priv;
    if (!gwy_set_member_object(list, resource, type,
                               &priv->active_resource,
                               "notify::name", &resource_notify,
                               &priv->resource_notify_id,
                               G_CONNECT_SWAPPED,
                               NULL))
        return;

    g_object_notify_by_pspec(G_OBJECT(list), properties[PROP_ACTIVE]);
}

static void
resource_notify(GwyResourceList *list,
                GParamSpec *param,
                G_GNUC_UNUSED GwyResource *resource)
{
    if (gwy_strequal(param->name, "name")) {
        g_object_notify_by_pspec(G_OBJECT(list), properties[PROP_ACTIVE]);
    }
}

static gboolean
set_only_preferred(GwyResourceList *list,
                   gboolean setting)
{
    ResourceList *priv = list->priv;
    if (!setting == !priv->only_preferred)
        return FALSE;

    priv->only_preferred = setting;
    gtk_tree_model_filter_refilter(priv->filter);
    return TRUE;
}

static gboolean
resource_is_visible(GtkTreeModel *model,
                    GtkTreeIter *iter,
                    gpointer user_data)
{
    GwyResourceList *list = (GwyResourceList*)user_data;
    ResourceList *priv = list->priv;
    if (!priv->only_preferred)
        return TRUE;

    GwyResource *resource;
    gtk_tree_model_get(model, iter, 0, &resource, -1);
    return gwy_resource_get_preferred(resource);
}

static void
preferred_toggled(GwyResourceList *list,
                  const gchar *stringpath)
{
    ResourceList *priv = list->priv;
    GwyInventory *inventory = gwy_inventory_store_get_inventory(priv->store);
    GtkTreePath *path = gtk_tree_path_new_from_string(stringpath);
    GtkTreeModel *model = GTK_TREE_MODEL(priv->filter);
    GwyResource *resource;
    GtkTreeIter iter;
    gtk_tree_model_get_iter(model, &iter, path);
    gtk_tree_model_get(model, &iter, 0, &resource, -1);
    gboolean preferred = gwy_resource_get_preferred(resource);
    gwy_resource_set_preferred(resource, !preferred);
    gint i = gtk_tree_path_get_indices(path)[0];
    gtk_tree_path_free(path);
    gwy_inventory_nth_updated(inventory, i);
}

/**
 * SECTION: resource-list
 * @title: GwyResourceList
 * @short_description: Base class for resource list views
 **/

/**
 * GwyResourceList:
 *
 * Base class for list views displaing lists of resources.
 *
 * The #GwyResourceList struct contains private data only and should be
 * accessed using the functions below.
 **/

/**
 * GwyResourceListClass:
 * @resource_type: Resource type.  It must be filled by instantiatable
 *                 subclasses as it is used to obtain the right resource
 *                 inventory for the model.
 *
 * Class of resource list views.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
