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

#include "config.h"
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/correlation.h>
#include <libgwydgets/gwydgets.h>
#include <libgwymodule/gwymodule.h>
#include <app/gwyapp.h>

#define CROSSCOR_RUN_MODES GWY_RUN_INTERACTIVE

typedef enum {
    GWY_CROSSCOR_ABS,
    GWY_CROSSCOR_X,
    GWY_CROSSCOR_Y,
    GWY_CROSSCOR_DIR,
    GWY_CROSSCOR_ANG,
    GWY_CROSSCOR_SCORE,
    GWY_CROSSCOR_LAST
} CrosscorResult;

typedef struct {
    CrosscorResult result;
    gint search_x;
    gint search_y;
    gint window_x;
    gint window_y;
    gdouble rot_pos;
    gdouble rot_neg;
    GwyDataWindow *win1;
    GwyDataWindow *win2;
    gboolean add_ls_mask;
    gdouble threshold;
} CrosscorArgs;

typedef struct {
    GtkWidget *dialog;
    GtkWidget *result;
    GtkObject *search_area_x;
    GtkObject *search_area_y;
    GtkObject *window_area_x;
    GtkObject *window_area_y;
    GtkObject *rotation_neg;
    GtkObject *rotation_pos;
    GtkWidget *add_ls_mask;
    GtkObject *threshold;
} CrosscorControls;

static gboolean   module_register             (void);
static void       crosscor                    (GwyContainer *data,
                                               GwyRunType run);
static GtkWidget* crosscor_window_construct   (CrosscorArgs *args,
                                               CrosscorControls *controls);
static GtkWidget* crosscor_data_option_menu   (GwyDataWindow **operand);
static void       crosscor_operation_cb       (GtkWidget *combo,
                                               CrosscorArgs *args);
static void       crosscor_data_cb            (GtkWidget *item);
static void       crosscor_update_values      (CrosscorControls *controls,
                                               CrosscorArgs *args);
static gboolean   crosscor_check              (CrosscorArgs *args,
                                               GtkWidget *crosscor_window);
static gboolean   crosscor_do                 (CrosscorArgs *args);
static void       crosscor_load_args          (GwyContainer *settings,
                                               CrosscorArgs *args);
static void       crosscor_save_args          (GwyContainer *settings,
                                               CrosscorArgs *args);
static void       crosscor_sanitize_args      (CrosscorArgs *args);
static void       mask_changed_cb             (GtkToggleButton *button,
                                               CrosscorArgs *args);

CrosscorControls *pcontrols;

static const GwyEnum results[] = {
    { N_("Absolute"),   GWY_CROSSCOR_ABS, },
    { N_("X Distance"), GWY_CROSSCOR_X,   },
    { N_("Y Distance"), GWY_CROSSCOR_Y,   },
    { N_("Angle"),      GWY_CROSSCOR_DIR, },
};

