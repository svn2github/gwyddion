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

#ifndef __LIBGWY_USER_FIT_FUNC_H__
#define __LIBGWY_USER_FIT_FUNC_H__

#include <libgwy/expr.h>
#include <libgwy/fit-param.h>
#include <libgwy/resource.h>

G_BEGIN_DECLS

#define GWY_USER_FIT_FUNC_ERROR gwy_user_fit_func_error_quark()

typedef enum {
    GWY_USER_FIT_FUNC_ERROR_NO_PARAM = 1,
} GwyUserFitFuncError;

GQuark gwy_user_fit_func_error_quark(void) G_GNUC_CONST;

#define GWY_TYPE_USER_FIT_FUNC \
    (gwy_user_fit_func_get_type())
#define GWY_USER_FIT_FUNC(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_USER_FIT_FUNC, GwyUserFitFunc))
#define GWY_USER_FIT_FUNC_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_USER_FIT_FUNC, GwyUserFitFuncClass))
#define GWY_IS_USER_FIT_FUNC(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_USER_FIT_FUNC))
#define GWY_IS_USER_FIT_FUNC_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_USER_FIT_FUNC))
#define GWY_USER_FIT_FUNC_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_USER_FIT_FUNC, GwyUserFitFuncClass))

typedef struct _GwyUserFitFunc      GwyUserFitFunc;
typedef struct _GwyUserFitFuncClass GwyUserFitFuncClass;

struct _GwyUserFitFunc {
    GwyResource resource;
    struct _GwyUserFitFuncPrivate *priv;
};

struct _GwyUserFitFuncClass {
    /*<private>*/
    GwyResourceClass resource_class;
};

#define gwy_user_fit_func_duplicate(userfitfunc) \
        (GWY_USER_FIT_FUNC(gwy_serializable_duplicate(GWY_SERIALIZABLE(userfitfunc))))
#define gwy_user_fit_func_assign(dest, src) \
        (gwy_serializable_assign(GWY_SERIALIZABLE(dest), GWY_SERIALIZABLE(src)))

GType           gwy_user_fit_func_get_type      (void)                          G_GNUC_CONST;
GwyUserFitFunc* gwy_user_fit_func_new           (void)                          G_GNUC_MALLOC;
const gchar*    gwy_user_fit_func_get_formula   (const GwyUserFitFunc *userfitfunc)   G_GNUC_PURE;
gboolean        gwy_user_fit_func_set_formula   (GwyUserFitFunc *userfitfunc,
                                                 const gchar *formula,
                                                 GError **error);
const gchar*    gwy_user_fit_func_get_group     (const GwyUserFitFunc *userfitfunc) G_GNUC_PURE;
void            gwy_user_fit_func_set_group     (GwyUserFitFunc *userfitfunc,
                                                 const gchar *group);
guint           gwy_user_fit_func_n_params      (const GwyUserFitFunc *userfitfunc)   G_GNUC_PURE;
GwyFitParam*    gwy_user_fit_func_param         (const GwyUserFitFunc *userfitfunc,
                                                 const gchar *name)             G_GNUC_PURE;
GwyFitParam*    gwy_user_fit_func_nth_param     (const GwyUserFitFunc *userfitfunc,
                                                 guint i)                       G_GNUC_PURE;
guint           gwy_user_fit_func_resolve_params(GwyUserFitFunc *userfitfunc,
                                                 GwyExpr *expr,
                                                 guint *indices);

#define gwy_user_fit_funcs() \
    (gwy_resource_type_get_inventory(GWY_TYPE_USER_FIT_FUNC))
#define gwy_user_fit_funcs_get(name) \
    ((GwyUserFitFunc*)gwy_inventory_get_or_default(gwy_user_fit_funcs(), (name)))

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
