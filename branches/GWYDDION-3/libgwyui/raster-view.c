/*
 *  $Id$
 *  Copyright (C) 2011 David Nečas (Yeti).
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
#include "libgwy/strfuncs.h"
#include "libgwy/object-utils.h"
#include "libgwy/field-statistics.h"
#include "libgwyui/field-render.h"
#include "libgwyui/raster-view.h"

enum {
    // Own.
    PROP_0,
    PROP_FIELD,
    PROP_GRADIENT,
    PROP_ZOOM,
    PROP_REAL_ASPECT_RATIO,
    N_PROPS,
    // Overriden.
    PROP_HADJUSTMENT = N_PROPS,
    PROP_VADJUSTMENT,
    PROP_HSCROLL_POLICY,
    PROP_VSCROLL_POLICY,
    N_TOTAL_PROPS
};

struct _GwyRasterViewPrivate {
    GtkAdjustment *hadjustment;
    gulong hadjustment_value_changed_id;

    GtkAdjustment *vadjustment;
    gulong vadjustment_value_changed_id;

    guint hscroll_policy;
    guint vscroll_policy;

    gdouble zoom;
    gboolean real_aspect_ratio;

    GwyField *field;
    gulong field_notify_id;
    gulong field_data_changed_id;
    gdouble field_aspect_ratio;

    GwyGradient *gradient;
    gulong gradient_data_changed_id;

    cairo_surface_t *surface;
    gboolean image_valid;
};

typedef struct _GwyRasterViewPrivate RasterView;

static void     gwy_raster_view_finalize            (GObject *object);
static void     gwy_raster_view_dispose             (GObject *object);
static void     gwy_raster_view_set_property        (GObject *object,
                                                     guint prop_id,
                                                     const GValue *value,
                                                     GParamSpec *pspec);
static void     gwy_raster_view_get_property        (GObject *object,
                                                     guint prop_id,
                                                     GValue *value,
                                                     GParamSpec *pspec);
static void     gwy_raster_view_get_preferred_width (GtkWidget *widget,
                                                     gint *minimum,
                                                     gint *natural);
static void     gwy_raster_view_get_preferred_height(GtkWidget *widget,
                                                     gint *minimum,
                                                     gint *natural);
static void     gwy_raster_view_size_allocate       (GtkWidget *widget,
                                                     GtkAllocation *allocation);
static gboolean gwy_raster_view_draw                (GtkWidget *widget,
                                                     cairo_t *cr);
static void     destroy_surface                     (GwyRasterView *rasterview);
static gboolean set_field                           (GwyRasterView *rasterview,
                                                     GwyField *field);
static gboolean set_gradient                        (GwyRasterView *rasterview,
                                                     GwyGradient *gradient);
static void     field_notify                        (GwyRasterView *rasterview,
                                                     GParamSpec *pspec,
                                                     GwyField *field);
static void     field_data_changed                  (GwyRasterView *rasterview,
                                                     GwyFieldPart *fpart,
                                                     GwyField *field);
static void     gradient_data_changed               (GwyRasterView *rasterview,
                                                     GwyGradient *gradient);
static gboolean set_hadjustment                     (GwyRasterView *rasterview,
                                                     GtkAdjustment *adjustment);
static gboolean set_vadjustment                     (GwyRasterView *rasterview,
                                                     GtkAdjustment *adjustment);
static void     set_hadjustment_values              (GwyRasterView *rasterview);
static void     set_vadjustment_values              (GwyRasterView *rasterview);
static void     adjustment_value_changed            (GwyRasterView *rasterview);
static gboolean set_zoom                            (GwyRasterView *rasterview,
                                                     gdouble zoom);
static gboolean set_real_aspect_ratio               (GwyRasterView *rasterview,
                                                     gboolean setting);
static guint calculate_full_width(GwyRasterView *rasterview);
static guint calculate_full_height(GwyRasterView *rasterview);

static GParamSpec *raster_view_pspecs[N_PROPS];

G_DEFINE_TYPE_WITH_CODE(GwyRasterView, gwy_raster_view, GTK_TYPE_WIDGET,
                        G_IMPLEMENT_INTERFACE(GTK_TYPE_SCROLLABLE, NULL));

static void
gwy_raster_view_class_init(GwyRasterViewClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

    g_type_class_add_private(klass, sizeof(RasterView));

    gobject_class->dispose = gwy_raster_view_dispose;
    gobject_class->finalize = gwy_raster_view_finalize;
    gobject_class->get_property = gwy_raster_view_get_property;
    gobject_class->set_property = gwy_raster_view_set_property;

    widget_class->get_preferred_width = gwy_raster_view_get_preferred_width;
    widget_class->get_preferred_height = gwy_raster_view_get_preferred_height;
    widget_class->size_allocate = gwy_raster_view_size_allocate;
    widget_class->draw = gwy_raster_view_draw;

    raster_view_pspecs[PROP_FIELD]
        = g_param_spec_object("field",
                              "Field",
                              "Two-dimensional field shown.",
                              GWY_TYPE_FIELD,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    raster_view_pspecs[PROP_GRADIENT]
        = g_param_spec_object("gradient",
                              "Gradient",
                              "Gradient used for visualisation.",
                              GWY_TYPE_GRADIENT,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    raster_view_pspecs[PROP_ZOOM]
        = g_param_spec_double("zoom",
                              "Zoom",
                              "Scaling of the field in the horizontal "
                              "direction.  Vertical scaling depends on "
                              "real-aspect-ratio.  Zoom of 0 means the widget "
                              "requests the minimum size 1×1 and scales to "
                              "whatever size is given to it.",
                              0.0, G_MAXDOUBLE, 1.0,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    raster_view_pspecs[PROP_REAL_ASPECT_RATIO]
        = g_param_spec_boolean("real-aspect-ratio",
                               "Real aspect ratio",
                               "Whether the real dimensions of the field "
                               "determine the sizes (as opposed to pixel "
                               "dimensions).",
                               FALSE,
                               G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    for (guint i = 1; i < N_PROPS; i++)
        g_object_class_install_property(gobject_class, i,
                                        raster_view_pspecs[i]);

    gwy_override_class_properties(gobject_class, raster_view_pspecs,
                                  "hadjustment", PROP_HADJUSTMENT,
                                  "vadjustment", PROP_VADJUSTMENT,
                                  "hscroll-policy", PROP_HSCROLL_POLICY,
                                  "vscroll-policy", PROP_VSCROLL_POLICY,
                                  NULL);
}

static void
gwy_raster_view_init(GwyRasterView *rasterview)
{
    rasterview->priv = G_TYPE_INSTANCE_GET_PRIVATE(rasterview,
                                                   GWY_TYPE_RASTER_VIEW,
                                                   RasterView);
    rasterview->priv->field_aspect_ratio = 1.0;
    gtk_widget_set_has_window(GTK_WIDGET(rasterview), FALSE);
}

static void
gwy_raster_view_finalize(GObject *object)
{
    G_OBJECT_CLASS(gwy_raster_view_parent_class)->finalize(object);
}

static void
gwy_raster_view_dispose(GObject *object)
{
    GwyRasterView *rasterview = GWY_RASTER_VIEW(object);

    set_field(rasterview, NULL);
    set_gradient(rasterview, NULL);
    set_hadjustment(rasterview, NULL);
    set_vadjustment(rasterview, NULL);
    destroy_surface(rasterview);

    G_OBJECT_CLASS(gwy_raster_view_parent_class)->dispose(object);
}

static void
gwy_raster_view_set_property(GObject *object,
                             guint prop_id,
                             const GValue *value,
                             GParamSpec *pspec)
{
    GwyRasterView *rasterview = GWY_RASTER_VIEW(object);

    switch (prop_id) {
        case PROP_FIELD:
        set_field(rasterview, g_value_get_object(value));
        break;

        case PROP_GRADIENT:
        set_gradient(rasterview, g_value_get_object(value));
        break;

        case PROP_ZOOM:
        set_zoom(rasterview, g_value_get_double(value));
        break;

        case PROP_REAL_ASPECT_RATIO:
        set_real_aspect_ratio(rasterview, g_value_get_boolean(value));
        break;

        case PROP_HADJUSTMENT:
        set_hadjustment(rasterview, (GtkAdjustment*)g_value_get_object(value));
        break;

        case PROP_VADJUSTMENT:
        set_vadjustment(rasterview, (GtkAdjustment*)g_value_get_object(value));
        break;

        case PROP_HSCROLL_POLICY:
        rasterview->priv->hscroll_policy = g_value_get_enum(value);
        gtk_widget_queue_resize(GTK_WIDGET(rasterview));
        break;

        case PROP_VSCROLL_POLICY:
        rasterview->priv->vscroll_policy = g_value_get_enum(value);
        gtk_widget_queue_resize(GTK_WIDGET(rasterview));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_raster_view_get_property(GObject *object,
                             guint prop_id,
                             GValue *value,
                             GParamSpec *pspec)
{
    RasterView *rasterview = GWY_RASTER_VIEW(object)->priv;

    switch (prop_id) {
        case PROP_FIELD:
        g_value_set_object(value, rasterview->field);
        break;

        case PROP_GRADIENT:
        g_value_set_object(value, rasterview->gradient);
        break;

        case PROP_ZOOM:
        g_value_set_double(value, rasterview->zoom);
        break;

        case PROP_REAL_ASPECT_RATIO:
        g_value_set_boolean(value, rasterview->real_aspect_ratio);
        break;

        case PROP_HADJUSTMENT:
        g_value_set_object(value, rasterview->hadjustment);
        break;

        case PROP_VADJUSTMENT:
        g_value_set_object(value, rasterview->vadjustment);
        break;

        case PROP_HSCROLL_POLICY:
        g_value_set_enum(value, rasterview->hscroll_policy);
        break;

        case PROP_VSCROLL_POLICY:
        g_value_set_enum(value, rasterview->vscroll_policy);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

/**
 * gwy_raster_view_new:
 *
 * Creates a new raster view.
 *
 * Returns: A new raster view.
 **/
