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

#include "config.h"
#include <string.h>

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwydebugobjects.h>
#include <libgwyddion/gwymath.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwymodule/gwymodule-layer.h>

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
    PROP_IS_CROP,
    PROP_DRAW_REFLECTION,
    PROP_SNAP_TO_CENTER,
};

typedef struct _GwyLayerRectangle          GwyLayerRectangle;
typedef struct _GwyLayerRectangleClass     GwyLayerRectangleClass;
typedef struct _GwySelectionRectangle      GwySelectionRectangle;
typedef struct _GwySelectionRectangleClass GwySelectionRectangleClass;

struct _GwyLayerRectangle {
    GwyVectorLayer parent_instance;

    GdkCursor *corner_cursor[4];
    GdkCursor *resize_cursor;

    /* Properties */
    gboolean is_crop;
    gboolean draw_reflection;
    gboolean snap;

    /* Dynamic state */
    gboolean square;
};

struct _GwyLayerRectangleClass {
    GwyVectorLayerClass parent_class;
};

struct _GwySelectionRectangle {
    GwySelection parent_instance;
};

struct _GwySelectionRectangleClass {
    GwySelectionClass parent_class;
};

static gboolean module_register                    (void);
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
                                                    GdkDrawable *drawable,
                                                    GwyRenderingTarget target);
static void     gwy_layer_rectangle_draw_object    (GwyVectorLayer *layer,
                                                    GdkDrawable *drawable,
                                                    GwyRenderingTarget target,
                                                    gint i);
static void     gwy_layer_rectangle_draw_rectangle (GwyVectorLayer *layer,
                                                    GwyDataView *data_view,
                                                    GdkDrawable *drawable,
                                                    GwyRenderingTarget target,
                                                    const gdouble *xy);
static gboolean gwy_layer_rectangle_motion_notify  (GwyVectorLayer *layer,
                                                    GdkEventMotion *event);
static gboolean gwy_layer_rectangle_button_pressed (GwyVectorLayer *layer,
                                                    GdkEventButton *event);
static gboolean gwy_layer_rectangle_button_released(GwyVectorLayer *layer,
                                                    GdkEventButton *event);
static void     gwy_layer_rectangle_set_is_crop    (GwyLayerRectangle *layer,
                                                    gboolean is_crop);
static void     gwy_layer_rectangle_set_reflection (GwyLayerRectangle *layer,
                                                    gboolean draw_reflection);
static void     gwy_layer_rectangle_set_snap       (GwyLayerRectangle *layer,
                                                    gboolean snap);
static void     gwy_layer_rectangle_realize        (GwyDataViewLayer *dlayer);
static void     gwy_layer_rectangle_unrealize      (GwyDataViewLayer *dlayer);
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
    "2.7",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

GWY_MODULE_QUERY(module_info)

G_DEFINE_TYPE(GwySelectionRectangle, gwy_selection_rectangle,
              GWY_TYPE_SELECTION)
G_DEFINE_TYPE(GwyLayerRectangle, gwy_layer_rectangle,
              GWY_TYPE_VECTOR_LAYER)

static gboolean
module_register(void)
{
    gwy_layer_func_register(GWY_TYPE_LAYER_RECTANGLE);
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

    layer_class->realize = gwy_layer_rectangle_realize;
    layer_class->unrealize = gwy_layer_rectangle_unrealize;

    vector_class->selection_type = GWY_TYPE_SELECTION_RECTANGLE;
    vector_class->draw = gwy_layer_rectangle_draw;
    vector_class->motion_notify = gwy_layer_rectangle_motion_notify;
    vector_class->button_press = gwy_layer_rectangle_button_pressed;
    vector_class->button_release = gwy_layer_rectangle_button_released;

    g_object_class_install_property
        (gobject_class,
         PROP_IS_CROP,
         g_param_spec_boolean("is-crop",
                              "Crop style",
                              "Whether the selection is drawn crop-style with "
                              "lines from border to border instead of plain "
                              "rectangle",
                              FALSE,
                              G_PARAM_READWRITE));

    g_object_class_install_property
        (gobject_class,
         PROP_DRAW_REFLECTION,
         g_param_spec_boolean("draw-reflection",
                              "Draw reflection",
                              "Whether central reflection of selection should "
                              "be drawn too",
                              FALSE,
                              G_PARAM_READWRITE));

    g_object_class_install_property
        (gobject_class,
         PROP_SNAP_TO_CENTER,
         g_param_spec_boolean("snap-to-center",
                              "Snap to Center",
                              "Whether the selection should snap to the "
                              "center.",
                              FALSE,
                              G_PARAM_READWRITE));
}

