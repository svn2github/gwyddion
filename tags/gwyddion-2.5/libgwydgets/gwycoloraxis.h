/*
 *  @(#) $Id$
 *  Copyright (C) 2003-2006 David Necas (Yeti), Petr Klapetek.
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

#ifndef __GWY_COLOR_AXIS_H__
#define __GWY_COLOR_AXIS_H__

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtkwidget.h>
#include <libgwyddion/gwysiunit.h>
#include <libdraw/gwygradient.h>
#include <libgwydgets/gwydgetenums.h>

G_BEGIN_DECLS

#define GWY_TYPE_COLOR_AXIS            (gwy_color_axis_get_type())
#define GWY_COLOR_AXIS(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_COLOR_AXIS, GwyColorAxis))
#define GWY_COLOR_AXIS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_COLOR_AXIS, GwyColorAxisClass))
#define GWY_IS_COLOR_AXIS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_COLOR_AXIS))
#define GWY_IS_COLOR_AXIS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_COLOR_AXIS))
#define GWY_COLOR_AXIS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_COLOR_AXIS, GwyColorAxisClass))

typedef struct _GwyColorAxis      GwyColorAxis;
typedef struct _GwyColorAxisClass GwyColorAxisClass;

struct _GwyColorAxis {
    GtkWidget widget;

    GtkOrientation orientation;
    GwyTicksStyle ticks_style;
    gboolean labels_visible;

    GwyGradient *gradient;
    gulong gradient_id;

    GdkPixbuf *stripe;
    gint stripe_width;

    gdouble min;
    gdouble max;

    GString *label_text;
    GwySIUnit *siunit;

    gint tick_length;
    gint labelb_size;
    gint labele_size;

    gboolean reserved_bool1;

    gpointer reserved1;
    gpointer reserved2;
};

struct _GwyColorAxisClass {
    GtkWidgetClass parent_class;

    void (*reserved1)(void);
    void (*reserved2)(void);
};


GType         gwy_color_axis_get_type          (void) G_GNUC_CONST;
GtkWidget*    gwy_color_axis_new               (GtkOrientation orientation);
GtkWidget*    gwy_color_axis_new_with_range    (GtkOrientation orientation,
                                                gdouble min,
                                                gdouble max);
void          gwy_color_axis_get_range         (GwyColorAxis *axis,
                                                gdouble *min,
                                                gdouble *max);
void          gwy_color_axis_set_range         (GwyColorAxis *axis,
                                                gdouble min,
                                                gdouble max);
GwySIUnit*    gwy_color_axis_get_si_unit       (GwyColorAxis *axis);
void          gwy_color_axis_set_si_unit       (GwyColorAxis *axis,
                                                GwySIUnit *unit);
void          gwy_color_axis_set_gradient      (GwyColorAxis *axis,
                                                const gchar *gradient);
const gchar*  gwy_color_axis_get_gradient      (GwyColorAxis *axis);
GwyTicksStyle gwy_color_axis_get_ticks_style   (GwyColorAxis *axis);
void          gwy_color_axis_set_ticks_style   (GwyColorAxis *axis,
                                                GwyTicksStyle ticks_style);
gboolean      gwy_color_axis_get_labels_visible(GwyColorAxis *axis);
void          gwy_color_axis_set_labels_visible(GwyColorAxis *axis,
                                                gboolean labels_visible);

G_END_DECLS

#endif /*__GWY_COLOR_AXIS_H__*/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
