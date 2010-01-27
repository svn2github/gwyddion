/*
 *  $Id$
 *  Copyright (C) 2010 David Necas (Yeti).
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

#ifndef __LIBGWY_FIT_FUNCTION_H__
#define __LIBGWY_FIT_FUNCTION_H__

#include <libgwy/user-fit-function.h>
#include <libgwy/fit-task.h>
#include <libgwy/unit.h>

G_BEGIN_DECLS

#define GWY_TYPE_FIT_FUNCTION \
    (gwy_fit_function_get_type())
#define GWY_FIT_FUNCTION(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_FIT_FUNCTION, GwyFitFunction))
#define GWY_FIT_FUNCTION_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_FIT_FUNCTION, GwyFitFunctionClass))
#define GWY_IS_FIT_FUNCTION(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_FIT_FUNCTION))
#define GWY_IS_FIT_FUNCTION_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_FIT_FUNCTION))
#define GWY_FIT_FUNCTION_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_FIT_FUNCTION, GwyFitFunctionClass))

typedef struct _GwyFitFunction      GwyFitFunction;
typedef struct _GwyFitFunctionClass GwyFitFunctionClass;

struct _GwyFitFunction {
    GObject g_object;
    struct _GwyFitFunctionPrivate *priv;
};

struct _GwyFitFunctionClass {
    /*< private >*/
    GObjectClass g_object_class;
};
GType           gwy_fit_function_get_type       (void)                        G_GNUC_CONST;
GwyFitFunction* gwy_fit_function_new            (const gchar *name)           G_GNUC_MALLOC;
gboolean        gwy_fit_function_get_value      (GwyFitFunction *fitfunction,
                                                 gdouble x,
                                                 const gdouble *params,
                                                 gdouble *value);
const gchar*    gwy_fit_function_get_formula    (GwyFitFunction *fitfunction) G_GNUC_PURE;
guint           gwy_fit_function_get_n_params   (GwyFitFunction *fitfunction) G_GNUC_PURE;
const gchar*    gwy_fit_function_get_param_name (GwyFitFunction *fitfunction,
                                                 guint i)                     G_GNUC_PURE;
GwyUnit*        gwy_fit_function_get_param_units(GwyFitFunction *fitfunction,
                                                 guint i,
                                                 GwyUnit *unit_x,
                                                 GwyUnit *unit_y)             G_GNUC_MALLOC;
gboolean        gwy_fit_function_estimate       (GwyFitFunction *fitfunction,
                                                 gdouble *params);
GwyFitTask*     gwy_fit_function_get_fit_task   (GwyFitFunction *fitfunction) G_GNUC_PURE;
void            gwy_fit_function_set_data       (GwyFitFunction *fitfunction,
                                                 const GwyXY *points,
                                                 guint npoints);

G_END_DECLS

#endif /*__GWY_FIT_FUNCTION_H__*/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
