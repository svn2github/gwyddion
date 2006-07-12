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
#include <libprocess/inttrans.h>
#include <libprocess/stats.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwydgets/gwystock.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>

#define FFT_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

typedef enum {
    GWY_FFT_OUTPUT_REAL_IMG   = 0,
    GWY_FFT_OUTPUT_MOD_PHASE  = 1,
    GWY_FFT_OUTPUT_REAL       = 2,
    GWY_FFT_OUTPUT_IMG        = 3,
    GWY_FFT_OUTPUT_MOD        = 4,
    GWY_FFT_OUTPUT_PHASE      = 5
} GwyFFTOutputType;

typedef struct {
    gboolean preserve;
    gboolean zeromean;
    GwyInterpolationType interp;
    GwyWindowingType window;
    GwyFFTOutputType out;
} FFTArgs;

typedef struct {
    GtkWidget *preserve;
    GtkWidget *zeromean;
    GtkWidget *interp;
    GtkWidget *window;
    GtkWidget *out;
} FFTControls;

static gboolean module_register    (void);
static void     fft                (GwyContainer *data,
                                    GwyRunType run);
static void     fft_create_output  (GwyContainer *data,
                                    GwyDataField *dfield,
                                    const gchar *window_name);
static gboolean fft_dialog         (FFTArgs *args,
                                    gint oldsize,
                                    gint newsize);
static void     preserve_changed_cb(GtkToggleButton *button,
                                    FFTArgs *args);
static void     zeromean_changed_cb(GtkToggleButton *button,
                                    FFTArgs *args);
static void     fft_dialog_update  (FFTControls *controls,
                                    FFTArgs *args);
static void     set_dfield_modulus (GwyDataField *re,
                                    GwyDataField *im,
                                    GwyDataField *target);
static void     set_dfield_phase   (GwyDataField *re,
                                    GwyDataField *im,
                                    GwyDataField *target);
static void     fft_load_args      (GwyContainer *container,
                                    FFTArgs *args);
static void     fft_save_args      (GwyContainer *container,
                                    FFTArgs *args);
static void     fft_sanitize_args  (FFTArgs *args);

static const FFTArgs fft_defaults = {
    FALSE,
    TRUE,
    GWY_INTERPOLATION_BILINEAR,
    GWY_WINDOWING_HANN,
    GWY_FFT_OUTPUT_MOD,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Two-dimensional FFT (Fast Fourier Transform)."),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.6",
    "David Nečas (Yeti) & Petr Klapetek",
    "2003",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("fft",
                              (GwyProcessFunc)&fft,
                              N_("/_Integral Transforms/_2D FFT..."),
                              GWY_STOCK_FFT,
                              FFT_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Compute Fast Fourier Transform"));

    return TRUE;
}

static void
fft(GwyContainer *data, GwyRunType run)
{
    GtkWidget *dialog;
    GwyDataField *dfield, *tmp, *raout, *ipout;
    GwySIUnit *xyunit;
    FFTArgs args;
    gboolean ok;
    gint xsize, ysize, newsize;
    gdouble newreals;

    g_return_if_fail(run & FFT_RUN_MODES);

    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
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
             _("%s: Data must be square."), "FFT");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
    }

    fft_load_args(gwy_app_settings_get(), &args);
    newsize = gwy_fft_find_nice_size(xsize);
    if (run == GWY_RUN_INTERACTIVE) {
        ok = fft_dialog(&args, newsize, xsize);
        fft_save_args(gwy_app_settings_get(), &args);
        if (!ok)
            return;
    }

    dfield = gwy_data_field_new_resampled(dfield, newsize, newsize,
                                          GWY_INTERPOLATION_BILINEAR);
    xyunit = gwy_data_field_get_si_unit_xy(dfield);
    gwy_si_unit_power(xyunit, -1, xyunit);

    raout = gwy_data_field_new_alike(dfield, FALSE);
    ipout = gwy_data_field_new_alike(dfield, FALSE);

    gwy_data_field_multiply(dfield, 1.0
                            /(gwy_data_field_get_max(dfield)
                              - gwy_data_field_get_min(dfield)));

    gwy_data_field_2dfft(dfield, NULL,
                         raout, ipout,
                         args.window,
                         GWY_TRANSFORM_DIRECTION_FORWARD,
                         args.interp,
                         FALSE,
                         args.zeromean ? 1 : 0);

    gwy_data_field_2dfft_humanize(raout);
    gwy_data_field_2dfft_humanize(ipout);

    newreals = ((gdouble)gwy_data_field_get_xres(dfield))
               /gwy_data_field_get_xreal(dfield);

    if (args.preserve) {
        gwy_data_field_resample(dfield, xsize, ysize, args.interp);
        gwy_data_field_resample(raout, xsize, ysize, args.interp);
        gwy_data_field_resample(ipout, xsize, ysize, args.interp);
    }

    gwy_data_field_set_xreal(dfield, newreals);
    gwy_data_field_set_yreal(dfield, newreals);

    if (args.out == GWY_FFT_OUTPUT_REAL_IMG
        || args.out == GWY_FFT_OUTPUT_REAL) {
        tmp = gwy_data_field_new_alike(dfield, FALSE);
        gwy_data_field_area_copy(raout, tmp, 0, 0, xsize, ysize, 0, 0);
        fft_create_output(data, tmp, _("FFT Real"));
    }
    if (args.out == GWY_FFT_OUTPUT_REAL_IMG
        || args.out == GWY_FFT_OUTPUT_IMG) {
        tmp = gwy_data_field_new_alike(dfield, FALSE);
        gwy_data_field_area_copy(ipout, tmp, 0, 0, xsize, ysize, 0, 0);
        fft_create_output(data, tmp, _("FFT Imag"));
    }
    if (args.out == GWY_FFT_OUTPUT_MOD_PHASE
        || args.out == GWY_FFT_OUTPUT_MOD) {
        tmp = gwy_data_field_new_alike(dfield, FALSE);
        set_dfield_modulus(raout, ipout, tmp);
        fft_create_output(data, tmp, _("FFT Modulus"));
    }
    if (args.out == GWY_FFT_OUTPUT_MOD_PHASE
        || args.out == GWY_FFT_OUTPUT_PHASE) {
        tmp = gwy_data_field_new_alike(dfield, FALSE);
        set_dfield_phase(raout, ipout, tmp);
        fft_create_output(data, tmp, _("FFT Phase"));
    }

    g_object_unref(dfield);
    g_object_unref(raout);
    g_object_unref(ipout);
}

