/*
 *  $Id$
 *  Copyright (C) 2009 David Nečas (Yeti).
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

#include "testlibgwy.h"

/***************************************************************************
 *
 * Basic math functions
 *
 ***************************************************************************/

void
test_math_powi_actual(void)
{
    g_assert_cmpfloat(gwy_powi(0.0, 0), ==, 1.0);

    long double x = -G_PI;
    long double pp = 1.0, pm = 1.0;
    for (gint i = 0; i < 40; i++) {
        g_assert_cmpfloat(fabs(gwy_powi(x, i) - pp), <=, 8e-16*fabs(pp));
        g_assert_cmpfloat(fabs(gwy_powi(x, -i) - pm), <=, 8e-16*fabs(pm));
        pp *= x;
        pm /= x;
    }
}

// Ensure we get the Gwyddion implementation
#undef gwy_powi
double gwy_powi(double x, int i);

void
test_math_powi_our(void)
{
    g_assert_cmpfloat(gwy_powi(0.0, 0), ==, 1.0);

    long double x = -G_PI;
    long double pp = 1.0, pm = 1.0;
    double arg = x;
    for (gint i = 0; i < 40; i++) {
        g_assert_cmpfloat(fabs(gwy_powi(arg, i) - pp), <=, 8e-16*fabs(pp));
        g_assert_cmpfloat(fabs(gwy_powi(arg, -i) - pm), <=, 8e-16*fabs(pm));
        pp *= x;
        pm /= x;
    }
}

void
test_math_overlapping(void)
{
    g_assert(gwy_overlapping(0, 1, 0, 1));
    g_assert(gwy_overlapping(1, 1, 0, 2));
    g_assert(gwy_overlapping(0, 3, 1, 1));
    g_assert(gwy_overlapping(1, 2, 0, 2));
    g_assert(!gwy_overlapping(1, 1, 0, 1));
    g_assert(!gwy_overlapping(0, 1, 1, 1));
    g_assert(!gwy_overlapping(2, 1, 0, 2));
    g_assert(!gwy_overlapping(0, 2, 0, 0));
    g_assert(!gwy_overlapping(0, 2, 1, 0));
    g_assert(!gwy_overlapping(0, 2, 2, 0));
}

void
test_math_intersecting(void)
{
    g_assert(gwy_math_intersecting(0.0, 1.0, 0.0, 1.0));
    g_assert(gwy_math_intersecting(0.0, 1.0, 1.0, 2.0));
    g_assert(gwy_math_intersecting(1.0, 2.0, 0.0, 1.0));
    g_assert(gwy_math_intersecting(0.0, 1.0, 0.0, 0.0));
    g_assert(gwy_math_intersecting(0.0, 1.0, 0.5, 0.5));
    g_assert(gwy_math_intersecting(0.0, 1.0, 1.0, 1.0));
    g_assert(gwy_math_intersecting(0.0, 0.0, 0.0, 1.0));
    g_assert(gwy_math_intersecting(0.5, 0.5, 0.0, 1.0));
    g_assert(gwy_math_intersecting(1.0, 1.0, 0.0, 1.0));
    g_assert(gwy_math_intersecting(0.0, 1.0, -HUGE_VAL, 1.0));
    g_assert(gwy_math_intersecting(0.0, 1.0, 0.0, HUGE_VAL));
    g_assert(gwy_math_intersecting(0.0, 1.0, -HUGE_VAL, HUGE_VAL));
    g_assert(!gwy_math_intersecting(0.0, 1.0 - 1e-16, 1.0 + 1e-16, 2.0));
    g_assert(!gwy_math_intersecting(1.0 + 1e-16, 2.0, 0.0, 1.0 - 1e-16));
}

void
test_math_power_sum(void)
{
    for (guint p = 0; p <= 11; p++) {
        for (guint n = 0; n <= p+2; n++) {
            gdouble expected_sum = 0.0;

            for (guint i = 1; i <= n; i++)
                expected_sum += gwy_powi(i, p);

            gdouble sum = gwy_power_sum(n, p);

            g_assert_cmpfloat(fabs(sum - expected_sum),
                              <=,
                              1e-14*(fabs(sum) + fabs(expected_sum)));
        }
    }
}

