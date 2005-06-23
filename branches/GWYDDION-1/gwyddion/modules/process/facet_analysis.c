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
    (GWY_RUN_MODAL)

enum {
    PREVIEW_SIZE = 320,
    FDATA_RES = 201,
    MAX_LENGTH = 1024
};

typedef struct {
    gdouble tolerance;
    /* Interface only */
    gdouble theta0;
    gdouble phi0;
} FacetsArgs;

typedef struct {
    gboolean in_update;
    GtkWidget *inverted;
    GtkWidget *view;
    GtkWidget *fview;
    GtkWidget *theta_label;
    GtkWidget *phi_label;
    GtkWidget *mtheta_label;
    GtkWidget *mphi_label;
    GtkObject *tolerance;
    GtkWidget *color_button;
    GwyContainer *mydata;
    GwyContainer *fdata;
    FacetsArgs *args;
} FacetsControls;

static gboolean module_register                     (const gchar *name);
static gboolean facets_analyse                      (GwyContainer *data,
                                                     GwyRunType run);
static void     load_mask_color                     (GtkWidget *color_button,
                                                     GwyContainer *data);
static void     save_mask_color                     (GtkWidget *color_button,
                                                     GwyContainer *data);
static gboolean facets_mark_dialog                  (FacetsArgs *args,
                                                     GwyContainer *data,
                                                     GwyDataField **dtheta,
                                                     GwyDataField **dphi);
static void     facets_mark_dialog_update_controls  (FacetsControls *controls,
                                                     FacetsArgs *args);
static void     facets_mark_dialog_update_values    (FacetsControls *controls,
                                                     FacetsArgs *args);
static void     facet_view_selection_updated        (GwyVectorLayer *layer,
                                                     FacetsControls *controls);
static void     preview_selection_updated           (GwyVectorLayer *layer,
                                                     FacetsControls *controls);
static void     mask_color_change_cb                (GtkWidget *color_button,
                                                     FacetsControls *controls);
static void     gwy_data_field_mark_facets          (GwyDataField *dtheta,
                                                     GwyDataField *dphi,
                                                     gdouble theta0,
                                                     gdouble phi0,
                                                     gdouble tolerance,
                                                     GwyDataField *mask);
static void     calculate_average_angle             (GwyDataField *dtheta,
                                                     GwyDataField *dphi,
                                                     gdouble theta0,
                                                     gdouble phi0,
                                                     gdouble radius,
                                                     gdouble *theta,
                                                     gdouble *phi);
static void     gwy_data_field_facet_distribution   (GwyDataField *dfield,
                                                     gint kernel_size,
                                                     GwyContainer *container);
static void     compute_slopes                      (GwyDataField *dfield,
                                                     gint kernel_size,
                                                     GwyDataField *xder,
                                                     GwyDataField *yder);
static void     preview                             (FacetsControls *controls,
                                                     FacetsArgs *args);
static void     facets_mark_do                      (FacetsArgs *args,
                                                     GwyContainer *data,
                                                     GwyDataField *dtheta,
                                                     GwyDataField *dphi);
static void     facets_mark_load_args               (GwyContainer *container,
                                                     FacetsArgs *args);
static void     facets_mark_save_args               (GwyContainer *container,
                                                     FacetsArgs *args);

static const FacetsArgs facets_defaults = {
    2.0*G_PI/180.0,
    /* Interface only */
    0.0,
    0.0,
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
        N_("/_Statistics/Facet _Analysis..."),
        (GwyProcessFunc)&facets_analyse,
        FACETS_RUN_MODES,
        0,
    };

    gwy_process_func_register(name, &facet_analysis_func_info);

    return TRUE;
}


static gboolean
facets_analyse(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dtheta = NULL, *dphi = NULL;
    FacetsArgs args;
    gboolean ok = FALSE;

    g_return_val_if_fail(run & FACETS_RUN_MODES, FALSE);
    g_return_val_if_fail(g_type_from_name("GwyLayerPoints"), FALSE);
    facets_mark_load_args(gwy_app_settings_get(), &args);
    ok = facets_mark_dialog(&args, data, &dtheta, &dphi);
    facets_mark_save_args(gwy_app_settings_get(), &args);
    if (ok) {
        gwy_app_undo_checkpoint(data, "/0/mask", NULL);
        facets_mark_do(&args, data, dtheta, dphi);
    }
    g_object_unref(dtheta);
    g_object_unref(dphi);

    return ok;
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
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, *row, *row+1,
                     GTK_FILL, 0, 2, 2);

    label = gtk_label_new(NULL);
    gtk_widget_set_size_request(label, req.width, -1);
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, *row, *row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);
    (*row)++;

    return label;
}