static void
gwy_selection_rectangle_init(GwySelectionRectangle *selection)
{
    /* Set max. number of objects to one */
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

        case PROP_DRAW_REFLECTION:
        gwy_layer_rectangle_set_reflection(layer, g_value_get_boolean(value));
        break;

        case PROP_SNAP_TO_CENTER:
        gwy_layer_rectangle_set_snap(layer, g_value_get_boolean(value));
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

        case PROP_DRAW_REFLECTION:
        g_value_set_boolean(value, layer->draw_reflection);
        break;

        case PROP_SNAP_TO_CENTER:
        g_value_set_boolean(value, layer->snap);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_layer_rectangle_draw(GwyVectorLayer *layer,
                         GdkDrawable *drawable,
                         GwyRenderingTarget target)
{
    gint i, n;

    if (!layer->selection)
        return;

    g_return_if_fail(GDK_IS_DRAWABLE(drawable));

    n = gwy_selection_get_data(layer->selection, NULL);
    for (i = 0; i < n; i++)
        gwy_layer_rectangle_draw_object(layer, drawable, target, i);
}

static void
gwy_layer_rectangle_draw_object(GwyVectorLayer *layer,
                                GdkDrawable *drawable,
                                GwyRenderingTarget target,
                                gint i)
{
    GwyDataView *data_view;
    gdouble xy[OBJECT_SIZE];
    gdouble xreal, yreal;
    gboolean has_object;
    gint xy_pixel[OBJECT_SIZE], j;

    g_return_if_fail(GDK_IS_DRAWABLE(drawable));
    data_view = GWY_DATA_VIEW(GWY_DATA_VIEW_LAYER(layer)->parent);

    has_object = gwy_selection_get_object(layer->selection, i, xy);
    g_return_if_fail(has_object);

    gwy_layer_rectangle_draw_rectangle(layer, data_view, drawable, target, xy);
    if (GWY_LAYER_RECTANGLE(layer)->draw_reflection) {
        gwy_data_view_get_real_data_sizes(data_view, &xreal, &yreal);
        xy[0] = xreal - xy[0];
        xy[1] = yreal - xy[1];
        xy[2] = xreal - xy[2];
        xy[3] = yreal - xy[3];

        /* XXX - this should be controlled by a property */
        if (TRUE) {
            /* shift down/right one pixel */
            gwy_data_view_coords_real_to_xy(data_view, xy[0], xy[1],
                                            &xy_pixel[0], &xy_pixel[1]);
            gwy_data_view_coords_real_to_xy(data_view, xy[2], xy[3],
                                            &xy_pixel[2], &xy_pixel[3]);
            for (j = 0; j < OBJECT_SIZE; j++)
                xy_pixel[j]++;
            gwy_data_view_coords_xy_to_real(data_view, xy_pixel[0], xy_pixel[1],
                                            &xy[0], &xy[1]);
            gwy_data_view_coords_xy_to_real(data_view, xy_pixel[2], xy_pixel[3],
                                            &xy[2], &xy[3]);
        }

        gwy_layer_rectangle_draw_rectangle(layer, data_view,
                                           drawable, target, xy);
    }
}

static void
gwy_layer_rectangle_draw_rectangle(GwyVectorLayer *layer,
                                   GwyDataView *data_view,
                                   GdkDrawable *drawable,
                                   GwyRenderingTarget target,
                                   const gdouble *xy)
{
    gint xmin, ymin, xmax, ymax, width, height;
    gdouble xreal, yreal;

    switch (target) {
        case GWY_RENDERING_TARGET_SCREEN:
        gwy_data_view_coords_real_to_xy(data_view, xy[0], xy[1], &xmin, &ymin);
        gwy_data_view_coords_real_to_xy(data_view, xy[2], xy[3], &xmax, &ymax);
        break;

        case GWY_RENDERING_TARGET_PIXMAP_IMAGE:
        gwy_data_view_get_real_data_sizes(data_view, &xreal, &yreal);
        gdk_drawable_get_size(drawable, &width, &height);
        xmin = floor(xy[0]*width/xreal);
        ymin = floor(xy[1]*height/yreal);
        xmax = floor(xy[2]*width/xreal);
        ymax = floor(xy[3]*height/yreal);
        break;

        default:
        g_return_if_reached();
        break;
    }

    if (xmax < xmin)
        GWY_SWAP(gint, xmin, xmax);
    if (ymax < ymin)
        GWY_SWAP(gint, ymin, ymax);

    if (GWY_LAYER_RECTANGLE(layer)->is_crop) {
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
    GdkWindow *window;
    GdkCursor *cursor;
    gint x, y, i;
    gdouble xreal, yreal, xsize, ysize, xy[OBJECT_SIZE];
    gdouble rectW, rectH;
    gboolean square;
    GwyLayerRectangle *layer_rect = GWY_LAYER_RECTANGLE(layer);

    if (!layer->selection)
        return FALSE;

    data_view = GWY_DATA_VIEW(GWY_DATA_VIEW_LAYER(layer)->parent);
    g_return_val_if_fail(data_view, FALSE);
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
        if (i >= 0) {
            i = i % OBJECT_SIZE;
            cursor = GWY_LAYER_RECTANGLE(layer)->corner_cursor[i];
        }
        else
            cursor = NULL;
        gdk_window_set_cursor(window, cursor);
        return FALSE;
    }

    g_assert(layer->selecting != -1);
    GWY_LAYER_RECTANGLE(layer)->square = square;
    gwy_layer_rectangle_undraw_object(layer, window,
                                      GWY_RENDERING_TARGET_SCREEN, i);
    if (square)
        gwy_layer_rectangle_squarize(data_view, x, y, xy);
    else {
        xy[2] = xreal;
        xy[3] = yreal;
    }

    if (layer_rect->snap) {
        /* shift selection to be centered around origin */
        gwy_data_view_get_real_data_sizes(data_view, &xsize, &ysize);
        if (square) {
            rectW = xy[2] - xy[0];
            rectH = xy[3] - xy[1];
            xy[0] = xsize / 2.0 - rectW / 2.0;
            xy[1] = ysize / 2.0 - rectH / 2.0;
            xy[2] = xy[0] + rectW;
            xy[3] = xy[1] + rectH;
        }
        else {
            xy[0] = -xy[2] + xsize;
            xy[1] = -xy[3] + ysize;
        }

        /* XXX - this should be controlled by a property */
        if (TRUE) {
            /* shift down/right one pixel */
            gwy_data_view_coords_real_to_xy(data_view, xy[0], xy[1], &x, &y);
            x++;
            y++;
            gwy_data_view_coords_xy_to_real(data_view, x, y, &xy[0], &xy[1]);
        }
    }

    gwy_selection_set_object(layer->selection, i, xy);
    gwy_layer_rectangle_draw_object(layer, window,
                                    GWY_RENDERING_TARGET_SCREEN, i);

    return FALSE;
}

static gboolean
gwy_layer_rectangle_button_pressed(GwyVectorLayer *layer,
                                   GdkEventButton *event)
{
    GwyDataView *data_view;
    GdkWindow *window;
    gint x, y, i;
    gdouble xreal, yreal, xy[OBJECT_SIZE];
    gboolean square;

    if (!layer->selection)
        return FALSE;

    if (event->button != 1)
        return FALSE;

    data_view = GWY_DATA_VIEW(GWY_DATA_VIEW_LAYER(layer)->parent);
    g_return_val_if_fail(data_view, FALSE);
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

    i = gwy_layer_rectangle_near_point(layer, xreal, yreal);
    /* just emit "object-chosen" when selection is not editable */
    if (!layer->editable) {
        if (i >= 0)
            gwy_vector_layer_object_chosen(layer, i/4);
        return FALSE;
    }
    /* handle existing selection */
    if (i >= 0) {
        layer->selecting = i/4;
        gwy_layer_rectangle_undraw_object(layer, window,
                                          GWY_RENDERING_TARGET_SCREEN,
                                          layer->selecting);

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
            gwy_layer_rectangle_undraw_object(layer, window,
                                              GWY_RENDERING_TARGET_SCREEN, i);
        }
        layer->selecting = 0;    /* avoid "update" signal emission */
        layer->selecting = gwy_selection_set_object(layer->selection, i, xy);
    }
    GWY_LAYER_RECTANGLE(layer)->square = square;
    layer->button = event->button;
    gwy_layer_rectangle_draw_object(layer, window,
                                    GWY_RENDERING_TARGET_SCREEN,
                                    layer->selecting);

    gdk_window_set_cursor(window, GWY_LAYER_RECTANGLE(layer)->resize_cursor);
    gwy_vector_layer_object_chosen(layer, layer->selecting);

    return FALSE;
}

