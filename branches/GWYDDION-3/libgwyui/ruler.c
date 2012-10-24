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
#include "libgwyui/ruler.h"

#define TESTMARKUP "<small>(q₁¹)</small>"

enum {
    PROP_0,
    PROP_SHOW_MARK,
    PROP_MARK,
    N_PROPS,
};

struct _GwyRulerPrivate {
    gdouble mark;
    gdouble mark_pos;
    gboolean show_mark;
};

typedef struct _GwyRulerPrivate Ruler;

static void     gwy_ruler_dispose             (GObject *object);
static void     gwy_ruler_finalize            (GObject *object);
static void     gwy_ruler_set_property        (GObject *object,
                                               guint prop_id,
                                               const GValue *value,
                                               GParamSpec *pspec);
static void     gwy_ruler_get_property        (GObject *object,
                                               guint prop_id,
                                               GValue *value,
                                               GParamSpec *pspec);
static void     gwy_ruler_get_preferred_width (GtkWidget *widget,
                                               gint *minimum,
                                               gint *natural);
static void     gwy_ruler_get_preferred_height(GtkWidget *widget,
                                               gint *minimum,
                                               gint *natural);
static gboolean gwy_ruler_draw                (GtkWidget *widget,
                                               cairo_t *cr);
static gboolean set_show_mark                 (GwyRuler *ruler,
                                               gboolean setting);
static gboolean set_mark                      (GwyRuler *ruler,
                                               gdouble value);
static void     draw_mark                     (GwyRuler *ruler,
                                               cairo_t *cr,
                                               const cairo_matrix_t *matrix,
                                               gdouble length,
                                               gdouble breadth);
static void     invalidate_mark_area          (GwyRuler *ruler);
static gint     preferred_breadth             (GtkWidget *widget);

static GParamSpec *properties[N_PROPS];

G_DEFINE_TYPE(GwyRuler, gwy_ruler, GWY_TYPE_AXIS);

