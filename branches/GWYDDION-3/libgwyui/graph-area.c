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
#include "libgwy/listable.h"
#include "libgwy/object-utils.h"
#include "libgwyui/types.h"
#include "libgwyui/cairo-utils.h"
#include "libgwyui/graph-area.h"
#include "libgwyui/graph-internal.h"

#define curve_proxy_index(a, i) g_array_index((a), CurveProxy, (i))

enum {
    PROP_0,
    PROP_XSCALE,
    PROP_YSCALE,
    PROP_XRANGE,
    PROP_YRANGE,
    PROP_SHOW_XGRID,
    PROP_SHOW_YGRID,
    N_PROPS
};

typedef struct {
    GwyGraphCurve *curve;
    gulong notify_id;
    gulong updated_id;
    gulong data_updated_id;
} CurveProxy;

typedef struct _GwyGraphAreaPrivate GraphArea;

struct _GwyGraphAreaPrivate {
    GdkWindow *input_window;
    GArray *curves;  // of CurveProxy
    GArray *xgrid;   // of gdouble
    GArray *ygrid;
    GwyRange xrange;  // Plotting ranges
    GwyRange yrange;
    GwyGraphDataRange xdatarange;  // Data ranges
    GwyGraphDataRange ydatarange;
    GwyGraphScaleType xscale;
    GwyGraphScaleType yscale;
    gboolean show_xgrid;
    gboolean show_ygrid;
};

static void     gwy_graph_area_listable_init(GwyListableInterface *iface);
static void     gwy_graph_area_finalize     (GObject *object);
static void     gwy_graph_area_dispose      (GObject *object);
static void     gwy_graph_area_set_property (GObject *object,
                                             guint prop_id,
                                             const GValue *value,
                                             GParamSpec *pspec);
static void     gwy_graph_area_get_property (GObject *object,
                                             guint prop_id,
                                             GValue *value,
                                             GParamSpec *pspec);
static void     gwy_graph_area_realize      (GtkWidget *widget);
static void     gwy_graph_area_unrealize    (GtkWidget *widget);
static void     gwy_graph_area_map          (GtkWidget *widget);
static void     gwy_graph_area_unmap        (GtkWidget *widget);
static void     gwy_graph_area_size_allocate(GtkWidget *widget,
                                             GtkAllocation *allocation);
static gboolean gwy_graph_area_draw         (GtkWidget *widget,
                                             cairo_t *cr);
static void     create_input_window         (GwyGraphArea *grapharea);
static void     destroy_input_window        (GwyGraphArea *grapharea);
static gboolean set_xscale                  (GwyGraphArea *grapharea,
                                             GwyGraphScaleType scale);
static gboolean set_yscale                  (GwyGraphArea *grapharea,
                                             GwyGraphScaleType scale);
static gboolean set_xrange                  (GwyGraphArea *grapharea,
                                             const GwyRange *range);
static gboolean set_yrange                  (GwyGraphArea *grapharea,
                                             const GwyRange *range);
static gboolean set_show_xgrid              (GwyGraphArea *grapharea,
                                             gboolean show);
static gboolean set_show_ygrid              (GwyGraphArea *grapharea,
                                             gboolean show);
static gboolean set_grid                    (GArray **pgrid,
                                             const gdouble *ticks,
                                             guint n);
static gboolean set_curve                   (GwyGraphArea *grapharea,
                                             GwyGraphCurve *graphcurve,
                                             guint pos);
static void     curve_notify                (GwyGraphArea *grapharea,
                                             GParamSpec *pspec,
                                             GwyGraphCurve *graphcurve);
static void     curve_updated               (GwyGraphArea *grapharea,
                                             const gchar *name,
                                             GwyGraphCurve *graphcurve);
static void     curve_data_updated          (GwyGraphArea *grapharea,
                                             GwyGraphCurve *graphcurve);
static void     draw_xgrid                  (const GwyGraphArea *grapharea,
                                             cairo_t *cr,
                                             const cairo_rectangle_int_t *rect);
static void     draw_ygrid                  (const GwyGraphArea *grapharea,
                                             cairo_t *cr,
                                             const cairo_rectangle_int_t *rect);
static void     draw_curves                 (const GwyGraphArea *grapharea,
                                             cairo_t *cr,
                                             const cairo_rectangle_int_t *rect);
static void     setup_grid_style            (cairo_t *cr);
static void     ensure_data_xrange          (GraphArea *priv);
static void     ensure_data_yrange          (GraphArea *priv);
static guint    listable_size               (const GwyListable *listable);
static gpointer listable_get                (const GwyListable *listable,
                                             guint pos);

static GParamSpec *properties[N_PROPS];
static const GwyRange default_range = { 0.1, 1.0 };
static const CurveProxy null_proxy = { NULL, 0L, 0L, 0L };

G_DEFINE_TYPE_EXTENDED(GwyGraphArea, gwy_graph_area, GTK_TYPE_WIDGET, 0,
                       GWY_IMPLEMENT_LISTABLE(gwy_graph_area_listable_init));

