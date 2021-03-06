/*
 *  @(#) $Id$
 *  Copyright (C) 2003,2004 David Necas (Yeti), Petr Klapetek.
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

#ifndef __GWY_AXIS_H__
#define __GWY_AXIS_H__

#include <gdk/gdk.h>
#include <gtk/gtkadjustment.h>
#include <gtk/gtkwidget.h>
#include <libgwydgets/gwyaxisdialog.h>
#include <libgwydgets/gwydgetenums.h>
#include <libgwyddion/gwysiunit.h>

G_BEGIN_DECLS

#define GWY_TYPE_AXIS            (gwy_axis_get_type())
#define GWY_AXIS(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_AXIS, GwyAxis))
#define GWY_AXIS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_AXIS, GwyAxisClass))
#define GWY_IS_AXIS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_AXIS))
#define GWY_IS_AXIS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_AXIS))
#define GWY_AXIS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_AXIS, GwyAxisClass))

typedef struct _GwyAxis      GwyAxis;
typedef struct _GwyAxisClass GwyAxisClass;

typedef struct {
    gint xmin;         /*x offset of the active area with respect to drawable left border*/
    gint ymin;         /*y offset of the active area with respect to drawable top border*/
    gint height;       /*active area height*/
    gint width;        /*active area width*/
} GwyAxisActiveAreaSpecs;

typedef struct {
    gdouble value;      /*tick value*/
    gint scrpos;        /*precomputed tick screen position*/
} GwyAxisTick;


typedef struct {
    GwyAxisTick t;
    GString *ttext;
} GwyAxisLabeledTick;

typedef struct {
    gint major_length;
    gint major_thickness;
    gint major_maxticks;
    GwyAxisScaleFormat major_printmode;

    gint minor_length;
    gint minor_thickness;
    gint minor_division;          /*minor division*/

    gint line_thickness;

    PangoFontDescription *major_font;
    PangoFontDescription *label_font;
} GwyAxisParams;

struct _GwyAxis {
    GtkWidget widget;

    GdkGC *gc;
    GwyAxisParams par;

    gboolean is_visible;
    gboolean is_logarithmic;
    gboolean is_auto;           /*affects: tick numbers and label positions.*/
    gboolean is_standalone;
    gboolean has_unit;
    GtkPositionType orientation;    /*north, south, east, west*/

    gdouble reqmin;
    gdouble reqmax;
    gdouble max;                /*axis beginning value*/
    gdouble min;                /*axis end value*/

    GArray *mjticks;            /*array of GwyLabeledTicks*/
    GArray *miticks;            /*array of GwyTicks*/

    gint label_x_pos;           /*label position*/
    gint label_y_pos;
    GString *label_text;

    GwySIUnit *unit;                /*axis unit (if any)*/
    GString *magnification_string;
    gdouble magnification;

    GtkWidget *dialog;      /*axis label and other properties dialog*/

    gboolean enable_label_edit;
    gpointer reserved1;
    gpointer reserved2;
};

struct _GwyAxisClass {
    GtkWidgetClass parent_class;

    void (*label_updated)(GwyAxis *axis);
    void (*rescaled)(GwyAxis *axis);
        
    gpointer reserved1;
    gpointer reserved2;
};


GType       gwy_axis_get_type           (void) G_GNUC_CONST;
GtkWidget*  gwy_axis_new                (gint orientation,
                                         gdouble min,
                                         gdouble max,
                                         const gchar *label);
void        gwy_axis_set_logarithmic    (GwyAxis *axis,
                                         gboolean is_logarithmic);
void        gwy_axis_set_visible        (GwyAxis *axis,
                                         gboolean is_visible);
gboolean    gwy_axis_is_visible         (GwyAxis *axis);
gboolean    gwy_axis_is_logarithmic     (GwyAxis *axis);
void        gwy_axis_set_auto           (GwyAxis *axis,
                                         gboolean is_auto);
void        gwy_axis_set_req            (GwyAxis *axis,
                                         gdouble min,
                                         gdouble max);
void        gwy_axis_set_style          (GwyAxis *axis,
                                         GwyAxisParams style);
gdouble     gwy_axis_get_maximum        (GwyAxis *axis);
gdouble     gwy_axis_get_minimum        (GwyAxis *axis);
gdouble     gwy_axis_get_reqmaximum     (GwyAxis *axis);
gdouble     gwy_axis_get_reqminimum     (GwyAxis *axis);

gdouble     gwy_axis_get_magnification  (GwyAxis *axis);
GString*    gwy_axis_get_magnification_string(GwyAxis *axis);

void        gwy_axis_set_label          (GwyAxis *axis,
                                         GString *label_text);
GString*    gwy_axis_get_label          (GwyAxis *axis);
void        gwy_axis_set_unit           (GwyAxis *axis,
                                           GwySIUnit *unit);
void        gwy_axis_enable_label_edit  (GwyAxis *axis,
                                         gboolean enable);
void        gwy_axis_signal_rescaled   (GwyAxis *axis);

void        gwy_axis_draw_on_drawable  (GdkDrawable *drawable, 
                                          GdkGC *gc, 
                                          gint xmin, gint ymin, gint width, gint height,
                                          GwyAxis *axis);
GString*    gwy_axis_export_vector       (GwyAxis *axis, 
                                          gint xmin, 
                                          gint ymin, 
                                          gint width, 
                                          gint height,
                                          gint fontsize);
void        gwy_axis_set_grid_data      (GwyAxis *axis, GArray *array);

G_END_DECLS

#endif /* __GWY_AXIS_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