void
test_math_curvature(void)
{
    gdouble xc, yc, zc, k1, k2, phi1, phi2;
    guint ndims;
    GRand *rng = g_rand_new();
    g_rand_set_seed(rng, 42);

    // Flat surfaces
    static const gdouble coeffs1[] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
    ndims = gwy_math_curvature(coeffs1, &k1, &k2, &phi1, &phi2, &xc, &yc, &zc);
    g_assert_cmpuint(ndims, ==, 0);
    g_assert_cmpfloat(k1, ==, 0.0);
    g_assert_cmpfloat(k2, ==, 0.0);
    g_assert_cmpfloat(phi1, ==, 0.0);
    g_assert_cmpfloat(phi2, ==, G_PI/2.0);
    g_assert_cmpfloat(xc, ==, 0.0);
    g_assert_cmpfloat(yc, ==, 0.0);
    g_assert_cmpfloat(zc, ==, 0.0);

    static const gdouble coeffs2[] = { 1.0, 2.0, 3.0, 0.0, 0.0, 0.0 };
    ndims = gwy_math_curvature(coeffs2, &k1, &k2, &phi1, &phi2, &xc, &yc, &zc);
    g_assert_cmpuint(ndims, ==, 0);
    g_assert_cmpfloat(k1, ==, 0.0);
    g_assert_cmpfloat(k2, ==, 0.0);
    g_assert_cmpfloat(phi1, ==, 0.0);
    g_assert_cmpfloat(phi2, ==, G_PI/2.0);
    g_assert_cmpfloat(xc, ==, 0.0);
    g_assert_cmpfloat(yc, ==, 0.0);
    g_assert_cmpfloat(zc, ==, 1.0);

    // Centered surfaces
    static const gdouble coeffs3[] = { 0.0, 0.0, 0.0, 0.5, 0.0, 1.0 };
    ndims = gwy_math_curvature(coeffs3, &k1, &k2, &phi1, &phi2, &xc, &yc, &zc);
    g_assert_cmpuint(ndims, ==, 2);
    g_assert_cmpfloat(k1, ==, 1.0);
    g_assert_cmpfloat(k2, ==, 2.0);
    // As the angle is exactly π/2 we get -G_PI/2 or G_PI/2 depending on the
    // rounding errors.  Accept both but check the interval.
    if (phi2 > G_PI/4) {
        g_assert_cmpfloat(fabs(phi1), <=, 1e-14);
        g_assert_cmpfloat(fabs(phi2 - G_PI/2), <=, 1e-14);
    }
    else {
        g_assert_cmpfloat(fabs(phi1 + G_PI/2), <=, 1e-14);
        g_assert_cmpfloat(fabs(phi2), <=, 1e-14);
    }
    g_assert_cmpfloat(phi1, >=, -G_PI/2);
    g_assert_cmpfloat(phi1, <=, G_PI/2);
    g_assert_cmpfloat(phi2, >=, -G_PI/2);
    g_assert_cmpfloat(phi2, <=, G_PI/2);
    g_assert_cmpfloat(xc, ==, 0.0);
    g_assert_cmpfloat(yc, ==, 0.0);
    g_assert_cmpfloat(zc, ==, 0.0);

    static const gdouble coeffs4[] = { 0.0, 0.0, 0.0, 0.0, 1.0, 0.0 };
    ndims = gwy_math_curvature(coeffs4, &k1, &k2, &phi1, &phi2, &xc, &yc, &zc);
    g_assert_cmpuint(ndims, ==, 2);
    // Left-handed system means positive κ corresponds to negative φ.
    g_assert_cmpfloat(fabs(k1 + 1.0), <=, 1e-14);
    g_assert_cmpfloat(fabs(k2 - 1.0), <=, 1e-14);
    g_assert_cmpfloat(fabs(phi1 + G_PI/4), <=, 1e-14);
    g_assert_cmpfloat(fabs(phi2 - G_PI/4), <=, 1e-14);
    g_assert_cmpfloat(xc, ==, 0.0);
    g_assert_cmpfloat(yc, ==, 0.0);
    g_assert_cmpfloat(zc, ==, 0.0);

    static const gdouble coeffs5[] = { 0.0, 0.0, 0.0, 0.0, -1.0, 0.0 };
    ndims = gwy_math_curvature(coeffs5, &k1, &k2, &phi1, &phi2, &xc, &yc, &zc);
    g_assert_cmpuint(ndims, ==, 2);
    g_assert_cmpfloat(fabs(k1 + 1.0), <=, 1e-14);
    g_assert_cmpfloat(fabs(k2 - 1.0), <=, 1e-14);
    g_assert_cmpfloat(fabs(phi1 - G_PI/4), <=, 1e-14);
    g_assert_cmpfloat(fabs(phi2 + G_PI/4), <=, 1e-14);
    g_assert_cmpfloat(xc, ==, 0.0);
    g_assert_cmpfloat(yc, ==, 0.0);
    g_assert_cmpfloat(zc, ==, 0.0);

    for (guint i = 0; i < 10; i++) {
        gdouble alpha = g_rand_double_range(rng, -G_PI/4.0, G_PI/4.0);
        gdouble ca = cos(2.0*alpha), sa = sin(2.0*alpha);
        gdouble coeffs6[] = { 0.0, 0.0, 0.0, ca/2.0, sa, -ca/2.0 };
        ndims = gwy_math_curvature(coeffs6,
                                   &k1, &k2, &phi1, &phi2, &xc, &yc, &zc);
        g_assert_cmpuint(ndims, ==, 2);
        g_assert_cmpfloat(fabs(k1 + 1.0), <=, 1e-14);
        g_assert_cmpfloat(fabs(k2 - 1.0), <=, 1e-14);
        g_assert_cmpfloat(phi1, >=, -G_PI/2);
        g_assert_cmpfloat(phi1, <=, G_PI/2);
        g_assert_cmpfloat(phi2, >=, -G_PI/2);
        g_assert_cmpfloat(phi2, <=, G_PI/2);
        g_assert_cmpfloat(fmin(fabs(phi1 - alpha + G_PI/2.0),
                               fabs(phi1 - alpha - G_PI/2.0)), <=, 1e-14);
        g_assert_cmpfloat(fabs(phi2 - alpha), <=, 1e-14);
        g_assert_cmpfloat(xc, ==, 0.0);
        g_assert_cmpfloat(yc, ==, 0.0);
        g_assert_cmpfloat(zc, ==, 0.0);
    }

    // Shifted surfaces
    static const gdouble coeffs7[] = { 2.0, -2.0, 2.0, 1.0, 0.0, 1.0 };
    ndims = gwy_math_curvature(coeffs7, &k1, &k2, &phi1, &phi2, &xc, &yc, &zc);
    g_assert_cmpuint(ndims, ==, 2);
    g_assert_cmpfloat(k1, ==, 2.0);
    g_assert_cmpfloat(k2, ==, 2.0);
    // As the angle is exactly π/2 we get -G_PI/2 or G_PI/2 depending on the
    // rounding errors.  Accept both but check the interval.
    if (phi2 > G_PI/4) {
        g_assert_cmpfloat(fabs(phi1), <=, 1e-14);
        g_assert_cmpfloat(fabs(phi2 - G_PI/2), <=, 1e-14);
    }
    else {
        g_assert_cmpfloat(fabs(phi1 + G_PI/2), <=, 1e-14);
        g_assert_cmpfloat(fabs(phi2), <=, 1e-14);
    }
    g_assert_cmpfloat(phi1, >=, -G_PI/2);
    g_assert_cmpfloat(phi1, <=, G_PI/2);
    g_assert_cmpfloat(phi2, >=, -G_PI/2);
    g_assert_cmpfloat(phi2, <=, G_PI/2);
    g_assert_cmpfloat(fabs(xc - 1.0), <=, 1e-14);
    g_assert_cmpfloat(fabs(yc + 1.0), <=, 1e-14);
    g_assert_cmpfloat(fabs(zc), <=, 1e-14);

    static const gdouble coeffs8[] = { 0.5, 2.0, 1.0, 1.0, 0.0, -0.5 };
    ndims = gwy_math_curvature(coeffs8, &k1, &k2, &phi1, &phi2, &xc, &yc, &zc);
    g_assert_cmpuint(ndims, ==, 2);
    g_assert_cmpfloat(k1, ==, -1.0);
    g_assert_cmpfloat(k2, ==, 2.0);
    // As the angle is exactly π/2 we get -G_PI/2 or G_PI/2 depending on the
    // rounding errors.  Accept both but check the interval.
    if (phi1 > G_PI/4) {
        g_assert_cmpfloat(fabs(phi2), <=, 1e-14);
        g_assert_cmpfloat(fabs(phi1 - G_PI/2), <=, 1e-14);
    }
    else {
        g_assert_cmpfloat(fabs(phi1), <=, 1e-14);
        g_assert_cmpfloat(fabs(phi2 + G_PI/2), <=, 1e-14);
    }
    g_assert_cmpfloat(phi1, >=, -G_PI/2);
    g_assert_cmpfloat(phi1, <=, G_PI/2);
    g_assert_cmpfloat(phi2, >=, -G_PI/2);
    g_assert_cmpfloat(phi2, <=, G_PI/2);
    g_assert_cmpfloat(fabs(xc + 1.0), <=, 1e-14);
    g_assert_cmpfloat(fabs(yc - 1.0), <=, 1e-14);
    g_assert_cmpfloat(fabs(zc), <=, 1e-14);

    g_rand_free(rng);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
