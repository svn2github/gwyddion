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
};

typedef struct _GwyListSelectionBindingPrivate ListSelectionBinding;

static void           gwy_list_selection_binding_finalize            (GObject *object);
static void           gwy_list_selection_binding_dispose             (GObject *object);
static void           gwy_list_selection_binding_set_property        (GObject *object,
                                                           guint prop_id,
                                                           const GValue *value,
                                                           GParamSpec *pspec);
static void           gwy_list_selection_binding_get_property        (GObject *object,
                                                           guint prop_id,
                                                           GValue *value,
                                                           GParamSpec *pspec);

static GParamSpec *properties[N_PROPS];

G_DEFINE_TYPE(GwyListSelectionBinding, gwy_list_selection_binding,
              G_TYPE_OBJECT);

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
    G_OBJECT_CLASS(gwy_list_selection_binding_parent_class)->finalize(object);
}

static void
gwy_list_selection_binding_dispose(GObject *object)
{
    GwyListSelectionBinding *binding = GWY_LIST_SELECTION_BINDING(object);
    gwy_list_selection_binding_unbind(binding);

    ListSelectionBinding *priv = binding->priv;
    GWY_OBJECT_UNREF(priv->intset);

    G_OBJECT_CLASS(gwy_list_selection_binding_parent_class)->dispose(object);
}

static void
gwy_list_selection_binding_set_property(GObject *object,
                                        guint prop_id,
                                        const GValue *value,
                                        GParamSpec *pspec)
{
    ListSelectionBinding *priv = GWY_LIST_SELECTION_BINDING(object)->priv;

    switch (prop_id) {
        case PROP_INT_SET:
        priv->intset = g_value_get_object(value);
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
 * Returns: A new list selection binding.
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
 * languages where the destruction of the binding may be delayed.  In C you can
 * just release the last reference to @binding and it will go away.
 **/
void
gwy_list_selection_binding_unbind(GwyListSelectionBinding *binding)
{
    g_return_if_fail(GWY_IS_LIST_SELECTION_BINDING(binding));

}

#if 0
// XXX: This is too ad-hocish.  Create a data structure for each association,
// holding for each association:
// - The associated GtkTreeSelection.
// - Id of the signal to the tree selection.
// And once:
// - Ids of the GwyIntSet
//
// Proceed more agressively in the selection-to-intset path: We know that we
// are going to sync the other selections, if any, so do that explicitly while
// preventing any back-updates.  This turns selections-to-intset into a special
// kind of intset-to-selections which needs to sync from one of the selections
// first (blocking any intset signals).

static void
sync_single_selection_to_intset(GtkTreeSelection *selection,
                                GwyIntSet *intset)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        GtkTreePath *path = gtk_tree_model_get_path(model, &iter);
        gint depth;
        gint *indices = gtk_tree_path_get_indices_with_depth(path, &depth);
        g_assert(depth == 1);
        gwy_int_set_update(intset, indices, 1);
        gtk_tree_path_free(path);
    }
    else {
        gwy_int_set_update(intset, NULL, 0);
    }
}

static void
sync_multiple_selection_to_intset(GtkTreeSelection *selection,
                                  GwyIntSet *intset)
{
    GList *paths = gtk_tree_selection_get_selected_rows(selection, NULL);
    guint n = g_list_length(paths);
    gint *selected = g_new(gint, n);

    n = 0;
    for (GList *l = paths; l; l = g_list_next(l)) {
        GtkTreePath *path = (GtkTreePath*)l->data;
        gint depth;
        gint *indices = gtk_tree_path_get_indices_with_depth(path, &depth);
        g_assert(depth == 1);
        selected[n++] = indices[0];
    }
    g_list_free_full(paths, (GDestroyNotify)gtk_tree_path_free);

    gwy_int_set_update(intset, selected, n);
    g_free(selected);
}

/*
 * Connected to GtkTreeSelection::changed.  Updates the associated GwyIntSet,
 * ensuring that we do not attempt to propagate the changes back.
 *
 * Bad things:
 * - GwyIntSet has multiple signals we need to block.
 * - GtkTreeSelection doesn't provide any information how the selection has
 *   changed.
 * Good things:
 * - gwy_int_set_update() does not emit anything if the set does not change.
 */
