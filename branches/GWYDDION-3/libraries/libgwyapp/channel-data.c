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
#include "libgwyapp/channel-data.h"
#include "libgwyapp/data-list-internal.h"

enum { BITS_PER_SAMPLE = 8 };

enum {
    PROP_0,
    PROP_FIELD,
    PROP_GRADIENT_NAME,
    PROP_MASK_ID,
    N_PROPS,
    PROP_RANGE_FROM_METHOD = N_PROPS,
    PROP_RANGE_TO_METHOD,
    PROP_MASK_COLOR,
    PROP_USER_RANGE,
    PROP_ZOOM,
    PROP_REAL_ASPECT_RATIO,
    N_TOTAL_PROPS
};

struct _GwyChannelDataPrivate {
    /* RasterArea settings */
    GwyField *field;
    gchar *gradient_name;
    GwyColorRangeType range_from_method;
    GwyColorRangeType range_to_method;
    GwyRGBA *mask_color;
    GwyRange *user_range;
    gdouble zoom;
    gboolean real_aspect_ratio;
    guint mask_id;

    GwyRasterArea *rasterarea;
    gulong rasterarea_destroy_id;
    gulong rasterarea_notify_id;
    gboolean rasterarea_updating;
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
static gboolean     set_range_from_method            (GwyChannelData *channeldata,
                                                      GwyColorRangeType method);
static gboolean     set_range_to_method              (GwyChannelData *channeldata,
                                                      GwyColorRangeType method);
static gboolean     set_mask_color                   (GwyChannelData *channeldata,
                                                      const GwyRGBA *color);
static gboolean     set_user_range                   (GwyChannelData *channeldata,
                                                      const GwyRange *range);
static gboolean     set_zoom                         (GwyChannelData *channeldata,
                                                      gdouble zoom);
static gboolean     set_real_aspect_ratio            (GwyChannelData *channeldata,
                                                      gdouble setting);
static void         raster_area_notify               (GwyChannelData *channeldata,
                                                      GParamSpec *pspec,
                                                      GwyRasterArea *rasterarea);
static void         raster_area_destroy              (GwyChannelData *channeldata,
                                                      GwyRasterArea *rasterarea);
static void         show_in_raster_area              (GwyChannelData *channeldata);
static void         unshow_in_raster_area            (GwyChannelData *channeldata,
                                                      gboolean destroying);
static void         update_raster_area_gradient      (GwyChannelData *channeldata);
static void         update_raster_area_property      (GwyChannelData *channeldata,
                                                      guint propid);

static GParamSpec *properties[N_TOTAL_PROPS];

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

    gwy_replicate_class_properties(gobject_class, GWY_TYPE_RASTER_AREA,
                                   properties, N_PROPS,
                                   "range-from-method",
                                   "range-to-method",
                                   "mask-color",
                                   "user-range",
                                   "zoom",
                                   "real-aspect-ratio",
                                   NULL);
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
    gwy_channel_data_show_in_raster_area(channeldata, NULL);
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

        case PROP_RANGE_FROM_METHOD:
        set_range_from_method(channeldata, g_value_get_enum(value));
        break;

        case PROP_RANGE_TO_METHOD:
        set_range_to_method(channeldata, g_value_get_enum(value));
        break;

        case PROP_MASK_COLOR:
        set_mask_color(channeldata, g_value_get_boxed(value));
        break;

        case PROP_USER_RANGE:
        set_user_range(channeldata, g_value_get_boxed(value));
        break;

        case PROP_ZOOM:
        set_zoom(channeldata, g_value_get_double(value));
        break;

        case PROP_REAL_ASPECT_RATIO:
        set_real_aspect_ratio(channeldata, g_value_get_boolean(value));
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

        case PROP_RANGE_FROM_METHOD:
        g_value_set_enum(value, priv->range_from_method);
        break;

        case PROP_RANGE_TO_METHOD:
        g_value_set_enum(value, priv->range_to_method);
        break;

