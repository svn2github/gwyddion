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
#include <stdlib.h>
#include <math.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule-graph.h>
#include <libgwydgets/gwygraph.h>
#include <libgwydgets/gwystock.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwyinventorystore.h>
#include <libprocess/cdline.h>
#include <app/gwyapp.h>
#include <app/gwymoduleutils.h>

enum { MAX_PARAMS = 5 };
enum { RESPONSE_SAVE = 1 };

typedef struct {
    gint function_type;
    gint curve;
    gdouble from;
    gdouble to;
    gboolean par_fix[MAX_PARAMS];
    gdouble par_init[MAX_PARAMS];
    gdouble par_res[MAX_PARAMS];
    gdouble err[MAX_PARAMS];
    gdouble crit;
    GwyCDLine *fitfunc;
    GwyGraph *parent_graph;
    gint parent_nofcurves;
    gboolean is_fitted;
    GwyGraphModel *graph_model;
} FitArgs;

typedef struct {
    FitArgs *args;
    GtkWidget *graph;
    GtkWidget *from;
    GtkWidget *to;
    GtkObject *data;
    GtkWidget *selector;
    GtkWidget **param_des;
    GtkWidget **param_res;
    GtkWidget **param_err;
    GtkWidget *criterium;
    GtkWidget *image;
} FitControls;

static gboolean    module_register           (void);
static void        fit                       (GwyGraph *graph);
static void        fit_dialog                (FitArgs *args);
static void        recompute                 (FitArgs *args,
                                              FitControls *controls);
static void        reset                     (FitArgs *args,
                                              FitControls *controls);
static void        plot_inits                (FitArgs *args,
                                              FitControls *controls);
static void        type_changed_cb           (GtkWidget *combo,
                                              FitControls *controls);
static void        from_changed_cb           (GtkWidget *entry,
                                              FitControls *controls);
static void        to_changed_cb             (GtkWidget *entry,
                                              FitControls *controls);
static void        dialog_update             (FitControls *controls,
                                              FitArgs *args);
static void        graph_selected            (GwySelection *selection,
                                              gint i,
                                              FitControls *controls);
static gint        normalize_data            (FitArgs *args,
                                              GwyDataLine *xdata,
                                              GwyDataLine *ydata,
                                              gint curve);
static GtkWidget*  create_preset_menu        (GCallback callback,
                                              gpointer cbdata,
                                              gint current);
static void        load_args                 (GwyContainer *container,
                                              FitArgs *args);
static void        save_args                 (GwyContainer *container,
                                              FitArgs *args);
static void        create_results_window     (FitArgs *args);
static GString*    create_fit_report         (FitArgs *args);
static void        destroy                   (FitArgs *args,
                                              FitControls *controls);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Critical dimension measurements"),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.5",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_graph_func_register("graph_cd",
                            (GwyGraphFunc)&fit,
                            N_("/_Critical dimension"),
                            GWY_STOCK_GRAPH_MEASURE,
                            GWY_MENU_FLAG_GRAPH,
                            N_("Fit critical dimension"));

    return TRUE;
}

static void
fit(GwyGraph *graph)
{
    GwyContainer *settings;
    gint i;
    FitArgs args;

    args.fitfunc = NULL;
    args.function_type = 0;
    args.from = 0;
    args.to = 0;
    args.parent_graph = graph;

    for (i = 0; i < MAX_PARAMS; i++)
        args.par_fix[i] = FALSE;
    args.curve = 1;
    args.is_fitted = FALSE;

    settings = gwy_app_settings_get();
    load_args(settings, &args);
    fit_dialog(&args);
    save_args(settings, &args);
}


