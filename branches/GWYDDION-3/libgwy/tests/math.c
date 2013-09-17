/*
 *  $Id$
 *  Copyright (C) 2009-2013 David Nečas (Yeti).
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
test_math_spow(void)
{
    g_assert_cmpfloat(gwy_spow(1.0, 1.0), ==, 1.0);
    g_assert_cmpfloat(gwy_spow(-1.0, 1.0), ==, -1.0);
    g_assert_cmpfloat(gwy_spow(1e42, 1.0), ==, 1e42);
    g_assert_cmpfloat(gwy_spow(-1e42, 1.0), ==, -1e42);
    g_assert_cmpfloat(gwy_spow(0.0, 1.0), ==, 0.0);

    g_assert_cmpfloat(gwy_spow(3.0, 2.0), ==, 9.0);
    g_assert_cmpfloat(gwy_spow(-3.0, 2.0), ==, -9.0);
    g_assert_cmpfloat(gwy_spow(2.0, -2.0), ==, 0.25);
    g_assert_cmpfloat(gwy_spow(-2.0, -2.0), ==, -0.25);
    g_assert_cmpfloat(gwy_spow(9.0, 0.5), ==, 3.0);
    g_assert_cmpfloat(gwy_spow(-9.0, 0.5), ==, -3.0);
}

void
test_math_ssqrt(void)
{
    g_assert_cmpfloat(gwy_ssqrt(0.0), ==, 0.0);
    g_assert_cmpfloat(gwy_ssqrt(1.0), ==, 1.0);
    g_assert_cmpfloat(gwy_ssqrt(-1.0), ==, -1.0);
    g_assert_cmpfloat(gwy_ssqrt(9.0), ==, 3.0);
    g_assert_cmpfloat(gwy_ssqrt(-9.0), ==, -3.0);
    g_assert_cmpfloat(gwy_ssqrt(INFINITY), ==, INFINITY);
    g_assert_cmpfloat(gwy_ssqrt(-INFINITY), ==, -INFINITY);
}

void
test_math_ssqr(void)
{
    g_assert_cmpfloat(gwy_ssqr(0.0), ==, 0.0);
    g_assert_cmpfloat(gwy_ssqr(1.0), ==, 1.0);
    g_assert_cmpfloat(gwy_ssqr(-1.0), ==, -1.0);
    g_assert_cmpfloat(gwy_ssqr(3.0), ==, 9.0);
    g_assert_cmpfloat(gwy_ssqr(-3.0), ==, -9.0);
    g_assert_cmpfloat(gwy_ssqr(INFINITY), ==, INFINITY);
    g_assert_cmpfloat(gwy_ssqr(-INFINITY), ==, -INFINITY);
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
    for (guint p = 0; p <= 20; p++) {
        for (guint n = 0; n <= p+3; n++) {
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
test_math_curvature_at_centre_flat(void)
{
    GwyCurvatureParams curv;
    guint ndims;

    // Flat surfaces
    static const gdouble coeffs1[] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
    ndims = gwy_math_curvature_at_centre(coeffs1, &curv);
    g_assert_cmpuint(ndims, ==, 0);
    g_assert_cmpfloat(curv.k1, ==, 0.0);
    g_assert_cmpfloat(curv.k2, ==, 0.0);
    g_assert_cmpfloat(curv.phi1, ==, 0.0);
    g_assert_cmpfloat(curv.phi2, ==, G_PI/2.0);
    g_assert_cmpfloat(curv.xc, ==, 0.0);
    g_assert_cmpfloat(curv.yc, ==, 0.0);
    g_assert_cmpfloat(curv.zc, ==, 0.0);

    static const gdouble coeffs2[] = { 1.0, 2.0, 3.0, 0.0, 0.0, 0.0 };
    ndims = gwy_math_curvature_at_centre(coeffs2, &curv);
    g_assert_cmpuint(ndims, ==, 0);
    g_assert_cmpfloat(curv.k1, ==, 0.0);
    g_assert_cmpfloat(curv.k2, ==, 0.0);
    g_assert_cmpfloat(curv.phi1, ==, 0.0);
    g_assert_cmpfloat(curv.phi2, ==, G_PI/2.0);
    g_assert_cmpfloat(curv.xc, ==, 0.0);
    g_assert_cmpfloat(curv.yc, ==, 0.0);
    g_assert_cmpfloat(curv.zc, ==, 1.0);
}

void
test_math_curvature_at_centre_centered(void)
{
    GwyCurvatureParams curv;
    guint ndims;
    GRand *rng = g_rand_new_with_seed(42);

    // Centered surfaces
    static const gdouble coeffs3[] = { 0.0, 0.0, 0.0, 0.5, 0.0, 1.0 };
    ndims = gwy_math_curvature_at_centre(coeffs3, &curv);
    g_assert_cmpuint(ndims, ==, 2);
    g_assert_cmpfloat(curv.k1, ==, 1.0);
    g_assert_cmpfloat(curv.k2, ==, 2.0);
    // As the angle is exactly π/2 we get -G_PI/2 or G_PI/2 depending on the
    // rounding errors.  Accept both but check the interval.
    if (curv.phi2 > G_PI/4) {
        g_assert_cmpfloat(fabs(curv.phi1), <=, 1e-14);
        gwy_assert_floatval(curv.phi2, G_PI/2, 1e-14);
    }
    else {
        g_assert_cmpfloat(fabs(curv.phi1 + G_PI/2), <=, 1e-14);
        g_assert_cmpfloat(fabs(curv.phi2), <=, 1e-14);
    }
    g_assert_cmpfloat(curv.phi1, >=, -G_PI/2);
    g_assert_cmpfloat(curv.phi1, <=, G_PI/2);
    g_assert_cmpfloat(curv.phi2, >=, -G_PI/2);
    g_assert_cmpfloat(curv.phi2, <=, G_PI/2);
    g_assert_cmpfloat(curv.xc, ==, 0.0);
    g_assert_cmpfloat(curv.yc, ==, 0.0);
    g_assert_cmpfloat(curv.zc, ==, 0.0);

    static const gdouble coeffs4[] = { 0.0, 0.0, 0.0, 0.0, 1.0, 0.0 };
    ndims = gwy_math_curvature_at_centre(coeffs4, &curv);
    g_assert_cmpuint(ndims, ==, 2);
    // Left-handed system means positive κ corresponds to negative φ.
    g_assert_cmpfloat(fabs(curv.k1 + 1.0), <=, 1e-14);
    gwy_assert_floatval(curv.k2, 1.0, 1e-14);
    g_assert_cmpfloat(fabs(curv.phi1 + G_PI/4), <=, 1e-14);
    gwy_assert_floatval(curv.phi2, G_PI/4, 1e-14);
    g_assert_cmpfloat(curv.xc, ==, 0.0);
    g_assert_cmpfloat(curv.yc, ==, 0.0);
    g_assert_cmpfloat(curv.zc, ==, 0.0);

    static const gdouble coeffs5[] = { 0.0, 0.0, 0.0, 0.0, -1.0, 0.0 };
    ndims = gwy_math_curvature_at_centre(coeffs5, &curv);
    g_assert_cmpuint(ndims, ==, 2);
    g_assert_cmpfloat(fabs(curv.k1 + 1.0), <=, 1e-14);
    gwy_assert_floatval(curv.k2, 1.0, 1e-14);
    gwy_assert_floatval(curv.phi1, G_PI/4, 1e-14);
    g_assert_cmpfloat(fabs(curv.phi2 + G_PI/4), <=, 1e-14);
    g_assert_cmpfloat(curv.xc, ==, 0.0);
    g_assert_cmpfloat(curv.yc, ==, 0.0);
    g_assert_cmpfloat(curv.zc, ==, 0.0);

    for (guint i = 0; i < 10; i++) {
        gdouble alpha = g_rand_double_range(rng, -G_PI/4.0, G_PI/4.0);
        gdouble ca = cos(2.0*alpha), sa = sin(2.0*alpha);
        gdouble coeffs[] = { 0.0, 0.0, 0.0, ca/2.0, sa, -ca/2.0 };
        ndims = gwy_math_curvature_at_centre(coeffs, &curv);
        g_assert_cmpuint(ndims, ==, 2);
        g_assert_cmpfloat(fabs(curv.k1 + 1.0), <=, 1e-14);
        gwy_assert_floatval(curv.k2, 1.0, 1e-14);
        g_assert_cmpfloat(curv.phi1, >=, -G_PI/2);
        g_assert_cmpfloat(curv.phi1, <=, G_PI/2);
        g_assert_cmpfloat(curv.phi2, >=, -G_PI/2);
        g_assert_cmpfloat(curv.phi2, <=, G_PI/2);
        g_assert_cmpfloat(fmin(fabs(curv.phi1 - alpha + G_PI/2.0),
                               fabs(curv.phi1 - alpha - G_PI/2.0)), <=, 1e-14);
        gwy_assert_floatval(curv.phi2, alpha, 1e-14);
        g_assert_cmpfloat(curv.xc, ==, 0.0);
        g_assert_cmpfloat(curv.yc, ==, 0.0);
        g_assert_cmpfloat(curv.zc, ==, 0.0);
    }

    for (guint i = 0; i < 20; i++) {
        gdouble alpha = g_rand_double_range(rng, -G_PI/4.0, G_PI/4.0);
        gdouble a = g_rand_double_range(rng, -5.0, 5.0);
        gdouble b = g_rand_double_range(rng, -5.0, 5.0);
        gdouble c2a = cos(alpha)*cos(alpha),
                s2a = sin(alpha)*sin(alpha),
                csa = cos(alpha)*sin(alpha);
        gdouble cxx = a*c2a + b*s2a, cyy = a*s2a + b*c2a, cxy = 2.0*(a - b)*csa;
        gdouble coeffs[] = { 0.0, 0.0, 0.0, cxx, cxy, cyy };
        ndims = gwy_math_curvature_at_centre(coeffs, &curv);
        // XXX: If we are unlucky, a or b ≈ 0 and ndims is just 1.
        g_assert_cmpuint(ndims, ==, 2);
        g_assert_cmpfloat(fabs(curv.k1 - 2.0*fmin(a, b)), <=, 1e-14);
        g_assert_cmpfloat(fabs(curv.k2 - 2.0*fmax(a, b)), <=, 1e-14);
        g_assert_cmpfloat(curv.phi1, >=, -G_PI/2);
        g_assert_cmpfloat(curv.phi1, <=, G_PI/2);
        g_assert_cmpfloat(curv.phi2, >=, -G_PI/2);
        g_assert_cmpfloat(curv.phi2, <=, G_PI/2);
        if (a <= b) {
            g_assert_cmpfloat(fmin(fabs(curv.phi2 - alpha + G_PI/2.0),
                                   fabs(curv.phi2 - alpha - G_PI/2.0)),
                              <=, 1e-14);
            gwy_assert_floatval(curv.phi1, alpha, 1e-14);
        }
        else {
            g_assert_cmpfloat(fmin(fabs(curv.phi1 - alpha + G_PI/2.0),
                                   fabs(curv.phi1 - alpha - G_PI/2.0)),
                              <=, 1e-14);
            gwy_assert_floatval(curv.phi2, alpha, 1e-14);
        }
        g_assert_cmpfloat(curv.xc, ==, 0.0);
        g_assert_cmpfloat(curv.yc, ==, 0.0);
        g_assert_cmpfloat(curv.zc, ==, 0.0);
    }

    g_rand_free(rng);
}

void
test_math_curvature_at_centre_shifted(void)
{
    GwyCurvatureParams curv;
    guint ndims;
    GRand *rng = g_rand_new_with_seed(42);

    // Shifted surfaces
    static const gdouble coeffs6[] = { 2.0, -2.0, 2.0, 1.0, 0.0, 1.0 };
    ndims = gwy_math_curvature_at_centre(coeffs6, &curv);
    g_assert_cmpuint(ndims, ==, 2);
    g_assert_cmpfloat(curv.k1, ==, 2.0);
    g_assert_cmpfloat(curv.k2, ==, 2.0);
    // As the angle is exactly π/2 we get -G_PI/2 or G_PI/2 depending on the
    // rounding errors.  Accept both but check the interval.
    if (curv.phi2 > G_PI/4) {
        g_assert_cmpfloat(fabs(curv.phi1), <=, 1e-14);
        gwy_assert_floatval(curv.phi2, G_PI/2, 1e-14);
    }
    else {
        g_assert_cmpfloat(fabs(curv.phi1 + G_PI/2), <=, 1e-14);
        g_assert_cmpfloat(fabs(curv.phi2), <=, 1e-14);
    }
    g_assert_cmpfloat(curv.phi1, >=, -G_PI/2);
    g_assert_cmpfloat(curv.phi1, <=, G_PI/2);
    g_assert_cmpfloat(curv.phi2, >=, -G_PI/2);
    g_assert_cmpfloat(curv.phi2, <=, G_PI/2);
    gwy_assert_floatval(curv.xc, 1.0, 1e-14);
    g_assert_cmpfloat(fabs(curv.yc + 1.0), <=, 1e-14);
    g_assert_cmpfloat(fabs(curv.zc), <=, 1e-14);

    static const gdouble coeffs7[] = { 0.5, 2.0, 1.0, 1.0, 0.0, -0.5 };
    ndims = gwy_math_curvature_at_centre(coeffs7, &curv);
    g_assert_cmpuint(ndims, ==, 2);
    g_assert_cmpfloat(curv.k1, ==, -1.0);
    g_assert_cmpfloat(curv.k2, ==, 2.0);
    // As the angle is exactly π/2 we get -G_PI/2 or G_PI/2 depending on the
    // rounding errors.  Accept both but check the interval.
    if (curv.phi1 > G_PI/4) {
        g_assert_cmpfloat(fabs(curv.phi2), <=, 1e-14);
        gwy_assert_floatval(curv.phi1, G_PI/2, 1e-14);
    }
    else {
        g_assert_cmpfloat(fabs(curv.phi1), <=, 1e-14);
        g_assert_cmpfloat(fabs(curv.phi2 + G_PI/2), <=, 1e-14);
    }
    g_assert_cmpfloat(curv.phi1, >=, -G_PI/2);
    g_assert_cmpfloat(curv.phi1, <=, G_PI/2);
    g_assert_cmpfloat(curv.phi2, >=, -G_PI/2);
    g_assert_cmpfloat(curv.phi2, <=, G_PI/2);
    g_assert_cmpfloat(fabs(curv.xc + 1.0), <=, 1e-14);
    gwy_assert_floatval(curv.yc, 1.0, 1e-14);
    g_assert_cmpfloat(fabs(curv.zc), <=, 1e-14);

    g_rand_free(rng);
}

void
test_math_curvature_at_centre_general(void)
{
    GwyCurvatureParams curv;
    guint ndims;
    GRand *rng = g_rand_new_with_seed(42);

    // Rotated shifted surface
    for (guint i = 0; i < 30; i++) {
        gdouble alpha = g_rand_double_range(rng, -G_PI/4.0, G_PI/4.0);
        gdouble a = g_rand_double_range(rng, -5.0, 5.0);
        gdouble b = g_rand_double_range(rng, -5.0, 5.0);
        gdouble p = g_rand_double_range(rng, -2.0, 2.0);
        gdouble q = g_rand_double_range(rng, -2.0, 2.0);
        gdouble c2a = cos(alpha)*cos(alpha),
                s2a = sin(alpha)*sin(alpha),
                csa = cos(alpha)*sin(alpha);
        gdouble cxx = a*c2a + b*s2a, cyy = a*s2a + b*c2a, cxy = 2.0*(a - b)*csa;
        gdouble coeffs[] = {
            p*p*cxx + q*q*cyy + p*q*cxy,
            2.0*p*cxx + q*cxy, 2.0*q*cyy + p*cxy,
            cxx, cxy, cyy
        };
        ndims = gwy_math_curvature_at_centre(coeffs, &curv);
        // XXX: If we are unlucky, a or b ≈ 0 and ndims is just 1.
        g_assert_cmpuint(ndims, ==, 2);
        g_assert_cmpfloat(fabs(curv.k1 - 2.0*fmin(a, b)), <=, 1e-14);
        g_assert_cmpfloat(fabs(curv.k2 - 2.0*fmax(a, b)), <=, 1e-14);
        g_assert_cmpfloat(curv.phi1, >=, -G_PI/2);
        g_assert_cmpfloat(curv.phi1, <=, G_PI/2);
        g_assert_cmpfloat(curv.phi2, >=, -G_PI/2);
        g_assert_cmpfloat(curv.phi2, <=, G_PI/2);
        if (a <= b) {
            g_assert_cmpfloat(fmin(fabs(curv.phi2 - alpha + G_PI/2.0),
                                   fabs(curv.phi2 - alpha - G_PI/2.0)),
                              <=, 1e-14);
            gwy_assert_floatval(curv.phi1, alpha, 1e-14);
        }
        else {
            g_assert_cmpfloat(fmin(fabs(curv.phi1 - alpha + G_PI/2.0),
                                   fabs(curv.phi1 - alpha - G_PI/2.0)),
                              <=, 1e-14);
            gwy_assert_floatval(curv.phi2, alpha, 1e-14);
        }
        g_assert_cmpfloat(fabs(curv.xc + p), <=, 1e-14);
        g_assert_cmpfloat(fabs(curv.yc + q), <=, 1e-14);
        g_assert_cmpfloat(fabs(curv.zc), <=, 1e-14);
    }

    g_rand_free(rng);
}

void
test_math_curvature_at_origin_unsloped(void)
{
    GRand *rng = g_rand_new_with_seed(42);

    for (guint i = 0; i < 30; i++) {
        gdouble coeffs[6];
        gwy_clear(coeffs, 6);

        coeffs[0] = g_rand_double_range(rng, -5.0, 5.0);
        coeffs[3] = g_rand_double_range(rng, -5.0, 5.0);
        coeffs[4] = g_rand_double_range(rng, -5.0, 5.0);
        coeffs[5] = g_rand_double_range(rng, -5.0, 5.0);

        GwyCurvatureParams curvc, curvo;
        guint ndimsc = gwy_math_curvature_at_centre(coeffs, &curvc);
        guint ndimso = gwy_math_curvature_at_origin(coeffs, &curvo);

        g_assert_cmpuint(ndimso, ==, ndimsc);
        gwy_assert_floatval(curvo.k1, curvc.k1, 1e-12);
        gwy_assert_floatval(curvo.k2, curvc.k2, 1e-12);
        gwy_assert_floatval(curvo.phi1, curvc.phi1, 1e-12);
        gwy_assert_floatval(curvo.phi2, curvc.phi2, 1e-12);
        g_assert_cmpfloat(curvo.xc, ==, 0.0);
        g_assert_cmpfloat(curvo.yc, ==, 0.0);
        g_assert_cmpfloat(curvo.zc, ==, coeffs[0]);
    }

    g_rand_free(rng);
}

void
test_math_curvature_at_origin_flat(void)
{
    GRand *rng = g_rand_new_with_seed(42);

    for (guint i = 0; i < 30; i++) {
        gdouble coeffs[6];
        gwy_clear(coeffs, 6);

        coeffs[0] = g_rand_double_range(rng, -5.0, 5.0);
        coeffs[1] = g_rand_double_range(rng, -5.0, 5.0);
        coeffs[2] = g_rand_double_range(rng, -5.0, 5.0);

        GwyCurvatureParams curv;
        guint ndims = gwy_math_curvature_at_centre(coeffs, &curv);

        g_assert_cmpuint(ndims, ==, 0);
        g_assert_cmpfloat(curv.k1, ==, 0.0);
        g_assert_cmpfloat(curv.k2, ==, 0.0);
        g_assert_cmpfloat(curv.phi1, ==, 0.0);
        g_assert_cmpfloat(curv.phi2, ==, G_PI/2.0);
        g_assert_cmpfloat(curv.xc, ==, 0.0);
        g_assert_cmpfloat(curv.yc, ==, 0.0);
        g_assert_cmpfloat(curv.zc, ==, coeffs[0]);
    }

    g_rand_free(rng);
}

void
test_math_curvature_at_origin_sloped(void)
{
    GRand *rng = g_rand_new_with_seed(42);

    for (guint i = 0; i < 30; i++) {
        gdouble coeffs[6];
        gwy_clear(coeffs, 6);

        // Add a really small slope; this should invoke the case where slope is
        // non-zero but change the actual curvatures only very slightly (the
        // effect is second-order).
        coeffs[0] = g_rand_double_range(rng, -5.0, 5.0);
        coeffs[1] = g_rand_double_range(rng, -1.0e-4, 1.0e-4);
        coeffs[2] = g_rand_double_range(rng, -1.0e-4, 1.0e-4);
        coeffs[3] = g_rand_double_range(rng, -5.0, 5.0);
        coeffs[4] = g_rand_double_range(rng, -5.0, 5.0);
        coeffs[5] = g_rand_double_range(rng, -5.0, 5.0);

        GwyCurvatureParams curvc, curvo;
        guint ndimsc = gwy_math_curvature_at_centre(coeffs, &curvc);
        guint ndimso = gwy_math_curvature_at_origin(coeffs, &curvo);

        gdouble b2 = coeffs[1]*coeffs[1] + coeffs[2]*coeffs[2],
                eps = MAX(100*b2, 1e-14);
        g_assert_cmpuint(ndimso, ==, ndimsc);
        gwy_assert_floatval(curvo.k1, curvc.k1, eps);
        gwy_assert_floatval(curvo.k2, curvc.k2, eps);
        gwy_assert_floatval(curvo.phi1, curvc.phi1, eps);
        gwy_assert_floatval(curvo.phi2, curvc.phi2, eps);
        g_assert_cmpfloat(curvo.xc, ==, 0.0);
        g_assert_cmpfloat(curvo.yc, ==, 0.0);
        g_assert_cmpfloat(curvo.zc, ==, coeffs[0]);
    }

    g_rand_free(rng);
}

static void
scale(gdouble s, gdouble *x, gdouble *y)
{
    *x *= s;
    *y *= s;
}

static void
rotate(gdouble phi, gdouble *x, gdouble *y)
{
    gdouble cphi = cos(phi), sphi = sin(phi);
    gdouble t = (*x)*cphi - (*y)*sphi;
    *y = (*x)*sphi + (*y)*cphi;
    *x = t;
}

static void
translate(gdouble dx, gdouble dy, gdouble *x, gdouble *y)
{
    *x += dx;
    *y += dy;
}

void
test_math_line_point_distance_before(void)
{
    enum { niter = 300 };
    GRand *rng = g_rand_new_with_seed(42);

    for (guint iter = 0; iter < niter; iter++) {
        gdouble lx0 = -0.5, ly0 = 0.0, lx1 = 0.5, ly1 = 0.0;
        gdouble x = g_rand_double_range(rng, -10.0, -0.5);
        gdouble y = g_rand_double_range(rng, -10.0, 10.0);
        gdouble expected_dist2 = (x + 0.5)*(x + 0.5) + y*y;

        gdouble s = exp(g_rand_double_range(rng, -5.0, 5.0));
        scale(s, &x, &y);
        scale(s, &lx0, &ly0);
        scale(s, &lx1, &ly1);
        expected_dist2 *= s*s;

        gdouble phi = g_rand_double_range(rng, 0.0, 2.0*G_PI);
        rotate(phi, &x, &y);
        rotate(phi, &lx0, &ly0);
        rotate(phi, &lx1, &ly1);

        gdouble dx = g_rand_double_range(rng, -10.0, 10.0);
        gdouble dy = g_rand_double_range(rng, -10.0, 10.0);
        translate(dx, dy, &x, &y);
        translate(dx, dy, &lx0, &ly0);
        translate(dx, dy, &lx1, &ly1);

        guint endpoint = G_MAXUINT;
        gdouble dist2 = gwy_line_point_distance2(lx0, ly0, lx1, ly1, x, y,
                                                 &endpoint);

        g_assert_cmpint(endpoint, ==, 0);
        gwy_assert_floatval(dist2, expected_dist2, 1e-12*expected_dist2);
    }

    g_rand_free(rng);
}

void
test_math_line_point_distance_after(void)
{
    enum { niter = 300 };
    GRand *rng = g_rand_new_with_seed(42);

    for (guint iter = 0; iter < niter; iter++) {
        gdouble lx0 = -0.5, ly0 = 0.0, lx1 = 0.5, ly1 = 0.0;
        gdouble x = g_rand_double_range(rng, 0.5, 10.0);
        gdouble y = g_rand_double_range(rng, -10.0, 10.0);
        gdouble expected_dist2 = (x - 0.5)*(x - 0.5) + y*y;

        gdouble s = exp(g_rand_double_range(rng, -5.0, 5.0));
        scale(s, &x, &y);
        scale(s, &lx0, &ly0);
        scale(s, &lx1, &ly1);
        expected_dist2 *= s*s;

        gdouble phi = g_rand_double_range(rng, 0.0, 2.0*G_PI);
        rotate(phi, &x, &y);
        rotate(phi, &lx0, &ly0);
        rotate(phi, &lx1, &ly1);

        gdouble dx = g_rand_double_range(rng, -10.0, 10.0);
        gdouble dy = g_rand_double_range(rng, -10.0, 10.0);
        translate(dx, dy, &x, &y);
        translate(dx, dy, &lx0, &ly0);
        translate(dx, dy, &lx1, &ly1);

        guint endpoint = G_MAXUINT;
        gdouble dist2 = gwy_line_point_distance2(lx0, ly0, lx1, ly1, x, y,
                                                 &endpoint);

        g_assert_cmpint(endpoint, ==, 1);
        gwy_assert_floatval(dist2, expected_dist2, 1e-12*expected_dist2);
    }

    g_rand_free(rng);
}

void
test_math_line_point_distance_inside(void)
{
    enum { niter = 300 };
    GRand *rng = g_rand_new_with_seed(42);

    for (guint iter = 0; iter < niter; iter++) {
        gdouble lx0 = -0.5, ly0 = 0.0, lx1 = 0.5, ly1 = 0.0;
        gdouble x = g_rand_double_range(rng, -0.5, 0.5);
        gdouble y = g_rand_double_range(rng, -10.0, 10.0);
        gdouble expected_dist2 = y*y;
        guint expected_endpoint = (x > 0.0);
        gboolean test_endpoint = (fabs(x) > 1e-14);

        gdouble s = exp(g_rand_double_range(rng, -5.0, 5.0));
        scale(s, &x, &y);
        scale(s, &lx0, &ly0);
        scale(s, &lx1, &ly1);
        expected_dist2 *= s*s;

        gdouble phi = g_rand_double_range(rng, 0.0, 2.0*G_PI);
        rotate(phi, &x, &y);
        rotate(phi, &lx0, &ly0);
        rotate(phi, &lx1, &ly1);

        gdouble dx = g_rand_double_range(rng, -10.0, 10.0);
        gdouble dy = g_rand_double_range(rng, -10.0, 10.0);
        translate(dx, dy, &x, &y);
        translate(dx, dy, &lx0, &ly0);
        translate(dx, dy, &lx1, &ly1);

        guint endpoint = G_MAXUINT;
        gdouble dist2 = gwy_line_point_distance2(lx0, ly0, lx1, ly1, x, y,
                                                 &endpoint);

        if (test_endpoint) {
            g_assert_cmpint(endpoint, ==, expected_endpoint);
        }
        gwy_assert_floatval(dist2, expected_dist2, 1e-12*expected_dist2);
    }

    g_rand_free(rng);
}

/***************************************************************************
 *
 * Linear algebra
 *
 ***************************************************************************/

