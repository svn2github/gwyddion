/*
 *  $Id$
 *  Copyright (C) 2011 David Nečas (Yeti).
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

#ifndef __LIBGWY_GRAIN_VALUE_H__
#define __LIBGWY_GRAIN_VALUE_H__

#include <libgwy/user-grain-value.h>
#include <libgwy/unit.h>
#include <libgwy/mask-field.h>
#include <libgwy/field.h>

G_BEGIN_DECLS

#define GWY_TYPE_GRAIN_VALUE \
    (gwy_grain_value_get_type())
#define GWY_GRAIN_VALUE(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_GRAIN_VALUE, GwyGrainValue))
#define GWY_GRAIN_VALUE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_GRAIN_VALUE, GwyGrainValueClass))
#define GWY_IS_GRAIN_VALUE(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_GRAIN_VALUE))
#define GWY_IS_GRAIN_VALUE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_GRAIN_VALUE))
#define GWY_GRAIN_VALUE_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_GRAIN_VALUE, GwyGrainValueClass))

typedef struct _GwyGrainValue      GwyGrainValue;
typedef struct _GwyGrainValueClass GwyGrainValueClass;

struct _GwyGrainValue {
    GObject g_object;
    struct _GwyGrainValuePrivate *priv;
};

struct _GwyGrainValueClass {
    /*<private>*/
    GObjectClass g_object_class;
};

GType              gwy_grain_value_get_type        (void)                            G_GNUC_CONST;
GwyGrainValue*     gwy_grain_value_new             (const gchar *name) G_GNUC_MALLOC;
const GwyUnit*     gwy_grain_value_unit            (const GwyGrainValue *grainvalue) G_GNUC_PURE;
const gdouble*     gwy_grain_value_data            (const GwyGrainValue *grainvalue,
                                                    guint *ngrains)                  G_GNUC_PURE;
const gchar*       gwy_grain_value_get_name        (const GwyGrainValue *grainvalue) G_GNUC_PURE;
const gchar*       gwy_grain_value_get_group       (const GwyGrainValue *grainvalue) G_GNUC_PURE;
const gchar*       gwy_grain_value_get_ident       (const GwyGrainValue *grainvalue) G_GNUC_PURE;
const gchar*       gwy_grain_value_get_symbol      (const GwyGrainValue *grainvalue) G_GNUC_PURE;
GwyUserGrainValue* gwy_grain_value_get_resource    (const GwyGrainValue *grainvalue) G_GNUC_PURE;
gboolean           gwy_grain_value_needs_same_units(const GwyGrainValue *grainvalue) G_GNUC_PURE;
gboolean           gwy_grain_value_is_angle        (const GwyGrainValue *grainvalue) G_GNUC_PURE;
gboolean           gwy_grain_value_is_valid        (const GwyGrainValue *grainvalue) G_GNUC_PURE;

void gwy_field_evaluate_grains(const GwyField *field,
                               const GwyMaskField *mask,
                               GwyGrainValue **grainvalues,
                               guint nvalues);

const gchar** gwy_grain_value_list_builtins(void) G_GNUC_MALLOC;

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
