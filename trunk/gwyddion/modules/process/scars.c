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

#include <string.h>

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule.h>
#include <libgwydgets/gwydgets.h>
#include <libprocess/datafield.h>
#include <app/gwyapp.h>

#define SCARS_MARK_RUN_MODES \
    (GWY_RUN_MODAL | GWY_RUN_NONINTERACTIVE | GWY_RUN_WITH_DEFAULTS)

#define SCARS_RUN_MODES \
    (GWY_RUN_NONINTERACTIVE | GWY_RUN_WITH_DEFAULTS)

typedef struct {
    gboolean inverted;   /* unused */
    gdouble threshold_high;
    gdouble threshold_low;
    gint min_len;
    gint max_width;
} ScarsArgs;

typedef struct {
    GtkWidget *view;
    GtkObject *threshold_high;
    GtkObject *threshold_low;
    GtkObject *min_len;
    GtkObject *max_width;
    GtkWidget *color_button;
    GwyContainer *mydata;
    ScarsArgs *args;
} ScarsControls;

static gboolean    module_register                   (const gchar *name);
static gboolean    scars_remove                      (GwyContainer *data,
                                                      GwyRunType run);
static gboolean    scars_mark                        (GwyContainer *data,
                                                      GwyRunType run);
static void        load_mask_color                   (GtkWidget *color_button,
                                                      GwyContainer *data);
static void        save_mask_color                   (GtkWidget *color_button,
                                                      GwyContainer *data);
static gboolean    scars_mark_dialog                 (ScarsArgs *args,
                                                      GwyContainer *data);
static void        scars_mark_dialog_update_controls (ScarsControls *controls,
                                                      ScarsArgs *args);
static void        scars_mark_dialog_update_values   (ScarsControls *controls,
                                                      ScarsArgs *args);
static void        scars_mark_dialog_update_thresholds(GtkObject *adj,
                                                       ScarsControls *controls);
static void        mask_color_change_cb              (GtkWidget *color_button,
                                                      ScarsControls *controls);
static void        preview                           (ScarsControls *controls,
                                                      ScarsArgs *args);
static void        scars_mark_do                     (ScarsArgs *args,
                                                      GwyContainer *data);
static void        scars_mark_load_args              (GwyContainer *container,
                                                      ScarsArgs *args);
static void        scars_mark_save_args              (GwyContainer *container,
                                                      ScarsArgs *args);

static const ScarsArgs scars_defaults = {
    FALSE,
    0.666,
    0.25,
    16,
    4
};

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "scars",
    "Scar detection and removal.",
    "Yeti <yeti@gwyddion.net>",
    "1.1",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyProcessFuncInfo scars_mark_func_info = {
        "scars_mark",
        "/_Correct Data/M_ark Scars",
        (GwyProcessFunc)&scars_mark,
        SCARS_MARK_RUN_MODES,
        0,
    };
    static GwyProcessFuncInfo scars_remove_func_info = {
        "scars_remove",
        "/_Correct Data/Remove _Scars",
        (GwyProcessFunc)&scars_remove,
        SCARS_RUN_MODES,
        0,
    };

    gwy_process_func_register(name, &scars_mark_func_info);
    gwy_process_func_register(name, &scars_remove_func_info);

    return TRUE;
}

