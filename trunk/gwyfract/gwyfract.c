/*
 *  @(#) $Id$
 *  Copyright (C) 2007 David Necas (Yeti).
 *  E-mail: yeti@gwyddion.net.
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
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <locale.h>
#include <gmp.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwyddion.h>
#include <libprocess/gwyprocess.h>
#include <libgwydgets/gwydgets.h>
#include <app/gwyapp.h>

/* All good names such as INFINITY or OVERFLOW are taken. */
#define BIG_UGLY_NUMBER 1e38
#define CPUINFO_FILE "/proc/cpuinfo"

/* Hopefully it's either defined or we don't need it */
#ifndef HW_NCPU
#define HW_NCPU 3
#endif

#ifndef HAVE_LOG_2
#define log2(x) (log(x)/G_LN2)
#endif

enum {
    TASK_BLOCK_SIZE     = 256,
    PROGRESSBAR_TIMEOUT = 250
};

typedef enum {
    STATE_READY, STATE_CALCULATING, STATE_ABORTED
} StateType;

typedef enum {
    MAP_LINEAR, MAP_SQRT, MAP_S_L, MAP_LOG, MAP_ADAPT
} MapType;

typedef struct {
    long double xo;
    long double yo;
    long double xs;
    long double ys;
} DoubleRegion;

typedef struct {
    mpf_t xo;
    mpf_t yo;
    long double xs;
    long double ys;
} MPRegion;

typedef struct {
    gint id;
    gint ntasks;
    GwyDataField *iters;
    GwyDataField *eiters;
    gboolean julia;
    gboolean enhance;
    gboolean smooth;
    guint mpf_prec;
    MPRegion *region;
    gdouble iter_min;
    gdouble iter_max;
    gint progress;
    gboolean done;
} Task;

typedef struct {
    guint ncpus;
    guint bits;
    GThreadPool *workers;
    Task *tasks;
    StateType state;
    gulong progress_id;

    GwyContainer *container;
    GwyDataField *iters;
    GwyDataField *eiters;
    gboolean enhanced;
    gdouble iter_min;
    gdouble iter_max;
    GSList *zoom_stack;
    gint new_w;
    gint new_h;
    gulong resize_id;

    gboolean julia;
    gboolean enhance;
    gboolean smooth;
    gboolean invert;
    gboolean from_0;
    MapType map_type;

    GtkWidget *toolbox;
    GtkWidget *fwindow;
    GtkWidget *status;
    GtkWidget *pbar;
    GtkWidget *benhance;
    GtkWidget *bsave;
    GtkWidget *brestore;
    GtkWidget *filechooser;
} Application;

static const DoubleRegion defmxy = { -0.7, 0.0, 3.0, 2.5 };
static const DoubleRegion defjxy = {  0.0, 0.0, 3.6, 3.0 };

static const gdouble r2 = 1e12;
static const gdouble fuzz = 3.0;

/* from ncpus.c */
int dcc_ncpus(int *ncpus);

static gint
get_n_cpus(void)
{
    gint ncpus;

    /* ignore errors, 1 is good enough for us */
    dcc_ncpus(&ncpus);

    if (ncpus > 64) {
        g_printerr("More than 64 processors (%d), we don't think we are that "
                   "scalable.\n", ncpus);
        ncpus = 64;
    }

    return ncpus;
}

static gint
get_long_double_bits(void)
{
    long double x, y;
    gint bits;

    for (bits = 0, x = y = 1.0; x + y != x; bits++)
        y /= 2;

    return bits;
}

static void
double_to_mpf(mpf_t x, long double d)
{
    mpf_t t;

    mpf_set_d(x, d);
    d -= mpf_get_d(x);
    mpf_init_set_d(t, d);
    mpf_add(x, x, t);
    mpf_clear(t);
}

static long double
double_from_mpf(mpf_t x)
{
    long double d;
    mpf_t t;

    d = mpf_get_d(x);
    mpf_init_set_d(t, d);
    mpf_sub(t, x, t);
    d += mpf_get_d(t);
    mpf_clear(t);

    return d;
}

static void
region_set_from_double(MPRegion *mr, const DoubleRegion *dr)
{

    double_to_mpf(mr->xo, dr->xo);
    double_to_mpf(mr->yo, dr->yo);
    mr->xs = dr->xs;
    mr->ys = dr->ys;
}

static void
region_init_set_from_double(MPRegion *mr, const DoubleRegion *dr)
{

    mpf_init(mr->xo);
    mpf_init(mr->yo);
    region_set_from_double(mr, dr);
}

static void
region_to_double(MPRegion *mr, DoubleRegion *dr)
{

    dr->xo = double_from_mpf(mr->xo);
    dr->yo = double_from_mpf(mr->yo);
    dr->xs = mr->xs;
    dr->ys = mr->ys;
}

static guint
region_get_precision(MPRegion *mr, gint w, gint h)
{
    return (guint)ceil(log2(MAX(w/mr->xs, h/mr->ys))) + 1;
}

static void
region_clear(MPRegion *mr)
{
    mpf_clear(mr->xo);
    mpf_clear(mr->yo);
}

static gboolean
waste_of_time(gboolean new_value, gboolean set)
{
    static gboolean wst = FALSE;
    static GStaticMutex mutex = G_STATIC_MUTEX_INIT;

    gboolean ret_val;

    g_static_mutex_lock(&mutex);
    ret_val = wst;
    if (set)
        wst = new_value;
    g_static_mutex_unlock(&mutex);

    return ret_val;
}

