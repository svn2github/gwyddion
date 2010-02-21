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
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyexpr.h>
#include <libprocess/datafield.h>
#include <libprocess/gwyprocess.h>
#include <libgwydgets/gwystock.h>
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

typedef struct {
    GwyContainer *data;
    gint id;
} GwyDataObjectId;

typedef struct {
    GwyDataObjectId objects[NARGS];
    gchar *name[NARGS];
    guint pos[NARGS];
} SimpleArgs;

typedef struct {
    SimpleArgs *args;
    GtkWidget *dialog;
    GtkWidget *data[NARGS];
} SimpleControls;

static gboolean     module_register           (void);
static void         simple                (GwyContainer *data,
                                               GwyRunType run);
static gboolean     simple_dialog         (SimpleArgs *args);
static void         simple_data_cb        (GwyDataChooser *chooser,
                                               SimpleControls *controls);
static const gchar* simple_check          (SimpleArgs *args);
static void         simple_do             (SimpleArgs *args);
static void         flip_xy              (GwyDataField *source, 
                                          GwyDataField *dest, 
                                          gboolean minor);


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
    gint id;

    g_return_if_fail(run & SIMPLE_RUN_MODES);

    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD_ID, &id, 0);

    settings = gwy_app_settings_get();
    for (i = 0; i < NARGS; i++) {
        args.objects[i].data = data;
        args.objects[i].id = id;
    }

    if (simple_dialog(&args)) {
        simple_do(&args);
    }
}

static gboolean
simple_dialog(SimpleArgs *args)
{
    SimpleControls controls;
    GtkWidget *dialog, *table, *chooser,  *label;
    guint i, row, response;

    controls.args = args;

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

    args->name[0] = g_strdup_printf("Graging image");
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

