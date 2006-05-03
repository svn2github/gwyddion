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
#include <libprocess/correlation.h>
#include <libgwydgets/gwydgets.h>
#include <libgwymodule/gwymodule.h>
#include <app/gwyapp.h>

#define MASKCOR_RUN_MODES GWY_RUN_INTERACTIVE

typedef enum {
    GWY_MASKCOR_OBJECTS,
    GWY_MASKCOR_MAXIMA,
    GWY_MASKCOR_SCORE,
    GWY_MASKCOR_LAST
} MaskcorResult;

typedef struct {
    MaskcorResult result;
    gdouble threshold;
    GwyCorrelationType method;
    GwyDataWindow *win1;
    GwyDataWindow *win2;
} MaskcorArgs;

typedef struct {
    GtkObject *threshold;
    MaskcorArgs *args;
} MaskcorControls;

static gboolean   module_register          (void);
static void       maskcor                  (GwyContainer *data,
                                            GwyRunType run);
static GtkWidget* maskcor_window_construct (MaskcorArgs *args,
                                            MaskcorControls *controls);
static GtkWidget* maskcor_data_option_menu (GwyDataWindow **operand);
static void       maskcor_operation_cb     (GtkWidget *item,
                                            MaskcorControls *controls);
static void       maskcor_threshold_cb     (GtkAdjustment *adj,
                                            gdouble *value);
static void       maskcor_data_cb          (GtkWidget *item);
static gboolean   maskcor_do               (MaskcorArgs *args);
static void       maskcor_load_args        (GwyContainer *settings,
                                            MaskcorArgs *args);
static void       maskcor_save_args        (GwyContainer *settings,
                                            MaskcorArgs *args);
static void       maskcor_sanitize_args    (MaskcorArgs *args);

static const MaskcorArgs maskcor_defaults = {
    GWY_MASKCOR_OBJECTS, 0.95, GWY_CORRELATION_NORMAL, NULL, NULL
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Creates mask by correlation with another data."),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.4",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("maskcor",
                              (GwyProcessFunc)&maskcor,
                              N_("/M_ultidata/_Mask by Correlation..."),
                              NULL,
                              MASKCOR_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Create mask by correlation with another "
                                 "data"));

    return TRUE;
}

