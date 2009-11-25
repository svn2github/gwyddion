/*
 *  $Id$
 *  Copyright (C) 2009 David Necas (Yeti).
 *  E-mail: yeti@gwyddion.net.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include <string.h>
#include <glib.h>
#include "libgwy/macros.h"
#include "libgwy/fit-task.h"
#include "libgwy/math.h"
#include "libgwy/types.h"
#include "libgwy/libgwy-aliases.h"

#define SLi gwy_lower_triangular_matrix_index
#define MATRIX_LEN gwy_triangular_matrix_length
#define ASSIGN(p, q, n) memcpy((p), (q), (n)*sizeof(gdouble))

#define GWY_FIT_TASK_GET_PRIVATE(o)  \
   (G_TYPE_INSTANCE_GET_PRIVATE((o), GWY_TYPE_FIT_TASK, GwyFitTaskPrivate))

enum {
    VARARG_PARAM_MAX = 5
};

typedef enum {
    NONE = 0,
    POINT_VARARG,
    VECTOR_VARARG,
    VECTOR_ARRAY
} GwyFitTaskInterfaceType;

typedef gboolean (*GwyFitTaskPointFunc1)(gdouble x,
                                         gdouble *retval,
                                         gdouble p0);
typedef gboolean (*GwyFitTaskPointFunc2)(gdouble x,
                                         gdouble *retval,
                                         gdouble p0,
                                         gdouble p1);
typedef gboolean (*GwyFitTaskPointFunc3)(gdouble x,
                                         gdouble *retval,
                                         gdouble p0,
                                         gdouble p1,
                                         gdouble p2);
typedef gboolean (*GwyFitTaskPointFunc4)(gdouble x,
                                         gdouble *retval,
                                         gdouble p0,
                                         gdouble p1,
                                         gdouble p2,
                                         gdouble p3);
typedef gboolean (*GwyFitTaskPointFunc5)(gdouble x,
                                         gdouble *retval,
                                         gdouble p0,
                                         gdouble p1,
                                         gdouble p2,
                                         gdouble p3,
                                         gdouble p4);
typedef gboolean (*GwyFitTaskPointFunc6)(gdouble x,
                                         gdouble *retval,
                                         gdouble p0,
                                         gdouble p1,
                                         gdouble p2,
                                         gdouble p3,
                                         gdouble p4,
                                         gdouble p5);

typedef gdouble (*GwyFitTaskPointWeightFunc1)(gdouble x,
                                              gdouble p0);
typedef gdouble (*GwyFitTaskPointWeightFunc2)(gdouble x,
                                              gdouble p0,
                                              gdouble p1);
typedef gdouble (*GwyFitTaskPointWeightFunc3)(gdouble x,
                                              gdouble p0,
                                              gdouble p1,
                                              gdouble p2);
typedef gdouble (*GwyFitTaskPointWeightFunc4)(gdouble x,
                                              gdouble p0,
                                              gdouble p1,
                                              gdouble p2,
                                              gdouble p3);
typedef gdouble (*GwyFitTaskPointWeightFunc5)(gdouble x,
                                              gdouble p0,
                                              gdouble p1,
                                              gdouble p2,
                                              gdouble p3,
                                              gdouble p4);
typedef gdouble (*GwyFitTaskPointWeightFunc6)(gdouble x,
                                              gdouble p0,
                                              gdouble p1,
                                              gdouble p2,
                                              gdouble p3,
                                              gdouble p4,
                                              gdouble p5);

typedef gboolean (*GwyFitTaskVectorFunc1)(guint i,
                                          gpointer user_data,
                                          gdouble *retval,
                                          gdouble p0);
typedef gboolean (*GwyFitTaskVectorFunc2)(guint i,
                                          gpointer user_data,
                                          gdouble *retval,
                                          gdouble p0,
                                          gdouble p1);
typedef gboolean (*GwyFitTaskVectorFunc3)(guint i,
                                          gpointer user_data,
                                          gdouble *retval,
                                          gdouble p0,
                                          gdouble p1,
                                          gdouble p2);
typedef gboolean (*GwyFitTaskVectorFunc4)(guint i,
                                          gpointer user_data,
                                          gdouble *retval,
                                          gdouble p0,
                                          gdouble p1,
                                          gdouble p2,
                                          gdouble p3);
typedef gboolean (*GwyFitTaskVectorFunc5)(guint i,
                                          gpointer user_data,
                                          gdouble *retval,
                                          gdouble p0,
                                          gdouble p1,
                                          gdouble p2,
                                          gdouble p3,
                                          gdouble p4);
typedef gboolean (*GwyFitTaskVectorFunc6)(guint i,
                                          gpointer user_data,
                                          gdouble *retval,
                                          gdouble p0,
                                          gdouble p1,
                                          gdouble p2,
                                          gdouble p3,
                                          gdouble p4,
                                          gdouble p5);

typedef struct {
    GwyFitter *fitter;
    GwyFitTaskInterfaceType type;
    guint nparam;
    guint ndata;
    gboolean *fixed_param;
    gdouble *h;
    gdouble *mparam;
    gdouble *diff;
    /* Point interface */
    GwyFitTaskPointFunc point_func;
    GwyFitTaskPointWeightFunc point_weight;
    const GwyPointXY *point_data;
    /* Indexed data interface */
    GwyFitTaskVectorFunc vector_func;
    GwyFitTaskVectorVFunc vector_vfunc;
    GwyFitTaskVectorDFunc vector_dfunc;
    gpointer vector_data;
} FitTask;

