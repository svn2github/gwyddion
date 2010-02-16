/*
 *  @(#) $Id: cc_view.c 8263 2007-06-25 21:21:28Z yeti-dn $
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwynlfit.h>
#include <libgwydgets/gwydgets.h>
#include <libprocess/stats.h>
#include <libprocess/arithmetic.h>
#include <libprocess/gwycalibration.h>
#include <libprocess/gwycaldata.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwylayer-basic.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwymoduleutils.h>
#include <app/gwyapp.h>


#define CC_VIEW_RUN_MODES GWY_RUN_INTERACTIVE

#define MAX_PARAMS 4


enum { PREVIEW_SIZE = 200 };

enum { RESPONSE_PREVIEW = 1,
       RESPONSE_LOAD = 2};

typedef enum {
    GWY_CC_VIEW_DISPLAY_X_CORR   = 0,
    GWY_CC_VIEW_DISPLAY_Y_CORR = 1,
    GWY_CC_VIEW_DISPLAY_Z_CORR = 2,
    GWY_CC_VIEW_DISPLAY_X_UNC = 3,
    GWY_CC_VIEW_DISPLAY_Y_UNC = 4,
    GWY_CC_VIEW_DISPLAY_Z_UNC = 5,
} GwyCCViewDisplayType;

typedef enum {
    GWY_CC_VIEW_PLANE_X   = 0,
    GWY_CC_VIEW_PLANE_Y = 1,
    GWY_CC_VIEW_PLANE_Z = 2
} GwyCCViewPlaneType;

typedef enum {
    GWY_CC_VIEW_INTERPOLATION_3D   = 0,
    GWY_CC_VIEW_INTERPOLATION_PLANE = 1,
    GWY_CC_VIEW_INTERPOLATION_NATURAL   = 2
} GwyCCViewInterpolationType;


typedef struct {
    gdouble xrange;
    gdouble yrange;
    gdouble zrange;
    gint ndata;
    gdouble **calval;  // set of calibration values: x, y, z, x_cor, y_cor, z_cor, x_unc, y_unc, z_unc
} GwyCalibrationData;


typedef struct {
    GwyCCViewDisplayType display_type;
    GwyCCViewPlaneType plane_type;
    GwyCCViewInterpolationType interpolation_type;
    gdouble xplane;
    gdouble yplane;
    gdouble zplane;
    gboolean crop;
    gboolean update;
    gint calibration;
    gboolean computed;
    gint id;
} CCViewArgs;

typedef struct {
    GtkWidget *dialog;
    GtkWidget *view;
    GtkWidget *type;
    GwyContainer *mydata;
    GtkWidget *menu_display;
    GtkWidget *menu_plane;
    GtkWidget *menu_interpolation;
    GtkWidget *crop;
    GtkWidget *update;
    GtkWidget *calibration;
    GwyContainer *data;
    gint original_id;
    GwyDataField *view_field;
    GwyDataField *actual_field;
    GwyDataField *xerr;
    GwyDataField *yerr;
    GwyDataField *zerr;
    GwyDataField *xunc;
    GwyDataField *yunc;
    GwyDataField *zunc;
    GtkObject *xplane;
    GtkObject *yplane;
    GtkObject *zplane;
    CCViewArgs *args;
} CCViewControls;

static gboolean     module_register        (void);
static void         cc_view                 (GwyContainer *data,
                                            GwyRunType run);
static void         cc_view_dialog          (CCViewArgs *args,
                                            GwyContainer *data,
                                            GwyDataField *dfield,
                                            gint id);
static void         cc_view_load_args       (GwyContainer *container,
                                            CCViewArgs *args);
static void         cc_view_save_args       (GwyContainer *container,
                                            CCViewArgs *args);
static void         cc_view_sanitize_args   (CCViewArgs *args);
static void         cc_view_run             (CCViewControls *controls,
                                            CCViewArgs *args);
static void         cc_view_do              (CCViewControls *controls);
static void         cc_view_dialog_abandon  (CCViewControls *controls);
static GtkWidget*   menu_display           (GCallback callback,
                                            gpointer cbdata,
                                            GwyCCViewDisplayType current);
static GtkWidget*   menu_plane             (GCallback callback,
                                            gpointer cbdata,
                                            GwyCCViewPlaneType current);
static GtkWidget*   menu_interpolation     (GCallback callback,
                                            gpointer cbdata,
                                            GwyCCViewInterpolationType current);
static void         display_changed        (GtkComboBox *combo,
                                            CCViewControls *controls);
static void         calculation_changed        (GtkComboBox *combo,
                                            CCViewControls *controls);


static void         update_view            (CCViewControls *controls,
                                            CCViewArgs *args);
static void         settings_changed       (CCViewControls *controls);
static void         crop_change_cb         (CCViewControls *controls);
static void         update_change_cb       (CCViewControls *controls);
static void         brutal_search          (GwyCalData *caldata, 
                                            gdouble x, 
                                            gdouble y, 
                                            gdouble z, 
                                            gdouble radius,
                                            gint *pos, 
                                            gdouble *dist, 
                                            gint *ndata, 
                                            GwyCCViewInterpolationType snap_type);
static void         get_value              (GwyCalData *caldata, 
                                            gdouble x, 
                                            gdouble y, 
                                            gdouble z, 
                                            gdouble *xerr, 
                                            gdouble *yerr, 
                                            gdouble *zerr, 
                                            gdouble *xunc, 
                                            gdouble *yunc, 
                                            gdouble *zunc, 
                                            GwyCCViewInterpolationType snap_type);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("3D calibration/uncertainty"),
    "Petr Klapetek <petr@klapetek.cz>",
    "1.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2008",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("cc_view",
                              (GwyProcessFunc)&cc_view,
                              N_("/Cali_bration/_3D calibration/Apply to data..."),
                              NULL,
                              CC_VIEW_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("3D calibration and uncertainty"));

    return TRUE;
}

static void
cc_view(GwyContainer *data, GwyRunType run)
{
    CCViewArgs args;
    GwyDataField *dfield;
    g_return_if_fail(run & CC_VIEW_RUN_MODES);

    cc_view_load_args(gwy_app_settings_get(), &args);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &(args.id),
                                     0);
    g_return_if_fail(dfield);

    cc_view_dialog(&args, data, dfield, args.id);
    cc_view_save_args(gwy_app_settings_get(), &args);
}

static void
cc_view_dialog(CCViewArgs *args,
              GwyContainer *data,
              GwyDataField *dfield,
              gint id)
{
    GtkWidget *dialog, *table, *hbox, *vbox, *alignment;
    CCViewControls controls;
    GwyInventory *inventory;
    GwyInventoryStore *store;
    GwyPixmapLayer *layer;
    GwyCalibration *calibration;
    GtkCellRenderer *renderer;
    gint response;
    guint row = 0;
    GtkWidget *label;


    controls.args = args;
    args->crop = 0;
    args->calibration = 0;
    args->computed = 0;
    args->interpolation_type = GWY_CC_VIEW_INTERPOLATION_3D;


    dialog = gtk_dialog_new_with_buttons(_("3D calibration"), NULL, 0, NULL);
    gtk_dialog_add_action_widget(GTK_DIALOG(dialog),
                                 gwy_stock_like_button_new(_("_Update"),
                                                           GTK_STOCK_EXECUTE),
                                 RESPONSE_PREVIEW);
     gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_OK, GTK_RESPONSE_OK);

    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    controls.dialog = dialog;

    hbox = gtk_hbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 4);

    controls.actual_field = dfield; 
    controls.view_field = gwy_data_field_new(200, 200, 100, 100, TRUE);
    controls.xerr = gwy_data_field_new_alike(controls.view_field, TRUE);
    controls.yerr = gwy_data_field_new_alike(controls.view_field, TRUE);
    controls.zerr = gwy_data_field_new_alike(controls.view_field, TRUE);
    controls.xunc = gwy_data_field_new_alike(controls.view_field, TRUE);
    controls.yunc = gwy_data_field_new_alike(controls.view_field, TRUE);
    controls.zunc = gwy_data_field_new_alike(controls.view_field, TRUE);

 

    controls.data = data;
    controls.mydata = gwy_container_new();
    gwy_container_set_object_by_name(controls.mydata, "/0/data", controls.view_field);
    gwy_app_sync_data_items(data, controls.mydata, id, 0, FALSE,
                            GWY_DATA_ITEM_PALETTE,
                            0);
    controls.view = gwy_data_view_new(controls.mydata);
    layer = gwy_layer_basic_new();
    gwy_pixmap_layer_set_data_key(layer, "/0/data");
    //gwy_layer_basic_set_gradient_key(GWY_LAYER_BASIC(layer), "/0/base/palette");
    gwy_data_view_set_data_prefix(GWY_DATA_VIEW(controls.view), "/0/data");
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.view), layer);
    gwy_set_data_preview_size(GWY_DATA_VIEW(controls.view), PREVIEW_SIZE);

    alignment = GTK_WIDGET(gtk_alignment_new(0.5, 0, 0, 0));
    gtk_container_add(GTK_CONTAINER(alignment), controls.view);
    gtk_box_pack_start(GTK_BOX(hbox), alignment, FALSE, FALSE, 4);

    /*set up fit controls*/
    vbox = gtk_vbox_new(FALSE, 3);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 4);

    label = gwy_label_new_header(_("Used calibration data:"));
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 4);

    inventory = gwy_calibrations();
    store = gwy_inventory_store_new(inventory);
    controls.calibration = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));
    renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(controls.calibration), renderer, FALSE);
    gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(controls.calibration), renderer,
                                  "text", 1);
    gtk_combo_box_set_active(controls.calibration, args->calibration);

    gtk_box_pack_start(GTK_BOX(vbox), controls.calibration, FALSE, FALSE, 4);

    label = gwy_label_new_header(_("Shown planes:"));
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 4);

    table = gtk_table_new(8, 4, FALSE);


    label = gtk_label_new_with_mnemonic(_("View:"));

    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);

    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);

    controls.menu_display = menu_display(G_CALLBACK(display_changed),
                                         &controls,
                                         args->display_type);
    //row++;

    gtk_table_attach(GTK_TABLE(table), controls.menu_display, 1, 2, row, row+1,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);

    label = gtk_label_new_with_mnemonic(_("Plane:"));

    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);

    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);

    row++;
    controls.menu_plane = menu_plane(G_CALLBACK(calculation_changed),
                                         &controls,
                                         args->display_type);

    gtk_table_attach(GTK_TABLE(table), controls.menu_plane, 1, 2, row, row+1,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);

    row++;
    args->xplane = args->yplane = args->zplane = 0;
    controls.xplane = gtk_adjustment_new(args->xplane,
                                       0.0, 100.0, 1, 100, 0);
    gwy_table_attach_hscale(table, row++, _("X pos:"), "%",
                                       controls.xplane, 0);
