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

#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwydgets.h>
#include <app/settings.h>
#include <app/app.h>

#define SCALE_RUN_MODES \
    (GWY_RUN_MODAL | GWY_RUN_NONINTERACTIVE | GWY_RUN_WITH_DEFAULTS)

/* Data for this function. */
typedef struct {
    gdouble ratio;
    GwyInterpolationType interp;
    /* interface only */
    gint xres;
    gint yres;
} ScaleArgs;

typedef struct {
    GtkObject *ratio;
    GtkWidget *interp;
    /* interface only */
    GtkObject *xres;
    GtkObject *yres;
    gboolean in_update;
} ScaleControls;

static gboolean    module_register           (const gchar *name);
static gboolean    scale                     (GwyContainer *data,
                                              GwyRunType run);
static gboolean    scale_dialog              (ScaleArgs *args);
static void        interp_changed_cb         (GObject *item,
                                              ScaleArgs *args);
static void        scale_changed_cb          (GtkAdjustment *adj,
                                              ScaleArgs *args);
static void        width_changed_cb          (GtkAdjustment *adj,
                                              ScaleArgs *args);
static void        height_changed_cb         (GtkAdjustment *adj,
                                              ScaleArgs *args);
static void        scale_dialog_update       (ScaleControls *controls,
                                              ScaleArgs *args);
static void        scale_sanitize_args       (ScaleArgs *args);
static void        scale_load_args           (GwyContainer *container,
                                              ScaleArgs *args);
static void        scale_save_args           (GwyContainer *container,
                                              ScaleArgs *args);

ScaleArgs scale_defaults = {
    1.0,
    GWY_INTERPOLATION_BILINEAR,
    0,
    0,
};

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Scales data by arbitrary factor."),
    "Yeti <yeti@gwyddion.net>",
    "1.3",
    "David Nečas (Yeti) & Petr Klapetek",
    "2003",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyProcessFuncInfo scale_func_info = {
        "scale",
        N_("/_Basic Operations/Scale..."),
        (GwyProcessFunc)&scale,
        SCALE_RUN_MODES,
        0,
    };

    gwy_process_func_register(name, &scale_func_info);

    return TRUE;
}

static gboolean
scale(GwyContainer *data, GwyRunType run)
{
    GtkWidget *data_window;
    GwyDataField *dfield;
    ScaleArgs args;
    gboolean ok;

    g_return_val_if_fail(run & SCALE_RUN_MODES, FALSE);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    if (run == GWY_RUN_WITH_DEFAULTS)
        args = scale_defaults;
    else
        scale_load_args(gwy_app_settings_get(), &args);
    args.xres = gwy_data_field_get_xres(dfield);
    args.yres = gwy_data_field_get_yres(dfield);
    ok = (run != GWY_RUN_MODAL) || scale_dialog(&args);
    if (run == GWY_RUN_MODAL)
        scale_save_args(gwy_app_settings_get(), &args);
    if (!ok)
        return FALSE;

    data = gwy_container_duplicate(data);
    gwy_app_clean_up_data(data);
    dfield = gwy_container_get_object_by_name(data, "/0/data");
    gwy_data_field_resample(dfield,
                            ROUND(args.ratio*args.xres),
                            ROUND(args.ratio*args.yres),
                            args.interp);
    if (gwy_container_gis_object_by_name(data, "/0/mask", &dfield))
        gwy_data_field_resample(dfield,
                                ROUND(args.ratio*args.xres),
                                ROUND(args.ratio*args.yres),
                                args.interp);
    if (gwy_container_gis_object_by_name(data, "/0/show", &dfield))
        gwy_data_field_resample(dfield,
                                ROUND(args.ratio*args.xres),
                                ROUND(args.ratio*args.yres),
                                args.interp);
    data_window = gwy_app_data_window_create(data);
    gwy_app_data_window_set_untitled(GWY_DATA_WINDOW(data_window), NULL);

    return FALSE;
}

