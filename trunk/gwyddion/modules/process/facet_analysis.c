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

/**
 * Facet (angle) view uses a zoomed area-preserving projection of north
 * hemisphere normal.  Coordinates on hemisphere are labeled (theta, phi),
 * coordinates on the projection (x, y)
 **/

#include "config.h"
#include <string.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/stats.h>
#include <libprocess/level.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwylayer-basic.h>
#include <libgwydgets/gwylayer-mask.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>

#define FACETS_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

#define FVIEW_GRADIENT "DFit"

enum {
    PREVIEW_SIZE = 400,
    /* XXX: don't change */
    FDATA_RES = 2*113 + 1,
    MAX_PLANE_SIZE = 7,  /* this is actually half */
};

typedef struct {
    gdouble tolerance;
    gint kernel_size;
    /* Interface only */
    gdouble theta0;
    gdouble phi0;
} FacetsArgs;

typedef struct {
    FacetsArgs *args;
    GtkWidget *dialog;
    GtkWidget *inverted;
    GtkWidget *view;
    GtkWidget *fview;
    GtkWidget *theta_label;
    GtkWidget *phi_label;
    GtkWidget *mtheta_label;
    GtkWidget *mphi_label;
    GtkObject *tolerance;
    GtkObject *kernel_size;
    GtkWidget *color_button;
    GwyContainer *mydata;
    GwyContainer *fdata;
    gboolean in_update;
    gboolean computed;
} FacetsControls;

static gboolean module_register                  (void);
static void     facets_analyse                   (GwyContainer *data,
                                                  GwyRunType run);
static void     load_mask_color                  (GtkWidget *color_button,
                                                  GwyContainer *data);
static void     facets_dialog                    (FacetsArgs *args,
                                                  GwyContainer *data,
                                                  GwyContainer *fdata,
                                                  GwyDataField *dfield,
                                                  GwyDataField *mfield,
                                                  gint id,
                                                  GQuark mquark);
static void     run_noninteractive               (FacetsArgs *args,
                                                  GwyContainer *data,
                                                  GwyContainer *fdata,
                                                  GwyDataField *dfield,
                                                  GwyDataField *mfield,
                                                  GQuark mquark);
static void     facets_dialog_update_controls    (FacetsControls *controls,
                                                  FacetsArgs *args);
static void     facet_view_recompute             (GtkAdjustment *adj,
                                                  FacetsControls *controls);
static void     facet_view_reset_maximum         (FacetsControls *controls);
static void     facet_view_select_angle          (FacetsControls *controls,
                                                  gdouble theta,
                                                  gdouble phi);
static void     facet_view_selection_updated     (GwySelection *selection,
                                                  gint id,
                                                  FacetsControls *controls);
static void     update_average_angle             (FacetsControls *controls,
                                                  FacetsArgs *args);
static void     preview_selection_updated        (GwySelection *selection,
                                                  gint id,
                                                  FacetsControls *controls);
static void     mask_color_change_cb             (GtkWidget *color_button,
                                                  FacetsControls *controls);
static void     gwy_data_field_mark_facets       (GwyDataField *dtheta,
                                                  GwyDataField *dphi,
                                                  gdouble theta0,
                                                  gdouble phi0,
                                                  gdouble tolerance,
                                                  GwyDataField *mask);
static void     calculate_average_angle          (GwyDataField *dtheta,
                                                  GwyDataField *dphi,
                                                  GwyDataField *mask,
                                                  gdouble *theta,
                                                  gdouble *phi);
static void     gwy_data_field_facet_distribution(GwyDataField *dfield,
                                                  gint kernel_size,
                                                  GwyContainer *container);
static void     compute_slopes                   (GwyDataField *dfield,
                                                  gint kernel_size,
                                                  GwyDataField *xder,
                                                  GwyDataField *yder);
static void     facets_invalidate                (FacetsControls *controls);
static void     facets_tolerance_changed         (GtkAdjustment *adj,
                                                  FacetsControls *controls);
static void     preview                          (FacetsControls *controls,
                                                  FacetsArgs *args);
static void     add_mask_layer                   (GwyDataView *view,
                                                  const GwyRGBA *color);
static void     facets_mark_fdata                (FacetsArgs *args,
                                                  GwyContainer *fdata);
static void     facets_load_args                 (GwyContainer *container,
                                                  FacetsArgs *args);
static void     facets_save_args                 (GwyContainer *container,
                                                  FacetsArgs *args);

