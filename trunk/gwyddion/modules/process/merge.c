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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "config.h"
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/arithmetic.h>
#include <libprocess/correlation.h>
#include <libprocess/stats.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwydgets/gwystock.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>

#define MERGE_RUN_MODES GWY_RUN_INTERACTIVE

typedef enum {
    GWY_MERGE_DIRECTION_UP,
    GWY_MERGE_DIRECTION_DOWN,
    GWY_MERGE_DIRECTION_RIGHT,
    GWY_MERGE_DIRECTION_LEFT,
    GWY_MERGE_DIRECTION_LAST
} GwyMergeDirectionType;

typedef enum {
    GWY_MERGE_MODE_CORRELATE,
    GWY_MERGE_MODE_NONE,
    GWY_MERGE_MODE_LAST
} GwyMergeModeType;

typedef enum {
    GWY_MERGE_BOUNDARY_FIRST,
    GWY_MERGE_BOUNDARY_SECOND,
    GWY_MERGE_BOUNDARY_SMOOTH,
    GWY_MERGE_BOUNDARY_LAST
} GwyMergeBoundaryType;

typedef struct {
    GwyContainer *data;
    gint id;
} GwyDataObjectId;

typedef struct {
    gint x;
    gint y;
    gint width;
    gint height;
} GwyRectangle;

typedef struct {
    gint x;
    gint y;
} GwyCoord;

typedef struct {
    GwyMergeDirectionType direction;
    GwyMergeModeType mode;
    GwyMergeBoundaryType boundary;
    GwyDataObjectId op1;
    GwyDataObjectId op2;
} MergeArgs;

static gboolean module_register      (void);
static void     merge                (GwyContainer *data,
                                      GwyRunType run);
static gboolean merge_dialog         (MergeArgs *args);
static void     merge_data_cb        (GwyDataChooser *chooser,
                                      GwyDataObjectId *object);
static gboolean merge_data_filter    (GwyContainer *data,
                                      gint id,
                                      gpointer user_data);
static gboolean merge_do             (MergeArgs *args);
static void     merge_do_uncorrelated(MergeArgs *args);
static void     merge_direction_cb   (GtkWidget *combo,
                                      MergeArgs *args);
static void     merge_mode_cb        (GtkWidget *combo,
                                      MergeArgs *args);
static void     merge_boundary_cb    (GtkWidget *combo,
                                      MergeArgs *args);
static void     merge_load_args      (GwyContainer *settings,
                                      MergeArgs *args);
static void     merge_save_args      (GwyContainer *settings,
                                      MergeArgs *args);
static void     merge_sanitize_args  (MergeArgs *args);
static gboolean get_score_iteratively(GwyDataField *data_field,
                                      GwyDataField *kernel_field,
                                      GwyDataField *score,
                                      MergeArgs *args);
static void     find_score_maximum   (GwyDataField *correlation_score,
                                      gint *max_col,
                                      gint *max_row);
static void     merge_boundary       (GwyDataField *dfield1,
                                      GwyDataField *dfield2,
                                      GwyDataField *result,
                                      GwyRectangle res_rect,
                                      GwyCoord f1_pos,
                                      GwyCoord f2_pos);

static void     put_fields           (GwyDataField *dfield1,
                                      GwyDataField *dfield2,
                                      GwyDataField *result,
                                      GwyMergeBoundaryType boundary,
                                      gint px1,
                                      gint py1,
                                      gint px2,
                                      gint py2);

static const GwyEnum directions[] = {
    { N_("Up"),           GWY_MERGE_DIRECTION_UP,    },
    { N_("Down"),         GWY_MERGE_DIRECTION_DOWN,  },
    { N_("adverb|Right"), GWY_MERGE_DIRECTION_RIGHT, },
    { N_("adverb|Left"),  GWY_MERGE_DIRECTION_LEFT,  },
};

