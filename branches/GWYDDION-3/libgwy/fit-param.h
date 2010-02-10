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

#ifdef __LIBGWY_ALIASES_H__
#error Public headers must be included before the aliasing header.
#endif

#ifndef __LIBGWY_FIT_PARAM_H__
#define __LIBGWY_FIT_PARAM_H__

#include <libgwy/serializable.h>

G_BEGIN_DECLS

#define GWY_FIT_PARAM_ERROR gwy_fit_param_error_quark()

typedef enum {
    GWY_FIT_PARAM_ERROR_VARIABLE = 1,
} GwyFitParamError;

GQuark gwy_fit_param_error_quark(void) G_GNUC_CONST;

#define GWY_TYPE_FIT_PARAM \
    (gwy_fit_param_get_type())
#define GWY_FIT_PARAM(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_FIT_PARAM, GwyFitParam))
#define GWY_FIT_PARAM_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_FIT_PARAM, GwyFitParamClass))
#define GWY_IS_FIT_PARAM(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_FIT_PARAM))
#define GWY_IS_FIT_PARAM_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_FIT_PARAM))
#define GWY_FIT_PARAM_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_FIT_PARAM, GwyFitParamClass))

typedef struct _GwyFitParam      GwyFitParam;
typedef struct _GwyFitParamClass GwyFitParamClass;

struct _GwyFitParam {
    GObject g_object;
    struct _GwyFitParamPrivate *priv;
};

struct _GwyFitParamClass {
    /*<private>*/
    GObjectClass g_object_class;
};

#define gwy_fit_param_duplicate(fitparam) \
        (GWY_FIT_PARAM(gwy_serializable_duplicate(GWY_SERIALIZABLE(fitparam))))
#define gwy_fit_param_assign(dest, src) \
        (gwy_serializable_assign(GWY_SERIALIZABLE(dest), GWY_SERIALIZABLE(src)))

GType        gwy_fit_param_get_type      (void)                        G_GNUC_CONST;
GwyFitParam* gwy_fit_param_new           (const gchar *name)           G_GNUC_MALLOC;
GwyFitParam* gwy_fit_param_new_set       (const gchar *name,
                                          gint power_x,
                                          gint power_y,
                                          const gchar *estimate)       G_GNUC_MALLOC;
const gchar* gwy_fit_param_get_name      (const GwyFitParam *fitparam) G_GNUC_PURE;
gint         gwy_fit_param_get_power_x   (const GwyFitParam *fitparam) G_GNUC_PURE;
void         gwy_fit_param_set_power_x   (GwyFitParam *fitparam,
                                          gint power_x);
gint         gwy_fit_param_get_power_y   (const GwyFitParam *fitparam) G_GNUC_PURE;
void         gwy_fit_param_set_power_y   (GwyFitParam *fitparam,
                                          gint power_y);
const gchar* gwy_fit_param_get_estimate  (const GwyFitParam *fitparam) G_GNUC_PURE;
void         gwy_fit_param_set_estimate  (GwyFitParam *fitparam,
                                          const gchar *estimate);
gboolean     gwy_fit_param_check_estimate(const gchar *estimate,
                                          GError **error);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