static inline gdouble
iterate_double(long double cx, long double cy,
               long double jx, long double jy,
               int maxiter,
               gboolean julia, gboolean smooth)
{
    long double x, y, x2, y2;
    int i;

    x = cx;
    y = cy;
    x2 = x*x;
    y2 = y*y;
    if (julia) {
        cx = jx;
        cy = jy;
    }

    for (i = 0; i < maxiter && x2 + y2 < r2; i++) {
        y = 2*x*y + cy;
        x = x2 - y2 + cx;
        x2 = x*x;
        y2 = y*y;
    }

    if (i == maxiter)
        return BIG_UGLY_NUMBER;

    return smooth ? i+1 - log(log(x2 + y2)/2.0)/G_LN2 : i;
}

static void
worker_calculate_double(Task *task)
{
    MPRegion *mpregion;
    DoubleRegion region;
    long double jx, jy, cx, cy;
    gdouble v, *field;
    gint bs, tbs, bno, w, h, k, maxiter;
    gboolean julia, smooth;

    bs = task->enhance ? TASK_BLOCK_SIZE : 8*TASK_BLOCK_SIZE;
    tbs = bs*task->ntasks;
    julia = task->julia;
    smooth = task->smooth;
    mpregion = task->region;
    region_to_double(mpregion, &region);
    w = gwy_data_field_get_xres(task->eiters);
    h = gwy_data_field_get_yres(task->eiters);
    jx = region.xo;
    jy = region.yo;
    if (julia)
        region = defjxy;

    if (task->enhance) {
        gdouble buf[13], d[8];
        gdouble *ef;
        gint bi;

        maxiter = task->iter_max + (smooth ? log(log(r2)/2.0)/G_LN2 : 1);
        field = gwy_data_field_get_data(task->iters);
        ef = gwy_data_field_get_data(task->eiters);

        for (bno = 0; bno < (w*h + tbs-1)/tbs; bno++) {
            gint from, to;

            from = bno*tbs + task->id*bs;
            to = MIN(from + bs, w*h);
            for (k = from; k < to; k++) {
                gdouble s, p, q;
                gint bk;

                cy = region.yo + ((k/w + 0.5)/h - 0.5)*region.ys;
                cx = region.xo + ((k % w + 0.5)/w - 0.5)*region.xs;
                d[1] = (k/w > 0) ? fabs(field[k] - field[k-w]) : 0.0;
                d[3] = (k % w > 0) ? fabs(field[k] - field[k-1]) : 0.0;
                d[4] = (k % w < w-1) ? fabs(field[k] - field[k+1]) : 0.0;
                d[6] = (k/w < h-1) ? fabs(field[k] - field[k+w]) : 0.0;
                bi = 0;
                buf[bi++] = field[k];
                if (d[1] > 2*fuzz)
                    buf[bi++] = iterate_double(cx, cy - region.ys/(8.0*h),
                                               jx, jy, maxiter, julia, smooth);
                if (d[1] > fuzz)
                    buf[bi++] = iterate_double(cx, cy - region.ys/(4.0*h),
                                               jx, jy, maxiter, julia, smooth);
                if (d[6] > 2*fuzz)
                    buf[bi++] = iterate_double(cx, cy + region.ys/(8.0*h),
                                               jx, jy, maxiter, julia, smooth);
                if (d[6] > fuzz)
                    buf[bi++] = iterate_double(cx, cy + region.ys/(4.0*h),
                                               jx, jy, maxiter, julia, smooth);
                if (d[3] > 2*fuzz)
                    buf[bi++] = iterate_double(cx - region.xs/(8.0*w), cy,
                                               jx, jy, maxiter, julia, smooth);
                if (d[3] > fuzz)
                    buf[bi++] = iterate_double(cx - region.xs/(4.0*w), cy,
                                               jx, jy, maxiter, julia, smooth);
                if (d[4] > 2*fuzz)
                    buf[bi++] = iterate_double(cx + region.xs/(8.0*w), cy,
                                               jx, jy, maxiter, julia, smooth);
                if (d[4] > fuzz)
                    buf[bi++] = iterate_double(cx + region.xs/(4.0*w), cy,
                                               jx, jy, maxiter, julia, smooth);
                if (d[1] + d[3] > 2*fuzz)
                    buf[bi++] = iterate_double(cx - region.xs/(6.0*w),
                                               cy - region.ys/(6.0*h),
                                               jx, jy, maxiter, julia, smooth);
                if (d[1] + d[4] > 2*fuzz)
                    buf[bi++] = iterate_double(cx + region.xs/(6.0*w),
                                               cy - region.ys/(6.0*h),
                                               jx, jy, maxiter, julia, smooth);
                if (d[6] + d[3] > 2*fuzz)
                    buf[bi++] = iterate_double(cx - region.xs/(6.0*w),
                                               cy + region.ys/(6.0*h),
                                               jx, jy, maxiter, julia, smooth);
                if (d[6] + d[4] > 2*fuzz)
                    buf[bi++] = iterate_double(cx + region.xs/(6.0*w),
                                               cy + region.ys/(6.0*h),
                                               jx, jy, maxiter, julia, smooth);

                gwy_math_sort(bi, buf);
                s = p = 0.0;
                q = 1.0;
                for (bk = 0; bk < bi; bk++) {
                    s += q*MIN(buf[bi-1 - bk], task->iter_max);
                    p += q;
                    q /= 1.2;
                }
                ef[k] = s/p;
                task->progress++;
            }
            if (waste_of_time(FALSE, FALSE))
                break;
        }
    }
    else {
        field = gwy_data_field_get_data(task->eiters);
        maxiter = GWY_ROUND(-197*(log(region.xs*region.ys) - 3.0));
        task->iter_min = G_MAXDOUBLE;
        task->iter_max = -G_MAXDOUBLE;
        for (bno = 0; bno < (w*h + tbs-1)/tbs; bno++) {
            gint from, to;

            from = bno*tbs + task->id*bs;
            to = MIN(from + bs, w*h);
            for (k = from; k < to; k++) {
                cy = region.yo + ((k/w + 0.5)/h - 0.5)*region.ys;
                cx = region.xo + ((k % w + 0.5)/w - 0.5)*region.xs;
                v = iterate_double(cx, cy, jx, jy, maxiter, julia, smooth);
                field[k] = v;
                if (v < BIG_UGLY_NUMBER) {
                    task->iter_max = MAX(task->iter_max, v);
                    task->iter_min = MIN(task->iter_min, v);
                }
                task->progress++;
            }
            if (waste_of_time(FALSE, FALSE))
                break;
        }
        /* Completely inside the set??? */
        if (task->iter_min > task->iter_max)
            task->iter_min = task->iter_max = 1.0;
    }

    task->done = TRUE;
}

