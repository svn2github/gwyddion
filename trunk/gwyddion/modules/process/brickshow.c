/*
 *  @(#) $Id: brickshow.c 13809 2012-09-19 18:19:21Z yeti-dn $
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "config.h"
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/arithmetic.h>
#include <libprocess/stats.h>
#include <libprocess/brick.h>
#include <libgwydgets/gwystock.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwylayer-basic.h>
#include <libgwydgets/gwylayer-mask.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>

#define BRICKSHOW_RUN_MODES (GWY_RUN_INTERACTIVE)

typedef enum {
    CUT_DIRX = 0,
    CUT_DIRY = 1,
    CUT_DIRZ = 2,
    PROJ_DIRX = 3,
    PROJ_DIRY = 4,
    PROJ_DIRZ = 5,
} GwyDirType;

enum {
    PREVIEW_SIZE = 400,
    MAX_LENGTH = 1024
};

enum {
    RESPONSE_RESET   = 1,
    RESPONSE_PREVIEW = 2,
    RESPONSE_LOAD    = 3
};

typedef struct {
    GwyDirType type;
    gdouble xpos;
    gdouble ypos;
    gdouble zpos;
    gboolean update;
    gint active_page;
} BrickshowArgs;

typedef struct {
    BrickshowArgs *args;
    GtkWidget *type;
    GtkWidget *dialog;
    GtkWidget *view;
    GtkWidget *info;
    GtkObject *xpos;
    GtkObject *ypos;
    GtkObject *zpos;
    GtkWidget *update;
    GtkWidget *drawarea;
    GwyContainer *mydata;
    GwyContainer *data;
    GwyBrick *brick;
    gboolean computed;
    gboolean in_init;
} BrickshowControls;

static gboolean module_register                    (void);
static void     brickshow                         (GwyContainer *data,
                                                    GwyRunType run);
static void     brickshow_dialog                  (BrickshowArgs *args,
                                                    GwyContainer *data,
                                                    gint id);

static void     brickshow_dialog_update_controls  (BrickshowControls *controls,
                                                    BrickshowArgs *args);
static void     brickshow_dialog_update_values    (BrickshowControls *controls,
                                                    BrickshowArgs *args);
static void     brickshow_load_data               (BrickshowControls *controls,
                                                    BrickshowArgs *args);
static void     brickshow_invalidate                   (BrickshowControls *controls);
static void     update_change_cb                   (BrickshowControls *controls);
static void     preview                            (BrickshowControls *controls,
                                                    BrickshowArgs *args);
static void     brickshow_load_args               (GwyContainer *container,
                                                    BrickshowArgs *args);
static void     brickshow_save_args               (GwyContainer *container,
                                                    BrickshowArgs *args);
static void     type_changed_cb               (GtkWidget *combo, 
                                                   BrickshowControls *controls);
static void     page_switched                      (BrickshowControls *controls,
                                                    GtkNotebookPage *page,
                                                    gint pagenum);


static const BrickshowArgs brickshow_defaults = {
    CUT_DIRX,
    50,
    50,
    50,
    TRUE,
    0,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Loads and shows a brick (3D volume data)."),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2012",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("brickshow",
                              (GwyProcessFunc)&brickshow,
                              N_("/_Bricks/Load and show..."),
                              NULL,
                              BRICKSHOW_RUN_MODES,
                              0,
                              N_("Load and show 3D data"));

    return TRUE;
}

static void
brickshow(GwyContainer *data, GwyRunType run)
{
    BrickshowArgs args;
    gint id;

    g_return_if_fail(run & BRICKSHOW_RUN_MODES);

    brickshow_load_args(gwy_app_settings_get(), &args);
   
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD_ID, &id, 0);

    brickshow_dialog(&args, data, id);
    brickshow_save_args(gwy_app_settings_get(), &args);
}


static void
brickshow_dialog(BrickshowArgs *args,
                  GwyContainer *data,
                  gint id)
{
    static const GwyEnum types[] = {
        { N_("X cross-section"), CUT_DIRX, },
        { N_("Y cross-section"), CUT_DIRY, },
        { N_("Z cross-section"), CUT_DIRZ, },
        { N_("X direction sum"), PROJ_DIRX, },
        { N_("Y direction sum"), PROJ_DIRY, },
        { N_("Z direction sum"), PROJ_DIRZ, },
    };
    GtkWidget *dialog, *table, *hbox, *label, *notebook, *msgdialog;
    GwyDataField *dfield;
    BrickshowControls controls;
    gint response;
    gdouble zoomval;
    GwyPixmapLayer *layer;
    gboolean temp;
    gint row, newid;
    gchar description[50];
    GwyContainer *newdata;

    controls.in_init = TRUE;
    controls.args = args;
    controls.data = data;
    controls.brick = NULL;
    controls.computed = FALSE; 
    controls.args->active_page = 0;

    dialog = gtk_dialog_new_with_buttons(_("Volume data"), NULL, 0, NULL);
    gtk_dialog_add_action_widget(GTK_DIALOG(dialog),
                                 gwy_stock_like_button_new(_("_Load"),
                                                           GTK_STOCK_OPEN),
                                 RESPONSE_LOAD);
    gtk_dialog_add_action_widget(GTK_DIALOG(dialog),
                                 gwy_stock_like_button_new(_("_Update"),
                                                           GTK_STOCK_EXECUTE),
                                 RESPONSE_PREVIEW);
    gtk_dialog_add_button(GTK_DIALOG(dialog), _("_Reset"), RESPONSE_RESET);
    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);

    gtk_dialog_add_action_widget(GTK_DIALOG(dialog),
                          gwy_stock_like_button_new(_("_Extract projection"), 
                                                    GTK_STOCK_OK), 
                          GTK_RESPONSE_OK);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_CANCEL);
    controls.dialog = dialog;


    notebook = gtk_notebook_new();
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), notebook, 
                       TRUE, TRUE, 4);
    g_signal_connect_swapped(notebook, "switch-page",
                                 G_CALLBACK(page_switched), &controls);


    /////////////////////////////  projections page ///////////////////////////////////////

    hbox = gtk_hbox_new(FALSE, 2);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
                               hbox,
                               gtk_label_new(_("Projections")));

    controls.mydata = gwy_container_new();
    dfield = gwy_data_field_new(PREVIEW_SIZE, PREVIEW_SIZE, 
                                PREVIEW_SIZE, PREVIEW_SIZE, 
                                TRUE);
    gwy_container_set_object_by_name(controls.mydata, "/0/data", dfield);

    if (data) gwy_app_sync_data_items(data, controls.mydata, id, 0, FALSE,
                            GWY_DATA_ITEM_PALETTE,
                            0);

    
    controls.view = gwy_data_view_new(controls.mydata);
    layer = gwy_layer_basic_new();
    g_object_set(layer,
                 "data-key", "/0/data",
                 "gradient-key", "/0/base/palette",
                 "range-type-key", "/0/base/range-type",
                 "min-max-key", "/0/base",
                 NULL);
    gwy_data_view_set_data_prefix(GWY_DATA_VIEW(controls.view), "/0/data");
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.view), layer);
    zoomval = PREVIEW_SIZE/(gdouble)MAX(gwy_data_field_get_xres(dfield),
                                        gwy_data_field_get_yres(dfield));
    gwy_data_view_set_zoom(GWY_DATA_VIEW(controls.view), zoomval);

    gtk_box_pack_start(GTK_BOX(hbox), controls.view, FALSE, FALSE, 4);

    table = gtk_table_new(11, 4, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(hbox), table, TRUE, TRUE, 4);
    row = 0;

    controls.xpos = gtk_adjustment_new(args->xpos,
                                            0.0, 100, 1, 10, 0);
    gwy_table_attach_hscale(table, row++, _("X position"), "%",
                            controls.xpos, 0);
    g_signal_connect_swapped(controls.xpos, "value-changed",
                             G_CALLBACK(brickshow_invalidate), &controls);

    controls.ypos = gtk_adjustment_new(args->ypos,
                                            0.0, 100, 1, 10, 0);
    gwy_table_attach_hscale(table, row++, _("Y position"), "%",
                            controls.ypos, 0);
    g_signal_connect_swapped(controls.ypos, "value-changed",
                             G_CALLBACK(brickshow_invalidate), &controls);

    controls.zpos = gtk_adjustment_new(args->zpos,
                                            0.0, 100, 1, 10, 0);
    gwy_table_attach_hscale(table, row++, _("Z position"), "%",
                            controls.zpos, 0);
    g_signal_connect_swapped(controls.zpos, "value-changed",
                             G_CALLBACK(brickshow_invalidate), &controls);


    label = gtk_label_new(_("Shown cut direction:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 4, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    controls.type
        = gwy_enum_combo_box_new(types, G_N_ELEMENTS(types),
                                 G_CALLBACK(type_changed_cb), &controls,
                                 args->type, TRUE);
    gwy_table_attach_hscale(table, row, _("Show mode:"), NULL,
                            GTK_OBJECT(controls.type), GWY_HSCALE_WIDGET);
    type_changed_cb(controls.type, &controls);
    row++;


    controls.update = gtk_check_button_new_with_mnemonic(_("I_nstant updates"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.update),
                                 args->update);
    gtk_table_attach(GTK_TABLE(table), controls.update,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_signal_connect_swapped(controls.update, "toggled",
                             G_CALLBACK(update_change_cb), &controls);
    row++;

    controls.info = gtk_label_new(_("No data loaded"));
    gtk_misc_set_alignment(GTK_MISC(controls.info), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), controls.info,
                     0, 4, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;


    /////////////////////////////  projections page ///////////////////////////////////////

    hbox = gtk_hbox_new(FALSE, 2);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
                               hbox,
                               gtk_label_new(_("3D view")));

    /*
    controls.drawarea = gtk_label_new("Here comes the widget");
    gtk_box_pack_start(GTK_BOX(hbox), controls.drawarea, FALSE, FALSE, 4);
    gtk_widget_set_size_request(controls.drawarea, PREVIEW_SIZE, PREVIEW_SIZE);


    table = gtk_table_new(11, 4, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(hbox), table, TRUE, TRUE, 4);
    row = 0;
    */
    

    brickshow_invalidate(&controls);
    controls.in_init = FALSE;

    /* show initial preview if instant updates are on */
    if (args->update) {
        gtk_dialog_set_response_sensitive(GTK_DIALOG(controls.dialog),
                                          RESPONSE_PREVIEW, FALSE);
        preview(&controls, args);
    }

    gtk_widget_show_all(dialog);
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            brickshow_dialog_update_values(&controls, args);
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            g_object_unref(controls.mydata);
            return;
            break;

            case GTK_RESPONSE_OK:

            break;

            case RESPONSE_LOAD:
            brickshow_load_data(&controls, args);
            break;

            case RESPONSE_RESET:
            temp = args->update;
            *args = brickshow_defaults;
            args->update = temp;
            controls.in_init = TRUE;
            brickshow_dialog_update_controls(&controls, args);
            controls.in_init = FALSE;
            preview(&controls, args);
            break;

            case RESPONSE_PREVIEW:
            brickshow_dialog_update_values(&controls, args);
            preview(&controls, args);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    brickshow_dialog_update_values(&controls, args);

    if (response == GTK_RESPONSE_OK)
    {
        if (!controls.computed) {
            preview(&controls, args);
        }

        if (controls.computed) {
            dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls.mydata,
                                                                     "/0/data"));


            if (args->type == CUT_DIRX) 
                g_snprintf(description, sizeof(description), _("X cross-section at xpos %d"), 
                           (gint)(args->xpos/100.0*(gwy_brick_get_xres(controls.brick)-1))); 
            else if (args->type == CUT_DIRY) 
                g_snprintf(description, sizeof(description), _("Y cross-section at ypos %d"), 
                           (gint)(args->ypos/100.0*(gwy_brick_get_yres(controls.brick)-1))); 
            else if (args->type == CUT_DIRZ) 
                g_snprintf(description, sizeof(description), _("Z cross-section at zpos %d"), 
                           (gint)(args->zpos/100.0*(gwy_brick_get_zres(controls.brick)-1))); 
            else if (args->type == PROJ_DIRX) 
                g_snprintf(description, sizeof(description), _("X direction sum")); 
            else if (args->type == PROJ_DIRY) 
                g_snprintf(description, sizeof(description), _("Y direction sum")); 
            else if (args->type == PROJ_DIRZ) 
                g_snprintf(description, sizeof(description), _("Z direction sum")); 

            if (data) {

                newid = gwy_app_data_browser_add_data_field(dfield, data, TRUE);
                gwy_app_sync_data_items(data, data, id, newid, FALSE,
                                        GWY_DATA_ITEM_GRADIENT,
                                        0);

                gwy_app_set_data_field_title(data, newid, description);

            }
            else { 
                newid = 0;
                newdata = gwy_container_new();
                gwy_container_set_object(newdata, gwy_app_get_data_key_for_id(newid),
                                         dfield);
                gwy_app_data_browser_add(newdata);
                gwy_app_data_browser_reset_visibility(newdata,
                                                      GWY_VISIBILITY_RESET_SHOW_ALL);
                g_object_unref(newdata);
                gwy_app_set_data_field_title(newdata, newid, description);

            }
        }

    }

    gtk_widget_destroy(dialog);

    g_object_unref(controls.mydata);
}

