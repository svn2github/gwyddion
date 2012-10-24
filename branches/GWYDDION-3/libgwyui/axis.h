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

#ifndef __LIBGWYUI_AXIS_H__
#define __LIBGWYUI_AXIS_H__

#include <gtk/gtk.h>
#include <libgwy/math.h>
#include <libgwy/unit.h>

G_BEGIN_DECLS

typedef enum {
    GWY_AXIS_TICK_EDGE  = 0,
    GWY_AXIS_TICK_MAJOR = 1,
    GWY_AXIS_TICK_MINOR = 2,
    GWY_AXIS_TICK_MICRO = 3,
} GwyAxisTickLevel;

typedef struct {
    gdouble value;
    gdouble position;
    gchar *label;
    PangoRectangle extents;
    GwyAxisTickLevel level;
} GwyAxisTick;

#define GWY_TYPE_AXIS \
    (gwy_axis_get_type())
#define GWY_AXIS(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_AXIS, GwyAxis))
#define GWY_AXIS_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_AXIS, GwyAxisClass))
#define GWY_IS_AXIS(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_AXIS))
#define GWY_IS_AXIS_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_AXIS))
#define GWY_AXIS_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_AXIS, GwyAxisClass))

typedef struct _GwyAxis      GwyAxis;
typedef struct _GwyAxisClass GwyAxisClass;

struct _GwyAxis {
    GtkWidget widget;
    struct _GwyAxisPrivate *priv;
};

struct _GwyAxisClass {
    /*<private>*/
    GtkWidgetClass widget_class;
    /*<public>*/
    gboolean perpendicular_labels;
};

#define GWY_TYPE_AXIS_TICK (gwy_axis_tick_get_type())

GType        gwy_axis_tick_get_type(void)                    G_GNUC_CONST;
GwyAxisTick* gwy_axis_tick_copy    (const GwyAxisTick *tick) G_GNUC_MALLOC;
void         gwy_axis_tick_free    (GwyAxisTick *tick);

GType              gwy_axis_get_type           (void)                     G_GNUC_CONST;
void               gwy_axis_request_range      (GwyAxis *axis,
                                                const GwyRange *request);
void               gwy_axis_get_requested_range(GwyAxis *axis,
                                                GwyRange *range);
void               gwy_axis_get_range          (const GwyAxis *axis,
                                                GwyRange *range);
GwyUnit*           gwy_axis_get_unit           (const GwyAxis *axis)      G_GNUC_PURE;
void               gwy_axis_set_show_labels    (GwyAxis *axis,
                                                gboolean showlabels);
gboolean           gwy_axis_get_show_labels    (const GwyAxis *axis)      G_GNUC_PURE;
void               gwy_axis_set_edge           (GwyAxis *axis,
                                                GtkPositionType edge);
GtkPositionType    gwy_axis_get_edge           (const GwyAxis *axis)      G_GNUC_PURE;
void               gwy_axis_set_snap_to_ticks  (GwyAxis *axis,
                                                gboolean snaptoticks);
gboolean           gwy_axis_get_snap_to_ticks  (const GwyAxis *axis)      G_GNUC_PURE;
PangoLayout*       gwy_axis_get_pango_layout   (GwyAxis *axis);
const GwyAxisTick* gwy_axis_ticks              (GwyAxis *axis,
                                                guint *nticks);
gdouble            gwy_axis_position_to_value  (GwyAxis *axis,
                                                gdouble position)         G_GNUC_PURE;
gdouble            gwy_axis_value_to_position  (GwyAxis *axis,
                                                gdouble value)            G_GNUC_PURE;

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
