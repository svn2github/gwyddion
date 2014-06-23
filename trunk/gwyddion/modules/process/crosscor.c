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
#include <libprocess/arithmetic.h>
#include <libprocess/correlation.h>
#include <libprocess/filters.h>
#include <libgwymodule/gwymodule-process.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwycombobox.h>
#include <app/gwyapp.h>

#define CROSSCOR_RUN_MODES GWY_RUN_INTERACTIVE

typedef enum {
    GWY_CROSSCOR_ALL,
    GWY_CROSSCOR_ABS,
    GWY_CROSSCOR_X,
    GWY_CROSSCOR_Y,
    GWY_CROSSCOR_DIR,
    GWY_CROSSCOR_SCORE,
    GWY_CROSSCOR_LAST
} CrosscorResult;

typedef struct {
    GwyContainer *data;
    gint id;
} GwyDataObjectId;

typedef struct {
    CrosscorResult result;
    gint search_x;
    gint search_y;
    gint window_x;
    gint window_y;
    gdouble rot_pos;
    gdouble rot_neg;
    gboolean add_ls_mask;
    gdouble threshold;
    gboolean multiple;
    GwyDataObjectId op1;
    GwyDataObjectId op2;
    GwyDataObjectId op3;
    GwyDataObjectId op4;

} CrosscorArgs;

typedef struct {
    CrosscorArgs *args;
    GtkWidget *result;
    GtkObject *search_area_x;
    GtkObject *search_area_y;
    GtkObject *window_area_x;
    GtkObject *window_area_y;
    GtkObject *rotation_neg;
    GtkObject *rotation_pos;
    GtkWidget *add_ls_mask;
    GtkObject *threshold;
    GtkWidget *multiple;
    GtkWidget *chooser_op3;
    GtkWidget *chooser_op4;
} CrosscorControls;

static gboolean module_register       (void);
static void     crosscor              (GwyContainer *data,
                                       GwyRunType run);
static gboolean crosscor_dialog       (CrosscorArgs *args);
static void     crosscor_operation_cb (GtkWidget *combo,
                                       CrosscorArgs *args);
static void     crosscor_data_cb      (GwyDataChooser *chooser,
                                       GwyDataObjectId *object);
static gboolean crosscor_data_filter  (GwyContainer *data,
                                       gint id,
                                       gpointer user_data);
static gboolean crosscor_weaker_filter  (GwyContainer *data,
                                       gint id,
                                       gpointer user_data);
static void     crosscor_update_values(CrosscorControls *controls,
                                       CrosscorArgs *args);
static gboolean crosscor_do           (CrosscorArgs *args);
static void     crosscor_load_args    (GwyContainer *settings,
                                       CrosscorArgs *args);
static void     crosscor_save_args    (GwyContainer *settings,
                                       CrosscorArgs *args);
static void     crosscor_sanitize_args(CrosscorArgs *args);
static void     mask_changed_cb       (GtkToggleButton *button,
                                       CrosscorControls *controls);
static void     multiple_changed_cb   (GtkToggleButton *button,
                                       CrosscorControls *controls);
static void     crosscor_update_areas_cb(GtkObject *adj,
                                       CrosscorControls *controls);


static const CrosscorArgs crosscor_defaults = {
    GWY_CROSSCOR_ABS, 10, 10, 25, 25, 0.0, 0.0, 1, 0.95, 0,
    { NULL, -1 }, { NULL, -1 }, { NULL, -1 }, { NULL, -1 },
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Calculates cross-correlation of two data fields."),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.6",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("crosscor",
                              (GwyProcessFunc)&crosscor,
                              N_("/M_ultidata/_Cross-Correlation..."),
                              NULL,
                              CROSSCOR_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Cross-correlate two data fields"));

    return TRUE;
}

static void
crosscor(GwyContainer *data, GwyRunType run)
{
    CrosscorArgs args;
    GwyContainer *settings;
    gboolean dorun;

    g_return_if_fail(run & CROSSCOR_RUN_MODES);

    settings = gwy_app_settings_get();
    crosscor_load_args(settings, &args);

    args.op1.data = data;
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD_ID, &args.op1.id, 0);
    args.op2.data = NULL;

    dorun = crosscor_dialog(&args);
    crosscor_save_args(settings, &args);

    if (dorun)
        crosscor_do(&args);
}