static const GwyEnum modes[] = {
    { N_("Correlation"),     GWY_MERGE_MODE_CORRELATE, },
    { N_("merge-mode|None"), GWY_MERGE_MODE_NONE,      },
};

static const GwyEnum boundaries[] = {
    { N_("First operand"),   GWY_MERGE_BOUNDARY_FIRST  },
    { N_("Second operand"),  GWY_MERGE_BOUNDARY_SECOND },
    { N_("Smooth"),          GWY_MERGE_BOUNDARY_SMOOTH },
};

static const MergeArgs merge_defaults = {
    GWY_MERGE_DIRECTION_RIGHT,
    GWY_MERGE_MODE_CORRELATE,
    GWY_MERGE_BOUNDARY_FIRST,
    { NULL, -1 },
    { NULL, -1 },
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Merges two images."),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.5",
    "David Nečas (Yeti) & Petr Klapetek",
    "2006",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("merge",
                              (GwyProcessFunc)&merge,
                              N_("/M_ultidata/_Merge..."),
                              GWY_STOCK_MERGE,
                              MERGE_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Merge two images"));

    return TRUE;
}

static void
merge(GwyContainer *data, GwyRunType run)
{
    MergeArgs args;
    GwyContainer *settings;

    g_return_if_fail(run & MERGE_RUN_MODES);

    settings = gwy_app_settings_get();
    merge_load_args(settings, &args);

    args.op1.data = data;
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD_ID, &args.op1.id, 0);
    args.op2 = args.op1;

    if (merge_dialog(&args)) {
        if (args.mode == GWY_MERGE_MODE_NONE)
            merge_do_uncorrelated(&args);
        else
            merge_do(&args);
    }

    merge_save_args(settings, &args);
}

static gboolean
merge_dialog(MergeArgs *args)
{
    GtkWidget *dialog, *table, *chooser, *combo;
    gint response, row;
    gboolean ok;

    dialog = gtk_dialog_new_with_buttons(_("Merge Data"), NULL, 0,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    table = gtk_table_new(4, 4, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, TRUE, TRUE, 4);
    row = 0;

    /* Merge with */
    chooser = gwy_data_chooser_new_channels();
    g_object_set_data(G_OBJECT(chooser), "dialog", dialog);
    gwy_data_chooser_set_filter(GWY_DATA_CHOOSER(chooser),
                                merge_data_filter, &args->op1, NULL);
    g_signal_connect(chooser, "changed",
                     G_CALLBACK(merge_data_cb), &args->op2);
    merge_data_cb(GWY_DATA_CHOOSER(chooser), &args->op2);
    gwy_table_attach_hscale(table, row, _("_Merge with:"), NULL,
                            GTK_OBJECT(chooser), GWY_HSCALE_WIDGET);
    row++;

    /* Parameters */
    combo = gwy_enum_combo_box_new(directions, G_N_ELEMENTS(directions),
                                   G_CALLBACK(merge_direction_cb), args,
                                   args->direction, TRUE);
    gwy_table_attach_hscale(table, row, _("_Put second operand:"), NULL,
                            GTK_OBJECT(combo), GWY_HSCALE_WIDGET);
    row++;

    combo = gwy_enum_combo_box_new(modes, G_N_ELEMENTS(modes),
                                   G_CALLBACK(merge_mode_cb), args,
                                   args->mode, TRUE);
    gwy_table_attach_hscale(table, row, _("_Align second operand:"), NULL,
                            GTK_OBJECT(combo), GWY_HSCALE_WIDGET);
    row++;

    combo = gwy_enum_combo_box_new(boundaries, G_N_ELEMENTS(boundaries),
                                   G_CALLBACK(merge_boundary_cb), args,
                                   args->boundary, TRUE);
    gwy_table_attach_hscale(table, row, _("_Boundary treatment:"), NULL,
                            GTK_OBJECT(combo), GWY_HSCALE_WIDGET);
    row++;

    gtk_widget_show_all(dialog);

    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            case GTK_RESPONSE_NONE:
            gtk_widget_destroy(dialog);
            return FALSE;
            break;

            case GTK_RESPONSE_OK:
            ok = TRUE;
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (!ok);

    gtk_widget_destroy(dialog);

    return TRUE;
}

static void
merge_data_cb(GwyDataChooser *chooser,
              GwyDataObjectId *object)
{
    GtkWidget *dialog;

    object->data = gwy_data_chooser_get_active(chooser, &object->id);
    gwy_debug("data: %p %d", object->data, object->id);

    dialog = g_object_get_data(G_OBJECT(chooser), "dialog");
    g_assert(GTK_IS_DIALOG(dialog));
    gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog), GTK_RESPONSE_OK,
                                      object->data != NULL);
}

