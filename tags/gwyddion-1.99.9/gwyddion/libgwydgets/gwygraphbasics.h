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

#ifndef __GWY_GRAPH_BASIC_H__
#define __GWY_GRAPH_BASIC_H__

#include <gdk/gdk.h>
#include <libgwydgets/gwydgetenums.h>
#include <libdraw/gwyrgba.h>
#include <libgwydgets/gwygraphselections.h>

G_BEGIN_DECLS

typedef struct {
    gdouble x;
    gdouble y;
} GwyGraphDataPoint;

typedef struct {
    gint xmin;  /*x offset of the active area with respect to drawable left border*/
    gint ymin;  /*y offset of the active area with respect to drawable top border*/
    gint height;       /*active area height*/
    gint width;        /*active area width*/
    gdouble real_xmin; /*real units values*/
    gdouble real_ymin; /*real units values*/
    gdouble real_height; /*real units values*/
    gdouble real_width; /*real units values*/
    gboolean log_x;     /*x axis is logarithmic*/
    gboolean log_y;     /*y axis is logarithmic*/
} GwyGraphActiveAreaSpecs;


void gwy_graph_draw_point           (GdkDrawable *drawable,
                                     GdkGC *gc,
                                     gint x,
                                     gint y,
                                     GwyGraphPointType type,
                                     gint size,
                                     const GwyRGBA *color,
                                     gboolean clear);
void gwy_graph_draw_line            (GdkDrawable *drawable,
                                     GdkGC *gc,
                                     gint x_from,
                                     gint y_from,
                                     gint x_to,
                                     gint y_to,
                                     GdkLineStyle line_style,
                                     gint size,
                                     const GwyRGBA *color);
void gwy_graph_draw_curve           (GdkDrawable *drawable,
                                     GdkGC *gc,
                                     GwyGraphActiveAreaSpecs *specs,
                                     GObject *curvemodel);
void gwy_graph_draw_selection_points(GdkDrawable *drawable,
                                     GdkGC *gc,
                                     GwyGraphActiveAreaSpecs *specs,
                                     GwySelectionGraphPoint *selection);
void gwy_graph_draw_selection_areas (GdkDrawable *drawable,
                                     GdkGC *gc,
                                     GwyGraphActiveAreaSpecs *specs,
                                     GwySelectionGraphArea *selection);
void gwy_graph_draw_selection_lines (GdkDrawable *drawable,
                                     GdkGC *gc,
                                     GwyGraphActiveAreaSpecs *specs,
                                     GwySelectionGraphLine *selection,
                                     GtkOrientation orientation);

void gwy_graph_draw_selection_xareas (GdkDrawable *drawable,
                                     GdkGC *gc,
                                     GwyGraphActiveAreaSpecs *specs,
                                     GwySelectionGraph1DArea *selection);
void gwy_graph_draw_selection_yareas (GdkDrawable *drawable,
                                     GdkGC *gc,
                                     GwyGraphActiveAreaSpecs *specs,
                                     GwySelectionGraph1DArea *selection);

void gwy_graph_draw_grid            (GdkDrawable *drawable,
                                     GdkGC *gc,
                                     GwyGraphActiveAreaSpecs *specs,
                                     GArray *x_grid_data,
                                     GArray *y_grid_data);

const GwyRGBA* gwy_graph_get_preset_color     (guint i) G_GNUC_CONST;
guint          gwy_graph_get_n_preset_colors  (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __GWY_GRAPH_BASIC_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
