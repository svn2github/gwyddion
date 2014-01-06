/*
 *  @(#) $Id$
 *  Copyright (C) 2003-2007,2012 David Necas (Yeti), Petr Klapetek.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include <string.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/stats.h>
#include <libprocess/filters.h>
#include <libprocess/hough.h>
#include <libprocess/level.h>
#include <libprocess/grains.h>
#include <libprocess/elliptic.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwylayer-basic.h>
#include <libgwydgets/gwyradiobuttons.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwystock.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>

#define EDGE_RUN_MODES GWY_RUN_IMMEDIATE
#define EDGE_UI_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

enum {
    PREVIEW_SIZE = 400
};

enum {
    RESPONSE_RESET   = 1,
    RESPONSE_PREVIEW = 2
};

typedef enum {
    DISPLAY_DATA,
    DISPLAY_LOG,
    DISPLAY_SHOW
} ZeroCrossingDisplayType;

typedef struct {
    gdouble gaussian_fwhm;
    gdouble threshold;
    gboolean update;
} ZeroCrossingArgs;

typedef struct {
    GtkWidget *dialog;
    GtkWidget *view;
    GwyPixmapLayer *layer;
    GtkObject *gaussian_fwhm;
    GtkObject *threshold;
    GSList *display_group;
    GtkWidget *update;
    GwyContainer *mydata;
    ZeroCrossingArgs *args;
    ZeroCrossingDisplayType display;
    gboolean in_init;
    gboolean computed;
    gboolean gauss_computed;
} ZeroCrossingControls;

static gboolean module_register(void);
static void     edge           (GwyContainer *data,
                                GwyRunType run,
                                const gchar *name);
static void     laplacian_do   (GwyDataField *dfield,
                                GwyDataField *show);
static void     canny_do       (GwyDataField *dfield,
                                GwyDataField *show);
static void     rms_do         (GwyDataField *dfield,
                                GwyDataField *show);
static void     rms_edge_do    (GwyDataField *dfield,
                                GwyDataField *show);
static void     nonlinearity_do(GwyDataField *dfield,
                                GwyDataField *show);
static void     hough_lines_do (GwyDataField *dfield,
                                GwyDataField *show);
static void     harris_do      (GwyDataField *dfield,
                                GwyDataField *show);
static void     inclination_do (GwyDataField *dfield,
                                GwyDataField *show);
static void     step_do        (GwyDataField *dfield,
                                GwyDataField *show);
static void     sobel_do       (GwyDataField *dfield,
                                GwyDataField *show);
static void     prewitt_do     (GwyDataField *dfield,
                                GwyDataField *show);

static void zero_crossing                      (GwyContainer *data,
                                                GwyRunType run);
static void zero_crossing_dialog               (ZeroCrossingArgs *args,
                                                GwyContainer *data,
                                                GwyDataField *dfield,
                                                gint id,
                                                GQuark squark);
static void zero_crossing_gaussian_fwhm_changed(GtkAdjustment *adj,
                                                ZeroCrossingControls *controls);
static void zero_crossing_threshold_changed    (GtkAdjustment *adj,
                                                ZeroCrossingControls *controls);
static void zero_crossing_display_changed      (GtkToggleButton *radio,
                                                ZeroCrossingControls *controls);
static void zero_crossing_update_changed       (GtkToggleButton *check,
                                                ZeroCrossingControls *controls);
static void zero_crossing_invalidate           (ZeroCrossingControls *controls);
static void zero_crossing_update_controls      (ZeroCrossingControls *controls,
                                                ZeroCrossingArgs *args);
static void zero_crossing_preview              (ZeroCrossingControls *controls,
                                                ZeroCrossingArgs *args);
static void zero_crossing_run                  (const ZeroCrossingArgs *args,
                                                GwyContainer *data,
                                                GwyDataField *dfield,
                                                GQuark squark);
static gdouble zero_crossing_do_log            (GwyDataField *dfield,
                                                GwyDataField *gauss,
                                                gdouble gaussian_fwhm);
static void zero_crossing_do_edge              (GwyDataField *show,
                                                GwyDataField *gauss,
                                                gdouble theshold);
static void zero_crossing_load_args            (GwyContainer *container,
                                                ZeroCrossingArgs *args);
static void zero_crossing_save_args            (GwyContainer *container,
                                                ZeroCrossingArgs *args);

static const ZeroCrossingArgs zero_crossing_defaults = {
    2.0,
    0.1,
    FALSE,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Several edge detection methods (Laplacian of Gaussian, Canny, "
       "and some experimental), creates presentation."),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.13",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("edge_laplacian",
                              (GwyProcessFunc)&edge,
                              N_("/_Presentation/_Edge Detection/_Laplacian of Gaussian"),
                              NULL,
                              EDGE_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Laplacian of Gaussian step detection "
                                 "presentation"));
    gwy_process_func_register("edge_canny",
                              (GwyProcessFunc)&edge,
                              N_("/_Presentation/_Edge Detection/_Canny"),
                              NULL,
                              EDGE_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Canny edge detection presentation"));
    gwy_process_func_register("edge_rms",
                              (GwyProcessFunc)&edge,
                              N_("/_Presentation/_Edge Detection/_RMS"),
                              NULL,
                              EDGE_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Local RMS value based step detection "
                                 "presentation"));
    gwy_process_func_register("edge_rms_edge",
                              (GwyProcessFunc)&edge,
                              N_("/_Presentation/_Edge Detection/RMS _Edge"),
                              NULL,
                              EDGE_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Local RMS value based step detection "
                                 "with postprocessing"));
    gwy_process_func_register("edge_nonlinearity",
                              (GwyProcessFunc)&edge,
                              N_("/_Presentation/_Edge Detection/Local _Nonlinearity"),
                              NULL,
                              EDGE_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Local nonlinearity based edge detection "
                                 "presentation"));
    gwy_process_func_register("edge_hough_lines",
                              (GwyProcessFunc)&edge,
                              N_("/_Presentation/_Edge Detection/_Hough Lines"),
                              NULL,
                              EDGE_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              /* FIXME */
                              N_("Hough lines presentation"));
    gwy_process_func_register("edge_harris",
                              (GwyProcessFunc)&edge,
                              N_("/_Presentation/_Edge Detection/_Harris Corner"),
                              NULL,
                              EDGE_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              /* FIXME */
                              N_("Harris corner presentation"));
    gwy_process_func_register("edge_inclination",
                              (GwyProcessFunc)&edge,
                              N_("/_Presentation/_Edge Detection/_Inclination"),
                              NULL,
                              EDGE_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Local inclination visualization "
                                 "presentation"));
    gwy_process_func_register("edge_step",
                              (GwyProcessFunc)&edge,
                              N_("/_Presentation/_Edge Detection/_Step"),
                              NULL,
                              EDGE_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Fine step detection presentation"));
    gwy_process_func_register("edge_sobel",
                              (GwyProcessFunc)&edge,
                              N_("/_Presentation/_Edge Detection/_Sobel"),
                              NULL,
                              EDGE_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Sobel edge presentation"));
    gwy_process_func_register("edge_prewitt",
                              (GwyProcessFunc)&edge,
                              N_("/_Presentation/_Edge Detection/_Prewitt"),
                              NULL,
                              EDGE_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Prewitt edge presentation"));
    /*
    gwy_process_func_register("edge_local_maxima",
                              (GwyProcessFunc)&edge,
                              N_("/_Presentation/_Edge Detection/Local _Maxima"),
                              NULL,
                              EDGE_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Local maxima presentation presentation"));
                              */
    gwy_process_func_register("edge_zero_crossing",
                              (GwyProcessFunc)&zero_crossing,
                              N_("/_Presentation/_Edge Detection/_Zero Crossing..."),
                              NULL,
                              EDGE_UI_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Zero crossing step detection presentation"));

    return TRUE;
}

static void
edge(GwyContainer *data, GwyRunType run, const gchar *name)
{
    static const struct {
        const gchar *name;
        void (*func)(GwyDataField *dfield, GwyDataField *show);
    }
    functions[] = {
        { "edge_canny",         canny_do,         },
        { "edge_harris",        harris_do,        },
        { "edge_hough_lines",   hough_lines_do,   },
        { "edge_inclination",   inclination_do,   },
        { "edge_laplacian",     laplacian_do,     },
        { "edge_nonlinearity",  nonlinearity_do,  },
        { "edge_rms",           rms_do,           },
        { "edge_rms_edge",      rms_edge_do,      },
        { "edge_step",          step_do,          },
        { "edge_sobel",         sobel_do,         },
        { "edge_prewitt",       prewitt_do,       },
    };
    GwyDataField *dfield, *showfield;
    GQuark dquark, squark;
    GwySIUnit *siunit;
    gchar *qualname;
    gint id;
    guint i;

    g_return_if_fail(run & EDGE_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD_KEY, &dquark,
                                     GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     GWY_APP_SHOW_FIELD_KEY, &squark,
                                     GWY_APP_SHOW_FIELD, &showfield,
                                     0);
    g_return_if_fail(dfield && dquark && squark);

    gwy_app_undo_qcheckpointv(data, 1, &squark);
    if (!showfield) {
        showfield = gwy_data_field_new_alike(dfield, FALSE);
        siunit = gwy_si_unit_new(NULL);
        gwy_data_field_set_si_unit_z(showfield, siunit);
        g_object_unref(siunit);
        gwy_container_set_object(data, squark, showfield);
        g_object_unref(showfield);
    }

    for (i = 0; i < G_N_ELEMENTS(functions); i++) {
        if (gwy_strequal(name, functions[i].name)) {
            functions[i].func(dfield, showfield);
            break;
        }
    }
    if (i == G_N_ELEMENTS(functions)) {
        g_warning("edge does not provide function `%s'", name);
        gwy_data_field_copy(dfield, showfield, FALSE);
    }

    gwy_data_field_normalize(showfield);
    gwy_data_field_data_changed(showfield);

    qualname = g_strconcat("proc::", functions[i].name, NULL);
    gwy_app_channel_log_add(data, id, id, qualname, NULL);
    g_free(qualname);
}

/* Note this is the limiting case when LoG reduces for discrete data just to
 * Laplacian */
static void
laplacian_do(GwyDataField *dfield, GwyDataField *show)
{
    gwy_data_field_copy(dfield, show, FALSE);
    gwy_data_field_filter_laplacian(show);
}

static void
canny_do(GwyDataField *dfield, GwyDataField *show)
{
    /*now we use fixed threshold, but in future, there could be API
     with some setting. We could also do smooting before apllying filter.*/
    gwy_data_field_copy(dfield, show, FALSE);
    gwy_data_field_filter_canny(show, 0.1);
}

static void
rms_do(GwyDataField *dfield, GwyDataField *show)
{
    gwy_data_field_copy(dfield, show, FALSE);
    gwy_data_field_filter_rms(show, 5);
}

static void
rms_edge_do(GwyDataField *dfield, GwyDataField *show)
{
    GwyDataField *tmp;
    gint xres, yres, i, j;
    gdouble *d;
    const gdouble *t;
    gdouble s;

    gwy_data_field_copy(dfield, show, FALSE);
    xres = gwy_data_field_get_xres(show);
    yres = gwy_data_field_get_yres(show);

    tmp = gwy_data_field_duplicate(show);
    gwy_data_field_filter_rms(tmp, 5);
    t = gwy_data_field_get_data_const(tmp);
    d = gwy_data_field_get_data(show);
    for (i = 0; i < yres; i++) {
        gint iim = MAX(i-2, 0)*xres;
        gint iip = MIN(i+2, yres-1)*xres;
        gint ii = i*xres;

        for (j = 0; j < xres; j++) {
            gint jm = MAX(j-2, 0);
            gint jp = MIN(j+2, xres-1);

            s = t[ii + jm] + t[ii + jp] + t[iim + j] + t[iip + j]
                + (t[iim + jm] + t[iim + jp] + t[iip + jm] + t[iip + jp])/2.0;
            s /= 6.0;

            d[ii + j] = MAX(t[ii + j] - s, 0);
        }
    }
    g_object_unref(tmp);
}

static gdouble
fit_local_plane_by_pos(gint n,
                       const gint *xp, const gint *yp, const gdouble *z,
                       gdouble *bx, gdouble *by)
{
    gdouble m[12], b[4];
    gint i;

    memset(m, 0, 6*sizeof(gdouble));
    memset(b, 0, 4*sizeof(gdouble));
    for (i = 0; i < n; i++) {
        m[1] += xp[i];
        m[2] += xp[i]*xp[i];
        m[3] += yp[i];
        m[4] += xp[i]*yp[i];
        m[5] += yp[i]*yp[i];
        b[0] += z[i];
        b[1] += xp[i]*z[i];
        b[2] += yp[i]*z[i];
        b[3] += z[i]*z[i];
    }
    m[0] = n;
    memcpy(m + 6, m, 6*sizeof(gdouble));
    if (gwy_math_choleski_decompose(3, m))
        gwy_math_choleski_solve(3, m, b);
    else
        b[0] = b[1] = b[2] = 0.0;

    *bx = b[1];
    *by = b[2];
    return (b[3] - (b[0]*b[0]*m[6+0] + b[1]*b[1]*m[6+2] + b[2]*b[2]*m[6+5])
            - 2.0*(b[0]*b[1]*m[6+1] + b[0]*b[2]*m[6+3] + b[1]*b[2]*m[6+4]));
}

static void
nonlinearity_do(GwyDataField *dfield, GwyDataField *show)
{
    static const gdouble r = 2.5;
    gint xres, yres, i, j, size;
    gdouble qx, qy;
    gint *xp, *yp;
    gdouble *d, *z;

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    d = gwy_data_field_get_data(show);
    qx = gwy_data_field_get_xmeasure(dfield);
    qy = gwy_data_field_get_xmeasure(dfield);

    size = gwy_data_field_get_circular_area_size(r);
    z = g_new(gdouble, size);
    xp = g_new(gint, 2*size);
    yp = xp + size;
    for (i = 0; i < yres; i++) {
        for (j = 0; j < xres; j++) {
            gdouble bx, by, s0r;
            gint n;

            n = gwy_data_field_circular_area_extract_with_pos(dfield, j, i, r,
                                                              z, xp, yp);
            s0r = fit_local_plane_by_pos(n, xp, yp, z, &bx, &by);
            bx /= qx;
            by /= qy;
            d[i*xres + j] = sqrt(MAX(s0r, 0.0)/(1.0 + bx*bx + by*by));
        }
    }
    g_free(xp);
    g_free(z);
}

static void
inclination_do(GwyDataField *dfield, GwyDataField *show)
{
    static const gdouble r = 2.5;
    gint xres, yres, i, j, size;
    gdouble qx, qy;
    gint *xp, *yp;
    gdouble *d, *z;

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    d = gwy_data_field_get_data(show);
    qx = gwy_data_field_get_xmeasure(dfield);
    qy = gwy_data_field_get_xmeasure(dfield);

    size = gwy_data_field_get_circular_area_size(r);
    z = g_new(gdouble, size);
    xp = g_new(gint, 2*size);
    yp = xp + size;
    for (i = 0; i < yres; i++) {
        for (j = 0; j < xres; j++) {
            gdouble bx, by;
            gint n;

            n = gwy_data_field_circular_area_extract_with_pos(dfield, j, i, r,
                                                              z, xp, yp);
            fit_local_plane_by_pos(n, xp, yp, z, &bx, &by);
            bx /= qx;
            by /= qy;
            d[i*xres + j] = atan(hypot(bx, by));
        }
    }
    g_free(xp);
    g_free(z);
}

static void
step_do(GwyDataField *dfield, GwyDataField *show)
{
    static const gdouble r = 2.5;
    gint xres, yres, i, j, size;
    gdouble *d, *z;

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    d = gwy_data_field_get_data(show);

    size = gwy_data_field_get_circular_area_size(r);
    z = g_new(gdouble, size);
    for (i = 0; i < yres; i++) {
        for (j = 0; j < xres; j++) {
            gint n;

            n = gwy_data_field_circular_area_extract(dfield, j, i, r, z);
            gwy_math_sort(n, z);
            d[i*xres + j] = sqrt(z[n-1 - n/3] - z[n/3]);
        }
    }
    g_free(z);
}

static void
hough_lines_do(GwyDataField *dfield, GwyDataField *show)
{
    GwyDataField *x_gradient, *y_gradient;

    gwy_data_field_copy(dfield, show, FALSE);
    gwy_data_field_filter_canny(show, 0.1);

    x_gradient = gwy_data_field_duplicate(dfield);
    gwy_data_field_filter_sobel(x_gradient, GWY_ORIENTATION_HORIZONTAL);
    y_gradient = gwy_data_field_duplicate(dfield);
    gwy_data_field_filter_sobel(y_gradient, GWY_ORIENTATION_VERTICAL);

    gwy_data_field_hough_line_strenghten(show, x_gradient, y_gradient,
                                         1, 0.2);
}
static void
harris_do(GwyDataField *dfield, GwyDataField *show)
{
    GwyDataField *x_gradient, *y_gradient;

    gwy_data_field_copy(dfield, show, FALSE);
    x_gradient = gwy_data_field_duplicate(dfield);
    gwy_data_field_filter_sobel(x_gradient, GWY_ORIENTATION_HORIZONTAL);
    y_gradient = gwy_data_field_duplicate(dfield);
    gwy_data_field_filter_sobel(y_gradient, GWY_ORIENTATION_VERTICAL);

    gwy_data_field_filter_harris(x_gradient, y_gradient, show, 20, 0.1);
    /*gwy_data_field_clear(show);
    gwy_data_field_grains_mark_watershed_minima(ble, show, 1, 0, 5,
                       0.05*(gwy_data_field_get_max(ble) - gwy_data_field_get_min(ble)));
*/
}

static void
sobel_do(GwyDataField *dfield, GwyDataField *show)
{
    gwy_data_field_copy(dfield, show, FALSE);
    gwy_data_field_filter_sobel_total(show);
}

static void
prewitt_do(GwyDataField *dfield, GwyDataField *show)
{
    gwy_data_field_copy(dfield, show, FALSE);
    gwy_data_field_filter_prewitt_total(show);
}

/*
static void
local_maxima_do(GwyDataField *dfield, GwyDataField *show)
{
    static const gdouble r = 3.5;
    gint xres, yres, i, j, size, rr;
    gint *xp, *yp;
    gdouble *d, *z;

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    d = gwy_data_field_get_data(show);
    rr = (gint)ceil(r);

    size = gwy_data_field_get_circular_area_size(r);
    z = g_new(gdouble, size);
    xp = g_new(gint, 2*size);
    yp = xp + size;
    for (i = 0; i < yres; i++) {
        for (j = 0; j < xres; j++) {
            gdouble s1, s2, s3, x;
            gint k, n;

            if (j < rr || j + rr > xres || i < rr || i + rr > yres) {
                d[i*xres + j] = 0.0;
                continue;
            }

            n = gwy_data_field_circular_area_extract_with_pos(dfield, j, i, r,
                                                              z, xp, yp);
            s1 = s2 = s3 = 0.0;
            for (k = 0; k < n; k++) {
                x = 2.0*xp[k];
                s1 += z[k]*(1.0 - x*x);
                x = xp[k] + GWY_SQRT3*yp[k];
                s2 += z[k]*(1.0 - x*x);
                x = xp[k] - GWY_SQRT3*yp[k];
                s3 += z[k]*(1.0 - x*x);
            }
            x = s1 + s2 + s3;
            x = (s1*s1 + s2*s2 + s3*s3) - x*x/3.0;
            d[i*xres + j] = sqrt(x);
        }
    }
    g_free(xp);
    g_free(z);
}
*/

static void
zero_crossing(GwyContainer *data, GwyRunType run)
{
    ZeroCrossingArgs args;
    GwyDataField *dfield;
    GQuark squark;
    gint id;

    g_return_if_fail(run & EDGE_UI_RUN_MODES);
    zero_crossing_load_args(gwy_app_settings_get(), &args);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     GWY_APP_SHOW_FIELD_KEY, &squark,
                                     0);
    g_return_if_fail(dfield && squark);

    if (run == GWY_RUN_IMMEDIATE)
        zero_crossing_run(&args, data, dfield, squark);
    else {
        zero_crossing_dialog(&args, data, dfield, id, squark);
        zero_crossing_save_args(gwy_app_settings_get(), &args);
    }
}

static void
zero_crossing_dialog(ZeroCrossingArgs *args,
                     GwyContainer *data,
                     GwyDataField *dfield,
                     gint id,
                     GQuark squark)
{
    GtkWidget *dialog, *table, *hbox, *label;
    GtkObject *adj;
    ZeroCrossingControls controls;
    gint response;
    gdouble zoomval;
    GwyDataField *sfield;
    gint row;
    gboolean temp;

    memset(&controls, 0, sizeof(ZeroCrossingControls));
    controls.args = args;
    controls.in_init = TRUE;

    dialog = gtk_dialog_new_with_buttons(_("Zero Crossing Step Detection"),
                                         NULL, 0, NULL);
    gtk_dialog_add_action_widget(GTK_DIALOG(dialog),
                                 gwy_stock_like_button_new(_("_Update"),
                                                           GTK_STOCK_EXECUTE),
                                 RESPONSE_PREVIEW);
    gtk_dialog_add_button(GTK_DIALOG(dialog), _("_Reset"), RESPONSE_RESET);
    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_OK, GTK_RESPONSE_OK);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    controls.dialog = dialog;

    hbox = gtk_hbox_new(FALSE, 2);

    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 4);

    controls.mydata = gwy_container_new();
    gwy_container_set_object_by_name(controls.mydata, "/0/data", dfield);
    gwy_app_sync_data_items(data, controls.mydata, id, 0, FALSE,
                            GWY_DATA_ITEM_PALETTE,
                            GWY_DATA_ITEM_MASK_COLOR,
                            GWY_DATA_ITEM_RANGE,
                            GWY_DATA_ITEM_REAL_SQUARE,
                            0);
    controls.view = gwy_data_view_new(controls.mydata);
    controls.layer = gwy_layer_basic_new();
    g_object_set(controls.layer,
                 "data-key", "/0/data",
                 "gradient-key", "/0/base/palette",
                 NULL);
    gwy_data_view_set_data_prefix(GWY_DATA_VIEW(controls.view), "/0/data");
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.view), controls.layer);
    zoomval = PREVIEW_SIZE/(gdouble)MAX(gwy_data_field_get_xres(dfield),
                                        gwy_data_field_get_yres(dfield));
    gwy_data_view_set_zoom(GWY_DATA_VIEW(controls.view), zoomval);

    gtk_box_pack_start(GTK_BOX(hbox), controls.view, FALSE, FALSE, 4);

    table = gtk_table_new(7, 4, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(hbox), table, TRUE, TRUE, 4);
    row = 0;

    adj = gtk_adjustment_new(args->gaussian_fwhm, 0.0, 20.0, 0.1, 1.0, 0);
    controls.gaussian_fwhm = adj;
    gwy_table_attach_hscale(table, row, _("_Gaussian FWHM:"), "px", adj,
                            GWY_HSCALE_DEFAULT);
    g_signal_connect(adj, "value-changed",
                     G_CALLBACK(zero_crossing_gaussian_fwhm_changed),
                     &controls);
    row++;

    adj = gtk_adjustment_new(args->threshold, 0.0, 3.0, 0.01, 0.1, 0);
    controls.threshold = adj;
    gwy_table_attach_hscale(table, row, _("_Threshold:"), _("NRMS"), adj,
                            GWY_HSCALE_DEFAULT);
    g_signal_connect(adj, "value-changed",
                     G_CALLBACK(zero_crossing_threshold_changed), &controls);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    label = gtk_label_new(gwy_sgettext("verb|Display"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    controls.display_group
        = gwy_radio_buttons_createl(G_CALLBACK(zero_crossing_display_changed),
                                    &controls, DISPLAY_DATA,
                                    _("Original _image"), DISPLAY_DATA,
                                    _("_LoG convolved"), DISPLAY_LOG,
                                    _("Detected st_ep"), DISPLAY_SHOW,
                                    NULL);
    row = gwy_radio_buttons_attach_to_table(controls.display_group,
                                            GTK_TABLE(table), 3, row);
    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);

    controls.update = gtk_check_button_new_with_mnemonic(_("I_nstant updates"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.update),
                                 args->update);
    gtk_table_attach(GTK_TABLE(table), controls.update,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_signal_connect(controls.update, "toggled",
                     G_CALLBACK(zero_crossing_update_changed), &controls);

    /* finished initializing, allow instant updates */
    controls.in_init = FALSE;

    /* show initial preview if instant updates are on */
    if (args->update)
        gtk_dialog_set_response_sensitive(GTK_DIALOG(controls.dialog),
                                          RESPONSE_PREVIEW, FALSE);

    gtk_widget_show_all(dialog);
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
            temp = args->update;
            *args = zero_crossing_defaults;
            args->update = temp;
            controls.in_init = TRUE;
            zero_crossing_update_controls(&controls, args);
            controls.in_init = FALSE;
            zero_crossing_preview(&controls, args);
            break;

            case RESPONSE_PREVIEW:
            zero_crossing_preview(&controls, args);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    gtk_widget_destroy(dialog);

    if (controls.computed) {
        sfield = gwy_container_get_object_by_name(controls.mydata, "/0/show");
        gwy_app_undo_qcheckpointv(data, 1, &squark);
        gwy_container_set_object(data, squark, sfield);
        g_object_unref(controls.mydata);
    }
    else {
        g_object_unref(controls.mydata);
        zero_crossing_run(args, data, dfield, squark);
    }
}

static void
zero_crossing_gaussian_fwhm_changed(GtkAdjustment *adj,
                                    ZeroCrossingControls *controls)
{
    controls->args->gaussian_fwhm = gtk_adjustment_get_value(adj);
    controls->gauss_computed = FALSE;
    zero_crossing_invalidate(controls);
}

static void
zero_crossing_threshold_changed(GtkAdjustment *adj,
                                ZeroCrossingControls *controls)
{
    controls->args->threshold = gtk_adjustment_get_value(adj);
    zero_crossing_invalidate(controls);
}

static void
zero_crossing_display_changed(GtkToggleButton *radio,
                              ZeroCrossingControls *controls)
{
    if (!gtk_toggle_button_get_active(radio))
        return;

    controls->display = gwy_radio_buttons_get_current(controls->display_group);
    zero_crossing_preview(controls, controls->args);
    switch (controls->display) {
        case DISPLAY_DATA:
        gwy_pixmap_layer_set_data_key(controls->layer, "/0/data");
        break;

        case DISPLAY_LOG:
        gwy_pixmap_layer_set_data_key(controls->layer, "/0/gauss");
        break;

        case DISPLAY_SHOW:
        gwy_pixmap_layer_set_data_key(controls->layer, "/0/show");
        break;

        default:
        g_return_if_reached();
        break;
    }
}

static void
zero_crossing_update_changed(GtkToggleButton *check,
                             ZeroCrossingControls *controls)
{
    controls->args->update = gtk_toggle_button_get_active(check);
    gtk_dialog_set_response_sensitive(GTK_DIALOG(controls->dialog),
                                      RESPONSE_PREVIEW,
                                      !controls->args->update);
    if (controls->args->update)
        zero_crossing_preview(controls, controls->args);
}

static void
zero_crossing_invalidate(ZeroCrossingControls *controls)
{
    controls->computed = FALSE;
    if (controls->args->update && !controls->in_init)
        zero_crossing_preview(controls, controls->args);
}

static void
zero_crossing_update_controls(ZeroCrossingControls *controls,
                              ZeroCrossingArgs *args)
{
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->gaussian_fwhm),
                             args->gaussian_fwhm);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->threshold),
                             args->threshold);
}

