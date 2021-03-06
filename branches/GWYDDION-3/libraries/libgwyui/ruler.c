/*
 *  $Id$
 *  Copyright (C) 2012-2013 David Nečas (Yeti).
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

#define pangoscale ((gdouble)PANGO_SCALE)

struct _GwyRulerPrivate {
    gushort dummy;
};

typedef struct _GwyRulerPrivate Ruler;

static void     gwy_ruler_dispose             (GObject *object);
static void     gwy_ruler_finalize            (GObject *object);
static void     gwy_ruler_get_preferred_width (GtkWidget *widget,
                                               gint *minimum,
                                               gint *natural);
static void     gwy_ruler_get_preferred_height(GtkWidget *widget,
                                               gint *minimum,
                                               gint *natural);
static gboolean gwy_ruler_draw                (GtkWidget *widget,
                                               cairo_t *cr);
static void     gwy_ruler_redraw_mark         (GwyAxis *axis);
static void     draw_ticks                    (GwyAxis *axis,
                                               cairo_t *cr,
                                               const cairo_matrix_t *matrix,
                                               gdouble length,
                                               gdouble breadth);
static void     draw_labels                   (GwyAxis *axis,
                                               cairo_t *cr,
                                               const cairo_matrix_t *matrix,
                                               gdouble length,
                                               gdouble breadth);
static void     draw_mark                     (GwyAxis *axis,
                                               cairo_t *cr,
                                               const cairo_matrix_t *matrix,
                                               gdouble length,
                                               gdouble breadth);
static gint     preferred_breadth             (GtkWidget *widget);
static void     draw_line_transformed         (cairo_t *cr,
                                               const cairo_matrix_t *matrix,
                                               gdouble xf,
                                               gdouble yf,
                                               gdouble xt,
                                               gdouble yt);
static void     calculate_scaling             (GwyAxis *axis,
                                               cairo_rectangle_int_t *allocation,
                                               gdouble *length,
                                               gdouble *breadth,
                                               cairo_matrix_t *matrix);

static const gdouble tick_level_sizes[4] = { 1.0, 0.9, 0.45, 0.25 };

G_DEFINE_TYPE(GwyRuler, gwy_ruler, GWY_TYPE_AXIS);

static void
gwy_ruler_class_init(GwyRulerClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
    GwyAxisClass *axis_class = GWY_AXIS_CLASS(klass);

    g_type_class_add_private(klass, sizeof(Ruler));

    gobject_class->dispose = gwy_ruler_dispose;
    gobject_class->finalize = gwy_ruler_finalize;

    widget_class->get_preferred_width = gwy_ruler_get_preferred_width;
    widget_class->get_preferred_height = gwy_ruler_get_preferred_height;
    widget_class->draw = gwy_ruler_draw;

    axis_class->redraw_mark = gwy_ruler_redraw_mark;
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

static gboolean
gwy_ruler_draw(GtkWidget *widget,
               cairo_t *cr)
{
    GwyAxis *axis = GWY_AXIS(widget);
    GtkStyleContext *context = gtk_widget_get_style_context(widget);
    cairo_rectangle_int_t allocation;
    gdouble length, breadth;
    cairo_matrix_t matrix;
    calculate_scaling(axis, &allocation, &length, &breadth, &matrix);

    GdkRGBA rgba;
    gtk_render_background(context, cr,
                          0, 0, allocation.width, allocation.height);
    gtk_style_context_get_color(context, GTK_STATE_NORMAL, &rgba);
    gdk_cairo_set_source_rgba(cr, &rgba);
    cairo_set_line_width(cr, 1.0);

    draw_mark(axis, cr, &matrix, length, breadth);
    draw_ticks(axis, cr, &matrix, length, breadth);
    draw_labels(axis, cr, &matrix, length, breadth);

    return FALSE;
}

static void
gwy_ruler_redraw_mark(GwyAxis *axis)
{
    GtkWidget *widget = GTK_WIDGET(axis);
    gdouble mark = gwy_axis_get_mark(axis);
    if (!isfinite(mark))
        return;

    cairo_rectangle_int_t allocation;
    gdouble length, breadth;
    cairo_matrix_t matrix;
    calculate_scaling(axis, &allocation, &length, &breadth, &matrix);

    gdouble x = gwy_axis_value_to_position(axis, mark),
            hs = 0.2*breadth, y = hs;
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

static void
draw_ticks(GwyAxis *axis, cairo_t *cr,
           const cairo_matrix_t *matrix,
           gdouble length, gdouble breadth)
{
    guint nticks;
    const GwyAxisTick *ticks = gwy_axis_ticks(axis, &nticks);

    draw_line_transformed(cr, matrix, 0, 0.5, length, 0.5);
    for (guint i = 0; i < nticks; i++) {
        gdouble pos = ticks[i].position;
        gdouble s = tick_level_sizes[ticks[i].level];
        draw_line_transformed(cr, matrix, pos, 0, pos, s*breadth);
    }
    cairo_stroke(cr);
}

static void
draw_labels(GwyAxis *axis, cairo_t *cr,
            const cairo_matrix_t *matrix,
            G_GNUC_UNUSED gdouble length, gdouble breadth)
{
    if (!gwy_axis_get_show_tick_labels(axis))
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

    GtkPositionType edge = gwy_axis_get_edge(axis);
    GtkStyleContext *context = gtk_widget_get_style_context(GTK_WIDGET(axis));
    PangoLayout *layout = gwy_axis_get_pango_layout(axis);
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
            y += 2.0;
            x += a + d;
        }
        else if (edge == GTK_POS_TOP) {
            if (i == nticks-1 && ticks[i].level == GWY_AXIS_TICK_EDGE)
                x -= 2.0 + ticks[i].extents.width/pangoscale;
            else
                x += 2.0;
            y = a;
        }
        else if (edge == GTK_POS_RIGHT) {
            y += 2.0 + PANGO_RBEARING(ticks[i].extents)/pangoscale;
            x = breadth - (a + d);
        }
        gtk_render_layout(context, cr, x, y, layout);
    }
}

static void
draw_mark(GwyAxis *axis, cairo_t *cr,
          const cairo_matrix_t *matrix,
          gdouble length, gdouble breadth)
{
    gdouble mark = gwy_axis_get_mark(axis);
    if (!gwy_axis_get_show_mark(axis) || !isfinite(mark))
        return;

    GtkPositionType edge = gwy_axis_get_edge(axis);
    guint nticks;
    gwy_axis_ticks(axis, &nticks);
    gdouble x = gwy_axis_value_to_position(axis, mark),
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

static gint
preferred_breadth(GtkWidget *widget)
{
    GwyAxis *axis = GWY_AXIS(widget);
    PangoLayout *layout = gwy_axis_get_pango_layout(axis);
    g_return_val_if_fail(layout, 20);

    // We fit the labels between ticks so don't depend on "show-labels".
    gint breadth;
    pango_layout_set_markup(layout, TESTMARKUP, sizeof(TESTMARKUP)-1);
    pango_layout_get_size(layout, NULL, &breadth);
    breadth = 8*breadth/(5*PANGO_SCALE);
    return MAX(breadth, 4);
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
        cairo_matrix_init(matrix, 1.0, 0.0, 0.0, -1.0, 0.0, allocation->height);
    else if (edge == GTK_POS_LEFT)
        cairo_matrix_init(matrix, 0.0, 1.0, -1.0, 0.0, allocation->width, 0.0);
    else if (edge == GTK_POS_BOTTOM)
        cairo_matrix_init_identity(matrix);
    else if (edge == GTK_POS_RIGHT)
        cairo_matrix_init(matrix, 0.0, 1.0, 1.0, 0.0, 0.0, 0.0);
    else {
        g_assert_not_reached();
    }
}

/**
 * SECTION: ruler
 * @title: GwyRuler
 * @short_description: Ruler showing dimensions of raster data
 * @image: GwyRuler.png
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