static const FacetsArgs facets_defaults = {
    3.0*G_PI/180.0,
    3,
    /* Interface only */
    0.0,
    0.0,
};

static const GwyRGBA mask_color = { 0.56, 0.39, 0.07, 0.5 };

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Visualizes, marks and measures facet orientation."),
    "Yeti <yeti@gwyddion.net>",
    "1.7",
    "David Nečas (Yeti) & Petr Klapetek",
    "2005",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("facet_analysis",
                              (GwyProcessFunc)&facets_analyse,
                              N_("/_Statistics/Facet _Analysis..."),
                              NULL,
                              FACETS_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Mark areas by 2D slope"));

    return TRUE;
}


static void
facets_analyse(GwyContainer *data, GwyRunType run)
{
    FacetsArgs args;
    GwyContainer *fdata;
    GwyDataField *dfield, *mfield;
    GQuark mquark;
    gint id;

    g_return_if_fail(run & FACETS_RUN_MODES);
    g_return_if_fail(g_type_from_name("GwyLayerPoint"));
    facets_load_args(gwy_app_settings_get(), &args);

    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_MASK_FIELD_KEY, &mquark,
                                     GWY_APP_MASK_FIELD, &mfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     0);
    g_return_if_fail(dfield && mquark);

    fdata = gwy_container_new();
    gwy_data_field_facet_distribution(dfield, 2*args.kernel_size + 1, fdata);
    args.theta0 = gwy_container_get_double_by_name(fdata, "/theta0");
    args.phi0 = gwy_container_get_double_by_name(fdata, "/phi0");
    if (run == GWY_RUN_IMMEDIATE)
        run_noninteractive(&args, data, fdata, dfield, mfield, mquark);
    else {
        facets_dialog(&args, data, fdata, dfield, mfield, id, mquark);
        facets_save_args(gwy_app_settings_get(), &args);
    }
    g_object_unref(fdata);
}

static GtkWidget*
add_angle_label(GtkWidget *table,
                const gchar *name,
                gint *row)
{
    GtkWidget *label;
    GtkRequisition req;

    label = gtk_label_new("-188.00 deg");
    gtk_widget_size_request(label, &req);

    gtk_label_set_text(GTK_LABEL(label), name);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, *row, *row+1, GTK_FILL, 0, 0, 0);

    label = gtk_label_new(NULL);
    gtk_widget_set_size_request(label, req.width, -1);
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     1, 2, *row, *row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    (*row)++;

    return label;
}