/*extract relevant part of data and normalize it to be fitable*/
static gint
normalize_data(FitArgs *args,
               GwyDataLine *xdata,
               GwyDataLine *ydata,
               gint curve)
{
    gint i, j, ns, n;
    GwyGraphCurveModel *cmodel;
    const gdouble *xs, *ys;
    gdouble *xd, *yd;

    if (curve >= gwy_graph_model_get_n_curves(args->graph_model))
                                  return 0;
    cmodel = gwy_graph_model_get_curve(args->graph_model, curve);
    xs = gwy_graph_curve_model_get_xdata(cmodel);
    ys = gwy_graph_curve_model_get_ydata(cmodel);
    ns = gwy_graph_curve_model_get_ndata(cmodel);

    gwy_data_line_resample(xdata, ns, GWY_INTERPOLATION_NONE);
    gwy_data_line_resample(ydata, ns, GWY_INTERPOLATION_NONE);


    j = 0;
    xd = gwy_data_line_get_data(xdata);
    yd = gwy_data_line_get_data(ydata);
    n = gwy_data_line_get_res(xdata);
    for (i = 0; i < n; i++) {
        if ((xs[i] >= args->from
             && xs[i] <= args->to)
              || (args->from == args->to)) {
            xd[j] = xs[i];
            yd[j] = ys[i];
            j++;
        }
    }
    if (j == 0)
        return 0;

    if (j < gwy_data_line_get_res(xdata)) {
        gwy_data_line_resize(xdata, 0, j);
        gwy_data_line_resize(ydata, 0, j);
    }

    return j;
}