static void
fft_create_output(GwyContainer *data,
                  GwyDataField *dfield,
                  const gchar *output_name)
{
    gint newid;
    gchar *key;

    newid = gwy_app_data_browser_add_data_field(dfield, data, TRUE);
    g_object_unref(dfield);
    gwy_app_set_data_field_title(data, newid, output_name);

    /* make fft more visible by choosing a good gradient and using auto range */
    key = g_strdup_printf("/%i/base/palette", newid);
    gwy_container_set_string_by_name(data, key, g_strdup("DFit"));
    g_free(key);
    key = g_strdup_printf("/%i/base/range-type", newid);
    gwy_container_set_enum_by_name(data, key, GWY_LAYER_BASIC_RANGE_AUTO);
    g_free(key);
}

static void
set_dfield_modulus(GwyDataField *re, GwyDataField *im, GwyDataField *target)
{
    const gdouble *datare, *dataim;
    gdouble *data;
    gint xres, yres, i;

    xres = gwy_data_field_get_xres(re);
    yres = gwy_data_field_get_yres(re);
    datare = gwy_data_field_get_data_const(re);
    dataim = gwy_data_field_get_data_const(im);
    data = gwy_data_field_get_data(target);
    for (i = xres*yres; i; i--, datare++, dataim++, data++)
        *data = hypot(*datare, *dataim);
}

static void
set_dfield_phase(GwyDataField *re, GwyDataField *im,
                 GwyDataField *target)
{
    GwySIUnit *unit;
    const gdouble *datare, *dataim;
    gdouble *data;
    gint xres, yres, i;

    xres = gwy_data_field_get_xres(re);
    yres = gwy_data_field_get_yres(re);
    datare = gwy_data_field_get_data_const(re);
    dataim = gwy_data_field_get_data_const(im);
    data = gwy_data_field_get_data(target);
    for (i = xres*yres; i; i--, datare++, dataim++, data++)
        *data = atan2(*dataim, *datare);

    unit = gwy_data_field_get_si_unit_z(target);
    gwy_si_unit_set_from_string(unit, NULL);
}