static void
linalg_make_vector(gdouble *d,
                   guint n,
                   GRand *rng)
{
    for (guint j = 0; j < n; j++)
        d[j] = g_rand_double_range(rng, -1.0, 1.0)
               * exp(g_rand_double_range(rng, -5.0, 5.0));
}

/* Multiply a vector with a matrix from left. */
static void
linalg_matvec(gdouble *a, const gdouble *m, const gdouble *v, guint n)
{
    for (guint i = 0; i < n; i++) {
        a[i] = 0.0;
        for (guint j = 0; j < n; j++)
            a[i] += m[i*n + j] * v[j];
    }
}

/* Multiply two square matrices. */
static void
linalg_matmul(gdouble *a, const gdouble *d1, const gdouble *d2, guint n)
{
    for (guint i = 0; i < n; i++) {
        for (guint j = 0; j < n; j++) {
            a[i*n + j] = 0.0;
            for (guint k = 0; k < n; k++)
                a[i*n + j] += d1[i*n + k] * d2[k*n + j];
        }
    }
}

/* Note the precision checks are very tolerant as the matrices we generate
 * are not always well-conditioned. */
void
test_math_linalg(void)
{
    guint nmax = 8, niter = 50;
    /* Make it realy reproducible. */
    GRand *rng = g_rand_new_with_seed(42);

    for (guint n = 1; n < nmax; n++) {
        /* Use descriptive names for less cryptic g_assert() messages. */
        gdouble *matrix = g_new(gdouble, n*n);
        gdouble *unity = g_new(gdouble, n*n);
        gdouble *decomp = g_new(gdouble, n*n);
        gdouble *vector = g_new(gdouble, n);
        gdouble *rhs = g_new(gdouble, n);
        gdouble *solution = g_new(gdouble, n);

        for (guint iter = 0; iter < niter; iter++) {
            linalg_make_vector(matrix, n*n, rng);
            linalg_make_vector(vector, n, rng);
            gdouble eps;

            /* Solution */
            linalg_matvec(rhs, matrix, vector, n);
            memcpy(decomp, matrix, n*n*sizeof(gdouble));
            g_assert(gwy_linalg_solve(decomp, rhs, solution, n));
            eps = gwy_powi(10.0, (gint)n - 13);
            for (guint j = 0; j < n; j++) {
                g_assert_cmpfloat(fabs(solution[j] - vector[j]),
                                  <=,
                                  eps * (fabs(solution[j]) + fabs(vector[j])));
            }
            /* Inversion */
            memcpy(unity, matrix, n*n*sizeof(gdouble));
            g_assert(gwy_linalg_invert(unity, decomp, n));
            /* Multiplication with inverted must give unity */
            linalg_matmul(unity, matrix, decomp, n);
            for (guint j = 0; j < n; j++) {
                gwy_assert_floatval(unity[j*n + j], 1.0, eps);
                for (guint i = 0; i < n; i++) {
                    if (i == j)
                        continue;
                    g_assert_cmpfloat(fabs(unity[j*n + i]), <=, eps);
                }
            }
            /* Double inversion must give the original */
            g_assert(gwy_linalg_invert(decomp, unity, n));
            for (guint j = 0; j < n; j++) {
                for (guint i = 0; i < n; i++) {
                    g_assert_cmpfloat(fabs(matrix[j] - unity[j]),
                                      <=,
                                      eps * (fabs(matrix[j]) + fabs(unity[j])));
                }
            }
        }

        g_free(vector);
        g_free(rhs);
        g_free(solution);
        g_free(unity);
        g_free(matrix);
        g_free(decomp);
    }

    g_rand_free(rng);
}

