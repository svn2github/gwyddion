/*
 *  @(#) $Id$
 *  Copyright (C) 2006 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#ifndef __GWY_SELECTION_GRAPH_POINT_H__
#define __GWY_SELECTION_GRAPH_POINT_H__

#include <libdraw/gwyselection.h>

G_BEGIN_DECLS

#define GWY_TYPE_SELECTION_GRAPH_POINT            (gwy_selection_graph_point_get_type())
#define GWY_SELECTION_GRAPH_POINT(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_SELECTION_GRAPH_POINT, GwySelectionGraphPoint))
#define GWY_SELECTION_GRAPH_POINT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_SELECTION_GRAPH_POINT, GwySelectionGraphPointClass))
#define GWY_IS_SELECTION_GRAPH_POINT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_SELECTION_GRAPH_POINT))
#define GWY_IS_SELECTION_GRAPH_POINT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_SELECTION_GRAPH_POINT))
#define GWY_SELECTION_GRAPH_POINT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_SELECTION_GRAPH_POINT, GwySelectionGraphPointClass))

typedef struct _GwySelectionGraphPoint      GwySelectionGraphPoint;
typedef struct _GwySelectionGraphPointClass GwySelectionGraphPointClass;

struct _GwySelectionGraphPoint {
    GwySelection parent_instance;
};

struct _GwySelectionGraphPointClass {
    GwySelectionClass parent_class;
};

GType         gwy_selection_graph_point_get_type(void) G_GNUC_CONST;
GwySelection* gwy_selection_graph_point_new     (void);

G_END_DECLS

#endif /* __GWY_SELECTION_GRAPH_POINT_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
