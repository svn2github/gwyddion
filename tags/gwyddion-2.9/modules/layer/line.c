/*
 *  @(#) $Id$
 *  Copyright (C) 2003-2007 David Necas (Yeti), Petr Klapetek.
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
#include <pango/pangocairo.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwydebugobjects.h>
#include <libgwyddion/gwymath.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwymodule/gwymodule-layer.h>

#include "layer.h"

#define GWY_TYPE_LAYER_LINE            (gwy_layer_line_get_type())
#define GWY_LAYER_LINE(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_LAYER_LINE, GwyLayerLine))
#define GWY_IS_LAYER_LINE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_LAYER_LINE))
#define GWY_LAYER_LINE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_LAYER_LINE, GwyLayerLineClass))

#define GWY_TYPE_SELECTION_LINE            (gwy_selection_line_get_type())
#define GWY_SELECTION_LINE(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_SELECTION_LINE, GwySelectionLine))
#define GWY_IS_SELECTION_LINE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_SELECTION_LINE))
#define GWY_SELECTION_LINE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_SELECTION_LINE, GwySelectionLineClass))

enum {
    OBJECT_SIZE = 4
};

enum {
    PROP_0,
    PROP_LINE_NUMBERS
};

typedef struct _GwyLayerLine          GwyLayerLine;
typedef struct _GwyLayerLineClass     GwyLayerLineClass;
typedef struct _GwySelectionLine      GwySelectionLine;
typedef struct _GwySelectionLineClass GwySelectionLineClass;

struct _GwyLayerLine {
    GwyVectorLayer parent_instance;

    GdkCursor *near_cursor;
    GdkCursor *nearline_cursor;
    GdkCursor *move_cursor;

    /* Text rendering, these are approximate values */
    PangoFontDescription *fontdesc;
    gint digit_width;
    gint digit_height;

    /* Properties */
    gboolean line_numbers;

    /* Dynamic state */
    gint endpoint;  /* We need information which endpoint is moving; old
                       versions abused @selecting for that, but it's confusing
                       when one layer uses it for something else than all
                       others. */
    gboolean moving_line;
    gboolean restricted;
    gdouble lmove_x;
    gdouble lmove_y;
    GPtrArray *line_labels;
};

struct _GwyLayerLineClass {
    GwyVectorLayerClass parent_class;
};

struct _GwySelectionLine {
    GwySelection parent_instance;
};

struct _GwySelectionLineClass {
    GwySelectionClass parent_class;
};

static gboolean   module_register                (void);
static GType      gwy_layer_line_get_type        (void) G_GNUC_CONST;
static GType      gwy_selection_line_get_type    (void) G_GNUC_CONST;
static void       gwy_layer_line_set_property    (GObject *object,
                                                  guint prop_id,
                                                  const GValue *value,
                                                  GParamSpec *pspec);
static void       gwy_layer_line_get_property    (GObject*object,
                                                  guint prop_id,
                                                  GValue *value,
                                                  GParamSpec *pspec);
static void       gwy_layer_line_draw            (GwyVectorLayer *layer,
                                                  GdkDrawable *drawable,
                                                  GwyRenderingTarget target);
static void       gwy_layer_line_draw_object     (GwyVectorLayer *layer,
                                                  GdkDrawable *drawable,
                                                  GwyRenderingTarget target,
                                                  gint i);
static void       gwy_layer_line_setup_label     (GwyLayerLine *layer,
                                                  GdkDrawable *drawable,
                                                  gint i);
static void       gwy_layer_line_draw_label      (GwyVectorLayer *layer,
                                                  GdkDrawable *drawable,
                                                  gdouble zoom,
                                                  gint i,
                                                  gint xt,
                                                  gint yt);
static GdkPixbuf* gwy_layer_line_render_string_bw(GwyLayerLine *layer,
                                                  gdouble zoom,
                                                  const gchar *markup);
static gboolean   gwy_layer_line_motion_notify   (GwyVectorLayer *layer,
                                                  GdkEventMotion *event);
static gboolean   gwy_layer_line_move_line       (GwyVectorLayer *layer,
                                                  gdouble xreal,
                                                  gdouble yreal);
static gboolean   gwy_layer_line_button_pressed  (GwyVectorLayer *layer,
                                                  GdkEventButton *event);
static gboolean   gwy_layer_line_button_released (GwyVectorLayer *layer,
                                                  GdkEventButton *event);