static gboolean
merge_data_filter(GwyContainer *data,
                  gint id,
                  gpointer user_data)
{
    GwyDataObjectId *object = (GwyDataObjectId*)user_data;
    GwyDataField *op1, *op2;
    GQuark quark;

    quark = gwy_app_get_data_key_for_id(id);
    op1 = GWY_DATA_FIELD(gwy_container_get_object(data, quark));

    quark = gwy_app_get_data_key_for_id(object->id);
    op2 = GWY_DATA_FIELD(gwy_container_get_object(object->data, quark));

    return !gwy_data_field_check_compatibility(op1, op2,
                                               GWY_DATA_COMPATIBILITY_MEASURE
                                               | GWY_DATA_COMPATIBILITY_LATERAL
                                               | GWY_DATA_COMPATIBILITY_VALUE);
}

static void
merge_direction_cb(GtkWidget *combo, MergeArgs *args)
{
    args->direction = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(combo));
}

static void
merge_mode_cb(GtkWidget *combo, MergeArgs *args)
{
    args->mode = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(combo));
}

static void
merge_boundary_cb(GtkWidget *combo, MergeArgs *args)
{
    args->boundary = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(combo));
}

static gboolean
merge_do(MergeArgs *args)
{
    GwyContainer *data;
    GwyDataField *dfield1, *dfield2;
    GwyDataField *correlation_data, *correlation_kernel, *correlation_score;
    GwyDataField *result;
    GwyRectangle cdata, kdata;
    gint max_col, max_row;
    gint newxres, newyres;
    gint xres1, xres2, yres1, yres2;
    gint xshift, yshift;
    GQuark quark;
    gint newid;
    GwyMergeDirectionType real_dir = args->direction;
    GwyMergeBoundaryType real_boundary = args->boundary;
    gint px1, py1, px2, py2;

    quark = gwy_app_get_data_key_for_id(args->op1.id);
    dfield1 = GWY_DATA_FIELD(gwy_container_get_object(args->op1.data, quark));

    quark = gwy_app_get_data_key_for_id(args->op2.id);
    dfield2 = GWY_DATA_FIELD(gwy_container_get_object(args->op2.data, quark));

    if ((dfield1->xres*dfield1->yres) < (dfield2->xres*dfield2->yres)) {
        GWY_SWAP(GwyDataField*, dfield1, dfield2);
        if (args->direction == GWY_MERGE_DIRECTION_UP)
            real_dir = GWY_MERGE_DIRECTION_DOWN;
        else if (args->direction == GWY_MERGE_DIRECTION_DOWN)
            real_dir = GWY_MERGE_DIRECTION_UP;
        else if (args->direction == GWY_MERGE_DIRECTION_LEFT)
            real_dir = GWY_MERGE_DIRECTION_RIGHT;
        else if (args->direction == GWY_MERGE_DIRECTION_RIGHT)
            real_dir = GWY_MERGE_DIRECTION_LEFT;
        else
            g_return_val_if_reached(FALSE);

        if (args->boundary == GWY_MERGE_BOUNDARY_FIRST)
            real_boundary = GWY_MERGE_BOUNDARY_SECOND;
        else if (args->boundary == GWY_MERGE_BOUNDARY_SECOND)
            real_boundary = GWY_MERGE_BOUNDARY_FIRST;
    }

    result = gwy_data_field_new_alike(dfield1, FALSE);

    xres1 = gwy_data_field_get_xres(dfield1);
    xres2 = gwy_data_field_get_xres(dfield2);
    yres1 = gwy_data_field_get_yres(dfield1);
    yres2 = gwy_data_field_get_yres(dfield2);

    /*cut data for correlation*/
    switch (real_dir) {
        case GWY_MERGE_DIRECTION_UP:
        cdata.x = 0;
        cdata.y = 0;
        cdata.width = xres1;
        cdata.height = yres1/2;
        kdata.width = MIN(xres2, cdata.width/2);
        kdata.height = MIN(yres2, cdata.height/3);
        kdata.x = MAX(0, xres2/2 - kdata.width/2);
        kdata.y = MAX(0, yres2 - cdata.height/3);
        break;

        case GWY_MERGE_DIRECTION_DOWN:
        cdata.x = 0;
        cdata.y = yres1 - (yres1/2);
        cdata.width = xres1;
        cdata.height = yres1/2;
        kdata.width = MIN(xres2, cdata.width/2);
        kdata.height = MIN(yres2, cdata.height/3);
        kdata.x = MAX(0, xres2/2 - kdata.width/2);
        kdata.y = 0;
        break;

        case GWY_MERGE_DIRECTION_RIGHT:
        cdata.x = xres1 - (xres1/2);
        cdata.y = 0;
        cdata.width = xres1/2;
        cdata.height = yres1;
        kdata.width = MIN(xres2, cdata.width/3);
        kdata.height = MIN(yres2, cdata.height/2);
        kdata.x = 0;
        kdata.y = MAX(0, yres2/2 - kdata.height/2);
        break;

        case GWY_MERGE_DIRECTION_LEFT:
        cdata.x = 0;
        cdata.y = 0;
        cdata.width = xres1/2;
        cdata.height = yres1;
        kdata.width = MIN(xres2, cdata.width/3);
        kdata.height = MIN(yres2, cdata.height/2);
        kdata.x = MAX(0, xres2 - cdata.width/3);
        kdata.y = MAX(0, yres2/2 - kdata.height/2);
        break;

        default:
        g_assert_not_reached();
        break;
    }

    correlation_data = gwy_data_field_area_extract(dfield1,
                                                   cdata.x,
                                                   cdata.y,
                                                   cdata.width,
                                                   cdata.height);
    correlation_kernel = gwy_data_field_area_extract(dfield2,
                                                     kdata.x,
                                                     kdata.y,
                                                     kdata.width,
                                                     kdata.height);
    correlation_score = gwy_data_field_new_alike(correlation_data, FALSE);

    /* get appropriate correlation score */
    if (!get_score_iteratively(correlation_data, correlation_kernel,
                               correlation_score, args)) {
        g_object_unref(correlation_score);
        g_object_unref(correlation_data);
        g_object_unref(correlation_kernel);
        g_object_unref(result);
        return FALSE;
    }

    find_score_maximum(correlation_score, &max_col, &max_row);
    gwy_debug("c: %d %d %dx%d  k: %d %d %dx%d res: %d %d",
              cdata.x,
              cdata.y,
              cdata.width,
              cdata.height,
              kdata.x,
              kdata.y,
              kdata.width,
              kdata.height,
              max_col, max_row);
    /* enlarge result field to fit the new data */
    switch (real_dir) {
        case GWY_MERGE_DIRECTION_UP:
        xshift = MAX(0,  (max_col - cdata.width/2) + (xres1 - xres2)/2);
        newxres =  MAX(MAX(xres1, xres2),
                      (MAX(0, (max_col - cdata.width/2)) + xres2/2) -
                      (MIN(0, (max_col - cdata.width/2)) - xres2/2));
        newyres = yres1 + yres2 - max_row - kdata.height/2;

        px2 = xshift;
        py2 = 0;
        px1 = xshift - ((max_col - kdata.width/2) - kdata.x);
        py1 = yres2 - max_row - kdata.height/2;
        break;

        case GWY_MERGE_DIRECTION_DOWN:
        xshift = MAX(0,  (max_col - cdata.width/2) + (xres1 - xres2)/2);
        newxres =  MAX(MAX(xres1, xres2),
                      (MAX(0, (max_col - cdata.width/2)) + xres2/2) -
                      (MIN(0, (max_col - cdata.width/2)) - xres2/2));
        newyres = cdata.y + (max_row - kdata.height/2) + yres2;;

        px2 = xshift;
        py1 = 0;
        px1 = xshift - ((max_col - kdata.width/2) - kdata.x);
        py2 = cdata.y + (max_row - kdata.height/2);
        break;

        case GWY_MERGE_DIRECTION_LEFT:
        yshift = MAX(0,  (max_row - cdata.height/2) + (yres1 - yres2)/2);
        newxres = xres1 + xres2 - max_col - kdata.width/2;
        newyres = MAX(MAX(yres1, yres2),
                      (MAX(0, (max_row - cdata.height/2)) + yres2/2) -
                      (MIN(0, (max_row - cdata.height/2)) - yres2/2));
        gwy_debug("%d %d %d %d",
                  yres1, yres2,
                  (MAX(0, (max_row - cdata.height/2)) + yres2/2),
                  (MIN(0, (max_row - cdata.height/2)) - yres2/2));
        gwy_debug("newyres: %d, yshift: %d", newyres, yshift);

        px2 = 0;
        py2 = yshift;
        px1 = xres2 - max_col - kdata.width/2;
        py1 = yshift - ((max_row - kdata.height/2) - kdata.y);
        break;

        case GWY_MERGE_DIRECTION_RIGHT:
        yshift = MAX(0,  -(max_row - cdata.height/2) - (yres1 - yres2)/2);
        newxres = cdata.x + (max_col - kdata.width/2) + xres2;
        newyres = MAX(MAX(yres1, yres2),
                      (MAX(0, (max_row - cdata.height/2)) + yres2/2) -
                      (MIN(0, (max_row - cdata.height/2)) - yres2/2));
        gwy_debug("%d %d %d %d",
                  yres1, yres2,
                  (MAX(0, (max_row - cdata.height/2)) + yres2/2),
                  (MIN(0, (max_row - cdata.height/2)) - yres2/2));
        gwy_debug("newyres: %d, yshift: %d", newyres, yshift);


        px1 = 0;
        py1 = yshift;
        px2 = cdata.x + (max_col - kdata.width/2);
        py2 = yshift + ((max_row - kdata.height/2) - kdata.y);
        break;

        default:
        g_assert_not_reached();
        break;
    }

    gwy_data_field_resample(result, newxres, newyres, GWY_INTERPOLATION_NONE);
    put_fields(dfield1, dfield2, result, real_boundary, px1, py1, px2, py2);

    /* set right output */
    if (result) {
        gwy_app_data_browser_get_current(GWY_APP_CONTAINER, &data, 0);
        newid = gwy_app_data_browser_add_data_field(result, data, TRUE);
        gwy_app_set_data_field_title(data, newid, _("Merged images"));
        gwy_app_sync_data_items(args->op1.data, data, args->op1.id, newid,
                                FALSE,
                                GWY_DATA_ITEM_PALETTE,
                                GWY_DATA_ITEM_MASK_COLOR,
                                GWY_DATA_ITEM_RANGE,
                                0);
        g_object_unref(result);
    }

    g_object_unref(correlation_data);
    g_object_unref(correlation_kernel);
    g_object_unref(correlation_score);

    return TRUE;
}

