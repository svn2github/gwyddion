/*
 *  @(#) $Id$
 *  Copyright (C) 2003,2004 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@physics.muni.cz, klapetek@physics.muni.cz.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 */

#include <math.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwycontainer.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwydgets.h>
#include <app/settings.h>
#include <app/app.h>

typedef struct {
    gboolean is_visible;  /* XXX: GTK_WIDGET_VISIBLE() returns BS? */
    GtkWidget *coords[6];
    GtkWidget *values[3];
    GtkWidget *windowname;
    GtkObject *radius;
    gdouble mag;
    gint precision;
    gchar *units;
} Level3Controls;

static gboolean   module_register               (const gchar *name);
static void       level3_use                    (GwyDataWindow *data_window,
                                                 GwyToolSwitchEvent reason);
static GtkWidget* level3_dialog_create          (GwyDataWindow *data_window);
static void       level3_do                     (void);
static gdouble    level3_get_z_average          (GwyDataField *dfield,
                                                 gdouble xreal,
                                                 gdouble yreal,
                                                 gint radius);
static void       level3_selection_updated_cb   (void);
static void       level3_data_updated_cb        (void);
static void       level3_update_view            (void);
static void       level3_dialog_response_cb     (gpointer unused,
                                                 gint response);
static void       level3_dialog_abandon         (void);
static void       level3_dialog_set_visible     (gboolean visible);

static const gchar *radius_key = "/tool/level3/radius";

static GtkWidget *level3_dialog = NULL;
static Level3Controls controls;
static gulong layer_updated_id = 0;
static gulong data_updated_id = 0;
static gulong response_id = 0;
static GwyDataViewLayer *points_layer = NULL;

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "level3",
    "Level tool.  Allows to level data by fitting a plane through three "
        "selected points.",
    "Yeti <yeti@physics.muni.cz>",
    "1.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2003",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyToolFuncInfo level3_func_info = {
        "level3",
        "gwy_fit_triangle",
        "Level data using three points.",
        50,
        level3_use,
    };

    gwy_tool_func_register(name, &level3_func_info);

    return TRUE;
}

static void
level3_use(GwyDataWindow *data_window,
           GwyToolSwitchEvent reason)
{
    GwyVectorLayer *layer;
    GwyDataView *data_view;

    gwy_debug("%p", data_window);

    if (!data_window) {
        level3_dialog_abandon();
        return;
    }
    g_return_if_fail(GWY_IS_DATA_WINDOW(data_window));
    data_view = (GwyDataView*)gwy_data_window_get_data_view(data_window);
    layer = gwy_data_view_get_top_layer(data_view);
    if (layer && (GwyDataViewLayer*)layer == points_layer)
        return;
    if (points_layer) {
        if (layer_updated_id)
        g_signal_handler_disconnect(points_layer, layer_updated_id);
        if (points_layer->parent && data_updated_id)
            g_signal_handler_disconnect(points_layer->parent, data_updated_id);
    }

    if (layer && GWY_IS_LAYER_POINTS(layer)) {
        points_layer = GWY_DATA_VIEW_LAYER(layer);
        gwy_layer_points_set_max_points(GWY_LAYER_POINTS(layer), 3);
    }
    else {
        points_layer = (GwyDataViewLayer*)gwy_layer_points_new();
        gwy_layer_points_set_max_points(GWY_LAYER_POINTS(points_layer), 3);
        gwy_data_view_set_top_layer(data_view, GWY_VECTOR_LAYER(points_layer));
    }
    if (!level3_dialog)
        level3_dialog = level3_dialog_create(data_window);

    layer_updated_id = g_signal_connect(points_layer, "updated",
                                        G_CALLBACK(level3_selection_updated_cb),
                                        NULL);
    data_updated_id = g_signal_connect(data_view, "updated",
                                       G_CALLBACK(level3_data_updated_cb),
                                       NULL);
    if (reason == GWY_TOOL_SWITCH_TOOL)
        level3_dialog_set_visible(TRUE);
    /* FIXME: window name can change also when saving under different name */
    if (reason == GWY_TOOL_SWITCH_WINDOW)
        gtk_label_set_text(GTK_LABEL(controls.windowname),
                           gwy_data_window_get_base_name(data_window));
    if (controls.is_visible)
        level3_selection_updated_cb();
}