static inline gdouble
iterate_mpf(mpf_t cx, mpf_t cy,
            mpf_t x, mpf_t y, mpf_t x2, mpf_t y2, mpf_t rr,
            int maxiter,
            gboolean smooth)
{
    int i;

    mpf_set(x, cx);
    mpf_set(y, cy);
    mpf_mul(x2, x, x);
    mpf_mul(y2, y, y);
    mpf_add(rr, x2, y2);

    for (i = 0; i < maxiter && mpf_cmp_d(rr, r2) < 0; i++) {
        mpf_mul(y, y, x);
        mpf_mul_2exp(y, y, 1);
        mpf_add(y, y, cy);
        mpf_sub(x, x2, y2);
        mpf_add(x, x, cx);
        mpf_mul(x2, x, x);
        mpf_mul(y2, y, y);
        mpf_add(rr, x2, y2);
    }

    if (i == maxiter)
        return BIG_UGLY_NUMBER;

    return smooth ? i+1 - log(log(mpf_get_d(rr))/2.0)/G_LN2 : i;
}

static void
worker_calculate_mpf(Task *task)
{
    MPRegion *region;
    mpf_t cx, cy, x, y, x2, y2, rr;
    gdouble v, *field;
    gint bs, tbs, bno, w, h, k, maxiter;
    gboolean smooth;

    /* XXX */
    task->enhance = FALSE;

    bs = task->enhance ? TASK_BLOCK_SIZE/8 : TASK_BLOCK_SIZE;
    tbs = bs*task->ntasks;
    smooth = task->smooth;
    region = task->region;
    w = gwy_data_field_get_xres(task->eiters);
    h = gwy_data_field_get_yres(task->eiters);
    field = gwy_data_field_get_data(task->eiters);

    mpf_init2(cx, task->mpf_prec);
    mpf_init2(cy, task->mpf_prec);
    mpf_init2(x, task->mpf_prec);
    mpf_init2(y, task->mpf_prec);
    mpf_init2(x2, task->mpf_prec);
    mpf_init2(y2, task->mpf_prec);
    mpf_init2(rr, task->mpf_prec);

    if (task->enhance) {
        /* TODO */
    }
    else {
        mpf_t xs, ys;

        mpf_init(xs);
        double_to_mpf(xs, region->xs);
        mpf_init(ys);
        double_to_mpf(ys, region->ys);

        maxiter = GWY_ROUND(-97*(log(region->xs*region->ys) - 3.0));
        task->iter_min = G_MAXDOUBLE;
        task->iter_max = -G_MAXDOUBLE;
        for (bno = 0; bno < (w*h + tbs-1)/tbs; bno++) {
            gint from, to;

            from = bno*tbs + task->id*bs;
            to = MIN(from + bs, w*h);
            for (k = from; k < to; k++) {
                mpf_set_d(cy, ((k/w + 0.5)/h - 0.5));
                mpf_mul(cy, cy, ys);
                mpf_add(cy, cy, region->yo);
                mpf_set_d(cx, ((k % w + 0.5)/w - 0.5));
                mpf_mul(cx, cx, xs);
                mpf_add(cx, cx, region->xo);
                v = iterate_mpf(cx, cy, x, y, x2, y2, rr, maxiter, smooth);
                field[k] = v;
                if (v < BIG_UGLY_NUMBER) {
                    task->iter_max = MAX(task->iter_max, v);
                    task->iter_min = MIN(task->iter_min, v);
                }
                task->progress++;
            }
            if (waste_of_time(FALSE, FALSE))
                break;
        }

        mpf_clear(xs);
        mpf_clear(ys);
    }

    mpf_clear(x);
    mpf_clear(y);
    mpf_clear(x2);
    mpf_clear(y2);
    mpf_clear(rr);
    mpf_clear(cx);
    mpf_clear(cy);

    task->done = TRUE;
}

static void
worker_calculate(Task *task)
{
    if (task->mpf_prec)
        worker_calculate_mpf(task);
    else
        worker_calculate_double(task);
}

