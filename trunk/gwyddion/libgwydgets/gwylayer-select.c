/* @(#) $Id$ */

#include <string.h>
#include <glib-object.h>

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include "gwylayer-select.h"
#include "gwydataview.h"

#define GWY_LAYER_SELECT_TYPE_NAME "GwyLayerSelect"

#define PROXIMITY_DISTANCE 8

#define BITS_PER_SAMPLE 8

/* Forward declarations */

static void       gwy_layer_select_class_init        (GwyLayerSelectClass *klass);
static void       gwy_layer_select_init              (GwyLayerSelect *layer);
static void       gwy_layer_select_finalize          (GObject *object);
static void       gwy_layer_select_draw              (GwyDataViewLayer *layer,
                                                      GdkDrawable *drawable);
static gboolean   gwy_layer_select_motion_notify     (GwyDataViewLayer *layer,
                                                      GdkEventMotion *event);
static gboolean   gwy_layer_select_button_pressed    (GwyDataViewLayer *layer,
                                                      GdkEventButton *event);
static gboolean   gwy_layer_select_button_released   (GwyDataViewLayer *layer,
                                                      GdkEventButton *event);
static void       gwy_layer_select_plugged           (GwyDataViewLayer *layer);
static void       gwy_layer_select_unplugged         (GwyDataViewLayer *layer);
static void       gwy_layer_select_save              (GwyDataViewLayer *layer);
static void       gwy_layer_select_restore           (GwyDataViewLayer *layer);

static int        gwy_layer_select_near_point        (GwyLayerSelect *layer,
                                                      gdouble xreal,
                                                      gdouble yreal);

/* Local data */

static GtkObjectClass *parent_class = NULL;

GType
gwy_layer_select_get_type(void)
{
    static GType gwy_layer_select_type = 0;

    if (!gwy_layer_select_type) {
        static const GTypeInfo gwy_layer_select_info = {
            sizeof(GwyLayerSelectClass),
            NULL,
            NULL,
            (GClassInitFunc)gwy_layer_select_class_init,
            NULL,
            NULL,
            sizeof(GwyLayerSelect),
            0,
            (GInstanceInitFunc)gwy_layer_select_init,
            NULL,
        };
        gwy_debug("%s", __FUNCTION__);
        gwy_layer_select_type
            = g_type_register_static(GWY_TYPE_DATA_VIEW_LAYER,
                                     GWY_LAYER_SELECT_TYPE_NAME,
                                     &gwy_layer_select_info,
                                     0);
    }

    return gwy_layer_select_type;
}

static void
gwy_layer_select_class_init(GwyLayerSelectClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GwyDataViewLayerClass *layer_class = GWY_DATA_VIEW_LAYER_CLASS(klass);

    gwy_debug("%s", __FUNCTION__);

    parent_class = g_type_class_peek_parent(klass);

    gobject_class->finalize = gwy_layer_select_finalize;

    layer_class->draw = gwy_layer_select_draw;
    layer_class->motion_notify = gwy_layer_select_motion_notify;
    layer_class->button_press = gwy_layer_select_button_pressed;
    layer_class->button_release = gwy_layer_select_button_released;
    layer_class->plugged = gwy_layer_select_plugged;
    layer_class->unplugged = gwy_layer_select_unplugged;

    memset(klass->corner_cursor, 0, 4*sizeof(GdkCursor*));
    klass->resize_cursor = NULL;
}

static void
gwy_layer_select_init(GwyLayerSelect *layer)
{
    GwyLayerSelectClass *klass;

    gwy_debug("%s", __FUNCTION__);

    klass = GWY_LAYER_SELECT_GET_CLASS(layer);
    gwy_layer_cursor_new_or_ref(&klass->resize_cursor, GDK_CROSS);
    gwy_layer_cursor_new_or_ref(&klass->corner_cursor[0], GDK_UL_ANGLE);
    gwy_layer_cursor_new_or_ref(&klass->corner_cursor[1], GDK_LL_ANGLE);
    gwy_layer_cursor_new_or_ref(&klass->corner_cursor[2], GDK_UR_ANGLE);
    gwy_layer_cursor_new_or_ref(&klass->corner_cursor[3], GDK_LR_ANGLE);
    layer->selected = FALSE;
}