void
test_math_linalg_fail(void)
{
    GRand *rng = g_rand_new_with_seed(42);

    for (guint n = 2; n <= 10; n++) {
        gdouble *matrix = g_new(gdouble, n*n);
        gdouble *rhs = g_new(gdouble, n);
        gdouble *solution = g_new(gdouble, n);
        gdouble *lastrow = matrix + n*(n-1);

        for (guint iter = 0; iter < 5; iter++) {
            linalg_make_vector(matrix, n*n, rng);
            linalg_make_vector(rhs, n, rng);
            // Make the last row identical to some other row.
            // FIXME: It would be nice if arbitrary linear combination failed
            // but it often passes as `just' an ill-conditioned matrix.
            memcpy(lastrow, matrix + n*g_rand_int_range(rng, 0, n-1),
                   n*sizeof(gdouble));
            g_assert(!gwy_linalg_solve(matrix, rhs, solution, n));
        }

        g_free(solution);
        g_free(rhs);
        g_free(matrix);
    }
    g_rand_free(rng);
}

typedef struct {
    gdouble K;
    guint n;
    guint degree;
} PolyData1;

// Fits (virtual) symmetric Kx³ data
static gboolean
poly1(guint i,
      gdouble *fvalues,
      gdouble *value,
      gpointer user_data)
{
    const PolyData1 *polydata = (const PolyData1*)user_data;
    gdouble x = (i/(gdouble)polydata->n - 1.0)*polydata->K;
    gdouble p = 1.0;
    for (guint j = 0; j <= polydata->degree; j++) {
        fvalues[j] = p;
        p *= x;
    }
    *value = gwy_powi(x, 3);

    return TRUE;
}

