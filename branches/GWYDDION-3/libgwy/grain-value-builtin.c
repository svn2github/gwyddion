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

enum {
    NEED_SIZE = 1 << 0,
    NEED_ANYBOUNDPOS = 1 << 1,
    NEED_MIN = 1 << 2,
    NEED_MAX = 1 << 3,
    NEED_XMEAN = (1 << 4) | NEED_SIZE,
    NEED_YMEAN = (1 << 5) | NEED_SIZE,
    NEED_ZMEAN = (1 << 6) | NEED_SIZE,
    NEED_LINEAR = (1 << 7) | NEED_ZMEAN | NEED_XMEAN | NEED_YMEAN,
    NEED_QUADRATIC = (1 << 8) | NEED_LINEAR,
    NEED_VOLUME = (1 << 9),
};

// Must be signed, we use signed differences.
typedef struct {
    gint i;
    gint j;
} GridPoint;

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

            gdouble v = (52.0*d[ix + j] + 10.0*(d[imx + j] + d[ix + jm]
                                                + d[ix + jp] + d[ipx + j])
                         + (d[imx + jm] + d[imx + jp]
                            + d[ipx + jm] + d[ipx + jp]));

            values[gno] += v;
        }
    }
    for (guint gno = 1; gno <= ngrains; gno++)
        values[gno] *= dxdy/96.0;
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

// Calculate twice the ‘contribution of one corner’ (the twice is to move
// multiplications from inner loops) to the surface area.
// Direction 1-2 is x, 1-4 is y.
static inline gdouble
square_area2w_1c(gdouble z1, gdouble z2, gdouble z4, gdouble c,
                 gdouble xx, gdouble yy)
{
    gdouble z12 = z1 - z2,
            z14 = z1 - z4,
            z12c = z1 + z2 - c,
            z14c = z1 + z4 - c;
    return sqrt(1.0 + z12*z12/xx + z12c*z12c/yy)
           + sqrt(1.0 + z14*z14/yy + z14c*z14c/xx);
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
    gdouble dx2 = dx*dx, dy2 = dy*dy, dxdy = dx*dy;
    guint ngrains = grainvalue->priv->ngrains;
    const guint *g = grains;
    const gdouble *d = field->data;

    // Every contribution is calculated twice -- for each pixel (vertex)
    // participating to a particular triangle.  So we divide by 8, not by 4.
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
            gdouble c;

            c = 0.5*(d[ix + j] + d[ix + jm] + d[imx + jm] + d[imx + j]);
            values[gno] += square_area2w_1c(d[ix + j], d[ix + jm],
                                            d[imx + j], c, dx2, dy2);

            c = 0.5*(d[ix + j] + d[ix + jp] + d[imx + jp] + d[imx + j]);
            values[gno] += square_area2w_1c(d[ix + j], d[ix + jp],
                                            d[imx + j], c, dx2, dy2);

            c = 0.5*(d[ix + j] + d[ix + jm] + d[ipx + jm] + d[ipx + j]);
            values[gno] += square_area2w_1c(d[ix + j], d[ix + jm],
                                            d[ipx + j], c, dx2, dy2);

            c = 0.5*(d[ix + j] + d[ix + jp] + d[ipx + jp] + d[ipx + j]);
            values[gno] += square_area2w_1c(d[ix + j], d[ix + jp],
                                            d[ipx + j], c, dx2, dy2);
        }
    }

    for (guint gno = 0; gno <= ngrains; gno++)
        values[gno] *= dxdy/8.0;
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
        for (guint j = 0; j <= xres; j++) {
            // Hope the compiler will optimize this mess...
            guint g1 = (i > 0 && j > 0) ? grains[i*xres + j - xres - 1] : 0;
            guint g2 = (i > 0 && j < xres) ? grains[i*xres + j - xres] : 0;
            guint g3 = (i < yres && j > 0) ? grains[i*xres + j - 1] : 0;
            guint g4 = (i < yres && j < xres) ? grains[i*xres + j] : 0;
            guint f = (g1 > 0) + (g2 > 0) + (g3 > 0) + (g4 > 0);
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
                       GArray *vertices)
{
    enum { RIGHT = 0, DOWN, LEFT, UP } newdir = RIGHT, dir;
    g_return_if_fail(grains[pos]);
    gint initpos = pos;
    guint gno = grains[pos];
    GridPoint v = { .i = pos/xres, .j = pos % xres };
    g_array_set_size(vertices, 0);
    g_array_append_val(vertices, v);

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
            g_array_append_val(vertices, v);
            guint len = vertices->len;
            while (len > 2) {
                GridPoint *cur = &g_array_index(vertices, GridPoint, len-1);
                GridPoint *mid = &g_array_index(vertices, GridPoint, len-2);
                GridPoint *prev = &g_array_index(vertices, GridPoint, len-3);
                gdouble phi = atan2(cur->i - mid->i, cur->j - mid->j);
                gdouble phim = atan2(mid->i - prev->i, mid->j - prev->j);
                phi = fmod(phi - phim + 4.0*G_PI, 2.0*G_PI);
                /* This should be fairly safe as (a) not real harm is done
                 * when we have an occasional extra vertex (b) the greatest
                 * possible angle is G_PI/2.0 */
                if (phi > 1e-12 && phi < G_PI)
                    break;

                // Get rid of mid, it is in a locally concave part.
                g_array_index(vertices, GridPoint, len-2) = *cur;
                g_array_set_size(vertices, len-1);
            }
        }
    } while (v.i*xres + v.j != initpos);

    // The last point is duplicated first point.
    g_array_set_size(vertices, vertices->len-1);
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
 *
 * FIXME: This is a blatantly naive O(n^2) algorithm.
 **/
