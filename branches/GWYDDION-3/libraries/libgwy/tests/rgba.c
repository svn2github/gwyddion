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
    gwy_assert_floatval(result.a, expected_result.a, 1e-15);
    // Compare premultiplied values.  This means if alpha == 0 anything goes.
    gwy_assert_floatval(result.a*result.r, expected_result.a*expected_result.r,
                        1e-15);
    gwy_assert_floatval(result.a*result.g, expected_result.a*expected_result.g,
                        1e-15);
    gwy_assert_floatval(result.a*result.b, expected_result.a*expected_result.b,
                        1e-15);
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

void
test_rgba_preset(void)
{
    guint n = gwy_rgba_n_preset_colors();
    g_assert_cmpuint(n, >, 1);

    for (guint i = 0; i < n; i++) {
        const GwyRGBA *prgba = gwy_rgba_get_preset_color(i);
        GwyRGBA rgba;
        gwy_rgba_preset_color(&rgba, i);
        g_assert_cmpfloat(rgba.r, ==, prgba->r);
        g_assert_cmpfloat(rgba.g, ==, prgba->g);
        g_assert_cmpfloat(rgba.b, ==, prgba->b);
        g_assert_cmpfloat(rgba.a, ==, 1.0);
        g_assert_cmpfloat(prgba->a, ==, 1.0);

        for (guint j = 0; j < i; j++) {
            const GwyRGBA *cmp = gwy_rgba_get_preset_color(j);
            g_assert(!gwy_equal(cmp, prgba));
        }
    }

    GwyRGBA rgba0, rgban;
    gwy_rgba_preset_color(&rgba0, 0);
    g_assert_cmpfloat(rgba0.r, ==, 0.0);
    g_assert_cmpfloat(rgba0.g, ==, 0.0);
    g_assert_cmpfloat(rgba0.b, ==, 0.0);
    gwy_rgba_preset_color(&rgban, n);
    g_assert(gwy_equal(&rgba0, &rgban));
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
