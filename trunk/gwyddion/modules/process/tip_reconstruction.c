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
#include <libprocess/datafield.h>
#include <libprocess/tip.h>
#include <libgwydgets/gwydgets.h>
#include <libgwymodule/gwymodule.h>
#include <app/gwyapp.h>

#define TIP_RECONSTRUCTION_RUN_MODES GWY_RUN_INTERACTIVE

typedef struct {
    GwyDataWindow *win1;
    GwyDataWindow *win2;
} TipReconstructionArgs;

static gboolean   module_register                     (const gchar *name);
static void       tip_reconstruction                  (GwyContainer *data,
                                                       GwyRunType run);
static GtkWidget* tip_reconstruction_window_construct (TipReconstructionArgs *args);
static void       tip_reconstruction_data_cb          (GtkWidget *item);
static gboolean   tip_reconstruction_check            (TipReconstructionArgs *args,
                                                       GtkWidget *tip_reconstruction_window);
static gboolean   tip_reconstruction_do               (TipReconstructionArgs *args);
static GtkWidget* tip_reconstruction_data_option_menu (GwyDataWindow **operand);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Reconstructs surface assuming specified tip."),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{

    gwy_process_func_register("tip_reconstruction",
                              (GwyProcessFunc)&tip_reconstruction,
                              N_("/_Tip/_Surface Reconstruction..."),
                              NULL,
                              TIP_RECONSTRUCTION_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Surface reconstruction by defined tip"));

    return TRUE;
}

/* FIXME: we ignore the Container argument and use current data window */
static void
tip_reconstruction(GwyContainer *data, GwyRunType run)
{
    GtkWidget *tip_reconstruction_window;
    TipReconstructionArgs args;
    gboolean ok = FALSE;

    g_return_if_fail(run & TIP_RECONSTRUCTION_RUN_MODES);
    args.win1 = args.win2 = gwy_app_data_window_get_current();
    g_assert(gwy_data_window_get_data(args.win1) == data);
    tip_reconstruction_window = tip_reconstruction_window_construct(&args);
    gtk_window_present(GTK_WINDOW(tip_reconstruction_window));

    do {
        switch (gtk_dialog_run(GTK_DIALOG(tip_reconstruction_window))) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            case GTK_RESPONSE_NONE:
            gtk_widget_destroy(tip_reconstruction_window);
            ok = TRUE;
            break;

            case GTK_RESPONSE_OK:
            ok = tip_reconstruction_check(&args, tip_reconstruction_window);
            if (ok) {
                gtk_widget_destroy(tip_reconstruction_window);
                tip_reconstruction_do(&args);
            }
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (!ok);
}

static GtkWidget*
tip_reconstruction_window_construct(TipReconstructionArgs *args)
{
    GtkWidget *dialog, *table, *omenu, *label;
    gint row;

    dialog = gtk_dialog_new_with_buttons(_("Surface Reconstruction"), NULL, 0,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    table = gtk_table_new(3, 3, FALSE);
    gtk_table_set_col_spacings(GTK_TABLE(table), 4);
    gtk_table_set_row_spacings(GTK_TABLE(table), 4);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, TRUE, TRUE, 4);
    row = 0;

    /***** First operand *****/
    label = gtk_label_new_with_mnemonic(_("_Tip morphology:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);

    omenu = tip_reconstruction_data_option_menu(&args->win1);
    gtk_table_attach_defaults(GTK_TABLE(table), omenu, 1, 2, row, row+1);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), omenu);
    row++;

    /***** Second operand *****/
    label = gtk_label_new_with_mnemonic(_("_Surface to be reconstructed:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);

    omenu = tip_reconstruction_data_option_menu(&args->win2);
    gtk_table_attach_defaults(GTK_TABLE(table), omenu, 1, 2, row, row+1);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), omenu);
    row++;

    gtk_widget_show_all(dialog);

    return dialog;
}

static GtkWidget*
tip_reconstruction_data_option_menu(GwyDataWindow **operand)
{
    GtkWidget *omenu, *menu;

    omenu = gwy_option_menu_data_window(G_CALLBACK(tip_reconstruction_data_cb),
                                        NULL, NULL, GTK_WIDGET(*operand));
    menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(omenu));
    g_object_set_data(G_OBJECT(menu), "operand", operand);

    return omenu;
}


static void
tip_reconstruction_data_cb(GtkWidget *item)
{
    GtkWidget *menu;
    gpointer p, *pp;

    menu = gtk_widget_get_parent(item);

    p = g_object_get_data(G_OBJECT(item), "data-window");
    pp = (gpointer*)g_object_get_data(G_OBJECT(menu), "operand");
    g_return_if_fail(pp);
    *pp = p;
}


static gboolean
tip_reconstruction_check(TipReconstructionArgs *args,
                         GtkWidget *tip_reconstruction_window)
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

    if (fabs((dfield1->xreal/dfield1->xres)
             /(dfield2->xreal/dfield2->xres) - 1) > 0.01
       || fabs((dfield1->yreal/dfield1->yres)
               /(dfield2->yreal/dfield2->yres) - 1) > 0.01) {
        dialog = gtk_message_dialog_new(GTK_WINDOW(tip_reconstruction_window),
                                    GTK_DIALOG_DESTROY_WITH_PARENT,
                                    GTK_MESSAGE_INFO,
                                    GTK_BUTTONS_OK,
                                    _("Tip has different range/resolution "
                                      "ratio than image. Tip will be "
                                      "resampled."));
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
    }
    return TRUE;
}

static gboolean
tip_reconstruction_do(TipReconstructionArgs *args)
{
    GwyContainer *data;
    GwyDataField *dfield, *dfield1, *dfield2;
    GwyDataWindow *operand1, *operand2;
    gint newid;
    
    operand1 = args->win1;
    operand2 = args->win2;
    g_return_val_if_fail(operand1 != NULL && operand2 != NULL, FALSE);

    data = gwy_data_window_get_data(operand2);
    dfield2 = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    data = gwy_data_window_get_data(operand1);
    dfield1 = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));

    /*result fields - after computation result should be at dfield */
    data = gwy_container_duplicate_by_prefix(data,
                                             "/0/data",
                                             "/0/base/palette",
                                             "/0/select",
                                             NULL);

    dfield = gwy_data_field_duplicate(dfield1);
    gwy_app_wait_start(GTK_WIDGET(args->win2), _("Initializing"));
    dfield = gwy_tip_erosion(dfield1, dfield2, dfield,
                             gwy_app_wait_set_fraction,
                             gwy_app_wait_set_message);
    gwy_app_wait_finish();

   if (dfield) {
        gwy_app_data_browser_get_current(GWY_APP_CONTAINER, &data, 0);
        newid = gwy_app_data_browser_add_data_field(dfield, data, TRUE);
        gwy_app_set_data_field_title(data, newid, _("Surface reconstruction"));
        g_object_unref(dfield);

    }

    return TRUE;
}



/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

