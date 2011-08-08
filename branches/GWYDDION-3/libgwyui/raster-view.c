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
#include "libgwy/field-statistics.h"
#include "libgwyui/field-render.h"
#include "libgwyui/raster-view.h"

enum {
    PROP_0,
    PROP_FIELD,
    PROP_GRADIENT,
    N_PROPS
};

struct _GwyRasterViewPrivate {
    GwyField *field;
    GwyGradient *gradient;
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

static GParamSpec *raster_view_pspecs[N_PROPS];

G_DEFINE_TYPE(GwyRasterView, gwy_raster_view, GTK_TYPE_WIDGET);

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
                              "Gradient used for visualisastion.",
                              GWY_TYPE_GRADIENT,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    for (guint i = 1; i < N_PROPS; i++)
        g_object_class_install_property(gobject_class, i,
                                        raster_view_pspecs[i]);
}

static void
gwy_raster_view_init(GwyRasterView *rasterview)
{
    rasterview->priv = G_TYPE_INSTANCE_GET_PRIVATE(rasterview,
                                                   GWY_TYPE_RASTER_VIEW,
                                                   RasterView);
    gtk_widget_set_has_window(GTK_WIDGET(rasterview), FALSE);
}

static void
gwy_raster_view_finalize(GObject *object)
{
    G_OBJECT_CLASS(gwy_raster_view_parent_class)->finalize(object);
}

static void
destroy_surface(RasterView *rasterview)
{
    if (rasterview->surface) {
        guchar *data = cairo_image_surface_get_data(rasterview->surface);
        cairo_surface_destroy(rasterview->surface);
        rasterview->surface = NULL;
        g_free(data);
    }
    rasterview->image_valid = FALSE;
}

static void
gwy_raster_view_dispose(GObject *object)
{
    RasterView *rasterview = GWY_RASTER_VIEW(object)->priv;

    GWY_OBJECT_UNREF(rasterview->gradient);
    GWY_OBJECT_UNREF(rasterview->field);
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
        gwy_raster_view_set_field(rasterview,
                                  g_value_get_object(value));
        break;

        case PROP_GRADIENT:
        gwy_raster_view_set_gradient(rasterview,
                                     g_value_get_object(value));
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

void
gwy_raster_view_set_field(GwyRasterView *rasterview,
                          GwyField *field)
{
    g_return_if_fail(GWY_IS_RASTER_VIEW(rasterview));
    g_return_if_fail(!field || GWY_IS_FIELD(field));

    RasterView *priv = rasterview->priv;
    if (field == priv->field)
        return;

    if (field)
        g_object_ref(field);

    GwyField *old = priv->field;
    priv->field = field;
    GWY_OBJECT_UNREF(old);

    priv->image_valid = FALSE;
    gtk_widget_queue_resize(GTK_WIDGET(rasterview));
}

GwyField*
gwy_raster_view_get_field(GwyRasterView *rasterview)
{
    g_return_val_if_fail(GWY_IS_RASTER_VIEW(rasterview), NULL);
    return GWY_RASTER_VIEW(rasterview)->priv->field;
}

void
gwy_raster_view_set_gradient(GwyRasterView *rasterview,
                             GwyGradient *gradient)
{
    g_return_if_fail(GWY_IS_RASTER_VIEW(rasterview));
    g_return_if_fail(!gradient || GWY_IS_GRADIENT(gradient));

    RasterView *priv = rasterview->priv;
    if (gradient == priv->gradient)
        return;

    if (gradient)
        g_object_ref(gradient);

    GwyGradient *old = priv->gradient;
    priv->gradient = gradient;
    GWY_OBJECT_UNREF(old);

    priv->image_valid = FALSE;
    gtk_widget_queue_draw(GTK_WIDGET(rasterview));
}

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
    *minimum = 1;
    *natural = rasterview->priv->field ? rasterview->priv->field->xres : 1;
}

static void
gwy_raster_view_get_preferred_height(GtkWidget *widget,
                                     gint *minimum,
                                     gint *natural)
{
    GwyRasterView *rasterview = GWY_RASTER_VIEW(widget);
    *minimum = 1;
    *natural = rasterview->priv->field ? rasterview->priv->field->yres : 1;
}

static void
gwy_raster_view_size_allocate(GtkWidget *widget,
                              GtkAllocation *allocation)
{
    gtk_widget_set_allocation(widget, allocation);
    g_printerr("ALLOC: %d,%d\n", allocation->width, allocation->height);

    GwyRasterView *rasterview = GWY_RASTER_VIEW(widget);
    rasterview->priv->image_valid = FALSE;
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
        destroy_surface(priv);
        guint stride = cairo_format_stride_for_width(CAIRO_FORMAT_RGB24, width);
        guchar *data = g_new(guchar, stride*height);
        priv->surface = cairo_image_surface_create_for_data(data,
                                                            CAIRO_FORMAT_RGB24,
                                                            width, height,
                                                            stride);
    }

    gdouble min, max;
    gwy_field_min_max_full(field, &min, &max);

    GwyGradient *gradient = (priv->gradient
                             ? priv->gradient
                             : gwy_gradients_get(NULL));

    gwy_field_render_cairo(field, priv->surface, gradient,
                           0.0, 0.0, field->xres, field->yres,
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

/**
 * SECTION: raster-view
 * @section_id: GwyRasterView
 * @title: Two-dimensional view
 * @short_description: Display fields using false colour maps
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
