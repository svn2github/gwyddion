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

#include <string.h>
#include <glib.h>
#include "libgwy/macros.h"
#include "libgwy/fit-task.h"
#include "libgwy/types.h"
#include "libgwy/libgwy-aliases.h"
#include "libgwy/processing-internal.h"

enum { VARARG_PARAM_MAX = 5 };

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

struct _GwyFitTaskPrivate {
    GObject g_object;
    GwyFitter *fitter;
    GwyFitTaskInterfaceType type;
    guint nparam;
    guint ndata;
    gboolean *fixed_param;
    gdouble *h;
    gdouble *mparam;
    gdouble *diff;
    gdouble *matrix;
    /* Point interface */
    GwyFitTaskPointFunc point_func;
    GwyFitTaskPointWeightFunc point_weight;
    const GwyXY *point_data;
    /* Indexed data interface */
    GwyFitTaskVectorFunc vector_func;
    GwyFitTaskVectorVFunc vector_vfunc;
    GwyFitTaskVectorDFunc vector_dfunc;
    gpointer vector_data;
};

typedef struct _GwyFitTaskPrivate FitTask;

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

    g_type_class_add_private(klass, sizeof(FitTask));

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
gwy_fit_task_init(GwyFitTask *fittask)
{
    fittask->priv = G_TYPE_INSTANCE_GET_PRIVATE(fittask, GWY_TYPE_FIT_TASK,
                                                FitTask);
}

static void
gwy_fit_task_finalize(GObject *object)
{
    GwyFitTask *fittask = GWY_FIT_TASK(object);
    set_n_params(fittask->priv, 0);
    G_OBJECT_CLASS(gwy_fit_task_parent_class)->finalize(object);
}

static void
gwy_fit_task_dispose(GObject *object)
{
    GwyFitTask *fittask = GWY_FIT_TASK(object);
    GWY_OBJECT_UNREF(fittask->priv->fitter);
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
    guint matrix_len = MATRIX_LEN(nparam);
    fittask->fixed_param = g_renew(gboolean, fittask->fixed_param, nparam);
    fittask->h = g_renew(gdouble, fittask->h, 3*nparam + matrix_len);
    fittask->mparam = fittask->h + nparam;
    fittask->diff = fittask->mparam + nparam;
    fittask->matrix = fittask->diff + nparam;
    if (nparam)
        gwy_memclear(fittask->fixed_param, nparam);
    fittask->nparam = nparam;

    if (nparam) {
        ensure_fitter(fittask);
        gwy_fitter_set_n_params(fittask->fitter, nparam);
    }
}

/**
 * gwy_fit_task_set_point_function:
 * @fittask: A fitting task.
 * @nparams: Number of function parameters.
 * @function: Function to fit.
 *
 * Sets the point model function for a fit task.
 **/
void
gwy_fit_task_set_point_function(GwyFitTask *fittask,
                                guint nparams,
                                GwyFitTaskPointFunc function)
{
    g_return_if_fail(GWY_IS_FIT_TASK(fittask));
    g_return_if_fail(nparams > 0);
    g_return_if_fail(nparams <= VARARG_PARAM_MAX);
    FitTask *priv = fittask->priv;

    invalidate_vector_interface(priv);
    priv->type = POINT_VARARG;
    set_n_params(priv, nparams);
    priv->point_func = function;
}

/**
 * gwy_fit_task_set_point_weight:
 * @fittask: A fitting task.
 * @weight: Weighting function.
 *
 * Sets the point model weight function for a fit task.
 *
 * Note the weight is applied to the differences, not to their squares.
 **/
