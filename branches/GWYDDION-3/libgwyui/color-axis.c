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
#include "libgwy/math.h"
#include "libgwy/strfuncs.h"
#include "libgwy/curve-statistics.h"
#include "libgwy/object-utils.h"
#include "libgwyui/cairo-utils.h"
#include "libgwyui/widget-utils.h"
#include "libgwyui/color-axis.h"

#define TESTMARKUP "<small>199.9 (μm₁¹)</small>"

#define pangoscale ((gdouble)PANGO_SCALE)

enum {
    PROP_0,
    PROP_GRADIENT,
    PROP_EDITABLE_RANGE,
    PROP_DISTRIBUTION,
    N_PROPS,
};

enum {
    SGNL_MODIFY_RANGE,
    N_SIGNALS
};

struct _GwyColorAxisPrivate {
    GwyGradient *gradient;
    gulong gradient_data_changed_id;
    gboolean editable_range;
    gint last_boundary;

    GwyCurve *distribution;
    gulong distribution_data_changed_id;
    gdouble distribution_max;
    gboolean distribution_max_valid;

    GdkCursor *cursor_from;
    GdkCursor *cursor_to;

    gdouble stripewidth;
};

typedef struct _GwyColorAxisPrivate ColorAxis;

static void     gwy_color_axis_dispose              (GObject *object);
static void     gwy_color_axis_finalize             (GObject *object);
static void     gwy_color_axis_set_property         (GObject *object,
                                                     guint prop_id,
                                                     const GValue *value,
                                                     GParamSpec *pspec);
static void     gwy_color_axis_get_property         (GObject *object,
                                                     guint prop_id,
                                                     GValue *value,
                                                     GParamSpec *pspec);
static void     gwy_color_axis_notify               (GObject *object,
                                                     GParamSpec *pspec);
static void     gwy_color_axis_get_preferred_width  (GtkWidget *widget,
                                                     gint *minimum,
                                                     gint *natural);
static void     gwy_color_axis_get_preferred_height (GtkWidget *widget,
                                                     gint *minimum,
                                                     gint *natural);
static void     gwy_color_axis_realize              (GtkWidget *widget);
static void     gwy_color_axis_unrealize            (GtkWidget *widget);
static gboolean gwy_color_axis_draw                 (GtkWidget *widget,
                                                     cairo_t *cr);
static gboolean gwy_color_axis_scroll               (GtkWidget *widget,
                                                     GdkEventScroll *event);
static gboolean gwy_color_axis_motion_notify        (GtkWidget *widget,
                                                     GdkEventMotion *event);
static gboolean gwy_color_axis_get_horizontal_labels(const GwyAxis *axis);
static guint    gwy_color_axis_get_split_width      (const GwyAxis *axis);
static void     gwy_color_axis_get_units_affinity   (const GwyAxis *axis,
                                                     GwyAxisUnitPlacement *primary,
                                                     GwyAxisUnitPlacement *secondary);
static void     gwy_color_axis_modify_range         (GwyColorAxis *coloraxis,
                                                     const GwyRange *range);
static void     draw_ticks                          (const GwyAxisTick *ticks,
                                                     guint nticks,
                                                     cairo_t *cr,
                                                     GtkStyleContext *context,
                                                     const cairo_matrix_t *matrix,
                                                     gdouble stripebreadth,
                                                     gint *max_ascent,
                                                     gint *max_descent);
static void     draw_stripe                         (GwyGradient *gradient,
                                                     cairo_t *cr,
                                                     GtkPositionType edge,
                                                     const cairo_matrix_t *matrix,
                                                     gdouble length,
                                                     gdouble stripebreadth);
static void     draw_labels                         (const GwyAxisTick *ticks,
                                                     guint nticks,
                                                     cairo_t *cr,
                                                     GtkPositionType edge,
                                                     GtkStyleContext *context,
                                                     PangoLayout *layout,
                                                     const cairo_matrix_t *matrix,
                                                     gdouble breadth,
                                                     gdouble stripebreadth,
                                                     gint max_ascent,
                                                     gint max_descent);
static void     draw_distribution                   (GwyColorAxis *coloraxis,
                                                     cairo_t *cr,
                                                     const cairo_matrix_t *matrix,
                                                     gdouble breadth,
                                                     gdouble stripebreadth,
                                                     gdouble length);
static void     set_up_transform                    (GtkPositionType edge,
                                                     cairo_matrix_t *matrix,
                                                     gdouble width,
                                                     gdouble height);
