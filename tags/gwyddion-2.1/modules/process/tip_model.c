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
#include <libprocess/stats.h>
#include <libprocess/tip.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwylayer-basic.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>

#define TIP_MODEL_RUN_MODES GWY_RUN_INTERACTIVE

typedef struct {
    GwyContainer *data;
    gint id;
} GwyDataObjectId;

typedef struct {
    gint nsides;
    gdouble angle;
    gdouble radius;
    gdouble theta;
    GwyTipType type;
    GwyDataObjectId object;
} TipModelArgs;

typedef struct {
    TipModelArgs *args;
    GtkWidget *view;
    GtkWidget *type;
    GtkObject *radius;
    GtkWidget *radius_spin;
    GtkWidget *radius_unit;
    GtkWidget *radius_label;
    GtkObject *angle;
    GtkObject *theta;
    GtkObject *nsides;
    GtkWidget *labsize;
    GtkObject *slope;
    GwyDataField *tip;
    GwyContainer *vtip;
    gint vxres;
    gint vyres;
    gboolean tipdone;
} TipModelControls;

static gboolean    module_register                 (void);
static void        tip_model                       (GwyContainer *data,
                                                    GwyRunType run);
static void        tip_model_dialog                (TipModelArgs *args);
static void        tip_model_dialog_update_controls(TipModelControls *controls,
                                                    TipModelArgs *args);
static void        tip_model_dialog_update_values  (TipModelControls *controls,
                                                    TipModelArgs *args);
static void        preview                         (TipModelControls *controls,
                                                    TipModelArgs *args);
static void        tip_model_do                    (TipModelArgs *args,
                                                    TipModelControls *controls);
static void        tip_process                     (TipModelArgs *args,
                                                    TipModelControls *controls);
static void        tip_model_load_args             (GwyContainer *container,
                                                    TipModelArgs *args);
static void        tip_model_save_args             (GwyContainer *container,
                                                    TipModelArgs *args);
static void        tip_model_sanitize_args         (TipModelArgs *args);
static void        sci_entry_set_value             (GtkAdjustment *adj,
                                                    GtkComboBox *metric,
                                                    gdouble val);
static void        tip_type_cb                     (GtkWidget *combo,
                                                    TipModelControls *controls);
static GtkWidget*  create_preset_menu              (GCallback callback,
                                                    gpointer cbdata,
                                                    gint current);
static void        tip_update                      (TipModelControls *controls,
                                                    TipModelArgs *args);
static void        radius_changed_cb               (gpointer object,
                                                    TipModelControls *controls);
static void        tip_model_dialog_abandon        (TipModelControls *controls);