typedef FitTask GwyFitTaskPrivate;

static void     gwy_fit_task_finalize(GObject *object);
static void     gwy_fit_task_dispose (GObject *object);
static void     set_n_params         (FitTask *fittask,
                                      guint nparam);
static gboolean fit_task_residuum    (const gdouble *param,
                                      gdouble *residuum,
                                      gpointer user_data);
static gboolean fit_task_gradient    (const gdouble *param,
                                      gdouble *gradient,
                                      gdouble *hessian,
                                      gpointer user_data);

G_DEFINE_TYPE(GwyFitTask, gwy_fit_task, G_TYPE_OBJECT)

static void
gwy_fit_task_class_init(GwyFitTaskClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    g_type_class_add_private(klass, sizeof(GwyFitTaskPrivate));

    gobject_class->finalize = gwy_fit_task_finalize;
    gobject_class->dispose = gwy_fit_task_dispose;
}

/**
 * gwy_fit_task_new:
 *
 * Creates a new non-linear least-squares fitting task.
 *
 * Returns: A new non-linear least-squares fitting task.
 **/
GwyFitTask*
gwy_fit_task_new(void)
{
    return g_object_newv(GWY_TYPE_FIT_TASK, 0, NULL);
}

static void
gwy_fit_task_init(GwyFitTask *object)
{
    G_GNUC_UNUSED FitTask *fittask = GWY_FIT_TASK_GET_PRIVATE(object);
}

static void
gwy_fit_task_finalize(GObject *object)
{
    FitTask *fittask = GWY_FIT_TASK_GET_PRIVATE(object);
    set_n_params(fittask, 0);
    G_OBJECT_CLASS(gwy_fit_task_parent_class)->finalize(object);
}

static void
gwy_fit_task_dispose(GObject *object)
{
    FitTask *fittask = GWY_FIT_TASK_GET_PRIVATE(object);
    GWY_OBJECT_UNREF(fittask->fitter);
    G_OBJECT_CLASS(gwy_fit_task_parent_class)->dispose(object);
}

/**
 * gwy_fit_task_get_max_vararg_params:
 *
 * Obtains the maximum supported number of parameters for the vararg
 * interfaces.
 *
 * If you need more parameters than that you have to use the vector interface
 * with parameters passed in arrays.
 *
 * Returns: The maximum number of vararg parameters.
 **/
guint
gwy_fit_task_get_max_vararg_params(void)
{
    return VARARG_PARAM_MAX;
}

static void
ensure_fitter(FitTask *fittask)
{
    if (fittask->fitter)
        return;

    fittask->fitter = gwy_fitter_new();
    gwy_fitter_set_functions(fittask->fitter,
                             fit_task_residuum, fit_task_gradient);
}

/* The values would not confuse us when using the current interface but we
 * do not want any leftovers if someone switches interfaces back and forth. */
static void
invalidate_point_interface(FitTask *fittask)
{
    fittask->point_func = NULL;
    fittask->point_weight = NULL;
    if (fittask->point_data) {
        fittask->ndata = 0;
        fittask->point_data = NULL;
    }
}