static const CrosscorArgs crosscor_defaults = {
    GWY_CROSSCOR_ABS, 10, 10, 25, 25, 0.0, 0.0, NULL, NULL, 1, 0.95
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Calculates cross-correlation of two data fields."),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.3",
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

/* FIXME: we ignore the Container argument and use current data window */
static void
crosscor(GwyContainer *data, GwyRunType run)
{
    GtkWidget *crosscor_window;
    CrosscorArgs args;
    CrosscorControls controls;
    GwyContainer *settings;
    gboolean ok = FALSE;

    g_return_if_fail(run & CROSSCOR_RUN_MODES);
    settings = gwy_app_settings_get();
    crosscor_load_args(settings, &args);
    args.win1 = args.win2 = gwy_app_data_window_get_current();
    g_assert(gwy_data_window_get_data(args.win1) == data);
    pcontrols = &controls;
    crosscor_window = crosscor_window_construct(&args, &controls);
    gtk_window_present(GTK_WINDOW(crosscor_window));

    do {
        switch (gtk_dialog_run(GTK_DIALOG(crosscor_window))) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            crosscor_update_values(&controls, &args);
            crosscor_save_args(settings, &args);
            case GTK_RESPONSE_NONE:
            gtk_widget_destroy(crosscor_window);
            ok = TRUE;
            break;

            case GTK_RESPONSE_OK:
            crosscor_update_values(&controls, &args);
            ok = crosscor_check(&args, crosscor_window);
            if (ok) {
                gtk_widget_destroy(crosscor_window);
                crosscor_do(&args);
                crosscor_save_args(settings, &args);
            }
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (!ok);
}

static GtkWidget*
crosscor_window_construct(CrosscorArgs *args,
                          CrosscorControls *controls)
{
    GtkWidget *dialog, *table, *omenu, *label, *spin, *combo;
    gint row;

    dialog = gtk_dialog_new_with_buttons(_("Cross-Correlation"), NULL, 0,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    table = gtk_table_new(10, 4, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, TRUE, TRUE, 4);
    row = 0;

    /***** First operand *****/
    omenu = crosscor_data_option_menu(&args->win1);
    gwy_table_attach_hscale(table, row, _("_First operand:"), NULL,
                            GTK_OBJECT(omenu), GWY_HSCALE_WIDGET);
    row++;

    /***** Second operand *****/
    omenu = crosscor_data_option_menu(&args->win2);
    gwy_table_attach_hscale(table, row, _("_Second operand:"), NULL,
                            GTK_OBJECT(omenu), GWY_HSCALE_WIDGET);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    /**** Parameters ********/
    /*search size*/
    label = gtk_label_new(_("Search size"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 4, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    controls->search_area_x = gtk_adjustment_new(args->search_x,
                                                 0.0, 100.0, 1, 5, 0);
    gwy_table_attach_hscale(table, row, _("_Width:"), "px",
                            controls->search_area_x, 0);
    row++;

    controls->search_area_y = gtk_adjustment_new(args->search_y,
                                                 0.0, 100.0, 1, 5, 0);
    gwy_table_attach_hscale(table, row, _("_Height:"), "px",
                            controls->search_area_y, 0);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    /*window size*/
    label = gtk_label_new(_("Window size"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 4, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    controls->window_area_x = gtk_adjustment_new(args->window_x,
                                                 0.0, 100.0, 1, 5, 0);
    gwy_table_attach_hscale(table, row, _("_Width:"), "px",
                            controls->window_area_x, 0);
    row++;

    controls->window_area_y = gtk_adjustment_new(args->window_y,
                                                 0.0, 100.0, 1, 5, 0);
    gwy_table_attach_hscale(table, row, _("_Height:"), "px",
                            controls->window_area_y, 0);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    /*Result*/
    combo = gwy_enum_combo_box_new(results, G_N_ELEMENTS(results),
                                   G_CALLBACK(crosscor_operation_cb), args,
                                   args->result, TRUE);
    gwy_table_attach_hscale(table, row, _("_Output type:"), NULL,
                            GTK_OBJECT(combo), GWY_HSCALE_WIDGET);
    row++;

    /*do mask of thresholds*/
    controls->add_ls_mask = gtk_check_button_new_with_mnemonic
                                (_("Add _low score results mask"));
    gtk_table_attach(GTK_TABLE(table), controls->add_ls_mask, 0, 4, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->add_ls_mask),
                                 args->add_ls_mask);
    g_signal_connect(controls->add_ls_mask, "toggled",
                     G_CALLBACK(mask_changed_cb), args);
    row++;

    controls->threshold = gtk_adjustment_new(args->threshold,
                                             -1, 1, 0.005, 0.05, 0);
    spin = gwy_table_attach_hscale(table, row, _("_Threshold:"), NULL,
                                   controls->threshold, 0);
    gwy_table_hscale_set_sensitive(controls->threshold, args->add_ls_mask);

    gtk_widget_show_all(dialog);

    return dialog;
}

static GtkWidget*
crosscor_data_option_menu(GwyDataWindow **operand)
{
    GtkWidget *omenu, *menu;

    omenu = gwy_option_menu_data_window(G_CALLBACK(crosscor_data_cb),
                                        NULL, NULL, GTK_WIDGET(*operand));
    menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(omenu));
    g_object_set_data(G_OBJECT(menu), "operand", operand);

    return omenu;
}

static void
crosscor_operation_cb(GtkWidget *combo,
                      CrosscorArgs *args)
{
    args->result = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(combo));
}

static void
mask_changed_cb(GtkToggleButton *button, CrosscorArgs *args)
{
    args->add_ls_mask = gtk_toggle_button_get_active(button);
    gwy_table_hscale_set_sensitive(pcontrols->threshold, args->add_ls_mask);
}


static void
crosscor_data_cb(GtkWidget *item)
{
    GtkWidget *menu;
    gpointer p, *pp;

    menu = gtk_widget_get_parent(item);

    p = g_object_get_data(G_OBJECT(item), "data-window");
    pp = (gpointer*)g_object_get_data(G_OBJECT(menu), "operand");
    g_return_if_fail(pp);
    *pp = p;
}

static void
abs_field(GwyDataField *dfieldx, GwyDataField *dfieldy)
{
    gint i;

    for (i = 0; i < (dfieldx->xres * dfieldx->yres); i++)
        dfieldx->data[i] = hypot(dfieldx->data[i], dfieldy->data[i]);
}

static void
dir_field(GwyDataField *dfieldx, GwyDataField *dfieldy)
{
    gint i;

    for (i = 0; i < (dfieldx->xres * dfieldx->yres); i++) {
        dfieldx->data[i] = atan2(dfieldy->data[i], dfieldx->data[i]);
    }
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
crosscor_check(CrosscorArgs *args,
               GtkWidget *crosscor_window)
{
    GtkWidget *dialog;
    GwyContainer *data;
    GwyDataField *dfield1, *dfield2;
    GwyDataWindow *operand1, *operand2;

    operand1 = args->win1;
    operand2 = args->win2;
    g_return_val_if_fail(GWY_IS_DATA_WINDOW(operand1)
                         && GWY_IS_DATA_WINDOW(operand2),
                         FALSE);

    data = gwy_data_window_get_data(operand1);
    dfield1 = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    data = gwy_data_window_get_data(operand2);
    dfield2 = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));

    if (dfield1->xres == dfield2->xres && dfield1->yres == dfield2->yres)
        return TRUE;

    dialog = gtk_message_dialog_new(GTK_WINDOW(crosscor_window),
                                    GTK_DIALOG_DESTROY_WITH_PARENT,
                                    GTK_MESSAGE_INFO,
                                    GTK_BUTTONS_CLOSE,
                                    _("Both data fields must have same size."));
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    return FALSE;
}

static gboolean
crosscor_do(CrosscorArgs *args)
{
    GtkWidget *data_window;
    GwyContainer *data;
    GwyDataField *dfieldx, *dfieldy, *dfield1, *dfield2, *score;
    GwyDataWindow *operand1, *operand2;
    gint iteration = 0, newid;
    GwyComputationStateType state;

    operand1 = args->win1;
    operand2 = args->win2;
    g_return_val_if_fail(operand1 != NULL && operand2 != NULL, FALSE);

    data = gwy_data_window_get_data(operand2);
    dfield2 = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    data = gwy_data_window_get_data(operand1);
    dfield1 = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));

    /*result fields - after computation result should be at dfieldx */
    dfieldx = gwy_data_field_duplicate(
                   GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data")));
    dfieldy = gwy_data_field_duplicate(dfieldx);
    score = gwy_data_field_duplicate(dfieldx);

    /*compute crosscorelation */

    iteration = 0;
    state = GWY_COMPUTATION_STATE_INIT;
    gwy_app_wait_start(GTK_WIDGET(args->win1), "Initializing...");
    do {
        gwy_data_field_crosscorrelate_iteration(dfield1, dfield2, dfieldx,
                                                dfieldy, score, args->search_x,
                                                args->search_y, args->window_x,
                                                args->window_y, &state,
                                                &iteration);
        gwy_app_wait_set_message("Correlating...");
        if (!gwy_app_wait_set_fraction
                (iteration/(gdouble)(dfield1->xres - (args->search_x)/2)))
        {
            g_object_unref(dfieldx);
            g_object_unref(dfieldy);
            g_object_unref(score);
            return FALSE;
        }

    } while (state != GWY_COMPUTATION_STATE_FINISHED);
    gwy_app_wait_finish();

    switch (args->result) {
        case GWY_CROSSCOR_ABS:
        abs_field(dfieldx, dfieldy);
        break;

        case GWY_CROSSCOR_X:
        break;

        case GWY_CROSSCOR_Y:
        gwy_data_field_copy(dfieldy, dfieldx, FALSE);
        break;

        case GWY_CROSSCOR_DIR:
        dir_field(dfieldx, dfieldy);
        break;

        default:
        g_assert_not_reached();
        break;
    }

    /*create score mask if requested */
    if (args->add_ls_mask) {
        gwy_data_field_threshold(score, args->threshold, 1, 0);
        gwy_container_set_object_by_name(data, "/0/mask", score);
    }


    gwy_app_data_browser_get_current(GWY_APP_CONTAINER, &data, 0);
    newid = gwy_app_data_browser_add_data_field(dfieldx, data, TRUE);
    g_object_unref(dfieldx);
    gwy_app_copy_data_items(data, data, 0, newid, GWY_DATA_ITEM_GRADIENT, 0);

    switch (args->result) {
        case GWY_CROSSCOR_ABS:
        gwy_app_set_data_field_title(data, newid, _("Absolute difference"));
        break;

        case GWY_CROSSCOR_X:
        gwy_app_set_data_field_title(data, newid, _("X difference"));
        break;

        case GWY_CROSSCOR_Y:
        gwy_app_set_data_field_title(data, newid, _("Y difference"));
        break;

        case GWY_CROSSCOR_DIR:
        gwy_app_set_data_field_title(data, newid, _("Direction difference"));
        break;

        default:
        g_assert_not_reached();
        break;
    }


    g_object_unref(score);
    g_object_unref(dfieldy);

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

