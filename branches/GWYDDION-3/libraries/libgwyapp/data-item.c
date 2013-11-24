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
    THUMBNAIL_SIZE = 64
};

enum {
    PROP_0,
    PROP_ID,
    PROP_DATA_LIST,
    N_PROPS
};

struct _GwyDataItemPrivate {
    GwyDataList *parent_list;
    guint id;

    GdkPixbuf *thumbnail;
};

typedef struct _GwyDataItemPrivate DataItem;

static void         gwy_data_item_finalize    (GObject *object);
static void         gwy_data_item_dispose     (GObject *object);
static void         gwy_data_item_set_property(GObject *object,
                                               guint prop_id,
                                               const GValue *value,
                                               GParamSpec *pspec);
static void         gwy_data_item_get_property(GObject *object,
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
                            0, GWY_DATA_ITEM_MAX_ID, GWY_DATA_ITEM_MAX_ID,
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
    dataitem->priv->id = GWY_DATA_ITEM_MAX_ID;
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
    GwyDataItem *dataitem = GWY_DATA_ITEM(object);
    DataItem *priv = dataitem->priv;
    GWY_OBJECT_UNREF(priv->thumbnail);
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

/**
 * gwy_data_item_get_data:
 * @dataitem: A high-level data item.
 *
 * Gets the primary data object of a data item.
 *
 * The primary data object is a #GwyField for channels, #GwyCurve or #GwyLine
 * for graphs, #GwyBrick for volume data, etc.  This method just represent
 * generic means of obtaining it.
 *
 * Returns: (allow-none) (transfer none):
 *          The primary data object of the item.  This method may return %NULL
 *          only for items not belonging to any #GwyDataList.
 **/
GObject*
gwy_data_item_get_data(const GwyDataItem *dataitem)
{
    g_return_val_if_fail(GWY_IS_DATA_ITEM(dataitem), NULL);
    GwyDataItemClass *klass = GWY_DATA_ITEM_GET_CLASS(dataitem);
    // XXX: This should be also available via the "data" property.  So no
    // dedicated method will be necessary.
    g_return_val_if_fail(klass && klass->get_data, NULL);
    return klass->get_data(dataitem);
}

/**
 * gwy_data_item_get_name:
 * @dataitem: A high-level data item.
 *
 * Gets the name of a data item.
 *
 * The name is determined by the primary data object (#GwyField, #GwyCurve,
 * #GwyBrick, etc.).  This method just represent generic means of obtaining it.
 *
 * Returns: (allow-none):
 *          Name of the primary data object, possibly %NULL.
 **/
const gchar*
gwy_data_item_get_name(const GwyDataItem *dataitem)
{
    g_return_val_if_fail(GWY_IS_DATA_ITEM(dataitem), NULL);
    GwyDataItemClass *klass = GWY_DATA_ITEM_GET_CLASS(dataitem);
    g_return_val_if_fail(klass && klass->get_name, NULL);
    return klass->get_name(dataitem);
}

/**
 * gwy_data_item_set_name:
 * @dataitem: A high-level data item.
 * @name: (allow-none):
 *        New item name.
 *
 * Sets the name of a data item.
 *
 * The name is set on by the primary data object (#GwyField, #GwyCurve,
 * #GwyBrick, etc.).  This method just represent generic means of obtaining it.
 * It is an error to call it when no primary data object has been set yet.
 **/
void
gwy_data_item_set_name(GwyDataItem *dataitem,
                       const gchar *name)
{
    g_return_if_fail(GWY_IS_DATA_ITEM(dataitem));
    GwyDataItemClass *klass = GWY_DATA_ITEM_GET_CLASS(dataitem);
    g_return_if_fail(klass && klass->set_name);
    klass->set_name(dataitem, name);
}

/**
 * gwy_data_item_invalidate:
 * @dataitem: A high-level data item.
 *
 * Invalidates cached data, namely the thumbnail, of a data item.
 **/
void
gwy_data_item_invalidate(GwyDataItem *dataitem)
{
    g_return_if_fail(GWY_IS_DATA_ITEM(dataitem));
    DataItem *priv = dataitem->priv;
    GWY_OBJECT_UNREF(priv->thumbnail);
}

/**
 * gwy_data_item_get_thumbnail:
 * @dataitem: A high-level data item.
 * @maxsize: Maximum width and height.  The width and height of the returned
 *           thumbnail does not exceed @maxsize but they may be smaller.
 *           Passing 0 means the default dimensions are used (which often
 *           means a cached thumbnail can be returned).
 *
 * Gets the thumbnail for a data item.
 *
 * If a subclass does not provide thumbnails or, for whatever reason, the
 * thumbnail cannot be created this method returns %NULL.
 *
 * Returns: (allow-none) (transfer full):
 *          A pixbuf with the thumbnail.  The caller always owns a reference
 *          but must not modify the pixbuf because it may be the cached (i.e.
 *          shared) thumbnail.
 **/
GdkPixbuf*
gwy_data_item_get_thumbnail(GwyDataItem *dataitem,
                            guint maxsize)
{
    g_return_val_if_fail(GWY_IS_DATA_ITEM(dataitem), NULL);
    GwyDataItemClass *klass = GWY_DATA_ITEM_GET_CLASS(dataitem);
    DataItem *priv = dataitem->priv;
    gboolean cache_it = FALSE;
    if (!maxsize)
        maxsize = THUMBNAIL_SIZE;
    if (maxsize == THUMBNAIL_SIZE) {
        if (priv->thumbnail) {
            g_object_ref(priv->thumbnail);
            return priv->thumbnail;
        }
        cache_it = TRUE;
    }
    g_return_val_if_fail(klass, NULL);
    if (!klass->render_thumbnail)
        return NULL;

    GdkPixbuf *thumbnail = klass->render_thumbnail(dataitem, maxsize);
    if (cache_it && thumbnail) {
        priv->thumbnail = g_object_ref(thumbnail);
    }

    return thumbnail;
}

void
_gwy_data_item_set_list_and_id(GwyDataItem *dataitem,
                               GwyDataList *datalist,
                               guint id)
{
    g_return_if_fail(GWY_IS_DATA_ITEM(dataitem));
    g_return_if_fail(GWY_IS_DATA_LIST(datalist));
    g_return_if_fail(id < GWY_DATA_ITEM_MAX_ID);
    DataItem *priv = dataitem->priv;
    g_return_if_fail(!priv->parent_list || priv->parent_list == datalist);
    g_return_if_fail(priv->id == GWY_DATA_ITEM_MAX_ID || priv->id == id);
    priv->id = id;
    priv->parent_list = datalist;
    // FIXME: We should hold a weak ref/pointer to avoid parent_list becoming
    // invalid.
    g_object_notify_by_pspec(G_OBJECT(datalist), properties[PROP_DATA_LIST]);
    g_object_notify_by_pspec(G_OBJECT(datalist), properties[PROP_ID]);
}

G_DEFINE_QUARK(gwy-data-item, _gwy_data_item);

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
 * @get_data: Obtains the primary data object for the item.
 * @get_name: Obtains the name of primary data object (usually its "name"
 *            property).
 * @set_name: Changes the name of primary data object (usually its "name"
 *            property).
 * @render_thumbnail: Renders a thumbnail at speficied size.  Thumbnails at
 *                    the default size are cached by #GwyDataItem and
 *                    subclasses do not need perform any caching for them.
 *
 * Class of high-level data items.
 **/

/**
 * GWY_DATA_ITEM_MAX_ID:
 *
 * Maximum possible identifier of a data item.
 *
 * This is also the identifier of newly created data items not yet in any list.
 * Data items within a data list never get assigned identifier
 * %GWY_DATA_ITEM_MAX_ID; the maximum permitted value is
 * %GWY_DATA_ITEM_MAX_ID-1.  So, in many circumstances it is used to represent
 * an invalid identifier and stands for ‘no data’.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
