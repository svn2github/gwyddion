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
#include "libgwy/object-utils.h"
#include "libgwyui/scroller.h"

enum {
    PROP_0,
    PROP_HADJUSTMENT,
    PROP_VADJUSTMENT,
    N_PROPS,
};

struct _GwyScrollerPrivate {
    GtkAdjustment *hadjustment;
    GtkAdjustment *vadjustment;
};

typedef struct _GwyScrollerPrivate Scroller;

static void     gwy_scroller_dispose             (GObject *object);
static void     gwy_scroller_finalize            (GObject *object);
static void     gwy_scroller_set_property        (GObject *object,
                                                  guint prop_id,
                                                  const GValue *value,
                                                  GParamSpec *pspec);
static void     gwy_scroller_get_property        (GObject *object,
                                                  guint prop_id,
                                                  GValue *value,
                                                  GParamSpec *pspec);
static void     gwy_scroller_get_preferred_width (GtkWidget *widget,
                                                  gint *minimum,
                                                  gint *natural);
static void     gwy_scroller_get_preferred_height(GtkWidget *widget,
                                                  gint *minimum,
                                                  gint *natural);
static gboolean set_hadjustment                  (GwyScroller *scroller,
                                                  GtkAdjustment *adjustment);
static gboolean set_vadjustment                  (GwyScroller *scroller,
                                                  GtkAdjustment *adjustment);

static GParamSpec *properties[N_PROPS];

G_DEFINE_TYPE(GwyScroller, gwy_scroller, GTK_TYPE_BIN);

