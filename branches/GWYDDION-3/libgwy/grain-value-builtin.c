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

#include <string.h>
#include <glib/gi18n.h>
#include "libgwy/macros.h"
#include "libgwy/math.h"
#include "libgwy/grain-value.h"
#include "libgwy/mask-field-grains.h"
#include "libgwy/grain-value-builtin.h"

enum {
    NEED_SIZES = 1 << 0,
    NEED_ANYBOUNDPOS = 1 << 1,
    NEED_MIN = 1 << 2,
    NEED_MAX = 1 << 3,
    NEED_XVALUE = (1 << 4) | NEED_SIZES,
    NEED_YVALUE = (1 << 5) | NEED_SIZES,
    NEED_ZVALUE = (1 << 6) | NEED_SIZES,
    NEED_LINEAR = (1 << 7) | NEED_ZVALUE | NEED_XVALUE | NEED_YVALUE,
    NEED_QUADRATIC = (1 << 8) | NEED_LINEAR,
};

// Must be signed, we use signed differences.
typedef struct {
    gint i;
    gint j;
} GridPoint;

// TODO: Move this to builtin_table.
static const guint value_dependences[GWY_GRAIN_NVALUES] = {
    NEED_XVALUE,                  /* centre x */
    NEED_YVALUE,                  /* centre y */
    NEED_SIZES,                   /* projected area */
    NEED_SIZES,                   /* equiv disc radius */
    0,                            /* surface area */
    NEED_MIN | NEED_MAX,          /* half-height area */
    NEED_MIN,                     /* minimum */
    NEED_MAX,                     /* maximum */
    NEED_ZVALUE,                  /* mean */
    NEED_SIZES,                   /* median */
    0,                            /* flat boundary length */
    NEED_ANYBOUNDPOS,             /* min bounding size */
    NEED_ANYBOUNDPOS,             /* min bounding direction */
    NEED_ANYBOUNDPOS,             /* max bounding size */
    NEED_ANYBOUNDPOS,             /* max bounding direction */
    0,                            /* boundary minimum */
    0,                            /* boundary maximum */
    0,                            /* volume, 0-based */
    NEED_MIN,                     /* volume, min-based */
    0,                            /* volume, Laplace-based */
    NEED_LINEAR,                  /* slope theta */
    NEED_LINEAR,                  /* slope phi */
    NEED_QUADRATIC,               /* curvature centre x */
    NEED_QUADRATIC,               /* curvature centre y */
    NEED_QUADRATIC,               /* curvature centre z */
    NEED_QUADRATIC,               /* curvature invrad 1 */
    NEED_QUADRATIC,               /* curvature invrad 2 */
    NEED_QUADRATIC,               /* curvature direction 1 */
    NEED_QUADRATIC,               /* curvature direction 2 */
};

// TODO: Initialise this
static const BuiltinGrainValue builtin_table[GWY_GRAIN_NVALUES];
// TODO: Move to builtin_table.
static const gchar *const value_names[GWY_GRAIN_NVALUES] = {
    NC_("grain value", "Center X position"),
    NC_("grain value", "Center Y position"),
    NC_("grain value", "Projected area"),
    NC_("grain value", "Equivalent disc radius"),
    NC_("grain value", "Surface area"),
    NC_("grain value", "Half-height area"),
    NC_("grain value", "Minimum"),
    NC_("grain value", "Maximum"),
    NC_("grain value", "Mean"),
    NC_("grain value", "Median"),
    NC_("grain value", "Flat boundary length"),
    NC_("grain value", "Minimum bounding size"),
    NC_("grain value", "Minimum bounding angle"),
    NC_("grain value", "Maximum bounding size"),
    NC_("grain value", "Maximum bounding angle"),
    NC_("grain value", "Zero-based volume"),
    NC_("grain value", "Minimum-based volume"),
    NC_("grain value", "Laplace-based volume"),
    NC_("grain value", "Slope normal angle"),
    NC_("grain value", "Slope azimuth"),
    NC_("grain value", "Curvature center X position"),
    NC_("grain value", "Curvature center Y position"),
    NC_("grain value", "Curvature central value"),
    NC_("grain value", "Curvature 1"),
    NC_("grain value", "Curvature 2"),
    NC_("grain value", "Curvature angle 1"),
    NC_("grain value", "Curvature angle 2"),
};

#define add_builtin(name, func) \
    g_hash_table_insert(builtins, (gpointer)name, (gpointer)&func)