void
test_math_fit_poly(void)
{
    for (guint degree = 0; degree <= 4; degree++) {
        for (guint ndata = (degree + 2) | 1; ndata < 30; ndata += 2) {
            PolyData1 polydata = { 1.0, ndata/2, degree };
            gdouble coeffs[degree+1], residuum;
            gboolean ok = gwy_linear_fit(poly1, ndata, coeffs, degree+1,
                                         &residuum, &polydata);
            g_assert(ok);

            gdouble eps = 1e-15;
            for (guint i = 0; i <= degree; i += 2)
                g_assert_cmpfloat(fabs(coeffs[i]), <, eps);

            if (degree >= 3) {
                g_assert_cmpfloat(fabs(coeffs[1]), <, eps);
                g_assert_cmpfloat(fabs(coeffs[3] - polydata.K), <, eps);
            }
            else if (degree == 1 || degree == 2) {
                gdouble t = 1.0/(ndata/2);
                t = 3.0/5.0*(1.0 + t*(1.0 - t/3.0));
                g_assert_cmpfloat(fabs(coeffs[1] - t*polydata.K), <, eps);
            }
        }
    }
}

/***************************************************************************
 *
 * Math sorting
 *
 ***************************************************************************/

/* Return %TRUE if @array is ordered. */
static gboolean
test_sort_is_strictly_ordered(const gdouble *array, gsize n)
{
    gsize i;

    for (i = 1; i < n; i++, array++) {
        if (array[0] >= array[1])
            return FALSE;
    }
    return TRUE;
}

