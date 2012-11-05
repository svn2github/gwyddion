/*
 *  $Id$
 *
 *  GTK - The GIMP Toolkit
 *  Copyright(C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 *  GwySpinButton widget for GTK+
 *  Copyright(C) 1998 Lars Hamann and Stefan Jeske
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or(at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the
 *  Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 */

/*
 *  Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 *  file for a list of people on the GTK+ Team.  See the ChangeLog
 *  files for a list of changes.  These files are distributed with
 *  GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

/*  Modified by Yeti 2012.
 *  This file branched off the last good commit of GtkSpinButton before
 *  horizontal madness arrived: 68c74e142709458b95ccc76d8d21c3f038e42ecf */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <locale.h>
#include "libgwyui/marshal.h"
#include "libgwyui/spin-button.h"

#define MIN_SPIN_BUTTON_WIDTH 30
#define MAX_TIMER_CALLS       5
#define EPSILON               1e-10
#define MAX_DIGITS            20
#define MIN_ARROW_WIDTH       6
#define NO_ARROW 2

typedef struct _GwySpinButtonPrivate GwySpinButtonPrivate;

struct _GwySpinButtonPrivate {
    GtkAdjustment *adjustment;

    GdkWindow     *panel;

    guint32        timer;

    GtkSpinButtonUpdatePolicy update_policy;

    gdouble        climb_rate;
    gdouble        timer_step;

    guint          button        : 2;
    /* valid: GTK_ARROW_UP=0, GTK_ARROW_DOWN=1 or 2=NONE/BOTH */
    guint          click_child   : 2;
    guint          digits        : 10;
    guint          in_child      : 2;
    guint          need_timer    : 1;
    guint          numeric       : 1;
    guint          snap_to_ticks : 1;
    guint          timer_calls   : 3;
    guint          wrap          : 1;
};

enum {
    PROP_0,
    PROP_ADJUSTMENT,
    PROP_CLIMB_RATE,
    PROP_DIGITS,
    PROP_SNAP_TO_TICKS,
    PROP_NUMERIC,
    PROP_WRAP,
    PROP_UPDATE_POLICY,
    PROP_VALUE
};

/* Signals */
enum {
    INPUT,
    OUTPUT,
    VALUE_CHANGED,
    CHANGE_VALUE,
    WRAPPED,
    LAST_SIGNAL
};

static void     gwy_spin_button_editable_init      (GtkEditableInterface *iface);
static void     gwy_spin_button_finalize           (GObject *object);
static void     gwy_spin_button_set_property       (GObject *object,
                                                    guint prop_id,
                                                    const GValue *value,
                                                    GParamSpec *pspec);
static void     gwy_spin_button_get_property       (GObject *object,
                                                    guint prop_id,
                                                    GValue *value,
                                                    GParamSpec *pspec);
static void     gwy_spin_button_destroy            (GtkWidget *widget);
static void     gwy_spin_button_map                (GtkWidget *widget);
static void     gwy_spin_button_unmap              (GtkWidget *widget);
static void     gwy_spin_button_realize            (GtkWidget *widget);
static void     gwy_spin_button_unrealize          (GtkWidget *widget);
static void     gwy_spin_button_get_preferred_width(GtkWidget *widget,
                                                    gint *minimum,
                                                    gint *natural);
static void     gwy_spin_button_size_allocate      (GtkWidget *widget,
                                                    GtkAllocation *allocation);
static gint     gwy_spin_button_draw               (GtkWidget *widget,
                                                    cairo_t *cr);
static gint     gwy_spin_button_button_press       (GtkWidget *widget,
                                                    GdkEventButton *event);
static gint     gwy_spin_button_button_release     (GtkWidget *widget,
                                                    GdkEventButton *event);
static gint     gwy_spin_button_motion_notify      (GtkWidget *widget,
                                                    GdkEventMotion *event);
static gint     gwy_spin_button_enter_notify       (GtkWidget *widget,
                                                    GdkEventCrossing *event);
static gint     gwy_spin_button_leave_notify       (GtkWidget *widget,
                                                    GdkEventCrossing *event);
static gint     gwy_spin_button_focus_out          (GtkWidget *widget,
                                                    GdkEventFocus *event);
static void     gwy_spin_button_grab_notify        (GtkWidget *widget,
                                                    gboolean was_grabbed);
static void     gwy_spin_button_state_flags_changed(GtkWidget *widget,
                                                    GtkStateFlags previous_state);
static void     gwy_spin_button_style_updated      (GtkWidget *widget);
static void     gwy_spin_button_draw_arrow         (GwySpinButton *spinbutton,
                                                    GtkStyleContext *context,
                                                    cairo_t *cr,
                                                    GtkArrowType arrow_type);
static gboolean gwy_spin_button_timer              (GwySpinButton *spinbutton);
static gboolean gwy_spin_button_stop_spinning      (GwySpinButton *spin);
static void     gwy_spin_button_value_changed      (GtkAdjustment *adjustment,
                                                    GwySpinButton *spinbutton);
static gint     gwy_spin_button_key_release        (GtkWidget *widget,
                                                    GdkEventKey *event);
static gint     gwy_spin_button_scroll             (GtkWidget *widget,
                                                    GdkEventScroll *event);
static void     gwy_spin_button_activate           (GtkEntry *entry);
static void     gwy_spin_button_get_text_area_size (GtkEntry *entry,
                                                    gint *x,
                                                    gint *y,
                                                    gint *width,
                                                    gint *height);
static void     gwy_spin_button_snap               (GwySpinButton *spinbutton,
                                                    gdouble val);
static void     gwy_spin_button_insert_text        (GtkEditable *editable,
                                                    const gchar *new_text,
                                                    gint new_text_length,
                                                    gint *position);
static void     gwy_spin_button_real_spin          (GwySpinButton *spinbutton,
                                                    gdouble step);
static void     gwy_spin_button_real_change_value  (GwySpinButton *spin,
                                                    GtkScrollType scroll);
static gint     gwy_spin_button_default_input      (GwySpinButton *spinbutton,
                                                    gdouble *new_val);
static gint     gwy_spin_button_default_output     (GwySpinButton *spinbutton);
static gint     spin_button_get_arrow_size         (GwySpinButton *spinbutton);

static gboolean boolean_handled_accumulator(GSignalInvocationHint *ihint,
                                            GValue *return_accu,
                                            const GValue *handler_return,
                                            gpointer dummy);
static void entry_get_borders(GtkEntry *entry,
                              GtkBorder *border_out);

static guint spinbutton_signals[LAST_SIGNAL];

G_DEFINE_TYPE_WITH_CODE(GwySpinButton, gwy_spin_button, GTK_TYPE_ENTRY,
                        G_IMPLEMENT_INTERFACE(GTK_TYPE_EDITABLE,
                                              gwy_spin_button_editable_init));

#define add_spin_binding(binding_set, keyval, mask, scroll)                 \
        gtk_binding_entry_add_signal(binding_set, keyval, mask,             \
                                     "change_value", 1,                     \
                                     GTK_TYPE_SCROLL_TYPE, scroll)