//    label = gwy_table_hscale_get_units(controls.xplane);
//    gtk_label_set_text(label, "ble");
    g_signal_connect_swapped(controls.xplane, "value-changed",
                                       G_CALLBACK(settings_changed), &controls);

    controls.yplane = gtk_adjustment_new(args->yplane,
                                       0.0, 100.0, 1, 100, 0);
    gwy_table_attach_hscale(table, row++, _("Y pos:"), "%",
                                       controls.yplane, 0);
    g_signal_connect_swapped(controls.yplane, "value-changed",
                                       G_CALLBACK(settings_changed), &controls);

    controls.zplane = gtk_adjustment_new(args->zplane,
                                       0.0, 100.0, 1, 100, 0);
    gwy_table_attach_hscale(table, row++, _("Z pos:"), "%",
                                       controls.zplane, 0);
    g_signal_connect_swapped(controls.zplane, "value-changed",
                                       G_CALLBACK(settings_changed), &controls);

    label = gtk_label_new_with_mnemonic(_("Interpolation:"));

    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);

    gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row+1,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);


    controls.menu_interpolation = menu_interpolation(G_CALLBACK(calculation_changed),
                                         &controls,
                                         args->interpolation_type);

    gtk_table_attach(GTK_TABLE(table), controls.menu_interpolation, 1, 2, row, row+1,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);

    row++;

    controls.crop = gtk_check_button_new_with_mnemonic(_("crop to actual data"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.crop),
                                                    args->crop);
    gtk_table_attach(GTK_TABLE(table), controls.crop,
                         0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_signal_connect_swapped(controls.crop, "toggled",
                        G_CALLBACK(crop_change_cb), &controls);
    row++;


    controls.update = gtk_check_button_new_with_mnemonic(_("i_nstant updates"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.update),
                                                    args->update);
    gtk_table_attach(GTK_TABLE(table), controls.update,
                         0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_signal_connect_swapped(controls.update, "toggled",
                         G_CALLBACK(update_change_cb), &controls);
    row++;


    gtk_container_add(GTK_CONTAINER(vbox), table);
    calculation_changed(NULL, &controls);
    update_view(&controls, args);

    gtk_widget_show_all(dialog);
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            return;
            break;

            case GTK_RESPONSE_OK:
            if (!args->computed || !args->crop)
            {
                printf("recomputing for crop\n");
                args->crop = TRUE;
                args->computed = FALSE;
                update_view(&controls, args);
                printf("done\n");
            }
            cc_view_do(&controls);
            break;

            case RESPONSE_PREVIEW:
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    args->calibration = gtk_combo_box_get_active(controls.calibration);
    calibration = gwy_inventory_get_nth_item(inventory, args->calibration);
   // printf("Applying %d %s %s\n", calibration->ndata, calibration->filename, GWY_RESOURCE(calibration)->name->str);

    gtk_widget_destroy(dialog);
    cc_view_dialog_abandon(&controls);

}

static void
cc_view_dialog_abandon(CCViewControls *controls)
{
    gwy_object_unref(controls->view_field);
    gwy_object_unref(controls->mydata);
}


/*update preview depending on user's wishes*/
static void
update_view(CCViewControls *controls, CCViewArgs *args)
{
    gint i, col, row, xres, yres, zres;
    gdouble x, y, z, xerr, yerr, zerr, xunc, yunc, zunc;
    GwyDataField *viewfield;
    GwyCalibration *calibration;
    GwyCalData *caldata;
    gsize len;
    GError *err = NULL;
    gchar *contents;
    gchar *filename;
    gsize pos = 0;
    viewfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                                  "/0/data"));
   //viewfield = controls->view_field;

    args->calibration = gtk_combo_box_get_active(controls->calibration);
    calibration = gwy_inventory_get_nth_item(gwy_calibrations(), args->calibration);
    if (calibration) {
        filename = g_build_filename(gwy_get_user_dir(), "caldata", calibration->filename, NULL);
        if (!g_file_get_contents(filename, 
                                 &contents, &len, &err))
        {
             g_warning("Error loading file: %s\n", err->message);
             g_clear_error(&err);
             return;
        }    
        else {
            if (len)
              caldata = GWY_CALDATA(gwy_serializable_deserialize(contents, len, &pos));
            g_free(contents);
        }

    } else {
        /*output something?*/
        printf("update view: no calibration data\n");
        caldata = NULL;
        calibration = NULL;
        return;
    }
    printf("updating view\n");

    /*FIXME determine maximum necessary size of field*/
    xres = 200; 
    yres = 200;
    zres = 200;

    if (!caldata) {
        gwy_data_field_fill(viewfield, 0);
        gwy_data_field_data_changed(viewfield);
        printf("update view: no calibration data\n");
        return;
    }
  
    printf("output caldata\n"); 
    printf("total %d caldata (%g %g %g    %g %g %g)\n", caldata->ndata, caldata->x_from, caldata->y_from, caldata->z_from,
           caldata->x_to, caldata->y_to, caldata->z_to);
    /*for (i=0; i<caldata->ndata; i++)
    {
        printf("caldata: %g %g %g %g %g %g %g %g %g\n", caldata->x[i], caldata->y[i], caldata->z[i], 
                                                        caldata->xerr[i], caldata->yerr[i], caldata->zerr[i],
                                                        caldata->xunc[i], caldata->yunc[i], caldata->zunc[i]);
    }*/

    if (!args->computed) {
        gwy_app_wait_cursor_start(GTK_WINDOW(controls->dialog));

        if (args->interpolation_type == GWY_CC_VIEW_INTERPOLATION_NATURAL)
        {
            printf("setup interpolation\n");
            gwy_caldata_setup_interpolation(caldata);
            printf("done\n");
        }

        if (controls->args->crop) {
            for (row=0; row<yres; row++)
            {
                y = gwy_data_field_get_yoffset(controls->actual_field) + 
                    row*gwy_data_field_get_yreal(controls->actual_field)/yres;
                for (col=0; col<xres; col++) {
                    x = gwy_data_field_get_xoffset(controls->actual_field) +
                        col*gwy_data_field_get_yreal(controls->actual_field)/xres;
                    z = gwy_data_field_get_dval(controls->actual_field, 
                                                x - gwy_data_field_get_xoffset(controls->actual_field), 
                                                y - gwy_data_field_get_yoffset(controls->actual_field),
                                                GWY_INTERPOLATION_BILINEAR);

                    get_value(caldata, x, y, z, &xerr, &yerr, &zerr, &xunc, &yunc, &zunc, args->interpolation_type);
                    controls->xerr->data[col + xres*row] = xerr;
                    controls->yerr->data[col + xres*row] = yerr;
                    controls->zerr->data[col + xres*row] = zerr;
                    controls->xunc->data[col + xres*row] = xunc;
                    controls->yunc->data[col + xres*row] = yunc;
                    controls->zunc->data[col + xres*row] = zunc;


                }
            }
        } else {
            if (controls->args->plane_type == GWY_CC_VIEW_PLANE_X)
            {
                printf("up x\n");
                gwy_data_field_resample(viewfield, yres, zres, GWY_INTERPOLATION_NONE);
                x = caldata->x_from + (caldata->x_to-caldata->x_from)*(gdouble)args->xplane/100.0;
                for (col=0; col<yres; col++)
                {
                    y = caldata->y_from + (caldata->y_to-caldata->y_from)*(gdouble)col/(double)yres;
                    for (row=0; row<zres; row++) {
                        z = caldata->z_from + (caldata->z_to-caldata->z_from)*(gdouble)row/(double)zres;

                        get_value(caldata, x, y, z, &xerr, &yerr, &zerr, &xunc, &yunc, &zunc, args->interpolation_type);
                        controls->xerr->data[col + yres*row] = xerr;
                        controls->yerr->data[col + yres*row] = yerr;
                        controls->zerr->data[col + yres*row] = zerr;
                        controls->xunc->data[col + yres*row] = xunc;
                        controls->yunc->data[col + yres*row] = yunc;
                        controls->zunc->data[col + yres*row] = zunc;
                    }
                }
            }
            if (controls->args->plane_type == GWY_CC_VIEW_PLANE_Y)
            {
                printf("up y\n");
                gwy_data_field_resample(viewfield, xres, zres, GWY_INTERPOLATION_NONE);
                y = caldata->y_from + (caldata->y_to-caldata->y_from)*(gdouble)args->yplane/100.0;
                for (col=0; col<xres; col++)
                { 
                    x = caldata->x_from + (caldata->x_to-caldata->x_from)*(gdouble)col/(double)xres;
                    for (row=0; row<zres; row++) {
                        z = caldata->z_from + (caldata->z_to-caldata->z_from)*(gdouble)row/(double)zres;
                        get_value(caldata, x, y, z, &xerr, &yerr, &zerr, &xunc, &yunc, &zunc, args->interpolation_type);
                        controls->xerr->data[col + xres*row] = xerr;
                        controls->yerr->data[col + xres*row] = yerr;
                        controls->zerr->data[col + xres*row] = zerr;
                        controls->xunc->data[col + xres*row] = xunc;
                        controls->yunc->data[col + xres*row] = yunc;
                        controls->zunc->data[col + xres*row] = zunc;
                    }
                }
            }
            if (controls->args->plane_type == GWY_CC_VIEW_PLANE_Z)
            {
                printf("up z\n");
                gwy_data_field_resample(viewfield, xres, yres, GWY_INTERPOLATION_NONE);
                gwy_data_field_set_xreal(viewfield, caldata->x_to - caldata->x_from);
                gwy_data_field_set_yreal(viewfield, caldata->y_to - caldata->y_from);

                z = caldata->z_from + (caldata->z_to-caldata->z_from)*(gdouble)args->zplane/100.0;
                for (col=0; col<xres; col++)
                {
                    x = gwy_data_field_get_yoffset(viewfield) +
                        col*gwy_data_field_get_xreal(viewfield)/xres;
                    for (row=0; row<yres; row++) {
                        y = gwy_data_field_get_yoffset(viewfield) +
                            row*gwy_data_field_get_yreal(viewfield)/yres;

                        get_value(caldata, x, y, z, &xerr, &yerr, &zerr, &xunc, &yunc, &zunc, args->interpolation_type);
                        controls->xerr->data[col + xres*row] = xerr;
                        controls->yerr->data[col + xres*row] = yerr;
                        controls->zerr->data[col + xres*row] = zerr;
                        controls->xunc->data[col + xres*row] = xunc;
                        controls->yunc->data[col + xres*row] = yunc;
                        controls->zunc->data[col + xres*row] = zunc;
                    }
                }
            }
        }
        gwy_data_field_invalidate(controls->xerr);
        gwy_data_field_invalidate(controls->yerr);
        gwy_data_field_invalidate(controls->zerr);
        gwy_data_field_invalidate(controls->xunc);
        gwy_data_field_invalidate(controls->yunc);
        gwy_data_field_invalidate(controls->zunc);
        args->computed = TRUE;
    }

    if (controls->args->display_type == GWY_CC_VIEW_DISPLAY_X_CORR)
        gwy_data_field_copy(controls->xerr, viewfield, FALSE);
    else if (controls->args->display_type == GWY_CC_VIEW_DISPLAY_Y_CORR) 
        gwy_data_field_copy(controls->yerr, viewfield, FALSE); 
    else if (controls->args->display_type == GWY_CC_VIEW_DISPLAY_Z_CORR) 
        gwy_data_field_copy(controls->zerr, viewfield, FALSE);
    else if (controls->args->display_type == GWY_CC_VIEW_DISPLAY_X_UNC) 
        gwy_data_field_copy(controls->xunc, viewfield, FALSE);
    else if (controls->args->display_type == GWY_CC_VIEW_DISPLAY_Y_UNC) 
        gwy_data_field_copy(controls->yunc, viewfield, FALSE);
    else if (controls->args->display_type == GWY_CC_VIEW_DISPLAY_Z_UNC) 
        gwy_data_field_copy(controls->zunc, viewfield, FALSE);
 

    gwy_app_wait_cursor_finish(GTK_WINDOW(controls->dialog));
    printf("updated\n");
    gwy_data_field_invalidate(controls->view_field);
    gwy_data_field_data_changed(controls->view_field);
}


