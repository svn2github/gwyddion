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

#ifndef __LIBGWYUI_GRAPH_CURVE_H__
#define __LIBGWYUI_GRAPH_CURVE_H__

#include <gtk/gtk.h>
#include <libgwy/curve.h>
#include <libgwy/line.h>

G_BEGIN_DECLS

typedef enum {
    GWY_PLOT_HIDDEN,
    GWY_PLOT_POINTS,
    GWY_PLOT_LINE,
    GWY_PLOT_LINE_POINTS,
} GwyPlotType;

typedef enum {
    GWY_GRAPH_POINT_CROSS,
    GWY_GRAPH_POINT_TIMES,
    GWY_GRAPH_POINT_STAR,
    GWY_GRAPH_POINT_SQUARE,
    GWY_GRAPH_POINT_CIRCLE,
    GWY_GRAPH_POINT_DIAMOND,
    GWY_GRAPH_POINT_TRIANGLE_UP,
    GWY_GRAPH_POINT_TRIANGLE_DOWN,
    GWY_GRAPH_POINT_TRIANGLE_LEFT,
    GWY_GRAPH_POINT_TRIANGLE_RIGHT,
    GWY_GRAPH_POINT_FILLED_SQUARE,
    GWY_GRAPH_POINT_DISC,
    GWY_GRAPH_POINT_FILLED_CIRCLE = GWY_GRAPH_POINT_DISC,
    GWY_GRAPH_POINT_FILLED_DIAMOND,
    GWY_GRAPH_POINT_FILLED_TRIANGLE_UP,
    GWY_GRAPH_POINT_FILLED_TRIANGLE_DOWN,
    GWY_GRAPH_POINT_FILLED_TRIANGLE_LEFT,
    GWY_GRAPH_POINT_FILLED_TRIANGLE_RIGHT,
    GWY_GRAPH_POINT_ASTERISK,
} GwyGraphPointType;

typedef enum {
    GWY_GRAPH_LINE_SOLID,
    GWY_GRAPH_LINE_DASHED,
    GWY_GRAPH_LINE_DOTTED,
} GwyGraphLineType;

#define GWY_TYPE_GRAPH_CURVE \
    (gwy_graph_curve_get_type())
#define GWY_GRAPH_CURVE(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_GRAPH_CURVE, GwyGraphCurve))
#define GWY_GRAPH_CURVE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_GRAPH_CURVE, GwyGraphCurveClass))
#define GWY_IS_GRAPH_CURVE(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_GRAPH_CURVE))
#define GWY_IS_GRAPH_CURVE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_GRAPH_CURVE))
#define GWY_GRAPH_CURVE_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_GRAPH_CURVE, GwyGraphCurveClass))

typedef struct _GwyGraphCurve      GwyGraphCurve;
typedef struct _GwyGraphCurveClass GwyGraphCurveClass;

#include <libgwyui/graph-area.h>

struct _GwyGraphCurve {
    GInitiallyUnowned unowned;
    struct _GwyGraphCurvePrivate *priv;
};

struct _GwyGraphCurveClass {
    /*<private>*/
    GInitiallyUnownedClass unowned_class;
    void (*reserved1)(void);
    void (*reserved2)(void);
    /*<public>*/
};

GType          gwy_graph_curve_get_type (void)                            G_GNUC_CONST;
GwyGraphCurve* gwy_graph_curve_new      (void);
GwyCurve*      gwy_graph_curve_get_curve(const GwyGraphCurve *graphcurve) G_GNUC_PURE;
void           gwy_graph_curve_set_curve(GwyGraphCurve *graphcurve,
                                         GwyCurve *curve);
GwyLine*       gwy_graph_curve_get_line (const GwyGraphCurve *graphcurve) G_GNUC_PURE;
void           gwy_graph_curve_set_line (GwyGraphCurve *graphcurve,
                                         GwyLine *line);
gboolean       gwy_graph_curve_xrange   (const GwyGraphCurve *graphcurve,
                                         GwyRange *range);
gboolean       gwy_graph_curve_yrange   (const GwyGraphCurve *graphcurve,
                                         GwyRange *range);
gchar*         gwy_graph_curve_name     (const GwyGraphCurve *graphcurve) G_GNUC_PURE;
void           gwy_graph_curve_xunit    (const GwyGraphCurve *graphcurve,
                                         GwyUnit *unit);
void           gwy_graph_curve_yunit    (const GwyGraphCurve *graphcurve,
                                         GwyUnit *unit);
void           gwy_graph_curve_draw     (const GwyGraphCurve *graphcurve,
                                         cairo_t *cr,
                                         const GwyGraphArea *grapharea);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