static gboolean
facets_mark_dialog(FacetsArgs *args,
                   GwyContainer *data,
                   GwyDataField **dtheta,
                   GwyDataField **dphi)
{
    GtkWidget *dialog, *table, *hbox, *hbox2, *vbox, *label, *scale;
    FacetsControls controls;
    enum {
        RESPONSE_RESET = 1,
        RESPONSE_PREVIEW = 2
    };
    gint response;
    gdouble zoomval;
    GtkObject *layer;
    GtkObject *vlayer;
    GwyDataField *dfield;
    gint row;

    controls.in_update = FALSE;
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

    vlayer = g_object_new(g_type_from_name("GwyLayerPoints"), NULL);
    g_object_set(G_OBJECT(vlayer), "max_points", 1, NULL);
    gwy_data_view_set_top_layer(GWY_DATA_VIEW(controls.view),
                                GWY_VECTOR_LAYER(vlayer));
    g_signal_connect(vlayer, "updated",
                     G_CALLBACK(preview_selection_updated), &controls);

    gtk_box_pack_start(GTK_BOX(hbox), controls.view, FALSE, FALSE, 4);

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);

    hbox2 = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(vbox), hbox2, FALSE, FALSE, 0);

    /* Slope view */
    controls.fdata = GWY_CONTAINER(gwy_container_new());
    gwy_data_field_facet_distribution(dfield, 3, controls.fdata);
    *dtheta = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls.fdata,
                                                              "/theta"));
    *dphi = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls.fdata,
                                                            "/phi"));

    controls.fview = gwy_data_view_new(controls.fdata);
    g_object_unref(controls.fdata);
    gtk_box_pack_start(GTK_BOX(hbox2), controls.fview, FALSE, FALSE, 0);

    layer = gwy_layer_basic_new();
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.fview),
                                 GWY_PIXMAP_LAYER(layer));

    vlayer = g_object_new(g_type_from_name("GwyLayerPoints"), NULL);
    g_object_set(G_OBJECT(vlayer), "max_points", 1, NULL);
    gwy_data_view_set_top_layer(GWY_DATA_VIEW(controls.fview),
                                GWY_VECTOR_LAYER(vlayer));
    g_signal_connect(vlayer, "updated",
                     G_CALLBACK(facet_view_selection_updated), &controls);

    /* Info table */
    table = gtk_table_new(6, 2, FALSE);
    gtk_box_pack_start(GTK_BOX(hbox2), table, TRUE, TRUE, 4);
    row = 0;

    label = gtk_label_new("");
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Inclination</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 2, row, row+1,
                     GTK_FILL, 0, 2, 2);
    row++;

    controls.theta_label = add_angle_label(table, _("Theta:"), &row);
    controls.phi_label = add_angle_label(table, _("Phi:"), &row);
    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);

    label = gtk_label_new("");
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Mean Inclination</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 2, row, row+1,
                     GTK_FILL, 0, 2, 2);
    row++;

    controls.mtheta_label = add_angle_label(table, _("Theta:"), &row);
    controls.mphi_label = add_angle_label(table, _("Phi:"), &row);

    table = gtk_table_new(10, 4, FALSE);
    gtk_box_pack_start(GTK_BOX(vbox), table, TRUE, TRUE, 4);
    row = 0;

    controls.tolerance = gtk_adjustment_new(args->tolerance*180.0/G_PI,
                                            0.0, 12.0, 0.01, 0.1, 0);
    scale = gwy_table_attach_hscale(table, row++, _("Tolerance:"), "deg",
                                    controls.tolerance, 0);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(scale), 3);

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
    GwyDataField *dtheta, *dphi;
    gdouble selection[2];
    gdouble theta, phi, x, y, q;
    gchar s[24];

    q = gwy_container_get_double_by_name(controls->fdata, "/q");
    gwy_vector_layer_get_selection(layer, selection);
    /* q/G_SQRT2/G_SQRT2 is correction to coordinate of pixel centre instead
     * of edge */
    x = selection[0] - G_SQRT2/q + q/G_SQRT2/FDATA_RES;
    y = G_SQRT2/q - selection[1] - q/G_SQRT2/FDATA_RES;
    xy_to_angles(x, y, &theta, &phi);

    g_snprintf(s, sizeof(s), "%.2f deg", 180.0/G_PI*theta);
    gtk_label_set_text(GTK_LABEL(controls->theta_label), s);
    controls->args->theta0 = theta;

    g_snprintf(s, sizeof(s), "%.2f deg", 180.0/G_PI*phi);
    gtk_label_set_text(GTK_LABEL(controls->phi_label), s);
    controls->args->phi0 = -phi;

    if (!controls->in_update) {
        layer = gwy_data_view_get_top_layer(GWY_DATA_VIEW(controls->view));
        if (gwy_vector_layer_get_selection(layer, NULL))
            gwy_vector_layer_unselect(layer);
    }

    dtheta = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->fdata,
                                                             "/theta"));
    dphi = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->fdata,
                                                           "/phi"));
    /*
    calculate_average_angle(dtheta, dphi, theta, phi,
                            2*G_PI/180.0, &theta, &phi);

    g_snprintf(s, sizeof(s), "%.2f deg", 180.0/G_PI*theta);
    gtk_label_set_text(GTK_LABEL(controls->mtheta_label), s);
    g_snprintf(s, sizeof(s), "%.2f deg", 180.0/G_PI*phi);
    gtk_label_set_text(GTK_LABEL(controls->mphi_label), s);
    */
}