/* Return %TRUE if @array is ordered and its items correspond to @orig_array
 * items with permutations given by @index_array. */
static gboolean
test_sort_is_ordered_with_index(const gdouble *array, const guint *index_array,
                                const gdouble *orig_array, gsize n)
{
    gsize i;

    for (i = 0; i < n; i++) {
        if (index_array[i] >= n)
            return FALSE;
        if (array[i] != orig_array[index_array[i]])
            return FALSE;
    }
    return TRUE;
}

void
test_math_sort_double(void)
{
    gsize nmin = 0, nmax = 65536;

    if (g_test_quick())
        nmax = 8192;

    gdouble *array = g_new(gdouble, nmax);
    gdouble *orig_array = g_new(gdouble, nmax);
    guint *index_array = g_new(guint, nmax);
    for (gsize n = nmin; n < nmax; n = 7*n/6 + 1) {
        for (gsize i = 0; i < n; i++)
            orig_array[i] = sin(n/G_SQRT2 + 1.618*i);

        gwy_assign(array, orig_array, n);
        gwy_math_sort(array, NULL, n);
        g_assert(test_sort_is_strictly_ordered(array, n));

        gwy_assign(array, orig_array, n);
        for (gsize i = 0; i < n; i++)
            index_array[i] = i;
        gwy_math_sort(array, index_array, n);
        g_assert(test_sort_is_strictly_ordered(array, n));
        g_assert(test_sort_is_ordered_with_index(array, index_array,
                                                 orig_array, n));
    }
    g_free(index_array);
    g_free(orig_array);
    g_free(array);
}