void add_calibration(GwyDataField *dfield,
                     GwyContainer *data,
                     gint id,
                     GwyCCViewDisplayType type)
{
    gchar key[24];


    if (type == GWY_CC_VIEW_DISPLAY_X_CORR)
       g_snprintf(key, sizeof(key), "/%d/cal_xerr", id);
    else if (type == GWY_CC_VIEW_DISPLAY_Y_CORR)
       g_snprintf(key, sizeof(key), "/%d/cal_yerr", id);
    else if (type == GWY_CC_VIEW_DISPLAY_Z_CORR)
       g_snprintf(key, sizeof(key), "/%d/cal_zerr", id);
    else if (type == GWY_CC_VIEW_DISPLAY_X_UNC)
       g_snprintf(key, sizeof(key), "/%d/cal_xunc", id);
    else if (type == GWY_CC_VIEW_DISPLAY_Y_UNC)
       g_snprintf(key, sizeof(key), "/%d/cal_yunc", id);
    else if (type == GWY_CC_VIEW_DISPLAY_Z_UNC)
       g_snprintf(key, sizeof(key), "/%d/cal_zunc", id);
    else {
        g_critical("No such calibration key.");
        return;
    }
    gwy_container_set_object_by_name(data, key, dfield);

}


/*dialog finished, everything should be computed*/
static void
cc_view_do(CCViewControls *controls)
{

    add_calibration(controls->xerr, controls->data, controls->args->id, GWY_CC_VIEW_DISPLAY_X_CORR);
    add_calibration(controls->yerr, controls->data, controls->args->id, GWY_CC_VIEW_DISPLAY_Y_CORR);
    add_calibration(controls->zerr, controls->data, controls->args->id, GWY_CC_VIEW_DISPLAY_Z_CORR);
    add_calibration(controls->xunc, controls->data, controls->args->id, GWY_CC_VIEW_DISPLAY_X_UNC);
    add_calibration(controls->yunc, controls->data, controls->args->id, GWY_CC_VIEW_DISPLAY_Y_UNC);
    add_calibration(controls->zunc, controls->data, controls->args->id, GWY_CC_VIEW_DISPLAY_Z_UNC);

    /*now the data should be present in container and user functions can use them*/

/*
    gint newid = gwy_app_data_browser_add_data_field(controls->xerr, controls->data, TRUE);
    g_object_unref(controls->xerr);
    gwy_app_set_data_field_title(controls->data, newid, "X correction");

    newid = gwy_app_data_browser_add_data_field(controls->yerr, controls->data, TRUE);
    g_object_unref(controls->yerr);
    gwy_app_set_data_field_title(controls->data, newid, "Y correction");

    newid = gwy_app_data_browser_add_data_field(controls->zerr, controls->data, TRUE);
    g_object_unref(controls->zerr);
    gwy_app_set_data_field_title(controls->data, newid, "Z correction");

    newid = gwy_app_data_browser_add_data_field(controls->xunc, controls->data, TRUE);
    g_object_unref(controls->xunc);
    gwy_app_set_data_field_title(controls->data, newid, "X uncertainty");

    newid = gwy_app_data_browser_add_data_field(controls->yunc, controls->data, TRUE);
    g_object_unref(controls->yunc);
    gwy_app_set_data_field_title(controls->data, newid, "Y uncertainty");

    newid = gwy_app_data_browser_add_data_field(controls->zunc, controls->data, TRUE);
    g_object_unref(controls->zunc);
    gwy_app_set_data_field_title(controls->data, newid, "Z uncertainty");
*/

}
static void
brutal_search(GwyCalData *caldata, gdouble x, gdouble y, gdouble z, gdouble radius,
                   gint *pos, gdouble *dist, gint *ndata, GwyCCViewInterpolationType snap_type)
{
    gint i, j, smallest = 0, largest = 0;
    gdouble val, minval, maxval;
    gint maxdata = *ndata;
    gboolean snap = 0;
    gdouble splane = 0;
    *ndata = 0;

    if (!caldata) return;

    /*find closest plane, if requested*/
    if (snap_type == GWY_CC_VIEW_INTERPOLATION_PLANE)
    {
        minval = G_MAXDOUBLE;
        for (i=0; i<caldata->ndata; i++)
        {
            if (fabs(z - caldata->z[i]) < minval) {
                minval = fabs(z - caldata->z[i]);
                smallest = i;
            }
        }
        splane = caldata->z[smallest];
        snap = 1;
    }
  //  printf("plane: %g\n", splane);

    for (i=0; i<caldata->ndata; i++)
    {
        if (snap && (fabs(caldata->z[i]-splane))>1e-6) continue;

        if ((val=((caldata->x[i] - x)*(caldata->x[i] - x) +
             (caldata->y[i] - y)*(caldata->y[i] - y) +
             (caldata->z[i] - z)*(caldata->z[i] - z))) < (radius*radius))
        {
            if ((*ndata) == maxdata)
            {
                maxval = -G_MAXDOUBLE;
                for (j=0; j<(*ndata); j++)
                {
                    if (dist[j] > maxval) {
                        maxval = dist[j];
                        largest = j;
                    }
                }
                if ((dist[largest]*dist[largest]) > val)
                {
                    pos[largest] = i;
                    dist[largest] = sqrt(val);
                }

            }
            else {
                pos[(*ndata)] = i;
                dist[(*ndata)++] = sqrt(val);
            }
        }
    }
}