static void
merge_do_uncorrelated(MergeArgs *args)
{
    GwyContainer *data;
    GwyDataField *dfield1, *dfield2, *result;
    gint newxres, newyres, xres1, xres2, yres1, yres2;
    GQuark quark;
    gint newid;
    gdouble m;

    quark = gwy_app_get_data_key_for_id(args->op1.id);
    dfield1 = GWY_DATA_FIELD(gwy_container_get_object(args->op1.data, quark));

    quark = gwy_app_get_data_key_for_id(args->op2.id);
    dfield2 = GWY_DATA_FIELD(gwy_container_get_object(args->op2.data, quark));

    xres1 = gwy_data_field_get_xres(dfield1);
    xres2 = gwy_data_field_get_xres(dfield2);
    yres1 = gwy_data_field_get_yres(dfield1);
    yres2 = gwy_data_field_get_yres(dfield2);
    result = gwy_data_field_new_alike(dfield1, FALSE);
    m = MIN(gwy_data_field_get_min(dfield1), gwy_data_field_get_min(dfield2));

    if (args->direction == GWY_MERGE_DIRECTION_UP
        || args->direction == GWY_MERGE_DIRECTION_DOWN) {
        newxres = MAX(xres1, xres2);
        newyres = yres1 + yres2;
    }
    else if (args->direction == GWY_MERGE_DIRECTION_LEFT
          || args->direction == GWY_MERGE_DIRECTION_RIGHT) {
        newxres = xres1 + xres2;
        newyres = MAX(yres1, yres2);
    }
    else {
        g_return_if_reached();
    }

    gwy_data_field_resample(result, newxres, newyres, GWY_INTERPOLATION_NONE);
    gwy_data_field_fill(result, m);

    if (args->direction == GWY_MERGE_DIRECTION_UP) {
        gwy_data_field_area_copy(dfield2, result, 0, 0, xres2, yres2, 0, 0);
        gwy_data_field_area_copy(dfield1, result, 0, 0, xres1, yres1, 0, yres2);
    }
    else if (args->direction == GWY_MERGE_DIRECTION_DOWN) {
        gwy_data_field_area_copy(dfield1, result, 0, 0, xres1, yres1, 0, 0);
        gwy_data_field_area_copy(dfield2, result, 0, 0, xres2, yres2, 0, yres1);
    }
    else if (args->direction == GWY_MERGE_DIRECTION_LEFT) {
        gwy_data_field_area_copy(dfield2, result, 0, 0, xres2, yres2, 0, 0);
        gwy_data_field_area_copy(dfield1, result, 0, 0, xres1, yres1, xres2, 0);
    }
    else if (args->direction == GWY_MERGE_DIRECTION_RIGHT) {
        gwy_data_field_area_copy(dfield1, result, 0, 0, xres1, yres1, 0, 0);
        gwy_data_field_area_copy(dfield2, result, 0, 0, xres2, yres2, xres1, 0);
    }

    gwy_app_data_browser_get_current(GWY_APP_CONTAINER, &data, 0);
    newid = gwy_app_data_browser_add_data_field(result, data, TRUE);
    gwy_app_set_data_field_title(data, newid, _("Merged images"));
    gwy_app_sync_data_items(args->op1.data, data, args->op1.id, newid,
                            FALSE,
                            GWY_DATA_ITEM_PALETTE,
                            GWY_DATA_ITEM_MASK_COLOR,
                            GWY_DATA_ITEM_RANGE,
                            0);
    g_object_unref(result);
}