static void       gwy_layer_line_set_line_numbers(GwyLayerLine *layer,
                                                  gboolean line_numbers);
static void       gwy_layer_line_realize         (GwyDataViewLayer *dlayer);
static void       gwy_layer_line_unrealize       (GwyDataViewLayer *dlayer);
static gint       gwy_layer_line_near_line       (GwyVectorLayer *layer,
                                                  gdouble xreal,
                                                  gdouble yreal);
static gint       gwy_layer_line_near_point      (GwyVectorLayer *layer,
                                                  gdouble xreal,
                                                  gdouble yreal);
static void       gwy_layer_line_restrict_angle  (GwyDataView *data_view,
                                                  gint endpoint,
                                                  gint x,
                                                  gint y,
                                                  gdouble *xy);

/* Allow to express intent. */
#define gwy_layer_line_undraw        gwy_layer_line_draw
#define gwy_layer_line_undraw_object gwy_layer_line_draw_object

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Layer allowing selection of arbitrary straight lines."),
    "Yeti <yeti@gwyddion.net>",
    "3.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

GWY_MODULE_QUERY(module_info)

G_DEFINE_TYPE(GwySelectionLine, gwy_selection_line, GWY_TYPE_SELECTION)
G_DEFINE_TYPE(GwyLayerLine, gwy_layer_line, GWY_TYPE_VECTOR_LAYER)

static gboolean
module_register(void)
{
    gwy_layer_func_register(GWY_TYPE_LAYER_LINE);
    return TRUE;
}

static void
gwy_selection_line_class_init(GwySelectionLineClass *klass)
{
    GwySelectionClass *sel_class = GWY_SELECTION_CLASS(klass);

    sel_class->object_size = OBJECT_SIZE;
}

static void
gwy_layer_line_class_init(GwyLayerLineClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GwyDataViewLayerClass *layer_class = GWY_DATA_VIEW_LAYER_CLASS(klass);
    GwyVectorLayerClass *vector_class = GWY_VECTOR_LAYER_CLASS(klass);

    gwy_debug("");

    gobject_class->set_property = gwy_layer_line_set_property;
    gobject_class->get_property = gwy_layer_line_get_property;

    layer_class->realize = gwy_layer_line_realize;
    layer_class->unrealize = gwy_layer_line_unrealize;

    vector_class->selection_type = GWY_TYPE_SELECTION_LINE;
    vector_class->draw = gwy_layer_line_draw;
    vector_class->motion_notify = gwy_layer_line_motion_notify;
    vector_class->button_press = gwy_layer_line_button_pressed;
    vector_class->button_release = gwy_layer_line_button_released;

    g_object_class_install_property
        (gobject_class,
         PROP_LINE_NUMBERS,
         g_param_spec_boolean("line-numbers",
                              "Number lines",
                              "Whether to attach line numbers to the lines.",
                              TRUE,
                              G_PARAM_READWRITE));
}

static void
gwy_selection_line_init(GwySelectionLine *selection)
{
    /* Set max. number of objects to one */
    g_array_set_size(GWY_SELECTION(selection)->objects, OBJECT_SIZE);
}

static void
gwy_layer_line_init(GwyLayerLine *layer)
{
    layer->line_numbers = TRUE;
}

