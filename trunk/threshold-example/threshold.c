/*
 *  threshold.c
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@physics.muni.cz, klapetek@physics.muni.cz.
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

#include <math.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwydgets.h>
#include <app/settings.h>
#include <app/app.h>

#define THRESHOLD_MOD_NAME PACKAGE

#define THRESHOLD_RUN_MODES \
    (GWY_RUN_MODAL | GWY_RUN_NONINTERACTIVE | GWY_RUN_WITH_DEFAULTS)

#define ROUND(x) ((gint)floor((x) + 0.5))

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
    gdouble mag;
    gint precision;
    gboolean in_update;
} ThresholdControls;

static gboolean    module_register                (const gchar *name);
static gboolean    threshold                      (GwyContainer *data,
                                                   GwyRunType run);
static gboolean    threshold_dialog               (ThresholdArgs *args);
static void        threshold_do                   (GwyContainer *data,
                                                   GwyDataField *dfield,
                                                   ThresholdArgs *args);
static void        fractile_changed_cb            (GtkAdjustment *adj,
                                                   ThresholdArgs *args);
static void        absolute_changed_cb            (GtkAdjustment *adj,
                                                   ThresholdArgs *args);
static void        threshold_load_args            (GwyContainer *container,
                                                   ThresholdArgs *args);
static void        threshold_save_args            (GwyContainer *container,
                                                   ThresholdArgs *args);
static void        threshold_dialog_update        (ThresholdControls *controls,
                                                   ThresholdArgs *args);
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
    THRESHOLD_MOD_NAME,
    "This is an example module.  Splits the values to only two distinct ones "
        "using either an absolute threshold or a fractile value.",
    "Yeti <yeti@physics.muni.cz>",
    VERSION,
    "David Nečas (Yeti) & Petr Klapetek",
    "2003",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

/* Module registering function.
 * Called at Gwyddion startup and registeres one or more function.
 */
static gboolean
module_register(const gchar *name)
{
    static GwyProcessFuncInfo threshold_func_info = {
        "threshold",
        "/_Test/_Threshold",
        (GwyProcessFunc)&threshold,
        THRESHOLD_RUN_MODES,
    };

    gwy_process_func_register(name, &threshold_func_info);

    return TRUE;
}

static gboolean
threshold(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield;
    ThresholdArgs args;
    gboolean ok;

    g_return_val_if_fail(run & THRESHOLD_RUN_MODES, FALSE);
    /* load saved arguments or use defaults */
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    if (run == GWY_RUN_WITH_DEFAULTS)
        args = threshold_defaults;
    else
        threshold_load_args(gwy_app_settings_get(), &args);
    args.min = gwy_data_field_get_min(dfield);
    args.max = gwy_data_field_get_max(dfield);
    args.fmap = threshold_fmap_compute(dfield, args.min, args.max);
    threshold_fmap_fractile_to_abs(&args);

    /* eventually present the GUI */
    ok = (run != GWY_RUN_MODAL) || threshold_dialog(&args);
    if (ok) {
        threshold_do(data, dfield, &args);
        if (run != GWY_RUN_WITH_DEFAULTS)
            threshold_save_args(gwy_app_settings_get(), &args);
    }

    return TRUE;
}

static void
threshold_do(GwyContainer *data,
             GwyDataField *dfield,
             ThresholdArgs *args)
{
    GwyDataField *mask, *show;
    gdouble *p, *q;
    gsize n;

    switch (args->mode) {
        case THRESHOLD_CHANGE_DATA:
        gwy_app_undo_checkpoint(data, "/0/data");
        gwy_data_field_threshold(dfield, args->absolute, args->min, args->max);
        break;

        case THRESHOLD_CHANGE_MASK:
        gwy_app_undo_checkpoint(data, "/0/mask");
        if (gwy_container_contains_by_name(data, "/0/mask")) {
            mask = GWY_DATA_FIELD(gwy_container_get_object_by_name(data,
                                                                   "/0/mask"));
            gwy_data_field_resample(mask,
                                    gwy_data_field_get_xres(dfield),
                                    gwy_data_field_get_yres(dfield),
                                    GWY_INTERPOLATION_NONE);
        }
        else {
            mask = GWY_DATA_FIELD(gwy_serializable_duplicate(G_OBJECT(dfield)));
            gwy_container_set_object_by_name(data, "/0/mask", G_OBJECT(mask));
        }

        n = gwy_data_field_get_xres(dfield)*gwy_data_field_get_yres(dfield);
        p = gwy_data_field_get_data(dfield);
        q = gwy_data_field_get_data(mask);
        for ( ; n; n--, p++, q++)
            *q = (*p > args->absolute) ? 1.0 : 0.0;
        break;

        case THRESHOLD_CHANGE_PRESENTATION:
        gwy_app_undo_checkpoint(data, "/0/show");
        if (gwy_container_contains_by_name(data, "/0/show")) {
            show = GWY_DATA_FIELD(gwy_container_get_object_by_name(data,
                                                                   "/0/show"));
            gwy_data_field_resample(show,
                                    gwy_data_field_get_xres(dfield),
                                    gwy_data_field_get_yres(dfield),
                                    GWY_INTERPOLATION_NONE);
            gwy_data_field_copy(dfield, show);
        }
        else {
            show = GWY_DATA_FIELD(gwy_serializable_duplicate(G_OBJECT(dfield)));
            gwy_container_set_object_by_name(data, "/0/show", G_OBJECT(show));
        }
        gwy_data_field_threshold(show, args->absolute, args->min, args->max);
        break;

        default:
        g_assert_not_reached();
        break;
    }
}