GtkWidget*
gwy_raster_view_new(void)
{
    return g_object_newv(GWY_TYPE_RASTER_VIEW, 0, NULL);
}

/**
 * gwy_raster_view_set_field:
 * @rasterview: A raster view.
 * @field: (allow-none):
 *         A two-dimensional data field.
 *
 * Sets the field a raster view will display.
 **/
void
gwy_raster_view_set_field(GwyRasterView *rasterview,
                          GwyField *field)
{
    g_return_if_fail(GWY_IS_RASTER_VIEW(rasterview));
    if (!set_field(rasterview, field))
        return;

    g_object_notify_by_pspec(G_OBJECT(rasterview),
                             raster_view_pspecs[PROP_FIELD]);
    gtk_widget_queue_resize(GTK_WIDGET(rasterview));
}

/**
 * gwy_raster_view_get_field:
 * @rasterview: A raster view.
 *
 * Obtains the field a raster view displays.
 *
 * Returns: (transfer none):
 *          The field displayed by @rasterview.
 **/
GwyField*
gwy_raster_view_get_field(GwyRasterView *rasterview)
{
    g_return_val_if_fail(GWY_IS_RASTER_VIEW(rasterview), NULL);
    return GWY_RASTER_VIEW(rasterview)->priv->field;
}

