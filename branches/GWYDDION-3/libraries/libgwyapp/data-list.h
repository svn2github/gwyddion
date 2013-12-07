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

#ifndef __LIBGWYAPP_DATA_LIST_H__
#define __LIBGWYAPP_DATA_LIST_H__

#include <glib-object.h>
#include <libgwy/array.h>

G_BEGIN_DECLS

typedef enum {
    GWY_DATA_UNKNOWN = -1,
    GWY_DATA_CHANNEL = 0,
    GWY_DATA_CHANNEL_MASK,
    GWY_DATA_SHAPES_POINT,
    GWY_DATA_SHAPES_LINE,
    GWY_DATA_SHAPES_RECTANGLE,
    GWY_DATA_CURVE,
    GWY_DATA_LINE,
    GWY_DATA_VOLUME,
    GWY_DATA_SURFACE,
} GwyDataKind;

#define GWY_TYPE_DATA_LIST \
    (gwy_data_list_get_type())
#define GWY_DATA_LIST(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_DATA_LIST, GwyDataList))
#define GWY_DATA_LIST_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_DATA_LIST, GwyDataListClass))
#define GWY_IS_DATA_LIST(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_DATA_LIST))
#define GWY_IS_DATA_LIST_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_DATA_LIST))
#define GWY_DATA_LIST_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_DATA_LIST, GwyDataListClass))

typedef struct _GwyDataList      GwyDataList;
typedef struct _GwyDataListClass GwyDataListClass;

G_END_DECLS

#include <libgwy/int-set.h>
#include <libgwyapp/data-item.h>
#include <libgwyapp/file.h>

G_BEGIN_DECLS

struct _GwyDataList {
    GwyArray array;
    struct _GwyDataListPrivate *priv;
};

struct _GwyDataListClass {
    /*<private>*/
    GwyArrayClass array_class;
    /*<public>*/
    void (*remove)(GwyDataList *datalist,
                   guint id);
};

GType        gwy_data_list_get_type     (void)                        G_GNUC_CONST;
GwyDataList* gwy_data_list_new          (void)                        G_GNUC_MALLOC;
GwyDataList* gwy_data_list_new_for_file (const GwyFile *file,
                                         GType type)                  G_GNUC_MALLOC;
void         gwy_data_list_set_data_type(GwyDataList *datalist,
                                         GType type);
GType        gwy_data_list_get_data_type(const GwyDataList *datalist) G_GNUC_PURE;
GwyDataKind  gwy_data_list_get_data_kind(const GwyDataList *datalist) G_GNUC_PURE;
guint*       gwy_data_list_get_ids      (const GwyDataList *datalist,
                                         guint *nids)                 G_GNUC_MALLOC;
GwyIntSet*   gwy_data_list_get_selection(GwyDataList *datalist)       G_GNUC_PURE;
guint        gwy_data_list_add          (GwyDataList *datalist,
                                         GwyDataItem *dataitem);
gboolean     gwy_data_list_add_with_id  (GwyDataList *datalist,
                                         GwyDataItem *dataitem,
                                         guint id);

GType       gwy_data_kind_to_type (GwyDataKind kind) G_GNUC_CONST;
GwyDataKind gwy_data_type_to_kind (GType type)       G_GNUC_CONST;
guint       gwy_data_kinds_n_kinds(void)             G_GNUC_CONST;

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
