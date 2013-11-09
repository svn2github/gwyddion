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
#include "libgwyapp/data.h"

enum {
    PROP_0,
    PROP_ID,
    PROP_DATA_LIST,
    N_PROPS
};

struct _GwyDataPrivate {
    GwyDataList *parent_list;
    guint id;
};

typedef struct _GwyDataPrivate Data;

static void     gwy_data_finalize    (GObject *object);
static void     gwy_data_dispose     (GObject *object);
static void     gwy_data_set_property(GObject *object,
                                      guint prop_id,
                                      const GValue *value,
                                      GParamSpec *pspec);
static void     gwy_data_get_property(GObject *object,
                                      guint prop_id,
                                      GValue *value,
                                      GParamSpec *pspec);
static gboolean set_id               (GwyData *data,
                                      guint id);
static gboolean set_data_list        (GwyData *data,
                                      GwyDataList *datalist);

static GParamSpec *properties[N_PROPS];

G_DEFINE_TYPE(GwyData, gwy_data, G_TYPE_OBJECT);

static void
gwy_data_class_init(GwyDataClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    g_type_class_add_private(klass, sizeof(Data));

    gobject_class->dispose = gwy_data_dispose;
    gobject_class->finalize = gwy_data_finalize;
    gobject_class->get_property = gwy_data_get_property;
    gobject_class->set_property = gwy_data_set_property;

    properties[PROP_ID]
        = g_param_spec_uint("id",
                            "Id",
                            "Unique identified of the data in the list.",
                             0, G_MAXUINT32, 0,
                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS
                             | G_PARAM_CONSTRUCT_ONLY);

    properties[PROP_DATA_LIST]
        = g_param_spec_object("data-list",
                              "Data list",
                              "Data list the data belong to.",
                              GWY_TYPE_DATA_LIST,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS
                              | G_PARAM_CONSTRUCT_ONLY);

    for (guint i = 1; i < N_PROPS; i++)
        g_object_class_install_property(gobject_class, i, properties[i]);
}

static void
gwy_data_init(GwyData *data)
{
    data->priv = G_TYPE_INSTANCE_GET_PRIVATE(data,
                                                 GWY_TYPE_DATA, Data);
}

static void
gwy_data_finalize(GObject *object)
{
    //GwyData *data = GWY_DATA(object);
    G_OBJECT_CLASS(gwy_data_parent_class)->finalize(object);
}

static void
gwy_data_dispose(GObject *object)
{
    //GwyData *data = GWY_DATA(object);
    G_OBJECT_CLASS(gwy_data_parent_class)->dispose(object);
}

static void
gwy_data_set_property(GObject *object,
                      guint prop_id,
                      const GValue *value,
                      GParamSpec *pspec)
{
    GwyData *data = GWY_DATA(object);

    switch (prop_id) {
        case PROP_ID:
        set_id(data, g_value_get_uint(value));
        break;

        case PROP_DATA_LIST:
        set_data_list(data, g_value_get_object(value));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_data_get_property(GObject *object,
                      guint prop_id,
                      GValue *value,
                      GParamSpec *pspec)
{
    GwyData *data = GWY_DATA(object);
    Data *priv = data->priv;

    switch (prop_id) {
        case PROP_ID:
        g_value_set_uint(value, priv->id);
        break;

        case PROP_DATA_LIST:
        g_value_set_object(value, priv->parent_list);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

/**
 * gwy_data_get_id:
 * @data: A high-level data item.
 *
 * Gets the unique identifier of a data item within its list.
 *
 * Returns: The unique identifier of the data item.
 **/
guint
gwy_data_get_id(const GwyData *data)
{
    g_return_val_if_fail(GWY_IS_DATA(data), 0);
    return data->priv->id;
}

/**
 * gwy_data_get_data_list:
 * @data: A high-level data item.
 *
 * Gets the data list a data item belongs to.
 *
 * Returns: (allow-none) (transfer none):
 *          The data list the data item belongs to.
 **/
GwyDataList*
gwy_data_get_data_list(const GwyData *data)
{
    g_return_val_if_fail(GWY_IS_DATA(data), NULL);
    return data->priv->parent_list;
}

static gboolean
set_id(GwyData *data,
       guint id)
{
    Data *priv = data->priv;
    if (id == priv->id)
        return FALSE;

    priv->id = id;
    return TRUE;
}

static gboolean
set_data_list(GwyData *data,
              GwyDataList *datalist)
{
    Data *priv = data->priv;
    if (datalist == priv->parent_list)
        return FALSE;

    // FIXME: We should hold a weak ref/pointer to avoid parent_list becoming
    // invalid.
    priv->parent_list = datalist;
    return TRUE;
}

/************************** Documentation ****************************/

/**
 * SECTION: data
 * @title: GwyData
 * @short_description: High-level data item
 *
 * #GwyData represent the high-level concept of a data item, for instance a
 * channel (image) or volume data.  The corresponding raw data may be
 * represented with a low-level object such as #GwyField or #GwyBrick.
 * However, a channel or volume data can have various other properties and
 * settings: false colour mapping setup, metadata, associated selections and
 * mask, etc.  The purpose of #GwyData subclasses is to manage all these pieces
 * together.
 **/

/**
 * GwyData:
 *
 * Object representing one high-level data item.
 *
 * The #GwyData struct contains private data only and should be accessed
 * using the functions below.
 **/

/**
 * GwyDataClass:
 *
 * Class of high-level data items.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
