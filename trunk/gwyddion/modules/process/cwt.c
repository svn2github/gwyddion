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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libprocess/gwyprocesstypes.h>
#include <libprocess/inttrans.h>
#include <libprocess/cwt.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwydgets/gwystock.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>

#define CWT_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

typedef struct {
    gdouble scale;
    Gwy2DCWTWaveletType wavelet;
} CWTArgs;

typedef struct {
    GtkObject *scale;
    GtkWidget *wavelet;
} CWTControls;

static gboolean    module_register            (void);
static void        cwt                        (GwyContainer *data,
                                               GwyRunType run);
static gboolean    cwt_dialog                 (CWTArgs *args);
static void        cwt_load_args              (GwyContainer *container,
                                               CWTArgs *args);
static void        cwt_save_args              (GwyContainer *container,
                                               CWTArgs *args);
static void        cwt_dialog_update          (CWTControls *controls,
                                               CWTArgs *args);

static const CWTArgs cwt_defaults = {
    10,
    GWY_2DCWT_GAUSS,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Two-dimensional CWT (Continuous Wavelet Transform)."),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.5",
    "David Nečas (Yeti) & Petr Klapetek",
    "2003",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("cwt",
                              (GwyProcessFunc)&cwt,
                              N_("/_Integral Transforms/2D _CWT..."),
                              GWY_STOCK_CWT,
                              CWT_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Compute continuous wavelet transform"));

    return TRUE;
}

static void
cwt(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield;
    CWTArgs args;
    gboolean ok;
    gint oldid, newid;

    g_return_if_fail(run & CWT_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &oldid,
                                     0);
    g_return_if_fail(dfield);

    cwt_load_args(gwy_app_settings_get(), &args);
    if (run == GWY_RUN_INTERACTIVE) {
        ok = cwt_dialog(&args);
        cwt_save_args(gwy_app_settings_get(), &args);
        if (!ok)
            return;
    }

    dfield = gwy_data_field_duplicate(dfield);
    gwy_data_field_cwt(dfield,
                       GWY_INTERPOLATION_LINEAR,  /* ignored */
                       args.scale, args.wavelet);

    newid = gwy_app_data_browser_add_data_field(dfield, data, TRUE);
    gwy_app_sync_data_items(data, data, oldid, newid, FALSE,
                            GWY_DATA_ITEM_GRADIENT,
                            GWY_DATA_ITEM_MASK_COLOR,
                            0);

    g_object_unref(dfield);
    gwy_app_set_data_field_title(data, newid, _("CWT"));
    gwy_app_channel_log_add_proc(data, oldid, newid);
}

static gboolean
cwt_dialog(CWTArgs *args)
{
    GtkWidget *dialog, *table;
    CWTControls controls;
    enum { RESPONSE_RESET = 1 };
    gint response;

    dialog = gtk_dialog_new_with_buttons(_("2D CWT"), NULL, 0,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    table = gtk_table_new(4, 4, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table,
                       FALSE, FALSE, 4);


    controls.scale = gtk_adjustment_new(args->scale, 0.0, 1000.0, 1, 10, 0);
    gwy_table_attach_spinbutton(table, 1, _("_Scale:"), _("pixels"),
                                controls.scale);

    controls.wavelet
        = gwy_enum_combo_box_new(gwy_2d_cwt_wavelet_type_get_enum(), -1,
                                 G_CALLBACK(gwy_enum_combo_box_update_int),
                                 &args->wavelet, args->wavelet, TRUE);
    gwy_table_attach_row(table, 3, _("_Wavelet type:"), "",
                         controls.wavelet);

    gtk_widget_show_all(dialog);
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            args->scale
                = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls.scale));
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            return FALSE;
            break;

            case GTK_RESPONSE_OK:
            break;

            case RESPONSE_RESET:
            *args = cwt_defaults;
            cwt_dialog_update(&controls, args);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    args->scale = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls.scale));
    gtk_widget_destroy(dialog);

    return TRUE;
}

static void
cwt_dialog_update(CWTControls *controls,
                  CWTArgs *args)
{
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->scale),
                             args->scale);
    gwy_enum_combo_box_set_active(GTK_COMBO_BOX(controls->wavelet),
                                  args->wavelet);
}

static const gchar wavelet_key[]  = "/module/cwt/wavelet";
static const gchar scale_key[]    = "/module/cwt/scale";

static void
cwt_sanitize_args(CWTArgs *args)
{
    args->wavelet = gwy_enum_sanitize_value(args->wavelet,
                                            GWY_TYPE_2D_CWT_WAVELET_TYPE);
    args->scale = CLAMP(args->scale, 0.0, 1000.0);
}

static void
cwt_load_args(GwyContainer *container,
              CWTArgs *args)
{
    *args = cwt_defaults;

    gwy_container_gis_enum_by_name(container, wavelet_key, &args->wavelet);
    gwy_container_gis_double_by_name(container, scale_key, &args->scale);
    cwt_sanitize_args(args);
}

static void
cwt_save_args(GwyContainer *container,
              CWTArgs *args)
{
    gwy_container_set_enum_by_name(container, wavelet_key, args->wavelet);
    gwy_container_set_double_by_name(container, scale_key, args->scale);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