        case PROP_MASK_COLOR:
        g_value_set_boxed(value, priv->mask_color);
        break;

        case PROP_USER_RANGE:
        g_value_set_boxed(value, priv->user_range);
        break;

        case PROP_ZOOM:
        g_value_set_double(value, priv->zoom);
        break;

        case PROP_REAL_ASPECT_RATIO:
        g_value_set_boolean(value, priv->real_aspect_ratio);
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
 * gwy_channel_data_show_in_raster_area:
 * @channeldata: A channel data item.
 * @rasterarea: (allow-none) (transfer none):
 *              A raster area widget.
 *
 * Sets up a raster area to show a channel data.
 *
 * The widget will follow the channel data changes and, conversely, user
 * changes of the raster area settings will be reflected in the channel data.
 *
 * At most one such widget can exist at a time; calling this method when the
 * channel is already shown elsewhere will un-show it there first.
 **/
void
gwy_channel_data_show_in_raster_area(GwyChannelData *channeldata,
                                     GwyRasterArea *rasterarea)
{
    g_printerr("SHOW-IN-RASTER-AREA %p %p\n", channeldata, rasterarea);
    g_return_if_fail(GWY_IS_CHANNEL_DATA(channeldata));
    ChannelData *priv = GWY_CHANNEL_DATA(channeldata)->priv;
    if (rasterarea) {
        g_return_if_fail(GWY_IS_RASTER_AREA(rasterarea));
        GwyChannelData *cdata = g_object_get_qdata(G_OBJECT(rasterarea),
                                                   GWY_DATA_ITEM_QUARK);
        if (cdata == channeldata && priv->rasterarea == rasterarea)
            return;

        g_assert(cdata != channeldata && priv->rasterarea != rasterarea);

        // Some other channel is shown in @rasterarea.  Unshow it.
        if (cdata)
            gwy_channel_data_show_in_raster_area(cdata, NULL);
    }

    // This channel is shown elsewhere.  Unshow it.
    gwy_set_member_object(channeldata, NULL, GWY_TYPE_RASTER_AREA,
                          &priv->rasterarea,
                          "notify", &raster_area_notify,
                          &priv->rasterarea_notify_id, G_CONNECT_SWAPPED,
                          "destroy", &raster_area_destroy,
                          &priv->rasterarea_destroy_id, G_CONNECT_SWAPPED,
                          NULL);

    unshow_in_raster_area(channeldata, FALSE);

    gwy_set_member_object(channeldata, rasterarea, GWY_TYPE_RASTER_AREA,
                          &priv->rasterarea,
                          "notify", &raster_area_notify,
                          &priv->rasterarea_notify_id, G_CONNECT_SWAPPED,
                          "destroy", &raster_area_destroy,
                          &priv->rasterarea_destroy_id, G_CONNECT_SWAPPED,
                          NULL);

    if (rasterarea)
        show_in_raster_area(channeldata);
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

    update_raster_area_gradient(channeldata);
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

static gboolean
set_range_from_method(GwyChannelData *channeldata,
                      GwyColorRangeType method)
{
    ChannelData *priv = channeldata->priv;
    if (method == priv->range_from_method)
        return FALSE;

    priv->range_from_method = method;
    update_raster_area_property(channeldata, PROP_RANGE_FROM_METHOD);
    return TRUE;
}

static gboolean
set_range_to_method(GwyChannelData *channeldata,
                    GwyColorRangeType method)
{
    ChannelData *priv = channeldata->priv;
    if (method == priv->range_to_method)
        return FALSE;

    priv->range_to_method = method;
    update_raster_area_property(channeldata, PROP_RANGE_TO_METHOD);
    return TRUE;
}

static gboolean
set_mask_color(GwyChannelData *channeldata,
               const GwyRGBA *color)
{
    ChannelData *priv = channeldata->priv;
    if (!gwy_assign_boxed((gpointer*)&priv->mask_color, color, GWY_TYPE_RGBA))
        return FALSE;

    update_raster_area_property(channeldata, PROP_MASK_COLOR);
    return TRUE;
}

static gboolean
set_user_range(GwyChannelData *channeldata,
               const GwyRange *range)
{
    ChannelData *priv = channeldata->priv;
    if (!gwy_assign_boxed((gpointer*)&priv->user_range, range, GWY_TYPE_RANGE))
        return FALSE;

    update_raster_area_property(channeldata, PROP_USER_RANGE);
    return TRUE;
}

static gboolean
set_zoom(GwyChannelData *channeldata,
         gdouble zoom)
{
    ChannelData *priv = channeldata->priv;
    if (zoom == priv->zoom)
        return FALSE;

    priv->zoom = zoom;
    update_raster_area_property(channeldata, PROP_ZOOM);
    return TRUE;
}

static gboolean
set_real_aspect_ratio(GwyChannelData *channeldata,
                      gdouble setting)
{
    ChannelData *priv = channeldata->priv;
    if (setting == priv->real_aspect_ratio)
        return FALSE;

    priv->real_aspect_ratio = setting;
    update_raster_area_property(channeldata, PROP_REAL_ASPECT_RATIO);
    return TRUE;
}

static void
raster_area_notify(GwyChannelData *channeldata,
                   GParamSpec *pspec,
                   GwyRasterArea *rasterarea)
{
    ChannelData *priv = channeldata->priv;
    g_return_if_fail(priv->rasterarea == rasterarea);

    if (priv->rasterarea_updating)
        return;

    priv->rasterarea_updating = TRUE;

    if (gwy_strequal(pspec->name, "gradient")) {
        GwyGradient *grad = gwy_raster_area_get_gradient(rasterarea);
        if (grad) {
            GwyResource *resource = GWY_RESOURCE(grad);
            if (gwy_resource_is_managed(resource)) {
                const gchar *name = gwy_resource_get_name(resource);
                gwy_channel_data_set_gradient_name(channeldata, name);
            }
            else {
                g_warning("ChannelData cannot follow unmanaged gradients "
                          "(yet).");
            }
        }
        else
            gwy_channel_data_set_gradient_name(channeldata, NULL);
    }
    else if (gwy_strequal(pspec->name, "range-from-method"))
        set_range_from_method(channeldata,
                              gwy_raster_area_get_range_from_method(rasterarea));
    else if (gwy_strequal(pspec->name, "range-to-method"))
        set_range_to_method(channeldata,
                            gwy_raster_area_get_range_to_method(rasterarea));
    else if (gwy_strequal(pspec->name, "mask-color"))
        set_mask_color(channeldata, gwy_raster_area_get_mask_color(rasterarea));
    else if (gwy_strequal(pspec->name, "user-range")) {
        set_user_range(channeldata, gwy_raster_area_get_user_range(rasterarea));
    }
    else if (gwy_strequal(pspec->name, "zoom")) {
        gdouble zoom;
        g_object_get(rasterarea, "zoom", &zoom, NULL);
        set_zoom(channeldata, zoom);
    }
    else if (gwy_strequal(pspec->name, "real-aspect-ratio")) {
        gboolean real_aspect_ratio;
        g_object_get(rasterarea, "real-aspect-ratio", &real_aspect_ratio, NULL);
        set_real_aspect_ratio(channeldata, real_aspect_ratio);
    }

    priv->rasterarea_updating = FALSE;
}

static void
raster_area_destroy(GwyChannelData *channeldata,
                    GwyRasterArea *rasterarea)
{
    ChannelData *priv = channeldata->priv;
    g_return_if_fail(priv->rasterarea == rasterarea);

    priv->rasterarea_destroy_id = 0;
    unshow_in_raster_area(channeldata, TRUE);
}

static void
show_in_raster_area(GwyChannelData *channeldata)
{
    ChannelData *priv = channeldata->priv;
    GwyRasterArea *rasterarea = GWY_RASTER_AREA(priv->rasterarea);
    g_printerr("SHOW %p %p\n", channeldata, rasterarea);

    g_object_set_qdata(G_OBJECT(rasterarea), GWY_DATA_ITEM_QUARK, channeldata);

    if (priv->field) {
        priv->rasterarea_updating = TRUE;
        gwy_raster_area_set_field(rasterarea, priv->field);
        priv->rasterarea_updating = FALSE;
    }

    update_raster_area_gradient(channeldata);
    update_raster_area_property(channeldata, PROP_RANGE_FROM_METHOD);
    update_raster_area_property(channeldata, PROP_RANGE_TO_METHOD);
    update_raster_area_property(channeldata, PROP_MASK_COLOR);
    update_raster_area_property(channeldata, PROP_USER_RANGE);
    update_raster_area_property(channeldata, PROP_ZOOM);
    update_raster_area_property(channeldata, PROP_REAL_ASPECT_RATIO);

    if (priv->mask_id != GWY_DATA_ITEM_MAX_ID) {
        // TODO
    }

    // TODO: Connect backwards.  A number of properties can be changed in the
    // GUI and we need to be notified and reflect them.
}

static void
unshow_in_raster_area(GwyChannelData *channeldata,
                      gboolean destroying)
{
    ChannelData *priv = channeldata->priv;
    g_printerr("UNSHOW %p %p\n", channeldata, priv->rasterarea);
    if (!priv->rasterarea)
        return;

    g_object_set_qdata(G_OBJECT(priv->rasterarea), GWY_DATA_ITEM_QUARK, NULL);

    if (!destroying) {
        // Dont' bother otherwise.
        GwyRasterArea *rasterarea = GWY_RASTER_AREA(priv->rasterarea);
        priv->rasterarea_updating = TRUE;
        gwy_raster_area_set_gradient(rasterarea, NULL);
        gwy_raster_area_set_mask(rasterarea, NULL);
        gwy_raster_area_set_shapes(rasterarea, NULL);
        gwy_raster_area_set_range_from_method(rasterarea, GWY_COLOR_RANGE_FULL);
        gwy_raster_area_set_range_to_method(rasterarea, GWY_COLOR_RANGE_FULL);
        // FIXME: There other things: mask colour, ...
        gwy_raster_area_set_field(rasterarea, NULL);

        // FIXME: More?  Disconnect forward signals?  Emit notification?
        priv->rasterarea_updating = FALSE;
    }

    priv->rasterarea = NULL;
}

static void
update_raster_area_gradient(GwyChannelData *channeldata)
{
    ChannelData *priv = channeldata->priv;
    if (!priv->rasterarea || priv->rasterarea_updating)
        return;

    GwyGradient *grad = NULL;
    if (priv->gradient_name) {
        GwyInventory *gradients = gwy_gradients();
        grad = gwy_inventory_get(gradients, priv->gradient_name);
    }
    priv->rasterarea_updating = TRUE;
    gwy_raster_area_set_gradient(priv->rasterarea, grad);
    priv->rasterarea_updating = FALSE;
}

static void
update_raster_area_property(GwyChannelData *channeldata, guint propid)
{
    ChannelData *priv = channeldata->priv;
    if (!priv->rasterarea || priv->rasterarea_updating)
        return;

    GParamSpec *pspec = properties[propid];
    const gchar *name = pspec->name;
    GValue value = G_VALUE_INIT;
    priv->rasterarea_updating = TRUE;
    g_value_init(&value, pspec->value_type);
    g_printerr("%s %s\n", name, G_PARAM_SPEC_TYPE_NAME(pspec));
    g_object_get_property(G_OBJECT(channeldata), name, &value);
    g_object_set_property(G_OBJECT(priv->rasterarea), name, &value);
    g_value_unset(&value);
    priv->rasterarea_updating = FALSE;
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
