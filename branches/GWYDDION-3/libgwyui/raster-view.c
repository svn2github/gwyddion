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

#include <glib/gi18n-lib.h>
#include "libgwy/macros.h"
#include "libgwy/object-utils.h"
#include "libgwy/field-statistics.h"
#include "libgwyui/main.h"
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

enum {
    AXIS_TAB_AXIS,
    AXIS_TAB_GRADIENTS,
    AXIS_TAB_RANGES,
};

struct _GwyRasterViewPrivate {
    GwyRulerScaleType scale_type;

    GwyScroller *scroller;
    GwyRasterArea *area;

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
    gboolean requesting_axis_range;

    GtkWidget *gradients;
    GtkWidget *ranges;

    GwyField *field;
    gulong field_notify_id;
    gulong field_data_changed_id;

    GtkMenu *ruler_popup;
    GtkCheckMenuItem *pixel_distances;
    GtkCheckMenuItem *real_distances;
    GtkCheckMenuItem *square_pixels;
    GtkCheckMenuItem *real_aspect_ratio;

    GtkToggleButton *axisbutton;
    GtkToggleButton *gradbutton;
    GtkToggleButton *rangebutton;
};

typedef struct _GwyRasterViewPrivate RasterView;

static void       gwy_raster_view_dispose     (GObject *object);
static void       gwy_raster_view_finalize    (GObject *object);
static void       gwy_raster_view_set_property(GObject *object,
                                               guint prop_id,
                                               const GValue *value,
                                               GParamSpec *pspec);
static void       gwy_raster_view_get_property(GObject *object,
                                               guint prop_id,
                                               GValue *value,
                                               GParamSpec *pspec);
static void       area_notify                 (GwyRasterView *rasterview,
                                               GParamSpec *pspec,
                                               GwyRasterArea *area);
static void       field_notify                (GwyRasterView *rasterview,
                                               GParamSpec *pspec,
                                               GwyField *field);
static void       set_ruler_units_to_field    (GwyRasterView *rasterview);
static void       field_data_changed          (GwyRasterView *rasterview,
                                               const GwyFieldPart *fpart,
                                               GwyField *field);
static gboolean   set_scale_type              (GwyRasterView *rasterview,
                                               GwyRulerScaleType scale_type);
static gboolean   set_hadjustment             (GwyRasterView *rasterview,
                                               GtkAdjustment *adjustment);
static gboolean   set_vadjustment             (GwyRasterView *rasterview,
                                               GtkAdjustment *adjustment);
static gboolean   set_field                   (GwyRasterView *rasterview,
                                               GwyField *field);
static void       update_ruler_ranges         (GwyRasterView *rasterview);
static gboolean   area_motion_notify          (GwyRasterView *rasterview,
                                               GdkEventMotion *event,
                                               GwyRasterArea *area);
static gboolean   area_enter_notify           (GwyRasterView *rasterview,
                                               GdkEventCrossing *event,
                                               GwyRasterArea *area);
static gboolean   area_leave_notify           (GwyRasterView *rasterview,
                                               GdkEventCrossing *event,
                                               GwyRasterArea *area);
static gboolean   ruler_button_press          (GwyRasterView *rasterview,
                                               GdkEventButton *event,
                                               GwyRuler *ruler);
static void       axis_range_notify           (GwyRasterView *rasterview,
                                               GParamSpec *pspec,
                                               GwyColorAxis *coloraxis);
static GtkWidget* create_axis_button_box      (GwyRasterView *rasterview);
static GtkWidget* create_axis_button          (GtkRadioButton *groupwidget,
                                               guint id,
                                               const gchar *content);
static void       axis_button_clicked         (GwyRasterView *rasterview,
                                               GtkToggleButton *button);
static GtkWidget* create_range_controls       (GwyRasterView *rasterview);
static GtkWidget* create_gradient_list        (GwyRasterView *rasterview);
static void       pop_up_ruler_menu           (GwyRasterView *rasterview,
                                               GtkWidget *widget,
                                               GdkEventButton *event);
static GtkMenu*   create_ruler_popup          (GwyRasterView *rasterview);
static void       ruler_popup_item_toggled    (GwyRasterView *rasterview,
                                               GtkCheckMenuItem *menuitem);
