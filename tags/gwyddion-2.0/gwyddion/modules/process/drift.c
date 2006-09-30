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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 */

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule-process.h>
#include <libprocess/correct.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwylayer-basic.h>
#include <app/gwyapp.h>

#define DRIFT_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

enum {
    PREVIEW_SIZE = 320
};


typedef enum {
    GWY_DRIFT_CORRELATION = 0,
    GWY_DRIFT_ISOTROPY    = 1,
    GWY_DRIFT_SAMPLE      = 2
} GwyDriftMethod;

/* Data for this function. */
typedef struct {
    GwyDriftMethod method;
    gdouble sensitivity;
    gdouble smoothing;
    gboolean is_graph;
    gboolean is_crop;
    gboolean is_correct;
    GwyInterpolationType interpolation;
} DriftArgs;

typedef struct {
    GtkWidget *view;
    GtkWidget *is_graph;
    GtkWidget *is_correct;
    GtkWidget *is_crop;
    GtkObject *sensitivity;
    GtkObject *smoothing;
    GtkWidget *interpolation;
    GtkWidget *method;
    GtkWidget *color_button;
    GwyContainer *viewdata;
    GwyDataLine *result;
    gboolean computed;
} DriftControls;

static gboolean    module_register            (void);
static void        drift                      (GwyContainer *data,
                                               GwyRunType run);
static gboolean    drift_dialog                (DriftArgs *args,
                                               GwyContainer *data);
static void        mask_color_change_cb       (GtkWidget *color_button,
                                               DriftControls *controls);
static void        load_mask_color            (GtkWidget *color_button,
                                               GwyContainer *data);
static void        save_mask_color            (GtkWidget *color_button,
                                               GwyContainer *data);
static void        drift_dialog_update_controls(DriftControls *controls,
                                               DriftArgs *args);
static void        drift_dialog_update_values  (DriftControls *controls,
                                               DriftArgs *args);
static void        drift_invalidate            (GObject *obj,
                                               DriftControls *controls);
static void        preview                    (DriftControls *controls,
                                               DriftArgs *args);
static void        reset                    (DriftControls *controls,
                                               DriftArgs *args);
static void        drift_ok                    (DriftControls *controls,
                                               DriftArgs *args, GwyContainer *data);
static void        drift_load_args              (GwyContainer *container,
                                               DriftArgs *args);
static void        drift_save_args              (GwyContainer *container,
                                               DriftArgs *args);
static void        drift_sanitize_args         (DriftArgs *args);
static void         mask_process               (GwyDataField *dfield,
                                                GwyDataField *maskfield,
                                                DriftArgs *args,
                                                DriftControls *controls);


static const GwyEnum methods[] = {
    { N_("Scan lines correlation"), GWY_DRIFT_CORRELATION, },
    { N_("Local isotropy"),         GWY_DRIFT_ISOTROPY,    },
    { N_("Calibration sample"),     GWY_DRIFT_SAMPLE,      },
};



static const DriftArgs drift_defaults = {
    GWY_DRIFT_CORRELATION,
    50,
    50,
    TRUE,
    TRUE,
    TRUE,
    GWY_INTERPOLATION_BILINEAR,
};

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Evaluate/correct thermal drift in fast scan axis."),
    "Petr Klapetek <petr@klapetek.cz>",
    "1.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2005",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("drift",
                              (GwyProcessFunc)&drift,
                              N_("/_Correct Data/_Compensate drift..."),
                              NULL,
                              DRIFT_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Evaluate/correct thermal drift in fast scan axis."));


    return TRUE;
}

static void
drift(GwyContainer *data, GwyRunType run)
{
    DriftArgs args;
    gboolean ok = TRUE;

    g_return_if_fail(run & DRIFT_RUN_MODES);
    drift_load_args(gwy_app_settings_get(), &args);
    if (run == GWY_RUN_INTERACTIVE) {
        ok = drift_dialog(&args, data);
        drift_save_args(gwy_app_settings_get(), &args);
    }
    else
        /* FIXME: This crashes.  What it was supposed to do? */
        drift_ok(NULL, &args, data);
}

