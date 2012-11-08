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

#include "libgwy/object-utils.h"
#include "libgwyui/cairo-utils.h"
#include "libgwyui/cell-renderer-gradient.h"

enum {
    ASPECT_RATIO = 5
};

enum {
    PROP_0,
    PROP_GRADIENT,
    N_PROPS
};

typedef struct _GwyCellRendererGradientPrivate CellRendererGradient;

struct _GwyCellRendererGradientPrivate {
    GwyGradient *gradient;
    cairo_pattern_t *pattern;
    gulong gradient_data_changed_id;
    guint height;
};

static void               gwy_cell_renderer_gradient_finalize            (GObject *object);
static void               gwy_cell_renderer_gradient_dispose             (GObject *object);
static void               gwy_cell_renderer_gradient_get_property        (GObject *object,
                                                                          guint prop_id,
                                                                          GValue *value,
                                                                          GParamSpec *pspec);
static void               gwy_cell_renderer_gradient_set_property        (GObject *object,
                                                                          guint prop_id,
                                                                          const GValue *value,
                                                                          GParamSpec *pspec);
static GtkSizeRequestMode gwy_cell_renderer_gradient_get_request_mode    (GtkCellRenderer *cell);
static void               gwy_cell_renderer_gradient_get_preferred_width (GtkCellRenderer *cell,
                                                                          GtkWidget *widget,
                                                                          gint *minimum_size,
                                                                          gint *natural_size);
static void               gwy_cell_renderer_gradient_get_preferred_height(GtkCellRenderer *cell,
                                                                          GtkWidget *widget,
                                                                          gint *minimum_size,
                                                                          gint *natural_size);
static void               gwy_cell_renderer_gradient_render              (GtkCellRenderer *cell,
                                                                          cairo_t *cr,
                                                                          GtkWidget *widget,
                                                                          const GdkRectangle *background_area,
                                                                          const GdkRectangle *cell_area,
                                                                          GtkCellRendererState flags);
static gboolean           set_gradient                                   (GwyCellRendererGradient *renderer,
                                                                          GwyGradient *gradient);
static void               gradient_data_changed                          (GwyCellRendererGradient *renderer,
                                                                          GwyGradient *gradient);
static void               ensure_pattern                                 (GwyCellRendererGradient *renderer);
static void               discard_pattern                                (GwyCellRendererGradient *renderer);

static GParamSpec *properties[N_PROPS];

G_DEFINE_TYPE(GwyCellRendererGradient, gwy_cell_renderer_gradient,
              GTK_TYPE_CELL_RENDERER);

static void
gwy_cell_renderer_gradient_dispose(GObject *object)
{
    GwyCellRendererGradient *renderer = GWY_CELL_RENDERER_GRADIENT(object);
    discard_pattern(renderer);
    set_gradient(renderer, NULL);
    G_OBJECT_CLASS(gwy_cell_renderer_gradient_parent_class)->dispose(object);
}

static void
gwy_cell_renderer_gradient_class_init(GwyCellRendererGradientClass *class)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(class);
    GtkCellRendererClass *cell_class = GTK_CELL_RENDERER_CLASS(class);

    g_type_class_add_private(gobject_class, sizeof(CellRendererGradient));

    gobject_class->finalize = gwy_cell_renderer_gradient_finalize;
    gobject_class->dispose = gwy_cell_renderer_gradient_dispose;
    gobject_class->get_property = gwy_cell_renderer_gradient_get_property;
    gobject_class->set_property = gwy_cell_renderer_gradient_set_property;

    cell_class->get_request_mode = gwy_cell_renderer_gradient_get_request_mode;
    cell_class->get_preferred_width = gwy_cell_renderer_gradient_get_preferred_width;
    cell_class->get_preferred_height = gwy_cell_renderer_gradient_get_preferred_height;
    cell_class->render = gwy_cell_renderer_gradient_render;

    properties[PROP_GRADIENT]
        = g_param_spec_object("gradient",
                              "Gradient",
                              "Gradient used for visualisation.",
                              GWY_TYPE_GRADIENT,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    for (guint i = 1; i < N_PROPS; i++)
        g_object_class_install_property(gobject_class, i, properties[i]);
}

static void
gwy_cell_renderer_gradient_init(GwyCellRendererGradient *renderer)
{
    renderer->priv
        = G_TYPE_INSTANCE_GET_PRIVATE(renderer,
                                      GWY_TYPE_CELL_RENDERER_GRADIENT,
                                      CellRendererGradient);
    CellRendererGradient *priv = renderer->priv;

    gint w, h;
    gtk_icon_size_lookup(GTK_ICON_SIZE_SMALL_TOOLBAR, &w, &h);
    priv->height = h;
}

static void
gwy_cell_renderer_gradient_finalize(GObject *object)
{
    G_OBJECT_CLASS(gwy_cell_renderer_gradient_parent_class)->finalize(object);
}

static void
gwy_cell_renderer_gradient_set_property(GObject *object,
                                        guint prop_id,
                                        const GValue *value,
                                        GParamSpec *pspec)
{
    GwyCellRendererGradient *renderer = GWY_CELL_RENDERER_GRADIENT(object);

    switch (prop_id) {
        case PROP_GRADIENT:
        set_gradient(renderer, g_value_get_object(value));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_cell_renderer_gradient_get_property(GObject *object,
                                        guint prop_id,
                                        GValue *value,
                                        GParamSpec *pspec)
{
    CellRendererGradient *priv = GWY_CELL_RENDERER_GRADIENT(object)->priv;

    switch (prop_id) {
        case PROP_GRADIENT:
        g_value_set_object(value, priv->gradient);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static GtkSizeRequestMode
gwy_cell_renderer_gradient_get_request_mode(G_GNUC_UNUSED GtkCellRenderer *cell)
{
    return GTK_SIZE_REQUEST_CONSTANT_SIZE;
}

static void
gwy_cell_renderer_gradient_get_preferred_width(GtkCellRenderer *cell,
                                               G_GNUC_UNUSED GtkWidget *widget,
                                               gint *minimum_size,
                                               gint *natural_size)
{
    CellRendererGradient *priv = GWY_CELL_RENDERER_GRADIENT(cell)->priv;
    gint xpad;
    gtk_cell_renderer_get_padding(cell, &xpad, NULL);
    *minimum_size = 2 + 2*xpad;
    *natural_size = ASPECT_RATIO*priv->height + 2*xpad;
}

static void
gwy_cell_renderer_gradient_get_preferred_height(GtkCellRenderer *cell,
                                                G_GNUC_UNUSED GtkWidget *widget,
                                                gint *minimum_size,
                                                gint *natural_size)
{
    CellRendererGradient *priv = GWY_CELL_RENDERER_GRADIENT(cell)->priv;
    gint ypad;
    gtk_cell_renderer_get_padding(cell, &ypad, NULL);
    *minimum_size = 2 + 2*ypad;
    *natural_size = priv->height + 2*ypad;
}

// FIXME: Do we want some fancy style dependencies or rather faithful rendering
// of the gradient?
static void
gwy_cell_renderer_gradient_render(GtkCellRenderer *cell,
                                  cairo_t *cr,
                                  G_GNUC_UNUSED GtkWidget *widget,
                                  G_GNUC_UNUSED const GdkRectangle *background_area,
                                  const GdkRectangle *cell_area,
                                  G_GNUC_UNUSED GtkCellRendererState flags)

{
    gint xpad, ypad;
    gtk_cell_renderer_get_padding(cell, &xpad, &ypad);
    GdkRectangle rect = {
        cell_area->x + xpad,
        cell_area->y + ypad,
        cell_area->width - 2*xpad,
        cell_area->height - 2*ypad,
    };
    if (!gdk_rectangle_intersect(cell_area, &rect, NULL))
        return;

    GwyCellRendererGradient *renderer = GWY_CELL_RENDERER_GRADIENT(cell);
    CellRendererGradient *priv = renderer->priv;

    ensure_pattern(renderer);
    // This can happen with a NULL gradient.
    if (!priv->pattern)
        return;

    cairo_save(cr);
    cairo_rectangle(cr, rect.x, rect.y, rect.width, rect.height);
    cairo_set_source(cr, priv->pattern);
    // TODO: Set up pattern scaling!
    cairo_fill(cr);
    cairo_restore(cr);
}

/**
 * gwy_cell_renderer_gradient_new:
 *
 * Creates a new gradient cell renderer.
 *
 * Return value: A new gradient cell renderer.
 **/
GtkCellRenderer*
gwy_cell_renderer_gradient_new(void)
{
    return g_object_new(GWY_TYPE_CELL_RENDERER_GRADIENT, NULL);
}

// This seems a bit unnecessary as the renderer will always get new gradients
// for rendering individual cells.  But we should be able to render a fixed
// gradient without bugs.
static gboolean
set_gradient(GwyCellRendererGradient *renderer,
             GwyGradient *gradient)
{
    CellRendererGradient *priv = renderer->priv;
    if (!gwy_set_member_object(renderer, gradient, GWY_TYPE_GRADIENT,
                               &priv->gradient,
                               "data-changed", &gradient_data_changed,
                               &priv->gradient_data_changed_id,
                               G_CONNECT_SWAPPED,
                               NULL))
        return FALSE;

    discard_pattern(renderer);
    return TRUE;
}

static void
gradient_data_changed(GwyCellRendererGradient *renderer,
                      G_GNUC_UNUSED GwyGradient *gradient)
{
    discard_pattern(renderer);
}

static void
ensure_pattern(GwyCellRendererGradient *renderer)
{
    CellRendererGradient *priv = renderer->priv;
    if (priv->pattern)
        return;

    if (priv->gradient)
        priv->pattern = gwy_cairo_pattern_create_gradient(priv->gradient,
                                                          GTK_POS_RIGHT);
}

static void
discard_pattern(GwyCellRendererGradient *renderer)
{
    CellRendererGradient *priv = renderer->priv;
    if (priv->pattern) {
        cairo_pattern_destroy(priv->pattern);
        priv->pattern = NULL;
    }
}

/**
 * SECTION:cell-renderer-gradient
 * @title: GwyCellRendererGradient
 * @short_description: Renders a gradient in a cell
 *
 * A #GwyCellRendererGradient can be used to render a #GwyGradient in a cell
 * of a #GtkTreeView or a similar cell-view widget.
 */

/**
 * GwyCellRendererGradient:
 *
 * Object representing a gradient cell renderer.
 *
 * The #GwyCellRendererGradient struct contains private data only and should be
 * accessed using the functions below.
 **/

/**
 * GwyCellRendererGradientClass:
 *
 * Class of gradient cell renderers.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
