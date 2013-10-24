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
#include "libgwy/math.h"
#include "libgwy/strfuncs.h"
#include "libgwy/object-utils.h"
#include "libgwy/fft.h"
#include "libgwyui/cairo-utils.h"
#include "libgwyui/graph-axis.h"
#include "libgwyui/axis-internal.h"

#define TESTMARKUP "<small>(q₁¹)</small>"

#define pangoscale ((gdouble)PANGO_SCALE)

enum {
    PROP_0,
    PROP_LABEL,
    PROP_SHOW_LABEL,
    PROP_SHOW_TICKS,
    PROP_LOG_SCALE,
    N_PROPS,
};

struct _GwyGraphAxisPrivate {
    gchar *label;
    guint ticksbreadth;
    gboolean show_label : 1;
    gboolean show_ticks : 1;

    // Scratch space
    GString *str;
};

typedef struct _GwyGraphAxisPrivate GraphAxis;

static void     gwy_graph_axis_dispose              (GObject *object);
static void     gwy_graph_axis_finalize             (GObject *object);
static void     gwy_graph_axis_set_property         (GObject *object,
                                                     guint prop_id,
                                                     const GValue *value,
                                                     GParamSpec *pspec);
static void     gwy_graph_axis_get_property         (GObject *object,
                                                     guint prop_id,
                                                     GValue *value,
                                                     GParamSpec *pspec);
static void     gwy_graph_axis_get_preferred_width  (GtkWidget *widget,
                                                     gint *minimum,
                                                     gint *natural);
static void     gwy_graph_axis_get_preferred_height (GtkWidget *widget,
                                                     gint *minimum,
                                                     gint *natural);
static gboolean gwy_graph_axis_draw                 (GtkWidget *widget,
                                                     cairo_t *cr);
static gboolean gwy_graph_axis_get_horizontal_labels(const GwyAxis *axis);
static void     gwy_graph_axis_get_units_affinity   (const GwyAxis *axis,
                                                     GwyAxisUnitPlacement *primary,
                                                     GwyAxisUnitPlacement *secondary);
static void     gwy_graph_axis_redraw_mark          (GwyAxis *axis);
static void     draw_ticks                          (GwyAxis *axis,
                                                     cairo_t *cr,
                                                     const cairo_matrix_t *matrix,
                                                     gdouble length,
                                                     gdouble breadth);
static void     draw_labels                         (GwyAxis *axis,
                                                     cairo_t *cr,
                                                     const cairo_matrix_t *matrix,
                                                     gdouble length,
                                                     gdouble breadth);
static void     draw_axis_label                     (GwyAxis *axis,
                                                     cairo_t *cr,
                                                     const cairo_matrix_t *matrix,
                                                     gdouble length,
                                                     gdouble breadth);
static void     draw_mark                           (GwyAxis *axis,
                                                     cairo_t *cr,
                                                     const cairo_matrix_t *matrix,
                                                     gdouble length,
                                                     gdouble breadth);
static void     draw_line_transformed               (cairo_t *cr,
                                                     const cairo_matrix_t *matrix,
                                                     gdouble xf,
                                                     gdouble yf,
                                                     gdouble xt,
                                                     gdouble yt);
static void     calculate_scaling                   (GwyAxis *axis,
                                                     cairo_rectangle_int_t *allocation,
                                                     gdouble *length,
                                                     gdouble *breadth,
                                                     cairo_matrix_t *matrix);
static gboolean set_label                           (GwyGraphAxis *graphaxis,
                                                     const gchar *label);
static gboolean set_show_label                      (GwyGraphAxis *graphaxis,
                                                     gboolean setting);
static gboolean set_show_ticks                      (GwyGraphAxis *graphaxis,
                                                     gboolean setting);
static void     rotate_pango_context                (GwyGraphAxis *graphaxis,
                                                     GwyTransformDirection direction);

static GParamSpec *properties[N_PROPS];

static const gdouble tick_level_sizes[4] = { 1.0, 0.9, 0.45, 0.25 };

G_DEFINE_TYPE(GwyGraphAxis, gwy_graph_axis, GWY_TYPE_AXIS);