/* FIXME: we ignore the Container argument and use current data window */
static void
maskcor(GwyContainer *data, GwyRunType run)
{
    GtkWidget *maskcor_window;
    MaskcorArgs args;
    MaskcorControls controls;
    GwyContainer *settings;
    gboolean ok = FALSE;

    g_return_if_fail(run & MASKCOR_RUN_MODES);
    settings = gwy_app_settings_get();
    maskcor_load_args(settings, &args);
    args.win1 = args.win2 = gwy_app_data_window_get_current();
    g_assert(gwy_data_window_get_data(args.win1) == data);
    maskcor_window = maskcor_window_construct(&args, &controls);
    gtk_window_present(GTK_WINDOW(maskcor_window));

    do {
        switch (gtk_dialog_run(GTK_DIALOG(maskcor_window))) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            gtk_widget_destroy(maskcor_window);
            maskcor_save_args(settings, &args);
            case GTK_RESPONSE_NONE:
            ok = TRUE;
            break;

            case GTK_RESPONSE_OK:
            gtk_widget_destroy(maskcor_window);
            maskcor_do(&args);
            maskcor_save_args(settings, &args);
            ok = TRUE;
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (!ok);
}

static GtkWidget*
maskcor_window_construct(MaskcorArgs *args, MaskcorControls *controls)
{
    static const GwyEnum results[] = {
        { N_("Objects marked"),     GWY_MASKCOR_OBJECTS },
        { N_("Correlation maxima"), GWY_MASKCOR_MAXIMA },
        { N_("Correlation score"),  GWY_MASKCOR_SCORE },
    };
    GtkWidget *dialog, *table, *omenu, *spin, *combo, *method;
    GtkObject *adj;

    controls->args = args;

    dialog = gtk_dialog_new_with_buttons(_("Mask by Correlation"), NULL, 0,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    table = gtk_table_new(2, 5, FALSE);
    gtk_table_set_col_spacings(GTK_TABLE(table), 4);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, TRUE, TRUE, 4);

    /* Operands */
    omenu = maskcor_data_option_menu(&args->win1);
    gwy_table_attach_hscale(table, 0, _("_Data to modify:"), NULL,
                            GTK_OBJECT(omenu), GWY_HSCALE_WIDGET);

    omenu = maskcor_data_option_menu(&args->win2);
    gwy_table_attach_hscale(table, 1, _("_Correlation kernel:"), NULL,
                            GTK_OBJECT(omenu), GWY_HSCALE_WIDGET);

    /***** Result *****/
    combo = gwy_enum_combo_box_new(results, G_N_ELEMENTS(results),
                                   G_CALLBACK(maskcor_operation_cb), controls,
                                   args->result, TRUE);
    gwy_table_attach_row(table, 2, _("_Output type:"), "", combo);

    /**** Parameters ********/
    method = gwy_enum_combo_box_new(gwy_correlation_type_get_enum(), -1,
                                       G_CALLBACK(gwy_enum_combo_box_update_int),
                                              &args->method, args->method, TRUE);
    gwy_table_attach_row(table, 3, _("_Correlation method:"), "", method);

    adj = gtk_adjustment_new(args->threshold, -1.0, 1.0, 0.01, 0.1, 0);
    controls->threshold = adj;
    spin = gwy_table_attach_hscale(table, 4, _("_Threshold:"), NULL, adj, 0);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 3);
    gwy_table_hscale_set_sensitive(adj, args->result != GWY_MASKCOR_SCORE);
    g_signal_connect(adj, "value-changed",
                     G_CALLBACK(maskcor_threshold_cb),
                     &args->threshold);

    gtk_widget_show_all(dialog);

    return dialog;
}

GtkWidget*
maskcor_data_option_menu(GwyDataWindow **operand)
{
    GtkWidget *omenu, *menu;

    omenu = gwy_option_menu_data_window(G_CALLBACK(maskcor_data_cb),
                                        NULL, NULL, GTK_WIDGET(*operand));
    menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(omenu));
    g_object_set_data(G_OBJECT(menu), "operand", operand);

    return omenu;
}

static void
maskcor_operation_cb(GtkWidget *combo, MaskcorControls *controls)
{
    controls->args->result
        = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(combo));
    gwy_table_hscale_set_sensitive(controls->threshold,
                                   controls->args->result != GWY_MASKCOR_SCORE);
}

static void
maskcor_threshold_cb(GtkAdjustment *adj, gdouble *value)
{
    *value = gtk_adjustment_get_value(adj);
}

static void
maskcor_data_cb(GtkWidget *item)
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
plot_correlated(GwyDataField * retfield, gint xsize, gint ysize,
                gdouble threshold)
{
    GwyDataField *field;
    gint i, j;

    field = gwy_data_field_duplicate(retfield);
    gwy_data_field_clear(retfield);

    for (i = 0; i < retfield->xres; i++) {
        for (j = 0; j < retfield->yres; j++) {
            if ((field->data[i + retfield->xres * j]) > threshold)
                gwy_data_field_area_fill(retfield,
                                         i - xsize/2, j - ysize/2, xsize, ysize,
                                         1.0);
        }
    }

}

static void
plot_maxima(GwyDataField * retfield, gdouble threshold)
{
    gint i, j;

    for (i = 0; i < retfield->xres; i++) {
        for (j = 0; j < retfield->yres; j++) {
            if (retfield->data[i + retfield->xres * j] > threshold)
                retfield->data[i + retfield->xres * j] = 1;
            else
                retfield->data[i + retfield->xres * j] = 0;
        }
    }

}

