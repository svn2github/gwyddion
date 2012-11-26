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

#include "libgwy/macros.h"
#include "libgwy/math.h"
#include "libgwy/object-utils.h"
#include "libgwyui/cairo-utils.h"
#include "libgwyui/widget-utils.h"
#include "libgwyui/types.h"
#include "libgwyui/adjust-bar.h"

enum {
    PROP_0,
    PROP_ADJUSTMENT,
    PROP_MAPPING,
    N_PROPS,
};

enum {
    SGNL_CHANGE_VALUE,
    N_SIGNALS
};

typedef gdouble (*MappingFunc)(gdouble value);

struct _GwyAdjustBarPrivate {
    GdkWindow *input_window;
    GdkCursor *cursor_move;

    GtkAdjustment *adjustment;
    gulong adjustment_value_changed_id;
    gulong adjustment_changed_id;
    gdouble oldvalue;    // This is to avoid acting on no-change notifications.
    gboolean adjustment_ok;

    GwyScaleMappingType mapping;
    MappingFunc map_value;
    MappingFunc map_position;
    gdouble a;
    gdouble b;
};

typedef struct _GwyAdjustBarPrivate AdjustBar;

static void     gwy_adjust_bar_dispose             (GObject *object);
static void     gwy_adjust_bar_finalize            (GObject *object);
static void     gwy_adjust_bar_set_property        (GObject *object,
                                                    guint prop_id,
                                                    const GValue *value,
                                                    GParamSpec *pspec);
static void     gwy_adjust_bar_get_property        (GObject *object,
                                                    guint prop_id,
                                                    GValue *value,
                                                    GParamSpec *pspec);
static void     gwy_adjust_bar_realize             (GtkWidget *widget);
static void     gwy_adjust_bar_unrealize           (GtkWidget *widget);
static void     gwy_adjust_bar_map                 (GtkWidget *widget);
static void     gwy_adjust_bar_unmap               (GtkWidget *widget);
static void     gwy_adjust_bar_get_preferred_width (GtkWidget *widget,
                                                    gint *minimum,
                                                    gint *natural);
static void     gwy_adjust_bar_get_preferred_height(GtkWidget *widget,
                                                    gint *minimum,
                                                    gint *natural);
static void     gwy_adjust_bar_size_allocate       (GtkWidget *widget,
                                                    GtkAllocation *allocation);
static gboolean gwy_adjust_bar_draw                (GtkWidget *widget,
                                                    cairo_t *cr);
static gboolean gwy_adjust_bar_enter_notify        (GtkWidget *widget,
                                                    GdkEventCrossing *event);
static gboolean gwy_adjust_bar_leave_notify        (GtkWidget *widget,
                                                    GdkEventCrossing *event);
static gboolean gwy_adjust_bar_scroll              (GtkWidget *widget,
                                                    GdkEventScroll *event);
static gboolean gwy_adjust_bar_button_press        (GtkWidget *widget,
                                                    GdkEventButton *event);
static gboolean gwy_adjust_bar_button_release      (GtkWidget *widget,
                                                    GdkEventButton *event);
static gboolean gwy_adjust_bar_motion_notify       (GtkWidget *widget,
                                                    GdkEventMotion *event);
static GType    gwy_adjust_bar_child_type          (GtkContainer *container);
static void     gwy_adjust_bar_change_value        (GwyAdjustBar *adjbar,
                                                    gdouble newvalue);
static gboolean set_adjustment                     (GwyAdjustBar *adjbar,
                                                    GtkAdjustment *adjustment);
static gboolean set_mapping                        (GwyAdjustBar *adjbar,
                                                    GwyScaleMappingType mapping);
static void     create_input_window                (GwyAdjustBar *adjbar);
static void     destroy_input_window               (GwyAdjustBar *adjbar);
static void     draw_bar                           (GwyAdjustBar *adjbar,
                                                    cairo_t *cr);