static void
invalidate_vector_interface(FitTask *fittask)
{
    fittask->vector_func = NULL;
    fittask->vector_vfunc = NULL;
    fittask->vector_dfunc = NULL;
    if (fittask->vector_data) {
        fittask->ndata = 0;
        fittask->vector_data = NULL;
    }
}

static void
set_n_params(FitTask *fittask,
             guint nparam)
{
    fittask->fixed_param = g_renew(gboolean, fittask->fixed_param,
                                      nparam);
    fittask->h = g_renew(gdouble, fittask->h, 3*nparam);
    fittask->mparam = fittask->h + nparam;
    fittask->diff = fittask->mparam + nparam;
    if (nparam)
        gwy_memclear(fittask->fixed_param, nparam);
    fittask->nparam = nparam;

    ensure_fitter(fittask);
    gwy_fitter_set_n_params(fittask->fitter, nparam);
}

/**
 * gwy_fit_task_set_point_function:
 * @fittask: A fitting task.
 * @nparam: Number of function parameters.
 * @function: Function to fit.
 *
 * Sets the point model function for a fit task.
 **/
void
gwy_fit_task_set_point_function(GwyFitTask *object,
                                guint nparams,
                                GwyFitTaskPointFunc function)
{
    g_return_if_fail(GWY_IS_FIT_TASK(object));
    g_return_if_fail(nparams <= VARARG_PARAM_MAX);
    FitTask *fittask = GWY_FIT_TASK_GET_PRIVATE(object);

    invalidate_vector_interface(fittask);
    fittask->type = POINT_VARARG;
    set_n_params(fittask, nparams);
    fittask->point_func = function;
}

void
gwy_fit_task_set_point_weight(GwyFitTask *object,
                              GwyFitTaskPointWeightFunc weight)
{
    g_return_if_fail(GWY_IS_FIT_TASK(object));
    FitTask *fittask = GWY_FIT_TASK_GET_PRIVATE(object);

    invalidate_vector_interface(fittask);
    g_return_if_fail(fittask->type == POINT_VARARG);
    fittask->point_weight = weight;
}

/**
 * gwy_fit_task_set_point_data:
 * @fittask: A fitting task.
 * @data: Point data, x-values are abscissas y-values are the data to fit.
 *        The data must exist during the lifetime of @fittask (or until another
 *        data is set) as @fittask does not make a copy of them.
 * @ndata: Number of data points.
 *
 * Sets the point data for a fit task.
 **/
void
gwy_fit_task_set_point_data(GwyFitTask *object,
                            const GwyPointXY *data,
                            guint ndata)
{
    g_return_if_fail(GWY_IS_FIT_TASK(object));
    FitTask *fittask = GWY_FIT_TASK_GET_PRIVATE(object);

    invalidate_vector_interface(fittask);
    g_return_if_fail(fittask->type == POINT_VARARG);
    fittask->ndata = ndata;
    fittask->point_data = data;
}

/**
 * gwy_fit_task_set_vector_function:
 * @fittask: A fitting task.
 * @nparam: Number of function parameters.
 * @function: Function to fit.
 *
 * Sets the indexed-data model function for a fit task.
 **/
void
gwy_fit_task_set_vector_function(GwyFitTask *object,
                                 guint nparams,
                                 GwyFitTaskVectorFunc function)
{
    g_return_if_fail(GWY_IS_FIT_TASK(object));
    g_return_if_fail(nparams <= VARARG_PARAM_MAX);
    FitTask *fittask = GWY_FIT_TASK_GET_PRIVATE(object);

    invalidate_point_interface(fittask);
    fittask->vector_vfunc = NULL;
    fittask->vector_dfunc = NULL;
    fittask->type = VECTOR_VARARG;
    set_n_params(fittask, nparams);
    fittask->vector_func = function;
}

void
gwy_fit_task_set_vector_vfunction(GwyFitTask *object,
                                  guint nparams,
                                  GwyFitTaskVectorVFunc function,
                                  GwyFitTaskVectorDFunc derivative)
{
    g_return_if_fail(GWY_IS_FIT_TASK(object));
    FitTask *fittask = GWY_FIT_TASK_GET_PRIVATE(object);

    invalidate_point_interface(fittask);
    fittask->vector_func = NULL;
    fittask->type = VECTOR_ARRAY;
    set_n_params(fittask, nparams);
    fittask->vector_vfunc = function;
    fittask->vector_dfunc = derivative;
}