static gboolean
gwy_layer_rectangle_button_released(GwyVectorLayer *layer,
                                    GdkEventButton *event)
{
    GwyDataView *data_view;
    GdkWindow *window;
    GdkCursor *cursor;
    gint x, y, xx, yy, i;
    gdouble xreal, yreal, xy[OBJECT_SIZE];
    gboolean outside;

    if (!layer->selection)
        return FALSE;

    if (!layer->button)
        return FALSE;

    data_view = GWY_DATA_VIEW(GWY_DATA_VIEW_LAYER(layer)->parent);
    g_return_val_if_fail(data_view, FALSE);
    window = GTK_WIDGET(data_view)->window;

    layer->button = 0;
    x = event->x;
    y = event->y;
    i = layer->selecting;
    gwy_debug("i = %d", i);
    gwy_data_view_coords_xy_clamp(data_view, &x, &y);
    outside = (event->x != x) || (event->y != y);
    gwy_data_view_coords_xy_to_real(data_view, x, y, &xreal, &yreal);
    gwy_layer_rectangle_undraw_object(layer, window,
                                      GWY_RENDERING_TARGET_SCREEN, i);
    gwy_selection_get_object(layer->selection, i, xy);
    gwy_data_view_coords_real_to_xy(data_view, xy[0], xy[1], &xx, &yy);
    gwy_debug("event: [%f, %f], xy: [%d, %d]", event->x, event->y, xx, yy);
    if (xx == event->x || yy == event->y)
        gwy_selection_delete_object(layer->selection, i);
    else {
        if (GWY_LAYER_RECTANGLE(layer)->square)
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
        gwy_layer_rectangle_draw_object(layer, window,
                                        GWY_RENDERING_TARGET_SCREEN, i);
    }

    layer->selecting = -1;
    i = gwy_layer_rectangle_near_point(layer, xreal, yreal);
    if (i >= 0)
        i = i % OBJECT_SIZE;
    outside = outside || (i == -1);
    cursor = outside ? NULL : GWY_LAYER_RECTANGLE(layer)->corner_cursor[i];
    gdk_window_set_cursor(window, cursor);
    gwy_selection_finished(layer->selection);

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

    if (parent && GTK_WIDGET_REALIZED(parent))
        gwy_layer_rectangle_undraw(vector_layer, parent->window,
                                   GWY_RENDERING_TARGET_SCREEN);
    layer->is_crop = is_crop;
    if (parent && GTK_WIDGET_REALIZED(parent))
        gwy_layer_rectangle_draw(vector_layer, parent->window,
                                 GWY_RENDERING_TARGET_SCREEN);
    g_object_notify(G_OBJECT(layer), "is-crop");
}

