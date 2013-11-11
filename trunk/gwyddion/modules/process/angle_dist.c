/*
 *  @(#) $Id$
 *  Copyright (C) 2004 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule-process.h>
#include <libprocess/level.h>
#include <libgwydgets/gwydgetutils.h>
#include <app/gwyapp.h>

#define ANGLE_DIST_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

enum {
    MAX_OUT_SIZE = 1024,
    MAX_STEPS = 65536
};

typedef struct {
    gint size;
    gint steps;
    gboolean logscale;
    gboolean fit_plane;
    gint kernel_size;
} AngleArgs;

typedef struct {
    GtkObject *size;
    GtkObject *steps;
    GtkWidget *logscale;
    GtkWidget *fit_plane;
    GtkObject *kernel_size;
    gboolean in_update;
} AngleControls;

static gboolean      module_register              (void);
static void          angle_dist                   (GwyContainer *data,
                                                   GwyRunType run);
static gboolean      angle_dialog                 (AngleArgs *args);
static void          angle_dialog_update_controls (AngleControls *controls,
                                                   AngleArgs *args);
static void          angle_dialog_update_values   (AngleControls *controls,
                                                   AngleArgs *args);
static void          angle_fit_plane_cb           (GtkToggleButton *check,
                                                   AngleControls *controls);
static GwyDataField* angle_do                     (GwyDataField *dfield,
                                                   AngleArgs *args);
static gdouble       compute_slopes               (GwyDataField *dfield,
                                                   gint kernel_size,
                                                   gdouble *xder,
                                                   gdouble *yder);
static gboolean      count_angles                 (gint n,
                                                   gdouble *xder,
                                                   gdouble *yder,
                                                   gdouble max,
                                                   gint size,
                                                   gulong *count,
                                                   gint steps);
static GwyDataField* make_datafield               (GwyDataField *old,
                                                   gint res,
                                                   gulong *count,
                                                   gdouble real,
                                                   gboolean logscale);
static void          load_args                    (GwyContainer *container,
                                                   AngleArgs *args);
static void          save_args                    (GwyContainer *container,
                                                   AngleArgs *args);
static void          sanitize_args                (AngleArgs *args);

static const AngleArgs angle_defaults = {
    200,
    360,
    FALSE,
    FALSE,
    5,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Calculates two-dimensional distribution of angles, "
       "that is projections of slopes to all directions."),
    "Yeti <yeti@gwyddion.net>",
    "1.6",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("angle_dist",
                              (GwyProcessFunc)&angle_dist,
                              N_("/_Statistics/An_gle Distribution..."),
                              NULL,
                              ANGLE_DIST_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Calculate two-dimensional angle "
                                 "distribution"));

    return TRUE;
}

static void
angle_dist(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield;
    AngleArgs args;
    gint oldid, newid;
    gboolean ok;

    g_return_if_fail(run & ANGLE_DIST_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &oldid,
                                     NULL);
    g_return_if_fail(dfield);
    load_args(gwy_app_settings_get(), &args);
    if (run == GWY_RUN_INTERACTIVE) {
        ok = angle_dialog(&args);
        save_args(gwy_app_settings_get(), &args);
        if (!ok)
            return;
    }

    gwy_app_wait_start(gwy_app_find_window_for_channel(data, oldid),
                       _("Computing angle distribution"));
    dfield = angle_do(dfield, &args);
    gwy_app_wait_finish();
    if (!dfield)
        return;

    newid = gwy_app_data_browser_add_data_field(dfield, data, TRUE);
    g_object_unref(dfield);
    gwy_app_sync_data_items(data, data, oldid, newid, FALSE,
                            GWY_DATA_ITEM_PALETTE,
                            0);
    gwy_app_set_data_field_title(data, newid, _("Angle distribution"));
}

static gboolean
angle_dialog(AngleArgs *args)
{
    GtkWidget *dialog, *table;
    AngleControls controls;
    enum { RESPONSE_RESET = 1 };
    gint response;
    gint row;

    dialog = gtk_dialog_new_with_buttons(_("Angle Distribution"), NULL, 0,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    table = gtk_table_new(5, 4, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table,
                       FALSE, FALSE, 4);
    row = 0;

    controls.size = gtk_adjustment_new(args->size, 10, MAX_OUT_SIZE, 1, 10, 0);
    gwy_table_attach_hscale(table, row, _("Output _size:"), "px",
                            controls.size, 0);
    row++;

    controls.steps = gtk_adjustment_new(args->steps, 6, MAX_STEPS, 1, 10, 0);
    gwy_table_attach_hscale(table, row, _("_Number of steps:"), NULL,
                            controls.steps, GWY_HSCALE_SQRT);
    row++;

    controls.logscale
        = gtk_check_button_new_with_mnemonic(_("_Logarithmic value scale"));
    gtk_table_attach(GTK_TABLE(table), controls.logscale,
                     0, 4, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    controls.fit_plane
        = gtk_check_button_new_with_mnemonic(_("Use local plane _fitting"));
    gtk_table_attach(GTK_TABLE(table), controls.fit_plane,
                     0, 4, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_signal_connect(controls.fit_plane, "toggled",
                     G_CALLBACK(angle_fit_plane_cb), &controls);
    row++;

    controls.kernel_size = gtk_adjustment_new(args->kernel_size,
                                              2, 16, 1, 4, 0);
    gwy_table_attach_hscale(table, row, _("_Plane size:"), "px",
                            controls.kernel_size, 0);
    row++;

    angle_dialog_update_controls(&controls, args);

    gtk_widget_show_all(dialog);
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            angle_dialog_update_values(&controls, args);
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            return FALSE;
            break;

            case GTK_RESPONSE_OK:
            break;

            case RESPONSE_RESET:
            *args = angle_defaults;
            angle_dialog_update_controls(&controls, args);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    angle_dialog_update_values(&controls, args);
    gtk_widget_destroy(dialog);

    return TRUE;
}

static void
angle_dialog_update_controls(AngleControls *controls,
                             AngleArgs *args)
{
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->size),
                             args->size);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->steps),
                             args->steps);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->kernel_size),
                             args->kernel_size);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->logscale),
                                 args->logscale);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->fit_plane),
                                 args->fit_plane);
    gwy_table_hscale_set_sensitive(controls->kernel_size, args->fit_plane);
}

static void
angle_dialog_update_values(AngleControls *controls,
                           AngleArgs *args)
{
    args->size = gwy_adjustment_get_int(controls->size);
    args->steps = gwy_adjustment_get_int(controls->steps);
    args->logscale
        = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->logscale));
    args->kernel_size = gwy_adjustment_get_int(controls->kernel_size);
    args->fit_plane =
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->fit_plane));
}

static void
angle_fit_plane_cb(GtkToggleButton *check,
                   AngleControls *controls)
{
    gwy_table_hscale_set_sensitive(controls->kernel_size,
                                   gtk_toggle_button_get_active(check));
}

static GwyDataField*
angle_do(GwyDataField *dfield,
         AngleArgs *args)
{
    gdouble *xder, *yder;
    gdouble max;
    gint xres, yres, n;
    gulong *count;
    gboolean ok;

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);

    n = args->fit_plane ? args->kernel_size : 2;
    n = (xres - n)*(yres - n);
    xder = g_new(gdouble, n);
    yder = g_new(gdouble, n);
    max = compute_slopes(dfield, args->fit_plane ? args->kernel_size : 0,
                         xder, yder);
    count = g_new0(gulong, args->size*args->size);
    ok = count_angles(n, xder, yder, max, args->size, count, args->steps);
    g_free(yder);
    g_free(xder);
    if (!ok) {
        g_free(count);
        return NULL;
    }

    return make_datafield(dfield, args->size, count, 2.0*G_PI, args->logscale);
}

static gdouble
compute_slopes(GwyDataField *dfield,
               gint kernel_size,
               gdouble *xder,
               gdouble *yder)
{
    const gdouble *data;
    gdouble qx, qy;
    gdouble dx, dy;
    gdouble d, max;
    gint xres, yres;
    gint col, row;

    data = gwy_data_field_get_data_const(dfield);
    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    max = 0.0;
    if (kernel_size) {
        for (row = 0; row + kernel_size < yres; row++) {
            for (col = 0; col + kernel_size < xres; col++) {
                gwy_data_field_area_fit_plane(dfield, NULL, col, row,
                                              kernel_size, kernel_size,
                                              NULL, &dx, &dy);
                *(xder++) = dx;
                *(yder++) = dy;
                d = dx*dx + dy*dy;
                max = MAX(d, max);
            }
        }
    }
    else {
        qx = xres/gwy_data_field_get_xreal(dfield);
        qy = yres/gwy_data_field_get_yreal(dfield);
        for (row = 1; row + 1 < yres; row++) {
            for (col = 1; col + 1 < xres; col++) {
                dx = data[row*xres + col + 1] - data[row*xres + col - 1];
                dx *= qx;
                *(xder++) = dx;
                dy = data[row*xres + xres + col] - data[row*xres - xres + col];
                dy *= qy;
                *(yder++) = dy;
                d = dx*dx + dy*dy;
                max = MAX(d, max);
            }
        }
    }

    return max;
}


static gboolean
count_angles(gint n, gdouble *xder, gdouble *yder,
             gdouble max,
             gint size, gulong *count,
             gint steps)
{
    gint xider, yider, i, j;
    gdouble *ct, *st;
    gdouble d, phi;
    gboolean ok = TRUE;

    max = atan(sqrt(max));
    gwy_debug("max = %g", max);

    ct = g_new(gdouble, steps);
    st = g_new(gdouble, steps);
    for (j = 0; j < steps; j++) {
        gdouble theta = 2.0*G_PI*j/steps;

        ct[j] = cos(theta);
        st[j] = sin(theta);
    }

    for (i = 0; i < n; i++) {
        gdouble xd = *(xder++);
        gdouble yd = *(yder++);

        d = atan(hypot(xd, yd));
        phi = atan2(yd, xd);
        for (j = 0; j < steps; j++) {
            gdouble v = d*cos(2.0*G_PI*j/steps - phi);

            xider = size*(v*ct[j]/(2.0*max) + 0.5);
            xider = CLAMP(xider, 0, size-1);
            yider = size*(v*st[j]/(2.0*max) + 0.5);
            yider = CLAMP(yider, 0, size-1);

            count[yider*size + xider]++;
        }
        if (i % 4096 == 0) {
            if (!gwy_app_wait_set_fraction((gdouble)i/n)) {
                ok = FALSE;
                break;
            }
        }
    }

    g_free(ct);
    g_free(st);

    return ok;
}

static GwyDataField*
make_datafield(G_GNUC_UNUSED GwyDataField *old,
               gint res, gulong *count,
               gdouble real, gboolean logscale)
{
    GwyDataField *dfield;
    GwySIUnit *unit;
    gdouble *d;
    gint i;

    dfield = gwy_data_field_new(res, res, real, real, FALSE);
    gwy_data_field_set_xoffset(dfield, -gwy_data_field_get_xreal(dfield)/2);
    gwy_data_field_set_yoffset(dfield, -gwy_data_field_get_yreal(dfield)/2);

    unit = gwy_si_unit_new("");
    gwy_data_field_set_si_unit_z(dfield, unit);
    g_object_unref(unit);
    unit = gwy_si_unit_new("");
    gwy_data_field_set_si_unit_xy(dfield, unit);
    g_object_unref(unit);

    d = gwy_data_field_get_data(dfield);
    if (logscale) {
        for (i = 0; i < res*res; i++)
            d[i] = count[i] ? log((gdouble)count[i]) + 1.0 : 0.0;
    }
    else {
        for (i = 0; i < res*res; i++)
            d[i] = count[i];
    }
    g_free(count);

    return dfield;
}

static const gchar size_key[]        = "/module/angle_dist/size";
static const gchar steps_key[]       = "/module/angle_dist/steps";
static const gchar logscale_key[]    = "/module/angle_dist/logscale";
static const gchar fit_plane_key[]   = "/module/angle_dist/fit_plane";
static const gchar kernel_size_key[] = "/module/angle_dist/kernel_size";

static void
sanitize_args(AngleArgs *args)
{
    args->size = CLAMP(args->size, 1, MAX_OUT_SIZE);
    args->steps = CLAMP(args->steps, 1, 16384);
    args->kernel_size = CLAMP(args->kernel_size, 2, 16);
    args->logscale = !!args->logscale;
    args->fit_plane = !!args->fit_plane;
}

static void
load_args(GwyContainer *container,
          AngleArgs *args)
{
    *args = angle_defaults;

    gwy_container_gis_int32_by_name(container, size_key, &args->size);
    gwy_container_gis_int32_by_name(container, steps_key, &args->steps);
    gwy_container_gis_boolean_by_name(container, logscale_key, &args->logscale);
    gwy_container_gis_boolean_by_name(container, fit_plane_key,
                                      &args->fit_plane);
    gwy_container_gis_int32_by_name(container, kernel_size_key,
                                    &args->kernel_size);
    sanitize_args(args);
}

static void
save_args(GwyContainer *container,
          AngleArgs *args)
{
    gwy_container_set_int32_by_name(container, size_key, args->size);
    gwy_container_set_int32_by_name(container, steps_key, args->steps);
    gwy_container_set_boolean_by_name(container, logscale_key, args->logscale);
    gwy_container_set_boolean_by_name(container, fit_plane_key,
                                      args->fit_plane);
    gwy_container_set_int32_by_name(container, kernel_size_key,
                                    args->kernel_size);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