static void
sync_selection_to_intset(GtkTreeSelection *selection,
                         GwyIntSet *intset)
{
    gulong assign_id = g_signal_handler_find(intset,
                                             G_SIGNAL_MATCH_FUNC
                                             | G_SIGNAL_MATCH_DATA,
                                             0, 0, NULL,
                                             sync_intset_to_selections,
                                             selection);
    g_return_if_fail(assign_id);
    gulong removed_id = g_signal_handler_find(intset,
                                              G_SIGNAL_MATCH_FUNC
                                              | G_SIGNAL_MATCH_DATA,
                                              0, 0, NULL,
                                              intset_removed_sync_to_selections,
                                              selection);
    g_return_if_fail(removed_id);
    gulong added_id = g_signal_handler_find(intset,
                                            G_SIGNAL_MATCH_FUNC
                                            | G_SIGNAL_MATCH_DATA,
                                            0, 0, NULL,
                                            intset_added_sync_to_selections,
                                            selection);
    g_return_if_fail(added_id);

    g_signal_handler_block(intset, assign_id);
    g_signal_handler_block(intset, removed_id);
    g_signal_handler_block(intset, added_id);

    GtkSelectionMode mode = gtk_tree_selection_get_mode(selection);

    if (mode == GTK_SELECTION_NONE
        || mode == GTK_SELECTION_SINGLE
        || mode == GTK_SELECTION_BROWSE)
        sync_single_selection_to_intset(selection, intset);
    else
        sync_multiple_selection_to_intset(selection, intset);

    g_signal_handler_unblock(intset, added_id);
    g_signal_handler_unblock(intset, removed_id);
    g_signal_handler_unblock(intset, assign_id);
}

static void
intset_removed_sync_to_selection(GwyIntSet *intset,
                                 gint value,
                                 GtkTreeSelection *selection)
{
    gulong *ids;
    GSList *assocs = find_and_block_sel_to_inset(intset, &ids);
    unselect_by_index(selection, value);
    unblock_and_free_sel_to_intset(assocs, ids);
}

static void
intset_added_sync_to_selection(GwyIntSet *intset,
                               gint value,
                               GtkTreeSelection *selection)
{
    gulong *ids;
    GSList *assocs = find_and_block_sel_to_inset(intset, &ids);
    select_by_index(selection, value);
    unblock_and_free_sel_to_intset(assocs, ids);
}

static void
sync_intset_to_multiple_selection(GwyIntSet *intset,
                                  GtkTreeSelection *selection)
{
    GList *paths = gtk_tree_selection_get_selected_rows(selection, NULL);
    guint n = g_list_length(paths);
    gint *toremove = g_new(gint, n);

    // Gather indices to add in toadd and indices to remove in toremove.
    guint nremove = 0;
    GwyIntSet *toadd = gwy_int_set_duplicate(intset);
    for (GList *l = paths; l; l = g_list_next(l)) {
        GtkTreePath *path = (GtkTreePath*)l->data;
        gint depth;
        gint *indices = gtk_tree_path_get_indices_with_depth(path, &depth);
        g_assert(depth == 1);
        if (gwy_int_set_contains(toadd, indices[0]))
            gwy_int_set_remove(toadd);
        else
            toremove[nremove++] = indices[0];
    }
    g_list_free_full(paths, (GDestroyNotify)gtk_tree_path_free);

    // Remove
    GtkTreePath *path = gtk_tree_path_new_from_indices(0, -1);
    gint *indices = gtk_tree_path_get_indices(path);
    for (guint i = 0; i < nremove; i++) {
        // FIXME: Is this correct?
        indices[0] = toremove[i];
        gtk_tree_selection_unselect_path(selection, path);
    }
    g_free(toremove);

    // Add
    GwyIntSetIter iter;
    if (gwy_int_set_first(toadd, &iter)) {
        do {
            // FIXME: Is this correct?
            indices[0] = iter.value;
            gtk_tree_selection_select_path(selection, path);
        } while (gwy_int_set_next(toadd, &iter));
    }

    gtk_tree_path_free(path);
    g_object_unref(toadd);
}

