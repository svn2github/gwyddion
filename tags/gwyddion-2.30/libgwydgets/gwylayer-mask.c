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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "config.h"
#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libdraw/gwypixfield.h>
#include <libgwydgets/gwylayer-mask.h>

#define connect_swapped_after(obj, signal, cb, data) \
    g_signal_connect_object(obj, signal, G_CALLBACK(cb), data, \
                            G_CONNECT_SWAPPED | G_CONNECT_AFTER)

enum {
    PROP_0,
    PROP_COLOR_KEY
};

static void       gwy_layer_mask_set_property    (GObject *object,
                                                  guint prop_id,
                                                  const GValue *value,
                                                  GParamSpec *pspec);
static void       gwy_layer_mask_get_property    (GObject *object,
                                                  guint prop_id,
                                                  GValue *value,
                                                  GParamSpec *pspec);
static GdkPixbuf* gwy_layer_mask_paint           (GwyPixmapLayer *layer);
static void       gwy_layer_mask_plugged         (GwyDataViewLayer *layer);
static void       gwy_layer_mask_unplugged       (GwyDataViewLayer *layer);
static void       gwy_layer_mask_connect_color   (GwyLayerMask *mask_layer);
static void       gwy_layer_mask_disconnect_color(GwyLayerMask *mask_layer);
static void       gwy_layer_mask_changed         (GwyPixmapLayer *pixmap_layer);

G_DEFINE_TYPE(GwyLayerMask, gwy_layer_mask, GWY_TYPE_PIXMAP_LAYER)

static void
gwy_layer_mask_class_init(GwyLayerMaskClass *klass)
{
    GwyDataViewLayerClass *layer_class = GWY_DATA_VIEW_LAYER_CLASS(klass);
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GwyPixmapLayerClass *pixmap_class = GWY_PIXMAP_LAYER_CLASS(klass);

    gobject_class->set_property = gwy_layer_mask_set_property;
    gobject_class->get_property = gwy_layer_mask_get_property;

    layer_class->plugged = gwy_layer_mask_plugged;
    layer_class->unplugged = gwy_layer_mask_unplugged;

    pixmap_class->paint = gwy_layer_mask_paint;

    /**
     * GwyLayerMask:color-key:
     *
     * The :color-key property is the container key used to identify mask color
     * in container.
     **/
    g_object_class_install_property
        (gobject_class,
         PROP_COLOR_KEY,
         g_param_spec_string("color-key",
                             "Color key",
                             "Key prefix identifying mask color in container",
                             NULL, G_PARAM_READWRITE));
}

static void
gwy_layer_mask_init(G_GNUC_UNUSED GwyLayerMask *layer)
{
}

