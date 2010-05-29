/*
 *  @(#) $Id: simple.c 8929 2008-12-31 13:40:16Z yeti-dn $
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
#include <gtk/gtk.h>
#include <glib/gstdio.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyexpr.h>
#include <libprocess/datafield.h>
#include <libprocess/gwyprocess.h>
#include <libgwydgets/gwystock.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>

#define SIMPLE_RUN_MODES GWY_RUN_INTERACTIVE

enum {
    NARGS = 2
};

enum {
    SIMPLE_OK   = 0,
    SIMPLE_DATA = 1,
    SIMPLE_EXPR = 2
};

typedef enum {
    DUPLICATE_NONE = 0,
    DUPLICATE_OVERWRITE = 1,
    DUPLICATE_APPEND = 2
} ResponseDuplicate;


typedef struct {
    GwyContainer *data;
    gint id;
} GwyDataObjectId;

typedef struct {
    GwyDataObjectId objects[NARGS];
    gchar *name[NARGS];
    guint pos[NARGS];
    gdouble xoffset;
    gdouble yoffset;
    gdouble zoffset;
    gdouble xperiod;
    gdouble yperiod;
    gint xyexponent;
    gchar *xyunit;
    gint zexponent;
    gchar *zunit;
    gchar *calname;
    ResponseDuplicate duplicate;
    GwyCalData *caldata;
} SimpleArgs;

typedef struct {
    SimpleArgs *args;
    GtkWidget *dialog;
    GtkWidget *data[NARGS];
    GtkObject *xoffset;
    GtkObject *yoffset;
    GtkObject *zoffset;
    GtkObject *xperiod;
    GtkObject *yperiod;
    gboolean in_update;
    GtkWidget *xyunits;
    GtkWidget *zunits;
    GtkWidget *xyexponent;
    GtkWidget *zexponent;
    GtkEntry *name;
} SimpleControls;

static gboolean     module_register           (void);
static void         simple                (GwyContainer *data,
                                               GwyRunType run);
static gboolean     simple_dialog         (SimpleArgs *args,
                                           GwyDataField *dfield);
static void         simple_data_cb        (GwyDataChooser *chooser,
                                               SimpleControls *controls);
static const gchar* simple_check          (SimpleArgs *args);
static void         simple_do             (SimpleArgs *args);
static void         flip_xy              (GwyDataField *source, 
                                          GwyDataField *dest, 
                                          gboolean minor);
static void        xyexponent_changed_cb       (GtkWidget *combo,
                                               SimpleControls *controls);
static void        zexponent_changed_cb       (GtkWidget *combo,
                                               SimpleControls *controls);
static void        units_change_cb             (GtkWidget *button,
                                               SimpleControls *controls);
static void        set_combo_from_unit       (GtkWidget *combo,
                                              const gchar *str,
                                              gint basepower);
static void        xoffset_changed_cb          (GtkAdjustment *adj,
                                               SimpleControls *controls);
static void        yoffset_changed_cb          (GtkAdjustment *adj,
                                               SimpleControls *controls);
static void        zoffset_changed_cb          (GtkAdjustment *adj,
                                               SimpleControls *controls);
static void        xperiod_changed_cb          (GtkAdjustment *adj,
                                               SimpleControls *controls);
static void        yperiod_changed_cb          (GtkAdjustment *adj,
                                               SimpleControls *controls);



static const gchar default_expression[] = "d1 - d2";

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Digital AFM data recalibration"),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2010",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("simple",
                              (GwyProcessFunc)&simple,
                              N_("/Cali_bration/_3D calibration/_Get simple errop map..."),
                              NULL,
                              SIMPLE_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Simple error mapping"));

    return TRUE;
}

void
simple(GwyContainer *data, GwyRunType run)
{
    SimpleArgs args;
    guint i;
    GwyContainer *settings;
    GwyDataField *dfield;
    gint n, ok, id;
    GwyCalibration *calibration;
    GwyCalData *caldata;
    gchar *filename;
    gchar *contents;
    gsize len;
    GError *err = NULL;
    gsize pos = 0;
    GString *str;
    GByteArray *barray;
    FILE *fh; 

    g_return_if_fail(run & SIMPLE_RUN_MODES);

    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD_ID, &id, 
                                     GWY_APP_DATA_FIELD, &dfield, 0);

    settings = gwy_app_settings_get();
    for (i = 0; i < NARGS; i++) {
        args.objects[i].data = data;
        args.objects[i].id = id;
    }

    if (simple_dialog(&args, dfield)) {
        simple_do(&args);
    } else return; 


    /*if append requested, copy newly created calibration into old one*/
    if (args.duplicate == DUPLICATE_APPEND && (calibration = gwy_inventory_get_item(gwy_calibrations(), args.name)))
    {

        filename = g_build_filename(gwy_get_user_dir(), "caldata", calibration->filename, NULL);
        if (!g_file_get_contents(filename,
                                 &contents, &len, &err))
        {
             g_warning("Error loading file: %s\n", err->message);
             g_clear_error(&err);
             return;
        }
        else {
            if (len)
              caldata = GWY_CALDATA(gwy_serializable_deserialize(contents, len, &pos));
            g_free(contents);
        }
        n = caldata->ndata + args.caldata->ndata;

        //add to args->caldata
        args.caldata->x = g_realloc(args.caldata->x, n*sizeof(gdouble));
        args.caldata->y = g_realloc(args.caldata->y, n*sizeof(gdouble));
        args.caldata->z = g_realloc(args.caldata->z, n*sizeof(gdouble));
        args.caldata->xerr = g_realloc(args.caldata->xerr, n*sizeof(gdouble));
        args.caldata->yerr = g_realloc(args.caldata->yerr, n*sizeof(gdouble));
        args.caldata->zerr = g_realloc(args.caldata->zerr, n*sizeof(gdouble));
        args.caldata->xunc = g_realloc(args.caldata->xunc, n*sizeof(gdouble));
        args.caldata->yunc = g_realloc(args.caldata->yunc, n*sizeof(gdouble));
        args.caldata->zunc = g_realloc(args.caldata->zunc, n*sizeof(gdouble));

        for (i=args.caldata->ndata; i<n; i++)
        {
           args.caldata->x[i] = caldata->x[i];
           args.caldata->y[i] = caldata->y[i];
           args.caldata->z[i] = caldata->z[i];
           args.caldata->xerr[i] = caldata->xerr[i];
           args.caldata->yerr[i] = caldata->yerr[i];
           args.caldata->zerr[i] = caldata->zerr[i];
           args.caldata->xunc[i] = caldata->xunc[i];
           args.caldata->yunc[i] = caldata->yunc[i];
           args.caldata->zunc[i] = caldata->zunc[i];
        }
        args.caldata->ndata = n;
    }

    /*now create and save the resource*/
    if ((calibration = GWY_CALIBRATION(gwy_inventory_get_item(gwy_calibrations(), args.name)))==NULL)
    {
        calibration = gwy_calibration_new(args.name, 8, g_strconcat(args.name, ".dat", NULL));
        gwy_inventory_insert_item(gwy_calibrations(), calibration);
        g_object_unref(calibration);
    }

    filename = gwy_resource_build_filename(GWY_RESOURCE(calibration));
    fh = g_fopen(filename, "w");
    if (!fh) {
        g_warning("Cannot save preset: %s", filename);
        g_free(filename);
        return;
    }
    g_free(filename);

    str = gwy_resource_dump(GWY_RESOURCE(calibration));
    fwrite(str->str, 1, str->len, fh);
    fclose(fh);
    g_string_free(str, TRUE);

    gwy_resource_data_saved(GWY_RESOURCE(calibration));


    /*now save the calibration data*/
    if (!g_file_test(g_build_filename(gwy_get_user_dir(), "caldata", NULL), G_FILE_TEST_EXISTS)) {
        g_mkdir(g_build_filename(gwy_get_user_dir(), "caldata", NULL), 0700);
    }
    fh = g_fopen(g_build_filename(gwy_get_user_dir(), "caldata", calibration->filename, NULL), "w");
    if (!fh) {
        g_warning("Cannot save caldata\n");
        return;
    }
    barray = gwy_serializable_serialize(G_OBJECT(args.caldata), NULL);
    //g_file_set_contents(fh, barray->data, sizeof(guint8)*barray->len, NULL);
    fwrite(barray->data, sizeof(guint8), barray->len, fh);
    fclose(fh);



}