static void
facets_dialog(FacetsArgs *args,
              GwyContainer *data,
              GwyContainer *fdata,
              GwyDataField *dfield,
              GwyDataField *mfield,
              gint id,
              GQuark mquark)
{
    GtkWidget *dialog, *table, *hbox, *hbox2, *vbox, *label, *scale, *button;
    GtkWidget *spin;
    FacetsControls controls;
    enum {
        RESPONSE_RESET = 1,
        RESPONSE_PREVIEW = 2
    };
    gint response;
    gdouble zoomval;
    GwyPixmapLayer *layer;
    GwyVectorLayer *vlayer;
    GwySelection *selection;
    gint row;

    memset(&controls, 0, sizeof(FacetsControls));
    controls.args = args;
    dialog = gtk_dialog_new_with_buttons(_("Mark Facets"),
                                         NULL,
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         _("_Mark"), RESPONSE_PREVIEW,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    controls.dialog = dialog;

    /* Shallow-copy stuff to temporary container */
    controls.fdata = fdata;
    controls.mydata = gwy_container_new();
    gwy_container_set_object_by_name(controls.mydata, "/0/data", dfield);
    gwy_app_sync_data_items(data, controls.mydata, id, 0, FALSE,
                            GWY_DATA_ITEM_PALETTE,
                            GWY_DATA_ITEM_RANGE,
                            GWY_DATA_ITEM_MASK_COLOR,
                            GWY_DATA_ITEM_REAL_SQUARE,
                            0);

    hbox = gtk_hbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 4);

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

    vlayer = g_object_new(g_type_from_name("GwyLayerPoint"), NULL);
    gwy_vector_layer_set_selection_key(vlayer, "/0/select/pointer");
    gwy_data_view_set_top_layer(GWY_DATA_VIEW(controls.view), vlayer);
    selection = gwy_vector_layer_ensure_selection(vlayer);
    g_signal_connect(selection, "changed",
                     G_CALLBACK(preview_selection_updated), &controls);

    gtk_box_pack_start(GTK_BOX(hbox), controls.view, FALSE, FALSE, 4);

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);

    hbox2 = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(vbox), hbox2, FALSE, FALSE, 0);

    /* Slope view */
    controls.fview = gwy_data_view_new(controls.fdata);
    gtk_box_pack_start(GTK_BOX(hbox2), controls.fview, FALSE, FALSE, 0);

    layer = gwy_layer_basic_new();
    gwy_pixmap_layer_set_data_key(layer, "/0/data");
    gwy_layer_basic_set_gradient_key(GWY_LAYER_BASIC(layer), "/0/base/palette");
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.fview),
                                 layer);

    vlayer = g_object_new(g_type_from_name("GwyLayerPoint"), NULL);
    gwy_vector_layer_set_selection_key(vlayer, "/0/select/pointer");
    gwy_data_view_set_top_layer(GWY_DATA_VIEW(controls.fview),
                                GWY_VECTOR_LAYER(vlayer));
    selection = gwy_vector_layer_ensure_selection(vlayer);
    g_signal_connect(selection, "changed",
                     G_CALLBACK(facet_view_selection_updated), &controls);

    /* Info table */
    table = gtk_table_new(7, 2, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(hbox2), table, TRUE, TRUE, 4);
    row = 0;

    label = gwy_label_new_header(gwy_sgettext("noun|Normal"));
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 2, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    controls.theta_label = add_angle_label(table, _("θ:"), &row);
    controls.phi_label = add_angle_label(table, _("φ:"), &row);

    button = gtk_button_new_with_mnemonic(_("_Find Maximum"));
    gtk_table_attach(GTK_TABLE(table), button,
                     0, 2, row, row+1, GTK_FILL, 0, 0, 0);
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(facet_view_reset_maximum), &controls);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    label = gwy_label_new_header(_("Mean Normal"));
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 2, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    controls.mtheta_label = add_angle_label(table, _("θ:"), &row);
    controls.mphi_label = add_angle_label(table, _("φ:"), &row);
    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);

    label = gtk_label_new_with_mnemonic(_("Facet plane size:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 2, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    controls.kernel_size = gtk_adjustment_new(args->kernel_size,
                                              0.0, MAX_PLANE_SIZE, 1.0, 1.0, 0);
    spin = gtk_spin_button_new(GTK_ADJUSTMENT(controls.kernel_size), 0.0, 0);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), spin);
    gtk_table_attach(GTK_TABLE(table), spin,
                     0, 1, row, row+1, 0, 0, 0, 0);
    g_signal_connect(controls.kernel_size, "value-changed",
                     G_CALLBACK(facet_view_recompute), &controls);
    row++;

    table = gtk_table_new(9, 4, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(vbox), table, TRUE, TRUE, 4);
    row = 0;

    controls.tolerance = gtk_adjustment_new(args->tolerance*180.0/G_PI,
                                            0.0, 15.0, 0.01, 0.1, 0);
    scale = gwy_table_attach_hscale(table, row++, _("_Tolerance:"), "deg",
                                    controls.tolerance, 0);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(scale), 3);
    g_signal_connect(controls.tolerance, "value-changed",
                     G_CALLBACK(facets_tolerance_changed), &controls);

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

    if (!gwy_si_unit_equal(gwy_data_field_get_si_unit_xy(dfield),
                           gwy_data_field_get_si_unit_z(dfield))) {
        gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);
        label = gtk_label_new(_("Warning: Lateral and value units differ. "
                                "Angles are not physically meaningful."));
        gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
        gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
        gtk_table_attach(GTK_TABLE(table), label,
                         0, 4, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
        row++;
    }

    facets_invalidate(&controls);
    gtk_widget_show_all(dialog);
    facet_view_select_angle(&controls, args->theta0, args->phi0);

    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            g_object_unref(controls.mydata);
            return;
            break;

            case GTK_RESPONSE_OK:
            break;

            case RESPONSE_RESET:
            args->tolerance = facets_defaults.tolerance;
            args->kernel_size = facets_defaults.kernel_size;
            facets_dialog_update_controls(&controls, args);
            break;

            case RESPONSE_PREVIEW:
            preview(&controls, args);
            update_average_angle(&controls, args);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    gwy_app_sync_data_items(controls.mydata, data, 0, id, FALSE,
                            GWY_DATA_ITEM_MASK_COLOR,
                            0);
    gtk_widget_destroy(dialog);

    if (controls.computed) {
        mfield = gwy_container_get_object_by_name(controls.mydata, "/0/mask");
        gwy_app_undo_qcheckpointv(data, 1, &mquark);
        gwy_container_set_object(data, mquark, mfield);
        g_object_unref(controls.mydata);
    }
    else {
        g_object_unref(controls.mydata);
        run_noninteractive(args, data, fdata, dfield, mfield, mquark);
    }
}