static void
put_fields(GwyDataField *dfield1, GwyDataField *dfield2,
           GwyDataField *result,
           GwyMergeBoundaryType boundary,
           gint px1, gint py1,
           gint px2, gint py2)
{
    GwyRectangle res_rect;
    GwyCoord f1_pos;
    GwyCoord f2_pos;
    gint x1, x2, y1_, y2, w1, w2, h1, h2;
    gint xres1, xres2, yres1, yres2;

    gwy_debug("px1: %u, py1: %u, px2: %u, py2: %u", px1, py1, px2, py2);
    xres1 = gwy_data_field_get_xres(dfield1);
    yres1 = gwy_data_field_get_yres(dfield1);
    xres2 = gwy_data_field_get_xres(dfield2);
    yres2 = gwy_data_field_get_yres(dfield2);

    /* Use the pixels sizes of field 1 but they must be identical. */
    gwy_data_field_set_xreal(result,
                             gwy_data_field_get_xres(result)
                             * gwy_data_field_get_xmeasure(dfield1));
    gwy_data_field_set_yreal(result,
                             gwy_data_field_get_yres(result)
                             * gwy_data_field_get_ymeasure(dfield1));

    gwy_data_field_fill(result,
                        MIN(gwy_data_field_get_min(dfield1),
                            gwy_data_field_get_min(dfield2)));

    x1 = y1_ = x2 = y2 = 0;
    w1 = gwy_data_field_get_xres(dfield1);
    h1 = gwy_data_field_get_yres(dfield1);
    w2 = gwy_data_field_get_xres(dfield2);
    h2 = gwy_data_field_get_yres(dfield2);

    if (boundary == GWY_MERGE_BOUNDARY_SMOOTH
        || boundary == GWY_MERGE_BOUNDARY_SECOND) {
        gwy_data_field_area_copy(dfield1, result,
                                 x1, y1_, w1, h1,
                                 px1, py1);
        gwy_data_field_area_copy(dfield2, result,
                                 x2, y2, w2, h2,
                                 px2, py2);
    }
    else {
        gwy_data_field_area_copy(dfield2, result,
                                 x2, y2, w2, h2,
                                 px2, py2);

        gwy_data_field_area_copy(dfield1, result,
                                 x1, y1_, w1, h1,
                                 px1, py1);
    }


    /* adjust boundary to be as smooth as possible */
    if (boundary == GWY_MERGE_BOUNDARY_SMOOTH) {
        if (px1 < px2) {
            res_rect.x = px2;
            res_rect.width = px1 + xres1 - px2;
        } else {
            res_rect.x = px1;
            res_rect.width = px2 + xres2 - px1;
        }

        if (py1 < py2) {
            res_rect.y = py2;
            res_rect.height = MIN(MIN(yres1, yres2),
                                  py1 + yres1 - py2 - (yres1 - yres2));
        }
        else {
            res_rect.y = py1;
            res_rect.height = py2 + yres2 - py1;
        }

        f1_pos.x = res_rect.x - px1;
        f1_pos.y = res_rect.y - py1;
        f2_pos.x = res_rect.x - px2;
        f2_pos.y = res_rect.y - py2;


        /*gwy_data_field_area_multiply(result, res_rect.x, res_rect.y,
                                     res_rect.width, res_rect.height, 1.5);*/
        merge_boundary(dfield1, dfield2, result, res_rect, f1_pos, f2_pos);
    }
}

