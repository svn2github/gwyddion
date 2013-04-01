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

#ifndef __LIBGWYUI_GRAPH_AREA_H__
#define __LIBGWYUI_GRAPH_AREA_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GWY_TYPE_GRAPH_AREA \
    (gwy_graph_area_get_type())
#define GWY_GRAPH_AREA(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_GRAPH_AREA, GwyGraphArea))
#define GWY_GRAPH_AREA_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_GRAPH_AREA, GwyGraphAreaClass))
#define GWY_IS_GRAPH_AREA(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_GRAPH_AREA))
#define GWY_IS_GRAPH_AREA_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_GRAPH_AREA))
#define GWY_GRAPH_AREA_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_GRAPH_AREA, GwyGraphAreaClass))

typedef struct _GwyGraphArea      GwyGraphArea;
typedef struct _GwyGraphAreaClass GwyGraphAreaClass;

#include <libgwyui/graph-curve.h>

struct _GwyGraphArea {
    GtkWidget unowned;
    struct _GwyGraphAreaPrivate *priv;
};

struct _GwyGraphAreaClass {
    /*<private>*/
    GtkWidgetClass unowned_class;
    void (*reserved1)(void);
    void (*reserved2)(void);
    /*<public>*/
};

GType         gwy_graph_area_get_type(void)  G_GNUC_CONST;
GwyGraphArea* gwy_graph_area_new     (void);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