static inline void
slopes_to_angles(gdouble xder, gdouble yder,
                 gdouble *theta, gdouble *phi)
{
    *phi = atan2(yder, -xder);
    *theta = atan(hypot(xder, yder));
}

static inline void
angles_to_xy(gdouble theta, gdouble phi,
             gdouble *x, gdouble *y)
{
    gdouble s = 2.0*sin(theta/2.0);

    *x = -s*cos(phi);
    *y = s*sin(phi);
}

static inline void
xy_to_angles(gdouble x, gdouble y,
             gdouble *theta, gdouble *phi)
{
    *phi = atan2(y, -x);
    *theta = 2.0*asin(hypot(x, y)/2.0);
}

static void
facet_view_recompute(GtkAdjustment *adj,
                     FacetsControls *controls)
{
    GwyVectorLayer *layer;
    GwyDataField *dfield;
    GwySelection *selection;
    const gchar *key;

    controls->args->kernel_size = gwy_adjustment_get_int(adj);
    gwy_app_wait_cursor_start(GTK_WINDOW(controls->dialog));
    dfield = gwy_container_get_object_by_name(controls->mydata, "/0/data");
    gwy_data_field_facet_distribution(dfield, 2*controls->args->kernel_size + 1,
                                      controls->fdata);

    /* XXX: Clear selections since we cannot recalculate it properly */
    if (gwy_container_gis_object_by_name(controls->mydata, "/0/mask",
                                         &dfield)) {
        gwy_data_field_clear(dfield);
        gwy_data_field_data_changed(dfield);
    }

    layer = gwy_data_view_get_top_layer(GWY_DATA_VIEW(controls->fview));
    key = gwy_vector_layer_get_selection_key(layer);
    selection = gwy_container_get_object_by_name(controls->fdata, key);
    gwy_selection_clear(selection);
    gwy_app_wait_cursor_finish(GTK_WINDOW(controls->dialog));
    facets_invalidate(controls);
}

static void
facet_view_reset_maximum(FacetsControls *controls)
{
    GwyDataField *mask = NULL;
    FacetsArgs *args;

    args = controls->args;
    args->theta0 = gwy_container_get_double_by_name(controls->fdata,
                                                    "/theta0");
    args->phi0 = gwy_container_get_double_by_name(controls->fdata,
                                                  "/phi0");
    facet_view_select_angle(controls, args->theta0, args->phi0);
    gtk_label_set_text(GTK_LABEL(controls->mtheta_label), "");
    gtk_label_set_text(GTK_LABEL(controls->mphi_label), "");

    if (gwy_container_gis_object_by_name(controls->fdata, "/0/mask", &mask)) {
        gwy_data_field_clear(mask);
        gwy_data_field_data_changed(mask);
    }
    facets_invalidate(controls);
}

static void
facet_view_select_angle(FacetsControls *controls,
                        gdouble theta,
                        gdouble phi)
{
    gdouble x, y, q, xy[2];
    GwyVectorLayer *layer;
    GwySelection *selection;
    const gchar *key;

    angles_to_xy(theta, phi, &x, &y);
    controls->in_update = TRUE;
    q = gwy_container_get_double_by_name(controls->fdata, "/q");
    xy[0] = x + G_SQRT2/q;
    xy[1] = y + G_SQRT2/q;
    layer = gwy_data_view_get_top_layer(GWY_DATA_VIEW(controls->fview));
    key = gwy_vector_layer_get_selection_key(layer);
    selection = gwy_container_get_object_by_name(controls->fdata, key);
    gwy_selection_set_object(selection, 0, xy);
    controls->in_update = FALSE;
}