static void     draw_line_transformed               (cairo_t *cr,
                                                     const cairo_matrix_t *matrix,
                                                     gdouble xf,
                                                     gdouble yf,
                                                     gdouble xt,
                                                     gdouble yt);
static gboolean set_gradient                        (GwyColorAxis *coloraxis,
                                                     GwyGradient *gradient);
static gboolean set_editable_range                  (GwyColorAxis *coloraxis,
                                                     gboolean setting);
static gboolean set_distribution                    (GwyColorAxis *coloraxis,
                                                     GwyCurve *distribution);
static void     gradient_data_changed               (GwyColorAxis *coloraxis,
                                                     GwyGradient *gradient);
static void     distribution_data_changed           (GwyColorAxis *coloraxis,
                                                     GwyCurve *distribution);
static void     ensure_cursors                      (GwyColorAxis *coloraxis);
static void     discard_cursors                     (GwyColorAxis *coloraxis);
static gint     find_boundary                       (GwyAxis *axis,
                                                     gdouble x,
                                                     gdouble y);

static GParamSpec *properties[N_PROPS];
static guint signals[N_SIGNALS];

static const gdouble tick_level_sizes[4] = { 1.0, 0.65, 0.4, 0.25 };

G_DEFINE_TYPE(GwyColorAxis, gwy_color_axis, GWY_TYPE_AXIS);