static GwyDataField*
create_show_field(GwyDataField *dfield)
{
    GwyDataField *mfield;
    GwySIUnit *siunit;

    mfield = gwy_data_field_new_alike(dfield, FALSE);
    siunit = gwy_si_unit_new(NULL);
    gwy_data_field_set_si_unit_z(mfield, siunit);
    g_object_unref(siunit);

    return mfield;
}

static void
zero_crossing_preview(ZeroCrossingControls *controls,
                      ZeroCrossingArgs *args)
{
    GwyDataField *dfield, *show, *gauss;
    gdouble nrms;

    if (controls->computed)
        return;

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                             "/0/data"));

    /* Set up the show */
    if (!gwy_container_gis_object_by_name(controls->mydata, "/0/show", &show)) {
        show = create_show_field(dfield);
        gwy_container_set_object_by_name(controls->mydata, "/0/show", show);
        g_object_unref(show);

        gauss = gwy_data_field_new_alike(show, FALSE);
        gwy_container_set_object_by_name(controls->mydata, "/0/gauss", gauss);
        g_object_unref(gauss);
    }
    else
        gwy_container_gis_object_by_name(controls->mydata, "/0/gauss", &gauss);

    if (controls->gauss_computed)
        nrms = gwy_container_get_double_by_name(controls->mydata, "/0/nrms");
    else {
        nrms = zero_crossing_do_log(dfield, gauss, args->gaussian_fwhm);
        gwy_container_set_double_by_name(controls->mydata, "/0/nrms", nrms);
        gwy_data_field_data_changed(gauss);
        controls->gauss_computed = TRUE;
    }
    zero_crossing_do_edge(show, gauss, nrms*args->threshold);
    gwy_data_field_data_changed(show);
    controls->computed = TRUE;
}