static void
facet_view_selection_updated(GwySelection *selection,
                             G_GNUC_UNUSED gint id,
                             FacetsControls *controls)
{
    GwyVectorLayer *layer;
    const gchar *key;
    gdouble theta, phi, x, y, q, xy[2];
    gchar s[24];

    q = gwy_container_get_double_by_name(controls->fdata, "/q");
    gwy_selection_get_object(selection, 0, xy);
    x = xy[0] - G_SQRT2/q;
    y = xy[1] - G_SQRT2/q;
    xy_to_angles(x, y, &theta, &phi);

    g_snprintf(s, sizeof(s), "%.2f deg", 180.0/G_PI*theta);
    gtk_label_set_text(GTK_LABEL(controls->theta_label), s);
    controls->args->theta0 = theta;

    g_snprintf(s, sizeof(s), "%.2f deg", 180.0/G_PI*phi);
    gtk_label_set_text(GTK_LABEL(controls->phi_label), s);
    controls->args->phi0 = phi;

    if (!controls->in_update) {
        layer = gwy_data_view_get_top_layer(GWY_DATA_VIEW(controls->view));
        key = gwy_vector_layer_get_selection_key(layer);
        selection = gwy_container_get_object_by_name(controls->mydata, key);
        if (gwy_selection_get_data(selection, NULL))
            gwy_selection_clear(selection);
    }

    controls->computed = FALSE;
}

static void
update_average_angle(FacetsControls *controls,
                     G_GNUC_UNUSED FacetsArgs *args)
{
    GwyDataField *dtheta, *dphi, *mask;
    gdouble theta, phi;
    gchar s[24];

    dtheta = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->fdata,
                                                             "/theta"));
    dphi = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->fdata,
                                                           "/phi"));
    mask = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                           "/0/mask"));
    calculate_average_angle(dtheta, dphi, mask, &theta, &phi);

    g_snprintf(s, sizeof(s), "%.2f deg", 180.0/G_PI*theta);
    gtk_label_set_text(GTK_LABEL(controls->mtheta_label), s);
    g_snprintf(s, sizeof(s), "%.2f deg", 180.0/G_PI*phi);
    gtk_label_set_text(GTK_LABEL(controls->mphi_label), s);
}

static void
preview_selection_updated(GwySelection *selection,
                          G_GNUC_UNUSED gint id,
                          FacetsControls *controls)
{
    GwyDataField *dfield;
    gdouble theta, phi, xy[2];
    gint i, j;

    if (controls->in_update)
        return;

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                             "/0/data"));
    if (!gwy_selection_get_object(selection, 0, xy))
        return;

    j = gwy_data_field_rtoj(dfield, xy[0]);
    i = gwy_data_field_rtoi(dfield, xy[1]);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->fdata,
                                                             "/theta"));
    theta = gwy_data_field_get_val(dfield, j, i);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->fdata,
                                                             "/phi"));
    phi = gwy_data_field_get_val(dfield, j, i);
    facet_view_select_angle(controls, theta, phi);
}

static GwyDataField*
create_mask_field(GwyDataField *dfield)
{
    GwyDataField *mfield;
    GwySIUnit *siunit;

    mfield = gwy_data_field_new_alike(dfield, FALSE);
    siunit = gwy_si_unit_new("");
    gwy_data_field_set_si_unit_z(mfield, siunit);
    g_object_unref(siunit);

    return mfield;
}

static void
run_noninteractive(FacetsArgs *args,
                   GwyContainer *data,
                   GwyContainer *fdata,
                   GwyDataField *dfield,
                   GwyDataField *mfield,
                   GQuark mquark)
{
    GwyDataField *dtheta, *dphi;

    gwy_app_undo_qcheckpointv(data, 1, &mquark);
    if (!mfield) {
        mfield = create_mask_field(dfield);
        gwy_container_set_object(data, mquark, mfield);
        g_object_unref(mfield);
    }

    dtheta = GWY_DATA_FIELD(gwy_container_get_object_by_name(fdata, "/theta"));
    dphi = GWY_DATA_FIELD(gwy_container_get_object_by_name(fdata, "/phi"));
    gwy_data_field_mark_facets(dtheta, dphi, args->theta0, args->phi0,
                               args->tolerance, mfield);
    gwy_data_field_data_changed(mfield);
}

static void
gwy_data_field_mark_facets(GwyDataField *dtheta,
                           GwyDataField *dphi,
                           gdouble theta0,
                           gdouble phi0,
                           gdouble tolerance,
                           GwyDataField *mask)
{
    gdouble cr, cth0, sth0, cro;
    const gdouble *xd, *yd;
    gdouble *md;
    gint i;

    cr = cos(tolerance);
    cth0 = cos(theta0);
    sth0 = sin(theta0);

    xd = gwy_data_field_get_data_const(dtheta);
    yd = gwy_data_field_get_data_const(dphi);
    md = gwy_data_field_get_data(mask);
    for (i = gwy_data_field_get_xres(dtheta)*gwy_data_field_get_yres(dtheta);
         i;
         i--, xd++, yd++, md++) {
        cro = cth0*cos(*xd) + sth0*sin(*xd)*cos(*yd - phi0);
        *md = (cro >= cr);
    }
}

