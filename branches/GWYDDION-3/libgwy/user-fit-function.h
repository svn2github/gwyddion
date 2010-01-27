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

#ifndef __LIBGWY_USER_FIT_FUNCTION_H__
#define __LIBGWY_USER_FIT_FUNCTION_H__

#include <libgwy/resource.h>

G_BEGIN_DECLS

typedef struct {
    gchar *name;
    gchar *estimate;
    gint power_x;
    gint power_y;
} GwyUserFitFunctionParam;

#define GWY_TYPE_USER_FIT_FUNCTION \
    (gwy_user_fit_function_get_type())
#define GWY_USER_FIT_FUNCTION(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_USER_FIT_FUNCTION, GwyUserFitFunction))
#define GWY_USER_FIT_FUNCTION_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_USER_FIT_FUNCTION, GwyUserFitFunctionClass))
#define GWY_IS_USER_FIT_FUNCTION(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_USER_FIT_FUNCTION))
#define GWY_IS_USER_FIT_FUNCTION_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_USER_FIT_FUNCTION))
#define GWY_USER_FIT_FUNCTION_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_USER_FIT_FUNCTION, GwyUserFitFunctionClass))

typedef struct _GwyUserFitFunction      GwyUserFitFunction;
typedef struct _GwyUserFitFunctionClass GwyUserFitFunctionClass;

struct _GwyUserFitFunction {
    GwyResource resource;
    struct _GwyUserFitFunctionPrivate *priv;
};

struct _GwyUserFitFunctionClass {
    /*< private >*/
    GwyResourceClass resource_class;
};

#define gwy_user_fit_function_duplicate(user_fit_function) \
        (GWY_USER_FIT_FUNCTION(gwy_serializable_duplicate(GWY_SERIALIZABLE(user_fit_function))))
#define gwy_user_fit_function_assign(dest, src) \
        (gwy_serializable_assign(GWY_SERIALIZABLE(dest), GWY_SERIALIZABLE(src)))

GType                          gwy_user_fit_function_get_type      (void)                                G_GNUC_CONST;
GwyUserFitFunction*            gwy_user_fit_function_new           (void)                                G_GNUC_MALLOC;
const gchar*                   gwy_user_fit_function_get_expression(GwyUserFitFunction *userfitfunction) G_GNUC_PURE;
const GwyUserFitFunctionParam* gwy_user_fit_function_get_params    (GwyUserFitFunction *userfitfunction,
                                                                    guint *nparams);

#define gwy_user_fit_functions() \
    (gwy_resource_type_get_inventory(GWY_TYPE_USER_FIT_FUNCTION))
#define gwy_user_fit_functions_get(name) \
    ((GwyUserFitFunction*)gwy_inventory_get_or_default(gwy_user_fit_functions(), (name)))

G_END_DECLS

#endif /*__GWY_USER_FIT_FUNCTION_H__*/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
