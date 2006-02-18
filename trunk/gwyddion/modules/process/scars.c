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
#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule.h>
#include <libgwydgets/gwydgets.h>
#include <libprocess/arithmetic.h>
#include <app/gwyapp.h>

#define SCARS_MARK_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)
#define SCARS_REMOVE_RUN_MODES GWY_RUN_IMMEDIATE

typedef enum {
    FEATURES_POSITIVE = 1 << 0,
    FEATURES_NEGATIVE = 1 << 2,
    FEATURES_BOTH     = (FEATURES_POSITIVE | FEATURES_NEGATIVE),
} GwyFeaturesType;

enum {
    PREVIEW_SIZE = 320,
    MAX_LENGTH = 1024
};

typedef struct {
    GwyFeaturesType type;
    gdouble threshold_high;
    gdouble threshold_low;
    gint min_len;
    gint max_width;
} ScarsArgs;

typedef struct {
    ScarsArgs *args;
    GSList *type;
    GtkWidget *view;
    GtkObject *threshold_high;
    GtkObject *threshold_low;
    GtkObject *min_len;
    GtkObject *max_width;
    GtkWidget *color_button;
    GwyContainer *mydata;
    gboolean computed;
} ScarsControls;

static gboolean module_register                    (const gchar *name);
static void     scars_remove                       (GwyContainer *data,
                                                    GwyRunType run);
static void     scars_mark                         (GwyContainer *data,
                                                    GwyRunType run);
static void     run_noninteractive                 (ScarsArgs *args,
                                                    GwyContainer *data);
static void     load_mask_color                    (GtkWidget *color_button,
                                                    GwyContainer *data);
static void     scars_mark_dialog                  (ScarsArgs *args,
                                                    GwyContainer *data);
static void     scars_mark_dialog_update_controls  (ScarsControls *controls,
                                                    ScarsArgs *args);
static void     scars_mark_dialog_update_values    (ScarsControls *controls,
                                                    ScarsArgs *args);
static void     scars_mark_dialog_update_thresholds(GtkObject *adj,
                                                    ScarsControls *controls);
static void     scars_invalidate                   (ScarsControls *controls);
static void     mask_color_change_cb               (GtkWidget *color_button,
                                                    ScarsControls *controls);
static void     preview                            (ScarsControls *controls,
                                                    ScarsArgs *args);
static void     scars_mark_load_args               (GwyContainer *container,
                                                    ScarsArgs *args);
static void     scars_mark_save_args               (GwyContainer *container,
                                                    ScarsArgs *args);