static void
zero_crossing_run(const ZeroCrossingArgs *args,
                  GwyContainer *data,
                  GwyDataField *dfield,
                  GQuark squark)
{
    GwyDataField *show, *gauss;
    gdouble nrms;

    gwy_app_undo_qcheckpointv(data, 1, &squark);
    show = create_show_field(dfield);
    gauss = gwy_data_field_new_alike(show, FALSE);
    nrms = zero_crossing_do_log(dfield, gauss, args->gaussian_fwhm);
    zero_crossing_do_edge(show, gauss, nrms*args->threshold);
    g_object_unref(gauss);
    gwy_container_set_object(data, squark, show);
    g_object_unref(show);
}

static gdouble
zero_crossing_do_log(GwyDataField *dfield,
                     GwyDataField *gauss,
                     gdouble gaussian_fwhm)
{
    const gdouble *data, *row;
    gint xres, yres, i, j;
    gdouble nrms;

    gwy_data_field_copy(dfield, gauss, FALSE);
    gwy_data_field_filter_gaussian(gauss, gaussian_fwhm/(2.0*sqrt(2.0*G_LN2)));
    gwy_data_field_filter_laplacian(gauss);

    xres = gwy_data_field_get_xres(gauss);
    yres = gwy_data_field_get_yres(gauss);
    data = gwy_data_field_get_data_const(gauss);
    nrms = 0.0;

    for (i = 0; i < yres-1; i++) {
        row = data + i*xres;
        for (j = 0; j < xres; j++)
            nrms += (row[j] - row[j + xres])*(row[j] - row[j + xres]);
    }
    for (i = 0; i < yres; i++) {
        row = data + i*xres;
        for (j = 0; j < xres-1; j++)
            nrms += (row[j] - row[j + 1])*(row[j] - row[j + 1]);
    }

    nrms /= 2*xres*yres - xres - yres;
    return sqrt(nrms);
}