static void     
get_value(GwyCalData *caldata, gdouble x, gdouble y, gdouble z, 
          gdouble *xerr, gdouble *yerr, gdouble *zerr, 
          gdouble *xunc, gdouble *yunc, gdouble *zunc, 
          GwyCCViewInterpolationType snap_type)
{

    gint i;
    gint pos[500];
    gint ndata=9;
    gdouble sumxerr, sumyerr, sumzerr, sumxunc, sumyunc, sumzunc, sumw;
    gdouble w, dist[500];

    if (!caldata) {printf("no caldata!\n"); return 0.0;}

    //printf("request for point %g %g %g\n", x, y, z);

    if (snap_type == GWY_CC_VIEW_INTERPOLATION_NATURAL)
    gwy_caldata_interpolate(caldata, x, y, z, xerr, yerr, zerr, xunc, yunc, zunc);
    else
    {
        brutal_search(caldata, x, y, z, 1e-1, pos, dist, &ndata, snap_type);
        sumxerr = sumyerr = sumzerr = sumxunc = sumyunc = sumzunc = sumw = 0;
        for (i=0; i<ndata; i++)
        {
            if (dist[i]<1e-9) {
                sumw = 1;
                sumxerr = caldata->xerr[pos[i]];
                sumyerr = caldata->yerr[pos[i]];
                sumzerr = caldata->zerr[pos[i]];
                sumxunc = caldata->xunc[pos[i]];
                sumyunc = caldata->yunc[pos[i]];
                sumzunc = caldata->zunc[pos[i]];
                break;
            }
            else {
                w = 1.0/dist[i];
                w = w*w;
                sumw += w;
                sumxerr += w*caldata->xerr[pos[i]];
                sumyerr += w*caldata->yerr[pos[i]];
                sumzerr += w*caldata->zerr[pos[i]];
                sumxunc += w*caldata->xunc[pos[i]];
                sumyunc += w*caldata->yunc[pos[i]];
                sumzunc += w*caldata->zunc[pos[i]];
            }
        }
        *xerr = sumxerr/sumw;
        *yerr = sumyerr/sumw;
        *zerr = sumzerr/sumw;
        *xunc = sumxunc/sumw;
        *yunc = sumyunc/sumw;
        *zunc = sumzunc/sumw;
    }

}



