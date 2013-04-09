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
#include "libgwyui/graph-internal.h"

#define GGP(x) GWY_GRAPH_POINT_##x

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

typedef void (*SymbolDrawFunc)(cairo_t *cr,
                               gdouble x,
                               gdouble y,
                               gdouble halfside);

typedef struct {
    const GwyXY *xy;
    const gdouble *y;
    gdouble xoff;
    gdouble dx;
    guint n;
    guint i;
    GwyGraphScaleType xscale;
    GwyGraphScaleType yscale;
} GraphCurveIter;

typedef struct {
    SymbolDrawFunc draw;
    gdouble size_factor;
    GwyGraphPointType type;
    gboolean do_stroke : 1;
    gboolean do_fill : 1;
} CurveSymbolInfo;

typedef struct _GwyGraphCurvePrivate GraphCurve;

struct _GwyGraphCurvePrivate {
    GwyCurve *curve;
    GwyLine *line;

    // Data (not plotting) ranges.
    GwyGraphDataRange xrange;
    GwyGraphDataRange yrange;

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
static void     setup_line_type             (cairo_t *cr,
                                             gdouble linewidth,
                                             GwyGraphLineType type);
static void     draw_points                 (const GwyGraphCurve *graphcurve,
                                             cairo_t *cr,
                                             const cairo_rectangle_int_t *rect,
                                             const GwyGraphArea *grapharea);
static void     stroke_fill_point           (const CurveSymbolInfo *syminfo,
                                             cairo_t *cr);
static void     draw_lines                  (const GwyGraphCurve *graphcurve,
                                             cairo_t *cr,
                                             const cairo_rectangle_int_t *rect,
                                             const GwyGraphArea *grapharea);
static void     draw_star                   (cairo_t *cr,
                                             gdouble x,
                                             gdouble y,
                                             gdouble halfside);
static void     draw_circle                 (cairo_t *cr,
                                             gdouble x,
                                             gdouble y,
                                             gdouble halfside);
static gpointer check_symbol_table_sanity   (gpointer arg);
static gboolean graph_curve_iter_init       (const GwyGraphCurve *graphcurve,
                                             GraphCurveIter *iter,
                                             GwyGraphScaleType xscale,
                                             GwyGraphScaleType yscale);
static gboolean graph_curve_iter_get        (const GraphCurveIter *iter,
                                             gdouble *x,
                                             gdouble *y);
static gboolean graph_curve_iter_next       (GraphCurveIter *iter);

static GParamSpec *properties[N_PROPS];
static guint signals[N_SIGNALS];

static const CurveSymbolInfo symbol_table[] = {
    { gwy_cairo_cross,          1.2,  GGP(CROSS),                 TRUE,  FALSE, },
    { gwy_cairo_times,          0.84, GGP(TIMES),                 TRUE,  FALSE, },
    { draw_star,                1.0,  GGP(STAR),                  TRUE,  FALSE, },
    { gwy_cairo_square,         0.84, GGP(SQUARE),                TRUE,  FALSE, },
    { draw_circle,              1.0,  GGP(CIRCLE),                TRUE,  FALSE, },
    { gwy_cairo_diamond,        1.2,  GGP(DIAMOND),               TRUE,  FALSE, },
    { gwy_cairo_triangle_up,    1.0,  GGP(TRIANGLE_UP),           TRUE,  FALSE, },
    { gwy_cairo_triangle_down,  1.0,  GGP(TRIANGLE_DOWN),         TRUE,  FALSE, },
    { gwy_cairo_triangle_left,  1.0,  GGP(TRIANGLE_LEFT),         TRUE,  FALSE, },
    { gwy_cairo_triangle_right, 1.0,  GGP(TRIANGLE_RIGHT),        TRUE,  FALSE, },
    { gwy_cairo_square,         0.84, GGP(FILLED_SQUARE),         FALSE, TRUE,  },
    { draw_circle,              1.0,  GGP(DISC),                  FALSE, TRUE,  },
    { gwy_cairo_diamond,        1.2,  GGP(FILLED_DIAMOND),        FALSE, TRUE,  },
    { gwy_cairo_triangle_up,    1.0,  GGP(FILLED_TRIANGLE_UP),    FALSE, TRUE,  },
    { gwy_cairo_triangle_down,  1.0,  GGP(FILLED_TRIANGLE_DOWN),  FALSE, TRUE,  },
    { gwy_cairo_triangle_left,  1.0,  GGP(FILLED_TRIANGLE_LEFT),  FALSE, TRUE,  },
    { gwy_cairo_triangle_right, 1.0,  GGP(FILLED_TRIANGLE_RIGHT), FALSE, TRUE,  },
    { gwy_cairo_asterisk,       1.1,  GGP(ASTERISK),              TRUE,  FALSE, },
};

G_DEFINE_TYPE(GwyGraphCurve, gwy_graph_curve, G_TYPE_INITIALLY_UNOWNED);

static void
gwy_graph_curve_class_init(GwyGraphCurveClass *klass)
{
    static GOnce table_sanity_checked = G_ONCE_INIT;
    g_once(&table_sanity_checked, check_symbol_table_sanity, NULL);

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
    //GraphCurve *priv = GWY_GRAPH_CURVE(object)->priv;
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
 * Returns: (allow-none) (transfer none):
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
 * Returns: (allow-none) (transfer none):
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
 *         If there are no data points the contents of @range is undefined.
 *         They fields may be set to %NAN for instance.
 *
 * Obtains the abscissa range of a graph curve data.
 *
 * Returns: %TRUE if the curve has any data point.
 **/
gboolean
gwy_graph_curve_xrange(const GwyGraphCurve *graphcurve,
                       GwyRange *range)
{
    g_return_val_if_fail(GWY_IS_GRAPH_CURVE(graphcurve), FALSE);
    g_return_val_if_fail(range, FALSE);
    GraphCurve *priv = graphcurve->priv;
    ensure_ranges(priv);
    *range = priv->xrange.full;
    return priv->curve || priv->line;
}

/**
 * gwy_graph_curve_yrange:
 * @graphcurve: A graph curve.
 * @range: (out):
 *         Location to fill with the ordinate data range.
 *         If there are no data points the contents of @range is undefined.
 *         They fields may be set to %NAN for instance.
 *
 * Obtains the ordinate range of a graph curve data.
 *
 * Returns: %TRUE if the curve has any data point.
 **/
gboolean
gwy_graph_curve_yrange(const GwyGraphCurve *graphcurve,
                       GwyRange *range)
{
    g_return_val_if_fail(GWY_IS_GRAPH_CURVE(graphcurve), FALSE);
    g_return_val_if_fail(range, FALSE);
    GraphCurve *priv = graphcurve->priv;
    ensure_ranges(priv);
    *range = priv->yrange.full;
    return priv->curve || priv->line;
}

/**
 * gwy_graph_curve_xposrange:
 * @graphcurve: A graph curve.
 * @range: (out):
 *         Location to fill with the abscissa data range.
 *         If there are no data points with positive abscissa values the
 *         contents of @range is undefined.  They fields may be set to %NAN for
 *         instance.
 *
 * Obtains the abscissa range of a graph curve positive data.
 *
 * Returns: %TRUE if the curve has any data point with positive abscissa value.
 **/
gboolean
gwy_graph_curve_xposrange(const GwyGraphCurve *graphcurve,
                          GwyRange *range)
{
    g_return_val_if_fail(GWY_IS_GRAPH_CURVE(graphcurve), FALSE);
    g_return_val_if_fail(range, FALSE);
    GraphCurve *priv = graphcurve->priv;
    ensure_ranges(priv);
    range->from = priv->xrange.posmin;
    // The maximum value may be rubbish if we return FALSE but that's allowed.
    range->to = priv->xrange.full.to;
    return priv->xrange.pospresent;
}

/**
 * gwy_graph_curve_yposrange:
 * @graphcurve: A graph curve.
 * @range: (out):
 *         Location to fill with the ordinate positive data range.
 *         If there are no data points with positive ordinate values the
 *         contents of @range is undefined.  They fields may be set to %NAN for
 *         instance.
 *
 * Obtains the ordinate range of a graph curve positive data.
 *
 * Returns: %TRUE if the curve has any data point with positive ordinate value.
 **/
gboolean
gwy_graph_curve_yposrange(const GwyGraphCurve *graphcurve,
                          GwyRange *range)
{
    g_return_val_if_fail(GWY_IS_GRAPH_CURVE(graphcurve), FALSE);
    g_return_val_if_fail(range, FALSE);
    GraphCurve *priv = graphcurve->priv;
    ensure_ranges(priv);
    range->from = priv->yrange.posmin;
    // The maximum value may be rubbish if we return FALSE but that's allowed.
    range->to = priv->yrange.full.to;
    return priv->yrange.pospresent;
}

/**
 * gwy_graph_curve_name:
 * @graphcurve: A graph curve.
 *
 * Obtains the name of the backend data object.
 *
 * This is a convenience function.  The same information can be obtained by
 * querying the data backend directly.  Note, however, that this function
 * copies the name.
 *
 * Returns: (transfer full) (allow-none):
 *          A newly allocated string with the name, or %NULL.
 **/
gchar*
gwy_graph_curve_name(const GwyGraphCurve *graphcurve)
{
    g_return_val_if_fail(GWY_IS_GRAPH_CURVE(graphcurve), NULL);
    GraphCurve *priv = graphcurve->priv;
    if (priv->curve)
        return g_strdup(gwy_curve_get_name(priv->curve));
    if (priv->line)
        return g_strdup(gwy_line_get_name(priv->line));
    return NULL;
}

/**
 * gwy_graph_curve_xunit:
 * @graphcurve: A graph curve.
 * @unit: (transfer none):
 *        Unit to set to graph backend x units.  If there is no backend the
 *        unit is cleared.
 *
 * Obtains the abscissa unit of graph curve data backend.
 *
 * This is a convenience function.  The same information can be obtained by
 * querying the data backend directly.  Note, however, that this function
 * copies the unit.
 **/
void
gwy_graph_curve_xunit(const GwyGraphCurve *graphcurve,
                      GwyUnit *unit)
{
    g_return_if_fail(GWY_IS_GRAPH_CURVE(graphcurve));
    g_return_if_fail(GWY_IS_UNIT(unit));
    GraphCurve *priv = graphcurve->priv;
    if (priv->curve)
        gwy_unit_assign(unit, gwy_curve_get_xunit(priv->curve));
    else if (priv->line)
        gwy_unit_assign(unit, gwy_line_get_xunit(priv->line));
    else
        gwy_unit_clear(unit);
}

/**
 * gwy_graph_curve_yunit:
 * @graphcurve: A graph curve.
 * @unit: (transfer none):
 *        Unit to set to graph backend x units.  If there is no backend the
 *        unit is cleared.
 *
 * Obtains the ordinate unit of graph curve data backend.
 *
 * This is a convenience function.  The same information can be obtained by
 * querying the data backend directly.  Note, however, that this function
 * copies the unit.
 **/
void
gwy_graph_curve_yunit(const GwyGraphCurve *graphcurve,
                      GwyUnit *unit)
{
    g_return_if_fail(GWY_IS_GRAPH_CURVE(graphcurve));
    g_return_if_fail(GWY_IS_UNIT(unit));
    GraphCurve *priv = graphcurve->priv;
    if (priv->curve)
        gwy_unit_assign(unit, gwy_curve_get_yunit(priv->curve));
    else if (priv->line)
        gwy_unit_assign(unit, gwy_line_get_yunit(priv->line));
    else
        gwy_unit_clear(unit);
}

/**
 * gwy_graph_curve_draw:
 * @graphcurve: A graph curve.
 * @cr: Cairo context to draw to.  Often it is the @grapharea's context but
 *      it does not have to be.
 * @rect: Rectangle in Cairo user units representing the entire graph area.
 *        The current clip rectangle must lie entirely within.
 * @grapharea: Graph area determing the ranges, linear/logaritimic scale types
 *             and similar global graph properties.
 *
 * Draws a graph curve to a graph area.
 *
 * This function is namely useful for graph area implementation.
 **/
void
gwy_graph_curve_draw(const GwyGraphCurve *graphcurve,
                     cairo_t *cr,
                     const cairo_rectangle_int_t *rect,
                     const GwyGraphArea *grapharea)
{
    g_return_if_fail(GWY_IS_GRAPH_CURVE(graphcurve));
    g_return_if_fail(GWY_IS_GRAPH_AREA(grapharea));
    g_return_if_fail(cr);

    GraphCurve *priv = graphcurve->priv;
    if (!priv->line && !priv->curve)
        return;

    // For logscale just replace range values with their logarithms and then
    // use log(x) in place of x everywhwere.
    if (priv->type == GWY_PLOT_POINTS || priv->type == GWY_PLOT_LINE_POINTS)
        draw_points(graphcurve, cr, rect, grapharea);
    if (priv->type == GWY_PLOT_LINE || priv->type == GWY_PLOT_LINE_POINTS)
        draw_lines(graphcurve, cr, rect, grapharea);
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
    GraphCurve *priv = graphcurve->priv;
    priv->xrange.cached = priv->yrange.cached = FALSE;
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
    GwyGraphDataRange *xrange = &priv->xrange, *yrange = &priv->yrange;
    if (xrange->cached && yrange->cached)
        return;

    gdouble xposmin = G_MAXDOUBLE, yposmin = G_MAXDOUBLE;
    gboolean xpospresent = FALSE, ypospresent = FALSE;

    if (priv->curve) {
        const GwyCurve *curve = priv->curve;
        gwy_curve_range_full(curve, &xrange->full.from, &xrange->full.to);
        gwy_curve_min_max_full(curve, &yrange->full.from, &yrange->full.to);

        const GwyXY *data = curve->data;
        for (guint i = curve->n; i; i--, data++) {
            if (data->x > 0.0) {
                xpospresent = TRUE;
                if (data->x < xposmin)
                    xposmin = data->x;
            }
            if (data->y > 0.0) {
                ypospresent = TRUE;
                if (data->y < yposmin)
                    yposmin = data->y;
            }
        }
    }
    else if (priv->line) {
        const GwyLine *line = priv->line;
        gdouble dx = gwy_line_dx(line);
        xrange->full.from = line->off + 0.5*dx;
        xrange->full.to = xrange->full.from + dx*(line->res - 1);
        gwy_line_min_max_full(line, &yrange->full.from, &yrange->full.to);

        const gdouble *ydata = line->data;
        for (guint i = line->res; i; i--, ydata++) {
            if (*ydata > 0.0) {
                ypospresent = TRUE;
                if (*ydata < yposmin)
                    yposmin = *ydata;
            }
        }
        if (xrange->full.to > 0.0) {
            xpospresent = TRUE;
            if (xrange->full.from > 0.0)
                xposmin = xrange->full.from;
            else {
                gint i = ceil(-xrange->full.from/dx);
                xposmin = xrange->full.from + dx*i;
                // Avoid almost-zero points for range calculation.  We might
                // also want to avoid them for drawing in logscale but
                // hopefully Cairo can handle that.
                if (xposmin < 1e-6*dx)
                    xposmin += dx;
            }
        }
    }
    else {
        xrange->full = (GwyRange){ NAN, NAN };
        yrange->full = (GwyRange){ NAN, NAN };
    }

    xrange->cached = yrange->cached = TRUE;
    // Fight rounding errors in single-point cases that may cause
    // posmin > range.to.
    xrange->posmin = fmax(xposmin, xrange->full.to);
    yrange->posmin = fmax(yposmin, yrange->full.to);
    xrange->pospresent = xpospresent;
    yrange->pospresent = ypospresent;
}

static void
setup_line_type(cairo_t *cr,
                gdouble linewidth,
                GwyGraphLineType type)
{
    if (type == GWY_GRAPH_LINE_SOLID)
        cairo_set_dash(cr, NULL, 0, 0.0);
    else if (type == GWY_GRAPH_LINE_DASHED) {
        gdouble dash[1] = { 5.0*linewidth };
        cairo_set_dash(cr, dash, 1, 2.5*linewidth);
    }
    else if (type == GWY_GRAPH_LINE_DOTTED) {
        gdouble dash[1] = { 2.0*linewidth };
        cairo_set_dash(cr, dash, 1, 0.0);
    }
    else {
        g_assert_not_reached();
    }
}

static void
draw_points(const GwyGraphCurve *graphcurve,
            cairo_t *cr,
            const cairo_rectangle_int_t *rect,
            const GwyGraphArea *grapharea)
{
    GraphCurve *priv = graphcurve->priv;
    const CurveSymbolInfo *syminfo = symbol_table + priv->point_type;
    gdouble halfside = priv->point_size * syminfo->size_factor;
    SymbolDrawFunc draw = syminfo->draw;
    GwyGraphScaleType xscale = gwy_graph_area_get_xscale(grapharea),
                      yscale = gwy_graph_area_get_yscale(grapharea);

    GraphCurveIter iter;
    if (!graph_curve_iter_init(graphcurve, &iter, xscale, yscale))
        return;

    gdouble xq, xoff, yq, yoff;
    _gwy_graph_calculate_scaling(grapharea, rect, &xq, &xoff, &yq, &yoff);

    cairo_save(cr);
    cairo_set_line_width(cr, 1.0);
    gwy_cairo_set_source_rgba(cr, &priv->point_color);

    gboolean stroke_each_point = priv->point_color.a < 1.0;

    do {
        gdouble x, y;
        if (!graph_curve_iter_get(&iter, &x, &y))
            continue;
        x = xq*x + xoff;
        y = yq*y + yoff;
        if (within_range(rect->x, rect->width, x, halfside)
            && within_range(rect->y, rect->height, y, halfside)) {
            draw(cr, x, y, halfside);
            if (stroke_each_point)
                stroke_fill_point(syminfo, cr);
        }
    } while (graph_curve_iter_next(&iter));

    if (!stroke_each_point)
        stroke_fill_point(syminfo, cr);

    cairo_restore(cr);
}

static void
stroke_fill_point(const CurveSymbolInfo *syminfo,
                  cairo_t *cr)
{
    if (syminfo->do_fill) {
        if (syminfo->do_stroke)
            cairo_fill_preserve(cr);
        else
            cairo_fill(cr);
    }
    if (syminfo->do_stroke)
        cairo_stroke(cr);
}

static void
draw_lines(const GwyGraphCurve *graphcurve,
           cairo_t *cr,
           const cairo_rectangle_int_t *rect,
           const GwyGraphArea *grapharea)
{
    GraphCurve *priv = graphcurve->priv;

    GraphCurveIter iter;
    if (!graph_curve_iter_init(graphcurve, &iter,
                               gwy_graph_area_get_xscale(grapharea),
                               gwy_graph_area_get_yscale(grapharea)))
        return;

    gdouble xq, xoff, yq, yoff;
    _gwy_graph_calculate_scaling(grapharea, rect, &xq, &xoff, &yq, &yoff);

    cairo_save(cr);
    cairo_set_line_width(cr, priv->line_width);
    gwy_cairo_set_source_rgba(cr, &priv->line_color);
    setup_line_type(cr, priv->line_width, priv->line_type);

    gdouble x, y;
    gint xtype, ytype;
    if (graph_curve_iter_get(&iter, &x, &y)) {
        x = xq*x + xoff;
        y = yq*y + yoff;
        xtype = range_type(rect->x, rect->width, x, 1.0);
        ytype = range_type(rect->y, rect->height, y, 1.0);
    }
    else
        xtype = ytype = 42;

    gboolean skipping = TRUE;
    while (graph_curve_iter_next(&iter)) {
        gdouble xprev = x, yprev = y;
        gint xprevtype = xtype, yprevtype = ytype;
        if (graph_curve_iter_get(&iter, &x, &y)) {
            x = xq*x + xoff;
            y = yq*y + yoff;
            xtype = range_type(rect->x, rect->width, x, 1.0);
            ytype = range_type(rect->y, rect->height, y, 1.0);
        }
        else
            xtype = ytype = 42;

        // This means any of them is 42.
        if (xtype + ytype + xprevtype + yprevtype > 30) {
            skipping = TRUE;
            continue;
        }

        if ((xtype && xtype == xprevtype) || (ytype && ytype == yprevtype)) {
            skipping = TRUE;
            continue;
        }

        if (skipping) {
            cairo_move_to(cr, xprev, yprev);
            skipping = FALSE;
        }
        cairo_line_to(cr, x, y);
    }

    cairo_stroke(cr);
    cairo_restore(cr);
}

static void
draw_star(cairo_t *cr,
          gdouble x,
          gdouble y,
          gdouble halfside)
{
    gwy_cairo_cross(cr, x, y, 1.2*halfside);
    gwy_cairo_times(cr, x, y, 0.84*halfside);
}

static void
draw_circle(cairo_t *cr,
            gdouble x,
            gdouble y,
            gdouble halfside)
{
    cairo_new_sub_path(cr);
    cairo_arc(cr, x, y, halfside, 0.0, 2.0*G_PI);
    cairo_close_path(cr);
}

static gpointer
check_symbol_table_sanity(G_GNUC_UNUSED gpointer arg)
{
    gboolean ok = TRUE;
    guint i;

    for (i = 0; i < G_N_ELEMENTS(symbol_table); i++) {
        if (symbol_table[i].type != i) {
            g_critical("Inconsistent symbol table: %u at pos %u\n",
                       symbol_table[i].type, i);
            ok = FALSE;
        }
    }

    return GINT_TO_POINTER(ok);
}

static gboolean
graph_curve_iter_init(const GwyGraphCurve *graphcurve,
                      GraphCurveIter *iter,
                      GwyGraphScaleType xscale,
                      GwyGraphScaleType yscale)
{
    GraphCurve *priv = graphcurve->priv;
    const GwyCurve *curve = priv->curve;
    const GwyLine *line = priv->line;
    gwy_clear(iter, 1);
    g_return_val_if_fail(!curve ^ !line, FALSE);

    if (curve) {
        iter->xy = curve->data;
        iter->n = curve->n;
    }
    if (line) {
        iter->y = line->data;
        iter->n = line->res;
        iter->dx = gwy_line_dx(line);
        iter->xoff = line->off + 0.5*iter->dx;
    }
    iter->xscale = xscale;
    iter->yscale = yscale;
    return iter->n > 0;
}

static gboolean
graph_curve_iter_get(const GraphCurveIter *iter,
                     gdouble *x, gdouble *y)
{
    if (iter->xy) {
        *x = iter->xy[iter->i].x;
        *y = iter->xy[iter->i].y;
    }
    if (iter->y) {
        *x = iter->xoff + iter->i*iter->dx;
        *y = iter->y[iter->i];
    }

    if (iter->xscale == GWY_GRAPH_SCALE_SQRT)
        *x = gwy_ssqrt(*x);
    else if (iter->xscale == GWY_GRAPH_SCALE_LOG)
        *x = log(*x);

    if (iter->yscale == GWY_GRAPH_SCALE_SQRT)
        *y = gwy_ssqrt(*y);
    else if (iter->yscale == GWY_GRAPH_SCALE_LOG)
        *y = log(*y);

    if (!isfinite(*x) || !isfinite(*y))
        return FALSE;

    return TRUE;
}

static gboolean
graph_curve_iter_next(GraphCurveIter *iter)
{
    iter->i++;
    if (iter->i < iter->n)
        return TRUE;

    if (G_UNLIKELY(iter->i > iter->n))
        iter->i--;
    return FALSE;
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

/**
 * GwyPlotType:
 * @GWY_PLOT_HIDDEN: Data are invisible.
 * @GWY_PLOT_POINTS: Data are plotted with symbols.
 * @GWY_PLOT_LINE: Data are plotted with a line.
 * @GWY_PLOT_LINE_POINTS: Data are plotted with both symbols and a line.
 *
 * Graph plotting type.
 **/

/**
 * GwyGraphPointType:
 * @GWY_GRAPH_POINT_CROSS: Crossed horizontal and vertical line.
 * @GWY_GRAPH_POINT_TIMES: Crossed diagonal lines.
 * @GWY_GRAPH_POINT_STAR: Crossed horizontal, vertical and diagonal lines.
 * @GWY_GRAPH_POINT_SQUARE: Open square.
 * @GWY_GRAPH_POINT_CIRCLE: Open circle.
 * @GWY_GRAPH_POINT_DIAMOND: Open diagonal square.
 * @GWY_GRAPH_POINT_TRIANGLE_UP: Open upward pointing triangle.
 * @GWY_GRAPH_POINT_TRIANGLE_DOWN: Open downward pointing triangle.
 * @GWY_GRAPH_POINT_TRIANGLE_LEFT: Open leftward pointing triangle.
 * @GWY_GRAPH_POINT_TRIANGLE_RIGHT: Open rightward pointing triangle.
 * @GWY_GRAPH_POINT_FILLED_SQUARE: Filled square.
 * @GWY_GRAPH_POINT_DISC: Filled circle.
 * @GWY_GRAPH_POINT_FILLED_CIRCLE: Another name for %GWY_GRAPH_POINT_DISC.
 * @GWY_GRAPH_POINT_FILLED_DIAMOND: Filled diagonal square.
 * @GWY_GRAPH_POINT_FILLED_TRIANGLE_UP: Filled upward pointing triangle.
 * @GWY_GRAPH_POINT_FILLED_TRIANGLE_DOWN: Filled downward pointing triangle.
 * @GWY_GRAPH_POINT_FILLED_TRIANGLE_LEFT: Filled leftward pointing triangle.
 * @GWY_GRAPH_POINT_FILLED_TRIANGLE_RIGHT: Filled rightward pointing triangle.
 * @GWY_GRAPH_POINT_ASTERISK: Six-armed asterisk.
 *
 * Type of graph symbols for point plotting mode.
 **/

/**
 * GwyGraphLineType:
 * @GWY_GRAPH_LINE_SOLID: Solid line.
 * @GWY_GRAPH_LINE_DASHED: Medium long dashes.
 * @GWY_GRAPH_LINE_DOTTED: Very short dashes (dots).
 *
 * Type of graph lines.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