static gboolean
scars_remove(GwyContainer *data, GwyRunType run)
{
    ScarsArgs args;
    GwyDataField *dfield, *mask;
    gint xres, yres, i, j, k;
    gdouble *d, *m;

    g_return_val_if_fail(run & SCARS_RUN_MODES, FALSE);
    if (run == GWY_RUN_WITH_DEFAULTS)
        args = scars_defaults;
    else
        scars_mark_load_args(gwy_app_settings_get(), &args);

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    d = gwy_data_field_get_data(dfield);

    gwy_app_undo_checkpoint(data, "/0/data", NULL);
    mask = GWY_DATA_FIELD(gwy_data_field_new(xres, yres,
                                             gwy_data_field_get_xreal(dfield),
                                             gwy_data_field_get_yreal(dfield),
                                             FALSE));
    gwy_data_field_mark_scars(dfield, mask,
                              args.threshold_high, args.threshold_low,
                              args.min_len, args.max_width);
    m = gwy_data_field_get_data(mask);

    /* interpolate */
    for (i = 1; i < yres-1; i++) {
        for (j = 0; j < xres; j++) {
            if (m[i*xres + j] > 0.0) {
                gdouble first, last;
                gint width;

                first = d[(i - 1)*xres + j];
                for (k = 1; m[(i + k)*xres + j] > 0.0; k++)
                    ;
                last = d[(i + k)*xres + j];
                width = k + 1;
                while (k) {
                    gdouble x = (gdouble)k/width;

                    d[(i + k - 1)*xres + j] = x*last + (1.0 - x)*first;
                    m[(i + k - 1)*xres + j] = 0.0;
                    k--;
                }
            }
        }
    }

    g_object_unref(mask);

    return TRUE;
}

static gboolean
scars_mark(GwyContainer *data, GwyRunType run)
{
    ScarsArgs args;
    gboolean ok = FALSE;

    g_return_val_if_fail(run & SCARS_MARK_RUN_MODES, FALSE);
    if (run == GWY_RUN_WITH_DEFAULTS)
        args = scars_defaults;
    else
        scars_mark_load_args(gwy_app_settings_get(), &args);

    ok = (run != GWY_RUN_MODAL) || scars_mark_dialog(&args, data);
    if (run == GWY_RUN_MODAL)
        scars_mark_save_args(gwy_app_settings_get(), &args);
    if (!ok)
        return FALSE;

    scars_mark_do(&args, data);

    return ok;
}

static void
scars_mark_do(ScarsArgs *args, GwyContainer *data)
{
    GObject *dfield, *mask;

    dfield = gwy_container_get_object_by_name(data, "/0/data");

    gwy_app_undo_checkpoint(data, "/0/mask", NULL);
    if (!gwy_container_gis_object_by_name(data, "/0/mask", &mask)) {
        mask = gwy_serializable_duplicate(dfield);
        gwy_container_set_object_by_name(data, "/0/mask", mask);
        g_object_unref(mask);
    }
    gwy_data_field_mark_scars(GWY_DATA_FIELD(dfield), GWY_DATA_FIELD(mask),
                              args->threshold_high, args->threshold_low,
                              args->min_len, args->max_width);
}