static void
label_set_text_printf(GtkLabel *label,
                      const gchar *format,
                      ...)
{
    gchar *s;
    va_list ap;

    va_start(ap, format);
    s = g_strdup_vprintf(format, ap);
    va_end(ap);
    gtk_label_set_markup(label, s);
    g_free(s);
}

static gboolean
check_all_done(Application *app)
{
    guint i;

    for (i = 0; i < app->ncpus; i++) {
        if (!app->tasks[i].done)
            return FALSE;
    }

    return TRUE;
}

static void
stop_calculation(Application *app)
{
    if (app->state != STATE_CALCULATING)
        return;

    waste_of_time(TRUE, TRUE);
    while (!check_all_done(app))
        g_thread_yield();

    app->state = STATE_ABORTED;
}

static void
calculate(Application *app)
{
    guint i, precision;
    gint w, h;

    stop_calculation(app);

    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(app->pbar), 0.0);
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(app->pbar), "0.0");
    gtk_widget_hide(app->status);
    gtk_widget_show(app->pbar);
    gtk_widget_set_sensitive(app->benhance, FALSE);

    waste_of_time(FALSE, TRUE);
    app->state = STATE_CALCULATING;
    w = gwy_data_field_get_xres(app->iters);
    h = gwy_data_field_get_yres(app->iters);
    gwy_data_field_resample(app->eiters, w, h, GWY_INTERPOLATION_NONE);

    precision = region_get_precision((MPRegion*)app->zoom_stack->data, w, h);
    if (precision <= app->bits || app->julia)
        precision = 0;

    if (precision && app->enhance)
        g_printerr("FIXME: Enhance not implemented for Deep Zoom yet!\n");

    for (i = 0; i < app->ncpus; i++) {
        Task *task = app->tasks + i;

        task->julia = app->julia;
        task->enhance = app->enhance;
        task->smooth = app->smooth;
        task->region = (MPRegion*)app->zoom_stack->data;
        task->mpf_prec = precision;
        task->progress = 0;
        task->done = FALSE;
        g_thread_pool_push(app->workers, task, NULL);
    }
}

static void
remap(Application *app)
{
    GwyDataField *dfield;
    const gdouble *idata;
    gdouble *fdata;
    gint w, h, i, l;

    idata = gwy_data_field_get_data(app->iters);
    w = gwy_data_field_get_xres(app->iters);
    h = gwy_data_field_get_yres(app->iters);

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(app->container,
                                                             "/data"));
    gwy_data_field_resample(dfield, w, h, GWY_INTERPOLATION_NONE);
    fdata = gwy_data_field_get_data(dfield);

    l = app->from_0 ? 0 : app->iter_min;
    for (i = 0; i < w*h; i++) {
        fdata[i] = idata[i] - l;
        switch (app->map_type) {
            case MAP_LINEAR:
            case MAP_ADAPT:
            break;

            case MAP_SQRT:
            fdata[i] = sqrt(fdata[i]);
            break;

            case MAP_LOG:
            fdata[i] = log(fdata[i] + 1.0);
            break;

            case MAP_S_L:
            fdata[i] = sqrt(log(fdata[i] + 1.0)*sqrt(fdata[i]));
            break;
        }
    }

    if (app->invert)
        gwy_data_field_invert(dfield, FALSE, FALSE, TRUE);

    gwy_data_field_data_changed(dfield);
}

static void
finished(Application *app)
{
    g_assert(check_all_done(app));

    if (app->state != STATE_ABORTED) {
        MPRegion *region;
        Task *task;
        guint i;

        task = app->tasks;
        region = (MPRegion*)app->zoom_stack->data;
        if (task->mpf_prec)
            label_set_text_printf(GTK_LABEL(app->status),
                                "<span foreground='#c00000'>Deep Zoom</span>:"
                                " %.2g (%d bits)",
                                1.0/sqrt(region->xs*region->ys),
                                task->mpf_prec);
        else
            label_set_text_printf(GTK_LABEL(app->status), "Zoom: %.2g",
                                1.0/sqrt(region->xs*region->ys));

        app->iter_min = G_MAXDOUBLE;
        app->iter_max = -G_MAXDOUBLE;
        for (i = 0; i < app->ncpus; i++) {
            app->iter_min = MIN(app->iter_min, app->tasks[i].iter_min);
            app->iter_max = MAX(app->iter_max, app->tasks[i].iter_max);
        }

        gwy_data_field_copy(app->eiters, app->iters, FALSE);
        if (!(app->enhanced = task->enhance))
            gwy_data_field_clamp(app->iters,
                                 app->iter_min, app->iter_max+1);

    }
    app->state = STATE_READY;

    remap(app);
    gtk_widget_hide(app->pbar);
    gtk_widget_show(app->status);
    gtk_widget_set_sensitive(app->benhance, !app->enhanced);
    gtk_widget_hide(app->benhance);
    gtk_widget_show(app->benhance);
}

