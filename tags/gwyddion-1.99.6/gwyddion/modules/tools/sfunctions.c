/*
 *  @(#) $Id$
 *  Copyright (C) 2003,2004 David Necas (Yeti), Petr Klapetek.
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
#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwycontainer.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/linestats.h>
#include <libprocess/stats.h>
#include <libgwydgets/gwydgets.h>
#include <app/settings.h>
#include <app/app.h>
#include <app/data-browser.h>
#include <app/unitool.h>

#define CHECK_LAYER_TYPE(l) \
    (G_TYPE_CHECK_INSTANCE_TYPE((l), func_slots.layer_type))

typedef enum {
    GWY_SF_DH                     = 0,
    GWY_SF_CDH                    = 1,
    GWY_SF_DA                     = 2,
    GWY_SF_CDA                    = 3,
    GWY_SF_ACF                    = 4,
    GWY_SF_HHCF                   = 5,
    GWY_SF_PSDF                   = 6,
    GWY_SF_MINKOWSKI_VOLUME       = 7,
    GWY_SF_MINKOWSKI_BOUNDARY     = 8,
    GWY_SF_MINKOWSKI_CONNECTIVITY = 9,
} GwySFOutputType;

typedef struct {
    GwyUnitoolState *state;
    GwyUnitoolRectLabels labels;
    GtkWidget *graph;
    GtkWidget *interpolation;
    GtkWidget *output;
    GtkWidget *direction;
    GtkWidget *isnofpoints;
    GtkObject *size;
    GwyInterpolationType interp;
    GwySFOutputType out;
    GwyOrientation dir;
    gint siz;
    GwyGraphModel *graphmodel;
    gboolean isnpoints;
} ToolControls;

static gboolean   module_register      (void);
static gboolean   use                  (GwyDataWindow *data_window,
                                        GwyToolSwitchEvent reason);
static void       layer_setup          (GwyUnitoolState *state);
static GtkWidget* dialog_create        (GwyUnitoolState *state);
static void       dialog_update        (GwyUnitoolState *state,
                                        GwyUnitoolUpdateType reason);
static void       dialog_abandon       (GwyUnitoolState *state);
static void       apply                (GwyUnitoolState *state);
static void       interp_changed_cb    (GtkWidget *combo,
                                        ToolControls *controls);
static void       output_changed_cb    (GtkWidget *combo,
                                        ToolControls *controls);
static void       direction_changed_cb (GtkWidget *combo,
                                        ToolControls *controls);
static void       size_changed_cb      (GObject *adjustment,
                                        ToolControls *controls);
static void      isnofpoints_changed_cb(GtkToggleButton *button,
                                        ToolControls *controls);
static void       load_args            (GwyContainer *container,
                                        ToolControls *controls);
static void       save_args            (GwyContainer *container,
                                        ToolControls *controls);


static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Statistical function tool, calculates one-dimensional statistical "
       "functions (height distribution, correlations, PSDF, Minkowski "
       "functionals) of selected part of data."),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.5",
    "David Nečas (Yeti) & Petr Klapetek",
    "2003",
};

static GwyUnitoolSlots func_slots = {
    0,                             /* layer type, must be set runtime */
    layer_setup,                   /* layer setup func */
    dialog_create,                 /* dialog constructor */
    dialog_update,                 /* update view and controls */
    dialog_abandon,                /* dialog abandon hook */
    apply,                         /* apply action */
    NULL,                          /* nonstandard response handler */
};

static const GwyEnum sf_types[] =  {
    { N_("Height distribution"),         GWY_SF_DH,                     },
    { N_("Cum. height distribution"),    GWY_SF_CDH,                    },
    { N_("Distribution of angles"),      GWY_SF_DA,                     },
    { N_("Cum. distribution of angles"), GWY_SF_CDA,                    },
    { N_("ACF"),                         GWY_SF_ACF,                    },
    { N_("HHCF"),                        GWY_SF_HHCF,                   },
    { N_("PSDF"),                        GWY_SF_PSDF,                   },
    { N_("Minkowski volume"),            GWY_SF_MINKOWSKI_VOLUME,       },
    { N_("Minkowski boundary"),          GWY_SF_MINKOWSKI_BOUNDARY,     },
    { N_("Minkowski connectivity"),      GWY_SF_MINKOWSKI_CONNECTIVITY, },
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    static GwyToolFuncInfo func_info = {
        "sfunctions",
        GWY_STOCK_GRAPH_HALFGAUSS,
        N_("Compute 1D statistical functions."),
        use,
    };

    gwy_tool_func_register(&func_info);

    return TRUE;
}

