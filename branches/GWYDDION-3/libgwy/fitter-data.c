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
#include "libgwy/fitter-data.h"
#include "libgwy/math.h"
#include "libgwy/types.h"
#include "libgwy/libgwy-aliases.h"

#define SLi gwy_lower_triangular_matrix_index
#define MATRIX_LEN gwy_triangular_matrix_length
#define ASSIGN(p, q, n) memcpy((p), (q), (n)*sizeof(gdouble))

#define GWY_FITTER_DATA_GET_PRIVATE(o)  \
   (G_TYPE_INSTANCE_GET_PRIVATE((o), GWY_TYPE_FITTER_DATA, GwyFitterDataPrivate))

enum {
    VARARG_PARAM_MAX = 5
};

typedef enum {
    NONE = 0,
    POINT_VARARG,
    VECTOR_VARARG,
    VECTOR_ARRAY
} GwyFitterInterfaceType;

typedef gboolean (*GwyFitterPointFunc1)(gdouble x,
                                        gdouble *retval,
                                        gdouble p0);
typedef gboolean (*GwyFitterPointFunc2)(gdouble x,
                                        gdouble *retval,
                                        gdouble p0,
                                        gdouble p1);
typedef gboolean (*GwyFitterPointFunc3)(gdouble x,
                                        gdouble *retval,
                                        gdouble p0,
                                        gdouble p1,
                                        gdouble p2);
typedef gboolean (*GwyFitterPointFunc4)(gdouble x,
                                        gdouble *retval,
                                        gdouble p0,
                                        gdouble p1,
                                        gdouble p2,
                                        gdouble p3);
typedef gboolean (*GwyFitterPointFunc5)(gdouble x,
                                        gdouble *retval,
                                        gdouble p0,
                                        gdouble p1,
                                        gdouble p2,
                                        gdouble p3,
                                        gdouble p4);
typedef gboolean (*GwyFitterPointFunc6)(gdouble x,
                                        gdouble *retval,
                                        gdouble p0,
                                        gdouble p1,
                                        gdouble p2,
                                        gdouble p3,
                                        gdouble p4,
                                        gdouble p5);

typedef gdouble (*GwyFitterPointWeightFunc1)(gdouble x,
                                             gdouble p0);
typedef gdouble (*GwyFitterPointWeightFunc2)(gdouble x,
                                             gdouble p0,
                                             gdouble p1);
typedef gdouble (*GwyFitterPointWeightFunc3)(gdouble x,
                                             gdouble p0,
                                             gdouble p1,
                                             gdouble p2);
typedef gdouble (*GwyFitterPointWeightFunc4)(gdouble x,
                                             gdouble p0,
                                             gdouble p1,
                                             gdouble p2,
                                             gdouble p3);
typedef gdouble (*GwyFitterPointWeightFunc5)(gdouble x,
                                             gdouble p0,
                                             gdouble p1,
                                             gdouble p2,
                                             gdouble p3,
                                             gdouble p4);
typedef gdouble (*GwyFitterPointWeightFunc6)(gdouble x,
                                             gdouble p0,
                                             gdouble p1,
                                             gdouble p2,
                                             gdouble p3,
                                             gdouble p4,
                                             gdouble p5);

typedef gboolean (*GwyFitterVectorFunc1)(guint i,
                                         gpointer user_data,
                                         gdouble *retval,
                                         gdouble p0);
typedef gboolean (*GwyFitterVectorFunc2)(guint i,
                                         gpointer user_data,
                                         gdouble *retval,
                                         gdouble p0,
                                         gdouble p1);
typedef gboolean (*GwyFitterVectorFunc3)(guint i,
                                         gpointer user_data,
                                         gdouble *retval,
                                         gdouble p0,
                                         gdouble p1,
                                         gdouble p2);
typedef gboolean (*GwyFitterVectorFunc4)(guint i,
                                         gpointer user_data,
                                         gdouble *retval,
                                         gdouble p0,
                                         gdouble p1,
                                         gdouble p2,
                                         gdouble p3);
typedef gboolean (*GwyFitterVectorFunc5)(guint i,
                                         gpointer user_data,
                                         gdouble *retval,
                                         gdouble p0,
                                         gdouble p1,
                                         gdouble p2,
                                         gdouble p3,
                                         gdouble p4);
