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

#ifndef __LIBGWYUI_SHAPES_LINE_H__
#define __LIBGWYUI_SHAPES_LINE_H__

#include <libgwyui/shapes.h>

G_BEGIN_DECLS

#define GWY_TYPE_SHAPES_LINE \
    (gwy_shapes_line_get_type())
#define GWY_SHAPES_LINE(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_SHAPES_LINE, GwyShapesLine))
#define GWY_SHAPES_LINE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_SHAPES_LINE, GwyShapesLineClass))
#define GWY_IS_SHAPES_LINE(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_SHAPES_LINE))
#define GWY_IS_SHAPES_LINE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_SHAPES_LINE))
#define GWY_SHAPES_LINE_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_SHAPES_LINE, GwyShapesLineClass))

typedef struct _GwyShapesLine      GwyShapesLine;
typedef struct _GwyShapesLineClass GwyShapesLineClass;

struct _GwyShapesLine {
    GwyShapes shapes;
    struct _GwyShapesLinePrivate *priv;
};

struct _GwyShapesLineClass {
    /*<private>*/
    GwyShapesClass shapes_class;
};

GType      gwy_shapes_line_get_type(void) G_GNUC_CONST;
GwyShapes* gwy_shapes_line_new     (void) G_GNUC_MALLOC;

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