static void       ruler_popup_deleted         (GtkWidget *menu);

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
    gwy_raster_area_set_scrollable(priv->area, TRUE);
    gwy_raster_area_set_zoomable(priv->area, TRUE);
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
    gtk_widget_add_events(hruler, GDK_BUTTON_PRESS_MASK);
    g_object_set(hruler, "max-tick-level", 3, NULL);
    gtk_grid_attach(grid, hruler, 2, 1, 1, 1);
    gtk_widget_show(hruler);

    GtkWidget *vruler = gwy_ruler_new();
    priv->vruler = GWY_RULER(vruler);
    gwy_axis_set_edge(GWY_AXIS(vruler), GTK_POS_LEFT);
    gtk_widget_add_events(vruler, GDK_BUTTON_PRESS_MASK);
    g_object_set(vruler, "max-tick-level", 3, NULL);
    gtk_grid_attach(grid, vruler, 1, 2, 1, 1);
    gtk_widget_show(vruler);

    GtkWidget *coloraxis = gwy_color_axis_new();
    priv->coloraxis = GWY_COLOR_AXIS(coloraxis);
    g_object_ref(priv->coloraxis);
    gwy_axis_set_edge(GWY_AXIS(coloraxis), GTK_POS_RIGHT);
    g_object_set(coloraxis,
                 "max-tick-level", 2,
                 "ticks-at-edges", TRUE,
                 "editable-range", TRUE,
                 NULL);
    gtk_grid_attach(grid, coloraxis, 3, 2, 1, 1);
    gtk_widget_show(coloraxis);

    g_signal_connect_swapped(area, "notify",
                             G_CALLBACK(area_notify), rasterview);
    set_hadjustment(rasterview, hadj);
    set_vadjustment(rasterview, vadj);
    g_signal_connect_swapped(area, "motion-notify-event",
                             G_CALLBACK(area_motion_notify), rasterview);
    g_signal_connect_swapped(area, "enter-notify-event",
                             G_CALLBACK(area_enter_notify), rasterview);
    g_signal_connect_swapped(area, "leave-notify-event",
                             G_CALLBACK(area_leave_notify), rasterview);
    g_signal_connect_swapped(hruler, "button-press-event",
                             G_CALLBACK(ruler_button_press), rasterview);
    g_signal_connect_swapped(vruler, "button-press-event",
                             G_CALLBACK(ruler_button_press), rasterview);
    g_signal_connect_swapped(coloraxis, "notify::range",
                             G_CALLBACK(axis_range_notify), rasterview);

    GtkWidget *buttonbox = create_axis_button_box(rasterview);
    gtk_grid_attach(grid, buttonbox, 3, 0, 1, 2);
    gtk_widget_show_all(buttonbox);
}

