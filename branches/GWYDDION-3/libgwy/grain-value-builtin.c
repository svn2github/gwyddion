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

#include <stdarg.h>
#include <string.h>
#include <glib/gi18n.h>
#include "libgwy/macros.h"
#include "libgwy/object-utils.h"
#include "libgwy/math.h"
#include "libgwy/grain-value.h"
#include "libgwy/field-level.h"
#include "libgwy/mask-field-grains.h"
#include "libgwy/grain-value-builtin.h"

// There are several possible choices for the volume quadrature coefficients:
// Bilinear interpolation:                          7/12, 1/12, 1/48
// Triangular interpolation (like surface area):    9/16, 3/32, 1/64
// Gwyddion 2 (how I obtained these? maybe a bug):  13/24, 5/48, 1/96
// Exact integration of all bi-quadratic functions: 121/144, 11/288, 1/576
#define VOLUME_W_SELF  0.840277777777777777777777777777
#define VOLUME_W_ORTHO 0.0381944444444444444444444444444
#define VOLUME_W_DIAG  0.00173611111111111111111111111111

enum {
    NEED_SIZE = 1 << 0,
    NEED_ANYBOUNDPOS = 1 << 1,
    NEED_MIN = 1 << 2,
    NEED_MAX = 1 << 3,
    NEED_XMEAN = (1 << 4) | NEED_SIZE,
    NEED_YMEAN = (1 << 5) | NEED_SIZE,
    NEED_CENTER = NEED_XMEAN | NEED_YMEAN,
    NEED_ZMEAN = (1 << 6) | NEED_SIZE,
    NEED_LINEAR = (1 << 7) | NEED_ZMEAN | NEED_CENTER,
    NEED_QUADRATIC = (1 << 8) | NEED_LINEAR,
    NEED_VOLUME = (1 << 9),
};

// Must be signed, we use signed differences.
typedef struct {
    gint i;
    gint j;
} GridPoint;

typedef struct {
    guint size;
    guint len;
    GridPoint *points;
} GridPointList;

// Edges are used for inscribed discs.
typedef struct {
    gdouble xa;
    gdouble ya;
    gdouble xb;
    gdouble yb;
    gdouble r2;    // Used to remember squared distance from current centre.
} Edge;

typedef struct {
    guint size;
    guint len;
    Edge *edges;
} EdgeList;

// Inscribed/excrcribed disc/circle.
typedef struct {
    gdouble x;
    gdouble y;
    gdouble R2;
    guint size;   // For candidate sorting.
} FooscribedDisc;

// Iterative algorithms that try to moving some position use NDIRECTIONS
// directions in each quadrant; shift_directions[] lists the vectors.
enum { NDIRECTIONS = 12 };

static const gdouble shift_directions[NDIRECTIONS*2] = {
    1.0, 0.0,
    0.9914448613738104, 0.1305261922200516,
    0.9659258262890683, 0.2588190451025207,
    0.9238795325112867, 0.3826834323650898,
    0.8660254037844387, 0.5,
    0.7933533402912352, 0.6087614290087207,
    0.7071067811865475, 0.7071067811865475,
    0.6087614290087207, 0.7933533402912352,
    0.5,                0.8660254037844387,
    0.3826834323650898, 0.9238795325112867,
    0.2588190451025207, 0.9659258262890683,
    0.1305261922200517, 0.9914448613738104,
};

// Grain values that satisfy the NEEDs defined above.  G_MAXUINT means an
// integer/non-scalar auxiliary value that does not directly correspond to any
// grain value.
static const BuiltinGrainValueId satisfies_needs[] = {
    /* NEED_SIZE */        G_MAXUINT,
    /* NEED_ANYBOUNDPOS */ G_MAXUINT,
    /* NEED_MIN */         GWY_GRAIN_VALUE_MINIMUM,
    /* NEED_MAX */         GWY_GRAIN_VALUE_MAXIMUM,
    /* NEED_XMEAN */       GWY_GRAIN_VALUE_CENTER_X,
    /* NEED_YMEAN */       GWY_GRAIN_VALUE_CENTER_Y,
    /* NEED_ZMEAN */       GWY_GRAIN_VALUE_MEAN,
    /* NEED_LINEAR */      G_MAXUINT,
    /* NEED_QUADRATIC */   G_MAXUINT,
    /* NEED_VOLUME */      GWY_GRAIN_VALUE_VOLUME_0,
};

static const BuiltinGrainValue builtin_table[GWY_GRAIN_NVALUES] = {
    {
        .id = GWY_GRAIN_VALUE_CENTER_X,
        .need = NEED_XMEAN,
        .name = NC_("grain value", "Center x position"),
        .group = NC_("grain value group", "Position"),
        .ident = "x_0",
        .symbol = "<i>x</i>₀",
        .powerxy = 1,
    },
    {
        .id = GWY_GRAIN_VALUE_CENTER_Y,
        .need = NEED_YMEAN,
        .name = NC_("grain value", "Center y position"),
        .group = NC_("grain value group", "Position"),
        .ident = "y_0",
        .symbol = "<i>y</i>₀",
        .powerxy = 1,
    },
    {
        .id = GWY_GRAIN_VALUE_PROJECTED_AREA,
        .need = NEED_SIZE,
        .name = NC_("grain value", "Projected area"),
        .group = NC_("grain value group", "Area"),
        .ident = "A_0",
        .symbol = "<i>A</i>₀",
        .powerxy = 2,
    },
    {
        .id = GWY_GRAIN_VALUE_EQUIV_DISC_RADIUS,
        .need = NEED_SIZE,
        .name = NC_("grain value", "Equivalent disc radius"),
        .group = NC_("grain value group", "Area"),
        .ident = "r_eq",
        .symbol = "<i>r</i><sub>eq</sub>",
        .powerxy = 1,
    },
    {
        .id = GWY_GRAIN_VALUE_SURFACE_AREA,
        .name = NC_("grain value", "Surface area"),
        .group = NC_("grain value group", "Area"),
        .ident = "A_s",
        .symbol = "<i>A</i><sub>s</sub>",
        .same_units = TRUE,
        .powerxy = 2,
    },
    {
        .id = GWY_GRAIN_VALUE_HALF_HEIGHT_AREA,
        .need = NEED_MIN | NEED_MAX,
        .name = NC_("grain value", "Area above half-height"),
        .group = NC_("grain value group", "Area"),
        .ident = "A_h",
        .symbol = "<i>A</i><sub>h</sub>",
        .powerxy = 2,
    },
    {
        .id = GWY_GRAIN_VALUE_CONVEX_HULL_AREA,
        .need = NEED_ANYBOUNDPOS,
        .name = NC_("grain value", "Area of convex hull"),
        .group = NC_("grain value group", "Area"),
        .ident = "A_c",
        .symbol = "<i>A</i><sub>c</sub>",
        .powerxy = 2,
    },
    {
        .id = GWY_GRAIN_VALUE_MINIMUM,
        .need = NEED_MIN,
        .name = NC_("grain value", "Minimum value"),
        .group = NC_("grain value group", "Value"),
        .ident = "z_min",
        .symbol = "<i>z</i><sub>min</sub>",
        .powerz = 1,
        .fillvalue = G_MAXDOUBLE,
    },
    {
        .id = GWY_GRAIN_VALUE_MAXIMUM,
        .need = NEED_MAX,
        .name = NC_("grain value", "Maximum value"),
        .group = NC_("grain value group", "Value"),
        .ident = "z_max",
        .symbol = "<i>z</i><sub>max</sub>",
        .powerz = 1,
        .fillvalue = -G_MAXDOUBLE,
    },
    {
        .id = GWY_GRAIN_VALUE_MEAN,
        .need = NEED_ZMEAN,
        .name = NC_("grain value", "Mean value"),
        .group = NC_("grain value group", "Value"),
        .ident = "z_m",
        .symbol = "<i>z</i><sub>m</sub>",
        .powerz = 1,
    },
    {
        .id = GWY_GRAIN_VALUE_MEDIAN,
        .need = NEED_SIZE,
        .name = NC_("grain value", "Median value"),
        .group = NC_("grain value group", "Value"),
        .ident = "z_med",
        .symbol = "<i>z</i><sub>med</sub>",
        .powerz = 1,
    },
    {
        .id = GWY_GRAIN_VALUE_RMS_INTRA,
        .need = NEED_ZMEAN,
        .name = NC_("grain value", "Value rms (intragrain)"),
        .group = NC_("grain value group", "Value"),
        .ident = "sigma_i",
        .symbol = "<i>σ</i><sub>i</sub>",
        .powerz = 1,
    },
    {
        .id = GWY_GRAIN_VALUE_FLAT_BOUNDARY_LENGTH,
        .name = NC_("grain value", "Projected boundary length"),
        .group = NC_("grain value group", "Boundary"),
        .ident = "L_b0",
        .symbol = "<i>L</i><sub>b0</sub>",
        .powerxy = 1,
    },
    {
        .id = GWY_GRAIN_VALUE_MINIMUM_BOUND_SIZE,
        .need = NEED_ANYBOUNDPOS,
        .name = NC_("grain value", "Minimum bounding size"),
        .group = NC_("grain value group", "Boundary"),
        .ident = "D_min",
        .symbol = "<i>D</i><sub>min</sub>",
        .powerxy = 1,
        .fillvalue = G_MAXDOUBLE,
    },
    {
        .id = GWY_GRAIN_VALUE_MINIMUM_BOUND_ANGLE,
        .need = NEED_ANYBOUNDPOS,
        .name = NC_("grain value", "Minimum bounding direction"),
        .group = NC_("grain value group", "Boundary"),
        .ident = "phi_min",
        .symbol = "<i>φ</i><sub>min</sub>",
        .is_angle = TRUE,
    },
    {
        .id = GWY_GRAIN_VALUE_MAXIMUM_BOUND_SIZE,
        .need = NEED_ANYBOUNDPOS,
        .name = NC_("grain value", "Maximum bounding size"),
        .group = NC_("grain value group", "Boundary"),
        .ident = "D_max",
        .symbol = "<i>D</i><sub>max</sub>",
        .powerxy = 1,
        .fillvalue = -G_MAXDOUBLE,
    },
    {
        .id = GWY_GRAIN_VALUE_MAXIMUM_BOUND_ANGLE,
        .need = NEED_ANYBOUNDPOS,
        .name = NC_("grain value", "Maximum bounding direction"),
        .group = NC_("grain value group", "Boundary"),
        .ident = "phi_max",
        .symbol = "<i>φ</i><sub>max</sub>",
        .is_angle = TRUE,
    },
    {
        .id = GWY_GRAIN_VALUE_BOUNDARY_MINIMUM,
        .name = NC_("grain value", "Minimum value on boundary"),
        .group = NC_("grain value group", "Boundary"),
        .ident = "b_min",
        .symbol = "<i>b</i><sub>min</sub>",
        .powerz = 1,
        .fillvalue = G_MAXDOUBLE,
    },
    {
        .id = GWY_GRAIN_VALUE_BOUNDARY_MAXIMUM,
        .name = NC_("grain value", "Maximum value on boundary"),
        .group = NC_("grain value group", "Boundary"),
        .ident = "b_max",
        .symbol = "<i>b</i><sub>max</sub>",
        .powerz = 1,
        .fillvalue = -G_MAXDOUBLE,
    },
    {
        .id = GWY_GRAIN_VALUE_INSCRIBED_DISC_R,
        .need = NEED_SIZE | NEED_CENTER,
        .name = NC_("grain value", "Maximum inscribed disc radius"),
        .group = NC_("grain value group", "Boundary"),
        .ident = "R_i",
        .symbol = "<i>R</i><sub>i</sub>",
        .powerxy = 1,
    },
    {
        .id = GWY_GRAIN_VALUE_INSCRIBED_DISC_X,
        .need = NEED_SIZE | NEED_CENTER,
        .name = NC_("grain value", "Maximum inscribed disc center x position"),
        .group = NC_("grain value group", "Boundary"),
        .ident = "x_i",
        .symbol = "<i>x</i><sub>i</sub>",
        .powerxy = 1,
    },
    {
        .id = GWY_GRAIN_VALUE_INSCRIBED_DISC_Y,
        .need = NEED_SIZE | NEED_CENTER,
        .name = NC_("grain value", "Maximum inscribed disc center y position"),
        .group = NC_("grain value group", "Boundary"),
        .ident = "y_i",
        .symbol = "<i>y</i><sub>i</sub>",
        .powerxy = 1,
    },
    {
        .id = GWY_GRAIN_VALUE_CIRCUMCIRCLE_R,
        .need = NEED_ANYBOUNDPOS,
        .name = NC_("grain value", "Minimum circumcircle radius"),
        .group = NC_("grain value group", "Boundary"),
        .ident = "R_e",
        .symbol = "<i>R</i><sub>e</sub>",
        .powerxy = 1,
    },
    {
        .id = GWY_GRAIN_VALUE_CIRCUMCIRCLE_X,
        .need = NEED_ANYBOUNDPOS,
        .name = NC_("grain value", "Minimum circumcircle center x position"),
        .group = NC_("grain value group", "Boundary"),
        .ident = "x_e",
        .symbol = "<i>x</i><sub>e</sub>",
        .powerxy = 1,
    },
    {
        .id = GWY_GRAIN_VALUE_CIRCUMCIRCLE_Y,
        .need = NEED_ANYBOUNDPOS,
        .name = NC_("grain value", "Minimum circumcircle center y position"),
        .group = NC_("grain value group", "Boundary"),
        .ident = "y_e",
        .symbol = "<i>y</i><sub>e</sub>",
        .powerxy = 1,
    },
    {
        .id = GWY_GRAIN_VALUE_MEAN_RADIUS,
        .need = NEED_CENTER,
        .name = NC_("grain value", "Mean radius"),
        .group = NC_("grain value group", "Boundary"),
        .ident = "R_m",
        .symbol = "<i>R</i><sub>m</sub>",
        .powerxy = 1,
    },
    {
        .id = GWY_GRAIN_VALUE_VOLUME_0,
        .need = NEED_VOLUME,
        .name = NC_("grain value", "Zero-based volume"),
        .group = NC_("grain value group", "Volume"),
        .ident = "V_0",
        .symbol = "<i>V</i>₀",
        .powerxy = 2,
        .powerz = 1,
    },
    {
        .id = GWY_GRAIN_VALUE_VOLUME_MIN,
        .need = NEED_MIN | NEED_VOLUME | NEED_SIZE,
        .name = NC_("grain value", "Minimum-based volume"),
        .group = NC_("grain value group", "Volume"),
        .ident = "V_min",
        .symbol = "<i>V</i><sub>min</sub>",
        .powerxy = 2,
        .powerz = 1,
    },
    {
        .id = GWY_GRAIN_VALUE_VOLUME_LAPLACE,
        .need = NEED_VOLUME | NEED_SIZE,
        .name = NC_("grain value", "Laplace-based volume"),
        .group = NC_("grain value group", "Volume"),
        .ident = "V_L",
        .symbol = "<i>V</i><sub>L</sub>",
        .powerxy = 2,
        .powerz = 1,
    },
    {
        .id = GWY_GRAIN_VALUE_SLOPE_THETA,
        .need = NEED_LINEAR,
        .name = NC_("grain value", "Slope normal angle"),
        .group = NC_("grain value group", "Slope"),
        .ident = "theta",
        .symbol = "<i>ϑ</i>",
        .same_units = TRUE,
        .is_angle = TRUE,
    },
    {
        .id = GWY_GRAIN_VALUE_SLOPE_PHI,
        .need = NEED_LINEAR,
        .name = NC_("grain value", "Slope direction"),
        .group = NC_("grain value group", "Slope"),
        .ident = "phi",
        .symbol = "<i>ϑ</i>",
        .is_angle = TRUE,
    },
    {
        .id = GWY_GRAIN_VALUE_CURVATURE_CENTER_X,
        .need = NEED_QUADRATIC,
        .name = NC_("grain value", "Curvature center x position"),
        .group = NC_("grain value group", "Curvature"),
        .ident = "x_c",
        .symbol = "<i>x</i><sub>c</sub>",
        .powerxy = 1,
    },
    {
        .id = GWY_GRAIN_VALUE_CURVATURE_CENTER_Y,
        .need = NEED_QUADRATIC,
        .name = NC_("grain value", "Curvature center y position"),
        .group = NC_("grain value group", "Curvature"),
        .ident = "y_c",
        .symbol = "<i>y</i><sub>c</sub>",
        .powerxy = 1,
    },
    {
        .id = GWY_GRAIN_VALUE_CURVATURE_CENTER_Z,
        .need = NEED_QUADRATIC,
        .name = NC_("grain value", "Curvature center value"),
        .group = NC_("grain value group", "Curvature"),
        .ident = "z_c",
        .symbol = "<i>z</i><sub>c</sub>",
        .powerz = 1,
    },
    {
        .id = GWY_GRAIN_VALUE_CURVATURE1,
        .need = NEED_QUADRATIC,
        .name = NC_("grain value", "Curvature 1"),
        .group = NC_("grain value group", "Curvature"),
        .ident = "kappa_1",
        .symbol = "<i>κ</i>₁",
        .same_units = TRUE,
        .powerz = -1,
    },
    {
        .id = GWY_GRAIN_VALUE_CURVATURE2,
        .need = NEED_QUADRATIC,
        .name = NC_("grain value", "Curvature 2"),
        .group = NC_("grain value group", "Curvature"),
        .ident = "kappa_2",
        .symbol = "<i>κ</i>₂",
        .same_units = TRUE,
        .powerz = -1,
    },
    {
        .id = GWY_GRAIN_VALUE_CURVATURE_ANGLE1,
        .need = NEED_QUADRATIC,
        .name = NC_("grain value", "Curvature direction 1"),
        .group = NC_("grain value group", "Curvature"),
        .ident = "phi_1",
        .symbol = "<i>φ</i>₁",
        .is_angle = TRUE,
    },
    {
        .id = GWY_GRAIN_VALUE_CURVATURE_ANGLE2,
        .need = NEED_QUADRATIC,
        .name = NC_("grain value", "Curvature direction 2"),
        .group = NC_("grain value group", "Curvature"),
        .ident = "phi_2",
        .symbol = "<i>φ</i>₂",
        .is_angle = TRUE,
    },
};

