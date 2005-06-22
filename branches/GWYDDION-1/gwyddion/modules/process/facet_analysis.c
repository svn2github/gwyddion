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

#include <string.h>

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule.h>
#include <libgwydgets/gwydgets.h>
#include <libprocess/datafield.h>
#include <app/gwyapp.h>

#define FACETS_RUN_MODES \
    (GWY_RUN_MODAL | GWY_RUN_NONINTERACTIVE | GWY_RUN_WITH_DEFAULTS)

enum {
    PREVIEW_SIZE = 320,
    FDATA_RES = 201,
    MAX_LENGTH = 1024
};

typedef struct {
    gboolean inverted;   /* unused */
} FacetsArgs;

typedef struct {
    GtkWidget *inverted;
    GtkWidget *view;
    GtkWidget *fview;
    GtkWidget *theta_label;
    GtkWidget *phi_label;
    GtkObject *threshold_high;
    GtkObject *threshold_low;
    GtkObject *min_len;
    GtkObject *max_width;
    GtkWidget *color_button;
    GwyContainer *mydata;
    GwyContainer *fdata;
    FacetsArgs *args;
} FacetsControls;

static gboolean module_register                   (const gchar *name);
static gboolean facets_remove                      (GwyContainer *data,
                                                   GwyRunType run);
static gboolean facets_mark                        (GwyContainer *data,
                                                   GwyRunType run);
static void     load_mask_color                   (GtkWidget *color_button,
                                                   GwyContainer *data);
static void     save_mask_color                   (GtkWidget *color_button,
                                                   GwyContainer *data);
static gboolean facets_mark_dialog                 (FacetsArgs *args,
                                                   GwyContainer *data);
static void     facets_mark_dialog_update_controls (FacetsControls *controls,
                                                   FacetsArgs *args);
static void     facets_mark_dialog_update_values   (FacetsControls *controls,
                                                   FacetsArgs *args);
static void     facets_mark_dialog_update_thresholds(GtkObject *adj,
                                                    FacetsControls *controls);
static void     facet_view_selection_updated      (GwyVectorLayer *layer,
                                                   FacetsControls *controls);
static void     mask_color_change_cb              (GtkWidget *color_button,
                                                   FacetsControls *controls);
static void     gwy_data_field_facet_distribution(GwyDataField *dfield,
                                                  gint kernel_size,
                                                  GwyContainer *container);
static void     compute_slopes                    (GwyDataField *dfield,
                                                   gint kernel_size,
                                                   GwyDataField *xder,
                                                   GwyDataField *yder);
static void     preview                           (FacetsControls *controls,
                                                   FacetsArgs *args);
static void     facets_mark_do                     (FacetsArgs *args,
                                                   GwyContainer *data);
static void     facets_mark_load_args              (GwyContainer *container,
                                                   FacetsArgs *args);
static void     facets_mark_save_args              (GwyContainer *container,
                                                      FacetsArgs *args);

static const FacetsArgs facets_defaults = {
    FALSE,
};

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "facet_analysis",
    N_(""),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2005",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyProcessFuncInfo facet_analysis_func_info = {
        "facet_analysis",
        N_("/_Statistics/_Facet Analysis..."),
        (GwyProcessFunc)&facets_mark,
        FACETS_RUN_MODES,
        0,
    };

    gwy_process_func_register(name, &facet_analysis_func_info);

    return TRUE;
}


static gboolean
facets_mark(GwyContainer *data, GwyRunType run)
{
    FacetsArgs args;
    gboolean ok = FALSE;

    g_return_val_if_fail(run & FACETS_RUN_MODES, FALSE);
    if (run == GWY_RUN_WITH_DEFAULTS)
        args = facets_defaults;
    else
        facets_mark_load_args(gwy_app_settings_get(), &args);

    ok = (run != GWY_RUN_MODAL) || facets_mark_dialog(&args, data);
    if (run == GWY_RUN_MODAL)
        facets_mark_save_args(gwy_app_settings_get(), &args);
    if (!ok)
        return FALSE;

    gwy_app_undo_checkpoint(data, "/0/mask", NULL);
    facets_mark_do(&args, data);

    return ok;
}

static void
facets_mark_do(G_GNUC_UNUSED FacetsArgs *args, GwyContainer *data)
{
    GObject *dfield, *mask;

    dfield = gwy_container_get_object_by_name(data, "/0/data");

    if (!gwy_container_gis_object_by_name(data, "/0/mask", &mask)) {
        mask = gwy_serializable_duplicate(dfield);
        gwy_container_set_object_by_name(data, "/0/mask", mask);
        g_object_unref(mask);
    }
    /*
    gwy_data_field_mark_facets(GWY_DATA_FIELD(dfield), GWY_DATA_FIELD(mask),
                              args->threshold_high, args->threshold_low,
                              args->min_len, args->max_width, args->inverted);
                              */
}

