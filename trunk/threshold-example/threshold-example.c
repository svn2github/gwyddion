/*
 *  @(#) $Id$
 *  Copyright (C) 2003,2004,2008 David Necas (Yeti)
 *  E-mail: yeti@gwyddion.net
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

#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/stats.h>
#include <libprocess/filters.h>
#include <libgwydgets/gwydgets.h>
#include <app/gwyapp.h>
#include "config.h"

#define THRESHOLD_MOD_NAME PACKAGE_NAME

#define THRESHOLD_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

#define THRESHOLD_FMAP_SIZE 1024

enum {
    THRESHOLD_CHANGE_DATA = 0,
    THRESHOLD_CHANGE_MASK,
    THRESHOLD_CHANGE_PRESENTATION
};

/* Data for this function. */
typedef struct {
    gdouble fractile;
    gsize mode;
    /* interface only */
    gdouble absolute;
    gdouble min;
    gdouble max;
    gsize *fmap;
} ThresholdArgs;

typedef struct {
    GtkObject *fractile;
    GSList *mode;
    /* interface only */
    GtkObject *absolute;
    GwySIValueFormat *format;
    gboolean in_update;
    ThresholdArgs *args;
} ThresholdControls;

static gboolean    module_register                (void);
static void        threshold                      (GwyContainer *data,
                                                   GwyRunType run);
static gboolean    threshold_dialog               (ThresholdArgs *args,
                                                   GwyDataField *dfield);
static GwyDataField* create_auxiliary_data_field  (GwyContainer *data,
                                                   GwyDataField *dfield,
                                                   GQuark quark);
static void        threshold_do                   (GwyContainer *data,
                                                   GwyDataField *dfield,
                                                   GQuark dquark,
                                                   GwyDataField *mfield,
                                                   GQuark mquark,
                                                   GwyDataField *sfield,
                                                   GQuark squark,
                                                   ThresholdArgs *args);
static void        fractile_changed               (GtkAdjustment *adj,
                                                   ThresholdControls *controls);
static void        absolute_changed               (GtkAdjustment *adj,
                                                   ThresholdControls *controls);
static void        mode_changed                   (GtkToggleButton *button,
                                                   ThresholdControls *controls);
static void        threshold_load_args            (GwyContainer *container,
                                                   ThresholdArgs *args);
static void        threshold_save_args            (GwyContainer *container,
                                                   ThresholdArgs *args);
static void        threshold_dialog_update        (ThresholdControls *controls);
static gsize*      threshold_fmap_compute         (GwyDataField *dfield,
                                                   gdouble min,
                                                   gdouble max);
static void        threshold_fmap_fractile_to_abs (ThresholdArgs *args);
static void        threshold_fmap_abs_to_fractile (ThresholdArgs *args);

ThresholdArgs threshold_defaults = {
    0.5,
    THRESHOLD_CHANGE_DATA,
    0,
    0,
    0,
    NULL,
};

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "This is an example module.  Splits the values to only two distinct ones "
        "using either an absolute threshold or a fractile value.",
    "Yeti <yeti@gwyddion.net>",
    PACKAGE_VERSION,
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

/* Module registering function.
 * Called at Gwyddion startup and registeres one or more function.
 */
static gboolean
module_register(void)
{
    gwy_process_func_register(THRESHOLD_MOD_NAME,
                              (GwyProcessFunc)&threshold,
                              N_("/_Test/_Threshold..."),
                              NULL,
                              THRESHOLD_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Threshold data."));

    return TRUE;
}

static void
threshold(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield, *mfield, *sfield;
    GQuark dquark, mquark, squark;
    ThresholdArgs args;
    gboolean ok;

    g_return_if_fail(run & THRESHOLD_RUN_MODES);
    /* Obtain the current data and mask fields */
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD_KEY, &dquark,
                                     GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_MASK_FIELD_KEY, &mquark,
                                     GWY_APP_MASK_FIELD, &mfield,
                                     GWY_APP_SHOW_FIELD_KEY, &squark,
                                     GWY_APP_SHOW_FIELD, &sfield,
                                     NULL);
    /* Set up parameters */
    args = threshold_defaults;
    threshold_load_args(gwy_app_settings_get(), &args);
    gwy_data_field_get_min_max(dfield, &args.min, &args.max);
    args.fmap = threshold_fmap_compute(dfield, args.min, args.max);
    threshold_fmap_fractile_to_abs(&args);

    /* Possibly present the GUI */
    ok = (run != GWY_RUN_INTERACTIVE) || threshold_dialog(&args, dfield);
    if (ok) {
        threshold_do(data, dfield, dquark, mfield, mquark, sfield, squark,
                     &args);
        threshold_save_args(gwy_app_settings_get(), &args);
    }
}

