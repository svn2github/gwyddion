/*
 *  $Id$
 *  Copyright (C) 2011 David Necas (Yeti).
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

#include "libgwy/macros.h"
#include "libgwy/math.h"
#include "libgwy/surface-statistics.h"
#include "libgwy/surface-internal.h"

#define CVAL GWY_SURFACE_CVAL
#define CBIT GWY_SURFACE_CBIT
#define CTEST GWY_SURFACE_CTEST

/**
 * gwy_surface_min_max_full:
 * @surface: A surface.
 * @min: (allow-none):
 *       Location to store the minimum value to, or %NULL.
 * @max: (allow-none):
 *       Location to store the maximum value to, or %NULL.
 *
 * Finds the minimum and maximum value in a surface.
 *
 * If the surface is empty @min is set to a huge positive value and @max to
 * a huge negative value.
 *
 * The minimum and maximum of the entire surface are cached, see
 * gwy_surface_invalidate().
 **/
void
gwy_surface_min_max_full(const GwySurface *surface,
                         gdouble *pmin,
                         gdouble *pmax)
{
    g_return_if_fail(GWY_IS_SURFACE(surface));
    if (!pmin && !pmax)
        return;

    if (CTEST(surface->priv, MIN) && CTEST(surface->priv, MAX)) {
        GWY_MAYBE_SET(pmin, CVAL(surface->priv, MIN));
        GWY_MAYBE_SET(pmax, CVAL(surface->priv, MAX));
        return;
    }

    gdouble min = HUGE_VAL;
    gdouble max = -HUGE_VAL;
    const GwyXYZ *p = surface->data;
    for (guint i = surface->n; i; i--, p++) {
        if (p->z < min)
            min = p->z;
        if (p->z > max)
            max = p->z;
    }
    CVAL(surface->priv, MIN) = min;
    CVAL(surface->priv, MAX) = max;
    surface->priv->cached |= CBIT(MIN) | CBIT(MAX);
    GWY_MAYBE_SET(pmin, min);
    GWY_MAYBE_SET(pmax, max);
}

static void
surface_ensure_range(const GwySurface *surface)
{
    if (surface->priv->cached_range)
        return;

    gdouble xmin = HUGE_VAL, ymin = HUGE_VAL;
    gdouble xmax = -HUGE_VAL, ymax = -HUGE_VAL;
    const GwyXYZ *p = surface->data;
    for (guint i = surface->n; i; i--, p++) {
        if (p->x < xmin)
            xmin = p->x;
        if (p->x > xmax)
            xmax = p->x;
        if (p->y < ymin)
            ymin = p->y;
        if (p->y > ymax)
            ymax = p->y;
    }

    surface->priv->cached_range = TRUE;
    surface->priv->xmin = xmin;
    surface->priv->xmax = xmax;
    surface->priv->ymin = ymin;
    surface->priv->ymax = ymax;
}

/**
 * gwy_surface_xrange_full:
 * @surface: A surface.
 * @min: (allow-none):
 *       Location to store the minimum x-coordinate to, or %NULL.
 * @max: (allow-none):
 *       Location to store the maximum x-coordinate to, or %NULL.
 *
 * Finds the minimum and maximum x-coordinates of an entire surface.
 *
 * If the surface is empty @min and @max are set to NaN.
 *
 * The bounding box of the entire surface is cached, see
 * gwy_surface_invalidate().
 **/
void
gwy_surface_xrange_full(const GwySurface *surface,
                        gdouble *pmin,
                        gdouble *pmax)
{
    g_return_if_fail(GWY_IS_SURFACE(surface));
    if (!pmin && !pmax)
        return;

    surface_ensure_range(surface);
    GWY_MAYBE_SET(pmin, surface->priv->xmin);
    GWY_MAYBE_SET(pmax, surface->priv->xmax);
}

/**
 * gwy_surface_yrange_full:
 * @surface: A surface.
 * @min: (allow-none):
 *       Location to store the minimum y-coordinate to, or %NULL.
 * @max: (allow-none):
 *       Location to store the maximum y-coordinate to, or %NULL.
 *
 * Finds the minimum and maximum y-coordinates of an entire surface.
 *
 * If the surface is empty @min and @max are set to NaN.
 *
 * The bounding box of the entire surface is cached, see
 * gwy_surface_invalidate().
 **/
void
gwy_surface_yrange_full(const GwySurface *surface,
                        gdouble *pmin,
                        gdouble *pmax)
{
    g_return_if_fail(GWY_IS_SURFACE(surface));
    if (!pmin && !pmax)
        return;

    surface_ensure_range(surface);
    GWY_MAYBE_SET(pmin, surface->priv->ymin);
    GWY_MAYBE_SET(pmax, surface->priv->ymax);
}

/**
 * gwy_surface_mean_full:
 * @surface: A surface.
 *
 * Calculates the mean value of an entire surface.
 *
 * The mean value of the entire surface is cached, see
 * gwy_surface_invalidate().
 *
 * Returns: The mean value.  The mean value of an empty surface is NaN.
 **/
gdouble
gwy_surface_mean_full(const GwySurface *surface)
{
    g_return_val_if_fail(GWY_IS_SURFACE(surface), NAN);
    if (G_UNLIKELY(!surface->n))
        return NAN;
    if (CTEST(surface->priv, AVG))
        return CVAL(surface->priv, AVG);

    gdouble s = 0.0;
    const GwyXYZ *p = surface->data;
    for (guint i = surface->n; i; i--, p++)
        s += p->z;

    s /= surface->n;
    CVAL(surface->priv, AVG) = s;
    surface->priv->cached |= CBIT(AVG);

    return s;
}

/**
 * gwy_surface_rms_full:
 * @surface: A surface.
 *
 * Calculates the mean square value of an entire surface.
 *
 * The mean square value of the entire surface is cached, see
 * gwy_surface_invalidate().
 *
 * Returns: The mean square value.  The mean value square of an empty surface
 *          is zero.
 **/
gdouble
gwy_surface_rms_full(const GwySurface *surface)
{
    g_return_val_if_fail(GWY_IS_SURFACE(surface), 0.0);
    if (G_UNLIKELY(!surface->n))
        return 0.0;
    if (CTEST(surface->priv, RMS))
        return CVAL(surface->priv, RMS);

    gdouble mean = gwy_surface_mean_full(surface);
    gdouble s = 0.0;
    const GwyXYZ *p = surface->data;
    for (guint i = surface->n; i; i--, p++)
        s += (p->z - mean)*(p->x - mean);

    s = sqrt(s/surface->n);
    CVAL(surface->priv, RMS) = s;
    surface->priv->cached |= CBIT(RMS);

    return s;
}

/**
 * SECTION: surface-statistics
 * @section_id: GwySurface-statistics
 * @title: GwySurface statistics
 * @short_description: Statistical characteristics of surfaces
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