static void
calculate_average_angle(GwyDataField *dtheta,
                        GwyDataField *dphi,
                        GwyDataField *mask,
                        gdouble *theta,
                        gdouble *phi)
{
    gdouble sx, sy, sz;
    const gdouble *td, *pd, *md;
    gint i, n;

    td = gwy_data_field_get_data_const(dtheta);
    pd = gwy_data_field_get_data_const(dphi);
    md = gwy_data_field_get_data_const(mask);
    n = 0;
    sx = sy = sz = 0.0;
    for (i = gwy_data_field_get_xres(dtheta)*gwy_data_field_get_yres(dtheta);
         i;
         i--, td++, pd++, md++) {
        if (!*md)
            continue;

        sx += sin(*td)*cos(*pd);
        sy += sin(*td)*sin(*pd);
        sz += cos(*td);
        n++;
    }

    if (!n)
        n = 1;
    sx /= n;
    sy /= n;
    sz /= n;
    *theta = atan2(hypot(sx, sy), sz);
    *phi = atan2(sy, sx);
}

static void
gwy_data_field_facet_distribution(GwyDataField *dfield,
                                  gint kernel_size,
                                  GwyContainer *container)
{
    GwyDataField *dtheta, *dphi, *dist;
    GwySIUnit *siunit;
    gdouble *xd, *yd, *data;
    const gdouble *xdc, *ydc;
    gdouble q, max;
    gint res, hres, i, j, mi, mj, xres, yres;

    if (gwy_container_gis_object_by_name(container, "/theta", &dtheta))
        g_object_ref(dtheta);
    else
        dtheta = gwy_data_field_new_alike(dfield, FALSE);

    if (gwy_container_gis_object_by_name(container, "/phi", &dphi))
        g_object_ref(dphi);
    else
        dphi = gwy_data_field_new_alike(dfield, FALSE);

    compute_slopes(dfield, kernel_size, dtheta, dphi);
    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    xd = gwy_data_field_get_data(dtheta);
    yd = gwy_data_field_get_data(dphi);

    for (i = xres*yres; i; i--, xd++, yd++) {
        gdouble theta, phi;

        slopes_to_angles(*xd, *yd, &theta, &phi);
        *xd = theta;
        *yd = phi;
    }
    q = gwy_data_field_get_max(dtheta);
    q = MIN(q*1.05, G_PI/2.0);
    q = G_SQRT2/(2.0*sin(q/2.0));

    if (gwy_container_gis_object_by_name(container, "/0/data", &dist)) {
        g_object_ref(dist);
        gwy_data_field_clear(dist);
        gwy_data_field_set_xreal(dist, 2.0*G_SQRT2/q);
        gwy_data_field_set_yreal(dist, 2.0*G_SQRT2/q);
    }
    else {
        dist = gwy_data_field_new(FDATA_RES, FDATA_RES,
                                  2.0*G_SQRT2/q, 2.0*G_SQRT2/q,
                                  TRUE);
        siunit = gwy_si_unit_new("");
        gwy_data_field_set_si_unit_z(dist, siunit);
        g_object_unref(siunit);
        /* FIXME */
        siunit = gwy_si_unit_new("");
        gwy_data_field_set_si_unit_xy(dist, siunit);
        g_object_unref(siunit);
    }

    res = FDATA_RES;
    hres = (res - 1)/2;
    data = gwy_data_field_get_data(dist);

    xdc = gwy_data_field_get_data_const(dtheta);
    ydc = gwy_data_field_get_data_const(dphi);
    for (i = xres*yres; i; i--, xdc++, ydc++) {
        gdouble x, y;
        gint xx, yy;

        angles_to_xy(*xdc, *ydc, &x, &y);
        xx = GWY_ROUND(q*x/G_SQRT2*hres) + hres;
        yy = GWY_ROUND(q*y/G_SQRT2*hres) + hres;
        data[yy*res + xx] += 1.0;
    }

    /* Find maxima */
    mi = mj = hres;
    max = 0;
    for (i = 1; i+1 < res; i++) {
        for (j = 1; j+1 < res; j++) {
            gdouble z;

            z = data[i*res + j]
                + 0.3*(data[i*res + j - 1]
                       + data[i*res + j + 1]
                       + data[i*res - res + j]
                       + data[i*res + res + j])
                + 0.1*(data[i*res - res + j - 1]
                       + data[i*res - res + j + 1]
                       + data[i*res + res + j - 1]
                       + data[i*res + res + j + 1]);
            if (G_UNLIKELY(z > max)) {
                max = z;
                mi = i;
                mj = j;
            }
        }
    }

    for (i = res*res; i; i--, data++)
        *data = pow(*data, 0.35);

    gwy_container_set_double_by_name(container, "/q", q);
    {
        gdouble x, y, theta, phi;

        x = (mj - hres)*G_SQRT2/(q*hres);
        y = (mi - hres)*G_SQRT2/(q*hres);
        xy_to_angles(x, y, &theta, &phi);
        gwy_container_set_double_by_name(container, "/theta0", theta);
        gwy_container_set_double_by_name(container, "/phi0", phi);
    }
    gwy_container_set_object_by_name(container, "/0/data", dist);
    g_object_unref(dist);
    gwy_container_set_object_by_name(container, "/theta", dtheta);
    g_object_unref(dtheta);
    gwy_container_set_object_by_name(container, "/phi", dphi);
    g_object_unref(dphi);
    gwy_container_set_string_by_name(container, "/0/base/palette",
                                     g_strdup(FVIEW_GRADIENT));

    gwy_data_field_data_changed(dist);
}

