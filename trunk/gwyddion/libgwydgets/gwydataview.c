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

#include "config.h"
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <glib-object.h>

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwydebugobjects.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwydgettypes.h>
#include <libgwydgets/gwydataview.h>

#define BITS_PER_SAMPLE 8

/* Rounding errors */
#define EPS 1e-6

#define GWY_DATA_VIEW_GET_PRIVATE(o) \
   (G_TYPE_INSTANCE_GET_PRIVATE((o), GWY_TYPE_DATA_VIEW, GwyDataViewPrivate))

enum {
    REDRAWN,
    RESIZED,
    LAYER_PLUGGED,
    LAYER_UNPLUGGED,
    LAST_SIGNAL
};

enum {
    PROP_0,
    PROP_ZOOM,
    PROP_DATA_PREFIX,
};

typedef struct _GwyDataViewPrivate GwyDataViewPrivate;

/* The data field offsets, not display offsets */
struct _GwyDataViewPrivate {
    gdouble xoffset;
    gdouble yoffset;
};

static void     gwy_data_view_destroy              (GtkObject *object);
static void     gwy_data_view_finalize             (GObject *object);
static void     gwy_data_view_set_property         (GObject *object,
                                                    guint prop_id,
                                                    const GValue *value,
                                                    GParamSpec *pspec);
static void     gwy_data_view_get_property         (GObject*object,
                                                    guint prop_id,
                                                    GValue *value,
                                                    GParamSpec *pspec);
static void     gwy_data_view_realize              (GtkWidget *widget);
static void     gwy_data_view_unrealize            (GtkWidget *widget);
static void     gwy_data_view_size_request         (GtkWidget *widget,
                                                    GtkRequisition *requisition);
static void     gwy_data_view_size_allocate        (GtkWidget *widget,
                                                    GtkAllocation *allocation);
static void     simple_gdk_pixbuf_composite        (GdkPixbuf *source,
                                                    GdkPixbuf *dest);
static void     simple_gdk_pixbuf_scale_or_copy    (GdkPixbuf *source,
                                                    GdkPixbuf *dest);
static void     gwy_data_view_make_pixmap          (GwyDataView *data_view);
static void     gwy_data_view_paint                (GwyDataView *data_view);
static gboolean gwy_data_view_expose               (GtkWidget *widget,
                                                    GdkEventExpose *event);
static gboolean gwy_data_view_button_press         (GtkWidget *widget,
                                                    GdkEventButton *event);
static gboolean gwy_data_view_button_release       (GtkWidget *widget,
                                                    GdkEventButton *event);
static gboolean gwy_data_view_motion_notify        (GtkWidget *widget,
                                                    GdkEventMotion *event);
static gboolean gwy_data_view_key_press            (GtkWidget *widget,
                                                    GdkEventKey *event);
static gboolean gwy_data_view_key_release          (GtkWidget *widget,
                                                    GdkEventKey *event);
static void     gwy_data_view_set_layer            (GwyDataView *data_view,
                                                    gpointer which,
                                                    gulong *hid,
                                                    GwyDataViewLayer *layer,
                                                    GwyDataViewLayerType type);
static void     gwy_data_view_square_changed       (GwyDataView *data_view,
                                                    GQuark quark);
static void     gwy_data_view_connect_data         (GwyDataView *data_view);
static void     gwy_data_view_disconnect_data      (GwyDataView *data_view);

static guint data_view_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE(GwyDataView, gwy_data_view, GTK_TYPE_WIDGET)

static void
gwy_data_view_class_init(GwyDataViewClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkObjectClass *object_class;
    GtkWidgetClass *widget_class;

    object_class = (GtkObjectClass*)klass;
    widget_class = (GtkWidgetClass*)klass;

    gobject_class->finalize = gwy_data_view_finalize;
    gobject_class->set_property = gwy_data_view_set_property;
    gobject_class->get_property = gwy_data_view_get_property;

    object_class->destroy = gwy_data_view_destroy;

    widget_class->realize = gwy_data_view_realize;
    widget_class->expose_event = gwy_data_view_expose;
    widget_class->size_request = gwy_data_view_size_request;
    widget_class->unrealize = gwy_data_view_unrealize;
    widget_class->size_allocate = gwy_data_view_size_allocate;
    /* user-interaction events */
    widget_class->button_press_event = gwy_data_view_button_press;
    widget_class->button_release_event = gwy_data_view_button_release;
    widget_class->motion_notify_event = gwy_data_view_motion_notify;
    widget_class->key_press_event = gwy_data_view_key_press;
    widget_class->key_release_event = gwy_data_view_key_release;

    g_type_class_add_private(klass, sizeof(GwyDataViewPrivate));

    /**
     * GwyDataView:zoom:
     *
     * The :zoom property is the ratio between displayed and real data size.
     **/
    g_object_class_install_property
        (gobject_class,
         PROP_ZOOM,
         g_param_spec_double("zoom",
                             "Zoom",
                             "Ratio between displayed and real data size",
                             1/16.0, 16.0, 1.0, G_PARAM_READWRITE));

    /**
     * GwyDataView:data-prefix:
     *
     * The :data-prefix property is the container prefix the data displayed
     * by this view lies under.
     *
     * Note it is only used for items used by #GwyDataView itself, layers have
     * their own keys.
     **/
    g_object_class_install_property
        (gobject_class,
         PROP_DATA_PREFIX,
         g_param_spec_string("data-prefix",
                             "Data prefix",
                             "Container data prefix",
                             NULL, G_PARAM_READWRITE));

    /**
     * GwyDataView::redrawn:
     * @gwydataview: The #GwyDataView which received the signal.
     *
     * The ::redrawn signal is emitted when #GwyDataView redraws pixbufs after
     * an update.  That is, when it's the right time to get a new pixbuf from
     * gwy_data_view_get_pixbuf().
     **/
    data_view_signals[REDRAWN]
        = g_signal_new("redrawn",
                       G_OBJECT_CLASS_TYPE(object_class),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET(GwyDataViewClass, redrawn),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__VOID,
                       G_TYPE_NONE, 0);

    /**
     * GwyDataView::resized:
     * @gwydataview: The #GwyDataView which received the signal.
     *
     * The ::resized signal is emitted when #GwyDataView wants to be resized,
     * that is when the dimensions of base data field changes or square mode
     * is changed.
     * Its purpose is to subvert the normal resizing logic of #GwyDataWindow
     * (due to geometry hints, its size requests are generally ignored, so
     * an explicit resize is needed).  You should usually ignore it.
     **/
    data_view_signals[RESIZED]
        = g_signal_new("resized",
                       G_OBJECT_CLASS_TYPE(object_class),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET(GwyDataViewClass, resized),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__VOID,
                       G_TYPE_NONE, 0);

    /**
     * GwyDataView::layer-plugged:
     * @arg1: Which layer was plugged (a #GwyDataViewLayerType value).
     * @gwydataview: The #GwyDataView which received the signal.
     *
     * The ::layer-plugged signal is emitted when a layer is plugged into
     * a #GwyDataView.
     *
     * When a layer replaces an existing layer, ::layer-unplugged is emitted
     * once the old layer is unplugged, then ::layer-plugged when the new
     * is plugged.
     **/
    data_view_signals[LAYER_PLUGGED]
        = g_signal_new("layer-plugged",
                       G_OBJECT_CLASS_TYPE(object_class),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET(GwyDataViewClass, layer_plugged),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__ENUM,
                       G_TYPE_NONE, 1, GWY_TYPE_DATA_VIEW_LAYER_TYPE);

    /**
     * GwyDataView::layer-unplugged:
     * @arg1: Which layer was unplugged (a #GwyDataViewLayerType value).
     * @gwydataview: The #GwyDataView which received the signal.
     *
     * The ::layer-unplugged signal is emitted when a layer is unplugged from
     * a #GwyDataView.
     *
     * When a layer replaces an existing layer, ::layer-unplugged is emitted
     * once the old layer is unplugged, then ::layer-plugged when the new
     * is plugged.
     **/
    data_view_signals[LAYER_UNPLUGGED]
        = g_signal_new("layer-unplugged",
                       G_OBJECT_CLASS_TYPE(object_class),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET(GwyDataViewClass, layer_unplugged),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__ENUM,
                       G_TYPE_NONE, 1, GWY_TYPE_DATA_VIEW_LAYER_TYPE);
}

