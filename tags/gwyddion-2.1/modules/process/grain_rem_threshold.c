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
#include <libprocess/grains.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwylayer-basic.h>
#include <libgwydgets/gwylayer-mask.h>
#include <libgwydgets/gwystock.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>

#define REMOVE_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

enum {
    PREVIEW_SIZE = 320
};

typedef struct {
    gboolean inverted;
    gdouble area;
    gdouble height;
    gboolean is_height;
    gboolean is_area;
    gboolean update;
    gboolean init;
    gboolean computed;
    GwyMergeType merge_type;
} RemoveArgs;

typedef struct {
    GtkWidget *dialog;
    GtkWidget *inverted;
    GtkWidget *view;
    GtkWidget *is_height;
    GtkWidget *is_area;
    GtkObject *threshold_height;
    GtkObject *threshold_area;
    GtkWidget *merge;
    GtkWidget *color_button;
    GtkWidget *update;
    GwyContainer *mydata;
    RemoveArgs *args;
    GwyDataField *mask;
} RemoveControls;

static gboolean    module_register               (void);
static void        remove_th                     (GwyContainer *data,
                                                  GwyRunType run);
static void        run_noninteractive            (RemoveArgs *args,
                                                  GwyContainer *data,
                                                  GwyDataField *dfield,
                                                  GwyDataField *mfield,
                                                  GQuark mquark);
static void        remove_dialog                 (RemoveArgs *args,
                                                  GwyContainer *data,
                                                  GwyDataField *dfield,
                                                  GwyDataField *mfield,
                                                  gint id,
                                                  GQuark mquark);
static void        mask_color_change_cb          (GtkWidget *color_button,
                                                  RemoveControls *controls);
static void        load_mask_color               (GtkWidget *color_button,
                                                  GwyContainer *data);
static void        remove_dialog_update_controls (RemoveControls *controls,
                                                  RemoveArgs *args);
static void        remove_dialog_update_values   (RemoveControls *controls,
                                                  RemoveArgs *args);
static void        update_change_cb              (RemoveControls *controls);
static void        remove_invalidate             (RemoveControls *controls);
static void        remove_invalidate2            (gpointer whatever,
                                                  RemoveControls *controls);
static void        preview                       (RemoveControls *controls,
                                                  RemoveArgs *args,
                                                  GwyDataField *mfield);
static void        mask_process                  (GwyDataField *dfield,
                                                  GwyDataField *maskfield,
                                                  RemoveArgs *args);
static void        intersect_removes             (GwyDataField *mask_a,
                                                  GwyDataField *mask_b,
                                                  GwyDataField *mask);
static void        remove_load_args              (GwyContainer *container,
                                                  RemoveArgs *args);
static void        remove_save_args              (GwyContainer *container,
                                                  RemoveArgs *args);
static void        remove_sanitize_args          (RemoveArgs *args);

static const RemoveArgs remove_defaults = {
    FALSE,
    50,
    50,
    TRUE,
    FALSE,
    TRUE,
    FALSE,
    FALSE,
    GWY_MERGE_UNION,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Removes grains by thresholding (height, size)."),
    "Petr Klapetek <petr@klapetek.cz>",
    "1.9",
    "David Nečas (Yeti) & Petr Klapetek",
    "2003",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("grain_rem_threshold",
                              (GwyProcessFunc)&remove_th,
                              N_("/_Grains/_Remove by Threshold..."),
                              GWY_STOCK_GRAINS_REMOVE,
                              REMOVE_RUN_MODES,
                              GWY_MENU_FLAG_DATA | GWY_MENU_FLAG_DATA_MASK,
                              N_("Remove grains by threshold"));

    return TRUE;
}

static void
remove_th(GwyContainer *data, GwyRunType run)
{
    RemoveArgs args;
    GwyDataField *dfield, *mfield;
    GQuark mquark;
    gint id;

    g_return_if_fail(run & REMOVE_RUN_MODES);
    remove_load_args(gwy_app_settings_get(), &args);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_MASK_FIELD, &mfield,
                                     GWY_APP_MASK_FIELD_KEY, &mquark,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     0);
    g_return_if_fail(dfield && mfield);

    if (run == GWY_RUN_IMMEDIATE)
        run_noninteractive(&args, data, dfield, mfield, mquark);
    else {
        remove_dialog(&args, data, dfield, mfield, id, mquark);
        remove_save_args(gwy_app_settings_get(), &args);
    }
}