static void
page_switched(BrickshowControls *controls,
              G_GNUC_UNUSED GtkNotebookPage *page,
              gint pagenum)
{
    if (controls->in_init)
        return;

    controls->args->active_page = pagenum;

}


static void
type_changed_cb(GtkWidget *combo, BrickshowControls *controls)
{
    controls->args->type = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(combo));
    switch (controls->args->type) {
        case CUT_DIRX:
        gwy_table_hscale_set_sensitive(controls->xpos, TRUE);
        gwy_table_hscale_set_sensitive(controls->ypos, FALSE);
        gwy_table_hscale_set_sensitive(controls->zpos, FALSE);
        break;

        case CUT_DIRY:
        gwy_table_hscale_set_sensitive(controls->xpos, FALSE);
        gwy_table_hscale_set_sensitive(controls->ypos, TRUE);
        gwy_table_hscale_set_sensitive(controls->zpos, FALSE);
        break;

        case CUT_DIRZ:
        gwy_table_hscale_set_sensitive(controls->xpos, FALSE);
        gwy_table_hscale_set_sensitive(controls->ypos, FALSE);
        gwy_table_hscale_set_sensitive(controls->zpos, TRUE);
        break;

        default:
        gwy_table_hscale_set_sensitive(controls->xpos, FALSE);
        gwy_table_hscale_set_sensitive(controls->ypos, FALSE);
        gwy_table_hscale_set_sensitive(controls->zpos, FALSE);
        break;
    }
    brickshow_invalidate(controls);

}