static void
fit_dialog(FitArgs *args)
{
    enum {
        RESPONSE_RESET = 1,
        RESPONSE_FIT = 2,
        RESPONSE_PLOT = 3
    };

    GtkWidget *label;
    GtkWidget *table;
    GtkWidget *dialog;
    GtkWidget *hbox;
    GtkWidget *hbox2;
    GtkWidget *table2;
    GtkWidget *vbox;
    FitControls controls;
    gint response, i;
    GwyGraphModel *gmodel;
    GwyGraphArea *area;
    GwySelection *selection;
    char *p, *filename;

    controls.args = args;
    dialog = gtk_dialog_new_with_buttons(_("Critical Dimension"),
                                         NULL, 0, NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_add_action_widget(GTK_DIALOG(dialog),
                                 gwy_stock_like_button_new(_("_Fit"),
                                                           GTK_STOCK_EXECUTE),
                                 RESPONSE_FIT);
    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_OK, GTK_RESPONSE_OK);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    hbox = gtk_hbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 0);

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 4);

    /*fit equation*/
    gtk_container_add(GTK_CONTAINER(vbox),
                      gwy_label_new_header(_("Function Definition")));

    controls.selector = create_preset_menu(G_CALLBACK(type_changed_cb),
                                           &controls, args->function_type);
    gtk_container_add(GTK_CONTAINER(vbox), controls.selector);

    p = gwy_find_self_dir("pixmaps");
    args->fitfunc = gwy_inventory_get_nth_item(gwy_cdlines(),
                                              args->function_type);
    filename = g_build_filename(p, gwy_cdline_get_definition(args->fitfunc),
                                NULL);
    g_free(p);

    controls.image = gtk_image_new_from_file(filename);
    gtk_container_add(GTK_CONTAINER(vbox), controls.image);
    g_free(filename);

    /*fit parameters*/
    gtk_container_add(GTK_CONTAINER(vbox),
                      gwy_label_new_header(_("Fitting Parameters")));

    table = gtk_table_new(MAX_PARAMS, 4, FALSE);
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), " ");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);

    label = gwy_label_new_header(_("Result"));
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 0, 1,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);

    label = gwy_label_new_header(_("Error"));
    gtk_table_attach(GTK_TABLE(table), label, 2, 3, 0, 1,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);


    controls.param_des = g_new(GtkWidget*, MAX_PARAMS);
    for (i = 0; i < MAX_PARAMS; i++) {
        controls.param_des[i] = gtk_label_new(NULL);
        gtk_misc_set_alignment(GTK_MISC(controls.param_des[i]), 0.0, 0.5);
        gtk_table_attach(GTK_TABLE(table), controls.param_des[i],
                         0, 1, i+1, i+2,
                         GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);
    }


    controls.param_res = g_new(GtkWidget*, MAX_PARAMS);
    for (i = 0; i < MAX_PARAMS; i++) {
        controls.param_res[i] = gtk_label_new(NULL);
        gtk_table_attach(GTK_TABLE(table), controls.param_res[i],
                         1, 2, i+1, i+2,
                         GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);

    }

    controls.param_err = g_new(GtkWidget*, MAX_PARAMS);
    for (i = 0; i < MAX_PARAMS; i++) {
        controls.param_err[i] = gtk_label_new(NULL);
        gtk_table_attach(GTK_TABLE(table), controls.param_err[i],
                         2, 3, i+1, i+2,
                         GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);

    }

    gtk_container_add(GTK_CONTAINER(vbox), table);

    /* Fit area */
    gtk_container_add(GTK_CONTAINER(vbox),
                      gwy_label_new_header(_("Fit Area")));

    table2 = gtk_table_new(2, 2, FALSE);
    gmodel = gwy_graph_get_model(GWY_GRAPH(args->parent_graph));
    controls.data = gtk_adjustment_new(args->curve, 1,
                                       gwy_graph_model_get_n_curves(gmodel),
                                       1, 5, 0);
    gwy_table_attach_spinbutton(table2, 1, _("Graph data curve:"), NULL,
                                controls.data);
    gtk_container_add(GTK_CONTAINER(vbox), table2);


    hbox2 = gtk_hbox_new(FALSE, 0);
    gtk_box_set_spacing(GTK_BOX(hbox2), 4);

    label = gtk_label_new(_("From"));
    gtk_container_add(GTK_CONTAINER(hbox2), label);

    controls.from = gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(controls.from), 12);
    gtk_entry_set_width_chars(GTK_ENTRY(controls.from), 12);
    gtk_container_add(GTK_CONTAINER(hbox2), controls.from);
    g_signal_connect(controls.from, "changed",
                      G_CALLBACK(from_changed_cb), &controls);


    label = gtk_label_new(_("To"));
    gtk_container_add(GTK_CONTAINER(hbox2), label);

    controls.to = gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(controls.to), 12);
    gtk_entry_set_width_chars(GTK_ENTRY(controls.to), 12);
    gtk_container_add(GTK_CONTAINER(hbox2), controls.to);
    g_signal_connect(controls.to, "changed",
                      G_CALLBACK(to_changed_cb), &controls);

    gtk_container_add(GTK_CONTAINER(vbox), hbox2);

    args->graph_model = gwy_graph_model_duplicate(gmodel);
    controls.graph = gwy_graph_new(args->graph_model);
    g_object_unref(args->graph_model);
    gtk_widget_set_size_request(controls.graph, 400, 300);

    gwy_graph_enable_user_input(GWY_GRAPH(controls.graph), FALSE);
    gtk_box_pack_start(GTK_BOX(hbox), controls.graph, TRUE, TRUE, 0);
    gwy_graph_set_status(GWY_GRAPH(controls.graph), GWY_GRAPH_STATUS_XSEL);

    area = GWY_GRAPH_AREA(gwy_graph_get_area(GWY_GRAPH(controls.graph)));
    selection = gwy_graph_area_get_selection(area, GWY_GRAPH_STATUS_XSEL);

    gwy_selection_set_max_objects(selection, 1);
    g_signal_connect(selection, "changed",
                     G_CALLBACK(graph_selected), &controls);

    args->fitfunc = gwy_inventory_get_nth_item(gwy_cdlines(),
                                               args->function_type);

    reset(args, &controls);
    dialog_update(&controls, args);

    gtk_widget_show_all(dialog);

    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            destroy(args, &controls);
            gtk_widget_destroy(dialog);
            return;
            break;

            case GTK_RESPONSE_OK:
            if (args->is_fitted)
                create_results_window(args);
            gtk_widget_destroy(dialog);
            break;

            case RESPONSE_RESET:
            reset(args, &controls);
            break;

            case RESPONSE_PLOT:
            plot_inits(args, &controls);
            break;

            case RESPONSE_FIT:
            recompute(args, &controls);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);
}

static void
destroy(G_GNUC_UNUSED FitArgs *args, FitControls *controls)
{
    g_free(controls->param_res);
    g_free(controls->param_err);
}

static void
clear(FitArgs *args, FitControls *controls)
{
    gint i;

    gwy_graph_model_remove_curve_by_description(args->graph_model, "fit");
    for (i = 0; i < MAX_PARAMS; i++) {
        gtk_label_set_markup(GTK_LABEL(controls->param_res[i]), " ");
        gtk_label_set_markup(GTK_LABEL(controls->param_err[i]), " ");
    }
}