typedef gboolean (*GwyFitterVectorFunc6)(guint i,
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
    GwyFitterInterfaceType type;
    guint nparam;
    guint ndata;
    gboolean *fixed_param;
    gdouble *h;
    gdouble *mparam;
    gdouble *diff;
    /* Point interface */
    GwyFitterPointFunc point_func;
    GwyFitterPointWeightFunc point_weight;
    GwyPointXY *point_data;
    /* Vector interface */
    GwyFitterVectorFunc vector_func;
    GwyFitterVectorVFunc vector_vfunc;
    GwyFitterVectorDFunc vector_dfunc;
    gpointer vector_data;
} FitterData;

typedef FitterData GwyFitterDataPrivate;

static void     gwy_fitter_data_finalize(GObject *object);
static void     gwy_fitter_data_dispose (GObject *object);
static void     set_n_params            (FitterData *fitterdata,
                                         guint nparam);
static gboolean fitter_data_residuum    (const gdouble *param,
                                         gdouble *residuum,
                                         gpointer user_data);
static gboolean fitter_data_gradient    (const gdouble *param,
                                         gdouble *gradient,
                                         gdouble *hessian,
                                         gpointer user_data);

G_DEFINE_TYPE(GwyFitterData, gwy_fitter_data, G_TYPE_OBJECT)

static void
gwy_fitter_data_class_init(GwyFitterDataClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    g_type_class_add_private(klass, sizeof(GwyFitterDataPrivate));

    gobject_class->finalize = gwy_fitter_data_finalize;
    gobject_class->dispose = gwy_fitter_data_dispose;
}

GwyFitterData*
gwy_fitter_data_new(void)
{
    return g_object_newv(GWY_TYPE_FITTER_DATA, 0, NULL);
}

static void
gwy_fitter_data_init(GwyFitterData *object)
{
    G_GNUC_UNUSED FitterData *fitterdata = GWY_FITTER_DATA_GET_PRIVATE(object);
}

static void
gwy_fitter_data_finalize(GObject *object)
{
    FitterData *fitterdata = GWY_FITTER_DATA_GET_PRIVATE(object);
    set_n_params(fitterdata, 0);
    G_OBJECT_CLASS(gwy_fitter_data_parent_class)->finalize(object);
}

static void
gwy_fitter_data_dispose(GObject *object)
{
    FitterData *fitterdata = GWY_FITTER_DATA_GET_PRIVATE(object);
    GWY_OBJECT_UNREF(fitterdata->fitter);
    G_OBJECT_CLASS(gwy_fitter_data_parent_class)->dispose(object);
}

guint
gwy_fitter_data_get_max_vararg_params(void)
{
    return VARARG_PARAM_MAX;
}

static void
ensure_fitter(FitterData *fitterdata)
{
    if (fitterdata->fitter)
        return;

    fitterdata->fitter = gwy_fitter_new();
    gwy_fitter_set_functions(fitterdata->fitter,
                             fitter_data_residuum, fitter_data_gradient);
}

/* The values would not confuse us when using the current interface but we
 * do not want any leftovers if someone switches interfaces back and forth. */
static void
invalidate_point_interface(FitterData *fitterdata)
{
    fitterdata->point_func = NULL;
    fitterdata->point_weight = NULL;
    if (fitterdata->point_data) {
        fitterdata->ndata = 0;
        fitterdata->point_data = NULL;
    }
}

static void
invalidate_vector_interface(FitterData *fitterdata)
{
    fitterdata->vector_func = NULL;
    fitterdata->vector_vfunc = NULL;
    fitterdata->vector_dfunc = NULL;
    if (fitterdata->vector_data) {
        fitterdata->ndata = 0;
        fitterdata->vector_data = NULL;
    }
}

static void
set_n_params(FitterData *fitterdata,
             guint nparam)
{
    fitterdata->fixed_param = g_renew(gboolean, fitterdata->fixed_param,
                                      nparam);
    fitterdata->h = g_renew(gdouble, fitterdata->h, 3*nparam);
    fitterdata->mparam = fitterdata->h + nparam;
    fitterdata->diff = fitterdata->mparam + nparam;
    if (nparam)
        gwy_memclear(fitterdata->fixed_param, nparam);
    fitterdata->nparam = nparam;

    ensure_fitter(fitterdata);
    gwy_fitter_set_n_params(fitterdata->fitter, nparam);
}

