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

#ifndef __LIBGWY_FIT_TASK_H__
#define __LIBGWY_FIT_TASK_H__

#include <glib-object.h>
#include <libgwy/math.h>
#include <libgwy/fitter.h>

G_BEGIN_DECLS

typedef gboolean (*GwyFitTaskPointFunc)(gdouble x,
                                        gdouble *retval,
                                        ...);

typedef gdouble (*GwyFitTaskPointWeightFunc)(gdouble x);

typedef gboolean (*GwyFitTaskVectorFunc)(guint i,
                                         gpointer user_data,
                                         gdouble *retval,
                                         ...);

typedef gboolean (*GwyFitTaskVectorVFunc)(guint i,
                                          gpointer user_data,
                                          gdouble *retval,
                                          const gdouble *params);

typedef gboolean (*GwyFitTaskVectorDFunc)(guint i,
                                          gpointer user_data,
                                          const gboolean *fixed_params,
                                          gdouble *diff,
                                          gdouble *params);

#define GWY_TYPE_FIT_TASK \
    (gwy_fit_task_get_type())
#define GWY_FIT_TASK(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_FIT_TASK, GwyFitTask))
#define GWY_FIT_TASK_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_FIT_TASK, GwyFitTaskClass))
#define GWY_IS_FIT_TASK(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_FIT_TASK))
#define GWY_IS_FIT_TASK_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_FIT_TASK))
#define GWY_FIT_TASK_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_FIT_TASK, GwyFitTaskClass))

typedef struct _GwyFitTask      GwyFitTask;
typedef struct _GwyFitTaskClass GwyFitTaskClass;

struct _GwyFitTask {
    GObject g_object;
    struct _GwyFitTaskPrivate *priv;
};

struct _GwyFitTaskClass {
    GObjectClass g_object_class;
};

guint       gwy_fit_task_get_max_vararg_params(void);
GType       gwy_fit_task_get_type             (void)                              G_GNUC_CONST;
GwyFitTask* gwy_fit_task_new                  (void)                              G_GNUC_MALLOC;
void        gwy_fit_task_set_point_function   (GwyFitTask *fittask,
                                               guint nparams,
                                               GwyFitTaskPointFunc function);
void        gwy_fit_task_set_point_weight     (GwyFitTask *fittask,
                                               GwyFitTaskPointWeightFunc weight);
void        gwy_fit_task_set_point_data       (GwyFitTask *fittask,
                                               const GwyPointXY *data,
                                               guint ndata);
void        gwy_fit_task_set_vector_function  (GwyFitTask *fittask,
                                               guint nparams,
                                               GwyFitTaskVectorFunc function);
void        gwy_fit_task_set_vector_vfunction (GwyFitTask *fittask,
                                               guint nparams,
                                               GwyFitTaskVectorVFunc function,
                                               GwyFitTaskVectorDFunc derivative);
void        gwy_fit_task_set_vector_data      (GwyFitTask *fittask,
                                               gpointer user_data,
                                               guint ndata);
void        gwy_fit_task_set_fixed_params     (GwyFitTask *fittask,
                                               const gboolean *fixed_params);
guint       gwy_fit_task_get_fixed_params     (GwyFitTask *fittask,
                                               gboolean *fixed_params);
void        gwy_fit_task_set_fixed_param      (GwyFitTask *fittask,
                                               guint i,
                                               gboolean fixed);
gboolean    gwy_fit_task_get_fixed_param      (GwyFitTask *fittask,
                                               guint i)                           G_GNUC_PURE;
gboolean    gwy_fit_task_fit                  (GwyFitTask *object);
gdouble     gwy_fit_task_eval_residuum        (GwyFitTask *object);
GwyFitter*  gwy_fit_task_get_fitter           (GwyFitTask *object)                G_GNUC_PURE;
gboolean    gwy_fit_task_get_param_errors     (GwyFitTask *fittask,
                                               gboolean variance_covariance,
                                               gdouble *errors);
gdouble     gwy_fit_task_get_param_error      (GwyFitTask *fittask,
                                               guint i,
                                               gboolean variance_covariance)      G_GNUC_PURE;
gboolean    gwy_fit_task_get_correlations     (GwyFitTask *fittask,
                                               gdouble *corr_matrix);
gdouble     gwy_fit_task_get_chi              (GwyFitTask *fittask)               G_GNUC_PURE;

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