/**
 * gwy_raster_view_set_gradient:
 * @rasterview: A raster view.
 * @gradient: (allow-none):
 *            A colour gradient.
 *
 * Sets the false colour gradient a raster view will use for visualisation.
 **/
void
gwy_raster_view_set_gradient(GwyRasterView *rasterview,
                             GwyGradient *gradient)
{
    g_return_if_fail(GWY_IS_RASTER_VIEW(rasterview));
    if (!set_gradient(rasterview, gradient))
        return;

    g_object_notify_by_pspec(G_OBJECT(rasterview),
                             raster_view_pspecs[PROP_GRADIENT]);
    gtk_widget_queue_draw(GTK_WIDGET(rasterview));
}

/**
 * gwy_raster_view_get_gradient:
 * @rasterview: A raster view.
 *
 * Obtains the false colour gradient used by a raster view for visualisation.
 *
 * Returns: (transfer none):
 *          The colour gradient used by @rasterview.
 **/
GwyGradient*
gwy_raster_view_get_gradient(GwyRasterView *rasterview)
{
    g_return_val_if_fail(GWY_IS_RASTER_VIEW(rasterview), NULL);
    return GWY_RASTER_VIEW(rasterview)->priv->gradient;
}

static void
gwy_raster_view_get_preferred_width(GtkWidget *widget,
                                    gint *minimum,
                                    gint *natural)
{
    GwyRasterView *rasterview = GWY_RASTER_VIEW(widget);
    *natural = calculate_full_width(rasterview);
    *minimum = rasterview->priv->zoom ? *natural : 1;
}