static void
plot_inits(FitArgs *args, FitControls *controls)
{

    GwyDataLine *xdata;
    GwyDataLine *ydata;
    GwyCDLine *function;
    gboolean ok;
    gint i, n;
    GwyGraphCurveModel *cmodel;
    gdouble *xd, *yd;


    xdata = gwy_data_line_new(10, 10, FALSE);
    ydata = gwy_data_line_new(10, 10, FALSE);


    args->curve = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->data));
    if (!normalize_data(args, xdata, ydata, args->curve - 1)) {
        g_object_unref(xdata);
        g_object_unref(ydata);
        return;
    }

    function = gwy_inventory_get_nth_item(gwy_cdlines(),
                                        args->function_type);

    xd = gwy_data_line_get_data(xdata);
    yd = gwy_data_line_get_data(ydata);
    n = gwy_data_line_get_res(xdata);
    for (i = 0; i < n; i++)
        yd[i] = gwy_cdline_get_value(function, xd[i],
                                            args->par_res, &ok);



    cmodel = gwy_graph_curve_model_new();
    g_object_set(cmodel,
                 "mode", GWY_GRAPH_CURVE_LINE,
                 "description", "fit",
                 NULL);
    gwy_graph_curve_model_set_data(cmodel, xd, yd, n);
    gwy_graph_model_add_curve(args->graph_model, cmodel);

    g_object_unref(cmodel);
    g_object_unref(xdata);
    g_object_unref(ydata);

}

 /*recompute fit and update everything*/
static void
recompute(FitArgs *args, FitControls *controls)
{
    GwyDataLine *xdata;
    GwyDataLine *ydata;
    GwyCDLine *function;
    gboolean fixed[MAX_PARAMS];
    gchar buffer[64];
    gboolean ok;
    gint i, n, nparams;
    GwyGraphCurveModel *cmodel;
    gdouble *xd, *yd;

    xdata = gwy_data_line_new(10, 10, FALSE);
    ydata = gwy_data_line_new(10, 10, FALSE);


    args->curve = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->data));

    if (!normalize_data(args, xdata, ydata, args->curve - 1)) {
        g_object_unref(xdata);
        g_object_unref(ydata);
        return;
    }

    function = gwy_inventory_get_nth_item(gwy_cdlines(),
                                             args->function_type);
    nparams = gwy_cdline_get_nparams(args->fitfunc);

    for (i = 0; i < MAX_PARAMS; i++) {
        fixed[i] = args->par_fix[i];
        args->par_res[i] = args->par_init[i];
    }

    xd = gwy_data_line_get_data(xdata);
    yd = gwy_data_line_get_data(ydata);
    n = gwy_data_line_get_res(xdata);
    gwy_cdline_fit(function,
                   n, xd, yd,
                   gwy_cdline_get_nparams(function),
                   args->par_res, args->err, fixed, NULL);

    for (i = 0; i < nparams; i++) {
        g_snprintf(buffer, sizeof(buffer), "%3.4g", args->par_res[i]);
        gtk_label_set_markup(GTK_LABEL(controls->param_res[i]), buffer);
    }
    for (i = 0; i < nparams; i++) {
        if (args->err[i] == -1)
            g_snprintf(buffer, sizeof(buffer), "-");
        else
            g_snprintf(buffer, sizeof(buffer), "%3.4g", args->err[i]);
        gtk_label_set_markup(GTK_LABEL(controls->param_err[i]), buffer);
    }



    for (i = 0; i < n; i++)
        yd[i] = gwy_cdline_get_value(function, xd[i],
                                            args->par_res, &ok);


    cmodel = gwy_graph_curve_model_new();
    g_object_set(cmodel,
                 "mode", GWY_GRAPH_CURVE_LINE,
                 "description", "fit",
                 NULL);
    gwy_graph_curve_model_set_data(cmodel, xd, yd, n);
    gwy_graph_model_add_curve(args->graph_model, cmodel);
    g_object_unref(cmodel);

    args->is_fitted = TRUE;
    g_object_unref(xdata);
    g_object_unref(ydata);
}

/*get default parameters (guessed)*/
static void
reset(FitArgs *args, FitControls *controls)
{
    dialog_update(controls, args);
}


