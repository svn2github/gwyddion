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
#include "libgwyapp/data-item.h"
#include "libgwyapp/data-list-internal.h"

enum {
    PROP_0,
    PROP_ID,
    PROP_DATA_LIST,
    N_PROPS
};

struct _GwyDataItemPrivate {
    GwyDataList *parent_list;
    guint id;
};

typedef struct _GwyDataItemPrivate DataItem;

static void     gwy_data_item_finalize    (GObject *object);
static void     gwy_data_item_dispose     (GObject *object);
static void     gwy_data_item_set_property(GObject *object,
                                           guint prop_id,
                                           const GValue *value,
                                           GParamSpec *pspec);
static void     gwy_data_item_get_property(GObject *object,
                                           guint prop_id,
                                           GValue *value,
                                           GParamSpec *pspec);

static GParamSpec *properties[N_PROPS];

G_DEFINE_ABSTRACT_TYPE(GwyDataItem, gwy_data_item, G_TYPE_INITIALLY_UNOWNED);

static void
gwy_data_item_class_init(GwyDataItemClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    g_type_class_add_private(klass, sizeof(DataItem));

    gobject_class->dispose = gwy_data_item_dispose;
    gobject_class->finalize = gwy_data_item_finalize;
    gobject_class->get_property = gwy_data_item_get_property;
    gobject_class->set_property = gwy_data_item_set_property;

    properties[PROP_ID]
        = g_param_spec_uint("id",
                            "Id",
                            "Unique identified of the data in the list.",
                            0, MAX_ITEM_ID, MAX_ITEM_ID,
                            G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    properties[PROP_DATA_LIST]
        = g_param_spec_object("data-list",
                              "Data list",
                              "Data list the data belong to.",
                              GWY_TYPE_DATA_LIST,
                              G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    for (guint i = 1; i < N_PROPS; i++)
        g_object_class_install_property(gobject_class, i, properties[i]);
}

static void
gwy_data_item_init(GwyDataItem *dataitem)
{
    dataitem->priv = G_TYPE_INSTANCE_GET_PRIVATE(dataitem,
                                                 GWY_TYPE_DATA_ITEM, DataItem);
}

static void
gwy_data_item_finalize(GObject *object)
{
    //GwyDataItem *dataitem = GWY_DATA_ITEM(object);
    G_OBJECT_CLASS(gwy_data_item_parent_class)->finalize(object);
}

static void
gwy_data_item_dispose(GObject *object)
{
    //GwyDataItem *dataitem = GWY_DATA_ITEM(object);
    G_OBJECT_CLASS(gwy_data_item_parent_class)->dispose(object);
}

static void
gwy_data_item_set_property(GObject *object,
                           guint prop_id,
                           G_GNUC_UNUSED const GValue *value,
                           GParamSpec *pspec)
{
    //GwyDataItem *dataitem = GWY_DATA_ITEM(object);

    switch (prop_id) {
        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_data_item_get_property(GObject *object,
                           guint prop_id,
                           GValue *value,
                           GParamSpec *pspec)
{
    GwyDataItem *dataitem = GWY_DATA_ITEM(object);
    DataItem *priv = dataitem->priv;

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
 * gwy_data_item_get_id:
 * @dataitem: A high-level data item.
 *
 * Gets the unique identifier of a data item within its list.
 *
 * Returns: The unique identifier of the data item.
 **/
guint
gwy_data_item_get_id(const GwyDataItem *dataitem)
{
    g_return_val_if_fail(GWY_IS_DATA_ITEM(dataitem), 0);
    return dataitem->priv->id;
}

/**
 * gwy_data_item_get_data_list:
 * @dataitem: A high-level data item.
 *
 * Gets the data list a data item belongs to.
 *
 * Returns: (allow-none) (transfer none):
 *          The data list the data item belongs to.
 **/
GwyDataList*
gwy_data_item_get_data_list(const GwyDataItem *dataitem)
{
    g_return_val_if_fail(GWY_IS_DATA_ITEM(dataitem), NULL);
    return dataitem->priv->parent_list;
}

void
_gwy_data_item_set_list_and_id(GwyDataItem *dataitem,
                               GwyDataList *datalist,
                               guint id)
{
    g_return_if_fail(GWY_IS_DATA_ITEM(dataitem));
    g_return_if_fail(GWY_IS_DATA_LIST(datalist));
    g_return_if_fail(id < MAX_ITEM_ID);
    DataItem *priv = dataitem->priv;
    g_return_if_fail(!priv->parent_list || priv->parent_list == datalist);
    g_return_if_fail(priv->id == MAX_ITEM_ID || priv->id == id);
    priv->id = id;
    priv->parent_list = datalist;
    // FIXME: We should hold a weak ref/pointer to avoid parent_list becoming
    // invalid.
    g_object_notify_by_pspec(G_OBJECT(datalist), properties[PROP_DATA_LIST]);
    g_object_notify_by_pspec(G_OBJECT(datalist), properties[PROP_ID]);
}

/************************** Documentation ****************************/

/**
 * SECTION: data-item
 * @title: GwyDataItem
 * @short_description: High-level data item
 *
 * #GwyDataItem represent the high-level concept of a data item, for instance a
 * channel (image) or volume data.  The corresponding raw data may be
 * represented with a low-level object such as #GwyField or #GwyBrick.
 * However, a channel or volume data can have various other properties and
 * settings: false colour mapping setup, metadata, associated selections and
 * mask, etc.  The purpose of #GwyDataItem subclasses is to manage all these
 * pieces together.
 **/

/**
 * GwyDataItem:
 *
 * Object representing one high-level data item.
 *
 * The #GwyDataItem struct contains private data only and should be accessed
 * using the functions below.
 **/

/**
 * GwyDataItemClass:
 *
 * Class of high-level data items.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