static void
grain_maximum_bound(const GArray *vertices,
                    gdouble qx, gdouble qy,
                    gdouble *vx, gdouble *vy)
{
    gdouble vm = -G_MAXDOUBLE;
    for (guint g1 = 0; g1 < vertices->len; g1++) {
        const GridPoint *a = &g_array_index(vertices, GridPoint, g1);
        for (guint g2 = g1 + 1; g2 < vertices->len; g2++) {
            const GridPoint *x = &g_array_index(vertices, GridPoint, g2);
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
 *
 * FIXME: This is a blatantly naive O(n^2) algorithm.
 **/
static void
grain_minimum_bound(const GArray *vertices,
                    gdouble qx, gdouble qy,
                    gdouble *vx, gdouble *vy)
{
    g_return_if_fail(vertices->len >= 3);
    gdouble vm = G_MAXDOUBLE;
    for (guint g1 = 0; g1 < vertices->len; g1++) {
        const GridPoint *a = &g_array_index(vertices, GridPoint, g1);
        guint g1p = (g1 + 1) % vertices->len;
        const GridPoint *b = &g_array_index(vertices, GridPoint, g1p);
        gdouble bx = qx*(b->j - a->j);
        gdouble by = qy*(b->i - a->i);
        gdouble b2 = bx*bx + by*by;
        gdouble vm1 = -G_MAXDOUBLE, vx1 = -G_MAXDOUBLE, vy1 = -G_MAXDOUBLE;
        for (guint g2 = 0; g2 < vertices->len; g2++) {
            const GridPoint *x = &g_array_index(vertices, GridPoint, g2);
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

static void
calc_convex_hull(GwyGrainValue *minsizegrainvalue,
                 GwyGrainValue *minanglegrainvalue,
                 GwyGrainValue *maxsizegrainvalue,
                 GwyGrainValue *maxanglegrainvalue,
                 const guint *grains,
                 const guint *anyboundpos,
                 const GwyField *field)
{
    guint ngrains;
    gdouble *minsizevalues, *maxsizevalues, *minanglevalues, *maxanglevalues;
    if (all_null(4, &ngrains, minsizegrainvalue, maxsizegrainvalue,
                 minanglegrainvalue, maxanglegrainvalue)
        || !check_target(minsizegrainvalue, &minsizevalues,
                         GWY_GRAIN_VALUE_MINIMUM_BOUND_SIZE)
        || !check_target(maxsizegrainvalue, &maxsizevalues,
                         GWY_GRAIN_VALUE_MAXIMUM_BOUND_SIZE)
        || !check_target(minanglegrainvalue, &minanglevalues,
                         GWY_GRAIN_VALUE_MINIMUM_BOUND_ANGLE)
        || !check_target(maxanglegrainvalue, &maxanglevalues,
                         GWY_GRAIN_VALUE_MAXIMUM_BOUND_ANGLE))
        return;

    guint xres = field->xres, yres = field->yres;
    gdouble dx = gwy_field_dx(field), dy = gwy_field_dy(field);

    // Find the complete convex hulls.
    GArray *vertices = g_array_new(FALSE, FALSE, sizeof(GridPoint));
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
    }

    g_array_free(vertices, TRUE);
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
        g_return_if_fail(GWY_IS_GRAIN_VALUE(grainvalue));
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
        for (guint i = 0; i <= ngrains; i++)
            anyboundpos[i] = G_MAXUINT;
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
    calc_rms_intra(ourvalues[GWY_GRAIN_VALUE_MEDIAN],
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
                     grains, anyboundpos, field);
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

    // NB: This must be done last because other function expect coordinates
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

        if (grainvalue != ourvalues[id]) {
            _gwy_grain_value_set_size(grainvalue, ngrains);
            gwy_assign(grainvalue->priv->values,
                       ourvalues[id]->priv->values,
                       ngrains+1);
        }

        if (!priv->unit)
            priv->unit = gwy_unit_new();
        gwy_unit_power_multiply(priv->unit,
                                unitxy, builtin->powerxy,
                                unitz, builtin->powerz);
    }

    for (guint i = 0; i < GWY_GRAIN_NVALUES; i++)
        GWY_OBJECT_UNREF(ourvalues[i]);

    g_free(ourvalues);
    GWY_FREE(anyboundpos);
    GWY_FREE(quadratic);
    GWY_FREE(linear);
}

#define add_builtin(name, func) \
    g_hash_table_insert(builtins, (gpointer)name, (gpointer)&func)

GHashTable*
_gwy_grain_value_setup_builtins(void)
{
    GHashTable *builtins;

    builtins = g_hash_table_new(g_str_hash, g_str_equal);
    for (guint i = 0; i < GWY_GRAIN_NVALUES; i++) {
        const BuiltinGrainValue *builtin = builtin_table + i;
        g_hash_table_insert(builtins,
                            (gpointer)builtin->name, (gpointer)builtin);
    }
    return builtins;
}

void
_gwy_grain_value_set_size(GwyGrainValue *grainvalue, guint ngrains)
{
    GrainValue *priv = grainvalue->priv;
    if (priv->ngrains != ngrains) {
        GWY_FREE(priv->values);
        priv->values = g_new(gdouble, ngrains+1);
        priv->ngrains = ngrains;
    }
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
