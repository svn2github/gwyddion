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
#include "libgwyui/list-selection-binding.h"

#define BINDING_KEY "gwy-list-selection-binding"

enum {
    PROP_0,
    PROP_INT_SET,
    N_PROPS,
};

typedef struct {
    GtkTreeSelection *selection;
    gulong changed_id;
} SelectionInfo;

struct _GwyListSelectionBindingPrivate {
    GwyIntSet *intset;
    gulong assigned_id;
    gulong added_id;
    gulong removed_id;

    GArray *selections;

    GwyIntSet *toadd_set;
    GwyIntSet *toremove_set;
    GPtrArray *toadd_paths;
    GPtrArray *toremove_paths;

    GtkTreePath *path;
    GtkTreeSelection *source_selection;
};

typedef void (*ListSelectionFunc)(SelectionInfo *info,
                                  GwyListSelectionBinding *binding);

typedef struct _GwyListSelectionBindingPrivate ListSelectionBinding;

static void gwy_list_selection_binding_finalize    (GObject *object);
static void gwy_list_selection_binding_dispose     (GObject *object);
static void gwy_list_selection_binding_set_property(GObject *object,
                                                    guint prop_id,
                                                    const GValue *value,
                                                    GParamSpec *pspec);
static void gwy_list_selection_binding_get_property(GObject *object,
                                                    guint prop_id,
                                                    GValue *value,
                                                    GParamSpec *pspec);
static void set_intset                             (GwyListSelectionBinding *binding,
                                                    GwyIntSet *intset);
static void intset_weak_notify                     (gpointer user_data,
                                                    GObject *where_the_object_was);
static void selection_weak_notify                  (gpointer user_data,
                                                    GObject *where_the_object_was);
static void unbind_all                             (GwyListSelectionBinding *binding);
static void unbind_selection                       (GwyListSelectionBinding *binding,
                                                    SelectionInfo *info);
static void block_selection                        (SelectionInfo *info,
                                                    GwyListSelectionBinding *binding);
static void unblock_selection                      (SelectionInfo *info,
                                                    GwyListSelectionBinding *binding);
static void do_with_all_selections                 (ListSelectionFunc func,
                                                    GwyListSelectionBinding *binding);
static void select_path                            (SelectionInfo *info,
                                                    GwyListSelectionBinding *binding);
static void intset_added_sync_to_selections        (GwyListSelectionBinding *binding,
                                                    gint value);
static void unselect_path                          (SelectionInfo *info,
                                                    GwyListSelectionBinding *binding);
static void intset_removed_sync_to_selections      (GwyListSelectionBinding *binding,
                                                    gint value);
static void sync_selection                         (SelectionInfo *info,
                                                    GwyListSelectionBinding *binding);
static void sync_intset_to_selections              (GwyListSelectionBinding *binding);
static void sync_selection_to_intset               (GwyListSelectionBinding *binding,
                                                    GtkTreeSelection *selection);
static void block_intset                           (GwyListSelectionBinding *binding);
static void unblock_intset                         (GwyListSelectionBinding *binding);

static GParamSpec *properties[N_PROPS];
static GQuark binding_quark = 0;

G_DEFINE_TYPE_WITH_CODE(GwyListSelectionBinding, gwy_list_selection_binding,
                        G_TYPE_OBJECT,
                        binding_quark = g_quark_from_static_string(BINDING_KEY););

static void
gwy_list_selection_binding_class_init(GwyListSelectionBindingClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    g_type_class_add_private(klass, sizeof(ListSelectionBinding));

    gobject_class->dispose = gwy_list_selection_binding_dispose;
    gobject_class->finalize = gwy_list_selection_binding_finalize;
    gobject_class->get_property = gwy_list_selection_binding_get_property;
    gobject_class->set_property = gwy_list_selection_binding_set_property;

    properties[PROP_INT_SET]
        = g_param_spec_object("int-set",
                              "Int set",
                              "Integer set representing the selection.",
                              GWY_TYPE_INT_SET,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS
                              | G_PARAM_CONSTRUCT_ONLY);

    for (guint i = 1; i < N_PROPS; i++)
        g_object_class_install_property(gobject_class, i, properties[i]);
}

