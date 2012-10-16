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
#include "libgwy/object-utils.h"
#include "libgwyui/axis.h"

#define IGNORE_ME N_("A translatable string.")

enum {
    PROP_0,
    PROP_UNIT,
    PROP_EDGE,
    PROP_SNAP_TO_TICKS,
    PROP_SHOW_LABELS,
    PROP_SHOW_UNITS,
    PROP_DRAW_MINOR,
    N_PROPS,
};

struct _GwyAxisPrivate {
    GwyUnit *unit;
    gulong unit_changed_id;

    GtkPositionType edge;
    gboolean snap_to_ticks : 1;
    gboolean show_labels : 1;
    gboolean show_units : 1;
    gboolean draw_minor : 1;
};

typedef struct _GwyAxisPrivate Axis;

static void     gwy_axis_dispose          (GObject *object);
static void     gwy_axis_finalize         (GObject *object);
static void     gwy_axis_set_property     (GObject *object,
                                           guint prop_id,
                                           const GValue *value,
                                           GParamSpec *pspec);
static void     gwy_axis_get_property     (GObject *object,
                                           guint prop_id,
                                           GValue *value,
                                           GParamSpec *pspec);
static gboolean set_snap_to_ticks         (GwyAxis *axis,
                                           gboolean setting);
static gboolean set_show_labels           (GwyAxis *axis,
                                           gboolean setting);
static gboolean set_show_units            (GwyAxis *axis,
                                           gboolean setting);
static gboolean set_draw_minor            (GwyAxis *axis,
                                           gboolean setting);
static gboolean set_edge                  (GwyAxis *axis,
                                           GtkPositionType edge);
static void     unit_changed              (GwyAxis *axis,
                                           GwyUnit *unit);
static void     position_set_style_classes(GtkWidget *widget,
                                           GtkPositionType position);

static GParamSpec *properties[N_PROPS];

G_DEFINE_ABSTRACT_TYPE(GwyAxis, gwy_axis, GTK_TYPE_WIDGET);

