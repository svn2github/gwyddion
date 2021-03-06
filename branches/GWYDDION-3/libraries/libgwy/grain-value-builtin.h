/*
 *  $Id$
 *  Copyright (C) 2011-2013 David Nečas (Yeti).
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
// XXX: Two quantities are derived: r_eq and V_min.  But they seem too useful
// to leave their definitions to the users.
//
// If this enum changes then the arrays in _gwy_grain_value_evaluate_builtins()
// that are indexed by this enum must be updated (and providing this the
// values can be reordered freely).
typedef enum {
    GWY_GRAIN_VALUE_CENTER_X = 0,
    GWY_GRAIN_VALUE_CENTER_Y,
    GWY_GRAIN_VALUE_PROJECTED_AREA,
    GWY_GRAIN_VALUE_EQUIV_DISC_RADIUS,
    GWY_GRAIN_VALUE_SURFACE_AREA,
    GWY_GRAIN_VALUE_HALF_HEIGHT_AREA,
    GWY_GRAIN_VALUE_CONVEX_HULL_AREA,
    GWY_GRAIN_VALUE_MINIMUM,
    GWY_GRAIN_VALUE_MAXIMUM,
    GWY_GRAIN_VALUE_MEAN,
    GWY_GRAIN_VALUE_MEDIAN,
    GWY_GRAIN_VALUE_RMS_INTRA,
    GWY_GRAIN_VALUE_SKEWNESS_INTRA,
    GWY_GRAIN_VALUE_KURTOSIS_INTRA,
    GWY_GRAIN_VALUE_FLAT_BOUNDARY_LENGTH,
    GWY_GRAIN_VALUE_MINIMUM_BOUND_SIZE,
    GWY_GRAIN_VALUE_MINIMUM_BOUND_ANGLE,
    GWY_GRAIN_VALUE_MAXIMUM_BOUND_SIZE,
    GWY_GRAIN_VALUE_MAXIMUM_BOUND_ANGLE,
    GWY_GRAIN_VALUE_BOUNDARY_MINIMUM,
    GWY_GRAIN_VALUE_BOUNDARY_MAXIMUM,
    GWY_GRAIN_VALUE_INSCRIBED_DISC_R,
    GWY_GRAIN_VALUE_INSCRIBED_DISC_X,
    GWY_GRAIN_VALUE_INSCRIBED_DISC_Y,
    GWY_GRAIN_VALUE_CIRCUMCIRCLE_R,
    GWY_GRAIN_VALUE_CIRCUMCIRCLE_X,
    GWY_GRAIN_VALUE_CIRCUMCIRCLE_Y,
    GWY_GRAIN_VALUE_MEAN_RADIUS,
    GWY_GRAIN_VALUE_MEAN_EDGE_DISTANCE,
    GWY_GRAIN_VALUE_SHAPE_NUMBER,
    GWY_GRAIN_VALUE_VOLUME_0,
    GWY_GRAIN_VALUE_VOLUME_MIN,
    GWY_GRAIN_VALUE_VOLUME_LAPLACE,
    GWY_GRAIN_VALUE_SLOPE_THETA,
    GWY_GRAIN_VALUE_SLOPE_PHI,
    GWY_GRAIN_VALUE_CURVATURE_CENTER_X,
    GWY_GRAIN_VALUE_CURVATURE_CENTER_Y,
    GWY_GRAIN_VALUE_CURVATURE_CENTER_Z,
    GWY_GRAIN_VALUE_CURVATURE1,
    GWY_GRAIN_VALUE_CURVATURE2,
    GWY_GRAIN_VALUE_CURVATURE_ANGLE1,
    GWY_GRAIN_VALUE_CURVATURE_ANGLE2,
    GWY_GRAIN_VALUE_SEMIMAJOR_AXIS,
    GWY_GRAIN_VALUE_SEMIMINOR_AXIS,
    GWY_GRAIN_VALUE_SEMIMAJOR_ANGLE,
    GWY_GRAIN_VALUE_SEMIMINOR_ANGLE,
    GWY_GRAIN_NVALUES
} BuiltinGrainValueId;

typedef struct {
    GHashTable *table;
    guint n;
    const gchar **names;
    const gchar **idents;
} BuiltinGrainValueTable;

typedef struct {
    BuiltinGrainValueId id;
    guint need;
    const gchar *name;
    const gchar *group;
    const gchar *ident;
    const gchar *symbol;
    // 0 = none, 1 = lateral, 2 = all
    guint same_units : 2;
    gboolean is_angle : 1;
    gint powerx;
    gint powery;
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
    gulong data_changed_id;
    gulong notify_name_id;
};

typedef struct _GwyGrainValuePrivate GrainValue;

G_GNUC_INTERNAL
void _gwy_grain_value_setup_builtins(BuiltinGrainValueTable *table);

G_GNUC_INTERNAL
GwyExpr* _gwy_grain_value_new_expr_with_constants(void) G_GNUC_MALLOC;

G_GNUC_INTERNAL
void _gwy_grain_value_set_size(GwyGrainValue *grainvalue,
                               guint ngrains);

G_GNUC_INTERNAL
void _gwy_grain_value_assign(GwyGrainValue *dest,
                             const GwyGrainValue *source);

// Like gwy_field_evaluate_grains() but only for builtins.
G_GNUC_INTERNAL
void _gwy_grain_value_evaluate_builtins(const GwyField *field,
                                        const GwyMaskField *mask,
                                        GwyGrainValue **grainvalues,
                                        guint nvalues);

G_GNUC_INTERNAL
const gchar* const* _gwy_grain_value_list_builtin_idents(void);

G_GNUC_INTERNAL
void _gwy_grain_value_builtin_convex_hull(GwyGrainValue *minsizegrainvalue,
                                          GwyGrainValue *minanglegrainvalue,
                                          GwyGrainValue *maxsizegrainvalue,
                                          GwyGrainValue *maxanglegrainvalue,
                                          GwyGrainValue *chullareagrainvalue,
                                          GwyGrainValue *excircrgrainvalue,
                                          GwyGrainValue *excircxgrainvalue,
                                          GwyGrainValue *excircygrainvalue,
                                          const guint *grains,
                                          const guint *anyboundpos,
                                          const GwyField *field);

G_GNUC_INTERNAL
void _gwy_grain_value_builtin_inscribed_disc(GwyGrainValue *inscrdrgrainvalue,
                                             GwyGrainValue *inscrdxgrainvalue,
                                             GwyGrainValue *inscrdygrainvalue,
                                             GwyGrainValue *edmeangrainvalue,
                                             const GwyGrainValue *xgrainvalue,
                                             const GwyGrainValue *ygrainvalue,
                                             const guint *grains,
                                             const guint *sizes,
                                             const GwyMaskField *mask,
                                             const GwyField *field);

G_GNUC_INTERNAL
void _gwy_mask_field_grain_inscribed_discs(gdouble *inscrdrvalues,
                                           gdouble *inscrdxvalues,
                                           gdouble *inscrdyvalues,
                                           const gdouble *xvalues,
                                           const gdouble *yvalues,
                                           const guint *grains,
                                           const guint *sizes,
                                           guint ngrains,
                                           const GwyMaskField *mask,
                                           gdouble dx,
                                           gdouble dy);

G_GNUC_INTERNAL
void _gwy_mask_field_grain_centre_x(gdouble *values,
                                    const guint *grains,
                                    const guint *sizes,
                                    guint ngrains,
                                    guint xres,
                                    guint yres);

G_GNUC_INTERNAL
void _gwy_mask_field_grain_centre_y(gdouble *values,
                                    const guint *grains,
                                    const guint *sizes,
                                    guint ngrains,
                                    guint xres,
                                    guint yres);

G_GNUC_UNUSED
static gboolean
all_null(guint n, guint *ngrains, ...)
{
    va_list ap;
    va_start(ap, ngrains);
    for (guint i = 0; i < n; i++) {
        const GwyGrainValue *grainvalue = va_arg(ap, const GwyGrainValue*);
        if (grainvalue) {
            GWY_MAYBE_SET(ngrains, grainvalue->priv->ngrains);
            va_end(ap);
            return FALSE;
        }
    }
    va_end(ap);
    return TRUE;
}

G_GNUC_UNUSED
static gboolean
check_target(GwyGrainValue *grainvalue,
             gdouble **values,
             BuiltinGrainValueId id)
{
    if (!grainvalue) {
        *values = NULL;
        return TRUE;
    }

    GrainValue *priv = grainvalue->priv;
    const BuiltinGrainValue *builtin = priv->builtin;
    g_return_val_if_fail(builtin && builtin->id == id, FALSE);
    *values = priv->values;
    return TRUE;
}

G_GNUC_UNUSED
static gboolean
check_dependence(const GwyGrainValue *grainvalue,
                 const gdouble **values,
                 BuiltinGrainValueId id)
{
    *values = NULL;
    g_return_val_if_fail(grainvalue, FALSE);
    GrainValue *priv = grainvalue->priv;
    const BuiltinGrainValue *builtin = priv->builtin;
    g_return_val_if_fail(builtin && builtin->id == id, FALSE);
    *values = priv->values;
    return TRUE;
}

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
