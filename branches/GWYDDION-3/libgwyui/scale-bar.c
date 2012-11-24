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
#include "libgwyui/scale-bar.h"

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

struct _GwyScaleBarPrivate {
    GdkWindow *input_window;

    GtkAdjustment *adjustment;
    gulong adjustment_value_changed_id;
    gulong adjustment_changed_id;
    gboolean adjustment_ok;

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

typedef struct _GwyScaleBarPrivate ScaleBar;

static void     gwy_scale_bar_dispose             (GObject *object);
static void     gwy_scale_bar_finalize            (GObject *object);
static void     gwy_scale_bar_set_property        (GObject *object,
                                                   guint prop_id,
                                                   const GValue *value,
                                                   GParamSpec *pspec);
static void     gwy_scale_bar_get_property        (GObject *object,
                                                   guint prop_id,
                                                   GValue *value,
                                                   GParamSpec *pspec);
static void     gwy_scale_bar_get_preferred_width (GtkWidget *widget,
                                                   gint *minimum,
                                                   gint *natural);
static void     gwy_scale_bar_get_preferred_height(GtkWidget *widget,
                                                   gint *minimum,
                                                   gint *natural);
static void     gwy_scale_bar_realize             (GtkWidget *widget);
static void     gwy_scale_bar_unrealize           (GtkWidget *widget);
static void     gwy_scale_bar_map                 (GtkWidget *widget);
static void     gwy_scale_bar_unmap               (GtkWidget *widget);
static void     gwy_scale_bar_size_allocate       (GtkWidget *widget,
                                                   GtkAllocation *allocation);
static void     gwy_scale_bar_style_updated       (GtkWidget *widget);
static gboolean gwy_scale_bar_draw                (GtkWidget *widget,
                                                   cairo_t *cr);
static gboolean gwy_scale_bar_scroll              (GtkWidget *widget,
                                                   GdkEventScroll *event);
static gboolean gwy_scale_bar_button_press        (GtkWidget *widget,
                                                   GdkEventButton *event);
static gboolean gwy_scale_bar_button_release      (GtkWidget *widget,
                                                   GdkEventButton *event);
static gboolean gwy_scale_bar_motion_notify       (GtkWidget *widget,
                                                   GdkEventMotion *event);
static gboolean set_adjustment                    (GwyScaleBar *scalebar,
                                                   GtkAdjustment *adjustment);
static gboolean set_mapping                       (GwyScaleBar *scalebar,
                                                   GwyScaleMappingType mapping);
static gboolean set_use_markup                    (GwyScaleBar *scalebar,
                                                   gboolean use_markup);
static gboolean set_use_underline                 (GwyScaleBar *scalebar,
                                                   gboolean use_underline);
static gboolean set_label                         (GwyScaleBar *scalebar,
                                                   const gchar *label);
static gboolean set_mnemonic_widget               (GwyScaleBar *scalebar,
                                                   GtkWidget *widget);
static void     create_input_window               (GwyScaleBar *scalebar);
static void     destroy_input_window              (GwyScaleBar *scalebar);
static void     adjustment_changed                (GwyScaleBar *scalebar,
                                                   GtkAdjustment *adjustment);
static void     adjustment_value_changed          (GwyScaleBar *scalebar,
                                                   GtkAdjustment *adjustment);
static void     move_to_position                  (GwyScaleBar *scalebar,
                                                   gdouble x);
static void     update_mapping                    (GwyScaleBar *scalebar);
static void     ensure_layout                     (GwyScaleBar *scalebar);
static void     draw_bar                          (GwyScaleBar *scalebar,
                                                   cairo_t *cr);
static gdouble  map_value_to_position             (GwyScaleBar *scalebar,
                                                   gdouble length,
                                                   gdouble value);
static gdouble  map_position_to_value             (GwyScaleBar *scalebar,
                                                   gdouble length,
                                                   gdouble position);
static gdouble  map_both_linear                   (gdouble value);
static gdouble  map_value_sqrt                    (gdouble value);
static gdouble  map_position_sqrt                 (gdouble position);
static gdouble  map_value_log                     (gdouble value);
static gdouble  map_position_log                  (gdouble position);

static GParamSpec *properties[N_PROPS];
static guint signals[N_SIGNALS];

G_DEFINE_TYPE(GwyScaleBar, gwy_scale_bar, GTK_TYPE_WIDGET);

static void
gwy_scale_bar_class_init(GwyScaleBarClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

    g_type_class_add_private(klass, sizeof(ScaleBar));

    gobject_class->dispose = gwy_scale_bar_dispose;
    gobject_class->finalize = gwy_scale_bar_finalize;
    gobject_class->get_property = gwy_scale_bar_get_property;
    gobject_class->set_property = gwy_scale_bar_set_property;

    widget_class->get_preferred_width = gwy_scale_bar_get_preferred_width;
    widget_class->get_preferred_height = gwy_scale_bar_get_preferred_height;
    widget_class->realize = gwy_scale_bar_realize;
    widget_class->unrealize = gwy_scale_bar_unrealize;
    widget_class->map = gwy_scale_bar_map;
    widget_class->unmap = gwy_scale_bar_unmap;
    widget_class->size_allocate = gwy_scale_bar_size_allocate;
    widget_class->style_updated = gwy_scale_bar_style_updated;
    widget_class->draw = gwy_scale_bar_draw;
    widget_class->scroll_event = gwy_scale_bar_scroll;
    widget_class->button_press_event = gwy_scale_bar_button_press;
    widget_class->button_release_event = gwy_scale_bar_button_release;
    widget_class->motion_notify_event = gwy_scale_bar_motion_notify;

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
     * GwyScaleBar::change-value:
     * @gwyscalebar: The #GwyScaleBar which received the signal.
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
gwy_scale_bar_init(GwyScaleBar *scalebar)
{
    scalebar->priv = G_TYPE_INSTANCE_GET_PRIVATE(scalebar,
                                                  GWY_TYPE_SCALE_BAR,
                                                  ScaleBar);
    ScaleBar *priv = scalebar->priv;
    priv->mapping = GWY_SCALE_MAPPING_SQRT;
    gtk_widget_set_has_window(GTK_WIDGET(scalebar), FALSE);
}

static void
gwy_scale_bar_finalize(GObject *object)
{
    ScaleBar *priv = GWY_SCALE_BAR(object)->priv;
    GWY_FREE(priv->label);
    G_OBJECT_CLASS(gwy_scale_bar_parent_class)->finalize(object);
}

static void
gwy_scale_bar_dispose(GObject *object)
{
    GwyScaleBar *scalebar = GWY_SCALE_BAR(object);
    set_adjustment(scalebar, NULL);
    set_mnemonic_widget(scalebar, NULL);
    G_OBJECT_CLASS(gwy_scale_bar_parent_class)->dispose(object);
}

static void
gwy_scale_bar_set_property(GObject *object,
                            guint prop_id,
                            const GValue *value,
                            GParamSpec *pspec)
{
    GwyScaleBar *scalebar = GWY_SCALE_BAR(object);

    switch (prop_id) {
        case PROP_ADJUSTMENT:
        set_adjustment(scalebar, g_value_get_object(value));
        break;

        case PROP_MAPPING:
        set_mapping(scalebar, g_value_get_enum(value));
        break;

        case PROP_LABEL:
        set_label(scalebar, g_value_get_string(value));
        break;

        case PROP_USE_MARKUP:
        set_use_markup(scalebar, g_value_get_boolean(value));
        break;

        case PROP_USE_UNDERLINE:
        set_use_underline(scalebar, g_value_get_boolean(value));
        break;

        case PROP_MNEMONIC_WIDGET:
        set_mnemonic_widget(scalebar, g_value_get_object(value));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_scale_bar_get_property(GObject *object,
                            guint prop_id,
                            GValue *value,
                            GParamSpec *pspec)
{
    ScaleBar *priv = GWY_SCALE_BAR(object)->priv;

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
gwy_scale_bar_get_preferred_width(GtkWidget *widget,
                                  gint *minimum,
                                  gint *natural)
{
    *minimum = *natural = 200;
}

static void
gwy_scale_bar_get_preferred_height(GtkWidget *widget,
                                   gint *minimum,
                                   gint *natural)
{
    GwyScaleBar *scalebar = GWY_SCALE_BAR(widget);
    ScaleBar *priv = scalebar->priv;
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
    ensure_layout(scalebar);
    guint height = (priv->ascent + priv->descent)/pangoscale;
    *minimum = height;
    *natural = height;
}

static void
gwy_scale_bar_realize(GtkWidget *widget)
{
    GwyScaleBar *scalebar = GWY_SCALE_BAR(widget);
    GTK_WIDGET_CLASS(gwy_scale_bar_parent_class)->realize(widget);
    create_input_window(scalebar);
}

static void
gwy_scale_bar_unrealize(GtkWidget *widget)
{
    GwyScaleBar *scalebar = GWY_SCALE_BAR(widget);
    ScaleBar *priv = scalebar->priv;
    destroy_input_window(scalebar);
    priv->adjustment_ok = FALSE;
    GWY_OBJECT_UNREF(priv->layout);
    GTK_WIDGET_CLASS(gwy_scale_bar_parent_class)->unrealize(widget);
}

static void
gwy_scale_bar_map(GtkWidget *widget)
{
    GwyScaleBar *scale_bar = GWY_SCALE_BAR(widget);
    ScaleBar *priv = scale_bar->priv;
    GTK_WIDGET_CLASS(gwy_scale_bar_parent_class)->map(widget);
    if (priv->input_window)
        gdk_window_show(priv->input_window);
}

static void
gwy_scale_bar_unmap(GtkWidget *widget)
{
    GwyScaleBar *scale_bar = GWY_SCALE_BAR(widget);
    ScaleBar *priv = scale_bar->priv;
    if (priv->input_window)
        gdk_window_hide(priv->input_window);
    GTK_WIDGET_CLASS(gwy_scale_bar_parent_class)->unmap(widget);
}

static void
gwy_scale_bar_size_allocate(GtkWidget *widget,
                            GtkAllocation *allocation)
{
    GwyScaleBar *scalebar = GWY_SCALE_BAR(widget);
    ScaleBar *priv = scalebar->priv;

    GTK_WIDGET_CLASS(gwy_scale_bar_parent_class)->size_allocate(widget,
                                                                allocation);

    if (priv->input_window)
        gdk_window_move_resize(priv->input_window,
                               allocation->x, allocation->y,
                               allocation->width, allocation->height);

    ensure_layout(scalebar);
}

static void
gwy_scale_bar_style_updated(GtkWidget *widget)
{
    ScaleBar *priv = GWY_SCALE_BAR(widget)->priv;
    if (priv->layout)
        pango_layout_context_changed(priv->layout);

    GTK_WIDGET_CLASS(gwy_scale_bar_parent_class)->style_updated(widget);
}

static gboolean
gwy_scale_bar_draw(GtkWidget *widget,
                   cairo_t *cr)
{
    GwyScaleBar *scalebar = GWY_SCALE_BAR(widget);

    draw_bar(scalebar, cr);

    g_printerr("IMPLEMENT ME!\n");

    return FALSE;
}

static gboolean
gwy_scale_bar_scroll(GtkWidget *widget,
                     GdkEventScroll *event)
{
    GwyScaleBar *scalebar = GWY_SCALE_BAR(widget);
    ScaleBar *priv = scalebar->priv;
    if (!priv->adjustment_ok)
        return TRUE;

    GdkScrollDirection dir = event->direction;
    gdouble length = gtk_widget_get_allocated_width(widget);
    gdouble value = gtk_adjustment_get_value(priv->adjustment),
            position = map_value_to_position(scalebar, length, value),
            newposition = position;
    if (dir == GDK_SCROLL_UP || dir == GDK_SCROLL_RIGHT)
        newposition += 1.0;
    else
        newposition -= 1.0;

    newposition = CLAMP(newposition, 0.0, length);
    if (newposition != position) {
        gdouble newvalue = map_position_to_value(scalebar, length, newposition);
        gtk_adjustment_set_value(priv->adjustment, newvalue);
    }
    return TRUE;
}

static gboolean
gwy_scale_bar_button_press(GtkWidget *widget,
                           GdkEventButton *event)
{
    if (event->button != 1)
        return FALSE;

    move_to_position(GWY_SCALE_BAR(widget), event->x);
    return TRUE;
}

static gboolean
gwy_scale_bar_button_release(GtkWidget *widget,
                             GdkEventButton *event)
{
    if (event->button != 1)
        return FALSE;

    move_to_position(GWY_SCALE_BAR(widget), event->x);
    return TRUE;
}

static gboolean
gwy_scale_bar_motion_notify(GtkWidget *widget,
                            GdkEventMotion *event)
{
    if (!(event->state & GDK_BUTTON1_MASK))
        return FALSE;

    move_to_position(GWY_SCALE_BAR(widget), event->x);
    return TRUE;
}

/**
 * gwy_scale_bar_new:
 *
 * Creates a new scale bar.
 *
 * Returns: A new scale bar.
 **/
GtkWidget*
gwy_scale_bar_new(void)
{
    return g_object_newv(GWY_TYPE_SCALE_BAR, 0, NULL);
}

/**
 * gwy_scale_bar_set_adjustment:
 * @scalebar: A scale bar.
 * @adjustment: (allow-none):
 *              Adjustment to use for the value.
 *
 * Sets the adjustment that a scale bar visualises.
 **/
void
gwy_scale_bar_set_adjustment(GwyScaleBar *scalebar,
                             GtkAdjustment *adjustment)
{
    g_return_if_fail(GWY_IS_SCALE_BAR(scalebar));
    if (!set_adjustment(scalebar, adjustment))
        return;

    g_object_notify_by_pspec(G_OBJECT(scalebar), properties[PROP_ADJUSTMENT]);
}

/**
 * gwy_scale_bar_get_adjustment:
 * @scalebar: A scale bar.
 *
 * Obtains the adjustment that a scale bar visualises.
 *
 * Returns: (allow-none) (transfer none):
 *          The colour adjustment used by @scalebar.  If no adjustment was set and
 *          the default one is used, function returns %NULL.
 **/
GtkAdjustment*
gwy_scale_bar_get_adjustment(const GwyScaleBar *scalebar)
{
    g_return_val_if_fail(GWY_IS_SCALE_BAR(scalebar), NULL);
    return scalebar->priv->adjustment;
}

/**
 * gwy_scale_bar_set_label:
 * @scalebar: A scale bar.
 * @text: (allow-none):
 *        Label text.
 *
 * Sets the label text of a scale bar.
 *
 * The interpretation of @text with respect to Pango markup and mnemonics is
 * unchanged.  See gwy_scale_bar_set_label_full() for a method to set the label
 * together with markup and underline properties.
 **/
void
gwy_scale_bar_set_label(GwyScaleBar *scalebar,
                        const gchar *text)
{
    g_return_if_fail(GWY_IS_SCALE_BAR(scalebar));
    if (!set_label(scalebar, text))
        return;

    g_object_notify_by_pspec(G_OBJECT(scalebar), properties[PROP_LABEL]);
    // TODO: What about mnemonic keyval?
}

/**
 * gwy_scale_bar_set_label_full:
 * @scalebar: A scale bar.
 * @text: (allow-none):
 *        Label text.
 * @use_markup: %TRUE to interpret Pango markup in @text, %FALSE to take it
 *              literally.
 * @use_underline: %TRUE to interpret underlines in @text, %FALSE to take them
 *                 literally.
 *
 * Sets the label text and its interpretation in a scale bar.
 *
 * It is possible, though somewhat eccentric, to only change the markup and
 * underline properties (thus reintepreting the text) by passing the same @text
 * that is already set but different values of @use_markup and @use_underline.
 **/
void
gwy_scale_bar_set_label_full(GwyScaleBar *scalebar,
                             const gchar *text,
                             gboolean use_markup,
                             gboolean use_underline)
{
    g_return_if_fail(GWY_IS_SCALE_BAR(scalebar));
    GParamSpec *notify[3];
    guint nn = 0;
    if (set_use_markup(scalebar, use_markup))
        notify[nn++] = properties[PROP_USE_MARKUP];
    if (set_use_underline(scalebar, use_underline))
        notify[nn++] = properties[PROP_USE_UNDERLINE];
    if (set_label(scalebar, text))
        notify[nn++] = properties[PROP_LABEL];

    if (!nn)
        return;

    // TODO: What about mnemonic keyval?
    for (guint i = 0; i < nn; i++)
        g_object_notify_by_pspec(G_OBJECT(scalebar), notify[i]);
}

/**
 * gwy_scale_bar_get_label:
 * @scalebar: A scale bar.
 *
 * Gets the label of a scale bar.
 *
 * Returns: (allow-none):
 *          The label of a scale bar, as it was set, including any Pango markup
 *          and mnemonic underlines.
 **/
const gchar*
gwy_scale_bar_get_label(const GwyScaleBar *scalebar)
{
    g_return_val_if_fail(GWY_IS_SCALE_BAR(scalebar), NULL);
    return scalebar->priv->label;
}

/**
 * gwy_scale_bar_set_mapping:
 * @scalebar: A scale bar.
 * @mapping: Mapping function type between values and screen positions in the
 *           scale bar.
 *
 * Sets the mapping function type for a scale bar.
 **/
void
gwy_scale_bar_set_mapping(GwyScaleBar *scalebar,
                          GwyScaleMappingType mapping)
{
    g_return_if_fail(GWY_IS_SCALE_BAR(scalebar));
    if (!set_mapping(scalebar, mapping))
        return;

    g_object_notify_by_pspec(G_OBJECT(scalebar), properties[PROP_MAPPING]);
}

/**
 * gwy_scale_bar_get_mapping:
 * @scalebar: A scale bar.
 *
 * Gets the mapping function type of a scale bar.
 *
 * Returns: The type of mapping function between values and screen positions.
 **/
GwyScaleMappingType
gwy_scale_bar_get_mapping(const GwyScaleBar *scalebar)
{
    g_return_val_if_fail(GWY_IS_SCALE_BAR(scalebar), GWY_SCALE_MAPPING_LINEAR);
    return scalebar->priv->mapping;
}

/**
 * gwy_scale_bar_set_mnemonic_widget:
 * @scalebar: A scale bar.
 * @widget: (allow-none):
 *          Target mnemonic widget.
 *
 * Sets the target mnemonic widget of a scale bar.
 *
 * If the scale bar has been set so that it has an mnemonic key the scale bar
 * can be associated with a widget that is the target of the mnemonic.
 * Typically, you may want to associate a scale bar with a #GwySpinButton.
 *
 * The target widget will be accelerated by emitting the
 * GtkWidget::mnemonic-activate signal on it. The default handler for this
 * signal will activate the widget if there are no mnemonic collisions and
 * toggle focus between the colliding widgets otherwise.
 **/
void
gwy_scale_bar_set_mnemonic_widget(GwyScaleBar *scalebar,
                                  GtkWidget *widget)
{
    g_return_if_fail(GWY_IS_SCALE_BAR(scalebar));
    if (!set_mnemonic_widget(scalebar, widget))
        return;

    g_object_notify_by_pspec(G_OBJECT(scalebar),
                             properties[PROP_MNEMONIC_WIDGET]);
}

/**
 * gwy_scale_bar_get_mnemonic_widget:
 * @scalebar: A scale bar.
 *
 * Gets the mnemonic widget associated with a scale bar.
 *
 * Returns: The target of @scalebar's mnemonic.
 **/
GtkWidget*
gwy_scale_bar_get_mnemonic_widget(const GwyScaleBar *scalebar)
{
    g_return_val_if_fail(GWY_IS_SCALE_BAR(scalebar), NULL);
    return scalebar->priv->mnemonic_widget;
}

static gboolean
set_adjustment(GwyScaleBar *scalebar,
               GtkAdjustment *adjustment)
{
    ScaleBar *priv = scalebar->priv;
    if (!gwy_set_member_object(scalebar, adjustment, GTK_TYPE_ADJUSTMENT,
                               &priv->adjustment,
                               "changed", &adjustment_changed,
                               &priv->adjustment_changed_id,
                               G_CONNECT_SWAPPED,
                               "value-changed", &adjustment_value_changed,
                               &priv->adjustment_value_changed_id,
                               G_CONNECT_SWAPPED,
                               NULL))
        return FALSE;

    update_mapping(scalebar);
    gtk_widget_queue_draw(GTK_WIDGET(scalebar));
    return TRUE;
}

static gboolean
set_mapping(GwyScaleBar *scalebar,
            GwyScaleMappingType mapping)
{
    ScaleBar *priv = scalebar->priv;
    if (mapping == priv->mapping)
        return FALSE;

    if (mapping > GWY_SCALE_MAPPING_LOG) {
        g_warning("Wrong scale mapping %u.", mapping);
        return FALSE;
    }

    // TODO: Cancel editting.
    update_mapping(scalebar);
    gtk_widget_queue_draw(GTK_WIDGET(scalebar));
    return TRUE;
}

static gboolean
set_use_markup(GwyScaleBar *scalebar,
               gboolean use_markup)
{
    ScaleBar *priv = scalebar->priv;
    if (!use_markup == !priv->use_markup)
        return FALSE;

    priv->use_markup = use_markup;
    ensure_layout(scalebar);
    // TODO: invalidate label, emit size request
    return TRUE;
}

static gboolean
set_use_underline(GwyScaleBar *scalebar,
                  gboolean use_underline)
{
    ScaleBar *priv = scalebar->priv;
    if (!use_underline == !priv->use_underline)
        return FALSE;

    priv->use_underline = use_underline;
    ensure_layout(scalebar);
    // TODO: invalidate label, emit size request
    return TRUE;
}

static gboolean
set_label(GwyScaleBar *scalebar,
          const gchar *label)
{
    ScaleBar *priv = scalebar->priv;
    if (!gwy_assign_string(&priv->label, label))
        return FALSE;

    // TODO: invalidate label, emit size request
    ensure_layout(scalebar);
    return TRUE;
}

static gboolean
set_mnemonic_widget(GwyScaleBar *scalebar,
                    GtkWidget *widget)
{
    ScaleBar *priv = scalebar->priv;
    if (!gwy_set_member_object(scalebar, widget, GTK_TYPE_WIDGET,
                               &priv->mnemonic_widget, NULL))
        return FALSE;

    // TODO: ???
    if (priv->use_underline)
        ensure_layout(scalebar);
    return TRUE;
}

static void
create_input_window(GwyScaleBar *scalebar)
{
    ScaleBar *priv = scalebar->priv;
    GtkWidget *widget = GTK_WIDGET(scalebar);
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
destroy_input_window(GwyScaleBar *scalebar)
{
    ScaleBar *priv = scalebar->priv;
    if (!priv->input_window)
        return;
    gdk_window_set_user_data(priv->input_window, NULL);
    gdk_window_destroy(priv->input_window);
    priv->input_window = NULL;
}

static void
adjustment_changed(GwyScaleBar *scalebar,
                   G_GNUC_UNUSED GtkAdjustment *adjustment)
{
    // TODO: Cancel editting.
    update_mapping(scalebar);
    gtk_widget_queue_draw(GTK_WIDGET(scalebar));
}

static void
adjustment_value_changed(GwyScaleBar *scalebar,
                         G_GNUC_UNUSED GtkAdjustment *adjustment)
{
    gtk_widget_queue_draw(GTK_WIDGET(scalebar));
}

static void
move_to_position(GwyScaleBar *scalebar,
                 gdouble x)
{
    ScaleBar *priv = scalebar->priv;
    if (!priv->adjustment_ok)
        return;

    GtkAllocation alloc;
    gtk_widget_get_allocation(GTK_WIDGET(scalebar), &alloc);
    gdouble value = gtk_adjustment_get_value(priv->adjustment);
    gdouble pos = CLAMP(x - alloc.x, 0, alloc.width);
    gdouble newvalue = map_position_to_value(scalebar, alloc.width, pos);
    if (newvalue != value)
        gtk_adjustment_set_value(priv->adjustment, newvalue);
}

static void
update_mapping(GwyScaleBar *scalebar)
{
    ScaleBar *priv = scalebar->priv;
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

    gdouble length = gtk_widget_get_allocated_width(GTK_WIDGET(scalebar));
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
ensure_layout(GwyScaleBar *scalebar)
{
    ScaleBar *priv = scalebar->priv;
    if (!priv->layout) {
        priv->layout = gtk_widget_create_pango_layout(GTK_WIDGET(scalebar),
                                                      NULL);
        pango_layout_set_alignment(priv->layout, PANGO_ALIGN_LEFT);
    }

    gchar *escaped = NULL;
    if (!priv->use_markup)
        escaped = g_markup_escape_text(priv->label, -1);

    gchar *text = escaped ? escaped : priv->label;
    if (priv->use_underline) {
        // FIXME: Accelerators may be disabled globally, they can appear and
        // disappear dynamically and also we should not show them if we do not
        // have any mnemonic widget.  See GtkLabel for some of the convoluted
        // logic...
        gunichar accel_char = 0;
        pango_layout_set_markup_with_accel(priv->layout, text, -1,
                                           '_', &accel_char);
        // FIXME: Needs to emit signal if changes.
        if (accel_char) {
            guint keyval = gdk_unicode_to_keyval(accel_char);
            priv->mnemonic_keyval = gdk_keyval_to_lower(keyval);
        }
        else
            priv->mnemonic_keyval = GDK_KEY_VoidSymbol;
    }
    else {
        pango_layout_set_markup(priv->layout, text, -1);
    }

    GWY_FREE(escaped);
}

static void
draw_bar(GwyScaleBar *scalebar,
         cairo_t *cr)
{
    ScaleBar *priv = scalebar->priv;
    if (!priv->adjustment_ok)
        return;

    GtkWidget *widget = GTK_WIDGET(scalebar);
    gdouble width = gtk_widget_get_allocated_width(widget),
            height = gtk_widget_get_allocated_height(widget);
    gdouble val = gtk_adjustment_get_value(priv->adjustment);
    gdouble barlength = map_value_to_position(scalebar, width, val);

    cairo_save(cr);

    if (barlength > 2.0) {
        cairo_rectangle(cr, 0, 0, barlength, height);
        cairo_set_source_rgba(cr, 0.6, 0.6, 1.0, 0.4);
        cairo_fill(cr);

        cairo_set_line_width(cr, 1.0);
        cairo_rectangle(cr, 0.5, 0.5, barlength-1.0, height-1.0);
        cairo_set_source_rgb(cr, 0.6, 0.6, 1.0);
        cairo_stroke(cr);
    }
    else {
        // Do not stroke bars thinner than twice the ourline, draw the entire
        // bar using the border color instead.
        cairo_rectangle(cr, 0, 0, barlength, height);
        cairo_set_source_rgb(cr, 0.6, 0.6, 1.0);
        cairo_fill(cr);
    }

    cairo_restore(cr);
}

static gdouble
map_value_to_position(GwyScaleBar *scalebar,
                      gdouble length,
                      gdouble value)
{
    ScaleBar *priv = scalebar->priv;
    return (priv->map_value(value) - priv->b)/priv->a*length;
}

static gdouble
map_position_to_value(GwyScaleBar *scalebar,
                      gdouble length,
                      gdouble position)
{
    ScaleBar *priv = scalebar->priv;
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

/**
 * SECTION: scale-bar
 * @title: GwyScaleBar
 * @short_description: Compact adjustment visualisation
 **/

/**
 * GwyScaleBar:
 *
 * Scale bar widget visualising an adjustment.
 *
 * The #GwyScaleBar struct contains private data only and should be accessed
 * using the functions below.
 **/

/**
 * GwyScaleBarClass:
 *
 * Class of scale bars visualising adjustments.
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
 * Type of scale bar mapping functions.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