static gboolean
facets_mark_dialog(FacetsArgs *args, GwyContainer *data)
{
    GtkWidget *dialog, *table, *spin, *hbox, *hbox2, *vbox, *label;
    GtkRequisition req;
    FacetsControls controls;
    enum {
        RESPONSE_RESET = 1,
        RESPONSE_PREVIEW = 2
    };
    gint response;
    gdouble zoomval;
    GtkObject *layer;
    GtkObject *flayer;
    GwyDataField *dfield;
    gint row;

    controls.args = args;
    dialog = gtk_dialog_new_with_buttons(_("Mark Facets"),
                                         NULL,
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         _("_Update preview"), RESPONSE_PREVIEW,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    hbox = gtk_hbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 4);

    controls.mydata = gwy_container_duplicate_by_prefix(data,
                                                        "/0/data",
                                                        "/0/mask",
                                                        "/0/base/palette",
                                                        NULL);
    controls.view = gwy_data_view_new(controls.mydata);
    g_object_unref(controls.mydata);
    layer = gwy_layer_basic_new();
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.view),
                                 GWY_PIXMAP_LAYER(layer));
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls.mydata,
                                                             "/0/data"));

    zoomval = PREVIEW_SIZE/(gdouble)MAX(gwy_data_field_get_xres(dfield),
                                        gwy_data_field_get_yres(dfield));
    gwy_data_view_set_zoom(GWY_DATA_VIEW(controls.view), zoomval);

    gtk_box_pack_start(GTK_BOX(hbox), controls.view, FALSE, FALSE, 4);

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);

    hbox2 = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(vbox), hbox2, FALSE, FALSE, 0);

    /* Slope view */
    controls.fdata = GWY_CONTAINER(gwy_container_new());
    gwy_data_field_facet_distribution(dfield, 3, controls.fdata);

    controls.fview = gwy_data_view_new(controls.fdata);
    g_object_unref(controls.fdata);
    gtk_box_pack_start(GTK_BOX(hbox2), controls.fview, FALSE, FALSE, 0);

    layer = gwy_layer_basic_new();
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.fview),
                                 GWY_PIXMAP_LAYER(layer));

    flayer = g_object_new(g_type_from_name("GwyLayerPoints"), NULL);
    g_object_set(G_OBJECT(flayer), "max_points", 1, NULL);
    gwy_data_view_set_top_layer(GWY_DATA_VIEW(controls.fview),
                                GWY_VECTOR_LAYER(flayer));
    g_signal_connect(flayer, "updated",
                     G_CALLBACK(facet_view_selection_updated), &controls);

    table = gtk_table_new(4, 2, FALSE);
    gtk_box_pack_start(GTK_BOX(hbox2), table, TRUE, TRUE, 4);
    row = 0;

    label = gtk_label_new("");
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Inclination</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 2, row, row+1,
                     GTK_FILL, 0, 2, 2);
    row++;

    /* Theta */
    label = gtk_label_new("-180.00 deg");
    gtk_widget_size_request(label, &req);

    gtk_label_set_text(GTK_LABEL(label), _("Theta:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row+1,
                     GTK_FILL, 0, 2, 2);

    controls.theta_label = gtk_label_new(NULL);
    gtk_widget_set_size_request(controls.theta_label, req.width, -1);
    gtk_misc_set_alignment(GTK_MISC(controls.theta_label), 1.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), controls.theta_label, 1, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);
    row++;

    /* Phi */
    label = gtk_label_new(_("Phi:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row+1,
                     GTK_FILL, 0, 2, 2);

    controls.phi_label = gtk_label_new(NULL);
    gtk_widget_set_size_request(controls.phi_label, req.width, -1);
    gtk_misc_set_alignment(GTK_MISC(controls.phi_label), 1.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), controls.phi_label, 1, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);
    row++;

    table = gtk_table_new(10, 4, FALSE);
    gtk_box_pack_start(GTK_BOX(vbox), table, TRUE, TRUE, 4);
    row = 0;

    /*
    controls.max_width = gtk_adjustment_new(args->max_width,
                                            1.0, 16.0, 1, 3, 0);
    gwy_table_attach_hscale(table, row++, _("Maximum _width:"), "px",
                            controls.max_width, 0);

    controls.min_len = gtk_adjustment_new(args->min_len,
                                          1.0, MAX_LENGTH, 1, 10, 0);
    gwy_table_attach_hscale(table, row++, _("Minimum _length:"), "px",
                            controls.min_len, GWY_HSCALE_SQRT);

    controls.threshold_high = gtk_adjustment_new(args->threshold_high,
                                                 0.0, 2.0, 0.01, 0.1, 0);
    spin = gwy_table_attach_hscale(table, row++, _("_Hard threshold:"),
                                   _("RMS"), controls.threshold_high, 0);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 2);
    g_signal_connect(controls.threshold_high, "value_changed",
                     G_CALLBACK(facets_mark_dialog_update_thresholds),
                     &controls);

    controls.threshold_low = gtk_adjustment_new(args->threshold_low,
                                                0.0, 2.0, 0.01, 0.1, 0);
    spin = gwy_table_attach_hscale(table, row, _("_Soft threshold:"), _("RMS"),
                                   controls.threshold_low, 0);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 2);
    g_signal_connect(controls.threshold_low, "value_changed",
                     G_CALLBACK(facets_mark_dialog_update_thresholds),
                     &controls);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;
    */

    controls.inverted = gtk_check_button_new_with_mnemonic(_("_Negative"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.inverted),
                                 args->inverted);
    gtk_table_attach(GTK_TABLE(table), controls.inverted,
                     0, 4, row, row+1, GTK_EXPAND | GTK_FILL, 0, 2, 2);
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

    gtk_widget_show_all(dialog);
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            facets_mark_dialog_update_values(&controls, args);
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            return FALSE;
            break;

            case GTK_RESPONSE_OK:
            break;

            case RESPONSE_RESET:
            *args = facets_defaults;
            facets_mark_dialog_update_controls(&controls, args);
            break;

            case RESPONSE_PREVIEW:
            facets_mark_dialog_update_values(&controls, args);
            preview(&controls, args);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    facets_mark_dialog_update_values(&controls, args);
    save_mask_color(controls.color_button, data);
    gtk_widget_destroy(dialog);

    return TRUE;
}