static inline void
grid_point_list_add(GridPointList *list,
                    gint j, gint i)
{
    if (G_UNLIKELY(list->len == list->size)) {
        list->size = MAX(2*list->size, 16);
        list->points = g_renew(GridPoint, list->points, list->size);
    }

    list->points[list->len].i = i;
    list->points[list->len].j = j;
    list->len++;
}

static inline void
edge_list_add(EdgeList *list,
              gdouble xa, gdouble ya,
              gdouble xb, gdouble yb)
{
    if (G_UNLIKELY(list->len == list->size)) {
        list->size = MAX(2*list->size, 16);
        list->edges = g_renew(Edge, list->edges, list->size);
    }

    list->edges[list->len].xa = xa;
    list->edges[list->len].ya = ya;
    list->edges[list->len].xb = xb;
    list->edges[list->len].yb = yb;
    list->len++;
}

static guint*
grain_maybe_realloc(guint *grain, guint w, guint h, guint *grainsize)
{
    if (w*h > *grainsize) {
        g_free(grain);
        *grainsize = w*h;
        grain = g_new(guint, *grainsize);
    }
    return grain;
}

static void
ensure_value(BuiltinGrainValueId id,
             GwyGrainValue **ourvalues,
             guint ngrains)
{
    if (!ourvalues[id]) {
        ourvalues[id] = gwy_grain_value_new(builtin_table[id].name);
        g_assert(ourvalues[id]);
    }
    g_assert(ourvalues[id]->priv->builtin->id == id);
    _gwy_grain_value_set_size(ourvalues[id], ngrains);
}

static void
init_values(GwyGrainValue *grainvalue)
{
    GrainValue *priv = grainvalue->priv;
    gdouble value = priv->builtin->fillvalue;
    guint n = priv->ngrains + 1;
    gdouble *v = priv->values;

    if (value) {
        for (guint i = n; i; i--, v++)
            *v = value;
    }
    else
        gwy_clear(v, n);
}

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

static void
calc_anyboundpos(guint *anyboundpos,
                 const guint *grains,
                 const GwyField *field)
{
    if (!anyboundpos)
        return;

    guint nn = field->xres * field->yres;
    const guint *g = grains;
    for (guint k = 0; k < nn; k++, g++) {
        guint gno = *g;
        if (anyboundpos[gno] == G_MAXUINT)
            anyboundpos[gno] = k;
    }
}

static void
calc_minimum(GwyGrainValue *grainvalue,
             const guint *grains,
             const GwyField *field)
{
    gdouble *values;
    if (!grainvalue
        || !check_target(grainvalue, &values, GWY_GRAIN_VALUE_MINIMUM))
        return;

    guint nn = field->xres * field->yres;
    const guint *g = grains;
    const gdouble *d = field->data;

    for (guint k = nn; k; k--, g++, d++) {
        guint gno = *g;
        gdouble z = *d;
        if (z < values[gno])
            values[gno] = z;
    }
}

static void
calc_maximum(GwyGrainValue *grainvalue,
             const guint *grains,
             const GwyField *field)
{
    gdouble *values;
    if (!grainvalue
        || !check_target(grainvalue, &values, GWY_GRAIN_VALUE_MAXIMUM))
        return;

    guint nn = field->xres * field->yres;
    const guint *g = grains;
    const gdouble *d = field->data;

    for (guint k = nn; k; k--, g++, d++) {
        guint gno = *g;
        gdouble z = *d;
        if (z > values[gno])
            values[gno] = z;
    }
}

// Note all coordinates are pixel-wise, not real.
static void
calc_centre_x(GwyGrainValue *grainvalue,
              const guint *grains,
              const guint *sizes,
              const GwyField *field)
{
    gdouble *values;
    if (!grainvalue
        || !check_target(grainvalue, &values, GWY_GRAIN_VALUE_CENTER_X))
        return;

    g_return_if_fail(sizes);
    guint xres = field->xres, yres = field->yres;
    guint ngrains = grainvalue->priv->ngrains;
    const guint *g = grains;

    for (guint i = 0; i < yres; i++) {
        for (guint j = 0; j < xres; j++, g++) {
            guint gno = *g;
            values[gno] += j;
        }
    }
    for (guint gno = 0; gno <= ngrains; gno++)
        values[gno] /= sizes[gno];
}

static void
calc_centre_y(GwyGrainValue *grainvalue,
              const guint *grains,
              const guint *sizes,
              const GwyField *field)
{
    gdouble *values;
    if (!grainvalue
        || !check_target(grainvalue, &values, GWY_GRAIN_VALUE_CENTER_Y))
        return;

    g_return_if_fail(sizes);
    guint xres = field->xres, yres = field->yres;
    guint ngrains = grainvalue->priv->ngrains;
    const guint *g = grains;

    for (guint i = 0; i < yres; i++) {
        for (guint j = 0; j < xres; j++, g++) {
            guint gno = *g;
            values[gno] += i;
        }
    }
    for (guint gno = 0; gno <= ngrains; gno++)
        values[gno] /= sizes[gno];
}

static void
calc_mean(GwyGrainValue *grainvalue,
          const guint *grains,
          const guint *sizes,
          const GwyField *field)
{
    gdouble *values;
    if (!grainvalue 
        || !check_target(grainvalue, &values, GWY_GRAIN_VALUE_MEAN))
        return;

    g_return_if_fail(sizes);
    guint nn = field->xres * field->yres;
    guint ngrains = grainvalue->priv->ngrains;
    const guint *g = grains;
    const gdouble *d = field->data;

    for (guint k = nn; k; k--, g++, d++)
        values[*g] += *d;
    for (guint gno = 0; gno <= ngrains; gno++)
        values[gno] /= sizes[gno];
}