static void
gwy_layer_rectangle_set_reflection(GwyLayerRectangle *layer,
                                   gboolean draw_reflection)
{
    GwyVectorLayer *vector_layer;
    GtkWidget *parent;

    g_return_if_fail(GWY_IS_LAYER_RECTANGLE(layer));
    vector_layer = GWY_VECTOR_LAYER(layer);
    parent = GWY_DATA_VIEW_LAYER(layer)->parent;

    if (draw_reflection == layer->draw_reflection)
        return;

    if (parent && GTK_WIDGET_REALIZED(parent))
        gwy_layer_rectangle_undraw(vector_layer, parent->window,
                                   GWY_RENDERING_TARGET_SCREEN);
    layer->draw_reflection = draw_reflection;
    if (parent && GTK_WIDGET_REALIZED(parent))
        gwy_layer_rectangle_draw(vector_layer, parent->window,
                                 GWY_RENDERING_TARGET_SCREEN);
    g_object_notify(G_OBJECT(layer), "draw-reflection");
}

static void
gwy_layer_rectangle_set_snap(GwyLayerRectangle *layer, gboolean snap)
{
    GwyVectorLayer *vector_layer;
    GtkWidget *parent;

    g_return_if_fail(GWY_IS_LAYER_RECTANGLE(layer));
    vector_layer = GWY_VECTOR_LAYER(layer);
    parent = GWY_DATA_VIEW_LAYER(layer)->parent;

    if (snap == layer->snap)
        return;

    if (parent && GTK_WIDGET_REALIZED(parent))
        gwy_layer_rectangle_undraw(vector_layer, parent->window,
                                   GWY_RENDERING_TARGET_SCREEN);
    layer->snap = snap;
    if (parent && GTK_WIDGET_REALIZED(parent))
        gwy_layer_rectangle_draw(vector_layer, parent->window,
                                 GWY_RENDERING_TARGET_SCREEN);
    g_object_notify(G_OBJECT(layer), "snap-to-center");
}

