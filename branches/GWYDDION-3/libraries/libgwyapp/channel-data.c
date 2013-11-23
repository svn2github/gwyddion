/*
 *  $Id$
 *  Copyright (C) 2013 David Nečas (Yeti).
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
#include "libgwy/strfuncs.h"
#include "libgwy/gradient.h"
#include "libgwy/object-utils.h"
#include "libgwyui/raster-area.h"
#include "libgwyapp/channel-data.h"

enum { BITS_PER_SAMPLE = 8 };

enum {
    PROP_0,
    PROP_FIELD,
    PROP_GRADIENT_NAME,
    PROP_MASK_ID,
    N_PROPS
};

struct _GwyChannelDataPrivate {
    GwyField *field;
    gchar *gradient_name;
    guint mask_id;

    GtkWidget *rasterarea;
    gulong rasterarea_destroy_id;
};

typedef struct _GwyChannelDataPrivate ChannelData;

static void         gwy_channel_data_finalize        (GObject *object);
static void         gwy_channel_data_dispose         (GObject *object);
static void         gwy_channel_data_set_property    (GObject *object,
                                                      guint prop_id,
                                                      const GValue *value,
                                                      GParamSpec *pspec);
static void         gwy_channel_data_get_property    (GObject *object,
                                                      guint prop_id,
                                                      GValue *value,
                                                      GParamSpec *pspec);
static GObject*     gwy_channel_data_get_data        (const GwyDataItem *dataitem);
static const gchar* gwy_channel_data_get_name        (const GwyDataItem *dataitem);
static void         gwy_channel_data_set_name        (GwyDataItem *dataitem,
                                                      const gchar *name);
static GdkPixbuf*   gwy_channel_data_render_thumbnail(GwyDataItem *dataitem,
                                                      guint maxsize);
static gboolean     set_field                        (GwyChannelData *channeldata,
                                                      GwyField *field);
static gboolean     set_gradient_name                (GwyChannelData *channeldata,
                                                      const gchar *name);
static gboolean     set_mask_id                      (GwyChannelData *channeldata,
                                                      guint id);
static void         rasterarea_destroyed             (GwyChannelData *channeldata,
                                                      GwyRasterArea *rasterarea);

static GParamSpec *properties[N_PROPS];

G_DEFINE_TYPE(GwyChannelData, gwy_channel_data, GWY_TYPE_DATA_ITEM);

static void
gwy_channel_data_class_init(GwyChannelDataClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GwyDataItemClass *item_class = GWY_DATA_ITEM_CLASS(klass);

    g_type_class_add_private(klass, sizeof(ChannelData));

    gobject_class->dispose = gwy_channel_data_dispose;
    gobject_class->finalize = gwy_channel_data_finalize;
    gobject_class->get_property = gwy_channel_data_get_property;
    gobject_class->set_property = gwy_channel_data_set_property;

    item_class->get_data = gwy_channel_data_get_data;
    item_class->get_name = gwy_channel_data_get_name;
    item_class->set_name = gwy_channel_data_set_name;
    item_class->render_thumbnail = gwy_channel_data_render_thumbnail;

    properties[PROP_FIELD]
        = g_param_spec_object("field",
                              "Field",
                              "Data field representing the channel.",
                              GWY_TYPE_FIELD,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_GRADIENT_NAME]
        = g_param_spec_string("gradient-name",
                              "Gradient name",
                              "Name of false colour gradient resource used "
                              "for visualisation.",
                              NULL,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_MASK_ID]
        = g_param_spec_uint("mask-id",
                            "Mask id",
                            "Idenfitier of the associated mask.",
                            0, GWY_DATA_ITEM_MAX_ID, GWY_DATA_ITEM_MAX_ID,
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    for (guint i = 1; i < N_PROPS; i++)
        g_object_class_install_property(gobject_class, i, properties[i]);
}

static void
gwy_channel_data_init(GwyChannelData *channeldata)
{
    channeldata->priv = G_TYPE_INSTANCE_GET_PRIVATE(channeldata,
                                                    GWY_TYPE_CHANNEL_DATA,
                                                    ChannelData);
    ChannelData *priv = channeldata->priv;
    priv->mask_id = GWY_DATA_ITEM_MAX_ID;
}

static void
gwy_channel_data_finalize(GObject *object)
{
    GwyChannelData *channeldata = GWY_CHANNEL_DATA(object);
    ChannelData *priv = channeldata->priv;
    GWY_FREE(priv->gradient_name);
    G_OBJECT_CLASS(gwy_channel_data_parent_class)->finalize(object);
}

static void
gwy_channel_data_dispose(GObject *object)
{
    GwyChannelData *channeldata = GWY_CHANNEL_DATA(object);
    ChannelData *priv = channeldata->priv;
    // FIXME: What about the raster area?  We may let it live on its own
    // though it will not, of course, reflect any changes to the logical
    // channel, only to the underlying physical objects.  We ***MUST*** at
    // least ensure no odd signal handlers remain connected.
    GWY_SIGNAL_HANDLER_DISCONNECT(priv->rasterarea,
                                  priv->rasterarea_destroy_id);
    GWY_OBJECT_UNREF(priv->field);
    G_OBJECT_CLASS(gwy_channel_data_parent_class)->dispose(object);
}

static void
gwy_channel_data_set_property(GObject *object,
                              guint prop_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
    GwyChannelData *channeldata = GWY_CHANNEL_DATA(object);

    switch (prop_id) {
        case PROP_FIELD:
        set_field(channeldata, g_value_get_object(value));
        break;

        case PROP_GRADIENT_NAME:
        set_gradient_name(channeldata, g_value_get_string(value));
        break;

        case PROP_MASK_ID:
        set_mask_id(channeldata, g_value_get_uint(value));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_channel_data_get_property(GObject *object,
                              guint prop_id,
                              GValue *value,
                              GParamSpec *pspec)
{
    GwyChannelData *channeldata = GWY_CHANNEL_DATA(object);
    ChannelData *priv = channeldata->priv;

    switch (prop_id) {
        case PROP_FIELD:
        g_value_set_object(value, priv->field);
        break;

        case PROP_GRADIENT_NAME:
        g_value_set_string(value, priv->gradient_name);
        break;

        case PROP_MASK_ID:
        g_value_set_uint(value, priv->mask_id);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static GObject*
gwy_channel_data_get_data(const GwyDataItem *dataitem)
{
    ChannelData *priv = GWY_CHANNEL_DATA(dataitem)->priv;
    return (GObject*)priv->field;
}

static const gchar*
gwy_channel_data_get_name(const GwyDataItem *dataitem)
{
    ChannelData *priv = GWY_CHANNEL_DATA(dataitem)->priv;
    if (!priv->field)
        return NULL;
    return gwy_field_get_name(priv->field);
}

static void
gwy_channel_data_set_name(GwyDataItem *dataitem,
                          const gchar *name)
{
    ChannelData *priv = GWY_CHANNEL_DATA(dataitem)->priv;
    g_return_if_fail(priv->field);
    gwy_field_set_name(priv->field, name);
}

static GdkPixbuf*
gwy_channel_data_render_thumbnail(GwyDataItem *dataitem,
                                  guint maxsize)
{
    ChannelData *priv = GWY_CHANNEL_DATA(dataitem)->priv;
    if (!priv->field)
        return NULL;

    // FIXME: Hadle non-square data better.  And maybe use a range method
    // that avoids outliers.
    GdkPixbuf *pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE,
                                       BITS_PER_SAMPLE,
                                       maxsize, maxsize);
    GwyGradient *grad = gwy_gradients_get(priv->gradient_name);
    gwy_field_render_pixbuf(priv->field, pixbuf, grad, NULL, NULL);

    return pixbuf;
}

/**
 * gwy_channel_data_new: (constructor)
 *
 * Creates a new channel data item.
 *
 * Returns: (transfer full):
 *          A new channel data item.
 **/
