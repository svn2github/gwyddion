/*
 *  $Id$
 *  Copyright (C) 2009,2012 David Nečas (Yeti).
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
 * Interpolation
 *
 ***************************************************************************/

void
interpolation_constant_one(GwyInterpolationType interpolation,
                           gdouble value)
{
    guint support_len = gwy_interpolation_get_support_size(interpolation);
    g_assert_cmpint(support_len, >, 0);

    gdouble data[support_len];
    for (guint i = 0; i < support_len; i++)
        data[i] = value;

    if (!gwy_interpolation_has_interpolating_basis(interpolation))
        gwy_interpolation_resolve_coeffs_1d(support_len, data, interpolation);

    for (gdouble x = 0.0; x < 1.0; x += 0.0618) {
        gdouble ivalue = gwy_interpolate_1d(x, data, interpolation);
        g_assert_cmpfloat(fabs(ivalue - value),
                          <=,
                          1e-15 * (fabs(ivalue) + fabs(value)));
    }
}

static void
interpolation_linear_one(GwyInterpolationType interpolation,
                         gdouble a, gdouble b)
{
    guint support_len = gwy_interpolation_get_support_size(interpolation);
    g_assert_cmpint(support_len, >, 0);
    /* This is just an implementation assertion, not consistency check. */
    g_assert_cmpint(support_len % 2, ==, 0);

    gdouble data[support_len];
    for (guint i = 0; i < support_len; i++) {
        gdouble origin = support_len - support_len/2 - 1.0;
        data[i] = (i - origin)*(b - a) + a;
    }

    for (gdouble x = 0.0; x < 1.0; x += 0.0618) {
        gdouble value = x*b + (1.0 - x)*a;
        gdouble ivalue = gwy_interpolate_1d(x, data, interpolation);
        g_assert_cmpfloat(fabs(ivalue - value),
                          <=,
                          1e-14 * (fabs(ivalue) + fabs(value)));
    }
}

static void
interpolation_linear_long_one(GwyInterpolationType interpolation,
                              gdouble a, gdouble b)
{
    guint support_len = gwy_interpolation_get_support_size(interpolation);
    g_assert_cmpint(support_len, >, 0);
    /* This is just an implementation assertion, not consistency check. */
    g_assert_cmpint(support_len % 2, ==, 0);
    // Make the linear segment long enough even for non-interpolating
    // basis functions.
    guint len = 50*support_len;

    gdouble data[len];
    for (guint i = 0; i < len; i++) {
        gdouble origin = len/2 - support_len/2 - 1.0;
        data[i] = (i - origin)*(b - a) + a;
    }

    gwy_interpolation_resolve_coeffs_1d(len, data, interpolation);

    for (gdouble x = 0.0; x < 1.0; x += 0.0618) {
        gdouble value = x*b + (1.0 - x)*a;
        gdouble ivalue = gwy_interpolate_1d(x, data + len/2 - support_len,
                                            interpolation);
        g_assert_cmpfloat(fabs(ivalue - value),
                          <=,
                          1e-14 * (fabs(ivalue) + fabs(value)));
    }
}

void
test_interpolation_constant(void)
{
    GwyInterpolationType first_const = GWY_INTERPOLATION_ROUND;
    GwyInterpolationType last = GWY_INTERPOLATION_BSPLINE6;

    for (GwyInterpolationType interp = first_const; interp <= last; interp++) {
        interpolation_constant_one(interp, 1.0);
        interpolation_constant_one(interp, 0.0);
        interpolation_constant_one(interp, -1.0);
    }
}

void
test_interpolation_linear_interpolating(void)
{
    // Interpolating basis.
    GwyInterpolationType reproducing_linear1[] = {
        GWY_INTERPOLATION_LINEAR,
        GWY_INTERPOLATION_KEYS,
        GWY_INTERPOLATION_SCHAUM,
    };
    for (guint i = 0; i < G_N_ELEMENTS(reproducing_linear1); i++) {
        interpolation_linear_one(reproducing_linear1[i], 1.0, 2.0);
        interpolation_linear_one(reproducing_linear1[i], 1.0, -1.0);
    }
}

void
test_interpolation_linear_non_interpolating(void)
{
    // Non-interpolating basis.
    GwyInterpolationType reproducing_linear2[] = {
        GWY_INTERPOLATION_BSPLINE4,
        GWY_INTERPOLATION_OMOMS4,
        GWY_INTERPOLATION_BSPLINE6,
    };
    for (guint i = 0; i < G_N_ELEMENTS(reproducing_linear2); i++) {
        interpolation_linear_long_one(reproducing_linear2[i], 1.0, 2.0);
        interpolation_linear_long_one(reproducing_linear2[i], 1.0, -1.0);
    }
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
