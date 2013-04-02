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
#include "libgwy/curve-statistics.h"
#include "libgwy/line-statistics.h"
#include "libgwy/object-utils.h"
#include "libgwyui/types.h"
#include "libgwyui/cairo-utils.h"
#include "libgwyui/graph-curve.h"

enum {
    PROP_0,
    PROP_LINE,
    PROP_CURVE,
    PROP_PLOT_TYPE,
    PROP_POINT_COLOR,
    PROP_POINT_TYPE,
    PROP_POINT_SIZE,
    PROP_LINE_COLOR,
    PROP_LINE_TYPE,
    PROP_LINE_WIDTH,
    N_PROPS
};

enum {
    SGNL_DATA_UPDATED,
    SGNL_UPDATED,
    N_SIGNALS
};

typedef struct _GwyGraphCurvePrivate GraphCurve;

struct _GwyGraphCurvePrivate {
    GwyCurve *curve;
    GwyLine *line;

    GwyRange xrange;
    GwyRange yrange;
    gboolean cached_range;

    GwyPlotType type;

    GwyRGBA point_color;
    GwyGraphPointType point_type;
    gdouble point_size;

    GwyRGBA line_color;
    GwyGraphLineType line_type;
    gdouble line_width;

    gulong notify_id;
    gulong data_changed_id;
};

static void     gwy_graph_curve_finalize    (GObject *object);
static void     gwy_graph_curve_dispose     (GObject *object);
static void     gwy_graph_curve_set_property(GObject *object,
                                             guint prop_id,
                                             const GValue *value,
                                             GParamSpec *pspec);
static void     gwy_graph_curve_get_property(GObject *object,
                                             guint prop_id,
                                             GValue *value,
                                             GParamSpec *pspec);
static gboolean set_curve                   (GwyGraphCurve *graphcurve,
                                             GwyCurve *curve);
static gboolean set_line                    (GwyGraphCurve *graphcurve,
                                             GwyLine *line);
static gboolean set_plot_type               (GwyGraphCurve *graphcurve,
                                             GwyPlotType plottype);
static gboolean set_point_color             (GwyGraphCurve *graphcurve,
                                             const GwyRGBA *color);
static gboolean set_point_type              (GwyGraphCurve *graphcurve,
                                             GwyGraphPointType type);
static gboolean set_point_size              (GwyGraphCurve *graphcurve,
                                             gdouble size);
static gboolean set_line_color              (GwyGraphCurve *graphcurve,
                                             const GwyRGBA *color);
static gboolean set_line_type               (GwyGraphCurve *graphcurve,
                                             GwyGraphLineType type);
static gboolean set_line_width              (GwyGraphCurve *graphcurve,
                                             gdouble width);
static void     curve_notify                (GwyGraphCurve *graphcurve,
                                             GParamSpec *pspec,
                                             GwyCurve *curve);
static void     curve_data_changed          (GwyGraphCurve *graphcurve,
                                             GwyCurve *curve);
static void     line_notify                 (GwyGraphCurve *graphcurve,
                                             GParamSpec *pspec,
                                             GwyLine *line);
static void     line_data_changed           (GwyGraphCurve *graphcurve,
                                             GwyLine *line);
static void     updated                     (GwyGraphCurve *graphcurve,
                                             const gchar *name);
static void     data_updated                (GwyGraphCurve *graphcurve);
static void     all_updated                 (GwyGraphCurve *graphcurve);
static void     ensure_ranges               (GraphCurve *priv);

static GParamSpec *properties[N_PROPS];
static guint signals[N_SIGNALS];

G_DEFINE_TYPE(GwyGraphCurve, gwy_graph_curve, G_TYPE_INITIALLY_UNOWNED);