static void
calc_volume_0(GwyGrainValue *grainvalue,
              const guint *grains,
              const GwyField *field)
{
    gdouble *values;
    if (!grainvalue
        || !check_target(grainvalue, &values, GWY_GRAIN_VALUE_VOLUME_0))
        return;

    guint xres = field->xres, yres = field->yres;
    gdouble dxdy = gwy_field_dx(field)*gwy_field_dy(field);
    guint ngrains = grainvalue->priv->ngrains;
    const guint *g = grains;
    const gdouble *d = field->data;

    for (guint i = 0; i < yres; i++) {
        for (guint j = 0; j < xres; j++, g++) {
            guint gno = *g;
            if (!gno)
                continue;

            guint ix = i*xres;
            guint imx = (i > 0) ? ix-xres : ix;
            guint ipx = (i < yres-1) ? ix+xres : ix;
            guint jm = (j > 0) ? j-1 : j;
            guint jp = (j < xres-1) ? j+1 : j;

            gdouble v = (VOLUME_W_SELF*d[ix + j]
                         + VOLUME_W_ORTHO*(d[imx + j] + d[ix + jm]
                                           + d[ix + jp] + d[ipx + j])
                         + VOLUME_W_DIAG*(d[imx + jm] + d[imx + jp]
                                          + d[ipx + jm] + d[ipx + jp]));

            values[gno] += v;
        }
    }
    for (guint gno = 1; gno <= ngrains; gno++)
        values[gno] *= dxdy;
}

// The coordinate origin is always the grain centre.
// Also the sums are *NOT* divided by grain sizes because these will cancel out.
static void
calc_linear(gdouble *linear,
            const guint *grains,
            const GwyGrainValue *xgrainvalue,
            const GwyGrainValue *ygrainvalue,
            const GwyGrainValue *zgrainvalue,
            const GwyField *field)
{
    const gdouble *xvalue, *yvalue, *zvalue;
    if (!linear
        || !check_dependence(xgrainvalue, &xvalue, GWY_GRAIN_VALUE_CENTER_X)
        || !check_dependence(ygrainvalue, &yvalue, GWY_GRAIN_VALUE_CENTER_Y)
        || !check_dependence(zgrainvalue, &zvalue, GWY_GRAIN_VALUE_MEAN))
        return;

    guint xres = field->xres;
    guint yres = field->yres;
    const guint *g = grains;
    const gdouble *d = field->data;

    for (guint i = 0; i < yres; i++) {
        for (guint j = 0; j < xres; j++, g++, d++) {
            guint gno = *g;
            if (!gno)
                continue;

            gdouble *t = linear + 5*gno;
            gdouble x = j - xvalue[gno];
            gdouble y = i - yvalue[gno];
            gdouble z = *d - zvalue[gno];
            *(t++) += x*x;
            *(t++) += x*y;
            *(t++) += y*y;
            *(t++) += x*z;
            *t += y*z;
        }
    }
}

static void
calc_quadratic(gdouble *quadratic,
               const guint *grains,
               const GwyGrainValue *xgrainvalue,
               const GwyGrainValue *ygrainvalue,
               const GwyGrainValue *zgrainvalue,
               const GwyField *field)
{
    const gdouble *xvalue, *yvalue, *zvalue;
    if (!quadratic
        || !check_dependence(xgrainvalue, &xvalue, GWY_GRAIN_VALUE_CENTER_X)
        || !check_dependence(ygrainvalue, &yvalue, GWY_GRAIN_VALUE_CENTER_Y)
        || !check_dependence(zgrainvalue, &zvalue, GWY_GRAIN_VALUE_MEAN))
        return;

    guint xres = field->xres;
    guint yres = field->yres;
    const guint *g = grains;
    const gdouble *d = field->data;

    for (guint i = 0; i < yres; i++) {
        for (guint j = 0; j < xres; j++, g++, d++) {
            guint gno = *g;
            if (!gno)
                continue;

            gdouble *t = quadratic + 12*gno;
            gdouble x = j - xvalue[gno];
            gdouble y = i - yvalue[gno];
            gdouble z = *d - zvalue[gno];
            gdouble xx = x*x;
            gdouble xy = x*y;
            gdouble yy = y*y;
            *(t++) += xx*x;
            *(t++) += xx*y;
            *(t++) += x*yy;
            *(t++) += y*yy;
            *(t++) += xx*xx;
            *(t++) += xx*xy;
            *(t++) += xx*yy;
            *(t++) += xy*yy;
            *(t++) += yy*yy;
            *(t++) += xx*z;
            *(t++) += xy*z;
            *t += yy*z;
        }
    }
}

static void
calc_median(GwyGrainValue *grainvalue,
            const guint *grains,
            const guint *sizes,
            const GwyField *field)
{
    gdouble *values;
    if (!grainvalue
        || !check_target(grainvalue, &values, GWY_GRAIN_VALUE_MEDIAN))
        return;

    guint nn = field->xres * field->yres;
    guint ngrains = grainvalue->priv->ngrains;
    const gdouble *d = field->data;
    guint *csizes = g_new0(guint, ngrains + 1);
    guint *pos = g_new0(guint, ngrains + 1);

    // Find cumulative sizes (we care only about grains, ignore the
    // outside-grains area).
    csizes[0] = 0;
    csizes[1] = sizes[1];
    for (guint gno = 2; gno <= ngrains; gno++)
        csizes[gno] = sizes[gno] + csizes[gno-1];

    gdouble *tmp = g_new(gdouble, csizes[ngrains]);
    // Find where each grain starts in tmp sorted by grain number.
    for (guint gno = 1; gno <= ngrains; gno++)
        pos[gno] = csizes[gno-1];
    // Sort values by grain number to tmp.
    for (guint k = 0; k < nn; k++) {
        guint gno = grains[k];
        if (gno) {
            tmp[pos[gno]] = d[k];
            pos[gno]++;
        }
    }
    // Find the median of each block.
    for (guint gno = 1; gno <= ngrains; gno++)
        values[gno] = gwy_math_median(tmp + csizes[gno-1],
                                      csizes[gno] - csizes[gno-1]);

    g_free(csizes);
    g_free(pos);
    g_free(tmp);
}

static void
calc_rms_intra(GwyGrainValue *grainvalue,
               const GwyGrainValue *zgrainvalue,
               const guint *grains,
               const guint *sizes,
               const GwyField *field)
{
    gdouble *values;
    const gdouble *zvalue;
    if (!grainvalue
        || !check_target(grainvalue, &values, GWY_GRAIN_VALUE_RMS_INTRA)
        || !check_dependence(zgrainvalue, &zvalue, GWY_GRAIN_VALUE_MEAN))
        return;

    g_return_if_fail(sizes);
    guint nn = field->xres * field->yres;
    guint ngrains = grainvalue->priv->ngrains;
    const guint *g = grains;
    const gdouble *d = field->data;

    for (guint k = nn; k; k--, g++, d++)
        values[*g] += (*d - zvalue[*g])*(*d - zvalue[*g]);
    for (guint gno = 0; gno <= ngrains; gno++)
        values[gno] = sqrt(values[gno]/sizes[gno]);
}

static void
calc_projected_area(GwyGrainValue *grainvalue,
                    const GwyField *field,
                    const guint *sizes)
{
    gdouble *values;
    if (!grainvalue
        || !check_target(grainvalue, &values, GWY_GRAIN_VALUE_PROJECTED_AREA))
        return;

    g_return_if_fail(sizes);
    guint ngrains = grainvalue->priv->ngrains;
    gdouble dxdy = gwy_field_dx(field)*gwy_field_dy(field);
    for (guint gno = 0; gno <= ngrains; gno++)
        values[gno] = dxdy*sizes[gno];
}

static void
calc_equiv_disc_radius(GwyGrainValue *grainvalue,
                       const GwyField *field,
                       const guint *sizes)
{
    gdouble *values;
    if (!grainvalue
        || !check_target(grainvalue, &values, GWY_GRAIN_VALUE_EQUIV_DISC_RADIUS))
        return;

    g_return_if_fail(sizes);
    guint ngrains = grainvalue->priv->ngrains;
    gdouble dxdy = gwy_field_dx(field)*gwy_field_dy(field);
    for (guint gno = 0; gno <= ngrains; gno++)
        values[gno] = sqrt(dxdy*sizes[gno]/G_PI);
}

// Calculate the ‘contribution of one corner’ to the surface area.
// Direction 1-2 is x, 1-4 is y, 3 is the opposite corner.
static inline gdouble
pixel_quarter_area_2(gdouble z1, gdouble z2, gdouble z4, gdouble z3,
                     gdouble dx, gdouble dy)
{
    gdouble d21 = (z2 - z1)/dx, d23 = (z2 - z3)/dy,
            d14 = (z1 - z4)/dy, d34 = (z3 - z4)/dx,
            d1423 = 0.75*d14 + 0.25*d23, d2134 = 0.75*d21 + 0.25*d34,
            D1423 = d1423*d1423, D2134 = d2134*d2134,
            D21 = 1.0 + d21*d21, D14 = 1.0 + d14*d14,
            Dv = 1.0 + 0.25*(d14 + d23)*(d14 + d23),
            Dh = 1.0 + 0.25*(d21 + d34)*(d21 + d34);

    return (sqrt(Dv + D2134) + sqrt(Dh + D1423)
            + sqrt(D21 + D1423) + sqrt(D14 + D2134));
}

static void
calc_surface_area(GwyGrainValue *grainvalue,
                  const guint *grains,
                  const GwyField *field)
{
    gdouble *values;
    if (!grainvalue
        || !check_target(grainvalue, &values, GWY_GRAIN_VALUE_SURFACE_AREA))
        return;

    guint xres = field->xres, yres = field->yres;
    gdouble dx = gwy_field_dx(field), dy = gwy_field_dy(field);
    gdouble dxdy = dx*dy;
    guint ngrains = grainvalue->priv->ngrains;
    const guint *g = grains;
    const gdouble *d = field->data;

    for (guint i = 0; i < yres; i++) {
        for (guint j = 0; j < xres; j++, g++) {
            guint gno = *g;
            if (!gno)
                continue;

            guint ix = i*xres;
            guint imx = (i > 0) ? ix-xres : ix;
            guint ipx = (i < yres-1) ? ix+xres : ix;
            guint jm = (j > 0) ? j-1 : j;
            guint jp = (j < xres-1) ? j+1 : j;

            values[gno] += pixel_quarter_area_2(d[ix + j], d[ix + jm],
                                                d[imx + j], d[imx + jm],
                                                dx, dy);
            values[gno] += pixel_quarter_area_2(d[ix + j], d[ix + jp],
                                                d[imx + j], d[imx + jp],
                                                dx, dy);
            values[gno] += pixel_quarter_area_2(d[ix + j], d[ix + jm],
                                                d[ipx + j], d[ipx + jm],
                                                dx, dy);
            values[gno] += pixel_quarter_area_2(d[ix + j], d[ix + jp],
                                                d[ipx + j], d[ipx + jp],
                                                dx, dy);
        }
    }

    for (guint gno = 0; gno <= ngrains; gno++)
        values[gno] *= dxdy/16.0;
}

static void
calc_half_height_area(GwyGrainValue *grainvalue,
                      const GwyGrainValue *mingrainvalue,
                      const GwyGrainValue *maxgrainvalue,
                      const guint *grains,
                      const GwyField *field)
{
    gdouble *values;
    const gdouble *min, *max;
    if (!grainvalue
        || !check_target(grainvalue, &values, GWY_GRAIN_VALUE_HALF_HEIGHT_AREA)
        || !check_dependence(mingrainvalue, &min, GWY_GRAIN_VALUE_MINIMUM)
        || !check_dependence(maxgrainvalue, &max, GWY_GRAIN_VALUE_MAXIMUM))
        return;

    guint nn = field->xres * field->yres;
    gdouble dxdy = gwy_field_dx(field)*gwy_field_dy(field);
    guint ngrains = grainvalue->priv->ngrains;
    const gdouble *d = field->data;

    // Find the grain half-heights, i.e. (z_min + z_max)/2.
    gdouble *zhalf = g_new(gdouble, ngrains + 1);
    for (guint gno = 0; gno <= ngrains; gno++)
        zhalf[gno] = 0.5*(min[gno] + max[gno]);

    // Calculate the area of pixels above the half-heights.
    guint *zhsizes = g_new0(guint, ngrains + 1);
    for (guint k = 0; k < nn; k++) {
        guint gno = grains[k];
        if (d[k] >= zhalf[gno])
            zhsizes[gno]++;
    }
    for (guint gno = 0; gno <= ngrains; gno++)
        values[gno] = dxdy*zhsizes[gno];

    g_free(zhalf);
    g_free(zhsizes);
}