static gboolean
use(GwyDataWindow *data_window,
    GwyToolSwitchEvent reason)
{
    static const gchar *layer_name = "GwyLayerRectangle";
    static GwyUnitoolState *state = NULL;

    if (!state) {
        func_slots.layer_type = g_type_from_name(layer_name);
        if (!func_slots.layer_type) {
            g_warning("Layer type `%s' not available", layer_name);
            return FALSE;
        }
        state = g_new0(GwyUnitoolState, 1);
        state->func_slots = &func_slots;
        state->user_data = g_new0(ToolControls, 1);
    }
    ((ToolControls*)state->user_data)->state = state;
    return gwy_unitool_use(state, data_window, reason);
}

static void
layer_setup(GwyUnitoolState *state)
{
    g_assert(CHECK_LAYER_TYPE(state->layer));
    g_object_set(state->layer,
                 "selection-key", "/0/select/rectangle",
                 "is-crop", FALSE,
                 NULL);
}

static GtkWidget*
dialog_create(GwyUnitoolState *state)
{
    ToolControls *controls;
    GwyContainer *settings;
    GtkWidget *dialog, *spin, *table, *label, *vbox, *frame, *hbox;
    gint row;

    gwy_debug("");
    controls = (ToolControls*)state->user_data;
    settings = gwy_app_settings_get();
    load_args(settings, controls);

    dialog = gtk_dialog_new_with_buttons(_("Statistical Functions"),
                                         NULL, 0, NULL);
    gwy_unitool_dialog_add_button_clear(dialog);
    gwy_unitool_dialog_add_button_hide(dialog);
    gwy_unitool_dialog_add_button_apply(dialog);

    hbox = gtk_hbox_new(FALSE, 2);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), hbox);

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 0);

    frame = gwy_unitool_windowname_frame_create(state);
    gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);

    table = gtk_table_new(6, 3, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_table_set_col_spacings(GTK_TABLE(table), 4);
    gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);
    row = 0;

    row += gwy_unitool_rect_info_table_setup(&controls->labels,
                                             GTK_TABLE(table), 0, row);
    controls->labels.unselected_is_full = TRUE;
    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Options</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 3, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);
    row++;

    label = gtk_label_new_with_mnemonic(_("_Output type:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 3, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);
    row++;

    controls->output
        = gwy_enum_combo_box_new(sf_types, G_N_ELEMENTS(sf_types),
                                 G_CALLBACK(output_changed_cb), controls,
                                 controls->out, TRUE);
    gtk_table_attach(GTK_TABLE(table), controls->output, 0, 3, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 4);
    row++;

    label = gtk_label_new_with_mnemonic(_("Computation _direction:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 3, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);
    row++;

    controls->direction
        = gwy_enum_combo_box_new(gwy_orientation_get_enum(), -1,
                                 G_CALLBACK(direction_changed_cb), controls,
                                 controls->dir, TRUE);
    gtk_table_attach(GTK_TABLE(table), controls->direction, 0, 3, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 4);
    row++;

    controls->size = gtk_adjustment_new(controls->siz, 20, 1000, 1, 10, 0);

    spin = gwy_table_attach_hscale(table, row, _("Fix res.:"), NULL,
                                   GTK_OBJECT(controls->size),
                                   GWY_HSCALE_CHECK);
    g_signal_connect(controls->size, "value_changed",
                     G_CALLBACK(size_changed_cb), controls);
    controls->isnofpoints = gwy_table_hscale_get_check(controls->size);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->isnofpoints),
                                 controls->isnpoints);
    g_signal_connect(controls->isnofpoints, "toggled",
                     G_CALLBACK(isnofpoints_changed_cb), controls);
    gwy_table_hscale_set_sensitive(controls->size, controls->isnpoints);
    row++;

    label = gtk_label_new_with_mnemonic(_("Interpolation type:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 3, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);
    row++;

    controls->interpolation
        = gwy_enum_combo_box_new(gwy_interpolation_type_get_enum(), -1,
                                 G_CALLBACK(interp_changed_cb), controls,
                                 controls->interp, TRUE);
    gtk_table_attach(GTK_TABLE(table), controls->interpolation,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 2, 2);
    row++;

    controls->graphmodel = gwy_graph_model_new();
    controls->graph = gwy_graph_new(controls->graphmodel);
    gtk_widget_set_size_request(controls->graph, 400, 150);
    gwy_graph_enable_user_input(GWY_GRAPH(controls->graph), FALSE);
    gtk_box_pack_start(GTK_BOX(hbox), controls->graph, FALSE, FALSE, 0);

    return dialog;
}

static void
dialog_update(GwyUnitoolState *state,
              G_GNUC_UNUSED GwyUnitoolUpdateType reason)
{
    ToolControls *controls;
    GwyContainer *data;
    GwyGraphCurveModel *gcmodel;
    GwyDataField *dfield;
    GwyDataLine *dataline;
    GwyDataViewLayer *layer;
    gint isel[4], w, h, res;
    GString *lab;

    gwy_debug("");

    /* XXX: Does not work.  We don't need to update when the dialog is
     * visible, but we must update when it *becomes visible again* then
    if (!state->is_visible)
        return;
    */

    controls = (ToolControls*)state->user_data;
    gtk_widget_set_sensitive(controls->direction,
                             controls->out == GWY_SF_DA
                             || controls->out == GWY_SF_CDA
                             || controls->out == GWY_SF_ACF
                             || controls->out == GWY_SF_HHCF
                             || controls->out == GWY_SF_PSDF);

    layer = GWY_DATA_VIEW_LAYER(state->layer);
    data = gwy_data_view_get_data(GWY_DATA_VIEW(layer->parent));
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));

    gwy_unitool_rect_info_table_fill(state, &controls->labels, NULL, isel);


    gwy_graph_model_remove_all_curves(controls->graphmodel);
    gcmodel = gwy_graph_curve_model_new();
    gwy_graph_curve_model_set_curve_type(gcmodel, GWY_GRAPH_CURVE_LINE);

    dataline = gwy_data_line_new(10, 10, FALSE);
    lab = g_string_new(gwy_enum_to_string(controls->out,
                                          sf_types, G_N_ELEMENTS(sf_types)));

    w = isel[2] - isel[0];
    h = isel[3] - isel[1];
    if (w >= 4 && h >= 4) {
        res = controls->isnpoints ? controls->siz : -1;
        switch (controls->out) {
            case GWY_SF_DH:
            gwy_data_field_area_dh(dfield, dataline, isel[0], isel[1], w, h,
                                   res);
            break;

            case GWY_SF_CDH:
            gwy_data_field_area_cdh(dfield, dataline, isel[0], isel[1], w, h,
                                    res);
            break;

            case GWY_SF_DA:
            gwy_data_field_area_da(dfield, dataline, isel[0], isel[1], w, h,
                                   controls->dir, res);
            break;

            case GWY_SF_CDA:
            gwy_data_field_area_cda(dfield, dataline, isel[0], isel[1], w, h,
                                    controls->dir, res);
            break;

            case GWY_SF_ACF:
            gwy_data_field_area_acf(dfield, dataline, isel[0], isel[1], w, h,
                                    controls->dir, controls->interp, res);
            break;

            case GWY_SF_HHCF:
            gwy_data_field_area_hhcf(dfield, dataline, isel[0], isel[1], w, h,
                                     controls->dir, controls->interp, res);
            break;

            case GWY_SF_PSDF:
            gwy_data_field_area_psdf(dfield, dataline, isel[0], isel[1], w, h,
                                     controls->dir, controls->interp,
                                     GWY_WINDOWING_HANN, res);
            break;

            case GWY_SF_MINKOWSKI_VOLUME:
            gwy_data_field_area_minkowski_volume(dfield, dataline,
                                                 isel[0], isel[1], w, h,
                                                 res);
            break;

            case GWY_SF_MINKOWSKI_BOUNDARY:
            gwy_data_field_area_minkowski_boundary(dfield, dataline,
                                                   isel[0], isel[1], w, h,
                                                   res);
            break;

            case GWY_SF_MINKOWSKI_CONNECTIVITY:
            gwy_data_field_area_minkowski_euler(dfield, dataline,
                                                isel[0], isel[1], w, h,
                                                res);
            break;
        }

        gwy_graph_curve_model_set_data_from_dataline(gcmodel, dataline, 0, 0);
        gwy_graph_curve_model_set_description(gcmodel, lab->str);
        gwy_graph_model_add_curve(controls->graphmodel, gcmodel);
        g_object_unref(gcmodel);
        gwy_graph_model_set_title(controls->graphmodel,
                                  gettext(sf_types[controls->out].name));
        gwy_graph_model_set_units_from_data_line(controls->graphmodel,
                                                 dataline);
    }

    g_string_free(lab, TRUE);
    g_object_unref(dataline);
}

static void
apply(GwyUnitoolState *state)
{

    ToolControls *controls;
    GtkWidget *graph;
    GwyGraphModel *gmodel;
    GwyDataViewLayer *layer;
    GwyContainer *data;

    controls = (ToolControls*)state->user_data;
    gmodel = gwy_graph_model_duplicate(controls->graphmodel);

    layer = GWY_DATA_VIEW_LAYER(state->layer);
    data = gwy_data_view_get_data(GWY_DATA_VIEW(layer->parent));
        
    gwy_app_data_browser_add_graph_model(gmodel, data, TRUE);
    g_object_unref(gmodel);
}

static void
dialog_abandon(GwyUnitoolState *state)
{
    ToolControls *controls;
    GwyContainer *settings;

    controls = (ToolControls*)state->user_data;
    settings = gwy_app_settings_get();
    save_args(settings, controls);
    g_object_unref(controls->graphmodel);

    memset(state->user_data, 0, sizeof(ToolControls));
}

static void
interp_changed_cb(GtkWidget *combo, ToolControls *controls)
{
    gwy_debug("");
    controls->interp = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(combo));
    dialog_update(controls->state, GWY_UNITOOL_UPDATED_CONTROLS);
}

