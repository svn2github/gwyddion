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

#ifdef __LIBGWY_ALIASES_H__
#error Public headers must be included before the aliasing header.
#endif

#ifndef __LIBGWY_FITTER_H__
#define __LIBGWY_FITTER_H__

#include <glib-object.h>

G_BEGIN_DECLS

typedef enum {
    GWY_FITTER_STATUS_NONE             = 0,
    GWY_FITTER_STATUS_FUNCTION_FAILURE = 1,
    GWY_FITTER_STATUS_GRADIENT_FAILURE = 2,
    GWY_FITTER_STATUS_SILENT_FAILURE   = 3,
    GWY_FITTER_STATUS_MAX_ITER         = 4,
    GWY_FITTER_STATUS_LAMBDA_OVERFLOW  = 5,
    GWY_FITTER_STATUS_PARAM_OFF_BOUNDS = 6,
    GWY_FITTER_STATUS_TOO_SMALL_CHANGE = 7,
    GWY_FITTER_STATUS_CANNOT_STEP      = 8,
} GwyFitterStatus;

typedef gboolean (*GwyFitterResiduumFunc)(const gdouble *param,
                                          gdouble *residuum,
                                          gpointer user_data);

typedef gboolean (*GwyFitterGradientFunc)(const gdouble *param,
                                          gdouble *gradient,
                                          gdouble *hessian,
                                          gpointer user_data);

typedef gboolean (*GwyFitterConstrainFunc)(const gdouble *param,
                                           gboolean *ok,
                                           gpointer user_data);

#define GWY_TYPE_FITTER \
    (gwy_fitter_get_type())
#define GWY_FITTER(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_FITTER, GwyFitter))
#define GWY_FITTER_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_FITTER, GwyFitterClass))
#define GWY_IS_FITTER(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_FITTER))
#define GWY_IS_FITTER_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_FITTER))
#define GWY_FITTER_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_FITTER, GwyFitterClass))

typedef struct _GwyFitter      GwyFitter;
typedef struct _GwyFitterClass GwyFitterClass;

struct _GwyFitter {
    GObject g_object;
    struct _GwyFitterPrivate *priv;
};

struct _GwyFitterClass {
    /*<private>*/
    GObjectClass g_object_class;
};

GType      gwy_fitter_get_type           (void)                                 G_GNUC_CONST;
GwyFitter* gwy_fitter_new                (void)                                 G_GNUC_MALLOC;
void       gwy_fitter_set_n_params       (GwyFitter *fitter,
                                          guint nparams);
guint      gwy_fitter_get_n_params       (GwyFitter *fitter)                    G_GNUC_PURE;
void       gwy_fitter_set_params         (GwyFitter *fitter,
                                          const gdouble *params);
gboolean   gwy_fitter_get_params         (GwyFitter *fitter,
                                          gdouble *params);
void       gwy_fitter_set_functions      (GwyFitter *fitter,
                                          GwyFitterResiduumFunc eval_residuum,
                                          GwyFitterGradientFunc eval_gradient);
void       gwy_fitter_set_constraint     (GwyFitter *fitter,
                                          GwyFitterConstrainFunc constrain);
gdouble    gwy_fitter_get_lambda         (GwyFitter *fitter)                    G_GNUC_PURE;
guint      gwy_fitter_get_iter           (GwyFitter *fitter)                    G_GNUC_PURE;
gboolean   gwy_fitter_fit                (GwyFitter *fitter,
                                          gpointer user_data);
guint      gwy_fitter_get_status         (GwyFitter *fitter)                    G_GNUC_PURE;
gdouble    gwy_fitter_get_residuum       (GwyFitter *fitter)                    G_GNUC_PURE;
gdouble    gwy_fitter_eval_residuum      (GwyFitter *fitter,
                                          gpointer user_data);
gboolean   gwy_fitter_get_inverse_hessian(GwyFitter *fitter,
                                          gdouble *ihessian);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