static gboolean
simple_dialog(SimpleArgs *args, GwyDataField *dfield)
{
    SimpleControls controls;
    GtkWidget *dialog, *dialog2, *table, *chooser,  *label, *spin;
    GwySIUnit *unit;
    guint i, row, response;

    enum {
        RESPONSE_DUPLICATE_OVERWRITE = 2,
        RESPONSE_DUPLICATE_APPEND = 3 };


    controls.args = args;

    /*FIXME: use defaults*/
    args->xoffset = 0;
    args->yoffset = 0;
    args->zoffset = 0;
    args->xperiod = 0;
    args->yperiod = 0;
    args->xyexponent = -6;
    args->zexponent = -6;
    args->xyunit = "m";
    args->zunit = "m";


    dialog = gtk_dialog_new_with_buttons(_("Simple error map"), NULL, 0,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    controls.dialog = dialog;
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    table = gtk_table_new(4 + NARGS, 2, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, TRUE, TRUE, 4);
    row = 0;

    label = gtk_label_new(_("Operands:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    args->name[0] = g_strdup_printf("Grating image");
    args->name[1] = g_strdup_printf("Detail");
      for (i = 0; i < NARGS; i++) {
        label = gtk_label_new_with_mnemonic(args->name[i]);
        gwy_strkill(args->name[i], "_");
        gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
        gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row+1,
                         GTK_EXPAND | GTK_FILL, 0, 0, 0);

        chooser = gwy_data_chooser_new_channels();
        gwy_data_chooser_set_active(GWY_DATA_CHOOSER(chooser),
                                    args->objects[i].data, args->objects[i].id);
        g_signal_connect(chooser, "changed",
                         G_CALLBACK(simple_data_cb), &controls);
        g_object_set_data(G_OBJECT(chooser), "index", GUINT_TO_POINTER(i));
        gtk_table_attach(GTK_TABLE(table), chooser, 1, 2, row, row+1,
                         GTK_EXPAND | GTK_FILL, 0, 0, 0);
        gtk_label_set_mnemonic_widget(GTK_LABEL(label), chooser);
        controls.data[i] = chooser;

        row++;
      }
    label = gtk_label_new_with_mnemonic(_("_X offset:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    controls.xoffset = gtk_adjustment_new(args->xoffset/pow10(args->xyexponent),
                                        -10000, 10000, 1, 10, 0);
    spin = gtk_spin_button_new(GTK_ADJUSTMENT(controls.xoffset), 1, 2);
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin), TRUE);
    gtk_table_attach(GTK_TABLE(table), spin,
                     1, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    unit = gwy_data_field_get_si_unit_xy(dfield);
    controls.xyexponent
        = gwy_combo_box_metric_unit_new(G_CALLBACK(xyexponent_changed_cb),
                                        &controls, -15, 6, unit,
                                        args->xyexponent);
    gtk_table_attach(GTK_TABLE(table), controls.xyexponent, 2, 3, row, row+2,
                     GTK_EXPAND | GTK_FILL | GTK_SHRINK, 0, 0, 0);

    controls.xyunits = gtk_button_new_with_label(_("Change"));
    g_object_set_data(G_OBJECT(controls.xyunits), "id", (gpointer)"xy");
    gtk_table_attach(GTK_TABLE(table), controls.xyunits,
                     3, 4, row, row+2,
                     GTK_EXPAND | GTK_FILL | GTK_SHRINK, 0, 0, 0);
    row++;

    label = gtk_label_new_with_mnemonic(_("_Y offset:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    controls.yoffset = gtk_adjustment_new(args->yoffset/pow10(args->xyexponent),
                                        -10000, 10000, 1, 10, 0);
    spin = gtk_spin_button_new(GTK_ADJUSTMENT(controls.yoffset), 1, 2);
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin), TRUE);
    gtk_table_attach(GTK_TABLE(table), spin,
                     1, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    label = gtk_label_new_with_mnemonic(_("X _period:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    controls.xperiod = gtk_adjustment_new(args->xperiod/pow10(args->xyexponent),
                                        -10000, 10000, 1, 10, 0);
    spin = gtk_spin_button_new(GTK_ADJUSTMENT(controls.xperiod), 1, 2);
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin), TRUE);
    gtk_table_attach(GTK_TABLE(table), spin,
                     1, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    label = gtk_label_new_with_mnemonic(_("Y p_eriod:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    controls.yperiod = gtk_adjustment_new(args->yperiod/pow10(args->xyexponent),
                                        -10000, 10000, 1, 10, 0);
    spin = gtk_spin_button_new(GTK_ADJUSTMENT(controls.yperiod), 1, 2);
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin), TRUE);
    gtk_table_attach(GTK_TABLE(table), spin,
                     1, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;


    label = gtk_label_new_with_mnemonic(_("_Z offset:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    controls.zoffset = gtk_adjustment_new(args->zoffset/pow10(args->zexponent),
                                        -10000, 10000, 1, 10, 0);
    spin = gtk_spin_button_new(GTK_ADJUSTMENT(controls.zoffset), 1, 2);
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin), TRUE);
    gtk_table_attach(GTK_TABLE(table), spin,
                     1, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    unit = gwy_data_field_get_si_unit_z(dfield);
    controls.zexponent
        = gwy_combo_box_metric_unit_new(G_CALLBACK(zexponent_changed_cb),
                                        &controls, -15, 6, unit,
                                        args->zexponent);
    gtk_table_attach(GTK_TABLE(table), controls.zexponent, 2, 3, row, row+1,
                     GTK_EXPAND | GTK_FILL | GTK_SHRINK, 0, 0, 0);

    controls.zunits = gtk_button_new_with_label(_("Change"));
    g_object_set_data(G_OBJECT(controls.zunits), "id", (gpointer)"z");
    gtk_table_attach(GTK_TABLE(table), controls.zunits,
                     3, 4, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;


    label = gtk_label_new_with_mnemonic(_("Calibration name:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    args->calname = g_strdup("new"); //FIXME this should not be here
    controls.name = gtk_entry_new();
    gtk_entry_set_text(controls.name, args->calname);
    gtk_table_attach(GTK_TABLE(table), controls.name,
                     1, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

      g_signal_connect(controls.xoffset, "value-changed",
                       G_CALLBACK(xoffset_changed_cb), &controls);
      g_signal_connect(controls.yoffset, "value-changed",
                       G_CALLBACK(yoffset_changed_cb), &controls);
      g_signal_connect(controls.zoffset, "value-changed",
                       G_CALLBACK(zoffset_changed_cb), &controls);
      g_signal_connect(controls.xperiod, "value-changed",
                       G_CALLBACK(xperiod_changed_cb), &controls);
      g_signal_connect(controls.yperiod, "value-changed",
                       G_CALLBACK(yperiod_changed_cb), &controls);


     g_signal_connect(controls.xyunits, "clicked",
                             G_CALLBACK(units_change_cb), &controls);

     g_signal_connect(controls.zunits, "clicked",
                             G_CALLBACK(units_change_cb), &controls);

     controls.in_update = FALSE;



      gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);

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
            /*check whether this resource already exists*/
            args->calname = g_strdup(gtk_entry_get_text(controls.name));
            if (gwy_inventory_get_item(gwy_calibrations(), args->calname))
            {
                dialog2 = gtk_message_dialog_new (dialog,
                                                  GTK_DIALOG_DESTROY_WITH_PARENT,
                                                  GTK_MESSAGE_WARNING,
                                                  GTK_BUTTONS_CANCEL,
                                                  "Calibration '%s' alerady exists",
                                                  args->calname);
                gtk_dialog_add_button(dialog2, "Overwrite", RESPONSE_DUPLICATE_OVERWRITE);
                gtk_dialog_add_button(dialog2, "Append", RESPONSE_DUPLICATE_APPEND);
                response = gtk_dialog_run(GTK_DIALOG(dialog2));
                if (response == RESPONSE_DUPLICATE_OVERWRITE) {
                    args->duplicate = DUPLICATE_OVERWRITE;
                    response = GTK_RESPONSE_OK;
                } else if (response == RESPONSE_DUPLICATE_APPEND) {
                    args->duplicate = DUPLICATE_APPEND;
                    response = GTK_RESPONSE_OK;
                }
                gtk_widget_destroy (dialog2);
            } else args->duplicate = DUPLICATE_NONE;

            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    gtk_widget_destroy(dialog);

    return TRUE;
}

static void
simple_data_cb(GwyDataChooser *chooser,
                   SimpleControls *controls)
{
    SimpleArgs *args;
    guint i;

    args = controls->args;
    i = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(chooser), "index"));
    args->objects[i].data = gwy_data_chooser_get_active(chooser,
                                                        &args->objects[i].id);
}

void
get_object_list(GwyDataField *data, GwyDataField *kernel, gdouble threshold, 
                gdouble *xs, gdouble *ys, gint *nobjects, GwyCorrelationType type)
{
    GwyDataField *score = gwy_data_field_new_alike(data, 0);
    GwyDataField *retfield;
    gdouble *sdata, *maxval, min, max;
    gint i, *grains, *maxpos, ngrains;

    gwy_data_field_correlate(data, kernel, score, type);
    max = gwy_data_field_get_max(score);
    min = gwy_data_field_get_min(score);

    retfield = gwy_data_field_duplicate(score);
    gwy_data_field_threshold(retfield, threshold, 0.0, 1.0); 

    grains = (gint *)g_malloc(gwy_data_field_get_xres(score)*gwy_data_field_get_yres(score)*sizeof(int));
    ngrains = gwy_data_field_number_grains(retfield, grains);

    maxpos = (gint *) g_malloc(ngrains*sizeof(gint));
    maxval = (gdouble *) g_malloc(ngrains*sizeof(gdouble));
    sdata = gwy_data_field_get_data(score);

    for (i=0; i<ngrains; i++) maxval[i] = -G_MAXDOUBLE;
    
    //find correlation maximum of each grain
    for (i=0; i<(gwy_data_field_get_xres(score)*gwy_data_field_get_yres(score)); i++)
    {
        if (grains[i]!=0) {
            if (maxval[grains[i]-1]<sdata[i]) {
                maxval[grains[i]-1]=sdata[i];
                maxpos[grains[i]-1]=i;
            }
        }
    }
    //return correlation maxima (x, y), TODO do this in future with subpixel precision;
    *nobjects = MIN(*nobjects, ngrains);
    for (i=0; i<(*nobjects); i++) {
        ys[i] = (int)(maxpos[i]/gwy_data_field_get_xres(retfield));
        xs[i] = maxpos[i] - ys[i]*gwy_data_field_get_xres(retfield);
    }

    g_object_unref(score);
    g_object_unref(retfield);
    g_free(grains);
    g_free(maxpos);
    g_free(maxval);

}
static
void fill_matrix(gdouble *xs, gdouble *ys, gint n, gint tl, 
                 gdouble xxshift, gdouble xyshift,
                 gdouble yxshift, gdouble yyshift, 
                 GwyDataField *x_matrix, GwyDataField *y_matrix, gint nn)
{
    gint i, j, k, pos;
    gdouble tlx = xs[tl];
    gdouble tly = ys[tl];
    gdouble idxpos, idypos;
    gdouble min, dist;

    printf("shifts: x: %g %g  y: %g %g\n", xxshift, xyshift, yxshift , yyshift);

    for (j=0; j<nn; j++)
    {
        for (i=0; i<nn; i++)
        {
            //determine ideal position
            idxpos = tlx + i*xxshift + j*yxshift;
            idypos = tly + i*xyshift + j*yyshift;

            //find closest point
            min = G_MAXDOUBLE;
            for (k=0; k<n; k++) {
                if ((dist = (xs[k]-idxpos)*(xs[k]-idxpos) + (ys[k]-idypos)*(ys[k]-idypos))<min) {
                    min = dist;
                    pos = k;
                }
            }
            gwy_data_field_set_val(x_matrix, i, j, xs[pos]);
            gwy_data_field_set_val(y_matrix, i, j, ys[pos]);
//            printf("Point %d %d, idpos %g %g, found pos %g %g\n", 
//                   i, j, idxpos, idypos, xs[pos], ys[pos]);
        }
    }


}

static
void fill_matrixb(gdouble *xs, gdouble *ys, gint n, gint tl, 
                 gdouble xxshift, gdouble xyshift,
                 gdouble yxshift, gdouble yyshift, 
                 GwyDataField *x_matrix, GwyDataField *y_matrix, gint nn)
{
    gint i, j, k, ii, jj;
    gdouble tlx = xs[tl];
    gdouble tly = ys[tl];
    gdouble nextmin;
    gint pos, ppos, next, nypos;
    gdouble min, dist;

    printf("shifts: x: %g %g  y: %g %g\n", xxshift, xyshift, yxshift , yyshift);

    //pos se meni po xy, nypos je tl s updatovanou y-ovou pozici
    pos = ppos = nypos = tl;

    for (jj=0; jj<(nn); jj++)
    {
        for (ii=0; ii<(nn); ii++)
        {
            gwy_data_field_set_val(x_matrix, ii, jj, xs[pos]);
            gwy_data_field_set_val(y_matrix, ii, jj, ys[pos]);
            fprintf(stderr, "%d %d %g %g\n", ii, jj, xs[pos], ys[pos]);

            ppos = pos;
            //find next closest object in x direction
            nextmin = G_MAXDOUBLE;
            for (i=0; i<n; i++) {
                if (i==pos || xs[i]<=xs[pos]) continue; 
                if (ii>0 && fabs(ys[i]-ys[pos])>(0.5*fabs(xxshift))) {
                    //printf("too far in y (%d  %g > %g) \n", i, fabs(ys[i]-ys[present]), (0.5*fabs(yshift))); 
                    continue;
                }
                if (((xs[i]-xs[pos]) + (ys[i]-ys[pos])*(ys[i]-ys[pos]))<nextmin) {
                    nextmin = ((xs[i]-xs[pos]) + (ys[i]-ys[pos])*(ys[i]-ys[pos]));
                    next = i;
                }
            }
            if (nextmin!=G_MAXDOUBLE) {
                pos = next;
        //        fprintf(stderr, "%d %g %g\n", ii, xs[pos], ys[pos]);

            } else {
                fprintf(stderr, "Errrrroooorrr\n");
                break; //this should never happend
            }
        }

        pos = ppos = nypos;
        /*find next top left (y step)*/    
        nextmin = G_MAXDOUBLE;
        for (i=0; i<n; i++) {
            if (i==pos || ys[i]<=ys[pos]) continue; 
            if (jj>0 && fabs(xs[i]-xs[pos])>(0.5*fabs(yyshift))) {
                //printf("too far in y (%d  %g > %g) \n", i, fabs(ys[i]-ys[present]), (0.5*fabs(yshift))); 
                continue;
            }
            if (((ys[i]-ys[pos]) + (xs[i]-xs[pos])*(xs[i]-xs[pos]))<nextmin) {
                nextmin = ((ys[i]-ys[pos]) + (xs[i]-xs[pos])*(xs[i]-xs[pos]));
                next = i;
            }
        }
        if (nextmin!=G_MAXDOUBLE) {
            pos = nypos = next;
        //    fprintf(stderr, "%d %g %g\n", jj, xs[pos], ys[pos]);
        } else {
            fprintf(stderr, "!!! there was and error\n");
        }

    }
}

static void
simple_dialog_update(SimpleControls *controls,
                        SimpleArgs *args)
{
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->xoffset),
                             args->xoffset/pow10(args->xyexponent));

}


 
gdouble 
get_prod_grid(GwyDataField *a, GwyDataField *b, gdouble period)
{
    gint i, j;
    gint xres = gwy_data_field_get_xres(a);
    gint yres = gwy_data_field_get_yres(a);
    gdouble suma, sumb;
    gint shift = -xres/2;

    suma = sumb = 0;
    for (i=0; i<xres; i++)
    {
        for (j=0; j<yres; j++)
        {
            suma += gwy_data_field_get_val(a, i, j)*((i+shift)*period) - gwy_data_field_get_val(b, i, j)*((j+shift)*period);
            sumb += period*period*((i+shift)*(i+shift) + (j+shift)*(j+shift));
        }
    }

    return suma/sumb;
}

void
simple_calibration(GwyDataField *x_orig, GwyDataField *y_orig, 
                  gdouble period)
{
    GwyDataField *v0x, *v0y, *u0x, *u0y;
    GwyDataField *Gx, *Gy;
    gdouble theta0, t0x, t0y;
    gint i, j, shift;
    gint xres = gwy_data_field_get_xres(x_orig);
    gint yres = gwy_data_field_get_yres(y_orig);
    
    shift = -xres/2;

    v0x = gwy_data_field_new_alike(x_orig, FALSE);
    v0y = gwy_data_field_new_alike(y_orig, FALSE);
    u0x = gwy_data_field_new_alike(x_orig, FALSE);
    u0y = gwy_data_field_new_alike(y_orig, FALSE);
    
    Gx = gwy_data_field_new_alike(x_orig, FALSE);
    Gy = gwy_data_field_new_alike(x_orig, FALSE);

    /***********************************   step 1  ************************************/
    //create v0x, v0y
    for (i=0; i<xres; i++)
    {
        for (j=0; j<yres; j++)
        {
            gwy_data_field_set_val(v0x, i, j, gwy_data_field_get_val(x_orig, i, j)-((i+shift)*period));
            gwy_data_field_set_val(v0y, i, j, gwy_data_field_get_val(y_orig, i, j)-((j+shift)*period));
        }
    }

    //eq 25a, 25b, 26
    t0x = gwy_data_field_get_avg(v0x);
    t0y = gwy_data_field_get_avg(v0y);
    theta0 = get_prod_grid(v0y, v0x, period);
    printf("Original pre: ts: %g %g, theta %g\n", t0x, t0y, theta0);

    //eq 27a, b, tested that ts and theta itself compensates shift and rotation to zero
    for (i=0; i<xres; i++)
    {
        for (j=0; j<yres; j++)
        {
            gwy_data_field_set_val(u0x, i, j, gwy_data_field_get_val(v0x, i, j) - t0x + theta0*period*(j+shift));
            gwy_data_field_set_val(u0y, i, j, gwy_data_field_get_val(v0y, i, j) - t0y - theta0*period*(i+shift));
        }
    }
 
   //check the aligned data, just for sure:
    t0x = gwy_data_field_get_avg(u0x);
    t0y = gwy_data_field_get_avg(u0y);
    theta0 = get_prod_grid(u0y, u0x, period);
    printf("Original post: ts: %g %g, theta %g\n", t0x, t0y, theta0);
    printf("period determined for %g\n", period);

    //determine simple stage error map
    for (j=0; j<yres; j++)
    {
        for (i=0; i<xres; i++)
        {
            gwy_data_field_set_val(Gx, i, j, gwy_data_field_get_val(u0x, i, j) - (i+shift)*period);
            gwy_data_field_set_val(Gy, i, j, gwy_data_field_get_val(u0y, i, j) - (j+shift)*period);
        }
    } 

    printf("Final matrix positon (x,y) vs Gx, Gy:\n");
    for (j=0; j<yres; j++)
    {
        for (i=0; i<xres; i++)
        {
        //    printf("%g %g    %g %g \n", (i+shift)*period, (j+shift)*period, gwy_data_field_get_val(Gx, i, j), gwy_data_field_get_val(Gy, i, j));
              printf("%g %g    %g %g \n", (i+shift)*period, (j+shift)*period, gwy_data_field_get_val(u0x, i, j), gwy_data_field_get_val(u0y, i, j));
        }
        printf("\n");
    } 

 }

static void
simple_do(SimpleArgs *args)
{
    GwyContainer *data;
    GwyDataField *original, *detail, *score;
    gdouble *xs, *ys;
    GwyDataField *x_orig, *y_orig;
    gint i, j, newid, ndat, mdat, noriginal, nshifted, nrotated;
    gdouble xxshift, xyshift, yxshift, yyshift, avxxshift, avxyshift, avyxshift, avyyshift;
    gdouble xmult, ymult;
    gdouble tlmin, nextmin, boundary;
    gdouble original_tlx, original_tly;
    gdouble sn, cs, icor, jcor, x, y;
    gint tl, present, next, nx, ny, nn, xres, yres;
    gdouble period = 0;
    GQuark quark;

    printf("starting\n");

    data = args->objects[0].data;
    quark = gwy_app_get_data_key_for_id(args->objects[0].id);
    original = GWY_DATA_FIELD(gwy_container_get_object(data, quark));

    data = args->objects[1].data;
    quark = gwy_app_get_data_key_for_id(args->objects[1].id);
    detail = GWY_DATA_FIELD(gwy_container_get_object(data, quark));

    //________________________________________________________original____________________________________________
    //find objects on original
    noriginal = 10000;
    xs = (gdouble *)g_malloc(noriginal*sizeof(gdouble));
    ys = (gdouble *)g_malloc(noriginal*sizeof(gdouble));
    get_object_list(original, detail, 0.193, xs, ys, &noriginal, GWY_CORRELATION_NORMAL); 
          //0.335 for 2d100_3, 0.333 for 2d_100_4 0.391 for 2d100_5 a 2d100_1
          //0.333 for 2d300_10, 0.246 for 2d300_9-6, 0.193 for 2d300_7
    printf("%d object locations in original\n", noriginal);
    /*for (i=0; i<noriginal; i++)
    {
         printf("%d %g %g\n", i, xs[i], ys[i]);
    }
     printf("_____________________\n");*/

    //create matrix of NxN object positions for original image, skip left edge positions; determine xshift
    
    //determine size of array first:
    //find top left object
    tl = 0;
    tlmin = G_MAXDOUBLE;
    for (i=0; i<noriginal; i++) {
        if ((xs[i]*ys[i])<tlmin) {
            tlmin = (xs[i]*ys[i]);
            tl = i;
        }
    }
    original_tlx = xs[tl];
    original_tly = ys[tl];
    printf("top left object is %g %g\n", xs[tl], ys[tl]);

    present = tl;
    //determine number of objects in x direction and xshift. Discriminate objects at the right edge
    nx = 0;
    boundary = 0;
    xyshift = avxyshift = 0;
    xxshift = avxxshift = 0;
    do {
        //find next closest object in x direction
        nextmin = G_MAXDOUBLE;
        for (i=0; i<noriginal; i++) {
            if (i==present || xs[i]<=xs[present]) continue; 
            if (nx>0 && fabs(ys[i]-ys[present])>(0.5*fabs(xxshift))) {
                //printf("too far in y (%d  %g > %g) \n", i, fabs(ys[i]-ys[present]), (0.5*fabs(yshift))); 
                continue;
            }
            if (((xs[i]-xs[present]) + (ys[i]-ys[present])*(ys[i]-ys[present]))<nextmin) {
                nextmin = ((xs[i]-xs[present]) + (ys[i]-ys[present])*(ys[i]-ys[present]));
                next = i;
                xxshift = xs[next] - xs[present];
                xyshift = ys[next] - ys[present];
                boundary = 1.1*xxshift;
            }
        }
        if (nextmin!=G_MAXDOUBLE) {
            nx++;
            //printf("next object to the left is %g %g, shift %g %g\n", xs[next], ys[next], xxshift, xyshift);
            present = next;
            avxxshift += xxshift;
            avxyshift += xyshift;
        } else break;

    } while (nextmin!=G_MAXDOUBLE);
    printf("Original: found %d objects in x direction, average shift is %g %g\n", nx+1, avxxshift/nx, avxyshift/nx);
    avxxshift/=nx;
    avxyshift/=nx;
   
    present = tl;
    //determine number of objects in x direction and xshift, 
    ny = 0;
    yyshift = yxshift = 0;
    avyyshift = avyxshift = 0;
    do {
        //find next closest object in y direction
        nextmin = G_MAXDOUBLE;
        for (i=0; i<noriginal; i++) {
            if (i==present || ys[i]<=ys[present]) continue; 
            if (ny>0 && fabs(xs[i]-xs[present])>(0.5*fabs(yyshift))) {
                //printf("too far in y (%d  %g > %g) \n", i, fabs(ys[i]-ys[present]), (0.5*fabs(yshift))); 
                continue;
            }
            if (((ys[i]-ys[present]) + (xs[i]-xs[present])*(xs[i]-xs[present]))<nextmin) {
                nextmin = ((ys[i]-ys[present]) + (xs[i]-xs[present])*(xs[i]-xs[present]));
                next = i;
                yxshift = xs[next] - xs[present];
                yyshift = ys[next] - ys[present];
            }
        }
        if (nextmin!=G_MAXDOUBLE) {
            ny++;
            //printf("next object to the bottom is %g %g, shift %g %g\n", xs[next], ys[next], yxshift, yyshift);
            present = next;
            avyyshift += yxshift;
            avyyshift += yyshift;
         } else break;

    } while (nextmin!=G_MAXDOUBLE);
    printf("Original: found %d objects in y direction\n", ny+1);
    avyyshift/=ny;
    avyxshift/=ny;

    //determine final number of objects, it must be odd and same in both the directions
    nn = MIN(nx, ny) - 1;

    printf("I will use matrix of %d x %d calibration points\n", nn, nn);
 
    //allocate matrices
    x_orig = gwy_data_field_new(nn, nn, nn, nn, TRUE);
    y_orig = gwy_data_field_new_alike(x_orig, TRUE);

    //fill matrix of original
    printf("filling matrix\n");
    fill_matrixb(xs, ys, noriginal, tl, avxxshift, avxyshift, avyxshift, avyyshift, x_orig, y_orig, nn);
    printf("filled\n");

    //make real coordinates from pixel ones
    xmult = gwy_data_field_get_xreal(original)/gwy_data_field_get_xres(original);
    ymult = gwy_data_field_get_yreal(original)/gwy_data_field_get_yres(original);
    gwy_data_field_multiply(x_orig, xmult);
    gwy_data_field_multiply(y_orig, ymult);

    //move center of each matrix by half size in both direction, so the axis intersection is in the center of image
    gwy_data_field_add(x_orig, -gwy_data_field_get_xreal(original)/2);
    gwy_data_field_add(y_orig, -gwy_data_field_get_yreal(original)/2);

    //now we have matrices for all three data and we can throw all data away.
    //output all of the for debug:
//    printf("Original matrix:\n");
//    for (j = 0; j < yres; j++) {
//        for (i = 0; i < xres; i++) {
//            printf("(%g,%g)  ", gwy_data_field_get_val(x_orig, i, j), gwy_data_field_get_val(y_orig, i, j));
//        }
//        printf("\n");
//    } 

     /*  - create v0x, v0y field from original image matrix*/
    period = sqrt(avxxshift*avxxshift + avxyshift*avxyshift)*xmult; //FIXME this must be known experimentally
    period = 0.3e-6;
    simple_calibration(x_orig, y_orig, period); 

    /*
    newid = gwy_app_data_browser_add_data_field(score, data, TRUE);
    g_object_unref(score);
    gwy_app_set_data_field_title(data, newid, _("Score"));
    gwy_app_sync_data_items(data, data, args->objects[0].id, newid, FALSE,
                                               GWY_DATA_ITEM_GRADIENT, 0);
    */


}

static void
xoffset_changed_cb(GtkAdjustment *adj,
                 SimpleControls *controls)
{
    SimpleArgs *args = controls->args;

    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    args->xoffset = gtk_adjustment_get_value(adj) * pow10(args->xyexponent);
    simple_dialog_update(controls, args);
    controls->in_update = FALSE;

}

static void
yoffset_changed_cb(GtkAdjustment *adj,
                 SimpleControls *controls)
{
    SimpleArgs *args = controls->args;

    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    args->yoffset = gtk_adjustment_get_value(adj) * pow10(args->xyexponent);
    simple_dialog_update(controls, args);
    controls->in_update = FALSE;

}

static void
xperiod_changed_cb(GtkAdjustment *adj,
                 SimpleControls *controls)
{
    SimpleArgs *args = controls->args;

    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    args->xperiod = gtk_adjustment_get_value(adj) * pow10(args->xyexponent);
    simple_dialog_update(controls, args);
    controls->in_update = FALSE;

}

static void
yperiod_changed_cb(GtkAdjustment *adj,
                 SimpleControls *controls)
{
    SimpleArgs *args = controls->args;

    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    args->yperiod = gtk_adjustment_get_value(adj) * pow10(args->xyexponent);
    simple_dialog_update(controls, args);
    controls->in_update = FALSE;

}


static void
zoffset_changed_cb(GtkAdjustment *adj,
                 SimpleControls *controls)
{
    SimpleArgs *args = controls->args;

    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    args->zoffset = gtk_adjustment_get_value(adj) * pow10(args->xyexponent);
    simple_dialog_update(controls, args);
    controls->in_update = FALSE;

}


static void
xyexponent_changed_cb(GtkWidget *combo,
                      SimpleControls *controls)
{
    SimpleArgs *args = controls->args;

    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    args->xyexponent = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(combo));
    args->xoffset = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->xoffset))
                  * pow10(args->xyexponent);
    args->yoffset = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->yoffset))
                  * pow10(args->xyexponent);

    simple_dialog_update(controls, args);
    controls->in_update = FALSE;
}