static void
compute_slopes(GwyDataField *dfield,
               gint kernel_size,
               GwyDataField *xder,
               GwyDataField *yder)
{
    gint xres, yres;

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    if (kernel_size > 1) {
        GwyPlaneFitQuantity quantites[] = {
            GWY_PLANE_FIT_BX, GWY_PLANE_FIT_BY
        };
        GwyDataField *fields[2];

        fields[0] = xder;
        fields[1] = yder;
        gwy_data_field_fit_local_planes(dfield, kernel_size,
                                        2, quantites, fields);
    }
    else {
        gint col, row;
        gdouble *xd, *yd;
        const gdouble *data;
        gdouble d;

        data = gwy_data_field_get_data_const(dfield);
        xd = gwy_data_field_get_data(xder);
        yd = gwy_data_field_get_data(yder);
        for (row = 0; row < yres; row++) {
            for (col = 0; col < xres; col++) {
                if (!col)
                    d = data[row*xres + col + 1] - data[row*xres + col];
                else if (col == xres-1)
                    d = data[row*xres + col] - data[row*xres + col - 1];
                else
                    d = (data[row*xres + col + 1]
                         - data[row*xres + col - 1])/2;
                *(xd++) = d;

                if (!row)
                    d = data[row*xres + xres + col] - data[row*xres + col];
                else if (row == yres-1)
                    d = data[row*xres + col] - data[row*xres - xres + col];
                else
                    d = (data[row*xres + xres + col]
                         - data[row*xres - xres + col])/2;
                *(yd++) = d;
            }
        }
    }

    gwy_data_field_multiply(xder, xres/gwy_data_field_get_xreal(dfield));
    gwy_data_field_multiply(yder, yres/gwy_data_field_get_yreal(dfield));
}

static void
facets_dialog_update_controls(FacetsControls *controls,
                              FacetsArgs *args)
{
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->tolerance),
                             args->tolerance*180.0/G_PI);
}

static void
facets_invalidate(FacetsControls *controls)
{
    controls->computed = FALSE;
}

static void
facets_tolerance_changed(GtkAdjustment *adj,
                         FacetsControls *controls)
{
    controls->args->tolerance = gtk_adjustment_get_value(adj);
    controls->args->tolerance *= G_PI/180.0;
    facets_invalidate(controls);
}

