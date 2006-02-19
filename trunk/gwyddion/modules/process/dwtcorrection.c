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
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <libprocess/dwt.h>
#include <libgwydgets/gwydgets.h>
#include <app/gwyapp.h>

#define DWT_CORRECTION_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

/* Data for this function. */
typedef struct {
    GwyInterpolationType interp;
    GwyDWTType wavelet;
} DWTCorrectionArgs;

typedef struct {
    GtkWidget *wavelet;
    GtkWidget *interp;
} DWTCorrectionControls;

static gboolean module_register             (const gchar *name);
static void     dwt_correction              (GwyContainer *data,
                                             GwyRunType run);
static gboolean dwt_correction_dialog       (DWTCorrectionArgs *args);
static void     dwt_correction_dialog_update(DWTCorrectionControls *controls,
                                             DWTCorrectionArgs *args);
static void     dwt_correction_load_args    (GwyContainer *container,
                                             DWTCorrectionArgs *args);
static void     dwt_correction_save_args    (GwyContainer *container,
                                             DWTCorrectionArgs *args);
static void     dwt_correction_sanitize_args(DWTCorrectionArgs *args);


DWTCorrectionArgs dwt_correction_defaults = {
    GWY_INTERPOLATION_BILINEAR,
    GWY_DWT_DAUB12,
    4
};

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("2D Discrete Wavelet Transform module"),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.4",
    "David Nečas (Yeti) & Petr Klapetek",
    "2003",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    gwy_process_func_registe2("dwtcorrection",
                              (GwyProcessFunc)&dwt_correction,
                              N_("/_Integral Transforms/DWT C_orrection..."),
                              NULL,
                              DWT_CORRECTION_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Remove data under mask by DWT reconstruction"));


    return TRUE;
}

static gboolean
dwt_correction(GwyContainer *data, GwyRunType run)
{
    GtkWidget *dialog;
    GwyDataField *dfield, *mask, *maskfield;
    GwyDataLine *wtcoefs;
    DWTCorrectionArgs args;
    gboolean ok;
    gint xsize, ysize, newsize;

    g_return_if_fail(run & DWT_CORRECTION_RUN_MODES);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));


    xsize = gwy_data_field_get_xres(dfield);
    ysize = gwy_data_field_get_yres(dfield);
    if (xsize != ysize) {
        dialog = gtk_message_dialog_new
            (GTK_WINDOW(gwy_app_data_window_get_current()),
             GTK_DIALOG_DESTROY_WITH_PARENT,
             GTK_MESSAGE_ERROR,
             GTK_BUTTONS_OK,
             _("%s: Data must be square."), _("DWT Correction"));
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return;
    }

    dwt_correction_load_args(gwy_app_settings_get(), &args);
    if (run == GWY_RUN_INTERACTIVE) {
        ok = dwt_correction_dialog(&args);
        dwt_correction_save_args(gwy_app_settings_get(), &args);
        if (!ok)
            return;
    }

    if (!gwy_container_gis_object_by_name(data, "/0/mask", &mask)) {
        mask = gwy_data_field_new_alike(dfield, TRUE);
        gwy_container_set_object_by_name(data, "/0/mask", mask);
        g_object_unref(mask);
    }

    newsize = gwy_data_field_get_fft_res(xsize);
    gwy_data_field_add(dfield, -gwy_data_field_get_avg(dfield));
    gwy_data_field_resample(dfield, newsize, newsize,
                            GWY_INTERPOLATION_BILINEAR);
    gwy_data_field_resample(mask, newsize, newsize,
                            GWY_INTERPOLATION_BILINEAR);


    wtcoefs = gwy_data_line_new(10, 10, TRUE);
    wtcoefs = gwy_dwt_set_coefficients(wtcoefs, args.wavelet);
    mask = gwy_data_field_dwt_correction(dfield, mask, wtcoefs);

    gwy_data_field_resample(mask, xsize, ysize,
                            GWY_INTERPOLATION_BILINEAR);

    gwy_container_remove_by_name(data, "/0/mask");
    g_object_unref(wtcoefs);
}

static gboolean
dwt_correction_dialog(DWTCorrectionArgs *args)
{
    GtkWidget *dialog, *table, *spin;
    DWTCorrectionControls controls;
    enum { RESPONSE_RESET = 1 };
    gint response;

    dialog = gtk_dialog_new_with_buttons(_("2D DWT Correction"), NULL, 0,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    table = gtk_table_new(2, 5, FALSE);
    gtk_table_set_col_spacings(GTK_TABLE(table), 4);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table,
                       FALSE, FALSE, 4);


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
            *args = dwt_correction_defaults;
            dwt_correction_dialog_update(&controls, args);
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
dwt_correction_dialog_update(DWTCorrectionControls *controls,
                             DWTCorrectionArgs *args)
{
    gwy_enum_combo_box_set_active(GTK_COMBO_BOX(controls->interp),
                                  args->interp);
    gwy_enum_combo_box_set_active(GTK_COMBO_BOX(controls->wavelet),
                                  args->wavelet);
}


static const gchar interp_key[]  = "/module/dwtcorrection/interp";
static const gchar wavelet_key[] = "/module/dwtcorrection/wavelet";

static void
dwt_correction_sanitize_args(DWTCorrectionArgs *args)
{
    args->interp = CLAMP(args->interp,
                         GWY_INTERPOLATION_ROUND, GWY_INTERPOLATION_NNA);
    args->wavelet = CLAMP(args->wavelet, GWY_DWT_HAAR, GWY_DWT_DAUB20);
}

static void
dwt_correction_load_args(GwyContainer *container,
                         DWTCorrectionArgs *args)
{
    *args = dwt_correction_defaults;

    gwy_container_gis_enum_by_name(container, interp_key, &args->interp);
    gwy_container_gis_enum_by_name(container, wavelet_key, &args->wavelet);
    dwt_correction_sanitize_args(args);
}

static void
dwt_correction_save_args(GwyContainer *container,
                         DWTCorrectionArgs *args)
{
    gwy_container_set_enum_by_name(container, interp_key, args->interp);
    gwy_container_set_enum_by_name(container, wavelet_key, args->wavelet);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