static void
brickshow_dialog_update_controls(BrickshowControls *controls,
                                  BrickshowArgs *args)
{
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->xpos),
                             args->xpos);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->ypos),
                             args->ypos);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->zpos),
                             args->zpos);
    gwy_enum_combo_box_set_active(GTK_COMBO_BOX(controls->type), args->type);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->update),
                                 args->update);
}



static void
brickshow_dialog_update_values(BrickshowControls *controls,
                                BrickshowArgs *args)
{
    args->xpos
        = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->xpos));
    args->ypos
        = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->ypos));
    args->zpos
        = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->zpos));

    args->update
        = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->update));
}

static void
brickshow_invalidate(BrickshowControls *controls)
{
    controls->computed = FALSE;

    /* create preview if instant updates are on */
    if (controls->args->update && !controls->in_init) {
        brickshow_dialog_update_values(controls, controls->args);
        preview(controls, controls->args);
    }
}

static void
update_change_cb(BrickshowControls *controls)
{
    controls->args->update
            = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->update));

    gtk_dialog_set_response_sensitive(GTK_DIALOG(controls->dialog),
                                      RESPONSE_PREVIEW,
                                      !controls->args->update);

    if (controls->args->update)
        brickshow_invalidate(controls);
}

static void
brickshow_load_data(BrickshowControls *controls,
        BrickshowArgs *args)
{
    GtkWidget *dialog, *msgdialog;
    gchar *filename;
    gchar *line, *text = NULL;
    gsize size;
    gint xres, yres, zres, col, row, lev;
    gdouble *data;
    GError *err = NULL;


    dialog = gtk_file_chooser_dialog_new (N_("Load volume data"),
                                          GTK_WINDOW(controls->dialog),
                                          GTK_FILE_CHOOSER_ACTION_OPEN,
                                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                          GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                                          NULL);

    if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
    {
        filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
        if (!g_file_get_contents(filename, &text, &size, &err)) {
            msgdialog = gtk_message_dialog_new (GTK_WINDOW(dialog),
                                                GTK_DIALOG_DESTROY_WITH_PARENT,
                                                GTK_MESSAGE_ERROR,
                                                GTK_BUTTONS_CLOSE,
                                                N_("Error loading file '%s'"),
                                                filename);
            gtk_dialog_run(GTK_DIALOG(msgdialog));
            gtk_widget_destroy(msgdialog);
            gtk_widget_destroy(dialog);
        } else {
            //printf("load file: %s\n", filename);

            line = gwy_str_next_line(&text);
            g_strstrip(line);
            xres = g_ascii_strtod(line, &line);
            yres = g_ascii_strtod(line, &line);
            zres = g_ascii_strtod(line, &line);

            //printf("brick %d x %d x %d appears to be there\n", xres, yres, zres);

            controls->brick = gwy_brick_new(xres, yres, zres, xres, yres, zres, FALSE);
            data = gwy_brick_get_data(controls->brick);

            line = gwy_str_next_line(&text);
            for (col=0; col<xres; col++)
            {
                for (row=0; row<yres; row++)
                {
                    for (lev=0; lev<zres; lev++) {
                        data[col + row*xres + lev*xres*yres] = g_ascii_strtod(line, &line);
                    }
                }
            }

            //printf("brick loaded\n");
            //g_free(text); FIXME: something is wrong - this leads to segfault

            gtk_widget_destroy(dialog);
            brickshow_invalidate(controls);
        }
        g_free(filename);
    } else {
      gtk_widget_destroy(dialog);

    }


}