static void
gwy_data_view_init(GwyDataView *data_view)
{
    data_view->zoom = 1.0;
    data_view->newzoom = 1.0;
}

static void
gwy_data_view_set_property(GObject *object,
                           guint prop_id,
                           const GValue *value,
                           GParamSpec *pspec)
{
    GwyDataView *data_view = GWY_DATA_VIEW(object);

    switch (prop_id) {
        case PROP_ZOOM:
        gwy_data_view_set_zoom(data_view, g_value_get_double(value));
        break;

        case PROP_DATA_PREFIX:
        gwy_data_view_set_data_prefix(data_view, g_value_get_string(value));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_data_view_get_property(GObject *object,
                           guint prop_id,
                           GValue *value,
                           GParamSpec *pspec)
{
    GwyDataView *data_view = GWY_DATA_VIEW(object);

    switch (prop_id) {
        case PROP_ZOOM:
        g_value_set_double(value, gwy_data_view_get_zoom(data_view));
        break;

        case PROP_DATA_PREFIX:
        g_value_set_string(value, gwy_data_view_get_data_prefix(data_view));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_data_view_finalize(GObject *object)
{
    GwyDataView *data_view;

    g_return_if_fail(GWY_IS_DATA_VIEW(object));

    data_view = GWY_DATA_VIEW(object);
    gwy_object_unref(data_view->base_layer);
    gwy_object_unref(data_view->alpha_layer);
    gwy_object_unref(data_view->top_layer);
    gwy_object_unref(data_view->data);

    G_OBJECT_CLASS(gwy_data_view_parent_class)->finalize(object);
}

/**
 * gwy_data_view_new:
 * @data: A #GwyContainer containing the data to display.
 *
 * Creates a new data-displaying widget for @data.
 *
 * A newly created #GwyDataView doesn't display anything.  You have to add
 * some layers to it, at least a base layer with
 * gwy_data_view_set_base_layer(), and possibly others with
 * gwy_data_view_set_alpha_layer() and gwy_data_view_set_top_layer().
 *
 * The top layer is special. It must be a vector layer and can receive
 * mouse and keyboard events.
 *
 * The base layer it also special. It must be always present, and must not be
 * transparent or vector.
 *
 * Returns: A newly created data view as a #GtkWidget.
 **/
GtkWidget*
gwy_data_view_new(GwyContainer *data)
{
    GtkWidget *data_view;

    g_return_val_if_fail(GWY_IS_CONTAINER(data), NULL);

    data_view = gtk_widget_new(GWY_TYPE_DATA_VIEW, NULL);

    g_object_ref(data);
    GWY_DATA_VIEW(data_view)->data = data;

    return data_view;
}

static void
gwy_data_view_destroy(GtkObject *object)
{
    GwyDataView *data_view;

    g_return_if_fail(GWY_IS_DATA_VIEW(object));

    data_view = GWY_DATA_VIEW(object);
    gwy_data_view_disconnect_data(data_view);
    gwy_data_view_set_layer(data_view, &data_view->top_layer, NULL, NULL,
                            GWY_DATA_VIEW_LAYER_TOP);
    gwy_data_view_set_layer(data_view, &data_view->alpha_layer,
                            &data_view->alpha_hid, NULL,
                            GWY_DATA_VIEW_LAYER_ALPHA);
    gwy_data_view_set_layer(data_view, &data_view->base_layer,
                            &data_view->base_hid, NULL,
                            GWY_DATA_VIEW_LAYER_BASE);

    GTK_OBJECT_CLASS(gwy_data_view_parent_class)->destroy(object);
}

static void
gwy_data_view_unrealize(GtkWidget *widget)
{
    GwyDataView *data_view = GWY_DATA_VIEW(widget);

    if (data_view->base_layer)
        gwy_data_view_layer_unrealize
                                   (GWY_DATA_VIEW_LAYER(data_view->base_layer));
    if (data_view->alpha_layer)
        gwy_data_view_layer_unrealize
                                  (GWY_DATA_VIEW_LAYER(data_view->alpha_layer));
    if (data_view->top_layer)
        gwy_data_view_layer_unrealize
                                    (GWY_DATA_VIEW_LAYER(data_view->top_layer));

    gwy_object_unref(data_view->pixbuf);
    gwy_object_unref(data_view->base_pixbuf);

    if (GTK_WIDGET_CLASS(gwy_data_view_parent_class)->unrealize)
        GTK_WIDGET_CLASS(gwy_data_view_parent_class)->unrealize(widget);
}


static void
gwy_data_view_realize(GtkWidget *widget)
{
    GwyDataView *data_view;
    GdkWindowAttr attributes;
    gint attributes_mask;

    gwy_debug("realizing a GwyDataView (%ux%u)",
              widget->allocation.width, widget->allocation.height);

    g_return_if_fail(widget != NULL);

    GTK_WIDGET_SET_FLAGS(widget, GTK_REALIZED);
    data_view = GWY_DATA_VIEW(widget);

    attributes.x = widget->allocation.x;
    attributes.y = widget->allocation.y;
    attributes.width = widget->allocation.width;
    attributes.height = widget->allocation.height;
    attributes.wclass = GDK_INPUT_OUTPUT;
    attributes.window_type = GDK_WINDOW_CHILD;
    attributes.event_mask = gtk_widget_get_events(widget)
                            | GDK_EXPOSURE_MASK
                            | GDK_BUTTON_PRESS_MASK
                            | GDK_BUTTON_RELEASE_MASK
                            | GDK_KEY_PRESS_MASK
                            | GDK_KEY_RELEASE_MASK
                            | GDK_POINTER_MOTION_MASK
                            | GDK_POINTER_MOTION_HINT_MASK;
    attributes.visual = gtk_widget_get_visual(widget);
    attributes.colormap = gtk_widget_get_colormap(widget);

    attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
    widget->window = gdk_window_new(gtk_widget_get_parent_window(widget),
                                    &attributes, attributes_mask);
    gdk_window_set_user_data(widget->window, widget);

    widget->style = gtk_style_attach(widget->style, widget->window);
    gtk_style_set_background(widget->style, widget->window, GTK_STATE_NORMAL);

    if (data_view->base_layer)
        gwy_data_view_layer_realize(GWY_DATA_VIEW_LAYER(data_view->base_layer));
    if (data_view->alpha_layer)
        gwy_data_view_layer_realize
                                  (GWY_DATA_VIEW_LAYER(data_view->alpha_layer));
    if (data_view->top_layer)
        gwy_data_view_layer_realize(GWY_DATA_VIEW_LAYER(data_view->top_layer));

    gwy_data_view_make_pixmap(data_view);
}

static void
gwy_data_view_size_request(GtkWidget *widget,
                           GtkRequisition *requisition)
{
    GwyDataView *data_view;
    const gchar *key;

    gwy_debug(" ");

    data_view = GWY_DATA_VIEW(widget);
    requisition->width = requisition->height = 2;
    if (!data_view->base_layer)
        return;
    key = gwy_pixmap_layer_get_data_key(data_view->base_layer);
    if (!key)
        return;

    if (data_view->realsquare) {
        gdouble scale = MAX(data_view->xres/data_view->xreal,
                            data_view->yres/data_view->yreal);
        scale *= data_view->newzoom;
        requisition->width = GWY_ROUND(scale * data_view->xreal);
        requisition->height = GWY_ROUND(scale * data_view->yreal);
    }
    else {
        requisition->width = GWY_ROUND(data_view->newzoom * data_view->xres);
        requisition->height = GWY_ROUND(data_view->newzoom * data_view->yres);
    }

    data_view->size_requested = TRUE;
    gwy_debug("requesting %d x %d",
              requisition->width, requisition->height);
}

static void
gwy_data_view_size_allocate(GtkWidget *widget,
                            GtkAllocation *allocation)
{
    GwyDataView *data_view;

    gwy_debug("allocating %d x %d",
              allocation->width, allocation->height);

    g_return_if_fail(widget != NULL);
    g_return_if_fail(GWY_IS_DATA_VIEW(widget));
    g_return_if_fail(allocation != NULL);

    widget->allocation = *allocation;

    if (!GTK_WIDGET_REALIZED(widget))
        return;

    data_view = GWY_DATA_VIEW(widget);
    gdk_window_move_resize(widget->window,
                           allocation->x, allocation->y,
                           allocation->width, allocation->height);
    gwy_data_view_make_pixmap(data_view);
    /* Update ideal zoom after a `spontanoues' size-allocate when someone
     * simply changed the size w/o asking us.  But if we were queried first,
     * be persistent and request the same zoom also next time */
    if (!data_view->size_requested) {
        data_view->newzoom = data_view->zoom;
        g_object_notify(G_OBJECT(data_view), "zoom");
    }
    data_view->size_requested = FALSE;
}

static void
gwy_data_view_make_pixmap(GwyDataView *data_view)
{
    const GtkAllocation *alloc;
    GtkWidget *widget;
    gint width, height, scwidth, scheight;

    if (!data_view->xres || !data_view->yres) {
        gwy_object_unref(data_view->base_pixbuf);
        return;
    }

    if (data_view->base_pixbuf) {
        width = gdk_pixbuf_get_width(data_view->base_pixbuf);
        height = gdk_pixbuf_get_height(data_view->base_pixbuf);
        if (width != data_view->xres || height != data_view->yres)
            gwy_object_unref(data_view->base_pixbuf);
    }
    if (!data_view->base_pixbuf) {
        data_view->base_pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB,
                                                FALSE,
                                                BITS_PER_SAMPLE,
                                                data_view->xres,
                                                data_view->yres);
        gwy_debug_objects_creation(G_OBJECT(data_view->base_pixbuf));
    }

    if (data_view->pixbuf) {
        width = gdk_pixbuf_get_width(data_view->pixbuf);
        height = gdk_pixbuf_get_height(data_view->pixbuf);
    }
    else
        width = height = -1;

    widget = GTK_WIDGET(data_view);
    alloc = &widget->allocation;

    if (data_view->realsquare) {
        gdouble scale = MAX(data_view->xres/data_view->xreal,
                            data_view->yres/data_view->yreal);
        data_view->zoom = MIN(alloc->width/(scale*data_view->xreal),
                              alloc->height/(scale*data_view->yreal));
        scale *= data_view->zoom;
        scwidth = GWY_ROUND(scale * data_view->xreal);
        scheight = GWY_ROUND(scale * data_view->yreal);
    }
    else {
        data_view->zoom = MIN((gdouble)alloc->width/data_view->xres,
                              (gdouble)alloc->height/data_view->yres);
        scwidth = GWY_ROUND(data_view->xres * data_view->zoom);
        scheight = GWY_ROUND(data_view->yres * data_view->zoom);
    }
    data_view->xmeasure = data_view->xreal/scwidth;
    data_view->ymeasure = data_view->yreal/scheight;
    data_view->xoff = (alloc->width - scwidth)/2;
    data_view->yoff = (alloc->height - scheight)/2;
    if (scwidth != width || scheight != height) {
        gwy_object_unref(data_view->pixbuf);
        data_view->pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB,
                                           FALSE,
                                           BITS_PER_SAMPLE,
                                           scwidth, scheight);
        gwy_debug_objects_creation(G_OBJECT(data_view->pixbuf));
        gdk_pixbuf_fill(data_view->pixbuf, 0x00000000);
        gwy_data_view_paint(data_view);
    }
}

static void
simple_gdk_pixbuf_scale_or_copy(GdkPixbuf *source, GdkPixbuf *dest)
{
    gint height, width, src_height, src_width;

    src_height = gdk_pixbuf_get_height(source);
    src_width = gdk_pixbuf_get_width(source);
    height = gdk_pixbuf_get_height(dest);
    width = gdk_pixbuf_get_width(dest);

    if (src_width == width && src_height == height)
        gdk_pixbuf_copy_area(source, 0, 0, src_width, src_height,
                             dest, 0, 0);
    else
        gdk_pixbuf_scale(source, dest, 0, 0, width, height, 0.0, 0.0,
                         (gdouble)width/src_width, (gdouble)height/src_height,
                         GDK_INTERP_TILES);
}

static void
simple_gdk_pixbuf_composite(GdkPixbuf *source, GdkPixbuf *dest)
{
    gint height, width, src_height, src_width;

    src_height = gdk_pixbuf_get_height(source);
    src_width = gdk_pixbuf_get_width(source);
    height = gdk_pixbuf_get_height(dest);
    width = gdk_pixbuf_get_width(dest);

    gdk_pixbuf_composite(source, dest, 0, 0, width, height, 0.0, 0.0,
                         (gdouble)width/src_width, (gdouble)height/src_height,
                         GDK_INTERP_TILES, 0xff);
}

/* paint pixmap layers */
static void
gwy_data_view_paint(GwyDataView *data_view)
{
    GdkPixbuf *apixbuf, *bpixbuf;

    gwy_debug(" ");
    g_return_if_fail(GWY_IS_DATA_VIEW_LAYER(data_view->base_layer));

    /* Base layer is always present, however pixmap layers may return NULL if
     * they do not have corresponding data fields */
    bpixbuf = gwy_pixmap_layer_paint(data_view->base_layer);
    if (data_view->alpha_layer)
        apixbuf = gwy_pixmap_layer_paint(data_view->alpha_layer);
    else
        apixbuf = NULL;

    if (bpixbuf) {
        if (apixbuf) {
            simple_gdk_pixbuf_scale_or_copy(bpixbuf, data_view->base_pixbuf);
            simple_gdk_pixbuf_composite(apixbuf, data_view->base_pixbuf);
            simple_gdk_pixbuf_scale_or_copy(data_view->base_pixbuf,
                                            data_view->pixbuf);
        }
        else
            simple_gdk_pixbuf_scale_or_copy(bpixbuf, data_view->pixbuf);
    }
    else {
        gdk_pixbuf_fill(data_view->pixbuf, 0x00000000);
        if (apixbuf)
            simple_gdk_pixbuf_scale_or_copy(apixbuf, data_view->pixbuf);
    }
}

static gboolean
gwy_data_view_expose(GtkWidget *widget,
                     GdkEventExpose *event)
{
    GwyDataView *data_view;
    gint xs, ys, xe, ye, w, h;
    GdkRectangle rect;
    gboolean emit_redrawn = FALSE;

    data_view = GWY_DATA_VIEW(widget);

    if (!data_view->xres || !data_view->yres)
        return FALSE;

    /* This means we requested new size, but received no allocation -- because
     * the new size was identical to the old one.  BUT we had a reason for
     * that.  Typically this happens after a rotation of realsquare-displayed
     * data: the new widget size is the same, but the the stretched dimension
     * is now the other one and thus pixmap sizes have to be recalculated. */
    if (data_view->size_requested) {
        gwy_data_view_make_pixmap(data_view);
        data_view->size_requested = FALSE;
    }

    gdk_region_get_clipbox(event->region, &rect);
    gwy_debug("bbox = %dx%d  at (%d,%d)",
              rect.width, rect.height, rect.x, rect.y);
    w = gdk_pixbuf_get_width(data_view->pixbuf);
    h = gdk_pixbuf_get_height(data_view->pixbuf);
    xs = MAX(rect.x, data_view->xoff) - data_view->xoff;
    ys = MAX(rect.y, data_view->yoff) - data_view->yoff;
    xe = MIN(rect.x + rect.width, data_view->xoff + w) - data_view->xoff;
    ye = MIN(rect.y + rect.height, data_view->yoff + h) - data_view->yoff;
    gwy_debug("going to draw: %dx%d  at (%d,%d)",
              xe - xs, ye - ys, xs, ys);
    if (xs >= xe || ys >= ye)
        return FALSE;

    if (data_view->layers_changed
        || (data_view->base_layer
            && gwy_pixmap_layer_wants_repaint(data_view->base_layer))
        || (data_view->alpha_layer
            && gwy_pixmap_layer_wants_repaint(data_view->alpha_layer))) {
        gwy_data_view_paint(data_view);
        emit_redrawn = TRUE;
        data_view->layers_changed = FALSE;
    }

    gdk_draw_pixbuf(widget->window,
                    NULL,
                    data_view->pixbuf,
                    xs, ys,
                    xs + data_view->xoff, ys + data_view->yoff,
                    xe - xs, ye - ys,
                    GDK_RGB_DITHER_NORMAL,
                    0, 0);

    if (data_view->top_layer)
        gwy_vector_layer_draw(data_view->top_layer, widget->window,
                              GWY_RENDERING_TARGET_SCREEN);

    if (emit_redrawn)
        g_signal_emit(data_view, data_view_signals[REDRAWN], 0);

    return FALSE;
}

static gboolean
gwy_data_view_button_press(GtkWidget *widget,
                           GdkEventButton *event)
{
    GwyDataView *data_view;

    data_view = GWY_DATA_VIEW(widget);
    if (!data_view->top_layer)
        return FALSE;

    return gwy_vector_layer_button_press(data_view->top_layer, event);
}

static gboolean
gwy_data_view_button_release(GtkWidget *widget,
                             GdkEventButton *event)
{
    GwyDataView *data_view;

    data_view = GWY_DATA_VIEW(widget);
    if (!data_view->top_layer)
        return FALSE;

    return gwy_vector_layer_button_release(data_view->top_layer, event);
}

static gboolean
gwy_data_view_motion_notify(GtkWidget *widget,
                            GdkEventMotion *event)
{
    GwyDataView *data_view;

    data_view = GWY_DATA_VIEW(widget);
    if (!data_view->top_layer)
        return FALSE;

    return gwy_vector_layer_motion_notify(data_view->top_layer, event);
}

static gboolean
gwy_data_view_key_press(GtkWidget *widget,
                        GdkEventKey *event)
{
    GwyDataView *data_view;

    data_view = GWY_DATA_VIEW(widget);
    if (!data_view->top_layer)
        return FALSE;

    return gwy_vector_layer_key_press(data_view->top_layer, event);
}

static gboolean
gwy_data_view_key_release(GtkWidget *widget,
                          GdkEventKey *event)
{
    GwyDataView *data_view;

    data_view = GWY_DATA_VIEW(widget);
    if (!data_view->top_layer)
        return FALSE;

    return gwy_vector_layer_key_release(data_view->top_layer, event);
}

static void
gwy_data_view_update(GwyDataView *data_view)
{
    GwyDataViewPrivate *priv;
    GtkWidget *widget;
    GwyDataField *data_field;
    const gchar *key;
    gint pxres, pyres;
    gboolean need_resize = FALSE;

    if (!data_view->base_layer
        || !(key = gwy_pixmap_layer_get_data_key(data_view->base_layer))
        || !(data_field = gwy_container_get_object_by_name(data_view->data,
                                                           key)))
        return;

    data_view->xres = gwy_data_field_get_xres(data_field);
    data_view->yres = gwy_data_field_get_yres(data_field);
    data_view->xreal = gwy_data_field_get_xreal(data_field);
    data_view->yreal = gwy_data_field_get_yreal(data_field);

    priv = GWY_DATA_VIEW_GET_PRIVATE(data_view);
    priv->xoffset = gwy_data_field_get_xoffset(data_field);
    priv->yoffset = gwy_data_field_get_yoffset(data_field);

    widget = GTK_WIDGET(data_view);
    if (!widget->window)
        return;

    if (data_view->base_pixbuf) {
        pxres = gdk_pixbuf_get_width(data_view->base_pixbuf);
        pyres = gdk_pixbuf_get_height(data_view->base_pixbuf);
        gwy_debug("field: %dx%d, pixbuf: %dx%d",
                  data_view->xres, data_view->yres, pxres, pyres);
        if (pxres != data_view->xres || pyres != data_view->yres)
            need_resize = TRUE;
    }

    if (need_resize) {
        gwy_debug("needs resize");
        gtk_widget_queue_resize(widget);
        g_signal_emit(widget, data_view_signals[RESIZED], 0);
    }
    else {
        if (data_view->pixbuf) {
            pxres = gdk_pixbuf_get_width(data_view->pixbuf);
            pyres = gdk_pixbuf_get_height(data_view->pixbuf);
            data_view->xmeasure = data_view->xreal/pxres;
            data_view->ymeasure = data_view->yreal/pyres;
        }
        gdk_window_invalidate_rect(widget->window, NULL, TRUE);
    }
}

/**
 * gwy_data_view_get_base_layer:
 * @data_view: A data view.
 *
 * Returns the base layer this data view currently uses.
 *
 * A base layer should be always present.
 *
 * Returns: The currently used base layer.
 **/
GwyPixmapLayer*
gwy_data_view_get_base_layer(GwyDataView *data_view)
{
    g_return_val_if_fail(GWY_IS_DATA_VIEW(data_view), NULL);
    return data_view->base_layer;
}

/**
 * gwy_data_view_get_alpha_layer:
 * @data_view: A data view.
 *
 * Returns the alpha layer this data view currently uses, or %NULL if none
 * is present.
 *
 * Returns: The currently used alpha layer.
 **/
GwyPixmapLayer*
gwy_data_view_get_alpha_layer(GwyDataView *data_view)
{
    g_return_val_if_fail(GWY_IS_DATA_VIEW(data_view), NULL);
    return data_view->alpha_layer;
}

/**
 * gwy_data_view_get_top_layer:
 * @data_view: A data view.
 *
 * Returns the top layer this data view currently uses, or %NULL if none
 * is present.
 *
 * Returns: The currently used top layer.
 **/
GwyVectorLayer*
gwy_data_view_get_top_layer(GwyDataView *data_view)
{
    g_return_val_if_fail(GWY_IS_DATA_VIEW(data_view), NULL);
    return data_view->top_layer;
}

static void
gwy_data_view_set_layer(GwyDataView *data_view,
                        gpointer which,
                        gulong *hid,
                        GwyDataViewLayer *layer,
                        GwyDataViewLayerType type)
{
    GwyDataViewLayer **which_layer;

    which_layer = (GwyDataViewLayer**)which;
    if (layer == *which_layer)
        return;
    if (*which_layer) {
        if (hid) {
            g_signal_handler_disconnect(*which_layer, *hid);
            *hid = 0;
        }
        if (GTK_WIDGET_REALIZED(GTK_WIDGET(data_view)))
            gwy_data_view_layer_unrealize(GWY_DATA_VIEW_LAYER(*which_layer));
        /* XXX: The order must be send signal first, really unplug later.
         * Other things expect it... */
        gwy_data_view_layer_unplugged(*which_layer);
        (*which_layer)->parent = NULL;
        g_object_unref(*which_layer);
        g_signal_emit(data_view, data_view_signals[LAYER_UNPLUGGED], 0, type);
    }
    if (layer) {
        g_assert(layer->parent == NULL);
        g_object_ref(layer);
        gtk_object_sink(GTK_OBJECT(layer));
        layer->parent = (GtkWidget*)data_view;
        if (hid)
            *hid = g_signal_connect_swapped(layer, "updated",
                                            G_CALLBACK(gwy_data_view_update),
                                            data_view);
        gwy_data_view_layer_plugged(layer);
        if (GTK_WIDGET_REALIZED(GTK_WIDGET(data_view)))
            gwy_data_view_layer_realize(GWY_DATA_VIEW_LAYER(layer));
    }
    data_view->layers_changed = TRUE;
    *which_layer = layer;
    g_signal_emit(data_view, data_view_signals[LAYER_PLUGGED], 0, type);
    gwy_data_view_update(data_view);
}

/**
 * gwy_data_view_set_base_layer:
 * @data_view: A data view.
 * @layer: A layer to be used as the base layer for @data_view.
 *
 * Plugs @layer to @data_view as the base layer.
 *
 * If another base layer is present, it's unplugged.
 *
 * The layer must not be a vector layer.  Theoretically, it can be %NULL to
 * use no base layer, but then @data_view will probably display garbage.
 **/
void
gwy_data_view_set_base_layer(GwyDataView *data_view,
                             GwyPixmapLayer *layer)
{
    g_return_if_fail(GWY_IS_DATA_VIEW(data_view));
    g_return_if_fail(!layer || GWY_IS_PIXMAP_LAYER(layer));
    gwy_data_view_set_layer(data_view,
                            &data_view->base_layer,
                            &data_view->base_hid,
                            GWY_DATA_VIEW_LAYER(layer),
                            GWY_DATA_VIEW_LAYER_BASE);
    gtk_widget_queue_resize(GTK_WIDGET(data_view));
}

/**
 * gwy_data_view_set_alpha_layer:
 * @data_view: A data view.
 * @layer: A layer to be used as the alpha layer for @data_view.
 *
 * Plugs @layer to @data_view as the alpha layer.
 *
 * If another alpha layer is present, it's unplugged.
 *
 * The layer must not be a vector layer.  It can be %NULL, meaning no alpha
 * layer is to be used.
 **/
void
gwy_data_view_set_alpha_layer(GwyDataView *data_view,
                              GwyPixmapLayer *layer)
{
    g_return_if_fail(GWY_IS_DATA_VIEW(data_view));
    g_return_if_fail(!layer || GWY_IS_PIXMAP_LAYER(layer));
    gwy_data_view_set_layer(data_view,
                            &data_view->alpha_layer,
                            &data_view->alpha_hid,
                            GWY_DATA_VIEW_LAYER(layer),
                            GWY_DATA_VIEW_LAYER_ALPHA);
}

/**
 * gwy_data_view_set_top_layer:
 * @data_view: A data view.
 * @layer: A layer to be used as the top layer for @data_view.
 *
 * Plugs @layer to @data_view as the top layer.
 *
 * If another top layer is present, it's unplugged.
 *
 * The layer must be a vector layer.  It can be %NULL, meaning no top
 * layer is to be used.
 **/
void
gwy_data_view_set_top_layer(GwyDataView *data_view,
                            GwyVectorLayer *layer)
{
    g_return_if_fail(GWY_IS_DATA_VIEW(data_view));
    g_return_if_fail(!layer || GWY_IS_VECTOR_LAYER(layer));
    gwy_data_view_set_layer(data_view,
                            &data_view->top_layer,
                            &data_view->top_hid,
                            GWY_DATA_VIEW_LAYER(layer),
                            GWY_DATA_VIEW_LAYER_TOP);
}

/**
 * gwy_data_view_get_hexcess:
 * @data_view: A data view.
 *
 * Return the horizontal excess of widget size to data size.
 *
 * Do not use.  Only useful for #GwyDataWindow implementation.
 *
 * Returns: The execess.
 **/
gdouble
gwy_data_view_get_hexcess(GwyDataView* data_view)
{
    g_return_val_if_fail(GWY_IS_DATA_VIEW(data_view), 0);
    if (!data_view->pixbuf)
        return 0;

    return (gdouble)GTK_WIDGET(data_view)->allocation.width
                    / gdk_pixbuf_get_width(data_view->pixbuf) - 1.0;
}

/**
 * gwy_data_view_get_vexcess:
 * @data_view: A data view.
 *
 * Return the vertical excess of widget size to data size.
 *
 * Do not use.  Only useful for #GwyDataWindow implementation.
 *
 * Returns: The execess.
 **/
gdouble
gwy_data_view_get_vexcess(GwyDataView *data_view)
{
    g_return_val_if_fail(GWY_IS_DATA_VIEW(data_view), 0);
    if (!data_view->pixbuf)
        return 0;

    return (gdouble)GTK_WIDGET(data_view)->allocation.height
                    / gdk_pixbuf_get_height(data_view->pixbuf) - 1.0;
}

/**
 * gwy_data_view_set_zoom:
 * @data_view: A data view.
 * @zoom: A new zoom value.
 *
 * Sets zoom of @data_view to @zoom.
 *
 * Zoom greater than 1 means larger image on screen and vice versa.
 *
 * Note window manager can prevent the window from resize and thus the zoom
 * from change.
 **/
void
gwy_data_view_set_zoom(GwyDataView *data_view,
                       gdouble zoom)
{
    g_return_if_fail(GWY_IS_DATA_VIEW(data_view));
    gwy_debug("zoom = %g, new = %g", data_view->newzoom, zoom);
    if (fabs(log(data_view->newzoom/zoom)) < 0.001)
        return;

    data_view->newzoom = zoom;
    g_object_notify(G_OBJECT(data_view), "zoom");
    gtk_widget_queue_resize(GTK_WIDGET(data_view));
}

/**
 * gwy_data_view_get_zoom:
 * @data_view: A data view.
 *
 * Returns current ideal zoom of a data view.
 *
 * More precisely the zoom value requested by gwy_data_view_set_zoom(), if
 * it's in use (real zoom may differ a bit due to pixel rounding).  If zoom
 * was set by explicite widget size change, real and requested zoom are
 * considered to be the same.
 *
 * When a resize is queued, the new zoom value is returned.
 *
 * In other words, this is the zoom @data_view would like to have.  Use
 * gwy_data_view_get_real_zoom() to get the real zoom.
 *
 * Returns: The zoom as a ratio between ideal displayed size and base data
 *          field size.
 **/
gdouble
gwy_data_view_get_zoom(GwyDataView *data_view)
{
    g_return_val_if_fail(GWY_IS_DATA_VIEW(data_view), 1.0);
    return data_view->newzoom;
}

/**
 * gwy_data_view_get_real_zoom:
 * @data_view: A data view.
 *
 * Returns current real zoom of a data view.
 *
 * This is the zoom value a data view may not wish to have, but was imposed
 * by window manager or other constraints.  Unlike ideal zoom set by
 * gwy_data_view_set_zoom(), this value cannot be set.
 *
 * When a resize is queued, the current (old) value is returned.
 *
 * Returns: The zoom as a ratio between real displayed size and base data
 *          field size.
 **/
gdouble
gwy_data_view_get_real_zoom(GwyDataView *data_view)
{
    g_return_val_if_fail(GWY_IS_DATA_VIEW(data_view), 1.0);
    return data_view->zoom;
}

/**
 * gwy_data_view_get_xmeasure:
 * @data_view: A data view.
 *
 * Returns the ratio between horizontal physical lengths and horizontal
 * screen lengths in pixels.
 *
 * Returns: The horizontal measure.
 **/
gdouble
gwy_data_view_get_xmeasure(GwyDataView *data_view)
{
    g_return_val_if_fail(GWY_IS_DATA_VIEW(data_view), 1.0);
    return data_view->xmeasure;
}

/**
 * gwy_data_view_get_ymeasure:
 * @data_view: A data view.
 *
 * Returns the ratio between vertical physical lengths and horizontal
 * screen lengths in pixels.
 *
 * Returns: The vertical measure.
 **/
gdouble
gwy_data_view_get_ymeasure(GwyDataView *data_view)
{
    g_return_val_if_fail(GWY_IS_DATA_VIEW(data_view), 1.0);
    return data_view->ymeasure;
}

/**
 * gwy_data_view_get_data:
 * @data_view: A data view.
 *
 * Returns the data container used by @data_view.
 *
 * Returns: The data as a #GwyContainer.
 **/
GwyContainer*
gwy_data_view_get_data(GwyDataView *data_view)
{
    g_return_val_if_fail(GWY_IS_DATA_VIEW(data_view), NULL);
    return data_view->data;
}

/**
 * gwy_data_view_coords_xy_clamp:
 * @data_view: A data view.
 * @xscr: A screen x-coordinate relative to widget origin.
 * @yscr: A screen y-coordinate relative to widget origin.
 *
 * Fixes screen coordinates @xscr and @yscr to be inside the data-displaying
 * area (which can be smaller than widget size).
 **/
void
gwy_data_view_coords_xy_clamp(GwyDataView *data_view,
                              gint *xscr, gint *yscr)
{
    gint size;

    g_return_if_fail(GWY_IS_DATA_VIEW(data_view));

    if (xscr) {
        size = gdk_pixbuf_get_width(data_view->pixbuf);
        *xscr = CLAMP(*xscr, data_view->xoff, data_view->xoff + size-1);
    }
    if (yscr) {
        size = gdk_pixbuf_get_height(data_view->pixbuf);
        *yscr = CLAMP(*yscr, data_view->yoff, data_view->yoff + size-1);
    }
}

/**
 * gwy_data_view_coords_xy_cut_line:
 * @data_view: A data view.
 * @x0scr: First point screen x-coordinate relative to widget origin.
 * @y0scr: First point screen y-coordinate relative to widget origin.
 * @x1scr: Second point screen x-coordinate relative to widget origin.
 * @y1scr: Second point screen y-coordinate relative to widget origin.
 *
 * Fixes screen coordinates of line endpoints to be inside the data-displaying
 * area (which can be smaller than widget size).
 *
 * Since: 2.11
 **/
void
gwy_data_view_coords_xy_cut_line(GwyDataView *data_view,
                                 gint *x0scr, gint *y0scr,
                                 gint *x1scr, gint *y1scr)
{
    gint i, i0, i1, xsize, ysize, x0s, y0s, x1s, y1s;
    gdouble t[6];

    g_return_if_fail(GWY_IS_DATA_VIEW(data_view));

    xsize = gdk_pixbuf_get_width(data_view->pixbuf);
    x0s = CLAMP(*x0scr, data_view->xoff, data_view->xoff + xsize-1);
    x1s = CLAMP(*x1scr, data_view->xoff, data_view->xoff + xsize-1);
    ysize = gdk_pixbuf_get_height(data_view->pixbuf);
    y0s = CLAMP(*y0scr, data_view->yoff, data_view->yoff + ysize-1);
    y1s = CLAMP(*y1scr, data_view->yoff, data_view->yoff + ysize-1);

    /* All inside */
    if (x0s == *x0scr && y0s == *y0scr && x1s == *x1scr && y1s == *y1scr)
        return;

    /* Horizontal/vertical lines */
    if (*x1scr == *x0scr) {
        *y0scr = y0s;
        *y1scr = y1s;
    }
    if (*y1scr == *y0scr) {
        *x0scr = x0s;
        *x1scr = x1s;
    }
    if (*y1scr == *y0scr || *x1scr == *x0scr)
        return;

    /* The hard case */
    x0s = *x0scr;
    x1s = *x1scr;
    y0s = *y0scr;
    y1s = *y1scr;

    t[0] = -(x0s)/(gdouble)(x1s - x0s);
    t[1] = (xsize - 1 -(x0s))/(gdouble)(x1s - x0s);
    t[2] = -(y0s)/(gdouble)(y1s - y0s);
    t[3] = (ysize - 1 -(y0s))/(gdouble)(y1s - y0s);
    /* Include the endpoints */
    t[4] = 0.0;
    t[5] = 1.0;

    gwy_math_sort(G_N_ELEMENTS(t), t);

    i0 = i1 = -1;
    for (i = 0; i < G_N_ELEMENTS(t); i++) {
        gdouble xy;

        if (t[i] < -EPS || t[i] > 1.0 + EPS)
            continue;

        xy = x0s + t[i]*(x1s - x0s);
        if (xy < -EPS || xy > xsize-1 + EPS)
            continue;

        xy = y0s + t[i]*(y1s - y0s);
        if (xy < -EPS || xy > ysize-1 + EPS)
            continue;

        /* i0 is the first index, once i0 != -1, do not change it any more */
        if (i0 == -1)
            i0 = i;

        /* i1 is the last index, move it as long as we are inside */
        i1 = i;
    }

    /* The line does not intersect the boundary at all.  Just return something
     * and pray... */
    if (i0 == -1) {
        *x0scr = *x1scr = xsize/2;
        *y0scr = *y1scr = ysize/2;
        return;
    }

    *x0scr = GWY_ROUND(x0s + t[i0]*(x1s - x0s));
    *x1scr = GWY_ROUND(x0s + t[i1]*(x1s - x0s));
    *y0scr = GWY_ROUND(y0s + t[i0]*(y1s - y0s));
    *y1scr = GWY_ROUND(y0s + t[i1]*(y1s - y0s));
}

/**
 * gwy_data_view_coords_xy_to_real:
 * @data_view: A data view.
 * @xscr: A screen x-coordinate relative to widget origin.
 * @yscr: A screen y-coordinate relative to widget origin.
 * @xreal: Where the physical x-coordinate in the data sample should be stored.
 * @yreal: Where the physical y-coordinate in the data sample should be stored.
 *
 * Recomputes screen coordinates relative to widget origin to physical
 * coordinates in the sample.
 **/
void
gwy_data_view_coords_xy_to_real(GwyDataView *data_view,
                                gint xscr, gint yscr,
                                gdouble *xreal, gdouble *yreal)
{
    g_return_if_fail(GWY_IS_DATA_VIEW(data_view));

    if (xreal)
        *xreal = (xscr + 0.5 - data_view->xoff) * data_view->xmeasure;
    if (yreal)
        *yreal = (yscr + 0.5 - data_view->yoff) * data_view->ymeasure;
}

/**
 * gwy_data_view_coords_real_to_xy:
 * @data_view: A data view.
 * @xreal: A physical x-coordinate in the data sample..
 * @yreal: A physical y-coordinate in the data sample.
 * @xscr: Where the screen x-coordinate relative to widget origin should be
 *        stored.
 * @yscr: Where the screen y-coordinate relative to widget origin should be
 *        stored.
 *
 * Recomputes physical coordinate in the sample to screen coordinate relative
 * to widget origin.
 **/
void
gwy_data_view_coords_real_to_xy(GwyDataView *data_view,
                                gdouble xreal, gdouble yreal,
                                gint *xscr, gint *yscr)
{
    g_return_if_fail(GWY_IS_DATA_VIEW(data_view));

    if (xscr)
        *xscr = floor(xreal/data_view->xmeasure) + data_view->xoff;
    if (yscr)
        *yscr = floor(yreal/data_view->ymeasure) + data_view->yoff;
}

/**
 * gwy_data_view_get_pixel_data_sizes:
 * @data_view: A data view.
 * @xres: Location to store x-resolution of displayed data (or %NULL).
 * @yres: Location to store y-resolution of displayed data (or %NULL).
 *
 * Obtains pixel dimensions of data displayed by a data view.
 *
 * This is a convenience method, the same values could be obtained
 * by gwy_data_field_get_xres() and gwy_data_field_get_yres() of the data
 * field displayed by the base layer.
 **/
void
gwy_data_view_get_pixel_data_sizes(GwyDataView *data_view,
                                   gint *xres,
                                   gint *yres)
{
    g_return_if_fail(GWY_IS_DATA_VIEW(data_view));

    if (xres)
        *xres = data_view->xres;
    if (yres)
        *yres = data_view->yres;
}

/**
 * gwy_data_view_get_real_data_sizes:
 * @data_view: A data view.
 * @xreal: Location to store physical x-dimension of the displayed data
 *         without excess (or %NULL).
 * @yreal: Location to store physical y-dimension of the displayed data
 *         without excess (or %NULL).
 *
 * Obtains physical dimensions of data displayed by a data view.
 *
 * Physical coordinates are always taken from data field displayed by the base
 * layer.  This is a convenience method, the same values could be obtained
 * by gwy_data_field_get_xreal() and gwy_data_field_get_yreal() of the data
 * field displayed by the base layer.
 **/
void
gwy_data_view_get_real_data_sizes(GwyDataView *data_view,
                                  gdouble *xreal,
                                  gdouble *yreal)
{
    g_return_if_fail(GWY_IS_DATA_VIEW(data_view));

    if (xreal)
        *xreal = data_view->xreal;
    if (yreal)
        *yreal = data_view->yreal;
}

/**
 * gwy_data_view_get_metric:
 * @data_view: A data view.
 * @metric: Metric matrix 2x2 (stored in sequentially by rows: m11, m12, m12,
 *          m22).
 *
 * Fills metric matrix for a data view.
 *
 * The metric matrix essentially transforms distances in physical coordinates
 * to screen distances.  It is to be used with functions like
 * gwy_math_find_nearest_point() and gwy_math_find_nearest_line() when the
 * distance should be screen-Euclidean.
 **/
void
gwy_data_view_get_metric(GwyDataView *data_view,
                         gdouble *metric)
{
    g_return_if_fail(GWY_IS_DATA_VIEW(data_view));
    g_return_if_fail(metric);

    metric[0] = 1.0/(data_view->xmeasure*data_view->xmeasure);
    metric[1] = metric[2] = 0.0;
    metric[3] = 1.0/(data_view->ymeasure*data_view->ymeasure);
}

/**
 * gwy_data_view_get_real_data_offsets:
 * @data_view: A data view.
 * @xoffset: Location to store physical x-offset of the top corner of
 *           displayed data without excess (or %NULL).
 * @yoffset: Location to store physical y-offset of the top corner of
 *           displayed data without excess (or %NULL).
 *
 * Obtains physical offsets of data displayed by a data view.
 *
 * Physical coordinates are always taken from data field displayed by the base
 * layer.  This is a convenience method, the same values could be obtained
 * by gwy_data_field_get_xoffset() and gwy_data_field_get_yoffset() of the data
 * field displayed by the base layer.
 *
 * Since: 2.16
 **/
void
gwy_data_view_get_real_data_offsets(GwyDataView *data_view,
                                    gdouble *xoffset,
                                    gdouble *yoffset)
{
    GwyDataViewPrivate *priv;

    g_return_if_fail(GWY_IS_DATA_VIEW(data_view));

    priv = GWY_DATA_VIEW_GET_PRIVATE(data_view);
    if (xoffset)
        *xoffset = priv->xoffset;
    if (yoffset)
        *yoffset = priv->yoffset;
}

/**
 * gwy_data_view_get_pixbuf:
 * @data_view: A data view.
 * @max_width: Pixbuf width that should not be exceeeded.  Value smaller than
 *             1 means unlimited size.
 * @max_height: Pixbuf height that should not be exceeeded.  Value smaller than
 *              1 means unlimited size.
 *
 * Creates and returns a pixbuf from the data view.
 *
 * If the data is not square, the resulting pixbuf is also nonsquare.
 * The returned pixbuf also never has an alpha channel.
 *
 * Returns: The pixbuf as a newly created #GdkPixbuf, it should be freed
 *          when no longer needed.  It is never larger than the actual data
 *          size, as @max_width and @max_height are only upper limits.
 **/
GdkPixbuf*
gwy_data_view_get_pixbuf(GwyDataView *data_view,
                         gint max_width,
                         gint max_height)
{
    GdkPixbuf *pixbuf;
    gint width, height, width_scaled, height_scaled;
    gdouble xscale, yscale, scale;

    g_return_val_if_fail(GWY_IS_DATA_VIEW(data_view), NULL);
    g_return_val_if_fail(data_view->pixbuf, NULL);

    width = gdk_pixbuf_get_width(data_view->pixbuf);
    height = gdk_pixbuf_get_height(data_view->pixbuf);
    xscale = (max_width > 0) ? (gdouble)max_width/width : 1.0;
    yscale = (max_height > 0) ? (gdouble)max_height/height : 1.0;
    scale = MIN(MIN(xscale, yscale), 1.0);
    width_scaled = (gint)(scale*width);
    height_scaled = (gint)(scale*height);
    if (max_width)
        width_scaled = CLAMP(width_scaled, 1, max_width);
    if (max_height)
        height_scaled = CLAMP(height_scaled, 1, max_height);

    pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE,
                            BITS_PER_SAMPLE, width_scaled, height_scaled);
    gwy_debug_objects_creation(G_OBJECT(pixbuf));
    gdk_pixbuf_scale(data_view->pixbuf, pixbuf, 0, 0,
                     width_scaled, height_scaled, 0.0, 0.0,
                     scale, scale, GDK_INTERP_TILES);

    return pixbuf;
}

/* A few more pixbuf exporting functions and we will be out of names... */
/**
 * gwy_data_view_export_pixbuf:
 * @data_view: A data view.
 * @zoom: Zoom to export data with (unrelated to data view zoom).
 * @draw_alpha: %TRUE to draw alpha layer (mask).
 * @draw_top: %TRUE to draw top layer (selection).
 *
 * Exports data view to a pixbuf.
 *
 * Returns: A newly created pixbuf, it must be freed by caller.
 **/
GdkPixbuf*
gwy_data_view_export_pixbuf(GwyDataView *data_view,
                            gdouble zoom,
                            gboolean draw_alpha,
                            gboolean draw_top)
{
    GdkPixbuf *bpixbuf, *apixbuf, *pixbuf, *aux_pixbuf;
    GwySelection *selection;
    GdkDrawable *drawable;
    gint width, height, rowstride, i;
    const gchar *key;
    guchar *src, *dst;

    g_return_val_if_fail(GWY_IS_DATA_VIEW(data_view), NULL);
    g_return_val_if_fail(GTK_WIDGET_REALIZED(data_view), NULL);
    g_return_val_if_fail(zoom > 0.0, NULL);
    g_return_val_if_fail(data_view->base_layer, NULL);

    if (data_view->realsquare) {
        gdouble scale = zoom*MAX(data_view->xres/data_view->xreal,
                                 data_view->yres/data_view->yreal);
        width = GWY_ROUND(scale * data_view->xreal);
        height = GWY_ROUND(scale * data_view->yreal);
    }
    else {
        width = zoom * data_view->xres;
        height = zoom * data_view->yres;
    }
    width = MAX(width, 2);
    height = MAX(height, 2);
    pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, BITS_PER_SAMPLE,
                            width, height);
    gwy_debug_objects_creation(G_OBJECT(pixbuf));

    /* Pixmap layers */
    bpixbuf = gwy_pixmap_layer_paint(data_view->base_layer);
    if (draw_alpha && data_view->alpha_layer)
        apixbuf = gwy_pixmap_layer_paint(data_view->alpha_layer);
    else
        apixbuf = NULL;

    if (bpixbuf) {
        if (apixbuf) {
            aux_pixbuf = gdk_pixbuf_copy(bpixbuf);
            simple_gdk_pixbuf_composite(apixbuf, aux_pixbuf);
            simple_gdk_pixbuf_scale_or_copy(aux_pixbuf, pixbuf);
            g_object_unref(aux_pixbuf);
        }
        else
            simple_gdk_pixbuf_scale_or_copy(bpixbuf, pixbuf);
    }
    else {
        gdk_pixbuf_fill(pixbuf, 0x00000000);
        if (apixbuf)
            simple_gdk_pixbuf_scale_or_copy(apixbuf, pixbuf);
    }

    if (!draw_top || !data_view->top_layer)
        return pixbuf;

    /* Avoid complicated hacks when there is no selection to draw */
    key = gwy_vector_layer_get_selection_key(data_view->top_layer);
    selection = NULL;
    gwy_container_gis_object_by_name(data_view->data, key, &selection);
    if (!selection || !gwy_selection_get_data(selection, NULL))
        return pixbuf;

    /* XXX: Now the ugly part begins, use Gdk to draw selection on Xserver
     * and fetch it back */
    drawable = gdk_pixmap_new(GTK_WIDGET(data_view)->window, width, height, -1);
    gdk_draw_rectangle(drawable, GTK_WIDGET(data_view)->style->black_gc,
                       TRUE, 0, 0, width, height);
    gwy_vector_layer_draw(data_view->top_layer, drawable,
                          GWY_RENDERING_TARGET_PIXMAP_IMAGE);
    aux_pixbuf = gdk_pixbuf_get_from_drawable(NULL, drawable, NULL,
                                              0, 0, 0, 0, width, height);

    src = gdk_pixbuf_get_pixels(aux_pixbuf);
    dst = gdk_pixbuf_get_pixels(pixbuf);
    rowstride = gdk_pixbuf_get_rowstride(aux_pixbuf);
    g_assert(rowstride == gdk_pixbuf_get_rowstride(pixbuf));
    for (i = height*rowstride; i; i--, src++, dst++)
        *dst ^= *src;
    g_object_unref(aux_pixbuf);

    return pixbuf;
}