static void
gwy_list_selection_binding_init(GwyListSelectionBinding *binding)
{
    binding->priv = G_TYPE_INSTANCE_GET_PRIVATE(binding,
                                                GWY_TYPE_LIST_SELECTION_BINDING,
                                                ListSelectionBinding);
    ListSelectionBinding *priv = binding->priv;
    priv->selections = g_array_new(FALSE, FALSE, sizeof(SelectionInfo));
}

static void
gwy_list_selection_binding_finalize(GObject *object)
{
    ListSelectionBinding *priv = GWY_LIST_SELECTION_BINDING(object)->priv;
    g_array_free(priv->selections, TRUE);
    GWY_OBJECT_UNREF(priv->toadd_set);
    GWY_OBJECT_UNREF(priv->toremove_set);
    GWY_PTR_ARRAY_FREE(priv->toadd_paths);
    GWY_PTR_ARRAY_FREE(priv->toremove_paths);
    G_OBJECT_CLASS(gwy_list_selection_binding_parent_class)->finalize(object);
}

static void
gwy_list_selection_binding_dispose(GObject *object)
{
    GwyListSelectionBinding *binding = GWY_LIST_SELECTION_BINDING(object);
    unbind_all(binding);
    G_OBJECT_CLASS(gwy_list_selection_binding_parent_class)->dispose(object);
}