/**
 * gwy_fit_task_set_vector_data:
 * @fittask: A fitting task.
 * @data: Data passed to #GwyFitTaskVectorFunc.
 * @ndata: Number of data points.
 *
 * Sets the indexed data for a fit task.
 **/
void
gwy_fit_task_set_vector_data(GwyFitTask *object,
                             gpointer user_data,
                             guint ndata)
{
    g_return_if_fail(GWY_IS_FIT_TASK(object));
    FitTask *fittask = GWY_FIT_TASK_GET_PRIVATE(object);

    invalidate_point_interface(fittask);
    g_return_if_fail(fittask->type == VECTOR_VARARG
                     || fittask->type == VECTOR_ARRAY);
    fittask->ndata = ndata;
    fittask->vector_data = user_data;
}

/**
 * gwy_fit_task_set_fixed_params:
 * @fittask: A fitting task.
 * @fixed_params: Array of length @nparams with %TRUE for fixed parameters,
 *                %FALSE for free parameters.  It is also possible to pass
 *                %NULL to make all parameters free.
 *
 * Sets the fixed/free state of fitting parameters of a fit task.
 **/
void
gwy_fit_task_set_fixed_params(GwyFitTask *object,
                              const gboolean *fixed_params)
{
    g_return_if_fail(GWY_IS_FIT_TASK(object));
    FitTask *fittask = GWY_FIT_TASK_GET_PRIVATE(object);
    guint nparam = fittask->nparam;
    if (fixed_params)
        memcpy(fittask->fixed_param, fixed_params, nparam*sizeof(gboolean));
    else
        gwy_memclear(fittask->fixed_param, nparam);
}

/**
 * gwy_fit_task_get_fixed_params:
 * @fittask: A fitting task.
 * @fixed_params: Array of length @nparams to fill with %TRUE for fixed
 *                parameters, %FALSE for free parameters.  It is possible to
 *                pass %NULL if the caller only needs the return value.
 *
 * Gets the fixed/free state of fitting parameters of a fit task.
 *
 * Returns: The number of fixed parameters.
 **/
guint
gwy_fit_task_get_fixed_params(GwyFitTask *object,
                              gboolean *fixed_params)
{
    g_return_val_if_fail(GWY_IS_FIT_TASK(object), 0);
    FitTask *fittask = GWY_FIT_TASK_GET_PRIVATE(object);
    guint nparam = fittask->nparam;
    guint count = 0;
    for (guint i = 0; i < nparam; i++)
        count += fittask->fixed_param[i] ? 1 : 0;
    if (fixed_params)
        memcpy(fixed_params, fittask->fixed_param, nparam*sizeof(gboolean));
    return count;
}

/**
 * gwy_fit_task_set_fixed_param:
 * @fittask: A fitting task.
 * @i: Parameter number.
 * @fixed: %TRUE to make the @i-th parameter fixed, %FALSE to make it free.
 *
 * Sets the fixed/free state of a fitting parameter of a fit task.
 **/
void
gwy_fit_task_set_fixed_param(GwyFitTask *object,
                             guint i,
                             gboolean fixed)
{
    g_return_if_fail(GWY_IS_FIT_TASK(object));
    FitTask *fittask = GWY_FIT_TASK_GET_PRIVATE(object);
    g_return_if_fail(i < fittask->nparam);
    fittask->fixed_param[i] = fixed;
}

/**
 * gwy_fit_task_get_fixed_param:
 * @fittask: A fitting task.
 * @i: Parameter number.
 *
 * Gets the fixed/free state of a fitting parameter of a fit task.
 *
 * Returns: %TRUE if the @i-th parameter is fixed, %FALSE if it is free.
 **/
gboolean
gwy_fit_task_get_fixed_param(GwyFitTask *object,
                             guint i)
{
    g_return_val_if_fail(GWY_IS_FIT_TASK(object), FALSE);
    FitTask *fittask = GWY_FIT_TASK_GET_PRIVATE(object);
    g_return_val_if_fail(i < fittask->nparam, FALSE);
    return fittask->fixed_param[i];
}

