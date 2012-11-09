/*
 *  $Id$
 *  Copyright (C) 2011-2012 David Nečas (Yeti).
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

#include <gdk/gdkkeysyms.h>
#include "libgwy/macros.h"
#include "libgwy/math.h"
#include "libgwy/strfuncs.h"
#include "libgwy/object-utils.h"
#include "libgwy/field-statistics.h"
#include "libgwy/mask-field-grains.h"
#include "libgwyui/types.h"
#include "libgwyui/marshal.h"
#include "libgwyui/cairo-utils.h"
#include "libgwyui/raster-area.h"

enum {
    // Own.
    PROP_0,
    PROP_SCROLLABLE,
    PROP_ZOOMABLE,
    PROP_RANGE_FROM_METHOD,
    PROP_RANGE_TO_METHOD,
    PROP_RANGE,
    PROP_USER_RANGE,
    PROP_FIELD,
    PROP_MASK,
    PROP_GRADIENT,
    PROP_SHAPES,
    PROP_MASK_COLOR,
    PROP_ZOOM,
    PROP_REAL_ASPECT_RATIO,
    PROP_NUMBER_GRAINS,
    PROP_GRAIN_NUMBER_COLOR,
    N_PROPS,
    // Overriden.
    PROP_HADJUSTMENT = N_PROPS,
    PROP_VADJUSTMENT,
    PROP_HSCROLL_POLICY,
    PROP_VSCROLL_POLICY,
    N_TOTAL_PROPS
};

enum {
    SGNL_SCROLL,
    SGNL_ZOOM,
    N_SIGNALS
};

struct _GwyRasterAreaPrivate {
    GdkWindow *window;

    gboolean scrollable : 1;
    gboolean zoomable : 1;

    // Implementation of GtkScrollable
    GtkAdjustment *hadjustment;
    gulong hadjustment_value_changed_id;

    GtkAdjustment *vadjustment;
    gulong vadjustment_value_changed_id;

    guint hscroll_policy;
    guint vscroll_policy;

    // Coordinate ranges and transforms
    gdouble zoom;
    gboolean real_aspect_ratio;
    gboolean pos_and_size_valid;
    // Position of the field image within the widget.  Unless we zoom out too
    // much (so that borders appear) x and y is zero.
    cairo_rectangle_int_t image_rectangle;
    // Field part, in pixel coordinates, shown in the visible area.
    cairo_rectangle_t field_rectangle;

    cairo_matrix_t window_to_field_matrix;
    cairo_matrix_t window_to_coords_matrix;
    cairo_matrix_t field_to_window_matrix;
    cairo_matrix_t coords_to_window_matrix;

    // Field
    GwyField *field;
    gulong field_notify_id;
    gulong field_data_changed_id;
    gdouble field_aspect_ratio;
    cairo_surface_t *field_surface;
    gboolean field_surface_valid;

    // Mask
    GwyMaskField *mask;
    gulong mask_notify_id;
    gulong mask_data_changed_id;
    cairo_surface_t *mask_surface;
    gboolean mask_surface_valid;
    guint active_grain;

    // Visualisation
    GwyGradient *gradient;
    gulong gradient_data_changed_id;

    GwyColorRangeType range_from_method;
    GwyColorRangeType range_to_method;
    GwyRange user_range;
    GwyRange range;
    gboolean range_valid;

    GwyShapes *shapes;
    gulong shapes_updated_id;

    GwyRGBA mask_color;
    GwyRGBA grain_number_color;
    gboolean number_grains;

    PangoLayout *layout;
    GtkWidget *area_widget;

    gulong scroll_timer_hid;
};

typedef struct _GwyRasterAreaPrivate RasterArea;

static void     gwy_raster_area_finalize            (GObject *object);
static void     gwy_raster_area_dispose             (GObject *object);
static void     gwy_raster_area_set_property        (GObject *object,
                                                     guint prop_id,
                                                     const GValue *value,
                                                     GParamSpec *pspec);
static void     gwy_raster_area_get_property        (GObject *object,
                                                     guint prop_id,
                                                     GValue *value,
                                                     GParamSpec *pspec);
static void     gwy_raster_area_style_updated       (GtkWidget *widget);
static void     gwy_raster_area_realize             (GtkWidget *widget);
static void     gwy_raster_area_unrealize           (GtkWidget *widget);
static void     gwy_raster_area_map                 (GtkWidget *widget);
static void     gwy_raster_area_unmap               (GtkWidget *widget);
static void     gwy_raster_area_get_preferred_width (GtkWidget *widget,
                                                     gint *minimum,
                                                     gint *natural);
static void     gwy_raster_area_get_preferred_height(GtkWidget *widget,
                                                     gint *minimum,
                                                     gint *natural);
static void     gwy_raster_area_size_allocate       (GtkWidget *widget,
                                                     GtkAllocation *allocation);
static gboolean gwy_raster_area_motion_notify       (GtkWidget *widget,
                                                     GdkEventMotion *event);
static gboolean gwy_raster_area_button_press        (GtkWidget *widget,
                                                     GdkEventButton *event);
static gboolean gwy_raster_area_button_release      (GtkWidget *widget,
                                                     GdkEventButton *event);
static gboolean gwy_raster_area_key_press           (GtkWidget *widget,
                                                     GdkEventKey *event);
static gboolean gwy_raster_area_key_release         (GtkWidget *widget,
                                                     GdkEventKey *event);
static gboolean gwy_raster_area_leave_notify        (GtkWidget *widget,
                                                     GdkEventCrossing *event);
static gboolean gwy_raster_area_scroll              (GwyRasterArea *rasterarea,
                                                     GtkScrollType scrolltype);
static gboolean gwy_raster_area_zoom                (GwyRasterArea *rasterarea,
                                                     GwyZoomType zoomtype);
static void     calculate_position_and_size         (GwyRasterArea *rasterarea);
static void     update_matrices                     (GwyRasterArea *rasterarea);
static void     ensure_range                        (GwyRasterArea *rasterarea);
static void     ensure_layout                       (GwyRasterArea *rasterarea);
static gboolean gwy_raster_area_draw                (GtkWidget *widget,
                                                     cairo_t *cr);
static void     destroy_field_surface               (GwyRasterArea *rasterarea);
static void     destroy_mask_surface                (GwyRasterArea *rasterarea);
static gboolean set_field                           (GwyRasterArea *rasterarea,
                                                     GwyField *field);
static gboolean set_mask                            (GwyRasterArea *rasterarea,
                                                     GwyMaskField *mask);
static gboolean set_gradient                        (GwyRasterArea *rasterarea,
                                                     GwyGradient *gradient);
static gboolean set_shapes                          (GwyRasterArea *rasterarea,
                                                     GwyShapes *shapes);
static void     set_shapes_transforms               (GwyRasterArea *rasterarea);
static gboolean set_mask_color                      (GwyRasterArea *rasterarea,
                                                     const GwyRGBA *color);
static gboolean set_grain_number_color              (GwyRasterArea *rasterarea,
                                                     const GwyRGBA *color);
static gboolean set_scrollable                      (GwyRasterArea *rasterarea,
                                                     gboolean setting);
static gboolean set_zoomable                        (GwyRasterArea *rasterarea,
                                                     gboolean setting);
static gboolean set_range_from_method               (GwyRasterArea *rasterarea,
                                                     GwyColorRangeType method);
static gboolean set_range_to_method                 (GwyRasterArea *rasterarea,
                                                     GwyColorRangeType method);
static gboolean set_user_range                      (GwyRasterArea *rasterarea,
                                                     const GwyRange *range);
static void     field_notify                        (GwyRasterArea *rasterarea,
                                                     GParamSpec *pspec,
                                                     GwyField *field);
static void     mask_notify                         (GwyRasterArea *rasterarea,
                                                     GParamSpec *pspec,
                                                     GwyMaskField *mask);
static void     field_data_changed                  (GwyRasterArea *rasterarea,
                                                     GwyFieldPart *fpart,
                                                     GwyField *field);
static void     mask_data_changed                   (GwyRasterArea *rasterarea,
                                                     GwyFieldPart *fpart,
                                                     GwyMaskField *mask);
static void     queue_draw_field_part               (GwyRasterArea *rasterarea,
                                                     const GwyFieldPart *fpart);
static void     gradient_data_changed               (GwyRasterArea *rasterarea,
                                                     GwyGradient *gradient);
static void     shapes_updated                      (GwyRasterArea *rasterarea,
                                                     GwyShapes *shapes);
static gboolean enable_scrolling_again              (gpointer user_data);
static gboolean scroll_to_current_point             (GwyRasterArea *rasterarea,
                                                     GwyShapes *shapes);
static gboolean set_hadjustment                     (GwyRasterArea *rasterarea,
                                                     GtkAdjustment *adjustment);
static gboolean set_vadjustment                     (GwyRasterArea *rasterarea,
                                                     GtkAdjustment *adjustment);
static void     set_hadjustment_values              (GwyRasterArea *rasterarea);
static void     set_vadjustment_values              (GwyRasterArea *rasterarea);
static void     adjustment_value_changed            (GwyRasterArea *rasterarea);
static void     freeze_adjustments_notify           (GwyRasterArea *rasterarea,
                                                     gboolean horizontal,
                                                     gboolean vertical);
static void     thaw_adjustments_notify             (GwyRasterArea *rasterarea,
                                                     gboolean horizontal,
                                                     gboolean vertical);
static gboolean set_zoom                            (GwyRasterArea *rasterarea,
                                                     gdouble zoom);
static gboolean set_real_aspect_ratio               (GwyRasterArea *rasterarea,
                                                     gboolean setting);
static gboolean set_number_grains                   (GwyRasterArea *rasterarea,
                                                     gboolean setting);
static guint    calculate_full_width                (const GwyRasterArea *rasterarea);
static guint    calculate_full_height               (const GwyRasterArea *rasterarea);

static const GwyRGBA mask_color_default = { 1.0, 0.0, 0.0, 0.5 };
static const GwyRGBA grain_number_color_default = { 0.7, 0.0, 0.9, 1.0 };
static const GwyRange void_range = { 0.0, 0.0 };

static GParamSpec *properties[N_TOTAL_PROPS];
static guint signals[N_SIGNALS];

static const struct {
    guint keyval;
    GtkScrollType scrolltype;
    GdkModifierType mask;
}
key_scroll_table[] = {
    { GDK_KEY_Left,     GTK_SCROLL_STEP_LEFT,  0, },
    { GDK_KEY_KP_Left,  GTK_SCROLL_STEP_LEFT,  0, },
    { GDK_KEY_Right,    GTK_SCROLL_STEP_RIGHT, 0, },
    { GDK_KEY_KP_Right, GTK_SCROLL_STEP_RIGHT, 0, },
    { GDK_KEY_Up,       GTK_SCROLL_STEP_UP,    0, },
    { GDK_KEY_KP_Up,    GTK_SCROLL_STEP_UP,    0, },
    { GDK_KEY_Down,     GTK_SCROLL_STEP_DOWN,  0, },
    { GDK_KEY_KP_Down,  GTK_SCROLL_STEP_DOWN,  0, },
    { GDK_KEY_Home,     GTK_SCROLL_START,      0, },
    { GDK_KEY_KP_Home,  GTK_SCROLL_START,      0, },
    { GDK_KEY_End,      GTK_SCROLL_END,        0, },
    { GDK_KEY_KP_End,   GTK_SCROLL_END,        0, },
};

static const struct {
    guint keyval;
    GwyZoomType zoomtype;
    GdkModifierType mask;
}
key_zoom_table[] = {
    { GDK_KEY_plus,        GWY_ZOOM_IN,  0, },
    { GDK_KEY_KP_Add,      GWY_ZOOM_IN,  0, },
    { GDK_KEY_equal,       GWY_ZOOM_IN,  0, },
    { GDK_KEY_KP_Equal,    GWY_ZOOM_IN,  0, },
    { GDK_KEY_minus,       GWY_ZOOM_OUT, 0, },
    { GDK_KEY_KP_Subtract, GWY_ZOOM_OUT, 0, },
    { GDK_KEY_z,           GWY_ZOOM_1_1, 0, },
    { GDK_KEY_Z,           GWY_ZOOM_1_1, 0, },
    { GDK_KEY_x,           GWY_ZOOM_FIT, 0, },
    { GDK_KEY_X,           GWY_ZOOM_FIT, 0, },
};

G_DEFINE_TYPE_WITH_CODE(GwyRasterArea, gwy_raster_area, GTK_TYPE_WIDGET,
                        G_IMPLEMENT_INTERFACE(GTK_TYPE_SCROLLABLE, NULL));

static void
gwy_raster_area_class_init(GwyRasterAreaClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

    g_type_class_add_private(klass, sizeof(RasterArea));

    gobject_class->dispose = gwy_raster_area_dispose;
    gobject_class->finalize = gwy_raster_area_finalize;
    gobject_class->get_property = gwy_raster_area_get_property;
    gobject_class->set_property = gwy_raster_area_set_property;

    widget_class->realize = gwy_raster_area_realize;
    widget_class->unrealize = gwy_raster_area_unrealize;
    widget_class->map = gwy_raster_area_map;
    widget_class->unmap = gwy_raster_area_unmap;
    widget_class->get_preferred_width = gwy_raster_area_get_preferred_width;
    widget_class->get_preferred_height = gwy_raster_area_get_preferred_height;
    widget_class->size_allocate = gwy_raster_area_size_allocate;
    widget_class->motion_notify_event = gwy_raster_area_motion_notify;
    widget_class->button_press_event = gwy_raster_area_button_press;
    widget_class->button_release_event = gwy_raster_area_button_release;
    widget_class->key_press_event = gwy_raster_area_key_press;
    widget_class->key_release_event = gwy_raster_area_key_release;
    widget_class->leave_notify_event = gwy_raster_area_leave_notify;
    widget_class->draw = gwy_raster_area_draw;
    widget_class->style_updated = gwy_raster_area_style_updated;

    properties[PROP_SCROLLABLE]
        = g_param_spec_boolean("scrollable",
                               "Scrollable",
                               "Whether the raster area responds to scrolling "
                               "keybindings.",
                               FALSE,
                               G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_ZOOMABLE]
        = g_param_spec_boolean("zoomable",
                               "Zoomable",
                               "Whether the raster area responds to zooming "
                               "keybindings.",
                               FALSE,
                               G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_RANGE_FROM_METHOD]
        = g_param_spec_enum("range-from-method",
                            "Range from-method",
                            "Method to determine the start of false colour "
                            "mapping.",
                            GWY_TYPE_COLOR_RANGE_TYPE,
                            GWY_COLOR_RANGE_FULL,
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_RANGE_TO_METHOD]
        = g_param_spec_enum("range-to-method",
                            "Range to-method",
                            "Method to determine the end of false colour "
                            "mapping.",
                            GWY_TYPE_COLOR_RANGE_TYPE,
                            GWY_COLOR_RANGE_FULL,
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_RANGE]
         = g_param_spec_boxed("range",
                              "Range",
                              "Actual false colour mapping range.  See "
                              "gwy_raster_area_get_range() for discussion.",
                              GWY_TYPE_RANGE,
                              G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    properties[PROP_USER_RANGE]
         = g_param_spec_boxed("user-range",
                              "User range",
                              "False colour mapping range used with method "
                              "GWY_COLOR_RANGE_USER.",
                              GWY_TYPE_RANGE,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_FIELD]
        = g_param_spec_object("field",
                              "Field",
                              "Two-dimensional field shown.",
                              GWY_TYPE_FIELD,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_MASK]
        = g_param_spec_object("mask",
                              "Mask",
                              "Two-dimensional mask shown over the field.",
                              GWY_TYPE_MASK_FIELD,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_GRADIENT]
        = g_param_spec_object("gradient",
                              "Gradient",
                              "Gradient used for visualisation.",
                              GWY_TYPE_GRADIENT,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_SHAPES]
        = g_param_spec_object("shapes",
                              "Shapes",
                              "Geometric shapes shown on top of the area.",
                              GWY_TYPE_SHAPES,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_MASK_COLOR]
        = g_param_spec_boxed("mask-color",
                             "Mask color",
                             "Colour used for mask visualisation.",
                             GWY_TYPE_RGBA,
                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_ZOOM]
        = g_param_spec_double("zoom",
                              "Zoom",
                              "Scaling of the field in the horizontal "
                              "direction.  Vertical scaling depends on "
                              "real-aspect-ratio.  Zoom of 0 means the widget "
                              "requests the minimum size 1×1 and scales to "
                              "whatever size is given to it.",
                              0.0, G_MAXDOUBLE, 1.0,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_REAL_ASPECT_RATIO]
        = g_param_spec_boolean("real-aspect-ratio",
                               "Real aspect ratio",
                               "Whether the real dimensions of the field "
                               "determine the sizes (as opposed to pixel "
                               "dimensions).",
                               FALSE,
                               G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_NUMBER_GRAINS]
        = g_param_spec_boolean("number-grains",
                               "Number grains",
                               "Whether to display mask grain numbers.",
                               FALSE,
                               G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_GRAIN_NUMBER_COLOR]
        = g_param_spec_boxed("grain-number-color",
                             "Grain number color",
                             "Colour used for grain number visualisation.",
                             GWY_TYPE_RGBA,
                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    for (guint i = 1; i < N_PROPS; i++)
        g_object_class_install_property(gobject_class, i, properties[i]);

    gwy_override_class_properties(gobject_class, properties,
                                  "hadjustment", PROP_HADJUSTMENT,
                                  "vadjustment", PROP_VADJUSTMENT,
                                  "hscroll-policy", PROP_HSCROLL_POLICY,
                                  "vscroll-policy", PROP_VSCROLL_POLICY,
                                  NULL);

    /**
     * GwyRasterArea::scroll:
     * @gwyrasterarea: The #GwyRasterArea which received the signal.
     * @arg1: #GtkScrollType describing where and how much to scroll.
     *
     * The ::scroll signal is a
     * <link linkend="keybinding-signals">keybinding signal</link>
     * which gets emitted when a keybinding that scrolls is pressed.
     * The raster area then scrolls itself as requested.
     *
     * #GwyRasterArea respons only to two-dimensional scrolling types such
     * as %GTK_SCROLL_STEP_LEFT since one-dimensional scrolling types such as
     * %GTK_SCROLL_PAGE_BACKWARD are meaningless.
     */
    signals[SGNL_SCROLL]
        = g_signal_new_class_handler("scroll",
                                     G_OBJECT_CLASS_TYPE(klass),
                                     G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                     G_CALLBACK(gwy_raster_area_scroll),
                                     NULL, NULL,
                                     _gwy_cclosure_marshal_BOOLEAN__ENUM,
                                     G_TYPE_BOOLEAN, 1,
                                     GTK_TYPE_SCROLL_TYPE);

    /**
     * GwyRasterArea::zoom:
     * @gwyrasterarea: The #GwyRasterArea which received the signal.
     * @arg1: #GwyZoomType describing the requested zoom change.
     *
     * The ::zoom signal is a
     * <link linkend="keybinding-signals">keybinding signal</link>
     * which gets emitted when a keybinding that scrolls is pressed.
     * The raster area then zooms itself as requested.
     */
    signals[SGNL_ZOOM]
        = g_signal_new_class_handler("zoom",
                                     G_OBJECT_CLASS_TYPE(klass),
                                     G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                     G_CALLBACK(gwy_raster_area_zoom),
                                     NULL, NULL,
                                     _gwy_cclosure_marshal_BOOLEAN__ENUM,
                                     G_TYPE_BOOLEAN, 1,
                                     GWY_TYPE_ZOOM_TYPE);

    GtkBindingSet *binding_set = gtk_binding_set_by_class(klass);

    for (guint i = 0; i < G_N_ELEMENTS(key_scroll_table); i++) {
        GtkScrollType scrolltype = key_scroll_table[i].scrolltype;
        guint keyval = key_scroll_table[i].keyval;
        GdkModifierType mask = key_scroll_table[i].mask;
        gtk_binding_entry_add_signal(binding_set, keyval, mask,
                                     "scroll", 1,
                                     GTK_TYPE_SCROLL_TYPE, scrolltype);
    }

    for (guint i = 0; i < G_N_ELEMENTS(key_zoom_table); i++) {
        GtkScrollType zoomtype = key_zoom_table[i].zoomtype;
        guint keyval = key_zoom_table[i].keyval;
        GdkModifierType mask = key_zoom_table[i].mask;
        gtk_binding_entry_add_signal(binding_set, keyval, mask,
                                     "zoom", 1,
                                     GWY_TYPE_ZOOM_TYPE, zoomtype);
    }
}