static void
gwy_raster_view_dispose(GObject *object)
{
    GwyRasterView *rasterview = GWY_RASTER_VIEW(object);
    RasterView *priv = rasterview->priv;
    set_field(rasterview, NULL);
    set_hadjustment(rasterview, NULL);
    set_vadjustment(rasterview, NULL);
    GWY_OBJECT_UNREF(priv->coloraxis);
    GWY_OBJECT_UNREF(priv->ranges);
    GWY_OBJECT_UNREF(priv->gradients);
    if (priv->ruler_popup) {
        gtk_widget_destroy(GTK_WIDGET(priv->ruler_popup));
        priv->ruler_popup = NULL;
    }
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

/**
 * gwy_raster_view_set_scale_type:
 * @rasterview: A raster view.
 * @scaletype: New scale type for rulers.
 *
 * Sets the scale type of raster view rulers.
 **/
void
gwy_raster_view_set_scale_type(GwyRasterView *rasterview,
                               GwyRulerScaleType scaletype)
{
    g_return_if_fail(GWY_IS_RASTER_VIEW(rasterview));
    if (!set_scale_type(rasterview, scaletype))
        return;

    RasterView *priv = rasterview->priv;
    if (scaletype == GWY_RULER_SCALE_PIXEL) {
        gwy_unit_set_from_string(gwy_axis_get_unit(GWY_AXIS(priv->hruler)),
                                 "px", NULL);
        gwy_unit_set_from_string(gwy_axis_get_unit(GWY_AXIS(priv->vruler)),
                                 "px", NULL);
    }
    else {
        if (priv->field)
            set_ruler_units_to_field(rasterview);
    }

    g_object_notify_by_pspec(G_OBJECT(rasterview), properties[PROP_SCALE_TYPE]);
}

/**
 * gwy_raster_view_get_scale_type:
 * @rasterview: A raster view.
 *
 * Gets the scale type of raster view rulers.
 *
 * Returns: The scale type used by rulers.
 **/
GwyRulerScaleType
gwy_raster_view_get_scale_type(const GwyRasterView *rasterview)
{
    g_return_val_if_fail(GWY_IS_RASTER_VIEW(rasterview), GWY_RULER_SCALE_PIXEL);
    return rasterview->priv->scale_type;
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
    else if (gwy_strequal(pspec->name, "real-aspect-ratio")) {
        if (priv->ruler_popup) {
            gboolean real_aspect_ratio;
            g_object_get(priv->area,
                         "real-aspect-ratio", &real_aspect_ratio,
                         NULL);
            GtkCheckMenuItem *item = (real_aspect_ratio
                                      ? priv->real_aspect_ratio
                                      : priv->square_pixels);
            if (!gtk_check_menu_item_get_active(item))
                gtk_check_menu_item_set_active(item, TRUE);
        }
    }
    else if (gwy_strequal(pspec->name, "range")) {
        GwyAxis *axis = GWY_AXIS(priv->coloraxis);
        GwyRange axisrange, arearange;
        gwy_raster_area_get_range(priv->area, &arearange);
        gwy_axis_get_range(axis, &axisrange);
        if (!gwy_equal(&axisrange, &arearange)) {
            priv->requesting_axis_range = TRUE;
            gwy_axis_request_range(axis, &arearange);
            priv->requesting_axis_range = FALSE;
        }
    }
}

static void
field_notify(GwyRasterView *rasterview,
             GParamSpec *pspec,
             GwyField *field)
{
    RasterView *priv = rasterview->priv;
    if (!pspec || gwy_stramong(pspec->name,
                               "xres", "yres",
                               "xreal", "yreal",
                               "xoff", "yoff",
                               NULL)) {
        update_ruler_ranges(rasterview);
    }
    if (!pspec || gwy_strequal(pspec->name, "unit-xy")) {
        if (priv->scale_type == GWY_RULER_SCALE_REAL)
            set_ruler_units_to_field(rasterview);
    }
    if (!pspec || gwy_strequal(pspec->name, "unit-z")) {
        GwyUnit *colorunit = gwy_axis_get_unit(GWY_AXIS(priv->coloraxis));
        GwyUnit *zunit = gwy_field_get_unit_z(field);
        gwy_unit_assign(colorunit, zunit);
    }
}

static void
set_ruler_units_to_field(GwyRasterView *rasterview)
{
    RasterView *priv = rasterview->priv;
    GwyUnit *hunit = gwy_axis_get_unit(GWY_AXIS(priv->hruler));
    GwyUnit *vunit = gwy_axis_get_unit(GWY_AXIS(priv->vruler));
    GwyUnit *xyunit = gwy_field_get_unit_xy(priv->field);
    gwy_unit_assign(hunit, xyunit);
    gwy_unit_assign(vunit, xyunit);
}

static void
field_data_changed(G_GNUC_UNUSED GwyRasterView *rasterview,
                   G_GNUC_UNUSED const GwyFieldPart *fpart,
                   G_GNUC_UNUSED GwyField *field)
{
    // RasterView *priv = rasterview->priv;
    // XXX: Do we want to do anything here?
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

    if (field) {
        field_notify(rasterview, NULL, field);
        field_data_changed(rasterview, NULL, field);
    }

    update_ruler_ranges(rasterview);
    return TRUE;
}

static void
update_ruler_ranges(GwyRasterView *rasterview)
{
    RasterView *priv = rasterview->priv;
    GwyRasterArea *rasterarea = GWY_RASTER_AREA(priv->area);
    cairo_rectangle_t area;
    gwy_raster_area_widget_area(rasterarea, &area);
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

static gboolean
ruler_button_press(GwyRasterView *rasterview,
                   GdkEventButton *event,
                   GwyRuler *ruler)
{
    if (!gdk_event_triggers_context_menu((GdkEvent*)event)
        || event->type != GDK_BUTTON_PRESS)
        return FALSE;

    pop_up_ruler_menu(rasterview, GTK_WIDGET(ruler), event);
    return TRUE;
}

static void
axis_range_notify(GwyRasterView *rasterview,
                  GParamSpec *pspec,
                  GwyColorAxis *coloraxis)
{
    g_return_if_fail(gwy_strequal(pspec->name, "range"));
    GwyRasterArea *area = rasterview->priv->area;
    GwyRange axisrange, arearange;
    gwy_axis_get_range(GWY_AXIS(coloraxis), &axisrange);
    gwy_raster_area_get_user_range(area, &arearange);
    if (gwy_equal(&axisrange, &arearange))
        return;

    if (rasterview->priv->requesting_axis_range) {
        g_warning("Recursion in false colour mapping range area/axis sync?!");
        return;
    }

    gwy_raster_area_set_user_range(area, &axisrange);
    if (arearange.from != axisrange.from)
        gwy_raster_area_set_range_from_method(area, GWY_COLOR_RANGE_USER);
    if (arearange.to != axisrange.to)
        gwy_raster_area_set_range_to_method(area, GWY_COLOR_RANGE_USER);
}

static GtkWidget*
create_axis_button_box(GwyRasterView *rasterview)
{
    RasterView *priv = rasterview->priv;

    GtkWidget *buttonbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);

    GtkWidget *axisbutton = create_axis_button(NULL, AXIS_TAB_AXIS,
                                               GTK_STOCK_YES);
    GtkRadioButton *groupwidget = GTK_RADIO_BUTTON(axisbutton);
    priv->axisbutton = GTK_TOGGLE_BUTTON(axisbutton);
    gtk_box_pack_start(GTK_BOX(buttonbox), axisbutton, TRUE, TRUE, 0);

    GtkWidget *gradbutton = create_axis_button(groupwidget, AXIS_TAB_GRADIENTS,
                                               GTK_STOCK_NO);
    priv->gradbutton = GTK_TOGGLE_BUTTON(gradbutton);
    gtk_box_pack_start(GTK_BOX(buttonbox), gradbutton, TRUE, TRUE, 0);

    GtkWidget *rangebutton = create_axis_button(groupwidget, AXIS_TAB_RANGES,
                                                GTK_STOCK_CANCEL);
    priv->rangebutton = GTK_TOGGLE_BUTTON(rangebutton);
    gtk_box_pack_start(GTK_BOX(buttonbox), rangebutton, TRUE, TRUE, 0);

    g_signal_connect_swapped(axisbutton, "toggled",
                             G_CALLBACK(axis_button_clicked), rasterview);
    g_signal_connect_swapped(gradbutton, "toggled",
                             G_CALLBACK(axis_button_clicked), rasterview);
    g_signal_connect_swapped(rangebutton, "toggled",
                             G_CALLBACK(axis_button_clicked), rasterview);

    return buttonbox;
}

static GtkWidget*
create_axis_button(GtkRadioButton *groupwidget,
                   guint id,
                   const gchar *content)
{
    GtkWidget *button = gtk_radio_button_new_from_widget(groupwidget);
    g_object_set_data(G_OBJECT(button), "id", GUINT_TO_POINTER(id));
    gtk_widget_set_name(button, "gwy-raster-view-axis-button");
    gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(button), FALSE);
    gtk_widget_set_can_focus(button, FALSE);
    gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
    gtk_button_set_alignment(GTK_BUTTON(button), 0.5, 0.5);
    GtkWidget *child = gtk_image_new_from_stock(content, GTK_ICON_SIZE_MENU);
    gtk_container_add(GTK_CONTAINER(button), child);
    GtkStyleContext *context = gtk_widget_get_style_context(button);
    GtkStyleProvider *provider = gwy_get_style_provider();
    gtk_style_context_add_provider(context, provider,
                                   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    return button;
}

static void
axis_button_clicked(GwyRasterView *rasterview,
                    GtkToggleButton *button)
{
    if (!gtk_toggle_button_get_active(button))
        return;

    guint id = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(button), "id"));
    GtkGrid *grid = GTK_GRID(rasterview);
    GtkWidget *child = gtk_grid_get_child_at(grid, 3, 2);
    gtk_container_remove(GTK_CONTAINER(rasterview), child);

    RasterView *priv = rasterview->priv;
    if (id == AXIS_TAB_AXIS)
        child = GTK_WIDGET(priv->coloraxis);
    else if (id == AXIS_TAB_RANGES) {
        if (!priv->ranges) {
            priv->ranges = create_range_controls(rasterview);
            g_object_ref(priv->ranges);
        }
        child = priv->ranges;
    }
    else if (id == AXIS_TAB_GRADIENTS) {
        if (!priv->gradients) {
            priv->gradients = create_gradient_list(rasterview);
            g_object_ref(priv->gradients);
        }
        child = priv->gradients;
    }
    else {
        g_assert_not_reached();
    }
    gtk_widget_show_all(child);
    gtk_grid_attach(grid, child, 3, 2, 1, 1);
}