static inline gboolean
eval_point_vararg(gdouble x, const gdouble *param, guint nparam,
                  GwyFitTaskPointFunc func,
                  gdouble *retval)
{
    if (nparam == 1)
        return ((GwyFitTaskPointFunc1)func)(x, retval, param[0]);
    if (nparam == 2)
        return ((GwyFitTaskPointFunc2)func)(x, retval, param[0], param[1]);
    if (nparam == 3)
        return ((GwyFitTaskPointFunc3)func)(x, retval, param[0], param[1],
                                           param[2]);
    if (nparam == 4)
        return ((GwyFitTaskPointFunc4)func)(x, retval, param[0], param[1],
                                           param[2], param[3]);
    if (nparam == 5)
        return ((GwyFitTaskPointFunc5)func)(x, retval, param[0], param[1],
                                           param[2], param[3], param[5]);
    if (nparam == 6)
        return ((GwyFitTaskPointFunc6)func)(x, retval, param[0], param[1],
                                           param[2], param[3], param[5],
                                           param[6]);

    g_return_val_if_reached(FALSE);
}

static inline gboolean
eval_vector_vararg(guint i, gpointer data, const gdouble *param, guint nparam,
                   GwyFitTaskVectorFunc func,
                   gdouble *retval)
{
    if (nparam == 1)
        return ((GwyFitTaskVectorFunc1)func)(i, data, retval, param[0]);
    if (nparam == 2)
        return ((GwyFitTaskVectorFunc2)func)(i, data, retval, param[0],
                                            param[1]);
    if (nparam == 3)
        return ((GwyFitTaskVectorFunc3)func)(i, data, retval, param[0],
                                            param[1], param[2]);
    if (nparam == 4)
        return ((GwyFitTaskVectorFunc4)func)(i, data, retval, param[0],
                                            param[1], param[2], param[3]);
    if (nparam == 5)
        return ((GwyFitTaskVectorFunc5)func)(i, data, retval, param[0],
                                            param[1], param[2], param[3],
                                            param[5]);
    if (nparam == 6)
        return ((GwyFitTaskVectorFunc6)func)(i, data, retval, param[0],
                                            param[1], param[2], param[3],
                                            param[5], param[6]);

    g_return_val_if_reached(FALSE);
}

static gboolean
fit_task_residuum(const gdouble *param,
                  gdouble *residuum,
                  gpointer user_data)
{
    FitTask *fittask = (FitTask*)user_data;
    guint nparam = fittask->nparam;
    gdouble r = 0.0;

    /* TODO: Weights */
    if (fittask->type == POINT_VARARG) {
        g_return_val_if_fail(fittask->point_func, FALSE);
        g_return_val_if_fail(fittask->nparam <= VARARG_PARAM_MAX, FALSE);
        GwyFitTaskPointFunc func = fittask->point_func;
        const GwyPointXY *pts = fittask->point_data;

        for (guint i = 0; i < fittask->ndata; i++) {
            gdouble x = pts[i].x, y = pts[i].y, v;
            if (!eval_point_vararg(x, param, nparam, func, &v))
                return FALSE;
            v -= y;
            r += v*v;
        }
    }
    else if (fittask->type == VECTOR_VARARG) {
        g_return_val_if_fail(fittask->vector_func, FALSE);
        g_return_val_if_fail(fittask->nparam <= VARARG_PARAM_MAX, FALSE);
        GwyFitTaskVectorFunc func = fittask->vector_func;
        gpointer data = fittask->vector_data;

        for (guint i = 0; i < fittask->ndata; i++) {
            gdouble v;
            if (!eval_vector_vararg(i, data, param, nparam, func, &v))
                return FALSE;
            r += v*v;
        }
    }
    else if (fittask->type == VECTOR_ARRAY) {
        g_return_val_if_fail(fittask->vector_vfunc, FALSE);
        GwyFitTaskVectorVFunc vfunc = fittask->vector_vfunc;
        gpointer data = fittask->vector_data;

        for (guint i = 0; i < fittask->ndata; i++) {
            gdouble v;
            if (!vfunc(i, data, &v, param))
                return FALSE;
            r += v*v;
        }
    }

    *residuum = r;
    return TRUE;
}

static inline void
add_to_gradient_and_hessian(const gdouble *diff, gdouble v,
                            gdouble *gradient, gdouble *hessian, guint nparam)
{
    for (guint j = 0; j < nparam; j++) {
        gradient[j] += v*diff[j];
        for (guint k = 0; k <= j; k++)
            SLi(hessian, j, k) += diff[j]*diff[k];
    }
}