static const ScarsArgs scars_defaults = {
    FEATURES_BOTH,
    0.666,
    0.25,
    16,
    4
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Marks and/or removes scars (horizontal linear artefacts)."),
    "Yeti <yeti@gwyddion.net>",
    "1.5",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    gwy_process_func_registe2("scars_mark",
                              (GwyProcessFunc)&scars_mark,
                              N_("/_Correct Data/M_ark Scars..."),
                              GWY_STOCK_SCARS,
                              SCARS_MARK_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Mark horizontal scars (strokes)"));
    gwy_process_func_registe2("scars_remove",
                              (GwyProcessFunc)&scars_remove,
                              N_("/_Correct Data/Remove _Scars"),
                              GWY_STOCK_SCARS,
                              SCARS_REMOVE_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Correct horizontal scars (strokes)"));

    return TRUE;
}

/**
 * gwy_data_field_mark_scars:
 * @data_field: A data field to find scars in.
 * @scar_field: A data field to store the result to (it is resized to match
 *              @data_field).
 * @threshold_high: Miminum relative step for scar marking, must be positive.
 * @threshold_low: Definite relative step for scar marking, must be at least
 *                 equal to @threshold_high.
 * @min_scar_len: Minimum length of a scar, shorter ones are discarded
 *                (must be at least one).
 * @max_scar_width: Maximum width of a scar, must be at least one.
 * @negative: %TRUE to detect negative scars, %FALSE to positive.
 *
 * Find and marks scars in a data field.
 *
 * Scars are linear horizontal defects, consisting of shifted values.
 * Zero or negative values in @scar_field siginify normal data, positive
 * values siginify samples that are part of a scar.
 **/
static void
gwy_data_field_mark_scars(GwyDataField *data_field,
                          GwyDataField *scar_field,
                          gdouble threshold_high,
                          gdouble threshold_low,
                          gdouble min_scar_len,
                          gdouble max_scar_width,
                          gboolean negative)
{
    gint xres, yres, i, j, k;
    gdouble rms;
    const gdouble *d;
    gdouble *m;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    g_return_if_fail(GWY_IS_DATA_FIELD(scar_field));
    g_return_if_fail(max_scar_width >= 1 && max_scar_width <= 16);
    g_return_if_fail(min_scar_len >= 1);
    g_return_if_fail(threshold_low >= 0.0);
    g_return_if_fail(threshold_high >= threshold_low);
    xres = gwy_data_field_get_xres(data_field);
    yres = gwy_data_field_get_yres(data_field);
    d = gwy_data_field_get_data_const(data_field);
    gwy_data_field_resample(scar_field, xres, yres, GWY_INTERPOLATION_NONE);
    gwy_data_field_clear(scar_field);
    m = gwy_data_field_get_data(scar_field);

    if (min_scar_len > xres)
        return;
    max_scar_width = MIN(max_scar_width, yres - 2);

    /* compute `vertical rms' */
    rms = 0.0;
    for (i = 0; i < yres-1; i++) {
        const gdouble *row = d + i*xres;

        for (j = 0; j < xres; j++) {
            gdouble z = row[j] - row[j + xres];

            rms += z*z;
        }
    }
    rms = sqrt(rms/(xres*yres));
    if (rms == 0.0)
        return;

    /* initial scar search */
    for (i = 0; i < yres - (max_scar_width + 1); i++) {
        for (j = 0; j < xres; j++) {
            gdouble top, bottom;
            const gdouble *row = d + i*xres + j;

            if (negative) {
                top = row[0];
                bottom = row[xres];
                for (k = 1; k <= max_scar_width; k++) {
                    top = MIN(row[0], row[xres*(k + 1)]);
                    bottom = MAX(bottom, row[xres*k]);
                    if (top - bottom >= threshold_low*rms)
                        break;
                }
                if (k <= max_scar_width) {
                    gdouble *mrow = m + i*xres + j;

                    while (k) {
                        mrow[k*xres] = (top - row[k*xres])/rms;
                        k--;
                    }
                }
            }
            else {
                bottom = row[0];
                top = row[xres];
                for (k = 1; k <= max_scar_width; k++) {
                    bottom = MAX(row[0], row[xres*(k + 1)]);
                    top = MIN(top, row[xres*k]);
                    if (top - bottom >= threshold_low*rms)
                        break;
                }
                if (k <= max_scar_width) {
                    gdouble *mrow = m + i*xres + j;

                    while (k) {
                        mrow[k*xres] = (row[k*xres] - bottom)/rms;
                        k--;
                    }
                }
            }
        }
    }
    /* expand high threshold to neighbouring low threshold */
    for (i = 0; i < yres; i++) {
        gdouble *mrow = m + i*xres;

        for (j = 1; j < xres; j++) {
            if (mrow[j] >= threshold_low && mrow[j-1] >= threshold_high)
                mrow[j] = threshold_high;
        }
        for (j = xres-1; j > 0; j--) {
            if (mrow[j-1] >= threshold_low && mrow[j] >= threshold_high)
                mrow[j-1] = threshold_high;
        }
    }
    /* kill too short segments, clamping scar_field along the way */
    for (i = 0; i < yres; i++) {
        gdouble *mrow = m + i*xres;

        k = 0;
        for (j = 0; j < xres; j++) {
            if (mrow[j] >= threshold_high) {
                mrow[j] = 1.0;
                k++;
                continue;
            }
            if (k && k < min_scar_len) {
                while (k) {
                    mrow[j-k] = 0.0;
                    k--;
                }
            }
            mrow[j] = 0.0;
            k = 0;
        }
        if (k && k < min_scar_len) {
            while (k) {
                mrow[j-k] = 0.0;
                k--;
            }
        }
    }
}

static void
mark_scars(GwyDataField *dfield,
           GwyDataField *mfield,
           const ScarsArgs *args)
{
    GwyDataField *tmp;

    switch (args->type) {
        case FEATURES_POSITIVE:
        case FEATURES_NEGATIVE:
        gwy_data_field_mark_scars(dfield, mfield,
                                  args->threshold_high, args->threshold_low,
                                  args->min_len, args->max_width,
                                  args->type == FEATURES_NEGATIVE);
        break;

        case FEATURES_BOTH:
        gwy_data_field_mark_scars(dfield, mfield,
                                  args->threshold_high, args->threshold_low,
                                  args->min_len, args->max_width, FALSE);
        tmp = gwy_data_field_new_alike(dfield, FALSE);
        gwy_data_field_mark_scars(dfield, tmp,
                                  args->threshold_high, args->threshold_low,
                                  args->min_len, args->max_width, TRUE);
        gwy_data_field_max_of_fields(mfield, mfield, tmp);
        g_object_unref(tmp);
        break;
    }
}

static void
scars_remove(GwyContainer *data, GwyRunType run)
{
    ScarsArgs args;
    GwyDataField *dfield, *mfield;
    GQuark dquark;
    gint xres, yres, i, j, k;
    gdouble *d, *m;

    g_return_if_fail(run & SCARS_REMOVE_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD_KEY, &dquark,
                                     GWY_APP_DATA_FIELD, &dfield,
                                     0);
    g_return_if_fail(dfield && dquark);
    scars_mark_load_args(gwy_app_settings_get(), &args);
    gwy_app_undo_qcheckpointv(data, 1, &dquark);

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    d = gwy_data_field_get_data(dfield);

    mfield = gwy_data_field_new_alike(dfield, FALSE);
    mark_scars(dfield, mfield, &args);
    m = gwy_data_field_get_data(mfield);

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
    g_object_unref(mfield);

    gwy_data_field_data_changed(dfield);
}

static GwyDataField*
create_mask_field(GwyDataField *dfield)
{
    GwyDataField *mfield;
    GwySIUnit *siunit;

    mfield = gwy_data_field_new_alike(dfield, FALSE);
    siunit = gwy_si_unit_new("");
    gwy_data_field_set_si_unit_z(mfield, siunit);
    g_object_unref(siunit);

    return mfield;
}

static void
scars_mark(GwyContainer *data, GwyRunType run)
{
    ScarsArgs args;

    g_return_if_fail(run & SCARS_MARK_RUN_MODES);
    scars_mark_load_args(gwy_app_settings_get(), &args);
    if (run == GWY_RUN_IMMEDIATE)
        run_noninteractive(&args, data);
    else {
        scars_mark_dialog(&args, data);
        scars_mark_save_args(gwy_app_settings_get(), &args);
    }
}

static void
run_noninteractive(ScarsArgs *args, GwyContainer *data)
{
    GwyDataField *dfield, *mfield;
    GQuark mquark;

    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_MASK_FIELD_KEY, &mquark,
                                     0);
    g_return_if_fail(dfield && mquark);

    gwy_app_undo_qcheckpointv(data, 1, &mquark);
    mfield = create_mask_field(dfield);
    mark_scars(dfield, mfield, args);
    gwy_container_set_object(data, mquark, mfield);
    g_object_unref(mfield);
}

static void
scars_mark_dialog(ScarsArgs *args,
                  GwyContainer *data)
{
    enum {
        RESPONSE_RESET = 1,
        RESPONSE_PREVIEW = 2
    };
    static const GwyEnum types[] = {
        { N_("Positive"), FEATURES_POSITIVE, },
        { N_("Negative"), FEATURES_NEGATIVE, },
        { N_("Both"),     FEATURES_BOTH,     },
    };
    GtkWidget *dialog, *table, *spin, *hbox, *label;
    GwyDataField *dfield, *mfield;
    GQuark mquark;
    ScarsControls controls;
    gint response;
    gdouble zoomval;
    GwyPixmapLayer *layer;
    GSList *group;
    gint row, id;

    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     GWY_APP_MASK_FIELD_KEY, &mquark,
                                     0);
    g_return_if_fail(dfield);

    controls.args = args;
    dialog = gtk_dialog_new_with_buttons(_("Mark Scars"),
                                         NULL,
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         _("_Update preview"), RESPONSE_PREVIEW,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    hbox = gtk_hbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 4);

    controls.mydata = gwy_container_new();
    gwy_container_set_object_by_name(controls.mydata, "/0/data", dfield);
    gwy_app_copy_data_items(data, controls.mydata, id, 0,
                            GWY_DATA_ITEM_PALETTE,
                            GWY_DATA_ITEM_MASK_COLOR,
                            GWY_DATA_ITEM_RANGE,
                            0);
    controls.view = gwy_data_view_new(controls.mydata);
    layer = gwy_layer_basic_new();
    gwy_pixmap_layer_set_data_key(layer, "/0/data");
    gwy_layer_basic_set_gradient_key(GWY_LAYER_BASIC(layer), "/0/base/palette");
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.view), layer);
    zoomval = PREVIEW_SIZE/(gdouble)MAX(gwy_data_field_get_xres(dfield),
                                        gwy_data_field_get_yres(dfield));
    gwy_data_view_set_zoom(GWY_DATA_VIEW(controls.view), zoomval);

    gtk_box_pack_start(GTK_BOX(hbox), controls.view, FALSE, FALSE, 4);

    table = gtk_table_new(10, 4, FALSE);
    gtk_box_pack_start(GTK_BOX(hbox), table, TRUE, TRUE, 4);
    row = 0;

    controls.max_width = gtk_adjustment_new(args->max_width,
                                            1.0, 16.0, 1, 3, 0);
    gwy_table_attach_hscale(table, row++, _("Maximum _width:"), "px",
                            controls.max_width, 0);
    g_signal_connect_swapped(controls.max_width, "value-changed",
                             G_CALLBACK(scars_invalidate), &controls);

    controls.min_len = gtk_adjustment_new(args->min_len,
                                          1.0, MAX_LENGTH, 1, 10, 0);
    gwy_table_attach_hscale(table, row++, _("Minimum _length:"), "px",
                            controls.min_len, GWY_HSCALE_SQRT);
    g_signal_connect_swapped(controls.min_len, "value-changed",
                             G_CALLBACK(scars_invalidate), &controls);

    controls.threshold_high = gtk_adjustment_new(args->threshold_high,
                                                 0.0, 2.0, 0.01, 0.1, 0);
    spin = gwy_table_attach_hscale(table, row++, _("_Hard threshold:"),
                                   _("RMS"), controls.threshold_high, 0);
    g_signal_connect(controls.threshold_high, "value-changed",
                     G_CALLBACK(scars_mark_dialog_update_thresholds),
                     &controls);

    controls.threshold_low = gtk_adjustment_new(args->threshold_low,
                                                0.0, 2.0, 0.01, 0.1, 0);
    spin = gwy_table_attach_hscale(table, row, _("_Soft threshold:"), _("RMS"),
                                   controls.threshold_low, 0);
    g_signal_connect(controls.threshold_low, "value-changed",
                     G_CALLBACK(scars_mark_dialog_update_thresholds),
                     &controls);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    label = gtk_label_new(_("Scars type:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 4, row, row+1, GTK_EXPAND | GTK_FILL, 0, 2, 2);
    row++;

    group = gwy_radio_buttons_create(types, G_N_ELEMENTS(types), "type",
                                     NULL, NULL, args->type);
    controls.type = group;
    while (group) {
        gtk_table_attach(GTK_TABLE(table), GTK_WIDGET(group->data),
                         0, 4, row, row+1, GTK_EXPAND | GTK_FILL, 0, 2, 2);
        g_signal_connect_swapped(group->data, "toggled",
                                 G_CALLBACK(scars_invalidate), &controls);
        group = g_slist_next(group);
        row++;
    }

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

    scars_invalidate(&controls);

    gtk_widget_show_all(dialog);
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            scars_mark_dialog_update_values(&controls, args);
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            g_object_unref(controls.mydata);
            return;
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
    gwy_app_copy_data_items(controls.mydata, data, 0, id,
                            GWY_DATA_ITEM_MASK_COLOR,
                            0);
    gtk_widget_destroy(dialog);

    if (controls.computed) {
        mfield = gwy_container_get_object_by_name(controls.mydata, "/0/mask");
        gwy_app_undo_qcheckpointv(data, 1, &mquark);
        gwy_container_set_object(data, mquark, mfield);
        g_object_unref(controls.mydata);
    }
    else {
        g_object_unref(controls.mydata);
        run_noninteractive(args, data);
    }
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
    scars_invalidate(controls);
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
    gwy_radio_buttons_set_current(controls->type, "type", args->type);
}

static void
scars_mark_dialog_update_values(ScarsControls *controls,
                                ScarsArgs *args)
{
    args->threshold_high
        = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->threshold_high));
    args->threshold_low
        = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->threshold_low));
    args->min_len = gwy_adjustment_get_int(controls->min_len);
    args->max_width = gwy_adjustment_get_int(controls->max_width);
    args->type = gwy_radio_buttons_get_current(controls->type, "type");
}

