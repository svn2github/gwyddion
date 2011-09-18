/*
 *  $Id$
 *  Copyright (C) 2011 David Nečas (Yeti).
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

#ifndef __LIBGWY_COORDS_POINT_H__
#define __LIBGWY_COORDS_POINT_H__

#include <libgwy/coords.h>

G_BEGIN_DECLS

#define GWY_TYPE_COORDS_POINT \
    (gwy_coords_point_get_type())
#define GWY_COORDS_POINT(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_COORDS_POINT, GwyCoordsPoint))
#define GWY_COORDS_POINT_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_COORDS_POINT, GwyCoordsPointClass))
#define GWY_IS_COORDS_POINT(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_COORDS_POINT))
#define GWY_IS_COORDS_POINT_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_COORDS_POINT))
#define GWY_COORDS_POINT_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_COORDS_POINT, GwyCoordsPointClass))

typedef struct _GwyCoordsPoint      GwyCoordsPoint;
typedef struct _GwyCoordsPointClass GwyCoordsPointClass;

struct _GwyCoordsPoint {
    GwyCoords coords;
    struct _GwyCoordsPointPrivate *priv;
};

struct _GwyCoordsPointClass {
    /*<private>*/
    GwyCoordsClass coords_class;
};

#define gwy_coords_point_duplicate(coords_point) \
        (GWY_COORDS_POINT(gwy_serializable_duplicate(GWY_SERIALIZABLE(coords_point))))
#define gwy_coords_point_assign(dest, src) \
        (gwy_serializable_assign(GWY_SERIALIZABLE(dest), GWY_SERIALIZABLE(src)))

GType        gwy_coords_point_get_type  (void)                       G_GNUC_CONST;

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
