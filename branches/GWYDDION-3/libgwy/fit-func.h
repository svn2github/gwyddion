/*
 *  $Id$
 *  Copyright (C) 2010 David Nečas (Yeti).
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

#ifndef __LIBGWY_FIT_FUNC_H__
#define __LIBGWY_FIT_FUNC_H__

#include <libgwy/user-fit-func.h>
#include <libgwy/fit-task.h>
#include <libgwy/unit.h>

G_BEGIN_DECLS

#define GWY_TYPE_FIT_FUNC \
    (gwy_fit_func_get_type())
#define GWY_FIT_FUNC(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_FIT_FUNC, GwyFitFunc))
#define GWY_FIT_FUNC_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_FIT_FUNC, GwyFitFuncClass))
#define GWY_IS_FIT_FUNC(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_FIT_FUNC))
#define GWY_IS_FIT_FUNC_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_FIT_FUNC))
#define GWY_FIT_FUNC_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_FIT_FUNC, GwyFitFuncClass))

typedef struct _GwyFitFunc      GwyFitFunc;
typedef struct _GwyFitFuncClass GwyFitFuncClass;

struct _GwyFitFunc {
    GObject g_object;
    struct _GwyFitFuncPrivate *priv;
};

struct _GwyFitFuncClass {
    /*<private>*/
    GObjectClass g_object_class;
};

GType           gwy_fit_func_get_type    (void)                      G_GNUC_CONST;
GwyFitFunc*     gwy_fit_func_new         (const gchar *name,
                                          const gchar *group)        G_GNUC_MALLOC;
const gchar*    gwy_fit_func_formula     (const GwyFitFunc *fitfunc) G_GNUC_PURE;
guint           gwy_fit_func_n_params    (const GwyFitFunc *fitfunc) G_GNUC_PURE;
const gchar*    gwy_fit_func_param_name  (const GwyFitFunc *fitfunc,
                                          guint i)                   G_GNUC_PURE;
guint           gwy_fit_func_param_number(const GwyFitFunc *fitfunc,
                                          const gchar *name)         G_GNUC_PURE;
GwyUnit*        gwy_fit_func_param_units (GwyFitFunc *fitfunc,
                                          guint i,
                                          GwyUnit *unit_x,
                                          GwyUnit *unit_y)           G_GNUC_MALLOC;
gboolean        gwy_fit_func_evaluate    (GwyFitFunc *fitfunc,
                                          gdouble x,
                                          const gdouble *params,
                                          gdouble *value);
gboolean        gwy_fit_func_estimate    (GwyFitFunc *fitfunc,
                                          gdouble *params);
GwyFitTask*     gwy_fit_func_get_fit_task(GwyFitFunc *fitfunc)       G_GNUC_PURE;
void            gwy_fit_func_set_data    (GwyFitFunc *fitfunc,
                                          const GwyXY *points,
                                          guint npoints);
const gchar*    gwy_fit_func_get_name    (const GwyFitFunc *fitfunc) G_GNUC_PURE;
const gchar*    gwy_fit_func_get_group   (const GwyFitFunc *fitfunc) G_GNUC_PURE;
GwyUserFitFunc* gwy_fit_func_get_user    (const GwyFitFunc *fitfunc) G_GNUC_PURE;

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