static gboolean
fft_dialog(FFTArgs *args,
           gint oldsize,
           gint newsize)
{
    enum { RESPONSE_RESET = 1 };
    static const GwyEnum fft_outputs[] = {
        { N_("Real + Imaginary"),  GWY_FFT_OUTPUT_REAL_IMG,  },
        { N_("Module + Phase"),    GWY_FFT_OUTPUT_MOD_PHASE, },
        { N_("Real"),              GWY_FFT_OUTPUT_REAL,      },
        { N_("Imaginary"),         GWY_FFT_OUTPUT_IMG,       },
        { N_("Module"),            GWY_FFT_OUTPUT_MOD,       },
        { N_("Phase"),             GWY_FFT_OUTPUT_PHASE,     },
    };
    GtkWidget *dialog, *table;
    FFTControls controls;
    gint response, row;
    gchar *s;

    dialog = gtk_dialog_new_with_buttons(_("2D FFT"), NULL, 0,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    table = gtk_table_new(5, 4, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table,
                       FALSE, FALSE, 4);
    row = 0;

    controls.interp
        = gwy_enum_combo_box_new(gwy_interpolation_type_get_enum(), -1,
                                 G_CALLBACK(gwy_enum_combo_box_update_int),
                                 &args->interp, args->interp, TRUE);
    gwy_table_attach_row(table, row, _("_Interpolation type:"), NULL,
                         controls.interp);
    row++;

    controls.window
        = gwy_enum_combo_box_new(gwy_windowing_type_get_enum(), -1,
                                 G_CALLBACK(gwy_enum_combo_box_update_int),
                                 &args->window, args->window, TRUE);
    gwy_table_attach_row(table, row, _("_Windowing type:"), NULL,
                         controls.window);
    row++;

    controls.out
        = gwy_enum_combo_box_new(fft_outputs, G_N_ELEMENTS(fft_outputs),
                                 G_CALLBACK(gwy_enum_combo_box_update_int),
                                 &args->out, args->out, TRUE);
    gwy_table_attach_row(table, row, _("_Output type:"), NULL,
                         controls.out);
    row++;

    s = g_strdup_printf(_("_Preserve size (don't resize to %d × %d)"),
                        newsize, newsize);
    controls.preserve = gtk_check_button_new_with_mnemonic(s);
    g_free(s);
    gtk_table_attach(GTK_TABLE(table), controls.preserve,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.preserve),
                                 args->preserve);
    if (newsize == oldsize)
        gtk_widget_set_sensitive(controls.preserve, FALSE);
    else
        g_signal_connect(controls.preserve, "toggled",
                         G_CALLBACK(preserve_changed_cb), args);
    row++;

    controls.zeromean
        = gtk_check_button_new_with_mnemonic(_("Subtract mean _value "
                                               "beforehand"));
    gtk_table_attach(GTK_TABLE(table), controls.zeromean,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.zeromean),
                                 args->zeromean);
    g_signal_connect(controls.zeromean, "toggled",
                     G_CALLBACK(zeromean_changed_cb), args);

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
            *args = fft_defaults;
            fft_dialog_update(&controls, args);
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
preserve_changed_cb(GtkToggleButton *button, FFTArgs *args)
{
    args->preserve = gtk_toggle_button_get_active(button);
}

static void
zeromean_changed_cb(GtkToggleButton *button, FFTArgs *args)
{
    args->zeromean = gtk_toggle_button_get_active(button);
}

static void
fft_dialog_update(FFTControls *controls,
                     FFTArgs *args)
{
    gwy_enum_combo_box_set_active(GTK_COMBO_BOX(controls->interp),
                                  args->interp);
    gwy_enum_combo_box_set_active(GTK_COMBO_BOX(controls->out),
                                  args->out);
    gwy_enum_combo_box_set_active(GTK_COMBO_BOX(controls->window),
                                  args->window);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->preserve),
                                 args->preserve);
}

static const gchar preserve_key[] = "/module/fft/preserve";
static const gchar zeromean_key[] = "/module/fft/zeromean";
static const gchar interp_key[]   = "/module/fft/interp";
static const gchar window_key[]   = "/module/fft/window";
static const gchar out_key[]      = "/module/fft/out";

static void
fft_sanitize_args(FFTArgs *args)
{
    args->preserve = !!args->preserve;
    args->zeromean = !!args->zeromean;
    args->interp = CLAMP(args->interp,
                         GWY_INTERPOLATION_ROUND, GWY_INTERPOLATION_NNA);
    args->window = MIN(args->window, GWY_WINDOWING_RECT);
    args->out = MIN(args->out, GWY_FFT_OUTPUT_PHASE);
}

static void
fft_load_args(GwyContainer *container,
              FFTArgs *args)
{
    *args = fft_defaults;

    gwy_container_gis_boolean_by_name(container, preserve_key, &args->preserve);
    gwy_container_gis_boolean_by_name(container, zeromean_key, &args->zeromean);
    gwy_container_gis_enum_by_name(container, interp_key, &args->interp);
    gwy_container_gis_enum_by_name(container, window_key, &args->window);
    gwy_container_gis_enum_by_name(container, out_key, &args->out);
    fft_sanitize_args(args);
}

static void
fft_save_args(GwyContainer *container,
              FFTArgs *args)
{
    gwy_container_set_boolean_by_name(container, preserve_key, args->preserve);
    gwy_container_set_boolean_by_name(container, zeromean_key, args->zeromean);
    gwy_container_set_enum_by_name(container, interp_key, args->interp);
    gwy_container_set_enum_by_name(container, window_key, args->window);
    gwy_container_set_enum_by_name(container, out_key, args->out);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
