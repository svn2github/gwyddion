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
#include <libgwyddion/gwyenum.h>
#include <libprocess/stats.h>
#include <libprocess/inttrans.h>
#include <libprocess/dwt.h>
#include <libprocess/gwyprocesstypes.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>

#define DWT_ANISOTROPY_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

typedef struct {
    GwyInterpolationType interp;
    GwyDWTType wavelet;
    gdouble ratio;
    gint lowlimit;
} DWTAnisotropyArgs;

typedef struct {
    GtkWidget *wavelet;
    GtkWidget *interp;
    GtkObject *ratio;
    GtkObject *lowlimit;
} DWTAnisotropyControls;

static gboolean module_register             (void);
static void     dwt_anisotropy              (GwyContainer *data,
                                             GwyRunType run);
static gboolean dwt_anisotropy_dialog       (DWTAnisotropyArgs *args);
static void     ratio_changed_cb            (GtkAdjustment *adj,
                                             DWTAnisotropyArgs *args);
static void     lowlimit_changed_cb         (GtkAdjustment *adj,
                                             DWTAnisotropyArgs *args);
static void     dwt_anisotropy_dialog_update(DWTAnisotropyControls *controls,
                                             DWTAnisotropyArgs *args);
static void     dwt_anisotropy_load_args    (GwyContainer *container,
                                             DWTAnisotropyArgs *args);
static void     dwt_anisotropy_save_args    (GwyContainer *container,
                                             DWTAnisotropyArgs *args);
static void     dwt_anisotropy_sanitize_args(DWTAnisotropyArgs *args);


static const DWTAnisotropyArgs dwt_anisotropy_defaults = {
    GWY_INTERPOLATION_BILINEAR,
    GWY_DWT_DAUB12,
    0.2,
    4
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("2D DWT anisotropy detection based on X/Y components ratio."),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.4",
    "David Nečas (Yeti) & Petr Klapetek",
    "2003",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("dwtanisotropy",
                              (GwyProcessFunc)&dwt_anisotropy,
                              N_("/_Integral Transforms/DWT _Anisotropy..."),
                              NULL,
                              DWT_ANISOTROPY_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("DWT anisotropy detection"));

    return TRUE;
}

static void
dwt_anisotropy(GwyContainer *data, GwyRunType run)
{
    GtkWidget *dialog;
    GwyDataField *dfield, *mask;
    GQuark dquark, mquark;
    GwyDataLine *wtcoefs;
    DWTAnisotropyArgs args;
    gboolean ok;
    gint xsize, ysize, newsize, limit, id;

    g_return_if_fail(run & DWT_ANISOTROPY_RUN_MODES);

    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD_KEY, &dquark,
                                     GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     GWY_APP_MASK_FIELD_KEY, &mquark,
                                     GWY_APP_MASK_FIELD, &mask,
                                     0);
    g_return_if_fail(dfield && dquark);

    xsize = gwy_data_field_get_xres(dfield);
    ysize = gwy_data_field_get_yres(dfield);
    if (xsize != ysize) {
        dialog = gtk_message_dialog_new
            (gwy_app_find_window_for_channel(data, id),
             GTK_DIALOG_DESTROY_WITH_PARENT,
             GTK_MESSAGE_ERROR,
             GTK_BUTTONS_OK,
             _("%s: Data must be square."), _("DWT Anisotropy"));
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return;
    }

    dwt_anisotropy_load_args(gwy_app_settings_get(), &args);
    if (run == GWY_RUN_INTERACTIVE) {
        ok = dwt_anisotropy_dialog(&args);
        dwt_anisotropy_save_args(gwy_app_settings_get(), &args);
        if (!ok)
            return;
    }

    newsize = gwy_fft_find_nice_size(xsize);
    dfield = gwy_data_field_duplicate(dfield);
    gwy_data_field_add(dfield, -gwy_data_field_get_avg(dfield));
    gwy_data_field_resample(dfield, newsize, newsize,
                            GWY_INTERPOLATION_BILINEAR);

    gwy_app_undo_qcheckpoint(data, dquark, mquark, 0);
    if (!mask) {
        mask = gwy_data_field_new_alike(dfield, FALSE);
        gwy_container_set_object(data, mquark, mask);
        g_object_unref(mask);
    }
    gwy_data_field_resample(mask, newsize, newsize, GWY_INTERPOLATION_NONE);

    wtcoefs = gwy_data_line_new(10, 10, TRUE);
    wtcoefs = gwy_dwt_set_coefficients(wtcoefs, args.wavelet);

    /*justo for sure clamp the lowlimit again*/
    limit = pow(2, CLAMP(args.lowlimit, 1, 20));
    gwy_data_field_dwt_mark_anisotropy(dfield, mask, wtcoefs, args.ratio,
                                              limit);

    gwy_data_field_resample(mask, xsize, ysize, GWY_INTERPOLATION_BILINEAR);
    g_object_unref(wtcoefs);
    g_object_unref(dfield);
    gwy_data_field_data_changed(mask);
}


