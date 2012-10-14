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

#ifndef __LIBGWY_COORDS_LINE_H__
#define __LIBGWY_COORDS_LINE_H__

#include <libgwy/coords.h>

G_BEGIN_DECLS

#define GWY_TYPE_COORDS_LINE \
    (gwy_coords_line_get_type())
#define GWY_COORDS_LINE(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_COORDS_LINE, GwyCoordsLine))
#define GWY_COORDS_LINE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_COORDS_LINE, GwyCoordsLineClass))
#define GWY_IS_COORDS_LINE(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_COORDS_LINE))
#define GWY_IS_COORDS_LINE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_COORDS_LINE))
#define GWY_COORDS_LINE_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_COORDS_LINE, GwyCoordsLineClass))

typedef struct _GwyCoordsLine      GwyCoordsLine;
typedef struct _GwyCoordsLineClass GwyCoordsLineClass;

struct _GwyCoordsLine {
    GwyCoords coords;
    struct _GwyCoordsLinePrivate *priv;
};

struct _GwyCoordsLineClass {
    /*<private>*/
    GwyCoordsClass coords_class;
};

#define gwy_coords_line_duplicate(coordsline) \
        (GWY_COORDS_LINE(gwy_serializable_duplicate(GWY_SERIALIZABLE(coordsline))))
#define gwy_coords_line_assign(dest, src) \
        (gwy_serializable_assign(GWY_SERIALIZABLE(dest), GWY_SERIALIZABLE(src)))

GType      gwy_coords_line_get_type(void) G_GNUC_CONST;
GwyCoords* gwy_coords_line_new     (void) G_GNUC_MALLOC;

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