static void
level3_do(void)
{
    GwyContainer *data;
    GwyDataField *dfield;
    gdouble points[6], z[3];
    gdouble bx, by, c, det;
    gint i, radius;

    if (gwy_layer_points_get_points(GWY_LAYER_POINTS(points_layer), points) < 3)
        return;

    data = gwy_data_view_get_data(GWY_DATA_VIEW(points_layer->parent));
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    radius = (gint)gtk_adjustment_get_value(GTK_ADJUSTMENT(controls.radius));

    /* find the plane levelling coeffs so that values in the three points
     * will be all zeroes */
    det = bx = by = c = 0;
    for (i = 0; i < 3; i++)
        z[i] = level3_get_z_average(dfield, points[2*i], points[2*i+1], radius);
    det = points[0]*(points[3] - points[5])
          + points[2]*(points[5] - points[1])
          + points[4]*(points[1] - points[3]);
    bx = z[0]*(points[3] - points[5])
         + z[1]*(points[5] - points[1])
         + z[2]*(points[1] - points[3]);
    by = z[0]*(points[4] - points[2])
         + z[1]*(points[0] - points[4])
         + z[2]*(points[2] - points[0]);
    c = z[0]*(points[2]*points[5] - points[3]*points[4])
         + z[1]*(points[1]*points[4] - points[5]*points[0])
         + z[2]*(points[0]*points[3] - points[1]*points[2]);
    gwy_debug("bx = %g, by = %g, c = %g, det = %g", bx, by, c, det);
    bx /= det;
    by /= det;
    c /= det;
    /* to keep mean value intact, the mean value of the plane we add to the
     * data has to be zero, i.e., in the center of the data the value must
     * be zero */
    c = -0.5*(bx*gwy_data_field_get_xreal(dfield)
              + by*gwy_data_field_get_yreal(dfield));
    gwy_debug("bx = %g, by = %g, c = %g", bx, by, c);
    gwy_debug("z[0] = %g, z[1] = %g, z[2] = %g", z[0], z[1], z[2]);
    gwy_debug("zn[0] = %g, zn[1] = %g, zn[2] = %g",
              z[0] - c - bx*points[0] - by*points[1],
              z[1] - c - bx*points[2] - by*points[3],
              z[2] - c - bx*points[4] - by*points[5]);
    gwy_app_undo_checkpoint(data, "/0/data");
    gwy_data_field_plane_level(dfield, c, bx, by);
    gwy_debug("zN[0] = %g, zN[1] = %g, zN[2] = %g",
              level3_get_z_average(dfield, points[0], points[1], radius),
              level3_get_z_average(dfield, points[2], points[3], radius),
              level3_get_z_average(dfield, points[4], points[5], radius));
    gwy_vector_layer_unselect(GWY_VECTOR_LAYER(points_layer));
    gwy_data_view_update(GWY_DATA_VIEW(points_layer->parent));
}

static gdouble
level3_get_z_average(GwyDataField *dfield,
                     gdouble xreal,
                     gdouble yreal,
                     gint radius)
{
    gint x, y, xres, yres, uli, ulj, bri, brj;

    if (radius < 1)
        g_warning("Bad averaging radius %d, fixing to 1", radius);
    x = gwy_data_field_rtoj(dfield, xreal);
    y = gwy_data_field_rtoi(dfield, yreal);
    if (radius < 2)
        return gwy_data_field_get_val(dfield, x, y);
    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    ulj = CLAMP(x - radius, 0, xres - 1);
    uli = CLAMP(y - radius, 0, yres - 1);
    brj = CLAMP(x + radius, 0, xres - 1);
    bri = CLAMP(y + radius, 0, yres - 1);

    return gwy_data_field_get_area_avg(dfield, ulj, uli, brj, bri);
}

