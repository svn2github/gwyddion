/*
 *  @(#) $Id$
 *  Copyright (C) 2003,2004 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 */

#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <glib-object.h>

#include <libgwyddion/gwymacros.h>
#include "gwydataviewlayer.h"
#include "gwyvectorlayer.h"
#include "gwypixmaplayer.h"
#include "gwydataview.h"

#define GWY_DATA_VIEW_LAYER_TYPE_NAME "GwyDataViewLayer"

#define BITS_PER_SAMPLE 8

enum {
    PLUGGED,
    UNPLUGGED,
    UPDATED,
    LAST_SIGNAL
};

/* Forward declarations */

static void     gwy_data_view_layer_class_init   (GwyDataViewLayerClass *klass);
static void     gwy_data_view_layer_init         (GwyDataViewLayer *layer);
static void     gwy_data_view_layer_finalize     (GObject *object);
static void     gwy_data_view_layer_real_plugged (GwyDataViewLayer *layer);
static void     gwy_data_view_layer_real_unplugged (GwyDataViewLayer *layer);

/* Local data */

static GtkObjectClass *parent_class = NULL;

static guint data_view_layer_signals[LAST_SIGNAL] = { 0 };

GType
gwy_data_view_layer_get_type(void)
{
    static GType gwy_data_view_layer_type = 0;

    if (!gwy_data_view_layer_type) {
        static const GTypeInfo gwy_data_view_layer_info = {
            sizeof(GwyDataViewLayerClass),
            NULL,
            NULL,
            (GClassInitFunc)gwy_data_view_layer_class_init,
            NULL,
            NULL,
            sizeof(GwyDataViewLayer),
            0,
            (GInstanceInitFunc)gwy_data_view_layer_init,
            NULL,
        };
        gwy_debug("");
        gwy_data_view_layer_type
            = g_type_register_static(GTK_TYPE_OBJECT,
                                     GWY_DATA_VIEW_LAYER_TYPE_NAME,
                                     &gwy_data_view_layer_info,
                                     0);
    }

    return gwy_data_view_layer_type;
}

static void
gwy_data_view_layer_class_init(GwyDataViewLayerClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkObjectClass *object_class = GTK_OBJECT_CLASS(klass);

    gwy_debug("");

    parent_class = g_type_class_peek_parent(klass);

    gobject_class->finalize = gwy_data_view_layer_finalize;

    klass->wants_repaint = NULL;

    klass->plugged = gwy_data_view_layer_real_plugged;
    klass->unplugged = gwy_data_view_layer_real_unplugged;
    klass->updated = NULL;

/**
 * GwyDataViewLayer::plugged:
 * @gwydataviewlayer: The #GwyDataViewLayer which received the signal.
 *
 * The ::plugged signal is emitted when a #GwyDataViewLayer is plugged into
 * a #GwyDataView.
 */
    data_view_layer_signals[PLUGGED] =
        g_signal_new("plugged",
                     G_OBJECT_CLASS_TYPE(object_class),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(GwyDataViewLayerClass, plugged),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE, 0);

/**
 * GwyDataViewLayer::unplugged:
 * @gwydataviewlayer: The #GwyDataViewLayer which received the signal.
 *
 * The ::unplugged signal is emitted when a #GwyDataViewLayer is removed from
 * its #GwyDataView.
 */
    data_view_layer_signals[UNPLUGGED] =
        g_signal_new("unplugged",
                     G_OBJECT_CLASS_TYPE(object_class),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(GwyDataViewLayerClass, unplugged),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE, 0);

/**
 * GwyDataViewLayer::updated:
 * @gwydataviewlayer: The #GwyDataViewLayer which received the signal.
 *
 * The ::updated signal is emitted when a #GwyDataViewLayer is updated;
 * the exact means how a layer can be updated depends its type.
 */
    data_view_layer_signals[UPDATED] =
        g_signal_new("updated",
                     G_OBJECT_CLASS_TYPE(object_class),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(GwyDataViewLayerClass, updated),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE, 0);
}

static void
gwy_data_view_layer_init(GwyDataViewLayer *layer)
{
    gwy_debug("");

    layer->parent = NULL;
    layer->data = NULL;
}

static void
gwy_data_view_layer_finalize(GObject *object)
{
    GwyDataViewLayer *layer;

    gwy_debug("");

    g_return_if_fail(GWY_IS_DATA_VIEW_LAYER(object));

    layer = GWY_DATA_VIEW_LAYER(object);

    gwy_object_unref(layer->data);

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

/**
 * gwy_data_view_layer_wants_repaint:
 * @layer: A data view layer.
 *
 * Checks whether a layer wants repaint.
 * FIXME FIXME FIXME  This is probably flawed and will be replaced by
 * a signal.
 *
 * Returns: %TRUE if the the layer wants repaint itself, %FALSE otherwise.
 **/
gboolean
gwy_data_view_layer_wants_repaint(GwyDataViewLayer *layer)
{
    GwyDataViewLayerClass *layer_class;

    if (!layer)
        return FALSE;

    layer_class = GWY_DATA_VIEW_LAYER_GET_CLASS(layer);
    /* when a layer doesn't have wants_repaint, assume it always wants */
    if (!layer_class->wants_repaint)
        return TRUE;

    return layer_class->wants_repaint(layer);
}

/**
 * gwy_data_view_layer_plugged:
 * @layer: A data view layer.
 *
 * Emits a "plugged" singal on a layer.
 **/
void
gwy_data_view_layer_plugged(GwyDataViewLayer *layer)
{
    gwy_debug("");
    g_return_if_fail(GWY_IS_DATA_VIEW_LAYER(layer));
    g_signal_emit(layer, data_view_layer_signals[PLUGGED], 0);
}

/**
 * gwy_data_view_layer_unplugged:
 * @layer: A data view layer.
 *
 * Emits a "unplugged" singal on a layer.
 **/
void
gwy_data_view_layer_unplugged(GwyDataViewLayer *layer)
{
    gwy_debug("");
    g_return_if_fail(GWY_IS_DATA_VIEW_LAYER(layer));
    g_signal_emit(layer, data_view_layer_signals[UNPLUGGED], 0);
}

/**
 * gwy_data_view_layer_updated:
 * @layer: A data view layer.
 *
 * Emits a "updated" singal on a layer.
 **/
void
gwy_data_view_layer_updated(GwyDataViewLayer *layer)
{
    gwy_debug("");
    g_return_if_fail(GWY_IS_DATA_VIEW_LAYER(layer));
    g_signal_emit(layer, data_view_layer_signals[UPDATED], 0);
}

static void
gwy_data_view_layer_real_plugged(GwyDataViewLayer *layer)
{
    GwyContainer *data;

    gwy_debug("");

    gwy_object_unref(layer->data);

    data = gwy_data_view_get_data(GWY_DATA_VIEW(layer->parent));
    g_return_if_fail(GWY_IS_CONTAINER(data));
    g_object_ref(data);
    layer->data = data;
}

static void
gwy_data_view_layer_real_unplugged(GwyDataViewLayer *layer)
{
    gwy_debug("");

    gwy_object_unref(layer->data);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