static void
gwy_scroller_class_init(GwyScrollerClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

    g_type_class_add_private(klass, sizeof(Scroller));

    gobject_class->dispose = gwy_scroller_dispose;
    gobject_class->finalize = gwy_scroller_finalize;
    gobject_class->get_property = gwy_scroller_get_property;
    gobject_class->set_property = gwy_scroller_set_property;

    widget_class->get_preferred_width = gwy_scroller_get_preferred_width;
    widget_class->get_preferred_height = gwy_scroller_get_preferred_height;

    properties[PROP_HADJUSTMENT]
        = g_param_spec_object("hadjustment",
                              "Horizontal adjustment",
                              "Adjustment for the horizontal position",
                              GTK_TYPE_ADJUSTMENT,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_VADJUSTMENT]
        = g_param_spec_object("vadjustment",
                              "Vertical adjustment",
                              "Adjustment for the vertical position",
                              GTK_TYPE_ADJUSTMENT,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    for (guint i = 1; i < N_PROPS; i++)
        g_object_class_install_property(gobject_class, i, properties[i]);
}

static void
gwy_scroller_init(GwyScroller *scroller)
{
    scroller->priv = G_TYPE_INSTANCE_GET_PRIVATE(scroller,
                                                 GWY_TYPE_SCROLLER, Scroller);
    Scroller *priv = scroller->priv;
    priv->hadjustment = g_object_newv(GTK_TYPE_ADJUSTMENT, 0, NULL);
    priv->vadjustment = g_object_newv(GTK_TYPE_ADJUSTMENT, 0, NULL);
}

static void
gwy_scroller_finalize(GObject *object)
{
    G_OBJECT_CLASS(gwy_scroller_parent_class)->finalize(object);
}

static void
gwy_scroller_dispose(GObject *object)
{
    GwyScroller *scroller = GWY_SCROLLER(object);
    set_hadjustment(scroller, NULL);
    set_vadjustment(scroller, NULL);
    G_OBJECT_CLASS(gwy_scroller_parent_class)->dispose(object);
}

static void
gwy_scroller_set_property(GObject *object,
                          guint prop_id,
                          const GValue *value,
                          GParamSpec *pspec)
{
    GwyScroller *scroller = GWY_SCROLLER(object);

    switch (prop_id) {
        case PROP_HADJUSTMENT:
        set_hadjustment(scroller, g_value_get_object(value));
        break;

        case PROP_VADJUSTMENT:
        set_vadjustment(scroller, g_value_get_object(value));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_scroller_get_property(GObject *object,
                          guint prop_id,
                          GValue *value,
                          GParamSpec *pspec)
{
    Scroller *priv = GWY_SCROLLER(object)->priv;

    switch (prop_id) {
        case PROP_HADJUSTMENT:
        g_value_set_object(value, priv->hadjustment);
        break;

        case PROP_VADJUSTMENT:
        g_value_set_object(value, priv->vadjustment);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_scroller_get_preferred_width(G_GNUC_UNUSED GtkWidget *widget,
                                 gint *minimum,
                                 gint *natural)
{
    *minimum = *natural = 1;
}

static void
gwy_scroller_get_preferred_height(G_GNUC_UNUSED GtkWidget *widget,
                                  gint *minimum,
                                  gint *natural)
{
    *minimum = *natural = 1;
}

/**
 * gwy_scroller_new:
 *
 * Creates a new scroller.
 *
 * Returns: A new scroller.
 **/
GtkWidget*
gwy_scroller_new(void)
{
    return g_object_newv(GWY_TYPE_SCROLLER, 0, NULL);
}

/**
 * gwy_scroller_set_hadjustment:
 * @scroller: A scroller.
 * @adjustment: Adjustment to use for the horizontal position.
 *
 * Sets the adjustment a scroller will use for the horizontal position.
 **/
void
gwy_scroller_set_hadjustment(GwyScroller *scroller,
                             GtkAdjustment *adjustment)
{
    g_return_if_fail(GWY_IS_SCROLLER(scroller));
    g_return_if_fail(!adjustment || GTK_IS_ADJUSTMENT(adjustment));
    if (!adjustment)
        adjustment = g_object_newv(GTK_TYPE_ADJUSTMENT, 0, NULL);
    if (!set_hadjustment(scroller, adjustment))
        return;

    g_object_notify_by_pspec(G_OBJECT(scroller), properties[PROP_HADJUSTMENT]);
}

/**
 * gwy_scroller_get_hadjustment:
 * @scroller: A scroller.
 *
 * Gets the adjustment a scroller uses for the horizontal position.
 *
 * Returns: (transfer none):
 *          The horizontal adjustment.
 **/
GtkAdjustment*
gwy_scroller_get_hadjustment(const GwyScroller *scroller)
{
    g_return_val_if_fail(GWY_IS_SCROLLER(scroller), NULL);
    return scroller->priv->hadjustment;
}

/**
 * gwy_scroller_set_vadjustment:
 * @scroller: A scroller.
 * @adjustment: Adjustment to use for the vertical position.
 *
 * Sets the adjustment a scroller will use for the vertical position.
 **/
void
gwy_scroller_set_vadjustment(GwyScroller *scroller,
                             GtkAdjustment *adjustment)
{
    g_return_if_fail(GWY_IS_SCROLLER(scroller));
    g_return_if_fail(!adjustment || GTK_IS_ADJUSTMENT(adjustment));
    if (!adjustment)
        adjustment = g_object_newv(GTK_TYPE_ADJUSTMENT, 0, NULL);
    if (!set_vadjustment(scroller, adjustment))
        return;

    g_object_notify_by_pspec(G_OBJECT(scroller), properties[PROP_VADJUSTMENT]);
}

/**
 * gwy_scroller_get_vadjustment:
 * @scroller: A scroller.
 *
 * Gets the adjustment a scroller uses for the vertical position.
 *
 * Returns: (transfer none):
 *          The horizontal adjustment.
 **/
GtkAdjustment*
gwy_scroller_get_vadjustment(const GwyScroller *scroller)
{
    g_return_val_if_fail(GWY_IS_SCROLLER(scroller), NULL);
    return scroller->priv->vadjustment;
}

static gboolean
set_hadjustment(GwyScroller *scroller,
                GtkAdjustment *adjustment)
{
    Scroller *priv = scroller->priv;
    if (!gwy_set_member_object(scroller, adjustment, GTK_TYPE_ADJUSTMENT,
                               &priv->hadjustment,
                               NULL))
        return FALSE;

    return TRUE;
}

static gboolean
set_vadjustment(GwyScroller *scroller,
                GtkAdjustment *adjustment)
{
    Scroller *priv = scroller->priv;
    if (!gwy_set_member_object(scroller, adjustment, GTK_TYPE_ADJUSTMENT,
                               &priv->vadjustment,
                               NULL))
        return FALSE;

    return TRUE;
}

/**
 * SECTION: scroller
 * @section_id: GwyScroller
 * @title: Scrollable area
 * @short_description: Simple scrollable area without any scrollbars
 *
 * #GwyScroller is somewhat similar to #GtkScrolledWindow.  However, it does
 * not show any scrollbars inside; the entire widget area is occupied by the
 * child.  The adjustments can be used to add scrollbars and other
 * #GtkRange<!-- -->-like widgets elsewhere, for instance on the outside.
 **/

/**
 * GwyScroller:
 *
 * Simple scrollable area widget without any toolbars.
 *
 * The #GwyScroller struct contains private data only and should be accessed
 * using the functions below.
 **/

/**
 * GwyScrollerClass:
 *
 * Class of simple scrollable areas without any toolbars.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
