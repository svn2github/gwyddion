/*
 *  @(#) $Id$
 *  Copyright (C) 2005 David Necas (Yeti), Petr Klapetek.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __GWY_GWYDGET_ENUMS_H__
#define __GWY_GWYDGET_ENUMS_H__

#include <glib.h>
#include <libgwyddion/gwyenum.h>

G_BEGIN_DECLS

typedef enum {
    GWY_AXIS_SCALE_FORMAT_AUTO,
    GWY_AXIS_SCALE_FORMAT_EXP,
    GWY_AXIS_SCALE_FORMAT_INT
} GwyAxisScaleFormat;

typedef enum { /*< lowercase_name=gwy_3d_movement >*/
    GWY_3D_MOVEMENT_NONE = 0,
    GWY_3D_MOVEMENT_ROTATION,
    GWY_3D_MOVEMENT_SCALE,
    GWY_3D_MOVEMENT_DEFORMATION,
    GWY_3D_MOVEMENT_LIGHT
} Gwy3DMovement;

typedef enum { /*< lowercase_name=gwy_3d_projection >*/
    GWY_3D_PROJECTION_ORTHOGRAPHIC = 0,
    GWY_3D_PROJECTION_PERSPECTIVE
} Gwy3DProjection;

typedef enum { /*< lowercase_name=gwy_3d_visualization >*/
    GWY_3D_VISUALIZATION_GRADIENT = 0,
    GWY_3D_VISUALIZATION_LIGHTING,
    GWY_3D_VISUALIZATION_OVERLAY
} Gwy3DVisualization;

typedef enum { /*< lowercase_name=gwy_3d_view_label >*/
    GWY_3D_VIEW_LABEL_X = 0,
    GWY_3D_VIEW_LABEL_Y,
    GWY_3D_VIEW_LABEL_MIN,
    GWY_3D_VIEW_LABEL_MAX,
    GWY_3D_VIEW_NLABELS
} Gwy3DViewLabel;

typedef enum {
    GWY_UNITS_PLACEMENT_NONE,
    GWY_UNITS_PLACEMENT_AT_ZERO
} GwyUnitsPlacement;

/* XXX */
typedef enum {
    GWY_HSCALE_DEFAULT          = 0,
    GWY_HSCALE_LOG              = 1,
    GWY_HSCALE_SQRT             = 2,
    GWY_HSCALE_NO_SCALE         = 6,
    GWY_HSCALE_WIDGET           = 7,
    GWY_HSCALE_WIDGET_NO_EXPAND = 8,
    GWY_HSCALE_CHECK            = 1024
} GwyHScaleStyle;

typedef enum {
    GWY_GRAPH_STATUS_PLAIN,
    GWY_GRAPH_STATUS_XSEL,
    GWY_GRAPH_STATUS_YSEL,
    GWY_GRAPH_STATUS_POINTS,
    GWY_GRAPH_STATUS_ZOOM,
    GWY_GRAPH_STATUS_XLINES,
    GWY_GRAPH_STATUS_YLINES
} GwyGraphStatusType;

typedef enum {
    GWY_GRAPH_GRID_NONE,
    GWY_GRAPH_GRID_AUTO,
    GWY_GRAPH_GRID_USER
} GwyGraphGridType;

typedef enum {
    GWY_GRAPH_POINT_SQUARE                = 0,
    GWY_GRAPH_POINT_CROSS                 = 1,
    GWY_GRAPH_POINT_CIRCLE                = 2,
    GWY_GRAPH_POINT_STAR                  = 3,
    GWY_GRAPH_POINT_TIMES                 = 4,
    GWY_GRAPH_POINT_TRIANGLE_UP           = 5,
    GWY_GRAPH_POINT_TRIANGLE_DOWN         = 6,
    GWY_GRAPH_POINT_DIAMOND               = 7,
    GWY_GRAPH_POINT_FILLED_SQUARE         = 8,
    GWY_GRAPH_POINT_DISC                  = 9,
    GWY_GRAPH_POINT_FILLED_CIRCLE         = GWY_GRAPH_POINT_DISC,
    GWY_GRAPH_POINT_FILLED_TRIANGLE_UP    = 10,
    GWY_GRAPH_POINT_FILLED_TRIANGLE_DOWN  = 11,
    GWY_GRAPH_POINT_FILLED_DIAMOND        = 12,
    GWY_GRAPH_POINT_TRIANGLE_LEFT         = 13,
    GWY_GRAPH_POINT_FILLED_TRIANGLE_LEFT  = 14,
    GWY_GRAPH_POINT_TRIANGLE_RIGHT        = 15,
    GWY_GRAPH_POINT_FILLED_TRIANGLE_RIGHT = 16,
    GWY_GRAPH_POINT_ASTERISK              = 17
} GwyGraphPointType;

typedef enum {
    GWY_GRAPH_CURVE_HIDDEN      = 0,
    GWY_GRAPH_CURVE_POINTS      = 1,
    GWY_GRAPH_CURVE_LINE        = 2,
    GWY_GRAPH_CURVE_LINE_POINTS = 3
} GwyGraphCurveType;

typedef enum {
    GWY_GRAPH_LABEL_NORTHEAST = 0,
    GWY_GRAPH_LABEL_NORTHWEST = 1,
    GWY_GRAPH_LABEL_SOUTHEAST = 2,
    GWY_GRAPH_LABEL_SOUTHWEST = 3,
    GWY_GRAPH_LABEL_USER      = 4
} GwyGraphLabelPosition;

typedef enum {
    GWY_GRAPH_MODEL_EXPORT_ASCII_PLAIN   = 0,
    GWY_GRAPH_MODEL_EXPORT_ASCII_GNUPLOT = 1,
    GWY_GRAPH_MODEL_EXPORT_ASCII_CSV     = 2,
    GWY_GRAPH_MODEL_EXPORT_ASCII_ORIGIN  = 3,
    GWY_GRAPH_MODEL_EXPORT_ASCII_POSIX   = 1024,
} GwyGraphModelExportStyle;

typedef enum {
    GWY_LAYER_BASIC_RANGE_FULL,
    GWY_LAYER_BASIC_RANGE_FIXED,
    GWY_LAYER_BASIC_RANGE_AUTO,
    GWY_LAYER_BASIC_RANGE_ADAPT
} GwyLayerBasicRangeType;

typedef enum {
    GWY_CURVE_TYPE_LINEAR,
    GWY_CURVE_TYPE_SPLINE,
    GWY_CURVE_TYPE_FREE
} GwyCurveType;

typedef enum {
    GWY_CURVE_CHANNEL_RED,
    GWY_CURVE_CHANNEL_GREEN,
    GWY_CURVE_CHANNEL_BLUE
} GwyCurveChannel;

const GwyEnum* gwy_graph_curve_type_get_enum(void) G_GNUC_CONST;

typedef enum {
    GWY_MARKER_OPERATION_MOVE,
    GWY_MARKER_OPERATION_ADD,
    GWY_MARKER_OPERATION_REMOVE
} GwyMarkerOperationType;

typedef enum {
    GWY_DATA_VIEW_LAYER_BASE,
    GWY_DATA_VIEW_LAYER_ALPHA,
    GWY_DATA_VIEW_LAYER_TOP
} GwyDataViewLayerType;

typedef enum {
    GWY_TICKS_STYLE_NONE,
    GWY_TICKS_STYLE_CENTER,
    GWY_TICKS_STYLE_AUTO
} GwyTicksStyle;

G_END_DECLS

#endif /* __GWY_GWYDGET_ENUMS_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