static void
output_changed_cb(GtkWidget *combo, ToolControls *controls)
{
    gwy_debug("");
    controls->out = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(combo));
    dialog_update(controls->state, GWY_UNITOOL_UPDATED_CONTROLS);
}

static void
direction_changed_cb(GtkWidget *combo, ToolControls *controls)
{
    gwy_debug("");
    controls->dir = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(combo));
    dialog_update(controls->state, GWY_UNITOOL_UPDATED_CONTROLS);
}

static void
size_changed_cb(GObject *adjustment, ToolControls *controls)
{
    controls->siz = gtk_adjustment_get_value(GTK_ADJUSTMENT(adjustment));
    dialog_update(controls->state, GWY_UNITOOL_UPDATED_CONTROLS);
}

static void
isnofpoints_changed_cb(GtkToggleButton *button, ToolControls *controls)
{
    controls->isnpoints = gtk_toggle_button_get_active(button);
    dialog_update(controls->state, GWY_UNITOOL_UPDATED_CONTROLS);
}


static const gchar interp_key[]    = "/tool/sfunctions/interp";
static const gchar out_key[]       = "/tool/sfunctions/out";
static const gchar dir_key[]       = "/tool/sfunctions/dir";
static const gchar siz_key[]       = "/tool/sfunctions/siz";
static const gchar isnpoints_key[] = "/tool/sfunctions/isnpoints";