GHashTable*
_gwy_grain_value_setup_builtins(void)
{
    GHashTable *builtins;

    builtins = g_hash_table_new(g_str_hash, g_str_equal);
    return builtins;
}

void
_gwy_grain_value_set_size(GwyGrainValue *grainvalue, guint ngrains)
{
    GrainValue *priv = grainvalue->priv;
    if (priv->ngrains != ngrains) {
        GWY_FREE(priv->values);
        priv->values = g_new(gdouble, ngrains);
    }
}

static void
ensure_value(BuiltinGrainValueId id,
             GwyGrainValue **ourvalues,
             guint ngrains)
{
    if (!ourvalues[id]) {
        ourvalues[id] = gwy_grain_value_new(builtin_table[id].name);
        g_assert(ourvalues[id]);
        g_assert(ourvalues[id]->priv->builtin->id == id);
        _gwy_grain_value_set_size(ourvalues[id], ngrains);
    }
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
    if (!grainvalue)
        return;

    GrainValue *priv = grainvalue->priv;
    const BuiltinGrainValue *builtin = priv->builtin;
    g_return_if_fail(builtin
                     && builtin->id == GWY_GRAIN_VALUE_MINIMUM);
    guint nn = field->xres * field->yres;
    gdouble *values = priv->values;
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
    if (!grainvalue)
        return;

    GrainValue *priv = grainvalue->priv;
    const BuiltinGrainValue *builtin = priv->builtin;
    g_return_if_fail(builtin
                     && builtin->id == GWY_GRAIN_VALUE_MAXIMUM);
    guint nn = field->xres * field->yres;
    gdouble *values = priv->values;
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
    if (!grainvalue)
        return;

    GrainValue *priv = grainvalue->priv;
    const BuiltinGrainValue *builtin = priv->builtin;
    g_return_if_fail(sizes);
    g_return_if_fail(builtin
                     && builtin->id == GWY_GRAIN_VALUE_CENTER_X);
    guint xres = field->xres, yres = field->yres;
    guint ngrains = grainvalue->priv->ngrains;
    gdouble *values = priv->values;
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
    if (!grainvalue)
        return;

    GrainValue *priv = grainvalue->priv;
    g_return_if_fail(sizes);
    g_return_if_fail(priv->builtin
                     && priv->builtin->id == GWY_GRAIN_VALUE_CENTER_Y);
    guint xres = field->xres, yres = field->yres;
    guint ngrains = grainvalue->priv->ngrains;
    gdouble *values = priv->values;
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
    if (!grainvalue)
        return;

    GrainValue *priv = grainvalue->priv;
    const BuiltinGrainValue *builtin = priv->builtin;
    g_return_if_fail(sizes);
    g_return_if_fail(builtin
                     && builtin->id == GWY_GRAIN_VALUE_MEAN);
    guint nn = field->xres * field->yres;
    gdouble *values = priv->values;
    guint ngrains = grainvalue->priv->ngrains;
    const guint *g = grains;
    const gdouble *d = field->data;

    for (guint k = nn; k; k--, g++, d++)
        values[*g] += *d;
    for (guint gno = 0; gno <= ngrains; gno++)
        values[gno] /= sizes[gno];
}