static void
gwy_raster_area_init(GwyRasterArea *rasterarea)
{
    rasterarea->priv = G_TYPE_INSTANCE_GET_PRIVATE(rasterarea,
                                                   GWY_TYPE_RASTER_AREA,
                                                   RasterArea);
    RasterArea *priv = rasterarea->priv;
    priv->field_aspect_ratio = 1.0;
    priv->mask_color = mask_color_default;
    priv->grain_number_color = grain_number_color_default;
    priv->range_from_method = GWY_COLOR_RANGE_FULL;
    priv->range_to_method = GWY_COLOR_RANGE_FULL;
    priv->user_range = void_range;
    update_matrices(rasterarea);
    gtk_widget_set_can_focus(GTK_WIDGET(rasterarea), TRUE);
}

static void
gwy_raster_area_finalize(GObject *object)
{
    G_OBJECT_CLASS(gwy_raster_area_parent_class)->finalize(object);
}

static void
gwy_raster_area_dispose(GObject *object)
{
    GwyRasterArea *rasterarea = GWY_RASTER_AREA(object);

    GWY_OBJECT_UNREF(rasterarea->priv->layout);
    set_field(rasterarea, NULL);
    set_mask(rasterarea, NULL);
    set_gradient(rasterarea, NULL);
    set_shapes(rasterarea, NULL);
    set_hadjustment(rasterarea, NULL);
    set_vadjustment(rasterarea, NULL);
    destroy_field_surface(rasterarea);
    destroy_mask_surface(rasterarea);
    rasterarea->priv->area_widget = NULL;

    G_OBJECT_CLASS(gwy_raster_area_parent_class)->dispose(object);
}