static void
gwy_ruler_class_init(GwyRulerClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

    g_type_class_add_private(klass, sizeof(Ruler));

    gobject_class->dispose = gwy_ruler_dispose;
    gobject_class->finalize = gwy_ruler_finalize;
    gobject_class->get_property = gwy_ruler_get_property;
    gobject_class->set_property = gwy_ruler_set_property;

    widget_class->get_preferred_width = gwy_ruler_get_preferred_width;
    widget_class->get_preferred_height = gwy_ruler_get_preferred_height;
    widget_class->draw = gwy_ruler_draw;

    properties[PROP_SHOW_MARK]
        = g_param_spec_boolean("show-mark",
                               "Show mark",
                               "Whether to show a mark at the mark value.",
                               FALSE,
                               G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_MARK]
        = g_param_spec_double("mark",
                              "Mark",
                              "Value at which a mark should be shown.",
                               -G_MAXDOUBLE, G_MAXDOUBLE, 0.0,
                               G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    for (guint i = 1; i < N_PROPS; i++)
        g_object_class_install_property(gobject_class, i, properties[i]);
}

static void
gwy_ruler_init(GwyRuler *ruler)
{
    ruler->priv = G_TYPE_INSTANCE_GET_PRIVATE(ruler, GWY_TYPE_RULER, Ruler);
}

static void
gwy_ruler_finalize(GObject *object)
{
    G_OBJECT_CLASS(gwy_ruler_parent_class)->finalize(object);
}

static void
gwy_ruler_dispose(GObject *object)
{
    G_OBJECT_CLASS(gwy_ruler_parent_class)->dispose(object);
}

static void
gwy_ruler_set_property(GObject *object,
                      guint prop_id,
                      const GValue *value,
                      GParamSpec *pspec)
{
    GwyRuler *ruler = GWY_RULER(object);

    switch (prop_id) {
        case PROP_SHOW_MARK:
        set_show_mark(ruler, g_value_get_boolean(value));
        break;

        case PROP_MARK:
        set_mark(ruler, g_value_get_double(value));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_ruler_get_property(GObject *object,
                      guint prop_id,
                      GValue *value,
                      GParamSpec *pspec)
{
    Ruler *priv = GWY_RULER(object)->priv;

    switch (prop_id) {
        case PROP_SHOW_MARK:
        g_value_set_boolean(value, priv->show_mark);
        break;

        case PROP_MARK:
        g_value_set_double(value, priv->mark);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_ruler_get_preferred_width(GtkWidget *widget,
                              gint *minimum,
                              gint *natural)
{
    GwyAxis *axis = GWY_AXIS(widget);
    GtkPositionType edge = gwy_axis_get_edge(axis);

    if (edge == GTK_POS_TOP || edge == GTK_POS_BOTTOM)
        *minimum = *natural = 1;
    else
        *minimum = *natural = preferred_breadth(widget);
}

static void
gwy_ruler_get_preferred_height(GtkWidget *widget,
                               gint *minimum,
                               gint *natural)
{
    GwyAxis *axis = GWY_AXIS(widget);
    GtkPositionType edge = gwy_axis_get_edge(axis);

    if (edge == GTK_POS_LEFT || edge == GTK_POS_RIGHT)
        *minimum = *natural = 1;
    else
        *minimum = *natural = preferred_breadth(widget);
}

static void
set_up_transform(GtkPositionType edge,
                 cairo_matrix_t *matrix,
                 gdouble width, gdouble height)
{
    if (edge == GTK_POS_TOP)
        cairo_matrix_init(matrix, 1.0, 0.0, 0.0, -1.0, 0.0, height);
    else if (edge == GTK_POS_LEFT)
        cairo_matrix_init(matrix, 0.0, 1.0, -1.0, 0.0, width, 0.0);
    else if (edge == GTK_POS_BOTTOM)
        cairo_matrix_init_identity(matrix);
    else if (edge == GTK_POS_RIGHT)
        cairo_matrix_init(matrix, 0.0, 1.0, 1.0, 0.0, 0.0, 0.0);
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
gwy_ruler_draw(GtkWidget *widget,
               cairo_t *cr)
{
    static const gdouble tick_level_sizes[4] = { 1.0, 0.9, 0.45, 0.25 };
    g_printerr("RULER DRAW %p\n", widget);

    GwyAxis *axis = GWY_AXIS(widget);
    GtkStyleContext *context = gtk_widget_get_style_context(widget);
    gdouble width = gtk_widget_get_allocated_width(widget),
            height = gtk_widget_get_allocated_height(widget);
    GtkPositionType edge = gwy_axis_get_edge(axis);
    cairo_matrix_t matrix;
    gdouble length = width, breadth = height;

    if (edge == GTK_POS_LEFT || edge == GTK_POS_RIGHT)
        GWY_SWAP(gdouble, length, breadth);

    set_up_transform(edge, &matrix, width, height);

    gtk_render_background(context, cr, 0, 0, width, height);
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_set_line_width(cr, 0.8);

    GwyRuler *ruler = GWY_RULER(widget);
    draw_mark(ruler, cr, &matrix, length, breadth);

    guint nticks;
    const GwyAxisTick *ticks = gwy_axis_ticks(axis, &nticks);
    gint max_ascent = G_MININT, max_descent = G_MININT;

    draw_line_transformed(cr, &matrix, 0, 0, length, 0);
    for (guint i = 0; i < nticks; i++) {
        gdouble pos = ticks[i].position;
        gdouble s = tick_level_sizes[ticks[i].level];
        draw_line_transformed(cr, &matrix, pos, 0, pos, s*breadth);
        if (ticks[i].label) {
            max_ascent = MAX(max_ascent, PANGO_ASCENT(ticks[i].extents));
            max_descent = MAX(max_descent, PANGO_DESCENT(ticks[i].extents));
        }
    }
    cairo_stroke(cr);

    PangoLayout *layout = gwy_axis_get_pango_layout(axis);
    gdouble a = max_ascent/(gdouble)PANGO_SCALE,
            d = max_descent/(gdouble)PANGO_SCALE;
    for (guint i = 0; i < nticks; i++) {
        if (!ticks[i].label)
            continue;

        gdouble x = ticks[i].position, y = breadth;
        cairo_matrix_transform_point(&matrix, &x, &y);
        pango_layout_set_markup(layout, ticks[i].label, -1);
        if (edge == GTK_POS_BOTTOM) {
            x += 2.0;
            y = breadth - (a + d);
        }
        else if (edge == GTK_POS_LEFT) {
            y += 2.0;
            x += a + d;
        }
        else if (edge == GTK_POS_TOP) {
            x += 2.0;
            y = a;
        }
        else if (edge == GTK_POS_RIGHT) {
            y += 2.0 + PANGO_RBEARING(ticks[i].extents)/(gdouble)PANGO_SCALE;
            x = breadth - (a + d);
        }
        gtk_render_layout(context, cr, x, y, layout);
    }

    return FALSE;
}

/**
 * gwy_ruler_new:
 *
 * Creates a new ruler.
 *
 * Returns: A new ruler.
 **/
GtkWidget*
gwy_ruler_new(void)
{
    return g_object_newv(GWY_TYPE_RULER, 0, NULL);
}

/**
 * gwy_ruler_set_show_mark:
 * @rurler: A ruler.
 * @showmark: %TRUE to show a mark at GwyRuler::mark position, %FALSE to
 *            disable it.
 *
 * Sets whether a mark should be drawn on a ruler.
 **/
void
gwy_ruler_set_show_mark(GwyRuler *ruler,
                        gboolean showmark)
{
    g_return_if_fail(GWY_IS_RULER(ruler));
    if (!set_show_mark(ruler, showmark))
        return;

    g_object_notify_by_pspec(G_OBJECT(ruler), properties[PROP_SHOW_MARK]);
}

/**
 * gwy_ruler_get_show_mark:
 * @rurler: A ruler.
 *
 * Gets whether a mark should be drawn on a ruler.
 *
 * Returns: %TRUE if a mark is drawn, %FALSE if it is not.
 **/
gboolean
gwy_ruler_get_show_mark(const GwyRuler *ruler)
{
    g_return_val_if_fail(GWY_IS_RULER(ruler), FALSE);
    return ruler->priv->show_mark;
}

/**
 * gwy_ruler_set_mark:
 * @rurler: A ruler.
 * @mark: Mark position, in real #GwyAxis coordinates.
 *
 * Sets the position of the mark drawn on a ruler.
 **/
void
gwy_ruler_set_mark(GwyRuler *ruler,
                   gdouble mark)
{
    g_return_if_fail(GWY_IS_RULER(ruler));
    if (!set_mark(ruler, mark))
        return;

    g_object_notify_by_pspec(G_OBJECT(ruler), properties[PROP_MARK]);
}

/**
 * gwy_ruler_get_mark:
 * @rurler: A ruler.
 *
 * Gets the position of the mark drawn on a ruler.
 *
 * Returns: The mark position, in real #GwyAxis coordinates.
 **/
gdouble
gwy_ruler_get_mark(const GwyRuler *ruler)
{
    g_return_val_if_fail(GWY_IS_RULER(ruler), NAN);
    return ruler->priv->mark;
}

static gboolean
set_show_mark(GwyRuler *ruler,
              gboolean setting)
{
    Ruler *priv = ruler->priv;
    setting = !!setting;
    if (setting == priv->show_mark)
        return FALSE;

    priv->show_mark = setting;
    invalidate_mark_area(ruler);
    return TRUE;
}

static gboolean
set_mark(GwyRuler *ruler,
         gdouble value)
{
    Ruler *priv = ruler->priv;
    if (value == priv->mark)
        return FALSE;

    invalidate_mark_area(ruler);
    priv->mark = value;
    invalidate_mark_area(ruler);
    return TRUE;
}

static void
draw_mark(GwyRuler *ruler, cairo_t *cr,
          const cairo_matrix_t *matrix,
          gdouble length, gdouble breadth)
{
    Ruler *priv = ruler->priv;
    if (!priv->show_mark || !isfinite(priv->mark))
        return;

    GwyAxis *axis = GWY_AXIS(ruler);
    GtkPositionType edge = gwy_axis_get_edge(axis);
    gdouble x = gwy_axis_value_to_position(axis, priv->mark),
            hs = 0.2*breadth, y = hs;
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
    cairo_set_source_rgb(cr, 0.6, 0.6, 1.0);
    cairo_fill_preserve(cr);
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_stroke(cr);
    cairo_restore(cr);
}

static void
invalidate_mark_area(GwyRuler *ruler)
{
    GtkWidget *widget = GTK_WIDGET(ruler);
    //Ruler *priv = ruler->priv;

    // TODO: Invalidate only the region around the mark.
    if (!gtk_widget_is_drawable(widget))
        return;

    gtk_widget_queue_draw(widget);
}

static gint
preferred_breadth(GtkWidget *widget)
{
    PangoLayout *layout = gwy_axis_get_pango_layout(GWY_AXIS(widget));
    g_return_val_if_fail(layout, 20);

    gint breadth;
    pango_layout_set_markup(layout, TESTMARKUP, sizeof(TESTMARKUP)-1);
    pango_layout_get_size(layout, NULL, &breadth);
    breadth = 8*breadth/(5*PANGO_SCALE);
    return MAX(breadth, 4);
}

/**
 * SECTION: ruler
 * @section_id: GwyRuler
 * @title: Ruler
 * @short_description: Ruler showing dimensions of raster data
 **/

/**
 * GwyRuler:
 *
 * Ruler widget showing dimensions of raster data.
 *
 * The #GwyRuler struct contains private data only and should be accessed
 * using the functions below.
 **/

/**
 * GwyRulerClass:
 *
 * Class of rulers showing dimensions of raster data.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