static void
scars_invalidate(ScarsControls *controls)
{
    controls->computed = FALSE;
}

static void
mask_color_change_cb(GtkWidget *color_button,
                     ScarsControls *controls)
{
    GwyContainer *data;

    data = gwy_data_view_get_data(GWY_DATA_VIEW(controls->view));
    gwy_color_selector_for_mask(NULL, GWY_COLOR_BUTTON(color_button), data,
                                "/0/mask");
    load_mask_color(color_button, data);
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
preview(ScarsControls *controls,
        ScarsArgs *args)
{
    GwyDataField *mask, *dfield;
    GwyPixmapLayer *layer;

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                             "/0/data"));

    /* Set up the mask */
    if (!gwy_container_gis_object_by_name(controls->mydata, "/0/mask", &mask)) {
        mask = create_mask_field(dfield);
        gwy_container_set_object_by_name(controls->mydata, "/0/mask", mask);
        g_object_unref(mask);

        layer = gwy_layer_mask_new();
        gwy_pixmap_layer_set_data_key(layer, "/0/mask");
        gwy_layer_mask_set_color_key(GWY_LAYER_MASK(layer), "/0/mask");
        gwy_data_view_set_alpha_layer(GWY_DATA_VIEW(controls->view), layer);
    }
    gwy_data_field_copy(dfield, mask, FALSE);
    mark_scars(dfield, mask, args);
    gwy_data_field_data_changed(mask);

    controls->computed = TRUE;
}

