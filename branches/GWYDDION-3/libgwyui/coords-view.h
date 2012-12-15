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

#ifndef __LIBGWYUI_COORDS_VIEW_H__
#define __LIBGWYUI_COORDS_VIEW_H__

#include <libgwyui/array-store.h>
#include <libgwyui/shapes.h>
#include <libgwyui/raster-view.h>

G_BEGIN_DECLS

#define GWY_TYPE_COORDS_VIEW \
    (gwy_coords_view_get_type())
#define GWY_COORDS_VIEW(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_COORDS_VIEW, GwyCoordsView))
#define GWY_COORDS_VIEW_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_COORDS_VIEW, GwyCoordsViewClass))
#define GWY_IS_COORDS_VIEW(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_COORDS_VIEW))
#define GWY_IS_COORDS_VIEW_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_COORDS_VIEW))
#define GWY_COORDS_VIEW_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_COORDS_VIEW, GwyCoordsViewClass))

typedef struct _GwyCoordsView      GwyCoordsView;
typedef struct _GwyCoordsViewClass GwyCoordsViewClass;

struct _GwyCoordsView {
    GtkTreeView tree_view;
    struct _GwyCoordsViewPrivate *priv;
};

struct _GwyCoordsViewClass {
    /*<private>*/
    GtkTreeViewClass tree_view_class;
    void (*reserved1)(void);
    void (*reserved2)(void);
};

GType              gwy_coords_view_get_type            (void)                      G_GNUC_CONST;
GtkWidget*         gwy_coords_view_new                 (void)                      G_GNUC_MALLOC;
void               gwy_coords_view_set_coords          (GwyCoordsView *view,
                                                        GwyCoords *coords);
GwyCoords*         gwy_coords_view_get_coords          (const GwyCoordsView *view) G_GNUC_PURE;
void               gwy_coords_view_set_shapes          (GwyCoordsView *view,
                                                        GwyShapes *shapes);
GwyShapes*         gwy_coords_view_get_shapes          (const GwyCoordsView *view) G_GNUC_PURE;
void               gwy_coords_view_set_coords_type     (GwyCoordsView *view,
                                                        GType type);
GType              gwy_coords_view_get_coords_type     (const GwyCoordsView *view) G_GNUC_PURE;
void               gwy_coords_view_set_dimension_format(GwyCoordsView *view,
                                                        guint d,
                                                        GwyValueFormat *format);
GwyValueFormat*    gwy_coords_view_get_dimension_format(const GwyCoordsView *view,
                                                        guint d)                   G_GNUC_PURE;
GtkTreeViewColumn* gwy_coords_view_create_column_coord (GwyCoordsView *view,
                                                        guint i)                   G_GNUC_MALLOC;

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