void
gwy_fitter_data_set_point_function(GwyFitterData *object,
                                   guint nparams,
                                   GwyFitterPointFunc function)
{
    g_return_if_fail(GWY_IS_FITTER_DATA(object));
    g_return_if_fail(nparams <= VARARG_PARAM_MAX);
    FitterData *fitterdata = GWY_FITTER_DATA_GET_PRIVATE(object);

    invalidate_vector_interface(fitterdata);
    fitterdata->type = POINT_VARARG;
    set_n_params(fitterdata, nparams);
    fitterdata->point_func = function;
}

void
gwy_fitter_data_set_point_weight(GwyFitterData *object,
                                 GwyFitterPointWeightFunc weight)
{
    g_return_if_fail(GWY_IS_FITTER_DATA(object));
    FitterData *fitterdata = GWY_FITTER_DATA_GET_PRIVATE(object);

    invalidate_vector_interface(fitterdata);
    g_return_if_fail(fitterdata->type == POINT_VARARG);
    fitterdata->point_weight = weight;
}

void
gwy_fitter_data_set_point_data(GwyFitterData *object,
                               GwyPointXY *data,
                               guint ndata)
{
    g_return_if_fail(GWY_IS_FITTER_DATA(object));
    FitterData *fitterdata = GWY_FITTER_DATA_GET_PRIVATE(object);

    invalidate_vector_interface(fitterdata);
    g_return_if_fail(fitterdata->type == POINT_VARARG);
    fitterdata->ndata = ndata;
    fitterdata->point_data = data;
}

void
gwy_fitter_data_set_vector_function(GwyFitterData *object,
                                    guint nparams,
                                    GwyFitterVectorFunc function)
{
    g_return_if_fail(GWY_IS_FITTER_DATA(object));
    g_return_if_fail(nparams <= VARARG_PARAM_MAX);
    FitterData *fitterdata = GWY_FITTER_DATA_GET_PRIVATE(object);

    invalidate_point_interface(fitterdata);
    fitterdata->vector_vfunc = NULL;
    fitterdata->vector_dfunc = NULL;
    fitterdata->type = VECTOR_VARARG;
    set_n_params(fitterdata, nparams);
    fitterdata->vector_func = function;
}

void
gwy_fitter_data_set_vector_vfunction(GwyFitterData *object,
                                     guint nparams,
                                     GwyFitterVectorVFunc function,
                                     GwyFitterVectorDFunc derivative)
{
    g_return_if_fail(GWY_IS_FITTER_DATA(object));
    FitterData *fitterdata = GWY_FITTER_DATA_GET_PRIVATE(object);

    invalidate_point_interface(fitterdata);
    fitterdata->vector_func = NULL;
    fitterdata->type = VECTOR_ARRAY;
    set_n_params(fitterdata, nparams);
    fitterdata->vector_vfunc = function;
    fitterdata->vector_dfunc = derivative;
}

void
gwy_fitter_data_set_vector_data(GwyFitterData *object,
                                guint ndata,
                                gpointer user_data)
{
    g_return_if_fail(GWY_IS_FITTER_DATA(object));
    FitterData *fitterdata = GWY_FITTER_DATA_GET_PRIVATE(object);

    invalidate_point_interface(fitterdata);
    g_return_if_fail(fitterdata->type == VECTOR_VARARG
                     || fitterdata->type == VECTOR_ARRAY);
    fitterdata->ndata = ndata;
    fitterdata->vector_data = user_data;
}

void
gwy_fitter_data_set_fixed_params(GwyFitterData *object,
                                 const gboolean *fixed_params)
{
    g_return_if_fail(GWY_IS_FITTER_DATA(object));
    FitterData *fitterdata = GWY_FITTER_DATA_GET_PRIVATE(object);
    memcpy(fitterdata->fixed_param, fixed_params,
           fitterdata->nparam*sizeof(gboolean));
}

void
gwy_fitter_data_get_fixed_params(GwyFitterData *object,
                                 gboolean *fixed_params)
{
    g_return_if_fail(GWY_IS_FITTER_DATA(object));
    FitterData *fitterdata = GWY_FITTER_DATA_GET_PRIVATE(object);
    memcpy(fixed_params, fitterdata->fixed_param,
           fitterdata->nparam*sizeof(gboolean));
}