GwyChannelData*
gwy_channel_data_new(void)
{
    return g_object_newv(GWY_TYPE_CHANNEL_DATA, 0, NULL);
}

/**
 * gwy_channel_data_new_with_field: (constructor)
 * @field: (allow-none) (transfer full):
 *         A two-dimensional data field.
 *
 * Creates a new channel data item for given field.
 *
 * Returns: (transfer full):
 *          A new channel data item.
 **/
GwyChannelData*
gwy_channel_data_new_with_field(GwyField *field)
{
    GwyChannelData *channeldata = g_object_newv(GWY_TYPE_CHANNEL_DATA, 0, NULL);
    g_return_val_if_fail(!field || GWY_IS_FIELD(field), channeldata);
    set_field(channeldata, field);
    return channeldata;
}

/**
 * gwy_channel_data_set_field:
 * @channeldata: A channel data item.
 * @field: (allow-none) (transfer full):
 *         A two-dimensional data field.
 *
 * Sets the field that represents the data of a channel.
 *
 * Since the field is the primary data object for the channel, it is not
 * permitted to set it to %NULL once a field has been set.
 **/
void
gwy_channel_data_set_field(GwyChannelData *channeldata,
                           GwyField *field)
{
    g_return_if_fail(GWY_IS_CHANNEL_DATA(channeldata));
    g_return_if_fail(!field || GWY_IS_FIELD(field));
    if (!set_field(channeldata, field))
        return;

    g_object_notify_by_pspec(G_OBJECT(channeldata), properties[PROP_FIELD]);
}