static gboolean
scars_mark_dialog(ScarsArgs *args, GwyContainer *data)
{
    GtkWidget *dialog, *table, *spin;
    ScarsControls controls;
    enum { RESPONSE_RESET = 1,
           RESPONSE_PREVIEW = 2 };
    gint response;
    gdouble zoomval;
    GtkObject *layer;
    GtkWidget *hbox;
    GwyDataField *dfield;
    GtkWidget *label;
    gint row;

    controls.args = args;
    dialog = gtk_dialog_new_with_buttons(_("Mark scars"),
                                         NULL,
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         _("Update preview"), RESPONSE_PREVIEW,
                                         _("Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);

    hbox = gtk_hbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 4);

    controls.mydata = GWY_CONTAINER(gwy_serializable_duplicate(G_OBJECT(data)));
    controls.view = gwy_data_view_new(controls.mydata);
    g_object_unref(controls.mydata);
    layer = gwy_layer_basic_new();
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.view),
                                 GWY_PIXMAP_LAYER(layer));
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls.mydata,
                                                             "/0/data"));

    zoomval = 400.0/MAX(gwy_data_field_get_xres(dfield),
                        gwy_data_field_get_yres(dfield));
    gwy_data_view_set_zoom(GWY_DATA_VIEW(controls.view), zoomval);

    gtk_box_pack_start(GTK_BOX(hbox), controls.view, FALSE, FALSE, 4);

    table = gtk_table_new(10, 3, FALSE);
    gtk_box_pack_start(GTK_BOX(hbox), table, FALSE, FALSE, 4);
    row = 0;

    controls.max_width = gtk_adjustment_new(args->max_width,
                                            1.0, 16.0, 1, 3, 0);
    spin = gwy_table_attach_spinbutton(table, row++, _("Maximum width"),
                                       "samples", controls.max_width);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 0);

    controls.min_len = gtk_adjustment_new(args->min_len,
                                          1.0, 1024.0, 1, 10, 0);
    spin = gwy_table_attach_spinbutton(table, row++, _("Minimum length"),
                                       "samples", controls.min_len);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 0);

    controls.threshold_high = gtk_adjustment_new(args->threshold_high,
                                                 0.0, 2.0, 0.01, 0.1, 0);
    spin = gwy_table_attach_spinbutton(table, row++, _("Hard threshold"),
                                       "% RMS", controls.threshold_high);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 2);
    g_signal_connect(controls.threshold_high, "value_changed",
                     G_CALLBACK(scars_mark_dialog_update_thresholds),
                     &controls);

    controls.threshold_low = gtk_adjustment_new(args->threshold_low,
                                                0.0, 2.0, 0.01, 0.1, 0);
    spin = gwy_table_attach_spinbutton(table, row, _("Soft threshold"),
                                       "% RMS", controls.threshold_low);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 2);
    g_signal_connect(controls.threshold_low, "value_changed",
                     G_CALLBACK(scars_mark_dialog_update_thresholds),
                     &controls);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    label = gtk_label_new_with_mnemonic(_("Preview _mask color:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,  0, 1, row, row+1,
                     GTK_FILL, 0, 2, 2);
    controls.color_button = gwy_color_button_new();
    gwy_color_button_set_use_alpha(GWY_COLOR_BUTTON(controls.color_button),
                                   TRUE);
    load_mask_color(controls.color_button,
                    gwy_data_view_get_data(GWY_DATA_VIEW(controls.view)));
    gtk_table_attach(GTK_TABLE(table), controls.color_button,
                     1, 2, row, row+1, GTK_FILL, 0, 2, 2);

    g_signal_connect(controls.color_button, "clicked",
                     G_CALLBACK(mask_color_change_cb), &controls);

    gtk_widget_show_all(dialog);
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            scars_mark_dialog_update_values(&controls, args);
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            return FALSE;
            break;

            case GTK_RESPONSE_OK:
            break;

            case RESPONSE_RESET:
            *args = scars_defaults;
            scars_mark_dialog_update_controls(&controls, args);
            break;

            case RESPONSE_PREVIEW:
            scars_mark_dialog_update_values(&controls, args);
            preview(&controls, args);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    scars_mark_dialog_update_values(&controls, args);
    save_mask_color(controls.color_button, data);
    gtk_widget_destroy(dialog);

    return TRUE;
}

static void
scars_mark_dialog_update_thresholds(GtkObject *adj,
                                    ScarsControls *controls)
{
    static gboolean in_update = FALSE;
    ScarsArgs *args;

    if (in_update)
        return;

    in_update = TRUE;
    args = controls->args;
    if (adj == controls->threshold_high) {
        args->threshold_high = gtk_adjustment_get_value(GTK_ADJUSTMENT(adj));
        if (args->threshold_low > args->threshold_high)
            gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->threshold_low),
                                     args->threshold_high);
    }
    else if (adj == controls->threshold_low) {
        args->threshold_low = gtk_adjustment_get_value(GTK_ADJUSTMENT(adj));
        if (args->threshold_low > args->threshold_high)
            gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->threshold_high),
                                     args->threshold_low);
    }
    else {
        g_assert_not_reached();
    }

    in_update = FALSE;
}

static void
scars_mark_dialog_update_controls(ScarsControls *controls,
                                  ScarsArgs *args)
{
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->threshold_high),
                             args->threshold_high);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->threshold_low),
                             args->threshold_low);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->min_len),
                             args->min_len);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->max_width),
                             args->max_width);
}