static void
gwy_layer_rectangle_realize(GwyDataViewLayer *dlayer)
{
    GwyLayerRectangle *layer;
    GdkDisplay *display;

    gwy_debug("");

    GWY_DATA_VIEW_LAYER_CLASS(gwy_layer_rectangle_parent_class)->realize(dlayer);
    layer = GWY_LAYER_RECTANGLE(dlayer);
    display = gtk_widget_get_display(dlayer->parent);
    layer->resize_cursor = gdk_cursor_new_for_display(display, GDK_CROSS);
    layer->corner_cursor[0] = gdk_cursor_new_for_display(display, GDK_UL_ANGLE);
    layer->corner_cursor[1] = gdk_cursor_new_for_display(display, GDK_LL_ANGLE);
    layer->corner_cursor[2] = gdk_cursor_new_for_display(display, GDK_UR_ANGLE);
    layer->corner_cursor[3] = gdk_cursor_new_for_display(display, GDK_LR_ANGLE);
}

static void
gwy_layer_rectangle_unrealize(GwyDataViewLayer *dlayer)
{
    GwyLayerRectangle *layer;
    gint i;

    gwy_debug("");

    layer = GWY_LAYER_RECTANGLE(dlayer);
    gdk_cursor_unref(layer->resize_cursor);
    for (i = 0; i < 4; i++)
        gdk_cursor_unref(layer->corner_cursor[i]);

    GWY_DATA_VIEW_LAYER_CLASS(gwy_layer_rectangle_parent_class)->unrealize(dlayer);
}

static int
gwy_layer_rectangle_near_point(GwyVectorLayer *layer,
                               gdouble xreal, gdouble yreal)
{
    gdouble d2min, xy[OBJECT_SIZE], metric[4];
    gint i, n;

    if (!(n = gwy_selection_get_data(layer->selection, NULL))
        || layer->focus >= n)
        return -1;

    gwy_data_view_get_metric(GWY_DATA_VIEW(GWY_DATA_VIEW_LAYER(layer)->parent),
                             metric);
    if (layer->focus >= 0) {
        gdouble coords[8];

        gwy_selection_get_object(layer->selection, layer->focus, xy);
        coords[0] = coords[2] = xy[0];
        coords[1] = coords[5] = xy[1];
        coords[4] = coords[6] = xy[2];
        coords[3] = coords[7] = xy[3];
        i = gwy_math_find_nearest_point(xreal, yreal, &d2min, 4, coords,
                                        metric);
    }
    else {
        gdouble *coords = g_newa(gdouble, 8*n);

        for (i = 0; i < n; i++) {
            gwy_selection_get_object(layer->selection, i, xy);
            coords[8*i + 0] = coords[8*i + 2] = xy[0];
            coords[8*i + 1] = coords[8*i + 5] = xy[1];
            coords[8*i + 4] = coords[8*i + 6] = xy[2];
            coords[8*i + 3] = coords[8*i + 7] = xy[3];
        }
        i = gwy_math_find_nearest_point(xreal, yreal, &d2min, 4*n, coords,
                                        metric);
    }
    if (d2min > PROXIMITY_DISTANCE*PROXIMITY_DISTANCE)
        return -1;
    return i;
}

static void
gwy_layer_rectangle_squarize(GwyDataView *data_view,
                             gint x, gint y,
                             gdouble *xy)
{
    gint size, xb, yb, xx, yy;

    gwy_data_view_coords_real_to_xy(data_view, xy[0], xy[1], &xb, &yb);
    size = MAX(ABS(x - xb), ABS(y - yb));
    x = xx = (x >= xb) ? xb + size : xb - size;
    y = yy = (y >= yb) ? yb + size : yb - size;
    gwy_data_view_coords_xy_clamp(data_view, &xx, &yy);
    if (xx != x || yy != y) {
        size = MIN(ABS(xx - xb), ABS(yy - yb));
        x = (xx >= xb) ? xb + size : xb - size;
        y = (yy >= yb) ? yb + size : yb - size;
    }
    gwy_data_view_coords_xy_to_real(data_view, x, y, &xy[2], &xy[3]);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