// The coordinate origin is always the grain centre.
// Also the sums are *NOT* divided by grain sizes because these will cancel out.
// FIXME: It would be also nice to use MEAN as the value origin to reduce
// rounding errors in the z-direction.
static void
calc_linear(gdouble *linear,
            const guint *grains,
            const GwyGrainValue *xgrainvalue,
            const GwyGrainValue *ygrainvalue,
            const GwyField *field)
{
    if (!linear)
        return;

    g_return_if_fail(xgrainvalue && ygrainvalue);
    const GrainValue *xpriv = xgrainvalue->priv, *ypriv = ygrainvalue->priv;
    const BuiltinGrainValue *xbuiltin = xpriv->builtin;
    const BuiltinGrainValue *ybuiltin = ypriv->builtin;
    g_return_if_fail(xbuiltin
                     && xbuiltin->id == GWY_GRAIN_VALUE_CENTER_X);
    g_return_if_fail(ybuiltin
                     && ybuiltin->id == GWY_GRAIN_VALUE_CENTER_Y);
    guint xres = field->xres;
    guint yres = field->yres;
    const gdouble *xvalue = xpriv->values;
    const gdouble *yvalue = ypriv->values;
    const guint *g = grains;
    const gdouble *d = field->data;

    for (guint i = 0; i < yres; i++) {
        for (guint j = 0; j < xres; j++, g++, d++) {
            guint gno = *g;
            gdouble *t = linear + 5*gno;
            gdouble x = j - xvalue[gno];
            gdouble y = i - yvalue[gno];
            gdouble z = *d;
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
               const GwyField *field)
{
    if (!quadratic)
        return;

    g_return_if_fail(xgrainvalue && ygrainvalue);
    const GrainValue *xpriv = xgrainvalue->priv, *ypriv = ygrainvalue->priv;
    const BuiltinGrainValue *xbuiltin = xpriv->builtin;
    const BuiltinGrainValue *ybuiltin = ypriv->builtin;
    g_return_if_fail(xbuiltin
                     && xbuiltin->id == GWY_GRAIN_VALUE_CENTER_X);
    g_return_if_fail(ybuiltin
                     && ybuiltin->id == GWY_GRAIN_VALUE_CENTER_Y);
    guint xres = field->xres;
    guint yres = field->yres;
    const gdouble *xvalue = xpriv->values;
    const gdouble *yvalue = ypriv->values;
    const guint *g = grains;
    const gdouble *d = field->data;

    for (guint i = 0; i < yres; i++) {
        for (guint j = 0; j < xres; j++, g++, d++) {
            guint gno = *g;
            gdouble *t = quadratic + 12*gno;
            gdouble x = j - xvalue[gno];
            gdouble y = i - yvalue[gno];
            gdouble xx = x*x;
            gdouble xy = x*y;
            gdouble yy = y*y;
            gdouble z = *d;
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
    if (!grainvalue)
        return;

    GrainValue *priv = grainvalue->priv;
    const BuiltinGrainValue *builtin = priv->builtin;
    g_return_if_fail(builtin
                     && builtin->id == GWY_GRAIN_VALUE_MEDIAN);
    guint nn = field->xres * field->yres;
    guint ngrains = priv->ngrains;
    gdouble *values = priv->values;
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
calc_projected_area(GwyGrainValue *grainvalue,
                    const GwyField *field,
                    const guint *sizes)
{
    if (!grainvalue)
        return;

    GrainValue *priv = grainvalue->priv;
    const BuiltinGrainValue *builtin = priv->builtin;
    g_return_if_fail(sizes);
    g_return_if_fail(builtin
                     && builtin->id == GWY_GRAIN_VALUE_PROJECTED_AREA);
    guint ngrains = priv->ngrains;
    gdouble *values = priv->values;
    gdouble dxdy = gwy_field_dx(field)*gwy_field_dy(field);
    for (guint gno = 0; gno <= ngrains; gno++)
        values[gno] = dxdy*sizes[gno];
}

static void
calc_equiv_disc_radius(GwyGrainValue *grainvalue,
                       const GwyField *field,
                       const guint *sizes)
{
    if (!grainvalue)
        return;

    GrainValue *priv = grainvalue->priv;
    const BuiltinGrainValue *builtin = priv->builtin;
    g_return_if_fail(sizes);
    g_return_if_fail(builtin
                     && builtin->id == GWY_GRAIN_VALUE_EQUIV_DISC_RADIUS);
    guint ngrains = priv->ngrains;
    gdouble *values = priv->values;
    gdouble dxdy = gwy_field_dx(field)*gwy_field_dy(field);
    for (guint gno = 0; gno <= ngrains; gno++)
        values[gno] = sqrt(dxdy*sizes[gno]);
}

// Calculate twice the `contribution of one corner' (the twice is to move
// multiplications from inner loops) to the surface area.
static inline gdouble
square_area2w_1c(gdouble z1, gdouble z2, gdouble z4, gdouble c,
                 gdouble x, gdouble y)
{
    return sqrt(1.0 + (z1 - z2)*(z1 - z2)/x + (z1 + z2 - c)*(z1 + z2 - c)/y)
            + sqrt(1.0 + (z1 - z4)*(z1 - z4)/y + (z1 + z4 - c)*(z1 + z4 - c)/x);
}

static void
calc_surface_area(GwyGrainValue *grainvalue,
                  const guint *grains,
                  const GwyField *field)
{
    if (!grainvalue)
        return;

    GrainValue *priv = grainvalue->priv;
    const BuiltinGrainValue *builtin = priv->builtin;
    g_return_if_fail(builtin
                     && builtin->id == GWY_GRAIN_VALUE_SURFACE_AREA);
    guint xres = field->xres, yres = field->yres;
    gdouble dx = gwy_field_dx(field);
    gdouble dy = gwy_field_dy(field);
    gdouble dx2 = dx*dx, dy2 = dy*dy, dxdy = dx*dy;
    guint ngrains = priv->ngrains;
    gdouble *values = priv->values;
    const gdouble *d = field->data;

    // Every contribution is calculated twice -- for each pixel (vertex)
    // participating to a particular triangle.  So we divide by 8, not by 4.
    for (guint i = 0; i < yres; i++) {
        for (guint j = 0; j < xres; j++) {
            guint ix = i*xres;
            guint gno = grains[ix + j];
            if (!gno)
                continue;

            guint imx = (i > 0) ? ix-xres : ix;
            guint ipx = (i < yres-1) ? ix+xres : ix;
            guint jm = (j > 0) ? j-1 : j;
            guint jp = (j < yres-1) ? j+1 : j;
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
    if (!grainvalue)
        return;

    GrainValue *priv = grainvalue->priv;
    const BuiltinGrainValue *builtin = priv->builtin;
    g_return_if_fail(builtin
                     && builtin->id == GWY_GRAIN_VALUE_HALF_HEIGHT_AREA);
    g_return_if_fail(mingrainvalue && maxgrainvalue);
    const GrainValue *minpriv = mingrainvalue->priv;
    const GrainValue *maxpriv = maxgrainvalue->priv;
    const BuiltinGrainValue *minbuiltin = minpriv->builtin;
    const BuiltinGrainValue *maxbuiltin = maxpriv->builtin;
    g_return_if_fail(minbuiltin
                     && minbuiltin->id == GWY_GRAIN_VALUE_MINIMUM);
    g_return_if_fail(maxbuiltin
                     && maxbuiltin->id == GWY_GRAIN_VALUE_MAXIMUM);
    guint nn = field->xres * field->yres;
    gdouble dxdy = gwy_field_dx(field)*gwy_field_dy(field);
    guint ngrains = priv->ngrains;
    gdouble *values = priv->values;
    const gdouble *d = field->data;
    const gdouble *min = minpriv->values;
    const gdouble *max = maxpriv->values;

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
    if (!grainvalue)
        return;

    GrainValue *priv = grainvalue->priv;
    const BuiltinGrainValue *builtin = priv->builtin;
    g_return_if_fail(builtin
                     && builtin->id == GWY_GRAIN_VALUE_FLAT_BOUNDARY_LENGTH);
    guint xres = field->xres, yres = field->yres;
    gdouble dx = gwy_field_dx(field);
    gdouble dy = gwy_field_dy(field);
    gdouble diag = hypot(dx, dy);
    gdouble *values = priv->values;

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
    if (!mingrainvalue && !maxgrainvalue)
        return;

    GrainValue *minpriv = NULL, *maxpriv = NULL;
    gdouble *minvalues = NULL, *maxvalues = NULL;

    if (mingrainvalue) {
        minpriv = mingrainvalue->priv;
        const BuiltinGrainValue *builtin = minpriv->builtin;
        g_return_if_fail(builtin
                         && builtin->id == GWY_GRAIN_VALUE_BOUNDARY_MINIMUM);
        minvalues = minpriv->values;
    }
    if (maxgrainvalue) {
        maxpriv = maxgrainvalue->priv;
        const BuiltinGrainValue *builtin = maxpriv->builtin;
        g_return_if_fail(builtin
                         && builtin->id == GWY_GRAIN_VALUE_BOUNDARY_MAXIMUM);
        maxvalues = maxpriv->values;
    }
    guint xres = field->xres, yres = field->yres;
    const gdouble *d = field->data;

    for (guint i = 0; i < yres; i++) {
        for (guint j = 0; j < xres; j++, d++) {
            guint gno = grains[i*xres + j];
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
    if (!minsizegrainvalue && !maxsizegrainvalue
        && !minanglegrainvalue && !maxanglegrainvalue)
        return;

    GrainValue *minsizepriv = NULL, *maxsizepriv = NULL,
               *minanglepriv = NULL, *maxanglepriv = NULL;
    gdouble *minsizevalues = NULL, *maxsizevalues = NULL,
            *minanglevalues = NULL, *maxanglevalues = NULL;
    guint ngrains = 0;

    if (minsizegrainvalue) {
        minsizepriv = minsizegrainvalue->priv;
        const BuiltinGrainValue *builtin = minsizepriv->builtin;
        g_return_if_fail(builtin
                         && builtin->id == GWY_GRAIN_VALUE_MINIMUM_BOUND_SIZE);
        minsizevalues = minsizepriv->values;
        ngrains = minsizepriv->ngrains;
    }
    if (maxsizegrainvalue) {
        maxsizepriv = maxsizegrainvalue->priv;
        const BuiltinGrainValue *builtin = maxsizepriv->builtin;
        g_return_if_fail(builtin
                         && builtin->id == GWY_GRAIN_VALUE_MAXIMUM_BOUND_SIZE);
        maxsizevalues = maxsizepriv->values;
        ngrains = maxsizepriv->ngrains;
    }
    if (minanglegrainvalue) {
        minanglepriv = minanglegrainvalue->priv;
        const BuiltinGrainValue *builtin = minanglepriv->builtin;
        g_return_if_fail(builtin
                         && builtin->id == GWY_GRAIN_VALUE_MINIMUM_BOUND_ANGLE);
        minanglevalues = minanglepriv->values;
        ngrains = minanglepriv->ngrains;
    }
    if (maxanglegrainvalue) {
        maxanglepriv = maxanglegrainvalue->priv;
        const BuiltinGrainValue *builtin = maxanglepriv->builtin;
        g_return_if_fail(builtin
                         && builtin->id == GWY_GRAIN_VALUE_MAXIMUM_BOUND_ANGLE);
        maxanglevalues = maxanglepriv->values;
        ngrains = maxanglepriv->ngrains;
    }

    guint xres = field->xres, yres = field->yres;
    gdouble dx = gwy_field_dx(field);
    gdouble dy = gwy_field_dy(field);

    // Find the complete convex hulls.
    GArray *vertices = g_array_new(FALSE, FALSE, sizeof(GridPoint));
    for (guint gno = 1; gno <= ngrains; gno++) {
        gdouble vx = dx, vy = dy;

        find_grain_convex_hull(xres, yres, grains, anyboundpos[gno], vertices);
        if (minsizevalues || minanglevalues) {
            grain_minimum_bound(vertices, dx, dy, &vx, &vy);
            if (minsizevalues)
                minsizevalues[gno] = hypot(vx, vy);
            if (minanglevalues) {
                minanglevalues[gno] = atan2(-vy, vx);
                if (minanglevalues[gno] <= -G_PI/2.0)
                    minanglevalues[gno] += G_PI;
                else if (minanglevalues[gno] > G_PI/2.0)
                    minanglevalues[gno] -= G_PI;
            }
        }
        if (maxsizevalues || maxanglevalues) {
            grain_maximum_bound(vertices, dx, dy, &vx, &vy);
            if (maxsizevalues)
                maxsizevalues[gno] = hypot(vx, vy);
            if (maxanglevalues) {
                maxanglevalues[gno] = atan2(-vy, vx);
                if (maxanglevalues[gno] <= -G_PI/2.0)
                    maxanglevalues[gno] += G_PI;
                else if (maxanglevalues[gno] > G_PI/2.0)
                    maxanglevalues[gno] -= G_PI;
            }
        }
    }

    g_array_free(vertices, TRUE);
}

static void
calc_slope(GwyGrainValue *thetagrainvalue,
           GwyGrainValue *phigrainvalue,
           const gdouble *linear,
           const GwyField *field)
{
    if (!thetagrainvalue && !phigrainvalue)
        return;

    GrainValue *thetapriv = NULL, *phipriv = NULL;
    gdouble *thetavalues = NULL, *phivalues = NULL;
    guint ngrains = 0;

    if (thetagrainvalue) {
        thetapriv = thetagrainvalue->priv;
        const BuiltinGrainValue *builtin = thetapriv->builtin;
        g_return_if_fail(builtin
                         && builtin->id == GWY_GRAIN_VALUE_SLOPE_THETA);
        thetavalues = thetapriv->values;
        ngrains = thetapriv->ngrains;
    }
    if (phigrainvalue) {
        phipriv = phigrainvalue->priv;
        const BuiltinGrainValue *builtin = phipriv->builtin;
        g_return_if_fail(builtin
                         && builtin->id == GWY_GRAIN_VALUE_SLOPE_PHI);
        phivalues = phipriv->values;
        ngrains = phipriv->ngrains;
    }

    gdouble dx = gwy_field_dx(field);
    gdouble dy = gwy_field_dy(field);

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
    if (!xcgrainvalue && !ycgrainvalue && !zcgrainvalue
        && !c1grainvalue && !c2grainvalue
        && !a1grainvalue && !a2grainvalue)
        return;

    g_return_if_fail(sizes);
    g_return_if_fail(linear);
    g_return_if_fail(quadratic);
    g_return_if_fail(xgrainvalue && ygrainvalue && zgrainvalue);
    const GrainValue *xpriv = xgrainvalue->priv;
    const GrainValue *ypriv = ygrainvalue->priv;
    const GrainValue *zpriv = zgrainvalue->priv;
    const BuiltinGrainValue *xbuiltin = xpriv->builtin;
    const BuiltinGrainValue *ybuiltin = ypriv->builtin;
    const BuiltinGrainValue *zbuiltin = zpriv->builtin;
    g_return_if_fail(xbuiltin
                     && xbuiltin->id == GWY_GRAIN_VALUE_MINIMUM);
    g_return_if_fail(ybuiltin
                     && ybuiltin->id == GWY_GRAIN_VALUE_MAXIMUM);
    g_return_if_fail(zbuiltin
                     && zbuiltin->id == GWY_GRAIN_VALUE_MEAN);

    GrainValue *xcpriv = NULL, *ycpriv = NULL, *zcpriv = NULL,
               *c1priv = NULL, *c2priv = NULL,
               *a1priv = NULL, *a2priv = NULL;
    gdouble *xcvalues = NULL, *ycvalues = NULL, *zcvalues = NULL,
            *c1values = NULL, *c2values = NULL,
            *a1values = NULL, *a2values = NULL;
    guint ngrains = 0;

    if (xcgrainvalue) {
        xcpriv = xcgrainvalue->priv;
        const BuiltinGrainValue *builtin = xcpriv->builtin;
        g_return_if_fail(builtin
                         && builtin->id == GWY_GRAIN_VALUE_CURVATURE_CENTER_X);
        xcvalues = xcpriv->values;
        ngrains = xcpriv->ngrains;
    }
    if (ycgrainvalue) {
        ycpriv = ycgrainvalue->priv;
        const BuiltinGrainValue *builtin = ycpriv->builtin;
        g_return_if_fail(builtin
                         && builtin->id == GWY_GRAIN_VALUE_CURVATURE_CENTER_Y);
        ycvalues = ycpriv->values;
        ngrains = ycpriv->ngrains;
    }
    if (zcgrainvalue) {
        zcpriv = zcgrainvalue->priv;
        const BuiltinGrainValue *builtin = zcpriv->builtin;
        g_return_if_fail(builtin
                         && builtin->id == GWY_GRAIN_VALUE_CURVATURE_CENTER_Z);
        zcvalues = zcpriv->values;
        ngrains = zcpriv->ngrains;
    }
    if (c1grainvalue) {
        c1priv = c1grainvalue->priv;
        const BuiltinGrainValue *builtin = c1priv->builtin;
        g_return_if_fail(builtin
                         && builtin->id == GWY_GRAIN_VALUE_CURVATURE1);
        c1values = c1priv->values;
        ngrains = c1priv->ngrains;
    }
    if (c2grainvalue) {
        c2priv = c2grainvalue->priv;
        const BuiltinGrainValue *builtin = c2priv->builtin;
        g_return_if_fail(builtin
                         && builtin->id == GWY_GRAIN_VALUE_CURVATURE2);
        c2values = c2priv->values;
        ngrains = c2priv->ngrains;
    }
    if (a1grainvalue) {
        a1priv = a1grainvalue->priv;
        const BuiltinGrainValue *builtin = a1priv->builtin;
        g_return_if_fail(builtin
                         && builtin->id == GWY_GRAIN_VALUE_CURVATURE_ANGLE1);
        ngrains = a1priv->ngrains;
        a1values = a1priv->values;
    }
    if (a2grainvalue) {
        a2priv = a2grainvalue->priv;
        const BuiltinGrainValue *builtin = a2priv->builtin;
        g_return_if_fail(builtin
                         && builtin->id == GWY_GRAIN_VALUE_CURVATURE_ANGLE2);
        ngrains = a2priv->ngrains;
        a2values = a2priv->values;
    }

    const gdouble *xvalues = xpriv->values;
    const gdouble *yvalues = ypriv->values;
    const gdouble *zvalues = zpriv->values;

    gdouble dx = gwy_field_dx(field), dy = gwy_field_dy(field);
    gdouble mx = sqrt(dx/dy), my = sqrt(dy/dx);
    gdouble dxdy = dx*dy;
    gdouble eqside = sqrt(dx*dy);

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
            if (gwy_cholesky_decompose(a, 6)) {
                b[0] = n*zvalues[gno];
                b[1] = lin[3];
                b[2] = lin[4];
                b[3] = quad[9];
                b[4] = quad[10];
                b[5] = quad[11];
                gwy_cholesky_solve(a, b, 6);
                // Get pixel aspect ratio right while keeping pixel size
                // around 1.
                b[1] /= mx;
                b[2] /= my;
                b[3] /= mx*mx;
                b[5] /= my*my;
            }
            else
                n = 0;
        }

        GwyCurvatureParams curvature;
        if (n >= 6)
            gwy_math_curvature(b, &curvature);
        else {
            gwy_clear1(curvature);
            curvature.phi2 = G_PI/2.0;
            curvature.zc = zvalues[gno];
        }
        if (c1values)
            c1values[gno] = a[0]/dxdy;
        if (c2values)
            c2values[gno] = a[1]/dxdy;
        if (a1values)
            a1values[gno] = a[2];
        if (a2values)
            a2values[gno] = a[3];
        if (xcvalues)
            xcvalues[gno] = eqside*a[4] + xvalues[gno];
        if (ycvalues)
            ycvalues[gno] = eqside*a[5] + yvalues[gno];
        if (zcvalues)
            zcvalues[gno] = a[6];
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

    for (guint i = 0; i < nvalues; i++) {
        GwyGrainValue *grainvalue = grainvalues[i];
        g_return_if_fail(GWY_IS_GRAIN_VALUE(grainvalue));
        GrainValue *priv = grainvalue->priv;
        g_return_if_fail(priv->is_valid && priv->builtin);
        g_return_if_fail(priv->builtin->id < GWY_GRAIN_NVALUES);
        _gwy_grain_value_set_size(grainvalue, ngrains);
    }

    // Figure out which quantities are requested.
    GwyGrainValue **ourvalues = g_new0(GwyGrainValue*, GWY_GRAIN_NVALUES);
    for (guint i = 0; i < nvalues; i++) {
        GwyGrainValue *grainvalue = grainvalues[i];
        BuiltinGrainValueId id = grainvalue->priv->builtin->id;

        // Take the first if the same quantity is requested multiple times.
        // We will deal with this later.
        if (!ourvalues[id])
            ourvalues[id] = g_object_ref(grainvalue);
    }

    // Figure out the auxiliary data to calculate.  Do this after we gathered
    // all quantities as some auxiliary data are in fact quantities too.
    guint need = 0;
    for (guint i = 0; i < nvalues; i++) {
        GwyGrainValue *grainvalue = grainvalues[i];
        BuiltinGrainValueId id = grainvalue->priv->builtin->id;
        need |= value_dependences[id];
    }

    // Integer data.
    const guint *sizes = ((need & NEED_SIZES)
                          ? gwy_mask_field_grain_sizes(mask) : NULL);

    guint *anyboundpos = NULL;
    if (need & NEED_ANYBOUNDPOS) {
        anyboundpos = g_new(guint, ngrains + 1);
        for (guint i = 0; i <= ngrains; i++)
            anyboundpos[i] = G_MAXUINT;
    }

    // Floating point data that coincide with some grain value.  If it is
    // requested by caller we use that otherwise create a new GwyGrainValue.
    if (need & NEED_MIN)
        ensure_value(GWY_GRAIN_VALUE_MINIMUM, ourvalues, ngrains);
    if (need & NEED_MAX)
        ensure_value(GWY_GRAIN_VALUE_MAXIMUM, ourvalues, ngrains);
    if (need & NEED_XVALUE)
        ensure_value(GWY_GRAIN_VALUE_CENTER_X, ourvalues, ngrains);
    if (need & NEED_YVALUE)
        ensure_value(GWY_GRAIN_VALUE_CENTER_Y, ourvalues, ngrains);
    if (need & NEED_ZVALUE)
        ensure_value(GWY_GRAIN_VALUE_MEAN, ourvalues, ngrains);

    for (guint i = 0; i < GWY_GRAIN_NVALUES; i++) {
        if (ourvalues[i])
            init_values(ourvalues[i]);
    }

    // Complex floating point data.
    gdouble *linear = ((need & NEED_LINEAR)
                       ? g_new0(gdouble, 5*(ngrains + 1)) : NULL);

    gdouble *quadratic = ((need & NEED_QUADRATIC)
                          ?  g_new0(gdouble, 12*(ngrains + 1)) : NULL);

    calc_anyboundpos(anyboundpos, grains, field);
    calc_minimum(ourvalues[GWY_GRAIN_VALUE_MINIMUM], grains, field);
    calc_maximum(ourvalues[GWY_GRAIN_VALUE_MAXIMUM], grains, field);
    calc_centre_x(ourvalues[GWY_GRAIN_VALUE_CENTER_X], grains, sizes, field);
    calc_centre_y(ourvalues[GWY_GRAIN_VALUE_CENTER_Y], grains, sizes, field);
    calc_mean(ourvalues[GWY_GRAIN_VALUE_MEAN], grains, sizes, field);
    calc_linear(linear, grains,
                ourvalues[GWY_GRAIN_VALUE_CENTER_X],
                ourvalues[GWY_GRAIN_VALUE_CENTER_Y],
                field);
    calc_quadratic(quadratic, grains,
                   ourvalues[GWY_GRAIN_VALUE_CENTER_X],
                   ourvalues[GWY_GRAIN_VALUE_CENTER_Y],
                   field);

    // Calculate specific requested quantities that do not directly correspond
    // to auxiliary quantities.
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

#if 0
    if (quantity_data[GWY_GRAIN_VALUE_VOLUME_0]
        || quantity_data[GWY_GRAIN_VALUE_VOLUME_MIN]) {
        gdouble *pv0 = quantity_data[GWY_GRAIN_VALUE_VOLUME_0];
        gdouble *pvm = quantity_data[GWY_GRAIN_VALUE_VOLUME_MIN];

        if (pv0)
            gwy_clear(pv0, ngrains + 1);
        if (pvm)
            gwy_clear(pvm, ngrains + 1);

        for (i = 0; i < yres; i++) {
            for (j = 0; j < xres; j++) {
                gint ix, ipx, imx, jp, jm;
                gdouble v;

                ix = i*xres;
                if (!(gno = grains[ix + j]))
                    continue;

                imx = (i > 0) ? ix-xres : ix;
                ipx = (i < yres-1) ? ix+xres : ix;
                jm = (j > 0) ? j-1 : j;
                jp = (j < yres-1) ? j+1 : j;

                v = (52.0*d[ix + j] + 10.0*(d[imx + j] + d[ix + jm]
                                            + d[ix + jp] + d[ipx + j])
                     + (d[imx + jm] + d[imx + jp] + d[ipx + jm] + d[ipx + jp]));

                /* We know the basis would appear with total weight -96 so
                 * don't bother subtracting it from individual heights */
                if (pv0)
                    pv0[gno] += v;
                if (pvm)
                    pvm[gno] += v - 96.0*min[gno];
            }
        }
        if (pv0) {
            for (gno = 1; gno <= ngrains; gno++)
                pv0[gno] *= dxdy/96.0;
        }
        if (pvm) {
            for (gno = 1; gno <= ngrains; gno++)
                pvm[gno] *= dxdy/96.0;
        }
    }
    if ((p = quantity_data[GWY_GRAIN_VALUE_VOLUME_LAPLACE])) {
        gint *bbox;

        gwy_clear(p, ngrains + 1);
        /* Fail gracefully when there is one big `grain' over all data.
         * FIXME: Is this correct?  The grain can touch all sides but still
         * have an exterior. */
        bbox = gwy_field_get_grain_bounding_boxes(field,
                                                       ngrains, grains, NULL);
        if (ngrains == 1
            && (bbox[4] == 0 && bbox[5] == 0
                && bbox[6] == xres && bbox[7] == yres)) {
            g_warning("Cannot interpolate from exterior of the grain when it "
                      "has no exterior.");
        }
        else {
            for (gno = 1; gno <= ngrains; gno++)
                p[gno] = dxdy/96.0*grain_volume_laplace(field, grains,
                                                         gno, bbox + 4*gno);
        }
        g_free(bbox);
    }
#endif
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
                     gwy_field_dx(field),
                     0.5*gwy_field_dx(field) + field->xoff);
    linear_transform(ourvalues[GWY_GRAIN_VALUE_CENTER_Y],
                     gwy_field_dy(field),
                     0.5*gwy_field_dy(field) + field->yoff);

    // Copy data to all other instances of the same grain value.
    for (guint i = 0; i < nvalues; i++) {
        GwyGrainValue *grainvalue = grainvalues[i];
        BuiltinGrainValueId id = grainvalue->priv->builtin->id;
        if (grainvalue != ourvalues[id]) {
            gwy_assign(grainvalue->priv->values,
                       ourvalues[id]->priv->values,
                       ngrains+1);
        }
    }

    // TODO: Units (probably in specific evaluators).


    g_free(ourvalues);
    GWY_FREE(anyboundpos);
    GWY_FREE(quadratic);
    GWY_FREE(linear);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