/*display mode menu*/
static GtkWidget*
menu_display(GCallback callback, gpointer cbdata,
             GwyCCViewDisplayType current)
{
    static const GwyEnum entries[] = {
        { N_("X correction"),        GWY_CC_VIEW_DISPLAY_X_CORR,   },
        { N_("Y correction"),        GWY_CC_VIEW_DISPLAY_Y_CORR,   },
        { N_("Z correction"),        GWY_CC_VIEW_DISPLAY_Z_CORR,   },
        { N_("X uncertainty"),  GWY_CC_VIEW_DISPLAY_X_UNC, },
        { N_("Y uncertainty"),  GWY_CC_VIEW_DISPLAY_Y_UNC, },
        { N_("Z uncertainty"),  GWY_CC_VIEW_DISPLAY_Z_UNC, },

    };
    return gwy_enum_combo_box_new(entries, G_N_ELEMENTS(entries),
                                  callback, cbdata, current, TRUE);
}

static GtkWidget*
menu_plane(GCallback callback, gpointer cbdata,
             GwyCCViewPlaneType current)
{
    static const GwyEnum entries[] = {
        { N_("Constant X"),        GWY_CC_VIEW_PLANE_X,   },
        { N_("Constant Y"),        GWY_CC_VIEW_PLANE_Y,   },
        { N_("Constant Z"),        GWY_CC_VIEW_PLANE_Z,   },
    };
    return gwy_enum_combo_box_new(entries, G_N_ELEMENTS(entries),
                                  callback, cbdata, current, TRUE);
}