static inline void
slopes_to_angles(gdouble xder, gdouble yder,
                 gdouble *theta, gdouble *phi)
{
    *phi = atan2(yder, xder);
    *theta = acos(1.0/sqrt(1.0 + xder*xder + yder*yder));
}

static inline void
angles_to_xy(gdouble theta, gdouble phi,
             gdouble *x, gdouble *y)
{
    gdouble s = 2.0*sin(theta/2.0);

    *x = s*cos(phi);
    *y = s*sin(phi);
}

static inline void
xy_to_angles(gdouble x, gdouble y,
             gdouble *theta, gdouble *phi)
{
    *phi = atan2(y, x);
    *theta = 2.0*asin(hypot(x, y)/2.0);
}

static void
facet_view_selection_updated(GwyVectorLayer *layer,
                             FacetsControls *controls)
{
    gdouble selection[2];
    gdouble theta, phi, x, y, q;
    gchar s[24];

    q = gwy_container_get_double_by_name(controls->fdata, "/q");
    gwy_vector_layer_get_selection(layer, selection);
    g_printerr("%g %g\n", selection[0], selection[1]);
    /* q/G_SQRT2/G_SQRT2 is correction to coordinate of pixel centre instead
     * of edge */
    x = selection[0] - G_SQRT2/q + q/G_SQRT2/FDATA_RES;
    y = G_SQRT2/q - selection[1] - q/G_SQRT2/FDATA_RES;
    xy_to_angles(x, y, &theta, &phi);

    g_snprintf(s, sizeof(s), "%.2f deg", 180.0/G_PI*theta);
    gtk_label_set_text(GTK_LABEL(controls->theta_label), s);
    g_snprintf(s, sizeof(s), "%.2f deg", 180.0/G_PI*phi);
    gtk_label_set_text(GTK_LABEL(controls->phi_label), s);
}