static void
type_changed_cb(GtkWidget *combo, FitControls *controls)
{
    char *p, *filename;
    const gchar *definition;
    gint active;

    active = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
    if (active == controls->args->function_type)
                return;


    controls->args->function_type = active;
    controls->args->fitfunc = gwy_inventory_get_nth_item(gwy_cdlines(),
                                             controls->args->function_type);

    p = gwy_find_self_dir("pixmaps");
    definition = gwy_cdline_get_definition(controls->args->fitfunc);
    filename = g_build_filename(p, definition, NULL);
    g_free(p);

    gtk_image_set_from_file(GTK_IMAGE(controls->image), filename);

    dialog_update(controls, controls->args);

    g_free(filename);
}

static void
dialog_update(FitControls *controls, FitArgs *args)
{
    gint i;

    clear(args, controls);

    /*TODO change chema image*/


    for (i = 0; i < MAX_PARAMS; i++) {
        if (i < gwy_cdline_get_nparams(args->fitfunc)) {
            gtk_widget_set_sensitive(controls->param_des[i], TRUE);
            gtk_label_set_markup(GTK_LABEL(controls->param_des[i]),
                      gwy_cdline_get_param_name(args->fitfunc, i));

        /*    gtk_widget_set_sensitive(controls->param_init[i], TRUE);
            gtk_widget_set_sensitive(controls->param_fit[i], TRUE);
            g_snprintf(buffer, sizeof(buffer), "%.3g", args->par_init[i]);
            gtk_entry_set_text(GTK_ENTRY(controls->param_init[i]), buffer);
            */
        }
        else {
            gtk_label_set_markup(GTK_LABEL(controls->param_des[i]), " ");
            gtk_widget_set_sensitive(controls->param_des[i], FALSE);
            /*gtk_widget_set_sensitive(controls->param_init[i], FALSE);
            gtk_widget_set_sensitive(controls->param_fit[i], FALSE);*/
            /*gtk_entry_set_text(GTK_ENTRY(controls->param_init[i]), " ");*/
        }
    }

}


static void
graph_selected(GwySelection *selection, gint i, FitControls *controls)
{
    FitArgs *args = controls->args;
    gchar buffer[24];
    GwyGraphModel *gmodel;
    GwyGraphCurveModel *gcmodel;
    GwyGraph *graph = GWY_GRAPH(controls->graph);
    const gdouble *data;
    gdouble area_selection[2];
    gint nselections;

    nselections = gwy_selection_get_data(selection, NULL);
    gwy_selection_get_object(selection,
                             i,
                             area_selection);
    if (nselections <= 0 || area_selection[0] == area_selection[1]) {
        gmodel = gwy_graph_get_model(graph);
        gcmodel = gwy_graph_model_get_curve(gmodel, args->curve - 1);
        data = gwy_graph_curve_model_get_xdata(gcmodel);
        args->from = data[0];
        args->to = data[gwy_graph_curve_model_get_ndata(gcmodel) - 1];
    }
    else {
        args->from = area_selection[0];
        args->to = area_selection[1];
        if (args->from > args->to)
            GWY_SWAP(gdouble, args->from, args->to);
    }
    g_snprintf(buffer, sizeof(buffer), "%.3g", args->from);
    gtk_entry_set_text(GTK_ENTRY(controls->from), buffer);
    g_snprintf(buffer, sizeof(buffer), "%.3g", args->to);
    gtk_entry_set_text(GTK_ENTRY(controls->to), buffer);

    dialog_update(controls, args);
}

static void
from_changed_cb(GtkWidget *entry, FitControls *controls)
{
    controls->args->from = atof(gtk_entry_get_text(GTK_ENTRY(entry)));
    dialog_update(controls, controls->args);
}

static void
to_changed_cb(GtkWidget *entry, FitControls *controls)
{
    controls->args->to = atof(gtk_entry_get_text(GTK_ENTRY(entry)));
    dialog_update(controls, controls->args);
}

static GtkWidget*
create_preset_menu(GCallback callback,
                   gpointer cbdata,
                   gint current)
{
    GtkCellRenderer *renderer;
    GtkWidget *combo;
    GwyInventoryStore *store;
    gint i;

    store = gwy_inventory_store_new(gwy_cdlines());

    combo = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));
    g_object_unref(store);
    gtk_combo_box_set_wrap_width(GTK_COMBO_BOX(combo), 2);
    renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combo), renderer, TRUE);
    i = gwy_inventory_store_get_column_by_name(store, "name");
    gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(combo), renderer, "text", i);
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo), current);
    g_signal_connect(combo, "changed", callback, cbdata);

    return combo;

}