static void
threshold_do(GwyContainer *data,
             GwyDataField *dfield,
             GQuark dquark,
             GwyDataField *mfield,
             GQuark mquark,
             GwyDataField *sfield,
             GQuark squark,
             ThresholdArgs *args)
{
    switch (args->mode) {
        case THRESHOLD_CHANGE_DATA:
        /* Save current data field for undo */
        gwy_app_undo_qcheckpointv(data, 1, &dquark);
        /* Do threshold */
        gwy_data_field_threshold(dfield, args->absolute, args->min, args->max);
        /* Signal views showing this data field it has changed -- do this
         * after all changes are done. */
        gwy_data_field_data_changed(dfield);
        break;

        case THRESHOLD_CHANGE_MASK:
        /* Save current mask field for undo */
        gwy_app_undo_qcheckpointv(data, 1, &mquark);
        /* If there is no mask yet, create one */
        if (!mfield)
            mfield = create_auxiliary_data_field(data, dfield, mquark);
        gwy_data_field_copy(dfield, sfield, FALSE);
        /* Create mask by threshold */
        gwy_data_field_threshold(mfield, args->absolute, 0.0, 1.0);
        /* Signal views showing this mask field it has changed -- do this
         * after all changes are done. */
        gwy_data_field_data_changed(mfield);
        break;

        case THRESHOLD_CHANGE_PRESENTATION:
        /* Save current presentation field for undo */
        gwy_app_undo_qcheckpointv(data, 1, &squark);
        /* If there is no presentation yet, create one */
        if (!sfield)
            sfield = create_auxiliary_data_field(data, dfield, squark);
        gwy_data_field_copy(dfield, sfield, FALSE);
        /* Do threshold */
        gwy_data_field_threshold(sfield, args->absolute, args->min, args->max);
        /* Signal views showing this presentation it has changed -- do this
         * after all changes are done. */
        gwy_data_field_data_changed(sfield);
        break;

        default:
        g_assert_not_reached();
        break;
    }
}

static GwyDataField*
create_auxiliary_data_field(GwyContainer *data,
                            GwyDataField *dfield,
                            GQuark quark)
{
    GwySIUnit *siunit;
    GwyDataField *afield;

    afield = gwy_data_field_new_alike(dfield, FALSE);
    siunit = gwy_si_unit_new(NULL);
    gwy_data_field_set_si_unit_z(afield, siunit);
    g_object_unref(siunit);
    gwy_container_set_object(data, quark, afield);
    g_object_unref(afield);

    return afield;
}

static gboolean
threshold_dialog(ThresholdArgs *args,
                 GwyDataField *dfield)
{
    GtkWidget *dialog, *table, *spin, *widget;
    GSList *group;
    GwySIValueFormat *fmt;
    ThresholdControls controls;
    enum { RESPONSE_RESET = 1 };
    gint response;

    controls.args = args;

    dialog = gtk_dialog_new_with_buttons(_("Threshold"), NULL, 0,
                                         _("Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    table = gtk_table_new(7, 3, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table,
                       FALSE, FALSE, 4);

    controls.fractile = gtk_adjustment_new(100*args->fractile,
                                           0, 100, 1, 10, 0);
    spin = gwy_table_attach_spinbutton(table, 0, _("_Fractile:"), "%",
                                       controls.fractile);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 0);
    gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(spin), TRUE);
    g_signal_connect(controls.fractile, "value_changed",
                     G_CALLBACK(fractile_changed), &controls);

    fmt = gwy_data_field_get_value_format_z(dfield,
                                            GWY_SI_UNIT_FORMAT_VFMARKUP, NULL);
    controls.format = fmt;
    controls.absolute = gtk_adjustment_new(args->absolute/fmt->magnitude,
                                           args->min/fmt->magnitude,
                                           args->max/fmt->magnitude,
                                           (args->max - args->min)/100
                                                /fmt->magnitude,
                                           (args->max - args->min)/10
                                                /fmt->magnitude,
                                           0);
    spin = gwy_table_attach_spinbutton(table, 1, _("Absolute _threshold:"),
                                       fmt->units,
                                       controls.absolute);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), fmt->precision);
    g_signal_connect(controls.absolute, "value_changed",
                     G_CALLBACK(absolute_changed), &controls);

    widget = gtk_label_new(_("Modify:"));
    gtk_misc_set_alignment(GTK_MISC(widget), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), widget, 0, 2, 2, 3,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);
    group = gwy_radio_buttons_createl(G_CALLBACK(mode_changed), &controls,
                                      args->mode,
                                      _("_Data"),
                                      THRESHOLD_CHANGE_DATA,
                                      _("_Mask"),
                                      THRESHOLD_CHANGE_MASK,
                                      _("_Presentation"),
                                      THRESHOLD_CHANGE_PRESENTATION,
                                      NULL);
    gwy_radio_buttons_attach_to_table(group, GTK_TABLE(table), 3, 3);
    controls.mode = group;
    controls.in_update = FALSE;

    gtk_widget_show_all(dialog);
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            g_free(controls.format);
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            return FALSE;
            break;

            case GTK_RESPONSE_OK:
            break;

            case RESPONSE_RESET:
            args->fractile = threshold_defaults.fractile;
            args->mode = threshold_defaults.mode;
            threshold_fmap_fractile_to_abs(args);
            threshold_dialog_update(&controls);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    g_free(controls.format);
    gtk_widget_destroy(dialog);

    return TRUE;
}