static void
level3_dialog_abandon(void)
{
    GwyContainer *settings;
    gint radius;

    if (points_layer) {
        if (layer_updated_id)
        g_signal_handler_disconnect(points_layer, layer_updated_id);
        if (points_layer->parent && data_updated_id)
            g_signal_handler_disconnect(points_layer->parent, data_updated_id);
    }
    layer_updated_id = 0;
    data_updated_id = 0;
    points_layer = NULL;
    if (level3_dialog) {
        radius = (gint)gtk_adjustment_get_value(GTK_ADJUSTMENT(controls.radius));
        radius = CLAMP(radius, 1, 16);
        settings = gwy_app_settings_get();
        gwy_container_set_int32_by_name(settings, radius_key, radius);
        g_signal_handler_disconnect(level3_dialog, response_id);
        gtk_widget_destroy(level3_dialog);
        level3_dialog = NULL;
        response_id = 0;
        g_free(controls.units);
        controls.is_visible = FALSE;
    }
}

static GtkWidget*
level3_dialog_create(GwyDataWindow *data_window)
{
    GwyContainer *data, *settings;
    GwyDataField *dfield;
    GtkWidget *dialog, *table, *label, *frame;
    gdouble xreal, yreal, max, unit;
    gint radius;
    guchar *buffer;
    gint i;

    gwy_debug("");
    data = gwy_data_window_get_data(data_window);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    xreal = gwy_data_field_get_xreal(dfield);
    yreal = gwy_data_field_get_yreal(dfield);
    max = MAX(xreal, yreal);
    unit = MIN(xreal/gwy_data_field_get_xres(dfield),
               yreal/gwy_data_field_get_yres(dfield));
    controls.mag = gwy_math_humanize_numbers(unit, max, &controls.precision);
    controls.units = g_strconcat(gwy_math_SI_prefix(controls.mag), "m", NULL);

    dialog = gtk_dialog_new_with_buttons(_("Level"),
                                         NULL,
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_STOCK_APPLY, GTK_RESPONSE_APPLY,
                                         GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                                         NULL);
    g_signal_connect(dialog, "delete_event",
                     G_CALLBACK(gwy_dialog_prevent_delete_cb), NULL);
    response_id = g_signal_connect(dialog, "response",
                                   G_CALLBACK(level3_dialog_response_cb), NULL);

    frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_OUT);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), frame,
                       FALSE, FALSE, 0);
    label = gtk_label_new(gwy_data_window_get_base_name(data_window));
    controls.windowname = label;
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_misc_set_padding(GTK_MISC(label), 4, 2);
    gtk_container_add(GTK_CONTAINER(frame), label);

    table = gtk_table_new(4, 4, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, TRUE, TRUE, 0);

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>X</b>"));
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 0, 1, GTK_FILL, 0, 2, 2);
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Y</b>"));
    gtk_table_attach(GTK_TABLE(table), label, 2, 3, 0, 1, GTK_FILL, 0, 2, 2);
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Value</b>"));
    gtk_table_attach(GTK_TABLE(table), label, 3, 4, 0, 1, GTK_FILL, 0, 2, 2);
    for (i = 0; i < 3; i++) {
        label = gtk_label_new(NULL);
        buffer = g_strdup_printf(_("<b>%d</b>"), i+1);
        gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
        gtk_label_set_markup(GTK_LABEL(label), buffer);
        g_free(buffer);
        gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
        gtk_table_attach(GTK_TABLE(table), label, 0, 1, i+1, i+2, 0, 0, 2, 2);
        label = controls.coords[2*i] = gtk_label_new("");
        gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
        gtk_table_attach(GTK_TABLE(table), label, 1, 2, i+1, i+2, 0, 0, 2, 2);
        label = controls.coords[2*i + 1] = gtk_label_new("");
        gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
        gtk_table_attach(GTK_TABLE(table), label, 2, 3, i+1, i+2, 0, 0, 2, 2);
        label = controls.values[i] = gtk_label_new("");
        gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
        gtk_table_attach(GTK_TABLE(table), label, 3, 4, i+1, i+2,
                         GTK_EXPAND | GTK_FILL, 0, 2, 2);
    }

    table = gtk_table_new(1, 3, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, TRUE, TRUE, 0);

    settings = gwy_app_settings_get();
    if (gwy_container_contains_by_name(settings, radius_key))
        radius = gwy_container_get_int32_by_name(settings, radius_key);
    else
        radius = 1;
    controls.radius = gtk_adjustment_new((gdouble)radius, 1, 16, 1, 5, 16);
    gwy_table_attach_spinbutton(table, 9, "Averaging radius", "px",
                                controls.radius);
    g_signal_connect(controls.radius, "value_changed",
                     G_CALLBACK(level3_selection_updated_cb), NULL);
    gtk_widget_show_all(GTK_DIALOG(dialog)->vbox);
    controls.is_visible = FALSE;

    return dialog;
}