static void
scars_mark_dialog_update_values(ScarsControls *controls,
                                ScarsArgs *args)
{
    args->threshold_high
        = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->threshold_high));
    args->threshold_low
        = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->threshold_low));
    args->min_len
        = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->min_len));
    args->max_width
        = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->max_width));
}

static void
mask_color_change_cb(GtkWidget *color_button,
                     ScarsControls *controls)
{
    gwy_color_selector_for_mask(NULL,
                                GWY_DATA_VIEW(controls->view),
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

static void
save_mask_color(GtkWidget *color_button,
                GwyContainer *data)
{
    GwyRGBA rgba;

    gwy_color_button_get_color(GWY_COLOR_BUTTON(color_button), &rgba);
    gwy_rgba_store_to_container(&rgba, data, "/0/mask");
}

static void
preview(ScarsControls *controls,
        ScarsArgs *args)
{
    GwyDataField *mask, *dfield;
    GwyPixmapLayer *layer;

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                             "/0/data"));

    /*set up the mask*/
    if (gwy_container_contains_by_name(controls->mydata, "/0/mask")) {
        mask = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                               "/0/mask"));
        gwy_data_field_resample(mask,
                                gwy_data_field_get_xres(dfield),
                                gwy_data_field_get_yres(dfield),
                                GWY_INTERPOLATION_NONE);
        gwy_data_field_copy(dfield, mask);
        if (!gwy_data_view_get_alpha_layer(GWY_DATA_VIEW(controls->view))) {
            layer = GWY_PIXMAP_LAYER(gwy_layer_mask_new());
            gwy_data_view_set_alpha_layer(GWY_DATA_VIEW(controls->view),
                                          GWY_PIXMAP_LAYER(layer));
        }
    }
    else {
        mask = GWY_DATA_FIELD(gwy_serializable_duplicate(G_OBJECT(dfield)));
        gwy_container_set_object_by_name(controls->mydata, "/0/mask",
                                         G_OBJECT(mask));
        layer = GWY_PIXMAP_LAYER(gwy_layer_mask_new());
        gwy_data_view_set_alpha_layer(GWY_DATA_VIEW(controls->view),
                                      GWY_PIXMAP_LAYER(layer));

    }

    scars_mark_do(args, controls->mydata);

    gwy_data_view_update(GWY_DATA_VIEW(controls->view));

}

static const gchar *threshold_low_key = "/module/scars/threshold_low";
static const gchar *threshold_high_key = "/module/scars/threshold_high";
static const gchar *min_len_key= "/module/scars/min_len";
static const gchar *max_width_key= "/module/scars/max_width";

static void
scars_mark_sanitize_args(ScarsArgs *args)
{
    args->inverted = !!args->inverted;
    args->threshold_low = MAX(args->threshold_low, 0.0);
    args->threshold_high = MAX(args->threshold_low, args->threshold_high);
    args->min_len = MAX(args->min_len, 1);
    args->max_width = CLAMP(args->max_width, 1, 16);
}

static void
scars_mark_load_args(GwyContainer *container,
                     ScarsArgs *args)
{
    *args = scars_defaults;

    gwy_container_gis_double_by_name(container, threshold_high_key,
                                     &args->threshold_high);
    gwy_container_gis_double_by_name(container, threshold_low_key,
                                     &args->threshold_low);
    gwy_container_gis_int32_by_name(container, min_len_key, &args->min_len);
    gwy_container_gis_int32_by_name(container, max_width_key, &args->max_width);
    scars_mark_sanitize_args(args);
}

static void
scars_mark_save_args(GwyContainer *container,
                     ScarsArgs *args)
{
    gwy_container_set_double_by_name(container, threshold_high_key,
                                     args->threshold_high);
    gwy_container_set_double_by_name(container, threshold_low_key,
                                     args->threshold_low);
    gwy_container_set_int32_by_name(container, min_len_key, args->min_len);
    gwy_container_set_int32_by_name(container, max_width_key, args->max_width);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