/* compute corelation */
static gboolean
get_score_iteratively(GwyDataField *data_field, GwyDataField *kernel_field,
                      GwyDataField *score, MergeArgs *args)
{
    enum { WORK_PER_UPDATE = 50000000 };
    GwyComputationState *state;
    gboolean ok = FALSE;
    int work, wpi;

    work = 0;
    wpi = gwy_data_field_get_xres(kernel_field)
          *gwy_data_field_get_yres(kernel_field);
    wpi = MIN(wpi, WORK_PER_UPDATE);
    state = gwy_data_field_correlate_init(data_field, kernel_field, score);

    /* FIXME */
    gwy_app_wait_start(gwy_app_find_window_for_channel(args->op1.data,
                                                       args->op1.id),
                       _("Initializing"));
    gwy_data_field_correlate_iteration(state);
    if (!gwy_app_wait_set_message(_("Correlating")))
        goto get_score_fail;
    do {
        gwy_data_field_correlate_iteration(state);
        work += wpi;
        if (work > WORK_PER_UPDATE) {
            work -= WORK_PER_UPDATE;
            if (!gwy_app_wait_set_fraction(state->fraction))
                goto get_score_fail;
        }
    } while (state->state != GWY_COMPUTATION_STATE_FINISHED);
    ok = TRUE;

get_score_fail:
    gwy_data_field_correlate_finalize(state);
    gwy_app_wait_finish();

    return ok;
}

