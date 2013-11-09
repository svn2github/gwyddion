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

#ifndef __LIBGWYAPP_DATA_H__
#define __LIBGWYAPP_DATA_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define GWY_TYPE_DATA \
    (gwy_data_get_type())
#define GWY_DATA(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_DATA, GwyData))
#define GWY_DATA_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_DATA, GwyDataClass))
#define GWY_IS_DATA(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_DATA))
#define GWY_IS_DATA_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_DATA))
#define GWY_DATA_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_DATA, GwyDataClass))

typedef struct _GwyData      GwyData;
typedef struct _GwyDataClass GwyDataClass;

#include <libgwyapp/data-list.h>

struct _GwyData {
    GObject g_object;
    struct _GwyDataPrivate *priv;
};

struct _GwyDataClass {
    /*<private>*/
    GObjectClass g_object_class;
};

GType        gwy_data_get_type     (void)                G_GNUC_CONST;
guint        gwy_data_get_id       (const GwyData *data) G_GNUC_PURE;
GwyDataList* gwy_data_get_data_list(const GwyData *data) G_GNUC_PURE;

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