static void
zexponent_changed_cb(GtkWidget *combo,
                      SimpleControls *controls)
{
    SimpleArgs *args = controls->args;

    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    args->zexponent = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(combo));
    args->zoffset = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->zoffset))
                  * pow10(args->zexponent);


    simple_dialog_update(controls, args);
    controls->in_update = FALSE;
}


static void
units_change_cb(GtkWidget *button,
                SimpleControls *controls)
{
    GtkWidget *dialog, *hbox, *label, *entry;
    const gchar *id, *unit;
    gint response;
    SimpleArgs *args = controls->args;

    if (controls->in_update)
        return;

    controls->in_update = TRUE;

    id = g_object_get_data(G_OBJECT(button), "id");
    dialog = gtk_dialog_new_with_buttons(_("Change Units"),
                                         NULL,
                                         GTK_DIALOG_MODAL
                                         | GTK_DIALOG_NO_SEPARATOR,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    hbox = gtk_hbox_new(FALSE, 6);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 0);

    label = gtk_label_new_with_mnemonic(_("New _units:"));
    gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);

    entry = gtk_entry_new();
    gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
    gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0);

    gtk_widget_show_all(dialog);
    response = gtk_dialog_run(GTK_DIALOG(dialog));
    if (response != GTK_RESPONSE_OK) {
        gtk_widget_destroy(dialog);
        controls->in_update = FALSE;
        return;
    }

    unit = gtk_entry_get_text(GTK_ENTRY(entry));

    if (gwy_strequal(id, "xy")) {
        set_combo_from_unit(controls->xyexponent, unit, 0);
        controls->args->xyunit = g_strdup(unit);
     }
    else if (gwy_strequal(id, "z")) {
        set_combo_from_unit(controls->zexponent, unit, 0);
        controls->args->zunit = g_strdup(unit);
    }

    gtk_widget_destroy(dialog);

    simple_dialog_update(controls, args);
    controls->in_update = FALSE;
}
static void
set_combo_from_unit(GtkWidget *combo,
                    const gchar *str,
                    gint basepower)
{
    GwySIUnit *unit;
    gint power10;

    unit = gwy_si_unit_new_parse(str, &power10);
    power10 += basepower;
    gwy_combo_box_metric_unit_set_unit(GTK_COMBO_BOX(combo),
                                       power10 - 6, power10 + 6, unit);
    g_object_unref(unit);
}


//note that this is taken from basicops.c
static void
flip_xy(GwyDataField *source, GwyDataField *dest, gboolean minor)
{
    gint xres, yres, i, j;
    gdouble *dd;
    const gdouble *sd;

    xres = gwy_data_field_get_xres(source);
    yres = gwy_data_field_get_yres(source);
    gwy_data_field_resample(dest, yres, xres, GWY_INTERPOLATION_NONE);
    sd = gwy_data_field_get_data_const(source);
    dd = gwy_data_field_get_data(dest);
    if (minor) {
        for (i = 0; i < xres; i++) {
            for (j = 0; j < yres; j++) {
                dd[i*yres + j] = sd[j*xres + (xres - 1 - i)];
            }
        }
    }
    else {
        for (i = 0; i < xres; i++) {
            for (j = 0; j < yres; j++) {
                dd[i*yres + (yres - 1 - j)] = sd[j*xres + i];
            }
        }
    }
    gwy_data_field_set_xreal(dest, gwy_data_field_get_yreal(source));
    gwy_data_field_set_yreal(dest, gwy_data_field_get_xreal(source));
}
/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

