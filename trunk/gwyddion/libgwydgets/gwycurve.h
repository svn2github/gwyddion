/*
 *  @(#) $Id$
 *  Copyright (C) 2005 Chris Anderson, Molecular Imaging, Corp.
 *  E-mail: sidewinder.asu@gmail.com.
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

/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, 
 * Boston, MA 02110-1301, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

/*
 * Modified by Chris Anderson 2005.
 * GwyCurve is based on GtkCurve (instead of subclassing) since GtkCurve
 * can be subject to removal from Gtk+ at some unspecified point in the
 * future.
 */

#ifndef __GWY_CURVE_H__
#define __GWY_CURVE_H__

#include <gdk/gdk.h>
#include <gtk/gtkdrawingarea.h>

#include <libgwydgets/gwydgetenums.h>
#include <libdraw/gwyrgba.h>

G_BEGIN_DECLS

#define GWY_TYPE_CURVE                 (gwy_curve_get_type ())
#define GWY_CURVE(obj) \
            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GWY_TYPE_CURVE, GwyCurve))
#define GWY_CURVE_CLASS(klass) \
            (G_TYPE_CHECK_CLASS_CAST ((klass), GWY_TYPE_CURVE, GwyCurveClass))
#define GWY_IS_CURVE(obj) \
            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GWY_TYPE_CURVE))
#define GWY_IS_CURVE_CLASS(klass) \
            (G_TYPE_CHECK_CLASS_TYPE ((klass), GWY_TYPE_CURVE))
#define GWY_CURVE_GET_CLASS(obj) \
            (G_TYPE_INSTANCE_GET_CLASS ((obj), GWY_TYPE_CURVE, GwyCurveClass))

typedef struct _GwyCurve        GwyCurve;
typedef struct _GwyCurveClass   GwyCurveClass;

typedef struct {
    gdouble x;
    gdouble y;
} GwyPoint;

typedef struct {
    /* curve points: */
    gint num_points;
    GwyPoint *points;

    /* control points: */
    gint num_ctlpoints;
    GwyPoint *ctlpoints;

    GwyRGBA color;
} GwyChannelData;

struct _GwyCurve {
    GtkDrawingArea graph;

    gint cursor_type;
    gdouble min_x;
    gdouble max_x;
    gdouble min_y;
    gdouble max_y;
    gboolean snap;
    GdkPixmap *pixmap;
    GwyCurveType curve_type;
    gint height;                  /* (cached) graph height in pixels */
    gint grab_point;              /* point currently grabbed */
    gint grab_channel;            /* channel of grabbed point */
    gint last;

    /* curve point and control point data
       (3 color channels: red, green, blue) */
    gint num_channels;
    GwyChannelData *channel_data;
};

struct _GwyCurveClass {
    GtkDrawingAreaClass parent_class;

    /* Signals */
    void (*curve_type_changed)(GwyCurve *curve);
    void (*curve_edited)(GwyCurve *curve);

    /* Padding for future expansion */
    void (*_gwy_reserved1) (void);
    void (*_gwy_reserved2) (void);
    void (*_gwy_reserved3) (void);
    void (*_gwy_reserved4) (void);
};

GtkWidget*  gwy_curve_new                   (void);
GType       gwy_curve_get_type              (void) G_GNUC_CONST;
void        gwy_curve_reset                 (GwyCurve *curve);
void        gwy_curve_set_range             (GwyCurve *curve,
                                             gdouble min_x, gdouble max_x,
                                             gdouble min_y, gdouble max_y);
void        gwy_curve_set_curve_type        (GwyCurve *curve,
                                             GwyCurveType type);
void        gwy_curve_set_channels          (GwyCurve *curve,
                                             gint num_channels,
                                             GwyRGBA *colors);

void        gwy_curve_set_control_points    (GwyCurve *curve,
                                             GwyChannelData *channel_data,
                                             gboolean prune);
void        gwy_curve_get_control_points    (GwyCurve *curve,
                                             GwyChannelData *channel_data,
                                             gboolean triplets);

G_END_DECLS

#endif /* __GWY_CURVE_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
