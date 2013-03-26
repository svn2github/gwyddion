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
#include "libgwy/strfuncs.h"
#include "libgwy/rgba.h"
#include "libgwyui/types.h"
#include "libgwyui/coords-view.h"

enum {
    PROP_0,
    PROP_COORDS,
    PROP_SHAPES,
    PROP_SCALE_TYPE,
    PROP_SPLIT_UNITS,
    N_PROPS,
};

typedef struct {
    GtkTreeViewColumn *column;
    gchar *title;
    gint i;
    gboolean is_index : 1;
} ColumnInfo;

typedef struct {
    GwyValueFormat *vf;
    gchar *units;
    gulong vf_notify_id;
} DimInfo;

struct _GwyCoordsViewPrivate {
    GwyArrayStore *store;
    GwyShapes *shapes;
    gulong shapes_notify_id;
    GwyIntSet *shapes_selection;
    gulong shapes_selection_added_id;
    gulong shapes_selection_removed_id;
    gulong shapes_selection_assigned_id;
    GwyCoords *coords;
    DimInfo *dim_info;
    GSList *column_info;
    GwyCoordsClass *coords_class;
    GwyValueFormat *pixel_format;
    GwyCoordScaleType scale_type;
    guint height;
    gboolean editable : 1;  // TODO: Make it a property
    gboolean split_units : 1;
    gboolean sync_view_to_shapes : 1;
    gboolean sync_shapes_to_view : 1;
};

typedef struct _GwyCoordsViewPrivate CoordsView;

static void     gwy_coords_view_dispose      (GObject *object);
static void     gwy_coords_view_finalize     (GObject *object);
static void     gwy_coords_view_set_property (GObject *object,
                                              guint prop_id,
                                              const GValue *value,
                                              GParamSpec *pspec);
static void     gwy_coords_view_get_property (GObject *object,
                                              guint prop_id,
                                              GValue *value,
                                              GParamSpec *pspec);
static void     render_value                 (GtkTreeViewColumn *column,
                                              GtkCellRenderer *renderer,
                                              GtkTreeModel *model,
                                              GtkTreeIter *iter,
                                              gpointer user_data);
static void     render_index                 (GtkTreeViewColumn *column,
                                              GtkCellRenderer *renderer,
                                              GtkTreeModel *model,
                                              GtkTreeIter *iter,
                                              gpointer user_data);
static gboolean set_shapes                   (GwyCoordsView *view,
                                              GwyShapes *shapes);
static gboolean set_coords                   (GwyCoordsView *view,
                                              GwyCoords *coords);
static gboolean set_coords_type              (GwyCoordsView *view,
                                              GType type);
static gboolean set_scale_type               (GwyCoordsView *view,
                                              GwyCoordScaleType scaletype);
static gboolean set_split_units              (GwyCoordsView *view,
                                              gboolean splitunits);
static void     free_dim_info                (GwyCoordsView *view);
static void     selection_changed            (GwyCoordsView *view,
                                              GtkTreeSelection *selection);
static void     shapes_notify                (GwyCoordsView *view,
                                              GParamSpec *pspec,
                                              GwyShapes *shapes);
static void     format_notify                (GwyCoordsView *view,
                                              GParamSpec *pspec,
                                              GwyValueFormat *vf);
static void     recalc_column_titles         (GwyCoordsView *view);
static void     recalc_column_titles_for_dim (GwyCoordsView *view,
                                              guint d);
static void     update_column_title          (GwyCoordsView *view,
                                              ColumnInfo *column_info);
static gchar*   auto_column_title            (const CoordsView *priv,
                                              guint i);
static void     column_gone                  (gpointer data,
                                              GObject *where_the_object_was);
static GSList*  find_column_info             (const GwyCoordsView *view,
                                              GtkTreeViewColumn *column);
static void     shapes_selection_added       (GwyCoordsView *view,
                                              gint n,
                                              GwyIntSet *intset);
static void     shapes_selection_removed     (GwyCoordsView *view,
                                              gint n,
                                              GwyIntSet *intset);
static void     shapes_selection_assigned    (GwyCoordsView *view,
                                              GwyIntSet *intset);
