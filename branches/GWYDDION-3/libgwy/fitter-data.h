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

#ifndef __LIBGWY_FITTER_DATA_H__
#define __LIBGWY_FITTER_DATA_H__

#include <glib-object.h>
#include <libgwy/math.h>
#include <libgwy/fitter.h>

G_BEGIN_DECLS

typedef gboolean (*GwyFitterPointFunc)(gdouble x,
                                       gdouble *retval,
                                       ...);

typedef gdouble (*GwyFitterPointWeightFunc)(gdouble x,
                                            ...);

typedef gboolean (*GwyFitterVectorFunc)(guint i,
                                        gpointer user_data,
                                        gdouble *retval,
                                        ...);

typedef gboolean (*GwyFitterVectorVFunc)(guint i,
                                         gpointer user_data,
                                         gdouble *retval,
                                         const gdouble *params);

typedef gboolean (*GwyFitterVectorDFunc)(guint i,
                                         gpointer user_data,
                                         const gboolean *fixed,
                                         gdouble *diff,
                                         const gdouble *params);

#define GWY_TYPE_FITTER_DATA \
    (gwy_fitter_data_get_type())
#define GWY_FITTER_DATA(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_FITTER_DATA, GwyFitterData))
#define GWY_FITTER_DATA_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_FITTER_DATA, GwyFitterDataClass))
#define GWY_IS_FITTER_DATA(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_FITTER_DATA))
#define GWY_IS_FITTER_DATA_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_FITTER_DATA))
#define GWY_FITTER_DATA_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_FITTER_DATA, GwyFitterDataClass))

typedef struct _GwyFitterData      GwyFitterData;
typedef struct _GwyFitterDataClass GwyFitterDataClass;

struct _GwyFitterData {
    GObject g_object;
};

struct _GwyFitterDataClass {
    GObjectClass g_object_class;
};

GType          gwy_fitter_data_get_type            (void)                             G_GNUC_CONST;
GwyFitterData* gwy_fitter_data_new                 (void)                             G_GNUC_MALLOC;
void           gwy_fitter_data_set_point_function  (GwyFitterData *fitterdata,
                                                    guint nparams,
                                                    GwyFitterPointFunc function);
void           gwy_fitter_data_set_point_weight    (GwyFitterData *fitterdata,
                                                    GwyFitterPointWeightFunc weight);
void           gwy_fitter_data_set_point_data      (GwyFitterData *fitterdata,
                                                    GwyPointXY *data,
                                                    guint ndata);
void           gwy_fitter_data_set_vector_function (GwyFitterData *fitterdata,
                                                    guint nparams,
                                                    GwyFitterVectorFunc function);
void           gwy_fitter_data_set_vector_vfunction(GwyFitterData *fitterdata,
                                                    guint nparams,
                                                    GwyFitterVectorVFunc function,
                                                    GwyFitterVectorDFunc derivative);
void           gwy_fitter_data_set_vector_data     (GwyFitterData *fitterdata,
                                                    guint ndata,
                                                    gpointer user_data);

/*
gboolean   gwy_fitter_data_get_param_errors(GwyFitterData *fitterdata,
                                            gdouble *errors);
gboolean   gwy_fitter_data_get_covariance_matrix(GwyFitterData *fitterdata,
                                                 gdouble *covar,
                                                 gboolean variance_covariance);
gdouble    gwy_fitter_data_get_chi(GwyFitterData *fitterdata);
*/

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

