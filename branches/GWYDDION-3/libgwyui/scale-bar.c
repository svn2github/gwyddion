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

struct _GwyScaleBarPrivate {
    GdkWindow *window;

    GtkAdjustment *adjustment;
    gulong adjustment_value_changed_id;
    gulong adjustment_changed_id;

    GwyScaleMappingType mapping;

    GtkWidget *mnemonic_widget;
    guint mnemonic_keyval;
    gchar *label;
    gboolean use_markup;
    gboolean use_underline;
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
static gboolean gwy_scale_bar_draw                (GtkWidget *widget,
                                                   cairo_t *cr);
static gboolean gwy_scale_bar_scroll              (GtkWidget *widget,
                                                   GdkEventScroll *event);
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
static void create_window(GwyScaleBar *scalebar);
static void destroy_window(GwyScaleBar *scalebar);
static void adjustment_changed(GwyScaleBar *scalebar,
                   GtkAdjustment *adjustment);
static void adjustment_value_changed(GwyScaleBar *scalebar,
                   GtkAdjustment *adjustment);

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
    widget_class->draw = gwy_scale_bar_draw;
    widget_class->scroll_event = gwy_scale_bar_scroll;

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
                            G_MAXUINT, 0, G_MAXUINT,
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
}

static void
gwy_scale_bar_finalize(GObject *object)
{
    G_OBJECT_CLASS(gwy_scale_bar_parent_class)->finalize(object);
}

static void
gwy_scale_bar_dispose(GObject *object)
{
    GwyScaleBar *scalebar = GWY_SCALE_BAR(object);

    set_adjustment(scalebar, NULL);

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
    *minimum = *natural = 20;
}

static void
gwy_scale_bar_get_preferred_height(GtkWidget *widget,
                                   gint *minimum,
                                   gint *natural)
{
    *minimum = *natural = 20;
}

static void
gwy_scale_bar_realize(GtkWidget *widget)
{
    GwyScaleBar *scalebar = GWY_SCALE_BAR(widget);
    gtk_widget_set_realized(widget, TRUE);
    create_window(scalebar);
}

static void
gwy_scale_bar_unrealize(GtkWidget *widget)
{
    GwyScaleBar *scalebar = GWY_SCALE_BAR(widget);
    destroy_window(scalebar);
    GTK_WIDGET_CLASS(gwy_scale_bar_parent_class)->unrealize(widget);
}

static gboolean
gwy_scale_bar_draw(GtkWidget *widget,
                   cairo_t *cr)
{
    GwyScaleBar *scalebar = GWY_SCALE_BAR(widget);
    ScaleBar *priv = scalebar->priv;
    GtkStyleContext *context = gtk_widget_get_style_context(widget);
    gdouble width = gtk_widget_get_allocated_width(widget),
            height = gtk_widget_get_allocated_height(widget);

    g_printerr("IMPLEMENT ME!\n");

    return FALSE;
}

static gboolean
gwy_scale_bar_scroll(GtkWidget *widget,
                     GdkEventScroll *event)
{
    return FALSE;
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
                               "value-changed", &adjustment_value_changed,
                               &priv->adjustment_value_changed_id,
                               G_CONNECT_SWAPPED,
                               NULL))
        return FALSE;

    gtk_widget_queue_draw(GTK_WIDGET(scalebar));
    return TRUE;
}

static gboolean
set_mapping(GwyScaleBar *scalebar,
            GwyScaleMappingType mapping)
{

    return TRUE;
}

static gboolean
set_use_markup(GwyScaleBar *scalebar,
               gboolean use_markup)
{

    return TRUE;
}

static gboolean
set_use_underline(GwyScaleBar *scalebar,
                  gboolean use_underline)
{

    return TRUE;
}

static gboolean
set_label(GwyScaleBar *scalebar,
          const gchar *label)
{

    return TRUE;
}

static gboolean
set_mnemonic_widget(GwyScaleBar *scalebar,
                    GtkWidget *widget)
{

    return TRUE;
}

static void
create_window(GwyScaleBar *scalebar)
{
    ScaleBar *priv = scalebar->priv;
    GtkWidget *widget = GTK_WIDGET(scalebar);

    g_assert(gtk_widget_get_realized(widget));

    if (priv->window)
        return;

    GtkAllocation allocation;
    gtk_widget_get_allocation(widget, &allocation);

    GdkWindowAttr attributes = {
        .x = allocation.x,
        .y = allocation.y,
        .width = allocation.width,
        .height = allocation.height,
        .window_type = GDK_WINDOW_CHILD,
        .wclass = GDK_INPUT_OUTPUT,
        .event_mask = (gtk_widget_get_events(widget)
                       | GDK_EXPOSURE_MASK
                       | GDK_BUTTON_PRESS_MASK
                       | GDK_BUTTON_RELEASE_MASK
                       | GDK_SCROLL_MASK
                       | GDK_POINTER_MOTION_MASK
                       | GDK_POINTER_MOTION_HINT_MASK),
        .visual = gtk_widget_get_visual(widget),
    };
    gint attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL;
    priv->window = gdk_window_new(gtk_widget_get_parent_window(widget),
                                  &attributes, attributes_mask);
    gtk_widget_set_window(widget, priv->window);
    gdk_window_set_user_data(priv->window, widget);
    g_object_ref(priv->window);
}

static void
destroy_window(GwyScaleBar *scalebar)
{
    ScaleBar *priv = scalebar->priv;

    if (!priv->window)
        return;

    gdk_window_set_user_data(priv->window, NULL);
    gdk_window_destroy(priv->window);
    priv->window = NULL;
}

static void
adjustment_changed(GwyScaleBar *scalebar,
                   GtkAdjustment *adjustment)
{
}

static void
adjustment_value_changed(GwyScaleBar *scalebar,
                         GtkAdjustment *adjustment)
{
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

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