static void
run_noninteractive(RemoveArgs *args,
                   GwyContainer *data,
                   GwyDataField *dfield,
                   GwyDataField *mfield,
                   GQuark mquark)
{
    gwy_app_undo_qcheckpointv(data, 1, &mquark);
    mask_process(dfield, mfield, args);
    gwy_data_field_data_changed(mfield);
}

static void
remove_dialog(RemoveArgs *args,
              GwyContainer *data,
              GwyDataField *dfield,
              GwyDataField *mfield,
              gint id,
              GQuark mquark)
{
    GtkWidget *dialog, *table, *spin, *hbox;
    RemoveControls controls;
    enum {
        RESPONSE_RESET = 1,
        RESPONSE_PREVIEW = 2
    };
    gint response;
    gdouble zoomval;
    GwyPixmapLayer *layer;
    GwyDataField *mfield2;
    gint row;
    gboolean temp;

    controls.args = args;
    controls.mask = mfield;

    dialog = gtk_dialog_new_with_buttons(_("Remove Grains by Threshold"),
                                         NULL, 0,
                                         _("_Update Preview"), RESPONSE_PREVIEW,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    controls.dialog = dialog;

    hbox = gtk_hbox_new(FALSE, 2);

    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 4);

    controls.mydata = gwy_container_new();
    gwy_container_set_object_by_name(controls.mydata, "/0/data", dfield);
    mfield2 = gwy_data_field_duplicate(mfield);
    gwy_container_set_object_by_name(controls.mydata, "/0/mask", mfield2);
    g_object_unref(mfield2);
    gwy_app_sync_data_items(data, controls.mydata, id, 0, FALSE,
                            GWY_DATA_ITEM_PALETTE,
                            GWY_DATA_ITEM_MASK_COLOR,
                            GWY_DATA_ITEM_RANGE,
                            0);
    controls.view = gwy_data_view_new(controls.mydata);
    layer = gwy_layer_basic_new();
    gwy_pixmap_layer_set_data_key(layer, "/0/data");
    gwy_layer_basic_set_gradient_key(GWY_LAYER_BASIC(layer), "/0/base/palette");
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.view), layer);
    layer = gwy_layer_mask_new();
    gwy_pixmap_layer_set_data_key(layer, "/0/mask");
    gwy_layer_mask_set_color_key(GWY_LAYER_MASK(layer), "/0/mask");
    gwy_data_view_set_alpha_layer(GWY_DATA_VIEW(controls.view), layer);
    zoomval = PREVIEW_SIZE/(gdouble)MAX(gwy_data_field_get_xres(dfield),
                                        gwy_data_field_get_yres(dfield));
    gwy_data_view_set_zoom(GWY_DATA_VIEW(controls.view), zoomval);

    gtk_box_pack_start(GTK_BOX(hbox), controls.view, FALSE, FALSE, 4);

    table = gtk_table_new(9, 4, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(hbox), table, TRUE, TRUE, 4);
    row = 0;

    gtk_table_attach(GTK_TABLE(table), gwy_label_new_header(_("Threshold by")),
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    controls.threshold_height = gtk_adjustment_new(args->height,
                                                   0.0, 100.0, 0.1, 5, 0);
    spin = gwy_table_attach_hscale(table, row, _("_Height:"), "%",
                                   controls.threshold_height, GWY_HSCALE_CHECK);
    controls.is_height = gwy_table_hscale_get_check(controls.threshold_height);
    g_signal_connect_swapped(controls.threshold_height, "value-changed",
                             G_CALLBACK(remove_invalidate), &controls);
    g_signal_connect_swapped(controls.is_height, "toggled",
                             G_CALLBACK(remove_invalidate), &controls);
    row++;

    controls.threshold_area = gtk_adjustment_new(args->area,
                                                 0.0, 16384.0, 1, 10, 0);
    spin = gwy_table_attach_hscale(table, row, _("_Area:"), "px<sup>2</sup>",
                                   controls.threshold_area,
                                   GWY_HSCALE_CHECK | GWY_HSCALE_SQRT);
    controls.is_area = gwy_table_hscale_get_check(controls.threshold_area);
    g_signal_connect_swapped(controls.threshold_area, "value-changed",
                             G_CALLBACK(remove_invalidate), &controls);
    g_signal_connect_swapped(controls.is_area, "toggled",
                             G_CALLBACK(remove_invalidate), &controls);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    gtk_table_attach(GTK_TABLE(table), gwy_label_new_header(_("Options")),
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    controls.inverted = gtk_check_button_new_with_mnemonic(_("_Invert height"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.inverted),
                                 args->inverted);
    gtk_table_attach(GTK_TABLE(table), controls.inverted,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_signal_connect_swapped(controls.inverted, "toggled",
                             G_CALLBACK(remove_invalidate), &controls);
    row++;

    controls.merge
        = gwy_enum_combo_box_new(gwy_merge_type_get_enum(), -1,
                                 G_CALLBACK(remove_invalidate2), &controls,
                                 args->merge_type, TRUE);
    gwy_table_attach_hscale(table, row, _("_Selection mode:"), NULL,
                            GTK_OBJECT(controls.merge), GWY_HSCALE_WIDGET);
    row++;

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
    row++;

    controls.update = gtk_check_button_new_with_mnemonic(_("I_nstant updates"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.update),
                                 args->update);
    gtk_table_attach(GTK_TABLE(table), controls.update,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_signal_connect_swapped(controls.update, "toggled",
                             G_CALLBACK(update_change_cb), &controls);

    remove_invalidate(&controls);

    /* cheap sync */
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.is_height),
                                 !args->is_height);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.is_height),
                                 args->is_height);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.is_area),
                                 !args->is_area);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.is_area),
                                 args->is_area);

    /* finished initializing, allow instant updates */
    args->init = TRUE;

    /* show initial preview if instant updates are on */
    if (args->update)
        preview(&controls, args, mfield);

    gtk_widget_show_all(dialog);
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            remove_dialog_update_values(&controls, args);
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            g_object_unref(controls.mydata);
            return;
            break;

            case GTK_RESPONSE_OK:
            break;

            case RESPONSE_RESET:
            temp = args->update;
            *args = remove_defaults;
            args->update = temp;
            remove_dialog_update_controls(&controls, args);
            preview(&controls, args, mfield);
            args->init = TRUE;
            break;

            case RESPONSE_PREVIEW:
            remove_dialog_update_values(&controls, args);
            preview(&controls, args, mfield);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    remove_dialog_update_values(&controls, args);
    gwy_app_sync_data_items(controls.mydata, data, 0, id, FALSE,
                            GWY_DATA_ITEM_MASK_COLOR,
                            0);
    gtk_widget_destroy(dialog);

    if (args->computed) {
        mfield = gwy_container_get_object_by_name(controls.mydata, "/0/mask");
        gwy_app_undo_qcheckpointv(data, 1, &mquark);
        gwy_container_set_object(data, mquark, mfield);
        g_object_unref(controls.mydata);
    }
    else {
        g_object_unref(controls.mydata);
        run_noninteractive(args, data, dfield, mfield, mquark);
    }
}