static gboolean
crosscor_dialog(CrosscorArgs *args)
{
    CrosscorControls controls;
    GtkWidget *dialog, *table, *chooser, *label, *combo;
    gint row, response;
    gboolean ok = FALSE;

    controls.args = args;

    dialog = gtk_dialog_new_with_buttons(_("Cross-Correlation"), NULL, 0,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    table = gtk_table_new(9, 4, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, TRUE, TRUE, 4);
    row = 0;

    /* Correlate with */
    chooser = gwy_data_chooser_new_channels();
    g_object_set_data(G_OBJECT(chooser), "dialog", dialog);
    gwy_data_chooser_set_filter(GWY_DATA_CHOOSER(chooser),
                                crosscor_data_filter, &args->op1, NULL);
    g_signal_connect(chooser, "changed",
                     G_CALLBACK(crosscor_data_cb), &args->op2);
    crosscor_data_cb(GWY_DATA_CHOOSER(chooser), &args->op2);
    gwy_table_attach_hscale(table, row, _("Co_rrelate with:"), NULL,
                            GTK_OBJECT(chooser), GWY_HSCALE_WIDGET);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    /* Search size */
    label = gtk_label_new(_("Search size"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 4, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    controls.search_area_x = gtk_adjustment_new(args->search_x,
                                                0.0, 100.0, 1, 5, 0);
    gwy_table_attach_hscale(table, row, _("_Width:"), "px",
                            controls.search_area_x, 0);

    g_signal_connect(controls.search_area_x, "value-changed",
                     G_CALLBACK(crosscor_update_areas_cb),
                     &controls);

    row++;

    controls.search_area_y = gtk_adjustment_new(args->search_y,
                                                0.0, 100.0, 1, 5, 0);
    gwy_table_attach_hscale(table, row, _("H_eight:"), "px",
                            controls.search_area_y, 0);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    g_signal_connect(controls.search_area_y, "value-changed",
                     G_CALLBACK(crosscor_update_areas_cb),
                     &controls);

    row++;

    /* Window size */
    label = gtk_label_new(_("Window size"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 4, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    controls.window_area_x = gtk_adjustment_new(args->window_x,
                                                0.0, 100.0, 1, 5, 0);
    gwy_table_attach_hscale(table, row, _("W_idth:"), "px",
                            controls.window_area_x, 0);
    g_signal_connect(controls.window_area_x, "value-changed",
                     G_CALLBACK(crosscor_update_areas_cb),
                     &controls);

    row++;

    controls.window_area_y = gtk_adjustment_new(args->window_y,
                                                0.0, 100.0, 1, 5, 0);
    gwy_table_attach_hscale(table, row, _("Hei_ght:"), "px",
                            controls.window_area_y, 0);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    g_signal_connect(controls.window_area_y, "value-changed",
                     G_CALLBACK(crosscor_update_areas_cb),
                     &controls);
    row++;

    /* Result */
    combo = gwy_enum_combo_box_newl(G_CALLBACK(crosscor_operation_cb), args,
                                    args->result,
                                    _("All"), GWY_CROSSCOR_ALL,
                                    _("Absolute"), GWY_CROSSCOR_ABS,
                                    _("X Distance"), GWY_CROSSCOR_X,
                                    _("Y Distance"), GWY_CROSSCOR_Y,
                                    _("Angle"), GWY_CROSSCOR_DIR,
                                    _("Score"), GWY_CROSSCOR_SCORE,
                                    NULL);
    gwy_table_attach_hscale(table, row, _("Output _type:"), NULL,
                            GTK_OBJECT(combo), GWY_HSCALE_WIDGET);
    row++;

    /* Do mask of thresholds */
    controls.add_ls_mask = gtk_check_button_new_with_mnemonic
                                           (_("Add _low score results mask"));
    gtk_table_attach(GTK_TABLE(table), controls.add_ls_mask, 0, 4, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.add_ls_mask),
                                 args->add_ls_mask);
    g_signal_connect(controls.add_ls_mask, "toggled",
                     G_CALLBACK(mask_changed_cb), &controls);
    row++;

    controls.threshold = gtk_adjustment_new(args->threshold,
                                            -1, 1, 0.005, 0.05, 0);
    gwy_table_attach_hscale(table, row, _("T_hreshold:"), NULL,
                            controls.threshold, 0);
    gwy_table_hscale_set_sensitive(controls.threshold, args->add_ls_mask);
    row++;

    /* Allow multiple channel cross-correlation */
    controls.multiple = gtk_check_button_new_with_mnemonic
                                           (_("Multichannel cross-corelation"));
    gtk_table_attach(GTK_TABLE(table), controls.multiple, 0, 4, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.multiple),
                                 args->multiple);
    g_signal_connect(controls.multiple, "toggled",
                     G_CALLBACK(multiple_changed_cb), &controls);
    row++;

    /* Second set to correlate with: source */
    controls.chooser_op3 = gwy_data_chooser_new_channels();
    g_object_set_data(G_OBJECT(controls.chooser_op3), "dialog", dialog);
    gwy_data_chooser_set_filter(GWY_DATA_CHOOSER(controls.chooser_op3),
                                crosscor_weaker_filter, &args->op1, NULL);
    g_signal_connect(controls.chooser_op3, "changed",
                     G_CALLBACK(crosscor_data_cb), &args->op3);
    crosscor_data_cb(GWY_DATA_CHOOSER(controls.chooser_op3), &args->op3);
    gwy_table_attach_hscale(table, row, _("Second _source data:"), NULL,
                            GTK_OBJECT(controls.chooser_op3), GWY_HSCALE_WIDGET);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    gtk_widget_set_sensitive(controls.chooser_op3, args->multiple);

    row++;

    /* Second set to correlate with: second data */
    controls.chooser_op4 = gwy_data_chooser_new_channels();
    g_object_set_data(G_OBJECT(controls.chooser_op4), "dialog", dialog);
    gwy_data_chooser_set_filter(GWY_DATA_CHOOSER(controls.chooser_op4),
                                crosscor_weaker_filter, &args->op1, NULL);
    g_signal_connect(controls.chooser_op4, "changed",
                     G_CALLBACK(crosscor_data_cb), &args->op4);
    crosscor_data_cb(GWY_DATA_CHOOSER(controls.chooser_op4), &args->op4);
    gwy_table_attach_hscale(table, row, _("Correlate with:"), NULL,
                            GTK_OBJECT(controls.chooser_op4), GWY_HSCALE_WIDGET);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    gtk_widget_set_sensitive(controls.chooser_op4, args->multiple);
    row++;



    gtk_widget_show_all(dialog);

    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            crosscor_update_values(&controls, args);
            case GTK_RESPONSE_NONE:
            gtk_widget_destroy(dialog);
            return FALSE;
            break;

            case GTK_RESPONSE_OK:
            crosscor_update_values(&controls, args);
            ok = TRUE;
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (!ok);

    gtk_widget_destroy(dialog);

    return TRUE;
}

static void
crosscor_data_cb(GwyDataChooser *chooser,
                 GwyDataObjectId *object)
{
    GtkWidget *dialog;

    object->data = gwy_data_chooser_get_active(chooser, &object->id);
    gwy_debug("data: %p %d", object->data, object->id);

    dialog = g_object_get_data(G_OBJECT(chooser), "dialog");
    g_assert(GTK_IS_DIALOG(dialog));
    gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog), GTK_RESPONSE_OK,
                                      object->data != NULL);
}

static void
crosscor_operation_cb(GtkWidget *combo,
                      CrosscorArgs *args)
{
    args->result = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(combo));
}