void
test_math_median(void)
{
    guint nmax = 1000;
    gdouble *data = g_new(gdouble, nmax);
    GRand *rng = g_rand_new_with_seed(42);

    for (guint n = 1; n < nmax; n++) {
        for (guint i = 0; i < n; i++)
            data[i] = i;
        for (guint i = 0; i < n; i++) {
            guint jj1 = g_rand_int_range(rng, 0, n);
            guint jj2 = g_rand_int_range(rng, 0, n);
            GWY_SWAP(gdouble, data[jj1], data[jj2]);
        }
        gdouble med = gwy_math_median(data, n);
        g_assert_cmpfloat(med, ==, (n/2));
    }
    g_rand_free(rng);
    g_free(data);
}

static void
bubble_sort(guint *array, gsize n)
{
    gboolean changed = TRUE;
    while (changed) {
        changed = FALSE;
        for (guint i = 1; i < n; i++) {
            if (array[i] < array[i-1]) {
                GWY_SWAP(guint, array[i], array[i-1]);
                changed = TRUE;
            }
        }
        n--;
    }
}

void
test_math_sort_uint(void)
{
    gsize nmin = 0, nmax = 65536;
    GRand *rng = g_rand_new_with_seed(42);

    if (g_test_quick())
        nmax = 8192;

    guint *array = g_new(guint, nmax);
    guint *orig_array = g_new(guint, nmax);
    for (gsize n = nmin; n < nmax; n = 7*n/6 + 1) {
        for (gsize i = 0; i < n; i++)
            orig_array[i] = g_rand_int(rng);

        gwy_assign(array, orig_array, n);
        gwy_sort_uint(array, n);
        bubble_sort(orig_array, n);
        for (guint i = 0; i < n; i++)
            g_assert_cmpuint(array[i], ==, orig_array[i]);
    }
    g_free(orig_array);
    g_free(array);
    g_rand_free(rng);
}

