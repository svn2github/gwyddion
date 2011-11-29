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

/*< private_header >*/

#ifndef __LIBGWY_GRAIN_VALUE_BUILTIN_H__
#define __LIBGWY_GRAIN_VALUE_BUILTIN_H__

#include "libgwy/field.h"
#include "libgwy/mask-field.h"
#include "libgwy/grain-value.h"

G_BEGIN_DECLS

// TODO: per-grain-RMS, RMS wrt global mean, RMS wrt mask mean, surface
// boundary length, grain-minimum-based volume, convex hull properties,
// bounding box (in real coordinates)
// XXX: Two quantities are derived: r_eq and V_min.  But they seem too useful to leave
// their definitions to the users.
// If this changes then the arrays in _gwy_grain_value_evaluate_builtins()
// that are indexed by this enum must be updates (and providing this the
// values can be reordered freely).
typedef enum {
    GWY_GRAIN_VALUE_CENTER_X             = 0,
    GWY_GRAIN_VALUE_CENTER_Y             = 1,
    GWY_GRAIN_VALUE_PROJECTED_AREA       = 2,
    GWY_GRAIN_VALUE_EQUIV_DISC_RADIUS    = 3,
    GWY_GRAIN_VALUE_SURFACE_AREA         = 4,
    GWY_GRAIN_VALUE_HALF_HEIGHT_AREA     = 5,
    GWY_GRAIN_VALUE_MINIMUM              = 6,
    GWY_GRAIN_VALUE_MAXIMUM              = 7,
    GWY_GRAIN_VALUE_MEAN                 = 8,
    GWY_GRAIN_VALUE_MEDIAN               = 9,
    GWY_GRAIN_VALUE_FLAT_BOUNDARY_LENGTH = 10,
    GWY_GRAIN_VALUE_MINIMUM_BOUND_SIZE   = 11,
    GWY_GRAIN_VALUE_MINIMUM_BOUND_ANGLE  = 12,
    GWY_GRAIN_VALUE_MAXIMUM_BOUND_SIZE   = 13,
    GWY_GRAIN_VALUE_MAXIMUM_BOUND_ANGLE  = 14,
    GWY_GRAIN_VALUE_BOUNDARY_MAXIMUM     = 15,
    GWY_GRAIN_VALUE_BOUNDARY_MINIMUM     = 16,
    GWY_GRAIN_VALUE_VOLUME_0             = 17,
    GWY_GRAIN_VALUE_VOLUME_MIN           = 18,
    GWY_GRAIN_VALUE_VOLUME_LAPLACE       = 19,
    GWY_GRAIN_VALUE_SLOPE_THETA          = 20,
    GWY_GRAIN_VALUE_SLOPE_PHI            = 21,
    GWY_GRAIN_VALUE_CURVATURE_CENTER_X   = 22,
    GWY_GRAIN_VALUE_CURVATURE_CENTER_Y   = 23,
    GWY_GRAIN_VALUE_CURVATURE_CENTER_Z   = 24,
    GWY_GRAIN_VALUE_CURVATURE1           = 25,
    GWY_GRAIN_VALUE_CURVATURE2           = 26,
    GWY_GRAIN_VALUE_CURVATURE_ANGLE1     = 27,
    GWY_GRAIN_VALUE_CURVATURE_ANGLE2     = 28,
    GWY_GRAIN_NVALUES
} BuiltinGrainValueId;

typedef struct {
    BuiltinGrainValueId id;
    guint need;
    const gchar *name;
    const gchar *group;
    const gchar *ident;
    const gchar *symbol;
    gboolean same_units;
    gboolean is_angle;
    gint powerxy;
    gint powerz;
    gdouble fillvalue;
    // TODO: Some functions?
} BuiltinGrainValue;

struct _GwyGrainValuePrivate {
    gchar *name;

    gboolean is_valid;  // Set to %TRUE if the function actually exists.

    guint ngrains;
    gdouble *values;
    GwyUnit *unit;

    // Exactly one of builtin/resource is set
    const BuiltinGrainValue *builtin;
    GwyUserGrainValue *resource;

    // User values only
    GwyExpr *expr;
    gulong data_changed_id;
    gulong notify_name_id;
};

typedef struct _GwyGrainValuePrivate GrainValue;

G_GNUC_INTERNAL
GHashTable* _gwy_grain_value_setup_builtins(void) G_GNUC_MALLOC;

G_GNUC_INTERNAL
GwyExpr* _gwy_grain_value_new_expr_with_constants(void) G_GNUC_MALLOC;

G_GNUC_INTERNAL
void _gwy_grain_value_set_size(GwyGrainValue *grainvalue,
                               guint ngrains);

// Like gwy_field_evaluate_grains() but only for builtins.
G_GNUC_INTERNAL
void _gwy_grain_value_evaluate_builtins(const GwyField *field,
                                        const GwyMaskField *mask,
                                        GwyGrainValue **grainvalues,
                                        guint nvalues);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