static void
gwy_data_field_facet_distribution(GwyDataField *dfield,
                                  gint kernel_size,
                                  GwyContainer *container)
{
    GwyDataField *dtheta, *dphi, *dist;
    GwySIUnit *siunit;
    gdouble *xd, *yd, *data;
    gdouble q;
    gint res, hres, i, xres, yres;

    dtheta = GWY_DATA_FIELD(gwy_data_field_new_alike(dfield, FALSE));
    dphi = GWY_DATA_FIELD(gwy_data_field_new_alike(dfield, FALSE));

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

    dist = GWY_DATA_FIELD(gwy_data_field_new(FDATA_RES, FDATA_RES,
                                             2.0*G_SQRT2/q, 2.0*G_SQRT2/q,
                                             TRUE));
    siunit = GWY_SI_UNIT(gwy_si_unit_new(""));
    gwy_data_field_set_si_unit_z(dist, siunit);
    g_object_unref(siunit);
    /* FIXME */
    siunit = GWY_SI_UNIT(gwy_si_unit_new(""));
    gwy_data_field_set_si_unit_xy(dist, siunit);
    g_object_unref(siunit);

    res = FDATA_RES;
    hres = (res - 1)/2;
    data = gwy_data_field_get_data(dist);

    xd = gwy_data_field_get_data(dtheta);
    yd = gwy_data_field_get_data(dphi);
    for (i = xres*yres; i; i--, xd++, yd++) {
        gdouble x, y;
        gint xx, yy;

        angles_to_xy(*xd, *yd, &x, &y);
        xx = ROUND(q*x/G_SQRT2*hres) + hres;
        yy = ROUND(q*y/G_SQRT2*hres) + hres;
        data[yy*res + xx] += 1.0;
    }

    for (i = res*res; i; i--, data++)
        *data = pow(*data, 0.25);

    gwy_container_set_double_by_name(container, "/q", q);
    gwy_container_set_object_by_name(container, "/0/data", (GObject*)dist);
    g_object_unref(dist);
    gwy_container_set_object_by_name(container, "/theta", (GObject*)dtheta);
    g_object_unref(dtheta);
    gwy_container_set_object_by_name(container, "/phi", (GObject*)dphi);
    g_object_unref(dphi);
    gwy_container_set_string_by_name(container, "/0/base/palette",
                                     g_strdup("DFit"));
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
    if (kernel_size) {
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
facets_mark_dialog_update_controls(FacetsControls *controls,
                                  FacetsArgs *args)
{
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->inverted),
                                 args->inverted);
}

static void
facets_mark_dialog_update_values(FacetsControls *controls,
                                FacetsArgs *args)
{
    args->inverted
        = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->inverted));
}

static void
mask_color_change_cb(GtkWidget *color_button,
                     FacetsControls *controls)
{
    gwy_color_selector_for_mask(NULL,
                                GWY_DATA_VIEW(controls->view),
                                GWY_COLOR_BUTTON(color_button),
                                NULL, "/0/mask");
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
save_mask_color(GtkWidget *color_button,
                GwyContainer *data)
{
    GwyRGBA rgba;

    gwy_color_button_get_color(GWY_COLOR_BUTTON(color_button), &rgba);
    gwy_rgba_store_to_container(&rgba, data, "/0/mask");
}

static void
preview(FacetsControls *controls,
        FacetsArgs *args)
{
    GwyDataField *mask, *dfield;
    GwyPixmapLayer *layer;

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                             "/0/data"));

    /*set up the mask*/
    if (gwy_container_gis_object_by_name(controls->mydata, "/0/mask",
                                         (GObject**)&mask)) {
        gwy_data_field_resample(mask,
                                gwy_data_field_get_xres(dfield),
                                gwy_data_field_get_yres(dfield),
                                GWY_INTERPOLATION_NONE);
        gwy_data_field_copy(dfield, mask);
        if (!gwy_data_view_get_alpha_layer(GWY_DATA_VIEW(controls->view))) {
            layer = GWY_PIXMAP_LAYER(gwy_layer_mask_new());
            gwy_data_view_set_alpha_layer(GWY_DATA_VIEW(controls->view),
                                          GWY_PIXMAP_LAYER(layer));
        }
    }
    else {
        mask = GWY_DATA_FIELD(gwy_serializable_duplicate(G_OBJECT(dfield)));
        gwy_container_set_object_by_name(controls->mydata, "/0/mask",
                                         G_OBJECT(mask));
        g_object_unref(mask);
        layer = GWY_PIXMAP_LAYER(gwy_layer_mask_new());
        gwy_data_view_set_alpha_layer(GWY_DATA_VIEW(controls->view),
                                      GWY_PIXMAP_LAYER(layer));

    }

    facets_mark_do(args, controls->mydata);

    gwy_data_view_update(GWY_DATA_VIEW(controls->view));

}

static const gchar *inverted_key = "/module/facet_analysis/inverted";

static void
facets_mark_sanitize_args(FacetsArgs *args)
{
    args->inverted = !!args->inverted;
}

static void
facets_mark_load_args(GwyContainer *container,
                     FacetsArgs *args)
{
    *args = facets_defaults;

    gwy_container_gis_boolean_by_name(container, inverted_key, &args->inverted);
    facets_mark_sanitize_args(args);
}

static void
facets_mark_save_args(GwyContainer *container,
                      FacetsArgs *args)
{
    gwy_container_set_boolean_by_name(container, inverted_key, args->inverted);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
