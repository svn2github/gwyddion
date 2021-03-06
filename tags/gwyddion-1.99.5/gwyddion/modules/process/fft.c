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
#include <libprocess/inttrans.h>
#include <libprocess/stats.h>
#include <libgwydgets/gwydgets.h>
#include <app/settings.h>
#include <app/app.h>

#define FFT_RUN_MODES \
    (GWY_RUN_MODAL | GWY_RUN_NONINTERACTIVE | GWY_RUN_WITH_DEFAULTS)

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
    GwyInterpolationType interp;
    GwyWindowingType window;
    GwyFFTOutputType out;
} FFTArgs;

typedef struct {
    GtkWidget *preserve;
    GtkWidget *interp;
    GtkWidget *window;
    GtkWidget *out;
} FFTControls;

static gboolean module_register    (const gchar *name);
static gboolean fft                (GwyContainer *data,
                                    GwyRunType run);
static void     fft_create_output  (GwyContainer *data,
                                    GwyDataField *dfield,
                                    const gchar *window_name);
static gboolean fft_dialog         (FFTArgs *args);
static void     preserve_changed_cb(GtkToggleButton *button,
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



FFTArgs fft_defaults = {
    0,
    GWY_INTERPOLATION_BILINEAR,
    GWY_WINDOWING_HANN,
    GWY_FFT_OUTPUT_MOD,
};

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Two-dimensional FFT (Fast Fourier Transform)."),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.5",
    "David Nečas (Yeti) & Petr Klapetek",
    "2003",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyProcessFuncInfo fft_func_info = {
        "fft",
        N_("/_Integral Transforms/_2D FFT..."),
        (GwyProcessFunc)&fft,
        FFT_RUN_MODES,
        0,
    };

    gwy_process_func_register(name, &fft_func_info);

    return TRUE;
}

static gboolean
fft(GwyContainer *data, GwyRunType run)
{
    GtkWidget *dialog;
    GwyDataField *dfield, *tmp, *raout, *ipout;
    GwySIUnit *xyunit;
    FFTArgs args;
    gboolean ok;
    gint xsize, ysize, newsize;
    gdouble newreals;

    g_assert(run & FFT_RUN_MODES);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
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

        return FALSE;
    }

    if (run == GWY_RUN_WITH_DEFAULTS)
        args = fft_defaults;
    else
        fft_load_args(gwy_app_settings_get(), &args);
    ok = (run != GWY_RUN_MODAL) || fft_dialog(&args);
    if (run == GWY_RUN_MODAL)
        fft_save_args(gwy_app_settings_get(), &args);
    if (!ok)
        return FALSE;

    dfield = gwy_data_field_duplicate(dfield);
    xyunit = gwy_data_field_get_si_unit_xy(dfield);
    gwy_si_unit_power(xyunit, -1, xyunit);

    newsize = gwy_fft_find_nice_size(xsize);
    gwy_data_field_resample(dfield, newsize, newsize,
                            GWY_INTERPOLATION_BILINEAR);
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
                         0,
                         0);

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

    return FALSE;
}

static void
fft_create_output(GwyContainer *data,
                  GwyDataField *dfield,
                  const gchar *window_name)
{
    GtkWidget *data_window;
    GwyContainer *newdata;
    const guchar *pal = NULL;

    newdata = gwy_container_new();
    gwy_container_set_object_by_name(newdata, "/0/data", dfield);
    g_object_unref(dfield);

    gwy_container_gis_string_by_name(data, "/0/base/palette", &pal);
    if (pal)
        gwy_container_set_string_by_name(newdata, "/0/base/palette",
                                         g_strdup(pal));

    data_window = gwy_app_data_window_create(newdata);
    gwy_app_data_window_set_untitled(GWY_DATA_WINDOW(data_window), window_name);
    g_object_unref(newdata);
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
    gwy_si_unit_set_unit_string(unit, "");
}

static gboolean
fft_dialog(FFTArgs *args)
{
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
    enum { RESPONSE_RESET = 1 };
    gint response;

    dialog = gtk_dialog_new_with_buttons(_("2D FFT"), NULL, 0,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    table = gtk_table_new(4, 4, FALSE);
    gtk_table_set_col_spacings(GTK_TABLE(table), 4);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table,
                       FALSE, FALSE, 4);

    controls.preserve
        = gtk_check_button_new_with_mnemonic(_("_Preserve size (don't "
                                               "resize to power of 2)"));
    gtk_table_attach(GTK_TABLE(table), controls.preserve, 0, 3, 0, 1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);
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
    controls.window
        = gwy_enum_combo_box_new(gwy_windowing_type_get_enum(), -1,
                                 G_CALLBACK(gwy_enum_combo_box_update_int),
                                 &args->window, args->window, TRUE);
    gwy_table_attach_row(table, 2, _("_Windowing type:"), "",
                         controls.window);

    controls.out
        = gwy_enum_combo_box_new(fft_outputs, G_N_ELEMENTS(fft_outputs),
                                 G_CALLBACK(gwy_enum_combo_box_update_int),
                                 &args->out, args->out, TRUE);
    gwy_table_attach_row(table, 3, _("_Output type:"), "",
                         controls.out);

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

static const gchar *preserve_key = "/module/fft/preserve";
static const gchar *interp_key = "/module/fft/interp";
static const gchar *window_key = "/module/fft/window";
static const gchar *out_key = "/module/fft/out";

static void
fft_sanitize_args(FFTArgs *args)
{
    args->preserve = !!args->preserve;
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
    gwy_container_set_enum_by_name(container, interp_key, args->interp);
    gwy_container_set_enum_by_name(container, window_key, args->window);
    gwy_container_set_enum_by_name(container, out_key, args->out);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