static void
calc_flat_boundary_length(GwyGrainValue *grainvalue,
                          const guint *grains,
                          const GwyField *field)
{
    gdouble *values;
    if (!grainvalue
        || !check_target(grainvalue, &values,
                         GWY_GRAIN_VALUE_FLAT_BOUNDARY_LENGTH))
        return;

    guint xres = field->xres, yres = field->yres;
    gdouble dx = gwy_field_dx(field), dy = gwy_field_dy(field);
    gdouble diag = hypot(dx, dy);

    // Note the cycles go to xres and yres inclusive as we calculate the
    // boundary, not pixel interiors.
    for (guint i = 0; i <= yres; i++) {
        const guint *g = grains + i*xres - xres - 1;
        for (guint j = 0; j <= xres; j++, g++) {
            // Hope the compiler will optimize this mess...
            guint g1 = (i > 0 && j > 0) ? g[0] : 0;
            guint g2 = (i > 0 && j < xres) ? g[1] : 0;
            guint g3 = (i < yres && j > 0) ? g[xres] : 0;
            guint g4 = (i < yres && j < xres) ? g[xres + 1] : 0;
            guint f = !!g1 + !!g2 + !!g3 + !!g4;
            if (f == 0 || f == 4)
                continue;

            if (f == 1 || f == 3) {
                // Try to avoid too many if-thens by using the fact they
                // are all either zero or an identical value.
                values[g1 | g2 | g3 | g4] += diag/2.0;
            }
            else if (g1 && g4) {
                // This works for both g1 == g4 and g1 != g4.
                values[g1] += diag/2.0;
                values[g4] += diag/2.0;
            }
            else if (g2 && g3) {
                // This works for both g2 == g3 and g2 != g3.
                values[g2] += diag/2.0;
                values[g3] += diag/2.0;
            }
            else if (g1 == g2)
                values[g1 | g3] += dx;
            else if (g1 == g3)
                values[g1 | g2] += dy;
            else {
                g_assert_not_reached();
            }
        }
    }
}

static void
calc_boundary_extrema(GwyGrainValue *mingrainvalue,
                      GwyGrainValue *maxgrainvalue,
                      const guint *grains,
                      const GwyField *field)
{
    gdouble *minvalues, *maxvalues;
    if (all_null(2, NULL, mingrainvalue, maxgrainvalue)
        || !check_target(mingrainvalue, &minvalues,
                         GWY_GRAIN_VALUE_BOUNDARY_MINIMUM)
        || !check_target(maxgrainvalue, &maxvalues,
                         GWY_GRAIN_VALUE_BOUNDARY_MAXIMUM))
        return;

    guint xres = field->xres, yres = field->yres;
    const guint *g = grains;
    const gdouble *d = field->data;

    for (guint i = 0; i < yres; i++) {
        for (guint j = 0; j < xres; j++, d++, g++) {
            guint gno = *g;
            if (!gno)
                continue;

            if (i && j && i < yres-1 && j < xres - 1
                && grains[(i - 1)*xres + j] == gno
                && grains[i*xres + j - 1] == gno
                && grains[i*xres + j + 1] == gno
                && grains[(i + 1)*xres + j] == gno)
                continue;

            gdouble z = *d;
            if (minvalues && z < minvalues[gno])
                minvalues[gno] = z;
            if (maxvalues && z > maxvalues[gno])
                maxvalues[gno] = z;
        }
    }
}

/**
 * find_grain_convex_hull:
 * @xres: The number of columns in @grains.
 * @yres: The number of rows in @grains.
 * @grains: Grain numbers filled with gwy_data_field_number_grains().
 * @pos: Position of the top-left vertex of grain's convex hull.
 * @vertices: Array to fill with vertices.
 *
 * Finds vertices of a grain's convex hull.
 *
 * The grain is identified by @pos which must lie in a grain.
 *
 * The positions are returned as indices to vertex grid.  NB: The size of the
 * grid is (@xres + 1)*(@yres + 1), not @xres*@yres.
 *
 * The method is a bit naive, some atan2() calculations could be easily saved.
 **/
static void
find_grain_convex_hull(gint xres, gint yres,
                       const guint *grains,
                       gint pos,
                       GridPointList *vertices)
{
    enum { RIGHT = 0, DOWN, LEFT, UP } newdir = RIGHT, dir;
    g_return_if_fail(grains[pos]);
    gint initpos = pos;
    guint gno = grains[pos];
    GridPoint v = { .i = pos/xres, .j = pos % xres };
    vertices->len = 0;
    grid_point_list_add(vertices, v.j, v.i);

    do {
        dir = newdir;
        switch (dir) {
            case RIGHT:
            v.j++;
            if (v.i > 0 && v.j < xres && grains[(v.i-1)*xres + v.j] == gno)
                newdir = UP;
            else if (v.j < xres && grains[v.i*xres + v.j] == gno)
                newdir = RIGHT;
            else
                newdir = DOWN;
            break;

            case DOWN:
            v.i++;
            if (v.j < xres && v.i < yres && grains[v.i*xres + v.j] == gno)
                newdir = RIGHT;
            else if (v.i < yres && grains[v.i*xres + v.j-1] == gno)
                newdir = DOWN;
            else
                newdir = LEFT;
            break;

            case LEFT:
            v.j--;
            if (v.i < yres && v.j > 0 && grains[v.i*xres + v.j-1] == gno)
                newdir = DOWN;
            else if (v.j > 0 && grains[(v.i-1)*xres + v.j-1] == gno)
                newdir = LEFT;
            else
                newdir = UP;
            break;

            case UP:
            v.i--;
            if (v.j > 0 && v.i > 0 && grains[(v.i-1)*xres + v.j-1] == gno)
                newdir = LEFT;
            else if (v.i > 0 && grains[(v.i-1)*xres + v.j] == gno)
                newdir = UP;
            else
                newdir = RIGHT;
            break;

            default:
            g_assert_not_reached();
            break;
        }

        /* When we turn right, the previous point is a potential vertex, and
         * it can also supersed previous vertices. */
        if (newdir == (dir + 1) % 4) {
            grid_point_list_add(vertices, v.j, v.i);
            guint len = vertices->len;
            while (len > 2) {
                GridPoint *cur = vertices->points + (len-1);
                GridPoint *mid = vertices->points + (len-2);
                GridPoint *prev = vertices->points + (len-3);
                gdouble phi = atan2(cur->i - mid->i, cur->j - mid->j);
                gdouble phim = atan2(mid->i - prev->i, mid->j - prev->j);
                phi = fmod(phi - phim + 4.0*G_PI, 2.0*G_PI);
                /* This should be fairly safe as (a) not real harm is done
                 * when we have an occasional extra vertex (b) the greatest
                 * possible angle is G_PI/2.0 */
                if (phi > 1e-12 && phi < G_PI)
                    break;

                // Get rid of mid, it is in a locally concave part.
                vertices->points[len-2] = *cur;
                vertices->len--;
                len = vertices->len;
            }
        }
    } while (v.i*xres + v.j != initpos);

    // The last point is duplicated first point.
    vertices->len--;
}

/**
 * grain_maximum_bound:
 * @vertices: Convex hull vertex list.
 * @qx: Scale (pixel size) in x-direction.
 * @qy: Scale (pixel size) in y-direction.
 * @vx: Location to store vector x component to.
 * @vy: Location to store vector y component to.
 *
 * Given a list of integer convex hull vertices, return the vector between
 * the two most distance vertices.
 **/
static void
grain_maximum_bound(const GridPointList *vertices,
                    gdouble qx, gdouble qy,
                    gdouble *vx, gdouble *vy)
{
    gdouble vm = -G_MAXDOUBLE;
    for (guint g1 = 0; g1 < vertices->len; g1++) {
        const GridPoint *a = vertices->points + g1;
        for (guint g2 = g1 + 1; g2 < vertices->len; g2++) {
            const GridPoint *x = vertices->points + g2;
            gdouble dx = qx*(x->j - a->j);
            gdouble dy = qy*(x->i - a->i);
            gdouble v = dx*dx + dy*dy;
            if (v > vm) {
                vm = v;
                *vx = dx;
                *vy = dy;
            }
        }
    }
}

/**
 * grain_minimum_bound:
 * @vertices: Convex hull vertex list.
 * @qx: Scale (pixel size) in x-direction.
 * @qy: Scale (pixel size) in y-direction.
 * @vx: Location to store vector x component to.
 * @vy: Location to store vector y component to.
 *
 * Given a list of integer convex hull vertices, return the vector
 * corresponding to the minimum linear projection.
 **/
static void
grain_minimum_bound(const GridPointList *vertices,
                    gdouble qx, gdouble qy,
                    gdouble *vx, gdouble *vy)
{
    g_return_if_fail(vertices->len >= 3);
    gdouble vm = G_MAXDOUBLE;
    for (guint g1 = 0; g1 < vertices->len; g1++) {
        const GridPoint *a = vertices->points + g1;
        guint g1p = (g1 + 1) % vertices->len;
        const GridPoint *b = vertices->points + g1p;
        gdouble bx = qx*(b->j - a->j);
        gdouble by = qy*(b->i - a->i);
        gdouble b2 = bx*bx + by*by;
        gdouble vm1 = -G_MAXDOUBLE, vx1 = -G_MAXDOUBLE, vy1 = -G_MAXDOUBLE;
        for (guint g2 = 0; g2 < vertices->len; g2++) {
            const GridPoint *x = vertices->points + g2;
            gdouble dx = qx*(x->j - a->j);
            gdouble dy = qy*(x->i - a->i);
            gdouble s = (dx*bx + dy*by)/b2;
            dx -= s*bx;
            dy -= s*by;
            gdouble v = dx*dx + dy*dy;
            if (v > vm1) {
                vm1 = v;
                vx1 = dx;
                vy1 = dy;
            }
        }
        if (vm1 < vm) {
            vm = vm1;
            *vx = vx1;
            *vy = vy1;
        }
    }
}

static gdouble
grain_convex_hull_area(const GridPointList *vertices, gdouble dx, gdouble dy)
{
    g_return_val_if_fail(vertices->len >= 4, 0.0);

    const GridPoint *a = vertices->points,
                    *b = vertices->points + 1,
                    *c = vertices->points + 2;
    gdouble s = 0.0;

    for (guint i = 2; i < vertices->len; i++) {
        gdouble bx = b->j - a->j, by = b->i - a->i,
                cx = c->j - a->j, cy = c->i - a->i;
        s += 0.5*(bx*cy - by*cx);
        b = c;
        c++;
    }

    return dx*dy*s;
}

// By centre we mean centre of mass of its area.
static void
grain_convex_hull_centre(const GridPointList *vertices,
                         gdouble dx, gdouble dy,
                         gdouble *centrex, gdouble *centrey)
{
    g_return_if_fail(vertices->len >= 4);

    const GridPoint *a = vertices->points,
                    *b = vertices->points + 1,
                    *c = vertices->points + 2;
    gdouble s = 0.0, xc = 0.0, yc = 0.0;

    for (guint i = 2; i < vertices->len; i++) {
        gdouble bx = b->j - a->j, by = b->i - a->i,
                cx = c->j - a->j, cy = c->i - a->i;
        gdouble s1 = bx*cy - by*cx;
        xc += s1*(a->j + b->j + c->j);
        yc += s1*(a->i + b->i + c->i);
        s += s1;
        b = c;
        c++;
    }
    *centrex = xc*dx/(3.0*s);
    *centrey = yc*dy/(3.0*s);
}

static gdouble
minimize_circle_radius(FooscribedDisc *circle,
                       const GridPointList *vertices,
                       gdouble dx, gdouble dy)
{
    const GridPoint *v = vertices->points;
    gdouble x = circle->x, y = circle->y, r2best = 0.0;
    guint n = vertices->len;

    while (n--) {
        gdouble deltax = dx*v->j - x, deltay = dy*v->i - y;
        gdouble r2 = deltax*deltax + deltay*deltay;

        if (r2 > r2best)
            r2best = r2;

        v++;
    }

    return r2best;
}