static void
update_coord_label(GtkWidget *label, gdouble value)
{
    gchar buffer[16];

    g_snprintf(buffer, sizeof(buffer), "%.*f %s",
               controls.precision, value/controls.mag, controls.units);
    gtk_label_set_text(GTK_LABEL(label), buffer);
}

static void
update_value_label(GtkWidget *label, gdouble value)
{
    gchar buffer[16];

    g_snprintf(buffer, sizeof(buffer), "%g", value);
    gtk_label_set_text(GTK_LABEL(label), buffer);
}

static void
level3_selection_updated_cb(void)
{
    gint nselected;

    gwy_debug("");
    nselected = gwy_vector_layer_get_nselected(GWY_VECTOR_LAYER(points_layer));
    level3_update_view();
    if (nselected && !controls.is_visible)
        level3_dialog_set_visible(TRUE);
}

static void
level3_data_updated_cb(void)
{
    gwy_debug("");
    level3_update_view();
}

static void
level3_update_view(void)
{
    GwyContainer *data;
    GwyDataField *dfield;
    gdouble points[6];
    gboolean is_visible;
    gdouble val;
    gint nselected, i, radius;

    gwy_debug("");

    data = gwy_data_view_get_data(GWY_DATA_VIEW(points_layer->parent));
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    radius = (gint)gtk_adjustment_get_value(GTK_ADJUSTMENT(controls.radius));

    is_visible = controls.is_visible;
    nselected = gwy_layer_points_get_points(GWY_LAYER_POINTS(points_layer),
                                            points);
    if (!is_visible && !nselected)
        return;
    for (i = 0; i < 6; i++) {
        if (i < 2*nselected) {
            update_coord_label(controls.coords[i], points[i]);
            if (i%2 == 0) {
                val = level3_get_z_average(dfield, points[i], points[i+1],
                                           radius);
                update_value_label(controls.values[i/2], val);
            }
        }
        else {
            gtk_label_set_text(GTK_LABEL(controls.coords[i]), "");
            if (i%2 == 0)
                gtk_label_set_text(GTK_LABEL(controls.values[i/2]), "");
        }
    }
}

static void
level3_dialog_response_cb(G_GNUC_UNUSED gpointer unused,
                          gint response)
{
    gwy_debug("response %d", response);
    switch (response) {
        case GTK_RESPONSE_CLOSE:
        case GTK_RESPONSE_DELETE_EVENT:
        level3_dialog_set_visible(FALSE);
        break;

        case GTK_RESPONSE_NONE:
        g_warning("Tool dialog destroyed.");
        level3_use(NULL, 0);
        break;

        case GTK_RESPONSE_APPLY:
        level3_do();
        level3_dialog_set_visible(FALSE);
        break;

        default:
        g_assert_not_reached();
        break;
    }
}

static void
level3_dialog_set_visible(gboolean visible)
{
    gwy_debug("now %d, setting to %d",
              controls.is_visible, visible);
    if (controls.is_visible == visible)
        return;

    controls.is_visible = visible;
    if (visible)
        gtk_window_present(GTK_WINDOW(level3_dialog));
    else
        gtk_widget_hide(level3_dialog);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