static void
zoomed(GwySelection *selection,
       Application *app)
{
    static gboolean in_update = FALSE;

    gdouble xy[4];
    gboolean just_stop;

    if (in_update)
        return;

    in_update = TRUE;
    just_stop = (app->state == STATE_CALCULATING);
    stop_calculation(app);
    if (gwy_selection_get_object(selection, 0, xy)
        && xy[0] != xy[2] && xy[1] != xy[3]) {
        MPRegion *region, *parent;
        DoubleRegion dr;
        guint precision, pp;
        mpf_t x;
        gdouble q;

        dr.xo = (xy[0] + xy[2])/2 - 0.5;
        dr.yo = (xy[1] + xy[3])/2 - 0.5;
        dr.xs = fabs(xy[2] - xy[0]);
        dr.ys = fabs(xy[3] - xy[1]);
        q = sqrt(dr.xs/dr.ys);
        dr.xs /= q;
        dr.ys *= q;

        parent = (MPRegion*)app->zoom_stack->data;
        precision = region_get_precision(parent, 1e4/dr.xs, 1e4/dr.ys);
        pp = MAX(mpf_get_prec(parent->yo), mpf_get_prec(parent->xo));
        precision = MAX(precision, pp);
        region = g_new(MPRegion, 1);

        mpf_init(x);

        mpf_set_d(x, dr.xo*parent->xs);
        mpf_init_set(region->xo, parent->xo);
        mpf_set_prec(region->xo, precision);
        mpf_add(region->xo, region->xo, x);

        mpf_set_d(x, dr.yo*parent->ys);
        mpf_init_set(region->yo, parent->yo);
        mpf_set_prec(region->yo, precision);
        mpf_add(region->yo, region->yo, x);

        region->xs = dr.xs*parent->xs;
        region->ys = dr.ys*parent->ys;

        mpf_clear(x);
        app->zoom_stack = g_slist_prepend(app->zoom_stack, region);
    }
    else if (app->zoom_stack->next) {
        region_clear((MPRegion*)app->zoom_stack->data);
        g_free(app->zoom_stack->data);
        app->zoom_stack = g_slist_delete_link(app->zoom_stack,
                                              app->zoom_stack);
    }
    else {
        MPRegion *region;
        gdouble q;

        region = (MPRegion*)app->zoom_stack->data;
        q = MIN(defmxy.xs/region->xs, defmxy.ys/region->ys);
        if (q < 1.000001)
            just_stop = TRUE;
        else {
            if (q < 4.0)
                region_set_from_double(region, &defmxy);
            else {
                region->xs *= 4.0;
                region->ys *= 4.0;
            }
        }
    }

    gwy_selection_clear(selection);
    if (just_stop)
        finished(app);
    else
        calculate(app);

    in_update = FALSE;
}

static void
update_title(Application *app)
{
    if (app->julia)
        gtk_window_set_title(GTK_WINDOW(app->fwindow), "Julia");
    else
        gtk_window_set_title(GTK_WINDOW(app->fwindow), "Mandelbrot");
}

static void
mode_changed(GtkWidget *radio,
             Application *app)
{
    if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radio)))
        return;
    app->julia = gwy_radio_button_get_value(radio);
    update_title(app);
    calculate(app);
}

static void
map_changed(GtkWidget *radio,
            Application *app)
{
    if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radio)))
        return;
    app->map_type = gwy_radio_button_get_value(radio);
    if (app->map_type == MAP_ADAPT)
        gwy_container_set_enum_by_name(app->container, "/range-type",
                                       GWY_LAYER_BASIC_RANGE_ADAPT);
    else
        gwy_container_set_enum_by_name(app->container, "/range-type",
                                       GWY_LAYER_BASIC_RANGE_FULL);
    remap(app);
}

static void
smooth_toggled(GtkToggleButton *check,
               Application *app)
{
    app->smooth = gtk_toggle_button_get_active(check);
    calculate(app);
}

static void
invert_toggled(GtkToggleButton *check,
               Application *app)
{
    app->invert = gtk_toggle_button_get_active(check);
    remap(app);
}

static void
from_0_toggled(GtkToggleButton *check,
               Application *app)
{
    app->from_0 = gtk_toggle_button_get_active(check);
    remap(app);
}

static void
enhance(Application *app)
{
    app->enhance = TRUE;
    calculate(app);
    app->enhance = FALSE;
}

static void
gradient_selected(GtkTreeSelection *selection,
                  Application *app)
{
    GwyResource *resource;
    GtkTreeModel *model;
    GtkTreeIter iter;

    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        guchar *name;

        gtk_tree_model_get(model, &iter, 0, &resource, -1);
        name = (guchar*)g_strdup(gwy_resource_get_name(resource));
        gwy_container_set_string_by_name(app->container, "/gradient", name);
    }
}

static gboolean
resize_calculate(Application *app)
{
    MPRegion *region;
    gdouble w, h, q;

    stop_calculation(app);
    w = gwy_data_field_get_xres(app->iters);
    h = gwy_data_field_get_yres(app->iters);
    q = sqrt((w*app->new_h)/(h*app->new_w));
    region = (MPRegion*)app->zoom_stack->data;
    region->xs /= q;
    region->ys *= q;
    gwy_data_field_resample(app->iters, app->new_w, app->new_h,
                            GWY_INTERPOLATION_NONE);
    calculate(app);

    return FALSE;
}

static void
resized(G_GNUC_UNUSED GtkWidget *window,
        GtkAllocation *allocation,
        Application *app)
{
    if (allocation->width == app->new_w && allocation->height == app->new_h)
        return;

    if (app->resize_id) {
        g_source_remove(app->resize_id);
        app->resize_id = 0;
    }
    waste_of_time(TRUE, TRUE);
    app->new_w = allocation->width;
    app->new_h = allocation->height;
    app->resize_id = g_idle_add_full(G_PRIORITY_LOW,
                                     (GSourceFunc)resize_calculate, app, NULL);
}