static GwyContainer*
create_preview_data(GwyContainer *data)
{
    GwyContainer *preview_container;
    GwyDataField *dfield, *dfield_show;
    gint oldid;
    gint xres, yres;
    gdouble zoomval;

    preview_container = gwy_container_new();

    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &oldid,
                                     0);

    dfield = gwy_data_field_duplicate(dfield);
    dfield_show = gwy_data_field_duplicate(dfield);

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    zoomval = (gdouble)PREVIEW_SIZE/MAX(xres, yres);
    gwy_data_field_resample(dfield, xres*zoomval, yres*zoomval,
                            GWY_INTERPOLATION_BILINEAR);
    dfield_show = gwy_data_field_duplicate(dfield);

    gwy_container_set_object_by_name(preview_container, "/0/data", dfield);
    g_object_unref(dfield);
    gwy_container_set_object_by_name(preview_container, "/0/show", dfield_show);
    g_object_unref(dfield_show);

    gwy_app_copy_data_items(data, preview_container, oldid, 0,
                            GWY_DATA_ITEM_GRADIENT,
                            GWY_DATA_ITEM_RANGE,
                            GWY_DATA_ITEM_MASK_COLOR,
                            0);
    return preview_container;
}



/* FIXME: What is the return value good for when drift_dialog() does all the
 * work itself? */