static GtkWidget*
create_range_controls(G_GNUC_UNUSED GwyRasterView *rasterview)
{
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(vbox), gtk_label_new("Ranges..."));
    return vbox;
}

static GtkWidget*
create_gradient_list(G_GNUC_UNUSED GwyRasterView *rasterview)
{
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(vbox), gtk_label_new("Gradients..."));
    return vbox;
}

static void
pop_up_ruler_menu(GwyRasterView *rasterview,
                  GtkWidget *widget,
                  GdkEventButton *event)
{
    RasterView *priv = rasterview->priv;
    gint button = event ? event->button : 0;
    gint event_time = event ? event->time : gtk_get_current_event_time();

    if (!priv->ruler_popup)
        priv->ruler_popup = create_ruler_popup(rasterview);

    gtk_menu_attach_to_widget(priv->ruler_popup, widget, NULL);
    gtk_menu_popup(priv->ruler_popup, NULL, NULL, NULL, NULL,
                   button, event_time);
}

static GtkMenu*
create_ruler_popup(GwyRasterView *rasterview)
{
    RasterView *priv = rasterview->priv;
    GtkWidget *menu = gtk_menu_new();
    GtkMenuShell *shell = GTK_MENU_SHELL(menu);
    GtkWidget *item;

    item = gtk_radio_menu_item_new_with_label(NULL, _("Pixel Distances"));
    gtk_menu_shell_append(shell, item);
    priv->pixel_distances = GTK_CHECK_MENU_ITEM(item);
    if (priv->scale_type == GWY_RULER_SCALE_PIXEL)
        gtk_check_menu_item_set_active(priv->pixel_distances, TRUE);

    item = gtk_radio_menu_item_new_with_label_from_widget
                            (GTK_RADIO_MENU_ITEM(item), _("Real Distances"));
    gtk_menu_shell_append(shell, item);
    priv->real_distances = GTK_CHECK_MENU_ITEM(item);
    if (priv->scale_type == GWY_RULER_SCALE_REAL)
        gtk_check_menu_item_set_active(priv->real_distances, TRUE);

    item = gtk_separator_menu_item_new();
    gtk_menu_shell_append(shell, item);

    gboolean real_aspect_ratio;
    g_object_get(priv->area, "real-aspect-ratio", &real_aspect_ratio, NULL);

    item = gtk_radio_menu_item_new_with_label(NULL, _("Square Pixels"));
    gtk_menu_shell_append(shell, item);
    priv->square_pixels = GTK_CHECK_MENU_ITEM(item);
    if (!real_aspect_ratio)
        gtk_check_menu_item_set_active(priv->square_pixels, TRUE);

    item = gtk_radio_menu_item_new_with_label_from_widget
                            (GTK_RADIO_MENU_ITEM(item), _("Real Aspect Ratio"));
    gtk_menu_shell_append(shell, item);
    priv->real_aspect_ratio = GTK_CHECK_MENU_ITEM(item);
    if (real_aspect_ratio)
        gtk_check_menu_item_set_active(priv->real_aspect_ratio, TRUE);

    gtk_widget_show_all(menu);

    g_signal_connect_swapped(priv->pixel_distances, "toggled",
                             G_CALLBACK(ruler_popup_item_toggled), rasterview);
    g_signal_connect_swapped(priv->real_distances, "toggled",
                             G_CALLBACK(ruler_popup_item_toggled), rasterview);
    g_signal_connect_swapped(priv->square_pixels, "toggled",
                             G_CALLBACK(ruler_popup_item_toggled), rasterview);
    g_signal_connect_swapped(priv->real_aspect_ratio, "toggled",
                             G_CALLBACK(ruler_popup_item_toggled), rasterview);
    g_signal_connect(menu, "deactivate",
                     G_CALLBACK(ruler_popup_deleted), NULL);
    // Prevent the menu from being destroyed on deactivation.
    g_object_ref(menu);

    return GTK_MENU(menu);
}

static void
ruler_popup_deleted(GtkWidget *menu)
{
    gtk_widget_hide(menu);
    gtk_menu_detach(GTK_MENU(menu));
}

static void
ruler_popup_item_toggled(GwyRasterView *rasterview,
                         GtkCheckMenuItem *menuitem)
{
    if (!gtk_check_menu_item_get_active(menuitem))
        return;

    RasterView *priv = rasterview->priv;
    if (menuitem == priv->pixel_distances)
        gwy_raster_view_set_scale_type(rasterview, GWY_RULER_SCALE_PIXEL);
    else if (menuitem == priv->real_distances)
        gwy_raster_view_set_scale_type(rasterview, GWY_RULER_SCALE_REAL);
    else if (menuitem == priv->square_pixels)
        g_object_set(priv->area, "real-aspect-ratio", FALSE, NULL);
    else if (menuitem == priv->real_aspect_ratio)
        g_object_set(priv->area, "real-aspect-ratio", TRUE, NULL);
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
