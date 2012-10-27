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
#include "libgwy/object-utils.h"
#include "libgwyui/marshal.h"
#include "libgwyui/widget-utils.h"
#include "libgwyui/scroller.h"

enum {
    PROP_0,
    PROP_HADJUSTMENT,
    PROP_VADJUSTMENT,
    N_PROPS,
};

enum {
    SCROLL_CHILD,
    N_SIGNALS
};

struct _GwyScrollerPrivate {
    GtkAdjustment *hadjustment;
    GtkAdjustment *vadjustment;
};

typedef struct _GwyScrollerPrivate Scroller;

static void     gwy_scroller_dispose             (GObject *object);
static void     gwy_scroller_finalize            (GObject *object);
static void     gwy_scroller_set_property        (GObject *object,
                                                  guint prop_id,
                                                  const GValue *value,
                                                  GParamSpec *pspec);
static void     gwy_scroller_get_property        (GObject *object,
                                                  guint prop_id,
                                                  GValue *value,
                                                  GParamSpec *pspec);
static void     gwy_scroller_get_preferred_width (GtkWidget *widget,
                                                  gint *minimum,
                                                  gint *natural);
static void     gwy_scroller_get_preferred_height(GtkWidget *widget,
                                                  gint *minimum,
                                                  gint *natural);
static void     gwy_scroller_size_allocate       (GtkWidget *widget,
                                                  GtkAllocation *allocation);
static gboolean gwy_scroller_scroll_event        (GtkWidget *widget,
                                                  GdkEventScroll *event);
static GType    gwy_scroller_child_type          (GtkContainer *container);
static void     gwy_scroller_add                 (GtkContainer *container,
                                                  GtkWidget *child);
static void     gwy_scroller_remove              (GtkContainer *container,
                                                  GtkWidget *child);
static gboolean gwy_scroller_scroll_child        (GwyScroller *scroller,
                                                  GtkScrollType scroll,
                                                  gboolean horizontal);
static void     add_scroll_binding               (GtkBindingSet *binding_set,
                                                  guint keyval,
                                                  GdkModifierType mask,
                                                  GtkScrollType scroll,
                                                  gboolean horizontal);
static gboolean set_hadjustment                  (GwyScroller *scroller,
                                                  GtkAdjustment *adjustment);
static gboolean set_vadjustment                  (GwyScroller *scroller,
                                                  GtkAdjustment *adjustment);

static GParamSpec *properties[N_PROPS];
static guint signals[N_SIGNALS];

G_DEFINE_TYPE(GwyScroller, gwy_scroller, GTK_TYPE_BIN);