static void
gwy_graph_axis_class_init(GwyGraphAxisClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
    GwyAxisClass *axis_class = GWY_AXIS_CLASS(klass);

    g_type_class_add_private(klass, sizeof(GraphAxis));

    gobject_class->dispose = gwy_graph_axis_dispose;
    gobject_class->finalize = gwy_graph_axis_finalize;
    gobject_class->get_property = gwy_graph_axis_get_property;
    gobject_class->set_property = gwy_graph_axis_set_property;

    widget_class->get_preferred_width = gwy_graph_axis_get_preferred_width;
    widget_class->get_preferred_height = gwy_graph_axis_get_preferred_height;
    widget_class->draw = gwy_graph_axis_draw;

    axis_class->get_units_affinity = gwy_graph_axis_get_units_affinity;
    axis_class->get_horizontal_labels = gwy_graph_axis_get_horizontal_labels;
    axis_class->redraw_mark = gwy_graph_axis_redraw_mark;

    properties[PROP_LABEL]
        = g_param_spec_string("label",
                              "Label",
                              "Label to show on the graph axis.",
                              NULL,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_SHOW_LABEL]
        = g_param_spec_boolean("show-label",
                               "Show label",
                               "Whether to show the axis label.",
                               TRUE,
                               G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_SHOW_TICKS]
        = g_param_spec_boolean("show-ticks",
                               "Show ticks",
                               "Whether to show ticks on the axis.",
                               TRUE,
                               G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_LOG_SCALE]
        = g_param_spec_boolean("log-scale",
                               "Log scale",
                               "Whether the axis scale is logarithmic.",
                               FALSE,
                               G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    for (guint i = 1; i < N_PROPS; i++)
        g_object_class_install_property(gobject_class, i, properties[i]);
}

static void
gwy_graph_axis_init(GwyGraphAxis *graphaxis)
{
    graphaxis->priv = G_TYPE_INSTANCE_GET_PRIVATE(graphaxis,
                                                  GWY_TYPE_GRAPH_AXIS,
                                                  GraphAxis);
    GraphAxis *priv = graphaxis->priv;
    priv->str = g_string_new(NULL);
    priv->show_label = TRUE;
    priv->show_ticks = TRUE;
}

static void
gwy_graph_axis_finalize(GObject *object)
{
    GraphAxis *priv = GWY_GRAPH_AXIS(object)->priv;
    GWY_FREE(priv->label);
    GWY_STRING_FREE(priv->str);
    G_OBJECT_CLASS(gwy_graph_axis_parent_class)->finalize(object);
}

static void
gwy_graph_axis_dispose(GObject *object)
{
    G_OBJECT_CLASS(gwy_graph_axis_parent_class)->dispose(object);
}

static void
gwy_graph_axis_set_property(GObject *object,
                            guint prop_id,
                            const GValue *value,
                            GParamSpec *pspec)
{
    GwyGraphAxis *graphaxis = GWY_GRAPH_AXIS(object);

    switch (prop_id) {
        case PROP_LABEL:
        set_label(graphaxis, g_value_get_string(value));
        break;

        case PROP_SHOW_LABEL:
        set_show_label(graphaxis, g_value_get_boolean(value));
        break;

        case PROP_SHOW_TICKS:
        set_show_ticks(graphaxis, g_value_get_boolean(value));
        break;

        case PROP_LOG_SCALE:
        _gwy_axis_set_logscale(GWY_AXIS(object), g_value_get_boolean(value));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_graph_axis_get_property(GObject *object,
                            guint prop_id,
                            GValue *value,
                            GParamSpec *pspec)
{
    GraphAxis *priv = GWY_GRAPH_AXIS(object)->priv;

    switch (prop_id) {
        case PROP_LABEL:
        g_value_set_string(value, priv->label);
        break;

        case PROP_SHOW_LABEL:
        g_value_set_boolean(value, priv->show_label);
        break;

        case PROP_SHOW_TICKS:
        g_value_set_boolean(value, priv->show_ticks);
        break;

        case PROP_LOG_SCALE:
        g_value_set_boolean(value, _gwy_axis_get_logscale(GWY_AXIS(object)));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_graph_axis_get_preferred_width(GtkWidget *widget,
                                   gint *minimum,
                                   gint *natural)
{
    GraphAxis *priv = GWY_GRAPH_AXIS(widget)->priv;
    GwyAxis *axis = GWY_AXIS(widget);
    GtkPositionType edge = gwy_axis_get_edge(axis);
    gboolean show_ticks = priv->show_ticks;
    gboolean show_label = priv->show_label;
    gboolean show_labels = gwy_axis_get_show_tick_labels(axis) && show_ticks;
    gboolean is_empty = !show_ticks && !show_label;

    if (edge == GTK_POS_TOP || edge == GTK_POS_BOTTOM || is_empty) {
        *minimum = *natural = 1;
        return;
    }

    *minimum = *natural = 20;
    PangoLayout *layout = gwy_axis_get_pango_layout(GWY_AXIS(widget));
    g_return_if_fail(layout);

    gint h = 0, w = 0;
    if (show_label || show_ticks) {
        pango_layout_set_markup(layout, TESTMARKUP, sizeof(TESTMARKUP)-1);
        pango_layout_get_size(layout, NULL, &h);
    }

    if (show_labels) {
        GwyRange range;
        GwyValueFormat *vf = gwy_axis_estimate_value_format(axis, &range);
        GString *str = priv->str;
        gint wf, wt;

        g_string_assign(str, "<small>");
        g_string_append(str, gwy_value_format_print_number(vf, range.from));
        g_string_append(str, "</small>");
        pango_layout_set_markup(layout, str->str, str->len);
        pango_layout_get_size(layout, &wf, NULL);

        g_string_assign(str, "<small>");
        g_string_append(str, gwy_value_format_print_number(vf, range.to));
        g_string_append(str, "</small>");
        pango_layout_set_markup(layout, str->str, str->len);
        pango_layout_get_size(layout, &wt, NULL);

        g_object_unref(vf);
        w = MAX(wf, wt);
    }

    gint breadth = 0;
    if (show_ticks)
        breadth += 5*h;
    if (show_labels)
        breadth += 5*w;
    if (show_label)
        breadth += 6*h;

    breadth = breadth/(5*PANGO_SCALE);
    priv->ticksbreadth = h/PANGO_SCALE;
    *minimum = *natural = MAX(breadth, 6);
}

static void
gwy_graph_axis_get_preferred_height(GtkWidget *widget,
                                    gint *minimum,
                                    gint *natural)
{
    GraphAxis *priv = GWY_GRAPH_AXIS(widget)->priv;
    GwyAxis *axis = GWY_AXIS(widget);
    GtkPositionType edge = gwy_axis_get_edge(axis);
    gboolean show_ticks = priv->show_ticks;
    gboolean show_label = priv->show_label;
    gboolean show_labels = gwy_axis_get_show_tick_labels(axis) && show_ticks;
    gboolean is_empty = !show_ticks && !show_label;

    if (edge == GTK_POS_LEFT || edge == GTK_POS_RIGHT || is_empty) {
        *minimum = *natural = 1;
        return;
    }

    *minimum = *natural = 20;
    PangoLayout *layout = gwy_axis_get_pango_layout(GWY_AXIS(widget));
    g_return_if_fail(layout);

    gint b;
    pango_layout_set_markup(layout, TESTMARKUP, sizeof(TESTMARKUP)-1);
    pango_layout_get_size(layout, NULL, &b);
    gint breadth = 0;
    if (show_ticks)
        breadth += 5*b;
    if (show_labels)
        breadth += 5*b;
    if (show_label)
        breadth += 6*b;
    breadth = breadth/(5*PANGO_SCALE);
    priv->ticksbreadth = b/PANGO_SCALE;
    *minimum = *natural = MAX(breadth, 4);
}

static gboolean
gwy_graph_axis_draw(GtkWidget *widget,
                    cairo_t *cr)
{
    GwyAxis *axis = GWY_AXIS(widget);
    GtkStyleContext *context = gtk_widget_get_style_context(widget);
    cairo_rectangle_int_t allocation;
    gdouble length, breadth;
    cairo_matrix_t matrix;
    calculate_scaling(axis, &allocation, &length, &breadth, &matrix);

    GdkRGBA rgba;
    gtk_style_context_get_color(context, GTK_STATE_NORMAL, &rgba);
    gdk_cairo_set_source_rgba(cr, &rgba);
    cairo_set_line_width(cr, 1.0);

    draw_mark(axis, cr, &matrix, length, breadth);
    draw_ticks(axis, cr, &matrix, length, breadth);
    draw_labels(axis, cr, &matrix, length, breadth);
    draw_axis_label(axis, cr, &matrix, length, breadth);

    return FALSE;
}

static gboolean
gwy_graph_axis_get_horizontal_labels(G_GNUC_UNUSED const GwyAxis *axis)
{
    return TRUE;
}

static void
gwy_graph_axis_get_units_affinity(G_GNUC_UNUSED const GwyAxis *axis,
                                  GwyAxisUnitPlacement *primary,
                                  GwyAxisUnitPlacement *secondary)
{
    *primary = *secondary = GWY_AXIS_UNITS_NEVER;
}

static void
gwy_graph_axis_redraw_mark(GwyAxis *axis)
{
    GtkWidget *widget = GTK_WIDGET(axis);
    gdouble mark = gwy_axis_get_mark(axis);
    if (!gwy_axis_get_show_mark(axis) || !isfinite(mark))
        return;

    GraphAxis *priv = GWY_GRAPH_AXIS(axis)->priv;
    cairo_rectangle_int_t allocation;
    gdouble length, breadth;
    cairo_matrix_t matrix;
    calculate_scaling(axis, &allocation, &length, &breadth, &matrix);
    gdouble tickbreadth = MIN(priv->ticksbreadth, breadth);

    gdouble x = gwy_axis_value_to_position(axis, mark),
            hs = 0.5*tickbreadth, y = hs;
    if (x < -hs || x > length + hs)
        return;

    cairo_matrix_transform_point(&matrix, &x, &y);

    cairo_rectangle_int_t rect = {
        (gint)floor(x - hs - 1.0 + allocation.x),
        (gint)floor(y - hs - 1.0 + allocation.y),
        (gint)ceil(2.0*hs + 2.01),
        (gint)ceil(2.0*hs + 2.01),
    };

    cairo_region_t *region = cairo_region_create_rectangle(&rect);
    cairo_region_intersect_rectangle(region, &allocation);
    gtk_widget_queue_draw_region(widget, region);
    cairo_region_destroy(region);
}

/**
 * gwy_graph_axis_new:
 *
 * Creates a new graph axis.
 *
 * Returns: A new graph axis.
 **/
GtkWidget*
gwy_graph_axis_new(void)
{
    return g_object_newv(GWY_TYPE_GRAPH_AXIS, 0, NULL);
}

/**
 * gwy_graph_axis_set_label:
 * @graphaxis: A graph axis.
 * @label: (allow-none):
 *         Label to show on the axis.
 *
 * Sets the label shown on a graph axis.
 **/
void
gwy_graph_axis_set_label(GwyGraphAxis *graphaxis,
                         const gchar *label)
{
    g_return_if_fail(GWY_IS_GRAPH_AXIS(graphaxis));
    if (!set_label(graphaxis, label))
        return;

    g_object_notify_by_pspec(G_OBJECT(graphaxis), properties[PROP_LABEL]);
}

/**
 * gwy_graph_axis_get_label:
 * @graphaxis: A graph axis.
 *
 * Gets the label shown on a graph axis.
 *
 * Returns: (allow-none):
 *          The label shown on the axis, possibly %NULL.
 **/
const gchar*
gwy_graph_axis_get_label(const GwyGraphAxis *graphaxis)
{
    g_return_val_if_fail(GWY_IS_GRAPH_AXIS(graphaxis), NULL);
    return graphaxis->priv->label;
}

/**
 * gwy_graph_axis_set_show_label:
 * @graphaxis: A graph axis.
 * @showlabel: %TRUE to show the axis label, %FALSE to disable it.
 *
 * Sets whether a graph axis should show the axis label.
 *
 * The display of units can be controlled separately using
 * gwy_axis_set_show_units().
 **/
void
gwy_graph_axis_set_show_label(GwyGraphAxis *graphaxis,
                              gboolean showlabel)
{
    g_return_if_fail(GWY_IS_GRAPH_AXIS(graphaxis));
    if (!set_show_label(graphaxis, showlabel))
        return;

    g_object_notify_by_pspec(G_OBJECT(graphaxis), properties[PROP_SHOW_LABEL]);
}

/**
 * gwy_graph_axis_get_show_label:
 * @graphaxis: A graph axis.
 *
 * Gets whether a graph axis should show the axis label.
 *
 * Returns: %TRUE if the axis label is shown, %FALSE if it is disabled.
 **/
gboolean
gwy_graph_axis_get_show_label(const GwyGraphAxis *graphaxis)
{
    g_return_val_if_fail(GWY_IS_GRAPH_AXIS(graphaxis), FALSE);
    return graphaxis->priv->show_label;
}

/**
 * gwy_graph_axis_set_show_ticks:
 * @graphaxis: A graph axis.
 * @showticks: %TRUE to show the ticks, %FALSE to disable them.
 *
 * Sets whether a graph axis should show ticks.
 *
 * Disabling the ticks disables them completely.  This means tick labels are
 * not show either, regardless of what is set with
 * gwy_axis_set_show_tick_labels().
 *
 * If both ticks and the axis label are disabled the axis is drawn just as a
 * single line.
 **/
void
gwy_graph_axis_set_show_ticks(GwyGraphAxis *graphaxis,
                              gboolean showticks)
{
    g_return_if_fail(GWY_IS_GRAPH_AXIS(graphaxis));
    if (!set_show_ticks(graphaxis, showticks))
        return;

    g_object_notify_by_pspec(G_OBJECT(graphaxis), properties[PROP_SHOW_TICKS]);
}

/**
 * gwy_graph_axis_get_show_ticks:
 * @graphaxis: A graph axis.
 *
 * Gets whether a graph axis should show ticks.
 *
 * Returns: %TRUE if the ticks are shown, %FALSE if they are disabled.
 **/
gboolean
gwy_graph_axis_get_show_ticks(const GwyGraphAxis *graphaxis)
{
    g_return_val_if_fail(GWY_IS_GRAPH_AXIS(graphaxis), FALSE);
    return graphaxis->priv->show_ticks;
}

/**
 * gwy_graph_axis_set_log_scale:
 * @graphaxis: A graph axis.
 * @logscale: %TRUE to make the axis scale logarithmic, %FALSE to make it
 *            linear.
 *
 * Sets whether a graph axis scale is linear or logarithmic.
 **/
void
gwy_graph_axis_set_log_scale(GwyGraphAxis *graphaxis,
                             gboolean logscale)
{
    g_return_if_fail(GWY_IS_GRAPH_AXIS(graphaxis));
    if (!_gwy_axis_set_logscale(GWY_AXIS(graphaxis), logscale))
        return;

    g_object_notify_by_pspec(G_OBJECT(graphaxis), properties[PROP_LOG_SCALE]);
}

/**
 * gwy_graph_axis_get_log_scale:
 * @graphaxis: A graph axis.
 *
 * Gets whether a graph axis scale is linear or logarithmic.
 *
 * Returns: %TRUE if the axis scale is logarithmic, %FALSE if the scale is
 *          linear.
 **/
gboolean
gwy_graph_axis_get_log_scale(const GwyGraphAxis *graphaxis)
{
    g_return_val_if_fail(GWY_IS_GRAPH_AXIS(graphaxis), FALSE);
    return _gwy_axis_get_logscale(GWY_AXIS(graphaxis));
}

static void
draw_ticks(GwyAxis *axis, cairo_t *cr,
           const cairo_matrix_t *matrix,
           G_GNUC_UNUSED gdouble length, gdouble breadth)
{
    GraphAxis *priv = GWY_GRAPH_AXIS(axis)->priv;
    if (!priv->show_ticks)
        return;

    guint nticks;
    const GwyAxisTick *ticks = gwy_axis_ticks(axis, &nticks);
    gdouble ticksbreadth = MIN(priv->ticksbreadth, breadth);

    for (guint i = 0; i < nticks; i++) {
        gdouble pos = ticks[i].position;
        gdouble s = tick_level_sizes[ticks[i].level];
        draw_line_transformed(cr, matrix, pos, 0, pos, s*ticksbreadth);
    }
    cairo_stroke(cr);
}

static void
draw_labels(GwyAxis *axis, cairo_t *cr,
            const cairo_matrix_t *matrix,
            G_GNUC_UNUSED gdouble length, gdouble breadth)
{
    GraphAxis *priv = GWY_GRAPH_AXIS(axis)->priv;
    if (!priv->show_ticks || !gwy_axis_get_show_tick_labels(axis))
        return;

    guint nticks;
    const GwyAxisTick *ticks = gwy_axis_ticks(axis, &nticks);
    gint max_ascent = G_MININT, max_descent = G_MININT;

    for (guint i = 0; i < nticks; i++) {
        if (ticks[i].label) {
            max_ascent = MAX(max_ascent, PANGO_ASCENT(ticks[i].extents));
            max_descent = MAX(max_descent, PANGO_DESCENT(ticks[i].extents));
        }
    }

    gdouble ticksbreadth = MIN(priv->ticksbreadth, breadth);
    GtkStyleContext *context = gtk_widget_get_style_context(GTK_WIDGET(axis));
    PangoLayout *layout = gwy_axis_get_pango_layout(axis);
    GtkPositionType edge = gwy_axis_get_edge(axis);
    gdouble a = max_ascent/pangoscale, d = max_descent/pangoscale;
    gdouble mtlen = ticksbreadth*tick_level_sizes[GWY_AXIS_TICK_MAJOR];
    for (guint i = 0; i < nticks; i++) {
        if (!ticks[i].label)
            continue;

        gdouble x = ticks[i].position, y = ticksbreadth;
        cairo_matrix_transform_point(matrix, &x, &y);
        pango_layout_set_markup(layout, ticks[i].label, -1);
        if (edge == GTK_POS_TOP || edge == GTK_POS_BOTTOM) {
            if (i == nticks-1 && ticks[i].level == GWY_AXIS_TICK_EDGE)
                x -= 2.0 + PANGO_RBEARING(ticks[i].extents)/pangoscale;
            else if (i == 0)
                x += 2.0;
            else
                x -= 0.5*PANGO_RBEARING(ticks[i].extents)/pangoscale;

            if (edge == GTK_POS_TOP)
                y = breadth - (mtlen + 1) - d;
            else {
                // XXX: We use descent difference to shift the baseline
                // because, for some reason, ascents are zero.
                y = (mtlen + 1) + a
                    + (d - PANGO_DESCENT(ticks[i].extents)/pangoscale);
            }
        }
        else if (edge == GTK_POS_LEFT || edge == GTK_POS_RIGHT) {
            if (i == 0 && ticks[i].level == GWY_AXIS_TICK_EDGE)
                y -= 2.0 + ticks[i].extents.height/pangoscale;
            else if (i == nticks-1)
                y += 2.0;
            else
                y -= 0.5*ticks[i].extents.height/pangoscale;

            // FIXME: Right edge position does not look good with negative
            // numbers.  We would need something more sophisticated...
            if (edge == GTK_POS_LEFT)
                x = breadth - (mtlen + 1) - ticks[i].extents.width/pangoscale;
            else
                x = mtlen + 1;
        }
        gtk_render_layout(context, cr, x, y, layout);
    }
}

static void
draw_axis_label(GwyAxis *axis, cairo_t *cr,
                G_GNUC_UNUSED const cairo_matrix_t *matrix,
                gdouble length, gdouble breadth)
{
    GraphAxis *priv = GWY_GRAPH_AXIS(axis)->priv;
    if (!priv->show_label)
        return;

    GString *str = priv->str;
    g_string_assign(str, "<small>");
    if (priv->label && *priv->label)
        g_string_append(str, priv->label);

    GtkStyleContext *context = gtk_widget_get_style_context(GTK_WIDGET(axis));
    GwyValueFormat *vf = gwy_axis_get_value_format(axis);
    const gchar *unitstr = vf ? gwy_value_format_get_units(vf) : NULL;
    if (gwy_axis_get_show_unit(axis) && unitstr && strlen(unitstr)) {
        if (str->len)
            g_string_append(str, " ");
        g_string_append(str, "[");
        g_string_append(str, unitstr);
        g_string_append(str, "]");
    }
    GWY_OBJECT_UNREF(vf);
    g_string_append(str, "</small>");

    PangoLayout *layout = gwy_axis_get_pango_layout(axis);
    rotate_pango_context(GWY_GRAPH_AXIS(axis), GWY_TRANSFORM_FORWARD);
    pango_layout_set_markup(layout, str->str, str->len);
    PangoRectangle extents;
    pango_layout_get_extents(layout, NULL, &extents);

    GtkPositionType edge = gwy_axis_get_edge(axis);
    gdouble x = 0.0, y = 0.0;
    if (edge == GTK_POS_TOP) {
        x = 0.5*(length - extents.width/pangoscale);
        y = PANGO_ASCENT(extents)/pangoscale + 1.0;
    }
    else if (edge == GTK_POS_BOTTOM) {
        x = 0.5*(length - extents.width/pangoscale);
        y = breadth - PANGO_DESCENT(extents)/pangoscale - 1.0;
    }
    else if (edge == GTK_POS_LEFT) {
        x = PANGO_ASCENT(extents)/pangoscale + 1.0;
        y = 0.5*(length + extents.width/pangoscale);
    }
    else if (edge == GTK_POS_RIGHT) {
        x = breadth - PANGO_DESCENT(extents)/pangoscale - 1.0;
        y = 0.5*(length + extents.width/pangoscale);
    }
    gtk_render_layout(context, cr, x, y, layout);

    rotate_pango_context(GWY_GRAPH_AXIS(axis), GWY_TRANSFORM_BACKWARD);
}

static void
draw_mark(GwyAxis *axis, cairo_t *cr,
          const cairo_matrix_t *matrix,
          gdouble length, gdouble breadth)
{
    GraphAxis *priv = GWY_GRAPH_AXIS(axis)->priv;
    if (!priv->show_ticks)
        return;

    gdouble mark = gwy_axis_get_mark(axis);
    if (!gwy_axis_get_show_mark(axis) || !isfinite(mark))
        return;

    GtkPositionType edge = gwy_axis_get_edge(axis);
    guint nticks;
    gwy_axis_ticks(axis, &nticks);
    gdouble x = gwy_axis_value_to_position(axis, mark),
            hs = 0.5*breadth*priv->ticksbreadth, y = hs;
    if (x < -hs || x > length + hs)
        return;

    cairo_matrix_transform_point(matrix, &x, &y);

    cairo_save(cr);
    if (edge == GTK_POS_TOP)
        gwy_cairo_triangle_down(cr, x, y, hs);
    else if (edge == GTK_POS_BOTTOM)
        gwy_cairo_triangle_up(cr, x, y, hs);
    else if (edge == GTK_POS_LEFT)
        gwy_cairo_triangle_right(cr, x, y, hs);
    else if (edge == GTK_POS_RIGHT)
        gwy_cairo_triangle_left(cr, x, y, hs);
    else {
        g_assert_not_reached();
    }

    GtkStyleContext *context = gtk_widget_get_style_context(GTK_WIDGET(axis));
    GdkRGBA rgba;
    // FIXME: Theming.
    cairo_set_source_rgb(cr, 0.6, 0.6, 1.0);
    cairo_fill_preserve(cr);
    gtk_style_context_get_color(context, GTK_STATE_NORMAL, &rgba);
    gdk_cairo_set_source_rgba(cr, &rgba);
    cairo_stroke(cr);
    cairo_restore(cr);
}

static void
draw_line_transformed(cairo_t *cr, const cairo_matrix_t *matrix,
                      gdouble xf, gdouble yf, gdouble xt, gdouble yt)
{
    cairo_matrix_transform_point(matrix, &xf, &yf);
    cairo_move_to(cr, xf, yf);
    cairo_matrix_transform_point(matrix, &xt, &yt);
    cairo_line_to(cr, xt, yt);
}

static void
calculate_scaling(GwyAxis *axis,
                  cairo_rectangle_int_t *allocation,
                  gdouble *length, gdouble *breadth,
                  cairo_matrix_t *matrix)
{
    GtkWidget *widget = GTK_WIDGET(axis);
    GtkPositionType edge = gwy_axis_get_edge(axis);
    gboolean vertical = (edge == GTK_POS_LEFT || edge == GTK_POS_RIGHT);
    gtk_widget_get_allocation(widget, allocation);
    *length = (vertical ? allocation->height : allocation->width),
    *breadth = (vertical ? allocation->width : allocation->height);

    if (edge == GTK_POS_TOP)
        cairo_matrix_init(matrix, 1.0, 0.0, 0.0, -1.0, 0.0, *breadth);
    else if (edge == GTK_POS_LEFT)
        cairo_matrix_init(matrix, 0.0, -1.0, -1.0, 0.0, *breadth, *length);
    else if (edge == GTK_POS_BOTTOM)
        cairo_matrix_init_identity(matrix);
    else if (edge == GTK_POS_RIGHT)
        cairo_matrix_init(matrix, 0.0, -1.0, 1.0, 0.0, 0.0, *length);
    else {
        g_assert_not_reached();
    }
}

static gboolean
set_label(GwyGraphAxis *graphaxis,
          const gchar *label)
{
    GraphAxis *priv = graphaxis->priv;
    if (!gwy_assign_string(&priv->label, label))
        return FALSE;

    gtk_widget_queue_draw(GTK_WIDGET(graphaxis));
    return TRUE;
}

static gboolean
set_show_label(GwyGraphAxis *graphaxis,
               gboolean setting)
{
    GraphAxis *priv = graphaxis->priv;
    if (!setting == !priv->show_label)
        return FALSE;

    priv->show_label = setting;
    gtk_widget_queue_resize(GTK_WIDGET(graphaxis));
    return TRUE;
}

static gboolean
set_show_ticks(GwyGraphAxis *graphaxis,
               gboolean setting)
{
    GraphAxis *priv = graphaxis->priv;
    if (!setting == !priv->show_ticks)
        return FALSE;

    priv->show_ticks = setting;
    gtk_widget_queue_resize(GTK_WIDGET(graphaxis));
    return TRUE;
}

static void
rotate_pango_context(GwyGraphAxis *graphaxis,
                     GwyTransformDirection direction)
{
    GwyAxis *axis = GWY_AXIS(graphaxis);
    GtkPositionType edge = gwy_axis_get_edge(axis);
    PangoLayout *layout = gwy_axis_get_pango_layout(axis);
    g_return_if_fail(layout);

    if (edge == GTK_POS_TOP || edge == GTK_POS_BOTTOM)
        return;

    PangoMatrix matrix = PANGO_MATRIX_INIT;
    const PangoMatrix *cmatrix;
    PangoContext *context = pango_layout_get_context(layout);
    if ((cmatrix = pango_context_get_matrix(context)))
        matrix = *cmatrix;
    if (direction == GWY_TRANSFORM_FORWARD)
        pango_matrix_rotate(&matrix, 90.0);
    else
        pango_matrix_rotate(&matrix, -90.0);
    pango_context_set_matrix(context, &matrix);
    pango_layout_context_changed(layout);
}

/**
 * SECTION: graph-axis
 * @title: GwyGraphAxis
 * @short_description: Graph axis
 **/

/**
 * GwyGraphAxis:
 *
 * GraphAxis widget is an axis suitable for graphs.
 *
 * The #GwyGraphAxis struct contains private data only and should be accessed
 * using the functions below.
 **/

/**
 * GwyGraphAxisClass:
 *
 * Class of axis widgets suitable for graphs.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