static void     adjustment_changed                 (GwyAdjustBar *adjbar,
                                                    GtkAdjustment *adjustment);
static void     adjustment_value_changed           (GwyAdjustBar *adjbar,
                                                    GtkAdjustment *adjustment);
static void     update_mapping                     (GwyAdjustBar *adjbar);
static gdouble  map_value_to_position              (GwyAdjustBar *adjbar,
                                                    gdouble length,
                                                    gdouble value);
static gdouble  map_position_to_value              (GwyAdjustBar *adjbar,
                                                    gdouble length,
                                                    gdouble position);
static gdouble  map_both_linear                    (gdouble value);
static gdouble  map_value_sqrt                     (gdouble value);
static gdouble  map_position_sqrt                  (gdouble position);
static gdouble  map_value_log                      (gdouble value);
static gdouble  map_position_log                   (gdouble position);
static void     change_value                       (GtkWidget *widget,
                                                    gdouble newposition);
static void     ensure_cursors                     (GwyAdjustBar *adjbar);
static void     discard_cursors                    (GwyAdjustBar *adjbar);

static GParamSpec *properties[N_PROPS];
static guint signals[N_SIGNALS];

G_DEFINE_TYPE(GwyAdjustBar, gwy_adjust_bar, GTK_TYPE_BIN);

