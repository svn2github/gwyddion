/*
 *  @(#) $Id$
 *  Copyright (C) 2003-2006 David Necas (Yeti), Petr Klapetek.
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
#include <math.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/grains.h>
#include <libgwydgets/gwydgets.h>
#include <app/gwyapp.h>

#define DIST_RUN_MODES GWY_RUN_IMMEDIATE
#define STAT_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

static gboolean module_register   (void);
static void     grain_size_dist   (GwyContainer *data,
                                   GwyRunType run);
static void     grain_height_dist (GwyContainer *data,
                                   GwyRunType run);
static void     grain_stat        (GwyContainer *data,
                                   GwyRunType run);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Evaluates distribution of grains (continuous parts of mask)."),
    "Petr Klapetek <petr@klapetek.cz>, Sven Neumann <neumann@jpk.com>",
    "1.6",
    "David Nečas (Yeti) & Petr Klapetek & Sven Neumann",
    "2003-2006",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("grain_size_dist",
                              (GwyProcessFunc)&grain_size_dist,
                              N_("/_Grains/_Size Distribution"),
                              GWY_STOCK_GRAINS_GRAPH,
                              DIST_RUN_MODES,
                              GWY_MENU_FLAG_DATA | GWY_MENU_FLAG_DATA_MASK,
                              N_("Calculate grain size distribution"));
    gwy_process_func_register("grain_height_dist",
                              (GwyProcessFunc)&grain_height_dist,
                              N_("/_Grains/_Height Distribution"),
                              GWY_STOCK_GRAINS_GRAPH,
                              DIST_RUN_MODES,
                              GWY_MENU_FLAG_DATA | GWY_MENU_FLAG_DATA_MASK,
                              N_("Calculate median grain height distribution"));
    gwy_process_func_register("grain_stat",
                              (GwyProcessFunc)&grain_stat,
                              N_("/_Grains/S_tatistics"),
                              NULL,
                              STAT_RUN_MODES,
                              GWY_MENU_FLAG_DATA | GWY_MENU_FLAG_DATA_MASK,
                              N_("Simple grain statistics"));

    return TRUE;
}

static void
grain_size_dist(GwyContainer *data, GwyRunType run)
{
    GwyGraphCurveModel *cmodel;
    GwyGraphModel *gmodel;
    GwyDataLine *dataline;
    GwyDataField *dfield, *mfield;

    g_return_if_fail(run & DIST_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_MASK_FIELD, &mfield,
                                     0);
    g_return_if_fail(dfield);
    g_return_if_fail(mfield);

    dataline = gwy_data_field_grains_get_distribution
                                        (dfield, mfield, NULL, 0, NULL,
                                         GWY_GRAIN_VALUE_EQUIV_SQUARE_SIDE, 0);

    gmodel = gwy_graph_model_new();
    cmodel = gwy_graph_curve_model_new();
    gwy_graph_model_add_curve(gmodel, cmodel);
    g_object_unref(cmodel);

    gwy_graph_model_set_title(gmodel, _("Grain Size Histogram"));
    gwy_graph_model_set_units_from_data_line(gmodel, dataline);
    gwy_graph_curve_model_set_description(cmodel, "Grain sizes");
    gwy_graph_curve_model_set_data_from_dataline(cmodel, dataline, 0, 0);
    g_object_unref(dataline);

    gwy_app_data_browser_add_graph_model(gmodel, data, TRUE);
    g_object_unref(gmodel);
}

static void
grain_height_dist(GwyContainer *data, GwyRunType run)
{
    GwyGraphCurveModel *cmodel;
    GwyGraphModel *gmodel;
    GwyDataLine *dataline;
    GwyDataField *dfield;
    GwyDataField *mfield;

    g_return_if_fail(run & DIST_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_MASK_FIELD, &mfield,
                                     0);
    g_return_if_fail(dfield);
    g_return_if_fail(mfield);

    dataline = gwy_data_field_grains_get_distribution
                                                (dfield, mfield, NULL, 0, NULL,
                                                 GWY_GRAIN_VALUE_MEDIAN, 0);

    gmodel = gwy_graph_model_new();
    cmodel = gwy_graph_curve_model_new();
    gwy_graph_model_add_curve(gmodel, cmodel);
    g_object_unref(cmodel);

    gwy_graph_model_set_title(gmodel, _("Grain Height Histogram"));
    gwy_graph_model_set_units_from_data_line(gmodel, dataline);
    gwy_graph_curve_model_set_description(cmodel, "Grain heights");
    gwy_graph_curve_model_set_data_from_dataline(cmodel, dataline, 0, 0);
    g_object_unref(dataline);

    gwy_app_data_browser_add_graph_model(gmodel, data, TRUE);
    g_object_unref(gmodel);
}

static void
grain_stat(G_GNUC_UNUSED GwyContainer *data, GwyRunType run)
{
    GtkWidget *dialog, *table, *label;
    GwyDataField *dfield, *mfield;
    GwySIUnit *siunit, *siunit2;
    GwySIValueFormat *vf;
    gint i, xres, yres, ngrains;
    gdouble total_area, area, v, size;
    gdouble *sizes;
    gint *grains;
    GString *str;
    gint row;

    g_return_if_fail(run & STAT_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_MASK_FIELD, &mfield,
                                     0);
    g_return_if_fail(dfield);
    g_return_if_fail(mfield);

    xres = gwy_data_field_get_xres(mfield);
    yres = gwy_data_field_get_yres(mfield);
    total_area = gwy_data_field_get_xreal(mfield)
                 *gwy_data_field_get_yreal(mfield);

    grains = g_new0(gint, xres*yres);
    ngrains = gwy_data_field_number_grains(mfield, grains);
    sizes = gwy_data_field_grains_get_values(dfield, NULL, ngrains, grains,
                                             GWY_GRAIN_VALUE_AREA);
    g_free(grains);
    size = area = 0.0;
    for (i = 1; i <= ngrains; i++) {
        area += sizes[i];
        size += sqrt(sizes[i]);
    }
    area /= ngrains;
    size /= ngrains;
    g_free(sizes);

    dialog = gtk_dialog_new_with_buttons(_("Grain Statistics"), NULL, 0,
                                         GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);

    table = gtk_table_new(4, 2, FALSE);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), table);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    row = 0;
    str = g_string_new("");

    label = gtk_label_new(_("Number of grains:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row+1,
                     GTK_FILL, 0, 2, 2);
    g_string_printf(str, "%d", ngrains);
    label = gtk_label_new(str->str);
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, row, row+1,
                     GTK_FILL, 0, 2, 2);
    row++;

    siunit = gwy_data_field_get_si_unit_xy(mfield);
    siunit2 = gwy_si_unit_power(siunit, 2, NULL);

    label = gtk_label_new(_("Total projected area (abs.):"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row+1,
                     GTK_FILL, 0, 2, 2);
    v = area;
    vf = gwy_si_unit_get_format(siunit2, GWY_SI_UNIT_FORMAT_VFMARKUP, v, NULL);
    g_string_printf(str, "%.*f %s",
                    vf->precision, v/vf->magnitude, vf->units);
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), str->str);
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, row, row+1,
                     GTK_FILL, 0, 2, 2);
    row++;

    label = gtk_label_new(_("Total projected area (rel.):"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row+1,
                     GTK_FILL, 0, 2, 2);
    g_string_printf(str, "%.2f %%", 100.0*area/total_area);
    label = gtk_label_new(str->str);
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, row, row+1,
                     GTK_FILL, 0, 2, 2);
    row++;

    label = gtk_label_new(_("Mean grain area:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row+1,
                     GTK_FILL, 0, 2, 2);
    v = area/ngrains;
    gwy_si_unit_get_format(siunit2, GWY_SI_UNIT_FORMAT_VFMARKUP, v, vf);
    g_string_printf(str, "%.*f %s",
                    vf->precision, v/vf->magnitude, vf->units);
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), str->str);
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, row, row+1,
                     GTK_FILL, 0, 2, 2);
    row++;

    label = gtk_label_new(_("Mean grain size:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row+1,
                     GTK_FILL, 0, 2, 2);
    v = size/ngrains;
    gwy_si_unit_get_format(siunit, GWY_SI_UNIT_FORMAT_VFMARKUP, v, vf);
    g_string_printf(str, "%.*f %s",
                    vf->precision, v/vf->magnitude, vf->units);
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), str->str);
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, row, row+1,
                     GTK_FILL, 0, 2, 2);
    row++;

    gwy_si_unit_value_format_free(vf);
    g_string_free(str, TRUE);
    g_object_unref(siunit2);

    gtk_widget_show_all(dialog);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