/**
 * gwy_channel_data_get_field:
 * @channeldata: A channel data item.
 *
 * Gets the field that represents the data of a channel.
 *
 * Returns: (allow-none) (transfer none):
 *          The data field representing the channel data.
 **/
GwyField*
gwy_channel_data_get_field(const GwyChannelData *channeldata)
{
    g_return_val_if_fail(GWY_IS_CHANNEL_DATA(channeldata), NULL);
    return channeldata->priv->field;
}

/**
 * gwy_channel_data_set_gradient_name:
 * @channeldata: A channel data item.
 * @name: (allow-none):
 *        Gradient name.  Passing %NULL unsets the name, causing the channel
 *        to use the default gradient.
 *
 * Sets the name of false colour gradient used for the visualisation of a
 * channel data.
 *
 * Setting the name implies the gradient is a gradient resource, and will be
 * looked up in the corresponding #GwyResource inventory.  If no gradient of
 * given name exists the default will be used.
 **/
void
gwy_channel_data_set_gradient_name(GwyChannelData *channeldata,
                                   const gchar *name)
{
    g_return_if_fail(GWY_IS_CHANNEL_DATA(channeldata));
    if (!set_gradient_name(channeldata, name))
        return;

    g_object_notify_by_pspec(G_OBJECT(channeldata),
                             properties[PROP_GRADIENT_NAME]);
}

/**
 * gwy_channel_data_get_gradient_name:
 * @channeldata: A channel data item.
 *
 * Gets the name of false colour gradient used for the visualisation of a
 * channel data.
 *
 * Returns: (allow-none):
 *          The gradient name.  It may be nonexistent.
 **/
const gchar*
gwy_channel_data_get_gradient_name(const GwyChannelData *channeldata)
{
    g_return_val_if_fail(GWY_IS_CHANNEL_DATA(channeldata), NULL);
    return channeldata->priv->gradient_name;
}

/**
 * gwy_channel_data_set_mask_id:
 * @channeldata: A channel data item.
 * @id: Identifier of a mask field (in the same file) associated with the
 *      channel.  It may not be an identifier of an existing mask.  Passing
 *      %GWY_DATA_ITEM_MAX_ID unsets the mask.
 *
 * Sets the identifier of the mask field associated with a channel data.
 **/
void
gwy_channel_data_set_mask_id(GwyChannelData *channeldata,
                             guint id)
{
    g_return_if_fail(GWY_IS_CHANNEL_DATA(channeldata));
    if (!set_mask_id(channeldata, id))
        return;

    g_object_notify_by_pspec(G_OBJECT(channeldata), properties[PROP_MASK_ID]);
}

/**
 * gwy_channel_data_get_mask_id:
 * @channeldata: A channel data item.
 *
 * Gets the identifier of the mask field associated with a channel data.
 *
 * Returns: The mask identifier (in the same file).  It may be nonexistent
 *          and it may be %GWY_DATA_ITEM_MAX_ID.
 **/
guint
gwy_channel_data_get_mask_id(const GwyChannelData *channeldata)
{
    g_return_val_if_fail(GWY_IS_CHANNEL_DATA(channeldata),
                         GWY_DATA_ITEM_MAX_ID);
    return channeldata->priv->mask_id;
}

/**
 * gwy_channel_data_get_raster_area:
 * @channeldata: A channel data item.
 *
 * Obtains the raster area widget showing a channel data.
 *
 * The widget is created by gwy_channel_data_get_raster_area().
 *
 * Returns: (allow-none) (transfer none):
 *          The raster area widget showing this channel data, if any.
 **/