static void
gwy_adjust_bar_class_init(GwyAdjustBarClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
    GtkContainerClass *container_class = GTK_CONTAINER_CLASS(klass);

    g_type_class_add_private(klass, sizeof(AdjustBar));

    gobject_class->dispose = gwy_adjust_bar_dispose;
    gobject_class->finalize = gwy_adjust_bar_finalize;
    gobject_class->get_property = gwy_adjust_bar_get_property;
    gobject_class->set_property = gwy_adjust_bar_set_property;

    widget_class->realize = gwy_adjust_bar_realize;
    widget_class->unrealize = gwy_adjust_bar_unrealize;
    widget_class->map = gwy_adjust_bar_map;
    widget_class->unmap = gwy_adjust_bar_unmap;
    widget_class->get_preferred_width = gwy_adjust_bar_get_preferred_width;
    widget_class->get_preferred_height = gwy_adjust_bar_get_preferred_height;
    widget_class->size_allocate = gwy_adjust_bar_size_allocate;
    widget_class->draw = gwy_adjust_bar_draw;
    widget_class->enter_notify_event = gwy_adjust_bar_enter_notify;
    widget_class->leave_notify_event = gwy_adjust_bar_leave_notify;
    widget_class->scroll_event = gwy_adjust_bar_scroll;
    widget_class->button_press_event = gwy_adjust_bar_button_press;
    widget_class->button_release_event = gwy_adjust_bar_button_release;
    widget_class->motion_notify_event = gwy_adjust_bar_motion_notify;

    container_class->child_type = gwy_adjust_bar_child_type;

    properties[PROP_ADJUSTMENT]
        = g_param_spec_object("adjustment",
                              "Adjustment",
                              "Adjustment representing the value.",
                              GTK_TYPE_ADJUSTMENT,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_MAPPING]
        = g_param_spec_enum("mapping",
                            "Mapping",
                            "Mapping function between values and screen "
                            "positions.",
                            GWY_TYPE_SCALE_MAPPING_TYPE,
                            GWY_SCALE_MAPPING_SQRT,
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    for (guint i = 1; i < N_PROPS; i++)
        g_object_class_install_property(gobject_class, i, properties[i]);

    /**
     * GwyAdjustBar::change-value:
     * @gwyadjbar: The #GwyAdjustBar which received the signal.
     * @arg1: New value for @gwyadjbar.
     *
     * The ::change-value signal is emitted when the user interactively
     * changes the value.
     *
     * It is an action signal.
     **/
    signals[SGNL_CHANGE_VALUE]
        = g_signal_new_class_handler("change-value",
                                     G_OBJECT_CLASS_TYPE(klass),
                                     G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                                     G_CALLBACK(gwy_adjust_bar_change_value),
                                     NULL, NULL,
                                     g_cclosure_marshal_VOID__DOUBLE,
                                     G_TYPE_NONE, 1, G_TYPE_DOUBLE);
}

static void
gwy_adjust_bar_init(GwyAdjustBar *adjbar)
{
    adjbar->priv = G_TYPE_INSTANCE_GET_PRIVATE(adjbar,
                                               GWY_TYPE_ADJUST_BAR,
                                               AdjustBar);
    AdjustBar *priv = adjbar->priv;
    priv->mapping = GWY_SCALE_MAPPING_SQRT;
    gtk_widget_set_has_window(GTK_WIDGET(adjbar), FALSE);
    GtkWidget *label = gtk_label_new(NULL);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_container_add(GTK_CONTAINER(adjbar), label);
}

static void
gwy_adjust_bar_finalize(GObject *object)
{
    G_OBJECT_CLASS(gwy_adjust_bar_parent_class)->finalize(object);
}

static void
gwy_adjust_bar_dispose(GObject *object)
{
    GwyAdjustBar *adjbar = GWY_ADJUST_BAR(object);
    set_adjustment(adjbar, NULL);
    G_OBJECT_CLASS(gwy_adjust_bar_parent_class)->dispose(object);
}

static void
gwy_adjust_bar_set_property(GObject *object,
                            guint prop_id,
                            const GValue *value,
                            GParamSpec *pspec)
{
    GwyAdjustBar *adjbar = GWY_ADJUST_BAR(object);

    switch (prop_id) {
        case PROP_ADJUSTMENT:
        set_adjustment(adjbar, g_value_get_object(value));
        break;

        case PROP_MAPPING:
        set_mapping(adjbar, g_value_get_enum(value));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_adjust_bar_get_property(GObject *object,
                            guint prop_id,
                            GValue *value,
                            GParamSpec *pspec)
{
    AdjustBar *priv = GWY_ADJUST_BAR(object)->priv;

    switch (prop_id) {
        case PROP_ADJUSTMENT:
        g_value_set_object(value, priv->adjustment);
        break;

        case PROP_MAPPING:
        g_value_set_enum(value, priv->mapping);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_adjust_bar_realize(GtkWidget *widget)
{
    GwyAdjustBar *adjbar = GWY_ADJUST_BAR(widget);
    GTK_WIDGET_CLASS(gwy_adjust_bar_parent_class)->realize(widget);
    create_input_window(adjbar);
}

static void
gwy_adjust_bar_unrealize(GtkWidget *widget)
{
    GwyAdjustBar *adjbar = GWY_ADJUST_BAR(widget);
    AdjustBar *priv = adjbar->priv;
    discard_cursors(adjbar);
    destroy_input_window(adjbar);
    priv->adjustment_ok = FALSE;
    GTK_WIDGET_CLASS(gwy_adjust_bar_parent_class)->unrealize(widget);
}

static void
gwy_adjust_bar_map(GtkWidget *widget)
{
    GwyAdjustBar *adjbar = GWY_ADJUST_BAR(widget);
    AdjustBar *priv = adjbar->priv;
    GTK_WIDGET_CLASS(gwy_adjust_bar_parent_class)->map(widget);
    if (priv->input_window)
        gdk_window_show(priv->input_window);
}

static void
gwy_adjust_bar_unmap(GtkWidget *widget)
{
    GwyAdjustBar *adjbar = GWY_ADJUST_BAR(widget);
    AdjustBar *priv = adjbar->priv;
    if (priv->input_window)
        gdk_window_hide(priv->input_window);
    GTK_WIDGET_CLASS(gwy_adjust_bar_parent_class)->unmap(widget);
}

// FIXME FIXME FIXME: These method are *exact* reproductions of GtkBin's.  But
// if we do not override them the child is completely mis-allocated.
static void
gwy_adjust_bar_get_preferred_width(GtkWidget *widget,
                                   gint *minimum,
                                   gint *natural)
{
    GtkWidget *label = gtk_bin_get_child(GTK_BIN(widget));
    gint child_min, child_nat;
    gtk_widget_get_preferred_width(label, &child_min, &child_nat);
    gint border = gtk_container_get_border_width(GTK_CONTAINER(widget));
    *minimum = child_min + 2*border;
    *natural = child_nat + 2*border;
}

static void
gwy_adjust_bar_get_preferred_height(GtkWidget *widget,
                                    gint *minimum,
                                    gint *natural)
{
    GtkWidget *label = gtk_bin_get_child(GTK_BIN(widget));
    gint child_min, child_nat;
    gtk_widget_get_preferred_height(label, &child_min, &child_nat);
    gint border = gtk_container_get_border_width(GTK_CONTAINER(widget));
    *minimum = child_min + 2*border;
    *natural = child_nat + 2*border;
}

static void
gwy_adjust_bar_size_allocate(GtkWidget *widget,
                             GtkAllocation *allocation)
{
    GtkBin *bin = GTK_BIN(widget);

    gtk_widget_set_allocation(widget, allocation);
    GtkWidget *child = gtk_bin_get_child(bin);
    if (child && gtk_widget_get_visible(child)) {
        GtkAllocation child_allocation;
        gint border_width = gtk_container_get_border_width(GTK_CONTAINER(bin));
        child_allocation.x = allocation->x + border_width;
        child_allocation.y = allocation->y + border_width;
        child_allocation.width = allocation->width - 2 * border_width;
        child_allocation.height = allocation->height - 2 * border_width;
        gtk_widget_size_allocate(child, &child_allocation);
    }

    GwyAdjustBar *adjbar = GWY_ADJUST_BAR(widget);
    AdjustBar *priv = adjbar->priv;
    if (priv->input_window)
        gdk_window_move_resize(priv->input_window,
                               allocation->x, allocation->y,
                               allocation->width, allocation->height);
}

static gboolean
gwy_adjust_bar_draw(GtkWidget *widget,
                    cairo_t *cr)
{
    draw_bar(GWY_ADJUST_BAR(widget), cr);
    GTK_WIDGET_CLASS(gwy_adjust_bar_parent_class)->draw(widget, cr);
    return FALSE;
}

static gboolean
gwy_adjust_bar_enter_notify(GtkWidget *widget,
                            G_GNUC_UNUSED GdkEventCrossing *event)
{
    GwyAdjustBar *adjbar = GWY_ADJUST_BAR(widget);
    ensure_cursors(adjbar);
    GtkStateFlags state = gtk_widget_get_state_flags(widget);
    if (!(state & GTK_STATE_FLAG_PRELIGHT))
        gtk_widget_set_state_flags(widget, state | GTK_STATE_FLAG_PRELIGHT,
                                   TRUE);
    return FALSE;
}

static gboolean
gwy_adjust_bar_leave_notify(GtkWidget *widget,
                            G_GNUC_UNUSED GdkEventCrossing *event)
{
    GtkStateFlags state = gtk_widget_get_state_flags(widget);
    if (state & GTK_STATE_FLAG_PRELIGHT)
        gtk_widget_set_state_flags(widget, state & ~GTK_STATE_FLAG_PRELIGHT,
                                   TRUE);
    return FALSE;
}

static gboolean
gwy_adjust_bar_scroll(GtkWidget *widget,
                      GdkEventScroll *event)
{
    GwyAdjustBar *adjbar = GWY_ADJUST_BAR(widget);
    AdjustBar *priv = adjbar->priv;
    if (!priv->adjustment_ok)
        return TRUE;

    GdkScrollDirection dir = event->direction;
    gdouble length = gtk_widget_get_allocated_width(widget);
    gdouble value = gtk_adjustment_get_value(priv->adjustment),
            position = map_value_to_position(adjbar, length, value),
            newposition = position;
    if (dir == GDK_SCROLL_UP || dir == GDK_SCROLL_RIGHT)
        newposition += 1.0;
    else
        newposition -= 1.0;

    change_value(widget, CLAMP(newposition, 0.0, length));
    return TRUE;
}

static gboolean
gwy_adjust_bar_button_press(GtkWidget *widget,
                            GdkEventButton *event)
{
    if (event->button != 1)
        return FALSE;

    change_value(widget, event->x);
    return TRUE;
}

static gboolean
gwy_adjust_bar_button_release(GtkWidget *widget,
                              GdkEventButton *event)
{
    if (event->button != 1)
        return FALSE;

    change_value(widget, event->x);
    return TRUE;
}

static gboolean
gwy_adjust_bar_motion_notify(GtkWidget *widget,
                             GdkEventMotion *event)
{
    if (!(event->state & GDK_BUTTON1_MASK))
        return FALSE;

    change_value(widget, event->x);
    return TRUE;
}

static GType
gwy_adjust_bar_child_type(GtkContainer *container)
{
    GtkWidget *child = gtk_bin_get_child(GTK_BIN(container));

    if (!child)
        return GTK_TYPE_LABEL;
    else
        return G_TYPE_NONE;
}

static void
gwy_adjust_bar_change_value(GwyAdjustBar *adjbar,
                            gdouble newvalue)
{
    AdjustBar *priv = adjbar->priv;
    g_return_if_fail(priv->adjustment);
    if (!priv->adjustment_ok)
        return;

    gdouble value = gtk_adjustment_get_value(priv->adjustment);
    if (newvalue == value)
        return;

    gtk_adjustment_set_value(priv->adjustment, newvalue);
}

/**
 * gwy_adjust_bar_new:
 *
 * Creates a new adjustment bar.
 *
 * Returns: A new adjustment bar.
 **/
GtkWidget*
gwy_adjust_bar_new(void)
{
    return g_object_newv(GWY_TYPE_ADJUST_BAR, 0, NULL);
}

/**
 * gwy_adjust_bar_set_adjustment:
 * @adjbar: A adjustment bar.
 * @adjustment: (allow-none):
 *              Adjustment to use for the value.
 *
 * Sets the adjustment that a adjustment bar visualises.
 **/
void
gwy_adjust_bar_set_adjustment(GwyAdjustBar *adjbar,
                              GtkAdjustment *adjustment)
{
    g_return_if_fail(GWY_IS_ADJUST_BAR(adjbar));
    g_return_if_fail(GTK_IS_ADJUSTMENT(adjustment));
    if (!set_adjustment(adjbar, adjustment))
        return;

    g_object_notify_by_pspec(G_OBJECT(adjbar), properties[PROP_ADJUSTMENT]);
}

/**
 * gwy_adjust_bar_get_adjustment:
 * @adjbar: A adjustment bar.
 *
 * Obtains the adjustment that a adjustment bar visualises.
 *
 * Returns: (allow-none) (transfer none):
 *          The colour adjustment used by @adjbar.  If no adjustment was set and
 *          the default one is used, function returns %NULL.
 **/
GtkAdjustment*
gwy_adjust_bar_get_adjustment(const GwyAdjustBar *adjbar)
{
    g_return_val_if_fail(GWY_IS_ADJUST_BAR(adjbar), NULL);
    return adjbar->priv->adjustment;
}

/**
 * gwy_adjust_bar_set_mapping:
 * @adjbar: A adjustment bar.
 * @mapping: Mapping function type between values and screen positions in the
 *           adjustment bar.
 *
 * Sets the mapping function type for a adjustment bar.
 **/
void
gwy_adjust_bar_set_mapping(GwyAdjustBar *adjbar,
                           GwyScaleMappingType mapping)
{
    g_return_if_fail(GWY_IS_ADJUST_BAR(adjbar));
    if (!set_mapping(adjbar, mapping))
        return;

    g_object_notify_by_pspec(G_OBJECT(adjbar), properties[PROP_MAPPING]);
}

/**
 * gwy_adjust_bar_get_mapping:
 * @adjbar: A adjustment bar.
 *
 * Gets the mapping function type of a adjustment bar.
 *
 * Returns: The type of mapping function between values and screen positions.
 **/
GwyScaleMappingType
gwy_adjust_bar_get_mapping(const GwyAdjustBar *adjbar)
{
    g_return_val_if_fail(GWY_IS_ADJUST_BAR(adjbar), GWY_SCALE_MAPPING_LINEAR);
    return adjbar->priv->mapping;
}

static gboolean
set_adjustment(GwyAdjustBar *adjbar,
               GtkAdjustment *adjustment)
{
    AdjustBar *priv = adjbar->priv;
    if (!gwy_set_member_object(adjbar, adjustment, GTK_TYPE_ADJUSTMENT,
                               &priv->adjustment,
                               "changed", &adjustment_changed,
                               &priv->adjustment_changed_id,
                               G_CONNECT_SWAPPED,
                               "value-changed", &adjustment_value_changed,
                               &priv->adjustment_value_changed_id,
                               G_CONNECT_SWAPPED,
                               NULL))
        return FALSE;

    update_mapping(adjbar);
    gtk_widget_queue_draw(GTK_WIDGET(adjbar));
    return TRUE;
}

static gboolean
set_mapping(GwyAdjustBar *adjbar,
            GwyScaleMappingType mapping)
{
    AdjustBar *priv = adjbar->priv;
    if (mapping == priv->mapping)
        return FALSE;

    if (mapping > GWY_SCALE_MAPPING_LOG) {
        g_warning("Wrong scale mapping %u.", mapping);
        return FALSE;
    }

    // FIXME: Cancel editting?  At present it's stateles...
    update_mapping(adjbar);
    gtk_widget_queue_draw(GTK_WIDGET(adjbar));
    return TRUE;
}

static void
create_input_window(GwyAdjustBar *adjbar)
{
    AdjustBar *priv = adjbar->priv;
    GtkWidget *widget = GTK_WIDGET(adjbar);
    g_assert(gtk_widget_get_realized(widget));
    if (priv->input_window)
        return;

    GtkAllocation allocation;
    gtk_widget_get_allocation(widget, &allocation);
    GdkWindowAttr attributes = {
        .x = allocation.x,
        .y = allocation.y,
        .width = allocation.width,
        .height = allocation.height,
        .window_type = GDK_WINDOW_CHILD,
        .wclass = GDK_INPUT_ONLY,
        .event_mask = (gtk_widget_get_events(widget)
                       | GDK_BUTTON_PRESS_MASK
                       | GDK_BUTTON_RELEASE_MASK
                       | GDK_ENTER_NOTIFY_MASK
                       | GDK_LEAVE_NOTIFY_MASK
                       | GDK_SCROLL_MASK
                       | GDK_POINTER_MOTION_MASK
                       | GDK_POINTER_MOTION_HINT_MASK),
    };
    gint attributes_mask = GDK_WA_X | GDK_WA_Y;
    priv->input_window = gdk_window_new(gtk_widget_get_window(widget),
                                        &attributes, attributes_mask);
    gdk_window_set_user_data(priv->input_window, widget);
}

static void
destroy_input_window(GwyAdjustBar *adjbar)
{
    AdjustBar *priv = adjbar->priv;
    if (!priv->input_window)
        return;
    gdk_window_set_user_data(priv->input_window, NULL);
    gdk_window_destroy(priv->input_window);
    priv->input_window = NULL;
}

static void
draw_bar(GwyAdjustBar *adjbar,
         cairo_t *cr)
{
    AdjustBar *priv = adjbar->priv;
    if (!priv->adjustment_ok)
        return;

    GtkWidget *widget = GTK_WIDGET(adjbar);
    GtkStateFlags state = gtk_widget_get_state_flags(widget);
    gdouble width = gtk_widget_get_allocated_width(widget),
            height = gtk_widget_get_allocated_height(widget);
    gdouble val = gtk_adjustment_get_value(priv->adjustment);
    gdouble barlength = map_value_to_position(adjbar, width, val);

    cairo_save(cr);

    GwyRGBA color = { 0.6, 0.6, 1.0, 1.0 };
    if (state & GTK_STATE_FLAG_INSENSITIVE)
        color.a *= 0.5;

    if (barlength > 2.0) {
        cairo_rectangle(cr, 0, 0, barlength, height);
        GwyRGBA fill_color = color;
        if (state & GTK_STATE_FLAG_PRELIGHT)
            fill_color.a *= 0.5;
        else
            fill_color.a *= 0.4;
        gwy_cairo_set_source_rgba(cr, &fill_color);
        cairo_fill(cr);

        cairo_set_line_width(cr, 1.0);
        cairo_rectangle(cr, 0.5, 0.5, barlength-1.0, height-1.0);
        gwy_cairo_set_source_rgba(cr, &color);
        cairo_stroke(cr);
    }
    else {
        // Do not stroke bars thinner than twice the ourline, draw the entire
        // bar using the border color instead.
        cairo_rectangle(cr, 0, 0, barlength, height);
        gwy_cairo_set_source_rgba(cr, &color);
        cairo_fill(cr);
    }
    cairo_restore(cr);
}

static void
adjustment_changed(GwyAdjustBar *adjbar,
                   G_GNUC_UNUSED GtkAdjustment *adjustment)
{
    update_mapping(adjbar);
    gtk_widget_queue_draw(GTK_WIDGET(adjbar));
}

static void
adjustment_value_changed(GwyAdjustBar *adjbar,
                         GtkAdjustment *adjustment)
{
    AdjustBar *priv = adjbar->priv;
    if (!priv->adjustment_ok)
        return;
    gdouble newvalue = gtk_adjustment_get_value(adjustment);
    if (newvalue == priv->oldvalue)
        return;

    priv->oldvalue = newvalue;
    gtk_widget_queue_draw(GTK_WIDGET(adjbar));
}

static void
update_mapping(GwyAdjustBar *adjbar)
{
    AdjustBar *priv = adjbar->priv;
    priv->adjustment_ok = FALSE;
    if (!priv->adjustment)
        return;

    gdouble lower = gtk_adjustment_get_lower(priv->adjustment);
    gdouble upper = gtk_adjustment_get_upper(priv->adjustment);

    if (!isfinite(lower) || !isfinite(upper))
        return;

    if (priv->mapping == GWY_SCALE_MAPPING_LOG) {
        if (lower <= 0.0 || upper <= 0.0)
            return;
    }

    gdouble length = gtk_widget_get_allocated_width(GTK_WIDGET(adjbar));
    if (priv->mapping == GWY_SCALE_MAPPING_LINEAR)
        priv->map_value = priv->map_position = map_both_linear;
    else if (priv->mapping == GWY_SCALE_MAPPING_SQRT) {
        priv->map_value = map_value_sqrt;
        priv->map_position = map_position_sqrt;
    }
    else if (priv->mapping == GWY_SCALE_MAPPING_LOG) {
        priv->map_value = map_value_log;
        priv->map_position = map_position_log;
    }
    priv->b = priv->map_value(lower);
    priv->a = (priv->map_value(upper) - priv->b)/length;
    if (!isfinite(priv->a) || !priv->a || !isfinite(priv->b))
        return;

    priv->adjustment_ok = TRUE;
}

static gdouble
map_value_to_position(GwyAdjustBar *adjbar,
                      gdouble length,
                      gdouble value)
{
    AdjustBar *priv = adjbar->priv;
    return (priv->map_value(value) - priv->b)/priv->a*length;
}

static gdouble
map_position_to_value(GwyAdjustBar *adjbar,
                      gdouble length,
                      gdouble position)
{
    AdjustBar *priv = adjbar->priv;
    return priv->map_position(priv->a*position/length + priv->b);
}

static gdouble
map_both_linear(gdouble value)
{
    return value;
}

static gdouble
map_value_sqrt(gdouble value)
{
    return gwy_spow(value, 0.5);
}

static gdouble
map_position_sqrt(gdouble position)
{
    return gwy_spow(position, 2.0);
}

static gdouble
map_value_log(gdouble value)
{
    return log(value);
}

static gdouble
map_position_log(gdouble position)
{
    return exp(position);
}

static void
change_value(GtkWidget *widget,
             gdouble newposition)
{
    GwyAdjustBar *adjbar = GWY_ADJUST_BAR(widget);
    AdjustBar *priv = adjbar->priv;
    if (!priv->adjustment_ok)
        return;

    gdouble length = gtk_widget_get_allocated_width(widget);
    gdouble value = gtk_adjustment_get_value(priv->adjustment);
    newposition = CLAMP(newposition, 0.0, length);
    gdouble newvalue = map_position_to_value(adjbar, length, newposition);
    if (newvalue != value)
        g_signal_emit(adjbar, signals[SGNL_CHANGE_VALUE], 0, newvalue);
}

static void
ensure_cursors(GwyAdjustBar *adjbar)
{
    AdjustBar *priv = adjbar->priv;
    if (priv->cursor_move)
        return;

    GdkDisplay *display = gtk_widget_get_display(GTK_WIDGET(adjbar));
    priv->cursor_move = gdk_cursor_new_for_display(display,
                                                   GDK_SB_H_DOUBLE_ARROW);
    gdk_window_set_cursor(priv->input_window, priv->cursor_move);
}

static void
discard_cursors(GwyAdjustBar *adjbar)
{
    AdjustBar *priv = adjbar->priv;
    GWY_OBJECT_UNREF(priv->cursor_move);
}

/**
 * SECTION: adjust-bar
 * @title: GwyAdjustBar
 * @short_description: Compact adjustment visualisation and modification
 * @image: GwyAdjustBar.png
 *
 * #GwyAdjustBar is a compact widget for visualisation and modification of the
 * value of an #GtkAdjustment.  It can contains a label with an overlaid bar
 * that can be clicked, dragged or modified by the scroll-wheel by the user.
 * For parameters without any meaningful numeric value this may be sufficient
 * alone.  However, usually a #GwyAdjustBar would be paired with a
 * #GwySpinButton, sharing the same adjustment.  Such spin button would also be
 * the typical mnemonic widget for the adjustment bar.
 *
 * #GwyAdjustBar supports several different types of mapping between screen
 * positions and values of the underlying adjustment.  Nevertheless, the
 * default mapping (signed square root, %GWY_SCALE_MAPPING_SQRT) should fit
 * most situations.
 **/

/**
 * GwyAdjustBar:
 *
 * Adjustment bar widget visualising an adjustment.
 *
 * The #GwyAdjustBar struct contains private data only and should be accessed
 * using the functions below.
 **/

/**
 * GwyAdjustBarClass:
 *
 * Class of adjustment bars visualising adjustments.
 **/

/**
 * GwyScaleMappingType:
 * @GWY_SCALE_MAPPING_LINEAR: Linear mapping between values and screen
 *                            positions.  This recommended for signed additive
 *                            quantities of a limited range.
 * @GWY_SCALE_MAPPING_SQRT: Screen positions correspond to ‘signed square
 *                          roots’ of the value, see gwy_spow().  This is the
 *                          recommended general-purpose default mapping type as
 *                          it works with both signed and usigned quantities
 *                          and offers good sensitivity for both large and
 *                          small values.
 * @GWY_SCALE_MAPPING_LOG: Screen positions correspond to logarithms of values.
 *                         The adjustment range must contain only positive
 *                         values.  For quantities of extreme ranges this
 *                         mapping may be preferred to %GWY_SCALE_MAPPING_SQRT.
 *
 * Type of adjustment bar mapping functions.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
