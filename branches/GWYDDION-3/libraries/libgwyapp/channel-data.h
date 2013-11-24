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

#ifndef __LIBGWYAPP_CHANNEL_DATA_H__
#define __LIBGWYAPP_CHANNEL_DATA_H__

#include <libgwyui/raster-area.h>
#include <libgwyapp/data-item.h>

G_BEGIN_DECLS

#define GWY_TYPE_CHANNEL_DATA \
    (gwy_channel_data_get_type())
#define GWY_CHANNEL_DATA(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_CHANNEL_DATA, GwyChannelData))
#define GWY_CHANNEL_DATA_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_CHANNEL_DATA, GwyChannelDataClass))
#define GWY_IS_CHANNEL_DATA(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_CHANNEL_DATA))
#define GWY_IS_CHANNEL_DATA_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_CHANNEL_DATA))
#define GWY_CHANNEL_DATA_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_CHANNEL_DATA, GwyChannelDataClass))

typedef struct _GwyChannelData      GwyChannelData;
typedef struct _GwyChannelDataClass GwyChannelDataClass;

struct _GwyChannelData {
    GwyDataItem data_item;
    struct _GwyChannelDataPrivate *priv;
};

struct _GwyChannelDataClass {
    /*<private>*/
    GwyDataItemClass data_item_class;
};

GType           gwy_channel_data_get_type           (void)                              G_GNUC_CONST;
GwyChannelData* gwy_channel_data_new                (void)                              G_GNUC_MALLOC;
GwyChannelData* gwy_channel_data_new_with_field     (GwyField *field)                   G_GNUC_MALLOC;
void            gwy_channel_data_set_field          (GwyChannelData *channeldata,
                                                     GwyField *field);
GwyField*       gwy_channel_data_get_field          (const GwyChannelData *channeldata) G_GNUC_PURE;
void            gwy_channel_data_set_gradient_name  (GwyChannelData *channeldata,
                                                     const gchar *name);
const gchar*    gwy_channel_data_get_gradient_name  (const GwyChannelData *channeldata) G_GNUC_PURE;
void            gwy_channel_data_set_mask_id        (GwyChannelData *channeldata,
                                                     guint id);
guint           gwy_channel_data_get_mask_id        (const GwyChannelData *channeldata) G_GNUC_PURE;
GtkWidget*      gwy_channel_data_get_raster_area    (const GwyChannelData *channeldata) G_GNUC_PURE;
void            gwy_channel_data_show_in_raster_area(GwyChannelData *channeldata,
                                                     GwyRasterArea *rasterarea);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