static void
sync_intset_to_signle_selection(GwyIntSet *intset,
                                GtkTreeSelection *selection)
{
    GwyIntSetIter iter;
    if (!gwy_int_set_first(intset, &iter)) {
        gtk_tree_selection_unselect_all(selection);
        return;
    }

    if (gtk_tree_selection_get_mode(selection) == GTK_SELECTION_NONE) {
        g_warning("Int set contains values but tree selection mode is NONE.");
        return;
    }

    select_by_index(selection, iter.value);

    if (gwy_int_set_next(intset, &iter)) {
        g_warning("Int set contains more than one value for "
                  "non-multiple selection.");
    }
}

/*
 * Connected to GwyIntSet::assigned.  Updates an associated GtkTreeSelection.
 *
 * Bad things:
 * - It's a PITA to get currently selected indices from GtkTreeSelection.
 * - Someone may try to update the IntSet in a way incompatible with the
 *   current selection mode (single/browse).
 * - There can be multiple selections.  We need to block all
 *   selection-to-intset handlers, even for the other selections because in
 *   this case we only want a single round of updates.
 */
static void
sync_intset_to_selections(GwyIntSet *intset,
                          GtkTreeSelection *selection)
{
    gulong *ids;
    GSList *assocs = find_and_block_sel_to_inset(intset, &ids);
    GtkSelectionMode mode = gtk_tree_selection_get_mode(selection);

    if (mode == GTK_SELECTION_NONE
        || mode == GTK_SELECTION_SINGLE
        || mode == GTK_SELECTION_BROWSE)
        sync_intset_to_single_selection(selection, intset);
    else
        sync_intset_to_multiple_selection(selection, intset);

    unblock_and_free_sel_to_intset(assocs, ids);
}

static void
select_by_index(GtkTreeSelection *selection, gint value)
{
    GtkTreePath *path = gtk_tree_path_new_from_indices(value, -1);
    gtk_tree_selection_select_path(selection, path);
    gtk_tree_path_free(path);
}

static void
unselect_by_index(GtkTreeSelection *selection, gint value)
{
    GtkTreePath *path = gtk_tree_path_new_from_indices(value, -1);
    gtk_tree_selection_unselect_path(selection, path);
    gtk_tree_path_free(path);
}

static GSList*
find_and_block_sel_to_inset(GwyIntSet *intset,
                            gulong **pids)
{
    GSList *assocs = g_object_get_qdata(G_OBJECT(intset),
                                        "gwy-associated-tree-selections");
    g_return_if_fail(assocs);

    guint nassoc = g_slist_length(assocs);
    gulong *ids = g_new(gulong, nassoc);
    GSList *l;
    for (guint i = 0, l = assocs; i < nassoc; i++, l = g_slist_next(l)) {
        GtkTreeSelection *sel = (GtkTreeSelection*)l->data;
        ids[i] = g_signal_handler_find(sel,
                                       G_SIGNAL_MATCH_FUNC
                                       | G_SIGNAL_MATCH_DATA,
                                       0, 0, NULL,
                                       sync_selection_to_intset, intset);
        g_assert(ids[i]);
        g_signal_handler_block(sel, ids[i]);
    }

    *pids = ids;
    return assocs;
}

static void
unblock_and_free_sel_to_intset(GSList *assocs, guint *ids)
{
    GSList *l;

    for (guint i = 0, l = assocs; i < nassoc; i++, l = g_slist_next(l)) {
        GtkTreeSelection *sel = (GtkTreeSelection*)l->data;
        g_signal_handler_unblock(selection, ids[i]);
    }

    g_free(ids);
}
#endif

/**
 * SECTION: list-selection-binding
 * @title: GwyListSelectionBinding
 * @short_description: Binding between tree selections and an integer set
 *
 * #GwyListSelectionBinding is a synchronisation mechanism between a #GwyIntSet
 * and an aribtrary number of #GtkTreeSelection objects.  The selections must
 * correspond to a list-type tree view (since #GwyIntSet obviously represent
 * only flat indices) and they should use the same selection mode.
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
