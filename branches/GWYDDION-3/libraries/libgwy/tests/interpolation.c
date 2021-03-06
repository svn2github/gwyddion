/*
 *  $Id$
 *  Copyright (C) 2009,2012-2013 David Nečas (Yeti).
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

static void
interpolation_constant_one(GwyInterpolation interpolation,
                           gdouble value)
{
    guint support_len = gwy_interpolation_get_support_size(interpolation);
    g_assert_cmpint(support_len, >, 0);

    gdouble data[support_len];
    for (guint i = 0; i < support_len; i++)
        data[i] = value;

    if (!gwy_interpolation_has_interpolating_basis(interpolation))
        gwy_interpolation_resolve_coeffs_1d(data, support_len, interpolation);

    for (gdouble x = 0.0; x < 1.0; x += 0.0618) {
        gdouble ivalue = gwy_interpolate_1d(x, data, interpolation);
        g_assert_cmpfloat(fabs(ivalue - value),
                          <=,
                          1e-15 * (fabs(ivalue) + fabs(value)));
    }
}

static void
interpolation_linear_one(GwyInterpolation interpolation,
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
interpolation_linear_long_one(GwyInterpolation interpolation,
                              gdouble a, gdouble b)
{
    guint support_len = gwy_interpolation_get_support_size(interpolation);
    g_assert_cmpint(support_len, >, 0);
    /* This is just an implementation assertion, not consistency check. */
    g_assert_cmpint(support_len % 2, ==, 0);
    // Make the linear segment long enough even for non-interpolating
    // basis functions.
    guint len = 50*support_len;

    gdouble *data = g_new(gdouble, len);
    for (guint i = 0; i < len; i++) {
        gdouble origin = len/2 - support_len/2 - 1.0;
        data[i] = (i - origin)*(b - a) + a;
    }

    gwy_interpolation_resolve_coeffs_1d(data, len, interpolation);

    for (gdouble x = 0.0; x < 1.0; x += 0.0618) {
        gdouble value = x*b + (1.0 - x)*a;
        gdouble ivalue = gwy_interpolate_1d(x, data + len/2 - support_len,
                                            interpolation);
        g_assert_cmpfloat(fabs(ivalue - value),
                          <=,
                          1e-14 * (fabs(ivalue) + fabs(value)));
    }

    g_free(data);
}

void
test_interpolation_constant(void)
{
    GwyInterpolation first_const = GWY_INTERPOLATION_ROUND;
    GwyInterpolation last = GWY_INTERPOLATION_BSPLINE6;

    for (GwyInterpolation interp = first_const; interp <= last; interp++) {
        interpolation_constant_one(interp, 1.0);
        interpolation_constant_one(interp, 0.0);
        interpolation_constant_one(interp, -1.0);
    }
}

void
test_interpolation_linear_interpolating(void)
{
    // Interpolating basis.
    GwyInterpolation reproducing_linear[] = {
        GWY_INTERPOLATION_LINEAR,
        GWY_INTERPOLATION_KEYS,
        GWY_INTERPOLATION_SCHAUM,
    };
    for (guint i = 0; i < G_N_ELEMENTS(reproducing_linear); i++) {
        interpolation_linear_one(reproducing_linear[i], 1.0, 2.0);
        interpolation_linear_one(reproducing_linear[i], 1.0, -1.0);
    }
}

void
test_interpolation_linear_non_interpolating(void)
{
    // Non-interpolating basis.
    GwyInterpolation reproducing_linear[] = {
        GWY_INTERPOLATION_BSPLINE4,
        GWY_INTERPOLATION_OMOMS4,
        GWY_INTERPOLATION_BSPLINE6,
    };
    for (guint i = 0; i < G_N_ELEMENTS(reproducing_linear); i++) {
        interpolation_linear_long_one(reproducing_linear[i], 1.0, 2.0);
        interpolation_linear_long_one(reproducing_linear[i], 1.0, -1.0);
    }
}

static void
interpolation_resample_linear_one(GwyInterpolation interpolation,
                                  gdouble a, gdouble b,
                                  guint len, guint newlen)
{
    guint support_len = gwy_interpolation_get_support_size(interpolation);
    g_assert_cmpint(support_len, >, 0);
    gboolean hinterp = gwy_interpolation_has_interpolating_basis(interpolation);
    gdouble edge = hinterp ? 0.5*(support_len - 1.0) : 8.0*(support_len - 1.0);

    // We could not compare anything at all.
    if (0.5*len < edge)
        return;

    gdouble *data = g_new(gdouble, len);
    for (guint i = 0; i < len;i++) {
        gdouble x = (i + 0.5)/len;
        data[i] = a*(1.0 - x) + b*x;
    }

    gdouble *newdata = g_new(gdouble, newlen);
    gwy_interpolation_resample_block_1d(data, len, newdata, newlen,
                                        interpolation, TRUE);

    for (guint i = 0; i < len; i++) {
        gdouble x = (i + 0.5)/len;
        data[i] = a*(1.0 - x) + b*x;
    }

    for (guint i = 0; i < newlen; i++) {
        gdouble x = (i + 0.5)/newlen;
        // We cannot check edge values because they depend on the specific
        // interpolation type.  For non-interpolating bases edge needs to be
        // taken much larger.
        gdouble iorig = x*len;
        if (iorig > 0.5*len)
            iorig = len - iorig;
        if (iorig < edge)
            continue;

        gdouble expected = a*(1.0 - x) + b*x;
        //g_printerr("[%u] %.16f %.16f :: %g\n", i, expected, newdata[i], iorig);
        gwy_assert_floatval(newdata[i], expected, 1e-13);
    }

    g_free(newdata);
    g_free(data);
}

void
test_interpolation_resample_1d_linear(void)
{
    GwyInterpolation reproducing_linear[] = {
        GWY_INTERPOLATION_LINEAR,
        GWY_INTERPOLATION_KEYS,
        GWY_INTERPOLATION_SCHAUM,
        GWY_INTERPOLATION_BSPLINE4,
        GWY_INTERPOLATION_OMOMS4,
        GWY_INTERPOLATION_BSPLINE6,
    };

    enum { niter = 1000 };
    GRand *rng = g_rand_new_with_seed(42);

    for (guint iter = 0; iter < niter; iter++) {
        guint i = g_rand_int_range(rng, 0, G_N_ELEMENTS(reproducing_linear));
        GwyInterpolation interpolation = reproducing_linear[i];
        // Generate also small initial sizes for non-interpolating bases.
        // The test will just pass without comparing any values at all.
        guint size = g_rand_int_range(rng, 2, 400);
        guint newsize = g_rand_int_range(rng, 2, 400);
        interpolation_resample_linear_one(interpolation, -1.0, 1.0,
                                          size, newsize);
    }

    g_rand_free(rng);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
