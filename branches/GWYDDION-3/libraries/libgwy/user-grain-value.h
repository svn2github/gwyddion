/*
 *  $Id$
 *  Copyright (C) 2011-2012 David Nečas (Yeti).
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

#ifndef __LIBGWY_USER_GRAIN_VALUE_H__
#define __LIBGWY_USER_GRAIN_VALUE_H__

#include <libgwy/expr.h>
#include <libgwy/fit-param.h>
#include <libgwy/resource.h>

G_BEGIN_DECLS

#define GWY_USER_GRAIN_VALUE_ERROR gwy_user_grain_value_error_quark()

typedef enum {
    GWY_USER_GRAIN_VALUE_ERROR_DEPENDS = 1,
} GwyUserGrainValueError;

typedef enum {
    GWY_GRAIN_VALUE_SAME_UNITS_NONE    = 0,
    GWY_GRAIN_VALUE_SAME_UNITS_LATERAL = 1,
    GWY_GRAIN_VALUE_SAME_UNITS_ALL     = 2,
} GwyGrainValueSameUnits;

GQuark gwy_user_grain_value_error_quark(void) G_GNUC_CONST;

#define GWY_TYPE_USER_GRAIN_VALUE \
    (gwy_user_grain_value_get_type())
#define GWY_USER_GRAIN_VALUE(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_USER_GRAIN_VALUE, GwyUserGrainValue))
#define GWY_USER_GRAIN_VALUE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_USER_GRAIN_VALUE, GwyUserGrainValueClass))
#define GWY_IS_USER_GRAIN_VALUE(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_USER_GRAIN_VALUE))
#define GWY_IS_USER_GRAIN_VALUE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_USER_GRAIN_VALUE))
#define GWY_USER_GRAIN_VALUE_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_USER_GRAIN_VALUE, GwyUserGrainValueClass))

typedef struct _GwyUserGrainValue      GwyUserGrainValue;
typedef struct _GwyUserGrainValueClass GwyUserGrainValueClass;

struct _GwyUserGrainValue {
    GwyResource resource;
    struct _GwyUserGrainValuePrivate *priv;
};

struct _GwyUserGrainValueClass {
    /*<private>*/
    GwyResourceClass resource_class;
};

#define gwy_user_grain_value_duplicate(usergrainvalue) \
        (GWY_USER_GRAIN_VALUE(gwy_serializable_duplicate(GWY_SERIALIZABLE(usergrainvalue))))
#define gwy_user_grain_value_assign(dest, src) \
        (gwy_serializable_assign(GWY_SERIALIZABLE(dest), GWY_SERIALIZABLE(src)))

GType                  gwy_user_grain_value_get_type      (void)                                    G_GNUC_CONST;
GwyUserGrainValue*     gwy_user_grain_value_new           (void)                                    G_GNUC_MALLOC;
const gchar*           gwy_user_grain_value_get_formula   (const GwyUserGrainValue *usergrainvalue) G_GNUC_PURE;
gboolean               gwy_user_grain_value_set_formula   (GwyUserGrainValue *usergrainvalue,
                                                           const gchar *formula,
                                                           GError **error);
const gchar*           gwy_user_grain_value_get_group     (const GwyUserGrainValue *usergrainvalue) G_GNUC_PURE;
void                   gwy_user_grain_value_set_group     (GwyUserGrainValue *usergrainvalue,
                                                           const gchar *group);
const gchar*           gwy_user_grain_value_get_symbol    (const GwyUserGrainValue *usergrainvalue) G_GNUC_PURE;
void                   gwy_user_grain_value_set_symbol    (GwyUserGrainValue *usergrainvalue,
                                                           const gchar *symbol);
const gchar*           gwy_user_grain_value_get_ident     (const GwyUserGrainValue *usergrainvalue) G_GNUC_PURE;
void                   gwy_user_grain_value_set_ident     (GwyUserGrainValue *usergrainvalue,
                                                           const gchar *ident);
gint                   gwy_user_grain_value_get_power_x   (const GwyUserGrainValue *usergrainvalue) G_GNUC_PURE;
void                   gwy_user_grain_value_set_power_x   (GwyUserGrainValue *usergrainvalue,
                                                           gint power_x);
gint                   gwy_user_grain_value_get_power_y   (const GwyUserGrainValue *usergrainvalue) G_GNUC_PURE;
void                   gwy_user_grain_value_set_power_y   (GwyUserGrainValue *usergrainvalue,
                                                           gint power_y);
gint                   gwy_user_grain_value_get_power_z   (const GwyUserGrainValue *usergrainvalue) G_GNUC_PURE;
void                   gwy_user_grain_value_set_power_z   (GwyUserGrainValue *usergrainvalue,
                                                           gint power_z);
GwyGrainValueSameUnits gwy_user_grain_value_get_same_units(const GwyUserGrainValue *usergrainvalue) G_GNUC_PURE;
void                   gwy_user_grain_value_set_same_units(GwyUserGrainValue *usergrainvalue,
                                                           GwyGrainValueSameUnits same_units);
gboolean               gwy_user_grain_value_get_is_angle  (const GwyUserGrainValue *usergrainvalue) G_GNUC_PURE;
void                   gwy_user_grain_value_set_is_angle  (GwyUserGrainValue *usergrainvalue,
                                                           gboolean is_angle);

#define gwy_user_grain_values() \
    (gwy_resource_type_get_inventory(GWY_TYPE_USER_GRAIN_VALUE))
#define gwy_user_grain_values_get(name) \
    ((GwyUserGrainValue*)gwy_inventory_get_or_default(gwy_user_grain_values(), (name)))

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