static void
load_args(GwyContainer *container, ToolControls *controls)
{
    controls->dir = GTK_ORIENTATION_HORIZONTAL;
    controls->out = GWY_SF_DH;
    controls->interp = GWY_INTERPOLATION_BILINEAR;
    controls->siz = 100;
    controls->isnpoints = FALSE;

    gwy_container_gis_boolean_by_name(container, isnpoints_key,
                                      &controls->isnpoints);
    gwy_container_gis_enum_by_name(container, dir_key, &controls->dir);
    gwy_container_gis_enum_by_name(container, out_key, &controls->out);
    gwy_container_gis_enum_by_name(container, interp_key, &controls->interp);
    gwy_container_gis_int32_by_name(container, siz_key, &controls->siz);

    /* sanitize */
    controls->isnpoints = !!controls->isnpoints;
    controls->dir = MIN(controls->dir, GTK_ORIENTATION_VERTICAL);
    controls->out = MIN(controls->out, GWY_SF_MINKOWSKI_CONNECTIVITY);
    controls->interp = CLAMP(controls->interp,
                             GWY_INTERPOLATION_ROUND, GWY_INTERPOLATION_NNA);
}

static void
save_args(GwyContainer *container, ToolControls *controls)
{
    gwy_container_set_boolean_by_name(container, isnpoints_key,
                                      controls->isnpoints);
    gwy_container_set_enum_by_name(container, interp_key, controls->interp);
    gwy_container_set_enum_by_name(container, dir_key, controls->dir);
    gwy_container_set_enum_by_name(container, out_key, controls->out);
    gwy_container_set_int32_by_name(container, siz_key, controls->siz);

}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