static const TipModelArgs tip_model_defaults = {
    4, 54.73561032, 200e-9, 0, 0,
    { NULL, -1, },
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Models SPM tip."),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.3",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{

    gwy_process_func_register("tip_model",
                              (GwyProcessFunc)&tip_model,
                              N_("/_Tip/_Model Tip..."),
                              NULL,
                              TIP_MODEL_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Model AFM tip"));

    return TRUE;
}

static void
tip_model(GwyContainer *data, GwyRunType run)
{
    TipModelArgs args;

    g_return_if_fail(run & TIP_MODEL_RUN_MODES);

    tip_model_load_args(gwy_app_settings_get(), &args);
    args.object.data = data;
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD_ID, &args.object.id, 0);
    tip_model_dialog(&args);
    tip_model_save_args(gwy_app_settings_get(), &args);
}

static void
tip_model_dialog(TipModelArgs *args)
{
    enum {
        RESPONSE_RESET   = 1,
        RESPONSE_PREVIEW = 2
    };
    GtkWidget *dialog, *table, *hbox, *spin;
    TipModelControls controls;
    GwyPixmapLayer *layer;
    GwyDataField *dfield;
    GwySIUnit *unit;
    GQuark quark;
    gint response, row;

    dialog = gtk_dialog_new_with_buttons(_("Model Tip"), NULL, 0,
                                         _("_Update Preview"), RESPONSE_PREVIEW,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    hbox = gtk_hbox_new(FALSE, 3);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 4);

    controls.args = args;
    controls.vxres = 200;
    controls.vyres = 200;

    /* set up initial tip field */
    quark = gwy_app_get_data_key_for_id(args->object.id);
    dfield = GWY_DATA_FIELD(gwy_container_get_object(args->object.data, quark));

    controls.tip = gwy_data_field_new_alike(dfield, TRUE);
    gwy_data_field_resample(controls.tip, controls.vxres, controls.vyres,
                            GWY_INTERPOLATION_NONE);
    gwy_data_field_clear(controls.tip);

    /*set up data of rescaled image of the tip*/
    controls.vtip = gwy_container_new();
    gwy_app_sync_data_items(args->object.data, controls.vtip,
                            args->object.id, 0, FALSE,
                            GWY_DATA_ITEM_PALETTE, 0);

    dfield = gwy_data_field_new_alike(controls.tip, TRUE);
    gwy_container_set_object_by_name(controls.vtip, "/0/data", dfield);
    g_object_unref(dfield);

    /* set up resampled view */
    controls.view = gwy_data_view_new(controls.vtip);
    layer = gwy_layer_basic_new();
    gwy_pixmap_layer_set_data_key(layer, "/0/data");
    gwy_layer_basic_set_gradient_key(GWY_LAYER_BASIC(layer), "/0/base/palette");
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.view), layer);

    /* set up tip model controls */
    gtk_box_pack_start(GTK_BOX(hbox), controls.view, FALSE, FALSE, 4);

    table = gtk_table_new(6, 4, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(hbox), table, FALSE, FALSE, 4);
    row = 0;

    controls.type = create_preset_menu(G_CALLBACK(tip_type_cb),
                                       &controls, args->type);
    gwy_table_attach_hscale(table, row, _("Tip _type:"), NULL,
                            GTK_OBJECT(controls.type), GWY_HSCALE_WIDGET);
    row++;

    controls.nsides = gtk_adjustment_new(args->nsides, 3, 24, 1, 5, 0);
    gwy_table_attach_hscale(table, row, _("_Number of sides:"), NULL,
                            controls.nsides, 0);
    row++;

    controls.angle = gtk_adjustment_new(args->angle, 0.1, 89.9, 0.1, 1, 0);
    spin = gwy_table_attach_hscale(table, row, _("Tip _slope:"), _("deg"),
                                   controls.angle, 0);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 2);
    row++;

    controls.theta = gtk_adjustment_new(args->theta, 0, 360, 0.1, 1, 0);
    spin = gwy_table_attach_hscale(table, row, _("Tip _rotation:"), _("deg"),
                                   controls.theta, 0);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 2);
    row++;

    controls.radius = gtk_adjustment_new(1.0, 0.01, 1000.0, 0.01, 1.0, 0.0);
    controls.radius_spin = gtk_spin_button_new(GTK_ADJUSTMENT(controls.radius),
                                               0.1, 2);
    gtk_table_attach(GTK_TABLE(table), controls.radius_spin,
                     2, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    controls.radius_label = gtk_label_new_with_mnemonic(_("Tip _apex radius:"));
    gtk_misc_set_alignment(GTK_MISC(controls.radius_label), 0.0, 0.5);
    gtk_label_set_mnemonic_widget(GTK_LABEL(controls.radius_label),
                                  controls.radius_spin);
    gtk_table_attach(GTK_TABLE(table), controls.radius_label,
                     0, 1, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    unit = gwy_data_field_get_si_unit_xy(dfield);
    controls.radius_unit
        = gwy_combo_box_metric_unit_new(G_CALLBACK(radius_changed_cb),
                                        &controls,
                                        -12, -3, unit, -9);
    gtk_table_attach(GTK_TABLE(table), controls.radius_unit,
                     3, 4, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_signal_connect(controls.radius, "value-changed",
                     G_CALLBACK(radius_changed_cb), &controls);
    row++;

    controls.labsize = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(controls.labsize), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), controls.labsize,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    controls.tipdone = FALSE;
    tip_model_dialog_update_controls(&controls, args);
    preview(&controls, args);

    gtk_widget_show_all(dialog);
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            tip_model_dialog_update_values(&controls, args);
            gtk_widget_destroy(dialog);
            tip_model_dialog_abandon(&controls);
            case GTK_RESPONSE_NONE:
            return;
            break;

            case GTK_RESPONSE_OK:
            tip_model_do(args, &controls);
            break;

            case RESPONSE_RESET:
            args->nsides = tip_model_defaults.nsides;
            args->angle = tip_model_defaults.angle;
            args->radius = tip_model_defaults.radius;
            args->theta = tip_model_defaults.theta;
            args->type = tip_model_defaults.type;
            tip_model_dialog_update_controls(&controls, args);
            break;

            case RESPONSE_PREVIEW:
            tip_model_dialog_update_values(&controls, args);
            preview(&controls, args);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    tip_model_dialog_update_values(&controls, args);
    gtk_widget_destroy(dialog);
    tip_model_dialog_abandon(&controls);
}

static void
tip_model_dialog_abandon(TipModelControls *controls)
{
    /*free data of the rescaled tip image*/
    g_object_unref(controls->vtip);

    /*if dialog was cancelled, free also tip data*/
    if (!controls->tipdone)
        g_object_unref(controls->tip);
}

static void
sci_entry_set_value(GtkAdjustment *adj,
                    GtkComboBox *metric,
                    gdouble val)
{
    gint mag;

    mag = 3*(gint)floor(log10(val)/3.0);
    mag = CLAMP(mag, -12, -3);
    g_signal_handlers_block_matched(metric, G_SIGNAL_MATCH_FUNC,
                                    0, 0, 0, radius_changed_cb, 0);
    gwy_enum_combo_box_set_active(GTK_COMBO_BOX(metric), mag);
    g_signal_handlers_unblock_matched(metric, G_SIGNAL_MATCH_FUNC,
                                      0, 0, 0, radius_changed_cb, 0);
    gtk_adjustment_set_value(adj, val/pow10(mag));
}

static void
tip_type_cb(GtkWidget *combo, TipModelControls *controls)
{
    controls->args->type = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(combo));
    tip_model_dialog_update_controls(controls, controls->args);
}