void
gwy_fit_task_set_point_weight(GwyFitTask *fittask,
                              GwyFitTaskPointWeightFunc weight)
{
    g_return_if_fail(GWY_IS_FIT_TASK(fittask));
    FitTask *priv = fittask->priv;

    invalidate_vector_interface(priv);
    g_return_if_fail(priv->type == POINT_VARARG);
    priv->point_weight = weight;
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
gwy_fit_task_set_point_data(GwyFitTask *fittask,
                            const GwyXY *data,
                            guint ndata)
{
    g_return_if_fail(GWY_IS_FIT_TASK(fittask));
    FitTask *priv = fittask->priv;

    invalidate_vector_interface(priv);
    g_return_if_fail(priv->type == POINT_VARARG);
    priv->ndata = ndata;
    priv->point_data = data;
}

/**
 * gwy_fit_task_set_vector_function:
 * @fittask: A fitting task.
 * @nparams: Number of function parameters.
 * @function: Function to calculate theoretical minus fitted data differences.
 *
 * Sets the indexed-data model function for a fit task.
 **/
void
gwy_fit_task_set_vector_function(GwyFitTask *fittask,
                                 guint nparams,
                                 GwyFitTaskVectorFunc function)
{
    g_return_if_fail(GWY_IS_FIT_TASK(fittask));
    g_return_if_fail(nparams > 0);
    g_return_if_fail(nparams <= VARARG_PARAM_MAX);
    FitTask *priv = fittask->priv;

    invalidate_point_interface(priv);
    priv->vector_vfunc = NULL;
    priv->vector_dfunc = NULL;
    priv->type = VECTOR_VARARG;
    set_n_params(priv, nparams);
    priv->vector_func = function;
}

/**
 * gwy_fit_task_set_vector_vfunction:
 * @fittask: A fitting task.
 * @nparams: Number of function parameters.
 * @function: Function to calculate theoretical minus fitted data differences.
 * @derivative: Function to calculate derivatives of @function by parameters.
 *              It can be %NULL to use the built-in function.
 *
 * Sets the indexed-data model functions with parameter arrays for a fit task.
 **/
void
gwy_fit_task_set_vector_vfunction(GwyFitTask *fittask,
                                  guint nparams,
                                  GwyFitTaskVectorVFunc function,
                                  GwyFitTaskVectorDFunc derivative)
{
    g_return_if_fail(GWY_IS_FIT_TASK(fittask));
    g_return_if_fail(nparams > 0);
    FitTask *priv = fittask->priv;

    invalidate_point_interface(priv);
    priv->vector_func = NULL;
    priv->type = VECTOR_ARRAY;
    set_n_params(priv, nparams);
    priv->vector_vfunc = function;
    priv->vector_dfunc = derivative;
}

/**
 * gwy_fit_task_set_vector_data:
 * @fittask: A fitting task.
 * @user_data: Data passed to #GwyFitTaskVectorFunc.
 * @ndata: Number of data points.
 *
 * Sets the indexed data for a fit task.
 **/
void
gwy_fit_task_set_vector_data(GwyFitTask *fittask,
                             gpointer user_data,
                             guint ndata)
{
    g_return_if_fail(GWY_IS_FIT_TASK(fittask));
    FitTask *priv = fittask->priv;

    invalidate_point_interface(priv);
    g_return_if_fail(priv->type == VECTOR_VARARG
                     || priv->type == VECTOR_ARRAY);
    priv->ndata = ndata;
    priv->vector_data = user_data;
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
gwy_fit_task_set_fixed_params(GwyFitTask *fittask,
                              const gboolean *fixed_params)
{
    g_return_if_fail(GWY_IS_FIT_TASK(fittask));
    FitTask *priv = fittask->priv;
    guint nparam = priv->nparam;
    if (fixed_params)
        memcpy(priv->fixed_param, fixed_params, nparam*sizeof(gboolean));
    else
        gwy_memclear(priv->fixed_param, nparam);
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
gwy_fit_task_get_fixed_params(GwyFitTask *fittask,
                              gboolean *fixed_params)
{
    g_return_val_if_fail(GWY_IS_FIT_TASK(fittask), 0);
    FitTask *priv = fittask->priv;
    guint nparam = priv->nparam;
    guint count = 0;
    for (guint i = 0; i < nparam; i++)
        count += priv->fixed_param[i] ? 1 : 0;
    if (fixed_params)
        memcpy(fixed_params, priv->fixed_param, nparam*sizeof(gboolean));
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
gwy_fit_task_set_fixed_param(GwyFitTask *fittask,
                             guint i,
                             gboolean fixed)
{
    g_return_if_fail(GWY_IS_FIT_TASK(fittask));
    FitTask *priv = fittask->priv;
    g_return_if_fail(i < priv->nparam);
    priv->fixed_param[i] = fixed;
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
gwy_fit_task_get_fixed_param(GwyFitTask *fittask,
                             guint i)
{
    g_return_val_if_fail(GWY_IS_FIT_TASK(fittask), FALSE);
    FitTask *priv = fittask->priv;
    g_return_val_if_fail(i < priv->nparam, FALSE);
    return priv->fixed_param[i];
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

    if (fittask->type == POINT_VARARG) {
        g_return_val_if_fail(fittask->point_func, FALSE);
        g_return_val_if_fail(fittask->nparam <= VARARG_PARAM_MAX, FALSE);
        GwyFitTaskPointFunc func = fittask->point_func;
        const GwyXY *pts = fittask->point_data;

        for (guint i = 0; i < fittask->ndata; i++) {
            gdouble x = pts[i].x, y = pts[i].y, v;
            if (!eval_point_vararg(x, param, nparam, func, &v))
                return FALSE;
            v -= y;
            if (fittask->point_weight)
                v *= fittask->point_weight(x);
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
    else {
        g_return_val_if_reached(FALSE);
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

    if (fittask->type == POINT_VARARG) {
        g_return_val_if_fail(fittask->point_func, FALSE);
        g_return_val_if_fail(nparam <= VARARG_PARAM_MAX, FALSE);
        GwyFitTaskPointFunc func = fittask->point_func;
        const GwyXY *pts = fittask->point_data;

        for (guint i = 0; i < fittask->ndata; i++) {
            gdouble x = pts[i].x, y = pts[i].y, v;
            if (!eval_point_vararg(x, mparam, nparam, func, &v))
                return FALSE;
            v -= y;
            gdouble w = fittask->point_weight ? fittask->point_weight(x) : 1.0;
            v *= w;
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
                diff[j] = w*(vplus - vminus)/(2.0*h[j]);
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
    else if (fittask->type == VECTOR_ARRAY && fittask->vector_dfunc) {
        g_return_val_if_fail(fittask->vector_vfunc, FALSE);
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
    else if (fittask->type == VECTOR_ARRAY) {
        g_return_val_if_fail(fittask->vector_vfunc, FALSE);
        GwyFitTaskVectorVFunc vfunc = fittask->vector_vfunc;
        gpointer data = fittask->vector_data;

        for (guint i = 0; i < fittask->ndata; i++) {
            gdouble v;
            if (!vfunc(i, data, &v, mparam))
                return FALSE;
            for (guint j = 0; j < nparam; j++) {
                if (fixed_param[j]) {
                    diff[j] = 0.0;
                    continue;
                }

                gdouble vminus;
                mparam[j] = param[j] - h[j];
                if (!vfunc(i, data, &vminus, mparam))
                    return FALSE;

                gdouble vplus;
                mparam[j] = param[j] + h[j];
                if (!vfunc(i, data, &vplus, mparam))
                    return FALSE;

                mparam[j] = param[j];
                diff[j] = (vplus - vminus)/(2.0*h[j]);
            }
            add_to_gradient_and_hessian(diff, v, gradient, hessian, nparam);
        }
    }
    else {
        g_return_val_if_reached(FALSE);
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
gwy_fit_task_fit(GwyFitTask *fittask)
{
    g_return_val_if_fail(GWY_IS_FIT_TASK(fittask), FALSE);
    FitTask *priv = fittask->priv;
    ensure_fitter(priv);
    return gwy_fitter_fit(priv->fitter, priv);
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
gwy_fit_task_eval_residuum(GwyFitTask *fittask)
{
    g_return_val_if_fail(GWY_IS_FIT_TASK(fittask), FALSE);
    FitTask *priv = fittask->priv;
    ensure_fitter(priv);
    return gwy_fitter_eval_residuum(priv->fitter, priv);
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
gwy_fit_task_get_fitter(GwyFitTask *fittask)
{
    g_return_val_if_fail(GWY_IS_FIT_TASK(fittask), NULL);
    FitTask *priv = fittask->priv;
    ensure_fitter(priv);
    return priv->fitter;
}

/**
 * gwy_fit_task_get_param_errors:
 * @fittask: A fitting task.
 * @variance_covariance: %TRUE for unweighted parameters, i.e. to estimate the
 *                       data statistical errors from the final fit.  %FALSE
 *                       if the differences between model and fitted data were
 *                       weighted by inverse squared standard deviations.
 * @errors: Array of length @nparams to store the parameter standard deviations
 *          to.
 *
 * Evaluates standard deviations of fitting task model parameters after fit.
 *
 * Returns: %TRUE if the evaluation succeeded, %FALSE on failue, e.g. if it
 *          is not possible to invert the Hessian.
 **/
gboolean
gwy_fit_task_get_param_errors(GwyFitTask *fittask,
                              gboolean variance_covariance,
                              gdouble *errors)
{
    g_return_val_if_fail(GWY_IS_FIT_TASK(fittask), FALSE);
    FitTask *priv = fittask->priv;
    GwyFitter *fitter = priv->fitter;
    gdouble *matrix = priv->matrix;
    if (!fitter || !gwy_fitter_get_inverse_hessian(fitter, matrix))
        return FALSE;
    /* If we have valid inverse Hessian we must have residuum too. */
    gdouble res = 1.0;
    if (variance_covariance) {
        res = gwy_fitter_get_residuum(fitter);
        g_return_val_if_fail(res >= 0.0, FALSE);
    }
    guint nparam = priv->nparam;
    for (guint i = 0; i < nparam; i++)
        errors[i] = sqrt(SLi(matrix, i, i)*res/(priv->ndata - nparam));
    return TRUE;
}

/**
 * gwy_fit_task_get_param_error:
 * @fittask: A fitting task.
 * @i: Parameter number.
 * @variance_covariance: %TRUE for unweighted parameters, i.e. to estimate the
 *                       data statistical errors from the final fit.  %FALSE
 *                       if the differences between model and fitted data were
 *                       weighted by inverse squared standard deviations.
 *
 * Evaluates standard deviations of a fitting task model parameter after fit.
 *
 * Returns: The standard deviation of @i-th parameter, a negative value on
 *          failure.
 **/
gdouble
gwy_fit_task_get_param_error(GwyFitTask *fittask,
                             guint i,
                             gboolean variance_covariance)
{
    g_return_val_if_fail(GWY_IS_FIT_TASK(fittask), -1.0);
    FitTask *priv = fittask->priv;
    g_return_val_if_fail(i < priv->nparam, -1.0);
    if (!gwy_fit_task_get_param_errors(fittask, variance_covariance, priv->h))
        return -1.0;
    return priv->h[i];
}

/**
 * gwy_fit_task_get_correlations:
 * @fittask: A fitting task.
 * @corr_matrix: Array to store the correlation matrix to.  The elements are
 *               stored as described in gwy_lower_triangular_matrix_index().
 *
 * Evaluates the parameter correlation matrix of a fitting task after fit.
 *
 * Returns: %TRUE if the evaluation succeeded, %FALSE on failue, e.g. if it
 *          is not possible to invert the Hessian.
 **/
gboolean
gwy_fit_task_get_correlations(GwyFitTask *fittask,
                              gdouble *corr_matrix)
{
    g_return_val_if_fail(GWY_IS_FIT_TASK(fittask), FALSE);
    FitTask *priv = fittask->priv;
    GwyFitter *fitter = priv->fitter;
    gdouble *matrix = priv->matrix;
    if (!fitter || !gwy_fitter_get_inverse_hessian(fitter, matrix))
        return FALSE;
    guint nparam = priv->nparam;
    gdouble *h = priv->h;
    for (guint i = 0; i < nparam; i++)
        h[i] = sqrt(SLi(matrix, i, i));
    for (guint i = 0; i < nparam; i++) {
        for (guint j = 0; j < i; i++)
            SLi(corr_matrix, i, j) = SLi(matrix, i, j)/(h[i]*h[j]);
        SLi(corr_matrix, i, i) = 1.0;
    }
    return TRUE;
}

/**
 * gwy_fit_task_get_chi:
 * @fittask: A fitting task.
 *
 * Evaluates the chi value of a fitting task after fit.
 *
 * Note the returned number is meaningful only if the differences between
 * theoretical and fitted data were weighted using inverse squared standard
 * deviations.
 *
 * Returns: The value of chi, a negative value on failure.
 **/
gdouble
gwy_fit_task_get_chi(GwyFitTask *fittask)
{
    g_return_val_if_fail(GWY_IS_FIT_TASK(fittask), FALSE);
    FitTask *priv = fittask->priv;
    GwyFitter *fitter = priv->fitter;
    if (!fitter)
        return -1.0;
    gdouble res = gwy_fitter_get_residuum(fitter);
    return (res < 0.0) ? res : sqrt(res/(priv->ndata - priv->nparam));
}

#define __LIBGWY_FIT_TASK_C__
#include "libgwy/libgwy-aliases.c"

/**
 * SECTION: fit-task
 * @title: GwyFitTask
 * @short_description: Non-linear least-squares fitter model and data
 *
 * A fitting task consists of the model function, the data to fit and the
 * set of parameters.  The model function can be defined in three independent
 * manners, listed by increasing abstractness, power and difficulty to use:
 * <itemizedlist>
 *   <listitem>
 *     Scalar-valued one-dimensional function (called point function), i.e.
 *     function that takes the abscissa value and parameters and it calculates
 *     the theoretical value. The data must be represented by #GwyXYs.
 *     This is everything that needs to be supplied.  The calculation of
 *     differences between theoretical and fitted data, derivatives, gradients
 *     and Hessians is done by #GwyFitTask.  Fixed parameters are also handled
 *     by #GwyFitTask.  Optionally, a weighting function can be set.
 *   </listitem>
 *   <listitem>
 *     Scalar-valued function (called indexed-data function) that takes an
 *     integer index and parameters and it calculates the weighted difference
 *     between theoretical and fitted data.  The data is opaque for #GwyFitTask
 *     and it is possible to simulate vector-valued functions by mapping
 *     several indices to one actual data point.  The calculation of
 *     derivatives, gradients and Hessians is done by #GwyFitTask.  Fixed
 *     parameters are also handled by #GwyFitTask.
 *   </listitem>
 *   <listitem>
 *     Scalar-valued function that takes an integer index and array of
 *     parameters and it calculates the weighted difference between theoretical
 *     and fitted data.  It can be coupled with a function calculating the
 *     derivatives by parameters. The data is opaque for #GwyFitTask and it is
 *     possible to simulate vector-valued functions by mapping several indices
 *     to one actual data point.  The calculation of derivatives (optionally),
 *     gradients and Hessians is done by #GwyFitTask.
 *   </listitem>
 * </itemizedlist>
 *
 * If weighting is used, note the differences is weighted, not their squares.
 * Hence in the usual case weights are the inverse standard deviations of the
 * fitted data points (unsquared).
 *
 * Since #GwyFitTask supplies its own evaluation methods and controls the
 * number of parameters, the low-level #GwyFitter setup methods
 * gwy_fitter_set_functions() and gwy_fitter_set_n_params() must not be used
 * on its fitter obtained with gwy_fit_task_get_fitter().
 * Neither you can use gwy_fitter_fit() and gwy_fitter_eval_residuum() as you
 * cannot pass the correct @user_data; use gwy_fit_task_fit() and
 * gwy_fit_task_eval_residuum() instead of them.
 *
 * A simple example demonstrating how to fit a shifted Gaussian without
 * weighting:
 * |[
 * // The function to fit: 4-parameter shifted Gaussian
 * gboolean
 * gaussian(gdouble x, gdouble *retval,
 *          gdouble xoff, gdouble yoff, gdouble b, gdouble a)
 * {
 *     x = (x - xoff)/b;
 *     *retval = yoff + a*exp(-x*x);
 *     // Width b must not be 0
 *     return b != 0.0;
 * }
 *
 * // Fit data using provided initial parameter estimate
 * gboolean
 * fit_gaussian(const GwyXY *data, guint ndata, gdouble *param)
 * {
 *     GwyFitTask *fittask = gwy_fit_task_new();
 *     GwyFitter *fitter = gwy_fit_task_get_fitter(fittask);
 *
 *     gwy_fit_task_set_point_function(fittask, 4, (GwyFitTaskPointFunc)gaussian);
 *     gwy_fitter_set_params(fitter, param);
 *     gwy_fit_task_set_point_data(fittask, data, ndata);
 *     if (!gwy_fit_task_fit(fittask))
 *         return FALSE;
 *     gwy_fitter_get_params(fitter, param);
 *     return TRUE;
 * }
 * ]|
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
 * Type of point model function for fitting tasks.
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
 * GwyFitTaskPointWeightFunc:
 * @x: Abscissa value to calculate the weight value in.
 *
 * Type of point weight function for fitting tasks.
 *
 * Returns: The weight.  Usually the inverse of the standard deviation is used.
 **/

/**
 * GwyFitTaskVectorFunc:
 * @i: Index of data point to calculate the function value in.
 * @user_data: Data set in gwy_fit_task_set_vector_data().
 * @retval: Location to store the difference between the function value in the
 *          @i-th point and the @i-th fitter data value to.
 * @...: Function parameters.
 *
 * Type of indexed-data model function for fitting tasks.
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
 * GwyFitTaskVectorVFunc:
 * @i: Index of data point to calculate the function value in.
 * @user_data: Data set in gwy_fit_task_set_vector_data().
 * @retval: Location to store the difference between the function value in the
 *          @i-th point and the @i-th fitter data value to.
 * @params: Array of length @nparams holding the parameters.
 *
 * Type of indexed-data model function with parameter array for fitting tasks.
 *
 * Returns: %TRUE if the calculation succeeded and @reval was set, %FALSE on
 *          failure.
 **/

/**
 * GwyFitTaskVectorDFunc:
 * @i: Index of data point to calculate the function value in.
 * @user_data: Data set in gwy_fit_task_set_vector_data().
 * @fixed_params: Array of length @nparams with %TRUE for fixed parameters,
 *                %FALSE for free parameters.
 * @diff: Array of length @nparam to fill with function derivatives (weighted
 *        appropriately, if applicable) in the @i-th point.
 * @params: Array of length @nparams holding the parameters.  It is writable
 *          to avoid temporary allocations.  However, when the function returns
 *          it must hold the same values as when it was called.
 *
 * Type of indexed-data model differentiation function with parameter array for
 * fitting tasks.
 *
 * Returns: %TRUE if the calculation succeeded and @reval was set, %FALSE on
 *          failure.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
