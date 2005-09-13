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

#include "config.h"
#include <string.h>

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwymodule/gwymodule.h>

#include "layer.h"

#define GWY_TYPE_LAYER_RECTANGLE            (gwy_layer_rectangle_get_type())
#define GWY_LAYER_RECTANGLE(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_LAYER_RECTANGLE, GwyLayerRectangle))
#define GWY_IS_LAYER_RECTANGLE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_LAYER_RECTANGLE))
#define GWY_LAYER_RECTANGLE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_LAYER_RECTANGLE, GwyLayerRectangleClass))

#define GWY_TYPE_SELECTION_RECTANGLE            (gwy_selection_rectangle_get_type())
#define GWY_SELECTION_RECTANGLE(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_SELECTION_RECTANGLE, GwySelectionRectangle))
#define GWY_IS_SELECTION_RECTANGLE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_SELECTION_RECTANGLE))
#define GWY_SELECTION_RECTANGLE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_SELECTION_RECTANGLE, GwySelectionRectangleClass))

enum {
    OBJECT_SIZE = 4
};

enum {
    PROP_0,
    PROP_IS_CROP
};

typedef struct _GwyLayerRectangle          GwyLayerRectangle;
typedef struct _GwyLayerRectangleClass     GwyLayerRectangleClass;
typedef struct _GwySelectionRectangle      GwySelectionRectangle;
typedef struct _GwySelectionRectangleClass GwySelectionRectangleClass;

struct _GwyLayerRectangle {
    GwyVectorLayer parent_instance;

    /* Properties */
    gboolean is_crop;

    /* Dynamic state */
    gboolean square;
};

struct _GwyLayerRectangleClass {
    GwyVectorLayerClass parent_class;

    GdkCursor *corner_cursor[4];
    GdkCursor *resize_cursor;
};

struct _GwySelectionRectangle {
    GwySelection parent_instance;
};

struct _GwySelectionRectangleClass {
    GwySelectionClass parent_class;
};

static gboolean module_register                    (const gchar *name);
static GType    gwy_layer_rectangle_get_type       (void) G_GNUC_CONST;
static GType    gwy_selection_rectangle_get_type   (void) G_GNUC_CONST;
static void     gwy_layer_rectangle_set_property   (GObject *object,
                                                    guint prop_id,
                                                    const GValue *value,
                                                    GParamSpec *pspec);
static void     gwy_layer_rectangle_get_property   (GObject*object,
                                                    guint prop_id,
                                                    GValue *value,
                                                    GParamSpec *pspec);
static void     gwy_layer_rectangle_draw           (GwyVectorLayer *layer,
                                                    GdkDrawable *drawable);
static void     gwy_layer_rectangle_draw_object    (GwyVectorLayer *layer,
                                                    GdkDrawable *drawable,
                                                    gint i);
static gboolean gwy_layer_rectangle_motion_notify  (GwyVectorLayer *layer,
                                                    GdkEventMotion *event);
static gboolean gwy_layer_rectangle_button_pressed (GwyVectorLayer *layer,
                                                    GdkEventButton *event);
static gboolean gwy_layer_rectangle_button_released(GwyVectorLayer *layer,
                                                    GdkEventButton *event);
static void     gwy_layer_rectangle_set_is_crop    (GwyLayerRectangle *layer,
                                                    gboolean is_crop);
static void     gwy_layer_rectangle_plugged        (GwyDataViewLayer *layer);
static void     gwy_layer_rectangle_unplugged      (GwyDataViewLayer *layer);
static gint     gwy_layer_rectangle_near_point     (GwyVectorLayer *layer,
                                                    gdouble xreal,
                                                    gdouble yreal);
static void     gwy_layer_rectangle_squarize       (GwyDataView *data_view,
                                                    gint x, gint y,
                                                    gdouble *xy);

/* Allow to express intent. */
#define gwy_layer_rectangle_undraw         gwy_layer_rectangle_draw
#define gwy_layer_rectangle_undraw_object  gwy_layer_rectangle_draw_object

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Layer allowing selection of rectangular areas."),
    "Yeti <yeti@gwyddion.net>",
    "2.1",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

GWY_MODULE_QUERY(module_info)

G_DEFINE_TYPE(GwySelectionRectangle, gwy_selection_rectangle,
              GWY_TYPE_SELECTION)