static void
zero_crossing_do_edge(GwyDataField *show,
                      GwyDataField *gauss,
                      gdouble threshold)
{
    gdouble *data;
    const gdouble *bdata;
    gdouble dm, dp;
    gint n, xres, yres, i, j;

    gwy_data_field_clear(show);

    xres = gwy_data_field_get_xres(show);
    yres = gwy_data_field_get_yres(show);
    data = gwy_data_field_get_data(show);
    bdata = gwy_data_field_get_data_const(gauss);

    /* Vertical pass */
    for (i = 1; i < yres; i++) {
        for (j = 0; j < xres; j++) {
            n = i*xres + j;
            dm = bdata[n - xres];
            dp = bdata[n];
            if (dm*dp <= 0.0) {
                dm = fabs(dm);
                dp = fabs(dp);
                if (dm >= threshold || dp >= threshold) {
                    if (dm < dp)
                        data[n - xres] = 1.0;
                    else if (dp < dm)
                        data[n] = 1.0;
                    /* If they are equal and different from zero, sigh and
                     * choose an arbitrary one */
                    else if (dm > 0.0)
                        data[n] = 1.0;
                }
            }
        }
    }

    /* Horizontal pass */
    for (i = 0; i < yres; i++) {
        for (j = 1; j < xres; j++) {
            n = i*xres + j;
            dm = bdata[n - 1];
            dp = bdata[n];
            if (dm*dp <= 0.0) {
                dm = fabs(dm);
                dp = fabs(dp);
                if (dm >= threshold || dp >= threshold) {
                    if (dm < dp)
                        data[n - 1] = 1.0;
                    else if (dp < dm)
                        data[n] = 1.0;
                    /* If they are equal and different from zero, sigh and
                     * choose an arbitrary one */
                    else if (dm > 0.0)
                        data[n] = 1.0;
                }
            }
        }
    }
}