static gboolean
drift_dialog(DriftArgs *args, GwyContainer *data)
{
    GtkWidget *dialog, *table, *label, *hbox, *spin;
    DriftControls controls;
    enum {
        RESPONSE_RESET = 1,
        RESPONSE_PREVIEW = 2
    };
    gint response;
    gdouble zoomval;
    GwyPixmapLayer *layer;
    GwyDataField *dfield;
    gint xres, yres,row;

    dialog = gtk_dialog_new_with_buttons(_("Correct drift"), NULL, 0,
                                         _("_Update Result"), RESPONSE_PREVIEW,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    hbox = gtk_hbox_new(FALSE, 2);

    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 4);

    controls.viewdata = create_preview_data(data);
    controls.view = gwy_data_view_new(controls.viewdata);
    g_object_unref(controls.viewdata);
    layer = gwy_layer_basic_new();

    gwy_pixmap_layer_set_data_key(layer, "/0/data");
    gwy_layer_basic_set_gradient_key(GWY_LAYER_BASIC(layer), "/0/base/palette");
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.view),
                                 layer);

    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                        0);

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    controls.result
        = gwy_data_line_new(yres, gwy_data_field_get_yreal(dfield), TRUE);

    zoomval = PREVIEW_SIZE/(gdouble)MAX(xres, yres);
    gwy_data_view_set_zoom(GWY_DATA_VIEW(controls.view), zoomval);

    gtk_box_pack_start(GTK_BOX(hbox), controls.view, FALSE, FALSE, 4);

    table = gtk_table_new(10, 4, FALSE);
    gtk_box_pack_start(GTK_BOX(hbox), table, TRUE, TRUE, 4);
    row = 0;

    label = gtk_label_new_with_mnemonic(_("_Method:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row+1,
                                GTK_EXPAND | GTK_FILL, 0, 2, 2);

    controls.method = gwy_enum_combo_box_new(methods, G_N_ELEMENTS(methods),
                                                 G_CALLBACK(gwy_enum_combo_box_update_int),
                                                 &args->method, args->method, TRUE);

 //   g_signal_connect(controls.method, "value_changed",
 //                                     G_CALLBACK(drift_invalidate), &controls);


    gtk_label_set_mnemonic_widget(GTK_LABEL(label), controls.method);
    gtk_table_attach(GTK_TABLE(table), controls.method, 1, 2, row, row+1,
                                 GTK_EXPAND | GTK_FILL, 0, 2, 2);

    row++;

    label = gtk_label_new_with_mnemonic(_("_Interpolation:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row+1,
                                GTK_EXPAND | GTK_FILL, 0, 2, 2);

    controls.interpolation =
          gwy_enum_combo_box_new(gwy_interpolation_type_get_enum(), -1,
                                 G_CALLBACK(gwy_enum_combo_box_update_int),
                                 &args->interpolation, args->interpolation, TRUE);


    gtk_label_set_mnemonic_widget(GTK_LABEL(label), controls.interpolation);
    gtk_table_attach(GTK_TABLE(table), controls.interpolation, 1, 2, row, row+1,
                                 GTK_EXPAND | GTK_FILL, 0, 2, 2);

    row++;


    controls.sensitivity = gtk_adjustment_new(args->sensitivity, 0.0, 100.0, 0.1, 5, 0);
    spin = gwy_table_attach_hscale(table, row, "sensitivity:", "", controls.sensitivity,
                                               GWY_HSCALE_DEFAULT);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 1);
    g_signal_connect(controls.sensitivity, "value_changed",
                     G_CALLBACK(drift_invalidate), &controls);

    row++;

    controls.smoothing = gtk_adjustment_new(args->smoothing, 0.0, 100.0, 0.1, 5, 0);
    spin = gwy_table_attach_hscale(table, row, "smoothing:", "%", controls.smoothing,
                                               GWY_HSCALE_DEFAULT);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 1);
    g_signal_connect(controls.smoothing, "value_changed",
                     G_CALLBACK(drift_invalidate), &controls);

    row++;

    controls.color_button = gwy_color_button_new();
    gwy_color_button_set_use_alpha(GWY_COLOR_BUTTON(controls.color_button),
                                                    TRUE);
    load_mask_color(controls.color_button,
                     gwy_data_view_get_data(GWY_DATA_VIEW(controls.view)));
    gwy_table_attach_hscale(table, row++, _("_Mask color:"), NULL,
                                    GTK_OBJECT(controls.color_button),
                                    GWY_HSCALE_WIDGET_NO_EXPAND);
    g_signal_connect(controls.color_button, "clicked",
                           G_CALLBACK(mask_color_change_cb), &controls);

    row++;

    controls.is_graph = gtk_check_button_new_with_mnemonic(_("_Extract drift graph"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.is_graph),
                                 args->is_graph);
    gtk_table_attach(GTK_TABLE(table), controls.is_graph,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 2, 2);
    g_signal_connect(controls.is_graph, "toggled",
                     G_CALLBACK(drift_invalidate), &controls);
    row++;

    controls.is_correct = gtk_check_button_new_with_mnemonic(_("_Correct data"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.is_correct),
                                 args->is_correct);
    gtk_table_attach(GTK_TABLE(table), controls.is_correct,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 2, 2);
    g_signal_connect(controls.is_correct, "toggled",
                     G_CALLBACK(drift_invalidate), &controls);
    row++;

    controls.is_crop = gtk_check_button_new_with_mnemonic(_("_Crop out unknown data"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.is_crop),
                                 args->is_crop);
    gtk_table_attach(GTK_TABLE(table), controls.is_crop,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 2, 2);
    g_signal_connect(controls.is_crop, "toggled",
                     G_CALLBACK(drift_invalidate), &controls);
    row++;


    controls.computed = FALSE;

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.is_graph),
                                 args->is_graph);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.is_crop),
                                 args->is_crop);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.is_correct),
                                 args->is_correct);

    gtk_widget_show_all(dialog);
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            drift_dialog_update_values(&controls, args);
            g_object_unref(controls.viewdata);
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            return FALSE;
            break;

            case GTK_RESPONSE_OK:
            break;

            case RESPONSE_RESET:
            *args = drift_defaults;
            drift_dialog_update_controls(&controls, args);
            break;

            case RESPONSE_PREVIEW:
            drift_dialog_update_values(&controls, args);
            preview(&controls, args);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    save_mask_color(controls.color_button, data);
    drift_dialog_update_values(&controls, args);
    gtk_widget_destroy(dialog);
    drift_ok(&controls, args, data);

    return TRUE;
}

static void
drift_dialog_update_controls(DriftControls *controls,
                            DriftArgs *args)
{
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->smoothing),
                             args->smoothing);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->sensitivity),
                             args->sensitivity);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->is_graph),
                                 args->is_graph);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->is_correct),
                                 args->is_correct);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->is_crop),
                                 args->is_crop);
}

static void
drift_dialog_update_values(DriftControls *controls,
                          DriftArgs *args)
{
    args->smoothing
        = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->smoothing));
    args->sensitivity
        = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->sensitivity));
    args->is_graph
        = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->is_graph));
    args->is_crop
        = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->is_crop));
    args->is_correct
        = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->is_correct));
}

static void
drift_invalidate(G_GNUC_UNUSED GObject *obj,
                DriftControls *controls)
{
    controls->computed = FALSE;
}