static void
remove_dialog_update_controls(RemoveControls *controls,
                              RemoveArgs *args)
{
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->threshold_height),
                                args->height);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->threshold_area),
                                args->area);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->inverted),
                                 args->inverted);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->is_height),
                                 args->is_height);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->is_area),
                                 args->is_area);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->update),
                                 args->update);
    gwy_enum_combo_box_set_active(GTK_COMBO_BOX(controls->merge),
                                  args->merge_type);
}

static void
remove_dialog_update_values(RemoveControls *controls,
                            RemoveArgs *args)
{
    args->is_height
        = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->is_height));
    args->is_area
        = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->is_area));
    args->inverted
        = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->inverted));
    args->height
        = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->threshold_height));
    args->area
        = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->threshold_area));
    args->update
        = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->update));
    args->merge_type
        = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(controls->merge));
}

static void
update_change_cb(RemoveControls *controls)
{
    controls->args->update
            = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->update));

    if (controls->args->update)
        remove_invalidate(controls);
}

static void
remove_invalidate(RemoveControls *controls)
{
    controls->args->computed = FALSE;

    /* create preview if instant updates are on */
    if (controls->args->update && controls->args->init) {
        remove_dialog_update_values(controls, controls->args);
        preview(controls, controls->args, controls->mask);
    }
}

static void
remove_invalidate2(G_GNUC_UNUSED gpointer whatever, RemoveControls *controls)
{
    remove_invalidate(controls);
}

static void
mask_color_change_cb(GtkWidget *color_button,
                     RemoveControls *controls)
{
    GwyContainer *data;

    data = gwy_data_view_get_data(GWY_DATA_VIEW(controls->view));
    gwy_mask_color_selector_run(NULL, GTK_WINDOW(controls->dialog),
                                GWY_COLOR_BUTTON(color_button), data,
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
preview(RemoveControls *controls,
        RemoveArgs *args,
        GwyDataField *mfield)
{
    GwyDataField *mask, *dfield;

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                             "/0/data"));
    mask = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                           "/0/mask"));
    gwy_data_field_copy(mfield, mask, FALSE);
    mask_process(dfield, mask, args);
    gwy_data_field_data_changed(mask);
    args->computed = TRUE;
}

