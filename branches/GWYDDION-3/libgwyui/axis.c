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
    PROP_SNAP_TO_TICKS,
    N_PROPS,
};

struct _GwyAxisPrivate {
    gboolean snap_to_ticks;
};

typedef struct _GwyAxisPrivate Axis;

static void     gwy_axis_dispose     (GObject *object);
static void     gwy_axis_finalize    (GObject *object);
static void     gwy_axis_set_property(GObject *object,
                                      guint prop_id,
                                      const GValue *value,
                                      GParamSpec *pspec);
static void     gwy_axis_get_property(GObject *object,
                                      guint prop_id,
                                      GValue *value,
                                      GParamSpec *pspec);
static gboolean set_snap_to_ticks    (GwyAxis *axis,
                                      gboolean setting);

static GParamSpec *properties[N_PROPS];

G_DEFINE_ABSTRACT_TYPE_WITH_CODE(GwyAxis, gwy_axis, GTK_TYPE_WIDGET,
                                 G_IMPLEMENT_INTERFACE(GTK_TYPE_ORIENTABLE,
                                                       NULL));

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

    properties[PROP_SNAP_TO_TICKS]
        = g_param_spec_boolean("snap-to-ticks",
                               "Snap to ticks",
                               "Whether the range should start and end at "
                               "a tick.",
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
    priv->snap_to_ticks = TRUE;
}

static void
gwy_axis_finalize(GObject *object)
{
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
        case PROP_SNAP_TO_TICKS:
        set_snap_to_ticks(axis, g_value_get_boolean(value));
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
        case PROP_SNAP_TO_TICKS:
        g_value_set_boolean(value, priv->snap_to_ticks);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
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
