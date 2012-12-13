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
#include "libgwy/gradient.h"
#include "libgwyui/cell-renderer-gradient.h"
#include "libgwyui/coords-view.h"

enum {
    PROP_0,
    PROP_COORDS,
    N_PROPS,
};

typedef struct {
    GwyValueFormat *vf;
    gulong vf_notify_id;
    guint dimension_id;
} CoordInfo;

struct _GwyCoordsViewPrivate {
    GwyArrayStore *store;
    GwyCoords *coords;
    CoordInfo *coord_info;
    GType coords_type;
    guint dimension;
    guint height;
};

typedef struct _GwyCoordsViewPrivate CoordsView;

static void     gwy_coords_view_dispose     (GObject *object);
static void     gwy_coords_view_finalize    (GObject *object);
static void     gwy_coords_view_set_property(GObject *object,
                                             guint prop_id,
                                             const GValue *value,
                                             GParamSpec *pspec);
static void     gwy_coords_view_get_property(GObject *object,
                                             guint prop_id,
                                             GValue *value,
                                             GParamSpec *pspec);
static void     render_value                (GtkTreeViewColumn *column,
                                             GtkCellRenderer *renderer,
                                             GtkTreeModel *model,
                                             GtkTreeIter *iter,
                                             gpointer user_data);
static gboolean set_coords                  (GwyCoordsView *view,
                                             GwyCoords *coords);
static gboolean set_coords_type             (GwyCoordsView *view,
                                             GType type);
static void     free_coord_info             (GwyCoordsView *view);
static void     selection_changed           (GwyCoordsView *view,
                                             GtkTreeSelection *selection);
static void     format_notify               (GwyCoordsView *view,
                                             GParamSpec *pspec,
                                             GwyValueFormat *vf);

static GParamSpec *properties[N_PROPS];

G_DEFINE_TYPE(GwyCoordsView, gwy_coords_view, GTK_TYPE_TREE_VIEW);

static void
gwy_coords_view_class_init(GwyCoordsViewClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    g_type_class_add_private(klass, sizeof(CoordsView));

    gobject_class->dispose = gwy_coords_view_dispose;
    gobject_class->finalize = gwy_coords_view_finalize;
    gobject_class->get_property = gwy_coords_view_get_property;
    gobject_class->set_property = gwy_coords_view_set_property;

    properties[PROP_COORDS]
        = g_param_spec_object("coords",
                              "Coords",
                              "Coords shown by the view.",
                              GWY_TYPE_COORDS,
                              G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    for (guint i = 1; i < N_PROPS; i++)
        g_object_class_install_property(gobject_class, i, properties[i]);
}

static void
gwy_coords_view_init(GwyCoordsView *view)
{
    view->priv = G_TYPE_INSTANCE_GET_PRIVATE(view, GWY_TYPE_COORDS_VIEW,
                                             CoordsView);
    CoordsView *priv = view->priv;
    gint w, h;
    gtk_icon_size_lookup(GTK_ICON_SIZE_SMALL_TOOLBAR, &w, &h);
    priv->height = h;

    GtkTreeView *treeview = GTK_TREE_VIEW(view);
    GtkTreeSelection *selection = gtk_tree_view_get_selection(treeview);
    g_signal_connect_swapped(selection, "changed",
                             G_CALLBACK(selection_changed), view);
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_MULTIPLE);
}

static void
gwy_coords_view_dispose(GObject *object)
{
    set_coords(GWY_COORDS_VIEW(object), NULL);
    G_OBJECT_CLASS(gwy_coords_view_parent_class)->dispose(object);
}

static void
gwy_coords_view_finalize(GObject *object)
{
    GwyCoordsView *view = GWY_COORDS_VIEW(object);
    free_coord_info(view);
    G_OBJECT_CLASS(gwy_coords_view_parent_class)->finalize(object);
}