static void
gwy_color_axis_class_init(GwyColorAxisClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
    GwyAxisClass *axis_class = GWY_AXIS_CLASS(klass);

    g_type_class_add_private(klass, sizeof(ColorAxis));

    gobject_class->dispose = gwy_color_axis_dispose;
    gobject_class->finalize = gwy_color_axis_finalize;
    gobject_class->get_property = gwy_color_axis_get_property;
    gobject_class->set_property = gwy_color_axis_set_property;
    gobject_class->notify = gwy_color_axis_notify;

    widget_class->get_preferred_width = gwy_color_axis_get_preferred_width;
    widget_class->get_preferred_height = gwy_color_axis_get_preferred_height;
    widget_class->realize = gwy_color_axis_realize;
    widget_class->unrealize = gwy_color_axis_unrealize;
    widget_class->draw = gwy_color_axis_draw;
    widget_class->scroll_event = gwy_color_axis_scroll;
    widget_class->motion_notify_event = gwy_color_axis_motion_notify;

    axis_class->get_horizontal_labels = gwy_color_axis_get_horizontal_labels;
    axis_class->get_split_width = gwy_color_axis_get_split_width;
    axis_class->get_units_affinity = gwy_color_axis_get_units_affinity;

    properties[PROP_GRADIENT]
        = g_param_spec_object("gradient",
                              "Gradient",
                              "Gradient used for visualisation.",
                              GWY_TYPE_GRADIENT,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_EDITABLE_RANGE]
        = g_param_spec_boolean("editable-range",
                               "Editable range",
                               "Whether the colour axis range can be edited "
                               "by the user.",
                               FALSE,
                               G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_DISTRIBUTION]
        = g_param_spec_object("distribution",
                              "Distribution",
                              "Distribution displayed along the axis.",
                              GWY_TYPE_CURVE,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    for (guint i = 1; i < N_PROPS; i++)
        g_object_class_install_property(gobject_class, i, properties[i]);

    /**
     * GwyColorAxis::modify-range:
     * @gwycoloraxis: The #GwyColorAxis which received the signal.
     * @arg1: The new range as #GwyRange.
     *
     * The ::modify-range signal is emitted when user interactively modifies
     * the axis range (if enabled with gwy_color_axis_set_editable_range()).
     * The new range is only <emphasis>requested</emphasis> at that time.
     *
     * It is an action signal.  So you may want to emit it programmatically
     * instead of gwy_axis_request_range(), however, this should be done
     * only if the colour axis is ‘authoritative’ for this range.  If the axis
     * range follows some other object you usually want to just call
     * gwy_axis_request_range() and be done with it.
     **/
    signals[SGNL_MODIFY_RANGE]
        = g_signal_new_class_handler("modify-range",
                                     G_OBJECT_CLASS_TYPE(klass),
                                     G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                                     G_CALLBACK(gwy_color_axis_modify_range),
                                     NULL, NULL,
                                     g_cclosure_marshal_VOID__BOXED,
                                     G_TYPE_NONE, 1, GWY_TYPE_RANGE);
}

static void
gwy_color_axis_init(GwyColorAxis *coloraxis)
{
    coloraxis->priv = G_TYPE_INSTANCE_GET_PRIVATE(coloraxis,
                                                  GWY_TYPE_COLOR_AXIS,
                                                  ColorAxis);
}

static void
gwy_color_axis_finalize(GObject *object)
{
    G_OBJECT_CLASS(gwy_color_axis_parent_class)->finalize(object);
}

static void
gwy_color_axis_dispose(GObject *object)
{
    GwyColorAxis *coloraxis = GWY_COLOR_AXIS(object);

    set_gradient(coloraxis, NULL);

    G_OBJECT_CLASS(gwy_color_axis_parent_class)->dispose(object);
}

static void
gwy_color_axis_set_property(GObject *object,
                            guint prop_id,
                            const GValue *value,
                            GParamSpec *pspec)
{
    GwyColorAxis *coloraxis = GWY_COLOR_AXIS(object);

    switch (prop_id) {
        case PROP_GRADIENT:
        set_gradient(coloraxis, g_value_get_object(value));
        break;

        case PROP_EDITABLE_RANGE:
        set_editable_range(coloraxis, g_value_get_boolean(value));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_color_axis_get_property(GObject *object,
                            guint prop_id,
                            GValue *value,
                            GParamSpec *pspec)
{
    ColorAxis *priv = GWY_COLOR_AXIS(object)->priv;

    switch (prop_id) {
        case PROP_GRADIENT:
        g_value_set_object(value, priv->gradient);
        break;

        case PROP_EDITABLE_RANGE:
        g_value_set_boolean(value, priv->editable_range);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_color_axis_notify(GObject *object,
                      GParamSpec *pspec)
{
    if (gwy_strequal(pspec->name, "edge"))
        discard_cursors(GWY_COLOR_AXIS(object));
}

static void
gwy_color_axis_get_preferred_width(GtkWidget *widget,
                                   gint *minimum,
                                   gint *natural)
{
    GwyAxis *axis = GWY_AXIS(widget);
    GtkPositionType edge = gwy_axis_get_edge(axis);

    if (edge == GTK_POS_TOP || edge == GTK_POS_BOTTOM) {
        *minimum = *natural = 1;
        return;
    }
    *minimum = *natural = 40;

    PangoLayout *layout = gwy_axis_get_pango_layout(axis);
    g_return_if_fail(layout);

    // Stripe width should be consistent with horizontal orientation.
    // So make it equal to the *height* of the test text while the rest of
    // the request is based on its width.
    ColorAxis *priv = GWY_COLOR_AXIS(widget)->priv;
    gint breadth, height;
    pango_layout_set_markup(layout, TESTMARKUP, sizeof(TESTMARKUP)-1);
    pango_layout_get_size(layout, &breadth, &height);
    priv->stripewidth = (gdouble)height/(breadth + height);
    breadth = (breadth + height)/(PANGO_SCALE);
    *minimum = *natural = MAX(breadth, 6);
}

static void
gwy_color_axis_get_preferred_height(GtkWidget *widget,
                                    gint *minimum,
                                    gint *natural)
{
    GwyAxis *axis = GWY_AXIS(widget);
    GtkPositionType edge = gwy_axis_get_edge(axis);

    if (edge == GTK_POS_LEFT || edge == GTK_POS_RIGHT) {
        *minimum = *natural = 1;
        return;
    }
    *minimum = *natural = 20;

    PangoLayout *layout = gwy_axis_get_pango_layout(axis);
    g_return_if_fail(layout);

    ColorAxis *priv = GWY_COLOR_AXIS(widget)->priv;
    gint breadth;
    pango_layout_set_markup(layout, TESTMARKUP, sizeof(TESTMARKUP)-1);
    pango_layout_get_size(layout, NULL, &breadth);
    priv->stripewidth = 5.0/13.0;
    breadth = 13*breadth/(5*PANGO_SCALE);
    *minimum = *natural = MAX(breadth, 4);
}

static void
gwy_color_axis_realize(GtkWidget *widget)
{
    GTK_WIDGET_CLASS(gwy_color_axis_parent_class)->realize(widget);
    GdkWindow *input_window = gwy_axis_get_input_window(GWY_AXIS(widget));
    gdk_window_set_events(input_window,
                          gdk_window_get_events(input_window)
                          | GDK_SCROLL_MASK
                          | GDK_POINTER_MOTION_MASK
                          | GDK_POINTER_MOTION_HINT_MASK);
}

static void
gwy_color_axis_unrealize(GtkWidget *widget)
{
    discard_cursors(GWY_COLOR_AXIS(widget));
    GTK_WIDGET_CLASS(gwy_color_axis_parent_class)->unrealize(widget);
}

static gboolean
gwy_color_axis_draw(GtkWidget *widget,
                    cairo_t *cr)
{
    GwyColorAxis *coloraxis = GWY_COLOR_AXIS(widget);
    ColorAxis *priv = coloraxis->priv;
    GwyAxis *axis = GWY_AXIS(widget);
    GtkStyleContext *context = gtk_widget_get_style_context(widget);
    gdouble width = gtk_widget_get_allocated_width(widget),
            height = gtk_widget_get_allocated_height(widget);
    GtkPositionType edge = gwy_axis_get_edge(axis);
    gboolean vertical = (edge == GTK_POS_LEFT || edge == GTK_POS_RIGHT);
    gdouble length = (vertical ? height : width),
            breadth = (vertical ? width : height),
            stripebreadth = gwy_round(priv->stripewidth*breadth);
    cairo_matrix_t matrix;
    set_up_transform(edge, &matrix, width, height);

    guint nticks;
    const GwyAxisTick *ticks = gwy_axis_ticks(axis, &nticks);
    PangoLayout *layout = gwy_axis_get_pango_layout(axis);
    gint max_ascent = G_MININT, max_descent = G_MININT;

    gtk_render_background(context, cr, 0, 0, width, height);
    draw_distribution(coloraxis, cr, &matrix, breadth, stripebreadth, length);
    draw_ticks(ticks, nticks, cr, context, &matrix, stripebreadth,
               &max_ascent, &max_descent);
    draw_stripe(priv->gradient, cr, edge, &matrix, length, stripebreadth);
    draw_labels(ticks, nticks, cr, edge, context, layout, &matrix,
                breadth, stripebreadth, max_ascent, max_descent);

    return FALSE;
}

static gboolean
gwy_color_axis_scroll(GtkWidget *widget,
                      GdkEventScroll *event)
{
    if (!(event->direction == GDK_SCROLL_UP
          || event->direction == GDK_SCROLL_DOWN))
        return FALSE;

    ColorAxis *priv = GWY_COLOR_AXIS(widget)->priv;
    if (!priv->editable_range)
        return FALSE;

    GwyAxis *axis = GWY_AXIS(widget);
    gint boundary = find_boundary(axis, event->x, event->y);
    priv->last_boundary = boundary;
    if (!boundary)
        return FALSE;

    GwyRange range;
    gwy_axis_get_range(axis, &range);
    gdouble len = fabs(range.to - range.from);
    GtkAdjustment *adj = gtk_adjustment_new(0.5*(range.from + range.to),
                                            range.from, range.to,
                                            0.002*len, 0.02*len, 0.0);
    gdouble dz = -gwy_scroll_wheel_delta(adj, event, GTK_ORIENTATION_VERTICAL);
    g_object_unref(adj);

    if (boundary == -1)
        range.from += dz;
    else
        range.to += dz;

    g_signal_emit(axis, signals[SGNL_MODIFY_RANGE], 0, &range);
    return TRUE;
}

static gboolean
gwy_color_axis_motion_notify(GtkWidget *widget,
                             GdkEventMotion *event)
{
    GwyColorAxis *coloraxis = GWY_COLOR_AXIS(widget);
    ColorAxis *priv = coloraxis->priv;
    if (!priv->editable_range)
        return FALSE;

    GwyAxis *axis = GWY_AXIS(widget);
    gint boundary = find_boundary(axis, event->x, event->y);
    if (boundary == priv->last_boundary)
        return FALSE;

    priv->last_boundary = boundary;
    ensure_cursors(coloraxis);
    GdkWindow *input_window = gwy_axis_get_input_window(axis);
    if (boundary == 1)
        gdk_window_set_cursor(input_window, priv->cursor_to);
    else if (boundary == -1)
        gdk_window_set_cursor(input_window, priv->cursor_from);
    else
        gdk_window_set_cursor(input_window, NULL);

    return FALSE;
}

static gboolean
gwy_color_axis_get_horizontal_labels(G_GNUC_UNUSED const GwyAxis *axis)
{
    return TRUE;
}

static guint
gwy_color_axis_get_split_width(const GwyAxis *axis)
{
    GtkPositionType edge = gwy_axis_get_edge(axis);
    if (edge == GTK_POS_TOP || edge == GTK_POS_BOTTOM)
        return G_MAXUINT;

    GwyColorAxis *coloraxis = GWY_COLOR_AXIS(axis);
    ColorAxis *priv = coloraxis->priv;
    guint breadth = gtk_widget_get_allocated_width(GTK_WIDGET(axis));
    gdouble stripebreadth = priv->stripewidth*breadth;
    gdouble w = fmax(breadth*(1.0 - priv->stripewidth) - 2.0, 1.0);
    return (guint)ceil(w - tick_level_sizes[GWY_AXIS_TICK_MINOR]*stripebreadth);
}

static void
gwy_color_axis_get_units_affinity(const GwyAxis *axis,
                                  GwyAxisUnitPlacement *primary,
                                  GwyAxisUnitPlacement *secondary)
{
    GtkPositionType edge = gwy_axis_get_edge(axis);
    if (edge == GTK_POS_RIGHT)
       *primary = *secondary = GWY_AXIS_UNITS_LAST;
    else
       *primary = *secondary = GWY_AXIS_UNITS_FIRST;
}

static void
gwy_color_axis_modify_range(GwyColorAxis *coloraxis,
                            const GwyRange *range)
{
    gwy_axis_request_range(GWY_AXIS(coloraxis), range);
}

/**
 * gwy_color_axis_new:
 *
 * Creates a new colour axis.
 *
 * Returns: A new colour axis.
 **/
GtkWidget*
gwy_color_axis_new(void)
{
    return g_object_newv(GWY_TYPE_COLOR_AXIS, 0, NULL);
}

/**
 * gwy_color_axis_set_gradient:
 * @coloraxis: A colour axis.
 * @gradient: (allow-none):
 *            A colour gradient.  %NULL means the default gradient
 *            will be used.
 *
 * Sets the false colour gradient a colour axis will visualise.
 **/
void
gwy_color_axis_set_gradient(GwyColorAxis *coloraxis,
                            GwyGradient *gradient)
{
    g_return_if_fail(GWY_IS_COLOR_AXIS(coloraxis));
    if (!set_gradient(coloraxis, gradient))
        return;

    g_object_notify_by_pspec(G_OBJECT(coloraxis), properties[PROP_GRADIENT]);
}

/**
 * gwy_color_axis_get_gradient:
 * @coloraxis: A colour axis.
 *
 * Obtains the false colour gradient that a colour axis visualises.
 *
 * Returns: (allow-none) (transfer none):
 *          The colour gradient used by @coloraxis.  If no gradient was set and
 *          the default one is used, function returns %NULL.
 **/
GwyGradient*
gwy_color_axis_get_gradient(const GwyColorAxis *coloraxis)
{
    g_return_val_if_fail(GWY_IS_COLOR_AXIS(coloraxis), NULL);
    return coloraxis->priv->gradient;
}

/**
 * gwy_color_axis_set_editable_range:
 * @coloraxis: A colour axis.
 * @editablerange: %TRUE to enable editing of the range by user, %FALSE to
 *                 disable it.
 *
 * Sets whether the range of a colour axis can be edited by the user.
 **/
void
gwy_color_axis_set_editable_range(GwyColorAxis *coloraxis,
                                  gboolean editablerange)
{
    g_return_if_fail(GWY_IS_COLOR_AXIS(coloraxis));
    if (!set_editable_range(coloraxis, editablerange))
        return;

    g_object_notify_by_pspec(G_OBJECT(coloraxis),
                             properties[PROP_EDITABLE_RANGE]);
}

/**
 * gwy_color_axis_get_editable_range:
 * @coloraxis: A colour axis.
 *
 * Gets whether the range of a colour axis can be edited by the user.
 *
 * Returns: %TRUE if the range is user-editable, %FALSE if it is not.
 **/
gboolean
gwy_color_axis_get_editable_range(const GwyColorAxis *coloraxis)
{
    g_return_val_if_fail(GWY_IS_COLOR_AXIS(coloraxis), FALSE);
    return coloraxis->priv->editable_range;
}

/**
 * gwy_color_axis_get_stripe_breadth:
 * @coloraxis: A colour axis.
 *
 * Gets the breadth of the colour stripe of a colour axis.
 *
 * Breadth is the dimension in direction orthogonal to the axis range.
 *
 * Returns: Colour stripe breadth.
 **/
guint
gwy_color_axis_get_stripe_breadth(const GwyColorAxis *coloraxis)
{
    g_return_val_if_fail(GWY_IS_COLOR_AXIS(coloraxis), 0);
    GwyAxis *axis = GWY_AXIS(coloraxis);
    GtkWidget *widget = GTK_WIDGET(axis);
    gdouble width = gtk_widget_get_allocated_width(widget),
            height = gtk_widget_get_allocated_height(widget);
    GtkPositionType edge = gwy_axis_get_edge(axis);
    gboolean vertical = (edge == GTK_POS_LEFT || edge == GTK_POS_RIGHT);
    gdouble breadth = (vertical ? width : height);
    return gwy_round(coloraxis->priv->stripewidth*breadth);
}

/**
 * gwy_color_axis_set_distribution:
 * @coloraxis: A color_axis.
 * @distribution: (allow-none):
 *                Curve to display along the colour axis a visualisation of
 *                distribution of the corresponding quantity.
 *
 * Sets the curve displayed along a colour axis.
 **/
void
gwy_color_axis_set_distribution(GwyColorAxis *coloraxis,
                                GwyCurve *distribution)
{
    g_return_if_fail(GWY_IS_COLOR_AXIS(coloraxis));
    if (!set_distribution(coloraxis, distribution))
        return;

    g_object_notify_by_pspec(G_OBJECT(coloraxis),
                             properties[PROP_DISTRIBUTION]);
}

/**
 * gwy_color_axis_get_distribution:
 * @coloraxis: A color_axis.
 *
 * Gets the curve displayed along the colour axis.
 *
 * Returns: (allow-none) (transfer none):
 *          The curve displayed along the colour axis, or %NULL.
 **/
GwyCurve*
gwy_color_axis_get_distribution(const GwyColorAxis *coloraxis)
{
    g_return_val_if_fail(GWY_IS_COLOR_AXIS(coloraxis), NULL);
    return coloraxis->priv->distribution;
}

static void
draw_ticks(const GwyAxisTick *ticks,
           guint nticks,
           cairo_t *cr,
           GtkStyleContext *context,
           const cairo_matrix_t *matrix,
           gdouble stripebreadth,
           gint *max_ascent, gint *max_descent)
{
    GdkRGBA rgba;
    gtk_style_context_get_color(context, GTK_STATE_NORMAL, &rgba);
    gdk_cairo_set_source_rgba(cr, &rgba);
    cairo_set_line_width(cr, 1.0);

    for (guint i = 0; i < nticks; i++) {
        gdouble pos = ticks[i].position;
        gdouble s = tick_level_sizes[ticks[i].level];
        draw_line_transformed(cr, matrix,
                              pos, stripebreadth,
                              pos, (1.0 + s)*stripebreadth);
        if (ticks[i].label) {
            *max_ascent = MAX(*max_ascent, PANGO_ASCENT(ticks[i].extents));
            *max_descent = MAX(*max_descent, PANGO_DESCENT(ticks[i].extents));
        }
    }
    cairo_stroke(cr);
}

static void
draw_stripe(GwyGradient *gradient,
            cairo_t *cr,
            GtkPositionType edge,
            const cairo_matrix_t *matrix,
            gdouble length, gdouble stripebreadth)
{
    gboolean vertical = (edge == GTK_POS_LEFT || edge == GTK_POS_RIGHT);
    GtkPositionType towards = (vertical ? GTK_POS_TOP : GTK_POS_RIGHT);
    if (!gradient)
        gradient = gwy_gradients_get(NULL);
    cairo_pattern_t *pattern = gwy_cairo_pattern_create_gradient(gradient,
                                                                 towards);
    cairo_matrix_t patmatrix;
    gdouble xo = 0.5, yo = 0.5, xe = length - 0.5, ye = stripebreadth - 0.5;
    cairo_matrix_transform_point(matrix, &xo, &yo);
    cairo_matrix_transform_point(matrix, &xe, &ye);
    GWY_ORDER(gdouble, xo, xe);
    GWY_ORDER(gdouble, yo, ye);
    cairo_matrix_init_scale(&patmatrix,
                            (vertical ? 1.0 : 1.0/length),
                            (vertical ? 1.0/length : 1.0));
    cairo_pattern_set_matrix(pattern, &patmatrix);
    cairo_rectangle(cr, xo, yo, xe - xo, ye - yo);
    cairo_save(cr);
    cairo_set_source(cr, pattern);
    cairo_fill_preserve(cr);
    cairo_restore(cr);
    cairo_pattern_destroy(pattern);
    cairo_stroke(cr);
}

static void
draw_labels(const GwyAxisTick *ticks,
            guint nticks,
            cairo_t *cr,
            GtkPositionType edge,
            GtkStyleContext *context,
            PangoLayout *layout,
            const cairo_matrix_t *matrix,
            gdouble breadth, gdouble stripebreadth,
            gint max_ascent, gint max_descent)
{
    gdouble a = max_ascent/pangoscale, d = max_descent/pangoscale;
    for (guint i = 0; i < nticks; i++) {
        if (!ticks[i].label)
            continue;

        gdouble x = ticks[i].position, y = breadth;
        cairo_matrix_transform_point(matrix, &x, &y);
        pango_layout_set_markup(layout, ticks[i].label, -1);
        if (edge == GTK_POS_BOTTOM) {
            if (i == nticks-1 && ticks[i].level == GWY_AXIS_TICK_EDGE)
                x -= 2.0 + ticks[i].extents.width/pangoscale;
            else
                x += 2.0;
            y = breadth - (a + d);
        }
        else if (edge == GTK_POS_LEFT) {
            if (i == 0 && ticks[i].level == GWY_AXIS_TICK_EDGE)
                y -= 2.0 + ticks[i].extents.height/pangoscale;
            else
                y += 2.0;
            x = breadth - ticks[i].extents.width/pangoscale - 2.0 - 2.0
                - (1.0 + tick_level_sizes[GWY_AXIS_TICK_MINOR])*stripebreadth;
        }
        else if (edge == GTK_POS_TOP) {
            if (i == nticks-1 && ticks[i].level == GWY_AXIS_TICK_EDGE)
                x -= 2.0 + ticks[i].extents.width/pangoscale;
            else
                x += 2.0;
            y = a;
        }
        else if (edge == GTK_POS_RIGHT) {
            if (i == 0 && ticks[i].level == GWY_AXIS_TICK_EDGE)
                y -= 2.0 + ticks[i].extents.height/pangoscale;
            else
                y += 2.0;
            x = (1.0 + tick_level_sizes[GWY_AXIS_TICK_MINOR])*stripebreadth
                + 2.0 + 2.0;
        }
        gtk_render_layout(context, cr, x, y, layout);
    }
}

static void
draw_distribution(GwyColorAxis *coloraxis,
                  cairo_t *cr,
                  const cairo_matrix_t *matrix,
                  gdouble breadth, gdouble stripebreadth, gdouble length)
{
    ColorAxis *priv = coloraxis->priv;
    GwyCurve *curve = priv->distribution;

    if (!curve)
        return;

    GwyRange range;
    gwy_axis_get_range(GWY_AXIS(coloraxis), &range);

    gdouble from = range.from, to = range.to;
    guint n = curve->n;
    const GwyXY *xy = curve->data;

    // TODO: Backward ranges!
    gboolean descending = (to < from);
    gdouble min = fmin(from, to), max = fmax(from, to);
    if (!n || xy[0].x > max || xy[n-1].x < min)
        return;

    if (!priv->distribution_max_valid) {
        gwy_curve_min_max_full(curve, NULL, &priv->distribution_max);
        priv->distribution_max_valid = TRUE;
    }

    gdouble m = priv->distribution_max;
    if (!m)
        return;

    cairo_save(cr);
    gdouble x = descending ? length : 0, y = stripebreadth;
    cairo_matrix_transform_point(matrix, &x, &y);
    cairo_move_to(cr, x, y);
    for (guint i = 0; i < n; i++) {
        // TODO: Handle edges more precisely, i.e. using interpolation, for
        // highly zoomed-in ranges.
        if (xy[i].x >= min && xy[i].x <= max) {
            x = length*(xy[i].x - from)/(to - from);
            y = stripebreadth + xy[i].y/m*(breadth - stripebreadth);
            cairo_matrix_transform_point(matrix, &x, &y);
            cairo_line_to(cr, x, y);
        }
    }
    x = descending ? 0 : length;
    y = stripebreadth;
    cairo_matrix_transform_point(matrix, &x, &y);
    cairo_line_to(cr, x, y);
    // FIXME: Theming.
    cairo_set_source_rgb(cr, 0.6, 0.6, 1.0);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke_preserve(cr);
    cairo_close_path(cr);
    cairo_set_source_rgba(cr, 0.6, 0.6, 1.0, 0.4);
    cairo_fill(cr);
    cairo_restore(cr);
}

static void
set_up_transform(GtkPositionType edge,
                 cairo_matrix_t *matrix,
                 gdouble width, gdouble height)
{
    if (edge == GTK_POS_TOP)
        cairo_matrix_init(matrix, 1.0, 0.0, 0.0, -1.0, 0.0, height);
    else if (edge == GTK_POS_LEFT)
        cairo_matrix_init(matrix, 0.0, -1.0, -1.0, 0.0, width, height);
    else if (edge == GTK_POS_BOTTOM)
        cairo_matrix_init_identity(matrix);
    else if (edge == GTK_POS_RIGHT)
        cairo_matrix_init(matrix, 0.0, -1.0, 1.0, 0.0, 0.0, height);
    else {
        g_assert_not_reached();
    }
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

static gboolean
set_gradient(GwyColorAxis *coloraxis,
             GwyGradient *gradient)
{
    ColorAxis *priv = coloraxis->priv;
    if (!gwy_set_member_object(coloraxis, gradient, GWY_TYPE_GRADIENT,
                               &priv->gradient,
                               "data-changed", &gradient_data_changed,
                               &priv->gradient_data_changed_id,
                               G_CONNECT_SWAPPED,
                               NULL))
        return FALSE;

    gtk_widget_queue_draw(GTK_WIDGET(coloraxis));
    return TRUE;
}

static gboolean
set_editable_range(GwyColorAxis *coloraxis,
                   gboolean setting)
{
    ColorAxis *priv = coloraxis->priv;
    if (!setting == !priv->editable_range)
        return FALSE;

    priv->editable_range = setting;
    return TRUE;
}

static gboolean
set_distribution(GwyColorAxis *coloraxis,
                 GwyCurve *distribution)
{
    ColorAxis *priv = coloraxis->priv;
    if (!gwy_set_member_object(coloraxis, distribution, GWY_TYPE_CURVE,
                               &priv->distribution,
                               "data-changed", &distribution_data_changed,
                               &priv->distribution_data_changed_id,
                               G_CONNECT_SWAPPED,
                               NULL))
        return FALSE;

    gtk_widget_queue_draw(GTK_WIDGET(coloraxis));
    return TRUE;
}

static void
gradient_data_changed(GwyColorAxis *coloraxis,
                      G_GNUC_UNUSED GwyGradient *gradient)
{
    gtk_widget_queue_draw(GTK_WIDGET(coloraxis));
}

static void
distribution_data_changed(GwyColorAxis *coloraxis,
                          G_GNUC_UNUSED GwyCurve *distribution)
{
    coloraxis->priv->distribution_max_valid = FALSE;
    gtk_widget_queue_draw(GTK_WIDGET(coloraxis));
}

static void
ensure_cursors(GwyColorAxis *coloraxis)
{
    ColorAxis *priv = coloraxis->priv;
    if (priv->cursor_from && priv->cursor_to)
        return;

    GdkDisplay *display = gtk_widget_get_display(GTK_WIDGET(coloraxis));
    GwyAxis *axis = GWY_AXIS(coloraxis);
    GtkPositionType edge = gwy_axis_get_edge(axis);
    if (edge == GTK_POS_LEFT || edge == GTK_POS_RIGHT) {
        priv->cursor_from = gdk_cursor_new_for_display(display,
                                                       GDK_SB_DOWN_ARROW);
        priv->cursor_to = gdk_cursor_new_for_display(display,
                                                     GDK_SB_UP_ARROW);
    }
    else {
        priv->cursor_from = gdk_cursor_new_for_display(display,
                                                       GDK_SB_LEFT_ARROW);
        priv->cursor_to = gdk_cursor_new_for_display(display,
                                                     GDK_SB_RIGHT_ARROW);
    }
}

static void
discard_cursors(GwyColorAxis *coloraxis)
{
    ColorAxis *priv = coloraxis->priv;
    GWY_OBJECT_UNREF(priv->cursor_from);
    GWY_OBJECT_UNREF(priv->cursor_to);
}

static gint
find_boundary(GwyAxis *axis, gdouble x, gdouble y)
{
    GtkWidget *widget = GTK_WIDGET(axis);
    GtkPositionType edge = gwy_axis_get_edge(axis);
    gboolean vertical = (edge == GTK_POS_LEFT || edge == GTK_POS_RIGHT);
    gdouble width = gtk_widget_get_allocated_width(widget),
            height = gtk_widget_get_allocated_height(widget);
    gdouble length = (vertical ? height : width),
            pos = (vertical ? height - y : x);

    if (pos >= 3.0*length/4.0)
        return 1;
    if (pos <= length/4.0)
        return -1;
    return 0;
}

/**
 * SECTION: color-axis
 * @title: GwyColorAxis
 * @short_description: Axis showing a false colour map
 * @image: GwyColorAxis.png
 **/

/**
 * GwyColorAxis:
 *
 * Axis widget showing a false colour rmap.
 *
 * The #GwyColorAxis struct contains private data only and should be accessed
 * using the functions below.
 **/

/**
 * GwyColorAxisClass:
 *
 * Class of axes showing false colour maps.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