static void
improve_circumscribed_circle(FooscribedDisc *circle,
                             const GridPointList *vertices,
                             gdouble dx, gdouble dy)
{
    gdouble eps = 1.0, improvement, qgeom = sqrt(dx*dy);

    do {
        FooscribedDisc best = *circle;

        improvement = 0.0;
        for (guint i = 0; i < NDIRECTIONS; i++) {
            FooscribedDisc cand;
            gdouble sx = eps*qgeom*shift_directions[2*i],
                    sy = eps*qgeom*shift_directions[2*i + 1];

            cand.size = circle->size;

            cand.x = circle->x + sx;
            cand.y = circle->y + sy;
            if ((cand.R2 = minimize_circle_radius(&cand, vertices, dx, dy))
                < best.R2)
                best = cand;

            cand.x = circle->x - sy;
            cand.y = circle->y + sx;
            if ((cand.R2 = minimize_circle_radius(&cand, vertices, dx, dy))
                < best.R2)
                best = cand;

            cand.x = circle->x - sx;
            cand.y = circle->y - sy;
            if ((cand.R2 = minimize_circle_radius(&cand, vertices, dx, dy))
                < best.R2)
                best = cand;

            cand.x = circle->x + sy;
            cand.y = circle->y - sx;
            if ((cand.R2 = minimize_circle_radius(&cand, vertices, dx, dy))
                < best.R2)
                best = cand;
        }
        if (best.R2 < circle->R2) {
            improvement = (best.R2 - circle->R2)/(dx*dy);
            *circle = best;
        }
        else {
            eps *= 0.5;
        }
    } while (eps > 1e-3 || improvement > 1e-3);
}

static void
calc_convex_hull(GwyGrainValue *minsizegrainvalue,
                 GwyGrainValue *minanglegrainvalue,
                 GwyGrainValue *maxsizegrainvalue,
                 GwyGrainValue *maxanglegrainvalue,
                 GwyGrainValue *chullareagrainvalue,
                 GwyGrainValue *excircrgrainvalue,
                 GwyGrainValue *excircxgrainvalue,
                 GwyGrainValue *excircygrainvalue,
                 const guint *grains,
                 const guint *anyboundpos,
                 const GwyField *field)
{
    guint ngrains;
    gdouble *minsizevalues, *maxsizevalues, *minanglevalues, *maxanglevalues,
            *chullareavalues, *excircrvalues, *excircxvalues, *excircyvalues;
    if (all_null(8, &ngrains, minsizegrainvalue, maxsizegrainvalue,
                 minanglegrainvalue, maxanglegrainvalue, chullareagrainvalue,
                 excircrgrainvalue, excircxgrainvalue, excircygrainvalue)
        || !check_target(minsizegrainvalue, &minsizevalues,
                         GWY_GRAIN_VALUE_MINIMUM_BOUND_SIZE)
        || !check_target(maxsizegrainvalue, &maxsizevalues,
                         GWY_GRAIN_VALUE_MAXIMUM_BOUND_SIZE)
        || !check_target(minanglegrainvalue, &minanglevalues,
                         GWY_GRAIN_VALUE_MINIMUM_BOUND_ANGLE)
        || !check_target(maxanglegrainvalue, &maxanglevalues,
                         GWY_GRAIN_VALUE_MAXIMUM_BOUND_ANGLE)
        || !check_target(chullareagrainvalue, &chullareavalues,
                         GWY_GRAIN_VALUE_CONVEX_HULL_AREA)
        || !check_target(excircrgrainvalue, &excircrvalues,
                         GWY_GRAIN_VALUE_CIRCUMCIRCLE_R)
        || !check_target(excircxgrainvalue, &excircxvalues,
                         GWY_GRAIN_VALUE_CIRCUMCIRCLE_X)
        || !check_target(excircygrainvalue, &excircyvalues,
                         GWY_GRAIN_VALUE_CIRCUMCIRCLE_Y))
        return;

    guint xres = field->xres, yres = field->yres;
    gdouble dx = gwy_field_dx(field), dy = gwy_field_dy(field);

    // Find the complete convex hulls.
    GridPointList *vertices = g_slice_new0(GridPointList);
    for (guint gno = 1; gno <= ngrains; gno++) {
        gdouble vx = dx, vy = dy;

        find_grain_convex_hull(xres, yres, grains, anyboundpos[gno], vertices);
        if (minsizevalues || minanglevalues) {
            grain_minimum_bound(vertices, dx, dy, &vx, &vy);
            if (minsizevalues)
                minsizevalues[gno] = hypot(vx, vy);
            if (minanglevalues)
                minanglevalues[gno] = gwy_standardize_direction(atan2(-vy, vx));
        }
        if (maxsizevalues || maxanglevalues) {
            grain_maximum_bound(vertices, dx, dy, &vx, &vy);
            if (maxsizevalues)
                maxsizevalues[gno] = hypot(vx, vy);
            if (maxanglevalues)
                maxanglevalues[gno] = gwy_standardize_direction(atan2(-vy, vx));
        }
        if (chullareavalues)
            chullareavalues[gno] = grain_convex_hull_area(vertices, dx, dy);
        if (excircrvalues || excircxvalues || excircyvalues) {
            FooscribedDisc circle = { 0.0, 0.0, 0.0, 0 };

            grain_convex_hull_centre(vertices, dx, dy, &circle.x, &circle.y);
            circle.R2 = minimize_circle_radius(&circle, vertices, dx, dy);
            improve_circumscribed_circle(&circle, vertices, dx, dy);

            if (excircrvalues)
                excircrvalues[gno] = sqrt(circle.R2);
            if (excircxvalues)
                excircxvalues[gno] = circle.x + field->xoff;
            if (excircyvalues)
                excircyvalues[gno] = circle.y + field->yoff;
        }
    }

    g_free(vertices->points);
    g_slice_free(GridPointList, vertices);
}

static guint*
extract_upsampled_square_pixel_grain(const guint *grains, guint xres,
                                     guint gno,
                                     const GwyFieldPart *bbox,
                                     guint *grain, guint *grainsize,
                                     guint *widthup, guint *heightup,
                                     gdouble dx, gdouble dy)
{
    guint col = bbox->col, row = bbox->row, w = bbox->width, h = bbox->height;
    guint w2 = 2*w, h2 = 2*h;

    /* Do not bother with nearly square pixels and upsample also 2×2. */
    if (fabs(log(dy/dx)) < 0.05) {
        grain = grain_maybe_realloc(grain, w2, h2, grainsize);
        for (guint i = 0; i < h; i++) {
            guint k2 = w2*(2*i);
            guint k = (i + row)*xres + col;
            for (guint j = 0; j < w; j++, k++, k2 += 2) {
                guint v = (grains[k] == gno) ? G_MAXUINT : 0;
                grain[k2] = v;
                grain[k2+1] = v;
                grain[k2 + w2] = v;
                grain[k2 + w2+1] = v;
            }
        }
    }
    else if (dy < dx) {
        /* Horizontal upsampling, precalculate index map to use in each row. */
        w2 = gwy_round(dx/dy*w2);
        grain = grain_maybe_realloc(grain, w2, h2, grainsize);
        guint *indices = (guint*)g_slice_alloc(w2*sizeof(guint));
        for (guint j = 0; j < w2; j++) {
            gint jj = gwy_round(0.5*j*dy/dx);
            indices[j] = CLAMP(jj, 0, (gint)w-1);
        }
        for (guint i = 0; i < h; i++) {
            guint k = (i + row)*xres + col;
            guint k2 = w2*(2*i);
            for (guint j = 0; j < w2; j++) {
                guint v = (grains[k + indices[j]] == gno) ? G_MAXUINT : 0;
                grain[k2 + j] = v;
                grain[k2 + w2 + j] = v;
            }
        }
        g_slice_free1(w2*sizeof(guint), indices);
    }
    else {
        /* Vertical upsampling, rows are 2× scaled copies but uneven. */
        h2 = gwy_round(dy/dx*h2);
        grain = grain_maybe_realloc(grain, w2, h2, grainsize);
        for (guint i = 0; i < h2; i++) {
            guint k, k2 = i*w2;
            gint ii = gwy_round(0.5*i*dx/dy);
            ii = CLAMP(ii, 0, (gint)h-1);
            k = (ii + row)*xres + col;
            for (guint j = 0; j < w; j++) {
                guint v = (grains[k + j] == gno) ? G_MAXUINT : 0;
                grain[k2 + 2*j] = v;
                grain[k2 + 2*j + 1] = v;
            }
        }
    }

    *widthup = w2;
    *heightup = h2;
    return grain;
}

/* Init @queue with all Von Neumann-neighbourhood boundary pixels. */
static void
init_erosion(guint *grain,
             guint width, guint height,
             GridPointList *queue)
{
    queue->len = 0;
    for (guint i = 0, k = 0; i < height; i++) {
        for (guint j = 0; j < width; j++, k++) {
            if (!grain[k])
                continue;

            if (!i || !grain[k - width]
                || !j || !grain[k - 1]
                || j == width-1 || !grain[k + 1]
                || i == height-1 || !grain[k + width]) {
                grain[k] = 1;
                grid_point_list_add(queue, j, i);
            }
        }
    }
}

static gboolean
erode_4(guint *grain,
        guint width, guint height,
        guint id,
        const GridPointList *inqueue,
        GridPointList *outqueue)
{
    const GridPoint *ipt = inqueue->points;

    outqueue->len = 0;
    for (guint m = inqueue->len; m; m--, ipt++) {
        guint i = ipt->i, j = ipt->j, k = i*width + j;

        if (i && grain[k - width] == G_MAXUINT) {
            grain[k - width] = id+1;
            grid_point_list_add(outqueue, j, i-1);
        }
        if (j && grain[k - 1] == G_MAXUINT) {
            grain[k - 1] = id+1;
            grid_point_list_add(outqueue, j-1, i);
        }
        if (j < width-1 && grain[k + 1] == G_MAXUINT) {
            grain[k + 1] = id+1;
            grid_point_list_add(outqueue, j+1, i);
        }
        if (i < height-1 && grain[k + width] == G_MAXUINT) {
            grain[k + width] = id+1;
            grid_point_list_add(outqueue, j, i+1);
        }
    }

    return outqueue->len;
}

static gboolean
erode_8(guint *grain,
        guint width, guint height,
        guint id,
        const GridPointList *inqueue,
        GridPointList *outqueue)
{
    const GridPoint *ipt = inqueue->points;

    outqueue->len = 0;
    for (guint m = inqueue->len; m; m--, ipt++) {
        guint i = ipt->i, j = ipt->j, k = i*width + j;
        if (i && j && grain[k - width - 1] == G_MAXUINT) {
            grain[k - width - 1] = id+1;
            grid_point_list_add(outqueue, j-1, i-1);
        }
        if (i && grain[k - width] == G_MAXUINT) {
            grain[k - width] = id+1;
            grid_point_list_add(outqueue, j, i-1);
        }
        if (i && j < width-1 && grain[k - width + 1] == G_MAXUINT) {
            grain[k - width + 1] = id+1;
            grid_point_list_add(outqueue, j+1, i-1);
        }
        if (j && grain[k - 1] == G_MAXUINT) {
            grain[k - 1] = id+1;
            grid_point_list_add(outqueue, j-1, i);
        }
        if (j < width-1 && grain[k + 1] == G_MAXUINT) {
            grain[k + 1] = id+1;
            grid_point_list_add(outqueue, j+1, i);
        }
        if (i < height-1 && j && grain[k + width - 1] == G_MAXUINT) {
            grain[k + width - 1] = id+1;
            grid_point_list_add(outqueue, j-1, i+1);
        }
        if (i < height-1 && grain[k + width] == G_MAXUINT) {
            grain[k + width] = id+1;
            grid_point_list_add(outqueue, j, i+1);
        }
        if (i < height-1 && j < width-1 && grain[k + width + 1] == G_MAXUINT) {
            grain[k + width + 1] = id+1;
            grid_point_list_add(outqueue, j+1, i+1);
        }
    }

    return outqueue->len;
}

static gint
compare_candidates(gconstpointer a,
                   gconstpointer b)
{
    const FooscribedDisc *da = (const FooscribedDisc*)a;
    const FooscribedDisc *db = (const FooscribedDisc*)b;

    if (da->size < db->size)
        return -1;
    if (da->size > db->size)
        return 1;

    if (da->R2 < db->R2)
        return -1;
    if (da->R2 > db->R2)
        return 1;

    return 0;
}