static void
gwy_scroller_class_init(GwyScrollerClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
    GtkContainerClass *container_class = GTK_CONTAINER_CLASS(klass);

    g_type_class_add_private(klass, sizeof(Scroller));

    gobject_class->dispose = gwy_scroller_dispose;
    gobject_class->finalize = gwy_scroller_finalize;
    gobject_class->get_property = gwy_scroller_get_property;
    gobject_class->set_property = gwy_scroller_set_property;

    widget_class->get_preferred_width = gwy_scroller_get_preferred_width;
    widget_class->get_preferred_height = gwy_scroller_get_preferred_height;
    widget_class->size_allocate = gwy_scroller_size_allocate;
    widget_class->scroll_event = gwy_scroller_scroll_event;

    container_class->add = gwy_scroller_add;
    container_class->remove = gwy_scroller_remove;
    container_class->child_type = gwy_scroller_child_type;
    gtk_container_class_handle_border_width(container_class);

    properties[PROP_HADJUSTMENT]
        = g_param_spec_object("hadjustment",
                              "Horizontal adjustment",
                              "Adjustment for the horizontal position",
                              GTK_TYPE_ADJUSTMENT,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_VADJUSTMENT]
        = g_param_spec_object("vadjustment",
                              "Vertical adjustment",
                              "Adjustment for the vertical position",
                              GTK_TYPE_ADJUSTMENT,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    for (guint i = 1; i < N_PROPS; i++)
        g_object_class_install_property(gobject_class, i, properties[i]);

    /**
     * GwyScroller::scroll-child:
     * @gwyscroller: The #GwyScroller which received the signal.
     * @arg1: #GtkScrollType describing how much to scroll.
     * @arg2: %TRUE to scroll horizontally, %FALSE to scroll vertically.
     *
     * The ::scroll-child signal is a
     * <link linkend="keybinding-signals">keybinding signal</link>
     * which gets emitted when a keybinding that scrolls is pressed.
     * The horizontal or vertical adjustment is updated which triggers a
     * signal that the scrolled windows child may listen to and scroll itself.
     */
    signals[SCROLL_CHILD]
        = g_signal_new_class_handler("scroll-child",
                                     G_OBJECT_CLASS_TYPE(klass),
                                     G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                     G_CALLBACK(gwy_scroller_scroll_child),
                                     NULL, NULL,
                                     _gwy_cclosure_marshal_BOOLEAN__ENUM_BOOLEAN,
                                     G_TYPE_BOOLEAN, 2,
                                     GTK_TYPE_SCROLL_TYPE, G_TYPE_BOOLEAN);

    GtkBindingSet *binding_set = gtk_binding_set_by_class(klass);

    add_scroll_binding(binding_set, GDK_KEY_Left, GDK_CONTROL_MASK,
                       GTK_SCROLL_STEP_BACKWARD, TRUE);
    add_scroll_binding(binding_set, GDK_KEY_Right, GDK_CONTROL_MASK,
                       GTK_SCROLL_STEP_FORWARD, TRUE);
    add_scroll_binding(binding_set, GDK_KEY_Up, GDK_CONTROL_MASK,
                       GTK_SCROLL_STEP_BACKWARD, FALSE);
    add_scroll_binding(binding_set, GDK_KEY_Down, GDK_CONTROL_MASK,
                       GTK_SCROLL_STEP_FORWARD, FALSE);

    add_scroll_binding(binding_set, GDK_KEY_Page_Up, GDK_CONTROL_MASK,
                       GTK_SCROLL_PAGE_BACKWARD, TRUE);
    add_scroll_binding(binding_set, GDK_KEY_Page_Down, GDK_CONTROL_MASK,
                       GTK_SCROLL_PAGE_FORWARD, TRUE);
    add_scroll_binding(binding_set, GDK_KEY_Page_Up, 0,
                       GTK_SCROLL_PAGE_BACKWARD, FALSE);
    add_scroll_binding(binding_set, GDK_KEY_Page_Down, 0,
                       GTK_SCROLL_PAGE_FORWARD, FALSE);

    add_scroll_binding(binding_set, GDK_KEY_Home, GDK_CONTROL_MASK,
                       GTK_SCROLL_START, TRUE);
    add_scroll_binding(binding_set, GDK_KEY_End, GDK_CONTROL_MASK,
                       GTK_SCROLL_END, TRUE);
    add_scroll_binding(binding_set, GDK_KEY_Home, 0,
                       GTK_SCROLL_START, FALSE);
    add_scroll_binding(binding_set, GDK_KEY_End, 0,
                       GTK_SCROLL_END, FALSE);
}

static void
gwy_scroller_init(GwyScroller *scroller)
{
    scroller->priv = G_TYPE_INSTANCE_GET_PRIVATE(scroller,
                                                 GWY_TYPE_SCROLLER, Scroller);
    set_hadjustment(scroller, g_object_newv(GTK_TYPE_ADJUSTMENT, 0, NULL));
    set_vadjustment(scroller, g_object_newv(GTK_TYPE_ADJUSTMENT, 0, NULL));
}

static void
gwy_scroller_finalize(GObject *object)
{
    G_OBJECT_CLASS(gwy_scroller_parent_class)->finalize(object);
}

static void
gwy_scroller_dispose(GObject *object)
{
    GwyScroller *scroller = GWY_SCROLLER(object);
    set_hadjustment(scroller, NULL);
    set_vadjustment(scroller, NULL);
    G_OBJECT_CLASS(gwy_scroller_parent_class)->dispose(object);
}

static void
gwy_scroller_set_property(GObject *object,
                          guint prop_id,
                          const GValue *value,
                          GParamSpec *pspec)
{
    GwyScroller *scroller = GWY_SCROLLER(object);

    switch (prop_id) {
        case PROP_HADJUSTMENT:
        set_hadjustment(scroller, g_value_get_object(value));
        break;

        case PROP_VADJUSTMENT:
        set_vadjustment(scroller, g_value_get_object(value));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_scroller_get_property(GObject *object,
                          guint prop_id,
                          GValue *value,
                          GParamSpec *pspec)
{
    Scroller *priv = GWY_SCROLLER(object)->priv;

    switch (prop_id) {
        case PROP_HADJUSTMENT:
        g_value_set_object(value, priv->hadjustment);
        break;

        case PROP_VADJUSTMENT:
        g_value_set_object(value, priv->vadjustment);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_scroller_get_preferred_width(G_GNUC_UNUSED GtkWidget *widget,
                                 gint *minimum,
                                 gint *natural)
{
    *minimum = *natural = 1;
}

static void
gwy_scroller_get_preferred_height(G_GNUC_UNUSED GtkWidget *widget,
                                  gint *minimum,
                                  gint *natural)
{
    *minimum = *natural = 1;
}

static void
gwy_scroller_size_allocate(GtkWidget *widget,
                           GtkAllocation *allocation)
{
    GtkBin *bin = GTK_BIN(widget);
    GtkWidget *child = gtk_bin_get_child(bin);

    if (child && gtk_widget_get_visible(child))
        gtk_widget_size_allocate(child, allocation);

    GTK_WIDGET_CLASS(gwy_scroller_parent_class)->size_allocate(widget,
                                                               allocation);
}

static gboolean
gwy_scroller_scroll_event(GtkWidget *widget,
                          GdkEventScroll *event)
{
    Scroller *priv = GWY_SCROLLER(widget)->priv;
    GtkOrientation orientation;
    GtkAdjustment *adj;

    if (event->direction == GDK_SCROLL_UP
        || event->direction == GDK_SCROLL_DOWN) {
        adj = priv->vadjustment;
        orientation = GTK_ORIENTATION_VERTICAL;
    }
    else {
        adj = priv->hadjustment;
        orientation = GTK_ORIENTATION_HORIZONTAL;
    }

    gdouble delta = gwy_scroll_wheel_delta(adj, event, orientation);
    gdouble value = gtk_adjustment_get_value(adj),
            lower = gtk_adjustment_get_lower(adj),
            upper = gtk_adjustment_get_upper(adj),
            page_size = gtk_adjustment_get_page_size(adj);

    // Try to mimic scrollbars.  Or, if scrollbars are present elsewhere and
    // also connected, be consisten with them.
    value = CLAMP(value + delta, lower, upper - page_size);
    gtk_adjustment_set_value(adj, value);

    return TRUE;
}

static GType
gwy_scroller_child_type(GtkContainer *container)
{
    GtkWidget *child = gtk_bin_get_child(GTK_BIN(container));

    if (!child)
        return GTK_TYPE_SCROLLABLE;
    else
        return G_TYPE_NONE;
}

static void
gwy_scroller_add(GtkContainer *container,
                 GtkWidget *child)
{
    if (!GTK_IS_SCROLLABLE(child)) {
        g_warning("gwy_scroller_add(): cannot add non scrollable widget "
                  "use gwy_scroller_add_with_viewport() instead");
        return;
    }

    GtkBin *bin = GTK_BIN(container);
    GtkWidget *current_child = gtk_bin_get_child(bin);
    g_return_if_fail(!current_child);

    GTK_CONTAINER_CLASS(gwy_scroller_parent_class)->add(container, child);

    GwyScroller *scroller = GWY_SCROLLER(container);
    Scroller *priv = scroller->priv;
    g_object_set(child,
                 "hadjustment", priv->hadjustment,
                 "vadjustment", priv->vadjustment,
                 NULL);
}

static void
gwy_scroller_remove(GtkContainer *container,
                    GtkWidget *child)
{
    g_return_if_fail(child);
    g_return_if_fail(gtk_bin_get_child(GTK_BIN(container)) == child);
    g_object_set(child, "hadjustment", NULL, "vadjustment", NULL, NULL);
    GTK_CONTAINER_CLASS(gwy_scroller_parent_class)->remove(container, child);
}

static gboolean
gwy_scroller_scroll_child(GwyScroller *scroller,
                          GtkScrollType scroll,
                          gboolean horizontal)
{
    Scroller *priv = scroller->priv;

    switch (scroll) {
        case GTK_SCROLL_STEP_UP:
        scroll = GTK_SCROLL_STEP_BACKWARD;
        horizontal = FALSE;
        break;

        case GTK_SCROLL_STEP_DOWN:
        scroll = GTK_SCROLL_STEP_FORWARD;
        horizontal = FALSE;
        break;

        case GTK_SCROLL_STEP_LEFT:
        scroll = GTK_SCROLL_STEP_BACKWARD;
        horizontal = TRUE;
        break;

        case GTK_SCROLL_STEP_RIGHT:
        scroll = GTK_SCROLL_STEP_FORWARD;
        horizontal = TRUE;
        break;

        case GTK_SCROLL_PAGE_UP:
        scroll = GTK_SCROLL_PAGE_BACKWARD;
        horizontal = FALSE;
        break;

        case GTK_SCROLL_PAGE_DOWN:
        scroll = GTK_SCROLL_PAGE_FORWARD;
        horizontal = FALSE;
        break;

        case GTK_SCROLL_PAGE_LEFT:
        scroll = GTK_SCROLL_STEP_BACKWARD;
        horizontal = TRUE;
        break;

        case GTK_SCROLL_PAGE_RIGHT:
        scroll = GTK_SCROLL_STEP_FORWARD;
        horizontal = TRUE;
        break;

        case GTK_SCROLL_STEP_BACKWARD:
        case GTK_SCROLL_STEP_FORWARD:
        case GTK_SCROLL_PAGE_BACKWARD:
        case GTK_SCROLL_PAGE_FORWARD:
        case GTK_SCROLL_START:
        case GTK_SCROLL_END:
        break;

        default:
        g_warning("Invalid scroll type %u for "
                  "GtkScrolledWindow::scroll-child", scroll);
        return FALSE;
    }

    GtkAdjustment *adjustment = (horizontal
                                 ? priv->hadjustment
                                 : priv->vadjustment);
    gdouble value = gtk_adjustment_get_value(adjustment);

    switch (scroll) {
        case GTK_SCROLL_STEP_FORWARD:
        value += gtk_adjustment_get_step_increment(adjustment);
        break;

        case GTK_SCROLL_STEP_BACKWARD:
        value -= gtk_adjustment_get_step_increment(adjustment);
        break;

        case GTK_SCROLL_PAGE_FORWARD:
        value += gtk_adjustment_get_page_increment(adjustment);
        break;

        case GTK_SCROLL_PAGE_BACKWARD:
        value -= gtk_adjustment_get_page_increment(adjustment);
        break;

        case GTK_SCROLL_START:
        value = gtk_adjustment_get_lower(adjustment);
        break;

        case GTK_SCROLL_END:
        value = gtk_adjustment_get_upper(adjustment);
        break;

        default:
        g_assert_not_reached();
        break;
    }

    gtk_adjustment_set_value(adjustment, value);

    return TRUE;
}

/**
 * gwy_scroller_new:
 *
 * Creates a new scroller.
 *
 * Returns: A new scroller.
 **/
GtkWidget*
gwy_scroller_new(void)
{
    return g_object_newv(GWY_TYPE_SCROLLER, 0, NULL);
}

/**
 * gwy_scroller_set_hadjustment:
 * @scroller: A scroller.
 * @adjustment: Adjustment to use for the horizontal position.
 *
 * Sets the adjustment a scroller will use for the horizontal position.
 **/
void
gwy_scroller_set_hadjustment(GwyScroller *scroller,
                             GtkAdjustment *adjustment)
{
    g_return_if_fail(GWY_IS_SCROLLER(scroller));
    g_return_if_fail(!adjustment || GTK_IS_ADJUSTMENT(adjustment));
    if (!adjustment)
        adjustment = g_object_newv(GTK_TYPE_ADJUSTMENT, 0, NULL);
    if (!set_hadjustment(scroller, adjustment))
        return;

    g_object_notify_by_pspec(G_OBJECT(scroller), properties[PROP_HADJUSTMENT]);
}

/**
 * gwy_scroller_get_hadjustment:
 * @scroller: A scroller.
 *
 * Gets the adjustment a scroller uses for the horizontal position.
 *
 * Returns: (transfer none):
 *          The horizontal adjustment.
 **/
GtkAdjustment*
gwy_scroller_get_hadjustment(const GwyScroller *scroller)
{
    g_return_val_if_fail(GWY_IS_SCROLLER(scroller), NULL);
    return scroller->priv->hadjustment;
}

/**
 * gwy_scroller_set_vadjustment:
 * @scroller: A scroller.
 * @adjustment: Adjustment to use for the vertical position.
 *
 * Sets the adjustment a scroller will use for the vertical position.
 **/
void
gwy_scroller_set_vadjustment(GwyScroller *scroller,
                             GtkAdjustment *adjustment)
{
    g_return_if_fail(GWY_IS_SCROLLER(scroller));
    g_return_if_fail(!adjustment || GTK_IS_ADJUSTMENT(adjustment));
    if (!adjustment)
        adjustment = g_object_newv(GTK_TYPE_ADJUSTMENT, 0, NULL);
    if (!set_vadjustment(scroller, adjustment))
        return;

    g_object_notify_by_pspec(G_OBJECT(scroller), properties[PROP_VADJUSTMENT]);
}

/**
 * gwy_scroller_get_vadjustment:
 * @scroller: A scroller.
 *
 * Gets the adjustment a scroller uses for the vertical position.
 *
 * Returns: (transfer none):
 *          The horizontal adjustment.
 **/
GtkAdjustment*
gwy_scroller_get_vadjustment(const GwyScroller *scroller)
{
    g_return_val_if_fail(GWY_IS_SCROLLER(scroller), NULL);
    return scroller->priv->vadjustment;
}

static void
add_scroll_binding(GtkBindingSet *binding_set,
                   guint keyval,
                   GdkModifierType mask,
                   GtkScrollType scroll,
                   gboolean horizontal)
{
    guint keypad_keyval = keyval - GDK_KEY_Left + GDK_KEY_KP_Left;

    gtk_binding_entry_add_signal(binding_set, keyval, mask,
                                 "scroll-child", 2,
                                 GTK_TYPE_SCROLL_TYPE, scroll,
                                 G_TYPE_BOOLEAN, horizontal);
    gtk_binding_entry_add_signal(binding_set, keypad_keyval, mask,
                                 "scroll-child", 2,
                                 GTK_TYPE_SCROLL_TYPE, scroll,
                                 G_TYPE_BOOLEAN, horizontal);
}

static gboolean
set_hadjustment(GwyScroller *scroller,
                GtkAdjustment *adjustment)
{
    Scroller *priv = scroller->priv;
    if (!gwy_set_member_object(scroller, adjustment, GTK_TYPE_ADJUSTMENT,
                               &priv->hadjustment,
                               NULL))
        return FALSE;

    return TRUE;
}

static gboolean
set_vadjustment(GwyScroller *scroller,
                GtkAdjustment *adjustment)
{
    Scroller *priv = scroller->priv;
    if (!gwy_set_member_object(scroller, adjustment, GTK_TYPE_ADJUSTMENT,
                               &priv->vadjustment,
                               NULL))
        return FALSE;

    return TRUE;
}

/**
 * SECTION: scroller
 * @section_id: GwyScroller
 * @title: Scrollable area
 * @short_description: Simple scrollable area without any scrollbars
 *
 * #GwyScroller is somewhat similar to #GtkScrolledWindow.  However, it does
 * not show any scrollbars inside; the entire widget area is occupied by the
 * child.  The adjustments can be used to add scrollbars and other
 * #GtkRange<!-- -->-like widgets elsewhere, for instance on the outside.
 **/

/**
 * GwyScroller:
 *
 * Simple scrollable area widget without any toolbars.
 *
 * The #GwyScroller struct contains private data only and should be accessed
 * using the functions below.
 **/

/**
 * GwyScrollerClass:
 *
 * Class of simple scrollable areas without any toolbars.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