static void
shut_down(Application *app)
{
    if (app->progress_id) {
        g_source_remove(app->progress_id);
        app->progress_id = 0;
    }
    stop_calculation(app);
    gtk_main_quit();
}

static gboolean
check_progress(Application *app)
{
    guint i;
    gint s, n;

    if (!app->tasks || app->state != STATE_CALCULATING)
        return TRUE;

    n = gwy_data_field_get_xres(app->iters)*gwy_data_field_get_yres(app->iters);
    for (i = s = 0; i < app->ncpus; i++)
        s += app->tasks[i].progress;

    if (s == n)
        finished(app);
    else {
        gdouble p = (gdouble)s/n;
        gchar buf[16];

        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(app->pbar), p);
        g_snprintf(buf, sizeof(buf), "%.1f %%", 100.0*p);
        gtk_progress_bar_set_text(GTK_PROGRESS_BAR(app->pbar), buf);
    }

    return TRUE;
}

static GtkWidget*
app_fractal_display_new(Application *app)
{
    GdkGeometry geom = { 16, 16, 0, 0, 0, 0, 0, 0, 0.0, 0.0, 0 };
    GtkWidget *window, *view;
    GwyPixmapLayer *layer;
    GwyVectorLayer *vlayer;
    GwySelection *selection;
    GwyDataField *dfield;

    app->iters = gwy_data_field_new(600, 500, 1.0, 1.0, FALSE);
    app->eiters = gwy_data_field_new_alike(app->iters, FALSE);
    dfield = gwy_data_field_new_alike(app->iters, TRUE);
    gwy_container_set_object_by_name(app->container, "/data", dfield);
    g_object_unref(dfield);
    gwy_container_set_enum_by_name(app->container, "/range-type",
                                   GWY_LAYER_BASIC_RANGE_FULL);

    view = gwy_data_view_new(app->container);

    layer = g_object_new(GWY_TYPE_LAYER_BASIC,
                         "data-key", "/data",
                         "gradient-key", "/gradient",
                         "range-type-key", "/range-type",
                         NULL);
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(view), layer);

    selection = g_object_new(g_type_from_name("GwySelectionRectangle"),
                             "max-objects", 1,
                             NULL);
    gwy_container_set_object_by_name(app->container, "/selection/rectangle",
                                     selection);
    g_signal_connect(selection, "finished", G_CALLBACK(zoomed), app);
    g_object_unref(selection);

    vlayer = g_object_new(g_type_from_name("GwyLayerRectangle"),
                          "selection-key", "/selection/rectangle",
                          NULL);
    gwy_data_view_set_top_layer(GWY_DATA_VIEW(view), vlayer);

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Fractal");
    gtk_window_set_geometry_hints(GTK_WINDOW(window), NULL,
                                  &geom, GDK_HINT_MIN_SIZE);
    g_signal_connect(window, "size-allocate", G_CALLBACK(resized), app);

    gtk_container_add(GTK_CONTAINER(window), view);

    return window;
}

static void
ensure_filechooser(Application *app,
                   GtkFileChooserAction action)
{
    const gchar *title;
    gboolean save;

    save = (action == GTK_FILE_CHOOSER_ACTION_SAVE);
    title = save ? "Save Parameters" : "Restore Parameters";
    if (app->filechooser) {
        gtk_file_chooser_set_action(GTK_FILE_CHOOSER(app->filechooser), action);
        gtk_window_set_title(GTK_WINDOW(app->filechooser), title);
    }
    else {
        app->filechooser
            = gtk_file_chooser_dialog_new(title, GTK_WINDOW(app->toolbox),
                                          action,
                                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                          GTK_STOCK_OK, GTK_RESPONSE_OK,
                                          NULL);
        gtk_dialog_set_default_response(GTK_DIALOG(app->filechooser),
                                        GTK_RESPONSE_OK);
    }

    gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(app->filechooser),
                                         FALSE);
    gtk_widget_show_all(app->filechooser);
    gtk_window_present(GTK_WINDOW(app->filechooser));
}

static void
format_mpf(mpf_t x, const gchar *name, size_t digits, gchar *s, GString *str)
{
    mp_exp_t e;
    const gchar *sg = "";

    memset(s, 0, digits + 2);
    /* FIXME: free()ing the return value of mpf_get_str() segfaults for some
     * reason -- a wrong set of memory functions?  Supply own buffers. */
    mpf_get_str(s, &e, 10, digits, x);
    if (s[0] == '-') {
        sg = "-";
        s++;
    }
    g_string_append_printf(str, "%s %s0.%s@%d\n", name, sg, s, (gint)e);
}

static void
write_region(MPRegion *region, GString *str)
{
    size_t digits;
    gchar *s;

    digits = (guint)(mpf_get_prec(region->xo)*G_LN2/G_LN10 + 2);
    s = g_new(gchar, MAX(digits + 2, G_ASCII_DTOSTR_BUF_SIZE));
    format_mpf(region->xo, "xo", digits, s, str);
    format_mpf(region->yo, "yo", digits, s, str);
    g_ascii_dtostr(s, G_ASCII_DTOSTR_BUF_SIZE, region->xs);
    g_string_append_printf(str, "xs %s\n", s);
    g_ascii_dtostr(s, G_ASCII_DTOSTR_BUF_SIZE, region->ys);
    g_string_append_printf(str, "ys %s\n", s);
    g_free(s);
}