/**
 * gwy_data_view_set_data_prefix:
 * @data_view: A data view.
 * @prefix: Container prefix for data  (eg. "/0/data").
 *
 * Sets the prefix for the container data channel to display in a data view.
 *
 * This function only affects where the data view itself takes parameters
 * from, it does not affect layer keys.
 **/
void
gwy_data_view_set_data_prefix(GwyDataView *data_view,
                              const gchar *prefix)
{
    GQuark quark;

    g_return_if_fail(GWY_IS_DATA_VIEW(data_view));

    quark = prefix ? g_quark_from_string(prefix) : 0;
    if (quark == data_view->data_prefix)
        return;

    gwy_data_view_disconnect_data(data_view);
    data_view->data_prefix = quark;
    if (quark)
        gwy_data_view_connect_data(data_view);

    g_object_notify(G_OBJECT(data_view), "data-prefix");
}

/**
 * gwy_data_view_get_data_prefix:
 * @data_view: A data view.
 *
 * Gets the prefix for the container data channel that the data view is
 * currently set to display.
 *
 * Returns: The container data prefix (eg. "/0/data").
 **/
const gchar*
gwy_data_view_get_data_prefix(GwyDataView *data_view)
{
    g_return_val_if_fail(GWY_IS_DATA_VIEW(data_view), NULL);

    return g_quark_to_string(data_view->data_prefix);
}

