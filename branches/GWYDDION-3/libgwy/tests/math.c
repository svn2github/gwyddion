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

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
