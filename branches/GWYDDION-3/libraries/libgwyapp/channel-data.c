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
#include "libgwy/object-utils.h"
#include "libgwyapp/channel-data.h"

enum {
    PROP_0,
    PROP_FIELD,
    N_PROPS
};

struct _GwyChannelDataPrivate {
    GwyField *field;
};

typedef struct _GwyChannelDataPrivate ChannelData;

static void     gwy_channel_data_finalize    (GObject *object);
static void     gwy_channel_data_dispose     (GObject *object);
static void     gwy_channel_data_set_property(GObject *object,
                                              guint prop_id,
                                              const GValue *value,
                                              GParamSpec *pspec);
static void     gwy_channel_data_get_property(GObject *object,
                                              guint prop_id,
                                              GValue *value,
                                              GParamSpec *pspec);
static gboolean set_field                    (GwyChannelData *channeldata,
                                              GwyField *field);

static GParamSpec *properties[N_PROPS];

G_DEFINE_TYPE(GwyChannelData, gwy_channel_data, GWY_TYPE_DATA);

static void
gwy_channel_data_class_init(GwyChannelDataClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    g_type_class_add_private(klass, sizeof(ChannelData));

    gobject_class->dispose = gwy_channel_data_dispose;
    gobject_class->finalize = gwy_channel_data_finalize;
    gobject_class->get_property = gwy_channel_data_get_property;
    gobject_class->set_property = gwy_channel_data_set_property;

    properties[PROP_FIELD]
        = g_param_spec_object("field",
                              "Field",
                              "Data field representing the channel.",
                              GWY_TYPE_FIELD,
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
}

static void
gwy_channel_data_finalize(GObject *object)
{
    //GwyChannelData *channeldata = GWY_CHANNEL_DATA(object);
    G_OBJECT_CLASS(gwy_channel_data_parent_class)->finalize(object);
}

static void
gwy_channel_data_dispose(GObject *object)
{
    GwyChannelData *channeldata = GWY_CHANNEL_DATA(object);
    ChannelData *priv = channeldata->priv;
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

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
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

static gboolean
set_field(GwyChannelData *channeldata,
          GwyField *field)
{
    ChannelData *priv = channeldata->priv;
    if (!gwy_set_member_object(channeldata, field, GWY_TYPE_FIELD,
                               &priv->field,
                               NULL))
        return FALSE;

    return TRUE;
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
