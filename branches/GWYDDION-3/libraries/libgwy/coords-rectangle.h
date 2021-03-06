/*
 *  $Id$
 *  Copyright (C) 2012 David Nečas (Yeti).
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

#ifndef __LIBGWY_COORDS_RECTANGLE_H__
#define __LIBGWY_COORDS_RECTANGLE_H__

#include <libgwy/coords.h>

G_BEGIN_DECLS

#define GWY_TYPE_COORDS_RECTANGLE \
    (gwy_coords_rectangle_get_type())
#define GWY_COORDS_RECTANGLE(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_COORDS_RECTANGLE, GwyCoordsRectangle))
#define GWY_COORDS_RECTANGLE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_COORDS_RECTANGLE, GwyCoordsRectangleClass))
#define GWY_IS_COORDS_RECTANGLE(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_COORDS_RECTANGLE))
#define GWY_IS_COORDS_RECTANGLE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_COORDS_RECTANGLE))
#define GWY_COORDS_RECTANGLE_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_COORDS_RECTANGLE, GwyCoordsRectangleClass))

typedef struct _GwyCoordsRectangle      GwyCoordsRectangle;
typedef struct _GwyCoordsRectangleClass GwyCoordsRectangleClass;

struct _GwyCoordsRectangle {
    GwyCoords coords;
    struct _GwyCoordsRectanglePrivate *priv;
};

struct _GwyCoordsRectangleClass {
    /*<private>*/
    GwyCoordsClass coords_class;
};

#define gwy_coords_rectangle_duplicate(coordsrectangle) \
    (GWY_COORDS_RECTANGLE(gwy_serializable_duplicate(GWY_SERIALIZABLE(coordsrectangle))))
#define gwy_coords_rectangle_assign(dest, src) \
    (gwy_serializable_assign(GWY_SERIALIZABLE(dest), GWY_SERIALIZABLE(src)))

GType      gwy_coords_rectangle_get_type(void) G_GNUC_CONST;
GwyCoords* gwy_coords_rectangle_new     (void) G_GNUC_MALLOC;

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
