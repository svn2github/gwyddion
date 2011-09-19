/*
 *  $Id$
 *  Copyright (C) 2010 David Nečas (Yeti).
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

#ifndef __LIBGWY_COORDS_H__
#define __LIBGWY_COORDS_H__

#include <libgwy/serializable.h>
#include <libgwy/unit.h>
#include <libgwy/array.h>

G_BEGIN_DECLS

#define GWY_TYPE_COORDS \
    (gwy_coords_get_type())
#define GWY_COORDS(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_COORDS, GwyCoords))
#define GWY_COORDS_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_COORDS, GwyCoordsClass))
#define GWY_IS_COORDS(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_COORDS))
#define GWY_IS_COORDS_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_COORDS))
#define GWY_COORDS_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_COORDS, GwyCoordsClass))

typedef struct _GwyCoords      GwyCoords;
typedef struct _GwyCoordsClass GwyCoordsClass;

struct _GwyCoords {
    GwyArray array;
    struct _GwyCoordsPrivate *priv;
};

struct _GwyCoordsClass {
    /*<private>*/
    GwyArrayClass array_class;
    /*<public>*/
    guint shape_size;
    guint dimension;
    const guint *unit_map;
};

typedef gboolean (*GwyCoordsFilterFunc)(GwyCoords *coords,
                                           guint i,
                                           gpointer user_data);

#define gwy_coords_duplicate(coords) \
        (GWY_COORDS(gwy_serializable_duplicate(GWY_SERIALIZABLE(coords))))
#define gwy_coords_assign(dest, src) \
        (gwy_serializable_assign(GWY_SERIALIZABLE(dest), GWY_SERIALIZABLE(src)))

GType        gwy_coords_get_type  (void)                       G_GNUC_CONST;
guint        gwy_coords_shape_size(GwyCoords *coords)          G_GNUC_PURE;
guint        gwy_coords_dimension (GwyCoords *coords)          G_GNUC_PURE;
const guint* gwy_coords_unit_map  (GwyCoords *coords)          G_GNUC_PURE;
void         gwy_coords_clear     (GwyCoords *coords);
gboolean     gwy_coords_get       (GwyCoords *coords,
                                   guint i,
                                   gdouble *data);
void         gwy_coords_set       (GwyCoords *coords,
                                   guint i,
                                   const gdouble *data);
void         gwy_coords_delete    (GwyCoords *coords,
                                   guint i);
guint        gwy_coords_size      (GwyCoords *coords)          G_GNUC_PURE;
void         gwy_coords_get_data  (GwyCoords *coords,
                                   gdouble *data);
void         gwy_coords_set_data  (GwyCoords *coords,
                                   guint n,
                                   const gdouble *data);
void         gwy_coords_filter    (GwyCoords *coords,
                                   GwyCoordsFilterFunc filter,
                                   gpointer user_data);
void         gwy_coords_finished  (GwyCoords *coords);
GwyUnit*     gwy_coords_get_units (GwyCoords *coords,
                                   guint i)                    G_GNUC_PURE;

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */