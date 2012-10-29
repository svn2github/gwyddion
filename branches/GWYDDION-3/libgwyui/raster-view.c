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
#include "libgwy/field-statistics.h"
#include "libgwyui/types.h"
#include "libgwyui/raster-view.h"

enum {
    PROP_0,
    PROP_SCALE_TYPE,
    PROP_SCROLLER,
    PROP_AREA,
    PROP_HRULER,
    PROP_VRULER,
    PROP_HSCROLLBAR,
    PROP_VSCROLLBAR,
    PROP_COLOR_AXIS,
    N_PROPS,
};

struct _GwyRasterViewPrivate {
    GwyRulerScaleType scale_type;

    GwyScroller *scroller;
    GwyRasterArea *area;
    gulong area_notify_id;
    gulong area_motion_notify_id;
    gulong area_enter_notify_id;
    gulong area_leave_notify_id;

    GtkAdjustment *hadjustment;
    GtkAdjustment *vadjustment;
    gulong hadjustment_changed_id;
    gulong hadjustment_value_changed_id;
    gulong vadjustment_changed_id;
    gulong vadjustment_value_changed_id;

    GwyRuler *hruler;
    GtkScrollbar *hscrollbar;
    GwyRuler *vruler;
    GtkScrollbar *vscrollbar;
    GwyColorAxis *coloraxis;

    GwyField *field;
    gulong field_notify_id;
    gulong field_data_changed_id;
};

typedef struct _GwyRasterViewPrivate RasterView;

static void     gwy_raster_view_dispose     (GObject *object);
static void     gwy_raster_view_finalize    (GObject *object);
static void     gwy_raster_view_set_property(GObject *object,
                                             guint prop_id,
                                             const GValue *value,
                                             GParamSpec *pspec);
static void     gwy_raster_view_get_property(GObject *object,
                                             guint prop_id,
                                             GValue *value,
                                             GParamSpec *pspec);
static void     area_notify                 (GwyRasterView *rasterview,
                                             GParamSpec *pspec,
                                             GwyRasterArea *area);
static void     field_notify                (GwyRasterView *rasterview,
                                             GParamSpec *pspec,
                                             GwyField *field);
static gboolean set_scale_type              (GwyRasterView *rasterview,
                                             GwyRulerScaleType scale_type);
static gboolean set_hadjustment             (GwyRasterView *rasterview,
                                             GtkAdjustment *adjustment);
static gboolean set_vadjustment             (GwyRasterView *rasterview,
                                             GtkAdjustment *adjustment);
static gboolean set_field                   (GwyRasterView *rasterview,
                                             GwyField *field);
static void     update_ruler_ranges         (GwyRasterView *rasterview);
static gboolean area_motion_notify          (GwyRasterView *rasterview,
                                             GdkEventMotion *event,
                                             GwyRasterArea *area);
static gboolean area_enter_notify           (GwyRasterView *rasterview,
                                             GdkEventCrossing *event,
                                             GwyRasterArea *area);
static gboolean area_leave_notify           (GwyRasterView *rasterview,
                                             GdkEventCrossing *event,
                                             GwyRasterArea *area);

static GParamSpec *properties[N_PROPS];

G_DEFINE_TYPE(GwyRasterView, gwy_raster_view, GTK_TYPE_GRID);

static void
gwy_raster_view_class_init(GwyRasterViewClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    //GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

    g_type_class_add_private(klass, sizeof(RasterView));

    gobject_class->dispose = gwy_raster_view_dispose;
    gobject_class->finalize = gwy_raster_view_finalize;
    gobject_class->get_property = gwy_raster_view_get_property;
    gobject_class->set_property = gwy_raster_view_set_property;

    properties[PROP_SCALE_TYPE]
        = g_param_spec_enum("scale-type",
                            "Scale type",
                            "Scale type of rulers.",
                            GWY_TYPE_RULER_SCALE_TYPE,
                            GWY_RULER_SCALE_REAL,
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_SCROLLER]
        = g_param_spec_object("scroller",
                              "Scroller",
                              "Scroller widget within the raster view.",
                              GWY_TYPE_SCROLLER,
                              G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    properties[PROP_AREA]
        = g_param_spec_object("area",
                              "Area",
                              "Raster area widget within the raster view.",
                              GWY_TYPE_RASTER_AREA,
                              G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    properties[PROP_HRULER]
        = g_param_spec_object("hruler",
                              "Horizontal ruler",
                              "Horizontal ruler widget within the raster view.",
                              GWY_TYPE_RULER,
                              G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    properties[PROP_VRULER]
        = g_param_spec_object("vruler",
                              "Vertical ruler",
                              "Vertical ruler widget within the raster view.",
                              GWY_TYPE_RULER,
                              G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    properties[PROP_HSCROLLBAR]
        = g_param_spec_object("hscrollbar",
                              "Horizontal scrollbar",
                              "Horizontal scrollbar widget within the raster view.",
                              GTK_TYPE_SCROLLBAR,
                              G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    properties[PROP_VSCROLLBAR]
        = g_param_spec_object("vscrollbar",
                              "Vertical scrollbar",
                              "Vertical scrollbar widget within the raster view.",
                              GTK_TYPE_SCROLLBAR,
                              G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    properties[PROP_COLOR_AXIS]
        = g_param_spec_object("color-axis",
                              "Color axis",
                              "Colo raxis widget within the raster view.",
                              GWY_TYPE_COLOR_AXIS,
                              G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    for (guint i = 1; i < N_PROPS; i++)
        g_object_class_install_property(gobject_class, i, properties[i]);
}

static void
gwy_raster_view_init(GwyRasterView *rasterview)
{
    rasterview->priv = G_TYPE_INSTANCE_GET_PRIVATE(rasterview,
                                                   GWY_TYPE_RASTER_VIEW,
                                                   RasterView);
    RasterView *priv = rasterview->priv;
    priv->scale_type = GWY_RULER_SCALE_REAL;

    GtkGrid *grid = GTK_GRID(rasterview);

    GtkWidget *scroller = gwy_scroller_new();
    priv->scroller = GWY_SCROLLER(scroller);
    gtk_grid_attach(grid, scroller, 2, 2, 1, 1);
    gtk_widget_set_hexpand(scroller, 1.0);
    gtk_widget_set_vexpand(scroller, 1.0);
    gtk_widget_show(scroller);

    GtkWidget *area = gwy_raster_area_new();
    priv->area = GWY_RASTER_AREA(area);
    gtk_container_add(GTK_CONTAINER(scroller), area);
    gtk_widget_show(area);

    GtkScrollable *scrollable = GTK_SCROLLABLE(area);

    GtkAdjustment *hadj = gtk_scrollable_get_hadjustment(scrollable);
    GtkWidget *hscrollbar = gtk_scrollbar_new(GTK_ORIENTATION_HORIZONTAL, hadj);
    priv->hscrollbar = GTK_SCROLLBAR(hscrollbar);
    gtk_grid_attach(grid, hscrollbar, 2, 0, 1, 1);
    gtk_widget_show(hscrollbar);

    GtkAdjustment *vadj = gtk_scrollable_get_vadjustment(scrollable);
    GtkWidget *vscrollbar = gtk_scrollbar_new(GTK_ORIENTATION_VERTICAL, vadj);
    priv->vscrollbar = GTK_SCROLLBAR(vscrollbar);
    gtk_grid_attach(grid, vscrollbar, 0, 2, 1, 1);
    gtk_widget_show(vscrollbar);

    GtkWidget *hruler = gwy_ruler_new();
    priv->hruler = GWY_RULER(hruler);
    gwy_axis_set_edge(GWY_AXIS(hruler), GTK_POS_TOP);
    gwy_ruler_set_show_mark(priv->hruler, TRUE);
    g_object_set(hruler, "max-tick-level", 3, NULL);
    gtk_grid_attach(grid, hruler, 2, 1, 1, 1);
    gtk_widget_show(hruler);

    GtkWidget *vruler = gwy_ruler_new();
    priv->vruler = GWY_RULER(vruler);
    gwy_axis_set_edge(GWY_AXIS(vruler), GTK_POS_LEFT);
    gwy_ruler_set_show_mark(priv->vruler, TRUE);
    g_object_set(vruler, "max-tick-level", 3, NULL);
    gtk_grid_attach(grid, vruler, 1, 2, 1, 1);
    gtk_widget_show(vruler);

    GtkWidget *coloraxis = gwy_color_axis_new();
    priv->coloraxis = GWY_COLOR_AXIS(coloraxis);
    gwy_axis_set_edge(GWY_AXIS(coloraxis), GTK_POS_RIGHT);
    g_object_set(coloraxis,
                 "max-tick-level", 2,
                 "ticks-at-edges", TRUE,
                 NULL);
    gtk_grid_attach(grid, coloraxis, 3, 2, 1, 1);
    gtk_widget_show(coloraxis);

    priv->area_notify_id
        = g_signal_connect_swapped(area, "notify",
                                   G_CALLBACK(area_notify), rasterview);
    set_hadjustment(rasterview, hadj);
    set_vadjustment(rasterview, vadj);
    priv->area_motion_notify_id
        = g_signal_connect_swapped(area, "motion-notify-event",
                                   G_CALLBACK(area_motion_notify), rasterview);
    priv->area_enter_notify_id
        = g_signal_connect_swapped(area, "enter-notify-event",
                                   G_CALLBACK(area_enter_notify), rasterview);
    priv->area_leave_notify_id
        = g_signal_connect_swapped(area, "leave-notify-event",
                                   G_CALLBACK(area_leave_notify), rasterview);
}

static void
gwy_raster_view_dispose(GObject *object)
{
    GwyRasterView *rasterview = GWY_RASTER_VIEW(object);
    RasterView *priv = rasterview->priv;
    GWY_SIGNAL_HANDLER_DISCONNECT(priv->area, priv->area_motion_notify_id);
    GWY_SIGNAL_HANDLER_DISCONNECT(priv->area, priv->area_enter_notify_id);
    GWY_SIGNAL_HANDLER_DISCONNECT(priv->area, priv->area_leave_notify_id);
    GWY_SIGNAL_HANDLER_DISCONNECT(priv->area, priv->area_notify_id);
    set_field(rasterview, NULL);
    set_hadjustment(rasterview, NULL);
    set_vadjustment(rasterview, NULL);
    G_OBJECT_CLASS(gwy_raster_view_parent_class)->dispose(object);
}


static void
gwy_raster_view_finalize(GObject *object)
{
    G_OBJECT_CLASS(gwy_raster_view_parent_class)->finalize(object);
}

static void
gwy_raster_view_set_property(GObject *object,
                             guint prop_id,
                             const GValue *value,
                             GParamSpec *pspec)
{
    GwyRasterView *rasterview = GWY_RASTER_VIEW(object);

    switch (prop_id) {
        case PROP_SCALE_TYPE:
        set_scale_type(rasterview, g_value_get_enum(value));
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
    RasterView *priv = GWY_RASTER_VIEW(object)->priv;

    switch (prop_id) {
        case PROP_SCALE_TYPE:
        g_value_set_enum(value, priv->scale_type);
        break;

        case PROP_SCROLLER:
        g_value_set_object(value, priv->scroller);
        break;

        case PROP_AREA:
        g_value_set_object(value, priv->area);
        break;

        case PROP_HRULER:
        g_value_set_object(value, priv->hruler);
        break;

        case PROP_VRULER:
        g_value_set_object(value, priv->vruler);
        break;

        case PROP_HSCROLLBAR:
        g_value_set_object(value, priv->hscrollbar);
        break;

        case PROP_VSCROLLBAR:
        g_value_set_object(value, priv->vscrollbar);
        break;

        case PROP_COLOR_AXIS:
        g_value_set_object(value, priv->coloraxis);
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
 * gwy_raster_view_get_area:
 * @rasterview: A raster view.
 *
 * Gets the raster area widget used by a raster view.
 *
 * Returns: (transfer none):
 *          #GwyRasterArea widget used by the raster view.
 **/
GwyRasterArea*
gwy_raster_view_get_area(const GwyRasterView *rasterview)
{
    g_return_val_if_fail(GWY_IS_RASTER_VIEW(rasterview), NULL);
    return rasterview->priv->area;
}

/**
 * gwy_raster_view_get_scroller:
 * @rasterview: A raster view.
 *
 * Gets the scroller widget used by a raster view.
 *
 * Returns: (transfer none):
 *          #GwyScroller widget used by the raster view.
 **/
GwyScroller*
gwy_raster_view_get_scroller(const GwyRasterView *rasterview)
{
    g_return_val_if_fail(GWY_IS_RASTER_VIEW(rasterview), NULL);
    return rasterview->priv->scroller;
}

/**
 * gwy_raster_view_get_hruler:
 * @rasterview: A raster view.
 *
 * Gets the horizontal ruler widget used by a raster view.
 *
 * Returns: (transfer none):
 *          #GwyRuler widget used by the raster view as the horizontal ruler.
 **/
GwyRuler*
gwy_raster_view_get_hruler(const GwyRasterView *rasterview)
{
    g_return_val_if_fail(GWY_IS_RASTER_VIEW(rasterview), NULL);
    return rasterview->priv->hruler;
}

/**
 * gwy_raster_view_get_vruler:
 * @rasterview: A raster view.
 *
 * Gets the vertical ruler widget used by a raster view.
 *
 * Returns: (transfer none):
 *          #GwyRuler widget used by the raster view as the vertical ruler.
 **/
GwyRuler*
gwy_raster_view_get_vruler(const GwyRasterView *rasterview)
{
    g_return_val_if_fail(GWY_IS_RASTER_VIEW(rasterview), NULL);
    return rasterview->priv->vruler;
}

/**
 * gwy_raster_view_get_hscrollbar:
 * @rasterview: A raster view.
 *
 * Gets the horizontal scrollbar widget used by a raster view.
 *
 * Returns: (transfer none):
 *          #GtkScrollbar widget used by the raster view as the horizontal
 *          scrollbar.
 **/
GtkScrollbar*
gwy_raster_view_get_hscrollbar(const GwyRasterView *rasterview)
{
    g_return_val_if_fail(GWY_IS_RASTER_VIEW(rasterview), NULL);
    return rasterview->priv->hscrollbar;
}

/**
 * gwy_raster_view_get_vscrollbar:
 * @rasterview: A raster view.
 *
 * Gets the vertical scrollbar widget used by a raster view.
 *
 * Returns: (transfer none):
 *          #GtkScrollbar widget used by the raster view as the vertical
 *          scrollbar.
 **/
GtkScrollbar*
gwy_raster_view_get_vscrollbar(const GwyRasterView *rasterview)
{
    g_return_val_if_fail(GWY_IS_RASTER_VIEW(rasterview), NULL);
    return rasterview->priv->vscrollbar;
}

/**
 * gwy_raster_view_get_color_axis:
 * @rasterview: A raster view.
 *
 * Gets the color axis widget used by a raster view.
 *
 * Returns: (transfer none):
 *          #GwyColorAxis widget used by the raster view as the colour axis.
 **/
GwyColorAxis*
gwy_raster_view_get_color_axis(const GwyRasterView *rasterview)
{
    g_return_val_if_fail(GWY_IS_RASTER_VIEW(rasterview), NULL);
    return rasterview->priv->coloraxis;
}

static void
area_notify(GwyRasterView *rasterview,
            GParamSpec *pspec,
            GwyRasterArea *area)
{
    RasterView *priv = rasterview->priv;
    if (gwy_strequal(pspec->name, "hadjustment")) {
        GtkScrollable *scrollable = GTK_SCROLLABLE(area);
        set_hadjustment(rasterview, gtk_scrollable_get_hadjustment(scrollable));
    }
    else if (gwy_strequal(pspec->name, "vadjustment")) {
        GtkScrollable *scrollable = GTK_SCROLLABLE(area);
        set_vadjustment(rasterview, gtk_scrollable_get_vadjustment(scrollable));
    }
    else if (gwy_strequal(pspec->name, "field")) {
        set_field(rasterview, gwy_raster_area_get_field(area));
    }
    else if (gwy_strequal(pspec->name, "gradient")) {
        gwy_color_axis_set_gradient(priv->coloraxis,
                                    gwy_raster_area_get_gradient(area));
    }
}

static void
field_notify(GwyRasterView *rasterview,
             GParamSpec *pspec,
             GwyField *field)
{
    RasterView *priv = rasterview->priv;
    if (gwy_stramong(pspec->name,
                     "xres", "yres", "xreal", "yreal", "xoff", "yoff", NULL)) {
        update_ruler_ranges(rasterview);
    }
    else if (gwy_strequal(pspec->name, "unit-xy")) {
        GwyUnit *hunit = gwy_axis_get_unit(GWY_AXIS(priv->hruler));
        GwyUnit *vunit = gwy_axis_get_unit(GWY_AXIS(priv->vruler));
        GwyUnit *xyunit = gwy_field_get_unit_xy(field);
        gwy_unit_assign(hunit, xyunit);
        gwy_unit_assign(vunit, xyunit);
    }
    else if (gwy_strequal(pspec->name, "unit-z")) {
        GwyUnit *colorunit = gwy_axis_get_unit(GWY_AXIS(priv->coloraxis));
        GwyUnit *zunit = gwy_field_get_unit_z(field);
        gwy_unit_assign(colorunit, zunit);
    }
}

static void
field_data_changed(GwyRasterView *rasterview,
                   G_GNUC_UNUSED const GwyFieldPart *fpart,
                   GwyField *field)
{
    // TODO: This needs elaboration depending on the color mapping type...
    RasterView *priv = rasterview->priv;
    GwyRange range;
    gwy_field_min_max_full(field, &range.from, &range.to);
    gwy_axis_request_range(GWY_AXIS(priv->coloraxis), &range);
}

static gboolean
set_scale_type(GwyRasterView *rasterview,
               GwyRulerScaleType scale_type)
{
    RasterView *priv = rasterview->priv;
    if (scale_type == priv->scale_type)
        return FALSE;

    priv->scale_type = scale_type;
    update_ruler_ranges(rasterview);
    return TRUE;
}

static gboolean
set_hadjustment(GwyRasterView *rasterview,
                GtkAdjustment *adjustment)
{
    RasterView *priv = rasterview->priv;
    if (!gwy_set_member_object(rasterview, adjustment, GTK_TYPE_ADJUSTMENT,
                               &priv->hadjustment,
                               "changed", &update_ruler_ranges,
                               &priv->hadjustment_changed_id,
                               G_CONNECT_SWAPPED,
                               "value-changed", &update_ruler_ranges,
                               &priv->hadjustment_value_changed_id,
                               G_CONNECT_SWAPPED,
                               NULL))
        return FALSE;

    update_ruler_ranges(rasterview);
    return TRUE;
}

static gboolean
set_vadjustment(GwyRasterView *rasterview,
                GtkAdjustment *adjustment)
{
    RasterView *priv = rasterview->priv;
    if (!gwy_set_member_object(rasterview, adjustment, GTK_TYPE_ADJUSTMENT,
                               &priv->vadjustment,
                               "changed", &update_ruler_ranges,
                               &priv->vadjustment_changed_id,
                               G_CONNECT_SWAPPED,
                               "value-changed", &update_ruler_ranges,
                               &priv->vadjustment_value_changed_id,
                               G_CONNECT_SWAPPED,
                               NULL))
        return FALSE;

    update_ruler_ranges(rasterview);
    return TRUE;
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

    update_ruler_ranges(rasterview);
    return TRUE;
}

static void
update_ruler_ranges(GwyRasterView *rasterview)
{
    RasterView *priv = rasterview->priv;
    GwyRasterArea *rasterarea = GWY_RASTER_AREA(priv->area);
    cairo_rectangle_t area;
    gwy_raster_area_get_widget_area(rasterarea, &area);
    GwyRange hrange = { area.x, area.x + area.width };
    GwyRange vrange = { area.y, area.y + area.height };
    if (priv->scale_type == GWY_RULER_SCALE_REAL) {
        if (priv->field) {
            GwyField *field = priv->field;
            hrange.from = hrange.from/field->xres*field->xreal + field->xoff;
            hrange.to = hrange.to/field->xres*field->xreal + field->xoff;
            vrange.from = vrange.from/field->yres*field->yreal + field->yoff;
            vrange.to = vrange.to/field->yres*field->yreal + field->yoff;
        }
        else {
            // XXX: Whatever.
        }
    }
    gwy_axis_request_range(GWY_AXIS(priv->hruler), &hrange);
    gwy_axis_request_range(GWY_AXIS(priv->vruler), &vrange);
}

static gboolean
area_motion_notify(GwyRasterView *rasterview,
                   GdkEventMotion *event,
                   GwyRasterArea *area)
{
    RasterView *priv = rasterview->priv;
    gdouble x = event->x, y = event->y;
    const cairo_matrix_t *matrix;
    if (priv->scale_type == GWY_RULER_SCALE_REAL) {
        matrix = gwy_raster_area_get_widget_to_coords_matrix(area);
        cairo_matrix_transform_point(matrix, &x, &y);
    }
    else {
        matrix = gwy_raster_area_get_widget_to_field_matrix(area);
        cairo_matrix_transform_point(matrix, &x, &y);
    }
    gwy_ruler_set_mark(priv->hruler, x);
    gwy_ruler_set_mark(priv->vruler, y);
    return FALSE;
}

static gboolean
area_enter_notify(GwyRasterView *rasterview,
                  G_GNUC_UNUSED GdkEventCrossing *event,
                  G_GNUC_UNUSED GwyRasterArea *area)
{
    RasterView *priv = rasterview->priv;
    gwy_ruler_set_show_mark(priv->hruler, TRUE);
    gwy_ruler_set_show_mark(priv->vruler, TRUE);
    return FALSE;
}

static gboolean
area_leave_notify(GwyRasterView *rasterview,
                  G_GNUC_UNUSED GdkEventCrossing *event,
                  G_GNUC_UNUSED GwyRasterArea *area)
{
    RasterView *priv = rasterview->priv;
    gwy_ruler_set_show_mark(priv->hruler, FALSE);
    gwy_ruler_set_show_mark(priv->vruler, FALSE);
    return FALSE;
}

/**
 * SECTION: raster-view
 * @section_id: GwyRasterView
 * @title: GwyRasterView
 * @short_description: Display fields with rulers and color axes
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

/**
 * GwyRulerScaleType:
 * @GWY_RULER_SCALE_REAL: Rulers display real coordinates in physical units.
 * @GWY_RULER_SCALE_PIXEL: Rulers display pixel coordinates.
 *
 * Type of rules scales (coordinates).
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