static void
find_score_maximum(GwyDataField *correlation_score,
                   gint *max_col,
                   gint *max_row)
{
    gint i, n, maxi = 0;
    gdouble max = -G_MAXDOUBLE;
    const gdouble *data;

    n = gwy_data_field_get_xres(correlation_score)
        *gwy_data_field_get_yres(correlation_score);
    data = gwy_data_field_get_data_const(correlation_score);

    for (i = 0; i < n; i++) {
        if (max < data[i]) {
            max = data[i];
            maxi = i;
        }
    }

    *max_row = (gint)floor(maxi/gwy_data_field_get_xres(correlation_score));
    *max_col = maxi - (*max_row)*gwy_data_field_get_xres(correlation_score);
}

static inline gboolean
gwy_data_field_inside(GwyDataField *data_field, gint i, gint j)
{
    if (i >= 0
        && j >= 0
        && i < gwy_data_field_get_xres(data_field)
        && j < gwy_data_field_get_yres(data_field))
        return TRUE;
    else
        return FALSE;
}


static void
merge_boundary(GwyDataField *dfield1,
               GwyDataField *dfield2,
               GwyDataField *result,
               GwyRectangle res_rect,
               GwyCoord f1_pos,
               GwyCoord f2_pos)
{
    gint col, row;
    gdouble weight, val1, val2;

    gwy_debug("dfield1: %u x %u at (%u, %u)",
              dfield1->xres, dfield1->yres, f1_pos.x, f1_pos.y);
    gwy_debug("dfield2: %u x %u at (%u, %u)",
              dfield2->xres, dfield2->yres, f2_pos.x, f2_pos.y);
    gwy_debug("result: %u x %u", result->xres, result->yres);
    gwy_debug("rect in result : %u x %u at (%u,%u)",
              res_rect.width, res_rect.height, res_rect.x, res_rect.y);

    for (col = 0; col < res_rect.width; col++) {
        for (row = 0; row < res_rect.height; row++) {

            weight = 0.5; /*FIXME adapt weight to direction*/

            /* FIXME: This sometimes asks for values outside the field. */
            val1 = gwy_data_field_get_val(dfield1,
                                          col + f1_pos.x, row + f1_pos.y);
            val2 = gwy_data_field_get_val(dfield2,
                                          col + f2_pos.x, row + f2_pos.y);
            gwy_data_field_set_val(result,
                                   col + res_rect.x, row + res_rect.y,
                                   (1 - weight)*val1 + weight*val2);
        }
    }
}

static const gchar direction_key[] = "/module/merge/direction";
static const gchar mode_key[]      = "/module/merge/mode";
static const gchar boundary_key[]  = "/module/merge/boundary";

static void
merge_sanitize_args(MergeArgs *args)
{
    args->direction = MIN(args->direction, GWY_MERGE_DIRECTION_LAST - 1);
    args->mode = MIN(args->mode, GWY_MERGE_MODE_LAST - 1);
    args->boundary = MIN(args->boundary, GWY_MERGE_BOUNDARY_LAST - 1);
}

static void
merge_load_args(GwyContainer *settings,
                MergeArgs *args)
{
    *args = merge_defaults;
    gwy_container_gis_enum_by_name(settings, direction_key, &args->direction);
    gwy_container_gis_enum_by_name(settings, mode_key, &args->mode);
    gwy_container_gis_enum_by_name(settings, boundary_key, &args->boundary);
    merge_sanitize_args(args);
}

static void
merge_save_args(GwyContainer *settings,
                MergeArgs *args)
{
    gwy_container_set_enum_by_name(settings, direction_key, args->direction);
    gwy_container_set_enum_by_name(settings, mode_key, args->mode);
    gwy_container_set_enum_by_name(settings, boundary_key, args->boundary);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