static void
preview(BrickshowControls *controls,
        BrickshowArgs *args)
{
    GwyDataField  *dfield;
    gchar message[50];

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                             "/0/data"));

    if (!controls->brick) return;

    if (args->active_page == 0) {

        if (args->type == CUT_DIRX) {
            gwy_brick_extract_plane(controls->brick, dfield,
                                    (gint)(args->xpos/100.0*(gwy_brick_get_xres(controls->brick)-1)), 
                                    0, 
                                    0,
                                    -1,
                                    gwy_brick_get_yres(controls->brick), 
                                    gwy_brick_get_zres(controls->brick), 
                                    0);
        } else if (args->type == CUT_DIRY) {
            gwy_brick_extract_plane(controls->brick, dfield,
                                    0, 
                                    (gint)(args->ypos/100.0*(gwy_brick_get_yres(controls->brick)-1)), 
                                    0,
                                    gwy_brick_get_xres(controls->brick),
                                    -1, 
                                    gwy_brick_get_zres(controls->brick), 
                                    0);
        } else if (args->type == CUT_DIRZ) {
            gwy_brick_extract_plane(controls->brick, dfield,
                                    0, 
                                    0, 
                                    (gint)(args->zpos/100.0*(gwy_brick_get_zres(controls->brick)-1)), 
                                    gwy_brick_get_xres(controls->brick), 
                                    gwy_brick_get_yres(controls->brick), 
                                    -1, 
                                    0);
        } else if (args->type == PROJ_DIRX) {
            gwy_brick_sum_plane(controls->brick, dfield,
                                0, 
                                0, 
                                0,
                                -1,
                                gwy_brick_get_yres(controls->brick), 
                                gwy_brick_get_zres(controls->brick), 
                                0);
        } else if (args->type == PROJ_DIRY) {
            gwy_brick_sum_plane(controls->brick, dfield,
                                0, 
                                0, 
                                0,
                                gwy_brick_get_xres(controls->brick),
                                -1, 
                                gwy_brick_get_zres(controls->brick), 
                                0);
        } else if (args->type == PROJ_DIRZ) {
            gwy_brick_sum_plane(controls->brick, dfield,
                                0, 
                                0, 
                                0, 
                                gwy_brick_get_xres(controls->brick), 
                                gwy_brick_get_yres(controls->brick), 
                                -1, 
                                0);
        }



        g_snprintf(message, sizeof(message), "Shown range %g to %g", 
                   gwy_data_field_get_min(dfield), 
                   gwy_data_field_get_max(dfield));
        gtk_label_set_text(GTK_LABEL(controls->info), message);

        gwy_data_field_data_changed(dfield);


        controls->computed = TRUE;
    } else if (args->active_page == 1)
    {
        //printf("3D view updated\n");
    }
}