static void
mask_color_change_cb(GtkWidget *color_button,
                     DriftControls *controls)
{
    gwy_color_selector_for_mask("Select color",
                                GWY_COLOR_BUTTON(color_button),
                                NULL, "/0/mask");
    load_mask_color(color_button,
                    gwy_data_view_get_data(GWY_DATA_VIEW(controls->view)));
}

static void
load_mask_color(GtkWidget *color_button,
                GwyContainer *data)
{
    GwyRGBA rgba;

    if (!gwy_rgba_get_from_container(&rgba, data, "/0/mask")) {
        gwy_rgba_get_from_container(&rgba, gwy_app_settings_get(), "/mask");
        gwy_rgba_store_to_container(&rgba, data, "/0/mask");
    }
    gwy_color_button_set_color(GWY_COLOR_BUTTON(color_button), &rgba);
}

/* FIXME: This is wrong. Look at grain_mark.c */
static void
save_mask_color(GtkWidget *color_button,
                GwyContainer *data)
{
    GwyRGBA rgba;

    gwy_color_button_get_color(GWY_COLOR_BUTTON(color_button), &rgba);
    gwy_rgba_store_to_container(&rgba, data, "/0/mask");
}

static void
preview(DriftControls *controls,
        DriftArgs *args)
{
    GwyDataField *dfield, *maskfield;
    GwyPixmapLayer *layer = NULL;

    dfield = gwy_container_get_object_by_name(controls->viewdata, "/0/data");

    if (gwy_container_gis_object_by_name(controls->viewdata, "/0/mask",
                                         &maskfield)) {
        gwy_data_field_copy(dfield, maskfield, FALSE);
    }
    else {
        maskfield = gwy_data_field_duplicate(dfield);
        gwy_container_set_object_by_name(controls->viewdata, "/0/mask",
                                         maskfield);
        g_object_unref(maskfield);
    }

    if (!gwy_data_view_get_alpha_layer(GWY_DATA_VIEW(controls->view))) {
        layer = gwy_layer_mask_new();
        gwy_pixmap_layer_set_data_key(layer, "/0/mask");
        gwy_layer_mask_set_color_key(GWY_LAYER_MASK(layer), "/0/mask");
        gwy_data_view_set_alpha_layer(GWY_DATA_VIEW(controls->view), layer);
    }

    mask_process(dfield, maskfield, args, controls);
    controls->computed = TRUE;

    gwy_data_field_data_changed(maskfield);
}

static void
reset(DriftControls *controls,
      DriftArgs *args)
{
    GwyDataField *maskfield;

    if (gwy_container_gis_object_by_name(controls->viewdata, "/0/mask",
                                         &maskfield)) {
        gwy_data_field_clear(maskfield);
        gwy_data_field_data_changed(maskfield);
    }
    controls->computed = FALSE;
}

static void
drift_ok(DriftControls *controls,
         DriftArgs *args,
         GwyContainer *data)
{

    GwyGraphCurveModel *cmodel;
    GwyGraphModel *gmodel;
    GwyDataField *data_field, *newdata_field;
    gint newid, oldid;

    /* FIXME: too late */
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &data_field,
                                     GWY_APP_DATA_FIELD_ID, &oldid,
                                     0);
    g_return_if_fail(data_field);

    if (!controls->computed) return;

    newdata_field = gwy_data_field_duplicate(data_field);

    newdata_field = gwy_data_field_correct_drift(data_field,
                                                 newdata_field,
                                                 controls->result,
                                                 args->is_crop);

    if (args->is_correct)
    {
        newid = gwy_app_data_browser_add_data_field(newdata_field, data, TRUE);

        gwy_app_copy_data_items(data, data, oldid, newid,
                            GWY_DATA_ITEM_GRADIENT,
                            GWY_DATA_ITEM_RANGE,
                            GWY_DATA_ITEM_MASK_COLOR,
                            0);

        gwy_app_set_data_field_title(data, newid, _("Drift corrected data"));

    }
    g_object_unref(newdata_field);

    if (args->is_graph)
    {
        gmodel = gwy_graph_model_new();
        cmodel = gwy_graph_curve_model_new();
        gwy_graph_model_add_curve(gmodel, cmodel);

        gwy_graph_model_set_title(gmodel, _("Drift graph"));
        gwy_graph_model_set_units_from_data_line(gmodel, controls->result);
        gwy_graph_curve_model_set_description(cmodel, "x-axis drift");
        gwy_graph_curve_model_set_data_from_dataline(cmodel, controls->result, 0, 0);

        newid = gwy_app_data_browser_add_graph_model(gmodel, data, TRUE);
        gwy_object_unref(cmodel);
        gwy_object_unref(gmodel);
        gwy_object_unref(controls->result);
        gwy_app_set_data_field_title(data, newid, _("X axis Drift"));
    }
}