static void
gwy_raster_view_get_preferred_height(GtkWidget *widget,
                                     gint *minimum,
                                     gint *natural)
{
    GwyRasterView *rasterview = GWY_RASTER_VIEW(widget);
    *natural = calculate_full_height(rasterview);
    *minimum = rasterview->priv->zoom ? *natural : 1;
}

static void
gwy_raster_view_size_allocate(GtkWidget *widget,
                              GtkAllocation *allocation)
{
    GtkAllocation prev_allocation;
    gtk_widget_get_allocation(widget, &prev_allocation);
    gtk_widget_set_allocation(widget, allocation);
    g_printerr("ALLOC: %d,%d\n", allocation->width, allocation->height);

    if (memcmp(allocation, &prev_allocation, sizeof(GtkAllocation)) == 0)
        return;

    GwyRasterView *rasterview = GWY_RASTER_VIEW(widget);
    rasterview->priv->image_valid = FALSE;

    set_hadjustment_values(rasterview);
    set_vadjustment_values(rasterview);
}

static void
ensure_image(GwyRasterView *rasterview)
{
    RasterView *priv = rasterview->priv;

    if (priv->image_valid)
        return;

    GwyField *field = priv->field;
    g_return_if_fail(field);

    GtkWidget *widget = GTK_WIDGET(rasterview);
    guint width = gtk_widget_get_allocated_width(widget);
    guint height = gtk_widget_get_allocated_height(widget);

    if (!priv->surface
        || (guint)cairo_image_surface_get_width(priv->surface) != width
        || (guint)cairo_image_surface_get_height(priv->surface) != height) {
        destroy_surface(rasterview);
        guint stride = cairo_format_stride_for_width(CAIRO_FORMAT_RGB24, width);
        guchar *data = g_new(guchar, stride*height);
        priv->surface = cairo_image_surface_create_for_data(data,
                                                            CAIRO_FORMAT_RGB24,
                                                            width, height,
                                                            stride);
    }

    guint full_width = calculate_full_width(rasterview);
    guint full_height = calculate_full_height(rasterview);
    g_printerr("ensure_image, zoom = %g, size = (%u, %u), full_size = (%u, %u)\n",
               priv->zoom, width, height, full_width, full_height);

    gdouble xfrom = 0.0, yfrom = 0.0, xto, yto;

    if (priv->hadjustment) {
        gdouble offset = gtk_adjustment_get_value(priv->hadjustment);
        g_printerr("Have adjustment, value = %g\n", offset);
        xfrom = offset * field->xres/full_width;
        // FIXME: We must use true zoom here, runding causes slight shiver.
        xto = xfrom + width * field->xres/full_width;
        // FIXME: is this right with zoom = 0?
    }
    else {
        // FIXME
        xto = field->xres;
    }

    if (priv->vadjustment) {
        gdouble offset = gtk_adjustment_get_value(priv->vadjustment);
        g_printerr("Have vdjustment, value = %g\n", offset);
        yfrom = offset * field->yres/full_height;
        // FIXME: We must use true zoom here, runding causes slight shiver.
        yto = yfrom + height * field->xres/full_width;
        // FIXME: is this right with zoom = 0?
    }
    else {
        // FIXME
        yto = field->yres;
    }

    gdouble min, max;
    gwy_field_min_max_full(field, &min, &max);

    GwyGradient *gradient = (priv->gradient
                             ? priv->gradient
                             : gwy_gradients_get(NULL));

    g_printerr("(%g, %g) to (%g, %g)\n", xfrom, yfrom, xto, yto);
    gwy_field_render_cairo(field, priv->surface, gradient,
                           xfrom, yfrom, xto, yto,
                           min, max);
    priv->image_valid = TRUE;
}