static void
save(Application *app)
{
    ensure_filechooser(app, GTK_FILE_CHOOSER_ACTION_SAVE);
    if (gtk_dialog_run(GTK_DIALOG(app->filechooser)) == GTK_RESPONSE_OK) {
        gchar *fnm;

        fnm = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(app->filechooser));
        if (fnm) {
            GString *str;
            GError *err = NULL;

            str = g_string_new(NULL);
            write_region((MPRegion*)app->zoom_stack->data, str);
            if (!g_file_set_contents(fnm, str->str, str->len, &err)) {
                g_printerr("Cannot write %s: %s\n", fnm, err->message);
                g_clear_error(&err);
            }
            g_string_free(str, TRUE);
            g_free(fnm);
        }
    }
    gtk_widget_hide(app->filechooser);
}

static gboolean
read_region(MPRegion *region, gchar *p)
{
    enum { XO, YO, XS, YS };
    static const gchar *names[] = { "xo", "yo", "xs", "ys" };
    gboolean seen[G_N_ELEMENTS(names)];
    struct lconv *lcnv;
    gchar *q, *line;
    guint i;

    lcnv = localeconv();
    memset(seen, 0, G_N_ELEMENTS(names)*sizeof(gboolean));
    while ((line = gwy_str_next_line(&p))) {
        g_strstrip(line);
        if (!line[0] || line[0] == '#')
            continue;
        for (i = 0; i < G_N_ELEMENTS(names); i++) {
            if (g_str_has_prefix(line, names[i])
                && g_ascii_isspace(line[strlen(names[i])]))
                break;
        }
        if (i == G_N_ELEMENTS(names)) {
            g_printerr("Cannot understand line: %s\n", line);
            continue;
        }
        if (seen[i])
            g_printerr("Duplicit value: %.*s\n", (gint)strlen(names[i]), line);
        else {
            if (i == XO)
                mpf_init(region->xo);
            else if (i == YO)
                mpf_init(region->yo);
        }

        line += strlen(names[i]) + 1;
        if (i == XO || i == YO) {
            /* Fix locale-dependent gmp functions */
            if ((q = strchr(line, '.')))
                *q = lcnv->decimal_point[0];

            if (i == XO)
                mpf_set_str(region->xo, line, 10);
            else if (i == YO)
                mpf_set_str(region->yo, line, 10);
            else {
                g_assert_not_reached();
            }
        }
        else if (i == XS)
            region->xs = g_ascii_strtod(line, NULL);
        else if (i == YS)
            region->ys = g_ascii_strtod(line, NULL);
        else
            g_assert_not_reached();
        seen[i] = TRUE;
    }

    for (i = 0; i < G_N_ELEMENTS(names); i++) {
        if (!seen[i]) {
            g_printerr("Missing %s\n", names[i]);
            break;
        }
    }
    if (i == G_N_ELEMENTS(names))
        return TRUE;

    if (seen[XO])
        mpf_clear(region->xo);
    if (seen[YO])
        mpf_clear(region->yo);

    return FALSE;
}

static void
restore_region(Application *app,
               const gchar *filename)
{
    MPRegion *region = NULL;
    gchar *buf;
    GError *err = NULL;

    if (!g_file_get_contents(filename, &buf, NULL, &err)) {
        g_printerr("Cannot read %s: %s\n", filename, err->message);
        g_clear_error(&err);
    }
    else {
        region = g_new(MPRegion, 1);
        if (!read_region(region, buf)) {
            g_free(region);
            region = NULL;
        }
        else
            waste_of_time(TRUE, TRUE);
        g_free(buf);
    }

    if (region) {
        stop_calculation(app);
        app->zoom_stack = g_slist_prepend(app->zoom_stack, region);
        calculate(app);
    }
}

static void
restore(Application *app)
{
    ensure_filechooser(app, GTK_FILE_CHOOSER_ACTION_SAVE);
    if (gtk_dialog_run(GTK_DIALOG(app->filechooser)) == GTK_RESPONSE_OK) {
        gchar *fnm;

        fnm = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(app->filechooser));
        if (fnm) {
            restore_region(app, fnm);
            g_free(fnm);
        }
    }
    gtk_widget_hide(app->filechooser);
}