static gboolean
threshold_dialog(ThresholdArgs *args)
{
    const gchar *modes[] = { "_Data", "_Mask", "_Presentation" };
    GtkWidget *dialog, *table, *spin, *widget;
    GtkRadioButton *group;
    ThresholdControls controls;
    enum { RESPONSE_RESET = 1 };
    gint response;
    GSList *l;
    gsize i;

    dialog = gtk_dialog_new_with_buttons(_("Threshold"),
                                         NULL,
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         _("Reset"), RESPONSE_RESET,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);

    table = gtk_table_new(6, 3, FALSE);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table,
                       FALSE, FALSE, 4);

    controls.mag = gwy_math_humanize_numbers((args->max - args->min)/100,
                                             MAX(fabs(args->min),
                                                 fabs(args->max)),
                                             &controls.precision);
    controls.fractile = gtk_adjustment_new(100*args->fractile,
                                           0, 100, 1, 10, 0);
    spin = gwy_table_attach_spinbutton(table, 0, _("_Fractile:"), "%",
                                       controls.fractile);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 0);
    gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(spin), TRUE);
    g_object_set_data(G_OBJECT(controls.fractile), "controls", &controls);
    g_signal_connect(controls.fractile, "value_changed",
                     G_CALLBACK(fractile_changed_cb), args);

    controls.absolute = gtk_adjustment_new(args->absolute/controls.mag,
                                           args->min/controls.mag,
                                           args->max/controls.mag,
                                           (args->max - args->min)/100
                                                /controls.mag,
                                           (args->max - args->min)/10
                                                /controls.mag,
                                           0);
    spin = gwy_table_attach_spinbutton(table, 1, _("Absolute _threshold:"),
                                       _("FIXME"),
                                       controls.absolute);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), controls.precision);
    g_object_set_data(G_OBJECT(controls.absolute), "controls", &controls);
    g_signal_connect(controls.absolute, "value_changed",
                     G_CALLBACK(absolute_changed_cb), args);

    widget = gtk_label_new(_("Modify:"));
    gtk_misc_set_alignment(GTK_MISC(widget), 0.0, 0.5);
    gtk_table_attach_defaults(GTK_TABLE(table), widget, 0, 2, 2, 3);
    group = NULL;
    for (i = 0; i < G_N_ELEMENTS(modes); i++) {
        if (group) {
            widget
                = gtk_radio_button_new_with_mnemonic_from_widget(group,
                                                                 _(modes[i]));
        }
        else {
            widget = gtk_radio_button_new_with_mnemonic(NULL, _(modes[i]));
            group = GTK_RADIO_BUTTON(widget);
        }
        g_object_set_data(G_OBJECT(widget), "mode", GUINT_TO_POINTER(i));
        gtk_table_attach_defaults(GTK_TABLE(table), widget, 1, 3, i+3, i+4);
        if (i == args->mode)
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), TRUE);
    }
    controls.mode = gtk_radio_button_get_group(group);

    controls.in_update = FALSE;

    gtk_widget_show_all(dialog);
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
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
            threshold_dialog_update(&controls, args);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    for (l = controls.mode; l; l = g_slist_next(l)) {
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(l->data))) {
            args->mode = GPOINTER_TO_UINT(g_object_get_data(l->data, "mode"));
            break;
        }
    }
    gtk_widget_destroy(dialog);

    return TRUE;
}

static void
fractile_changed_cb(GtkAdjustment *adj,
                    ThresholdArgs *args)
{
    ThresholdControls *controls;

    controls = g_object_get_data(G_OBJECT(adj), "controls");
    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    args->fractile = gtk_adjustment_get_value(adj)/100;
    threshold_fmap_fractile_to_abs(args);
    threshold_dialog_update(controls, args);
    controls->in_update = FALSE;
}

static void
absolute_changed_cb(GtkAdjustment *adj,
                    ThresholdArgs *args)
{
    ThresholdControls *controls;

    controls = g_object_get_data(G_OBJECT(adj), "controls");
    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    args->absolute = gtk_adjustment_get_value(adj)*controls->mag;
    threshold_fmap_abs_to_fractile(args);
    threshold_dialog_update(controls, args);
    controls->in_update = FALSE;
}

static const gchar *fractile_key = "/module/" THRESHOLD_MOD_NAME "/fractile";
static const gchar *mode_key = "/module/" THRESHOLD_MOD_NAME "/mode";

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
threshold_dialog_update(ThresholdControls *controls,
                        ThresholdArgs *args)
{
    GSList *l;

    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->fractile),
                             100*args->fractile);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->absolute),
                             args->absolute/controls->mag);

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