static void
gwy_layer_select_finalize(GObject *object)
{
    GwyLayerSelectClass *klass;
    gint i;

    gwy_debug("%s", __FUNCTION__);

    g_return_if_fail(GWY_IS_LAYER_SELECT(object));

    klass = GWY_LAYER_SELECT_GET_CLASS(object);
    gwy_layer_cursor_free_or_unref(&klass->resize_cursor);
    for (i = 0; i < 4; i++)
        gwy_layer_cursor_free_or_unref(&klass->corner_cursor[i]);

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

/**
 * gwy_layer_select_new:
 *
 * Creates a new rectangular selection layer.
 *
 * Container keys: "/0/select/x0", "/0/select/x1", "/0/select/y0",
 * "/0/select/y1", and "/0/select/selected".
 *
 * Returns: The newly created layer.
 **/
GtkObject*
gwy_layer_select_new(void)
{
    GtkObject *object;

    gwy_debug("%s", __FUNCTION__);
    object = g_object_new(GWY_TYPE_LAYER_SELECT, NULL);

    return object;
}

static void
gwy_layer_select_setup_gc(GwyDataViewLayer *layer)
{
    GdkColor fg, bg;

    if (!GTK_WIDGET_REALIZED(layer->parent))
        return;

    layer->gc = gdk_gc_new(layer->parent->window);
    gdk_gc_set_function(layer->gc, GDK_INVERT);
    fg.pixel = 0xFFFFFFFF;
    bg.pixel = 0x00000000;
    gdk_gc_set_foreground(layer->gc, &fg);
    gdk_gc_set_background(layer->gc, &bg);
}

static void
gwy_layer_select_draw(GwyDataViewLayer *layer,
                      GdkDrawable *drawable)
{
    GwyLayerSelect *select_layer;
    gint xmin, ymin, xmax, ymax;

    g_return_if_fail(GWY_IS_LAYER_SELECT(layer));
    g_return_if_fail(GDK_IS_DRAWABLE(drawable));

    select_layer = (GwyLayerSelect*)layer;

    if (!select_layer->selected)
        return;

    if (!layer->gc)
        gwy_layer_select_setup_gc(layer);

    gwy_debug("%s [%g,%g] to [%g,%g]",
              __FUNCTION__,
              select_layer->x0, select_layer->y0,
              select_layer->x1, select_layer->y1);
    gwy_data_view_coords_real_to_xy(GWY_DATA_VIEW(layer->parent),
                                    select_layer->x0, select_layer->y0,
                                    &xmin, &ymin);
    gwy_data_view_coords_real_to_xy(GWY_DATA_VIEW(layer->parent),
                                    select_layer->x1, select_layer->y1,
                                    &xmax, &ymax);
    if (xmax < xmin)
        GWY_SWAP(gint, xmin, xmax);
    if (ymax < ymin)
        GWY_SWAP(gint, ymin, ymax);

    gwy_debug("%s [%d,%d] to [%d,%d]",
              __FUNCTION__, xmin, ymin, xmax, ymax);
    gdk_draw_rectangle(drawable, layer->gc, FALSE,
                       xmin, ymin, xmax - xmin, ymax - ymin);

}

static gboolean
gwy_layer_select_motion_notify(GwyDataViewLayer *layer,
                               GdkEventMotion *event)
{
    GwyLayerSelectClass *klass;
    GwyLayerSelect *select_layer;
    gint x, y, i;
    gdouble oldx, oldy, xreal, yreal;

    select_layer = (GwyLayerSelect*)layer;
    oldx = select_layer->x1;
    oldy = select_layer->y1;
    x = event->x;
    y = event->y;
    gwy_data_view_coords_xy_clamp(GWY_DATA_VIEW(layer->parent), &x, &y);
    gwy_data_view_coords_xy_to_real(GWY_DATA_VIEW(layer->parent),
                                    x, y, &xreal, &yreal);
    if (xreal == oldx && yreal == oldy)
        return FALSE;

    klass = GWY_LAYER_SELECT_GET_CLASS(select_layer);
    if (!select_layer->button) {
        i = gwy_layer_select_near_point(select_layer, xreal, yreal);
        select_layer->near = i;
        gdk_window_set_cursor(layer->parent->window,
                              i == -1 ? NULL : klass->corner_cursor[i]);
        return FALSE;
    }

    gwy_layer_select_draw(layer, layer->parent->window);
    select_layer->x1 = xreal;
    select_layer->y1 = yreal;

    gwy_layer_select_save(layer);
    gwy_layer_select_draw(layer, layer->parent->window);
    gwy_data_view_layer_updated(layer);

    return FALSE;
}

static gboolean
gwy_layer_select_button_pressed(GwyDataViewLayer *layer,
                                GdkEventButton *event)
{
    GwyLayerSelectClass *klass;
    GwyLayerSelect *select_layer;
    gint x, y;
    gdouble xreal, yreal;
    gboolean keep_old = FALSE;

    gwy_debug("%s", __FUNCTION__);
    select_layer = (GwyLayerSelect*)layer;
    if (select_layer->button)
        g_warning("unexpected mouse button press when already pressed");

    x = event->x;
    y = event->y;
    gwy_data_view_coords_xy_clamp(GWY_DATA_VIEW(layer->parent), &x, &y);
    gwy_debug("%s [%d,%d]", __FUNCTION__, x, y);
    /* do nothing when we are outside */
    if (x != event->x || y != event->y)
        return FALSE;

    gwy_data_view_coords_xy_to_real(GWY_DATA_VIEW(layer->parent),
                                    x, y, &xreal, &yreal);
    /* handle a previous selection:
     * when we are near a corner, resize the existing one
     * otherwise forget it and start from scratch */
    klass = GWY_LAYER_SELECT_GET_CLASS(select_layer);
    if (select_layer->selected) {
        gint i;

        gwy_layer_select_draw(layer, layer->parent->window);
        i = gwy_layer_select_near_point(select_layer, xreal, yreal);
        if (i >= 0) {
            keep_old = TRUE;
            if (i/2)
                select_layer->x0 = MIN(select_layer->x0, select_layer->x1);
            else
                select_layer->x0 = MAX(select_layer->x0, select_layer->x1);
            if (i%2)
                select_layer->y0 = MIN(select_layer->y0, select_layer->y1);
            else
                select_layer->y0 = MAX(select_layer->y0, select_layer->y1);
        }
    }
    select_layer->button = event->button;
    select_layer->x1 = xreal;
    select_layer->y1 = yreal;
    if (!keep_old) {
        select_layer->x0 = xreal;
        select_layer->y0 = yreal;
    }
    else
        gwy_layer_select_draw(layer, layer->parent->window);
    gwy_debug("%s [%g,%g] to [%g,%g]",
              __FUNCTION__,
              select_layer->x0, select_layer->y0,
              select_layer->x1, select_layer->y1);
    select_layer->selected = TRUE;

    gdk_window_set_cursor(layer->parent->window, klass->resize_cursor);

    return FALSE;
}

static gboolean
gwy_layer_select_button_released(GwyDataViewLayer *layer,
                                 GdkEventButton *event)
{
    GwyLayerSelectClass *klass;
    GwyLayerSelect *select_layer;
    gint x, y, i;
    gdouble xreal, yreal;

    select_layer = (GwyLayerSelect*)layer;
    if (!select_layer->button)
        return FALSE;
    select_layer->button = 0;
    x = event->x;
    y = event->y;
    gwy_data_view_coords_xy_clamp(GWY_DATA_VIEW(layer->parent), &x, &y);
    gwy_data_view_coords_xy_to_real(GWY_DATA_VIEW(layer->parent),
                                    x, y, &xreal, &yreal);
    select_layer->x1 = xreal;
    select_layer->y1 = yreal;
    gwy_data_view_coords_real_to_xy(GWY_DATA_VIEW(layer->parent),
                                    select_layer->x0, select_layer->y0,
                                    &x, &y);
    select_layer->selected = (x != event->x) && (y != event->y);
    if (select_layer->selected) {
        if (select_layer->x1 < select_layer->x0)
            GWY_SWAP(gdouble, select_layer->x0, select_layer->x1);
        if (select_layer->y1 < select_layer->y0)
            GWY_SWAP(gdouble, select_layer->y0, select_layer->y1);
    }
    gwy_layer_select_save(layer);
    gwy_data_view_layer_updated(layer);
    gwy_data_view_layer_finished(layer);

    klass = GWY_LAYER_SELECT_GET_CLASS(select_layer);
    i = gwy_layer_select_near_point(select_layer, xreal, yreal);
    gdk_window_set_cursor(layer->parent->window,
                          i == -1 ? NULL : klass->corner_cursor[i]);

    /* XXX: this assures no artifacts ...  */
    gtk_widget_queue_draw(layer->parent);

    return FALSE;
}

/**
 * gwy_layer_select_get_selection:
 * @layer: A #GwyLayerSelect.
 * @xmin: Where the upper left corner x-coordinate should be stored.
 * @ymin: Where the upper left corner y-coordinate should be stored.
 * @xmax: Where the lower right corner x-coordinate should be stored.
 * @ymax: Where the lower right corner x-coordinate should be stored.
 *
 * Obtains the selected rectangle in real (i.e., physical) coordinates.
 *
 * FIXME: this should be done through container.
 *
 * Returns: %TRUE when there is some selection present (and some values were
 *          stored), %FALSE
 **/
gboolean
gwy_layer_select_get_selection(GwyDataViewLayer *layer,
                               gdouble *xmin, gdouble *ymin,
                               gdouble *xmax, gdouble *ymax)
{
    GwyLayerSelect *select_layer;

    g_return_val_if_fail(GWY_IS_LAYER_SELECT(layer), FALSE);

    select_layer = (GwyLayerSelect*)layer;
    if (!select_layer->selected)
        return FALSE;

    if (*xmin)
        *xmin = select_layer->x0;
    if (*ymin)
        *ymin = select_layer->y0;
    if (*xmax)
        *xmax = select_layer->x1;
    if (*ymax)
        *ymax = select_layer->y1;
    return TRUE;
}

/**
 * gwy_layer_select_unselect:
 * @layer: A #GwyLayerSelect.
 *
 * Clears the selection.
 *
 * Note: may have unpredictable effects when called while user is drawing
 * a selection.
 **/
void
gwy_layer_select_unselect(GwyDataViewLayer *layer)
{
    g_return_if_fail(GWY_IS_LAYER_SELECT(layer));

    GWY_LAYER_SELECT(layer)->selected = FALSE;
    gwy_layer_select_save(layer);
}

static void
gwy_layer_select_plugged(GwyDataViewLayer *layer)
{
    gwy_debug("%s", __FUNCTION__);
    g_return_if_fail(GWY_IS_LAYER_SELECT(layer));

    GWY_LAYER_SELECT(layer)->selected = FALSE;
    GWY_DATA_VIEW_LAYER_CLASS(parent_class)->plugged(layer);
    gwy_layer_select_restore(layer);
    gwy_data_view_layer_updated(layer);
}

static void
gwy_layer_select_unplugged(GwyDataViewLayer *layer)
{
    gwy_debug("%s", __FUNCTION__);
    g_return_if_fail(GWY_IS_LAYER_SELECT(layer));

    GWY_LAYER_SELECT(layer)->selected = FALSE;
    GWY_DATA_VIEW_LAYER_CLASS(parent_class)->unplugged(layer);
}

static void
gwy_layer_select_save(GwyDataViewLayer *layer)
{
    GwyLayerSelect *s = GWY_LAYER_SELECT(layer);

    /* TODO Container */
    gwy_container_set_boolean_by_name(layer->data, "/0/select/selected",
                                      s->selected);
    if (!s->selected)
        return;
    gwy_container_set_double_by_name(layer->data, "/0/select/x0", s->x0);
    gwy_container_set_double_by_name(layer->data, "/0/select/y0", s->y0);
    gwy_container_set_double_by_name(layer->data, "/0/select/x1", s->x1);
    gwy_container_set_double_by_name(layer->data, "/0/select/y1", s->y1);
}

static void
gwy_layer_select_restore(GwyDataViewLayer *layer)
{
    GwyLayerSelect *s = GWY_LAYER_SELECT(layer);

    /* TODO Container */
    if (!gwy_container_contains_by_name(layer->data, "/0/select/selected"))
        return;
    s->selected = gwy_container_get_boolean_by_name(layer->data,
                                                    "/0/select/selected");
    if (!s->selected)
        return;
    s->x0 = gwy_container_get_double_by_name(layer->data, "/0/select/x0");
    s->y0 = gwy_container_get_double_by_name(layer->data, "/0/select/y0");
    s->x1 = gwy_container_get_double_by_name(layer->data, "/0/select/x1");
    s->y1 = gwy_container_get_double_by_name(layer->data, "/0/select/y1");
}

static int
gwy_layer_select_near_point(GwyLayerSelect *layer,
                            gdouble xreal, gdouble yreal)
{
    GwyDataViewLayer *dlayer;
    gdouble coords[8], d2min;
    gint i;

    if (!layer->selected)
        return -1;

    coords[0] = coords[2] = layer->x0;
    coords[1] = coords[5] = layer->y0;
    coords[4] = coords[6] = layer->x1;
    coords[3] = coords[7] = layer->y1;
    i = gwy_math_find_nearest_point(xreal, yreal, &d2min, 4, coords);

    dlayer = (GwyDataViewLayer*)layer;
    /* FIXME: this is simply nonsense when x measure != y measure */
    d2min /= gwy_data_view_get_xmeasure(GWY_DATA_VIEW(dlayer->parent))
             *gwy_data_view_get_ymeasure(GWY_DATA_VIEW(dlayer->parent));

    if (d2min > PROXIMITY_DISTANCE*PROXIMITY_DISTANCE)
        return -1;
    return i;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