static void
gwy_graph_curve_class_init(GwyGraphCurveClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    g_type_class_add_private(klass, sizeof(GraphCurve));

    gobject_class->finalize = gwy_graph_curve_finalize;
    gobject_class->dispose = gwy_graph_curve_dispose;
    gobject_class->get_property = gwy_graph_curve_get_property;
    gobject_class->set_property = gwy_graph_curve_set_property;

    properties[PROP_LINE]
        = g_param_spec_object("line",
                              "Line",
                              "Line object providing the data.",
                              GWY_TYPE_LINE,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_CURVE]
        = g_param_spec_object("curve",
                              "Curve",
                              "Curve object providing the data.",
                              GWY_TYPE_CURVE,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_PLOT_TYPE]
        = g_param_spec_enum("plot-type",
                            "Plot type",
                            "Basic type of data visualisation.",
                            GWY_TYPE_PLOT_TYPE,
                            GWY_PLOT_LINE,
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_POINT_TYPE]
        = g_param_spec_enum("point-type",
                            "Point type",
                            "Point type for point plots.",
                            GWY_TYPE_GRAPH_POINT_TYPE,
                            GWY_GRAPH_POINT_SQUARE,
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_POINT_COLOR]
        = g_param_spec_boxed("point-color",
                             "Point color",
                             "Point color for point plots.",
                             GWY_TYPE_RGBA,
                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_POINT_SIZE]
        = g_param_spec_double("point-size",
                              "Point size",
                              "Point size for point plots.",
                              0.0, G_MAXDOUBLE, 5.0,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_LINE_TYPE]
        = g_param_spec_enum("line-type",
                            "Line type",
                            "Line type for line plots.",
                            GWY_TYPE_GRAPH_LINE_TYPE,
                            GWY_GRAPH_LINE_SOLID,
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_LINE_COLOR]
        = g_param_spec_boxed("line-color",
                             "Line color",
                             "Line color for line plots.",
                             GWY_TYPE_RGBA,
                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_LINE_WIDTH]
        = g_param_spec_double("line-width",
                              "Line width",
                              "Line width for line plots.",
                              0.0, G_MAXDOUBLE, 1.0,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    for (guint i = 1; i < N_PROPS; i++)
        g_object_class_install_property(gobject_class, i, properties[i]);

    /**
     * GwyGraphCurve::updated:
     * @gwygraphcurve: The #GwyGraphCurve which received the signal.
     * @arg1: Name of the updated property of the data backend.
     *
     * The ::updated signal is emitted when properties of the curve data
     * backend change.
     **/
    signals[SGNL_UPDATED]
        = g_signal_new_class_handler("updated",
                                     G_OBJECT_CLASS_TYPE(klass),
                                     G_SIGNAL_RUN_FIRST | G_SIGNAL_DETAILED,
                                     NULL, NULL, NULL,
                                     g_cclosure_marshal_VOID__STRING,
                                     G_TYPE_NONE, 1,
                                     G_TYPE_STRING);

    /**
     * GwyGraphCurve::data-updated:
     * @gwygraphcurve: The #GwyGraphCurve which received the signal.
     *
     * The ::data-updated signal is emitted when the curve data change.
     **/
    signals[SGNL_DATA_UPDATED]
        = g_signal_new_class_handler("data-updated",
                                     G_OBJECT_CLASS_TYPE(klass),
                                     G_SIGNAL_RUN_FIRST,
                                     NULL, NULL, NULL,
                                     g_cclosure_marshal_VOID__VOID,
                                     G_TYPE_NONE, 0);
}

static void
gwy_graph_curve_init(GwyGraphCurve *graphcurve)
{
    graphcurve->priv = G_TYPE_INSTANCE_GET_PRIVATE(graphcurve,
                                                   GWY_TYPE_GRAPH_CURVE,
                                                   GraphCurve);
    GraphCurve *priv = graphcurve->priv;
    priv->type = GWY_PLOT_LINE;
    priv->point_type = GWY_GRAPH_POINT_SQUARE;
    priv->point_color = (GwyRGBA){ 0.0, 0.0, 0.0, 1.0 };
    priv->point_size = 5.0;
    priv->line_type = GWY_GRAPH_LINE_SOLID;
    priv->line_color = (GwyRGBA){ 0.0, 0.0, 0.0, 1.0 };
    priv->line_width = 1.0;
}

static void
gwy_graph_curve_finalize(GObject *object)
{
    GraphCurve *priv = GWY_GRAPH_CURVE(object)->priv;
    G_OBJECT_CLASS(gwy_graph_curve_parent_class)->finalize(object);
}

static void
gwy_graph_curve_dispose(GObject *object)
{
    GwyGraphCurve *graphcurve = GWY_GRAPH_CURVE(object);
    set_curve(graphcurve, NULL);
    set_line(graphcurve, NULL);
    G_OBJECT_CLASS(gwy_graph_curve_parent_class)->dispose(object);
}

static void
gwy_graph_curve_set_property(GObject *object,
                             guint prop_id,
                             const GValue *value,
                             GParamSpec *pspec)
{
    GwyGraphCurve *graphcurve = GWY_GRAPH_CURVE(object);

    switch (prop_id) {
        case PROP_CURVE:
        set_curve(graphcurve, g_value_get_object(value));
        break;

        case PROP_LINE:
        set_line(graphcurve, g_value_get_object(value));
        break;

        case PROP_PLOT_TYPE:
        set_plot_type(graphcurve, g_value_get_enum(value));
        break;

        case PROP_POINT_TYPE:
        set_point_type(graphcurve, g_value_get_enum(value));
        break;

        case PROP_POINT_COLOR:
        set_point_color(graphcurve, g_value_get_boxed(value));
        break;

        case PROP_POINT_SIZE:
        set_point_size(graphcurve, g_value_get_double(value));
        break;

        case PROP_LINE_TYPE:
        set_line_type(graphcurve, g_value_get_enum(value));
        break;

        case PROP_LINE_COLOR:
        set_line_color(graphcurve, g_value_get_boxed(value));
        break;

        case PROP_LINE_WIDTH:
        set_line_width(graphcurve, g_value_get_double(value));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_graph_curve_get_property(GObject *object,
                             guint prop_id,
                             GValue *value,
                             GParamSpec *pspec)
{
    GraphCurve *priv = GWY_GRAPH_CURVE(object)->priv;

    switch (prop_id) {
        case PROP_CURVE:
        g_value_set_object(value, priv->curve);
        break;

        case PROP_LINE:
        g_value_set_object(value, priv->line);
        break;

        case PROP_PLOT_TYPE:
        g_value_set_enum(value, priv->type);
        break;

        case PROP_POINT_TYPE:
        g_value_set_enum(value, priv->point_type);
        break;

        case PROP_POINT_COLOR:
        g_value_set_boxed(value, &priv->point_color);
        break;

        case PROP_POINT_SIZE:
        g_value_set_double(value, priv->point_size);
        break;

        case PROP_LINE_TYPE:
        g_value_set_enum(value, priv->line_type);
        break;

        case PROP_LINE_COLOR:
        g_value_set_boxed(value, &priv->line_color);
        break;

        case PROP_LINE_WIDTH:
        g_value_set_double(value, priv->line_width);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

/**
 * gwy_graph_curve_new:
 *
 * Creates a new graph curve.
 *
 * Returns: The newly created graph curve.
 **/
GwyGraphCurve*
gwy_graph_curve_new(void)
{
    return (GwyGraphCurve*)g_object_newv(GWY_TYPE_GRAPH_CURVE, 0, NULL);
}

/**
 * gwy_graph_curve_get_curve:
 * @graphcurve: A graph curve.
 *
 * Gets the curve providing the data for a graph curve.
 *
 * Returns: (allow-none):
 *          The curve object backing @graphcurve, or %NULL.
 **/
GwyCurve*
gwy_graph_curve_get_curve(const GwyGraphCurve *graphcurve)
{
    g_return_val_if_fail(GWY_IS_GRAPH_CURVE(graphcurve), NULL);
    return graphcurve->priv->curve;
}

/**
 * gwy_graph_curve_set_curve:
 * @graphcurve: A graph curve.
 * @curve: (allow-none):
 *         The curve object that should provide data for @graphcurve.
 *         Any #GwyLine backend is automatically unset.
 *
 * Sets the curve providing the data for a graph curve.
 **/
void
gwy_graph_curve_set_curve(GwyGraphCurve *graphcurve,
                          GwyCurve *curve)
{
    g_return_if_fail(GWY_IS_GRAPH_CURVE(graphcurve));
    if (!set_curve(graphcurve, curve))
        return;

    g_object_notify_by_pspec(G_OBJECT(graphcurve), properties[PROP_CURVE]);
}

/**
 * gwy_graph_curve_get_line:
 * @graphcurve: A graph curve.
 *
 * Gets the line providing the data for a graph curve.
 *
 * Returns: (allow-none):
 *          The line object backing @graphcurve, or %NULL.
 **/
GwyLine*
gwy_graph_curve_get_line(const GwyGraphCurve *graphcurve)
{
    g_return_val_if_fail(GWY_IS_GRAPH_CURVE(graphcurve), NULL);
    return graphcurve->priv->line;
}

/**
 * gwy_graph_curve_set_line:
 * @graphcurve: A graph curve.
 * @line: (allow-none):
 *         The curve object that should provide data for @graphcurve.
 *         Any #GwyLine backend is automatically unset.
 *
 * Sets the line providing the data for a graph curve.
 **/
void
gwy_graph_curve_set_line(GwyGraphCurve *graphcurve,
                         GwyLine *line)
{
    g_return_if_fail(GWY_IS_GRAPH_CURVE(graphcurve));
    if (!set_line(graphcurve, line))
        return;

    g_object_notify_by_pspec(G_OBJECT(graphcurve), properties[PROP_LINE]);
}

/**
 * gwy_graph_curve_xrange:
 * @graphcurve: A graph curve.
 * @range: (out):
 *         Location to fill with the abscissa data range.
 *         If there is no data the range is set to (0.0, 0.0).
 *
 * Obtains the abscissa range of a graph curve data.
 *
 * Returns: %TRUE if the curve has any data.
 **/
gboolean
gwy_graph_curve_xrange(const GwyGraphCurve *graphcurve,
                       GwyRange *range)
{
    g_return_val_if_fail(GWY_IS_GRAPH_CURVE(graphcurve), FALSE);
    g_return_val_if_fail(range, FALSE);
    GraphCurve *priv = graphcurve->priv;
    ensure_ranges(priv);
    *range = priv->xrange;
    return priv->curve || priv->line;
}

/**
 * gwy_graph_curve_get_yrange:
 * @graphcurve: A graph curve.
 * @range: (out):
 *         Location to fill with the ordinate data range.
 *         If there is no data the range is set to (0.0, 0.0).
 *
 * Obtains the ordinate range of a graph curve data.
 *
 * Returns: %TRUE if the curve has any data.
 **/
gboolean
gwy_graph_curve_yrange(const GwyGraphCurve *graphcurve,
                       GwyRange *range)
{
    g_return_val_if_fail(GWY_IS_GRAPH_CURVE(graphcurve), FALSE);
    g_return_val_if_fail(range, FALSE);
    GraphCurve *priv = graphcurve->priv;
    ensure_ranges(priv);
    *range = priv->yrange;
    return priv->curve || priv->line;
}

/**
 * gwy_graph_curve_draw:
 * @graphcurve: A graph curve.
 * @cr: 
 * @grapharea: 
 *
 * Draws a graph curve to a graph area.
 *
 * This function is namely useful for graph area implementation.  It should not
 * be requred that @cr is a Cairo context created for @grapharea.
 *
 * FIXME: This may be a bad idea. Drawing the curve requires some ranges,
 * scales (linear/log) and similar stuff.  To make future extensions possible
 * we must pass SOMETHING that can be queried to obtain this information.
 * The graph area can provide it but what if we want to render to PDF instead?
 * We must be able to get at least the bounding box that corresponds to the
 * area ranges -- this one might be passes as the function argument.
 **/
void
gwy_graph_curve_draw(const GwyGraphCurve *graphcurve,
                     cairo_t *cr,
                     const GwyGraphArea *grapharea)
{
    g_return_if_fail(GWY_IS_GRAPH_CURVE(graphcurve));
    g_return_if_fail(GWY_IS_GRAPH_AREA(grapharea));
    g_return_if_fail(cr);
    // TODO:
}

static gboolean
set_curve(GwyGraphCurve *graphcurve,
          GwyCurve *curve)
{
    GraphCurve *priv = graphcurve->priv;
    // Fast path for cross-data backend clearing.
    if (!priv->curve && !curve)
        return FALSE;
    set_line(graphcurve, NULL);
    if (!gwy_set_member_object(graphcurve, curve, GWY_TYPE_CURVE,
                               &priv->curve,
                               "notify", &curve_notify,
                               &priv->notify_id,
                               G_CONNECT_SWAPPED,
                               "data-changed", &curve_data_changed,
                               &priv->data_changed_id,
                               G_CONNECT_SWAPPED,
                               NULL))
        return FALSE;

    all_updated(graphcurve);
    return TRUE;
}

static gboolean
set_line(GwyGraphCurve *graphcurve,
         GwyLine *line)
{
    GraphCurve *priv = graphcurve->priv;
    // Fast path for cross-data backend clearing.
    if (!priv->line && !line)
        return FALSE;
    set_curve(graphcurve, NULL);
    if (!gwy_set_member_object(graphcurve, line, GWY_TYPE_LINE,
                               &priv->line,
                               "notify", &line_notify,
                               &priv->notify_id,
                               G_CONNECT_SWAPPED,
                               "data-changed", &line_data_changed,
                               &priv->data_changed_id,
                               G_CONNECT_SWAPPED,
                               NULL))
        return FALSE;

    all_updated(graphcurve);
    return TRUE;
}

static gboolean
set_plot_type(GwyGraphCurve *graphcurve,
              GwyPlotType plottype)
{
    GraphCurve *priv = graphcurve->priv;
    if (priv->type == plottype)
        return FALSE;

    priv->type = plottype;
    return TRUE;
}

static gboolean
set_point_color(GwyGraphCurve *graphcurve,
                const GwyRGBA *color)
{
    GraphCurve *priv = graphcurve->priv;
    if (color->r == priv->point_color.r
        && color->g == priv->point_color.g
        && color->b == priv->point_color.b
        && color->a == priv->point_color.a)
        return FALSE;

    priv->point_color = *color;
    return TRUE;
}

static gboolean
set_point_type(GwyGraphCurve *graphcurve,
               GwyGraphPointType type)
{
    GraphCurve *priv = graphcurve->priv;
    if (priv->point_type == type)
        return FALSE;

    priv->point_type = type;
    return TRUE;
}

static gboolean
set_point_size(GwyGraphCurve *graphcurve,
               gdouble size)
{
    GraphCurve *priv = graphcurve->priv;
    if (priv->point_size == size)
        return FALSE;

    priv->point_size = size;
    return TRUE;
}

static gboolean
set_line_color(GwyGraphCurve *graphcurve,
               const GwyRGBA *color)
{
    GraphCurve *priv = graphcurve->priv;
    if (color->r == priv->line_color.r
        && color->g == priv->line_color.g
        && color->b == priv->line_color.b
        && color->a == priv->line_color.a)
        return FALSE;

    priv->line_color = *color;
    return TRUE;
}

static gboolean
set_line_type(GwyGraphCurve *graphcurve,
              GwyGraphLineType type)
{
    GraphCurve *priv = graphcurve->priv;
    if (priv->line_type == type)
        return FALSE;

    priv->line_type = type;
    return TRUE;
}

static gboolean
set_line_width(GwyGraphCurve *graphcurve,
               gdouble width)
{
    GraphCurve *priv = graphcurve->priv;
    if (priv->line_width == width)
        return FALSE;

    priv->line_width = width;
    return TRUE;
}

static void
curve_notify(GwyGraphCurve *graphcurve,
             GParamSpec *pspec,
             GwyCurve *curve)
{
    g_assert(curve == graphcurve->priv->curve);
    if (gwy_strequal(pspec->name, "n-points"))
        data_updated(graphcurve);
    else
        updated(graphcurve, pspec->name);
}

static void
curve_data_changed(GwyGraphCurve *graphcurve,
                   GwyCurve *curve)
{
    g_assert(curve == graphcurve->priv->curve);
    data_updated(graphcurve);
}

static void
line_notify(GwyGraphCurve *graphcurve,
            GParamSpec *pspec,
            GwyLine *line)
{
    g_assert(line == graphcurve->priv->line);
    if (gwy_stramong(pspec->name, "res", "real", "offset", NULL))
        data_updated(graphcurve);
    else
        updated(graphcurve, pspec->name);
}

static void
line_data_changed(GwyGraphCurve *graphcurve,
                  GwyLine *line)
{
    g_assert(line == graphcurve->priv->line);
    data_updated(graphcurve);
}

static void
updated(GwyGraphCurve *graphcurve, const gchar *name)
{
    g_signal_emit(graphcurve, signals[SGNL_UPDATED],
                  g_quark_from_string(name), name);
}

static void
data_updated(GwyGraphCurve *graphcurve)
{
    graphcurve->priv->cached_range = FALSE;
    g_signal_emit(graphcurve, signals[SGNL_DATA_UPDATED], 0);
}

static void
all_updated(GwyGraphCurve *graphcurve)
{
    g_signal_emit(graphcurve, signals[SGNL_UPDATED],
                  g_quark_from_static_string("xunit"), "xunit");
    g_signal_emit(graphcurve, signals[SGNL_UPDATED],
                  g_quark_from_static_string("yunit"), "yunit");
    g_signal_emit(graphcurve, signals[SGNL_UPDATED],
                  g_quark_from_static_string("name"), "name");
    data_updated(graphcurve);
}

static void
ensure_ranges(GraphCurve *priv)
{
    if (priv->cached_range)
        return;

    if (priv->curve) {
        gwy_curve_range_full(priv->curve,
                             &priv->xrange.from, &priv->xrange.to);
        gwy_curve_min_max_full(priv->curve,
                               &priv->yrange.from, &priv->yrange.to);
    }
    else if (priv->line) {
        const GwyLine *line = priv->line;
        gdouble dx = gwy_line_dx(line);
        priv->xrange.from = line->off + 0.5*dx;
        priv->xrange.to = priv->xrange.from + dx*(line->res - 1);
        gwy_line_min_max_full(line, &priv->yrange.from, &priv->yrange.to);
    }
    else {
        priv->xrange = (GwyRange){ 0.0, 0.0 };
        priv->yrange = (GwyRange){ 0.0, 0.0 };
    }
    priv->cached_range = TRUE;
}

/**
 * SECTION: graph-curve
 * @title: GwyGraphCurve
 * @short_description: Single graph curve
 *
 * #GwyGraphCurve represents a single graph curve in #GwyGraph.  It defines
 * the plot style, point type, line width, colours, etc.  The actual data is
 * provided either by #GwyCurve or #GwyLine.  Setting either as the data
 * backend automatically drops the other which is usually what you want.
 * In order to make a graph curve data-less you need to call both
 * gwy_graph_curve_set_curve() and gwy_graph_curve_set_line() with %NULL
 * arguments.
 *
 * It is possible, though not common, to put a #GwyGraphCurve to more than one
 * #GwyGraph.
 **/

/**
 * GwyGraphCurve:
 *
 * Object representing a graph curve.
 *
 * The #GwyGraphCurve struct contains private data only and should be
 * accessed using the functions below.
 **/

/**
 * GwyGraphCurveClass:
 *
 * Class of graph curves.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