static GtkWidget*
create_preset_menu(GCallback callback,
                   gpointer cbdata,
                   gint current)
{
    static GwyEnum *entries = NULL;
    static gint nentries = 0;

    if (!entries) {
        const GwyTipModelPreset *func;
        gint i;

        nentries = gwy_tip_model_get_npresets();
        entries = g_new(GwyEnum, nentries);
        for (i = 0; i < nentries; i++) {
            entries[i].value = i;
            func = gwy_tip_model_get_preset(i);
            entries[i].name = gwy_tip_model_get_preset_tip_name(func);
        }
    }

    /* XXX: presets currently not translatable? */
    return gwy_enum_combo_box_new(entries, nentries,
                                  callback, cbdata, current, FALSE);
}

static void
tip_model_dialog_update_controls(TipModelControls *controls,
                                 TipModelArgs *args)
{
    gboolean all_sensitive = FALSE, contact_sensitive = FALSE;

    gwy_enum_combo_box_set_active(GTK_COMBO_BOX(controls->type), args->type);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->nsides),
                             args->nsides);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->angle),
                             args->angle);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->theta),
                             args->theta);
    sci_entry_set_value(GTK_ADJUSTMENT(controls->radius),
                        GTK_COMBO_BOX(controls->radius_unit),
                        args->radius);
    switch (args->type) {
        case GWY_TIP_PYRAMIDE:
        all_sensitive = TRUE;
        case GWY_TIP_CONTACT:
        case GWY_TIP_NONCONTACT:
        contact_sensitive = TRUE;
        break;

        case GWY_TIP_DELTA:
        break;

        default:
        g_return_if_reached();
        break;
    }
    gwy_table_hscale_set_sensitive(controls->angle, all_sensitive);
    gwy_table_hscale_set_sensitive(controls->theta, contact_sensitive);
    gwy_table_hscale_set_sensitive(controls->nsides, all_sensitive);
    gtk_widget_set_sensitive(controls->radius_spin, contact_sensitive);
    gtk_widget_set_sensitive(controls->radius_unit, contact_sensitive);
    gtk_widget_set_sensitive(controls->radius_label, contact_sensitive);
}

static void
tip_model_dialog_update_values(TipModelControls *controls, TipModelArgs *args)
{
    args->nsides = gwy_adjustment_get_int(GTK_ADJUSTMENT(controls->nsides));
    args->angle = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->angle));
    args->theta = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->theta));
}

static void
tip_update(TipModelControls *controls,
           G_GNUC_UNUSED TipModelArgs *args)
{
   GwyDataField *vtipfield, *buffer;

   buffer = gwy_data_field_duplicate(controls->tip);
   gwy_data_field_resample(buffer, controls->vxres, controls->vyres,
                           GWY_INTERPOLATION_ROUND);

   vtipfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->vtip,
                                                               "/0/data"));
   gwy_data_field_copy(buffer, vtipfield, FALSE);
   g_object_unref(buffer);
   gwy_data_field_data_changed(vtipfield);
}


static void
preview(TipModelControls *controls,
        TipModelArgs *args)
{
    tip_process(args, controls);
    tip_update(controls, args);
}

