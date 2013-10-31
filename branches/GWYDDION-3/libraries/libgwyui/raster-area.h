/*
 *  $Id$
 *  Copyright (C) 2011-2012 David Nečas (Yeti).
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

#ifndef __LIBGWYUI_RASTER_AREA_H__
#define __LIBGWYUI_RASTER_AREA_H__

#include <gtk/gtk.h>
#include <libgwy/mask-field.h>
#include <libgwy/field.h>
#include <libgwy/gradient.h>
#include <libgwyui/field-render.h>
#include <libgwyui/shapes.h>

G_BEGIN_DECLS

typedef enum {
    GWY_ZOOM_1_1,
    GWY_ZOOM_IN,
    GWY_ZOOM_OUT,
    GWY_ZOOM_FIT,
} GwyZoomType;

#define GWY_TYPE_RASTER_AREA \
    (gwy_raster_area_get_type())
#define GWY_RASTER_AREA(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_RASTER_AREA, GwyRasterArea))
#define GWY_RASTER_AREA_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_RASTER_AREA, GwyRasterAreaClass))
#define GWY_IS_RASTER_AREA(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_RASTER_AREA))
#define GWY_IS_RASTER_AREA_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_RASTER_AREA))
#define GWY_RASTER_AREA_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_RASTER_AREA, GwyRasterAreaClass))

typedef struct _GwyRasterArea      GwyRasterArea;
typedef struct _GwyRasterAreaClass GwyRasterAreaClass;

struct _GwyRasterArea {
    GtkWidget widget;
    struct _GwyRasterAreaPrivate *priv;
};

struct _GwyRasterAreaClass {
    /*<private>*/
    GtkWidgetClass widget_class;
};

GType             gwy_raster_area_get_type              (void)                            G_GNUC_CONST;
GtkWidget*        gwy_raster_area_new                   (void)                            G_GNUC_MALLOC;
void              gwy_raster_area_set_field             (GwyRasterArea *rasterarea,
                                                         GwyField *field);
GwyField*         gwy_raster_area_get_field             (const GwyRasterArea *rasterarea) G_GNUC_PURE;
void              gwy_raster_area_set_mask              (GwyRasterArea *rasterarea,
                                                         GwyMaskField *mask);
GwyMaskField*     gwy_raster_area_get_mask              (const GwyRasterArea *rasterarea) G_GNUC_PURE;
void              gwy_raster_area_set_gradient          (GwyRasterArea *rasterarea,
                                                         GwyGradient *gradient);
GwyGradient*      gwy_raster_area_get_gradient          (const GwyRasterArea *rasterarea) G_GNUC_PURE;
void              gwy_raster_area_set_range_from_method (GwyRasterArea *rasterarea,
                                                         GwyColorRangeType method);
GwyColorRangeType gwy_raster_area_get_range_from_method (const GwyRasterArea *rasterarea) G_GNUC_PURE;
void              gwy_raster_area_set_range_to_method   (GwyRasterArea *rasterarea,
                                                         GwyColorRangeType method);
GwyColorRangeType gwy_raster_area_get_range_to_method   (const GwyRasterArea *rasterarea) G_GNUC_PURE;
void              gwy_raster_area_set_user_range        (GwyRasterArea *rasterarea,
                                                         const GwyRange *range);
void              gwy_raster_area_get_user_range        (const GwyRasterArea *rasterarea,
                                                         GwyRange *range);
gboolean          gwy_raster_area_get_range             (const GwyRasterArea *rasterarea,
                                                         GwyRange *range);
void              gwy_raster_area_set_mask_color        (GwyRasterArea *rasterarea,
                                                         const GwyRGBA *color);
const GwyRGBA*    gwy_raster_area_get_mask_color        (const GwyRasterArea *rasterarea) G_GNUC_PURE;
void              gwy_raster_area_set_grain_number_color(GwyRasterArea *rasterarea,
                                                         const GwyRGBA *color);
const GwyRGBA*    gwy_raster_area_get_grain_number_color(const GwyRasterArea *rasterarea) G_GNUC_PURE;
void              gwy_raster_area_set_shapes            (GwyRasterArea *rasterarea,
                                                         GwyShapes *shapes);
GwyShapes*        gwy_raster_area_get_shapes            (const GwyRasterArea *rasterarea) G_GNUC_PURE;
void              gwy_raster_area_set_scrollable        (GwyRasterArea *rasterarea,
                                                         gboolean scrollable);
gboolean          gwy_raster_area_get_scrollable        (GwyRasterArea *rasterarea)       G_GNUC_PURE;
void              gwy_raster_area_set_zoomable          (GwyRasterArea *rasterarea,
                                                         gboolean zoomable);
gboolean          gwy_raster_area_get_zoomable          (GwyRasterArea *rasterarea)       G_GNUC_PURE;
void              gwy_raster_area_widget_area           (const GwyRasterArea *rasterarea,
                                                         cairo_rectangle_t *area);
void              gwy_raster_area_field_area            (const GwyRasterArea *rasterarea,
                                                         cairo_rectangle_t *area);
GtkWidget*        gwy_raster_area_get_area_widget       (const GwyRasterArea *rasterarea) G_GNUC_PURE;
void              gwy_raster_area_set_area_widget       (GwyRasterArea *rasterarea,
                                                         GtkWidget *widget);

const cairo_matrix_t* gwy_raster_area_get_widget_to_field_matrix (const GwyRasterArea *rasterarea) G_GNUC_PURE;
const cairo_matrix_t* gwy_raster_area_get_widget_to_coords_matrix(const GwyRasterArea *rasterarea) G_GNUC_PURE;
const cairo_matrix_t* gwy_raster_area_get_field_to_widget_matrix (const GwyRasterArea *rasterarea) G_GNUC_PURE;
const cairo_matrix_t* gwy_raster_area_get_coords_to_widget_matrix(const GwyRasterArea *rasterarea) G_GNUC_PURE;

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