/***************************************************************************
 *
 * Cholesky
 *
 ***************************************************************************/

#define CHOLESKY_MATRIX_LEN(n) (((n) + 1)*(n)/2)
#define SLi gwy_lower_triangular_matrix_index

/* Square a triangular matrix. */
static void
test_cholesky_matsquare(gdouble *a, const gdouble *d, guint n)
{
    for (guint i = 0; i < n; i++) {
        for (guint j = 0; j <= i; j++) {
            SLi(a, i, j) = SLi(d, i, 0) * SLi(d, j, 0);
            for (guint k = 1; k <= MIN(i, j); k++)
                SLi(a, i, j) += SLi(d, i, k) * SLi(d, j, k);
        }
    }
}

/* Multiply two symmetrical matrices (NOT triangular). */
static void
test_cholesky_matmul(gdouble *a, const gdouble *d1, const gdouble *d2, guint n)
{
    for (guint i = 0; i < n; i++) {
        for (guint j = 0; j <= i; j++) {
            SLi(a, i, j) = 0.0;
            for (guint k = 0; k < n; k++) {
                guint ik = MAX(i, k);
                guint ki = MIN(i, k);
                guint jk = MAX(j, k);
                guint kj = MIN(j, k);
                SLi(a, i, j) += SLi(d1, ik, ki) * SLi(d2, jk, kj);
            }
        }
    }
}

/* Multiply a vector with a symmetrical matrix (NOT triangular) from left. */
static void
test_cholesky_matvec(gdouble *a, const gdouble *m, const gdouble *v, guint n)
{
    gwy_assign(a, v, n);
    gwy_cholesky_multiply(m, a, n);
}

/* Generate the decomposition.  As long as it has positive numbers on the
 * diagonal, the matrix is positive-definite.   Though maybe not numerically.
 */
static void
test_cholesky_make_matrix(gdouble *d,
                          guint n,
                          GRand *rng)
{
    for (guint j = 0; j < n; j++) {
        SLi(d, j, j) = exp(g_rand_double_range(rng, -2.0, 5.0));
        for (guint k = 0; k < j; k++) {
            SLi(d, j, k) = (g_rand_double_range(rng, -1.0, 1.0)
                            + g_rand_double_range(rng, -1.0, 1.0)
                            + g_rand_double_range(rng, -1.0, 1.0))/5.0;
        }
    }
}

static void
test_cholesky_make_vector(gdouble *d,
                          guint n,
                          GRand *rng)
{
    for (guint j = 0; j < n; j++)
        d[j] = g_rand_double_range(rng, -1.0, 1.0)
               * exp(g_rand_double_range(rng, -5.0, 5.0));
}

/* Note the precision checks are very tolerant as the matrices we generate
 * are not always well-conditioned. */
void
test_math_cholesky(void)
{
    enum { nmax = 8, niter = 50 };
    /* Make it realy reproducible. */
    GRand *rng = g_rand_new_with_seed(42);

    for (guint n = 1; n <= nmax; n++) {
        guint matlen = CHOLESKY_MATRIX_LEN(n);
        /* Use descriptive names for less cryptic g_assert() messages. */
        gdouble *matrix = g_new(gdouble, matlen);
        gdouble *decomp = g_new(gdouble, matlen);
        gdouble *inverted = g_new(gdouble, matlen);
        gdouble *unity = g_new(gdouble, matlen);
        gdouble *vector = g_new(gdouble, n);
        gdouble *solution = g_new(gdouble, n);

        for (guint iter = 0; iter < niter; iter++) {
            test_cholesky_make_matrix(decomp, n, rng);
            test_cholesky_make_vector(vector, n, rng);
            gdouble eps;

            /* Decomposition */
            test_cholesky_matsquare(matrix, decomp, n);
            gdouble cond = gwy_cholesky_condition(matrix, n);
            g_assert(isfinite(cond));
            test_cholesky_matvec(solution, matrix, vector, n);
            g_assert(gwy_cholesky_decompose(matrix, n));
            for (guint j = 0; j < matlen; j++) {
                eps = gwy_powi(10.0, (gint)j - 15);
                gwy_assert_floatval(matrix[j], decomp[j],
                                    eps*(fabs(matrix[j]) + fabs(decomp[j])));
            }

            /* Solution */
            eps = gwy_powi(10.0, (gint)n - 12);
            gwy_cholesky_solve(matrix, solution, n);
            for (guint j = 0; j < n; j++) {
                gwy_assert_floatval(solution[j], vector[j],
                                    eps*(fabs(solution[j]) + fabs(vector[j])));
            }

            /* Inversion */
            test_cholesky_matsquare(matrix, decomp, n);
            gwy_assign(inverted, matrix, matlen);
            g_assert(gwy_cholesky_invert(inverted, n));
            /* Multiplication with inverted must give unity */
            test_cholesky_matmul(unity, matrix, inverted, n);
            for (guint j = 0; j < n; j++) {
                eps = gwy_powi(10.0, (gint)n - 12);
                for (guint k = 0; k < j; k++) {
                    gwy_assert_floatval(SLi(unity, j, k), 0.0, eps);
                }
                eps = gwy_powi(10.0, (gint)n - 12);
                gwy_assert_floatval(SLi(unity, j, j), 1.0, eps);
            }
            /* Double inversion must give the original */
            eps = gwy_powi(10.0, (gint)n - 12);
            g_assert(gwy_cholesky_invert(inverted, n));
            for (guint j = 0; j < matlen; j++) {
                gwy_assert_floatval(matrix[j], inverted[j],
                                    eps*(fabs(matrix[j]) + fabs(inverted[j])));
            }
        }

        g_free(vector);
        g_free(solution);
        g_free(matrix);
        g_free(decomp);
        g_free(inverted);
        g_free(unity);
    }

    g_rand_free(rng);
}

