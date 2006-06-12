/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 */

#include "config.h"
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyenum.h>
#include <libprocess/stats.h>
#include <libprocess/inttrans.h>
#include <libprocess/dwt.h>
#include <libprocess/gwyprocesstypes.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>

#define DWT_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

typedef struct {
    gboolean preserve;
    GwyInterpolationType interp;
    GwyDWTType wavelet;
} DWTArgs;

typedef struct {
    GtkWidget *preserve;
    GtkWidget *wavelet;
    GtkWidget *interp;
} DWTControls;

static gboolean module_register    (void);
static void     dwt                (GwyContainer *data,
                                    GwyRunType run);
static gboolean dwt_dialog         (DWTArgs *args);
static void     preserve_changed_cb(GtkToggleButton *button,
                                    DWTArgs *args);
static void     dwt_dialog_update  (DWTControls *controls,
                                    DWTArgs *args);
static void     dwt_load_args      (GwyContainer *container,
                                    DWTArgs *args);
static void     dwt_save_args      (GwyContainer *container,
                                    DWTArgs *args);
static void     dwt_sanitize_args  (DWTArgs *args);


static const DWTArgs dwt_defaults = {
    0,
    GWY_INTERPOLATION_BILINEAR,
    GWY_DWT_DAUB12,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Two-dimensional DWT (Discrete Wavelet Transform)."),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.5",
    "David Nečas (Yeti) & Petr Klapetek",
    "2003",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("dwt",
                              (GwyProcessFunc)&dwt,
                              N_("/_Integral Transforms/_2D DWT..."),
                              NULL,
                              DWT_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Compute Discrete Wavelet Transform"));

    return TRUE;
}

static void
dwt(GwyContainer *data, GwyRunType run)
{
    GtkWidget *dialog;
    GwyDataField *dfield;
    GwyDataLine *wtcoefs;
    DWTArgs args;
    gboolean ok;
    gint xsize, ysize, newsize, newid, oldid;

    g_return_if_fail(run & DWT_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &oldid,
                                     0);
    g_return_if_fail(dfield);

    xsize = gwy_data_field_get_xres(dfield);
    ysize = gwy_data_field_get_yres(dfield);
    if (xsize != ysize) {
        dialog = gtk_message_dialog_new
            (GTK_WINDOW(gwy_app_data_window_get_for_data(data)),
             GTK_DIALOG_DESTROY_WITH_PARENT,
             GTK_MESSAGE_ERROR,
             GTK_BUTTONS_OK,
             _("%s: Data must be square."), "DWT");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return;
    }

    dwt_load_args(gwy_app_settings_get(), &args);
    if (run == GWY_RUN_INTERACTIVE) {
        ok = dwt_dialog(&args);
        dwt_save_args(gwy_app_settings_get(), &args);
        if (!ok)
            return;
    }

    newsize = gwy_fft_find_nice_size(xsize);
    dfield = gwy_data_field_new_resampled(dfield, newsize, newsize,
                                          GWY_INTERPOLATION_BILINEAR);
    gwy_data_field_add(dfield, -gwy_data_field_get_avg(dfield));

    wtcoefs = GWY_DATA_LINE(gwy_data_line_new(10, 10, TRUE));
    wtcoefs = gwy_dwt_set_coefficients(wtcoefs, args.wavelet);
    gwy_data_field_dwt(dfield, wtcoefs, 1, 4);

    if (args.preserve)
        gwy_data_field_resample(dfield, xsize, ysize, args.interp);


    newid = gwy_app_data_browser_add_data_field(dfield, data, TRUE);
    g_object_unref(dfield);
    gwy_app_set_data_field_title(data, newid, _("DWT"));
    gwy_app_copy_data_items(data, data, oldid, newid, GWY_DATA_ITEM_PALETTE, 0);

    g_object_unref(wtcoefs);
}


static gboolean
dwt_dialog(DWTArgs *args)
{
    GtkWidget *dialog, *table;
    DWTControls controls;
    enum { RESPONSE_RESET = 1 };
    gint response;

    dialog = gtk_dialog_new_with_buttons(_("2D DWT"), NULL, 0,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    table = gtk_table_new(2, 4, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table,
                       FALSE, FALSE, 4);

    controls.preserve
        = gtk_check_button_new_with_mnemonic(_("_Preserve size (don't "
                                               "resize to power of 2)"));
    gtk_table_attach(GTK_TABLE(table), controls.preserve,
                     0, 3, 0, 1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.preserve),
                                 args->preserve);
    g_signal_connect(controls.preserve, "toggled",
                     G_CALLBACK(preserve_changed_cb), args);

    controls.interp
        = gwy_enum_combo_box_new(gwy_interpolation_type_get_enum(), -1,
                                 G_CALLBACK(gwy_enum_combo_box_update_int),
                                 &args->interp, args->interp, TRUE);
    gwy_table_attach_row(table, 1, _("_Interpolation type:"), "",
                         controls.interp);

    controls.wavelet
        = gwy_enum_combo_box_new(gwy_dwt_type_get_enum(), -1,
                                 G_CALLBACK(gwy_enum_combo_box_update_int),
                                 &args->wavelet, args->wavelet, TRUE);
    gwy_table_attach_row(table, 2, _("_Wavelet type:"), "",
                         controls.wavelet);

    gtk_widget_show_all(dialog);
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            return FALSE;
            break;

            case GTK_RESPONSE_OK:
            break;

            case RESPONSE_RESET:
            *args = dwt_defaults;
            dwt_dialog_update(&controls, args);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    gtk_widget_destroy(dialog);

    return TRUE;
}

static void
preserve_changed_cb(GtkToggleButton *button, DWTArgs *args)
{
    args->preserve = gtk_toggle_button_get_active(button);
}

static void
dwt_dialog_update(DWTControls *controls,
                  DWTArgs *args)
{
    gwy_enum_combo_box_set_active(GTK_COMBO_BOX(controls->interp),
                                  args->interp);
    gwy_enum_combo_box_set_active(GTK_COMBO_BOX(controls->wavelet),
                                  args->wavelet);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->preserve),
                                 args->preserve);
}


static const gchar preserve_key[] = "/module/dwt/preserve";
static const gchar interp_key[]   = "/module/dwt/interp";
static const gchar wavelet_key[]  = "/module/dwt/window";

static void
dwt_sanitize_args(DWTArgs *args)
{
    args->preserve = !!args->preserve;
    args->interp = CLAMP(args->interp,
                         GWY_INTERPOLATION_ROUND, GWY_INTERPOLATION_NNA);
    args->wavelet = gwy_enum_sanitize_value(args->wavelet, GWY_TYPE_DWT_TYPE);
}

static void
dwt_load_args(GwyContainer *container,
              DWTArgs *args)
{
    *args = dwt_defaults;

    gwy_container_gis_boolean_by_name(container, preserve_key, &args->preserve);
    gwy_container_gis_enum_by_name(container, interp_key, &args->interp);
    gwy_container_gis_enum_by_name(container, wavelet_key, &args->wavelet);
    dwt_sanitize_args(args);
}

static void
dwt_save_args(GwyContainer *container,
              DWTArgs *args)
{
    gwy_container_set_boolean_by_name(container, preserve_key, args->preserve);
    gwy_container_set_enum_by_name(container, interp_key, args->interp);
    gwy_container_set_enum_by_name(container, wavelet_key, args->wavelet);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