static GtkWidget*
app_toolbox_new(Application *app)
{
    GtkWidget *window, *scwin, *vbox, *hbox, *button, *gradients;
    GSList *group, *l;

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "GwyFract");
    gtk_window_set_default_size(GTK_WINDOW(window), -1, 360);

    vbox = gtk_vbox_new(FALSE, 2);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 4);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    hbox = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    group = gwy_radio_buttons_createl(G_CALLBACK(mode_changed), app,
                                      app->julia,
                                      "_Mandelbrot", FALSE,
                                      "_Julia", TRUE,
                                      NULL);
    for (l = group; l; l = g_slist_next(l))
        gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(l->data), FALSE, FALSE, 0);

    hbox = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    group = gwy_radio_buttons_createl(G_CALLBACK(map_changed), app,
                                      app->map_type,
                                      "Li_near", MAP_LINEAR,
                                      "S_qrt", MAP_SQRT,
                                      "S-_L", MAP_S_L,
                                      "Lo_g", MAP_LOG,
                                      "_Adapt", MAP_ADAPT,
                                      NULL);
    for (l = group; l; l = g_slist_next(l))
        gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(l->data), FALSE, FALSE, 0);

    hbox = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    button = gtk_check_button_new_with_mnemonic("Smoot_h");
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), app->smooth);
    g_signal_connect(button, "toggled", G_CALLBACK(smooth_toggled), app);

    button = gtk_check_button_new_with_mnemonic("_Invert");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), app->invert);
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
    g_signal_connect(button, "toggled", G_CALLBACK(invert_toggled), app);

    button = gtk_check_button_new_with_mnemonic("_0-base");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), app->from_0);
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
    g_signal_connect(button, "toggled", G_CALLBACK(from_0_toggled), app);

    hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    app->benhance = gtk_button_new_with_mnemonic("_Enhance");
    gtk_box_pack_start(GTK_BOX(hbox), app->benhance, FALSE, FALSE, 0);
    g_signal_connect_swapped(app->benhance, "clicked",
                             G_CALLBACK(enhance), app);

    app->bsave = gtk_button_new_with_mnemonic("_Save");
    gtk_box_pack_start(GTK_BOX(hbox), app->bsave, FALSE, FALSE, 0);
    g_signal_connect_swapped(app->bsave, "clicked",
                             G_CALLBACK(save), app);

    app->brestore = gtk_button_new_with_mnemonic("_Restore");
    gtk_box_pack_start(GTK_BOX(hbox), app->brestore, FALSE, FALSE, 0);
    g_signal_connect_swapped(app->brestore, "clicked",
                             G_CALLBACK(restore), app);

    scwin = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scwin),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(vbox), scwin, TRUE, TRUE, 0);

    gradients = gwy_gradient_tree_view_new(G_CALLBACK(gradient_selected),
                                           app, NULL);
    gtk_container_add(GTK_CONTAINER(scwin), gradients);

    hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_end(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    app->status = g_object_new(GTK_TYPE_LABEL,
                               "single-line-mode", TRUE,
                               "xalign", 0.0,
                               "ellipsize", PANGO_ELLIPSIZE_END,
                               NULL);
    gtk_box_pack_start(GTK_BOX(hbox), app->status, TRUE, TRUE, 0);

    app->pbar = gtk_progress_bar_new();
    gtk_box_pack_start(GTK_BOX(hbox), app->pbar, TRUE, TRUE, 0);

    return window;
}

static void
setup_workers(Application *app)
{
    guint i;

    app->state = STATE_READY;
    app->tasks = g_new0(Task, app->ncpus);
    for (i = 0; i < app->ncpus; i++) {
        app->tasks[i].id = i;
        app->tasks[i].ntasks = app->ncpus;
        app->tasks[i].iters = app->iters;
        app->tasks[i].eiters = app->eiters;
    }
    app->workers = g_thread_pool_new((GFunc)worker_calculate,
                                     NULL, app->ncpus, FALSE, NULL);
}

static void
print_help(void)
{
        g_print(
"Usage: gwyfract [SAVEFILE]\n"
"A toy Mandelrbot set rendered using Gwyddion libraries.\n\n"
        );
    g_print(
"Options:\n"
" -h, --help                 Print this help and terminate.\n"
" -v, --version              Print version info and terminate.\n\n"
        );
    g_print("Report bugs to <yeti@gwyddion.net>\n");

    exit(EXIT_SUCCESS);
}

int
main(int argc, char *argv[])
{
    Application app;
    GtkWidget *window;
    MPRegion *region;

    g_thread_init(NULL);
    if (!g_thread_supported()) {
        g_printerr("Threads not supported.\n");
        exit(EXIT_FAILURE);
    }

    if (argc == 2) {
        if (gwy_strequal(argv[1], "--help") || gwy_strequal(argv[1], "-h"))
            print_help();
        if (gwy_strequal(argv[1], "--version") || gwy_strequal(argv[1], "-v")) {
            g_print("%s %s\n", PACKAGE_NAME, PACKAGE_VERSION);
            exit(EXIT_SUCCESS);
        }
    }
    else if (argc > 2)
        print_help();

    memset(&app, 0, sizeof(Application));
    app.ncpus = get_n_cpus();
    app.bits = get_long_double_bits();
    g_printerr("number of threads: %d\n"
               "mantissa bits:     %d\n",
               app.ncpus, app.bits);

    mpf_set_default_prec(app.bits);
    gtk_init(&argc, &argv);
    gwy_app_init_common(NULL, "layer", NULL);

    app.map_type = MAP_SQRT;
    app.smooth = TRUE;

    app.container = gwy_container_new();
    /* Always put the default to the zoom stack */
    region = g_new(MPRegion, 1);
    region_init_set_from_double(region, &defmxy);
    app.zoom_stack = g_slist_prepend(app.zoom_stack, region);

    app.toolbox = window = app_toolbox_new(&app);
    g_signal_connect(window, "destroy", G_CALLBACK(shut_down), &app);
    gtk_widget_show_all(window);

    app.fwindow = window = app_fractal_display_new(&app);
    update_title(&app);
    g_signal_connect(window, "destroy", G_CALLBACK(shut_down), &app);
    gtk_widget_show_all(window);

    setup_workers(&app);
    app.progress_id = g_timeout_add(PROGRESSBAR_TIMEOUT,
                                    (GSourceFunc)check_progress, &app);

    if (argv[1])
        restore_region(&app, argv[1]);

    gtk_main();

    return EXIT_SUCCESS;
}