static GtkWidget*
menu_interpolation(GCallback callback, gpointer cbdata,
             GwyCCViewInterpolationType current)
{
    static const GwyEnum entries[] = {
        { N_("NNA 3D"),         GWY_CC_VIEW_INTERPOLATION_3D,   },
        { N_("Snap to planes"), GWY_CC_VIEW_INTERPOLATION_PLANE,   },
        { N_("Delaunay"), GWY_CC_VIEW_INTERPOLATION_NATURAL,   },
    };
    return gwy_enum_combo_box_new(entries, G_N_ELEMENTS(entries),
                                  callback, cbdata, current, TRUE);
}


static void
display_changed(GtkComboBox *combo, CCViewControls *controls)
{
    controls->args->display_type = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(controls->menu_display));

    if (controls->args->crop) {
        gwy_table_hscale_set_sensitive(controls->xplane, FALSE);
        gwy_table_hscale_set_sensitive(controls->yplane, FALSE);
        gwy_table_hscale_set_sensitive(controls->zplane, FALSE);
    }
    else {
       if (controls->args->plane_type == GWY_CC_VIEW_PLANE_X) {
          gwy_table_hscale_set_sensitive(controls->xplane, TRUE);
          gwy_table_hscale_set_sensitive(controls->yplane, FALSE);
          gwy_table_hscale_set_sensitive(controls->zplane, FALSE);
       }
       else if (controls->args->plane_type == GWY_CC_VIEW_PLANE_Y) {
          gwy_table_hscale_set_sensitive(controls->xplane, FALSE);
          gwy_table_hscale_set_sensitive(controls->yplane, TRUE);
          gwy_table_hscale_set_sensitive(controls->zplane, FALSE);
       }
       else if (controls->args->plane_type == GWY_CC_VIEW_PLANE_Z) {
          gwy_table_hscale_set_sensitive(controls->xplane, FALSE);
          gwy_table_hscale_set_sensitive(controls->yplane, FALSE);
          gwy_table_hscale_set_sensitive(controls->zplane, TRUE);
       }
    }
    if (controls->args->update)
                          update_view(controls, controls->args);
}