static void
gwy_coords_view_set_property(GObject *object,
                             guint prop_id,
                             const GValue *value,
                             GParamSpec *pspec)
{
    GwyCoordsView *view = GWY_COORDS_VIEW(object);

    switch (prop_id) {
        case PROP_COORDS:
        set_coords(view, g_value_get_object(value));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_coords_view_get_property(GObject *object,
                             guint prop_id,
                             GValue *value,
                             GParamSpec *pspec)
{
    GwyCoordsView *view = GWY_COORDS_VIEW(object);
    CoordsView *priv = view->priv;

    switch (prop_id) {
        case PROP_COORDS:
        g_value_set_object(value, priv->coords);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

/**
 * gwy_coords_view_new:
 *
 * Creates a new coords view.
 *
 * Returns: A newly created coords view (with no columns).
 **/
GtkWidget*
gwy_coords_view_new(void)
{
    return g_object_newv(GWY_TYPE_COORDS_VIEW, 0, NULL);
}

/**
 * gwy_coords_view_set_coords:
 * @view: A coords view.
 * @coords: Coordinates to display.
 *
 * Sets the coordinates that a coords view displays.
 **/
void
gwy_coords_view_set_coords(GwyCoordsView *view,
                           GwyCoords *coords)
{
    g_return_if_fail(GWY_IS_COORDS_VIEW(view));
    if (!set_coords(view, coords))
        return;

    g_object_notify_by_pspec(G_OBJECT(view), properties[PROP_COORDS]);
}

/**
 * gwy_coords_view_get_coords:
 * @view: A coords view.
 *
 * Gets the coordinates that a coords view displays.
 *
 * Returns: (transfer none):
 *          Coordinates displayed by @view.
 **/
GwyCoords*
gwy_coords_view_get_coords(const GwyCoordsView *view)
{
    g_return_val_if_fail(GWY_IS_COORDS_VIEW(view), NULL);
    return view->priv->coords;
}

/**
 * gwy_coords_view_set_coords_type:
 * @view: A coords view.
 * @type: Type of coordinates this view will show.
 *
 * Sets the type of coordinates a coords view will show.
 *
 * It is not necessary to call this method before gwy_coords_view_set_coords()
 * because setting the coordinates object sets the type automatically.
 * However, you may want to use the setup function such as
 * gwy_coords_view_set_dimension_format() or
 * gwy_coords_view_create_column_coord() before setting any coordinate object
 * to show and these functions need to know the type.
 *
 * Once set, the type should not change.
 **/
void
gwy_coords_view_set_coords_type(GwyCoordsView *view,
                                GType type)
{
    g_return_if_fail(GWY_IS_COORDS_VIEW(view));
    g_return_if_fail(g_type_is_a(type, GWY_TYPE_COORDS));
    g_return_if_fail(G_TYPE_IS_INSTANTIATABLE(type));
    set_coords_type(view, type);
}

/**
 * gwy_coords_view_get_coords_type:
 * @view: A coords view.
 *
 * Gets the type of coordinates a coords view shows.
 *
 * Returns: The type of coords this view was set up for.  Zero may be returned
 *          if no setup for a specific coords type has been done yet.
 **/
GType
gwy_coords_view_get_coords_type(const GwyCoordsView *view)
{
    g_return_val_if_fail(GWY_IS_COORDS_VIEW(view), 0);
    return view->priv->coords_type;
}

/**
 * gwy_coords_view_set_dimension_format:
 * @view: A coords view.
 * @i: Dimension index.
 * @format: (allow-none) (transfer full):
 *          Format to use for coordinates in dimension @i.
 *
 * Sets the format for coordinates in a specific dimension in a coords view.
 *
 * Changing the format for a dimension influences all columns for this
 * dimension with gwy_coords_view_create_column_coord(), whether already
 * existing or created in the future.
 *
 * This method requires the coords type to be set.
 **/
void
gwy_coords_view_set_dimension_format(GwyCoordsView *view,
                                     guint i,
                                     GwyValueFormat *format)
{
    g_return_if_fail(GWY_IS_COORDS_VIEW(view));
    CoordsView *priv = view->priv;
    g_return_if_fail(priv->coord_info);
    g_return_if_fail(i < priv->dimension);
    if (!gwy_set_member_object(view, format, GWY_TYPE_VALUE_FORMAT,
                               &priv->coord_info[i].vf,
                               "notify", format_notify,
                               &priv->coord_info[i].vf_notify_id,
                               G_CONNECT_SWAPPED,
                               NULL))
        return;

    // Is this sufficient to re-render the cells?
    gtk_widget_queue_draw(GTK_WIDGET(view));
}

/**
 * gwy_coords_view_get_dimension_format:
 * @view: A coords view.
 * @i: Dimension index.
 *
 * Gets the format for coordinates in a specific dimension in a coords view.
 *
 * Returns: (allow-none) (transfer none):
 *          The value format for dimension @i.  %NULL is returned if the format
 *          is unset or the coords type is unset.
 **/
GwyValueFormat*
gwy_coords_view_get_dimension_format(const GwyCoordsView *view,
                                     guint i)
{
    g_return_val_if_fail(GWY_IS_COORDS_VIEW(view), NULL);
    CoordsView *priv = view->priv;
    if (!priv->coord_info)
        return NULL;
    g_return_val_if_fail(i < priv->dimension, NULL);
    return priv->coord_info[i].vf;
}

/**
 * gwy_coords_view_create_column_coord:
 * @view: A coords view.
 * @i: Coordinate index.
 *
 * Creates a standard column with coords names for a coords view.
 *
 * This method may be used with coords lists displaying any #GwyCoords
 * subclasses.
 *
 * This method requires the coords type to be set.
 **/
GtkTreeViewColumn*
gwy_coords_view_create_column_coord(GwyCoordsView *view,
                                    guint i)
{
    static const gchar *const names[] = {
        "X", "Y", "Z", "W", "V", "U"
    };

    g_return_val_if_fail(GWY_IS_COORDS_VIEW(view), NULL);
    CoordsView *priv = view->priv;
    g_return_val_if_fail(priv->coord_info, NULL);

    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    gtk_cell_renderer_set_fixed_size(renderer, -1, priv->height);
    guint d = priv->coord_info[i].dimension_id;
    const gchar *title = (d < G_N_ELEMENTS(names)) ? names[d] : "";
    GtkTreeViewColumn *column
        = gtk_tree_view_column_new_with_attributes(title, renderer, NULL);
    gtk_tree_view_column_set_cell_data_func(column, renderer, render_value,
                                            GUINT_TO_POINTER(i), NULL);

    return column;
}

static gboolean
set_coords(GwyCoordsView *view,
           GwyCoords *coords)
{
    CoordsView *priv = view->priv;
    if (!gwy_set_member_object(view, coords, GWY_TYPE_COORDS,
                               &priv->coords,
                               NULL))
        return FALSE;

    GWY_OBJECT_UNREF(priv->store);
    if (!coords) {
        gtk_tree_view_set_model(GTK_TREE_VIEW(view), NULL);
        return TRUE;
    }
    set_coords_type(view, G_OBJECT_TYPE(coords));

    priv->store = gwy_array_store_new(GWY_ARRAY(coords));
    g_object_ref_sink(priv->store);
    gtk_tree_view_set_model(GTK_TREE_VIEW(view), GTK_TREE_MODEL(priv->store));
    return TRUE;
}

static gboolean
set_coords_type(GwyCoordsView *view,
                GType type)
{
    CoordsView *priv = view->priv;
    if (priv->coords_type) {
        if (type == priv->coords_type)
            return FALSE;

        // XXX: This actually matters only if we have some columns.  But then
        // it may matter badly.
        g_warning("Coords view was set up for type %s, not %s.",
                  g_type_name(priv->coords_type), g_type_name(type));
        free_coord_info(view);
    }
    priv->coords_type = type;
    GwyCoordsClass *klass = g_type_class_ref(type);
    priv->dimension = klass->dimension;
    priv->coord_info = g_slice_alloc0(priv->dimension*sizeof(CoordInfo));
    for (guint i = 0; i < priv->dimension; i++)
        priv->coord_info[i].dimension_id = klass->dimension_map[i];
    g_type_class_unref(klass);

    return TRUE;
}

static void
free_coord_info(GwyCoordsView *view)
{
    CoordsView *priv = view->priv;
    if (!priv->coord_info)
        return;

    for (guint i = 0; i < priv->dimension; i++) {
        CoordInfo *info = priv->coord_info + i;
        GWY_SIGNAL_HANDLER_DISCONNECT(info->vf, info->vf_notify_id);
        GWY_OBJECT_UNREF(info->vf);
    }
    g_slice_free1(priv->dimension*sizeof(CoordInfo), priv->coord_info);
    priv->coord_info = NULL;
    priv->dimension = 0;
}

static void
render_value(GtkTreeViewColumn *column,
             GtkCellRenderer *renderer,
             GtkTreeModel *model,
             GtkTreeIter *iter,
             gpointer user_data)
{
    GtkWidget *parent = gtk_tree_view_column_get_tree_view(column);
    CoordsView *priv = GWY_COORDS_VIEW(parent)->priv;
    guint i = GPOINTER_TO_UINT(user_data);
    guint d = priv->coord_info[i].dimension_id;
    GwyValueFormat *vf = priv->coord_info[d].vf;
    gdouble *data = NULL;
    gtk_tree_model_get(model, iter, 1, &data, -1);
    gdouble value = data[i];
    if (vf) {
        const gchar *s = gwy_value_format_print_number(vf, value);
        g_object_set(renderer, "text", s, NULL);
    }
    else {
        gchar buf[32];
        g_snprintf(buf, sizeof(buf), "%g", value);
        g_object_set(renderer, "text", buf, NULL);
    }
}

static void
selection_changed(GwyCoordsView *view,
                  GtkTreeSelection *selection)
{
    GtkTreeView *treeview = GTK_TREE_VIEW(view);
    GtkSelectionMode mode = gtk_tree_selection_get_mode(selection);
    GtkTreeIter iter;
}

static void
format_notify(GwyCoordsView *view,
              G_GNUC_UNUSED GParamSpec *pspec,
              G_GNUC_UNUSED GwyValueFormat *vf)
{
    // Is this sufficient to re-render the cells?
    gtk_widget_queue_draw(GTK_WIDGET(view));
}

/**
 * SECTION: coords-view
 * @title: GwyCoordsView
 * @short_description: List view displaying coordinates of shapes
 **/

/**
 * GwyCoordsView:
 *
 * List view displaying coordinates of shapes.
 *
 * The #GwyCoordsView struct contains private data only and should be
 * accessed using the functions below.
 **/

/**
 * GwyCoordsViewClass:
 *
 * Class of coordinate list views.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