static const gchar type_key[]           = "/module/scars/type";
static const gchar threshold_low_key[]  = "/module/scars/threshold_low";
static const gchar threshold_high_key[] = "/module/scars/threshold_high";
static const gchar min_len_key[]        = "/module/scars/min_len";
static const gchar max_width_key[]      = "/module/scars/max_width";

static void
scars_mark_sanitize_args(ScarsArgs *args)
{
    args->type = CLAMP(args->type, FEATURES_POSITIVE, FEATURES_BOTH);
    args->threshold_low = MAX(args->threshold_low, 0.0);
    args->threshold_high = MAX(args->threshold_low, args->threshold_high);
    args->min_len = CLAMP(args->min_len, 1, MAX_LENGTH);
    args->max_width = CLAMP(args->max_width, 1, 16);
}

static void
scars_mark_load_args(GwyContainer *container,
                     ScarsArgs *args)
{
    *args = scars_defaults;

    gwy_container_gis_enum_by_name(container, type_key, &args->type);
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
    gwy_container_set_enum_by_name(container, type_key, args->type);
    gwy_container_set_double_by_name(container, threshold_high_key,
                                     args->threshold_high);
    gwy_container_set_double_by_name(container, threshold_low_key,
                                     args->threshold_low);
    gwy_container_set_int32_by_name(container, min_len_key, args->min_len);
    gwy_container_set_int32_by_name(container, max_width_key, args->max_width);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