static void
gwy_raster_area_set_property(GObject *object,
                             guint prop_id,
                             const GValue *value,
                             GParamSpec *pspec)
{
    GwyRasterArea *rasterarea = GWY_RASTER_AREA(object);

    switch (prop_id) {
        case PROP_SCROLLABLE:
        set_scrollable(rasterarea, g_value_get_boolean(value));
        break;

        case PROP_ZOOMABLE:
        set_zoomable(rasterarea, g_value_get_boolean(value));
        break;

        case PROP_RANGE_FROM_METHOD:
        set_range_from_method(rasterarea, g_value_get_enum(value));
        break;

        case PROP_RANGE_TO_METHOD:
        set_range_to_method(rasterarea, g_value_get_enum(value));
        break;

        case PROP_USER_RANGE:
        set_user_range(rasterarea, (const GwyRange*)g_value_get_boxed(value));
        break;

        case PROP_FIELD:
        set_field(rasterarea, g_value_get_object(value));
        break;

        case PROP_MASK:
        set_mask(rasterarea, g_value_get_object(value));
        break;

        case PROP_GRADIENT:
        set_gradient(rasterarea, g_value_get_object(value));
        break;

        case PROP_SHAPES:
        set_shapes(rasterarea, g_value_get_object(value));
        break;

        case PROP_MASK_COLOR:
        set_mask_color(rasterarea, g_value_get_boxed(value));
        break;

        case PROP_ZOOM:
        set_zoom(rasterarea, g_value_get_double(value));
        break;

        case PROP_REAL_ASPECT_RATIO:
        set_real_aspect_ratio(rasterarea, g_value_get_boolean(value));
        break;

        case PROP_NUMBER_GRAINS:
        set_number_grains(rasterarea, g_value_get_boolean(value));
        break;

        case PROP_GRAIN_NUMBER_COLOR:
        set_grain_number_color(rasterarea, g_value_get_boxed(value));
        break;

        case PROP_HADJUSTMENT:
        set_hadjustment(rasterarea, (GtkAdjustment*)g_value_get_object(value));
        break;

        case PROP_VADJUSTMENT:
        set_vadjustment(rasterarea, (GtkAdjustment*)g_value_get_object(value));
        break;

        case PROP_HSCROLL_POLICY:
        rasterarea->priv->hscroll_policy = g_value_get_enum(value);
        gtk_widget_queue_resize(GTK_WIDGET(rasterarea));
        break;

        case PROP_VSCROLL_POLICY:
        rasterarea->priv->vscroll_policy = g_value_get_enum(value);
        gtk_widget_queue_resize(GTK_WIDGET(rasterarea));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_raster_area_get_property(GObject *object,
                             guint prop_id,
                             GValue *value,
                             GParamSpec *pspec)
{
    RasterArea *priv = GWY_RASTER_AREA(object)->priv;

    switch (prop_id) {
        case PROP_SCROLLABLE:
        g_value_set_boolean(value, priv->scrollable);
        break;

        case PROP_ZOOMABLE:
        g_value_set_boolean(value, priv->zoomable);
        break;

        case PROP_RANGE_FROM_METHOD:
        g_value_set_enum(value, priv->range_from_method);
        break;

        case PROP_RANGE_TO_METHOD:
        g_value_set_enum(value, priv->range_from_method);
        break;

        case PROP_RANGE:
        g_value_set_boxed(value, &priv->range);
        break;

        case PROP_USER_RANGE:
        g_value_set_boxed(value, &priv->user_range);
        break;

        case PROP_FIELD:
        g_value_set_object(value, priv->field);
        break;

        case PROP_MASK:
        g_value_set_object(value, priv->mask);
        break;

        case PROP_GRADIENT:
        g_value_set_object(value, priv->gradient);
        break;

        case PROP_SHAPES:
        g_value_set_object(value, priv->shapes);
        break;

        case PROP_MASK_COLOR:
        g_value_set_boxed(value, &priv->mask_color);
        break;

        case PROP_ZOOM:
        g_value_set_double(value, priv->zoom);
        break;

        case PROP_REAL_ASPECT_RATIO:
        g_value_set_boolean(value, priv->real_aspect_ratio);
        break;

        case PROP_NUMBER_GRAINS:
        g_value_set_boolean(value, priv->number_grains);
        break;

        case PROP_GRAIN_NUMBER_COLOR:
        g_value_set_boxed(value, &priv->grain_number_color);
        break;

        case PROP_HADJUSTMENT:
        g_value_set_object(value, priv->hadjustment);
        break;

        case PROP_VADJUSTMENT:
        g_value_set_object(value, priv->vadjustment);
        break;

        case PROP_HSCROLL_POLICY:
        g_value_set_enum(value, priv->hscroll_policy);
        break;

        case PROP_VSCROLL_POLICY:
        g_value_set_enum(value, priv->vscroll_policy);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

/**
 * gwy_raster_area_new:
 *
 * Creates a new raster area.
 *
 * Returns: A new raster area.
 **/
GtkWidget*
gwy_raster_area_new(void)
{
    return g_object_newv(GWY_TYPE_RASTER_AREA, 0, NULL);
}

/**
 * gwy_raster_area_set_field:
 * @rasterarea: A raster area.
 * @field: (allow-none):
 *         A two-dimensional data field.
 *
 * Sets the field a raster area will display.
 *
 * The raster area conntects to #GwyField::data-changed which is not emitted
 * automatically so you have to emit it to update the area.  Normally this is
 * done once after all the modifications of the field data are finished.
 **/
void
gwy_raster_area_set_field(GwyRasterArea *rasterarea,
                          GwyField *field)
{
    g_return_if_fail(GWY_IS_RASTER_AREA(rasterarea));
    if (!set_field(rasterarea, field))
        return;

    g_object_notify_by_pspec(G_OBJECT(rasterarea), properties[PROP_FIELD]);
}

/**
 * gwy_raster_area_get_field:
 * @rasterarea: A raster area.
 *
 * Obtains the field a raster area displays.
 *
 * Returns: (transfer none):
 *          The field displayed by @rasterarea.
 **/
GwyField*
gwy_raster_area_get_field(const GwyRasterArea *rasterarea)
{
    g_return_val_if_fail(GWY_IS_RASTER_AREA(rasterarea), NULL);
    return GWY_RASTER_AREA(rasterarea)->priv->field;
}

/**
 * gwy_raster_area_set_mask:
 * @rasterarea: A raster area.
 * @mask: (allow-none):
 *         A two-dimensional mask field.
 *
 * Sets the mask a raster area will display over the field.
 *
 * The mask dimensions must match the field dimensions.  More precisely, the
 * dimensions must match at the time the raster area is actually drawn so you
 * can set the field and mask independently if the dimensions will be all right
 * at the end.
 *
 * The raster area conntects to #GwyMaskField::data-changed which is not
 * emitted automatically so you have to emit it to update the area.  Normally
 * this is done once after all the modifications of the mask data are
 * finished.
 **/
void
gwy_raster_area_set_mask(GwyRasterArea *rasterarea,
                         GwyMaskField *mask)
{
    g_return_if_fail(GWY_IS_RASTER_AREA(rasterarea));
    if (!set_mask(rasterarea, mask))
        return;

    g_object_notify_by_pspec(G_OBJECT(rasterarea), properties[PROP_MASK]);
}

/**
 * gwy_raster_area_get_mask:
 * @rasterarea: A raster area.
 *
 * Obtains the mask a raster area displays over the field.
 *
 * Returns: (transfer none):
 *          The mask displayed by @rasterarea over the field.
 **/
GwyMaskField*
gwy_raster_area_get_mask(const GwyRasterArea *rasterarea)
{
    g_return_val_if_fail(GWY_IS_RASTER_AREA(rasterarea), NULL);
    return GWY_RASTER_AREA(rasterarea)->priv->mask;
}

/**
 * gwy_raster_area_set_gradient:
 * @rasterarea: A raster area.
 * @gradient: (allow-none):
 *            A colour gradient.  %NULL means the default gradient
 *            will be used.
 *
 * Sets the false colour gradient a raster area will use for visualisation.
 **/
void
gwy_raster_area_set_gradient(GwyRasterArea *rasterarea,
                             GwyGradient *gradient)
{
    g_return_if_fail(GWY_IS_RASTER_AREA(rasterarea));
    if (!set_gradient(rasterarea, gradient))
        return;

    g_object_notify_by_pspec(G_OBJECT(rasterarea), properties[PROP_GRADIENT]);
}

/**
 * gwy_raster_area_get_gradient:
 * @rasterarea: A raster area.
 *
 * Obtains the false colour gradient used by a raster area for visualisation.
 *
 * Returns: (allow-none) (transfer none):
 *          The colour gradient used by @rasterarea.  If no gradient was set
 *          and the default one is used, function returns %NULL.
 **/
GwyGradient*
gwy_raster_area_get_gradient(const GwyRasterArea *rasterarea)
{
    g_return_val_if_fail(GWY_IS_RASTER_AREA(rasterarea), NULL);
    return GWY_RASTER_AREA(rasterarea)->priv->gradient;
}

/**
 * gwy_raster_area_set_range_from_method:
 * @rasterarea: A raster area.
 * @method: Method how to determine the start of false colour range.
 *
 * Sets the method how a raster area determines the start of false colour
 * mapping.
 **/
void
gwy_raster_area_set_range_from_method(GwyRasterArea *rasterarea,
                                      GwyColorRangeType method)
{
    g_return_if_fail(GWY_IS_RASTER_AREA(rasterarea));
    if (!set_range_from_method(rasterarea, method))
        return;

    g_object_notify_by_pspec(G_OBJECT(rasterarea),
                             properties[PROP_RANGE_FROM_METHOD]);
}

/**
 * gwy_raster_area_get_range_from_method:
 * @rasterarea: A raster area.
 *
 * Gets the method how a raster area determines the start of false colour
 * mapping.
 *
 * Returns: The method how the start of false colour range mapping is
 *          determined.
 **/
GwyColorRangeType
gwy_raster_area_get_range_from_method(const GwyRasterArea *rasterarea)
{
    g_return_val_if_fail(GWY_IS_RASTER_AREA(rasterarea),
                         GWY_COLOR_RANGE_FULL);
    return rasterarea->priv->range_from_method;
}

/**
 * gwy_raster_area_set_range_to_method:
 * @rasterarea: A raster area.
 * @method: Method how to determine the end of false colour range.
 *
 * Sets the method how a raster area determines the end of false colour
 * mapping.
 **/
void
gwy_raster_area_set_range_to_method(GwyRasterArea *rasterarea,
                                    GwyColorRangeType method)
{
    g_return_if_fail(GWY_IS_RASTER_AREA(rasterarea));
    if (!set_range_to_method(rasterarea, method))
        return;

    g_object_notify_by_pspec(G_OBJECT(rasterarea),
                             properties[PROP_RANGE_TO_METHOD]);
}

/**
 * gwy_raster_area_get_range_to_method:
 * @rasterarea: A raster area.
 *
 * Gets the method how a raster area determines the end of false colour
 * mapping.
 *
 * Returns: The method how the end of false colour range mapping is
 *          determined.
 **/
GwyColorRangeType
gwy_raster_area_get_range_to_method(const GwyRasterArea *rasterarea)
{
    g_return_val_if_fail(GWY_IS_RASTER_AREA(rasterarea),
                         GWY_COLOR_RANGE_FULL);
    return rasterarea->priv->range_to_method;
}

/**
 * gwy_raster_area_set_user_range:
 * @rasterarea: A raster area.
 * @range: (allow-none):
 *         The user false colour mapping range.  It has effect only on
 *         range endpoints with %GWY_COLOR_RANGE_USER mapping type.  Passing
 *         %NULL resets it to default (0,0).
 *
 * Sets the user false colour mapping range of a raster area.
 **/
void
gwy_raster_area_set_user_range(GwyRasterArea *rasterarea,
                               const GwyRange *range)
{
    g_return_if_fail(GWY_IS_RASTER_AREA(rasterarea));
    if (!range)
        range = &void_range;
    if (!set_user_range(rasterarea, range))
        return;

    g_object_notify_by_pspec(G_OBJECT(rasterarea), properties[PROP_USER_RANGE]);
}

/**
 * gwy_raster_area_get_user_range:
 * @rasterarea: A raster area.
 * @range: (out):
 *         Return location for the user false colour mapping range.
 *
 * Gets the user false colour mapping range of a raster area.
 **/
void
gwy_raster_area_get_user_range(const GwyRasterArea *rasterarea,
                               GwyRange *range)
{
    g_return_if_fail(GWY_IS_RASTER_AREA(rasterarea));
    g_return_if_fail(range);
    *range = rasterarea->priv->user_range;
}

/**
 * gwy_raster_area_get_range:
 * @rasterarea: A raster area.
 * @range: (out) (allow-none):
 *         Actual false colour mapping range.  It is set, even if the function
 *         returns %FALSE.  The utility of a stale range is limited though.
 *
 * Obtains the actual false colour mapping range of a raster area.
 *
 * The false colour mapping range is not recalculated immediately after the
 * properties that might influence it change.  In general, it is only updated
 * only for drawing.  If you want to bind another actions to range changes it
 * is recommended to connect to signal "notify::range" of @rasterarea.
 * Normally, such callback will be invoked immediately after the widget redraws
 * itself.
 *
 * Returns: %TRUE if the range is valid, i.e. there is no redraw accompanied
 *          with range recalculation queued.  %FALSE if the range is stale.
 **/
gboolean
gwy_raster_area_get_range(const GwyRasterArea *rasterarea,
                          GwyRange *range)
{
    g_return_val_if_fail(GWY_IS_RASTER_AREA(rasterarea), FALSE);
    GWY_MAYBE_SET(range, rasterarea->priv->range);
    return rasterarea->priv->range_valid;
}

/**
 * gwy_raster_area_set_mask_color:
 * @rasterarea: A raster area.
 * @color: A colour.
 *
 * Sets the colour a raster area will use for mask visualisation.
 **/
void
gwy_raster_area_set_mask_color(GwyRasterArea *rasterarea,
                               const GwyRGBA *color)
{
    g_return_if_fail(GWY_IS_RASTER_AREA(rasterarea));
    g_return_if_fail(color);
    if (!set_mask_color(rasterarea, color))
        return;

    g_object_notify_by_pspec(G_OBJECT(rasterarea), properties[PROP_MASK_COLOR]);
}

/**
 * gwy_raster_area_get_mask_color:
 * @rasterarea: A raster area.
 *
 * Obtains the colour used by a raster area for mask visualisation.
 *
 * Returns: (transfer none):
 *          The colour used by @rasterarea for mask visualisation.
 **/
const GwyRGBA*
gwy_raster_area_get_mask_color(const GwyRasterArea *rasterarea)
{
    g_return_val_if_fail(GWY_IS_RASTER_AREA(rasterarea), NULL);
    return &GWY_RASTER_AREA(rasterarea)->priv->mask_color;
}

/**
 * gwy_raster_area_set_grain_number_color:
 * @rasterarea: A raster area.
 * @color: A colour.
 *
 * Sets the colour a raster area will use for grain number visualisation.
 **/
void
gwy_raster_area_set_grain_number_color(GwyRasterArea *rasterarea,
                                       const GwyRGBA *color)
{
    g_return_if_fail(GWY_IS_RASTER_AREA(rasterarea));
    g_return_if_fail(color);
    if (!set_grain_number_color(rasterarea, color))
        return;

    g_object_notify_by_pspec(G_OBJECT(rasterarea),
                             properties[PROP_GRAIN_NUMBER_COLOR]);
}

/**
 * gwy_raster_area_get_grain_number_color:
 * @rasterarea: A raster area.
 *
 * Obtains the colour used by a raster area for grain number visualisation.
 *
 * Returns: (transfer none):
 *          The colour used by @rasterarea for grain number visualisation.
 **/
const GwyRGBA*
gwy_raster_area_get_grain_number_color(const GwyRasterArea *rasterarea)
{
    g_return_val_if_fail(GWY_IS_RASTER_AREA(rasterarea), NULL);
    return &GWY_RASTER_AREA(rasterarea)->priv->grain_number_color;
}

/**
 * gwy_raster_area_set_shapes:
 * @rasterarea: A raster area.
 * @shapes: (allow-none):
 *          A group of geometrical shapes.
 *
 * Sets the group of geometrical shapes to be displayed on the top of a raster
 * area.
 **/
void
gwy_raster_area_set_shapes(GwyRasterArea *rasterarea,
                           GwyShapes *shapes)
{
    g_return_if_fail(GWY_IS_RASTER_AREA(rasterarea));
    if (!set_shapes(rasterarea, shapes))
        return;

    g_object_notify_by_pspec(G_OBJECT(rasterarea), properties[PROP_SHAPES]);
}

/**
 * gwy_raster_area_get_shapes:
 * @rasterarea: A raster area.
 *
 * Obtains the group of geometrical shapes displayed on the top of a raster
 * area.
 *
 * Returns: (transfer none):
 *          The shapes displayed by @rasterarea.
 **/
GwyShapes*
gwy_raster_area_get_shapes(const GwyRasterArea *rasterarea)
{
    g_return_val_if_fail(GWY_IS_RASTER_AREA(rasterarea), NULL);
    return rasterarea->priv->shapes;
}

/**
 * gwy_raster_area_set_scrollable:
 * @rasterarea: A raster area.
 * @scrollable: %TRUE to enable user-invoked scrolling, %FALSE to disable it.
 *
 * Sets whether a raster area can scroll itself in response to user actions.
 *
 * User-invoked scrolling includes both scrolling keybindings and moving shapes
 * outside the currently displayed area.
 **/
void
gwy_raster_area_set_scrollable(GwyRasterArea *rasterarea,
                               gboolean scrollable)
{
    g_return_if_fail(GWY_IS_RASTER_AREA(rasterarea));
    if (!set_scrollable(rasterarea, scrollable))
        return;

    g_object_notify_by_pspec(G_OBJECT(rasterarea), properties[PROP_SCROLLABLE]);
}

/**
 * gwy_raster_area_get_scrollable:
 * @rasterarea: A raster area.
 *
 * Gets whether a raster area can scroll itself in response to user actions.
 *
 * Returns: %TRUE if user-invoked scrolling is enabled, %FALSE if it is
 *          disabled.
 **/
gboolean
gwy_raster_area_get_scrollable(GwyRasterArea *rasterarea)
{
    g_return_val_if_fail(GWY_IS_RASTER_AREA(rasterarea), FALSE);
    return rasterarea->priv->scrollable;
}

/**
 * gwy_raster_area_set_zoomable:
 * @rasterarea: A raster area.
 * @zoomable: %TRUE to enable user-invoked zooming, %FALSE to disable it.
 *
 * Sets whether a raster area can zoom itself in response to user actions.
 *
 * User-invoked zooming includes both zooming keybindings.
 **/
void
gwy_raster_area_set_zoomable(GwyRasterArea *rasterarea,
                             gboolean zoomable)
{
    g_return_if_fail(GWY_IS_RASTER_AREA(rasterarea));
    if (!set_zoomable(rasterarea, zoomable))
        return;

    g_object_notify_by_pspec(G_OBJECT(rasterarea), properties[PROP_ZOOMABLE]);
}

/**
 * gwy_raster_area_get_zoomable:
 * @rasterarea: A raster area.
 *
 * Gets whether a raster area can zoom itself in response to user actions.
 *
 * Returns: %TRUE if user-invoked zooming is enabled, %FALSE if it is
 *          disabled.
 **/
gboolean
gwy_raster_area_get_zoomable(GwyRasterArea *rasterarea)
{
    g_return_val_if_fail(GWY_IS_RASTER_AREA(rasterarea), FALSE);
    return rasterarea->priv->zoomable;
}

/**
 * gwy_raster_area_widget_area:
 * @rasterarea: A raster area.
 * @area: (out):
 *        Location to store the widget area.
 *
 * Calculates the field pixel rectangle corresponding to the size of an entire
 * raster area.
 *
 * The widget area is the same as the field area if the area shows only a part
 * of the field.  However, if there is padding the widget area may correspond
 * to coordinates that are outside the field.
 *
 * See also gwy_raster_area_set_area_widget().
 **/
void
gwy_raster_area_widget_area(const GwyRasterArea *rasterarea,
                            cairo_rectangle_t *area)
{
    g_return_if_fail(GWY_IS_RASTER_AREA(rasterarea));
    g_return_if_fail(area);

    RasterArea *priv = rasterarea->priv;
    GtkWidget *widget = (priv->area_widget
                         ? priv->area_widget
                         : GTK_WIDGET(rasterarea));
    area->x = area->y = 0.0;
    area->width = gtk_widget_get_allocated_width(widget);
    area->height = gtk_widget_get_allocated_height(widget);
    cairo_matrix_transform_point(&priv->window_to_field_matrix,
                                 &area->x, &area->y);
    cairo_matrix_transform_distance(&priv->window_to_field_matrix,
                                    &area->width, &area->height);
}

/**
 * gwy_raster_area_field_area:
 * @rasterarea: A raster area.
 * @area: (out):
 *        Location to store the field area.
 *
 * Obtains the field pixel rectangle corresponding to the part currently
 * displayed by a raster area.
 *
 * The field rectangle is never larger than the field.  It may be smaller if
 * only a part of the field is shown and the coordinates may be non-integral.
 **/
void
gwy_raster_area_field_area(const GwyRasterArea *rasterarea,
                           cairo_rectangle_t *area)
{
    g_return_if_fail(GWY_IS_RASTER_AREA(rasterarea));
    g_return_if_fail(area);
    *area = rasterarea->priv->field_rectangle;
}

/**
 * gwy_raster_area_get_area_widget:
 * @rasterarea: A raster area.
 *
 * Obtains the area widget of a raster area.
 *
 * See gwy_raster_area_set_area_widget() for discussion.
 *
 * Returns: (transfer none) (allow-none):
 *          The widget used for area calculation in
 *          gwy_raster_area_widget_area(), %NULL if no widget is set and
 *          @rasterarea itself is used.
 **/
GtkWidget*
gwy_raster_area_get_area_widget(const GwyRasterArea *rasterarea)
{
    g_return_val_if_fail(GWY_IS_RASTER_AREA(rasterarea), NULL);
    return rasterarea->priv->area_widget;
}

/**
 * gwy_raster_area_set_area_widget:
 * @rasterarea: A raster area.
 * @widget: (transfer none) (allow-none):
 *          Widget to use for determining the size of entire raster area
 *          in gwy_raster_area_widget_area().  Pass %NULL to unset the
 *          area widget and use @rasterarea itself.
 *
 * Sets the area widget of a raster area.
 *
 * If the raster area is put into a #GtkScrolledWindow or a similar container
 * that can ‘eat’ part of the child area, the meaning of entire widget as
 * used by gwy_raster_area_widget_area() changes.  The correct dimensions
 * that may be necessary to draw rulers right are those of the container, not
 * the raster area.  To fix the calculation use this function to tell
 * @rasterarea to use the dimensions of another widget instead of self:
 * |[
 * GtkWidget *scwin = gtk_scrolled_window_new(NULL, NULL);
 * gtk_container_add(GTK_CONTAINER(scwin), rasterarea);
 * gwy_raster_area_set_area_widget(GWY_RASTER_AREA(rasterarea), scwin);
 * ]|
 *
 * Since the common use case is that @widget contains and thus owns
 * @rasterarea, no reference to @widget is taken by @rasterarea.
 **/
void
gwy_raster_area_set_area_widget(GwyRasterArea *rasterarea,
                                GtkWidget *widget)
{
    g_return_if_fail(GWY_IS_RASTER_AREA(rasterarea));
    g_return_if_fail(!widget || GTK_IS_WIDGET(widget));
    rasterarea->priv->area_widget = widget;
}

/**
 * gwy_raster_area_get_widget_to_field_matrix:
 * @rasterarea: A raster area.
 *
 * Obtains the Cairo matrix representing transformation from widget coordinates
 * to field pixel coordinates for a raster area.
 *
 * Returns: (transfer none):
 *          Cairo matrix representing the coordinates.  The pointer remains
 *          valid through entire @rasterarea lifetime and the pointed-to matrix
 *          will reflect the current transformation.
 **/
const cairo_matrix_t*
gwy_raster_area_get_widget_to_field_matrix(const GwyRasterArea *rasterarea)
{
    g_return_val_if_fail(GWY_IS_RASTER_AREA(rasterarea), NULL);
    return &rasterarea->priv->window_to_field_matrix;
}

/**
 * gwy_raster_area_get_widget_to_coords_matrix:
 * @rasterarea: A raster area.
 *
 * Obtains the Cairo matrix representing transformation from widget coordinates
 * to field real coordinates for a raster area.
 *
 * Returns: (transfer none):
 *          Cairo matrix representing the coordinates.  The pointer remains
 *          valid through entire @rasterarea lifetime and the pointed-to matrix
 *          will reflect the current transformation.
 **/
const cairo_matrix_t*
gwy_raster_area_get_widget_to_coords_matrix(const GwyRasterArea *rasterarea)
{
    g_return_val_if_fail(GWY_IS_RASTER_AREA(rasterarea), NULL);
    return &rasterarea->priv->window_to_coords_matrix;
}

/**
 * gwy_raster_area_get_field_to_widget_matrix:
 * @rasterarea: A raster area.
 *
 * Obtains the Cairo matrix representing transformation from field pixel
 * coodinates to widget coordinates for a raster area.
 *
 * Returns: (transfer none):
 *          Cairo matrix representing the coordinates.  The pointer remains
 *          valid through entire @rasterarea lifetime and the pointed-to matrix
 *          will reflect the current transformation.
 **/
const cairo_matrix_t*
gwy_raster_area_get_field_to_widget_matrix(const GwyRasterArea *rasterarea)
{
    g_return_val_if_fail(GWY_IS_RASTER_AREA(rasterarea), NULL);
    return &rasterarea->priv->field_to_window_matrix;
}

/**
 * gwy_raster_area_get_coords_to_widget_matrix:
 * @rasterarea: A raster area.
 *
 * Obtains the Cairo matrix representing transformation from field real
 * coodinates to widget coordinates for a raster area.
 *
 * Returns: (transfer none):
 *          Cairo matrix representing the coordinates.  The pointer remains
 *          valid through entire @rasterarea lifetime and the pointed-to matrix
 *          will reflect the current transformation.
 **/
const cairo_matrix_t*
gwy_raster_area_get_coords_to_widget_matrix(const GwyRasterArea *rasterarea)
{
    g_return_val_if_fail(GWY_IS_RASTER_AREA(rasterarea), NULL);
    return &rasterarea->priv->coords_to_window_matrix;
}

static void
create_window(GwyRasterArea *rasterarea)
{
    RasterArea *priv = rasterarea->priv;
    GtkWidget *widget = GTK_WIDGET(rasterarea);

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
                       | GDK_KEY_PRESS_MASK
                       | GDK_KEY_RELEASE_MASK
                       | GDK_SCROLL_MASK
                       | GDK_POINTER_MOTION_MASK
                       | GDK_POINTER_MOTION_HINT_MASK
                       | GDK_ENTER_NOTIFY_MASK
                       | GDK_LEAVE_NOTIFY_MASK),
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
destroy_window(GwyRasterArea *rasterarea)
{
    RasterArea *priv = rasterarea->priv;

    if (!priv->window)
        return;

    gdk_window_set_user_data(priv->window, NULL);
    gdk_window_destroy(priv->window);
    priv->window = NULL;
}

static void
gwy_raster_area_realize(GtkWidget *widget)
{
    GwyRasterArea *rasterarea = GWY_RASTER_AREA(widget);
    gtk_widget_set_realized(widget, TRUE);
    create_window(rasterarea);
}

static void
gwy_raster_area_unrealize(GtkWidget *widget)
{
    GwyRasterArea *rasterarea = GWY_RASTER_AREA(widget);
    destroy_window(rasterarea);
    GWY_OBJECT_UNREF(rasterarea->priv->layout);
    GTK_WIDGET_CLASS(gwy_raster_area_parent_class)->unrealize(widget);
}

static void
gwy_raster_area_map(GtkWidget *widget)
{
    GwyRasterArea *rasterarea = GWY_RASTER_AREA(widget);
    GTK_WIDGET_CLASS(gwy_raster_area_parent_class)->map(widget);
    gdk_window_show(rasterarea->priv->window);
}

static void
gwy_raster_area_unmap(GtkWidget *widget)
{
    GwyRasterArea *rasterarea = GWY_RASTER_AREA(widget);
    RasterArea *priv = rasterarea->priv;
    gdk_window_hide(priv->window);
    if (priv->scroll_timer_hid) {
        g_source_remove(priv->scroll_timer_hid);
        priv->scroll_timer_hid = 0;
    }
    GTK_WIDGET_CLASS(gwy_raster_area_parent_class)->unmap(widget);
}

static void
gwy_raster_area_get_preferred_width(GtkWidget *widget,
                                    gint *minimum,
                                    gint *natural)
{
    GwyRasterArea *rasterarea = GWY_RASTER_AREA(widget);
    *natural = calculate_full_width(rasterarea);
    *minimum = rasterarea->priv->zoom ? *natural : 1;
}

static void
gwy_raster_area_get_preferred_height(GtkWidget *widget,
                                     gint *minimum,
                                     gint *natural)
{
    GwyRasterArea *rasterarea = GWY_RASTER_AREA(widget);
    *natural = calculate_full_height(rasterarea);
    *minimum = rasterarea->priv->zoom ? *natural : 1;
}

static void
gwy_raster_area_size_allocate(GtkWidget *widget,
                              GtkAllocation *allocation)
{
    GtkAllocation prev_allocation;
    gtk_widget_get_allocation(widget, &prev_allocation);

    GTK_WIDGET_CLASS(gwy_raster_area_parent_class)->size_allocate(widget,
                                                                  allocation);
    GwyRasterArea *rasterarea = GWY_RASTER_AREA(widget);
    RasterArea *priv = rasterarea->priv;

    // XXX: The input window can be smaller.  Does it matter?
    if (!gwy_equal(allocation, &prev_allocation) && priv->window) {
        gdk_window_move_resize(priv->window,
                               allocation->x, allocation->y,
                               allocation->width, allocation->height);
    }

    priv->field_surface_valid = FALSE;
    priv->mask_surface_valid = FALSE;

    freeze_adjustments_notify(rasterarea, TRUE, TRUE);
    set_hadjustment_values(rasterarea);
    set_vadjustment_values(rasterarea);
    calculate_position_and_size(rasterarea);
    thaw_adjustments_notify(rasterarea, TRUE, TRUE);
}

static gboolean
gwy_raster_area_motion_notify(GtkWidget *widget,
                              GdkEventMotion *event)
{
    GwyRasterArea *rasterarea = GWY_RASTER_AREA(widget);
    RasterArea *priv = rasterarea->priv;
    GwyField *field = priv->field;
    GwyMaskField *mask = priv->mask;

    if (priv->shapes)
        gwy_shapes_motion_notify(priv->shapes, event);

    if (!field || !mask)
        return FALSE;

    GwyXY pos = { .x = event->x, .y = event->y };
    cairo_matrix_transform_point(&priv->window_to_field_matrix, &pos.x, &pos.y);

    if (pos.x < 0.0 || pos.x >= field->xres
        || pos.y < 0.0 || pos.y >= field->yres) {
        if (priv->active_grain) {
            priv->active_grain = 0;
            if (priv->number_grains)
                gtk_widget_queue_draw(widget);
        }
        return FALSE;
    }

    guint j = (guint)floor(pos.x), i = (guint)floor(pos.y);
    g_assert(j < field->xres && i < field->yres);
    const guint *grains = gwy_mask_field_grain_numbers(mask);

    if (grains[i*field->xres + j] != priv->active_grain) {
        priv->active_grain = grains[i*field->xres + j];
        if (priv->number_grains)
            gtk_widget_queue_draw(widget);
    }

    return FALSE;
}

static gboolean
gwy_raster_area_button_press(GtkWidget *widget,
                             GdkEventButton *event)
{
    GwyRasterArea *rasterarea = GWY_RASTER_AREA(widget);
    RasterArea *priv = rasterarea->priv;
    if (priv->shapes)
        gwy_shapes_button_press(priv->shapes, event);

    return FALSE;
}

static gboolean
gwy_raster_area_button_release(GtkWidget *widget,
                               GdkEventButton *event)
{
    GwyRasterArea *rasterarea = GWY_RASTER_AREA(widget);
    RasterArea *priv = rasterarea->priv;
    if (priv->shapes)
        gwy_shapes_button_release(priv->shapes, event);

    return FALSE;
}

static gboolean
gwy_raster_area_key_press(GtkWidget *widget,
                          GdkEventKey *event)
{
    GwyRasterArea *rasterarea = GWY_RASTER_AREA(widget);
    RasterArea *priv = rasterarea->priv;

    if (priv->shapes) {
        if (gwy_shapes_key_press(priv->shapes, event))
            return TRUE;
    }

    return GTK_WIDGET_CLASS(gwy_raster_area_parent_class)->key_press_event(widget, event);
}

static gboolean
gwy_raster_area_key_release(GtkWidget *widget,
                            GdkEventKey *event)
{
    GwyRasterArea *rasterarea = GWY_RASTER_AREA(widget);
    RasterArea *priv = rasterarea->priv;

    if (priv->shapes) {
        if (gwy_shapes_key_release(priv->shapes, event))
            return TRUE;
    }

    return GTK_WIDGET_CLASS(gwy_raster_area_parent_class)->key_release_event(widget, event);
}

static gboolean
gwy_raster_area_leave_notify(GtkWidget *widget,
                             GdkEventCrossing *event)
{
    GwyRasterArea *rasterarea = GWY_RASTER_AREA(widget);
    RasterArea *priv = rasterarea->priv;

    g_assert(event->type == GDK_LEAVE_NOTIFY);
    if (!priv->field || !priv->mask || !priv->active_grain)
        return FALSE;

    priv->active_grain = 0;
    gtk_widget_queue_draw(widget);

    GtkWidgetClass *klass = GTK_WIDGET_CLASS(gwy_raster_area_parent_class);
    if (klass->leave_notify_event)
        return klass->leave_notify_event(widget, event);
    return FALSE;
}

static gboolean
gwy_raster_area_scroll(GwyRasterArea *rasterarea,
                       GtkScrollType scrolltype)
{
    RasterArea *priv = rasterarea->priv;
    if (!priv->scrollable)
        return FALSE;

    GtkAdjustment *vadj = priv->vadjustment,
                  *hadj = priv->hadjustment;
    gboolean scrolling = FALSE;

    if (hadj) {
        gdouble value = gtk_adjustment_get_value(hadj), newvalue = value;
        gdouble lower = gtk_adjustment_get_lower(hadj),
                upper = gtk_adjustment_get_upper(hadj);

        if (scrolltype == GTK_SCROLL_START)
            newvalue = lower;
        else if (scrolltype == GTK_SCROLL_PAGE_LEFT) {
            gdouble page = gtk_adjustment_get_page_increment(hadj);
            newvalue = MAX(lower, value - page);
        }
        else if (scrolltype == GTK_SCROLL_STEP_LEFT) {
            gdouble step = gtk_adjustment_get_step_increment(hadj);
            newvalue = MAX(lower, value - step);
        }
        else if (scrolltype == GTK_SCROLL_STEP_RIGHT) {
            gdouble step = gtk_adjustment_get_step_increment(hadj);
            newvalue = MIN(upper, value + step);
        }
        else if (scrolltype == GTK_SCROLL_PAGE_RIGHT) {
            gdouble page = gtk_adjustment_get_page_increment(hadj);
            newvalue = MIN(lower, value + page);
        }
        else if (scrolltype == GTK_SCROLL_END)
            newvalue = upper;

        if (newvalue != value) {
            gtk_adjustment_set_value(hadj, newvalue);
            scrolling = TRUE;
        }
    }

    if (vadj) {
        gdouble value = gtk_adjustment_get_value(vadj), newvalue = value;
        gdouble lower = gtk_adjustment_get_lower(vadj),
                upper = gtk_adjustment_get_upper(vadj);

        if (scrolltype == GTK_SCROLL_START)
            newvalue = lower;
        else if (scrolltype == GTK_SCROLL_PAGE_UP) {
            gdouble page = gtk_adjustment_get_page_increment(vadj);
            newvalue = MAX(lower, value - page);
        }
        else if (scrolltype == GTK_SCROLL_STEP_UP) {
            gdouble step = gtk_adjustment_get_step_increment(vadj);
            newvalue = MAX(lower, value - step);
        }
        else if (scrolltype == GTK_SCROLL_STEP_DOWN) {
            gdouble step = gtk_adjustment_get_step_increment(vadj);
            newvalue = MIN(upper, value + step);
        }
        else if (scrolltype == GTK_SCROLL_PAGE_DOWN) {
            gdouble page = gtk_adjustment_get_page_increment(vadj);
            newvalue = MIN(lower, value + page);
        }
        else if (scrolltype == GTK_SCROLL_END)
            newvalue = upper;

        if (newvalue != value) {
            gtk_adjustment_set_value(vadj, newvalue);
            scrolling = TRUE;
        }
    }

    return scrolling;
}

static gboolean
gwy_raster_area_zoom(GwyRasterArea *rasterarea,
                     GwyZoomType zoomtype)
{
    static const gdouble zoom_values[] = {
        0.0625, 0.0833333333333333, 0.1, 0.125, 0.166666666666667, 0.25,
        0.333333333333333, 0.5, 0.666666666666667, 1.0, 1.5, 2.0, 3.0,
        4.0, 6.0, 8.0, 10.0, 12.0, 16.0
    };

    RasterArea *priv = rasterarea->priv;
    if (!priv->zoomable)
        return FALSE;

    gdouble zoom = priv->zoom;
    if (!zoom)
        return FALSE;

    if (zoomtype == GWY_ZOOM_IN) {
        for (guint i = 0; i < G_N_ELEMENTS(zoom_values); i++) {
            if (zoom_values[i] > zoom)
                return set_zoom(rasterarea, zoom_values[i]);
        }
    }
    else if (zoomtype == GWY_ZOOM_OUT) {
        for (guint i = G_N_ELEMENTS(zoom_values); i; i--) {
            if (zoom_values[i-1] < zoom)
                return set_zoom(rasterarea, zoom_values[i-1]);
        }
    }
    else if (zoomtype == GWY_ZOOM_1_1) {
        return set_zoom(rasterarea, 1.0);
    }
    else if (zoomtype == GWY_ZOOM_FIT) {
        if (!priv->field)
            return FALSE;

        GwyField *field = priv->field;
        GtkWidget *widget = GTK_WIDGET(rasterarea);
        GtkAllocation allocation;
        gtk_widget_get_allocation(widget, &allocation);
        gdouble xscale = (gdouble)allocation.width/field->xres;
        gdouble yscale = (gdouble)allocation.height/field->yres;
        if (priv->real_aspect_ratio)
            yscale *= gwy_field_dx(field)/gwy_field_dy(field);

        return set_zoom(rasterarea, MIN(xscale, yscale));
    }

    return FALSE;
}

static void
calculate_position_and_size(GwyRasterArea *rasterarea)
{
    GtkWidget *widget = GTK_WIDGET(rasterarea);
    RasterArea *priv = rasterarea->priv;
    cairo_rectangle_int_t *irect = &priv->image_rectangle;
    cairo_rectangle_t *frect = &priv->field_rectangle;
    gint width = gtk_widget_get_allocated_width(widget);
    gint height = gtk_widget_get_allocated_height(widget);
    gint full_width = calculate_full_width(rasterarea);
    gint full_height = calculate_full_height(rasterarea);
    gint xres = priv->field ? priv->field->xres : 1;
    gint yres = priv->field ? priv->field->yres : 1;
    gdouble hoffset = priv->hadjustment
                      ? gtk_adjustment_get_value(priv->hadjustment)
                      : 0.0;
    gdouble voffset = priv->vadjustment
                      ? gtk_adjustment_get_value(priv->vadjustment)
                      : 0.0;

    *irect = (cairo_rectangle_int_t){ 0, 0, width, height };

    if (priv->zoom) {
        gdouble xscale = (gdouble)full_width/xres;
        gdouble yscale = (gdouble)full_height/yres;

        frect->x = hoffset/xscale;
        frect->width = width/xscale;
        frect->y = voffset/yscale;
        frect->height = height/yscale;
        // If we got outside the data first try to fix the adjustments and if
        // this does not help add padding.
        if (frect->x + frect->width > xres) {
            if (frect->x + frect->width - xres < 1e-6)
                frect->width = xres - frect->x;
            else if (frect->width <= xres)
                frect->x = xres - frect->width;
            else {
                irect->width = full_width;
                irect->x = (width - irect->width)/2;
                frect->width = xres;
                frect->x = 0.0;
            }
        }
        if (frect->y + frect->height > yres) {
            if (frect->y + frect->height - yres < 1e-6)
                frect->height = yres - frect->y;
            else if (frect->height <= yres)
                frect->y = yres - frect->height;
            else {
                irect->height = full_height;
                irect->y = (height - irect->height)/2;
                frect->height = yres;
                frect->y = 0.0;
            }
        }
    }
    else {
        gdouble xscale = (gdouble)width/full_width;
        gdouble yscale = (gdouble)height/full_height;
        gdouble q = ((gdouble)full_height/yres)/((gdouble)full_width/xres);

        *frect = (cairo_rectangle_t){ 0, 0, xres, yres };
        if (xscale <= yscale) {
            irect->height = gwy_round(xscale * yres * q);
            irect->height = CLAMP(irect->height, 1, height);
            irect->y = (height - irect->height)/2;
        }
        else {
            irect->width = gwy_round(yscale * xres);
            irect->width = CLAMP(irect->width, 1, width);
            irect->x = (width - irect->width)/2;
        }
    }

    g_assert(frect->x >= 0.0);
    g_assert(frect->y >= 0.0);
    g_assert(frect->x + frect->width <= xres);
    g_assert(frect->y + frect->height <= yres);

    update_matrices(rasterarea);
    priv->pos_and_size_valid = TRUE;

    if (priv->range_from_method == GWY_COLOR_RANGE_VISIBLE
        || priv->range_to_method == GWY_COLOR_RANGE_VISIBLE)
        priv->range_valid = FALSE;
}

// FIXME: Window coordinates are somewhat strange here.  Should we take pixel
// centres?
static void
update_matrices(GwyRasterArea *rasterarea)
{
    RasterArea *priv = rasterarea->priv;
    GwyField *field = priv->field;
    if (!field) {
        cairo_matrix_init_identity(&priv->window_to_field_matrix);
        cairo_matrix_init_identity(&priv->window_to_coords_matrix);
        cairo_matrix_init_identity(&priv->field_to_window_matrix);
        cairo_matrix_init_identity(&priv->coords_to_window_matrix);
        return;
    }

    cairo_rectangle_int_t *irect = &priv->image_rectangle;
    cairo_rectangle_t *frect = &priv->field_rectangle;
    cairo_matrix_t *matrix;

    matrix = &priv->window_to_field_matrix;
    cairo_matrix_init_identity(matrix);
    cairo_matrix_translate(matrix, frect->x, frect->y);
    if (priv->zoom) {
        cairo_matrix_scale(matrix, 1.0/priv->zoom, 1.0/priv->zoom);
        if (priv->real_aspect_ratio)
            cairo_matrix_scale(matrix, 1.0, 1.0/priv->field_aspect_ratio);
    }
    else
        cairo_matrix_scale(matrix,
                           frect->width/irect->width,
                           frect->height/irect->height);
    cairo_matrix_translate(matrix, -irect->x, -irect->y);

    matrix = &priv->field_to_window_matrix;
    cairo_matrix_init_identity(matrix);
    cairo_matrix_translate(matrix, irect->x, irect->y);
    if (priv->zoom) {
        cairo_matrix_scale(matrix, priv->zoom, priv->zoom);
        if (priv->real_aspect_ratio)
            cairo_matrix_scale(matrix, 1.0, priv->field_aspect_ratio);
    }
    else
        cairo_matrix_scale(matrix,
                           irect->width/frect->width,
                           irect->height/frect->height);
    cairo_matrix_translate(matrix, -frect->x, -frect->y);

    matrix = &priv->window_to_coords_matrix;
    cairo_matrix_init_identity(matrix);
    cairo_matrix_translate(matrix, field->xoff, field->yoff);
    cairo_matrix_scale(matrix, gwy_field_dx(field), gwy_field_dy(field));
    cairo_matrix_multiply(matrix, &priv->window_to_field_matrix, matrix);

    matrix = &priv->coords_to_window_matrix;
    cairo_matrix_init_identity(matrix);
    cairo_matrix_scale(matrix,
                       1.0/gwy_field_dx(field), 1.0/gwy_field_dy(field));
    cairo_matrix_translate(matrix, -field->xoff, -field->yoff);
    cairo_matrix_multiply(matrix, matrix, &priv->field_to_window_matrix);

    set_shapes_transforms(rasterarea);
}

static void
ensure_range(GwyRasterArea *rasterarea)
{
    RasterArea *priv = rasterarea->priv;

    if (priv->range_valid)
        return;

    if (!priv->field) {
        gwy_clear1(priv->range);
        priv->range_valid = TRUE;
        g_object_notify_by_pspec(G_OBJECT(rasterarea), properties[PROP_RANGE]);
        return;
    }

    g_return_if_fail(priv->pos_and_size_valid);
    const cairo_rectangle_t *frect = &priv->field_rectangle;
    GwyFieldPart fpart = {
        (guint)floor(frect->x),
        (guint)floor(frect->y),
        (guint)ceil(frect->x + frect->width) - (guint)floor(frect->x),
        (guint)ceil(frect->y + frect->height) - (guint)floor(frect->y),
    };
    priv->range = priv->user_range;
    gwy_field_find_color_range(priv->field, &fpart, priv->mask,
                               priv->range_from_method, priv->range_to_method,
                               &priv->range);
    priv->range_valid = TRUE;
    g_object_notify_by_pspec(G_OBJECT(rasterarea), properties[PROP_RANGE]);
}

static void
ensure_field_surface(GwyRasterArea *rasterarea)
{
    RasterArea *priv = rasterarea->priv;
    cairo_rectangle_int_t *irect = &priv->image_rectangle;
    cairo_rectangle_t *frect = &priv->field_rectangle;

    if (priv->field_surface_valid)
        return;

    GwyField *field = priv->field;
    g_return_if_fail(field);
    g_return_if_fail(priv->range_valid);

    cairo_surface_t *surface = priv->field_surface;
    if (!surface
        || cairo_image_surface_get_width(surface) != irect->width
        || cairo_image_surface_get_height(surface) != irect->height) {
        destroy_field_surface(rasterarea);
        guint stride = cairo_format_stride_for_width(CAIRO_FORMAT_RGB24,
                                                     irect->width);
        guchar *data = g_new(guchar, stride*irect->height);
        surface = cairo_image_surface_create_for_data(data, CAIRO_FORMAT_RGB24,
                                                      irect->width,
                                                      irect->height,
                                                      stride);
        priv->field_surface = surface;
    }

    GwyGradient *gradient = (priv->gradient
                             ? priv->gradient
                             : gwy_gradients_get(NULL));

    gwy_field_render_cairo(field, surface, gradient, frect, &priv->range);
    priv->field_surface_valid = TRUE;
}

static void
ensure_mask_surface(GwyRasterArea *rasterarea)
{
    RasterArea *priv = rasterarea->priv;
    cairo_rectangle_int_t *irect = &priv->image_rectangle;
    cairo_rectangle_t *frect = &priv->field_rectangle;

    if (priv->mask_surface_valid)
        return;

    GwyField *field = priv->field;
    g_return_if_fail(field);

    GwyMaskField *mask = priv->mask;

    if (!mask)
        return;

    if (mask->xres != field->xres || mask->yres != field->yres) {
        g_warning("Mask dimensions %ux%u differ from field dimensions %ux%u.",
                  mask->xres, mask->yres, field->xres, field->yres);
        destroy_mask_surface(rasterarea);
        return;
    }

    cairo_surface_t *surface = priv->mask_surface;
    if (!surface
        || cairo_image_surface_get_width(surface) != irect->width
        || cairo_image_surface_get_height(surface) != irect->height) {
        destroy_mask_surface(rasterarea);
        guint stride = cairo_format_stride_for_width(CAIRO_FORMAT_A8,
                                                     irect->width);
        guchar *data = g_new(guchar, stride*irect->height);
        surface = cairo_image_surface_create_for_data(data, CAIRO_FORMAT_A8,
                                                      irect->width,
                                                      irect->height,
                                                      stride);
        priv->mask_surface = surface;
    }

    gwy_mask_field_render_cairo(mask, surface, frect);
    priv->mask_surface_valid = TRUE;
}

static void
draw_grain_number(GwyRasterArea *rasterarea,
                  cairo_t *cr, PangoLayout *layout,
                  guint gno, const GwyXY *pos)
{
    gchar grain_label[16];
    gint w, h;

    snprintf(grain_label, sizeof(grain_label), "%u", gno);
    pango_layout_set_text(layout, grain_label, -1);
    pango_layout_get_size(layout, &w, &h);

    GwyXY windowxy = *pos;
    cairo_matrix_transform_point(&rasterarea->priv->field_to_window_matrix,
                                 &windowxy.x, &windowxy.y);
    cairo_move_to(cr,
                  windowxy.x - 0.5*w/PANGO_SCALE,
                  windowxy.y - 0.5*h/PANGO_SCALE);
    pango_cairo_show_layout(cr, layout);
}

static void
draw_grain_numbers(GwyRasterArea *rasterarea,
                   cairo_t *cr)
{
    RasterArea *priv = rasterarea->priv;
    if (!priv->number_grains)
        return;

    ensure_layout(rasterarea);
    guint ngrains = gwy_mask_field_n_grains(priv->mask);
    const GwyXY *positions = gwy_mask_field_grain_positions(priv->mask);

    const GwyRGBA *color = &priv->grain_number_color;
    cairo_save(cr);
    gwy_cairo_set_source_rgba(cr, color);
    for (guint gno = 1; gno <= ngrains; gno++) {
        if (gno != priv->active_grain)
            draw_grain_number(rasterarea, cr, priv->layout, gno,
                              positions + gno);
    }
    if (priv->active_grain) {
        guint gno = priv->active_grain;
        const GwyRGBA *mcolor = &priv->mask_color;
        // FIXME
        GwyRGBA acolor = {
            .r = 1.4*color->r - 0.4*mcolor->r,
            .g = 1.4*color->g - 0.4*mcolor->g,
            .b = 1.4*color->b - 0.4*mcolor->b,
            .a = color->a,
        };
        gwy_rgba_fix(&acolor);
        gwy_cairo_set_source_rgba(cr, &acolor);
        draw_grain_number(rasterarea, cr, priv->layout, gno, positions + gno);
    }
    cairo_restore(cr);
}

static void
ensure_layout(GwyRasterArea *rasterarea)
{
    RasterArea *priv = rasterarea->priv;
    if (priv->layout)
        return;

    priv->layout = gtk_widget_create_pango_layout(GTK_WIDGET(rasterarea), NULL);
    pango_layout_set_alignment(priv->layout, PANGO_ALIGN_CENTER);

    PangoAttrList *attrlist = pango_attr_list_new();
    PangoAttribute *attr = pango_attr_scale_new(PANGO_SCALE_SMALL);
    pango_attr_list_insert(attrlist, attr);
    pango_layout_set_attributes(priv->layout, attrlist);
    pango_attr_list_unref(attrlist);
}

static void
draw_mask(GwyRasterArea *rasterarea,
          cairo_t *cr)
{
    RasterArea *priv = rasterarea->priv;

    if (!priv->mask)
        return;

    ensure_mask_surface(rasterarea);
    if (!priv->mask_surface)
        return;

    cairo_save(cr);
    gwy_cairo_set_source_rgba(cr, &priv->mask_color);
    cairo_mask_surface(cr, priv->mask_surface,
                       priv->image_rectangle.x, priv->image_rectangle.y);
    cairo_restore(cr);
    draw_grain_numbers(rasterarea, cr);
}

static gboolean
gwy_raster_area_draw(GtkWidget *widget,
                     cairo_t *cr)
{
    GwyRasterArea *rasterarea = GWY_RASTER_AREA(widget);
    RasterArea *priv = rasterarea->priv;

    if (!priv->field)
        return FALSE;

    // Postpone emission of notify::range until after we finished drawing.
    g_object_freeze_notify(G_OBJECT(rasterarea));

    if (!priv->pos_and_size_valid)
        calculate_position_and_size(rasterarea);

    //GtkStyleContext *context = gtk_widget_get_style_context(widget);
    ensure_range(rasterarea);
    ensure_field_surface(rasterarea);

    cairo_save(cr);
    cairo_set_source_surface(cr, priv->field_surface,
                             priv->image_rectangle.x, priv->image_rectangle.y);
    cairo_paint(cr);
    cairo_restore(cr);

    draw_mask(rasterarea, cr);

    if (priv->shapes) {
        cairo_save(cr);
        gwy_shapes_draw(priv->shapes, cr);
        cairo_restore(cr);
    }

    g_object_thaw_notify(G_OBJECT(rasterarea));

    return FALSE;
}

static void
gwy_raster_area_style_updated(GtkWidget *widget)
{
    RasterArea *priv = GWY_RASTER_AREA(widget)->priv;
    if (priv->layout)
        pango_layout_context_changed(priv->layout);

    GTK_WIDGET_CLASS(gwy_raster_area_parent_class)->style_updated(widget);
}

static void
destroy_field_surface(GwyRasterArea *rasterarea)
{
    RasterArea *priv = rasterarea->priv;
    if (priv->field_surface) {
        guchar *data = cairo_image_surface_get_data(priv->field_surface);
        cairo_surface_destroy(priv->field_surface);
        priv->field_surface = NULL;
        g_free(data);
    }
    priv->field_surface_valid = FALSE;
}

static void
destroy_mask_surface(GwyRasterArea *rasterarea)
{
    RasterArea *priv = rasterarea->priv;
    if (priv->mask_surface) {
        guchar *data = cairo_image_surface_get_data(priv->mask_surface);
        cairo_surface_destroy(priv->mask_surface);
        priv->mask_surface = NULL;
        g_free(data);
    }
    priv->mask_surface_valid = FALSE;
}

static gboolean
set_field(GwyRasterArea *rasterarea,
          GwyField *field)
{
    RasterArea *priv = rasterarea->priv;
    if (!gwy_set_member_object(rasterarea, field, GWY_TYPE_FIELD,
                               &priv->field,
                               "notify", &field_notify,
                               &priv->field_notify_id,
                               G_CONNECT_SWAPPED,
                               "data-changed", &field_data_changed,
                               &priv->field_data_changed_id,
                               G_CONNECT_SWAPPED,
                               NULL))
        return FALSE;

    priv->field_aspect_ratio = field
                               ? gwy_field_dy(field)/gwy_field_dx(field)
                               : 1.0;

    priv->field_surface_valid = FALSE;
    priv->pos_and_size_valid = FALSE;
    // TODO: Queue either resize or draw, depending on the dimensions.
    gtk_widget_queue_resize(GTK_WIDGET(rasterarea));
    gtk_widget_queue_draw(GTK_WIDGET(rasterarea));
    return TRUE;
}

static gboolean
set_mask(GwyRasterArea *rasterarea,
         GwyMaskField *mask)
{
    RasterArea *priv = rasterarea->priv;
    if (!gwy_set_member_object(rasterarea, mask, GWY_TYPE_MASK_FIELD,
                               &priv->mask,
                               "notify", &mask_notify,
                               &priv->mask_notify_id,
                               G_CONNECT_SWAPPED,
                               "data-changed", &mask_data_changed,
                               &priv->mask_data_changed_id,
                               G_CONNECT_SWAPPED,
                               NULL))
        return FALSE;

    priv->mask_surface_valid = FALSE;
    priv->active_grain = 0;
    if (priv->range_from_method == GWY_COLOR_RANGE_MASKED
        || priv->range_from_method == GWY_COLOR_RANGE_UNMASKED
        || priv->range_to_method == GWY_COLOR_RANGE_MASKED
        || priv->range_to_method == GWY_COLOR_RANGE_UNMASKED)
        priv->range_valid = FALSE;
    gtk_widget_queue_draw(GTK_WIDGET(rasterarea));
    return TRUE;
}

static gboolean
set_gradient(GwyRasterArea *rasterarea,
             GwyGradient *gradient)
{
    RasterArea *priv = rasterarea->priv;
    if (!gwy_set_member_object(rasterarea, gradient, GWY_TYPE_GRADIENT,
                               &priv->gradient,
                               "data-changed", &gradient_data_changed,
                               &priv->gradient_data_changed_id,
                               G_CONNECT_SWAPPED,
                               NULL))
        return FALSE;

    priv->field_surface_valid = FALSE;
    gtk_widget_queue_draw(GTK_WIDGET(rasterarea));
    return TRUE;
}

static gboolean
set_shapes(GwyRasterArea *rasterarea,
           GwyShapes *shapes)
{
    RasterArea *priv = rasterarea->priv;

    if (!gwy_set_member_object(rasterarea, shapes, GWY_TYPE_SHAPES,
                               &priv->shapes,
                               "updated", &shapes_updated,
                               &priv->shapes_updated_id,
                               G_CONNECT_SWAPPED,
                               NULL))
        return FALSE;

    set_shapes_transforms(rasterarea);

    GtkWidget *widget = GTK_WIDGET(rasterarea);
    if (gtk_widget_is_drawable(widget))
        gtk_widget_queue_draw(widget);

    return TRUE;
}

// XXX: This must not be done in draw() because it would cause another draw.
// This means sizes must be recalculated upon resize but adjustment and stuff
// *before* draw, immediately.
static void
set_shapes_transforms(GwyRasterArea *rasterarea)
{
    RasterArea *priv = rasterarea->priv;
    GwyShapes *shapes = priv->shapes;
    GwyField *field = priv->field;
    if (!shapes)
        return;

    gwy_shapes_set_coords_matrices(shapes,
                                   &priv->coords_to_window_matrix,
                                   &priv->window_to_coords_matrix);
    gwy_shapes_set_pixel_matrices(shapes,
                                  &priv->field_to_window_matrix,
                                  &priv->window_to_field_matrix);

    // FIXME: Moving shapes outside the viewport should invoke scrolling!
    cairo_rectangle_t bbox = {
        field->xoff, field->yoff, field->xreal, field->yreal,
    };
    gwy_shapes_set_bounding_box(shapes, &bbox);
}

static gboolean
set_mask_color(GwyRasterArea *rasterarea,
               const GwyRGBA *color)
{
    RasterArea *priv = rasterarea->priv;
    if (color->r == priv->mask_color.r
        && color->g == priv->mask_color.g
        && color->b == priv->mask_color.b
        && color->a == priv->mask_color.a)
        return FALSE;

    priv->mask_color = *color;
    if (priv->mask)
        gtk_widget_queue_draw(GTK_WIDGET(rasterarea));
    return TRUE;
}

static gboolean
set_grain_number_color(GwyRasterArea *rasterarea,
                       const GwyRGBA *color)
{
    RasterArea *priv = rasterarea->priv;
    if (color->r == priv->mask_color.r
        && color->g == priv->mask_color.g
        && color->b == priv->mask_color.b
        && color->a == priv->mask_color.a)
        return FALSE;

    priv->grain_number_color = *color;
    if (priv->mask && priv->number_grains)
        gtk_widget_queue_draw(GTK_WIDGET(rasterarea));
    return TRUE;
}

static gboolean
set_scrollable(GwyRasterArea *rasterarea,
               gboolean setting)
{
    RasterArea *priv = rasterarea->priv;
    if (!setting == !priv->scrollable)
        return FALSE;

    priv->scrollable = setting;
    return TRUE;
}

static gboolean
set_zoomable(GwyRasterArea *rasterarea,
             gboolean setting)
{
    RasterArea *priv = rasterarea->priv;
    if (!setting == !priv->zoomable)
        return FALSE;

    priv->zoomable = setting;
    return TRUE;
}

static gboolean
set_range_from_method(GwyRasterArea *rasterarea,
                      GwyColorRangeType method)
{
    RasterArea *priv = rasterarea->priv;
    if (method == priv->range_from_method)
        return FALSE;

    priv->range_from_method = method;
    priv->field_surface_valid = FALSE;
    priv->range_valid = FALSE;
    gtk_widget_queue_draw(GTK_WIDGET(rasterarea));
    return TRUE;
}

static gboolean
set_range_to_method(GwyRasterArea *rasterarea,
                    GwyColorRangeType method)
{
    RasterArea *priv = rasterarea->priv;
    if (method == priv->range_to_method)
        return FALSE;

    priv->range_to_method = method;
    priv->field_surface_valid = FALSE;
    priv->range_valid = FALSE;
    gtk_widget_queue_draw(GTK_WIDGET(rasterarea));
    return TRUE;
}

static gboolean
set_user_range(GwyRasterArea *rasterarea,
               const GwyRange *range)
{
    RasterArea *priv = rasterarea->priv;
    if (gwy_equal(range, &priv->user_range))
        return FALSE;

    priv->user_range = *range;
    if (priv->range_from_method != GWY_COLOR_RANGE_USER
        && priv->range_to_method != GWY_COLOR_RANGE_USER)
        return TRUE;

    priv->field_surface_valid = FALSE;
    priv->range_valid = FALSE;
    gtk_widget_queue_draw(GTK_WIDGET(rasterarea));
    return TRUE;
}

static void
field_notify(GwyRasterArea *rasterarea,
             GParamSpec *pspec,
             GwyField *field)
{
    RasterArea *priv = rasterarea->priv;
    priv->field_aspect_ratio = gwy_field_dy(field)/gwy_field_dx(field);

    if (gwy_strequal(pspec->name, "x-res")
        || gwy_strequal(pspec->name, "y-res")) {
        priv->pos_and_size_valid = FALSE;
        gtk_widget_queue_resize(GTK_WIDGET(rasterarea));
        gtk_widget_queue_draw(GTK_WIDGET(rasterarea));
    }
    if (priv->real_aspect_ratio
             && (gwy_strequal(pspec->name, "x-real")
                 || gwy_strequal(pspec->name, "y-real"))) {
        priv->pos_and_size_valid = FALSE;
        gtk_widget_queue_resize(GTK_WIDGET(rasterarea));
        gtk_widget_queue_draw(GTK_WIDGET(rasterarea));
    }
}

static void
mask_notify(GwyRasterArea *rasterarea,
            GParamSpec *pspec,
            GwyMaskField *mask)
{
    RasterArea *priv = rasterarea->priv;

    if (gwy_strequal(pspec->name, "x-res")
        || gwy_strequal(pspec->name, "y-res")) {
        priv->mask_surface_valid = FALSE;
        gtk_widget_queue_draw(GTK_WIDGET(rasterarea));
    }
}

static void
field_data_changed(GwyRasterArea *rasterarea,
                   GwyFieldPart *fpart,
                   G_GNUC_UNUSED GwyField *field)
{
    RasterArea *priv = rasterarea->priv;

    gboolean fixed_range = (priv->range_from_method == GWY_COLOR_RANGE_USER
                            && priv->range_to_method == GWY_COLOR_RANGE_USER);

    priv->field_surface_valid = FALSE;
    if (!fixed_range)
        priv->range_valid = FALSE;

    GtkWidget *widget = GTK_WIDGET(rasterarea);
    if (!fixed_range || !fpart) {
        gtk_widget_queue_draw(widget);
        return;
    }

    queue_draw_field_part(rasterarea, fpart);
}

static void
mask_data_changed(GwyRasterArea *rasterarea,
                  GwyFieldPart *fpart,
                  G_GNUC_UNUSED GwyMaskField *mask)
{
    RasterArea *priv = rasterarea->priv;
    GtkWidget *widget = GTK_WIDGET(rasterarea);

    priv->mask_surface_valid = FALSE;
    if (priv->range_from_method == GWY_COLOR_RANGE_MASKED
        || priv->range_from_method == GWY_COLOR_RANGE_UNMASKED
        || priv->range_to_method == GWY_COLOR_RANGE_MASKED
        || priv->range_to_method == GWY_COLOR_RANGE_UNMASKED) {
        priv->field_surface_valid = FALSE;
        priv->range_valid = FALSE;
        gtk_widget_queue_draw(GTK_WIDGET(rasterarea));
        return;
    }

    if (!fpart) {
        gtk_widget_queue_draw(GTK_WIDGET(rasterarea));
        return;
    }

    // XXX: We assume mask::data-changed is emitted after we recalculate sizes.
    // If mask dimensions differ from field dimensions, weird things abound.
    queue_draw_field_part(rasterarea, fpart);
}

static void
queue_draw_field_part(GwyRasterArea *rasterarea,
                      const GwyFieldPart *fpart)
{
    GtkWidget *widget = GTK_WIDGET(rasterarea);
    RasterArea *priv = rasterarea->priv;
    gdouble xf = fpart->col, yf = fpart->row,
            xt = fpart->col + fpart->width, yt = fpart->row + fpart->height;
    cairo_matrix_transform_point(&priv->field_to_window_matrix, &xf, &yf);
    cairo_matrix_transform_point(&priv->field_to_window_matrix, &xt, &yt);
    const cairo_rectangle_int_t *irect = &priv->image_rectangle;
    gint ixf = CLAMP((gint)floor(xf), irect->x, irect->x + irect->width);
    gint iyf = CLAMP((gint)floor(yf), irect->y, irect->y + irect->height);
    gint ixt = CLAMP((gint)ceil(xt), irect->x, irect->x + irect->width);
    gint iyt = CLAMP((gint)ceil(yt), irect->y, irect->y + irect->height);
    gtk_widget_queue_draw_area(widget, ixf, iyf, ixt+1 - ixf, iyt+1 - iyf);
}

static void
gradient_data_changed(GwyRasterArea *rasterarea,
                      G_GNUC_UNUSED GwyGradient *gradient)
{
    rasterarea->priv->field_surface_valid = FALSE;
    gtk_widget_queue_draw(GTK_WIDGET(rasterarea));
}

static void
shapes_updated(GwyRasterArea *rasterarea,
               GwyShapes *shapes)
{
    RasterArea *priv = rasterarea->priv;
    if (!priv->scroll_timer_hid) {
        if (scroll_to_current_point(rasterarea, shapes)) {
            priv->scroll_timer_hid = g_timeout_add(50, &enable_scrolling_again,
                                                   rasterarea);
            return;
        }
    }
    gtk_widget_queue_draw(GTK_WIDGET(rasterarea));
}

static gboolean
enable_scrolling_again(gpointer user_data)
{
    GwyRasterArea *rasterarea = GWY_RASTER_AREA(user_data);
    RasterArea *priv = rasterarea->priv;
    if (priv->shapes && scroll_to_current_point(rasterarea, priv->shapes))
        return TRUE;
    priv->scroll_timer_hid = 0;
    return FALSE;
}

static gboolean
scroll_to_current_point(GwyRasterArea *rasterarea,
                        GwyShapes *shapes)
{
    RasterArea *priv = rasterarea->priv;
    gboolean scrolling = FALSE;
    GwyXY xy;

    if (!gwy_shapes_get_current_point(shapes, &xy))
        return scrolling;

    cairo_matrix_transform_point(&shapes->coords_to_view, &xy.x, &xy.y);
    const cairo_rectangle_int_t *irect = &priv->image_rectangle;

    if (xy.x < irect->x)
        scrolling |= gwy_raster_area_scroll(rasterarea, GTK_SCROLL_STEP_LEFT);
    else if (xy.x > irect->x + irect->width)
        scrolling |= gwy_raster_area_scroll(rasterarea, GTK_SCROLL_STEP_RIGHT);

    if (xy.y < irect->y)
        scrolling |= gwy_raster_area_scroll(rasterarea, GTK_SCROLL_STEP_UP);
    else if (xy.y > irect->y + irect->height)
        scrolling |= gwy_raster_area_scroll(rasterarea, GTK_SCROLL_STEP_DOWN);

    return scrolling;
}

static gboolean
set_hadjustment(GwyRasterArea *rasterarea,
                GtkAdjustment *adjustment)
{
    RasterArea *priv = rasterarea->priv;
    if (!gwy_set_member_object(rasterarea, adjustment, GTK_TYPE_ADJUSTMENT,
                               &priv->hadjustment,
                               "value-changed", &adjustment_value_changed,
                               &priv->hadjustment_value_changed_id,
                               G_CONNECT_SWAPPED,
                               NULL))
        return FALSE;

    priv->field_surface_valid = FALSE;
    priv->mask_surface_valid = FALSE;
    freeze_adjustments_notify(rasterarea, TRUE, FALSE);
    set_hadjustment_values(rasterarea);
    calculate_position_and_size(rasterarea);
    thaw_adjustments_notify(rasterarea, TRUE, FALSE);
    return TRUE;
}

static gboolean
set_vadjustment(GwyRasterArea *rasterarea,
                GtkAdjustment *adjustment)
{
    RasterArea *priv = rasterarea->priv;
    if (!gwy_set_member_object(rasterarea, adjustment, GTK_TYPE_ADJUSTMENT,
                               &priv->vadjustment,
                               "value-changed", &adjustment_value_changed,
                               &priv->vadjustment_value_changed_id,
                               G_CONNECT_SWAPPED,
                               NULL))
        return FALSE;

    priv->field_surface_valid = FALSE;
    priv->mask_surface_valid = FALSE;
    freeze_adjustments_notify(rasterarea, FALSE, TRUE);
    set_vadjustment_values(rasterarea);
    calculate_position_and_size(rasterarea);
    thaw_adjustments_notify(rasterarea, FALSE, TRUE);
    return TRUE;
}

static void
set_adjustment_values(GtkAdjustment *adjustment,
                      guint size,
                      guint full_size)
{
    if (!adjustment)
        return;

    gdouble old_value = gtk_adjustment_get_value(adjustment);
    gdouble new_upper = MAX(size, full_size);

    g_object_set(adjustment,
                 "lower", 0.0,
                 "upper", new_upper,
                 "page-size", 1.0*size,
                 "step-increment", 0.1*size,
                 "page-increment", 0.9*size,
                 NULL);

    gdouble new_value = CLAMP(old_value, 0, new_upper - size);
    if (new_value != old_value)
        gtk_adjustment_set_value(adjustment, new_value);
}

static void
set_hadjustment_values(GwyRasterArea *rasterarea)
{
    RasterArea *priv = rasterarea->priv;
    GtkAllocation allocation;
    gtk_widget_get_allocation(GTK_WIDGET(rasterarea), &allocation);
    set_adjustment_values(priv->hadjustment,
                          allocation.width, calculate_full_width(rasterarea));
}

static void
set_vadjustment_values(GwyRasterArea *rasterarea)
{
    RasterArea *priv = rasterarea->priv;
    GtkAllocation allocation;
    gtk_widget_get_allocation(GTK_WIDGET(rasterarea), &allocation);
    set_adjustment_values(priv->vadjustment,
                          allocation.height, calculate_full_height(rasterarea));
}

static void
adjustment_value_changed(GwyRasterArea *rasterarea)
{
    rasterarea->priv->field_surface_valid = FALSE;
    rasterarea->priv->mask_surface_valid = FALSE;
    calculate_position_and_size(rasterarea);
    gtk_widget_queue_draw(GTK_WIDGET(rasterarea));
}

// When we set adjustments we need to emit notifications after
// calculate_position_and_size() because whoever watches the adjustments will
// likely query the dimensions.
static void
freeze_adjustments_notify(GwyRasterArea *rasterarea,
                          gboolean horizontal,
                          gboolean vertical)
{
    RasterArea *priv = rasterarea->priv;
    if (horizontal && priv->hadjustment)
        g_object_freeze_notify(G_OBJECT(priv->hadjustment));
    if (vertical && priv->vadjustment)
        g_object_freeze_notify(G_OBJECT(priv->vadjustment));
}

static void
thaw_adjustments_notify(GwyRasterArea *rasterarea,
                        gboolean horizontal,
                        gboolean vertical)
{
    RasterArea *priv = rasterarea->priv;
    if (horizontal && priv->hadjustment)
        g_object_thaw_notify(G_OBJECT(priv->hadjustment));
    if (vertical && priv->vadjustment)
        g_object_thaw_notify(G_OBJECT(priv->vadjustment));
}

static gboolean
set_zoom(GwyRasterArea *rasterarea,
         gdouble zoom)
{
    RasterArea *priv = rasterarea->priv;
    if (zoom == priv->zoom)
        return FALSE;

    GtkWidget *widget = GTK_WIDGET(rasterarea);
    if (!priv->hadjustment || !priv->vadjustment || !priv->field) {
        priv->zoom = zoom;
        gtk_widget_queue_resize(widget);
        return TRUE;
    }

    guint xres = priv->field->xres, yres = priv->field->yres;
    gdouble x = 0.5*gtk_widget_get_allocated_width(widget);
    gdouble y = 0.5*gtk_widget_get_allocated_height(widget);
    cairo_matrix_transform_point(&priv->window_to_field_matrix, &x, &y);
    // In case of zooming in we want to preserve the centre.  In case of
    // zooming out we want to avoid showing borders.  The latter should be
    // ensured by calculate_position_and_size() so ensure the former.
    priv->zoom = zoom;
    calculate_position_and_size(rasterarea);
    set_hadjustment_values(rasterarea);
    set_vadjustment_values(rasterarea);
    if (priv->field_rectangle.width < xres) {
        x *= gtk_adjustment_get_upper(priv->hadjustment)/xres;
        x -= 0.5*gtk_adjustment_get_page_size(priv->hadjustment);
        gtk_adjustment_set_value(priv->hadjustment, x);
    }
    if (priv->field_rectangle.height < yres) {
        y *= gtk_adjustment_get_upper(priv->vadjustment)/yres;
        y -= 0.5*gtk_adjustment_get_page_size(priv->vadjustment);
        gtk_adjustment_set_value(priv->vadjustment, y);
    }

    priv->field_surface_valid = FALSE;
    priv->mask_surface_valid = FALSE;
    gtk_widget_queue_draw(widget);

    return TRUE;
}

static gboolean
set_real_aspect_ratio(GwyRasterArea *rasterarea,
                      gboolean setting)
{
    RasterArea *priv = rasterarea->priv;
    if (!setting == !priv->real_aspect_ratio)
        return FALSE;

    priv->real_aspect_ratio = setting;
    // Do not trigger a resize if the aspect ratios are the same.
    if (priv->field_aspect_ratio == 1.0)
        return TRUE;

    priv->pos_and_size_valid = FALSE;
    gtk_widget_queue_resize(GTK_WIDGET(rasterarea));
    gtk_widget_queue_draw(GTK_WIDGET(rasterarea));
    return TRUE;
}

static guint
calculate_full_width(const GwyRasterArea *rasterarea)
{
    const RasterArea *priv = rasterarea->priv;
    if (!priv->field)
        return 1;

    gdouble xzoom = priv->zoom ? priv->zoom : 1.0;
    return gwy_round(fmax(xzoom * priv->field->xres, 1.0));
}

static guint
calculate_full_height(const GwyRasterArea *rasterarea)
{
    const RasterArea *priv = rasterarea->priv;
    if (!priv->field)
        return 1;

    gdouble yzoom = priv->zoom ? priv->zoom : 1.0;
    if (priv->real_aspect_ratio)
        yzoom *= priv->field_aspect_ratio;
    return gwy_round(fmax(yzoom * priv->field->yres, 1.0));
}

static gboolean
set_number_grains(GwyRasterArea *rasterarea,
                  gboolean setting)
{
    RasterArea *priv = rasterarea->priv;
    if (!setting == !priv->number_grains)
        return FALSE;

    priv->number_grains = setting;
    // Do not trigger a redraw if there is no mask.
    if (!priv->mask)
        return TRUE;

    gtk_widget_queue_draw(GTK_WIDGET(rasterarea));
    return TRUE;
}

/**
 * SECTION: raster-area
 * @title: GwyRasterArea
 * @short_description: Display fields using false colour maps
 **/

/**
 * GwyRasterArea:
 *
 * Widget for raster display of two-dimensional data using false colour maps.
 *
 * The #GwyRasterArea struct contains private data only and should be accessed
 * using the functions below.
 **/

/**
 * GwyRasterAreaClass:
 *
 * Class of two-dimensional raster areas.
 **/

/**
 * GwyZoomType:
 * @GWY_ZOOM_1_1: Zoom 1:1, also called 100%, display the object in native
 *                size.
 * @GWY_ZOOM_IN: Zoom in, make the displayed object seem larger.
 * @GWY_ZOOM_OUT: Zoom out, make the displayed object seem smaller.
 * @GWY_ZOOM_FIT: Set zoom so that the object fits fully within the alloted
 *                area.
 *
 * Type of zoom.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