static void
gwy_list_selection_binding_set_property(GObject *object,
                                        guint prop_id,
                                        const GValue *value,
                                        GParamSpec *pspec)
{
    GwyListSelectionBinding *binding = GWY_LIST_SELECTION_BINDING(object);

    switch (prop_id) {
        case PROP_INT_SET:
        set_intset(binding, g_value_get_object(value));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_list_selection_binding_get_property(GObject *object,
                             guint prop_id,
                             GValue *value,
                             GParamSpec *pspec)
{
    ListSelectionBinding *priv = GWY_LIST_SELECTION_BINDING(object)->priv;

    switch (prop_id) {
        case PROP_INT_SET:
        g_value_set_object(value, priv->intset);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

/**
 * gwy_list_selection_binding_new:
 * @intset: Set of integer indices representing the selection.
 *
 * Creates a binding between a list tree view selection and set of integers.
 *
 * Returns: (transfer none):
 *          A new list selection binding.
 **/
GwyListSelectionBinding*
gwy_list_selection_binding_new(GwyIntSet *intset)
{
    g_assert(GWY_IS_INT_SET(intset));
    return g_object_new(GWY_TYPE_LIST_SELECTION_BINDING,
                        "int-set", intset,
                        NULL);
}

/**
 * gwy_list_selection_binding_add:
 * @binding: A binding between list tree selections and an integer set.
 * @selection: A list tree selection.
 *
 * Adds a tree view selection to the binding.
 **/
void
gwy_list_selection_binding_add(GwyListSelectionBinding *binding,
                               GtkTreeSelection *selection)
{
    g_return_if_fail(GWY_IS_LIST_SELECTION_BINDING(binding));
    g_return_if_fail(GTK_IS_TREE_SELECTION(selection));
    GtkTreeView *treeview = gtk_tree_selection_get_tree_view(selection);
    GtkTreeModel *model = gtk_tree_view_get_model(treeview);
    GtkTreeModelFlags flags = gtk_tree_model_get_flags(model);
    g_return_if_fail(flags & GTK_TREE_MODEL_LIST_ONLY);

    gpointer otherbinding;
    if ((otherbinding = g_object_get_qdata(G_OBJECT(selection),
                                           binding_quark))) {
        g_warning("GtkTreeSelection %p is already bound with binding %p.",
                  selection, otherbinding);
        return;
    }

    ListSelectionBinding *priv = binding->priv;
    g_object_set_qdata(G_OBJECT(selection), binding_quark, binding);
    g_object_weak_ref(G_OBJECT(selection), selection_weak_notify, binding);

    SelectionInfo info = { .selection = selection, .changed_id = 0 };
    sync_selection(&info, binding);
    guint id = g_signal_connect_swapped(selection, "changed",
                                        G_CALLBACK(sync_selection_to_intset),
                                        binding);
    info.changed_id = id;
    g_array_append_val(priv->selections, info);
}

/**
 * gwy_list_selection_binding_remove:
 * @binding: A binding between list tree selections and an integer set.
 * @selection: A list tree selection.
 *
 * Removes a tree view selection from the binding.
 *
 * The tree selection is not destroyed, it will be just no longer synchronised
 * with the integer set.
 **/
void
gwy_list_selection_binding_remove(GwyListSelectionBinding *binding,
                                  GtkTreeSelection *selection)
{
    g_return_if_fail(GWY_IS_LIST_SELECTION_BINDING(binding));
    g_return_if_fail(GTK_IS_TREE_SELECTION(selection));

}

/**
 * gwy_list_selection_binding_unbind:
 * @binding: A binding between list tree selections and an integer set.
 *
 * Unbinds all tree selections from the integer set.
 *
 * This explicit unbinding function is namely useful for garbage-collected
 * languages where the destruction of the binding may be delayed.
 **/
void
gwy_list_selection_binding_unbind(GwyListSelectionBinding *binding)
{
    g_return_if_fail(GWY_IS_LIST_SELECTION_BINDING(binding));

    unbind_all(binding);
    g_object_unref(binding);
}

static void
set_intset(GwyListSelectionBinding *binding,
           GwyIntSet *intset)
{
    ListSelectionBinding *priv = binding->priv;
    if (intset == priv->intset)
        return;

    gpointer otherbinding;
    if ((otherbinding = g_object_get_qdata(G_OBJECT(intset), binding_quark))) {
        g_warning("GwyIntSet %p is already bound with binding %p.",
                  intset, otherbinding);
        return;
    }

    g_return_if_fail(!priv->intset);
    priv->intset = intset;
    g_object_set_qdata(G_OBJECT(priv->intset), binding_quark, binding);
    g_object_weak_ref(G_OBJECT(intset), intset_weak_notify, binding);

    priv->assigned_id
        = g_signal_connect_swapped(intset, "assigned",
                                   G_CALLBACK(sync_intset_to_selections),
                                   binding);
    priv->added_id
        = g_signal_connect_swapped(intset, "added",
                                   G_CALLBACK(intset_added_sync_to_selections),
                                   binding);
    priv->removed_id
        = g_signal_connect_swapped(intset, "removed",
                                   G_CALLBACK(intset_removed_sync_to_selections),
                                   binding);
}

static void
intset_weak_notify(gpointer user_data,
                   G_GNUC_UNUSED GObject *where_the_object_was)
{
    GwyListSelectionBinding *binding = (GwyListSelectionBinding*)user_data;
    ListSelectionBinding *priv = binding->priv;
    priv->intset = NULL;
    priv->removed_id = priv->added_id = priv->assigned_id = 0;
    // This cleans up the selections if any.
    g_object_unref(binding);
}

static void
selection_weak_notify(gpointer user_data,
                      GObject *where_the_object_was)
{
    GwyListSelectionBinding *binding = (GwyListSelectionBinding*)user_data;
    ListSelectionBinding *priv = binding->priv;
    for (guint i = 0; i < priv->selections->len; i++) {
        SelectionInfo *info = &g_array_index(priv->selections,
                                             SelectionInfo, i);
        if (info->selection == (GtkTreeSelection*)where_the_object_was) {
            g_array_remove_index(priv->selections, i);
            break;
        }
    }
}

static void
unbind_all(GwyListSelectionBinding *binding)
{
    ListSelectionBinding *priv = binding->priv;

    if (priv->intset) {
        g_object_weak_unref(G_OBJECT(priv->intset),
                            intset_weak_notify, binding);
        g_signal_handler_disconnect(priv->intset, priv->assigned_id);
        g_signal_handler_disconnect(priv->intset, priv->added_id);
        g_signal_handler_disconnect(priv->intset, priv->removed_id);
        g_object_set_qdata(G_OBJECT(priv->intset), binding_quark, NULL);
        priv->intset = NULL;
    }

    while (priv->selections->len) {
        SelectionInfo *info = &g_array_index(priv->selections,
                                             SelectionInfo,
                                             priv->selections->len-1);
        unbind_selection(binding, info);
        g_array_set_size(priv->selections, priv->selections->len-1);
    }
}

static void
unbind_selection(GwyListSelectionBinding *binding,
                 SelectionInfo *info)
{
    g_object_weak_unref(G_OBJECT(info->selection),
                        selection_weak_notify, binding);
    g_signal_handler_disconnect(info->selection, info->changed_id);
    g_object_set_qdata(G_OBJECT(info->selection), binding_quark, NULL);
    info->changed_id = 0;
    info->selection = NULL;
}

static void
block_selection(SelectionInfo *info,
                G_GNUC_UNUSED GwyListSelectionBinding *binding)
{
    g_signal_handler_block(info->selection, info->changed_id);
}

static void
unblock_selection(SelectionInfo *info,
                  G_GNUC_UNUSED GwyListSelectionBinding *binding)
{
    g_signal_handler_unblock(info->selection, info->changed_id);
}

static void
do_with_all_selections(ListSelectionFunc func,
                       GwyListSelectionBinding *binding)
{
    ListSelectionBinding *priv = binding->priv;
    GArray *selections = priv->selections;
    guint n = selections->len;

    for (guint i = 0; i < n; i++) {
        SelectionInfo *info = &g_array_index(selections, SelectionInfo, i);
        func(info, binding);
    }
}

static void
select_path(SelectionInfo *info, GwyListSelectionBinding *binding)
{
    gtk_tree_selection_select_path(info->selection, binding->priv->path);
}

static void
intset_added_sync_to_selections(GwyListSelectionBinding *binding,
                                gint value)
{
    do_with_all_selections(block_selection, binding);
    binding->priv->path = gtk_tree_path_new_from_indices(value, -1);
    do_with_all_selections(select_path, binding);
    gtk_tree_path_free(binding->priv->path);
    binding->priv->path = NULL;
    do_with_all_selections(unblock_selection, binding);
}

static void
unselect_path(SelectionInfo *info, GwyListSelectionBinding *binding)
{
    gtk_tree_selection_unselect_path(info->selection, binding->priv->path);
}

static void
intset_removed_sync_to_selections(GwyListSelectionBinding *binding,
                                  gint value)
{
    do_with_all_selections(block_selection, binding);
    binding->priv->path = gtk_tree_path_new_from_indices(value, -1);
    do_with_all_selections(unselect_path, binding);
    gtk_tree_path_free(binding->priv->path);
    binding->priv->path = NULL;
    do_with_all_selections(unblock_selection, binding);
}

static void
gather_row_indices(G_GNUC_UNUSED GtkTreeModel *model,
                   GtkTreePath *path,
                   G_GNUC_UNUSED GtkTreeIter *iter,
                   gpointer user_data)
{
    gint **selected = (gint**)user_data;
    gint *indices = gtk_tree_path_get_indices(path);
    **selected = indices[0];
    (*selected)++;
}

// XXX: This is ugly because GtkTreeSelection does not have efficient methods
// to obtain the rows.
static void
transform_selection_to_intset(GtkTreeSelection *selection,
                              GwyIntSet *intset)
{
    guint n = gtk_tree_selection_count_selected_rows(selection);
    if (!n) {
        gwy_int_set_fill(intset, NULL, 0);
        return;
    }

    gint *selected = g_new(gint, n);
    gint *s = selected;
    gtk_tree_selection_selected_foreach(selection, gather_row_indices, &s);
    g_assert(s - selected == n);
    gwy_int_set_fill(intset, selected, n);
    g_free(selected);
}

static void
transform_intset_to_path_ranges(GwyIntSet *intset,
                                GPtrArray *paths)
{
    g_ptr_array_set_size(paths, 0);

    guint n;
    const GwyIntRange *ranges = gwy_int_set_ranges(intset, &n);
    for (guint i = 0; i < n; i++) {
        GtkTreePath *path;

        path = gtk_tree_path_new_from_indices(ranges[i].from, -1);
        g_ptr_array_add(paths, path);
        path = gtk_tree_path_new_from_indices(ranges[i].to, -1);
        g_ptr_array_add(paths, path);
    }
}

/* Constructs operations necessary to bring @out_of_sync_selection to sync
 * with the @binding's intset. */
static void
find_ranges_to_select_and_deselect(GwyListSelectionBinding *binding,
                                   GtkTreeSelection *out_of_sync_selection)
{
    ListSelectionBinding *priv = binding->priv;
    if (!priv->toremove_set) {
        priv->toremove_set = gwy_int_set_new();
        priv->toadd_set = gwy_int_set_new();
        priv->toremove_paths = g_ptr_array_new();
        g_ptr_array_set_free_func(priv->toremove_paths,
                                  (GDestroyNotify)g_ptr_array_unref);
        priv->toadd_paths = g_ptr_array_new();
        g_ptr_array_set_free_func(priv->toadd_paths,
                                  (GDestroyNotify)g_ptr_array_unref);
    }
    transform_selection_to_intset(out_of_sync_selection, priv->toremove_set);
    gwy_int_set_assign(priv->toadd_set, priv->intset);
    gwy_int_set_subtract(priv->toadd_set, priv->toremove_set);
    gwy_int_set_subtract(priv->toremove_set, priv->intset);
    transform_intset_to_path_ranges(priv->toadd_set, priv->toadd_paths);
    transform_intset_to_path_ranges(priv->toremove_set, priv->toremove_paths);
}

static void
sync_selection(SelectionInfo *info, GwyListSelectionBinding *binding)
{
    ListSelectionBinding *priv = binding->priv;
    if (info->selection == priv->source_selection)
        return;

    find_ranges_to_select_and_deselect(binding, info->selection);

    for (guint i = 0; i < priv->toremove_paths->len/2; i++) {
        GtkTreePath *frompath = g_ptr_array_index(priv->toremove_paths, 2*i);
        GtkTreePath *topath = g_ptr_array_index(priv->toremove_paths, 2*i + 1);
        gtk_tree_selection_unselect_range(info->selection, frompath, topath);
    }
    g_ptr_array_set_size(priv->toremove_paths, 0);

    for (guint i = 0; i < priv->toadd_paths->len/2; i++) {
        GtkTreePath *frompath = g_ptr_array_index(priv->toadd_paths, 2*i);
        GtkTreePath *topath = g_ptr_array_index(priv->toadd_paths, 2*i + 1);
        gtk_tree_selection_select_range(info->selection, frompath, topath);
    }
    g_ptr_array_set_size(priv->toadd_paths, 0);
}

static void
sync_intset_to_selections(GwyListSelectionBinding *binding)
{
    do_with_all_selections(block_selection, binding);
    do_with_all_selections(sync_selection, binding);
    do_with_all_selections(unblock_selection, binding);
}

static void
sync_selection_to_intset(GwyListSelectionBinding *binding,
                         GtkTreeSelection *selection)
{
    ListSelectionBinding *priv = binding->priv;
    block_intset(binding);
    priv->source_selection = selection;
    transform_selection_to_intset(selection, priv->intset);
    sync_intset_to_selections(binding);
    priv->source_selection = NULL;
    unblock_intset(binding);
}

static void
block_intset(GwyListSelectionBinding *binding)
{
    ListSelectionBinding *priv = binding->priv;
    g_signal_handler_block(priv->intset, priv->assigned_id);
    g_signal_handler_block(priv->intset, priv->removed_id);
    g_signal_handler_block(priv->intset, priv->added_id);
}

static void
unblock_intset(GwyListSelectionBinding *binding)
{
    ListSelectionBinding *priv = binding->priv;
    g_signal_handler_unblock(priv->intset, priv->assigned_id);
    g_signal_handler_unblock(priv->intset, priv->removed_id);
    g_signal_handler_unblock(priv->intset, priv->added_id);
}

/**
 * SECTION: list-selection-binding
 * @title: GwyListSelectionBinding
 * @short_description: Binding between tree selections and an integer set
 *
 * #GwyListSelectionBinding is a synchronisation mechanism between a #GwyIntSet
 * and an aribtrary number of #GtkTreeSelection objects.  The selections must
 * correspond to a list-type tree view (since #GwyIntSet obviously represent
 * only flat indices). The tree views should be based on the same model and the
 * selections should use the same selection mode although this is not enforced.
 * So different modes and models are permitted but you must ensure it does not
 * result in an attempt to select something impossible somewhere.
 *
 * One integer set can be bound to several selections (that then become
 * synchronised), however, one selection can be bound only to at most one
 * integer set.
 *
 * The binding is severed when the integer set is destroyed, with
 * gwy_list_selection_binding_unbind() or by destroying the binding object.
 **/

/**
 * GwyListSelectionBinding:
 *
 * Bindings between tree selections and integer sets.
 *
 * The #GwyListSelectionBinding struct contains private data only and should be
 * accessed using the functions below.
 **/

/**
 * GwyListSelectionBindingClass:
 *
 * Class of bindings between tree selections and integer sets.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