static void
calculation_changed(GtkComboBox *combo, CCViewControls *controls)
{
    controls->args->display_type = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(controls->menu_display));
    controls->args->plane_type = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(controls->menu_plane));
    controls->args->interpolation_type = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(controls->menu_interpolation));

    if (controls->args->crop) {
        gwy_table_hscale_set_sensitive(controls->xplane, FALSE);
        gwy_table_hscale_set_sensitive(controls->yplane, FALSE);
        gwy_table_hscale_set_sensitive(controls->zplane, FALSE);
    }
    else {
       if (controls->args->plane_type == GWY_CC_VIEW_PLANE_X) {
          gwy_table_hscale_set_sensitive(controls->xplane, TRUE);
          gwy_table_hscale_set_sensitive(controls->yplane, FALSE);
          gwy_table_hscale_set_sensitive(controls->zplane, FALSE);
       }
       else if (controls->args->plane_type == GWY_CC_VIEW_PLANE_Y) {
          gwy_table_hscale_set_sensitive(controls->xplane, FALSE);
          gwy_table_hscale_set_sensitive(controls->yplane, TRUE);
          gwy_table_hscale_set_sensitive(controls->zplane, FALSE);
       }
       else if (controls->args->plane_type == GWY_CC_VIEW_PLANE_Z) {
          gwy_table_hscale_set_sensitive(controls->xplane, FALSE);
          gwy_table_hscale_set_sensitive(controls->yplane, FALSE);
          gwy_table_hscale_set_sensitive(controls->zplane, TRUE);
       }
    }
    controls->args->computed = FALSE;
    if (controls->args->update)
                          update_view(controls, controls->args);
}