static void
fractile_changed(GtkAdjustment *adj,
                 ThresholdControls *controls)
{
    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    controls->args->fractile = gtk_adjustment_get_value(adj)/100;
    threshold_fmap_fractile_to_abs(controls->args);
    threshold_dialog_update(controls);
    controls->in_update = FALSE;
}

static void
absolute_changed(GtkAdjustment *adj,
                 ThresholdControls *controls)
{
    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    controls->args->absolute = gtk_adjustment_get_value(adj)
                               *controls->format->magnitude;
    threshold_fmap_abs_to_fractile(controls->args);
    threshold_dialog_update(controls);
    controls->in_update = FALSE;
}

static void
mode_changed(GtkToggleButton *button,
             ThresholdControls *controls)
{
    if (!gtk_toggle_button_get_active(button))
        return;

    controls->args->mode = gwy_radio_buttons_get_current(controls->mode);
}

static const gchar fractile_key[] = "/module/" THRESHOLD_MOD_NAME "/fractile";
static const gchar mode_key[] = "/module/" THRESHOLD_MOD_NAME "/mode";

static void
threshold_load_args(GwyContainer *container,
                    ThresholdArgs *args)
{
    *args = threshold_defaults;

    if (gwy_container_contains_by_name(container, fractile_key))
        args->fractile = gwy_container_get_double_by_name(container,
                                                          fractile_key);
    args->fractile = CLAMP(args->fractile, 0.0, 1.0);
    if (gwy_container_contains_by_name(container, mode_key))
        args->mode = gwy_container_get_int32_by_name(container, mode_key);
    args->mode = CLAMP(args->mode, 0, 2);
}

static void
threshold_save_args(GwyContainer *container,
                ThresholdArgs *args)
{
    gwy_container_set_double_by_name(container, fractile_key,
                                     args->fractile);
    gwy_container_set_int32_by_name(container, mode_key,
                                    args->mode);
}

static void
threshold_dialog_update(ThresholdControls *controls)
{
    ThresholdArgs *args;
    GSList *l;

    args = controls->args;
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->fractile),
                             100*args->fractile);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->absolute),
                             args->absolute/controls->format->magnitude);

    for (l = controls->mode; l; l = g_slist_next(l)) {
        if (GPOINTER_TO_UINT(g_object_get_data(l->data, "mode")) == args->mode)
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(l->data), TRUE);
    }
}

static gsize*
threshold_fmap_compute(GwyDataField *dfield,
                       gdouble min,
                       gdouble max)
{
    gsize *fmap;
    gdouble *data;
    gsize i, n;

    fmap = g_new0(gsize, THRESHOLD_FMAP_SIZE);
    n = gwy_data_field_get_xres(dfield)*gwy_data_field_get_yres(dfield);
    data = gwy_data_field_get_data(dfield);

    for ( ; n; data++, n--) {
        i = floor((*data - min)/(max - min)*0.999999*THRESHOLD_FMAP_SIZE);
        fmap[i]++;
    }

    for (i = 1; i < THRESHOLD_FMAP_SIZE; i++)
        fmap[i] += fmap[i-1];

    return fmap;
}

static void
threshold_fmap_fractile_to_abs(ThresholdArgs *args)
{
    gsize n, x, low, hi, i;

    n = args->fmap[THRESHOLD_FMAP_SIZE-1];
    x = ROUND(n*args->fractile);
    low = 0;
    hi = THRESHOLD_FMAP_SIZE-1;
    do {
        i = (low + hi)/2;
        if (args->fmap[i] > x)
            hi = i;
        else
            low = i;
    } while (hi - low > 1);
    if (args->fmap[hi] == x)
        low = hi;

    args->absolute = low/(THRESHOLD_FMAP_SIZE-1.0)*(args->max - args->min)
                     + args->min;
}

static void
threshold_fmap_abs_to_fractile(ThresholdArgs *args)
{
    gsize i;

    i = floor((args->absolute - args->min)/(args->max - args->min)
              *0.999999*THRESHOLD_FMAP_SIZE);

    args->fractile = (gdouble)args->fmap[i]/args->fmap[THRESHOLD_FMAP_SIZE-1];
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