static void
mask_process(GwyDataField *dfield,
             GwyDataField *maskfield,
             DriftArgs *args,
             DriftControls *controls)
{
    gint i, j, step, pos, xres, yres;
    gdouble *mdata, *rdata;

    gwy_data_field_clear(maskfield);
    gwy_data_line_clear(controls->result);

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);

    if (args->method == GWY_DRIFT_CORRELATION)
        gwy_data_field_get_drift_from_correlation
            (dfield, controls->result,
             MAX(1, (gint)(args->sensitivity/10)),
             100.0/8.0 - MAX(1, (gint)(args->smoothing/8)),
             1 - args->sensitivity/500.0);

    step = yres/10;
    mdata = gwy_data_field_get_data(maskfield);
    rdata = gwy_data_line_get_data(controls->result);
    for (i = 0; i < yres; i++) {
        for (j = -step; j < xres + step; j += step) {
            pos = j + gwy_data_field_rtoi(dfield, rdata[i]);
            if (pos > 1 && pos < xres) {
                mdata[(gint)(pos + i*xres)] = 1;
                if (xres >= 300)
                    mdata[(gint)(pos - 1 + i*xres)] = 1;
            }
        }
    }
}

static const gchar iscorrect_key[]     = "/module/drift/iscorrect";
static const gchar isgraph_key[]       = "/module/drift/isgraph";
static const gchar iscrop_key[]        = "/module/drift/iscrop";
static const gchar sensitivity_key[]   = "/module/drift/sensitivity";
static const gchar smoothing_key[]     = "/module/drift/smoothing";
static const gchar method_key[]        = "/module/drift/method";
static const gchar interpolation_key[] = "/module/drift/interpolation";

static void
drift_sanitize_args(DriftArgs *args)
{
    args->is_correct = !!args->is_correct;
    args->is_crop = !!args->is_crop;
    args->is_graph = !!args->is_graph;
    args->sensitivity = CLAMP(args->sensitivity, 0.0, 100.0);
    args->smoothing = CLAMP(args->smoothing, 0.0, 100.0);
    args->method = MIN(args->method, GWY_DRIFT_SAMPLE);
    args->interpolation = MIN(args->interpolation, GWY_INTERPOLATION_NNA);
}

static void
drift_load_args(GwyContainer *container,
               DriftArgs *args)
{
    *args = drift_defaults;

    gwy_container_gis_boolean_by_name(container, iscorrect_key, &args->is_correct);
    gwy_container_gis_boolean_by_name(container, iscrop_key,
                                      &args->is_crop);
    gwy_container_gis_boolean_by_name(container, isgraph_key, &args->is_graph);
    gwy_container_gis_double_by_name(container, sensitivity_key, &args->sensitivity);
    gwy_container_gis_double_by_name(container, smoothing_key, &args->smoothing);
    gwy_container_gis_enum_by_name(container, method_key,
                                   &args->method);
    gwy_container_gis_enum_by_name(container, interpolation_key,
                                   &args->interpolation);
     drift_sanitize_args(args);
}

static void
drift_save_args(GwyContainer *container,
               DriftArgs *args)
{
    gwy_container_set_boolean_by_name(container, isgraph_key, args->is_graph);
    gwy_container_set_boolean_by_name(container, iscorrect_key, args->is_correct);
    gwy_container_set_boolean_by_name(container, iscrop_key, args->is_crop);
    gwy_container_set_double_by_name(container, sensitivity_key, args->sensitivity);
    gwy_container_set_double_by_name(container, smoothing_key, args->smoothing);
    gwy_container_set_enum_by_name(container, interpolation_key, args->interpolation);
    gwy_container_set_enum_by_name(container, method_key, args->method);
}

/* VIM: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