static void     sync_selection_shapes_to_view(GwyCoordsView *view);
static void     clear_selection              (GwyCoordsView *view);
static void     select_or_unselect_range     (GtkTreeSelection *selection,
                                              guint from,
                                              guint to,
                                              gboolean select);

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
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_SHAPES]
        = g_param_spec_object("shapes",
                              "Shapes",
                              "Shapes shown by the view.",
                              GWY_TYPE_SHAPES,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_SCALE_TYPE]
        = g_param_spec_enum("scale-type",
                            "Scale type",
                            "Type of coordinates scale.",
                            GWY_TYPE_COORD_SCALE_TYPE,
                            GWY_COORD_SCALE_REAL,
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_SPLIT_UNITS]
        = g_param_spec_boolean("split-units",
                               "Split units",
                               "Whether units are split to the second line.",
                               TRUE,
                               G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    for (guint i = 1; i < N_PROPS; i++)
        g_object_class_install_property(gobject_class, i, properties[i]);
}

static void
gwy_coords_view_init(GwyCoordsView *view)
{
    view->priv = G_TYPE_INSTANCE_GET_PRIVATE(view, GWY_TYPE_COORDS_VIEW,
                                             CoordsView);
    CoordsView *priv = view->priv;
    priv->split_units = TRUE;

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
    GwyCoordsView *view = GWY_COORDS_VIEW(object);
    CoordsView *priv = view->priv;
    if (priv->shapes)
        set_shapes(view, NULL);
    else
        set_coords(view, NULL);
    G_OBJECT_CLASS(gwy_coords_view_parent_class)->dispose(object);
}