static gboolean
fit_task_gradient(const gdouble *param,
                  gdouble *gradient,
                  gdouble *hessian,
                  gpointer user_data)
{
    FitTask *fittask = (FitTask*)user_data;
    guint nparam = fittask->nparam;
    const gboolean *fixed_param = fittask->fixed_param;
    gdouble *h = fittask->h;
    gdouble *mparam = fittask->mparam;
    gdouble *diff = fittask->diff;
    gwy_memclear(gradient, nparam);
    gwy_memclear(hessian, MATRIX_LEN(nparam));

    ASSIGN(mparam, param, nparam);
    for (guint j = 0; j < nparam; j++)
        h[j] = param[j] ? 1e-5*fabs(param[j]) : 1e-9;

    /* TODO: Weights */
    if (fittask->type == POINT_VARARG) {
        g_return_val_if_fail(fittask->point_func, FALSE);
        g_return_val_if_fail(nparam <= VARARG_PARAM_MAX, FALSE);
        GwyFitTaskPointFunc func = fittask->point_func;
        const GwyPointXY *pts = fittask->point_data;

        for (guint i = 0; i < fittask->ndata; i++) {
            gdouble x = pts[i].x, y = pts[i].y, v;
            if (!eval_point_vararg(x, mparam, nparam, func, &v))
                return FALSE;
            v -= y;
            for (guint j = 0; j < nparam; j++) {
                if (fixed_param[j]) {
                    diff[j] = 0.0;
                    continue;
                }

                gdouble vminus;
                mparam[j] = param[j] - h[j];
                if (!eval_point_vararg(x, mparam, nparam, func, &vminus))
                    return FALSE;

                gdouble vplus;
                mparam[j] = param[j] + h[j];
                if (!eval_point_vararg(x, mparam, nparam, func, &vplus))
                    return FALSE;

                mparam[j] = param[j];
                diff[j] = (vplus - vminus)/(2.0*h[j]);
            }
            add_to_gradient_and_hessian(diff, v, gradient, hessian, nparam);
        }
    }
    else if (fittask->type == VECTOR_VARARG) {
        g_return_val_if_fail(fittask->vector_func, FALSE);
        g_return_val_if_fail(nparam <= VARARG_PARAM_MAX, FALSE);
        GwyFitTaskVectorFunc func = fittask->vector_func;
        gpointer data = fittask->vector_data;

        for (guint i = 0; i < fittask->ndata; i++) {
            gdouble v;
            if (!eval_vector_vararg(i, data, mparam, nparam, func, &v))
                return FALSE;
            for (guint j = 0; j < nparam; j++) {
                if (fixed_param[j]) {
                    diff[j] = 0.0;
                    continue;
                }

                gdouble vminus;
                mparam[j] = param[j] - h[j];
                if (!eval_vector_vararg(i, data, mparam, nparam, func, &vminus))
                    return FALSE;

                gdouble vplus;
                mparam[j] = param[j] + h[j];
                if (!eval_vector_vararg(i, data, mparam, nparam, func, &vplus))
                    return FALSE;

                mparam[j] = param[j];
                diff[j] = (vplus - vminus)/(2.0*h[j]);
            }
            add_to_gradient_and_hessian(diff, v, gradient, hessian, nparam);
        }
    }
    else if (fittask->type == VECTOR_ARRAY) {
        g_return_val_if_fail(fittask->vector_vfunc, FALSE);
        g_return_val_if_fail(fittask->vector_dfunc, FALSE);
        GwyFitTaskVectorVFunc vfunc = fittask->vector_vfunc;
        GwyFitTaskVectorDFunc dfunc = fittask->vector_dfunc;
        gpointer data = fittask->vector_data;

        for (guint i = 0; i < fittask->ndata; i++) {
            gdouble v;
            if (!vfunc(i, data, &v, mparam))
                return FALSE;
            if (!dfunc(i, data, fixed_param, diff, mparam))
                return FALSE;
            add_to_gradient_and_hessian(diff, v, gradient, hessian, nparam);
        }
    }
    return TRUE;
}

/**
 * gwy_fit_task_fit:
 * @fittask: A fitting task.
 *
 * Performs a non-linear least-squares fitting task.
 *
 * This is a wrapper for gwy_fitter_fit(), see its documentation for
 * discussion.
 *
 * Returns: %TRUE if the fit terminated normally.
 **/