static gboolean
dwt_anisotropy_dialog(DWTAnisotropyArgs *args)
{
    GtkWidget *dialog, *table, *spin;
    DWTAnisotropyControls controls;
    enum { RESPONSE_RESET = 1 };
    gint response;

    dialog = gtk_dialog_new_with_buttons(_("2D DWT Anisotropy"), NULL, 0,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    table = gtk_table_new(2, 5, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
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
    gwy_table_attach_row(table, 2, _("_Wavelet type:"), "", controls.wavelet);

    controls.ratio = gtk_adjustment_new(args->ratio,
                                        0.0001, 10.0, 1, 0.1, 0);
    spin = gwy_table_attach_spinbutton(table, 3,
                                       _("X/Y ratio threshold:"), NULL,
                                       controls.ratio);
    g_signal_connect(controls.ratio, "value-changed",
                     G_CALLBACK(ratio_changed_cb), args);

    controls.lowlimit = gtk_adjustment_new(args->lowlimit,
                                           1, 20, 1, 1, 0);
    spin = gwy_table_attach_spinbutton(table, 4,
                                       _("Low level exclude limit:"), NULL,
                                       controls.lowlimit);
    g_signal_connect(controls.lowlimit, "value-changed",
                     G_CALLBACK(lowlimit_changed_cb), args);

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
            *args = dwt_anisotropy_defaults;
            dwt_anisotropy_dialog_update(&controls, args);
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
ratio_changed_cb(GtkAdjustment *adj, DWTAnisotropyArgs *args)
{
    args->ratio = gtk_adjustment_get_value(adj);
}

static void
lowlimit_changed_cb(GtkAdjustment *adj, DWTAnisotropyArgs *args)
{
    args->lowlimit = gtk_adjustment_get_value(adj);
}

static void
dwt_anisotropy_dialog_update(DWTAnisotropyControls *controls,
                             DWTAnisotropyArgs *args)
{
    gwy_enum_combo_box_set_active(GTK_COMBO_BOX(controls->interp),
                                  args->interp);
    gwy_enum_combo_box_set_active(GTK_COMBO_BOX(controls->wavelet),
                                  args->wavelet);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->ratio),
                             args->ratio);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->lowlimit),
                             args->lowlimit);
}


static const gchar interp_key[]   = "/module/dwtanisotropy/interp";
static const gchar wavelet_key[]  = "/module/dwtanisotropy/wavelet";
static const gchar ratio_key[]    = "/module/dwtanisotropy/ratio";
static const gchar lowlimit_key[] = "/module/dwtanisotropy/lowlimit";

static void
dwt_anisotropy_sanitize_args(DWTAnisotropyArgs *args)
{
    args->interp = CLAMP(args->interp,
                         GWY_INTERPOLATION_ROUND, GWY_INTERPOLATION_NNA);
    args->wavelet = gwy_enum_sanitize_value(args->wavelet, GWY_TYPE_DWT_TYPE);
    args->lowlimit = CLAMP(args->lowlimit, 1, 20);
}

static void
dwt_anisotropy_load_args(GwyContainer *container,
              DWTAnisotropyArgs *args)
{
    *args = dwt_anisotropy_defaults;

    gwy_container_gis_enum_by_name(container, interp_key, &args->interp);
    gwy_container_gis_enum_by_name(container, wavelet_key, &args->wavelet);
    gwy_container_gis_double_by_name(container, ratio_key, &args->ratio);
    gwy_container_gis_int32_by_name(container, lowlimit_key, &args->lowlimit);
    dwt_anisotropy_sanitize_args(args);
}

static void
dwt_anisotropy_save_args(GwyContainer *container,
              DWTAnisotropyArgs *args)
{
    gwy_container_set_enum_by_name(container, interp_key, args->interp);
    gwy_container_set_enum_by_name(container, wavelet_key, args->wavelet);
    gwy_container_set_double_by_name(container, ratio_key, args->ratio);
    gwy_container_set_int32_by_name(container, lowlimit_key, args->lowlimit);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