static gboolean
maskcor_do(MaskcorArgs *args)
{
    GwyContainer *data, *ret, *kernel;
    GwyDataField *dfield, *kernelfield, *retfield, *scorefield;
    GwyDataWindow *operand1, *operand2;
    gint iteration = 0, newid;
    GwyComputationStateType state;

    operand1 = args->win1;
    operand2 = args->win2;
    g_return_val_if_fail(operand1 != NULL && operand2 != NULL, FALSE);

    data = gwy_data_window_get_data(operand1);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    retfield = gwy_data_field_duplicate(dfield);

    kernel = gwy_data_window_get_data(operand2);
    kernelfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(kernel,
                                                                  "/0/data"));

    if (args->method == GWY_CORRELATION_NORMAL)
    {
        state = GWY_COMPUTATION_STATE_INIT;
        gwy_app_wait_start(GTK_WIDGET(args->win1), "Initializing...");
        do {
            gwy_data_field_correlate_iteration(dfield, kernelfield, retfield,
                                           &state, &iteration);
            gwy_app_wait_set_message("Correlating...");
            if (!gwy_app_wait_set_fraction
                    (iteration/(gdouble)(gwy_data_field_get_xres(dfield)
                                     - gwy_data_field_get_xres(kernelfield)/2)))
                return FALSE;

        } while (state != GWY_COMPUTATION_STATE_FINISHED);
        gwy_app_wait_finish();
    }
    else gwy_data_field_correlate(dfield, kernelfield, retfield, args->method);

    /*score - do new data with score*/
    if (args->result == GWY_MASKCOR_SCORE) {
        scorefield = gwy_data_field_new_alike(retfield, TRUE);
        gwy_data_field_copy(retfield, scorefield, TRUE);
        newid = gwy_app_data_browser_add_data_field(scorefield, data, TRUE);
        gwy_app_copy_data_items(data, data, 0, newid, GWY_DATA_ITEM_GRADIENT, 0);
        gwy_app_set_data_field_title(data, newid, _("Correlation score"));
        g_object_unref(scorefield);
    }
    else { /*add mask*/
        gwy_app_undo_checkpoint(data, "/0/mask", NULL);
        if (args->result == GWY_MASKCOR_OBJECTS)
            plot_correlated(retfield, kernelfield->xres, kernelfield->yres,
                            args->threshold);
        else if (args->result == GWY_MASKCOR_MAXIMA)
            plot_maxima(retfield, args->threshold);

        if (gwy_container_gis_object_by_name(data, "/0/mask", &dfield))
            gwy_data_field_copy(retfield, dfield, FALSE);
        else
            gwy_container_set_object_by_name(data, "/0/mask", retfield);
        gwy_app_data_window_set_current(args->win1);
        gwy_data_field_data_changed(retfield);
    }
    g_object_unref(retfield);

    return TRUE;
}

static const gchar result_key[]    = "/module/maskcor/result";
static const gchar method_key[]    = "/module/maskcor/method";
static const gchar threshold_key[] = "/module/maskcor/threshold";

static void
maskcor_sanitize_args(MaskcorArgs *args)
{
    args->result = MIN(args->result, GWY_MASKCOR_LAST-1);
    args->method = MIN(args->method, GWY_CORRELATION_POC);
    args->threshold = CLAMP(args->threshold, -1.0, 1.0);
}

static void
maskcor_load_args(GwyContainer *settings,
                  MaskcorArgs *args)
{
    *args = maskcor_defaults;
    gwy_container_gis_enum_by_name(settings, result_key, &args->result);
    gwy_container_gis_enum_by_name(settings, method_key, &args->method);
    gwy_container_gis_double_by_name(settings, threshold_key, &args->threshold);
    maskcor_sanitize_args(args);
}

static void
maskcor_save_args(GwyContainer *settings,
                  MaskcorArgs *args)
{
    gwy_container_set_enum_by_name(settings, result_key, args->result);
    gwy_container_set_enum_by_name(settings, method_key, args->method);
    gwy_container_set_double_by_name(settings, threshold_key, args->threshold);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