static gboolean
scale_dialog(ScaleArgs *args)
{
    GtkWidget *dialog, *table, *spin;
    ScaleControls controls;
    enum { RESPONSE_RESET = 1 };
    gint response;

    dialog = gtk_dialog_new_with_buttons(_("Scale"), NULL, 0,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    table = gtk_table_new(4, 4, FALSE);
    gtk_table_set_col_spacings(GTK_TABLE(table), 4);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table,
                       FALSE, FALSE, 4);

    controls.ratio = gtk_adjustment_new(args->ratio,
                                        2.0/MIN(args->xres, args->yres),
                                        4096.0/MAX(args->xres, args->yres),
                                        0.01, 0.2, 0);
    spin = gwy_table_attach_hscale(table, 0, _("Scale by _ratio:"), NULL,
                                   controls.ratio, GWY_HSCALE_LOG);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 3);
    g_object_set_data(G_OBJECT(controls.ratio), "controls", &controls);
    g_signal_connect(controls.ratio, "value-changed",
                     G_CALLBACK(scale_changed_cb), args);

    controls.xres = gtk_adjustment_new(args->ratio*args->xres,
                                       2, 4096, 1, 10, 0);
    spin = gwy_table_attach_hscale(table, 1, _("New _width:"), "px",
                                   controls.xres, GWY_HSCALE_LOG);
    g_object_set_data(G_OBJECT(controls.xres), "controls", &controls);
    g_signal_connect(controls.xres, "value-changed",
                     G_CALLBACK(width_changed_cb), args);

    controls.yres = gtk_adjustment_new(args->ratio*args->yres,
                                       2, 4096, 1, 10, 0);
    spin = gwy_table_attach_hscale(table, 2, _("New _height:"), "px",
                                   controls.yres, GWY_HSCALE_LOG);
    g_object_set_data(G_OBJECT(controls.yres), "controls", &controls);
    g_signal_connect(controls.yres, "value-changed",
                     G_CALLBACK(height_changed_cb), args);

    controls.interp
        = gwy_option_menu_interpolation(G_CALLBACK(interp_changed_cb),
                                        args, args->interp);
    gwy_table_attach_hscale(table, 3, _("_Interpolation type:"), NULL,
                            GTK_OBJECT(controls.interp), GWY_HSCALE_WIDGET);

    controls.in_update = FALSE;
    scale_dialog_update(&controls, args);

    gtk_widget_show_all(dialog);
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            args->ratio
                = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls.ratio));
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            return FALSE;
            break;

            case GTK_RESPONSE_OK:
            break;

            case RESPONSE_RESET:
            args->ratio = scale_defaults.ratio;
            args->interp = scale_defaults.interp;
            scale_dialog_update(&controls, args);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    args->ratio = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls.ratio));
    gtk_widget_destroy(dialog);

    return TRUE;
}

static void
interp_changed_cb(GObject *item,
                  ScaleArgs *args)
{
    args->interp
        = GPOINTER_TO_INT(g_object_get_data(item, "interpolation-type"));
}

static void
scale_changed_cb(GtkAdjustment *adj,
                 ScaleArgs *args)
{
    ScaleControls *controls;

    controls = g_object_get_data(G_OBJECT(adj), "controls");
    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    args->ratio = gtk_adjustment_get_value(adj);
    scale_dialog_update(controls, args);
    controls->in_update = FALSE;
}

static void
width_changed_cb(GtkAdjustment *adj,
                 ScaleArgs *args)
{
    ScaleControls *controls;

    controls = g_object_get_data(G_OBJECT(adj), "controls");
    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    args->ratio = gtk_adjustment_get_value(adj)/args->xres;
    scale_dialog_update(controls, args);
    controls->in_update = FALSE;
}

static void
height_changed_cb(GtkAdjustment *adj,
                  ScaleArgs *args)
{
    ScaleControls *controls;

    controls = g_object_get_data(G_OBJECT(adj), "controls");
    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    args->ratio = gtk_adjustment_get_value(adj)/args->yres;
    scale_dialog_update(controls, args);
    controls->in_update = FALSE;
}

static void
scale_dialog_update(ScaleControls *controls,
                    ScaleArgs *args)
{
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->ratio),
                             args->ratio);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->xres),
                             args->ratio*args->xres);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->yres),
                             args->ratio*args->yres);
    gwy_option_menu_set_history(controls->interp, "interpolation-type",
                                args->interp);
}

static const gchar *ratio_key = "/module/scale/ratio";
static const gchar *interp_key = "/module/scale/interp";

static void
scale_sanitize_args(ScaleArgs *args)
{
    args->ratio = CLAMP(args->ratio, 0.01, 100.0);
    args->interp = CLAMP(args->interp,
                         GWY_INTERPOLATION_ROUND, GWY_INTERPOLATION_NNA);
}

static void
scale_load_args(GwyContainer *container,
                ScaleArgs *args)
{
    *args = scale_defaults;

    gwy_container_gis_double_by_name(container, ratio_key, &args->ratio);
    gwy_container_gis_enum_by_name(container, interp_key, &args->interp);
    scale_sanitize_args(args);
}

static void
scale_save_args(GwyContainer *container,
                ScaleArgs *args)
{
    gwy_container_set_double_by_name(container, ratio_key, args->ratio);
    gwy_container_set_enum_by_name(container, interp_key, args->interp);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