static void
mask_process(GwyDataField *dfield,
             GwyDataField *maskfield,
             RemoveArgs *args)
{
    GwyDataField *output_field_a, *output_field_b;

    if (args->merge_type == GWY_MERGE_UNION
        || (args->is_height*args->is_area) == 0) {
        if (args->is_height)
            gwy_data_field_grains_remove_by_height(dfield, maskfield,
                                                   args->inverted
                                                   ? 100.0 - args->height
                                                   : args->height,
                                                   args->inverted);
        if (args->is_area)
            gwy_data_field_grains_remove_by_size(maskfield, args->area);
    }
    else {
        output_field_a = gwy_data_field_duplicate(maskfield);
        output_field_b = gwy_data_field_duplicate(maskfield);

        gwy_data_field_grains_remove_by_height(dfield, output_field_a,
                                               args->inverted
                                               ? 100.0 - args->height
                                               : args->height,
                                               args->inverted);
        gwy_data_field_grains_remove_by_size(output_field_b, args->area);

        intersect_removes(output_field_a, output_field_b, maskfield);

        g_object_unref(output_field_a);
        g_object_unref(output_field_b);
    }
}

/* FIXME: this is *very* inefficient, should be streamlined like other grain
 * algorithms */
static void
intersect_removes(GwyDataField *mask_a, GwyDataField *mask_b, GwyDataField *mask)
{
    gint i, xres, yres;
    const gdouble *data, *data_a, *data_b;

    xres = gwy_data_field_get_xres(mask);
    yres = gwy_data_field_get_yres(mask);
    data = gwy_data_field_get_data_const(mask);
    data_a = gwy_data_field_get_data_const(mask_a);
    data_b = gwy_data_field_get_data_const(mask_b);

    for (i = 0; i < xres*yres; i++) {
        if (data[i] > 0 && !data_a[i] && !data_b[i])
            gwy_data_field_grains_remove_grain(mask, i%xres, i/xres);
    }
}

static const gchar inverted_key[]  = "/module/grain_rem_threshold/inverted";
static const gchar isheight_key[]  = "/module/grain_rem_threshold/isheight";
static const gchar isarea_key[]    = "/module/grain_rem_threshold/isarea";
static const gchar update_key[]    = "/module/grain_rem_threshold/update";
static const gchar height_key[]    = "/module/grain_rem_threshold/height";
static const gchar area_key[]      = "/module/grain_rem_threshold/area";
static const gchar mergetype_key[] = "/module/grain_rem_threshold/mergetype";

static void
remove_sanitize_args(RemoveArgs *args)
{
    args->inverted = !!args->inverted;
    args->is_height = !!args->is_height;
    args->is_area = !!args->is_area;
    args->update = !!args->update;
    args->height = CLAMP(args->height, 0.0, 100.0);
    args->area = CLAMP(args->area, 0.0, 100.0);
    args->merge_type = MIN(args->merge_type, GWY_MERGE_INTERSECTION);
}

static void
remove_load_args(GwyContainer *container,
                 RemoveArgs *args)
{
    *args = remove_defaults;

    gwy_container_gis_boolean_by_name(container, inverted_key, &args->inverted);
    gwy_container_gis_boolean_by_name(container, isheight_key,
                                      &args->is_height);
    gwy_container_gis_boolean_by_name(container, isarea_key, &args->is_area);
    gwy_container_gis_boolean_by_name(container, update_key, &args->update);
    gwy_container_gis_double_by_name(container, height_key, &args->height);
    gwy_container_gis_double_by_name(container, area_key, &args->area);
    gwy_container_gis_enum_by_name(container, mergetype_key,
                                   &args->merge_type);
    remove_sanitize_args(args);
}

static void
remove_save_args(GwyContainer *container,
                 RemoveArgs *args)
{
    gwy_container_set_boolean_by_name(container, inverted_key, args->inverted);
    gwy_container_set_boolean_by_name(container, isheight_key, args->is_height);
    gwy_container_set_boolean_by_name(container, isarea_key, args->is_area);
    gwy_container_set_boolean_by_name(container, update_key, args->update);
    gwy_container_set_double_by_name(container, height_key, args->height);
    gwy_container_set_double_by_name(container, area_key, args->area);
    gwy_container_set_enum_by_name(container, mergetype_key, args->merge_type);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
