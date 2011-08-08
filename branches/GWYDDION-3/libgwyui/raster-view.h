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

#ifndef __LIBGWYUI_RASTER_VIEW_H__
#define __LIBGWYUI_RASTER_VIEW_H__

#include <gtk/gtk.h>
#include <libgwy/field.h>
#include <libgwy/gradient.h>

G_BEGIN_DECLS

#define GWY_TYPE_RASTER_VIEW \
    (gwy_raster_view_get_type())
#define GWY_RASTER_VIEW(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_RASTER_VIEW, GwyRasterView))
#define GWY_RASTER_VIEW_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_RASTER_VIEW, GwyRasterViewClass))
#define GWY_IS_RASTER_VIEW(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_RASTER_VIEW))
#define GWY_IS_RASTER_VIEW_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_RASTER_VIEW))
#define GWY_RASTER_VIEW_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_RASTER_VIEW, GwyRasterViewClass))

typedef struct _GwyRasterView      GwyRasterView;
typedef struct _GwyRasterViewClass GwyRasterViewClass;

struct _GwyRasterView {
    GtkWidget widget;
    struct _GwyRasterViewPrivate *priv;
};

struct _GwyRasterViewClass {
    /*<private>*/
    GtkWidgetClass widget_class;
};

GType        gwy_raster_view_get_type    (void)                       G_GNUC_CONST;
GtkWidget*   gwy_raster_view_new         (void)                       G_GNUC_MALLOC;
void         gwy_raster_view_set_field   (GwyRasterView *rasterview,
                                          GwyField *field);
GwyField*    gwy_raster_view_get_field   (GwyRasterView *rasterview);
void         gwy_raster_view_set_gradient(GwyRasterView *rasterview,
                                          GwyGradient *gradient);
GwyGradient* gwy_raster_view_get_gradient(GwyRasterView *rasterview);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