static void
gwy_axis_class_init(GwyAxisClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    //GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

    g_type_class_add_private(klass, sizeof(Axis));

    gobject_class->dispose = gwy_axis_dispose;
    gobject_class->finalize = gwy_axis_finalize;
    gobject_class->get_property = gwy_axis_get_property;
    gobject_class->set_property = gwy_axis_set_property;

    properties[PROP_UNIT]
         = g_param_spec_object("unit",
                               "Unit",
                               "Units of the quantity shown on the axis.",
                               GWY_TYPE_UNIT,
                               G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    properties[PROP_EDGE]
         = g_param_spec_enum("edge",
                             "Edge",
                             "Edge on which this axis is placed with "
                             "respect to the data widget.  Ticks are "
                             "drawn on the opposite edge of the axis "
                             "so that they are adjacent to the data widget.",
                             GTK_TYPE_POSITION_TYPE,
                             GTK_POS_BOTTOM,
                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_SNAP_TO_TICKS]
        = g_param_spec_boolean("snap-to-ticks",
                               "Snap to ticks",
                               "Whether the range should start and end at "
                               "a major tick.",
                               TRUE,
                               G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_SHOW_LABELS]
        = g_param_spec_boolean("show-labels",
                               "Show labels",
                               "Whether labels should be shown at major ticks.",
                               TRUE,
                               G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_SHOW_UNITS]
        = g_param_spec_boolean("show-units",
                               "Show units",
                               "Whether units should be shown next to a major "
                               "tick.",
                               TRUE,
                               G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_DRAW_MINOR]
        = g_param_spec_boolean("draw-minor",
                               "Draw-minor",
                               "Whether minor ticks should be drawn.",
                               TRUE,
                               G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    for (guint i = 1; i < N_PROPS; i++)
        g_object_class_install_property(gobject_class, i, properties[i]);
}

static void
gwy_axis_init(GwyAxis *axis)
{
    axis->priv = G_TYPE_INSTANCE_GET_PRIVATE(axis, GWY_TYPE_AXIS, Axis);
    Axis *priv = axis->priv;

    priv->unit = gwy_unit_new();
    priv->unit_changed_id = g_signal_connect_swapped(priv->unit, "changed",
                                                     G_CALLBACK(unit_changed),
                                                     axis);
    priv->snap_to_ticks = TRUE;
    priv->show_labels = TRUE;
    priv->show_units = TRUE;
    priv->draw_minor = TRUE;
    priv->edge = GTK_POS_BOTTOM;
}

static void
gwy_axis_finalize(GObject *object)
{
    Axis *priv = GWY_AXIS(object)->priv;
    g_signal_handler_disconnect(priv->unit, priv->unit_changed_id);
    g_object_unref(priv->unit);
    G_OBJECT_CLASS(gwy_axis_parent_class)->finalize(object);
}

static void
gwy_axis_dispose(GObject *object)
{
    G_OBJECT_CLASS(gwy_axis_parent_class)->dispose(object);
}

static void
gwy_axis_set_property(GObject *object,
                      guint prop_id,
                      const GValue *value,
                      GParamSpec *pspec)
{
    GwyAxis *axis = GWY_AXIS(object);

    switch (prop_id) {
        case PROP_EDGE:
        set_edge(axis, g_value_get_enum(value));
        break;

        case PROP_SNAP_TO_TICKS:
        set_snap_to_ticks(axis, g_value_get_boolean(value));
        break;

        case PROP_SHOW_LABELS:
        set_show_labels(axis, g_value_get_boolean(value));
        break;

        case PROP_SHOW_UNITS:
        set_show_units(axis, g_value_get_boolean(value));
        break;

        case PROP_DRAW_MINOR:
        set_draw_minor(axis, g_value_get_boolean(value));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_axis_get_property(GObject *object,
                      guint prop_id,
                      GValue *value,
                      GParamSpec *pspec)
{
    Axis *priv = GWY_AXIS(object)->priv;

    switch (prop_id) {
        case PROP_UNIT:
        g_value_set_object(value, priv->unit);
        break;

        case PROP_EDGE:
        g_value_set_enum(value, priv->edge);
        break;

        case PROP_SNAP_TO_TICKS:
        g_value_set_boolean(value, priv->snap_to_ticks);
        break;

        case PROP_SHOW_LABELS:
        g_value_set_boolean(value, priv->show_labels);
        break;

        case PROP_SHOW_UNITS:
        g_value_set_boolean(value, priv->show_units);
        break;

        case PROP_DRAW_MINOR:
        g_value_set_boolean(value, priv->draw_minor);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static gboolean
set_edge(GwyAxis *axis,
         GtkPositionType edge)
{
    Axis *priv = axis->priv;
    if (edge == priv->edge)
        return FALSE;

    priv->edge = edge;
    gtk_widget_queue_resize(GTK_WIDGET(axis));
    position_set_style_classes(GTK_WIDGET(axis), edge);
    return TRUE;
}

static gboolean
set_snap_to_ticks(GwyAxis *axis,
                  gboolean setting)
{
    Axis *priv = axis->priv;
    setting = !!setting;
    if (setting == priv->snap_to_ticks)
        return FALSE;

    priv->snap_to_ticks = setting;
    gtk_widget_queue_draw(GTK_WIDGET(axis));
    return TRUE;
}

static gboolean
set_show_labels(GwyAxis *axis,
                gboolean setting)
{
    Axis *priv = axis->priv;
    setting = !!setting;
    if (setting == priv->show_labels)
        return FALSE;

    priv->show_labels = setting;
    gtk_widget_queue_draw(GTK_WIDGET(axis));
    return TRUE;
}

static gboolean
set_show_units(GwyAxis *axis,
               gboolean setting)
{
    Axis *priv = axis->priv;
    setting = !!setting;
    if (setting == priv->show_units)
        return FALSE;

    priv->show_units = setting;
    gtk_widget_queue_draw(GTK_WIDGET(axis));
    return TRUE;
}

static gboolean
set_draw_minor(GwyAxis *axis,
               gboolean setting)
{
    Axis *priv = axis->priv;
    setting = !!setting;
    if (setting == priv->draw_minor)
        return FALSE;

    priv->draw_minor = setting;
    gtk_widget_queue_draw(GTK_WIDGET(axis));
    return TRUE;
}

static void
unit_changed(GwyAxis *axis,
             G_GNUC_UNUSED GwyUnit *unit)
{
    Axis *priv = axis->priv;
    if (!priv->show_units)
        return;

    gtk_widget_queue_draw(GTK_WIDGET(axis));
}

// FIXME: This should be called when the widget is initially created.
// FIXME: This should go to general widget utils.
static void
position_set_style_classes(GtkWidget *widget,
                           GtkPositionType position)
{
    g_return_if_fail(GTK_IS_WIDGET(widget));

    GtkStyleContext *context = gtk_widget_get_style_context(widget);
    if (position == GTK_POS_TOP || position == GTK_POS_BOTTOM) {
        gtk_style_context_add_class(context, GTK_STYLE_CLASS_HORIZONTAL);
        gtk_style_context_remove_class(context, GTK_STYLE_CLASS_VERTICAL);
    }
    else if (position == GTK_POS_LEFT || position == GTK_POS_RIGHT) {
        gtk_style_context_add_class(context, GTK_STYLE_CLASS_VERTICAL);
        gtk_style_context_remove_class(context, GTK_STYLE_CLASS_HORIZONTAL);
    }
    else {
        g_assert_not_reached();
    }
}

// Tick algorithm.
// (1) Estimate minimum distance required between major ticks.  This is a
//     handful of pixels for no-labels case; approx. the dimension of the
//     label when tick labels are requested.
// (2) Estimate the major step.
// (3) If we cannot place even two major ticks, enter a fallback mode and just
//     try to draw something (possibly disabling labels even if they are
//     requested).  End here.
// (4) Calculate the label precision necessary for given major step.  Can we
//     do this exactly?
// (5) Try to actually format ticks to see whether they fit. This includes
//     sufficient spacing of minor ticks (if requested).
// (6) Ticks fit: Remember this base as LAST_GOOD and decrease the major step
//     to the next smaller, go to (4).
// (7) Ticks do not fit: If we have LAST_GOOD use that.  Otherwise increase the
//     majot step to the next larger.  Go to (3).
// (8) Take minor step as the next smaller for obtained major step.

/**
 * SECTION: axis
 * @section_id: GwyAxis
 * @title: Axis
 * @short_description: Base class for widgets displaying axes
 **/

/**
 * GwyAxis:
 *
 * Abstraction of widgets displaying graph and data view axes.
 *
 * The #GwyAxis struct contains private data only and should be accessed
 * using the functions below.
 **/

/**
 * GwyAxisClass:
 *
 * Class of graphs and data view axes.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
