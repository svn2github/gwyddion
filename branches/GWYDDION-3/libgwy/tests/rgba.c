/*
 *  $Id$
 *  Copyright (C) 2010 David Nečas (Yeti).
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
 * RGBA
 *
 ***************************************************************************/

// This is not really necessary as RGBA is used as a testing boxed type
// elsewhere.  But it does not harm.
void
test_rgba_boxed(void)
{
    GwyRGBA rgba = { 0.8, 0.5, 0.25, 1.0 };
    GwyRGBA *copy = serialize_boxed_and_back(&rgba, GWY_TYPE_RGBA);
    g_assert_cmpfloat(rgba.r, ==, copy->r);
    g_assert_cmpfloat(rgba.g, ==, copy->g);
    g_assert_cmpfloat(rgba.b, ==, copy->b);
    g_assert_cmpfloat(rgba.a, ==, copy->a);
    g_boxed_free(GWY_TYPE_RGBA, copy);
}

static void
check_rgba_interpolation(GwyRGBA c0,
                         GwyRGBA c1,
                         gdouble x,
                         GwyRGBA expected_result)
{
    GwyRGBA result;

    gwy_rgba_interpolate(&c0, &c1, x, &result);
    g_assert_cmpfloat(fabs(result.a - expected_result.a), <=, 1e-15);
    // Compare premultiplied values.  This means if alpha == 0 anything goes.
    g_assert_cmpfloat(fabs(result.a*result.r
                           - expected_result.a*expected_result.r), <=, 1e-15);
    g_assert_cmpfloat(fabs(result.a*result.g
                           - expected_result.a*expected_result.g), <=, 1e-15);
    g_assert_cmpfloat(fabs(result.a*result.b
                           - expected_result.a*expected_result.b), <=, 1e-15);
}

void
test_rgba_interpolate(void)
{
    // Opaque
    check_rgba_interpolation((GwyRGBA){0.0, 0.5, 1.0, 1.0},
                             (GwyRGBA){1.0, 0.5, 0.0, 1.0},
                             0.0,
                             (GwyRGBA){0.0, 0.5, 1.0, 1.0});
    check_rgba_interpolation((GwyRGBA){0.0, 0.5, 1.0, 1.0},
                             (GwyRGBA){1.0, 0.5, 0.0, 1.0},
                             1.0,
                             (GwyRGBA){1.0, 0.5, 0.0, 1.0});
    check_rgba_interpolation((GwyRGBA){0.0, 0.5, 1.0, 1.0},
                             (GwyRGBA){1.0, 0.5, 0.0, 1.0},
                             0.4,
                             (GwyRGBA){0.4, 0.5, 0.6, 1.0});

    // Equal-alpha
    check_rgba_interpolation((GwyRGBA){0.0, 0.5, 1.0, 0.5},
                             (GwyRGBA){1.0, 0.5, 0.0, 0.5},
                             0.0,
                             (GwyRGBA){0.0, 0.5, 1.0, 0.5});
    check_rgba_interpolation((GwyRGBA){0.0, 0.5, 1.0, 0.5},
                             (GwyRGBA){1.0, 0.5, 0.0, 0.5},
                             1.0,
                             (GwyRGBA){1.0, 0.5, 0.0, 0.5});
    check_rgba_interpolation((GwyRGBA){0.0, 0.5, 1.0, 0.5},
                             (GwyRGBA){1.0, 0.5, 0.0, 0.5},
                             0.4,
                             (GwyRGBA){0.4, 0.5, 0.6, 0.5});

    // Both transparent
    check_rgba_interpolation((GwyRGBA){0.0, 0.5, 1.0, 0.0},
                             (GwyRGBA){1.0, 0.5, 0.0, 0.0},
                             0.0,
                             (GwyRGBA){0.0, 0.5, 1.0, 0.0});
    check_rgba_interpolation((GwyRGBA){0.0, 0.5, 1.0, 0.0},
                             (GwyRGBA){1.0, 0.5, 0.0, 0.0},
                             1.0,
                             (GwyRGBA){1.0, 0.5, 0.0, 0.0});
    check_rgba_interpolation((GwyRGBA){0.0, 0.5, 1.0, 0.0},
                             (GwyRGBA){1.0, 0.5, 0.0, 0.0},
                             0.4,
                             (GwyRGBA){0.4, 0.5, 0.6, 0.0});

    // First transparent, second opaque
    check_rgba_interpolation((GwyRGBA){0.0, 0.5, 1.0, 0.0},
                             (GwyRGBA){1.0, 0.5, 0.0, 1.0},
                             0.0,
                             (GwyRGBA){0.0, 0.5, 1.0, 0.0});
    check_rgba_interpolation((GwyRGBA){0.0, 0.5, 1.0, 0.0},
                             (GwyRGBA){1.0, 0.5, 0.0, 1.0},
                             1.0,
                             (GwyRGBA){1.0, 0.5, 0.0, 1.0});
    check_rgba_interpolation((GwyRGBA){0.0, 0.5, 1.0, 0.0},
                             (GwyRGBA){1.0, 0.5, 0.0, 1.0},
                             0.4,
                             (GwyRGBA){1.0, 0.5, 0.0, 0.4});

    // First opaque, second transparent
    check_rgba_interpolation((GwyRGBA){0.0, 0.5, 1.0, 1.0},
                             (GwyRGBA){1.0, 0.5, 0.0, 0.0},
                             0.0,
                             (GwyRGBA){0.0, 0.5, 1.0, 1.0});
    check_rgba_interpolation((GwyRGBA){0.0, 0.5, 1.0, 1.0},
                             (GwyRGBA){1.0, 0.5, 0.0, 0.0},
                             1.0,
                             (GwyRGBA){1.0, 0.5, 0.0, 0.0});
    check_rgba_interpolation((GwyRGBA){0.0, 0.5, 1.0, 1.0},
                             (GwyRGBA){1.0, 0.5, 0.0, 0.0},
                             0.4,
                             (GwyRGBA){0.0, 0.5, 1.0, 0.6});

    // Arbitrary
    check_rgba_interpolation((GwyRGBA){0.0, 0.5, 1.0, 0.4},
                             (GwyRGBA){1.0, 0.5, 0.0, 0.8},
                             0.0,
                             (GwyRGBA){0.0, 0.5, 1.0, 0.4});
    check_rgba_interpolation((GwyRGBA){0.0, 0.5, 1.0, 0.4},
                             (GwyRGBA){1.0, 0.5, 0.0, 0.8},
                             1.0,
                             (GwyRGBA){1.0, 0.5, 0.0, 0.8});
    check_rgba_interpolation((GwyRGBA){0.0, 0.5, 1.0, 0.4},
                             (GwyRGBA){1.0, 0.5, 0.0, 0.8},
                             0.4,
                             (GwyRGBA){4.0/7.0, 0.5, 3.0/7.0, 0.56});
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
