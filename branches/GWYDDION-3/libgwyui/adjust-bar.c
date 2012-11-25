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

#define pangoscale ((gdouble)PANGO_SCALE)

enum {
    PROP_0,
    PROP_ADJUSTMENT,
    PROP_MAPPING,
    PROP_LABEL,
    PROP_USE_MARKUP,
    PROP_USE_UNDERLINE,
    PROP_MNEMONIC_WIDGET,
    PROP_MNEMONIC_KEYVAL,
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
    gboolean adjustment_ok;
    gdouble oldvalue;    // This is to avoid acting on no-change notifications.

    GwyScaleMappingType mapping;
    MappingFunc map_value;
    MappingFunc map_position;
    gdouble a;
    gdouble b;

    GtkWidget *mnemonic_widget;
    guint mnemonic_keyval;
    gchar *label;
    gboolean use_markup;
    gboolean use_underline;
    PangoLayout *layout;
    gint ascent;
    gint descent;
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
static void     gwy_adjust_bar_get_preferred_width (GtkWidget *widget,
                                                    gint *minimum,
                                                    gint *natural);
static void     gwy_adjust_bar_get_preferred_height(GtkWidget *widget,
                                                    gint *minimum,
                                                    gint *natural);
static void     gwy_adjust_bar_realize             (GtkWidget *widget);
static void     gwy_adjust_bar_unrealize           (GtkWidget *widget);
static void     gwy_adjust_bar_map                 (GtkWidget *widget);
static void     gwy_adjust_bar_unmap               (GtkWidget *widget);
static void     gwy_adjust_bar_size_allocate       (GtkWidget *widget,
                                                    GtkAllocation *allocation);
static void     gwy_adjust_bar_style_updated       (GtkWidget *widget);
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
static gboolean set_adjustment                     (GwyAdjustBar *adjbar,
                                                    GtkAdjustment *adjustment);
static gboolean set_mapping                        (GwyAdjustBar *adjbar,
                                                    GwyScaleMappingType mapping);
static gboolean set_use_markup                     (GwyAdjustBar *adjbar,
                                                    gboolean use_markup);
static gboolean set_use_underline                  (GwyAdjustBar *adjbar,
                                                    gboolean use_underline);
static gboolean set_label                          (GwyAdjustBar *adjbar,
                                                    const gchar *label);
static gboolean set_mnemonic_widget                (GwyAdjustBar *adjbar,
                                                    GtkWidget *widget);
static void     create_input_window                (GwyAdjustBar *adjbar);
static void     destroy_input_window               (GwyAdjustBar *adjbar);
static void     adjustment_changed                 (GwyAdjustBar *adjbar,
                                                    GtkAdjustment *adjustment);
static void     adjustment_value_changed           (GwyAdjustBar *adjbar,
                                                    GtkAdjustment *adjustment);
static void     move_to_position                   (GwyAdjustBar *adjbar,
                                                    gdouble x);
static void     update_mapping                     (GwyAdjustBar *adjbar);
static void     ensure_layout                      (GwyAdjustBar *adjbar);
static void     draw_bar                           (GwyAdjustBar *adjbar,
                                                    cairo_t *cr);
static void     draw_label                         (GwyAdjustBar *adjbar,
                                                    cairo_t *cr);
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
static void     ensure_cursors                     (GwyAdjustBar *adjbar);
static void     discard_cursors                    (GwyAdjustBar *adjbar);

static GParamSpec *properties[N_PROPS];
static guint signals[N_SIGNALS];

G_DEFINE_TYPE(GwyAdjustBar, gwy_adjust_bar, GTK_TYPE_WIDGET);

static void
gwy_adjust_bar_class_init(GwyAdjustBarClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

    g_type_class_add_private(klass, sizeof(AdjustBar));

    gobject_class->dispose = gwy_adjust_bar_dispose;
    gobject_class->finalize = gwy_adjust_bar_finalize;
    gobject_class->get_property = gwy_adjust_bar_get_property;
    gobject_class->set_property = gwy_adjust_bar_set_property;

    widget_class->get_preferred_width = gwy_adjust_bar_get_preferred_width;
    widget_class->get_preferred_height = gwy_adjust_bar_get_preferred_height;
    widget_class->realize = gwy_adjust_bar_realize;
    widget_class->unrealize = gwy_adjust_bar_unrealize;
    widget_class->map = gwy_adjust_bar_map;
    widget_class->unmap = gwy_adjust_bar_unmap;
    widget_class->size_allocate = gwy_adjust_bar_size_allocate;
    widget_class->style_updated = gwy_adjust_bar_style_updated;
    widget_class->draw = gwy_adjust_bar_draw;
    widget_class->enter_notify_event = gwy_adjust_bar_enter_notify;
    widget_class->leave_notify_event = gwy_adjust_bar_leave_notify;
    widget_class->scroll_event = gwy_adjust_bar_scroll;
    widget_class->button_press_event = gwy_adjust_bar_button_press;
    widget_class->button_release_event = gwy_adjust_bar_button_release;
    widget_class->motion_notify_event = gwy_adjust_bar_motion_notify;

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

    properties[PROP_LABEL]
        = g_param_spec_string("label",
                              "Label",
                              "Label, including any markup and underline.",
                              "",
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_USE_MARKUP]
        = g_param_spec_boolean("use-markup",
                               "Use markup",
                               "Whether the text of the label includes Pango "
                               "markup. See pango_parse_markup().",
                               FALSE,
                               G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_USE_UNDERLINE]
        = g_param_spec_boolean("use-underline",
                               "Use underline",
                               "Whether an underline in the text indicates "
                               "the next character should be used for the "
                               "mnemonic accelerator key.",
                               FALSE,
                               G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_MNEMONIC_WIDGET]
        = g_param_spec_object("mnemonic-widget",
                              "Mnemonic widget",
                              "Widget to be activated when the mnemonic "
                              "key is pressed.",
                              GTK_TYPE_WIDGET,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_MNEMONIC_KEYVAL]
        = g_param_spec_uint("mnemonic-keyval",
                            "Mnemonic keyval",
                            "Mnemonic accelerator key for this label.",
                            0, G_MAXUINT, G_MAXUINT,
                            G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    for (guint i = 1; i < N_PROPS; i++)
        g_object_class_install_property(gobject_class, i, properties[i]);

    /**
     * GwyAdjustBar::change-value:
     * @gwyadjbar: The #GwyAdjustBar which received the signal.
     * @arg1: Scroll type determing the value change.
     *
     * The ::modify-range signal is emitted when the user interactively
     * changes the value.
     *
     * It is an action signal.
     **/
    // TODO: This is an action signal.  We must implement it.
    signals[SGNL_CHANGE_VALUE]
        = g_signal_new_class_handler("change-value",
                                     G_OBJECT_CLASS_TYPE(klass),
                                     G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                                     NULL, NULL, NULL,
                                     g_cclosure_marshal_VOID__ENUM,
                                     G_TYPE_NONE, 1,
                                     GWY_TYPE_SCALE_MAPPING_TYPE);
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
}

static void
gwy_adjust_bar_finalize(GObject *object)
{
    AdjustBar *priv = GWY_ADJUST_BAR(object)->priv;
    GWY_FREE(priv->label);
    G_OBJECT_CLASS(gwy_adjust_bar_parent_class)->finalize(object);
}

static void
gwy_adjust_bar_dispose(GObject *object)
{
    GwyAdjustBar *adjbar = GWY_ADJUST_BAR(object);
    set_adjustment(adjbar, NULL);
    set_mnemonic_widget(adjbar, NULL);
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

        case PROP_LABEL:
        set_label(adjbar, g_value_get_string(value));
        break;

        case PROP_USE_MARKUP:
        set_use_markup(adjbar, g_value_get_boolean(value));
        break;

        case PROP_USE_UNDERLINE:
        set_use_underline(adjbar, g_value_get_boolean(value));
        break;

        case PROP_MNEMONIC_WIDGET:
        set_mnemonic_widget(adjbar, g_value_get_object(value));
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

        case PROP_LABEL:
        g_value_set_string(value, priv->label);
        break;

        case PROP_USE_MARKUP:
        g_value_set_boolean(value, priv->use_markup);
        break;

        case PROP_USE_UNDERLINE:
        g_value_set_boolean(value, priv->use_underline);
        break;

        case PROP_MNEMONIC_WIDGET:
        g_value_set_object(value, priv->mnemonic_widget);
        break;

        case PROP_MNEMONIC_KEYVAL:
        g_value_set_uint(value, priv->mnemonic_keyval);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_adjust_bar_get_preferred_width(GtkWidget *widget,
                                   gint *minimum,
                                   gint *natural)
{
    *minimum = *natural = 200;
}

static void
gwy_adjust_bar_get_preferred_height(GtkWidget *widget,
                                    gint *minimum,
                                    gint *natural)
{
    GwyAdjustBar *adjbar = GWY_ADJUST_BAR(widget);
    AdjustBar *priv = adjbar->priv;
    PangoContext *pangocontext = gtk_widget_get_pango_context(widget);
    GtkStyleContext *context = gtk_widget_get_style_context(widget);
    GtkStateFlags state = gtk_widget_get_state_flags(widget);
    PangoFontMetrics *metrics
        = pango_context_get_metrics(pangocontext,
                                    gtk_style_context_get_font(context, state),
                                    pango_context_get_language(pangocontext));
    priv->ascent = pango_font_metrics_get_ascent(metrics);
    priv->descent = pango_font_metrics_get_descent(metrics);
    pango_font_metrics_unref(metrics);
    GtkBorder borders;
    gtk_style_context_get_padding(context, 0, &borders);
    ensure_layout(adjbar);
    guint height = (priv->ascent + priv->descent)/pangoscale;
    *minimum = height;
    *natural = height;
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
    GWY_OBJECT_UNREF(priv->layout);
    GTK_WIDGET_CLASS(gwy_adjust_bar_parent_class)->unrealize(widget);
}

static void
gwy_adjust_bar_map(GtkWidget *widget)
{
    GwyAdjustBar *adjust_bar = GWY_ADJUST_BAR(widget);
    AdjustBar *priv = adjust_bar->priv;
    GTK_WIDGET_CLASS(gwy_adjust_bar_parent_class)->map(widget);
    if (priv->input_window)
        gdk_window_show(priv->input_window);
}

static void
gwy_adjust_bar_unmap(GtkWidget *widget)
{
    GwyAdjustBar *adjust_bar = GWY_ADJUST_BAR(widget);
    AdjustBar *priv = adjust_bar->priv;
    if (priv->input_window)
        gdk_window_hide(priv->input_window);
    GTK_WIDGET_CLASS(gwy_adjust_bar_parent_class)->unmap(widget);
}

static void
gwy_adjust_bar_size_allocate(GtkWidget *widget,
                             GtkAllocation *allocation)
{
    GwyAdjustBar *adjbar = GWY_ADJUST_BAR(widget);
    AdjustBar *priv = adjbar->priv;

    GTK_WIDGET_CLASS(gwy_adjust_bar_parent_class)->size_allocate(widget,
                                                                allocation);

    if (priv->input_window)
        gdk_window_move_resize(priv->input_window,
                               allocation->x, allocation->y,
                               allocation->width, allocation->height);

    ensure_layout(adjbar);
}

static void
gwy_adjust_bar_style_updated(GtkWidget *widget)
{
    AdjustBar *priv = GWY_ADJUST_BAR(widget)->priv;
    if (priv->layout)
        pango_layout_context_changed(priv->layout);

    GTK_WIDGET_CLASS(gwy_adjust_bar_parent_class)->style_updated(widget);
}

static gboolean
gwy_adjust_bar_draw(GtkWidget *widget,
                    cairo_t *cr)
{
    GwyAdjustBar *adjbar = GWY_ADJUST_BAR(widget);

    draw_bar(adjbar, cr);
    draw_label(adjbar, cr);

    g_printerr("IMPLEMENT ME!\n");

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

    newposition = CLAMP(newposition, 0.0, length);
    if (newposition != position) {
        gdouble newvalue = map_position_to_value(adjbar, length, newposition);
        gtk_adjustment_set_value(priv->adjustment, newvalue);
    }
    return TRUE;
}

static gboolean
gwy_adjust_bar_button_press(GtkWidget *widget,
                            GdkEventButton *event)
{
    if (event->button != 1)
        return FALSE;

    move_to_position(GWY_ADJUST_BAR(widget), event->x);
    return TRUE;
}

static gboolean
gwy_adjust_bar_button_release(GtkWidget *widget,
                              GdkEventButton *event)
{
    if (event->button != 1)
        return FALSE;

    move_to_position(GWY_ADJUST_BAR(widget), event->x);
    return TRUE;
}

static gboolean
gwy_adjust_bar_motion_notify(GtkWidget *widget,
                             GdkEventMotion *event)
{
    if (!(event->state & GDK_BUTTON1_MASK))
        return FALSE;

    move_to_position(GWY_ADJUST_BAR(widget), event->x);
    return TRUE;
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
 * gwy_adjust_bar_set_label:
 * @adjbar: A adjustment bar.
 * @text: (allow-none):
 *        Label text.
 *
 * Sets the label text of a adjustment bar.
 *
 * The interpretation of @text with respect to Pango markup and mnemonics is
 * unchanged.  See gwy_adjust_bar_set_label_full() for a method to set the label
 * together with markup and underline properties.
 **/
void
gwy_adjust_bar_set_label(GwyAdjustBar *adjbar,
                         const gchar *text)
{
    g_return_if_fail(GWY_IS_ADJUST_BAR(adjbar));
    if (!set_label(adjbar, text))
        return;

    g_object_notify_by_pspec(G_OBJECT(adjbar), properties[PROP_LABEL]);
    // TODO: What about mnemonic keyval?
}

/**
 * gwy_adjust_bar_set_label_full:
 * @adjbar: A adjustment bar.
 * @text: (allow-none):
 *        Label text.
 * @use_markup: %TRUE to interpret Pango markup in @text, %FALSE to take it
 *              literally.
 * @use_underline: %TRUE to interpret underlines in @text, %FALSE to take them
 *                 literally.
 *
 * Sets the label text and its interpretation in a adjustment bar.
 *
 * It is possible, though somewhat eccentric, to only change the markup and
 * underline properties (thus reintepreting the text) by passing the same @text
 * that is already set but different values of @use_markup and @use_underline.
 **/
void
gwy_adjust_bar_set_label_full(GwyAdjustBar *adjbar,
                             const gchar *text,
                             gboolean use_markup,
                             gboolean use_underline)
{
    g_return_if_fail(GWY_IS_ADJUST_BAR(adjbar));
    GParamSpec *notify[3];
    guint nn = 0;
    if (set_use_markup(adjbar, use_markup))
        notify[nn++] = properties[PROP_USE_MARKUP];
    if (set_use_underline(adjbar, use_underline))
        notify[nn++] = properties[PROP_USE_UNDERLINE];
    if (set_label(adjbar, text))
        notify[nn++] = properties[PROP_LABEL];

    if (!nn)
        return;

    // TODO: What about mnemonic keyval?
    for (guint i = 0; i < nn; i++)
        g_object_notify_by_pspec(G_OBJECT(adjbar), notify[i]);
}

/**
 * gwy_adjust_bar_get_label:
 * @adjbar: A adjustment bar.
 *
 * Gets the label of a adjustment bar.
 *
 * Returns: (allow-none):
 *          The label of a adjustment bar, as it was set, including any Pango markup
 *          and mnemonic underlines.
 **/
const gchar*
gwy_adjust_bar_get_label(const GwyAdjustBar *adjbar)
{
    g_return_val_if_fail(GWY_IS_ADJUST_BAR(adjbar), NULL);
    return adjbar->priv->label;
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

/**
 * gwy_adjust_bar_set_mnemonic_widget:
 * @adjbar: A adjustment bar.
 * @widget: (allow-none):
 *          Target mnemonic widget.
 *
 * Sets the target mnemonic widget of a adjustment bar.
 *
 * If the adjustment bar has been set so that it has an mnemonic key the adjustment bar
 * can be associated with a widget that is the target of the mnemonic.
 * Typically, you may want to associate a adjustment bar with a #GwySpinButton.
 *
 * The target widget will be accelerated by emitting the
 * GtkWidget::mnemonic-activate signal on it. The default handler for this
 * signal will activate the widget if there are no mnemonic collisions and
 * toggle focus between the colliding widgets otherwise.
 **/
void
gwy_adjust_bar_set_mnemonic_widget(GwyAdjustBar *adjbar,
                                  GtkWidget *widget)
{
    g_return_if_fail(GWY_IS_ADJUST_BAR(adjbar));
    if (!set_mnemonic_widget(adjbar, widget))
        return;

    g_object_notify_by_pspec(G_OBJECT(adjbar),
                             properties[PROP_MNEMONIC_WIDGET]);
}

/**
 * gwy_adjust_bar_get_mnemonic_widget:
 * @adjbar: A adjustment bar.
 *
 * Gets the mnemonic widget associated with a adjustment bar.
 *
 * Returns: The target of @adjbar's mnemonic.
 **/
GtkWidget*
gwy_adjust_bar_get_mnemonic_widget(const GwyAdjustBar *adjbar)
{
    g_return_val_if_fail(GWY_IS_ADJUST_BAR(adjbar), NULL);
    return adjbar->priv->mnemonic_widget;
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

    // TODO: Cancel editting.
    update_mapping(adjbar);
    gtk_widget_queue_draw(GTK_WIDGET(adjbar));
    return TRUE;
}

static gboolean
set_use_markup(GwyAdjustBar *adjbar,
               gboolean use_markup)
{
    AdjustBar *priv = adjbar->priv;
    if (!use_markup == !priv->use_markup)
        return FALSE;

    priv->use_markup = use_markup;
    ensure_layout(adjbar);
    // TODO: invalidate label, emit size request
    return TRUE;
}

static gboolean
set_use_underline(GwyAdjustBar *adjbar,
                  gboolean use_underline)
{
    AdjustBar *priv = adjbar->priv;
    if (!use_underline == !priv->use_underline)
        return FALSE;

    priv->use_underline = use_underline;
    ensure_layout(adjbar);
    // TODO: invalidate label, emit size request
    return TRUE;
}

static gboolean
set_label(GwyAdjustBar *adjbar,
          const gchar *label)
{
    AdjustBar *priv = adjbar->priv;
    if (!gwy_assign_string(&priv->label, label))
        return FALSE;

    // TODO: invalidate label, emit size request
    ensure_layout(adjbar);
    return TRUE;
}

static gboolean
set_mnemonic_widget(GwyAdjustBar *adjbar,
                    GtkWidget *widget)
{
    AdjustBar *priv = adjbar->priv;
    if (!gwy_set_member_object(adjbar, widget, GTK_TYPE_WIDGET,
                               &priv->mnemonic_widget, NULL))
        return FALSE;

    // TODO: ???
    if (priv->use_underline)
        ensure_layout(adjbar);
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
move_to_position(GwyAdjustBar *adjbar,
                 gdouble x)
{
    AdjustBar *priv = adjbar->priv;
    if (!priv->adjustment_ok)
        return;

    GtkAllocation allocation;
    gtk_widget_get_allocation(GTK_WIDGET(adjbar), &allocation);
    gdouble value = gtk_adjustment_get_value(priv->adjustment);
    gdouble pos = CLAMP(x - allocation.x, 0, allocation.width);
    gdouble newvalue = map_position_to_value(adjbar, allocation.width, pos);
    if (newvalue != value)
        gtk_adjustment_set_value(priv->adjustment, newvalue);
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

static void
ensure_layout(GwyAdjustBar *adjbar)
{
    AdjustBar *priv = adjbar->priv;
    if (!priv->layout) {
        priv->layout = gtk_widget_create_pango_layout(GTK_WIDGET(adjbar),
                                                      NULL);
        pango_layout_set_alignment(priv->layout, PANGO_ALIGN_LEFT);
    }

    gchar *escaped = NULL;
    if (priv->label && !priv->use_markup)
        escaped = g_markup_escape_text(priv->label, -1);

    gchar *text = escaped ? escaped : priv->label;
    if (text && priv->use_underline) {
        // FIXME: Accelerators may be disabled globally, they can appear and
        // disappear dynamically and also we should not show them if we do not
        // have any mnemonic widget.  See GtkLabel for some of the convoluted
        // logic...
        gunichar accel_char = 0;
        pango_layout_set_markup_with_accel(priv->layout, text, -1,
                                           '_', &accel_char);
        if (accel_char) {
            guint keyval = gdk_unicode_to_keyval(accel_char);
            priv->mnemonic_keyval = gdk_keyval_to_lower(keyval);
        }
        else
            priv->mnemonic_keyval = GDK_KEY_VoidSymbol;
    }
    else {
        pango_layout_set_markup(priv->layout, text ? text : "", -1);
    }

    // FIXME: Needs to emit signal if accel key changes.

    GWY_FREE(escaped);
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

static gint
calc_layout_yposition(GwyAdjustBar *adjbar)
{
    AdjustBar *priv = adjbar->priv;
    GtkAllocation allocation;
    gtk_widget_get_allocation(GTK_WIDGET(adjbar), &allocation);
    // TODO: handle borders.
    gint area_height = PANGO_SCALE*allocation.height;
    PangoLayoutLine *line = pango_layout_get_lines_readonly(priv->layout)->data;
    PangoRectangle logical_rect;
    pango_layout_line_get_extents(line, NULL, &logical_rect);

    // Align primarily for locale's ascent/descent.
    gint y_pos = ((area_height - priv->ascent - priv->descent)/2
                  + priv->ascent + logical_rect.y);

    // Now see if we need to adjust to fit in actual drawn string.
    if (logical_rect.height > area_height)
        y_pos = (area_height - logical_rect.height)/2;
    else if (y_pos < 0)
        y_pos = 0;
    else if (y_pos + logical_rect.height > area_height)
        y_pos = area_height - logical_rect.height;

    return y_pos/PANGO_SCALE;
}

static void
draw_label(GwyAdjustBar *adjbar,
           cairo_t *cr)
{
    AdjustBar *priv = adjbar->priv;
    if (!priv->label || !*priv->label)
        return;

    GtkWidget *widget = GTK_WIDGET(adjbar);
    GtkStateFlags state = gtk_widget_get_state_flags(widget);
    GtkStyleContext *context = gtk_widget_get_style_context(widget);
    GdkRGBA text_color;
    gtk_style_context_get_color(context, state, &text_color);

    cairo_save(cr);
    gint y = calc_layout_yposition(adjbar);
    cairo_move_to(cr, 2.0, y);
    gdk_cairo_set_source_rgba(cr, &text_color);
    pango_cairo_show_layout(cr, priv->layout);
    cairo_restore(cr);
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