static void
gwy_layer_mask_set_property(GObject *object,
                            guint prop_id,
                            const GValue *value,
                            GParamSpec *pspec)
{
    GwyLayerMask *layer_mask = GWY_LAYER_MASK(object);

    switch (prop_id) {
        case PROP_COLOR_KEY:
        gwy_layer_mask_set_color_key(layer_mask, g_value_get_string(value));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_layer_mask_get_property(GObject*object,
                            guint prop_id,
                            GValue *value,
                            GParamSpec *pspec)
{
    GwyLayerMask *layer_mask = GWY_LAYER_MASK(object);

    switch (prop_id) {
        case PROP_COLOR_KEY:
        g_value_set_static_string(value,
                                  g_quark_to_string(layer_mask->color_key));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

/**
 * gwy_layer_mask_new:
 *
 * Creates a new mask layer.
 *
 * By default, is uses a transparent color (thus not displaying anything).
 *
 * Returns: The newly created layer.
 **/
GwyPixmapLayer*
gwy_layer_mask_new(void)
{
    GwyLayerMask *layer;

    layer = g_object_new(GWY_TYPE_LAYER_MASK, NULL);

    return (GwyPixmapLayer*)layer;
}

static GdkPixbuf*
gwy_layer_mask_paint(GwyPixmapLayer *layer)
{
    GwyDataField *data_field;
    GwyLayerMask *mask_layer;
    GwyContainer *data;
    GwyRGBA color = { 0, 0, 0, 0 };

    mask_layer = GWY_LAYER_MASK(layer);
    data = GWY_DATA_VIEW_LAYER(layer)->data;

    data_field = GWY_DATA_FIELD(layer->data_field);
    g_return_val_if_fail(data && data_field, NULL);
    if (mask_layer->color_key)
        gwy_rgba_get_from_container(&color,
                                    GWY_DATA_VIEW_LAYER(mask_layer)->data,
                                    g_quark_to_string(mask_layer->color_key));
    gwy_pixmap_layer_make_pixbuf(layer, TRUE);
    gwy_pixbuf_draw_data_field_as_mask(layer->pixbuf, data_field, &color);

    return layer->pixbuf;
}

/**
 * gwy_layer_mask_get_color:
 * @mask_layer: A mask layer.
 *
 * Returns the color used by a mask layer.
 *
 * Returns: The color as #GwyRGBA.
 **/
GwyRGBA
gwy_layer_mask_get_color(GwyLayerMask *mask_layer)
{
    GwyContainer *data;
    GwyRGBA color = { 0, 0, 0, 0 };

    g_return_val_if_fail(GWY_IS_LAYER_MASK(mask_layer), color);
    data = GWY_DATA_VIEW_LAYER(mask_layer)->data;
    g_return_val_if_fail(data, color);

    if (!mask_layer->color_key)
        return color;

    gwy_rgba_get_from_container(&color, GWY_DATA_VIEW_LAYER(mask_layer)->data,
                                g_quark_to_string(mask_layer->color_key));

    return color;
}

static void
gwy_layer_mask_plugged(GwyDataViewLayer *layer)
{
    GwyLayerMask *mask_layer;

    mask_layer = GWY_LAYER_MASK(layer);

    GWY_DATA_VIEW_LAYER_CLASS(gwy_layer_mask_parent_class)->plugged(layer);

    gwy_layer_mask_connect_color(mask_layer);
}

static void
gwy_layer_mask_unplugged(GwyDataViewLayer *layer)
{
    GwyPixmapLayer *pixmap_layer;
    GwyLayerMask *mask_layer;

    pixmap_layer = GWY_PIXMAP_LAYER(layer);
    mask_layer = GWY_LAYER_MASK(layer);

    gwy_layer_mask_disconnect_color(mask_layer);

    gwy_object_unref(pixmap_layer->pixbuf);
    GWY_DATA_VIEW_LAYER_CLASS(gwy_layer_mask_parent_class)->unplugged(layer);
}

/**
 * gwy_layer_mask_set_color_key:
 * @mask_layer: A mask layer.
 * @prefix: Prefix of keys identifying mask color components, "/red", "/green",
 *          "/blue", and "/alpha" is appended to it to get the individual
 *          keys.
 *
 * Sets the container key of colour components of a mask layer.
 **/
void
gwy_layer_mask_set_color_key(GwyLayerMask *mask_layer,
                             const gchar *prefix)
{
    GQuark quark;

    g_return_if_fail(GWY_IS_LAYER_MASK(mask_layer));

    quark = prefix ? g_quark_try_string(prefix) : 0;
    if (quark == mask_layer->color_key)
        return;

    gwy_layer_mask_disconnect_color(mask_layer);
    mask_layer->color_key = quark;
    gwy_layer_mask_connect_color(mask_layer);

    gwy_layer_mask_changed(GWY_PIXMAP_LAYER(mask_layer));
}

/**
 * gwy_layer_mask_get_color_key:
 * @mask_layer: A mask layer.
 *
 * Gets prefix identifying color components.
 *
 * Returns: The prefix, or %NULL if it isn't set.
 **/
const gchar*
gwy_layer_mask_get_color_key(GwyLayerMask *mask_layer)
{
    g_return_val_if_fail(GWY_IS_LAYER_MASK(mask_layer), NULL);
    return g_quark_to_string(mask_layer->color_key);
}

static void
gwy_layer_mask_connect_color(GwyLayerMask *mask_layer)
{
    GwyDataViewLayer *layer;
    const gchar *prefix;
    gchar *detailed_signal;
    guint len;

    layer = GWY_DATA_VIEW_LAYER(mask_layer);
    if (!layer->data || !mask_layer->color_key)
        return;

    prefix = g_quark_to_string(mask_layer->color_key);
    len = strlen(prefix);
    detailed_signal = g_newa(gchar, len + sizeof("item-changed::")
                                    + sizeof("/alpha"));
    len += sizeof("item-changed::");

    g_stpcpy(g_stpcpy(g_stpcpy(detailed_signal, "item-changed::"), prefix),
             "/red");
    mask_layer->red_id = connect_swapped_after(layer->data, detailed_signal,
                                               gwy_layer_mask_changed, layer);

    strcpy(detailed_signal + len, "green");
    mask_layer->green_id = connect_swapped_after(layer->data, detailed_signal,
                                                 gwy_layer_mask_changed, layer);

    strcpy(detailed_signal + len, "blue");
    mask_layer->blue_id = connect_swapped_after(layer->data, detailed_signal,
                                                gwy_layer_mask_changed, layer);

    strcpy(detailed_signal + len, "alpha");
    mask_layer->alpha_id = connect_swapped_after(layer->data, detailed_signal,
                                                 gwy_layer_mask_changed, layer);
}

static void
gwy_layer_mask_disconnect_color(GwyLayerMask *mask_layer)
{
    GwyDataViewLayer *layer;

    layer = GWY_DATA_VIEW_LAYER(mask_layer);

    gwy_signal_handler_disconnect(layer->data, mask_layer->red_id);
    gwy_signal_handler_disconnect(layer->data, mask_layer->green_id);
    gwy_signal_handler_disconnect(layer->data, mask_layer->blue_id);
    gwy_signal_handler_disconnect(layer->data, mask_layer->alpha_id);
}

static void
gwy_layer_mask_changed(GwyPixmapLayer *pixmap_layer)
{
    pixmap_layer->wants_repaint = TRUE;
    gwy_data_view_layer_updated(GWY_DATA_VIEW_LAYER(pixmap_layer));
}

/************************** Documentation ****************************/

/**
 * SECTION:gwylayer-mask
 * @title: GwyLayerMask
 * @short_description: Display data as color mask
 * @see_also: #GwyDataView -- data display widget,
 *            #GwyLayerBasic -- display data colored with a gradient,
 *            #GwyRGBA -- color representation
 *
 * #GwyLayerMask is a data view layer that can paint data as a color mask with
 * varying opacity.  It is not normally usable outside a #GwyDataView.
 *
 * The layer takes all infromation from its parent #GwyDataView, therefore you
 * do not set its properties directly, but you change them in the corresponding
 * #GwyContainer and/or use methods like gwy_layer_mask_set_color_key() to
 * make the layer read a different value from the container.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