static void
tip_model_do(TipModelArgs *args,
             TipModelControls *controls)
{
    gint newid;

    newid = gwy_app_data_browser_add_data_field(controls->tip,
                                                args->object.data,
                                                TRUE);
    g_object_unref(controls->tip);
    gwy_app_sync_data_items(args->object.data, args->object.data,
                            args->object.id, newid, FALSE,
                            GWY_DATA_ITEM_GRADIENT, 0);
    gwy_app_set_data_field_title(args->object.data, newid, _("Modelled tip"));
    controls->tipdone = TRUE;
}

static void
tip_process(TipModelArgs *args,
            TipModelControls *controls)
{
    const GwyTipModelPreset *preset;
    GwyDataField *dfield;
    GwyDataField *sfield;
    GQuark quark;
    gchar label[64];
    gint xres, yres;
    gdouble xstep, ystep, min, max, zrange;
    gdouble params[2];

    preset = gwy_tip_model_get_preset(args->type);
    g_return_if_fail(preset);

    tip_model_dialog_update_values(controls, args);

    /* estimate x and y size */
    dfield = controls->tip;
    quark = gwy_app_get_data_key_for_id(args->object.id);
    sfield = GWY_DATA_FIELD(gwy_container_get_object(args->object.data, quark));

    gwy_data_field_set_xreal(dfield,
                             gwy_data_field_get_xmeasure(sfield)
                             *gwy_data_field_get_xres(dfield));
    gwy_data_field_set_yreal(dfield,
                             gwy_data_field_get_ymeasure(sfield)
                             *gwy_data_field_get_yres(dfield));

    params[0] = args->nsides;
    params[1] = args->angle*G_PI/180;
    gwy_data_field_get_min_max(sfield, &min, &max);
    zrange = max - min;
    preset->guess(sfield, zrange, args->radius, params, &xres, &yres);

    /* process tip */
    /* FIXME: this must be solved within guess functions */
    xres = CLAMP(xres, 20, 1000);
    yres = CLAMP(yres, 20, 1000);

    g_snprintf(label, sizeof(label), _("Tip resolution: %d × %d pixels"),
               xres, yres);
    gtk_label_set_text(GTK_LABEL(controls->labsize), label);

    xstep = gwy_data_field_get_xmeasure(dfield);
    ystep = gwy_data_field_get_ymeasure(dfield);
    gwy_data_field_resample(dfield, xres, yres, GWY_INTERPOLATION_NONE);
    gwy_data_field_set_xreal(dfield, xstep*xres);
    gwy_data_field_set_yreal(dfield, ystep*yres);

    preset->func(dfield, zrange, args->radius, args->theta*G_PI/180, params);
    tip_update(controls, args);
}

static void
radius_changed_cb(G_GNUC_UNUSED gpointer object,
                  TipModelControls *controls)
{
    gint p10;
    gdouble val;

    p10 = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(controls->radius_unit));
    val = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->radius));
    controls->args->radius = val * pow10(p10);
}

static const gchar nsides_key[] = "/module/tip_model/nsides";
static const gchar angle_key[]  = "/module/tip_model/angle";
static const gchar radius_key[] = "/module/tip_model/radius";
static const gchar theta_key[]  = "/module/tip_model/theta";
static const gchar type_key[]   = "/module/tip_model/tip_type";

static void
tip_model_sanitize_args(TipModelArgs *args)
{
    args->nsides = CLAMP(args->nsides, 3, 100);
    args->type = MIN(args->type, GWY_TIP_DELTA);
}

static void
tip_model_load_args(GwyContainer *container,
               TipModelArgs *args)
{
    *args = tip_model_defaults;
    args->type = 0;

    gwy_container_gis_int32_by_name(container, nsides_key, &args->nsides);
    gwy_container_gis_double_by_name(container, angle_key, &args->angle);
    gwy_container_gis_double_by_name(container, theta_key, &args->theta);
    gwy_container_gis_double_by_name(container, radius_key, &args->radius);
    gwy_container_gis_enum_by_name(container, type_key, &args->type);

    tip_model_sanitize_args(args);
}

static void
tip_model_save_args(GwyContainer *container,
               TipModelArgs *args)
{

    gwy_container_set_int32_by_name(container, nsides_key, args->nsides);
    gwy_container_set_double_by_name(container, angle_key, args->angle);
    gwy_container_set_double_by_name(container, radius_key, args->radius);
    gwy_container_set_double_by_name(container, theta_key, args->theta);
    gwy_container_set_enum_by_name(container, type_key, args->type);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