static void
gwy_coords_view_finalize(GObject *object)
{
    GwyCoordsView *view = GWY_COORDS_VIEW(object);
    CoordsView *priv = view->priv;
    free_dim_info(view);
    GWY_OBJECT_UNREF(priv->pixel_format);
    // The columns should be already destroyed.
    g_assert(!priv->column_info);
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
        set_shapes(view, NULL);
        set_coords(view, g_value_get_object(value));
        break;

        case PROP_SHAPES:
        set_shapes(view, g_value_get_object(value));
        break;

        case PROP_SCALE_TYPE:
        set_scale_type(view, g_value_get_enum(value));
        break;

        case PROP_SPLIT_UNITS:
        set_split_units(view, g_value_get_boolean(value));
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

        case PROP_SHAPES:
        g_value_set_object(value, priv->shapes);
        break;

        case PROP_SCALE_TYPE:
        g_value_set_enum(value, priv->scale_type);
        break;

        case PROP_SPLIT_UNITS:
        g_value_set_boolean(value, priv->split_units);
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
 * @coords: (allow-none) (transfer full):
 *          Coordinates to display.
 *
 * Sets the coordinates that a coords view displays.
 **/
void
gwy_coords_view_set_coords(GwyCoordsView *view,
                           GwyCoords *coords)
{
    g_return_if_fail(GWY_IS_COORDS_VIEW(view));
    set_shapes(view, NULL);
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
 * Returns: (allow-none) (transfer none):
 *          Coordinates displayed by @view, either set explicitly with
 *          gwy_coords_view_get_coords() or coming from a shapes object.
 **/
GwyCoords*
gwy_coords_view_get_coords(const GwyCoordsView *view)
{
    g_return_val_if_fail(GWY_IS_COORDS_VIEW(view), NULL);
    return view->priv->coords;
}

/**
 * gwy_coords_view_set_shapes:
 * @view: A coords view.
 * @shapes: (allow-none) (transfer full):
 *          Shapes to display coordinates of.
 *
 * Sets the shapes object whose coordinates a coords view will display.
 *
 * Setting the shapes causes several things to happen/start working
 * automatically.  The units and formats follow the real versus pixel scale
 * setting.  Furthermore, @shapes selection and @view selection becomes
 * synchronised and the selectability and editability of items in the list
 * follows the shapes.
 **/
void
gwy_coords_view_set_shapes(GwyCoordsView *view,
                           GwyShapes *shapes)
{
    g_return_if_fail(GWY_IS_COORDS_VIEW(view));
    if (!set_shapes(view, shapes))
        return;

    g_object_notify_by_pspec(G_OBJECT(view), properties[PROP_SHAPES]);
    // In principle, coords may remain the same.
    g_object_notify_by_pspec(G_OBJECT(view), properties[PROP_COORDS]);
}

/**
 * gwy_coords_view_get_shapes:
 * @view: A coords view.
 *
 * Gets the shapes object whose coordinates a coords view displays.
 *
 * Returns: (allow-none) (transfer none):
 *          Shapes object displayed by @view, if any.
 **/
GwyShapes*
gwy_coords_view_get_shapes(const GwyCoordsView *view)
{
    g_return_val_if_fail(GWY_IS_COORDS_VIEW(view), NULL);
    return view->priv->shapes;
}

/**
 * gwy_coords_view_set_scale_type:
 * @view: A coords view.
 * @scaletype: New scale type for rulers.
 *
 * Sets the scale type of a coords view coordinates.
 **/
void
gwy_coords_view_set_scale_type(GwyCoordsView *view,
                               GwyCoordScaleType scaletype)
{
    g_return_if_fail(GWY_IS_COORDS_VIEW(view));
    if (!set_scale_type(view, scaletype))
        return;

    g_object_notify_by_pspec(G_OBJECT(view), properties[PROP_SCALE_TYPE]);
}

/**
 * gwy_coords_view_get_scale_type:
 * @view: A coords view.
 *
 * Gets the scale type of a coords view coordinates.
 *
 * Returns: The scale type of coords view coordinates.
 **/
GwyCoordScaleType
gwy_coords_view_get_scale_type(const GwyCoordsView *view)
{
    g_return_val_if_fail(GWY_IS_COORDS_VIEW(view), GWY_COORD_SCALE_PIXEL);
    return view->priv->scale_type;
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
 *          if the specific coords type has been set up yet.
 **/
GType
gwy_coords_view_get_coords_type(const GwyCoordsView *view)
{
    g_return_val_if_fail(GWY_IS_COORDS_VIEW(view), 0);
    CoordsView *priv = view->priv;
    return priv->coords_class ? G_OBJECT_CLASS_TYPE(priv->coords_class) : 0;
}

/**
 * gwy_coords_view_set_dimension_format:
 * @view: A coords view.
 * @d: Dimension index.
 * @format: (allow-none) (transfer full):
 *          Format to use for coordinates in dimension @d.
 *
 * Sets the format for coordinates in a specific dimension in a coords view.
 *
 * Changing the format for a dimension influences all columns for this
 * dimension with gwy_coords_view_create_column_coord(), whether already
 * existing or created in the future.
 *
 * This method requires the coords type to be set.
 *
 * If @view displays shapes the format set by this function is in effect only
 * for real coordinates, a pixel format is used to display pixel values.
 **/
void
gwy_coords_view_set_dimension_format(GwyCoordsView *view,
                                     guint d,
                                     GwyValueFormat *format)
{
    g_return_if_fail(GWY_IS_COORDS_VIEW(view));
    CoordsView *priv = view->priv;
    g_return_if_fail(priv->dim_info);
    g_return_if_fail(d < priv->coords_class->dimension);
    DimInfo *dim_info = priv->dim_info + d;
    if (!gwy_set_member_object(view, format, GWY_TYPE_VALUE_FORMAT,
                               &dim_info->vf,
                               "notify", format_notify,
                               &dim_info->vf_notify_id,
                               G_CONNECT_SWAPPED,
                               NULL))
        return;

    gboolean title_changed = FALSE;
    if (!format && dim_info->units) {
        GWY_FREE(dim_info->units);
        title_changed = TRUE;
    }
    else if (format) {
       const gchar *units = gwy_value_format_get_units(format);
       if (!dim_info->units) {
           dim_info->units = g_strdup(units);
           title_changed = TRUE;
       }
       else if (dim_info->units && !gwy_strequal(dim_info->units, units)) {
           GWY_FREE(dim_info->units);
           dim_info->units = g_strdup(units);
           title_changed = TRUE;
       }
    }

    if (title_changed)
        recalc_column_titles_for_dim(view, d);

    // Is this sufficient to re-render the cells?
    gtk_widget_queue_draw(GTK_WIDGET(view));
}

/**
 * gwy_coords_view_get_dimension_format:
 * @view: A coords view.
 * @d: Dimension index.
 *
 * Gets the format for coordinates in a specific dimension in a coords view.
 *
 * Returns: (allow-none) (transfer none):
 *          The value format for dimension @d.  %NULL is returned if the format
 *          is unset or the coords type is unset.
 **/
GwyValueFormat*
gwy_coords_view_get_dimension_format(const GwyCoordsView *view,
                                     guint d)
{
    g_return_val_if_fail(GWY_IS_COORDS_VIEW(view), NULL);
    CoordsView *priv = view->priv;
    if (!priv->dim_info)
        return NULL;
    guint dimension = priv->coords_class->dimension;
    g_return_val_if_fail(d < dimension, NULL);
    return priv->dim_info[d].vf;
}

/**
 * gwy_coords_view_create_column_coord:
 * @view: A coords view.
 * @title: (allow-none):
 *         Column title.  Passing %NULL means a column title will be
 *         constructed automatically, containing something like "y₂".  If you
 *         want an empty title pass the empty string.
 * @i: Coordinate index.
 *
 * Creates a standard column with coordinate values for a coords view.
 *
 * This method requires the coords type to be set.
 **/
GtkTreeViewColumn*
gwy_coords_view_create_column_coord(GwyCoordsView *view,
                                    const gchar *title,
                                    guint i)
{
    g_return_val_if_fail(GWY_IS_COORDS_VIEW(view), NULL);
    CoordsView *priv = view->priv;
    g_return_val_if_fail(priv->dim_info, NULL);

    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    gtk_cell_renderer_set_fixed_size(renderer, -1, priv->height);
    GtkTreeViewColumn *column = gtk_tree_view_column_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(column), renderer, TRUE);
    gtk_tree_view_column_set_cell_data_func(column, renderer, render_value,
                                            GUINT_TO_POINTER(i), NULL);

    GtkWidget *header = gtk_label_new(NULL);
    gtk_label_set_justify(GTK_LABEL(header), GTK_JUSTIFY_CENTER);
    gtk_widget_show(header);
    gtk_tree_view_column_set_widget(column, header);

    ColumnInfo *column_info = g_slice_new(ColumnInfo);
    column_info->column = column;
    column_info->i = i;
    column_info->is_index = FALSE;
    column_info->title = title ? g_strdup(title) : auto_column_title(priv, i);
    priv->column_info = g_slist_prepend(priv->column_info, column_info);
    g_object_weak_ref(G_OBJECT(column), column_gone, view);
    update_column_title(view, column_info);

    return column;
}

/**
 * gwy_coords_view_create_column_index:
 * @view: A coords view.
 * @title: (allow-none):
 *         Column title.  Passing %NULL means a column title will be
 *         constructed automatically, containing something like "n".  If you
 *         want an empty title pass the empty string.
 *
 * Creates a standard column with item index for a coords view.
 **/
GtkTreeViewColumn*
gwy_coords_view_create_column_index(GwyCoordsView *view,
                                    const gchar *title)
{
    g_return_val_if_fail(GWY_IS_COORDS_VIEW(view), NULL);
    CoordsView *priv = view->priv;

    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    gtk_cell_renderer_set_fixed_size(renderer, -1, priv->height);
    g_object_set(renderer, "foreground-set", TRUE, NULL);
    GtkTreeViewColumn *column = gtk_tree_view_column_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(column), renderer, TRUE);
    gtk_tree_view_column_set_cell_data_func(column, renderer, render_index,
                                            NULL, NULL);

    GtkWidget *header = gtk_label_new(NULL);
    gtk_label_set_justify(GTK_LABEL(header), GTK_JUSTIFY_CENTER);
    gtk_widget_show(header);
    gtk_tree_view_column_set_widget(column, header);

    ColumnInfo *column_info = g_slice_new(ColumnInfo);
    column_info->column = column;
    column_info->i = G_MAXUINT;
    column_info->is_index = TRUE;
    column_info->title = g_strdup(title ? title : "n");
    priv->column_info = g_slist_prepend(priv->column_info, column_info);
    g_object_weak_ref(G_OBJECT(column), column_gone, view);
    update_column_title(view, column_info);

    return column;
}

static gboolean
set_shapes(GwyCoordsView *view,
           GwyShapes *shapes)
{
    CoordsView *priv = view->priv;
    if (!gwy_set_member_object(view, shapes, GWY_TYPE_SHAPES, &priv->shapes,
                               "notify", shapes_notify, &priv->shapes_notify_id,
                               G_CONNECT_SWAPPED,
                               NULL))
        return FALSE;

    set_coords(view, shapes ? gwy_shapes_get_coords(shapes) : NULL);

    gwy_set_member_object(view, shapes ? shapes->selection : NULL,
                          GWY_TYPE_INT_SET, &priv->shapes_selection,
                          "added", shapes_selection_added,
                          &priv->shapes_selection_added_id,
                          G_CONNECT_SWAPPED,
                          "removed", shapes_selection_removed,
                          &priv->shapes_selection_removed_id,
                          G_CONNECT_SWAPPED,
                          "assigned", shapes_selection_assigned,
                          &priv->shapes_selection_assigned_id,
                          G_CONNECT_SWAPPED,
                          NULL);
    sync_selection_shapes_to_view(view);

    return TRUE;
}

static gboolean
set_coords(GwyCoordsView *view,
           GwyCoords *coords)
{
    CoordsView *priv = view->priv;
    if (!gwy_set_member_object(view, coords, GWY_TYPE_COORDS, &priv->coords,
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
    if (priv->coords_class) {
        GType current_type = G_OBJECT_CLASS_TYPE(priv->coords_class);
        if (type == current_type)
            return FALSE;

        // XXX: This actually matters only if we have some columns.  But then
        // it may matter badly.
        g_warning("Coords view was set up for type %s, not %s.",
                  g_type_name(current_type), g_type_name(type));
        free_dim_info(view);
    }
    priv->coords_class = g_type_class_ref(type);
    guint dimension = priv->coords_class->dimension;
    priv->dim_info = g_slice_alloc0(dimension*sizeof(DimInfo));

    return TRUE;
}

static gboolean
set_scale_type(GwyCoordsView *view,
               GwyCoordScaleType scaletype)
{
    CoordsView *priv = view->priv;
    if (scaletype == priv->scale_type)
        return FALSE;

    if (scaletype > GWY_COORD_SCALE_PIXEL) {
        g_warning("Wrong scale type %u.", scaletype);
        return FALSE;
    }

    priv->scale_type = scaletype;
    if (!priv->shapes)
        return TRUE;

    if (scaletype == GWY_COORD_SCALE_PIXEL) {
        if (!priv->pixel_format)
            priv->pixel_format
                = gwy_value_format_new_set(GWY_VALUE_FORMAT_PANGO,
                                           0, 1, " ", "px");
    }

    recalc_column_titles(view);
    gtk_widget_queue_draw(GTK_WIDGET(view));
    return TRUE;
}

static gboolean
set_split_units(GwyCoordsView *view,
                gboolean splitunits)
{
    CoordsView *priv = view->priv;
    if (!splitunits == !priv->split_units)
        return FALSE;

    priv->split_units = splitunits;
    recalc_column_titles(view);
    return TRUE;
}

static void
free_dim_info(GwyCoordsView *view)
{
    CoordsView *priv = view->priv;
    if (!priv->dim_info)
        return;

    guint dimension = priv->coords_class->dimension;
    for (guint i = 0; i < dimension; i++) {
        DimInfo *info = priv->dim_info + i;
        GWY_SIGNAL_HANDLER_DISCONNECT(info->vf, info->vf_notify_id);
        GWY_OBJECT_UNREF(info->vf);
        GWY_FREE(info->units);
    }
    g_slice_free1(dimension*sizeof(DimInfo), priv->dim_info);
    priv->dim_info = NULL;
    g_type_class_unref(priv->coords_class);
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
    guint d = priv->coords_class->dimension_map[i];
    gdouble *data = NULL;
    gtk_tree_model_get(model, iter, 1, &data, -1);
    gdouble value = data[i];
    GwyValueFormat *vf = NULL;
    const GwyShapes *shapes = priv->shapes;

    if (priv->scale_type == GWY_COORD_SCALE_PIXEL && shapes) {
        gdouble x = (d == 0) ? value : 0.0,
                y = (d == 1) ? value : 0.0;
        cairo_matrix_transform_point(&shapes->coords_to_view, &x, &y);
        cairo_matrix_transform_point(&shapes->view_to_pixel, &x, &y);
        value = (d == 0) ? x : y;
        vf = priv->pixel_format;
    }
    else
        vf = priv->dim_info[d].vf;

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
render_index(G_GNUC_UNUSED GtkTreeViewColumn *column,
             GtkCellRenderer *renderer,
             GtkTreeModel *model,
             GtkTreeIter *iter,
             G_GNUC_UNUSED gpointer user_data)
{
    guint i;
    gtk_tree_model_get(model, iter, 0, &i, -1);
    gchar buf[16];
    g_snprintf(buf, sizeof(buf), "%u", i+1);
    const GwyRGBA *rgba = gwy_rgba_get_preset_color(i);
    g_object_set(renderer, "text", buf, "foreground-rgba", rgba, NULL);
}

static void
selection_changed(GwyCoordsView *view,
                  GtkTreeSelection *selection)
{
    CoordsView *priv = view->priv;
    // Not having coords can occur during destruction.
    if (priv->sync_shapes_to_view || !priv->shapes_selection || !priv->coords)
        return;

    guint n = gwy_coords_size(priv->coords);
    gint *indices = g_slice_alloc(sizeof(gint)*n);
    guint len = 0;
    for (guint i = 0; i < n; i++) {
        GtkTreeIter iter;
        // We know that this is actually very efficient.
        gwy_array_store_get_iter(priv->store, i, &iter);
        if (gtk_tree_selection_iter_is_selected(selection, &iter))
            indices[len++] =  i;
    }
    priv->sync_view_to_shapes = TRUE;
    gwy_int_set_update(priv->shapes_selection, indices, len);
    priv->sync_view_to_shapes = FALSE;
    g_slice_free1(sizeof(gint)*n, indices);
}

static void
shapes_notify(GwyCoordsView *view,
              GParamSpec *pspec,
              GwyShapes *shapes)
{
    CoordsView *priv = view->priv;

    if (gwy_strequal(pspec->name, "coords"))
        set_coords(view, gwy_shapes_get_coords(shapes));
    else if (gwy_strequal(pspec->name, "editable"))
        priv->editable = gwy_shapes_get_editable(shapes);
    else if (gwy_strequal(pspec->name, "selectable")) {
        GtkTreeView *treeview = GTK_TREE_VIEW(view);
        GtkTreeSelection *selection = gtk_tree_view_get_selection(treeview);
        if (gwy_shapes_get_selectable(shapes)) {
            gtk_tree_selection_set_mode(selection, GTK_SELECTION_MULTIPLE);
            sync_selection_shapes_to_view(view);
        }
        else {
            clear_selection(view);
            gtk_tree_selection_set_mode(selection, GTK_SELECTION_NONE);
        }
    }
}

static void
format_notify(GwyCoordsView *view,
              G_GNUC_UNUSED GParamSpec *pspec,
              G_GNUC_UNUSED GwyValueFormat *vf)
{
    // Is this sufficient to re-render the cells?
    gtk_widget_queue_draw(GTK_WIDGET(view));
}

static void
recalc_column_titles(GwyCoordsView *view)
{
    for (GSList *l = view->priv->column_info; l; l = g_slist_next(l)) {
        ColumnInfo *column_info = (ColumnInfo*)l->data;
        update_column_title(view, column_info);
    }
}

static void
recalc_column_titles_for_dim(GwyCoordsView *view,
                             guint d)
{
    CoordsView *priv = view->priv;
    const guint *dimension_map = priv->coords_class->dimension_map;
    for (GSList *l = view->priv->column_info; l; l = g_slist_next(l)) {
        ColumnInfo *column_info = (ColumnInfo*)l->data;
        if (dimension_map[column_info->i] != d)
            continue;

        update_column_title(view, column_info);
    }
}

static void
update_column_title(GwyCoordsView *view,
                    ColumnInfo *column_info)
{
    CoordsView *priv = view->priv;
    const gchar *title = column_info->title;
    const gchar *units = NULL;

    if (!column_info->is_index) {
        if (priv->scale_type == GWY_COORD_SCALE_PIXEL) {
            units = "px";
        }
        else {
            guint i = column_info->i;
            guint d = priv->coords_class->dimension_map[i];
            units = priv->dim_info[d].units;
        }
    }

    gboolean has_title = title && *title;
    gboolean has_units = units && *units;

    const gchar *separ = priv->split_units ? "\n" : " ";
    const gchar *nosepar = priv->split_units ? "\n" : "";
    gchar *s = NULL;

    if (has_title && has_units)
        s = g_strconcat("<b>", title, "</b>", separ, "[", units, "]", NULL);
    else if (has_title)
        s = g_strconcat("<b>", title, "</b>", nosepar, NULL);
    else if (has_units)
        s = g_strconcat(nosepar, "[", units, "]", NULL);
    else
        s = g_strdup(nosepar);

    GtkWidget *header = gtk_tree_view_column_get_widget(column_info->column);
    gtk_label_set_markup(GTK_LABEL(header), s);
    g_free(s);
}

static gchar*
auto_column_title(const CoordsView *priv,
                  guint i)
{
    static const gchar *const names[] = {
        "x", "y", "z", "w", "v", "u"
    };

    const GwyCoordsClass *coords_class = priv->coords_class;
    const guint *dimension_map = coords_class->dimension_map;
    guint n = 0, d = dimension_map[i];
    const gchar *var = (d < G_N_ELEMENTS(names)) ? names[d] : "t";

    if (coords_class->dimension == coords_class->shape_size)
        return g_strdup(var);

    for (guint k = 0; k <= i; k++) {
        if (dimension_map[k] == d)
            n++;
    }

    return g_strdup_printf("%s<sub>%u</sub>", var, n);
}

static void
column_gone(gpointer data,
            GObject *where_the_object_was)
{
    GwyCoordsView *view = (GwyCoordsView*)data;
    CoordsView *priv = view->priv;
    GSList *l = find_column_info(view,
                                 (GtkTreeViewColumn*)where_the_object_was);
    g_return_if_fail(l);
    priv->column_info = g_slist_remove_link(priv->column_info, l);

    ColumnInfo *column_info = (ColumnInfo*)l->data;
    column_info->column = NULL;
    GWY_FREE(column_info->title);
    g_slist_free1(l);
}

static GSList*
find_column_info(const GwyCoordsView *view,
                 GtkTreeViewColumn *column)
{
    for (GSList *l = view->priv->column_info; l; l = g_slist_next(l)) {
        const ColumnInfo *column_info = (const ColumnInfo*)l->data;
        if (column_info->column == column)
            return l;
    }
    return NULL;
}

static void
shapes_selection_added(GwyCoordsView *view,
                       gint n,
                       G_GNUC_UNUSED GwyIntSet *intset)
{
    CoordsView *priv = view->priv;
    if (priv->sync_view_to_shapes)
        return;

    g_assert(n >= 0);
    GtkTreeView *treeview = GTK_TREE_VIEW(view);
    GtkTreeSelection *selection = gtk_tree_view_get_selection(treeview);
    GtkTreeIter iter;
    if (gwy_array_store_get_iter(priv->store, n, &iter)) {
        priv->sync_shapes_to_view = TRUE;
        gtk_tree_selection_select_iter(selection, &iter);
        priv->sync_shapes_to_view = FALSE;
    }
    else
        g_critical("Cannot get iter for item #%d", n);
}

static void
shapes_selection_removed(GwyCoordsView *view,
                         gint n,
                         G_GNUC_UNUSED GwyIntSet *intset)
{
    CoordsView *priv = view->priv;
    if (priv->sync_view_to_shapes)
        return;

    g_assert(n >= 0);
    GtkTreeView *treeview = GTK_TREE_VIEW(view);
    GtkTreeSelection *selection = gtk_tree_view_get_selection(treeview);
    GtkTreeIter iter;
    if (gwy_array_store_get_iter(priv->store, n, &iter)) {
        priv->sync_shapes_to_view = TRUE;
        gtk_tree_selection_unselect_iter(selection, &iter);
        priv->sync_shapes_to_view = FALSE;
    }
    else
        g_critical("Cannot get iter for item #%d", n);
}

static void
shapes_selection_assigned(GwyCoordsView *view,
                          G_GNUC_UNUSED GwyIntSet *intset)
{
    CoordsView *priv = view->priv;
    if (priv->sync_view_to_shapes)
        return;

    sync_selection_shapes_to_view(view);
}

static void
sync_selection_shapes_to_view(GwyCoordsView *view)
{
    CoordsView *priv = view->priv;

    GwyShapes *shapes = priv->shapes;
    if (!shapes) {
        clear_selection(view);
        return;
    }

    guint n = 0, len = gwy_coords_size(priv->coords);
    const GwyIntRange *ranges = gwy_int_set_ranges(priv->shapes_selection, &n);
    if (!n) {
        clear_selection(view);
        return;
    }

    GtkTreeView *treeview = GTK_TREE_VIEW(view);
    GtkTreeSelection *selection = gtk_tree_view_get_selection(treeview);
    priv->sync_shapes_to_view = TRUE;
    select_or_unselect_range(selection, 0, ranges[0].from, FALSE);
    for (guint i = 0; ; i++) {
        select_or_unselect_range(selection, ranges[i].from, ranges[i].to, TRUE);
        if (i == n-1)
            break;
        select_or_unselect_range(selection, ranges[i].to, ranges[i+1].from,
                                 FALSE);
    }
    select_or_unselect_range(selection, ranges[n-1].to, len, FALSE);
    priv->sync_shapes_to_view = FALSE;
}

static void
clear_selection(GwyCoordsView *view)
{
    GtkTreeView *treeview = GTK_TREE_VIEW(view);
    CoordsView *priv = view->priv;
    GtkTreeSelection *selection = gtk_tree_view_get_selection(treeview);
    priv->sync_shapes_to_view = TRUE;
    gtk_tree_selection_unselect_all(selection);
    priv->sync_shapes_to_view = FALSE;
}

static void
select_or_unselect_range(GtkTreeSelection *selection,
                         guint from, guint to,
                         gboolean select)
{
    if (from == to)
        return;

    GtkTreePath *pathfrom = gtk_tree_path_new();
    gtk_tree_path_append_index(pathfrom, from);
    if (from+1 == to) {
        if (select)
            gtk_tree_selection_select_path(selection, pathfrom);
        else
            gtk_tree_selection_unselect_path(selection, pathfrom);
    }
    else {
        GtkTreePath *pathto = gtk_tree_path_new();
        gtk_tree_path_append_index(pathto, to);
        if (select)
            gtk_tree_selection_select_range(selection, pathfrom, pathto);
        else
            gtk_tree_selection_unselect_range(selection, pathfrom, pathto);
        gtk_tree_path_free(pathto);
    }
    gtk_tree_path_free(pathfrom);
}

/**
 * SECTION: coords-view
 * @title: GwyCoordsView
 * @short_description: List view displaying coordinates of shapes
 *
 * #GwyCoordsView displays a list of coordinates of some geometrical shapes
 * represented by a #GwyCoords object.  The underlying #GtkTreeModel is
 * a #GwyArrayStore created automatically for given coords object.  This array
 * store is destroyed and created anew whenever a different coords object is
 * set.  So if you show different #GwyCoords objects in a single #GwyCoordsView
 * remember that you cannot assume that its tree view model stays the same.
 *
 * Instead of displaying mere #GwyCoords, #GwyCoordsView can also display
 * #GwyShapes which enables switching between real and pixel coordinates and
 * two-way synchronisation of the tree selection with shapes' selection.
 * Setting shapes with gwy_coords_view_set_shapes() automatically displays its
 * coords.  So do not use gwy_coords_view_set_coords() afterwards because it
 * would cause a switch back to plain #GwyCoords view.  In this case, the
 * coordinates must obviously be two-dimensional.
 *
 * Column setup methods such as gwy_coords_view_create_column_coord() create
 * tree view columns that are intended to be used only in the single widget
 * that created them.
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