static void
gwy_data_view_square_changed(GwyDataView *data_view,
                             GQuark quark)
{
    gboolean realsquare;

    realsquare = data_view->realsquare;
    if (!gwy_container_gis_boolean(data_view->data, quark, &realsquare))
        realsquare = FALSE;
    if (realsquare != data_view->realsquare) {
        GtkWidget *widget = GTK_WIDGET(data_view);

        data_view->realsquare = realsquare;
        if (GTK_WIDGET_REALIZED(widget)) {
            gtk_widget_queue_resize(widget);
            g_signal_emit(widget, data_view_signals[RESIZED], 0);
        }
    }
}

static void
gwy_data_view_connect_data(GwyDataView *data_view)
{
    static const gchar ichg[] = "item-changed::";
    gchar *s;

    g_return_if_fail(data_view->data);
    g_return_if_fail(data_view->square_hid == 0);
    if (!data_view->data_prefix)
        return;

    s = g_strconcat(ichg,
                    g_quark_to_string(data_view->data_prefix),
                    "/realsquare",
                    NULL);
    data_view->square_hid
        = g_signal_connect_swapped(data_view->data, s,
                                   G_CALLBACK(gwy_data_view_square_changed),
                                   data_view);
    gwy_data_view_square_changed(data_view,
                                 g_quark_from_string(s + sizeof(ichg) - 1));
    g_free(s);
}