void
gwy_fitter_data_set_fixed_param(GwyFitterData *object,
                                guint i,
                                gboolean fixed)
{
    g_return_if_fail(GWY_IS_FITTER_DATA(object));
    FitterData *fitterdata = GWY_FITTER_DATA_GET_PRIVATE(object);
    g_return_if_fail(i < fitterdata->nparam);
    fitterdata->fixed_param[i] = fixed;
}

gboolean
gwy_fitter_data_get_fixed_param(GwyFitterData *object,
                                guint i)
{
    g_return_val_if_fail(GWY_IS_FITTER_DATA(object), FALSE);
    FitterData *fitterdata = GWY_FITTER_DATA_GET_PRIVATE(object);
    g_return_val_if_fail(i < fitterdata->nparam, FALSE);
    return fitterdata->fixed_param[i];
}

static inline gboolean
eval_point_vararg(gdouble x, const gdouble *param, guint nparam,
                  GwyFitterPointFunc func,
                  gdouble *retval)
{
    if (nparam == 1)
        return ((GwyFitterPointFunc1)func)(x, retval, param[0]);
    if (nparam == 2)
        return ((GwyFitterPointFunc2)func)(x, retval, param[0], param[1]);
    if (nparam == 3)
        return ((GwyFitterPointFunc3)func)(x, retval, param[0], param[1],
                                           param[2]);
    if (nparam == 4)
        return ((GwyFitterPointFunc4)func)(x, retval, param[0], param[1],
                                           param[2], param[3]);
    if (nparam == 5)
        return ((GwyFitterPointFunc5)func)(x, retval, param[0], param[1],
                                           param[2], param[3], param[5]);
    if (nparam == 6)
        return ((GwyFitterPointFunc6)func)(x, retval, param[0], param[1],
                                           param[2], param[3], param[5],
                                           param[6]);

    g_return_val_if_reached(FALSE);
}

static inline gboolean
eval_vector_vararg(guint i, gpointer data, const gdouble *param, guint nparam,
                   GwyFitterVectorFunc func,
                   gdouble *retval)
{
    if (nparam == 1)
        return ((GwyFitterVectorFunc1)func)(i, data, retval, param[0]);
    if (nparam == 2)
        return ((GwyFitterVectorFunc2)func)(i, data, retval, param[0],
                                            param[1]);
    if (nparam == 3)
        return ((GwyFitterVectorFunc3)func)(i, data, retval, param[0],
                                            param[1], param[2]);
    if (nparam == 4)
        return ((GwyFitterVectorFunc4)func)(i, data, retval, param[0],
                                            param[1], param[2], param[3]);
    if (nparam == 5)
        return ((GwyFitterVectorFunc5)func)(i, data, retval, param[0],
                                            param[1], param[2], param[3],
                                            param[5]);
    if (nparam == 6)
        return ((GwyFitterVectorFunc6)func)(i, data, retval, param[0],
                                            param[1], param[2], param[3],
                                            param[5], param[6]);

    g_return_val_if_reached(FALSE);
}