static void
gwy_graph_area_class_init(GwyGraphAreaClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

    g_type_class_add_private(klass, sizeof(GraphArea));

    gobject_class->finalize = gwy_graph_area_finalize;
    gobject_class->dispose = gwy_graph_area_dispose;
    gobject_class->get_property = gwy_graph_area_get_property;
    gobject_class->set_property = gwy_graph_area_set_property;

    widget_class->realize = gwy_graph_area_realize;
    widget_class->unrealize = gwy_graph_area_unrealize;
    widget_class->map = gwy_graph_area_map;
    widget_class->unmap = gwy_graph_area_unmap;
    widget_class->size_allocate = gwy_graph_area_size_allocate;
    widget_class->draw = gwy_graph_area_draw;

    properties[PROP_XSCALE]
        = g_param_spec_enum("xscale",
                            "X scale",
                            "Scale type (linear, logarithmic) of the abscissa.",
                            GWY_TYPE_GRAPH_SCALE_TYPE,
                            GWY_GRAPH_SCALE_LINEAR,
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_YSCALE]
        = g_param_spec_enum("yscale",
                            "Y scale",
                            "Scale type (linear, logarithmic) of the ordinate.",
                            GWY_TYPE_GRAPH_SCALE_TYPE,
                            GWY_GRAPH_SCALE_LINEAR,
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_XRANGE]
        = g_param_spec_boxed("xrange",
                             "X range",
                             "Range of the abscissa.",
                             GWY_TYPE_RANGE,
                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_YRANGE]
        = g_param_spec_boxed("yrange",
                             "Y range",
                             "Range of the ordinate.",
                             GWY_TYPE_RANGE,
                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_SHOW_XGRID]
        = g_param_spec_boolean("show-xgrid",
                               "Show X grid",
                               "Whether to draw vertical grid lines.",
                               TRUE,
                               G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_SHOW_YGRID]
        = g_param_spec_boolean("show-ygrid",
                               "Show Y grid",
                               "Whether to draw horizontal grid lines.",
                               TRUE,
                               G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    for (guint i = 1; i < N_PROPS; i++)
        g_object_class_install_property(gobject_class, i, properties[i]);
}

static void
gwy_graph_area_listable_init(GwyListableInterface *iface)
{
    iface->get = listable_get;
    iface->size = listable_size;
}

static void
gwy_graph_area_init(GwyGraphArea *grapharea)
{
    grapharea->priv = G_TYPE_INSTANCE_GET_PRIVATE(grapharea,
                                                  GWY_TYPE_GRAPH_AREA,
                                                  GraphArea);
    GraphArea *priv = grapharea->priv;
    priv->curves = g_array_new(FALSE, TRUE, sizeof(CurveProxy));
    priv->xrange = priv->yrange = default_range;
    priv->show_xgrid = priv->show_ygrid = TRUE;
    gtk_widget_set_has_window(GTK_WIDGET(grapharea), FALSE);
}

static void
gwy_graph_area_finalize(GObject *object)
{
    GraphArea *priv = GWY_GRAPH_AREA(object)->priv;
    GWY_ARRAY_FREE(priv->xgrid);
    GWY_ARRAY_FREE(priv->ygrid);
    g_array_free(priv->curves, TRUE);
    G_OBJECT_CLASS(gwy_graph_area_parent_class)->finalize(object);
}

static void
gwy_graph_area_dispose(GObject *object)
{
    GwyGraphArea *grapharea = GWY_GRAPH_AREA(object);
    GraphArea *priv = grapharea->priv;
    GArray *curves = priv->curves;
    while (curves->len) {
        set_curve(grapharea, NULL, curves->len-1);
        g_array_set_size(curves, curves->len-1);
    }
    G_OBJECT_CLASS(gwy_graph_area_parent_class)->dispose(object);
}

static void
gwy_graph_area_set_property(GObject *object,
                            guint prop_id,
                            const GValue *value,
                            GParamSpec *pspec)
{
    GwyGraphArea *grapharea = GWY_GRAPH_AREA(object);

    switch (prop_id) {
        case PROP_XSCALE:
        set_xscale(grapharea, g_value_get_enum(value));
        break;

        case PROP_YSCALE:
        set_yscale(grapharea, g_value_get_enum(value));
        break;

        case PROP_XRANGE:
        set_xrange(grapharea, g_value_get_boxed(value));
        break;

        case PROP_YRANGE:
        set_yrange(grapharea, g_value_get_boxed(value));
        break;

        case PROP_SHOW_XGRID:
        set_show_xgrid(grapharea, g_value_get_boolean(value));
        break;

        case PROP_SHOW_YGRID:
        set_show_ygrid(grapharea, g_value_get_boolean(value));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_graph_area_get_property(GObject *object,
                            guint prop_id,
                            GValue *value,
                            GParamSpec *pspec)
{
    GraphArea *priv = GWY_GRAPH_AREA(object)->priv;

    switch (prop_id) {
        case PROP_XSCALE:
        g_value_set_enum(value, priv->xscale);
        break;

        case PROP_YSCALE:
        g_value_set_enum(value, priv->yscale);
        break;

        case PROP_XRANGE:
        g_value_set_boxed(value, &priv->xrange);
        break;

        case PROP_YRANGE:
        g_value_set_boxed(value, &priv->yrange);
        break;

        case PROP_SHOW_XGRID:
        g_value_set_boolean(value, priv->show_xgrid);
        break;

        case PROP_SHOW_YGRID:
        g_value_set_boolean(value, priv->show_ygrid);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_graph_area_realize(GtkWidget *widget)
{
    GwyGraphArea *grapharea = GWY_GRAPH_AREA(widget);
    GTK_WIDGET_CLASS(gwy_graph_area_parent_class)->realize(widget);
    create_input_window(grapharea);
}

static void
gwy_graph_area_unrealize(GtkWidget *widget)
{
    GwyGraphArea *grapharea = GWY_GRAPH_AREA(widget);
    destroy_input_window(grapharea);
    GTK_WIDGET_CLASS(gwy_graph_area_parent_class)->unrealize(widget);
}

static void
gwy_graph_area_map(GtkWidget *widget)
{
    GwyGraphArea *grapharea = GWY_GRAPH_AREA(widget);
    GraphArea *priv = grapharea->priv;
    GTK_WIDGET_CLASS(gwy_graph_area_parent_class)->map(widget);
    if (priv->input_window)
        gdk_window_show(priv->input_window);
}

static void
gwy_graph_area_unmap(GtkWidget *widget)
{
    GwyGraphArea *grapharea = GWY_GRAPH_AREA(widget);
    GraphArea *priv = grapharea->priv;
    if (priv->input_window)
        gdk_window_hide(priv->input_window);
    GTK_WIDGET_CLASS(gwy_graph_area_parent_class)->unmap(widget);
}

static void
gwy_graph_area_size_allocate(GtkWidget *widget,
                             GtkAllocation *allocation)
{
    GwyGraphArea *grapharea = GWY_GRAPH_AREA(widget);
    GraphArea *priv = grapharea->priv;

    GTK_WIDGET_CLASS(gwy_graph_area_parent_class)->size_allocate(widget,
                                                                 allocation);
    if (priv->input_window)
        gdk_window_move_resize(priv->input_window,
                               allocation->x, allocation->y,
                               allocation->width, allocation->height);
}

static gboolean
gwy_graph_area_draw(GtkWidget *widget,
                    cairo_t *cr)
{
    GwyGraphArea *grapharea = GWY_GRAPH_AREA(widget);
    cairo_rectangle_int_t rect;
    gtk_widget_get_allocation(widget, &rect);
    cairo_save(cr);
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_paint(cr);
    cairo_restore(cr);
    draw_xgrid(grapharea, cr, &rect);
    draw_ygrid(grapharea, cr, &rect);
    draw_curves(grapharea, cr, &rect);
    return FALSE;
}

/**
 * gwy_graph_area_new:
 *
 * Creates a new graph curve.
 *
 * Returns: The newly created graph curve.
 **/
GtkWidget*
gwy_graph_area_new(void)
{
    return (GtkWidget*)g_object_newv(GWY_TYPE_GRAPH_AREA, 0, NULL);
}

/**
 * gwy_graph_area_add:
 * @grapharea: A graph area.
 * @graphcurve: A graph curve to add (after all already present curves).
 *
 * Adds a curve to a graph area.
 **/
void
gwy_graph_area_add(GwyGraphArea *grapharea,
                   GwyGraphCurve *graphcurve)
{
    g_return_if_fail(GWY_IS_GRAPH_AREA(grapharea));
    g_return_if_fail(GWY_IS_GRAPH_CURVE(graphcurve));
    GraphArea *priv = grapharea->priv;
    GArray *curves = priv->curves;
    g_array_append_val(curves, null_proxy);
    set_curve(grapharea, graphcurve, priv->curves->len-1);
    gwy_listable_item_inserted(GWY_LISTABLE(grapharea), priv->curves->len-1);
    gtk_widget_queue_draw(GTK_WIDGET(grapharea));
}

/**
 * gwy_graph_area_insert:
 * @grapharea: A graph area.
 * @graphcurve: A graph curve to insert.
 * @pos: Position to insert @graphcurve at.
 *
 * Inserts a curve to a graph area at given position.
 **/
void
gwy_graph_area_insert(GwyGraphArea *grapharea,
                      GwyGraphCurve *graphcurve,
                      guint pos)
{
    g_return_if_fail(GWY_IS_GRAPH_AREA(grapharea));
    g_return_if_fail(GWY_IS_GRAPH_CURVE(graphcurve));
    GraphArea *priv = grapharea->priv;
    GArray *curves = priv->curves;
    if (pos > curves->len) {
        g_warning("Insertion position %u is beyond the number of curves.", pos);
        pos = curves->len;
    }
    g_array_insert_val(curves, pos, null_proxy);
    set_curve(grapharea, graphcurve, pos);
    gwy_listable_item_inserted(GWY_LISTABLE(grapharea), pos);
    gtk_widget_queue_draw(GTK_WIDGET(grapharea));
}

/**
 * gwy_graph_area_remove:
 * @grapharea: A graph area.
 * @pos: Position to remove the curve from.
 *
 * Removes graph curve at given position from a graph area.
 **/
void
gwy_graph_area_remove(GwyGraphArea *grapharea,
                      guint pos)
{
    g_return_if_fail(GWY_IS_GRAPH_AREA(grapharea));
    GraphArea *priv = grapharea->priv;
    g_return_if_fail(pos < priv->curves->len);
    set_curve(grapharea, NULL, pos);
    g_array_remove_index(priv->curves, pos);
    gwy_listable_item_deleted(GWY_LISTABLE(grapharea), pos);
    gtk_widget_queue_draw(GTK_WIDGET(grapharea));
}

/**
 * gwy_graph_area_remove_curve:
 * @grapharea: A graph area.
 * @graphcurve: A graph curve to remove.
 *
 * Removes given graph curve object from a graph area.
 **/
void
gwy_graph_area_remove_curve(GwyGraphArea *grapharea,
                            GwyGraphCurve *graphcurve)
{
    gint i = gwy_graph_area_find(grapharea, graphcurve);
    if (i == -1) {
        g_warning("Graph curve %p not present in the graph area.", graphcurve);
        return;
    }
    gwy_graph_area_remove(grapharea, i);
}

/**
 * gwy_graph_area_get:
 * @grapharea: A graph area.
 * @pos: Position of the curve.
 *
 * Gets the curve at given position from a graph area.
 *
 * Returns: (transfer none):
 *          Curve at position @pos.
 **/
GwyGraphCurve*
gwy_graph_area_get(const GwyGraphArea *grapharea,
                   guint pos)
{
    g_return_val_if_fail(GWY_IS_GRAPH_AREA(grapharea), NULL);
    GraphArea *priv = grapharea->priv;
    g_return_val_if_fail(pos < priv->curves->len, NULL);
    return curve_proxy_index(priv->curves, pos).curve;
}

/**
 * gwy_graph_area_find:
 * @grapharea: A graph area.
 * @graphcurve: A graph curve to find.
 *
 * Finds a graph curve object in a graph area.
 *
 * Returns: Position of curve @graphcurve, -1 if it is not present in
 *          @grapharea.
 **/
gint
gwy_graph_area_find(const GwyGraphArea *grapharea,
                    const GwyGraphCurve *graphcurve)
{
    g_return_val_if_fail(GWY_IS_GRAPH_AREA(grapharea), -1);
    g_return_val_if_fail(GWY_IS_GRAPH_CURVE(graphcurve), -1);
    GraphArea *priv = grapharea->priv;
    GArray *curves = priv->curves;
    for (guint i = 0; i < curves->len; i++) {
        const CurveProxy *cproxy = &curve_proxy_index(curves, i);
        if (cproxy->curve == graphcurve)
            return i;
    }
    return -1;
}

/**
 * gwy_graph_area_n_curves:
 * @grapharea: A graph area.
 *
 * Obtains the number of curves in a graph area.
 *
 * Returns: The number of curves.
 **/
guint
gwy_graph_area_n_curves(const GwyGraphArea *grapharea)
{
    g_return_val_if_fail(GWY_IS_GRAPH_AREA(grapharea), 0);
    return grapharea->priv->curves->len;
}

/**
 * gwy_graph_area_set_xrange:
 * @grapharea: A graph area.
 * @range: New abscissa range.
 *
 * Sets the range of the abscissa of a graph area.
 **/
void
gwy_graph_area_set_xrange(GwyGraphArea *grapharea,
                          const GwyRange *range)
{
    g_return_if_fail(GWY_IS_GRAPH_AREA(grapharea));
    g_return_if_fail(range);
    if (!set_xrange(grapharea, range))
        return;

    g_object_notify_by_pspec(G_OBJECT(grapharea), properties[PROP_XRANGE]);
}

/**
 * gwy_graph_area_get_xrange:
 * @grapharea: A graph area.
 * @range: (out):
 *         Location to store the abscissa range.
 *
 * Gets the range of the abscissa of a graph area.
 **/
void
gwy_graph_area_get_xrange(const GwyGraphArea *grapharea,
                          GwyRange *range)
{
    g_return_if_fail(GWY_IS_GRAPH_AREA(grapharea));
    g_return_if_fail(range);
    *range = grapharea->priv->xrange;
}

/**
 * gwy_graph_area_set_yrange:
 * @grapharea: A graph area.
 * @range: New ordinate range.
 *
 * Sets the range of the ordinate of a graph area.
 **/
void
gwy_graph_area_set_yrange(GwyGraphArea *grapharea,
                          const GwyRange *range)
{
    g_return_if_fail(GWY_IS_GRAPH_AREA(grapharea));
    g_return_if_fail(range);
    if (!set_yrange(grapharea, range))
        return;

    g_object_notify_by_pspec(G_OBJECT(grapharea), properties[PROP_YRANGE]);
}

/**
 * gwy_graph_area_get_yrange:
 * @grapharea: A graph area.
 * @range: (out):
 *         Location to store the ordinate range.
 *
 * Gets the range of the ordinate of a graph area.
 **/
void
gwy_graph_area_get_yrange(const GwyGraphArea *grapharea,
                          GwyRange *range)
{
    g_return_if_fail(GWY_IS_GRAPH_AREA(grapharea));
    g_return_if_fail(range);
    *range = grapharea->priv->yrange;
}

/**
 * gwy_graph_area_full_xrange:
 * @grapharea: A graph area.
 * @range: (out):
 *         Location to store the abscissa range.
 *         If the function returns %FALSE its contents is undefined.
 *
 * Finds the full abscissa data range of all visible curves in a graph area.
 *
 * Curves with plot type %GWY_PLOT_HIDDEN do not influence the range.
 *
 * Returns: %TRUE if the there are any visible data points.
 **/
gboolean
gwy_graph_area_full_xrange(const GwyGraphArea *grapharea,
                           GwyRange *range)
{
    g_return_val_if_fail(GWY_IS_GRAPH_AREA(grapharea), FALSE);
    g_return_val_if_fail(range, FALSE);
    GraphArea *priv = grapharea->priv;
    ensure_data_xrange(priv);
    *range = priv->xdatarange.full;
    return priv->xdatarange.anypresent;
}

/**
 * gwy_graph_area_full_yrange:
 * @grapharea: A graph area.
 * @range: (out):
 *         Location to store the ordinate range.
 *         If the function returns %FALSE its contents is undefined.
 *
 * Finds the full ordinate data range of all visible curves in a graph area.
 *
 * Curves with plot type %GWY_PLOT_HIDDEN do not influence the range.
 *
 * Returns: %TRUE if the there are any visible data points.
 **/
gboolean
gwy_graph_area_full_yrange(const GwyGraphArea *grapharea,
                           GwyRange *range)
{
    g_return_val_if_fail(GWY_IS_GRAPH_AREA(grapharea), FALSE);
    g_return_val_if_fail(range, FALSE);
    GraphArea *priv = grapharea->priv;
    ensure_data_yrange(priv);
    *range = priv->ydatarange.full;
    return priv->ydatarange.anypresent;
}

/**
 * gwy_graph_area_full_xposrange:
 * @grapharea: A graph area.
 * @range: (out):
 *         Location to store the abscissa range.
 *         If the function returns %FALSE its contents is undefined.
 *
 * Finds the full positive abscissa data range of all visible curves in a graph
 * area.
 *
 * Curves with plot type %GWY_PLOT_HIDDEN and non-positive values do not
 * influence the range.
 *
 * Returns: %TRUE if the there are any visible data points.
 **/
gboolean
gwy_graph_area_full_xposrange(const GwyGraphArea *grapharea,
                              GwyRange *range)
{
    g_return_val_if_fail(GWY_IS_GRAPH_AREA(grapharea), FALSE);
    g_return_val_if_fail(range, FALSE);
    GraphArea *priv = grapharea->priv;
    ensure_data_xrange(priv);
    *range = priv->xdatarange.positive;
    return priv->xdatarange.pospresent;
}

/**
 * gwy_graph_area_full_yposrange:
 * @grapharea: A graph area.
 * @range: (out):
 *         Location to store the ordinate range.
 *         If the function returns %FALSE its contents is undefined.
 *
 * Finds the full positive ordinate data range of all visible curves in a graph
 * area.
 *
 * Curves with plot type %GWY_PLOT_HIDDEN and non-positive values do not
 * influence the range.
 *
 * Returns: %TRUE if the there are any visible data points.
 **/
gboolean
gwy_graph_area_full_yposrange(const GwyGraphArea *grapharea,
                              GwyRange *range)
{
    g_return_val_if_fail(GWY_IS_GRAPH_AREA(grapharea), FALSE);
    g_return_val_if_fail(range, FALSE);
    GraphArea *priv = grapharea->priv;
    ensure_data_yrange(priv);
    *range = priv->ydatarange.positive;
    return priv->ydatarange.pospresent;
}

/**
 * gwy_graph_area_set_xgrid:
 * @grapharea: A graph area.
 * @ticks: (array length=n):
 *         Abscissa values at which vertical lines should be drawn.
 * @n: Number of tick positions in @ticks.
 *
 * Sets the abscissa positions of vertical grid lines in a graph area.
 *
 * The usual convention is to draw grid lines at positions of major ticks of
 * the corresponding axis.  Ticks that are closer than half of line width from
 * the graph area edge will not be drawn.
 **/
void
gwy_graph_area_set_xgrid(GwyGraphArea *grapharea,
                         const gdouble *ticks,
                         guint n)
{
    g_return_if_fail(GWY_IS_GRAPH_AREA(grapharea));
    GraphArea *priv = grapharea->priv;
    if (!set_grid(&priv->xgrid, ticks, n))
        return;
    if (priv->show_xgrid)
        gtk_widget_queue_draw(GTK_WIDGET(grapharea));
}

/**
 * gwy_graph_area_set_ygrid:
 * @grapharea: A graph area.
 * @ticks: (array length=n):
 *         Ordinate values at which horizontal lines should be drawn.
 * @n: Number of tick positions in @ticks.
 *
 * Sets the ordinate positions of horizontal grid lines in a graph area.
 *
 * The usual convention is to draw grid lines at positions of major ticks of
 * the corresponding axis.  Ticks that are closer than half of line width from
 * the graph area edge will not be drawn.
 **/
void
gwy_graph_area_set_ygrid(GwyGraphArea *grapharea,
                         const gdouble *ticks,
                         guint n)
{
    g_return_if_fail(GWY_IS_GRAPH_AREA(grapharea));
    GraphArea *priv = grapharea->priv;
    if (!set_grid(&priv->ygrid, ticks, n))
        return;
    if (priv->show_ygrid)
        gtk_widget_queue_draw(GTK_WIDGET(grapharea));
}

/**
 * gwy_graph_area_get_xgrid:
 * @grapharea: A graph area.
 * @n: (out):
 *     Location where to store the number of returned ticks.
 *
 * Gets the abscissa positions of vertical grid lines in a graph area.
 *
 * Returns: (allow-none) (array length=n):
 *          Abscissa positions of vertical grid lines.  The returned array is
 *          owned by @grapharea and valid only until the ticks change.  If
 *          there are no ticks, %NULL is returned.
 **/
const gdouble*
gwy_graph_area_get_xgrid(GwyGraphArea *grapharea,
                         guint *n)
{
    g_return_val_if_fail(GWY_IS_GRAPH_AREA(grapharea), NULL);
    GArray *ticks = grapharea->priv->xgrid;
    GWY_MAYBE_SET(n, ticks ? ticks->len : 0);
    return ticks ? (const gdouble*)ticks->data : NULL;
}

/**
 * gwy_graph_area_get_ygrid:
 * @grapharea: A graph area.
 * @n: (out):
 *     Location where to store the number of returned ticks.
 *
 * Gets the ordinate positions of horizontal grid lines in a graph area.
 *
 * Returns: (allow-none) (array length=n):
 *          Abscissa positions of vertical grid lines.  The returned array is
 *          owned by @grapharea and valid only until the ticks change.  If
 *          there are no ticks, %NULL is returned.
 **/
const gdouble*
gwy_graph_area_get_ygrid(GwyGraphArea *grapharea,
                         guint *n)
{
    g_return_val_if_fail(GWY_IS_GRAPH_AREA(grapharea), NULL);
    GArray *ticks = grapharea->priv->ygrid;
    GWY_MAYBE_SET(n, ticks ? ticks->len : 0);
    return ticks ? (const gdouble*)ticks->data : NULL;
}

/**
 * gwy_graph_area_set_xscale:
 * @grapharea: A graph area.
 * @scale: Scale type for the abscissa.
 *
 * Sets the scale type for the abscissa of a graph area.
 **/
void
gwy_graph_area_set_xscale(GwyGraphArea *grapharea,
                          GwyGraphScaleType scale)
{
    g_return_if_fail(GWY_IS_GRAPH_AREA(grapharea));
    g_return_if_fail(scale <= GWY_GRAPH_SCALE_LOG);
    if (!set_xscale(grapharea, scale))
        return;

    g_object_notify_by_pspec(G_OBJECT(grapharea), properties[PROP_XSCALE]);
}

/**
 * gwy_graph_area_get_xscale:
 * @grapharea: A graph area.
 *
 * Gets the scale type for the abscissa of a graph area.
 *
 * Returns: The current abscissa scale type.
 **/
GwyGraphScaleType
gwy_graph_area_get_xscale(const GwyGraphArea *grapharea)
{
    g_return_val_if_fail(GWY_IS_GRAPH_AREA(grapharea), GWY_GRAPH_SCALE_LINEAR);
    return grapharea->priv->xscale;
}

/**
 * gwy_graph_area_set_yscale:
 * @grapharea: A graph area.
 * @scale: Scale type for the ordinate.
 *
 * Sets the scale type for the ordinate of a graph area.
 **/
void
gwy_graph_area_set_yscale(GwyGraphArea *grapharea,
                          GwyGraphScaleType scale)
{
    g_return_if_fail(GWY_IS_GRAPH_AREA(grapharea));
    g_return_if_fail(scale <= GWY_GRAPH_SCALE_LOG);
    if (!set_yscale(grapharea, scale))
        return;

    g_object_notify_by_pspec(G_OBJECT(grapharea), properties[PROP_YSCALE]);
}

/**
 * gwy_graph_area_get_yscale:
 * @grapharea: A graph area.
 *
 * Gets the scale type for the ordinate of a graph area.
 *
 * Returns: The current ordinate scale type.
 **/
GwyGraphScaleType
gwy_graph_area_get_yscale(const GwyGraphArea *grapharea)
{
    g_return_val_if_fail(GWY_IS_GRAPH_AREA(grapharea), GWY_GRAPH_SCALE_LINEAR);
    return grapharea->priv->yscale;
}

/**
 * gwy_graph_area_set_show_xgrid:
 * @grapharea: A graph area.
 * @show: %TRUE to show the vertical grid lines, %FALSE to hide them.
 *
 * Sets the visibility of vertical grid lines.
 *
 * The vertical grid lines represent abscissa ticks and are set with
 * gwy_graph_area_set_xgrid().
 **/
void
gwy_graph_area_set_show_xgrid(GwyGraphArea *grapharea,
                              gboolean show)
{
    g_return_if_fail(GWY_IS_GRAPH_AREA(grapharea));
    if (!set_show_xgrid(grapharea, show))
        return;

    g_object_notify_by_pspec(G_OBJECT(grapharea), properties[PROP_SHOW_XGRID]);
}

/**
 * gwy_graph_area_get_show_xgrid:
 * @grapharea: A graph area.
 *
 * Gets the visibility of vertical grid lines.
 *
 * Returns: %TRUE if vertical grid lines are visible, %FALSE if they are
 *          hidden.
 **/
gboolean
gwy_graph_area_get_show_xgrid(const GwyGraphArea *grapharea)
{
    g_return_val_if_fail(GWY_IS_GRAPH_AREA(grapharea), FALSE);
    return grapharea->priv->show_xgrid;
}

/**
 * gwy_graph_area_set_show_ygrid:
 * @grapharea: A graph area.
 * @show: %TRUE to show the horizontal grid lines, %FALSE to hide them.
 *
 * Sets the visibility of horizontal grid lines.
 *
 * The horizontal grid lines represent ordinare ticks and are set with
 * gwy_graph_area_set_ygrid().
 **/
void
gwy_graph_area_set_show_ygrid(GwyGraphArea *grapharea,
                              gboolean show)
{
    g_return_if_fail(GWY_IS_GRAPH_AREA(grapharea));
    if (!set_show_ygrid(grapharea, show))
        return;

    g_object_notify_by_pspec(G_OBJECT(grapharea), properties[PROP_SHOW_YGRID]);
}

/**
 * gwy_graph_area_get_show_ygrid:
 * @grapharea: A graph area.
 *
 * Gets the visibility of horizontal grid lines.
 *
 * Returns: %TRUE if horizontal grid lines are visible, %FALSE if they are
 *          hidden.
 **/
gboolean
gwy_graph_area_get_show_ygrid(const GwyGraphArea *grapharea)
{
    g_return_val_if_fail(GWY_IS_GRAPH_AREA(grapharea), FALSE);
    return grapharea->priv->show_ygrid;
}

static void
create_input_window(GwyGraphArea *grapharea)
{
    GraphArea *priv = grapharea->priv;
    GtkWidget *widget = GTK_WIDGET(grapharea);
    g_assert(gtk_widget_get_realized(widget));
    if (priv->input_window)
        return;

    GtkAllocation allocation;
    gtk_widget_get_allocation(widget, &allocation);
    GdkWindowAttr attributes = {
        .x = allocation.x,
        .y = allocation.y,
        .width = allocation.width,
        .height = allocation.height,
        .window_type = GDK_WINDOW_CHILD,
        .wclass = GDK_INPUT_ONLY,
        // TODO: Add events here?
        .event_mask = gtk_widget_get_events(widget),
    };
    gint attributes_mask = GDK_WA_X | GDK_WA_Y;
    priv->input_window = gdk_window_new(gtk_widget_get_window(widget),
                                        &attributes, attributes_mask);
    gdk_window_set_user_data(priv->input_window, widget);
}

static void
destroy_input_window(GwyGraphArea *grapharea)
{
    GraphArea *priv = grapharea->priv;
    if (!priv->input_window)
        return;
    gdk_window_set_user_data(priv->input_window, NULL);
    gdk_window_destroy(priv->input_window);
    priv->input_window = NULL;
}

static gboolean
set_xscale(GwyGraphArea *grapharea,
           GwyGraphScaleType scale)
{
    GraphArea *priv = grapharea->priv;
    if (priv->xscale == scale)
        return FALSE;

    priv->xscale = scale;
    return TRUE;
}

static gboolean
set_yscale(GwyGraphArea *grapharea,
           GwyGraphScaleType scale)
{
    GraphArea *priv = grapharea->priv;
    if (priv->yscale == scale)
        return FALSE;

    priv->yscale = scale;
    return TRUE;
}

static gboolean
set_xrange(GwyGraphArea *grapharea,
           const GwyRange *range)
{
    if (!range)
        range = &default_range;

    GraphArea *priv = grapharea->priv;
    if (gwy_equal(&priv->xrange, range))
        return FALSE;

    priv->xrange = *range;
    return TRUE;
}

static gboolean
set_yrange(GwyGraphArea *grapharea,
           const GwyRange *range)
{
    if (!range)
        range = &default_range;

    GraphArea *priv = grapharea->priv;
    if (gwy_equal(&priv->yrange, range))
        return FALSE;

    priv->yrange = *range;
    return TRUE;
}

static gboolean
set_show_xgrid(GwyGraphArea *grapharea,
               gboolean show)
{
    GraphArea *priv = grapharea->priv;
    if (!show == !priv->show_xgrid)
        return FALSE;

    priv->show_xgrid = !!show;
    return TRUE;
}

static gboolean
set_show_ygrid(GwyGraphArea *grapharea,
               gboolean show)
{
    GraphArea *priv = grapharea->priv;
    if (!show == !priv->show_xgrid)
        return FALSE;

    priv->show_xgrid = !!show;
    return TRUE;
}

static gboolean
set_grid(GArray **pgrid,
         const gdouble *ticks,
         guint n)
{
    g_return_val_if_fail(ticks || !n, FALSE);
    GArray *grid = *pgrid;
    if (!n && (!grid || !grid->len))
        return FALSE;

    if (!grid)
        grid = *pgrid = g_array_new(FALSE, FALSE, sizeof(gdouble));
    else if (n == grid->len && memcmp(grid->data, ticks, n*sizeof(gdouble)))
        return FALSE;

    if (ticks >= (const gdouble*)grid->data
        && ticks < (const gdouble*)grid->data + grid->len) {
        g_warning("Subarray of ticks passed to set_grid().  FIXME.");
    }
    g_array_set_size(grid, 0);
    g_array_append_vals(grid, ticks, n);
    return TRUE;
}

static gboolean
set_curve(GwyGraphArea *grapharea,
          GwyGraphCurve *graphcurve,
          guint pos)
{
    GraphArea *priv = grapharea->priv;
    CurveProxy *proxy = &curve_proxy_index(priv->curves, pos);
    if (!gwy_set_member_object(grapharea, graphcurve, GWY_TYPE_GRAPH_CURVE,
                               &proxy->curve,
                               "notify", &curve_notify,
                               &proxy->notify_id,
                               G_CONNECT_SWAPPED,
                               "updated", &curve_updated,
                               &proxy->updated_id,
                               G_CONNECT_SWAPPED,
                               "data-updated", &curve_data_updated,
                               &proxy->data_updated_id,
                               G_CONNECT_SWAPPED,
                               NULL))
        return FALSE;
    return TRUE;
}

static void
curve_notify(GwyGraphArea *grapharea,
             G_GNUC_UNUSED GParamSpec *pspec,
             GwyGraphCurve *graphcurve)
{
    gwy_listable_item_updated(GWY_LISTABLE(grapharea),
                              gwy_graph_area_find(grapharea, graphcurve));
    gtk_widget_queue_draw(GTK_WIDGET(grapharea));
}

static void
curve_updated(GwyGraphArea *grapharea,
              G_GNUC_UNUSED const gchar *name,
              GwyGraphCurve *graphcurve)
{
    gwy_listable_item_updated(GWY_LISTABLE(grapharea),
                              gwy_graph_area_find(grapharea, graphcurve));
}

static void
curve_data_updated(GwyGraphArea *grapharea,
                   GwyGraphCurve *graphcurve)
{
    gwy_listable_item_updated(GWY_LISTABLE(grapharea),
                              gwy_graph_area_find(grapharea, graphcurve));
    GwyPlotType plottype;
    g_object_get(graphcurve, "plot-type", &plottype, NULL);
    if (plottype != GWY_PLOT_HIDDEN)
        gtk_widget_queue_draw(GTK_WIDGET(grapharea));
}

static void
draw_xgrid(const GwyGraphArea *grapharea,
           cairo_t *cr,
           const cairo_rectangle_int_t *rect)
{
    GraphArea *priv = grapharea->priv;
    GArray *grid = priv->xgrid;
    GwyGraphScaleType scale = priv->xscale;
    if (!priv->show_xgrid || !grid || !grid->len)
        return;

    gdouble q, off;
    _gwy_graph_calculate_scaling(grapharea, rect, &q, &off, NULL, NULL);

    cairo_save(cr);
    setup_grid_style(cr);

    for (guint i = 0; i < grid->len; i++) {
        gdouble v = _gwy_graph_scale_data(g_array_index(grid, gdouble, i),
                                          scale);
        v = q*v + off;
        if (!isfinite(v) || !within_range(rect->x, rect->width, v, 1.0))
            continue;

        cairo_move_to(cr, v, rect->y);
        cairo_rel_line_to(cr, 0.0, rect->height);
    }

    cairo_stroke(cr);
    cairo_restore(cr);
}

static void
draw_ygrid(const GwyGraphArea *grapharea,
           cairo_t *cr,
           const cairo_rectangle_int_t *rect)
{
    GraphArea *priv = grapharea->priv;
    GArray *grid = priv->ygrid;
    GwyGraphScaleType scale = priv->yscale;
    if (!priv->show_ygrid || !grid || !grid->len)
        return;

    gdouble q, off;
    _gwy_graph_calculate_scaling(grapharea, rect, NULL, NULL, &q, &off);

    cairo_save(cr);
    setup_grid_style(cr);

    for (guint i = 0; i < grid->len; i++) {
        gdouble v = _gwy_graph_scale_data(g_array_index(grid, gdouble, i),
                                          scale);
        v = q*v + off;
        if (!isfinite(v) || !within_range(rect->y, rect->height, v, 1.0))
            continue;

        cairo_move_to(cr, rect->x, v);
        cairo_rel_line_to(cr, rect->width, 0.0);
    }

    cairo_stroke(cr);
    cairo_restore(cr);
}

static void
draw_curves(const GwyGraphArea *grapharea,
            cairo_t *cr,
            const cairo_rectangle_int_t *rect)
{
    GraphArea *priv = grapharea->priv;
    GArray *curves = priv->curves;

    for (guint i = 0; i < curves->len; i++) {
        const CurveProxy *proxy = &curve_proxy_index(curves, i);
        gwy_graph_curve_draw(proxy->curve, cr, rect, grapharea);
    }
}

static void
setup_grid_style(cairo_t *cr)
{
    cairo_set_source_rgb(cr, 0.85, 0.85, 0.85);
    cairo_set_line_width(cr, 1.0);
    gdouble dash[1] = { 2.0 };
    cairo_set_dash(cr, dash, 1, 0.0);
}

static void
ensure_data_xrange(GraphArea *priv)
{
    GwyGraphDataRange *datarange = &priv->xdatarange;
    if (datarange->cached)
        return;

    datarange->anypresent = datarange->pospresent = FALSE;
    GArray *curves = priv->curves;
    for (guint i = 0; i < curves->len; i++) {
        const CurveProxy *cproxy = &curve_proxy_index(curves, i);
        GwyPlotType plottype;
        g_object_get(cproxy->curve, "plot-type", &plottype, NULL);
        if (plottype == GWY_PLOT_HIDDEN)
            continue;

        GwyGraphDataRange range;
        range.anypresent = gwy_graph_curve_xrange(cproxy->curve, &range.full);
        range.pospresent = gwy_graph_curve_xposrange(cproxy->curve,
                                                     &range.positive);
        _gwy_graph_data_range_union(datarange, &range);
    }
}

static void
ensure_data_yrange(GraphArea *priv)
{
    GwyGraphDataRange *datarange = &priv->ydatarange;
    if (datarange->cached)
        return;

    datarange->anypresent = datarange->pospresent = FALSE;
    GArray *curves = priv->curves;
    for (guint i = 0; i < curves->len; i++) {
        const CurveProxy *cproxy = &curve_proxy_index(curves, i);
        GwyPlotType plottype;
        g_object_get(cproxy->curve, "plot-type", &plottype, NULL);
        if (plottype == GWY_PLOT_HIDDEN)
            continue;

        GwyGraphDataRange range;
        range.anypresent = gwy_graph_curve_yrange(cproxy->curve, &range.full);
        range.pospresent = gwy_graph_curve_yposrange(cproxy->curve,
                                                     &range.positive);
        _gwy_graph_data_range_union(datarange, &range);
    }
}

static guint
listable_size(const GwyListable *listable)
{
    return gwy_graph_area_n_curves(GWY_GRAPH_AREA(listable));
}

static gpointer
listable_get(const GwyListable *listable,
             guint pos)
{
    return gwy_graph_area_get(GWY_GRAPH_AREA(listable), pos);
}

/**
 * SECTION: graph-area
 * @title: GwyGraphArea
 * @short_description: Area of graph containing the plots
 *
 * #GwyGraphArea is the core part of a graph, containing the plotted data.
 * Individual plots in the graph are represented with #GwyGraphCurve objects
 * that carry the plot style for each plot and can encapsulate either
 * #GwyCurves or #GwyLines.  The graph area acts as a list of curves,
 * implementing #GwyListable.
 *
 * Sometimes the axis terminology may be confusing with respect to horizontal
 * and vertical because, for instance, the vertical grid lines corresponds to
 * values on the horizontal axis.  The rule used in function naming is simple:
 * if a setting is related to the abscissa it contains ‘x’ in its name and if
 * it is related to the ordinate it contains ‘y’.
 **/

/**
 * GwyGraphArea:
 *
 * Object representing a graph area.
 *
 * The #GwyGraphArea struct contains private data only and should be
 * accessed using the functions below.
 **/

/**
 * GwyGraphAreaClass:
 *
 * Class of graph areas.
 **/

/**
 * GwyGraphScaleType:
 * @GWY_GRAPH_SCALE_LINEAR: Linear mapping between values and screen
 *                          positions.  This gives equally sized error bars if
 *                          errors are independent on the value.
 * @GWY_GRAPH_SCALE_SQRT: Screen positions correspond to ‘signed square
 *                        roots’ of the value, see gwy_spow().  This gives
 *                        equally sized error bars if errors are proportional
 *                        to the square root of the value – like for most
 *                        histogram-like plots.
 * @GWY_GRAPH_SCALE_LOG: Screen positions correspond to logarithms of values.
 *                       This gives equally sized errors bars if errors are
 *                       proportional to the value.  Only positive values can
 *                       be plotted.
 *
 * Type of graph axis scaling functions.
 *
 * FIXME: This is exactly the same as #GwyScaleMappingType.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