static void
mask_changed_cb(GtkToggleButton *button, CrosscorControls *controls)
{
    controls->args->add_ls_mask = gtk_toggle_button_get_active(button);
    gwy_table_hscale_set_sensitive(controls->threshold,
                                   controls->args->add_ls_mask);
}
static void
multiple_changed_cb(GtkToggleButton *button, CrosscorControls *controls)
{
    controls->args->multiple = gtk_toggle_button_get_active(button);
    gtk_widget_set_sensitive(controls->chooser_op3,
                                   controls->args->multiple);
    gtk_widget_set_sensitive(controls->chooser_op4,
                                   controls->args->multiple);
}

static gboolean
crosscor_data_filter(GwyContainer *data,
                     gint id,
                     gpointer user_data)
{
    GwyDataObjectId *object = (GwyDataObjectId*)user_data;
    GwyDataField *op1, *op2;
    GQuark quark;

    quark = gwy_app_get_data_key_for_id(id);
    op1 = GWY_DATA_FIELD(gwy_container_get_object(data, quark));

    quark = gwy_app_get_data_key_for_id(object->id);
    op2 = GWY_DATA_FIELD(gwy_container_get_object(object->data, quark));

    /* It does not make sense to crosscorrelate with itself */
    if (op1 == op2)
        return FALSE;

    return !gwy_data_field_check_compatibility(op1, op2,
                                               GWY_DATA_COMPATIBILITY_RES
                                               | GWY_DATA_COMPATIBILITY_REAL
                                               | GWY_DATA_COMPATIBILITY_LATERAL
                                               | GWY_DATA_COMPATIBILITY_VALUE);
}