static const gchar xpos_key[]       = "/module/brickshow/xpos";
static const gchar ypos_key[]       = "/module/brickshow/ypos";
static const gchar zpos_key[]       = "/module/brickshow/zpos";
static const gchar type_key[]    = "/module/brickshow/dirtype";
static const gchar update_key[] = "/module/brickshow/update";

static void
brickshow_sanitize_args(BrickshowArgs *args)
{
    args->xpos = CLAMP(args->xpos, 0, 100);
    args->ypos = CLAMP(args->ypos, 0, 100);
    args->zpos = CLAMP(args->zpos, 0, 100);
    args->type = MIN(args->type, PROJ_DIRZ);
    args->update = !!args->update;
}

static void
brickshow_load_args(GwyContainer *container,
                     BrickshowArgs *args)
{
    *args = brickshow_defaults;

    gwy_container_gis_enum_by_name(container, type_key, &args->type);
    gwy_container_gis_double_by_name(container, xpos_key, &args->xpos);
    gwy_container_gis_double_by_name(container, ypos_key, &args->ypos);
    gwy_container_gis_double_by_name(container, zpos_key, &args->zpos);
    gwy_container_gis_boolean_by_name(container, update_key, &args->update);
    brickshow_sanitize_args(args);
}

static void
brickshow_save_args(GwyContainer *container,
                     BrickshowArgs *args)
{
    gwy_container_set_enum_by_name(container, type_key, args->type);
    gwy_container_set_double_by_name(container, xpos_key, args->xpos);
    gwy_container_set_double_by_name(container, ypos_key, args->ypos);
    gwy_container_set_double_by_name(container, zpos_key, args->zpos);
    gwy_container_set_boolean_by_name(container, update_key, args->update);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
