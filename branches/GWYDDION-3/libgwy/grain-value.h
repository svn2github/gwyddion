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

G_BEGIN_DECLS

typedef enum {
    GWY_GRAIN_VALUE_SAME_UNITS = 1 << 0,
} GwyGrainValueFlags;

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

GType              gwy_grain_value_get_type  (void)                            G_GNUC_CONST;
GwyGrainValue*     gwy_grain_value_new       (const gchar *name,
                                              const gchar *group)              G_GNUC_MALLOC;
const GwyUnit*     gwy_grain_value_unit      (const GwyGrainValue *grainvalue) G_GNUC_PURE;
const gdouble*     gwy_grain_value_data      (const GwyGrainValue *grainvalue,
                                              guint *ngrains)                  G_GNUC_PURE;
const gchar*       gwy_grain_value_get_name  (const GwyGrainValue *grainvalue) G_GNUC_PURE;
const gchar*       gwy_grain_value_get_ident (const GwyGrainValue *grainvalue) G_GNUC_PURE;
const gchar*       gwy_grain_value_get_symbol(const GwyGrainValue *grainvalue) G_GNUC_PURE;
GwyGrainValueFlags gwy_grain_value_get_flags (const GwyGrainValue *grainvalue) G_GNUC_PURE;
GwyUserGrainValue* gwy_grain_value_get_resource(const GwyGrainValue *grainvalue) G_GNUC_PURE;
// TODO: Something like gwy_grain_value_evaluate().  Except that we may really
// want only the smart interface (like gwy_data_field_grains_get_quantities()
// in 2.x).  But interface-wise, where should that go?
// * It looks like a GwyField method but making it such raises the question:
//   How the hell are the GwyGrainValues filled with data?
//   as it needs some private method to do this.  Of course, if the
//   implementation resides in grain-value.c (as it should) this presents no
//   practical problem.
// * Putting it here would make it look like an evaluation working on fields.
//   So far so good but the smart evaluator takes an arbitary array of
//   GwyGrainValues.  This could be mitigated by having also a single-value
//   evaluator that would easily be GwyGrainValue method.

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