static gboolean
crosscor_weaker_filter(GwyContainer *data,
                     gint id,
                     gpointer user_data)
{
    GwyDataObjectId *object = (GwyDataObjectId*)user_data;
    GwyDataField *op1, *op2;
    GQuark quark;

    quark = gwy_app_get_data_key_for_id(id);
    op1 = GWY_DATA_FIELD(gwy_container_get_object(data, quark));

    quark = gwy_app_get_data_key_for_id(object->id);
    op2 = GWY_DATA_FIELD(gwy_container_get_object(object->data, quark));

    return !gwy_data_field_check_compatibility(op1, op2,
                                               GWY_DATA_COMPATIBILITY_RES
                                               | GWY_DATA_COMPATIBILITY_REAL
                                               | GWY_DATA_COMPATIBILITY_LATERAL);
}

static GwyDataField*
abs_field(GwyDataField *dfieldx, GwyDataField *dfieldy)
{
    gdouble *data, *rdata;
    const gdouble *d2;
    gint i, n;
    GwyDataField *result = gwy_data_field_new_alike(dfieldx, TRUE);


    data = gwy_data_field_get_data(dfieldx);
    d2 = gwy_data_field_get_data_const(dfieldy);
    rdata = gwy_data_field_get_data(result);
    n = gwy_data_field_get_xres(dfieldx)*gwy_data_field_get_yres(dfieldx);

    for (i = 0; i < n; i++)
        rdata[i] = hypot(data[i], d2[i]);

    return result;
}

static GwyDataField*
dir_field(GwyDataField *dfieldx, GwyDataField *dfieldy)
{
    gdouble *data, *rdata;
    const gdouble *d2;
    gint i, n;
    GwyDataField *result = gwy_data_field_new_alike(dfieldx, TRUE);

    data = gwy_data_field_get_data(dfieldx);
    rdata = gwy_data_field_get_data(result);
    d2 = gwy_data_field_get_data_const(dfieldy);
    n = gwy_data_field_get_xres(dfieldx)*gwy_data_field_get_yres(dfieldx);

    for (i = 0; i < n; i++)
        rdata[i] = atan2(d2[i], data[i]);

    return result;
}

static void
crosscor_update_areas_cb(G_GNUC_UNUSED GtkObject *adj,
                         CrosscorControls *controls)
{
    static gboolean in_update = FALSE;
    if (in_update)  return;

    in_update = TRUE;

    controls->args->search_x = gwy_adjustment_get_int(controls->search_area_x);
    controls->args->search_y = gwy_adjustment_get_int(controls->search_area_y);

    controls->args->window_x = gwy_adjustment_get_int(controls->window_area_x);
    controls->args->window_y = gwy_adjustment_get_int(controls->window_area_y);

    if (controls->args->search_x<controls->args->window_x) {
        controls->args->search_x = controls->args->window_x;
        gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->search_area_x),
                                 controls->args->search_x);
    }
    if (controls->args->search_y<controls->args->window_y) {
        controls->args->search_y = controls->args->window_y;
        gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->search_area_y),
                                 controls->args->search_y);
    }

    in_update = FALSE;
}

static void
crosscor_update_values(CrosscorControls *controls,
                       CrosscorArgs *args)
{
    args->search_x = gwy_adjustment_get_int(controls->search_area_x);
    args->search_y = gwy_adjustment_get_int(controls->search_area_y);

    args->window_x = gwy_adjustment_get_int(controls->window_area_x);
    args->window_y = gwy_adjustment_get_int(controls->window_area_y);

    args->threshold =
        gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->threshold));
    args->add_ls_mask =
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->add_ls_mask));
}