G_DEFINE_TYPE(GwyLayerRectangle, gwy_layer_rectangle,
              GWY_TYPE_VECTOR_LAYER)

static gboolean
module_register(const gchar *name)
{
    GwyLayerFuncInfo func_info = {
        "rectangle",
        0,
    };

    func_info.type = GWY_TYPE_LAYER_RECTANGLE;
    gwy_layer_func_register(name, &func_info);

    return TRUE;
}

static void
gwy_selection_rectangle_class_init(GwySelectionRectangleClass *klass)
{
    GwySelectionClass *sel_class = GWY_SELECTION_CLASS(klass);

    sel_class->object_size = OBJECT_SIZE;
}

static void
gwy_layer_rectangle_class_init(GwyLayerRectangleClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GwyDataViewLayerClass *layer_class = GWY_DATA_VIEW_LAYER_CLASS(klass);
    GwyVectorLayerClass *vector_class = GWY_VECTOR_LAYER_CLASS(klass);

    gwy_debug("");

    gobject_class->set_property = gwy_layer_rectangle_set_property;
    gobject_class->get_property = gwy_layer_rectangle_get_property;

    layer_class->plugged = gwy_layer_rectangle_plugged;
    layer_class->unplugged = gwy_layer_rectangle_unplugged;

    vector_class->selection_type = GWY_TYPE_SELECTION_RECTANGLE;
    vector_class->draw = gwy_layer_rectangle_draw;
    vector_class->motion_notify = gwy_layer_rectangle_motion_notify;
    vector_class->button_press = gwy_layer_rectangle_button_pressed;
    vector_class->button_release = gwy_layer_rectangle_button_released;

    g_object_class_install_property(
        gobject_class,
        PROP_IS_CROP,
        g_param_spec_boolean("is-crop",
                             "Crop style",
                             "Whether the selection is drawn crop-style with "
                             "lines from border to border instead of plain "
                             "rectangle",
                             FALSE,
                             G_PARAM_READWRITE));
}

static void
gwy_selection_rectangle_init(GwySelectionRectangle *selection)
{
    g_array_set_size(GWY_SELECTION(selection)->objects, OBJECT_SIZE);
}

static void
gwy_layer_rectangle_init(G_GNUC_UNUSED GwyLayerRectangle *layer)
{
}