static gboolean
gwy_raster_view_draw(GtkWidget *widget,
                     cairo_t *cr)
{
    GwyRasterView *rasterview = GWY_RASTER_VIEW(widget);

    if (!rasterview->priv->field)
        return FALSE;

    //GtkStyleContext *context = gtk_widget_get_style_context(widget);
    ensure_image(rasterview);

    cairo_set_source_surface(cr, rasterview->priv->surface, 0, 0);
    cairo_paint(cr);

    return FALSE;
}

static void
destroy_surface(GwyRasterView *rasterview)
{
    RasterView *priv = rasterview->priv;
    if (priv->surface) {
        guchar *data = cairo_image_surface_get_data(priv->surface);
        cairo_surface_destroy(priv->surface);
        priv->surface = NULL;
        g_free(data);
    }
    priv->image_valid = FALSE;
}

static gboolean
set_field(GwyRasterView *rasterview,
          GwyField *field)
{
    RasterView *priv = rasterview->priv;
    if (!gwy_set_member_object(rasterview, field, GWY_TYPE_FIELD,
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

    priv->image_valid = FALSE;
    return TRUE;
}

static gboolean
set_gradient(GwyRasterView *rasterview,
             GwyGradient *gradient)
{
    RasterView *priv = rasterview->priv;
    if (!gwy_set_member_object(rasterview, gradient, GWY_TYPE_GRADIENT,
                               &priv->gradient,
                               "data-changed", &gradient_data_changed,
                               &priv->gradient_data_changed_id,
                               G_CONNECT_SWAPPED,
                               NULL))
        return FALSE;

    priv->image_valid = FALSE;
    return TRUE;
}

static void
field_notify(GwyRasterView *rasterview,
             GParamSpec *pspec,
             GwyField *field)
{
    RasterView *priv = rasterview->priv;
    priv->field_aspect_ratio = gwy_field_dy(field)/gwy_field_dx(field);

    if (gwy_strequal(pspec->name, "x-res")
        || gwy_strequal(pspec->name, "y-res")) {
        gtk_widget_queue_resize(GTK_WIDGET(rasterview));
    }
    if (priv->real_aspect_ratio
             && (gwy_strequal(pspec->name, "x-real")
                 || gwy_strequal(pspec->name, "y-real"))) {
        gtk_widget_queue_resize(GTK_WIDGET(rasterview));
    }
}

static void
field_data_changed(GwyRasterView *rasterview,
                   GwyFieldPart *fpart,
                   GwyField *field)
{
    rasterview->priv->image_valid = FALSE;
    gtk_widget_queue_draw(GTK_WIDGET(rasterview));
}

static void
gradient_data_changed(GwyRasterView *rasterview,
                      GwyGradient *gradient)
{
    rasterview->priv->image_valid = FALSE;
    gtk_widget_queue_draw(GTK_WIDGET(rasterview));
}

static gboolean
set_hadjustment(GwyRasterView *rasterview,
                GtkAdjustment *adjustment)
{
    RasterView *priv = rasterview->priv;
    if (!gwy_set_member_object(rasterview, adjustment, GTK_TYPE_ADJUSTMENT,
                               &priv->hadjustment,
                               "value-changed", &adjustment_value_changed,
                               &priv->hadjustment_value_changed_id,
                               G_CONNECT_SWAPPED,
                               NULL))
        return FALSE;

    priv->image_valid = FALSE;
    set_hadjustment_values(rasterview);
    return TRUE;
}

static gboolean
set_vadjustment(GwyRasterView *rasterview,
                GtkAdjustment *adjustment)
{
    RasterView *priv = rasterview->priv;
    if (!gwy_set_member_object(rasterview, adjustment, GTK_TYPE_ADJUSTMENT,
                               &priv->vadjustment,
                               "value-changed", &adjustment_value_changed,
                               &priv->vadjustment_value_changed_id,
                               G_CONNECT_SWAPPED,
                               NULL))
        return FALSE;

    priv->image_valid = FALSE;
    set_vadjustment_values(rasterview);
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
                 "page-size", (gdouble)size,
                 "step-increment", 0.1*size,
                 "page-increment", 0.9*size,
                 NULL);

    gdouble new_value = CLAMP(old_value, 0, new_upper - size);
    if (new_value != old_value)
        gtk_adjustment_set_value(adjustment, new_value);
}

static void
set_hadjustment_values(GwyRasterView *rasterview)
{
    RasterView *priv = rasterview->priv;
    GtkAllocation allocation;
    gtk_widget_get_allocation(GTK_WIDGET(rasterview), &allocation);
    set_adjustment_values(priv->hadjustment,
                          allocation.width, calculate_full_width(rasterview));
}

static void
set_vadjustment_values(GwyRasterView *rasterview)
{
    RasterView *priv = rasterview->priv;
    GtkAllocation allocation;
    gtk_widget_get_allocation(GTK_WIDGET(rasterview), &allocation);
    set_adjustment_values(priv->vadjustment,
                          allocation.height, calculate_full_height(rasterview));
}

static void
adjustment_value_changed(GwyRasterView *rasterview)
{
    g_printerr("adjustment changed: x = %g, y = %g\n",
               gtk_adjustment_get_value(rasterview->priv->hadjustment),
               gtk_adjustment_get_value(rasterview->priv->vadjustment));

    rasterview->priv->image_valid = FALSE;
    gtk_widget_queue_draw(GTK_WIDGET(rasterview));
}

static gboolean
set_zoom(GwyRasterView *rasterview,
         gdouble zoom)
{
    RasterView *priv = rasterview->priv;
    if (zoom == priv->zoom)
        return FALSE;

    priv->zoom = zoom;
    gtk_widget_queue_resize(GTK_WIDGET(rasterview));
    return TRUE;
}

static gboolean
set_real_aspect_ratio(GwyRasterView *rasterview,
                      gboolean setting)
{
    RasterView *priv = rasterview->priv;
    if (setting == priv->real_aspect_ratio)
        return FALSE;

    priv->real_aspect_ratio = setting;
    // Do not trigger a resize if the aspect ratios are the same.
    if (priv->field_aspect_ratio == 1.0)
        return TRUE;

    gtk_widget_queue_resize(GTK_WIDGET(rasterview));
    return TRUE;
}

static guint
calculate_full_width(GwyRasterView *rasterview)
{
    RasterView *priv = rasterview->priv;
    if (!priv->field)
        return 1;

    gdouble xzoom = priv->zoom ? priv->zoom : 1.0;
    return gwy_round(fmax(xzoom * priv->field->xres, 1.0));
}

static guint
calculate_full_height(GwyRasterView *rasterview)
{
    RasterView *priv = rasterview->priv;
    if (!priv->field)
        return 1;

    gdouble yzoom = priv->zoom ? priv->zoom : 1.0;
    if (priv->real_aspect_ratio)
        yzoom *= priv->field_aspect_ratio;
    return gwy_round(fmax(yzoom * priv->field->yres, 1.0));
}

/**
 * SECTION: raster-view
 * @section_id: GwyRasterView
 * @title: Raster view
 * @short_description: Display fields using false colour maps
 **/

/**
 * GwyRasterView:
 *
 * Widget for raster display of two-dimensional data using false colour maps.
 *
 * The #GwyRasterView struct contains private data only and should be accessed
 * using the functions below.
 **/

/**
 * GwyRasterViewClass:
 *
 * Class of two-dimensional raster views.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
