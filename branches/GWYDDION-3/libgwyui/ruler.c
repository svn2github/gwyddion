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
#include "libgwyui/ruler.h"

#define IGNORE_ME N_("A translatable string.")

enum {
    PROP_0,
    PROP_SHOW_MARK,
    PROP_MARK,
    N_PROPS,
};

struct _GwyRulerPrivate {
    gdouble mark;
    gboolean show_mark;

    // Scratch space
    GString *str;
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
static void     invalidate_mark_area          (GwyRuler *ruler);

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
    Ruler *priv = ruler->priv;
}

static void
gwy_ruler_finalize(GObject *object)
{
    Ruler *priv = GWY_RULER(object)->priv;
    GWY_STRING_FREE(priv->str);
    G_OBJECT_CLASS(gwy_ruler_parent_class)->finalize(object);
}

static void
gwy_ruler_dispose(GObject *object)
{
    Ruler *priv = GWY_RULER(object)->priv;
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

    if (edge == GTK_POS_TOP || edge == GTK_POS_BOTTOM) {
        *minimum = *natural = 1;
        return;
    }

    // TODO
    *minimum = *natural = 30;
}

static void
gwy_ruler_get_preferred_height(GtkWidget *widget,
                               gint *minimum,
                               gint *natural)
{
    GwyAxis *axis = GWY_AXIS(widget);
    GtkPositionType edge = gwy_axis_get_edge(axis);

    if (edge == GTK_POS_LEFT || edge == GTK_POS_RIGHT) {
        *minimum = *natural = 1;
        return;
    }

    // TODO
    *minimum = *natural = 30;
}

static gboolean
gwy_ruler_draw(GtkWidget *widget,
               cairo_t *cr)
{
    static const gdouble tick_level_sizes[4] = { 1.0, 1.0, 0.618, 0.382 };
    g_printerr("RULER DRAW %p\n", widget);

    GwyAxis *axis = GWY_AXIS(widget);
    GtkStyleContext *context = gtk_widget_get_style_context(widget);
    gdouble width = gtk_widget_get_allocated_width(widget),
            height = gtk_widget_get_allocated_height(widget);
    GtkPositionType edge = gwy_axis_get_edge(axis);

    gtk_render_background(context, cr, 0, 0, width, height);

    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_set_line_width(cr, 1.618);

    guint nticks;
    const GwyAxisTick *ticks = gwy_axis_ticks(axis, &nticks);

    if (edge == GTK_POS_TOP) {
        cairo_move_to(cr, 0, height);
        cairo_line_to(cr, width, height);

        for (guint i = 0; i < nticks; i++) {
            gdouble s = tick_level_sizes[ticks[i].level];
            cairo_move_to(cr, ticks[i].position, height);
            cairo_line_to(cr, ticks[i].position, height*(1.0 - 0.6*s));
        }
    }
    else if (edge == GTK_POS_BOTTOM) {
        cairo_move_to(cr, 0, 0);
        cairo_line_to(cr, width, 0);
    }
    else if (edge == GTK_POS_LEFT) {
        cairo_move_to(cr, width, 0);
        cairo_line_to(cr, width, height);

        for (guint i = 0; i < nticks; i++) {
            gdouble s = tick_level_sizes[ticks[i].level];
            cairo_move_to(cr, width, ticks[i].position);
            cairo_line_to(cr, width*(1.0 - 0.6*s), ticks[i].position);
        }
    }
    else {
        cairo_move_to(cr, 0, 0);
        cairo_line_to(cr, 0, height);
    }
    cairo_stroke(cr);

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
invalidate_mark_area(GwyRuler *ruler)
{
    // TODO: Invalidate only the region around the mark.
    gtk_widget_queue_draw(GTK_WIDGET(ruler));
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
