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

#ifndef __LIBGWYAPP_DATA_ITEM_H__
#define __LIBGWYAPP_DATA_ITEM_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GWY_DATA_ITEM_MAX_ID G_MAXINT

#define GWY_TYPE_DATA_ITEM \
    (gwy_data_item_get_type())
#define GWY_DATA_ITEM(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_DATA_ITEM, GwyDataItem))
#define GWY_DATA_ITEM_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_DATA_ITEM, GwyDataItemClass))
#define GWY_IS_DATA_ITEM(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_DATA_ITEM))
#define GWY_IS_DATA_ITEM_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_DATA_ITEM))
#define GWY_DATA_ITEM_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_DATA_ITEM, GwyDataItemClass))

typedef struct _GwyDataItem      GwyDataItem;
typedef struct _GwyDataItemClass GwyDataItemClass;

G_END_DECLS

#include <libgwyapp/data-list.h>

G_BEGIN_DECLS

struct _GwyDataItem {
    GInitiallyUnowned unowned;
    struct _GwyDataItemPrivate *priv;
};

struct _GwyDataItemClass {
    /*<private>*/
    GInitiallyUnownedClass unowned_class;

    /*<public>*/
    GObject*     (*get_data)        (const GwyDataItem *dataitem);
    const gchar* (*get_name)        (const GwyDataItem *dataitem);
    void         (*set_name)        (GwyDataItem *dataitem,
                                     const gchar *name);
    GdkPixbuf*   (*render_thumbnail)(GwyDataItem *dataitem,
                                     guint maxsize);
};

GType        gwy_data_item_get_type            (void)                        G_GNUC_CONST;
guint        gwy_data_item_get_id              (const GwyDataItem *dataitem) G_GNUC_PURE;
GwyDataList* gwy_data_item_get_data_list       (const GwyDataItem *dataitem) G_GNUC_PURE;
GObject*     gwy_data_item_get_data            (const GwyDataItem *dataitem) G_GNUC_PURE;
const gchar* gwy_data_item_get_name            (const GwyDataItem *dataitem) G_GNUC_PURE;
void         gwy_data_item_set_name            (GwyDataItem *dataitem,
                                                const gchar *name);
void         gwy_data_item_invalidate_thumbnail(GwyDataItem *dataitem);
void         gwy_data_item_invalidate_info     (GwyDataItem *dataitem);
GdkPixbuf*   gwy_data_item_get_thumbnail       (GwyDataItem *dataitem,
                                                guint maxsize)               G_GNUC_MALLOC;

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