static void
preview_selection_updated(GwyVectorLayer *layer,
                          FacetsControls *controls)
{
    GwyDataField *dfield;
    gdouble selection[2];
    gdouble theta, phi, x, y, q;
    gint i, j;

    if (controls->in_update)
        return;

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                             "/0/data"));
    if (!gwy_vector_layer_get_selection(layer, selection))
        return;

    controls->in_update = TRUE;
    j = gwy_data_field_rtoj(dfield, selection[0]);
    i = gwy_data_field_rtoi(dfield, selection[1]);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->fdata,
                                                             "/theta"));
    theta = gwy_data_field_get_val(dfield, j, i);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->fdata,
                                                             "/phi"));
    phi = gwy_data_field_get_val(dfield, j, i);

    angles_to_xy(theta, phi, &x, &y);
    q = gwy_container_get_double_by_name(controls->fdata, "/q");
    /* q/G_SQRT2/G_SQRT2 is correction to coordinate of pixel centre instead
     * of edge */
    selection[0] = x + G_SQRT2/q - q/G_SQRT2/FDATA_RES;
    selection[1] = y + G_SQRT2/q - q/G_SQRT2/FDATA_RES;
    layer = gwy_data_view_get_top_layer(GWY_DATA_VIEW(controls->fview));
    gwy_vector_layer_set_selection(layer, 1, selection);
    controls->in_update = FALSE;
}

static void
facets_mark_do(FacetsArgs *args,
               GwyContainer *data,
               GwyDataField *dtheta,
               GwyDataField *dphi)
{
    GObject *dfield, *mask;

    dfield = gwy_container_get_object_by_name(data, "/0/data");

    if (!gwy_container_gis_object_by_name(data, "/0/mask", &mask)) {
        mask = gwy_serializable_duplicate(dfield);
        gwy_container_set_object_by_name(data, "/0/mask", mask);
        g_object_unref(mask);
    }
    gwy_data_field_mark_facets(dtheta, dphi,
                               args->theta0, args->phi0, args->tolerance,
                               GWY_DATA_FIELD(mask));
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
                        gdouble theta0,
                        gdouble phi0,
                        gdouble radius,
                        gdouble *theta,
                        gdouble *phi)
{
    gdouble cr, cth0, sth0, cp0, sp0, cro, ct, st, cp, sp;
    gdouble sx, sy, sz;
    const gdouble *xd, *yd;
    gint i, n;

    cr = cos(radius);
    cth0 = cos(theta0);
    sth0 = sin(theta0);
    cp0 = cos(phi0);
    sp0 = sin(phi0);

    xd = gwy_data_field_get_data_const(dtheta);
    yd = gwy_data_field_get_data_const(dphi);
    n = 0;
    sx = sy = sz = 0.0;
    for (i = gwy_data_field_get_xres(dtheta)*gwy_data_field_get_yres(dtheta);
         i;
         i--, xd++, yd++) {
        ct = cos(*xd);
        st = sin(*xd);
        cp = cos(*yd);
        sp = sin(*yd);
        cro = cth0*ct + sth0*st*(cp0*cp + sp0*sp);
        if (cro >= cr) {
            sx += st*cp;
            sy += st*sp;
            sz += ct;
            n++;
        }
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
    gwy_container_set_object_by_name(container, "/phi", (GObject*)dphi);
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
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->tolerance),
                             args->tolerance*180.0/G_PI);
}

static void
facets_mark_dialog_update_values(FacetsControls *controls,
                                FacetsArgs *args)
{
    args->tolerance
        = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->tolerance));
    args->tolerance *= G_PI/180.0;
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
    GwyDataField *mask, *dfield, *dtheta, *dphi;
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

    dtheta = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->fdata,
                                                             "/theta"));
    dphi = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->fdata,
                                                             "/phi"));
    facets_mark_do(args, controls->mydata, dtheta, dphi);

    gwy_data_view_update(GWY_DATA_VIEW(controls->view));

}

static const gchar *tolerance_key = "/module/facet_analysis/tolerance";

static void
facets_mark_sanitize_args(FacetsArgs *args)
{
    args->tolerance = CLAMP(args->tolerance, 0.0, 12.0*G_PI/180.0);
}

static void
facets_mark_load_args(GwyContainer *container,
                     FacetsArgs *args)
{
    *args = facets_defaults;

    gwy_container_gis_double_by_name(container, tolerance_key,
                                     &args->tolerance);
    facets_mark_sanitize_args(args);
}

static void
facets_mark_save_args(GwyContainer *container,
                      FacetsArgs *args)
{
    gwy_container_set_double_by_name(container, tolerance_key,
                                     args->tolerance);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