static void
mask_color_change_cb(GtkWidget *color_button,
                     FacetsControls *controls)
{
    gwy_mask_color_selector_run(NULL, GTK_WINDOW(controls->dialog),
                                GWY_COLOR_BUTTON(color_button),
                                controls->mydata, "/0/mask");
    load_mask_color(color_button,
                    gwy_data_view_get_data(GWY_DATA_VIEW(controls->view)));
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
preview(FacetsControls *controls,
        FacetsArgs *args)
{
    GwyDataField *dtheta, *dphi, *dfield, *mfield;
    GwyContainer *data, *fdata;

    data = controls->mydata;
    fdata = controls->fdata;

    add_mask_layer(GWY_DATA_VIEW(controls->view), NULL);
    add_mask_layer(GWY_DATA_VIEW(controls->fview), &mask_color);

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    mfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/mask"));
    dtheta = GWY_DATA_FIELD(gwy_container_get_object_by_name(fdata, "/theta"));
    dphi = GWY_DATA_FIELD(gwy_container_get_object_by_name(fdata, "/phi"));

    gwy_data_field_mark_facets(dtheta, dphi, args->theta0, args->phi0,
                               args->tolerance, mfield);
    gwy_data_field_data_changed(mfield);
    facets_mark_fdata(args, fdata);
    facets_invalidate(controls);
}

static void
add_mask_layer(GwyDataView *view,
               const GwyRGBA *color)
{
    GwyContainer *data;
    GwyDataField *mfield, *dfield;
    GwyPixmapLayer *layer;

    data = gwy_data_view_get_data(view);
    if (!gwy_container_gis_object_by_name(data, "/0/mask", &mfield)) {
        gwy_container_gis_object_by_name(data, "/0/data", &dfield);
        mfield = create_mask_field(dfield);
        gwy_container_set_object_by_name(data, "/0/mask", mfield);
        g_object_unref(mfield);

        layer = gwy_layer_mask_new();
        gwy_pixmap_layer_set_data_key(layer, "/0/mask");
        gwy_layer_mask_set_color_key(GWY_LAYER_MASK(layer), "/0/mask");
        gwy_data_view_set_alpha_layer(view, layer);

        if (color)
            gwy_rgba_store_to_container(color, data, "/0/mask");
    }
}

static void
facets_mark_fdata(FacetsArgs *args,
                  GwyContainer *fdata)
{
    GwyDataField *mask;
    gdouble q, r, cr, cro, cth0, sth0, cphi0, sphi0;
    gint hres, i, j;
    gdouble *m;

    hres = (FDATA_RES - 1)/2;
    cr = cos(args->tolerance);
    cth0 = cos(args->theta0);
    sth0 = sin(args->theta0);
    cphi0 = cos(args->phi0);
    sphi0 = sin(args->phi0);
    q = gwy_container_get_double_by_name(fdata, "/q");
    mask = GWY_DATA_FIELD(gwy_container_get_object_by_name(fdata, "/0/mask"));
    m = gwy_data_field_get_data(mask);

    for (i = 0; i < FDATA_RES; i++) {
        gdouble y = G_SQRT2/(q*hres)*(i - hres);

        for (j = 0; j < FDATA_RES; j++) {
            gdouble x = -G_SQRT2/(q*hres)*(j - hres);

            /**
             * Orthodromic distance computed directly from x, y:
             * cos(theta) = 1 - 2*(r/2)^2
             * sin(theta) = r*sqrt(1 - (r/2)^2)
             * cos(phi) = x/r
             * sin(phi) = y/r
             * where r = hypot(x, y)
             **/
            r = hypot(x, y);
            cro = cth0*(1.0 - r*r/2.0)
                  + sth0*r*sqrt(1.0 - r*r/4.0)*(x/r*cphi0 + y/r*sphi0);
            m[i*FDATA_RES + j] = (cro >= cr);
        }
    }
    gwy_data_field_data_changed(mask);
}

static const gchar kernel_size_key[] = "/module/facet_analysis/kernel-size";
static const gchar tolerance_key[]   = "/module/facet_analysis/tolerance";

static void
facets_sanitize_args(FacetsArgs *args)
{
    args->tolerance = CLAMP(args->tolerance, 0.0, 15.0*G_PI/180.0);
    args->kernel_size = CLAMP(args->kernel_size, 0, MAX_PLANE_SIZE);
}

static void
facets_load_args(GwyContainer *container,
                     FacetsArgs *args)
{
    *args = facets_defaults;

    gwy_container_gis_double_by_name(container, tolerance_key,
                                     &args->tolerance);
    gwy_container_gis_int32_by_name(container, kernel_size_key,
                                    &args->kernel_size);
    facets_sanitize_args(args);
}

static void
facets_save_args(GwyContainer *container,
                      FacetsArgs *args)
{
    gwy_container_set_double_by_name(container, tolerance_key,
                                     args->tolerance);
    gwy_container_set_int32_by_name(container, kernel_size_key,
                                    args->kernel_size);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