static void
crop_change_cb(CCViewControls *controls)
{
    controls->args->crop
              = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->crop));

    controls->args->computed = FALSE;
    display_changed(NULL, controls);
}

static void
update_change_cb(CCViewControls *controls)
{
    controls->args->update
              = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->update));

   gtk_dialog_set_response_sensitive(GTK_DIALOG(controls->dialog),
                                            RESPONSE_PREVIEW,
                                           !controls->args->update);
   controls->args->computed = FALSE;
   if (controls->args->update)
               update_view(controls, controls->args);
}

static void         
settings_changed(CCViewControls *controls)
{
   controls->args->xplane = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->xplane));
   controls->args->yplane = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->yplane));
   controls->args->zplane = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->zplane));
   controls->args->computed = FALSE;
   if (controls->args->update)
       update_view(controls, controls->args);
}


static const gchar display_key[]  = "/module/cc_view/display";
static const gchar plane_key[]  = "/module/cc_view/plane";
static const gchar interpolation_key[]  = "/module/cc_view/interpolation";
static const gchar crop_key[]  = "/module/cc_view/crop";
static const gchar update_key[]  = "/module/cc_view/update";

static void
cc_view_sanitize_args(CCViewArgs *args)
{
    args->display_type = MIN(args->display_type, GWY_CC_VIEW_DISPLAY_Z_UNC);
    args->plane_type = MIN(args->plane_type, GWY_CC_VIEW_PLANE_Z);
    args->interpolation_type = MIN(args->interpolation_type, GWY_CC_VIEW_INTERPOLATION_PLANE);
    args->crop = !!args->crop;
    args->update = !!args->update;
}

static void
cc_view_load_args(GwyContainer *container,
                    CCViewArgs *args)
{
    gwy_container_gis_enum_by_name(container, display_key, &args->display_type);
    gwy_container_gis_enum_by_name(container, plane_key, &args->plane_type);
    gwy_container_gis_enum_by_name(container, interpolation_key, &args->interpolation_type);
    gwy_container_gis_boolean_by_name(container, crop_key, &args->crop);
    gwy_container_gis_boolean_by_name(container, update_key, &args->update);

    cc_view_sanitize_args(args);
}

static void
cc_view_save_args(GwyContainer *container,
                    CCViewArgs *args)
{
    gwy_container_set_enum_by_name(container, display_key, args->display_type);
    gwy_container_set_enum_by_name(container, plane_key, args->plane_type);
    gwy_container_set_enum_by_name(container, interpolation_key, args->interpolation_type);
    gwy_container_set_boolean_by_name(container, crop_key, args->crop);
    gwy_container_set_boolean_by_name(container, update_key, args->update);
}


/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
