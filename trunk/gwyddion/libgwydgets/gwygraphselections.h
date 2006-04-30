/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 */

#ifndef __GWY_GRAPHSEL_H__
#define __GWY_GRAPHSEL_H__

#include <gdk/gdk.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtktable.h>

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


GType    gwy_selection_graph_point_get_type       (void) G_GNUC_CONST;



#define GWY_TYPE_SELECTION_GRAPH_AREA            (gwy_selection_graph_area_get_type())
#define GWY_SELECTION_GRAPH_AREA(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_SELECTION_GRAPH_AREA, GwySelectionGraphArea))
#define GWY_SELECTION_GRAPH_AREA_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_SELECTION_GRAPH_AREA, GwySelectionGraphAreaClass))
#define GWY_IS_SELECTION_GRAPH_AREA(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_SELECTION_GRAPH_AREA))
#define GWY_IS_SELECTION_GRAPH_AREA_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_SELECTION_GRAPH_AREA))
#define GWY_SELECTION_GRAPH_AREA_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_SELECTION_GRAPH_AREA, GwySelectionGraphAreaClass))

typedef struct _GwySelectionGraphArea      GwySelectionGraphArea;
typedef struct _GwySelectionGraphAreaClass GwySelectionGraphAreaClass;


struct _GwySelectionGraphArea {
        GwySelection parent_instance;
};

struct _GwySelectionGraphAreaClass {
        GwySelectionClass parent_class;
};


GType    gwy_selection_graph_area_get_type       (void) G_GNUC_CONST;



#define GWY_TYPE_SELECTION_GRAPH_LINE            (gwy_selection_graph_line_get_type())
#define GWY_SELECTION_GRAPH_LINE(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_SELECTION_GRAPH_LINE, GwySelectionGraphLine))
#define GWY_SELECTION_GRAPH_LINE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_SELECTION_GRAPH_LINE, GwySelectionGraphLineClass))
#define GWY_IS_SELECTION_GRAPH_LINE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_SELECTION_GRAPH_LINE))
#define GWY_IS_SELECTION_GRAPH_LINE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_SELECTION_GRAPH_LINE))
#define GWY_SELECTION_GRAPH_LINE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_SELECTION_GRAPH_LINE, GwySelectionGraphLineClass))

typedef struct _GwySelectionGraphLine      GwySelectionGraphLine;
typedef struct _GwySelectionGraphLineClass GwySelectionGraphLineClass;


struct _GwySelectionGraphLine {
        GwySelection parent_instance;
};

struct _GwySelectionGraphLineClass {
        GwySelectionClass parent_class;
};


GType    gwy_selection_graph_line_get_type       (void) G_GNUC_CONST;


G_END_DECLS

#endif /* __GWY_GRAPHSEL_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