static void
find_disc_centre_candidates(GArray *candidates,
                            GridPointList *inqueue,
                            gdouble dx, gdouble dy,
                            gdouble centrex, gdouble centrey)
{
    /* Grain numbering for sparse areas, we reorder inqueue to contiguous
     * blocks, each getting the area number in candareas[]. */
    guint start = 0, end = 1;

    g_array_set_size(candidates, 0);
    for (guint m = 0; m < inqueue->len; m++) {
        GridPoint *mpt = inqueue->points + m;
        guint i = mpt->i, j = mpt->j;

        for (guint n = end; n < inqueue->len; n++) {
            GridPoint *npt = inqueue->points + n;
            guint ii = npt->i, jj = npt->j;

            if ((ii == i && (jj == j+1 || jj+1 == j))
                || (jj == j && (ii == i+1 || ii+1 == i))) {
                if (n != end)
                    GWY_SWAP(GridPoint, inqueue->points[end], *npt);
                end++;
            }
        }

        if (end == m+1 || end == inqueue->len) {
            FooscribedDisc cand = { 0.0, 0.0, 0.0, 0 };
            const GridPoint *npt = inqueue->points + start;

            for (guint n = end - start; n; n--, npt++) {
                cand.x += npt->j;
                cand.y += npt->i;
                cand.size++;
            }
            cand.x = (cand.x/cand.size + 0.5)*dx;
            cand.y = (cand.y/cand.size + 0.5)*dy;
            /* Use R2 temporarily for distance from the entire grain centre;
             * this is only for sorting below. */
            cand.R2 = ((cand.x - centrex)*(cand.x - centrex)
                        + (cand.y - centrey)*(cand.y - centrey));
            g_array_append_val(candidates, cand);
            if (end == inqueue->len)
                break;
            start = end;
            end++;
        }
    }

    g_array_sort(candidates, &compare_candidates);
}

static void
find_all_edges(EdgeList *edges,
               const guint *grains, guint xres,
               guint gno, const GwyFieldPart *bbox,
               gdouble dx, gdouble dy)
{
    guint col = bbox->col, row = bbox->row, w = bbox->width, h = bbox->height;
    guint *vertices = g_slice_alloc((w + 1)*sizeof(guint));

    memset(vertices, 0xff, sizeof(guint)*(w + 1));
    edges->len = 0;
    for (guint i = 0; i <= h; i++) {
        guint k = (i + row)*xres + col;
        guint vertex = G_MAXUINT;

        for (guint j = 0; j <= w; j++, k++) {
            /*
             * 1 2
             * 3 4
             */
            guint g0 = i && j && grains[k - xres - 1] == gno;
            guint g1 = i && j < w && grains[k - xres] == gno;
            guint g2 = i < h && j && grains[k - 1] == gno;
            guint g3 = i < h && j < w && grains[k] == gno;
            guint g = g0 | (g1 << 1) | (g2 << 2) | (g3 << 3);

            if (g == 8 || g == 7) {
                vertex = j;
                vertices[j] = i;
            }
            else if (g == 2 || g == 13) {
                edge_list_add(edges, dx*j, dy*vertices[j], dx*j, dy*i);
                vertex = j;
                vertices[j] = G_MAXUINT;
            }
            else if (g == 4 || g == 11) {
                edge_list_add(edges, dx*vertex, dy*i, dx*j, dy*i);
                vertex = G_MAXUINT;
                vertices[j] = i;
            }
            else if (g == 1 || g == 14) {
                edge_list_add(edges, dx*vertex, dy*i, dx*j, dy*i);
                edge_list_add(edges, dx*j, dy*vertices[j], dx*j, dy*i);
                vertex = G_MAXUINT;
                vertices[j] = G_MAXUINT;
            }
            else if (g == 6 || g == 9) {
                edge_list_add(edges, dx*vertex, dy*i, dx*j, dy*i);
                edge_list_add(edges, dx*j, dy*vertices[j], dx*j, dy*i);
                vertex = j;
                vertices[j] = i;
            }
        }
    }

    g_slice_free1((w + 1)*sizeof(guint), vertices);
}

static gdouble
maximize_disc_radius(FooscribedDisc *disc, Edge *edges, guint n)
{
    gdouble x = disc->x, y = disc->y, r2best = HUGE_VAL;

    while (n--) {
        gdouble rax = edges->xa - x, ray = edges->ya - y,
                rbx = edges->xb - x, rby = edges->yb - y,
                deltax = edges->xb - edges->xa, deltay = edges->yb - edges->ya;
        gdouble ca = -(deltax*rax + deltay*ray),
                cb = deltax*rbx + deltay*rby;

        if (ca <= 0.0)
            edges->r2 = rax*rax + ray*ray;
        else if (cb <= 0.0)
            edges->r2 = rbx*rbx + rby*rby;
        else {
            gdouble tx = cb*rax + ca*rbx, ty = cb*ray + ca*rby, D = ca + cb;
            edges->r2 = (tx*tx + ty*ty)/(D*D);
        }

        if (edges->r2 < r2best)
            r2best = edges->r2;
        edges++;
    }

    return r2best;
}

static guint
filter_relevant_edges(EdgeList *edges, gdouble r2, gdouble eps)
{
    Edge *edge = edges->edges, *enear = edges->edges;
    gdouble limitr = sqrt(r2) + 4.0*eps + 0.5, limit = limitr*limitr;

    for (guint i = edges->len; i; i--, edge++) {
        if (edge->r2 <= limit) {
            if (edge != enear)
                GWY_SWAP(Edge, *edge, *enear);
            enear++;
        }
    }

    return enear - edges->edges;
}

static void
improve_inscribed_disc(FooscribedDisc *disc, EdgeList *edges, guint dist)
{
    gdouble eps = 0.5 + 0.25*(dist > 4) + 0.25*(dist > 16), improvement;

    do {
        disc->R2 = maximize_disc_radius(disc, edges->edges, edges->len);
        FooscribedDisc best = *disc;
        guint nr = filter_relevant_edges(edges, best.R2, eps);

        improvement = 0.0;
        for (guint i = 0; i < NDIRECTIONS; i++) {
            FooscribedDisc cand = { .size = disc->size };
            gdouble sx = eps*shift_directions[2*i],
                    sy = eps*shift_directions[2*i + 1];

            cand.x = disc->x + sx;
            cand.y = disc->y + sy;
            if ((cand.R2 = maximize_disc_radius(&cand, edges->edges, nr))
                > best.R2)
                best = cand;

            cand.x = disc->x - sy;
            cand.y = disc->y + sx;
            if ((cand.R2 = maximize_disc_radius(&cand, edges->edges, nr))
                > best.R2)
                best = cand;

            cand.x = disc->x - sx;
            cand.y = disc->y - sy;
            if ((cand.R2 = maximize_disc_radius(&cand, edges->edges, nr))
                > best.R2)
                best = cand;

            cand.x = disc->x + sy;
            cand.y = disc->y - sx;
            if ((cand.R2 = maximize_disc_radius(&cand, edges->edges, nr))
                > best.R2)
                best = cand;
        }

        if (best.R2 > disc->R2) {
            improvement = sqrt(best.R2) - sqrt(disc->R2);
            *disc = best;
        }
        else {
            eps *= 0.5;
        }
    } while (eps > 1e-3 || improvement > 1e-3);
}

void
_gwy_mask_fied_grain_inscribed_discs(gdouble *inscrdrvalues,
                                     gdouble *inscrdxvalues,
                                     gdouble *inscrdyvalues,
                                     const gdouble *xvalues,
                                     const gdouble *yvalues,
                                     const guint *grains,
                                     const guint *sizes,
                                     guint ngrains,
                                     const GwyMaskField *mask,
                                     gdouble dx, gdouble dy)
{
    const GwyFieldPart *bbox = gwy_mask_field_grain_bounding_boxes(mask);
    guint xres = mask->xres;
    gdouble qgeom = sqrt(dx*dy);

    guint *grain = NULL;
    guint grainsize = 0;
    GridPointList *inqueue = g_slice_new0(GridPointList);
    GridPointList *outqueue = g_slice_new0(GridPointList);
    GArray *candidates = g_array_new(FALSE, FALSE, sizeof(FooscribedDisc));
    EdgeList edges = { 0, 0, NULL };
    FooscribedDisc *cand;

    /*
     * For each grain:
     *    Extract it, find all boundary pixels.
     *    Use (octagnoal) erosion to find disc centre candidate(s).
     *    For each candidate:
     *       Find maximum disc that fits with this centre.
     *       By expanding/moving try to find a larger disc until we cannot
     *       improve it.
     */
    for (guint gno = 1; gno <= ngrains; gno++) {
        guint w = bbox[gno].width, h = bbox[gno].height;
        gdouble xoff = dx*bbox[gno].col, yoff = dy*bbox[gno].row;

        /* If the grain is rectangular, calculate the disc directly.
         * Large rectangular grains are rare but the point is to catch
         * grains with width of height of 1 here. */
        if (sizes[gno] == w*h) {
            gdouble sdx = 0.5*w*dx, sdy = 0.5*h*dy;
            if (inscrdrvalues)
                inscrdrvalues[gno] = 0.999999*MIN(sdx, sdy);
            if (inscrdxvalues)
                inscrdxvalues[gno] = sdx + xoff;
            if (inscrdyvalues)
                inscrdyvalues[gno] = sdy + yoff;
            continue;
        }

        /* Upsampling twice combined with octagonal erosion has the nice
         * property that we get candidate pixels in places such as corners
         * or junctions of one-pixel thin lines. */
        guint width, height;
        grain = extract_upsampled_square_pixel_grain(grains, xres, gno,
                                                     bbox + gno,
                                                     grain, &grainsize,
                                                     &width, &height,
                                                     dx, dy);
        /* Size of upsamples pixel in original squeezed pixel coordinates.
         * Normally equal to 1/2 and always approximately 1:1. */
        gdouble sdx = w*(dx/qgeom)/width;
        gdouble sdy = h*(dy/qgeom)/height;
        /* Grain centre in squeezed pixel coordinates within the bbox. */
        gdouble centrex = (xvalues[gno] + 0.5)*(dx/qgeom);
        gdouble centrey = (yvalues[gno] + 0.5)*(dy/qgeom);

        guint dist = 1;
        init_erosion(grain, width, height, inqueue);
        while (TRUE) {
            if (!erode_8(grain, width, height, dist, inqueue, outqueue))
                break;
            GWY_SWAP(GridPointList*, inqueue, outqueue);
            dist++;

            if (!erode_4(grain, width, height, dist, inqueue, outqueue))
                break;
            GWY_SWAP(GridPointList*, inqueue, outqueue);
            dist++;
        }
#if 0
        for (guint i = 0; i < height; i++) {
            for (guint j = 0; j < width; j++) {
                if (!grain[i*width + j])
                    g_printerr("..");
                else
                    g_printerr("%02u", grain[i*width + j]);
                g_printerr("%c", j == width-1 ? '\n' : ' ');
            }
        }
#endif
        /* Now inqueue is always non-empty and contains max-distance
         * pixels of the upscaled grain. */
        find_disc_centre_candidates(candidates, inqueue, sdx, sdy,
                                    centrex, centrey);
        find_all_edges(&edges, grains, xres, gno, bbox + gno,
                       dx/qgeom, dy/qgeom);

        /* Try a few first candidates for the inscribed disc centre. */
        guint ncand = MIN(7, candidates->len);
        for (guint i = 0; i < ncand; i++) {
            cand = &g_array_index(candidates, FooscribedDisc, i);
            improve_inscribed_disc(cand, &edges, dist);
        }

        cand = &g_array_index(candidates, FooscribedDisc, 0);
        for (guint i = 1; i < ncand; i++) {
            if (g_array_index(candidates, FooscribedDisc, i).R2 > cand->R2)
                cand = &g_array_index(candidates, FooscribedDisc, i);
        }

        if (inscrdrvalues)
            inscrdrvalues[gno] = sqrt(cand->R2)*qgeom;
        if (inscrdxvalues)
            inscrdxvalues[gno] = cand->x*qgeom + xoff;
        if (inscrdyvalues)
            inscrdyvalues[gno] = cand->y*qgeom + yoff;
    }

    g_free(grain);
    g_free(inqueue->points);
    g_free(outqueue->points);
    g_slice_free(GridPointList, inqueue);
    g_slice_free(GridPointList, outqueue);
    g_free(edges.edges);
    g_array_free(candidates, TRUE);
}