static void
gwy_data_view_disconnect_data(GwyDataView *data_view)
{
    gwy_signal_handler_disconnect(data_view->data, data_view->square_hid);
}

/************************** Documentation ****************************/

/**
 * SECTION:gwydataview
 * @title: GwyDataView
 * @short_description: Data field displaying area
 * @see_also: #GwyDataWindow -- window combining data view with other controls,
 *            #GwyDataViewLayer -- layers a data view is composed of,
 *            <link linkend="libgwydraw-gwypixfield">gwypixfield</link> --
 *            low level functions for painting data fields,
 *            #Gwy3DView -- OpenGL 3D data display
 *
 * #GwyDataView is a basic two-dimensional data display widget.  The actual
 * rendering is performed by one or more #GwyDataViewLayer's, pluggable into
 * the data view.  Each layer generally displays different data field from
 * the container supplied to gwy_data_view_new().
 *
 * A base layer (set with gwy_data_view_set_base_layer()) must be always
 * present, and normally it is always a #GwyLayerBasic.
 *
 * Other layers are optional.  Middle, or alpha, layer (set with
 * gwy_data_view_set_alpha_layer()) used to display masks is normally always
 * a #GwyLayerMask.  Top layer, if present, is a #GwyVectorLayer allowing to
 * draw selections with mouse and otherwise interace with the view, it is set
 * with gwy_data_view_set_top_layer().
 *
 * The size of a data view is affected by two factors: zoom and outer
 * constraints. If an explicit size set by window manager or by Gtk+ means, the
 * view scales the displayed data to fit into this size (while keeping x/y
 * ratio). Zoom controlls the size a data view requests, and can be set with
 * gwy_data_view_set_zoom().
 *
 * Several helper functions are available for transformation between screen
 * coordinates in the view and physical coordinates in the displayed data
 * field: gwy_data_view_coords_xy_to_real(), gwy_data_view_get_xmeasure(),
 * gwy_data_view_get_hexcess(), and others. Physical coordinates are always
 * taken from data field displayed by base layer.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
