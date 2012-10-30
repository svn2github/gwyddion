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
#include "libgwy/object-utils.h"
#include "libgwyui/cairo-utils.h"
#include "libgwyui/color-axis.h"

#define TESTMARKUP "<small>9.9 (μm₁¹)</small>"

#define pangoscale ((gdouble)PANGO_SCALE)

enum {
    PROP_0,
    PROP_GRADIENT,
    N_PROPS,
};

struct _GwyColorAxisPrivate {
    GwyGradient *gradient;
    gulong gradient_data_changed_id;
    gdouble stripewidth;     // Relative to total.
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
static void     gwy_color_axis_get_preferred_width  (GtkWidget *widget,
                                                     gint *minimum,
                                                     gint *natural);
static void     gwy_color_axis_get_preferred_height (GtkWidget *widget,
                                                     gint *minimum,
                                                     gint *natural);
static gboolean gwy_color_axis_draw                 (GtkWidget *widget,
                                                     cairo_t *cr);
static gboolean gwy_color_axis_get_horizontal_labels(const GwyAxis *axis);
static guint    gwy_color_axis_get_split_width      (const GwyAxis *axis);
static gboolean set_gradient                        (GwyColorAxis *color_axis,
                                                     GwyGradient *gradient);
static void     gradient_data_changed               (GwyColorAxis *coloraxis,
                                                     GwyGradient *gradient);

static GParamSpec *properties[N_PROPS];

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

    widget_class->get_preferred_width = gwy_color_axis_get_preferred_width;
    widget_class->get_preferred_height = gwy_color_axis_get_preferred_height;
    widget_class->draw = gwy_color_axis_draw;

    axis_class->get_horizontal_labels = gwy_color_axis_get_horizontal_labels;
    axis_class->get_split_width = gwy_color_axis_get_split_width;

    properties[PROP_GRADIENT]
        = g_param_spec_object("gradient",
                              "Gradient",
                              "Gradient used for visualisation.",
                              GWY_TYPE_GRADIENT,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    for (guint i = 1; i < N_PROPS; i++)
        g_object_class_install_property(gobject_class, i, properties[i]);
}

static void
gwy_color_axis_init(GwyColorAxis *color_axis)
{
    color_axis->priv = G_TYPE_INSTANCE_GET_PRIVATE(color_axis,
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
    GwyColorAxis *color_axis = GWY_COLOR_AXIS(object);

    switch (prop_id) {
        case PROP_GRADIENT:
        set_gradient(color_axis, g_value_get_object(value));
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

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
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
            stripebreadth = priv->stripewidth*breadth;
    GtkPositionType towards = (vertical ? GTK_POS_TOP : GTK_POS_RIGHT);
    cairo_matrix_t matrix;
    set_up_transform(edge, &matrix, width, height);

    GdkRGBA rgba;
    gtk_render_background(context, cr, 0, 0, width, height);
    gtk_style_context_get_color(context, GTK_STATE_NORMAL, &rgba);
    gdk_cairo_set_source_rgba(cr, &rgba);
    cairo_set_line_width(cr, 1.0);

    guint nticks;
    const GwyAxisTick *ticks = gwy_axis_ticks(axis, &nticks);
    gint max_ascent = G_MININT, max_descent = G_MININT;

    for (guint i = 0; i < nticks; i++) {
        gdouble pos = ticks[i].position;
        gdouble s = tick_level_sizes[ticks[i].level];
        draw_line_transformed(cr, &matrix,
                              pos, stripebreadth,
                              pos, (1.0 + s)*stripebreadth);
        if (ticks[i].label) {
            max_ascent = MAX(max_ascent, PANGO_ASCENT(ticks[i].extents));
            max_descent = MAX(max_descent, PANGO_DESCENT(ticks[i].extents));
        }
    }
    cairo_stroke(cr);

    GwyGradient *gradient = (priv->gradient
                             ? priv->gradient
                             : gwy_gradients_get(NULL));
    cairo_pattern_t *pattern = gwy_cairo_pattern_create_gradient(gradient,
                                                                 towards);
    cairo_matrix_t patmatrix;
    gdouble xo = 0, yo = 0, xe = length, ye = stripebreadth;
    cairo_matrix_transform_point(&matrix, &xo, &yo);
    cairo_matrix_transform_point(&matrix, &xe, &ye);
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

    PangoLayout *layout = gwy_axis_get_pango_layout(axis);
    gdouble a = max_ascent/pangoscale, d = max_descent/pangoscale;
    for (guint i = 0; i < nticks; i++) {
        if (!ticks[i].label)
            continue;

        gdouble x = ticks[i].position, y = breadth;
        cairo_matrix_transform_point(&matrix, &x, &y);
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
            x = breadth - stripebreadth - 2.0 - 2.0
                - ticks[i].extents.width/pangoscale;
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
            x = stripebreadth + 2.0 + 2.0;
        }
        gtk_render_layout(context, cr, x, y, layout);
    }

    return FALSE;
}

static guint
gwy_color_axis_get_split_width(const GwyAxis *axis)
{
    GtkPositionType edge = gwy_axis_get_edge(axis);
    if (edge == GTK_POS_TOP || edge == GTK_POS_BOTTOM)
        return G_MAXUINT;

    GwyColorAxis *coloraxis = GWY_COLOR_AXIS(axis);
    guint width = gtk_widget_get_allocated_width(GTK_WIDGET(axis));
    gdouble w = fmax(width*(1.0 - coloraxis->priv->stripewidth) - 2.0, 1.0);
    return (guint)ceil(w);
}

static gboolean
gwy_color_axis_get_horizontal_labels(G_GNUC_UNUSED const GwyAxis *axis)
{
    return TRUE;
}

/**
 * gwy_color_axis_new:
 *
 * Creates a new color_axis.
 *
 * Returns: A new color_axis.
 **/
GtkWidget*
gwy_color_axis_new(void)
{
    return g_object_newv(GWY_TYPE_COLOR_AXIS, 0, NULL);
}

/**
 * gwy_color_axis_set_gradient:
 * @coloraxis: A color axis.
 * @gradient: (allow-none):
 *            A colour gradient.  %NULL means the default gradient
 *            will be used.
 *
 * Sets the false colour gradient a color axis will visualise.
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
 * @coloraxis: A color axis.
 *
 * Obtains the false colour gradient that a color axis visualises.
 *
 * Returns: (allow-none) (transfer none):
 *          The colour gradient used by @coloraxis.  If no gradient was set and
 *          the default one is used, function returns %NULL.
 **/
GwyGradient*
gwy_color_axis_get_gradient(const GwyColorAxis *coloraxis)
{
    g_return_val_if_fail(GWY_IS_COLOR_AXIS(coloraxis), NULL);
    return GWY_COLOR_AXIS(coloraxis)->priv->gradient;
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

static void
gradient_data_changed(GwyColorAxis *coloraxis,
                      G_GNUC_UNUSED GwyGradient *gradient)
{
    gtk_widget_queue_draw(GTK_WIDGET(coloraxis));
}

/**
 * SECTION: color-axis
 * @section_id: GwyColorAxis
 * @title: ColorAxis
 * @short_description: ColorAxis showing a false colour map
 **/

/**
 * GwyColorAxis:
 *
 * ColorAxis widget showing a false colour rmap.
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