static void
calc_inscribed_disc(GwyGrainValue *inscrdrgrainvalue,
                    GwyGrainValue *inscrdxgrainvalue,
                    GwyGrainValue *inscrdygrainvalue,
                    const GwyGrainValue *xgrainvalue,
                    const GwyGrainValue *ygrainvalue,
                    const guint *grains,
                    const guint *sizes,
                    const GwyMaskField *mask,
                    const GwyField *field)
{
    guint ngrains;
    const gdouble *xvalues, *yvalues;
    gdouble *inscrdrvalues, *inscrdxvalues, *inscrdyvalues;
    if (all_null(3, &ngrains,
                 inscrdrgrainvalue, inscrdxgrainvalue, inscrdygrainvalue)
        || !check_target(inscrdrgrainvalue, &inscrdrvalues,
                         GWY_GRAIN_VALUE_INSCRIBED_DISC_R)
        || !check_target(inscrdxgrainvalue, &inscrdxvalues,
                         GWY_GRAIN_VALUE_INSCRIBED_DISC_X)
        || !check_target(inscrdygrainvalue, &inscrdyvalues,
                         GWY_GRAIN_VALUE_INSCRIBED_DISC_Y)
        || !check_dependence(xgrainvalue, &xvalues, GWY_GRAIN_VALUE_CENTER_X)
        || !check_dependence(ygrainvalue, &yvalues, GWY_GRAIN_VALUE_CENTER_Y))
        return;

    _gwy_mask_fied_grain_inscribed_discs(inscrdrvalues, inscrdxvalues,
                                         inscrdyvalues,
                                         xvalues, yvalues,
                                         grains, sizes, ngrains, mask,
                                         gwy_field_dx(field),
                                         gwy_field_dy(field));

    if (inscrdxvalues) {
        gdouble off = field->xoff;
        for (guint i = 1; i <= ngrains; i++)
            inscrdxvalues[i] += off;
    }
    if (inscrdyvalues) {
        gdouble off = field->yoff;
        for (guint i = 1; i <= ngrains; i++)
            inscrdyvalues[i] += off;
    }
}

static void
calc_mean_radius(GwyGrainValue *grainvalue,
                 const GwyGrainValue *xgrainvalue,
                 const GwyGrainValue *ygrainvalue,
                 const guint *grains,
                 const GwyField *field)
{
    gdouble *values;
    const gdouble *xvalues, *yvalues;
    if (!grainvalue
        || !check_target(grainvalue, &values, GWY_GRAIN_VALUE_MEAN_RADIUS)
        || !check_dependence(xgrainvalue, &xvalues, GWY_GRAIN_VALUE_CENTER_X)
        || !check_dependence(ygrainvalue, &yvalues, GWY_GRAIN_VALUE_CENTER_Y))
        return;

    guint xres = field->xres, yres = field->yres;
    gdouble dx = gwy_field_dx(field), dy = gwy_field_dy(field);
    guint ngrains = grainvalue->priv->ngrains;
    guint *blen = g_new0(guint, ngrains+1);

    for (guint i = 0, k = 0; i < yres; i++) {
        for (guint j = 0; j < xres; j++, k++) {
            guint gno = grains[k];

            if (!gno)
                continue;

            gdouble xc = xvalues[gno], yc = yvalues[gno];
            if (!i || !grains[k - xres]) {
                values[gno] += hypot(dx*(j+0.5 - xc), dy*(i - yc));
                values[gno] += hypot(dx*(j+1 - xc), dy*(i - yc));
                blen[gno]++;
            }
            if (!j || !grains[k-1]) {
                values[gno] += hypot(dx*(j - xc), dy*(i - yc));
                values[gno] += hypot(dx*(j - xc), dy*(i+0.5 - yc));
                blen[gno]++;
            }
            if (j == xres-1 || !grains[k+1]) {
                values[gno] += hypot(dx*(j+1 - xc), dy*(i+0.5 - yc));
                values[gno] += hypot(dx*(j+1 - xc), dy*(i+1 - yc));
                blen[gno]++;
            }
            if (i == yres-1 || !grains[k + xres]) {
                values[gno] += hypot(dx*(j - xc), dy*(i+1 - yc));
                values[gno] += hypot(dx*(j+0.5 - xc), dy*(i+1 - yc));
                blen[gno]++;
            }
        }
    }

    for (guint gno = 1; gno <= ngrains; gno++)
        values[gno] /= 2*blen[gno];

    g_free(blen);
}

static void
calc_volume_min(GwyGrainValue *grainvalue,
                const GwyGrainValue *mingrainvalue,
                const GwyGrainValue *v0grainvalue,
                const guint *sizes,
                const GwyField *field)
{
    gdouble *values;
    const gdouble *min, *v0;
    if (!grainvalue
        || !check_target(grainvalue, &values, GWY_GRAIN_VALUE_VOLUME_MIN)
        || !check_dependence(mingrainvalue, &min, GWY_GRAIN_VALUE_MINIMUM)
        || !check_dependence(v0grainvalue, &v0, GWY_GRAIN_VALUE_VOLUME_0))
        return;

    gdouble dxdy = gwy_field_dx(field)*gwy_field_dy(field);
    guint ngrains = grainvalue->priv->ngrains;
    for (guint gno = 0; gno <= ngrains; gno++)
        values[gno] = v0[gno] - dxdy*sizes[gno]*min[gno];
}

static void
calc_volume_laplace(GwyGrainValue *grainvalue,
                    const GwyGrainValue *v0grainvalue,
                    const guint *grains,
                    const GwyMaskField *mask,
                    const GwyField *field)
{
    gdouble *values;
    const gdouble *v0;
    if (!grainvalue
        || !check_target(grainvalue, &values, GWY_GRAIN_VALUE_VOLUME_LAPLACE)
        || !check_dependence(v0grainvalue, &v0, GWY_GRAIN_VALUE_VOLUME_0))
        return;

    guint ngrains = grainvalue->priv->ngrains;
    GwyField *workspace = gwy_field_duplicate(field);
    gwy_field_laplace_solve(workspace, mask, G_MAXUINT);

    GwyGrainValue *laplacebase = gwy_grain_value_new(v0grainvalue->priv->name);
    g_assert(laplacebase);

    _gwy_grain_value_set_size(laplacebase, ngrains);
    init_values(laplacebase);
    calc_volume_0(laplacebase, grains, workspace);
    const gdouble *lbv = laplacebase->priv->values;
    for (guint gno = 0; gno <= ngrains; gno++)
        values[gno] = v0[gno] - lbv[gno];

    g_object_unref(laplacebase);
    g_object_unref(workspace);
}

static void
calc_slope(GwyGrainValue *thetagrainvalue,
           GwyGrainValue *phigrainvalue,
           const gdouble *linear,
           const GwyField *field)
{
    guint ngrains;
    gdouble *thetavalues, *phivalues;
    if (all_null(2, &ngrains, thetagrainvalue, phigrainvalue)
        || !check_target(thetagrainvalue, &thetavalues,
                         GWY_GRAIN_VALUE_SLOPE_THETA)
        || !check_target(phigrainvalue, &phivalues, GWY_GRAIN_VALUE_SLOPE_PHI))
        return;

    gdouble dx = gwy_field_dx(field), dy = gwy_field_dy(field);

    for (guint gno = 1; gno <= ngrains; gno++) {
        const gdouble *lin = linear + 5*gno;
        gdouble xx = lin[0];
        gdouble xy = lin[1];
        gdouble yy = lin[2];
        gdouble xz = lin[3];
        gdouble yz = lin[4];
        gdouble det = xx*yy - xy*xy;
        if (det) {
            gdouble bx = (xz*yy - xy*yz)/(dx*det);
            gdouble by = (yz*xx - xy*xz)/(dy*det);
            if (thetavalues)
                thetavalues[gno] = atan(hypot(bx, by));
            if (phivalues)
                phivalues[gno] = atan2(by, -bx);
        }
        // Zero is preset as fillvalue.
    }
}

static void
calc_curvature(GwyGrainValue *xcgrainvalue,
               GwyGrainValue *ycgrainvalue,
               GwyGrainValue *zcgrainvalue,
               GwyGrainValue *c1grainvalue,
               GwyGrainValue *c2grainvalue,
               GwyGrainValue *a1grainvalue,
               GwyGrainValue *a2grainvalue,
               const GwyGrainValue *xgrainvalue,
               const GwyGrainValue *ygrainvalue,
               const GwyGrainValue *zgrainvalue,
               const guint *sizes,
               const gdouble *linear,
               const gdouble *quadratic,
               const GwyField *field)
{
    guint ngrains;
    gdouble *xcvalues, *ycvalues, *zcvalues,
            *c1values, *c2values, *a1values, *a2values;
    const gdouble *xvalues, *yvalues, *zvalues;
    if (all_null(7, &ngrains, xcgrainvalue, ycgrainvalue, zcgrainvalue,
                 c1grainvalue, c2grainvalue, a1grainvalue, a2grainvalue)
        || !check_target(xcgrainvalue, &xcvalues,
                         GWY_GRAIN_VALUE_CURVATURE_CENTER_X)
        || !check_target(ycgrainvalue, &ycvalues,
                         GWY_GRAIN_VALUE_CURVATURE_CENTER_Y)
        || !check_target(zcgrainvalue, &zcvalues,
                         GWY_GRAIN_VALUE_CURVATURE_CENTER_Z)
        || !check_target(c1grainvalue, &c1values, GWY_GRAIN_VALUE_CURVATURE1)
        || !check_target(c2grainvalue, &c2values, GWY_GRAIN_VALUE_CURVATURE2)
        || !check_target(a1grainvalue, &a1values,
                         GWY_GRAIN_VALUE_CURVATURE_ANGLE1)
        || !check_target(a2grainvalue, &a2values,
                         GWY_GRAIN_VALUE_CURVATURE_ANGLE2)
        || !check_dependence(xgrainvalue, &xvalues, GWY_GRAIN_VALUE_CENTER_X)
        || !check_dependence(ygrainvalue, &yvalues, GWY_GRAIN_VALUE_CENTER_Y)
        || !check_dependence(zgrainvalue, &zvalues, GWY_GRAIN_VALUE_MEAN))
        return;

    g_return_if_fail(sizes);
    g_return_if_fail(linear);
    g_return_if_fail(quadratic);

    // q is used for transformation from square pixels to coordinates with
    // correct aspect ratio and pixel area of 1; s is then the remaining
    // uniform scaling factor.
    gdouble dx = gwy_field_dx(field), dy = gwy_field_dy(field);
    gdouble xoff = 0.5*dx + field->xoff, yoff = 0.5*dy + field->yoff;
    gdouble s = sqrt(dx*dy), q = sqrt(dy/dx);

    for (guint gno = 1; gno <= ngrains; gno++) {
        /* a:
         *  0 [<1>
         *  1  <x>   <x²>
         *  3  <y>   <xy>   <y²>
         *  6  <x²>  <x³>   <x²y>  <x⁴>
         * 10  <xy>  <x²y>  <xy²>  <x³y>   <x²y²>
         * 15  <y²>  <xy²>  <y³>   <x²y²>  <xy³>   <y⁴>]
         * b: [<z>  <xz>  <yz>  <x²z>  <xyz>  <y²z>]
         */
        gdouble a[21], b[6];
        const gdouble *lin = linear + 5*gno, *quad = quadratic + 12*gno;
        guint n = sizes[gno];

        gwy_clear(b, 6);
        if (n >= 6) {
            a[0] = n;
            a[1] = a[3] = 0.0;
            a[2] = a[6] = lin[0];
            a[4] = a[10] = lin[1];
            a[5] = a[15] = lin[2];
            a[7] = quad[0];
            a[8] = a[11] = quad[1];
            a[9] = quad[4];
            a[12] = a[16] = quad[2];
            a[13] = quad[5];
            a[14] = a[18] = quad[6];
            a[17] = quad[3];
            a[19] = quad[7];
            a[20] = quad[8];

            if (gwy_cholesky_condition(a, 6) < 1e9) {
                gwy_cholesky_decompose(a, 6);
                b[0] = 0;
                b[1] = lin[3];
                b[2] = lin[4];
                b[3] = quad[9];
                b[4] = quad[10];
                b[5] = quad[11];
                gwy_cholesky_solve(a, b, 6);
                b[1] *= q;
                b[2] /= q;
                b[3] *= q*q;
                b[5] /= q*q;
            }
        }

        GwyCurvatureParams curvature;
        gwy_math_curvature(b, &curvature);

        if (c1values)
            c1values[gno] = curvature.k1/(s*s);
        if (c2values)
            c2values[gno] = curvature.k2/(s*s);
        if (a1values)
            a1values[gno] = curvature.phi1;
        if (a2values)
            a2values[gno] = curvature.phi2;
        if (xcvalues)
            xcvalues[gno] = s*curvature.xc + dx*xvalues[gno] + xoff;
        if (ycvalues)
            ycvalues[gno] = s*curvature.yc + dy*yvalues[gno] + yoff;
        if (zcvalues)
            zcvalues[gno] = curvature.zc + zvalues[gno];
    }
}