static gboolean
crosscor_do(CrosscorArgs * args)
{
    GwyContainer *data;
    GwyDataField *dfieldx, *dfieldy, *dfield1, *dfield2,
                 *dfield3 = NULL, *dfield4 = NULL, *score, *dir = NULL,
                 *abs = NULL;
    GwyDataField *dfieldx2 = NULL, *dfieldy2 = NULL, *score2 = NULL;
    gint newid;
    GwyComputationState *state;
    GwySIUnit *siunit;
    GQuark quark;

    quark = gwy_app_get_data_key_for_id(args->op1.id);
    dfield1 = GWY_DATA_FIELD(gwy_container_get_object(args->op1.data, quark));

    quark = gwy_app_get_data_key_for_id(args->op2.id);
    dfield2 = GWY_DATA_FIELD(gwy_container_get_object(args->op2.data, quark));

    /* result fields - after computation result should be at dfieldx */
    dfieldx = gwy_data_field_new_alike(dfield1, FALSE);
    dfieldy = gwy_data_field_new_alike(dfield1, FALSE);
    score = gwy_data_field_new_alike(dfield1, FALSE);

    if (args->multiple) {
        quark = gwy_app_get_data_key_for_id(args->op3.id);
        dfield3 =
            GWY_DATA_FIELD(gwy_container_get_object(args->op3.data, quark));

        quark = gwy_app_get_data_key_for_id(args->op4.id);
        dfield4 =
            GWY_DATA_FIELD(gwy_container_get_object(args->op4.data, quark));

        dfieldx2 = gwy_data_field_new_alike(dfield1, FALSE);
        dfieldy2 = gwy_data_field_new_alike(dfield1, FALSE);
        score2 = gwy_data_field_new_alike(dfield1, FALSE);
    }

    /* FIXME */
    gwy_app_wait_start(gwy_app_find_window_for_channel(args->op1.data,
                                                       args->op1.id),
                       _("Initializing..."));

    /* compute crosscorelation */
    state = gwy_data_field_crosscorrelate_init(dfield1, dfield2,
                                               dfieldx, dfieldy, score,
                                               args->search_x, args->search_y,
                                               args->window_x, args->window_y);
    gwy_app_wait_set_message(_("Correlating first set..."));
    do {
        gwy_data_field_crosscorrelate_iteration(state);
        if (!gwy_app_wait_set_fraction(state->fraction)) {
            gwy_data_field_crosscorrelate_finalize(state);
            gwy_app_wait_finish();
            g_object_unref(dfieldx);
            g_object_unref(dfieldy);
            g_object_unref(score);
            return FALSE;
        }
    } while (state->state != GWY_COMPUTATION_STATE_FINISHED);
    gwy_data_field_crosscorrelate_finalize(state);
    gwy_app_wait_finish();

    /* compute crosscorelation of second set if it is there */
    if (args->multiple) {
        gwy_app_wait_start(gwy_app_find_window_for_channel(args->op1.data,
                                                           args->op1.id),
                           _("Initializing..."));

        state = gwy_data_field_crosscorrelate_init(dfield3, dfield4,
                                                   dfieldx2, dfieldy2, score2,
                                                   args->search_x,
                                                   args->search_y,
                                                   args->window_x,
                                                   args->window_y);
        gwy_app_wait_set_message(_("Correlating second set..."));
        do {
            gwy_data_field_crosscorrelate_iteration(state);
            if (!gwy_app_wait_set_fraction(state->fraction)) {
                gwy_data_field_crosscorrelate_finalize(state);
                gwy_app_wait_finish();
                g_object_unref(dfieldx2);
                g_object_unref(dfieldy2);
                g_object_unref(score2);
                return FALSE;
            }
        } while (state->state != GWY_COMPUTATION_STATE_FINISHED);
        gwy_data_field_crosscorrelate_finalize(state);
        gwy_app_wait_finish();

        gwy_data_field_sum_fields(dfieldx, dfieldx, dfieldx2);
        gwy_data_field_sum_fields(dfieldy, dfieldy, dfieldy2);
        gwy_data_field_sum_fields(score, score, score2);

        gwy_data_field_multiply(dfieldx, 0.5);
        gwy_data_field_multiply(dfieldy, 0.5);
        gwy_data_field_multiply(score, 0.5);
    }

    if (args->result == GWY_CROSSCOR_ALL || args->result == GWY_CROSSCOR_ABS)
        abs = abs_field(dfieldx, dfieldy);

    if (args->result == GWY_CROSSCOR_ALL || args->result == GWY_CROSSCOR_DIR)
        dir = dir_field(dfieldx, dfieldy);

    if (args->result == GWY_CROSSCOR_ALL || args->result == GWY_CROSSCOR_X) {
        siunit = gwy_data_field_get_si_unit_z(dfieldx);
        gwy_si_unit_set_from_string(siunit, NULL);

        data = args->op1.data;
        newid = gwy_app_data_browser_add_data_field(dfieldx, data, TRUE);
        gwy_app_sync_data_items(data, data, args->op1.id, newid, FALSE,
                                GWY_DATA_ITEM_GRADIENT, 0);
        gwy_app_channel_log_add(data, args->op1.id, newid, "proc::crosscor",
                                NULL);

        /* create score mask if requested */
        if (args->add_ls_mask) {
            quark = gwy_app_get_mask_key_for_id(newid);
            gwy_data_field_threshold(score, args->threshold, 1.0, 0.0);
            gwy_container_set_object(data, quark, score);
        }

        gwy_app_set_data_field_title(data, newid, _("X difference"));
    }

    if (args->result == GWY_CROSSCOR_ALL || args->result == GWY_CROSSCOR_Y) {
        siunit = gwy_data_field_get_si_unit_z(dfieldy);
        gwy_si_unit_set_from_string(siunit, NULL);

        data = args->op1.data;
        newid = gwy_app_data_browser_add_data_field(dfieldy, data, TRUE);
        gwy_app_sync_data_items(data, data, args->op1.id, newid, FALSE,
                                GWY_DATA_ITEM_GRADIENT, 0);
        gwy_app_channel_log_add(data, args->op1.id, newid, "proc::crosscor",
                                NULL);

        /* create score mask if requested */
        if (args->add_ls_mask) {
            quark = gwy_app_get_mask_key_for_id(newid);
            gwy_data_field_threshold(score, args->threshold, 1.0, 0.0);
            gwy_container_set_object(data, quark, score);
        }

        gwy_app_set_data_field_title(data, newid, _("Y difference"));
    }
    if (args->result == GWY_CROSSCOR_ALL || args->result == GWY_CROSSCOR_ABS) {
        siunit = gwy_data_field_get_si_unit_z(abs);
        gwy_si_unit_set_from_string(siunit, NULL);

        data = args->op1.data;
        newid = gwy_app_data_browser_add_data_field(abs, data, TRUE);
        gwy_app_sync_data_items(data, data, args->op1.id, newid, FALSE,
                                GWY_DATA_ITEM_GRADIENT, 0);
        gwy_app_channel_log_add(data, args->op1.id, newid, "proc::crosscor",
                                NULL);

        /* create score mask if requested */
        if (args->add_ls_mask) {
            quark = gwy_app_get_mask_key_for_id(newid);
            gwy_data_field_threshold(score, args->threshold, 1.0, 0.0);
            gwy_container_set_object(data, quark, score);
        }

        gwy_app_set_data_field_title(data, newid, _("Absolute difference"));
    }
    if (args->result == GWY_CROSSCOR_ALL || args->result == GWY_CROSSCOR_DIR) {
        siunit = gwy_data_field_get_si_unit_z(dir);
        gwy_si_unit_set_from_string(siunit, NULL);

        data = args->op1.data;
        newid = gwy_app_data_browser_add_data_field(dir, data, TRUE);
        gwy_app_sync_data_items(data, data, args->op1.id, newid, FALSE,
                                GWY_DATA_ITEM_GRADIENT, 0);
        gwy_app_channel_log_add(data, args->op1.id, newid, "proc::crosscor",
                                NULL);

        /* create score mask if requested */
        if (args->add_ls_mask) {
            quark = gwy_app_get_mask_key_for_id(newid);
            gwy_data_field_threshold(score, args->threshold, 1.0, 0.0);
            gwy_container_set_object(data, quark, score);
        }

        gwy_app_set_data_field_title(data, newid, _("Direction"));
    }
    if (args->result == GWY_CROSSCOR_ALL
        || args->result == GWY_CROSSCOR_SCORE) {
        siunit = gwy_data_field_get_si_unit_z(score);
        gwy_si_unit_set_from_string(siunit, NULL);

        data = args->op1.data;
        newid = gwy_app_data_browser_add_data_field(score, data, TRUE);
        gwy_app_sync_data_items(data, data, args->op1.id, newid, FALSE,
                                GWY_DATA_ITEM_GRADIENT, 0);
        gwy_app_channel_log_add(data, args->op1.id, newid, "proc::crosscor",
                                NULL);

        /* create score mask if requested */
        if (args->add_ls_mask) {
            quark = gwy_app_get_mask_key_for_id(newid);
            gwy_data_field_threshold(score, args->threshold, 1.0, 0.0);
            gwy_container_set_object(data, quark, score);
        }

        gwy_app_set_data_field_title(data, newid, _("Score"));
    }

    g_object_unref(score);
    g_object_unref(dfieldy);
    g_object_unref(dfieldx);
    if (abs)
        g_object_unref(abs);
    if (dir)
        g_object_unref(dir);

    return TRUE;
}