static void
gwy_layer_rectangle_set_property(GObject *object,
                                 guint prop_id,
                                 const GValue *value,
                                 GParamSpec *pspec)
{
    GwyLayerRectangle *layer = GWY_LAYER_RECTANGLE(object);

    switch (prop_id) {
        case PROP_IS_CROP:
        gwy_layer_rectangle_set_is_crop(layer, g_value_get_boolean(value));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_layer_rectangle_get_property(GObject*object,
                                 guint prop_id,
                                 GValue *value,
                                 GParamSpec *pspec)
{
    GwyLayerRectangle *layer = GWY_LAYER_RECTANGLE(object);

    switch (prop_id) {
        case PROP_IS_CROP:
        g_value_set_boolean(value, layer->is_crop);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_layer_rectangle_draw(GwyVectorLayer *layer,
                         GdkDrawable *drawable)
{
    gint i, n;

    g_return_if_fail(GWY_IS_LAYER_RECTANGLE(layer));
    g_return_if_fail(GDK_IS_DRAWABLE(drawable));

    n = gwy_selection_get_data(layer->selection, NULL);
    for (i = 0; i < n; i++)
        gwy_layer_rectangle_draw_object(layer, drawable, i);
}

static void
gwy_layer_rectangle_draw_object(GwyVectorLayer *layer,
                                GdkDrawable *drawable,
                                gint i)
{
    GwyDataView *data_view;
    gint xmin, ymin, xmax, ymax;
    gdouble xy[OBJECT_SIZE];
    gboolean has_object;

    g_return_if_fail(GDK_IS_DRAWABLE(drawable));
    data_view = GWY_DATA_VIEW(GWY_DATA_VIEW_LAYER(layer)->parent);

    has_object = gwy_selection_get_object(layer->selection, i, xy);
    g_return_if_fail(has_object);

    gwy_vector_layer_setup_gc(layer);
    gwy_data_view_coords_real_to_xy(data_view, xy[0], xy[1], &xmin, &ymin);
    gwy_data_view_coords_real_to_xy(data_view, xy[2], xy[3], &xmax, &ymax);
    if (xmax < xmin)
        GWY_SWAP(gint, xmin, xmax);
    if (ymax < ymin)
        GWY_SWAP(gint, ymin, ymax);

    if (GWY_LAYER_RECTANGLE(layer)->is_crop) {
        gint width, height;

        gdk_drawable_get_size(drawable, &width, &height);
        gdk_draw_line(drawable, layer->gc, 0, ymin, width, ymin);
        if (ymin != ymax)
            gdk_draw_line(drawable, layer->gc, 0, ymax, width, ymax);
        gdk_draw_line(drawable, layer->gc, xmin, 0, xmin, height);
        if (xmin != xmax)
            gdk_draw_line(drawable, layer->gc, xmax, 0, xmax, height);
    }
    else
        gdk_draw_rectangle(drawable, layer->gc, FALSE,
                           xmin, ymin, xmax - xmin, ymax - ymin);
}

static gboolean
gwy_layer_rectangle_motion_notify(GwyVectorLayer *layer,
                                  GdkEventMotion *event)
{
    GwyDataView *data_view;
    GwyLayerRectangleClass *klass;
    GdkWindow *window;
    gint x, y, i;
    gdouble xreal, yreal, xy[OBJECT_SIZE];
    gboolean square;

    data_view = GWY_DATA_VIEW(GWY_DATA_VIEW_LAYER(layer)->parent);
    window = GTK_WIDGET(data_view)->window;

    i = layer->selecting;

    if (event->is_hint)
        gdk_window_get_pointer(window, &x, &y, NULL);
    else {
        x = event->x;
        y = event->y;
    }
    square = event->state & GDK_SHIFT_MASK;
    gwy_debug("x = %d, y = %d", x, y);
    gwy_data_view_coords_xy_clamp(data_view, &x, &y);
    gwy_data_view_coords_xy_to_real(data_view, x, y, &xreal, &yreal);
    if (i > -1)
        gwy_selection_get_object(layer->selection, i, xy);
    if (i > -1 && xreal == xy[2] && yreal == xy[3])
        return FALSE;

    if (!layer->button) {
        i = gwy_layer_rectangle_near_point(layer, xreal, yreal);
        if (i > 0)
            i = i % OBJECT_SIZE;
        klass = GWY_LAYER_RECTANGLE_GET_CLASS(layer);
        gdk_window_set_cursor(window, i == -1 ? NULL : klass->corner_cursor[i]);
        return FALSE;
    }

    g_assert(layer->selecting != -1);
    GWY_LAYER_RECTANGLE(layer)->square = square;
    gwy_layer_rectangle_undraw_object(layer, window, i);
    if (square)
        gwy_layer_rectangle_squarize(data_view, x, y, xy);
    else {
        xy[2] = xreal;
        xy[3] = yreal;
    }
    gwy_selection_set_object(layer->selection, i, xy);
    gwy_layer_rectangle_draw_object(layer, window, i);

    return FALSE;
}

static gboolean
gwy_layer_rectangle_button_pressed(GwyVectorLayer *layer,
                                   GdkEventButton *event)
{
    GwyDataView *data_view;
    GdkWindow *window;
    GwyLayerRectangleClass *klass;
    gint x, y, i;
    gdouble xreal, yreal, xy[OBJECT_SIZE];
    gboolean square;

    gwy_debug("");
    if (event->button != 1)
        return FALSE;

    data_view = GWY_DATA_VIEW(GWY_DATA_VIEW_LAYER(layer)->parent);
    window = GTK_WIDGET(data_view)->window;

    x = event->x;
    y = event->y;
    square = event->state & GDK_SHIFT_MASK;
    gwy_data_view_coords_xy_clamp(data_view, &x, &y);
    gwy_debug("[%d,%d]", x, y);
    /* do nothing when we are outside */
    if (x != event->x || y != event->y)
        return FALSE;

    gwy_data_view_coords_xy_to_real(data_view, x, y, &xreal, &yreal);

    /* handle existing selection */
    i = gwy_layer_rectangle_near_point(layer, xreal, yreal);
    if (i >= 0) {
        layer->selecting = i/4;
        gwy_layer_rectangle_undraw_object(layer, window, layer->selecting);

        gwy_selection_get_object(layer->selection, layer->selecting, xy);
        if (i/2)
            xy[0] = MIN(xy[0], xy[2]);
        else
            xy[0] = MAX(xy[0], xy[2]);

        if (i%2)
            xy[1] = MIN(xy[1], xy[3]);
        else
            xy[1] = MAX(xy[1], xy[3]);

        if (square)
            gwy_layer_rectangle_squarize(data_view, x, y, xy);
        else {
            xy[2] = xreal;
            xy[3] = yreal;
        }
        gwy_selection_set_object(layer->selection, layer->selecting, xy);
    }
    else {
        xy[2] = xy[0] = xreal;
        xy[3] = xy[1] = yreal;

        /* add an object, or do nothing when maximum is reached */
        i = -1;
        if (gwy_selection_is_full(layer->selection)) {
            if (gwy_selection_get_max_objects(layer->selection) > 1)
                return FALSE;
            i = 0;
            gwy_layer_rectangle_undraw_object(layer, window, i);
        }
        layer->selecting = 0;    /* avoid "update" signal emission */
        layer->selecting = gwy_selection_set_object(layer->selection, i, xy);
    }
    GWY_LAYER_RECTANGLE(layer)->square = square;
    layer->button = event->button;
    gwy_layer_rectangle_draw(layer, window);

    klass = GWY_LAYER_RECTANGLE_GET_CLASS(layer);
    gdk_window_set_cursor(window, klass->resize_cursor);

    return FALSE;
}

static gboolean
gwy_layer_rectangle_button_released(GwyVectorLayer *layer,
                                    GdkEventButton *event)
{
    GwyDataView *data_view;
    GdkWindow *window;
    GwyLayerRectangleClass *klass;
    gint x, y, i;
    gdouble xreal, yreal, xy[OBJECT_SIZE];
    gboolean outside, square;

    if (!layer->button)
        return FALSE;
    data_view = GWY_DATA_VIEW(GWY_DATA_VIEW_LAYER(layer)->parent);
    window = GTK_WIDGET(data_view)->window;

    layer->button = 0;
    x = event->x;
    y = event->y;
    square = event->state & GDK_SHIFT_MASK;
    i = layer->selecting;
    gwy_debug("i = %d", i);
    gwy_data_view_coords_xy_clamp(data_view, &x, &y);
    outside = (event->x != x) || (event->y != y);
    gwy_data_view_coords_xy_to_real(data_view, x, y, &xreal, &yreal);
    gwy_layer_rectangle_undraw_object(layer, window, i);
    gwy_selection_get_object(layer->selection, i, xy);
    gwy_data_view_coords_real_to_xy(data_view, xy[0], xy[1], &x, &y);
    gwy_debug("event: [%f, %f], xy: [%d, %d]", event->x, event->y, x, y);
    if (x == event->x || y == event->y) {
        gwy_selection_delete_object(layer->selection, i);
        gwy_selection_finished(layer->selection);
    }
    else {
        if (square)
            gwy_layer_rectangle_squarize(data_view, x, y, xy);
        else {
            xy[2] = xreal;
            xy[3] = yreal;
        }

        if (xy[2] < xy[0])
            GWY_SWAP(gdouble, xy[0], xy[2]);
        if (xy[3] < xy[1])
            GWY_SWAP(gdouble, xy[1], xy[3]);

        gwy_selection_set_object(layer->selection, i, xy);
        gwy_layer_rectangle_draw_object(layer, window, i);
        gwy_selection_finished(layer->selection);
    }

    layer->selecting = -1;
    GWY_LAYER_RECTANGLE(layer)->square = square;
    klass = GWY_LAYER_RECTANGLE_GET_CLASS(layer);
    i = gwy_layer_rectangle_near_point(layer, xreal, yreal);
    if (i > 0)
        i = i % OBJECT_SIZE;
    outside = outside || (i == -1);
    gdk_window_set_cursor(window, outside ? NULL : klass->corner_cursor[i]);

    return FALSE;
}

static void
gwy_layer_rectangle_set_is_crop(GwyLayerRectangle *layer,
                                gboolean is_crop)
{
    GwyVectorLayer *vector_layer;
    GtkWidget *parent;

    g_return_if_fail(GWY_IS_LAYER_RECTANGLE(layer));
    vector_layer = GWY_VECTOR_LAYER(layer);
    parent = GWY_DATA_VIEW_LAYER(layer)->parent;

    if (is_crop == layer->is_crop)
        return;

    if (parent)
        gwy_layer_rectangle_undraw(vector_layer, parent->window);
    layer->is_crop = is_crop;
    if (parent)
        gwy_layer_rectangle_draw(vector_layer, parent->window);
    g_object_notify(G_OBJECT(layer), "is-crop");
}

static void
gwy_layer_rectangle_plugged(GwyDataViewLayer *layer)
{
    GwyLayerRectangleClass *klass;

    gwy_debug("");
    GWY_DATA_VIEW_LAYER_CLASS(gwy_layer_rectangle_parent_class)->plugged(layer);

    klass = GWY_LAYER_RECTANGLE_GET_CLASS(layer);
    gwy_gdk_cursor_new_or_ref(&klass->resize_cursor, GDK_CROSS);
    gwy_gdk_cursor_new_or_ref(&klass->corner_cursor[0], GDK_UL_ANGLE);
    gwy_gdk_cursor_new_or_ref(&klass->corner_cursor[1], GDK_LL_ANGLE);
    gwy_gdk_cursor_new_or_ref(&klass->corner_cursor[2], GDK_UR_ANGLE);
    gwy_gdk_cursor_new_or_ref(&klass->corner_cursor[3], GDK_LR_ANGLE);
}

static void
gwy_layer_rectangle_unplugged(GwyDataViewLayer *layer)
{
    GwyLayerRectangleClass *klass;
    gint i;

    gwy_debug("");

    klass = GWY_LAYER_RECTANGLE_GET_CLASS(layer);
    gwy_gdk_cursor_free_or_unref(&klass->resize_cursor);
    for (i = 0; i < 4; i++)
        gwy_gdk_cursor_free_or_unref(&klass->corner_cursor[i]);

    GWY_DATA_VIEW_LAYER_CLASS(gwy_layer_rectangle_parent_class)->unplugged(layer);
}

static int
gwy_layer_rectangle_near_point(GwyVectorLayer *layer,
                               gdouble xreal, gdouble yreal)
{
    GwyDataView *view;
    gdouble *coords, d2min, xy[OBJECT_SIZE];
    gint i, n;

    if (!(n = gwy_selection_get_data(layer->selection, NULL)))
        return -1;

    coords = g_newa(gdouble, 8*n);
    for (i = 0; i < n; i++) {
        gwy_selection_get_object(layer->selection, i, xy);
        coords[8*i + 0] = coords[8*i + 2] = xy[0];
        coords[8*i + 1] = coords[8*i + 5] = xy[1];
        coords[8*i + 4] = coords[8*i + 6] = xy[2];
        coords[8*i + 3] = coords[8*i + 7] = xy[3];
    }
    i = gwy_math_find_nearest_point(xreal, yreal, &d2min, 4, coords);

    view = GWY_DATA_VIEW(GWY_DATA_VIEW_LAYER(layer)->parent);
    /* FIXME: this is simply nonsense when x measure != y measure */
    d2min /= gwy_data_view_get_xmeasure(view)*gwy_data_view_get_ymeasure(view);

    if (d2min > PROXIMITY_DISTANCE*PROXIMITY_DISTANCE)
        return -1;
    return i;
}

static void
gwy_layer_rectangle_squarize(GwyDataView *data_view,
                             gint x, gint y,
                             gdouble *xy)
{
    gint size, x0, y0, xx, yy;

    gwy_data_view_coords_real_to_xy(data_view, xy[0], xy[1], &x0, &y0);
    size = MAX(ABS(x - x0), ABS(y - y0));
    x = xx = (x >= x0) ? x0 + size : x0 - size;
    y = yy = (y >= y0) ? y0 + size : y0 - size;
    gwy_data_view_coords_xy_clamp(data_view, &xx, &yy);
    if (xx != x || yy != y) {
        size = MIN(ABS(xx - x0), ABS(yy - y0));
        x = (xx >= x0) ? x0 + size : x0 - size;
        y = (yy >= y0) ? y0 + size : y0 - size;
    }
    gwy_data_view_coords_xy_to_real(data_view, x, y, &xy[2], &xy[3]);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