static const gchar preset_key[] = "/module/graph_cd/preset";

static void
load_args(GwyContainer *container,
          FitArgs *args)
{
    static const guchar *preset;

    if (gwy_container_gis_string_by_name(container, preset_key, &preset)) {
        args->function_type = gwy_inventory_get_item_position(gwy_cdlines(),
                                              (const gchar*)preset);
    }
}

static void
save_args(GwyContainer *container,
          FitArgs *args)
{
    GwyCDLine *func;
    const gchar *name;

    func = gwy_inventory_get_nth_item(gwy_cdlines(), args->function_type);
    name = gwy_resource_get_name(GWY_RESOURCE(func));
    gwy_container_set_string_by_name(container, preset_key, g_strdup(name));

}


/************************* fit report *****************************/
static void
attach_label(GtkWidget *table, const gchar *text,
             gint row, gint col, gdouble halign)
{
    GtkWidget *label;

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), text);
    gtk_misc_set_alignment(GTK_MISC(label), halign, 0.5);

    gtk_table_attach(GTK_TABLE(table), label,
                     col, col+1, row, row+1, GTK_FILL, 0, 2, 2);
}

static void
results_window_response_cb(GtkWidget *window,
                           gint response,
                           GString *report)
{
    if (response == RESPONSE_SAVE) {
        g_return_if_fail(report);
        gwy_save_auxiliary_data(_("Save Fit Report"), GTK_WINDOW(window),
                                -1, report->str);
    }
    else {
        gtk_widget_destroy(window);
        g_string_free(report, TRUE);
    }
}

static gchar*
format_magnitude(GString *str,
                 gdouble magnitude)
{
    if (magnitude)
        g_string_printf(str, "× 10<sup>%d</sup>",
                        (gint)floor(log10(magnitude) + 0.5));
    else
        g_string_assign(str, "");

    return str->str;
}

static gint
count_really_fitted_points(FitArgs *args)
{
    gint i, n, curve;
    GwyGraphCurveModel *cmodel;
    const gdouble *xs, *ys;
    gint ns;

    curve = args->curve - 1;
    n = 0;
    cmodel = gwy_graph_model_get_curve(args->graph_model, curve);
    xs = gwy_graph_curve_model_get_xdata(cmodel);
    ys = gwy_graph_curve_model_get_ydata(cmodel);
    ns = gwy_graph_curve_model_get_ndata(cmodel);

    for (i = 0; i < ns; i++) {
        if ((xs[i] >= args->from
             && xs[i] <= args->to)
            || (args->from == args->to))
            n++;
    }
    return n;
}