static void
gwy_spin_button_class_init(GwySpinButtonClass *class)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(class);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(class);
    GtkEntryClass *entry_class = GTK_ENTRY_CLASS(class);

    gobject_class->finalize = gwy_spin_button_finalize;
    gobject_class->set_property = gwy_spin_button_set_property;
    gobject_class->get_property = gwy_spin_button_get_property;

    widget_class->destroy = gwy_spin_button_destroy;
    widget_class->map = gwy_spin_button_map;
    widget_class->unmap = gwy_spin_button_unmap;
    widget_class->realize = gwy_spin_button_realize;
    widget_class->unrealize = gwy_spin_button_unrealize;
    widget_class->get_preferred_width = gwy_spin_button_get_preferred_width;
    widget_class->size_allocate = gwy_spin_button_size_allocate;
    widget_class->draw = gwy_spin_button_draw;
    widget_class->scroll_event = gwy_spin_button_scroll;
    widget_class->button_press_event = gwy_spin_button_button_press;
    widget_class->button_release_event = gwy_spin_button_button_release;
    widget_class->motion_notify_event = gwy_spin_button_motion_notify;
    widget_class->key_release_event = gwy_spin_button_key_release;
    widget_class->enter_notify_event = gwy_spin_button_enter_notify;
    widget_class->leave_notify_event = gwy_spin_button_leave_notify;
    widget_class->focus_out_event = gwy_spin_button_focus_out;
    widget_class->grab_notify = gwy_spin_button_grab_notify;
    widget_class->state_flags_changed = gwy_spin_button_state_flags_changed;
    widget_class->style_updated = gwy_spin_button_style_updated;

    entry_class->activate = gwy_spin_button_activate;
    entry_class->get_text_area_size = gwy_spin_button_get_text_area_size;

    class->input = NULL;
    class->output = NULL;
    class->change_value = gwy_spin_button_real_change_value;

    g_object_class_install_property(gobject_class,
                                    PROP_ADJUSTMENT,
                                    g_param_spec_object("adjustment",
                                                        "Adjustment",
                                                        "The adjustment that holds the value of the spin button",
                                                        GTK_TYPE_ADJUSTMENT,
                                                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class,
                                    PROP_CLIMB_RATE,
                                    g_param_spec_double("climb-rate",
                                                        "Climb Rate",
                                                        "The acceleration rate when you hold down a button",
                                                        0.0,
                                                        G_MAXDOUBLE,
                                                        0.0,
                                                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class,
                                    PROP_DIGITS,
                                    g_param_spec_uint("digits",
                                                      "Digits",
                                                      "The number of decimal places to display",
                                                      0,
                                                      MAX_DIGITS,
                                                      0,
                                                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class,
                                    PROP_SNAP_TO_TICKS,
                                    g_param_spec_boolean("snap-to-ticks",
                                                         "Snap to Ticks",
                                                         "Whether erroneous values are automatically changed to a spin button's nearest step increment",
                                                         FALSE,
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class,
                                    PROP_NUMERIC,
                                    g_param_spec_boolean("numeric",
                                                         "Numeric",
                                                         "Whether non-numeric characters should be ignored",
                                                         FALSE,
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class,
                                    PROP_WRAP,
                                    g_param_spec_boolean("wrap",
                                                         "Wrap",
                                                         "Whether a spin button should wrap upon reaching its limits",
                                                         FALSE,
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class,
                                    PROP_UPDATE_POLICY,
                                    g_param_spec_enum("update-policy",
                                                      "Update Policy",
                                                      "Whether the spin button should update always, or only when the value is legal",
                                                      GTK_TYPE_SPIN_BUTTON_UPDATE_POLICY,
                                                      GTK_UPDATE_ALWAYS,
                                                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class,
                                    PROP_VALUE,
                                    g_param_spec_double("value",
                                                        "Value",
                                                        "Reads the current value, or sets a new value",
                                                        -G_MAXDOUBLE,
                                                        G_MAXDOUBLE,
                                                        0.0,
                                                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    gtk_widget_class_install_style_property_parser(widget_class,
                                                   g_param_spec_enum("shadow-type",
                                                                     "Shadow Type",
                                                                     "Style of bevel around the spin button",
                                                                     GTK_TYPE_SHADOW_TYPE,
                                                                     GTK_SHADOW_IN,
                                                                     G_PARAM_READABLE | G_PARAM_STATIC_STRINGS),
                                                   gtk_rc_property_parse_enum);

    /**
     * GwySpinButton::input:
     * @spinbutton: the object on which the signal was emitted
     * @new_value: (out) (type double):
     *             Return location for the new value.
     *
     * The ::input signal can be used to influence the conversion of
     * the users input into a double value. The signal handler is
     * expected to use gtk_entry_get_text() to retrieve the text of
     * the entry and set @new_value to the new value.
     *
     * The default conversion uses g_strtod().
     *
     * Returns: %TRUE for a successful conversion, %FALSE if the input
     *     was not handled, and %GTK_INPUT_ERROR if the conversion failed.
     */

    spinbutton_signals[INPUT]
        = g_signal_new("input",
                       G_TYPE_FROM_CLASS(gobject_class),
                       G_SIGNAL_RUN_LAST,
                       G_STRUCT_OFFSET(GwySpinButtonClass, input),
                       NULL, NULL,
                       _gwy_cclosure_marshal_INT__POINTER,
                       G_TYPE_INT, 1,
                       G_TYPE_POINTER);

    /**
     * GwySpinButton::output:
     * @spinbutton: the object which received the signal
     *
     * The ::output signal can be used to change to formatting
     * of the value that is displayed in the spin buttons entry.
     * |[
     * /&ast; show leading zeros &ast;/
     * static gboolean
     * on_output(GwySpinButton *spin,
     *            gpointer       data)
     * {
     *    GtkAdjustment *adjustment;
     *    gchar *text;
     *    int value;
     *
     *    adjustment = gwy_spin_button_get_adjustment(spin);
     *    value = (int)gtk_adjustment_get_value(adjustment);
     *    text = g_strdup_printf("%02d", value);
     *    gtk_entry_set_text(GTK_ENTRY(spin), text);
     *    g_free(text);
     *
     *    return TRUE;
     * }
     * ]|
     *
     * Returns: %TRUE if the value has been displayed
     */
    spinbutton_signals[OUTPUT]
        = g_signal_new("output",
                       G_TYPE_FROM_CLASS(gobject_class),
                       G_SIGNAL_RUN_LAST,
                       G_STRUCT_OFFSET(GwySpinButtonClass, output),
                       boolean_handled_accumulator, NULL,
                       _gwy_cclosure_marshal_BOOLEAN__VOID,
                       G_TYPE_BOOLEAN, 0);

    spinbutton_signals[VALUE_CHANGED]
        = g_signal_new("value-changed",
                       G_TYPE_FROM_CLASS(gobject_class),
                       G_SIGNAL_RUN_LAST,
                       G_STRUCT_OFFSET(GwySpinButtonClass, value_changed),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__VOID,
                       G_TYPE_NONE, 0);

    /**
     * GwySpinButton::wrapped:
     * @spinbutton: the object which received the signal
     *
     * The wrapped signal is emitted right after the spinbutton wraps
     * from its maximum to minimum value or vice-versa.
     *
     * Since: 2.10
     */
    spinbutton_signals[WRAPPED] =
        g_signal_new("wrapped",
                     G_TYPE_FROM_CLASS(gobject_class),
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(GwySpinButtonClass, wrapped),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE, 0);

    /* Action signals */
    spinbutton_signals[CHANGE_VALUE] =
        g_signal_new("change-value",
                     G_TYPE_FROM_CLASS(gobject_class),
                     G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                     G_STRUCT_OFFSET(GwySpinButtonClass, change_value),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__ENUM,
                     G_TYPE_NONE, 1,
                     GTK_TYPE_SCROLL_TYPE);

    GtkBindingSet *binding_set = gtk_binding_set_by_class(class);

    add_spin_binding(binding_set, GDK_KEY_Up, 0, GTK_SCROLL_STEP_UP);
    add_spin_binding(binding_set, GDK_KEY_KP_Up, 0, GTK_SCROLL_STEP_UP);
    add_spin_binding(binding_set, GDK_KEY_Down, 0, GTK_SCROLL_STEP_DOWN);
    add_spin_binding(binding_set, GDK_KEY_KP_Down, 0, GTK_SCROLL_STEP_DOWN);
    add_spin_binding(binding_set, GDK_KEY_Page_Up, 0, GTK_SCROLL_PAGE_UP);
    add_spin_binding(binding_set, GDK_KEY_Page_Down, 0, GTK_SCROLL_PAGE_DOWN);
    add_spin_binding(binding_set, GDK_KEY_Page_Up, GDK_CONTROL_MASK, GTK_SCROLL_END);
    add_spin_binding(binding_set, GDK_KEY_Page_Down, GDK_CONTROL_MASK, GTK_SCROLL_START);

    g_type_class_add_private(class, sizeof(GwySpinButtonPrivate));
}

static void
gwy_spin_button_editable_init(GtkEditableInterface *iface)
{
    iface->insert_text = gwy_spin_button_insert_text;
}

static void
gwy_spin_button_set_property(GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
    GwySpinButton *spinbutton = GWY_SPIN_BUTTON(object);
    GwySpinButtonPrivate *priv = spinbutton->priv;

    switch (prop_id) {
        GtkAdjustment *adjustment;

        case PROP_ADJUSTMENT:
        adjustment = GTK_ADJUSTMENT(g_value_get_object(value));
        if (!adjustment)
            adjustment = gtk_adjustment_new(0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
        gwy_spin_button_set_adjustment(spinbutton, adjustment);
        break;

        case PROP_CLIMB_RATE:
        gwy_spin_button_configure(spinbutton,
                                  priv->adjustment,
                                  g_value_get_double(value),
                                  priv->digits);
        break;

        case PROP_DIGITS:
        gwy_spin_button_configure(spinbutton,
                                  priv->adjustment,
                                  priv->climb_rate,
                                  g_value_get_uint(value));
        break;

        case PROP_SNAP_TO_TICKS:
        gwy_spin_button_set_snap_to_ticks(spinbutton, g_value_get_boolean(value));
        break;

        case PROP_NUMERIC:
        gwy_spin_button_set_numeric(spinbutton, g_value_get_boolean(value));
        break;

        case PROP_WRAP:
        gwy_spin_button_set_wrap(spinbutton, g_value_get_boolean(value));
        break;

        case PROP_UPDATE_POLICY:
        gwy_spin_button_set_update_policy(spinbutton, g_value_get_enum(value));
        break;

        case PROP_VALUE:
        gwy_spin_button_set_value(spinbutton, g_value_get_double(value));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_spin_button_get_property(GObject      *object,
                             guint         prop_id,
                             GValue       *value,
                             GParamSpec   *pspec)
{
    GwySpinButton *spinbutton = GWY_SPIN_BUTTON(object);
    GwySpinButtonPrivate *priv = spinbutton->priv;

    switch (prop_id) {
        case PROP_ADJUSTMENT:
        g_value_set_object(value, priv->adjustment);
        break;

        case PROP_CLIMB_RATE:
        g_value_set_double(value, priv->climb_rate);
        break;

        case PROP_DIGITS:
        g_value_set_uint(value, priv->digits);
        break;

        case PROP_SNAP_TO_TICKS:
        g_value_set_boolean(value, priv->snap_to_ticks);
        break;

        case PROP_NUMERIC:
        g_value_set_boolean(value, priv->numeric);
        break;

        case PROP_WRAP:
        g_value_set_boolean(value, priv->wrap);
        break;

        case PROP_UPDATE_POLICY:
        g_value_set_enum(value, priv->update_policy);
        break;

        case PROP_VALUE:
        g_value_set_double(value, gtk_adjustment_get_value(priv->adjustment));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_spin_button_init(GwySpinButton *spinbutton)
{
    GwySpinButtonPrivate *priv;
    GtkStyleContext *context;

    spinbutton->priv = G_TYPE_INSTANCE_GET_PRIVATE(spinbutton,
                                                    GTK_TYPE_SPIN_BUTTON,
                                                    GwySpinButtonPrivate);
    priv = spinbutton->priv;

    priv->adjustment = NULL;
    priv->panel = NULL;
    priv->timer = 0;
    priv->climb_rate = 0.0;
    priv->timer_step = 0.0;
    priv->update_policy = GTK_UPDATE_ALWAYS;
    priv->in_child = NO_ARROW;
    priv->click_child = NO_ARROW;
    priv->button = 0;
    priv->need_timer = FALSE;
    priv->timer_calls = 0;
    priv->digits = 0;
    priv->numeric = FALSE;
    priv->wrap = FALSE;
    priv->snap_to_ticks = FALSE;

    gwy_spin_button_set_adjustment(spinbutton,
                                   gtk_adjustment_new(0, 0, 0, 0, 0, 0));

    context = gtk_widget_get_style_context(GTK_WIDGET(spinbutton));
    gtk_style_context_add_class(context, GTK_STYLE_CLASS_SPINBUTTON);
}

static void
gwy_spin_button_finalize(GObject *object)
{
    gwy_spin_button_set_adjustment(GWY_SPIN_BUTTON(object), NULL);

    G_OBJECT_CLASS(gwy_spin_button_parent_class)->finalize(object);
}

static void
gwy_spin_button_destroy(GtkWidget *widget)
{
    gwy_spin_button_stop_spinning(GWY_SPIN_BUTTON(widget));

    GTK_WIDGET_CLASS(gwy_spin_button_parent_class)->destroy(widget);
}

static void
gwy_spin_button_map(GtkWidget *widget)
{
    GwySpinButton *spinbutton = GWY_SPIN_BUTTON(widget);
    GwySpinButtonPrivate *priv = spinbutton->priv;

    if (gtk_widget_get_realized(widget) && !gtk_widget_get_mapped(widget)) {
        GTK_WIDGET_CLASS(gwy_spin_button_parent_class)->map(widget);
        gdk_window_show(priv->panel);
    }
}

static void
gwy_spin_button_unmap(GtkWidget *widget)
{
    GwySpinButton *spinbutton = GWY_SPIN_BUTTON(widget);
    GwySpinButtonPrivate *priv = spinbutton->priv;

    if (gtk_widget_get_mapped(widget)) {
        gwy_spin_button_stop_spinning(GWY_SPIN_BUTTON(widget));

        gdk_window_hide(priv->panel);
        GTK_WIDGET_CLASS(gwy_spin_button_parent_class)->unmap(widget);
    }
}

static void
gwy_spin_button_realize(GtkWidget *widget)
{
    GwySpinButton *spinbutton = GWY_SPIN_BUTTON(widget);
    GwySpinButtonPrivate *priv = spinbutton->priv;
    GtkStyleContext *context;
    GtkStateFlags state;
    GtkAllocation allocation;
    GtkRequisition requisition;
    GdkWindowAttr attributes;
    gint attributes_mask;
    gboolean return_val;
    gint arrow_size;
    gint req_height;
    GtkBorder padding;

    arrow_size = spin_button_get_arrow_size(spinbutton);

    gtk_widget_get_preferred_size(widget, &requisition, NULL);
    req_height = requisition.height - gtk_widget_get_margin_top(widget) - gtk_widget_get_margin_bottom(widget);
    gtk_widget_get_allocation(widget, &allocation);

    gtk_widget_set_events(widget, gtk_widget_get_events(widget) |
                          GDK_KEY_RELEASE_MASK);
    GTK_WIDGET_CLASS(gwy_spin_button_parent_class)->realize(widget);

    attributes.window_type = GDK_WINDOW_CHILD;
    attributes.wclass = GDK_INPUT_ONLY;
    attributes.visual = gtk_widget_get_visual(widget);
    attributes.event_mask = gtk_widget_get_events(widget);
    attributes.event_mask |= (GDK_EXPOSURE_MASK
                              | GDK_BUTTON_PRESS_MASK
                              | GDK_BUTTON_RELEASE_MASK
                              | GDK_LEAVE_NOTIFY_MASK
                              | GDK_ENTER_NOTIFY_MASK
                              | GDK_POINTER_MOTION_MASK
                              | GDK_POINTER_MOTION_HINT_MASK);

    attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL;

    state = gtk_widget_get_state_flags(widget);
    context = gtk_widget_get_style_context(widget);
    gtk_style_context_get_padding(context, state, &padding);

    attributes.x = allocation.x + allocation.width - arrow_size - (padding.left + padding.right);
    attributes.y = allocation.y + (allocation.height - req_height)/2;
    attributes.width = arrow_size + padding.left + padding.right;
    attributes.height = req_height;

    priv->panel = gdk_window_new(gtk_widget_get_window(widget),
                                 &attributes, attributes_mask);
    gdk_window_set_user_data(priv->panel, widget);

    return_val = FALSE;
    g_signal_emit(spinbutton, spinbutton_signals[OUTPUT], 0, &return_val);
    if (return_val == FALSE)
        gwy_spin_button_default_output(spinbutton);

    gtk_widget_queue_resize(GTK_WIDGET(spinbutton));
}

static void
gwy_spin_button_unrealize(GtkWidget *widget)
{
    GwySpinButton *spin = GWY_SPIN_BUTTON(widget);
    GwySpinButtonPrivate *priv = spin->priv;

    GTK_WIDGET_CLASS(gwy_spin_button_parent_class)->unrealize(widget);

    if (priv->panel) {
        gdk_window_set_user_data(priv->panel, NULL);
        gdk_window_destroy(priv->panel);
        priv->panel = NULL;
    }
}

static int
compute_double_length(double val, int digits)
{
    int a;
    int extra;

    a = 1;
    if (fabs(val) > 1.0)
        a = floor(log10(fabs(val))) + 1;

    extra = 0;

    /* The dot: */
    if (digits > 0)
        extra++;

    /* The sign: */
    if (val < 0)
        extra++;

    return a + digits + extra;
}

static void
gwy_spin_button_get_preferred_width(GtkWidget *widget,
                                    gint      *minimum,
                                    gint      *natural)
{
    GwySpinButton *spinbutton = GWY_SPIN_BUTTON(widget);
    GwySpinButtonPrivate *priv = spinbutton->priv;
    GtkEntry *entry = GTK_ENTRY(widget);
    GtkStyleContext *style_context;
    GtkBorder padding;
    gint arrow_size;

    style_context = gtk_widget_get_style_context(widget);

    arrow_size = spin_button_get_arrow_size(spinbutton);

    GTK_WIDGET_CLASS(gwy_spin_button_parent_class)->get_preferred_width(widget, minimum, natural);

    if (gtk_entry_get_width_chars(entry) < 0) {
        PangoContext *context;
        const PangoFontDescription *font_desc;
        PangoFontMetrics *metrics;
        gint width;
        gint w;
        gint string_len;
        gint max_string_len;
        gint digit_width;
        gboolean interior_focus;
        gint focus_width;

        // XXX: Internal Gtk+ implementation uses some cached values.  But this
        // should not be that hot path...
        gtk_widget_style_get(widget,
                             "interior-focus", &interior_focus,
                             "focus-line-width", &focus_width,
                             NULL);

        font_desc = gtk_style_context_get_font(style_context, 0);

        context = gtk_widget_get_pango_context(widget);
        metrics = pango_context_get_metrics(context, font_desc,
                                            pango_context_get_language(context));

        digit_width = pango_font_metrics_get_approximate_digit_width(metrics);
        digit_width = PANGO_SCALE *
            ((digit_width + PANGO_SCALE - 1)/PANGO_SCALE);

        pango_font_metrics_unref(metrics);

        /* Get max of MIN_SPIN_BUTTON_WIDTH, size of upper, size of lower */
        width = MIN_SPIN_BUTTON_WIDTH;
        max_string_len = MAX(10, compute_double_length(1e9 * gtk_adjustment_get_step_increment(priv->adjustment),
                                                       priv->digits));

        string_len = compute_double_length(gtk_adjustment_get_upper(priv->adjustment),
                                           priv->digits);
        w = PANGO_PIXELS(MIN(string_len, max_string_len) * digit_width);
        width = MAX(width, w);
        string_len = compute_double_length(gtk_adjustment_get_lower(priv->adjustment), priv->digits);
        w = PANGO_PIXELS(MIN(string_len, max_string_len) * digit_width);
        width = MAX(width, w);

        GtkBorder borders;
        entry_get_borders(entry, &borders);
        width += borders.left + borders.right;

        *minimum = width;
        *natural = width;
    }

    gtk_style_context_get_padding(style_context,
                                  gtk_widget_get_state_flags(widget),
                                  &padding);

    *minimum += arrow_size + padding.left + padding.right;
    *natural += arrow_size + padding.left + padding.right;
}

static void
gwy_spin_button_size_allocate(GtkWidget     *widget,
                              GtkAllocation *allocation)
{
    GwySpinButton *spin = GWY_SPIN_BUTTON(widget);
    GwySpinButtonPrivate *priv = spin->priv;
    GtkAllocation panel_allocation;
    GtkRequisition requisition;
    GtkStyleContext *context;
    GtkStateFlags state;
    GtkBorder padding;
    gint arrow_size;
    gint panel_width;
    gint req_height;

    arrow_size = spin_button_get_arrow_size(spin);
    context = gtk_widget_get_style_context(widget);
    state = gtk_widget_get_state_flags(widget);

    gtk_style_context_get_padding(context, state, &padding);
    panel_width = arrow_size + padding.left + padding.right;

    gtk_widget_get_preferred_size(widget, &requisition, NULL);
    req_height = requisition.height - gtk_widget_get_margin_top(widget) - gtk_widget_get_margin_bottom(widget);

    gtk_widget_set_allocation(widget, allocation);

    if (gtk_widget_get_direction(widget) == GTK_TEXT_DIR_RTL)
        panel_allocation.x = allocation->x;
    else
        panel_allocation.x = allocation->x + allocation->width - panel_width;

    panel_allocation.width = panel_width;
    panel_allocation.height = MIN(req_height, allocation->height);

    panel_allocation.y = allocation->y +
        (allocation->height - req_height)/2;

    GTK_WIDGET_CLASS(gwy_spin_button_parent_class)->size_allocate(widget, allocation);

    if (gtk_widget_get_realized(widget)) {
        gdk_window_move_resize(priv->panel,
                               panel_allocation.x,
                               panel_allocation.y,
                               panel_allocation.width,
                               panel_allocation.height);
    }

    gtk_widget_queue_draw(GTK_WIDGET(spin));
}

static gint
gwy_spin_button_draw(GtkWidget      *widget,
                     cairo_t        *cr)
{
    GwySpinButton *spin = GWY_SPIN_BUTTON(widget);
    GwySpinButtonPrivate *priv = spin->priv;
    GtkStyleContext *context;
    GtkStateFlags state = 0;
    gboolean is_rtl;

    is_rtl = (gtk_widget_get_direction(widget) == GTK_TEXT_DIR_RTL);
    context = gtk_widget_get_style_context(widget);

    cairo_save(cr);
    GTK_WIDGET_CLASS(gwy_spin_button_parent_class)->draw(widget, cr);
    cairo_restore(cr);

    state = gtk_widget_get_state_flags(widget);

    gtk_style_context_save(context);
    gtk_style_context_set_state(context, state);

    if (is_rtl)
        gtk_style_context_set_junction_sides(context, GTK_JUNCTION_RIGHT);
    else
        gtk_style_context_set_junction_sides(context, GTK_JUNCTION_LEFT);

    gtk_cairo_transform_to_window(cr, widget, priv->panel);

    gwy_spin_button_draw_arrow(spin, context, cr, GTK_ARROW_UP);
    gwy_spin_button_draw_arrow(spin, context, cr, GTK_ARROW_DOWN);

    gtk_style_context_restore(context);

    return FALSE;
}

static gboolean
spin_button_at_limit(GwySpinButton *spinbutton,
                     GtkArrowType   arrow)
{
    GwySpinButtonPrivate *priv = spinbutton->priv;
    GtkArrowType effective_arrow;

    if (priv->wrap)
        return FALSE;

    if (gtk_adjustment_get_step_increment(priv->adjustment) > 0)
        effective_arrow = arrow;
    else
        effective_arrow = arrow == GTK_ARROW_UP ? GTK_ARROW_DOWN : GTK_ARROW_UP;

    if (effective_arrow == GTK_ARROW_UP &&
        (gtk_adjustment_get_upper(priv->adjustment) - gtk_adjustment_get_value(priv->adjustment) <= EPSILON))
        return TRUE;

    if (effective_arrow == GTK_ARROW_DOWN &&
        (gtk_adjustment_get_value(priv->adjustment) - gtk_adjustment_get_lower(priv->adjustment) <= EPSILON))
        return TRUE;

    return FALSE;
}

static void
gwy_spin_button_draw_arrow(GwySpinButton   *spinbutton,
                           GtkStyleContext *context,
                           cairo_t         *cr,
                           GtkArrowType     arrow_type)
{
    GwySpinButtonPrivate *priv;
    GtkJunctionSides junction;
    GtkStateFlags state;
    GtkWidget *widget;
    gdouble angle;
    gint panel_height;
    gdouble size, width, height, x, y;

    g_return_if_fail(arrow_type == GTK_ARROW_UP || arrow_type == GTK_ARROW_DOWN);

    gtk_style_context_save(context);
    gtk_style_context_add_class(context, GTK_STYLE_CLASS_BUTTON);
    if (arrow_type == GTK_ARROW_UP)
        gtk_style_context_add_class(context, GTK_STYLE_CLASS_TOP);
    else
        gtk_style_context_add_class(context, GTK_STYLE_CLASS_BOTTOM);

    priv = spinbutton->priv;
    widget = GTK_WIDGET(spinbutton);
    junction = gtk_style_context_get_junction_sides(context);

    panel_height = gdk_window_get_height(priv->panel);

    if (spin_button_at_limit(spinbutton, arrow_type))
        state = GTK_STATE_FLAG_INSENSITIVE;
    else {
        if (priv->click_child == arrow_type)
            state = GTK_STATE_ACTIVE;
        else {
            if (priv->in_child == arrow_type &&
                priv->click_child == NO_ARROW)
                state = GTK_STATE_FLAG_PRELIGHT;
            else
                state = gtk_widget_get_state_flags(widget);
        }
    }

    /* first, draw the background and the frame */
    if (arrow_type == GTK_ARROW_UP) {
        x = 0;
        y = 0;

        junction |= GTK_JUNCTION_BOTTOM;
    }
    else {
        x = 0;
        y = panel_height/2.0;

        junction |= GTK_JUNCTION_TOP;
    }

    gtk_style_context_set_junction_sides(context, junction);
    gtk_style_context_set_state(context, state);

    height = panel_height/2.0;
    width = gdk_window_get_width(priv->panel);
    gtk_render_background(context, cr, x, y, width, height);
    gtk_render_frame(context, cr, x, y, width, height);

    /* make the actual rendered arrow smaller than text size */
    size = spin_button_get_arrow_size(spinbutton);
    size = MIN(size, width);
    size *= 0.8;

    x = (width - size)/2.0;

    if (arrow_type == GTK_ARROW_UP) {
        y = (height - size/2.0)/2.0;
        angle = 0;
    }
    else {
        y = height + ((height - size/2.0)/2.0) - size/2.0;
        angle = G_PI;
    }

    gtk_render_arrow(context, cr, angle, x, y, size);

    gtk_style_context_restore(context);
}

static gint
gwy_spin_button_enter_notify(GtkWidget        *widget,
                             GdkEventCrossing *event)
{
    GwySpinButton *spin = GWY_SPIN_BUTTON(widget);
    GwySpinButtonPrivate *priv = spin->priv;
    GtkRequisition requisition;
    gint req_height;

    if (event->window == priv->panel) {
        GdkDevice *device;
        gint x;
        gint y;

        device = gdk_event_get_device((GdkEvent *) event);
        gdk_window_get_device_position(priv->panel, device, &x, &y, NULL);

        gtk_widget_get_preferred_size(widget, &requisition, NULL);
        req_height = requisition.height - gtk_widget_get_margin_top(widget) - gtk_widget_get_margin_bottom(widget);

        if (y <= req_height/2)
            priv->in_child = GTK_ARROW_UP;
        else
            priv->in_child = GTK_ARROW_DOWN;

        gtk_widget_queue_draw(GTK_WIDGET(spin));
    }

    return GTK_WIDGET_CLASS(gwy_spin_button_parent_class)->enter_notify_event(widget, event);
}

static gint
gwy_spin_button_leave_notify(GtkWidget        *widget,
                             GdkEventCrossing *event)
{
    GwySpinButton *spin = GWY_SPIN_BUTTON(widget);
    GwySpinButtonPrivate *priv = spin->priv;

    if (priv->in_child != NO_ARROW) {
        priv->in_child = NO_ARROW;
        gtk_widget_queue_draw(GTK_WIDGET(spin));
    }

    return GTK_WIDGET_CLASS(gwy_spin_button_parent_class)->leave_notify_event(widget, event);
}

static gint
gwy_spin_button_focus_out(GtkWidget     *widget,
                          GdkEventFocus *event)
{
    if (gtk_editable_get_editable(GTK_EDITABLE(widget)))
        gwy_spin_button_update(GWY_SPIN_BUTTON(widget));

    return GTK_WIDGET_CLASS(gwy_spin_button_parent_class)->focus_out_event(widget, event);
}

static void
gwy_spin_button_grab_notify(GtkWidget *widget,
                            gboolean   was_grabbed)
{
    GwySpinButton *spin = GWY_SPIN_BUTTON(widget);

    if (!was_grabbed) {
        if (gwy_spin_button_stop_spinning(spin))
            gtk_widget_queue_draw(GTK_WIDGET(spin));
    }
}

static void
gwy_spin_button_state_flags_changed(GtkWidget *widget,
                                    G_GNUC_UNUSED GtkStateFlags previous_state)
{
    GwySpinButton *spin = GWY_SPIN_BUTTON(widget);

    if (!gtk_widget_is_sensitive(widget)) {
        if (gwy_spin_button_stop_spinning(spin))
            gtk_widget_queue_draw(GTK_WIDGET(spin));
    }
}

static void
gwy_spin_button_style_updated(GtkWidget *widget)
{
    GwySpinButton *spin = GWY_SPIN_BUTTON(widget);
    GwySpinButtonPrivate *priv = spin->priv;

    if (gtk_widget_get_realized(widget)) {
        GtkStyleContext *context;

        context = gtk_widget_get_style_context(widget);
        gtk_style_context_set_background(context, priv->panel);
    }

    GTK_WIDGET_CLASS(gwy_spin_button_parent_class)->style_updated(widget);
}


static gint
gwy_spin_button_scroll(GtkWidget      *widget,
                       GdkEventScroll *event)
{
    GwySpinButton *spin = GWY_SPIN_BUTTON(widget);
    GwySpinButtonPrivate *priv = spin->priv;

    if (event->direction == GDK_SCROLL_UP) {
        if (!gtk_widget_has_focus(widget))
            gtk_widget_grab_focus(widget);
        gwy_spin_button_real_spin(spin, gtk_adjustment_get_step_increment(priv->adjustment));
    }
    else if (event->direction == GDK_SCROLL_DOWN) {
        if (!gtk_widget_has_focus(widget))
            gtk_widget_grab_focus(widget);
        gwy_spin_button_real_spin(spin, -gtk_adjustment_get_step_increment(priv->adjustment));
    }
    else
        return FALSE;

    return TRUE;
}

static gboolean
gwy_spin_button_stop_spinning(GwySpinButton *spin)
{
    GwySpinButtonPrivate *priv = spin->priv;
    gboolean did_spin = FALSE;

    if (priv->timer) {
        g_source_remove(priv->timer);
        priv->timer = 0;
        priv->need_timer = FALSE;

        did_spin = TRUE;
    }

    priv->button = 0;
    priv->timer_step = gtk_adjustment_get_step_increment(priv->adjustment);
    priv->timer_calls = 0;

    priv->click_child = NO_ARROW;

    return did_spin;
}

static void
start_spinning(GwySpinButton *spin,
               GtkArrowType   click_child,
               gdouble        step)
{
    GwySpinButtonPrivate *priv;

    g_return_if_fail(click_child == GTK_ARROW_UP || click_child == GTK_ARROW_DOWN);

    priv = spin->priv;

    priv->click_child = click_child;

    if (!priv->timer) {
        GtkSettings *settings = gtk_widget_get_settings(GTK_WIDGET(spin));
        guint        timeout;

        g_object_get(settings, "gtk-timeout-initial", &timeout, NULL);

        priv->timer_step = step;
        priv->need_timer = TRUE;
        priv->timer = gdk_threads_add_timeout(timeout,
                                              (GSourceFunc) gwy_spin_button_timer,
                                              (gpointer) spin);
    }
    gwy_spin_button_real_spin(spin, click_child == GTK_ARROW_UP ? step : -step);

    gtk_widget_queue_draw(GTK_WIDGET(spin));
}

static gint
gwy_spin_button_button_press(GtkWidget      *widget,
                             GdkEventButton *event)
{
    GwySpinButton *spin = GWY_SPIN_BUTTON(widget);
    GwySpinButtonPrivate *priv = spin->priv;

    if (!priv->button) {
        if (event->window == priv->panel) {
            GtkRequisition requisition;
            gint req_height;

            if (!gtk_widget_has_focus(widget))
                gtk_widget_grab_focus(widget);
            priv->button = event->button;

            if (gtk_editable_get_editable(GTK_EDITABLE(widget)))
                gwy_spin_button_update(spin);

            gtk_widget_get_preferred_size(widget, &requisition, NULL);
            req_height = requisition.height - gtk_widget_get_margin_top(widget) - gtk_widget_get_margin_bottom(widget);

            if (event->y <= req_height/2) {
                if (event->button == 1)
                    start_spinning(spin, GTK_ARROW_UP, gtk_adjustment_get_step_increment(priv->adjustment));
                else if (event->button == 2)
                    start_spinning(spin, GTK_ARROW_UP, gtk_adjustment_get_page_increment(priv->adjustment));
                else
                    priv->click_child = GTK_ARROW_UP;
            }
            else {
                if (event->button == 1)
                    start_spinning(spin, GTK_ARROW_DOWN, gtk_adjustment_get_step_increment(priv->adjustment));
                else if (event->button == 2)
                    start_spinning(spin, GTK_ARROW_DOWN, gtk_adjustment_get_page_increment(priv->adjustment));
                else
                    priv->click_child = GTK_ARROW_DOWN;
            }
            return TRUE;
        }
        else
            return GTK_WIDGET_CLASS(gwy_spin_button_parent_class)->button_press_event(widget, event);
    }
    return FALSE;
}

static gint
gwy_spin_button_button_release(GtkWidget      *widget,
                               GdkEventButton *event)
{
    GwySpinButton *spin = GWY_SPIN_BUTTON(widget);
    GwySpinButtonPrivate *priv = spin->priv;
    gint arrow_size;

    arrow_size = spin_button_get_arrow_size(spin);

    if (event->button == priv->button) {
        int click_child = priv->click_child;

        gwy_spin_button_stop_spinning(spin);

        if (event->button == 3) {
            GtkRequisition requisition;
            gint req_height;
            GtkStyleContext *context;
            GtkStateFlags state;
            GtkBorder padding;

            gtk_widget_get_preferred_size(widget, &requisition, NULL);
            req_height = requisition.height - gtk_widget_get_margin_top(widget) - gtk_widget_get_margin_bottom(widget);

            context = gtk_widget_get_style_context(widget);
            state = gtk_widget_get_state_flags(widget);
            gtk_style_context_get_padding(context, state, &padding);

            if (event->y >= 0 && event->x >= 0 &&
                event->y <= req_height &&
                event->x <= arrow_size + padding.left + padding.right) {
                if (click_child == GTK_ARROW_UP &&
                    event->y <= req_height/2) {
                    gdouble diff;

                    diff = gtk_adjustment_get_upper(priv->adjustment) - gtk_adjustment_get_value(priv->adjustment);
                    if (diff > EPSILON)
                        gwy_spin_button_real_spin(spin, diff);
                }
                else if (click_child == GTK_ARROW_DOWN &&
                         event->y > req_height/2) {
                    gdouble diff;

                    diff = gtk_adjustment_get_value(priv->adjustment) - gtk_adjustment_get_lower(priv->adjustment);
                    if (diff > EPSILON)
                        gwy_spin_button_real_spin(spin, -diff);
                }
            }
        }
        gtk_widget_queue_draw(GTK_WIDGET(spin));

        return TRUE;
    }
    else
        return GTK_WIDGET_CLASS(gwy_spin_button_parent_class)->button_release_event(widget, event);
}

static gint
gwy_spin_button_motion_notify(GtkWidget      *widget,
                              GdkEventMotion *event)
{
    GwySpinButton *spin = GWY_SPIN_BUTTON(widget);
    GwySpinButtonPrivate *priv = spin->priv;

    if (priv->button)
        return FALSE;

    if (event->window == priv->panel) {
        GtkRequisition requisition;
        gint req_height;
        gint y = event->y;

        gdk_event_request_motions(event);

        gtk_widget_get_preferred_size(widget, &requisition, NULL);
        req_height = requisition.height - gtk_widget_get_margin_top(widget) - gtk_widget_get_margin_bottom(widget);

        if (y <= req_height/2 &&
            priv->in_child == GTK_ARROW_DOWN) {
            priv->in_child = GTK_ARROW_UP;
            gtk_widget_queue_draw(GTK_WIDGET(spin));
        }
        else if (y > req_height/2 &&
                 priv->in_child == GTK_ARROW_UP) {
            priv->in_child = GTK_ARROW_DOWN;
            gtk_widget_queue_draw(GTK_WIDGET(spin));
        }

        return FALSE;
    }

    return GTK_WIDGET_CLASS(gwy_spin_button_parent_class)->motion_notify_event(widget, event);
}

static gint
gwy_spin_button_timer(GwySpinButton *spinbutton)
{
    GwySpinButtonPrivate *priv = spinbutton->priv;
    gboolean retval = FALSE;

    if (priv->timer) {
        if (priv->click_child == GTK_ARROW_UP)
            gwy_spin_button_real_spin(spinbutton, priv->timer_step);
        else
            gwy_spin_button_real_spin(spinbutton, -priv->timer_step);

        if (priv->need_timer) {
            GtkSettings *settings = gtk_widget_get_settings(GTK_WIDGET(spinbutton));
            guint        timeout;

            g_object_get(settings, "gtk-timeout-repeat", &timeout, NULL);

            priv->need_timer = FALSE;
            priv->timer = gdk_threads_add_timeout(timeout,
                                                  (GSourceFunc) gwy_spin_button_timer,
                                                  (gpointer) spinbutton);
        }
        else {
            if (priv->climb_rate > 0.0 && priv->timer_step
                < gtk_adjustment_get_page_increment(priv->adjustment)) {
                if (priv->timer_calls < MAX_TIMER_CALLS)
                    priv->timer_calls++;
                else {
                    priv->timer_calls = 0;
                    priv->timer_step += priv->climb_rate;
                }
            }
            retval = TRUE;
        }
    }

    return retval;
}

static void
gwy_spin_button_value_changed(GtkAdjustment *adjustment,
                              GwySpinButton *spinbutton)
{
    gboolean return_val;

    g_return_if_fail(GTK_IS_ADJUSTMENT(adjustment));

    return_val = FALSE;
    g_signal_emit(spinbutton, spinbutton_signals[OUTPUT], 0, &return_val);
    if (return_val == FALSE)
        gwy_spin_button_default_output(spinbutton);

    g_signal_emit(spinbutton, spinbutton_signals[VALUE_CHANGED], 0);

    gtk_widget_queue_draw(GTK_WIDGET(spinbutton));

    g_object_notify(G_OBJECT(spinbutton), "value");
}

static void
gwy_spin_button_real_change_value(GwySpinButton *spin,
                                  GtkScrollType  scroll)
{
    GwySpinButtonPrivate *priv = spin->priv;
    gdouble old_value;

    /* When the key binding is activated, there may be an outstanding
     * value, so we first have to commit what is currently written in
     * the spin buttons text entry. See #106574
     */
    gwy_spin_button_update(spin);

    old_value = gtk_adjustment_get_value(priv->adjustment);

    /* We don't test whether the entry is editable, since
     * this key binding conceptually corresponds to changing
     * the value with the buttons using the mouse, which
     * we allow for non-editable spin buttons.
     */
    switch (scroll) {
        case GTK_SCROLL_STEP_BACKWARD:
        case GTK_SCROLL_STEP_DOWN:
        case GTK_SCROLL_STEP_LEFT:
        gwy_spin_button_real_spin(spin, -priv->timer_step);

        if (priv->climb_rate > 0.0 && priv->timer_step
            < gtk_adjustment_get_page_increment(priv->adjustment)) {
            if (priv->timer_calls < MAX_TIMER_CALLS)
                priv->timer_calls++;
            else {
                priv->timer_calls = 0;
                priv->timer_step += priv->climb_rate;
            }
        }
        break;

        case GTK_SCROLL_STEP_FORWARD:
        case GTK_SCROLL_STEP_UP:
        case GTK_SCROLL_STEP_RIGHT:
        gwy_spin_button_real_spin(spin, priv->timer_step);

        if (priv->climb_rate > 0.0 && priv->timer_step
            < gtk_adjustment_get_page_increment(priv->adjustment)) {
            if (priv->timer_calls < MAX_TIMER_CALLS)
                priv->timer_calls++;
            else {
                priv->timer_calls = 0;
                priv->timer_step += priv->climb_rate;
            }
        }
        break;

        case GTK_SCROLL_PAGE_BACKWARD:
        case GTK_SCROLL_PAGE_DOWN:
        case GTK_SCROLL_PAGE_LEFT:
        gwy_spin_button_real_spin(spin, -gtk_adjustment_get_page_increment(priv->adjustment));
        break;

        case GTK_SCROLL_PAGE_FORWARD:
        case GTK_SCROLL_PAGE_UP:
        case GTK_SCROLL_PAGE_RIGHT:
        gwy_spin_button_real_spin(spin, gtk_adjustment_get_page_increment(priv->adjustment));
        break;

        case GTK_SCROLL_START:
        {
            gdouble diff = gtk_adjustment_get_value(priv->adjustment) - gtk_adjustment_get_lower(priv->adjustment);
            if (diff > EPSILON)
                gwy_spin_button_real_spin(spin, -diff);
            break;
        }

        case GTK_SCROLL_END:
        {
            gdouble diff = gtk_adjustment_get_upper(priv->adjustment) - gtk_adjustment_get_value(priv->adjustment);
            if (diff > EPSILON)
                gwy_spin_button_real_spin(spin, diff);
            break;
        }

        default:
        g_warning("Invalid scroll type %d for GwySpinButton::change-value", scroll);
        break;
    }

    gwy_spin_button_update(spin);

    if (gtk_adjustment_get_value(priv->adjustment) == old_value)
        gtk_widget_error_bell(GTK_WIDGET(spin));
}

static gint
gwy_spin_button_key_release(GtkWidget *widget,
                            G_GNUC_UNUSED GdkEventKey *event)
{
    GwySpinButton *spin = GWY_SPIN_BUTTON(widget);
    GwySpinButtonPrivate *priv = spin->priv;

    /* We only get a release at the end of a key repeat run, so reset the timer_step */
    priv->timer_step = gtk_adjustment_get_step_increment(priv->adjustment);
    priv->timer_calls = 0;

    return TRUE;
}

static void
gwy_spin_button_snap(GwySpinButton *spinbutton,
                     gdouble        val)
{
    GwySpinButtonPrivate *priv = spinbutton->priv;
    gdouble inc;
    gdouble tmp;

    inc = gtk_adjustment_get_step_increment(priv->adjustment);
    if (inc == 0)
        return;

    tmp = (val - gtk_adjustment_get_lower(priv->adjustment))/inc;
    if (tmp - floor(tmp) < ceil(tmp) - tmp)
        val = gtk_adjustment_get_lower(priv->adjustment) + floor(tmp) * inc;
    else
        val = gtk_adjustment_get_lower(priv->adjustment) + ceil(tmp) * inc;

    gwy_spin_button_set_value(spinbutton, val);
}

static void
gwy_spin_button_activate(GtkEntry *entry)
{
    if (gtk_editable_get_editable(GTK_EDITABLE(entry)))
        gwy_spin_button_update(GWY_SPIN_BUTTON(entry));

    /* Chain up so that entry->activates_default is honored */
    GTK_ENTRY_CLASS(gwy_spin_button_parent_class)->activate(entry);
}

static void
gwy_spin_button_get_text_area_size(GtkEntry *entry,
                                   gint     *x,
                                   gint     *y,
                                   gint     *width,
                                   gint     *height)
{
    GtkStyleContext *context;
    GtkStateFlags state;
    GtkWidget *widget;
    GtkBorder padding;
    gint arrow_size;
    gint panel_width;

    GTK_ENTRY_CLASS(gwy_spin_button_parent_class)->get_text_area_size(entry, x, y, width, height);

    widget = GTK_WIDGET(entry);
    state = gtk_widget_get_state_flags(widget);
    context = gtk_widget_get_style_context(widget);
    gtk_style_context_get_padding(context, state, &padding);

    arrow_size = spin_button_get_arrow_size(GWY_SPIN_BUTTON(entry));
    panel_width = arrow_size + padding.left + padding.right;

    if (width)
        *width -= panel_width;

    if (gtk_widget_get_direction(GTK_WIDGET(entry)) == GTK_TEXT_DIR_RTL && x)
        *x += panel_width;
}

static void
gwy_spin_button_insert_text(GtkEditable *editable,
                            const gchar *new_text,
                            gint         new_text_length,
                            gint        *position)
{
    GtkEntry *entry = GTK_ENTRY(editable);
    GwySpinButton *spin = GWY_SPIN_BUTTON(editable);
    GwySpinButtonPrivate *priv = spin->priv;
    GtkEditableInterface *parent_editable_iface;

    parent_editable_iface = g_type_interface_peek(gwy_spin_button_parent_class,
                                                  GTK_TYPE_EDITABLE);

    if (priv->numeric) {
        struct lconv *lc;
        gboolean sign;
        gint dotpos = -1;
        gint i;
        guint32 pos_sign;
        guint32 neg_sign;
        gint entry_length;
        const gchar *entry_text;

        entry_length = gtk_entry_get_text_length(entry);
        entry_text = gtk_entry_get_text(entry);

        lc = localeconv();

        if (*(lc->negative_sign))
            neg_sign = *(lc->negative_sign);
        else
            neg_sign = '-';

        if (*(lc->positive_sign))
            pos_sign = *(lc->positive_sign);
        else
            pos_sign = '+';

#ifdef G_OS_WIN32
        /* Workaround for bug caused by some Windows application messing
         * up the positive sign of the current locale, more specifically
         * HKEY_CURRENT_USER\Control Panel\International\sPositiveSign.
         * See bug #330743 and for instance
         * http://www.msnewsgroups.net/group/microsoft.public.dotnet.languages.csharp/topic36024.aspx
         *
         * I don't know if the positive sign always gets bogusly set to
         * a digit when the above Registry value is corrupted as
         * described. (In my test case, it got set to "8", and in the
         * bug report above it presumably was set ot "0".) Probably it
         * might get set to almost anything? So how to distinguish a
         * bogus value from some correct one for some locale? That is
         * probably hard, but at least we should filter out the
         * digits...
         */
        if (pos_sign >= '0' && pos_sign <= '9')
            pos_sign = '+';
#endif

        for (sign=0, i=0; i<entry_length; i++)
            if (((guint)entry_text[i] == neg_sign) ||
                ((guint)entry_text[i] == pos_sign)) {
                sign = 1;
                break;
            }

        if (sign && !(*position))
            return;

        for (dotpos=-1, i=0; i<entry_length; i++)
            if (entry_text[i] == *(lc->decimal_point)) {
                dotpos = i;
                break;
            }

        if (dotpos > -1 && *position > dotpos &&
            (gint)priv->digits - entry_length
            + dotpos - new_text_length + 1 < 0)
            return;

        for (i = 0; i < new_text_length; i++) {
            if ((guint)new_text[i] == neg_sign
                || (guint)new_text[i] == pos_sign) {
                if (sign || (*position) || i)
                    return;
                sign = TRUE;
            }
            else if (new_text[i] == *(lc->decimal_point)) {
                if (!priv->digits || dotpos > -1 ||
                    (new_text_length - 1 - i + entry_length
                     - *position > (gint)priv->digits))
                    return;
                dotpos = *position + i;
            }
            else if (new_text[i] < 0x30 || new_text[i] > 0x39)
                return;
        }
    }

    parent_editable_iface->insert_text(editable, new_text,
                                       new_text_length, position);
}

static void
gwy_spin_button_real_spin(GwySpinButton *spinbutton,
                          gdouble        increment)
{
    GwySpinButtonPrivate *priv = spinbutton->priv;
    GtkAdjustment *adjustment;
    gdouble new_value = 0.0;
    gboolean wrapped = FALSE;

    adjustment = priv->adjustment;

    new_value = gtk_adjustment_get_value(adjustment) + increment;

    if (increment > 0) {
        if (priv->wrap) {
            if (fabs(gtk_adjustment_get_value(adjustment) - gtk_adjustment_get_upper(adjustment)) < EPSILON) {
                new_value = gtk_adjustment_get_lower(adjustment);
                wrapped = TRUE;
            }
            else if (new_value > gtk_adjustment_get_upper(adjustment))
                new_value = gtk_adjustment_get_upper(adjustment);
        }
        else
            new_value = MIN(new_value, gtk_adjustment_get_upper(adjustment));
    }
    else if (increment < 0) {
        if (priv->wrap) {
            if (fabs(gtk_adjustment_get_value(adjustment) - gtk_adjustment_get_lower(adjustment)) < EPSILON) {
                new_value = gtk_adjustment_get_upper(adjustment);
                wrapped = TRUE;
            }
            else if (new_value < gtk_adjustment_get_lower(adjustment))
                new_value = gtk_adjustment_get_lower(adjustment);
        }
        else
            new_value = MAX(new_value, gtk_adjustment_get_lower(adjustment));
    }

    if (fabs(new_value - gtk_adjustment_get_value(adjustment)) > EPSILON)
        gtk_adjustment_set_value(adjustment, new_value);

    if (wrapped)
        g_signal_emit(spinbutton, spinbutton_signals[WRAPPED], 0);

    gtk_widget_queue_draw(GTK_WIDGET(spinbutton));
}

static gint
gwy_spin_button_default_input(GwySpinButton *spinbutton,
                              gdouble       *new_val)
{
    gchar *err = NULL;

    *new_val = g_strtod(gtk_entry_get_text(GTK_ENTRY(spinbutton)), &err);
    if (*err)
        return GTK_INPUT_ERROR;
    else
        return FALSE;
}

static gint
gwy_spin_button_default_output(GwySpinButton *spinbutton)
{
    GwySpinButtonPrivate *priv = spinbutton->priv;

    gchar *buf = g_strdup_printf("%0.*f", priv->digits, gtk_adjustment_get_value(priv->adjustment));

    if (strcmp(buf, gtk_entry_get_text(GTK_ENTRY(spinbutton))))
        gtk_entry_set_text(GTK_ENTRY(spinbutton), buf);
    g_free(buf);
    return FALSE;
}


/***********************************************************
 ***********************************************************
 ***                  Public interface                   ***
 ***********************************************************
 ***********************************************************/

/**
 * gwy_spin_button_configure:
 * @spinbutton: A spin button.
 * @adjustment: (allow-none):
 *              An adjustment.
 * @climb_rate: New climb rate.
 * @digits: Number of decimal places to display in the spin button.
 *
 * Changes the properties of an existing spin button.
 *
 * The adjustment, climb rate, and number of decimal places are all changed
 * accordingly, after this function call.
 **/
void
gwy_spin_button_configure(GwySpinButton *spinbutton,
                          GtkAdjustment *adjustment,
                          gdouble        climb_rate,
                          guint          digits)
{
    GwySpinButtonPrivate *priv;

    g_return_if_fail(GTK_IS_SPIN_BUTTON(spinbutton));

    priv = spinbutton->priv;

    if (adjustment)
        gwy_spin_button_set_adjustment(spinbutton, adjustment);
    else
        adjustment = priv->adjustment;

    g_object_freeze_notify(G_OBJECT(spinbutton));
    if (priv->digits != digits) {
        priv->digits = digits;
        g_object_notify(G_OBJECT(spinbutton), "digits");
    }

    if (priv->climb_rate != climb_rate) {
        priv->climb_rate = climb_rate;
        g_object_notify(G_OBJECT(spinbutton), "climb-rate");
    }
    g_object_thaw_notify(G_OBJECT(spinbutton));

    gtk_adjustment_value_changed(adjustment);
}

/**
 * gwy_spin_button_new:
 * @adjustment: (allow-none):
 *              The adjustment object that this spin button should use, or
 *              %NULL.
 * @climb_rate: How much the spin button changes when an arrow is clicked on.
 * @digits: the number of decimal places to display
 *
 * Creates a new spin button.
 *
 * Returns: A newly created spin button.
 **/
GtkWidget*
gwy_spin_button_new(GtkAdjustment *adjustment,
                    gdouble climb_rate,
                    guint digits)
{
    g_return_val_if_fail(!adjustment || GTK_IS_ADJUSTMENT(adjustment), NULL);
    GwySpinButton *spin = g_object_new(GTK_TYPE_SPIN_BUTTON, NULL);
    gwy_spin_button_configure(spin, adjustment, climb_rate, digits);
    return GTK_WIDGET(spin);
}

/**
 * gwy_spin_button_new_with_range:
 * @min: Minimum allowable value
 * @max: Maximum allowable value
 * @step: Increment added or subtracted by spinning the widget
 *
 * This is a convenience constructor that allows creation of a numeric
 * #GwySpinButton without manually creating an adjustment. The value is
 * initially set to the minimum value and a page increment of 10 * @step
 * is the default. The precision of the spin button is equivalent to the
 * precision of @step.
 *
 * Note that the way in which the precision is derived works best if @step
 * is a power of ten. If the resulting precision is not suitable for your
 * needs, use gwy_spin_button_set_digits() to correct it.
 *
 * Return value: The new spin button as a #GtkWidget
 **/
GtkWidget *
gwy_spin_button_new_with_range(gdouble min,
                               gdouble max,
                               gdouble step)
{
    GtkAdjustment *adjustment;
    GwySpinButton *spin;
    gint digits;

    g_return_val_if_fail(min <= max, NULL);
    g_return_val_if_fail(step != 0.0, NULL);

    spin = g_object_new(GTK_TYPE_SPIN_BUTTON, NULL);

    adjustment = gtk_adjustment_new(min, min, max, step, 10 * step, 0);

    if (fabs(step) >= 1.0 || step == 0.0)
        digits = 0;
    else {
        digits = abs((gint) floor(log10(fabs(step))));
        if (digits > MAX_DIGITS)
            digits = MAX_DIGITS;
    }

    gwy_spin_button_configure(spin, adjustment, step, digits);

    gwy_spin_button_set_numeric(spin, TRUE);

    return GTK_WIDGET(spin);
}

/* Callback used when the spin button's adjustment changes.
 * We need to redraw the arrows when the adjustment's range
 * changes, and reevaluate our size request.
 */
static void
adjustment_changed_cb(G_GNUC_UNUSED GtkAdjustment *adjustment,
                      gpointer data)
{
    GwySpinButton *spinbutton = GWY_SPIN_BUTTON(data);
    GwySpinButtonPrivate *priv = spinbutton->priv;

    priv->timer_step = gtk_adjustment_get_step_increment(priv->adjustment);
    gtk_widget_queue_resize(GTK_WIDGET(spinbutton));
}

/**
 * gwy_spin_button_set_adjustment:
 * @spinbutton: A spin button.
 * @adjustment: a #GtkAdjustment to replace the existing adjustment
 *
 * Replaces the #GtkAdjustment associated with @spinbutton.
 **/
void
gwy_spin_button_set_adjustment(GwySpinButton *spinbutton,
                               GtkAdjustment *adjustment)
{
    GwySpinButtonPrivate *priv;

    g_return_if_fail(GTK_IS_SPIN_BUTTON(spinbutton));

    priv = spinbutton->priv;

    if (priv->adjustment != adjustment) {
        if (priv->adjustment) {
            g_signal_handlers_disconnect_by_func(priv->adjustment,
                                                 gwy_spin_button_value_changed,
                                                 spinbutton);
            g_signal_handlers_disconnect_by_func(priv->adjustment,
                                                 adjustment_changed_cb,
                                                 spinbutton);
            g_object_unref(priv->adjustment);
        }
        priv->adjustment = adjustment;
        if (adjustment) {
            g_object_ref_sink(adjustment);
            g_signal_connect(adjustment, "value-changed",
                             G_CALLBACK(gwy_spin_button_value_changed),
                             spinbutton);
            g_signal_connect(adjustment, "changed",
                             G_CALLBACK(adjustment_changed_cb),
                             spinbutton);
            priv->timer_step = gtk_adjustment_get_step_increment(priv->adjustment);
        }

        gtk_widget_queue_resize(GTK_WIDGET(spinbutton));
    }

    g_object_notify(G_OBJECT(spinbutton), "adjustment");
}

/**
 * gwy_spin_button_get_adjustment:
 * @spinbutton: A spin button.
 *
 * Get the adjustment associated with a spin button.
 *
 * Return value: (transfer none): 
 *               The adjustment used by @spinbutton.
 **/
GtkAdjustment *
               gwy_spin_button_get_adjustment(GwySpinButton *spinbutton)
{
    g_return_val_if_fail(GTK_IS_SPIN_BUTTON(spinbutton), NULL);

    return spinbutton->priv->adjustment;
}

/**
 * gwy_spin_button_set_digits:
 * @spinbutton: A spin button.
 * @digits: the number of digits after the decimal point to be displayed for the spin button's value
 *
 * Set the precision to be displayed by @spinbutton. Up to 20 digit precision
 * is allowed.
 **/
void
gwy_spin_button_set_digits(GwySpinButton *spinbutton,
                           guint          digits)
{
    GwySpinButtonPrivate *priv;

    g_return_if_fail(GTK_IS_SPIN_BUTTON(spinbutton));

    priv = spinbutton->priv;

    if (priv->digits != digits) {
        priv->digits = digits;
        gwy_spin_button_value_changed(priv->adjustment, spinbutton);
        g_object_notify(G_OBJECT(spinbutton), "digits");

        /* since lower/upper may have changed */
        gtk_widget_queue_resize(GTK_WIDGET(spinbutton));
    }
}

/**
 * gwy_spin_button_get_digits:
 * @spinbutton: A spin button.
 *
 * Fetches the precision of @spinbutton. See gwy_spin_button_set_digits().
 *
 * Returns: the current precision
 **/
guint
gwy_spin_button_get_digits(GwySpinButton *spinbutton)
{
    g_return_val_if_fail(GTK_IS_SPIN_BUTTON(spinbutton), 0);

    return spinbutton->priv->digits;
}

/**
 * gwy_spin_button_set_increments:
 * @spinbutton: A spin button.
 * @step: increment applied for a button 1 press.
 * @page: increment applied for a button 2 press.
 *
 * Sets the step and page increments for spinbutton.  This affects how
 * quickly the value changes when the spin button's arrows are activated.
 **/
void
gwy_spin_button_set_increments(GwySpinButton *spinbutton,
                               gdouble        step,
                               gdouble        page)
{
    GwySpinButtonPrivate *priv;

    g_return_if_fail(GTK_IS_SPIN_BUTTON(spinbutton));

    priv = spinbutton->priv;

    gtk_adjustment_configure(priv->adjustment,
                             gtk_adjustment_get_value(priv->adjustment),
                             gtk_adjustment_get_lower(priv->adjustment),
                             gtk_adjustment_get_upper(priv->adjustment),
                             step,
                             page,
                             gtk_adjustment_get_page_size(priv->adjustment));
}

/**
 * gwy_spin_button_get_increments:
 * @spinbutton: A spin button.
 * @step: (out) (allow-none):
 *        Location to store step increment, or %NULL.
 * @page: (out) (allow-none):
 *        Location to store page increment, or %NULL.
 *
 * Gets the current step and page the increments used by @spinbutton. See
 * gwy_spin_button_set_increments().
 **/
void
gwy_spin_button_get_increments(GwySpinButton *spinbutton,
                               gdouble       *step,
                               gdouble       *page)
{
    GwySpinButtonPrivate *priv;

    g_return_if_fail(GTK_IS_SPIN_BUTTON(spinbutton));

    priv = spinbutton->priv;

    if (step)
        *step = gtk_adjustment_get_step_increment(priv->adjustment);
    if (page)
        *page = gtk_adjustment_get_page_increment(priv->adjustment);
}

/**
 * gwy_spin_button_set_range:
 * @spinbutton: A spin button.
 * @min: Minimum allowable value.
 * @max: Maximum allowable value.
 *
 * Sets the minimum and maximum allowable values for a spin button.
 *
 * If the current value is outside this range, it will be adjusted
 * to fit within the range, otherwise it will remain unchanged.
 */
void
gwy_spin_button_set_range(GwySpinButton *spinbutton,
                          gdouble        min,
                          gdouble        max)
{
    GtkAdjustment *adjustment;

    g_return_if_fail(GTK_IS_SPIN_BUTTON(spinbutton));

    adjustment = spinbutton->priv->adjustment;

    gtk_adjustment_configure(adjustment,
                             CLAMP(gtk_adjustment_get_value(adjustment), min, max),
                             min,
                             max,
                             gtk_adjustment_get_step_increment(adjustment),
                             gtk_adjustment_get_page_increment(adjustment),
                             gtk_adjustment_get_page_size(adjustment));
}

/**
 * gwy_spin_button_get_range:
 * @spinbutton: A spin button.
 * @min: (out) (allow-none):
 *       Location to store minimum allowed value, or %NULL.
 * @max: (out) (allow-none):
 *       Location to store maximum allowed value, or %NULL.
 *
 * Gets the range allowed for a spin button.
 *
 * See gwy_spin_button_set_range().
 */
void
    gwy_spin_button_get_range(GwySpinButton *spinbutton,
                              gdouble       *min,
                              gdouble       *max)
{
    GwySpinButtonPrivate *priv;

    g_return_if_fail(GTK_IS_SPIN_BUTTON(spinbutton));

    priv = spinbutton->priv;

    if (min)
        *min = gtk_adjustment_get_lower(priv->adjustment);
    if (max)
        *max = gtk_adjustment_get_upper(priv->adjustment);
}

/**
 * gwy_spin_button_get_value:
 * @spinbutton: A spin button.
 *
 * Get the value in the spin button.
 *
 * Return value: the value of @spinbutton
 **/
gdouble
gwy_spin_button_get_value(GwySpinButton *spinbutton)
{
    g_return_val_if_fail(GTK_IS_SPIN_BUTTON(spinbutton), 0.0);

    return gtk_adjustment_get_value(spinbutton->priv->adjustment);
}

/**
 * gwy_spin_button_get_value_as_int:
 * @spinbutton: A spin button.
 *
 * Get the value spin button represented as an integer.
 *
 * Return value: the value of @spinbutton
 **/
gint
gwy_spin_button_get_value_as_int(GwySpinButton *spinbutton)
{
    GwySpinButtonPrivate *priv;
    gdouble val;

    g_return_val_if_fail(GTK_IS_SPIN_BUTTON(spinbutton), 0);

    priv = spinbutton->priv;

    val = gtk_adjustment_get_value(priv->adjustment);
    if (val - floor(val) < ceil(val) - val)
        return floor(val);
    else
        return ceil(val);
}

/**
 * gwy_spin_button_set_value:
 * @spinbutton: A spin button.
 * @value: the new value
 *
 * Sets the value of spin button.
 **/
void
gwy_spin_button_set_value(GwySpinButton *spinbutton,
                          gdouble        value)
{
    GwySpinButtonPrivate *priv;

    g_return_if_fail(GTK_IS_SPIN_BUTTON(spinbutton));

    priv = spinbutton->priv;

    if (fabs(value - gtk_adjustment_get_value(priv->adjustment)) > EPSILON)
        gtk_adjustment_set_value(priv->adjustment, value);
    else {
        gint return_val = FALSE;
        g_signal_emit(spinbutton, spinbutton_signals[OUTPUT], 0, &return_val);
        if (return_val == FALSE)
            gwy_spin_button_default_output(spinbutton);
    }
}

/**
 * gwy_spin_button_set_update_policy:
 * @spinbutton: A spin button.
 * @policy: New update behaviour.
 *
 * Sets the update behaviour of a spin button.
 *
 * This determines wether the spin button is always updated
 * or only when a valid value is set.
 **/
void
gwy_spin_button_set_update_policy(GwySpinButton             *spinbutton,
                                  GtkSpinButtonUpdatePolicy  policy)
{
    GwySpinButtonPrivate *priv;

    g_return_if_fail(GTK_IS_SPIN_BUTTON(spinbutton));

    priv = spinbutton->priv;

    if (priv->update_policy != policy) {
        priv->update_policy = policy;
        g_object_notify(G_OBJECT(spinbutton), "update-policy");
    }
}

/**
 * gwy_spin_button_get_update_policy:
 * @spinbutton: A spin button.
 *
 * Gets the update behaviour of a spin button.
 *
 * See gwy_spin_button_set_update_policy().
 *
 * Return value: the current update policy
 **/
GtkSpinButtonUpdatePolicy
gwy_spin_button_get_update_policy(GwySpinButton *spinbutton)
{
    g_return_val_if_fail(GTK_IS_SPIN_BUTTON(spinbutton), GTK_UPDATE_ALWAYS);

    return spinbutton->priv->update_policy;
}

/**
 * gwy_spin_button_set_numeric:
 * @spinbutton: A spin button.
 * @numeric: flag indicating if only numeric entry is allowed
 *
 * Sets the flag that determines if non-numeric text can be typed
 * into the spin button.
 **/
void
gwy_spin_button_set_numeric(GwySpinButton *spinbutton,
                            gboolean       numeric)
{
    GwySpinButtonPrivate *priv;

    g_return_if_fail(GTK_IS_SPIN_BUTTON(spinbutton));

    priv = spinbutton->priv;

    numeric = numeric != FALSE;

    if (priv->numeric != numeric) {
        priv->numeric = numeric;
        g_object_notify(G_OBJECT(spinbutton), "numeric");
    }
}

/**
 * gwy_spin_button_get_numeric:
 * @spinbutton: A spin button.
 *
 * Returns whether non-numeric text can be typed into the spin button.
 * See gwy_spin_button_set_numeric().
 *
 * Return value: %TRUE if only numeric text can be entered
 **/
gboolean
gwy_spin_button_get_numeric(GwySpinButton *spinbutton)
{
    g_return_val_if_fail(GTK_IS_SPIN_BUTTON(spinbutton), FALSE);

    return spinbutton->priv->numeric;
}

/**
 * gwy_spin_button_set_wrap:
 * @spinbutton: A spin button.
 * @wrap: a flag indicating if wrapping behaviour is performed
 *
 * Sets the flag that determines if a spin button value wraps
 * around to the opposite limit when the upper or lower limit
 * of the range is exceeded.
 **/
void
gwy_spin_button_set_wrap(GwySpinButton  *spinbutton,
                         gboolean        wrap)
{
    GwySpinButtonPrivate *priv;

    g_return_if_fail(GTK_IS_SPIN_BUTTON(spinbutton));

    priv = spinbutton->priv;

    wrap = wrap != FALSE;

    if (priv->wrap != wrap) {
        priv->wrap = wrap;

        g_object_notify(G_OBJECT(spinbutton), "wrap");
    }
}

/**
 * gwy_spin_button_get_wrap:
 * @spinbutton: A spin button.
 *
 * Returns whether the spin button's value wraps around to the
 * opposite limit when the upper or lower limit of the range is
 * exceeded. See gwy_spin_button_set_wrap().
 *
 * Return value: %TRUE if the spin button wraps around
 **/
gboolean
gwy_spin_button_get_wrap(GwySpinButton *spinbutton)
{
    g_return_val_if_fail(GTK_IS_SPIN_BUTTON(spinbutton), FALSE);

    return spinbutton->priv->wrap;
}

static gint
spin_button_get_arrow_size(GwySpinButton *spinbutton)
{
    const PangoFontDescription *font_desc;
    GtkStyleContext *context;
    gint size;
    gint arrow_size;

    /* FIXME: use getter */
    context = gtk_widget_get_style_context(GTK_WIDGET(spinbutton));
    font_desc = gtk_style_context_get_font(context, 0);

    size = pango_font_description_get_size(font_desc);
    arrow_size = MAX(PANGO_PIXELS(size), MIN_ARROW_WIDTH);

    return arrow_size - arrow_size % 2; /* force even */
}

/**
 * gwy_spin_button_set_snap_to_ticks:
 * @spinbutton: A spin button.
 * @snap_to_ticks: a flag indicating if invalid values should be corrected
 *
 * Sets the policy as to whether values are corrected to the
 * nearest step increment when a spin button is activated after
 * providing an invalid value.
 **/
void
gwy_spin_button_set_snap_to_ticks(GwySpinButton *spinbutton,
                                  gboolean       snap_to_ticks)
{
    GwySpinButtonPrivate *priv;
    guint new_val;

    g_return_if_fail(GTK_IS_SPIN_BUTTON(spinbutton));

    priv = spinbutton->priv;

    new_val = (snap_to_ticks != 0);

    if (new_val != priv->snap_to_ticks) {
        priv->snap_to_ticks = new_val;
        if (new_val && gtk_editable_get_editable(GTK_EDITABLE(spinbutton)))
            gwy_spin_button_update(spinbutton);

        g_object_notify(G_OBJECT(spinbutton), "snap-to-ticks");
    }
}

/**
 * gwy_spin_button_get_snap_to_ticks:
 * @spinbutton: A spin button.
 *
 * Returns whether the values are corrected to the nearest step.
 * See gwy_spin_button_set_snap_to_ticks().
 *
 * Return value: %TRUE if values are snapped to the nearest step
 **/
gboolean
gwy_spin_button_get_snap_to_ticks(GwySpinButton *spinbutton)
{
    g_return_val_if_fail(GTK_IS_SPIN_BUTTON(spinbutton), FALSE);

    return spinbutton->priv->snap_to_ticks;
}

/**
 * gwy_spin_button_spin:
 * @spinbutton: A spin button.
 * @direction: a #GtkSpinType indicating the direction to spin
 * @increment: step increment to apply in the specified direction
 *
 * Increment or decrement a spin button's value in a specified
 * direction by a specified amount.
 **/
void
gwy_spin_button_spin(GwySpinButton *spinbutton,
                     GtkSpinType    direction,
                     gdouble        increment)
{
    GwySpinButtonPrivate *priv;
    GtkAdjustment *adjustment;
    gdouble diff;

    g_return_if_fail(GTK_IS_SPIN_BUTTON(spinbutton));

    priv = spinbutton->priv;

    adjustment = priv->adjustment;

    /* for compatibility with the 1.0.x version of this function */
    if (increment != 0 && increment != gtk_adjustment_get_step_increment(adjustment) &&
        (direction == GTK_SPIN_STEP_FORWARD ||
         direction == GTK_SPIN_STEP_BACKWARD)) {
        if (direction == GTK_SPIN_STEP_BACKWARD && increment > 0)
            increment = -increment;
        direction = GTK_SPIN_USER_DEFINED;
    }

    switch (direction) {
        case GTK_SPIN_STEP_FORWARD:

        gwy_spin_button_real_spin(spinbutton, gtk_adjustment_get_step_increment(adjustment));
        break;

        case GTK_SPIN_STEP_BACKWARD:

        gwy_spin_button_real_spin(spinbutton, -gtk_adjustment_get_step_increment(adjustment));
        break;

        case GTK_SPIN_PAGE_FORWARD:

        gwy_spin_button_real_spin(spinbutton, gtk_adjustment_get_page_increment(adjustment));
        break;

        case GTK_SPIN_PAGE_BACKWARD:

        gwy_spin_button_real_spin(spinbutton, -gtk_adjustment_get_page_increment(adjustment));
        break;

        case GTK_SPIN_HOME:

        diff = gtk_adjustment_get_value(adjustment) - gtk_adjustment_get_lower(adjustment);
        if (diff > EPSILON)
            gwy_spin_button_real_spin(spinbutton, -diff);
        break;

        case GTK_SPIN_END:

        diff = gtk_adjustment_get_upper(adjustment) - gtk_adjustment_get_value(adjustment);
        if (diff > EPSILON)
            gwy_spin_button_real_spin(spinbutton, diff);
        break;

        case GTK_SPIN_USER_DEFINED:

        if (increment != 0)
            gwy_spin_button_real_spin(spinbutton, increment);
        break;

        default:
        break;
    }
}

/**
 * gwy_spin_button_update:
 * @spinbutton: A spin button.
 *
 * Manually force an update of the spin button.
 **/
void
gwy_spin_button_update(GwySpinButton *spinbutton)
{
    GwySpinButtonPrivate *priv;
    gdouble val;
    gint error = 0;
    gint return_val;

    g_return_if_fail(GTK_IS_SPIN_BUTTON(spinbutton));

    priv = spinbutton->priv;

    return_val = FALSE;
    g_signal_emit(spinbutton, spinbutton_signals[INPUT], 0, &val, &return_val);
    if (return_val == FALSE) {
        return_val = gwy_spin_button_default_input(spinbutton, &val);
        error = (return_val == GTK_INPUT_ERROR);
    }
    else if (return_val == GTK_INPUT_ERROR)
        error = 1;

    gtk_widget_queue_draw(GTK_WIDGET(spinbutton));

    if (priv->update_policy == GTK_UPDATE_ALWAYS) {
        if (val < gtk_adjustment_get_lower(priv->adjustment))
            val = gtk_adjustment_get_lower(priv->adjustment);
        else if (val > gtk_adjustment_get_upper(priv->adjustment))
            val = gtk_adjustment_get_upper(priv->adjustment);
    }
    else if ((priv->update_policy == GTK_UPDATE_IF_VALID) &&
             (error ||
              val < gtk_adjustment_get_lower(priv->adjustment) ||
              val > gtk_adjustment_get_upper(priv->adjustment))) {
        gwy_spin_button_value_changed(priv->adjustment, spinbutton);
        return;
    }

    if (priv->snap_to_ticks)
        gwy_spin_button_snap(spinbutton, val);
    else
        gwy_spin_button_set_value(spinbutton, val);
}

// Adopted and adapted private Gtk+ code.
static gboolean
boolean_handled_accumulator(G_GNUC_UNUSED GSignalInvocationHint *ihint,
                            GValue *return_accu,
                            const GValue *handler_return,
                            G_GNUC_UNUSED gpointer dummy)
{
    gboolean signal_handled = g_value_get_boolean(handler_return);
    g_value_set_boolean(return_accu, signal_handled);
    return !signal_handled;
}

static void
entry_get_borders(GtkEntry *entry,
                  GtkBorder *border_out)
{
    GtkWidget *widget = GTK_WIDGET (entry);
    GtkBorder tmp = { 0, 0, 0, 0 };
    GtkStyleContext *context;

    context = gtk_widget_get_style_context(widget);
    gtk_style_context_get_padding(context, 0, &tmp);

    if (gtk_entry_get_has_frame(entry)) {
        GtkBorder border;

        gtk_style_context_get_border (context, 0, &border);
        tmp.top += border.top;
        tmp.right += border.right;
        tmp.bottom += border.bottom;
        tmp.left += border.left;
    }

    gboolean interior_focus;
    gint focus_width;
    gtk_widget_style_get(GTK_WIDGET(entry),
                         "focus-line-width", &focus_width,
                         "interior-focus", &interior_focus,
                         NULL);

    if (!interior_focus) {
        tmp.top += focus_width;
        tmp.right += focus_width;
        tmp.bottom += focus_width;
        tmp.left += focus_width;
    }

    if (border_out != NULL)
        *border_out = tmp;
}


/**
 * SECTION:spin-button
 * @Title: GwySpinButton
 * @Short_description: Retrieve an integer or floating-point number from
 *     the user
 * @See_also: #GtkEntry
 *
 * A #GwySpinButton is an ideal way to allow the user to set the value of
 * some attribute. Rather than having to directly type a number into a
 * #GtkEntry, GwySpinButton allows the user to click on one of two arrows
 * to increment or decrement the displayed value. A value can still be
 * typed in, with the bonus that it can be checked to ensure it is in a
 * given range.
 *
 * The main properties of a GwySpinButton are through an adjustment.
 * See the #GtkAdjustment section for more details about an adjustment's
 * properties.
 *
 * <example>
 * <title>Using a GwySpinButton to get an integer</title>
 * <programlisting>
 * // Provides a function to retrieve an integer value from a
 * // GwySpinButton and creates a spin button to model percentage
 * // values.
 *
 * gint
 * grab_int_value(GwySpinButton *button,
 *                gpointer       user_data)
 * {
 *     return gwy_spin_button_get_value_as_int(button);
 * }
 *
 * void
 * create_integer_spin_button(void)
 * {
 *     GtkWidget *window, *button;
 *     GtkAdjustment *adjustment;
 *
 *     adjustment = gtk_adjustment_new(50.0, 0.0, 100.0, 1.0, 5.0, 0.0);
 *
 *     window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
 *     gtk_container_set_border_width(GTK_CONTAINER(window), 5);
 *
 *     // Create the spinbutton, with no decimal places.
 *     button = gwy_spin_button_new(adjustment, 1.0, 0);
 *     gtk_container_add(GTK_CONTAINER(window), button);
 *
 *     gtk_widget_show_all(window);
 * }
 * </programlisting>
 * </example>
 *
 * <example>
 * <title>Using a GwySpinButton to get a floating point value</title>
 * <programlisting>
 * // Provides a function to retrieve a floating point value from a
 * // GwySpinButton, and creates a high precision spin button.
 *
 * gfloat
 * grab_float_value(GwySpinButton *button,
 *                  gpointer       user_data)
 * {
 *     return gwy_spin_button_get_value(button);
 * }
 *
 * void
 * create_floating_spin_button(void)
 * {
 *     GtkWidget *window, *button;
 *     GtkAdjustment *adjustment;
 *
 *     adjustment = gtk_adjustment_new(2.500, 0.0, 5.0, 0.001, 0.1, 0.0);
 *
 *     window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
 *     gtk_container_set_border_width(GTK_CONTAINER(window), 5);
 *
 *     // Create the spinbutton, with three decimal places.
 *     button = gwy_spin_button_new(adjustment, 0.001, 3);
 *     gtk_container_add(GTK_CONTAINER(window), button);
 *
 *     gtk_widget_show_all(window);
 * }
 * </programlisting>
 * </example>
 **/

/**
 * GwySpinButton:
 *
 * The #GwySpinButton struct contains only private data and should
 * not be directly modified.
 */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