gboolean
gwy_fit_task_fit(GwyFitTask *object)
{
    g_return_val_if_fail(GWY_IS_FIT_TASK(object), FALSE);
    FitTask *fittask = GWY_FIT_TASK_GET_PRIVATE(object);
    ensure_fitter(fittask);
    return gwy_fitter_fit(fittask->fitter, fittask);
}

/**
 * gwy_fit_task_eval_residuum:
 * @fittask: A fitting task.
 *
 * Calculates the sum of squares of a fitting task.
 *
 * This is a wrapper for gwy_fitter_eval_residuum(), see its documentation for
 * discussion.
 *
 * Returns: The sum of squares of differences; a negative value on failure.
 **/
gdouble
gwy_fit_task_eval_residuum(GwyFitTask *object)
{
    g_return_val_if_fail(GWY_IS_FIT_TASK(object), FALSE);
    FitTask *fittask = GWY_FIT_TASK_GET_PRIVATE(object);
    ensure_fitter(fittask);
    return gwy_fitter_eval_residuum(fittask->fitter, fittask);
}

/**
 * gwy_fit_task_get_fitter:
 * @fittask: A fitting task.
 *
 * Obtains the #GwyFitter associated with a fitting task.
 *
 * The fitter can be used for instance to modify parameter values or to set the
 * fitting algorithm tunables.  However, certain low-level functions would
 * break @fittask, see the introductory section for details.
 *
 * Returns: The associated fitter.
 **/
GwyFitter*
gwy_fit_task_get_fitter(GwyFitTask *object)
{
    g_return_val_if_fail(GWY_IS_FIT_TASK(object), NULL);
    FitTask *fittask = GWY_FIT_TASK_GET_PRIVATE(object);
    ensure_fitter(fittask);
    return fittask->fitter;
}

#define __LIBGWY_FIT_TASK_C__
#include "libgwy/libgwy-aliases.c"

/**
 * SECTION: fit-task
 * @title: GwyFitTask
 * @short_description: Non-linear least-squares fitter model and data
 *
 * Since #GwyFitTask supplies its own evaluation methods and controls the
 * number of parameters, the low-level setup methods gwy_fitter_set_functions()
 * and gwy_fitter_set_n_params() must not be used.  Neither can you use
 * gwy_fitter_fit() and gwy_fitter_eval_residuum() as you cannot pass the
 * correct @user_data; use gwy_fit_task_fit() and gwy_fit_task_eval_residuum()
 * instead of them.
 **/

/**
 * GwyFitTask:
 *
 * Object representing non-linear least-squares fitter task.
 *
 * The #GwyFitTask struct contains private data only and should be accessed
 * using the functions below.
 **/

/**
 * GwyFitTaskClass:
 * @g_object_class: Parent class.
 *
 * Class of non-linear least-squares fitter task.
 **/

/**
 * GwyFitTaskPointFunc:
 * @x: Abscissa value to calculate the function value in.
 * @retval: Location to store the function value in @x to.
 * @...: Function parameters.
 *
 * Type of point model function for fitting task.
 *
 * Although the formal function type has a variable number of arguments the
 * particular functions are expected to take exactly @nparams parameter
 * arguments (where @nparams is set in gwy_fit_task_set_point_function()).
 *
 * The maximum number of parameters for the point model functions is given
 * by gwy_fit_task_get_max_vararg_params().
 *
 * Returns: %TRUE if the calculation succeeded and @reval was set, %FALSE on
 *          failure.
 **/

/**
 * GwyFitTaskVectorFunc:
 * @i: Index of data point to calculate the function value in.
 * @user_data: Data set in gwy_fit_task_set_vector_data().
 * @retval: Location to store the difference between the function value in @x
 *          and the @i-th data value to.
 * @...: Function parameters.
 *
 * Type of indexed-data model function for fitting task.
 *
 * Although the formal function type has a variable number of arguments the
 * particular functions are expected to take exactly @nparams parameter
 * arguments (where @nparams is set in gwy_fit_task_set_point_function()).
 *
 * The maximum number of parameters for the point model functions is given
 * by gwy_fit_task_get_max_vararg_params().
 *
 * Returns: %TRUE if the calculation succeeded and @reval was set, %FALSE on
 *          failure.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