static void
create_results_window(FitArgs *args)
{
    GtkWidget *window, *tab, *table, *label;
    GwyGraphCurveModel *gcmodel;
    gdouble mag, value, sigma;
    gint row, curve, n, i;
    gint precision;
    gchar *p, *filename;
    GString *str, *su;
    GtkWidget *image;
    const gchar *s, *definition;

    g_return_if_fail(args->is_fitted);

    window = gtk_dialog_new_with_buttons(_("Fit Results"), NULL, 0,
                                         GTK_STOCK_SAVE, RESPONSE_SAVE,
                                         GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(window), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(window), GTK_RESPONSE_CLOSE);

    table = gtk_table_new(9, 2, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 6);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(window)->vbox), table,
                       FALSE, FALSE, 0);
    row = 0;
    curve = args->curve - 1;

    attach_label(table, _("<b>Data:</b>"), row, 0, 0.0);
    gcmodel = gwy_graph_model_get_curve(args->graph_model, curve);
    g_object_get(gcmodel, "description", &p, NULL);
    str = g_string_new(p);
    g_free(p);

    attach_label(table, str->str, row, 1, 0.0);
    row++;

    str = g_string_new("");
    su = g_string_new("");
    attach_label(table, _("Num of points:"), row, 0, 0.0);
    g_string_printf(str, "%d of %d",
                    count_really_fitted_points(args),
                    gwy_graph_curve_model_get_ndata(gcmodel));

    attach_label(table, str->str, row, 1, 0.0);
    row++;

    attach_label(table, _("X range:"), row, 0, 0.0);
    mag = gwy_math_humanize_numbers((args->to - args->from)/120,
                                    MAX(fabs(args->from), fabs(args->to)),
                                    &precision);
    g_string_printf(str, "%.*f–%.*f %s",
                    precision, args->from/mag,
                    precision, args->to/mag,
                    format_magnitude(su, mag));
    attach_label(table, str->str, row, 1, 0.0);
    row++;

    attach_label(table, _("<b>Function:</b>"), row, 0, 0.0);
    row++;

    p = gwy_find_self_dir("pixmaps");
    definition = gwy_cdline_get_definition(args->fitfunc);
    filename = g_build_filename(p, definition, NULL);
    g_free(p);

    image = gtk_image_new_from_file(filename);
    gtk_table_attach(GTK_TABLE(table), image,
                     0, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 2, 2);
    g_free(filename);
    row++;

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label),
                         gwy_cdline_get_definition(args->fitfunc));
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 2, 2);
    row++;

    attach_label(table, _("<b>Results</b>"), row, 0, 0.0);
    row++;

    n = gwy_cdline_get_nparams(args->fitfunc);
    tab = gtk_table_new(n, 6, FALSE);
    gtk_table_attach(GTK_TABLE(table), tab, 0, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);
    for (i = 0; i < n; i++) {
        attach_label(tab, "=", i, 1, 0.5);
        attach_label(tab, "±", i, 3, 0.5);
        s = gwy_cdline_get_param_name(args->fitfunc, i);
        attach_label(tab, s, i, 0, 0.0);
        value = args->par_res[i];
        sigma = args->err[i];
        if (sigma == -1) {
            g_string_printf(str, "%g", value);
            attach_label(tab, str->str, i, 2, 1.0);
            g_string_printf(str, "-");
            attach_label(tab, str->str, i, 4, 1.0);
        }
        else
        {
            mag = gwy_math_humanize_numbers(sigma/12, fabs(value), &precision);
            g_string_printf(str, "%.*f", precision, value/mag);
            attach_label(tab, str->str, i, 2, 1.0);
            g_string_printf(str, "%.*f", precision, sigma/mag);
            attach_label(tab, str->str, i, 4, 1.0);
            attach_label(tab, format_magnitude(su, mag), i, 5, 0.0);
        }
    }
    row++;

    g_string_free(str, TRUE);
    g_string_free(su, TRUE);
    str = create_fit_report(args);

    g_signal_connect(window, "response",
                     G_CALLBACK(results_window_response_cb), str);
    gtk_widget_show_all(window);
}

static GString*
create_fit_report(FitArgs *args)
{
    GwyGraphCurveModel *gcmodel;
    GString *report, *str;
    gchar *s, *s2;
    gint i, curve, n;

    report = g_string_new("");

    curve = args->curve - 1;
    g_string_append_printf(report, _("\n===== Fit Results =====\n"));

    gcmodel = gwy_graph_model_get_curve(args->graph_model, curve);
    g_object_get(gcmodel, "description", &s, NULL);
    str = g_string_new(s);
    g_free(s);
    g_string_append_printf(report, _("Data: %s\n"), str->str);
    g_string_assign(str, "");
    g_string_append_printf(report, _("Number of points: %d of %d\n"),
                           count_really_fitted_points(args),
                           gwy_graph_curve_model_get_ndata(gcmodel));

    g_string_append_printf(report, _("X range:          %g to %g\n"),
                           args->from, args->to);
    g_string_append_printf(report, _("Fitted function:  %s\n"),
                           gwy_cdline_get_name(args->fitfunc));
    g_string_append_printf(report, _("\nResults\n"));
    n = gwy_cdline_get_nparams(args->fitfunc);
    for (i = 0; i < n; i++) {
        /* FIXME: how to do this better? use pango_parse_markup()? */
        s = gwy_strreplace(gwy_cdline_get_param_name(args->fitfunc,
                                                                i),
                           "<sub>", "", (gsize)-1);
        s2 = gwy_strreplace(s, "</sub>", "", (gsize)-1);
        g_string_append_printf(report, "%s = %g ± %g\n",
                               s2, args->par_res[i], args->err[i]);
        g_free(s2);
        g_free(s);
    }

    g_string_free(str, TRUE);

    return report;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