void
test_math_triangular_multiply_right(void)
{
    enum { nmax = 8, niter = 50 };
    /* Make it realy reproducible. */
    GRand *rng = g_rand_new_with_seed(42);

    for (guint n = 1; n <= nmax; n++) {
        guint matlen = CHOLESKY_MATRIX_LEN(n);
        /* Use descriptive names for less cryptic g_assert() messages. */
        gdouble *matrix = g_new(gdouble, matlen);
        gdouble *decomp = g_new(gdouble, matlen);
        gdouble *vector = g_new(gdouble, n);
        gdouble *multiplied = g_new(gdouble, n);

        for (guint iter = 0; iter < niter; iter++) {
            /* Square of multiplication with decomposition must give the same
             * as dot product with the matrix. */
            test_cholesky_make_matrix(decomp, n, rng);
            test_cholesky_matsquare(matrix, decomp, n);
            test_cholesky_make_vector(vector, n, rng);
            test_cholesky_matvec(multiplied, matrix, vector, n);
            gdouble sref = 0.0;
            for (guint j = 0; j < n; j++)
                sref += vector[j]*multiplied[j];
            gwy_triangular_multiply_right(vector, decomp, n);
            gdouble s = 0.0;
            for (guint j = 0; j < n; j++)
                s += vector[j]*vector[j];

            gwy_assert_floatval(s, sref, 1e-15*sref);
        }

        g_free(vector);
        g_free(multiplied);
        g_free(matrix);
        g_free(decomp);
    }

    g_rand_free(rng);
}

void
test_math_triangular_multiply_left(void)
{
    enum { nmax = 8, niter = 50 };
    /* Make it realy reproducible. */
    GRand *rng = g_rand_new_with_seed(42);

    for (guint n = 1; n <= nmax; n++) {
        guint matlen = CHOLESKY_MATRIX_LEN(n);
        /* Use descriptive names for less cryptic g_assert() messages. */
        gdouble *matrix = g_new(gdouble, matlen);
        gdouble *decomp = g_new(gdouble, matlen);
        gdouble *vector = g_new(gdouble, n);
        gdouble *reference = g_new(gdouble, n);
        gdouble *multiplied = g_new(gdouble, n);

        for (guint iter = 0; iter < niter; iter++) {
            /* Verify U^T(M^-1)U x = x */
            test_cholesky_make_matrix(decomp, n, rng);
            test_cholesky_matsquare(matrix, decomp, n);
            test_cholesky_make_vector(reference, n, rng);
            gwy_assign(vector, reference, n);
            gwy_cholesky_invert(matrix, n);
            gwy_triangular_multiply_left(decomp, vector, n);
            test_cholesky_matvec(multiplied, matrix, vector, n);
            gwy_triangular_multiply_right(multiplied, decomp, n);
            gdouble eps = gwy_powi(10.0, (gint)n - 12);
            for (guint j = 0; j < n; j++) {
                gwy_assert_floatval(multiplied[j], reference[j], eps);
            }
        }

        g_free(vector);
        g_free(reference);
        g_free(multiplied);
        g_free(matrix);
        g_free(decomp);
    }

    g_rand_free(rng);
}

void
test_math_cholesky_dotprod(void)
{
    enum { nmax = 8, niter = 50 };
    /* Make it realy reproducible. */
    GRand *rng = g_rand_new_with_seed(42);

    for (guint n = 1; n <= nmax; n++) {
        guint matlen = CHOLESKY_MATRIX_LEN(n);
        /* Use descriptive names for less cryptic g_assert() messages. */
        gdouble *matrix = g_new(gdouble, matlen);
        gdouble *vectora = g_new(gdouble, n);
        gdouble *vectorb = g_new(gdouble, n);
        gdouble *multiplieda = g_new(gdouble, n);
        gdouble *multipliedb = g_new(gdouble, n);

        for (guint iter = 0; iter < niter; iter++) {
            test_cholesky_make_matrix(matrix, n, rng);
            test_cholesky_make_vector(vectora, n, rng);
            test_cholesky_make_vector(vectorb, n, rng);
            gwy_assign(multiplieda, vectora, n);
            gwy_assign(multipliedb, vectorb, n);
            gwy_cholesky_multiply(matrix, multiplieda, n);
            gwy_cholesky_multiply(matrix, multipliedb, n);
            gdouble dotproda = 0.0, dotprodb = 0.0;
            for (guint i = 0; i < n; i++) {
                dotproda += vectora[i]*multipliedb[i];
                dotprodb += vectorb[i]*multiplieda[i];
            }
            gdouble dotprod = gwy_cholesky_dotprod(matrix, vectora, vectorb, n);
            gdouble eps = gwy_powi(10.0, (gint)n - 15)*(fabs(dotproda)
                                                        + fabs(dotprodb));
            gwy_assert_floatval(dotprod, dotproda, eps);
            gwy_assert_floatval(dotprod, dotprodb, eps);
        }

        g_free(vectorb);
        g_free(vectora);
        g_free(multipliedb);
        g_free(multiplieda);
        g_free(matrix);
    }

    g_rand_free(rng);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