static void
linear_transform(GwyGrainValue *grainvalue,
                 gdouble q,
                 gdouble c)
{
    if (!grainvalue)
        return;

    guint ngrains = grainvalue->priv->ngrains;
    gdouble *values = grainvalue->priv->values;
    for (guint i = 0; i <= ngrains; i++)
        values[i] = q*values[i] + c;
}

void
_gwy_grain_value_evaluate_builtins(const GwyField *field,
                                   const GwyMaskField *mask,
                                   GwyGrainValue **grainvalues,
                                   guint nvalues)
{
    guint ngrains = gwy_mask_field_n_grains(mask);
    const guint *grains = gwy_mask_field_grain_numbers(mask);
    gdouble dx = gwy_field_dx(field), dy = gwy_field_dy(field);

    for (guint i = 0; i < nvalues; i++) {
        GwyGrainValue *grainvalue = grainvalues[i];
        GrainValue *priv = grainvalue->priv;
        g_return_if_fail(priv->is_valid && priv->builtin);
        g_return_if_fail(priv->builtin->id < GWY_GRAIN_NVALUES);
        _gwy_grain_value_set_size(grainvalue, ngrains);
    }

    // Figure out which quantities are requested and the auxiliary data to
    // calculate.
    GwyGrainValue **ourvalues = g_new0(GwyGrainValue*, GWY_GRAIN_NVALUES);
    guint need = 0;
    for (guint i = 0; i < nvalues; i++) {
        GwyGrainValue *grainvalue = grainvalues[i];
        BuiltinGrainValueId id = grainvalue->priv->builtin->id;
        need |= grainvalue->priv->builtin->need;

        // Take the first if the same quantity is requested multiple times.
        // We will deal with this later.
        if (!ourvalues[id])
            ourvalues[id] = g_object_ref(grainvalue);
    }

    // Integer data.
    const guint *sizes = (((need & NEED_SIZE) == NEED_SIZE)
                          ? gwy_mask_field_grain_sizes(mask) : NULL);

    guint *anyboundpos = NULL;
    if ((need & NEED_ANYBOUNDPOS) == NEED_ANYBOUNDPOS) {
        anyboundpos = g_new(guint, ngrains + 1);
        memset(anyboundpos, 0xff, (ngrains + 1)*sizeof(guint));
    }

    // Floating point data that coincide with some grain value.  If it is
    // requested by caller we use that otherwise create a new GwyGrainValue.
    for (guint i = 0; i < G_N_ELEMENTS(satisfies_needs); i++) {
        if (satisfies_needs[i] != G_MAXUINT && (need & (1 << i)))
            ensure_value(satisfies_needs[i], ourvalues, ngrains);
    }

    for (guint i = 0; i < GWY_GRAIN_NVALUES; i++) {
        if (ourvalues[i])
            init_values(ourvalues[i]);
    }

    // Non-scalar auxiliary floating point data.
    gdouble *linear = (((need & NEED_LINEAR) == NEED_LINEAR)
                       ? g_new0(gdouble, 5*(ngrains + 1)) : NULL);

    gdouble *quadratic = (((need & NEED_QUADRATIC) == NEED_QUADRATIC)
                          ?  g_new0(gdouble, 12*(ngrains + 1)) : NULL);

    calc_anyboundpos(anyboundpos, grains, field);
    calc_minimum(ourvalues[GWY_GRAIN_VALUE_MINIMUM], grains, field);
    calc_maximum(ourvalues[GWY_GRAIN_VALUE_MAXIMUM], grains, field);
    calc_centre_x(ourvalues[GWY_GRAIN_VALUE_CENTER_X], grains, sizes, field);
    calc_centre_y(ourvalues[GWY_GRAIN_VALUE_CENTER_Y], grains, sizes, field);
    calc_mean(ourvalues[GWY_GRAIN_VALUE_MEAN], grains, sizes, field);
    calc_volume_0(ourvalues[GWY_GRAIN_VALUE_VOLUME_0], grains, field);
    calc_linear(linear, grains,
                ourvalues[GWY_GRAIN_VALUE_CENTER_X],
                ourvalues[GWY_GRAIN_VALUE_CENTER_Y],
                ourvalues[GWY_GRAIN_VALUE_MEAN],
                field);
    calc_quadratic(quadratic, grains,
                   ourvalues[GWY_GRAIN_VALUE_CENTER_X],
                   ourvalues[GWY_GRAIN_VALUE_CENTER_Y],
                   ourvalues[GWY_GRAIN_VALUE_MEAN],
                   field);

    // Values that do not directly correspond to auxiliary values and may
    // depend on them.
    calc_projected_area(ourvalues[GWY_GRAIN_VALUE_PROJECTED_AREA],
                        field, sizes);
    calc_equiv_disc_radius(ourvalues[GWY_GRAIN_VALUE_EQUIV_DISC_RADIUS],
                           field, sizes);
    calc_surface_area(ourvalues[GWY_GRAIN_VALUE_SURFACE_AREA], grains, field);
    calc_half_height_area(ourvalues[GWY_GRAIN_VALUE_HALF_HEIGHT_AREA],
                          ourvalues[GWY_GRAIN_VALUE_MINIMUM],
                          ourvalues[GWY_GRAIN_VALUE_MAXIMUM],
                          grains, field);
    calc_median(ourvalues[GWY_GRAIN_VALUE_MEDIAN], grains, sizes, field);
    calc_rms_intra(ourvalues[GWY_GRAIN_VALUE_RMS_INTRA],
                   ourvalues[GWY_GRAIN_VALUE_MEAN],
                   grains, sizes, field);
    calc_flat_boundary_length(ourvalues[GWY_GRAIN_VALUE_FLAT_BOUNDARY_LENGTH],
                              grains, field);
    calc_boundary_extrema(ourvalues[GWY_GRAIN_VALUE_BOUNDARY_MINIMUM],
                          ourvalues[GWY_GRAIN_VALUE_BOUNDARY_MAXIMUM],
                          grains, field);
    calc_convex_hull(ourvalues[GWY_GRAIN_VALUE_MINIMUM_BOUND_SIZE],
                     ourvalues[GWY_GRAIN_VALUE_MINIMUM_BOUND_ANGLE],
                     ourvalues[GWY_GRAIN_VALUE_MAXIMUM_BOUND_SIZE],
                     ourvalues[GWY_GRAIN_VALUE_MAXIMUM_BOUND_ANGLE],
                     ourvalues[GWY_GRAIN_VALUE_CONVEX_HULL_AREA],
                     ourvalues[GWY_GRAIN_VALUE_CIRCUMCIRCLE_R],
                     ourvalues[GWY_GRAIN_VALUE_CIRCUMCIRCLE_X],
                     ourvalues[GWY_GRAIN_VALUE_CIRCUMCIRCLE_Y],
                     grains, anyboundpos, field);
    calc_inscribed_disc(ourvalues[GWY_GRAIN_VALUE_INSCRIBED_DISC_R],
                        ourvalues[GWY_GRAIN_VALUE_INSCRIBED_DISC_X],
                        ourvalues[GWY_GRAIN_VALUE_INSCRIBED_DISC_Y],
                        ourvalues[GWY_GRAIN_VALUE_CENTER_X],
                        ourvalues[GWY_GRAIN_VALUE_CENTER_Y],
                        grains, sizes, mask, field);
    calc_mean_radius(ourvalues[GWY_GRAIN_VALUE_MEAN_RADIUS],
                     ourvalues[GWY_GRAIN_VALUE_CENTER_X],
                     ourvalues[GWY_GRAIN_VALUE_CENTER_Y],
                     grains, field);
    calc_volume_min(ourvalues[GWY_GRAIN_VALUE_VOLUME_MIN],
                    ourvalues[GWY_GRAIN_VALUE_MINIMUM],
                    ourvalues[GWY_GRAIN_VALUE_VOLUME_0],
                    sizes, field);
    calc_volume_laplace(ourvalues[GWY_GRAIN_VALUE_VOLUME_LAPLACE],
                        ourvalues[GWY_GRAIN_VALUE_VOLUME_0],
                        grains, mask, field);
    calc_slope(ourvalues[GWY_GRAIN_VALUE_SLOPE_THETA],
               ourvalues[GWY_GRAIN_VALUE_SLOPE_PHI],
               linear, field);
    calc_curvature(ourvalues[GWY_GRAIN_VALUE_CURVATURE_CENTER_X],
                   ourvalues[GWY_GRAIN_VALUE_CURVATURE_CENTER_Y],
                   ourvalues[GWY_GRAIN_VALUE_CURVATURE_CENTER_Z],
                   ourvalues[GWY_GRAIN_VALUE_CURVATURE1],
                   ourvalues[GWY_GRAIN_VALUE_CURVATURE2],
                   ourvalues[GWY_GRAIN_VALUE_CURVATURE_ANGLE1],
                   ourvalues[GWY_GRAIN_VALUE_CURVATURE_ANGLE2],
                   ourvalues[GWY_GRAIN_VALUE_CENTER_X],
                   ourvalues[GWY_GRAIN_VALUE_CENTER_Y],
                   ourvalues[GWY_GRAIN_VALUE_MEAN],
                   sizes, linear, quadratic, field);

    // NB: This must be done last because other functions expect coordinates
    // in pixels.
    linear_transform(ourvalues[GWY_GRAIN_VALUE_CENTER_X],
                     dx, 0.5*dx + field->xoff);
    linear_transform(ourvalues[GWY_GRAIN_VALUE_CENTER_Y],
                     dy, 0.5*dy + field->yoff);

    // Copy data to all other instances of the same grain value and set units.
    GwyUnit *unitxy = gwy_field_get_unit_xy(field);
    GwyUnit *unitz = gwy_field_get_unit_z(field);
    for (guint i = 0; i < nvalues; i++) {
        GwyGrainValue *grainvalue = grainvalues[i];
        GrainValue *priv = grainvalue->priv;
        const BuiltinGrainValue *builtin = priv->builtin;
        BuiltinGrainValueId id = builtin->id;

        if (grainvalue == ourvalues[id]) {
            if (!priv->unit)
                priv->unit = gwy_unit_new();
            gwy_unit_power_multiply(priv->unit,
                                    unitxy, builtin->powerxy,
                                    unitz, builtin->powerz);
        }
        else
            _gwy_grain_value_assign(grainvalue, ourvalues[i]);
    }

    for (guint i = 0; i < GWY_GRAIN_NVALUES; i++)
        GWY_OBJECT_UNREF(ourvalues[i]);

    g_free(ourvalues);
    GWY_FREE(anyboundpos);
    GWY_FREE(quadratic);
    GWY_FREE(linear);
}

void
_gwy_grain_value_setup_builtins(BuiltinGrainValueTable *builtins)
{
    builtins->table = g_hash_table_new(g_str_hash, g_str_equal);
    builtins->n = GWY_GRAIN_NVALUES;
    builtins->names = g_new(const gchar*, builtins->n + 1);
    builtins->idents = g_new(const gchar*, builtins->n + 1);
    for (guint i = 0; i < GWY_GRAIN_NVALUES; i++) {
        const BuiltinGrainValue *builtin = builtin_table + i;
        g_assert(builtin->id == i);
        g_hash_table_insert(builtins->table,
                            (gpointer)builtin->name, (gpointer)builtin);
        builtins->names[i] = builtin->name;
        builtins->idents[i] = builtin->ident;
    }
    builtins->names[builtins->n] = NULL;
    builtins->idents[builtins->n] = NULL;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
