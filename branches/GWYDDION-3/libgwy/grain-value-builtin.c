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

// TODO: Initialise this
static const BuiltinGrainValue builtin_table[GWY_GRAIN_NVALUES];
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

// Note all coordinates are pixel-wise, not real.
// For linear and quadratic, the coordinate origin is always the grain centre.
static void
calc_aux(const GwyField *field,
         const guint *grains,
         const guint *sizes,
         guint *anyboundpos,
         GwyGrainValue *mingrainvalue,
         GwyGrainValue *maxgrainvalue,
         GwyGrainValue *xgrainvalue,
         GwyGrainValue *ygrainvalue,
         GwyGrainValue *zgrainvalue,
         gdouble *linear,
         gdouble *quadratic)
{
    guint xres = field->xres;
    guint yres = field->yres;
    guint nn = xres*yres;

    if (anyboundpos) {
        const guint *g = grains;
        for (guint k = 0; k < nn; k++, g++) {
            guint gno = *g;
            if (anyboundpos[gno] == G_MAXUINT)
                anyboundpos[gno] = k;
        }
    }

    if (mingrainvalue) {
        gdouble *min = mingrainvalue->priv->values;
        const guint *g = grains;
        const gdouble *d = field->data;
        for (guint k = nn; k; k--, g++, d++) {
            guint gno = *g;
            gdouble z = *d;
            if (z < min[gno])
                min[gno] = z;
        }
    }

    if (maxgrainvalue) {
        gdouble *max = maxgrainvalue->priv->values;
        const guint *g = grains;
        const gdouble *d = field->data;
        for (guint k = nn; k; k--, g++, d++) {
            guint gno = *g;
            gdouble z = *d;
            if (z > max[gno])
                max[gno] = z;
        }
    }

    if (zgrainvalue) {
        g_assert(sizes);
        gdouble *zvalue = zgrainvalue->priv->values;
        const guint *g = grains;
        const gdouble *d = field->data;
        guint ngrains = zgrainvalue->priv->ngrains;
        for (guint k = nn; k; k--, g++, d++) {
            guint gno = *g;
            gdouble z = *d;
            zvalue[gno] += z;
        }
        for (guint gno = 0; gno <= ngrains; gno++)
            zvalue[gno] /= sizes[gno];
    }

    if (xgrainvalue) {
        g_assert(sizes);
        gdouble *xvalue = xgrainvalue->priv->values;
        const guint *g = grains;
        guint ngrains = zgrainvalue->priv->ngrains;
        for (guint i = 0; i < yres; i++) {
            for (guint j = 0; j < xres; j++, g++) {
                guint gno = *g;
                xvalue[gno] += j;
            }
        }
        for (guint gno = 0; gno <= ngrains; gno++)
            xvalue[gno] /= sizes[gno];
    }

    if (ygrainvalue) {
        g_assert(sizes);
        gdouble *yvalue = ygrainvalue->priv->values;
        const guint *g = grains;
        guint ngrains = zgrainvalue->priv->ngrains;
        for (guint i = 0; i < yres; i++) {
            for (guint j = 0; j < xres; j++, g++) {
                guint gno = *g;
                yvalue[gno] += i;
            }
        }
        for (guint gno = 0; gno <= ngrains; gno++)
            yvalue[gno] /= sizes[gno];
    }

    if (linear) {
        g_assert(xgrainvalue && ygrainvalue);
        const gdouble *xvalue = xgrainvalue->priv->values;
        const gdouble *yvalue = ygrainvalue->priv->values;
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

    if (quadratic) {
        g_assert(xgrainvalue && ygrainvalue);
        const gdouble *xvalue = xgrainvalue->priv->values;
        const gdouble *yvalue = ygrainvalue->priv->values;
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
calc_projected_area(GwyGrainValue *grainvalue,
                    const GwyField *field,
                    const guint *sizes)
{
    if (!grainvalue)
        return;

    GrainValue *priv = grainvalue->priv;
    g_return_if_fail(priv->builtin
                     && priv->builtin->id == GWY_GRAIN_VALUE_PROJECTED_AREA);
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
    g_return_if_fail(priv->builtin
                     && priv->builtin->id == GWY_GRAIN_VALUE_EQUIV_DISC_RADIUS);
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
    g_return_if_fail(priv->builtin
                     && priv->builtin->id == GWY_GRAIN_VALUE_SURFACE_AREA);
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
calc_median(GwyGrainValue *grainvalue,
            const guint *grains,
            const guint *sizes,
            const GwyField *field)
{
    if (!grainvalue)
        return;

    GrainValue *priv = grainvalue->priv;
    g_return_if_fail(priv->builtin
                     && priv->builtin->id == GWY_GRAIN_VALUE_MEDIAN);
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

void
_gwy_grain_value_evaluate_builtins(const GwyField *field,
                                   const GwyMaskField *mask,
                                   GwyGrainValue **grainvalues,
                                   guint nvalues)
{
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

    static const guint need_aux[GWY_GRAIN_NVALUES] = {
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
        need |= need_aux[id];
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

    // Calculate auxiliary quantities (in pixel lateral coordinates).
    // FIXME: Most of this can be formulated as separate calc-foo methods
    // such as below.
    calc_aux(field, grains, sizes, anyboundpos,
             ourvalues[GWY_GRAIN_VALUE_MINIMUM],
             ourvalues[GWY_GRAIN_VALUE_MAXIMUM],
             ourvalues[GWY_GRAIN_VALUE_CENTER_X],
             ourvalues[GWY_GRAIN_VALUE_CENTER_Y],
             ourvalues[GWY_GRAIN_VALUE_MEAN],
             linear, quadratic);

    /*
    const gdouble *d = field->data;
    gdouble dx = gwy_field_dx(field);
    gdouble dy = gwy_field_dy(field);
    gdouble diag = hypot(dx, dy);
    gdouble dxdy = dx*dy;
    gdouble eqside = sqrt(dxdy);
    GwyGrainValue *grainvalue;
    */

    // Calculate specific requested quantities that do not directly correspond
    // to auxiliary quantities.
    calc_projected_area(ourvalues[GWY_GRAIN_VALUE_PROJECTED_AREA],
                        field, sizes);
    calc_equiv_disc_radius(ourvalues[GWY_GRAIN_VALUE_EQUIV_DISC_RADIUS],
                           field, sizes);
    calc_surface_area(ourvalues[GWY_GRAIN_VALUE_SURFACE_AREA], grains, field);
    calc_median(ourvalues[GWY_GRAIN_VALUE_MEDIAN], grains, sizes, field);

#if 0
    if ((p = quantity_data[GWY_GRAIN_VALUE_HALF_HEIGHT_AREA])) {
        gdouble *zhalf;
        guint *zhsizes;

        /* Find the grain half-heights, i.e. (z_min + z_max)/2, first */
        zhalf = g_new(gdouble, ngrains + 1);
        for (gno = 0; gno <= ngrains; gno++)
            zhalf[gno] = (min[gno] + max[gno])/2.0;
        /* Calculate the area of pixels above the half-heights */
        zhsizes = g_new0(gint, ngrains + 1);
        for (k = 0; k < nn; k++) {
            gno = grains[k];
            if (d[k] >= zhalf[gno])
                zhsizes[gno]++;
        }
        for (gno = 0; gno <= ngrains; gno++)
            p[gno] = dxdy*zhsizes[gno];
        /* Finalize */
        g_free(zhalf);
        g_free(zhsizes);
    }
    if ((p = quantity_data[GWY_GRAIN_VALUE_FLAT_BOUNDARY_LENGTH])) {
        gwy_clear(p, ngrains + 1);
        /* Note the cycles go to xres and yres inclusive as we calculate the
         * boundary, not pixel interiors. */
        for (i = 0; i <= yres; i++) {
            for (j = 0; j <= xres; j++) {
                gint g1, g2, g3, g4, f;

                /* Hope compiler will optimize this mess... */
                g1 = (i > 0 && j > 0) ? grains[i*xres + j - xres - 1] : 0;
                g2 = (i > 0 && j < xres) ? grains[i*xres + j - xres] : 0;
                g3 = (i < yres && j > 0) ? grains[i*xres + j - 1] : 0;
                g4 = (i < yres && j < xres) ? grains[i*xres + j] : 0;
                f = (g1 > 0) + (g2 > 0) + (g3 > 0) + (g4 > 0);
                if (f == 0 || f == 4)
                    continue;

                if (f == 1 || f == 3) {
                    /* Try to avoid too many if-thens by using the fact they
                     * are all either zero or an identical value */
                    p[g1 | g2 | g3 | g4] += diag/2.0;
                }
                else if (g1 && g4) {
                    /* This works for both g1 == g4 and g1 != g4 */
                    p[g1] += diag/2.0;
                    p[g4] += diag/2.0;
                }
                else if (g2 && g3) {
                    /* This works for both g2 == g3 and g2 != g3 */
                    p[g2] += diag/2.0;
                    p[g3] += diag/2.0;
                }
                else if (g1 == g2)
                    p[g1 | g3] += dx;
                else if (g1 == g3)
                    p[g1 | g2] += dy;
                else {
                    g_assert_not_reached();
                }
            }
        }
    }
    if (quantity_data[GWY_GRAIN_VALUE_BOUNDARY_MINIMUM]
        || quantity_data[GWY_GRAIN_VALUE_BOUNDARY_MAXIMUM]) {
        gdouble *pmin = quantity_data[GWY_GRAIN_VALUE_BOUNDARY_MINIMUM];
        gdouble *pmax = quantity_data[GWY_GRAIN_VALUE_BOUNDARY_MAXIMUM];

        if (pmin) {
            for (gno = 0; gno <= ngrains; gno++)
                pmin[gno] = G_MAXDOUBLE;
        }
        if (pmax) {
            for (gno = 0; gno <= ngrains; gno++)
                pmax[gno] = -G_MAXDOUBLE;
        }

        for (i = 0; i < yres; i++) {
            for (j = 0; j < xres; j++) {
                gdouble z;

                /* Processing of the none-grain boundary is waste of time. */
                if (!(gno = grains[i*xres + j]))
                    continue;

                if (i && j && i < yres-1 && j < xres - 1
                    && grains[(i - 1)*xres + j] == gno
                    && grains[i*xres + j - 1] == gno
                    && grains[i*xres + j + 1] == gno
                    && grains[(i + 1)*xres + j] == gno)
                    continue;

                z = d[i*xres + j];
                if (pmin && z < pmin[gno])
                    pmin[gno] = z;
                if (pmax && z > pmax[gno])
                    pmax[gno] = z;
            }
        }
    }
    if (quantity_data[GWY_GRAIN_VALUE_MINIMUM_BOUND_SIZE]
        || quantity_data[GWY_GRAIN_VALUE_MINIMUM_BOUND_ANGLE]
        || quantity_data[GWY_GRAIN_VALUE_MAXIMUM_BOUND_SIZE]
        || quantity_data[GWY_GRAIN_VALUE_MAXIMUM_BOUND_ANGLE]) {
        gdouble *psmin = quantity_data[GWY_GRAIN_VALUE_MINIMUM_BOUND_SIZE];
        gdouble *psmax = quantity_data[GWY_GRAIN_VALUE_MAXIMUM_BOUND_SIZE];
        gdouble *pamin = quantity_data[GWY_GRAIN_VALUE_MINIMUM_BOUND_ANGLE];
        gdouble *pamax = quantity_data[GWY_GRAIN_VALUE_MAXIMUM_BOUND_ANGLE];
        GArray *vertices;

        /* Find the complete convex hulls */
        vertices = g_array_new(FALSE, FALSE, sizeof(GridPoint));
        for (gno = 1; gno <= ngrains; gno++) {
            gdouble dx = dx, dy = dy;

            find_grain_convex_hull(xres, yres, grains, anyboundpos[gno], vertices);
            if (psmin || pamin) {
                grain_minimum_bound(vertices, dx, dy, &dx, &dy);
                if (psmin)
                    psmin[gno] = hypot(dx, dy);
                if (pamin) {
                    pamin[gno] = atan2(-dy, dx);
                    if (pamin[gno] <= -G_PI/2.0)
                        pamin[gno] += G_PI;
                    else if (pamin[gno] > G_PI/2.0)
                        pamin[gno] -= G_PI;
                }
            }
            if (psmax || pamax) {
                grain_maximum_bound(vertices, dx, dy, &dx, &dy);
                if (psmax)
                    psmax[gno] = hypot(dx, dy);
                if (pamax) {
                    pamax[gno] = atan2(-dy, dx);
                    if (pamax[gno] <= -G_PI/2.0)
                        pamax[gno] += G_PI;
                    else if (pamax[gno] > G_PI/2.0)
                        pamax[gno] -= G_PI;
                }
            }
        }
        /* Finalize */
        g_array_free(vertices, TRUE);
    }
    if ((p = quantity_data[GWY_GRAIN_VALUE_CENTER_X])) {
        for (gno = 0; gno <= ngrains; gno++)
            p[gno] = dx*(p[gno] + 0.5) + field->xoff;
    }
    if ((p = quantity_data[GWY_GRAIN_VALUE_CENTER_Y])) {
        for (gno = 0; gno <= ngrains; gno++)
            p[gno] = dy*(p[gno] + 0.5) + field->yoff;
    }
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
    if (quantity_data[GWY_GRAIN_VALUE_SLOPE_THETA]
        || quantity_data[GWY_GRAIN_VALUE_SLOPE_PHI]) {
        gdouble *ptheta = quantity_data[GWY_GRAIN_VALUE_SLOPE_THETA];
        gdouble *pphi = quantity_data[GWY_GRAIN_VALUE_SLOPE_PHI];

        for (gno = 1; gno <= ngrains; gno++) {
            gdouble xx, yy, xy, xz, yz, det, bx, by;
            gdouble *lin = linear + 5*gno;

            xx = lin[0];
            xy = lin[1];
            yy = lin[2];
            xz = lin[3];
            yz = lin[4];
            det = xx*yy - xy*xy;
            if (det) {
                bx = (xz*yy - xy*yz)/(dx*det);
                by = (yz*xx - xy*xz)/(dy*det);
                if (ptheta)
                    ptheta[gno] = atan(hypot(bx, by));
                if (pphi)
                    pphi[gno] = atan2(by, -bx);
            }
            else {
                if (ptheta)
                    ptheta[gno] = 0.0;
                if (pphi)
                    pphi[gno] = 0.0;
            }
        }
    }
    if (quantity_data[GWY_GRAIN_VALUE_CURVATURE_CENTER_X]
        || quantity_data[GWY_GRAIN_VALUE_CURVATURE_CENTER_Y]
        || quantity_data[GWY_GRAIN_VALUE_CURVATURE_CENTER_Z]
        || quantity_data[GWY_GRAIN_VALUE_CURVATURE1]
        || quantity_data[GWY_GRAIN_VALUE_CURVATURE2]
        || quantity_data[GWY_GRAIN_VALUE_CURVATURE_ANGLE1]
        || quantity_data[GWY_GRAIN_VALUE_CURVATURE_ANGLE2]) {
        gdouble *px = quantity_data[GWY_GRAIN_VALUE_CURVATURE_CENTER_X];
        gdouble *py = quantity_data[GWY_GRAIN_VALUE_CURVATURE_CENTER_Y];
        gdouble *pz = quantity_data[GWY_GRAIN_VALUE_CURVATURE_CENTER_Z];
        gdouble *pk1 = quantity_data[GWY_GRAIN_VALUE_CURVATURE1];
        gdouble *pk2 = quantity_data[GWY_GRAIN_VALUE_CURVATURE2];
        gdouble *pa1 = quantity_data[GWY_GRAIN_VALUE_CURVATURE_ANGLE1];
        gdouble *pa2 = quantity_data[GWY_GRAIN_VALUE_CURVATURE_ANGLE2];
        gdouble mx = sqrt(dx/dy), my = sqrt(dy/dx);

        for (gno = 1; gno <= ngrains; gno++) {
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
            gdouble *lin = linear + 5*gno, *quad = quadratic + 12*gno;
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
                if (gwy_math_choleski_decompose(6, a)) {
                    b[0] = n*zvalue[gno];
                    b[1] = lin[3];
                    b[2] = lin[4];
                    b[3] = quad[9];
                    b[4] = quad[10];
                    b[5] = quad[11];
                    gwy_math_choleski_solve(6, a, b);
                    /* Get pixel aspect ratio right while keeping pixel size
                     * around 1. */
                    b[1] /= mx;
                    b[2] /= my;
                    b[3] /= mx*mx;
                    b[5] /= my*my;
                }
                else
                    n = 0;
            }

            /* Recycle a[] for the curvature parameters. */
            if (n >= 6)
                gwy_math_curvature(b, a+0, a+1, a+2, a+3, a+4, a+5, a+6);
            else {
                a[0] = a[1] = a[2] = a[4] = a[5] = 0.0;
                a[3] = G_PI/2.0;
                a[6] = zvalue[gno];
            }
            if (pk1)
                pk1[gno] = a[0]/(eqside*eqside);
            if (pk2)
                pk2[gno] = a[1]/(eqside*eqside);
            if (pa1)
                pa1[gno] = a[2];
            if (pa2)
                pa2[gno] = a[3];
            if (px)
                px[gno] = eqside*a[4] + xvalue[gno];
            if (py)
                py[gno] = eqside*a[5] + yvalue[gno];
            if (pz)
                pz[gno] = a[6];
        }
    }
#endif

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