static void
gwy_layer_line_set_property(GObject *object,
                             guint prop_id,
                             const GValue *value,
                             GParamSpec *pspec)
{
    GwyLayerLine *layer = GWY_LAYER_LINE(object);

    switch (prop_id) {
        case PROP_LINE_NUMBERS:
        gwy_layer_line_set_line_numbers(layer, g_value_get_boolean(value));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_layer_line_get_property(GObject*object,
                             guint prop_id,
                             GValue *value,
                             GParamSpec *pspec)
{
    GwyLayerLine *layer = GWY_LAYER_LINE(object);

    switch (prop_id) {
        case PROP_LINE_NUMBERS:
        g_value_set_boolean(value, layer->line_numbers);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_layer_line_set_line_numbers(GwyLayerLine *layer,
                                gboolean line_numbers)
{
    GwyVectorLayer *vector_layer;
    GtkWidget *parent;

    g_return_if_fail(GWY_IS_LAYER_LINE(layer));
    vector_layer = GWY_VECTOR_LAYER(layer);
    parent = GWY_DATA_VIEW_LAYER(layer)->parent;

    if (line_numbers == layer->line_numbers)
        return;

    if (parent && GTK_WIDGET_REALIZED(parent))
        gwy_layer_line_undraw(vector_layer, parent->window,
                              GWY_RENDERING_TARGET_SCREEN);
    layer->line_numbers = line_numbers;
    if (parent && GTK_WIDGET_REALIZED(parent))
        gwy_layer_line_draw(vector_layer, parent->window,
                            GWY_RENDERING_TARGET_SCREEN);
    g_object_notify(G_OBJECT(layer), "line-numbers");
}

static void
gwy_layer_line_draw(GwyVectorLayer *layer,
                    GdkDrawable *drawable,
                    GwyRenderingTarget target)
{
    gint i, n;

    g_return_if_fail(GDK_IS_DRAWABLE(drawable));

    if (!layer->selection)
        return;

    n = gwy_selection_get_data(layer->selection, NULL);
    for (i = 0; i < n; i++)
        gwy_layer_line_draw_object(layer, drawable, target, i);
}

static void
gwy_layer_line_draw_object(GwyVectorLayer *layer,
                           GdkDrawable *drawable,
                           GwyRenderingTarget target,
                           gint i)
{
    GwyDataView *data_view;
    GwyLayerLine *layer_line;
    gint xi0, yi0, xi1, yi1, xt, yt, width, height;
    gdouble xy[OBJECT_SIZE], xreal, yreal, zoom;
    gboolean has_object;

    g_return_if_fail(GDK_IS_DRAWABLE(drawable));
    data_view = GWY_DATA_VIEW(GWY_DATA_VIEW_LAYER(layer)->parent);

    has_object = gwy_selection_get_object(layer->selection, i, xy);
    g_return_if_fail(has_object);

    switch (target) {
        case GWY_RENDERING_TARGET_SCREEN:
        gwy_data_view_coords_real_to_xy(data_view, xy[0], xy[1], &xi0, &yi0);
        gwy_data_view_coords_real_to_xy(data_view, xy[2], xy[3], &xi1, &yi1);
        gwy_data_view_coords_xy_clamp(data_view, &xi0, &yi0);
        gwy_data_view_coords_xy_clamp(data_view, &xi1, &yi1);
        break;

        case GWY_RENDERING_TARGET_PIXMAP_IMAGE:
        gwy_data_view_get_real_data_sizes(data_view, &xreal, &yreal);
        gdk_drawable_get_size(drawable, &width, &height);
        xi0 = floor(xy[0]*width/xreal);
        yi0 = floor(xy[1]*height/yreal);
        xi1 = floor(xy[2]*width/xreal);
        yi1 = floor(xy[3]*height/yreal);
        break;

        default:
        g_return_if_reached();
        break;
    }
    gdk_draw_line(drawable, layer->gc, xi0, yi0, xi1, yi1);

    layer_line = GWY_LAYER_LINE(layer);
    if (!layer_line->line_numbers)
        return;

    xt = (xi0 + xi1)/2 + 1;
    yt = (yi0 + yi1)/2;
    switch (target) {
        case GWY_RENDERING_TARGET_SCREEN:
        gwy_layer_line_setup_label(layer_line, drawable, i);
        gdk_draw_drawable(drawable, layer->gc,
                          g_ptr_array_index(layer_line->line_labels, i),
                          0, 0, xt, yt, -1, -1);
        break;

        case GWY_RENDERING_TARGET_PIXMAP_IMAGE:
        gwy_data_view_get_pixel_data_sizes(data_view, &xi1, &yi1);
        gdk_drawable_get_size(drawable, &width, &height);
        zoom = sqrt(((gdouble)(width*height))/(xi1*yi1));

        gwy_layer_line_draw_label(layer, drawable, zoom, i, xt, yt);
        break;

        default:
        g_return_if_reached();
        break;
    }
}

static void
gwy_layer_line_setup_label(GwyLayerLine *layer_line,
                           GdkDrawable *drawable,
                           gint i)
{
    GdkPixmap *pixmap;
    GdkPixbuf *pixbuf;
    gchar buffer[8];
    GdkGC *gc;

    if (!layer_line->line_labels)
        layer_line->line_labels = g_ptr_array_new();

    if (i < layer_line->line_labels->len
        && GDK_IS_DRAWABLE(g_ptr_array_index(layer_line->line_labels, i)))
        return;

    if (i >= layer_line->line_labels->len)
        g_ptr_array_set_size(layer_line->line_labels, i+1);

    g_snprintf(buffer, sizeof(buffer), "%d", i+1);
    pixbuf = gwy_layer_line_render_string_bw(layer_line, 1.0, buffer);
    pixmap = gdk_pixmap_new(drawable,
                            gdk_pixbuf_get_width(pixbuf),
                            gdk_pixbuf_get_height(pixbuf),
                            -1);
    g_ptr_array_index(layer_line->line_labels, i) = pixmap;

    gc = gdk_gc_new(GDK_DRAWABLE(pixmap));
    gdk_gc_set_function(gc, GDK_COPY);
    gdk_draw_pixbuf(pixmap, gc, pixbuf, 0, 0, 0, 0, -1, -1,
                    GDK_RGB_DITHER_NONE, 0, 0);
    g_object_unref(gc);
    g_object_unref(pixbuf);
}

static void
gwy_layer_line_draw_label(GwyVectorLayer *layer,
                          GdkDrawable *drawable,
                          gdouble zoom,
                          gint i,
                          gint xt,
                          gint yt)
{
    GwyLayerLine *layer_line;
    GdkGCValues gcvalues;
    GdkPixbuf *pixbuf;
    gchar buffer[48];

    layer_line = GWY_LAYER_LINE(layer);
    g_snprintf(buffer, sizeof(buffer), "<span size=\"%d\">%d</span>",
               GWY_ROUND(MAX(2048.0, zoom*12*1024)), i+1);
    pixbuf = gwy_layer_line_render_string_bw(layer_line, zoom, buffer);

    gdk_gc_get_values(layer->gc, &gcvalues);
    gdk_gc_set_function(layer->gc, GDK_XOR);
    gdk_draw_pixbuf(drawable, layer->gc, pixbuf, 0, 0, xt, yt, -1, -1,
                    GDK_RGB_DITHER_NONE, 0, 0);
    gdk_gc_set_values(layer->gc, &gcvalues, GDK_GC_FUNCTION);
    g_object_unref(pixbuf);
}

static GdkPixbuf*
gwy_layer_line_render_string_bw(GwyLayerLine *layer,
                                gdouble zoom,
                                const gchar *markup)
{
    PangoLayout *layout;
    GdkPixbuf *pixbuf;
    gint wwidth, wheight, wstride, cwidth, cheight, pstride;
    cairo_t *cr;
    cairo_surface_t *surface;
    guchar *data, *pixels;
    gint i, j;

    wwidth = GWY_ROUND(3*(zoom*layer->digit_width));
    wstride = (wwidth + 31)/32*4;
    wwidth = wstride*8;
    wheight = GWY_ROUND(1.5*zoom*layer->digit_height);
    gwy_debug("w-layout size: %d %d", wwidth, wheight);

    data = g_new0(guchar, wstride*wheight);
    surface = cairo_image_surface_create_for_data(data, CAIRO_FORMAT_A1,
                                                  wwidth, wheight, wstride);
    cr = cairo_create(surface);
    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 1.0);

    layout = pango_cairo_create_layout(cr);
    pango_layout_set_font_description(layout, layer->fontdesc);
    pango_layout_set_markup(layout, markup, -1);
    pango_layout_get_pixel_size(layout, &cwidth, &cheight);
    pango_cairo_show_layout(cr, layout);

    gwy_debug("c-layout size: %d %d", cwidth, cheight);
    if (cwidth > wwidth || cheight > wheight) {
        g_warning("Cairo image surface is not large enough for text");
        cwidth = MIN(cwidth, wwidth);
        cheight = MIN(cheight, wheight);
    }

    pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, cwidth, cheight);
    pstride = gdk_pixbuf_get_rowstride(pixbuf);
    pixels = gdk_pixbuf_get_pixels(pixbuf);
    gdk_pixbuf_fill(pixbuf, 0x00000000);

    for (i = 0; i < cheight; i++) {
        const guint32 *crow = (const guint32*)(data + wstride*i);
        guchar *prow = pixels + pstride*i;
        guint32 bit;

        bit = 1;
        for (j = 0; j < cwidth; j++) {
            prow[0] = prow[1] = prow[2] = (*crow & bit) ? 255 : 0;
            if (!(bit <<= 1)) {
                bit = 1;
                crow++;
            }
            prow += 3;
        }
    }

    g_object_unref(layout);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    g_free(data);

    return pixbuf;
}

static gboolean
gwy_layer_line_motion_notify(GwyVectorLayer *layer,
                             GdkEventMotion *event)
{
    GwyDataView *data_view;
    GwyLayerLine *layer_line;
    GdkWindow *window;
    GdkCursor *cursor;
    gint x, y, i, j;
    gdouble xreal, yreal, xy[OBJECT_SIZE];
    gboolean restricted;

    if (!layer->selection)
        return FALSE;

    /* FIXME: No cursor change hint -- a bit too crude? */
    if (!layer->editable)
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
    restricted = event->state & GDK_SHIFT_MASK;
    gwy_debug("x = %d, y = %d", x, y);
    gwy_data_view_coords_xy_clamp(data_view, &x, &y);
    gwy_data_view_coords_xy_to_real(data_view, x, y, &xreal, &yreal);
    layer_line = GWY_LAYER_LINE(layer);
    if (layer->button && layer_line->moving_line)
        return gwy_layer_line_move_line(layer, xreal, yreal);

    if (i > -1)
        gwy_selection_get_object(layer->selection, i, xy);
    /* here is normally something like
    if (i > -1 && xreal == xy[2] && yreal == xy[3])
        return FALSE;
       but we need to know which endpoint is moving...
     */

    if (!layer->button) {
        j = gwy_layer_line_near_line(layer, xreal, yreal);
        i = gwy_layer_line_near_point(layer, xreal, yreal);
        if (i == -1 && j >= 0)
            cursor = layer_line->nearline_cursor;
        else
            cursor = (i == -1) ? NULL : layer_line->near_cursor;
        gdk_window_set_cursor(window, cursor);
        return FALSE;
    }

    g_assert(layer->selecting != -1);
    layer_line->restricted = restricted;
    gwy_layer_line_undraw_object(layer, window, GWY_RENDERING_TARGET_SCREEN, i);
    if (restricted)
        gwy_layer_line_restrict_angle(data_view, layer_line->endpoint,
                                      event->x, event->y, xy);
    else {
        xy[2*layer_line->endpoint + 0] = xreal;
        xy[2*layer_line->endpoint + 1] = yreal;
    }
    gwy_selection_set_object(layer->selection, i, xy);
    gwy_layer_line_draw_object(layer, window, GWY_RENDERING_TARGET_SCREEN, i);

    return FALSE;
}

static gboolean
gwy_layer_line_move_line(GwyVectorLayer *layer,
                         gdouble xreal, gdouble yreal)
{
    GwyDataView *data_view;
    GdkWindow *window;
    gdouble coords[OBJECT_SIZE], xy[OBJECT_SIZE];
    gint x, y, i;

    data_view = GWY_DATA_VIEW(GWY_DATA_VIEW_LAYER(layer)->parent);
    window = GTK_WIDGET(data_view)->window;

    g_return_val_if_fail(layer->selecting != -1, FALSE);

    i = layer->selecting;
    gwy_selection_get_object(layer->selection, i, xy);

    /* calculate wanted new coordinates of the first endpoint */
    coords[0] = xreal + GWY_LAYER_LINE(layer)->lmove_x;
    coords[1] = yreal + GWY_LAYER_LINE(layer)->lmove_y;
    if (coords[0] == xy[0] && coords[1] == xy[1])
        return FALSE;

    /* clamp them to get best possible */
    gwy_data_view_coords_real_to_xy(data_view, coords[0], coords[1], &x, &y);
    gwy_data_view_coords_xy_clamp(data_view, &x, &y);
    gwy_data_view_coords_xy_to_real(data_view, x, y, &coords[0], &coords[1]);

    /* calculate wanted new coordinates of the second endpoint */
    coords[2] = (coords[0] - xy[0]) + xy[2];
    coords[3] = (coords[1] - xy[1]) + xy[3];

    if (coords[2] == xy[2] && coords[3] == xy[3])
        return FALSE;

    /* clamp them to get best possible */
    gwy_data_view_coords_real_to_xy(data_view, coords[2], coords[3], &x, &y);
    gwy_data_view_coords_xy_clamp(data_view, &x, &y);
    gwy_data_view_coords_xy_to_real(data_view, x, y, &coords[2], &coords[3]);

    /* recompute the first point */
    coords[0] = (coords[2] - xy[2]) + xy[0];
    coords[1] = (coords[3] - xy[3]) + xy[1];

    if (coords[0] == xy[0] && coords[1] == xy[1])
        return FALSE;

    gwy_layer_line_undraw_object(layer, window,
                                 GWY_RENDERING_TARGET_SCREEN, i);
    gwy_debug("%d %g %g %g %g", i, coords[0], coords[1], coords[2], coords[3]);
    gwy_selection_set_object(layer->selection, i, coords);
    gwy_layer_line_draw_object(layer, window,
                               GWY_RENDERING_TARGET_SCREEN, i);

    return FALSE;
}

static gboolean
gwy_layer_line_button_pressed(GwyVectorLayer *layer,
                              GdkEventButton *event)
{
    GwyDataView *data_view;
    GdkWindow *window;
    GwyLayerLine *layer_line;
    gint x, y, i, j;
    gdouble xreal, yreal, xy[OBJECT_SIZE];
    gboolean restricted;

    if (!layer->selection)
        return FALSE;

    if (event->button != 1)
        return FALSE;

    data_view = GWY_DATA_VIEW(GWY_DATA_VIEW_LAYER(layer)->parent);
    g_return_val_if_fail(data_view, FALSE);
    window = GTK_WIDGET(data_view)->window;

    x = event->x;
    y = event->y;
    restricted = event->state & GDK_SHIFT_MASK;
    gwy_data_view_coords_xy_clamp(data_view, &x, &y);
    gwy_debug("[%d,%d]", x, y);
    /* do nothing when we are outside */
    if (x != event->x || y != event->y)
        return FALSE;

    gwy_data_view_coords_xy_to_real(data_view, x, y, &xreal, &yreal);
    layer_line = GWY_LAYER_LINE(layer);

    j = gwy_layer_line_near_line(layer, xreal, yreal);
    i = gwy_layer_line_near_point(layer, xreal, yreal);
    /* just emit "object-chosen" when selection is not editable */
    if (!layer->editable) {
        if (j >= 0)
            gwy_vector_layer_object_chosen(layer, j);
        if (i >= 0)
            gwy_vector_layer_object_chosen(layer, i/2);
        return FALSE;
    }
    /* handle existing selection */
    if (i == -1 && j >= 0) {
        gwy_selection_get_object(layer->selection, j, xy);
        layer->selecting = j;
        gwy_layer_line_undraw_object(layer, window,
                                     GWY_RENDERING_TARGET_SCREEN, j);
        layer_line->moving_line = TRUE;
        layer_line->lmove_x = xy[0] - xreal;
        layer_line->lmove_y = xy[1] - yreal;
    }
    else {
        if (i >= 0) {
            layer_line->endpoint = i % 2;
            i /= 2;
            layer->selecting = i;
            gwy_layer_line_undraw_object(layer, window,
                                         GWY_RENDERING_TARGET_SCREEN, i);
            if (restricted)
                gwy_layer_line_restrict_angle(data_view, layer_line->endpoint,
                                              event->x, event->y, xy);
            else {
                xy[2*layer_line->endpoint + 0] = xreal;
                xy[2*layer_line->endpoint + 1] = yreal;
            }
        }
        else {
            xy[2] = xy[0] = xreal;
            xy[3] = xy[1] = yreal;

            /* add an object, or do nothing when maximum is reached */
            i = -2;
            if (gwy_selection_is_full(layer->selection)) {
                if (gwy_selection_get_max_objects(layer->selection) > 1)
                    return FALSE;
                i = 0;
                gwy_layer_line_undraw_object(layer, window,
                                             GWY_RENDERING_TARGET_SCREEN, i);
            }
            layer->selecting = 0;    /* avoid "update" signal emission */
            layer->selecting = gwy_selection_set_object(layer->selection, i,
                                                        xy);
            /* When we start a new selection, second endpoint is moving */
            layer_line->endpoint = 1;
        }
    }
    layer_line->restricted = restricted;
    layer->button = event->button;
    gwy_layer_line_draw_object(layer, window,
                               GWY_RENDERING_TARGET_SCREEN, layer->selecting);

    gdk_window_set_cursor(window, layer_line->move_cursor);
    gwy_vector_layer_object_chosen(layer, layer->selecting);

    return FALSE;
}

static gboolean
gwy_layer_line_button_released(GwyVectorLayer *layer,
                               GdkEventButton *event)
{
    GwyDataView *data_view;
    GdkWindow *window;
    GdkCursor *cursor;
    GwyLayerLine *layer_line;
    gint x, y, i, j;
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
    layer_line = GWY_LAYER_LINE(layer);
    if (layer_line->moving_line)
        gwy_layer_line_move_line(layer, xreal, yreal);
    else {
        gwy_selection_get_object(layer->selection, i, xy);
        gwy_layer_line_undraw_object(layer, window,
                                     GWY_RENDERING_TARGET_SCREEN, i);
        if (layer_line->restricted)
            gwy_layer_line_restrict_angle(data_view, layer_line->endpoint,
                                          event->x, event->y, xy);
        else {
            xy[2*layer_line->endpoint + 0] = xreal;
            xy[2*layer_line->endpoint + 1] = yreal;
        }
        /* XXX this can happen also with rounding errors */
        if (xy[0] == xy[2] && xy[1] == xy[3])
            gwy_selection_delete_object(layer->selection, i);
        else
            gwy_layer_line_draw_object(layer, window,
                                       GWY_RENDERING_TARGET_SCREEN, i);
    }

    layer_line->moving_line = FALSE;
    layer->selecting = -1;
    j = gwy_layer_line_near_line(layer, xreal, yreal);
    i = gwy_layer_line_near_point(layer, xreal, yreal);
    if (outside)
        cursor = NULL;
    else if (i == -1 && j >= 0)
        cursor = layer_line->nearline_cursor;
    else
        cursor = (i == -1) ? NULL : layer_line->near_cursor;
    gdk_window_set_cursor(window, cursor);
    gwy_selection_finished(layer->selection);

    return FALSE;
}

static void
gwy_layer_line_realize(GwyDataViewLayer *dlayer)
{
    PangoFontDescription *fontdesc;
    PangoContext *context;
    PangoLayout *layout;
    GwyLayerLine *layer;
    GdkDisplay *display;
    gint size;

    gwy_debug("");

    GWY_DATA_VIEW_LAYER_CLASS(gwy_layer_line_parent_class)->realize(dlayer);
    layer = GWY_LAYER_LINE(dlayer);

    display = gtk_widget_get_display(dlayer->parent);
    layer->near_cursor = gdk_cursor_new_for_display(display, GDK_DOTBOX);
    layer->move_cursor = gdk_cursor_new_for_display(display, GDK_CROSS);
    layer->nearline_cursor = gdk_cursor_new_for_display(display, GDK_FLEUR);

    context = gtk_widget_get_pango_context(dlayer->parent);
    fontdesc = pango_context_get_font_description(context);
    layer->fontdesc = pango_font_description_copy(fontdesc);
    size = pango_font_description_get_size(layer->fontdesc);
    pango_font_description_set_size(layer->fontdesc, (gint)(1.22*size + 0.5));
    pango_font_description_set_weight(layer->fontdesc, PANGO_WEIGHT_BOLD);
    layout = pango_layout_new(context);
    pango_layout_set_font_description(layout, layer->fontdesc);
    pango_layout_set_text(layout, "0123456789", -1);
    pango_layout_get_pixel_size(layout,
                                &layer->digit_width, &layer->digit_height);
    layer->digit_width /= 10;
    g_object_unref(layout);
}

static void
gwy_layer_line_unrealize(GwyDataViewLayer *dlayer)
{
    GwyLayerLine *layer;
    guint i;

    gwy_debug("");

    layer = GWY_LAYER_LINE(dlayer);
    gdk_cursor_unref(layer->near_cursor);
    gdk_cursor_unref(layer->move_cursor);
    gdk_cursor_unref(layer->nearline_cursor);

    if (layer->line_labels) {
        for (i = 0; i < layer->line_labels->len; i++)
            gwy_object_unref(g_ptr_array_index(layer->line_labels, i));
        g_ptr_array_free(layer->line_labels, TRUE);
        layer->line_labels = NULL;
    }

    if (layer->fontdesc) {
        pango_font_description_free(layer->fontdesc);
        layer->fontdesc = NULL;
    }

    GWY_DATA_VIEW_LAYER_CLASS(gwy_layer_line_parent_class)->unrealize(dlayer);
}

static gint
gwy_layer_line_near_line(GwyVectorLayer *layer,
                         gdouble xreal, gdouble yreal)
{
    gdouble d2min, metric[4];
    gint i, n;

    if (!(n = gwy_selection_get_data(layer->selection, NULL))
        || layer->focus >= n)
        return -1;

    gwy_data_view_get_metric(GWY_DATA_VIEW(GWY_DATA_VIEW_LAYER(layer)->parent),
                             metric);
    if (layer->focus >= 0) {
        gdouble xy[OBJECT_SIZE];

        gwy_selection_get_object(layer->selection, layer->focus, xy);
        i = gwy_math_find_nearest_line(xreal, yreal, &d2min, 1, xy, metric);
    }
    else {
        gdouble *xy = g_newa(gdouble, OBJECT_SIZE*n);

        gwy_selection_get_data(layer->selection, xy);
        i = gwy_math_find_nearest_line(xreal, yreal, &d2min, n, xy, metric);
    }
    if (d2min > PROXIMITY_DISTANCE*PROXIMITY_DISTANCE)
        return -1;
    return i;
}

static gint
gwy_layer_line_near_point(GwyVectorLayer *layer,
                          gdouble xreal, gdouble yreal)
{
    gdouble d2min, metric[4];
    gint i, n;

    if (!(n = gwy_selection_get_data(layer->selection, NULL))
        || layer->focus >= n)
        return -1;

    gwy_data_view_get_metric(GWY_DATA_VIEW(GWY_DATA_VIEW_LAYER(layer)->parent),
                             metric);
    if (layer->focus >= 0) {
        gdouble xy[OBJECT_SIZE];

        gwy_selection_get_object(layer->selection, layer->focus, xy);
        i = gwy_math_find_nearest_point(xreal, yreal, &d2min, 2, xy, metric);
    }
    else {
        gdouble *xy = g_newa(gdouble, OBJECT_SIZE*n);

        gwy_selection_get_data(layer->selection, xy);
        i = gwy_math_find_nearest_point(xreal, yreal, &d2min, 2*n, xy, metric);
    }
    if (i < 0 || d2min > PROXIMITY_DISTANCE*PROXIMITY_DISTANCE)
        return -1;
    return i;
}

/**
 * gwy_layer_line_restrict_angle:
 * @data_view: Data view widget to use for coordinate conversion and
 *             restriction.
 * @endpoint: Which endpoint to move (0 for first, 1, for second).
 * @x: Unrestricted screen endpoint x-coordinate.
 * @y: Unrestricted screen endpoint y-coordinate.
 * @xy: Selection object to update.
 *
 * Restricts line endpoint to force line angle to be a multiple of 15 degrees.
 **/
static void
gwy_layer_line_restrict_angle(GwyDataView *data_view,
                              gint endpoint,
                              gint x, gint y,
                              gdouble *xy)
{
    gint length, phi, xb, yb, xx, yy, ept = endpoint;
    gdouble c, s;

    gwy_data_view_coords_real_to_xy(data_view,
                                    xy[2*(1 - ept) + 0], xy[2*(1 - ept) + 1],
                                    &xb, &yb);
    phi = GWY_ROUND(atan2(y - yb, x - xb)*12.0/G_PI);
    s = sin(phi*G_PI/12.0);
    c = cos(phi*G_PI/12.0);
    length = GWY_ROUND(hypot(x - xb, y - yb));
    if (!length) {
        xy[2*ept + 0] = xy[2*(1 - ept) + 0];
        xy[2*ept + 1] = xy[2*(1 - ept) + 1];
        return;
    }

    x = xx = xb + length*c;
    y = yy = yb + length*s;
    gwy_data_view_coords_xy_clamp(data_view, &x, &y);
    if (x != xx && y != yy) {
        xx = xb + (y - yb)*c/s;
        yy = yb + (x - xb)*s/c;
        if (hypot(xx - xb, y - yb) < hypot(x - xb, yy - yb))
            x = xx;
        else
            y = yy;
    }
    else if (x == xx && y != yy)
        x = xb + (y - yb)*c/s;
    else if (x != xx && y == yy)
        y = yb + (x - xb)*s/c;
    /* Final clamp to fix eventual rounding errors */
    gwy_data_view_coords_xy_clamp(data_view, &x, &y);
    gwy_data_view_coords_xy_to_real(data_view, x, y,
                                    &xy[2*ept + 0], &xy[2*ept + 1]);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