static gboolean
fitter_data_residuum(const gdouble *param,
                     gdouble *residuum,
                     gpointer user_data)
{
    FitterData *fitterdata = (FitterData*)user_data;
    guint nparam = fitterdata->nparam;
    gdouble r = 0.0;

    /* TODO: Weights */
    if (fitterdata->type == POINT_VARARG) {
        g_return_val_if_fail(fitterdata->point_func, FALSE);
        g_return_val_if_fail(fitterdata->nparam <= VARARG_PARAM_MAX, FALSE);
        GwyFitterPointFunc func = fitterdata->point_func;
        GwyPointXY *pts = fitterdata->point_data;

        for (guint i = 0; i < fitterdata->ndata; i++) {
            gdouble x = pts[i].x, y = pts[i].y, v;
            if (!eval_point_vararg(x, param, nparam, func, &v))
                return FALSE;
            v -= y;
            r += v*v;
        }
    }
    else if (fitterdata->type == VECTOR_VARARG) {
        g_return_val_if_fail(fitterdata->vector_func, FALSE);
        g_return_val_if_fail(fitterdata->nparam <= VARARG_PARAM_MAX, FALSE);
        GwyFitterVectorFunc func = fitterdata->vector_func;
        gpointer data = fitterdata->vector_data;

        for (guint i = 0; i < fitterdata->ndata; i++) {
            gdouble v;
            if (!eval_vector_vararg(i, data, param, nparam, func, &v))
                return FALSE;
            r += v*v;
        }
    }
    else if (fitterdata->type == VECTOR_ARRAY) {
        g_return_val_if_fail(fitterdata->vector_vfunc, FALSE);
        GwyFitterVectorVFunc vfunc = fitterdata->vector_vfunc;
        gpointer data = fitterdata->vector_data;

        for (guint i = 0; i < fitterdata->ndata; i++) {
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
fitter_data_gradient(const gdouble *param,
                     gdouble *gradient,
                     gdouble *hessian,
                     gpointer user_data)
{
    FitterData *fitterdata = (FitterData*)user_data;
    guint nparam = fitterdata->nparam;
    const gboolean *fixed_param = fitterdata->fixed_param;
    gdouble *h = fitterdata->h;
    gdouble *mparam = fitterdata->mparam;
    gdouble *diff = fitterdata->diff;
    gwy_memclear(gradient, nparam);
    gwy_memclear(hessian, MATRIX_LEN(nparam));

    ASSIGN(mparam, param, nparam);
    for (guint j = 0; j < nparam; j++)
        h[j] = param[j] ? 1e-5*fabs(param[j]) : 1e-9;

    /* TODO: Weights */
    if (fitterdata->type == POINT_VARARG) {
        g_return_val_if_fail(fitterdata->point_func, FALSE);
        g_return_val_if_fail(nparam <= VARARG_PARAM_MAX, FALSE);
        GwyFitterPointFunc func = fitterdata->point_func;
        GwyPointXY *pts = fitterdata->point_data;

        for (guint i = 0; i < fitterdata->ndata; i++) {
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
    else if (fitterdata->type == VECTOR_VARARG) {
        g_return_val_if_fail(fitterdata->vector_func, FALSE);
        g_return_val_if_fail(nparam <= VARARG_PARAM_MAX, FALSE);
        GwyFitterVectorFunc func = fitterdata->vector_func;
        gpointer data = fitterdata->vector_data;

        for (guint i = 0; i < fitterdata->ndata; i++) {
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
    else if (fitterdata->type == VECTOR_ARRAY) {
        g_return_val_if_fail(fitterdata->vector_vfunc, FALSE);
        g_return_val_if_fail(fitterdata->vector_dfunc, FALSE);
        GwyFitterVectorVFunc vfunc = fitterdata->vector_vfunc;
        GwyFitterVectorDFunc dfunc = fitterdata->vector_dfunc;
        gpointer data = fitterdata->vector_data;

        for (guint i = 0; i < fitterdata->ndata; i++) {
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

gboolean
gwy_fitter_data_fit(GwyFitterData *object)
{
    g_return_val_if_fail(GWY_IS_FITTER_DATA(object), FALSE);
    FitterData *fitterdata = GWY_FITTER_DATA_GET_PRIVATE(object);
    ensure_fitter(fitterdata);
    return gwy_fitter_fit(fitterdata->fitter, fitterdata);
}

gdouble
gwy_fitter_data_eval_residuum(GwyFitterData *object)
{
    g_return_val_if_fail(GWY_IS_FITTER_DATA(object), FALSE);
    FitterData *fitterdata = GWY_FITTER_DATA_GET_PRIVATE(object);
    ensure_fitter(fitterdata);
    return gwy_fitter_eval_residuum(fitterdata->fitter, fitterdata);
}

GwyFitter*
gwy_fitter_data_get_fitter(GwyFitterData *object)
{
    g_return_val_if_fail(GWY_IS_FITTER_DATA(object), NULL);
    FitterData *fitterdata = GWY_FITTER_DATA_GET_PRIVATE(object);
    ensure_fitter(fitterdata);
    return fitterdata->fitter;
}

#define __LIBGWY_FITTER_DATA_C__
#include "libgwy/libgwy-aliases.c"

/**
 * SECTION: fitter-data
 * @title: GwyFitterData
 * @short_description: Data wrapper for non-linear least-squares fitter
 **/

/**
 * GwyFitterData:
 *
 * Object representing non-linear least-squares fitter data.
 *
 * The #GwyFitterData struct contains private data only and should be accessed
 * using the functions below.
 **/

/**
 * GwyFitterDataClass:
 * @g_object_class: Parent class.
 *
 * Class of non-linear least-squares fitter data.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