GtkWidget*
gwy_channel_data_get_raster_area(const GwyChannelData *channeldata)
{
    g_return_val_if_fail(GWY_IS_CHANNEL_DATA(channeldata), NULL);
    return (GtkWidget*)channeldata->priv->rasterarea;
}

/**
 * gwy_channel_data_create_raster_area:
 * @channeldata: A channel data item.
 *
 * Creates a raster area widget for a channel data.
 *
 * The widget will follow the channel data changes and, conversely, user
 * changes of the raster area settings will be reflected in the channel data.
 *
 * Only one such widget can exist.  This method does not return the raster area
 * to avoid the temptation to use it instead of
 * gwy_channel_data_get_raster_area().
 **/
// XXX: This is not good because we might want to create a RasterView and
// in such case the RasterArea cannot be created independently.  So either
// we need a own-this-widget function or RasterView needs changeable
// RasterAreas.  The former is probably more realistic.
void
gwy_channel_data_create_raster_area(GwyChannelData *channeldata)
{
    g_return_if_fail(GWY_IS_CHANNEL_DATA(channeldata));
    ChannelData *priv = GWY_CHANNEL_DATA(channeldata)->priv;
    g_return_if_fail(!priv->rasterarea);

    priv->rasterarea = gwy_raster_area_new();
    GwyRasterArea *rasterarea = GWY_RASTER_AREA(priv->rasterarea);
    if (priv->field)
        gwy_raster_area_set_field(rasterarea, priv->field);
    if (priv->gradient_name) {
        GwyInventory *gradients = gwy_gradients();
        GwyGradient *grad = gwy_inventory_get(gradients, priv->gradient_name);
        if (grad)
            gwy_raster_area_set_gradient(rasterarea, grad);
    }
    if (priv->mask_id != GWY_DATA_ITEM_MAX_ID) {
        // TODO
    }

    // TODO: Connect backwards.  A number of properties can be changed in the
    // GUI and we need to be notified and reflect them.

    priv->rasterarea_destroy_id
        = g_signal_connect_swapped(rasterarea, "destroy",
                                   G_CALLBACK(rasterarea_destroyed),
                                   channeldata);
}

static gboolean
set_field(GwyChannelData *channeldata,
          GwyField *field)
{
    ChannelData *priv = channeldata->priv;
    g_return_val_if_fail(field || !priv->field, FALSE);
    if (!gwy_set_member_object(channeldata, field, GWY_TYPE_FIELD,
                               &priv->field,
                               NULL))
        return FALSE;

    // TODO: If there is a raster area/view, update it.
    return TRUE;
}

static gboolean
set_gradient_name(GwyChannelData *channeldata,
                  const gchar *name)
{
    ChannelData *priv = channeldata->priv;
    if (!gwy_assign_string(&priv->gradient_name, name))
        return FALSE;

    // TODO: If there is a raster area/view, update it.
    return TRUE;
}

static gboolean
set_mask_id(GwyChannelData *channeldata,
            guint id)
{
    ChannelData *priv = channeldata->priv;
    if (priv->mask_id == id)
        return FALSE;

    // TODO: If there is a raster area/view, update it.
    // TODO: We need a mechanism for cross-reference management.  If we set a
    // mask id and then, some time later, someone adds a mask field to the list
    // at the corresponding id, we want to be notified.  And, ideally, only
    // those interested in a specific id will be notified to avoid extenstive
    // cross-notification.  This means a GwyDataList needs detailed signals in
    // addition not just those required for GtkTreeModel, with integer-like
    // details – not good.
    return TRUE;
}

static void
rasterarea_destroyed(GwyChannelData *channeldata,
                     GwyRasterArea *rasterarea)
{
    ChannelData *priv = channeldata->priv;
    g_return_if_fail((GwyRasterArea*)priv->rasterarea == rasterarea);

    priv->rasterarea = NULL;
    priv->rasterarea_destroy_id = 0;
    // TODO: More?
}

/************************** Documentation ****************************/

/**
 * SECTION: channel-data
 * @title: GwyChannelData
 * @short_description: Channel data item
 *
 * #GwyChannelData represent the high-level concept of a channel (image) data
 * item.
 **/

/**
 * GwyChannelData:
 *
 * Object representing one channel data item.
 *
 * The #GwyChannelData struct contains private data only and should be accessed
 * using the functions below.
 **/

/**
 * GwyChannelDataClass:
 *
 * Class of channel data items.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