static const gchar gaussian_fwhm_key[] = "/module/zero_crossing/gaussian-fwhm";
static const gchar threshold_key[]     = "/module/zero_crossing/threshold";
static const gchar update_key[]        = "/module/zero_crossing/update";

static void
zero_crossing_sanitize_args(ZeroCrossingArgs *args)
{
    args->gaussian_fwhm = CLAMP(args->gaussian_fwhm, 0.0, 20.0);
    args->threshold = CLAMP(args->threshold, 0.0, 3.0);
    args->update = !!args->update;
}

static void
zero_crossing_load_args(GwyContainer *container,
                        ZeroCrossingArgs *args)
{
    *args = zero_crossing_defaults;

    gwy_container_gis_double_by_name(container, gaussian_fwhm_key,
                                     &args->gaussian_fwhm);
    gwy_container_gis_double_by_name(container, threshold_key,
                                     &args->threshold);
    gwy_container_gis_boolean_by_name(container, update_key,
                                      &args->update);

    zero_crossing_sanitize_args(args);
}

static void
zero_crossing_save_args(GwyContainer *container,
                        ZeroCrossingArgs *args)
{
    gwy_container_set_double_by_name(container, gaussian_fwhm_key,
                                     args->gaussian_fwhm);
    gwy_container_set_double_by_name(container, threshold_key,
                                     args->threshold);
    gwy_container_set_boolean_by_name(container, update_key,
                                      args->update);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