static const gchar result_key[]      = "/module/crosscor/result";
static const gchar search_x_key[]    = "/module/crosscor/search_x";
static const gchar search_y_key[]    = "/module/crosscor/search_y";
static const gchar window_x_key[]    = "/module/crosscor/window_x";
static const gchar window_y_key[]    = "/module/crosscor/window_y";
static const gchar add_ls_mask_key[] = "/module/crosscor/add_ls_mask";
static const gchar threshold_key[]   = "/module/crosscor/threshold";
static const gchar rot_pos_key[]     = "/module/crosscor/rot_pos";
static const gchar rot_neg_key[]     = "/module/crosscor/rot_neg";

static void
crosscor_sanitize_args(CrosscorArgs *args)
{
    args->result = MIN(args->result, GWY_CROSSCOR_LAST-1);
    args->search_x = CLAMP(args->search_x, 0, 100);
    args->search_y = CLAMP(args->search_y, 0, 100);
    args->window_x = CLAMP(args->window_x, 0, 100);
    args->window_y = CLAMP(args->window_y, 0, 100);
    args->threshold = CLAMP(args->threshold, -1.0, 1.0);
    args->add_ls_mask = !!args->add_ls_mask;
}

static void
crosscor_load_args(GwyContainer *settings,
                   CrosscorArgs *args)
{
    *args = crosscor_defaults;
    gwy_container_gis_enum_by_name(settings, result_key, &args->result);
    gwy_container_gis_int32_by_name(settings, search_x_key, &args->search_x);
    gwy_container_gis_int32_by_name(settings, search_y_key, &args->search_y);
    gwy_container_gis_int32_by_name(settings, window_x_key, &args->window_x);
    gwy_container_gis_int32_by_name(settings, window_y_key, &args->window_y);
    gwy_container_gis_double_by_name(settings, threshold_key, &args->threshold);
    gwy_container_gis_boolean_by_name(settings, add_ls_mask_key,
                                      &args->add_ls_mask);
    gwy_container_gis_double_by_name(settings, rot_pos_key, &args->rot_pos);
    gwy_container_gis_double_by_name(settings, rot_neg_key, &args->rot_neg);
    crosscor_sanitize_args(args);
}

static void
crosscor_save_args(GwyContainer *settings,
                   CrosscorArgs *args)
{
    gwy_container_set_enum_by_name(settings, result_key, args->result);
    gwy_container_set_int32_by_name(settings, search_x_key, args->search_x);
    gwy_container_set_int32_by_name(settings, search_y_key, args->search_y);
    gwy_container_set_int32_by_name(settings, window_x_key, args->window_x);
    gwy_container_set_int32_by_name(settings, window_y_key, args->window_y);
    gwy_container_set_double_by_name(settings, threshold_key, args->threshold);
    gwy_container_set_boolean_by_name(settings, add_ls_mask_key,
                                      args->add_ls_mask);
    gwy_container_set_double_by_name(settings, rot_pos_key, args->rot_pos);
    gwy_container_set_double_by_name(settings, rot_neg_key, args->rot_neg);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

