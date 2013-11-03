/*
 *  $Id$
 *  Copyright (C) 2013 David Nečas (Yeti).
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
 * Brick arithmetic
 *
 ***************************************************************************/

void
test_brick_compatibility_res(void)
{
    GwyBrick *brick1 = gwy_brick_new_sized(2, 3, 4, FALSE);
    GwyBrick *brick2 = gwy_brick_new_sized(2, 2, 4, FALSE);
    GwyBrick *brick3 = gwy_brick_new_sized(3, 2, 4, FALSE);
    GwyBrick *brick4 = gwy_brick_new_sized(3, 2, 1, FALSE);

    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick2,
                                               GWY_BRICK_COMPAT_XRES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick2,
                                               GWY_BRICK_COMPAT_YRES),
                     ==, GWY_BRICK_COMPAT_YRES);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick2,
                                               GWY_BRICK_COMPAT_RES),
                     ==, GWY_BRICK_COMPAT_YRES);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick1,
                                               GWY_BRICK_COMPAT_XRES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick1,
                                               GWY_BRICK_COMPAT_YRES),
                     ==, GWY_BRICK_COMPAT_YRES);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick1,
                                               GWY_BRICK_COMPAT_RES),
                     ==, GWY_BRICK_COMPAT_YRES);

    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick3,
                                               GWY_BRICK_COMPAT_XRES),
                     ==, GWY_BRICK_COMPAT_XRES);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick3,
                                               GWY_BRICK_COMPAT_YRES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick3,
                                               GWY_BRICK_COMPAT_RES),
                     ==, GWY_BRICK_COMPAT_XRES);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick2,
                                               GWY_BRICK_COMPAT_XRES),
                     ==, GWY_BRICK_COMPAT_XRES);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick2,
                                               GWY_BRICK_COMPAT_YRES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick2,
                                               GWY_BRICK_COMPAT_RES),
                     ==, GWY_BRICK_COMPAT_XRES);

    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick3,
                                               GWY_BRICK_COMPAT_XRES),
                     ==, GWY_BRICK_COMPAT_XRES);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick3,
                                               GWY_BRICK_COMPAT_YRES),
                     ==, GWY_BRICK_COMPAT_YRES);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick3,
                                               GWY_BRICK_COMPAT_RES),
                     ==, GWY_BRICK_COMPAT_XRES | GWY_BRICK_COMPAT_YRES);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick1,
                                               GWY_BRICK_COMPAT_XRES),
                     ==, GWY_BRICK_COMPAT_XRES);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick1,
                                               GWY_BRICK_COMPAT_YRES),
                     ==, GWY_BRICK_COMPAT_YRES);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick1,
                                               GWY_BRICK_COMPAT_RES),
                     ==, GWY_BRICK_COMPAT_XRES | GWY_BRICK_COMPAT_YRES);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick3,
                                               GWY_BRICK_COMPAT_ZRES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick1,
                                               GWY_BRICK_COMPAT_ZRES),
                     ==, 0);

    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick4,
                                               GWY_BRICK_COMPAT_ZRES),
                     ==, GWY_BRICK_COMPAT_ZRES);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick4, brick3,
                                               GWY_BRICK_COMPAT_ZRES),
                     ==, GWY_BRICK_COMPAT_ZRES);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick4,
                                               GWY_BRICK_COMPAT_RES),
                     ==, GWY_BRICK_COMPAT_ZRES);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick4, brick3,
                                               GWY_BRICK_COMPAT_RES),
                     ==, GWY_BRICK_COMPAT_ZRES);

    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick2,
                                               GWY_BRICK_COMPAT_DX),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick2,
                                               GWY_BRICK_COMPAT_DY),
                     ==, GWY_BRICK_COMPAT_DY);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick2,
                                               GWY_BRICK_COMPAT_DXDY),
                     ==, GWY_BRICK_COMPAT_DY);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick1,
                                               GWY_BRICK_COMPAT_DX),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick1,
                                               GWY_BRICK_COMPAT_DY),
                     ==, GWY_BRICK_COMPAT_DY);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick1,
                                               GWY_BRICK_COMPAT_DXDY),
                     ==, GWY_BRICK_COMPAT_DY);

    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick3,
                                               GWY_BRICK_COMPAT_DX),
                     ==, GWY_BRICK_COMPAT_DX);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick3,
                                               GWY_BRICK_COMPAT_DY),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick3,
                                               GWY_BRICK_COMPAT_DXDY),
                     ==, GWY_BRICK_COMPAT_DX);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick2,
                                               GWY_BRICK_COMPAT_DX),
                     ==, GWY_BRICK_COMPAT_DX);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick2,
                                               GWY_BRICK_COMPAT_DY),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick2,
                                               GWY_BRICK_COMPAT_DXDY),
                     ==, GWY_BRICK_COMPAT_DX);

    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick3,
                                               GWY_BRICK_COMPAT_DX),
                     ==, GWY_BRICK_COMPAT_DX);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick3,
                                               GWY_BRICK_COMPAT_DY),
                     ==, GWY_BRICK_COMPAT_DY);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick3,
                                               GWY_BRICK_COMPAT_DXDY),
                     ==, GWY_BRICK_COMPAT_DXDY);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick1,
                                               GWY_BRICK_COMPAT_DX),
                     ==, GWY_BRICK_COMPAT_DX);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick1,
                                               GWY_BRICK_COMPAT_DY),
                     ==, GWY_BRICK_COMPAT_DY);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick1,
                                               GWY_BRICK_COMPAT_DXDY),
                     ==, GWY_BRICK_COMPAT_DXDY);

    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick2,
                                               GWY_BRICK_COMPAT_REAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick1,
                                               GWY_BRICK_COMPAT_REAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick3,
                                               GWY_BRICK_COMPAT_REAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick1,
                                               GWY_BRICK_COMPAT_REAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick3,
                                               GWY_BRICK_COMPAT_REAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick2,
                                               GWY_BRICK_COMPAT_REAL),
                     ==, 0);

    g_object_unref(brick1);
    g_object_unref(brick2);
    g_object_unref(brick3);
    g_object_unref(brick4);
}

void
test_brick_compatibility_real(void)
{
    GwyBrick *brick1 = gwy_brick_new_sized(2, 2, 2, FALSE);
    GwyBrick *brick2 = gwy_brick_new_sized(2, 2, 2, FALSE);
    GwyBrick *brick3 = gwy_brick_new_sized(2, 2, 2, FALSE);
    GwyBrick *brick4 = gwy_brick_new_sized(2, 2, 2, FALSE);

    gwy_brick_set_yreal(brick1, 1.5);
    gwy_brick_set_xreal(brick3, 1.5);
    gwy_brick_set_zreal(brick4, 1.5);

    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick2,
                                               GWY_BRICK_COMPAT_XREAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick2,
                                               GWY_BRICK_COMPAT_YREAL),
                     ==, GWY_BRICK_COMPAT_YREAL);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick2,
                                               GWY_BRICK_COMPAT_REAL),
                     ==, GWY_BRICK_COMPAT_YREAL);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick1,
                                               GWY_BRICK_COMPAT_XREAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick1,
                                               GWY_BRICK_COMPAT_YREAL),
                     ==, GWY_BRICK_COMPAT_YREAL);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick1,
                                               GWY_BRICK_COMPAT_REAL),
                     ==, GWY_BRICK_COMPAT_YREAL);

    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick3,
                                               GWY_BRICK_COMPAT_XREAL),
                     ==, GWY_BRICK_COMPAT_XREAL);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick3,
                                               GWY_BRICK_COMPAT_YREAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick3,
                                               GWY_BRICK_COMPAT_REAL),
                     ==, GWY_BRICK_COMPAT_XREAL);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick2,
                                               GWY_BRICK_COMPAT_XREAL),
                     ==, GWY_BRICK_COMPAT_XREAL);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick2,
                                               GWY_BRICK_COMPAT_YREAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick2,
                                               GWY_BRICK_COMPAT_REAL),
                     ==, GWY_BRICK_COMPAT_XREAL);

    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick3,
                                               GWY_BRICK_COMPAT_XREAL),
                     ==, GWY_BRICK_COMPAT_XREAL);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick3,
                                               GWY_BRICK_COMPAT_YREAL),
                     ==, GWY_BRICK_COMPAT_YREAL);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick3,
                                               GWY_BRICK_COMPAT_REAL),
                     ==, GWY_BRICK_COMPAT_XREAL | GWY_BRICK_COMPAT_YREAL);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick1,
                                               GWY_BRICK_COMPAT_XREAL),
                     ==, GWY_BRICK_COMPAT_XREAL);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick1,
                                               GWY_BRICK_COMPAT_YREAL),
                     ==, GWY_BRICK_COMPAT_YREAL);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick1,
                                               GWY_BRICK_COMPAT_REAL),
                     ==, GWY_BRICK_COMPAT_XREAL | GWY_BRICK_COMPAT_YREAL);

    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick2,
                                               GWY_BRICK_COMPAT_DX),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick2,
                                               GWY_BRICK_COMPAT_DY),
                     ==, GWY_BRICK_COMPAT_DY);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick2,
                                               GWY_BRICK_COMPAT_DXDY),
                     ==, GWY_BRICK_COMPAT_DY);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick1,
                                               GWY_BRICK_COMPAT_DX),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick1,
                                               GWY_BRICK_COMPAT_DY),
                     ==, GWY_BRICK_COMPAT_DY);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick1,
                                               GWY_BRICK_COMPAT_DXDY),
                     ==, GWY_BRICK_COMPAT_DY);

    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick3,
                                               GWY_BRICK_COMPAT_DX),
                     ==, GWY_BRICK_COMPAT_DX);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick3,
                                               GWY_BRICK_COMPAT_DY),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick3,
                                               GWY_BRICK_COMPAT_DXDY),
                     ==, GWY_BRICK_COMPAT_DX);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick2,
                                               GWY_BRICK_COMPAT_DX),
                     ==, GWY_BRICK_COMPAT_DX);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick2,
                                               GWY_BRICK_COMPAT_DY),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick2,
                                               GWY_BRICK_COMPAT_DXDY),
                     ==, GWY_BRICK_COMPAT_DX);

    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick3,
                                               GWY_BRICK_COMPAT_DX),
                     ==, GWY_BRICK_COMPAT_DX);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick3,
                                               GWY_BRICK_COMPAT_DY),
                     ==, GWY_BRICK_COMPAT_DY);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick3,
                                               GWY_BRICK_COMPAT_DXDY),
                     ==, GWY_BRICK_COMPAT_DXDY);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick1,
                                               GWY_BRICK_COMPAT_DX),
                     ==, GWY_BRICK_COMPAT_DX);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick1,
                                               GWY_BRICK_COMPAT_DY),
                     ==, GWY_BRICK_COMPAT_DY);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick1,
                                               GWY_BRICK_COMPAT_DXDY),
                     ==, GWY_BRICK_COMPAT_DXDY);

    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick4,
                                               GWY_BRICK_COMPAT_DZ),
                     ==, GWY_BRICK_COMPAT_DZ);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick4,
                                               GWY_BRICK_COMPAT_DXDY),
                     ==, GWY_BRICK_COMPAT_DY);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick4,
                                               GWY_BRICK_COMPAT_DXDYDZ),
                     ==, GWY_BRICK_COMPAT_DY | GWY_BRICK_COMPAT_DZ);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick4, brick1,
                                               GWY_BRICK_COMPAT_DZ),
                     ==, GWY_BRICK_COMPAT_DZ);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick4, brick1,
                                               GWY_BRICK_COMPAT_DXDY),
                     ==, GWY_BRICK_COMPAT_DY);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick4, brick1,
                                               GWY_BRICK_COMPAT_DXDYDZ),
                     ==, GWY_BRICK_COMPAT_DY | GWY_BRICK_COMPAT_DZ);

    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick4,
                                               GWY_BRICK_COMPAT_DZ),
                     ==, GWY_BRICK_COMPAT_DZ);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick4,
                                               GWY_BRICK_COMPAT_DXDY),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick4,
                                               GWY_BRICK_COMPAT_DXDYDZ),
                     ==, GWY_BRICK_COMPAT_DZ);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick4, brick2,
                                               GWY_BRICK_COMPAT_DZ),
                     ==, GWY_BRICK_COMPAT_DZ);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick4, brick2,
                                               GWY_BRICK_COMPAT_DXDY),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick4, brick2,
                                               GWY_BRICK_COMPAT_DXDYDZ),
                     ==, GWY_BRICK_COMPAT_DZ);

    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick2,
                                               GWY_BRICK_COMPAT_RES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick1,
                                               GWY_BRICK_COMPAT_RES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick3,
                                               GWY_BRICK_COMPAT_RES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick1,
                                               GWY_BRICK_COMPAT_RES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick3,
                                               GWY_BRICK_COMPAT_RES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick2,
                                               GWY_BRICK_COMPAT_RES),
                     ==, 0);

    g_object_unref(brick1);
    g_object_unref(brick2);
    g_object_unref(brick3);
    g_object_unref(brick4);
}

void
test_brick_compatibility_units(void)
{
    enum { N = 5 };
    static const GwyBrickCompatFlags incompat[N] = {
        0,
        GWY_BRICK_COMPAT_X,
        GWY_BRICK_COMPAT_Y,
        GWY_BRICK_COMPAT_Z,
        GWY_BRICK_COMPAT_VALUE,
    };
    GwyBrick *bricks[N];
    for (guint i = 0; i < N; i++)
        bricks[i] = gwy_brick_new();

    gwy_unit_set_from_string(gwy_brick_get_xunit(bricks[1]), "m", NULL);
    gwy_unit_set_from_string(gwy_brick_get_yunit(bricks[2]), "m", NULL);
    gwy_unit_set_from_string(gwy_brick_get_zunit(bricks[3]), "m", NULL);
    gwy_unit_set_from_string(gwy_brick_get_wunit(bricks[4]), "m", NULL);

    for (guint tocheck = 0; tocheck <= GWY_BRICK_COMPAT_ALL; tocheck++) {
        for (guint i = 0; i < N; i++) {
            for (guint j = 0; j < N; j++) {
                GwyBrickCompatFlags expected = ((i == j) ? 0
                                                : incompat[i] | incompat[j]);
                g_assert_cmpuint(gwy_brick_is_incompatible(bricks[i], bricks[j],
                                                           tocheck),
                                 ==, expected & tocheck);
            }
        }
    }

    for (guint i = 0; i < N; i++)
        g_object_unref(bricks[i]);
}

void
test_brick_compatibility_field_res(void)
{
    GwyBrick *brick = gwy_brick_new_sized(2, 3, 4, FALSE);
    GwyField *field1 = gwy_field_new_sized(2, 3, FALSE);
    GwyField *field2 = gwy_field_new_sized(3, 2, FALSE);
    GwyField *field3 = gwy_field_new_sized(3, 3, FALSE);
    GwyField *field4 = gwy_field_new_sized(2, 2, FALSE);

    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field1, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_XRES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field1, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_YRES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field1, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_RES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field2, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_XRES),
                     ==, GWY_FIELD_COMPAT_XRES);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field2, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_YRES),
                     ==, GWY_FIELD_COMPAT_YRES);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field2, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_RES),
                     ==, GWY_FIELD_COMPAT_RES);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field3, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_XRES),
                     ==, GWY_FIELD_COMPAT_XRES);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field3, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_YRES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field3, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_RES),
                     ==, GWY_FIELD_COMPAT_XRES);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field4, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_XRES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field4, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_YRES),
                     ==, GWY_FIELD_COMPAT_YRES);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field4, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_RES),
                     ==, GWY_FIELD_COMPAT_YRES);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field1, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_XREAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field1, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_YREAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field1, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_REAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field2, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_XREAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field2, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_YREAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field2, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_REAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field3, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_XREAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field3, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_YREAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field3, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_REAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field4, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_XREAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field4, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_YREAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field4, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_REAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field1, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_DX),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field1, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_DY),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field1, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_DXDY),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field2, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_DX),
                     ==, GWY_FIELD_COMPAT_DX);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field2, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_DY),
                     ==, GWY_FIELD_COMPAT_DY);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field2, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_DXDY),
                     ==, GWY_FIELD_COMPAT_DXDY);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field3, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_DX),
                     ==, GWY_FIELD_COMPAT_DX);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field3, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_DY),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field3, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_DXDY),
                     ==, GWY_FIELD_COMPAT_DX);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field4, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_DX),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field4, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_DY),
                     ==, GWY_FIELD_COMPAT_DY);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field4, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_DXDY),
                     ==, GWY_FIELD_COMPAT_DY);

    g_object_unref(field1);
    g_object_unref(field2);
    g_object_unref(field3);
    g_object_unref(field4);
    g_object_unref(brick);
}

void
test_brick_compatibility_field_real(void)
{
    GwyBrick *brick = gwy_brick_new_sized(2, 2, 4, FALSE);
    GwyField *field1 = gwy_field_new_sized(2, 2, FALSE);
    GwyField *field2 = gwy_field_new_sized(2, 2, FALSE);
    GwyField *field3 = gwy_field_new_sized(2, 2, FALSE);
    GwyField *field4 = gwy_field_new_sized(2, 2, FALSE);

    gwy_field_set_xreal(field2, 2.0);
    gwy_field_set_yreal(field2, 2.0);
    gwy_field_set_xreal(field3, 2.0);
    gwy_field_set_yreal(field4, 2.0);

    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field1, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_XRES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field1, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_YRES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field1, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_RES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field2, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_XRES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field2, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_YRES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field2, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_RES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field3, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_XRES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field3, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_YRES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field3, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_RES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field4, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_XRES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field4, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_YRES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field4, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_RES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field1, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_XREAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field1, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_YREAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field1, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_REAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field2, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_XREAL),
                     ==, GWY_FIELD_COMPAT_XREAL);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field2, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_YREAL),
                     ==, GWY_FIELD_COMPAT_YREAL);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field2, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_REAL),
                     ==, GWY_FIELD_COMPAT_REAL);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field3, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_XREAL),
                     ==, GWY_FIELD_COMPAT_XREAL);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field3, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_YREAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field3, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_REAL),
                     ==, GWY_FIELD_COMPAT_XREAL);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field4, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_XREAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field4, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_YREAL),
                     ==, GWY_FIELD_COMPAT_YREAL);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field4, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_REAL),
                     ==, GWY_FIELD_COMPAT_YREAL);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field1, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_DX),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field1, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_DY),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field1, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_DXDY),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field2, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_DX),
                     ==, GWY_FIELD_COMPAT_DX);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field2, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_DY),
                     ==, GWY_FIELD_COMPAT_DY);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field2, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_DXDY),
                     ==, GWY_FIELD_COMPAT_DXDY);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field3, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_DX),
                     ==, GWY_FIELD_COMPAT_DX);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field3, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_DY),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field3, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_DXDY),
                     ==, GWY_FIELD_COMPAT_DX);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field4, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_DX),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field4, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_DY),
                     ==, GWY_FIELD_COMPAT_DY);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field4, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_DXDY),
                     ==, GWY_FIELD_COMPAT_DY);

    g_object_unref(field1);
    g_object_unref(field2);
    g_object_unref(field3);
    g_object_unref(field4);
    g_object_unref(brick);
}

void
test_brick_compatibility_field_units(void)
{
    GwyBrick *brick = gwy_brick_new_sized(2, 2, 2, FALSE);
    GwyField *field1 = gwy_field_new_sized(2, 2, FALSE);
    GwyField *field2 = gwy_field_new_sized(2, 2, FALSE);
    GwyField *field3 = gwy_field_new_sized(2, 2, FALSE);
    GwyField *field4 = gwy_field_new_sized(2, 2, FALSE);

    gwy_unit_set_from_string(gwy_brick_get_xunit(brick), "m", NULL);
    gwy_unit_set_from_string(gwy_brick_get_yunit(brick), "N", NULL);
    gwy_unit_set_from_string(gwy_brick_get_zunit(brick), "s", NULL);
    gwy_unit_set_from_string(gwy_brick_get_wunit(brick), "A", NULL);
    gwy_unit_set_from_string(gwy_field_get_xunit(field1), "m", NULL);
    gwy_unit_set_from_string(gwy_field_get_yunit(field1), "N", NULL);
    gwy_unit_set_from_string(gwy_field_get_zunit(field1), "A", NULL);
    gwy_unit_set_from_string(gwy_field_get_xunit(field3), "m", NULL);
    gwy_unit_set_from_string(gwy_field_get_yunit(field3), "N", NULL);
    gwy_unit_set_from_string(gwy_field_get_zunit(field4), "A", NULL);

    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field1, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_LATERAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field1, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_VALUE),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field1, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_UNITS),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field2, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_LATERAL),
                     ==, GWY_FIELD_COMPAT_LATERAL);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field2, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_VALUE),
                     ==, GWY_FIELD_COMPAT_VALUE);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field2, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_UNITS),
                     ==, GWY_FIELD_COMPAT_UNITS);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field3, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_LATERAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field3, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_VALUE),
                     ==, GWY_FIELD_COMPAT_VALUE);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field3, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_UNITS),
                     ==, GWY_FIELD_COMPAT_VALUE);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field4, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_LATERAL),
                     ==, GWY_FIELD_COMPAT_LATERAL);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field4, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_VALUE),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                           (brick, field4, GWY_DIMENSION_X, GWY_DIMENSION_Y,
                            GWY_FIELD_COMPAT_UNITS),
                     ==, GWY_FIELD_COMPAT_LATERAL);

    g_object_unref(field1);
    g_object_unref(field2);
    g_object_unref(field3);
    g_object_unref(field4);
    g_object_unref(brick);
}

void
test_brick_compatibility_line_res(void)
{
    GwyBrick *brick = gwy_brick_new_sized(2, 3, 4, FALSE);
    GwyLine *line1 = gwy_line_new_sized(4, FALSE);
    GwyLine *line2 = gwy_line_new_sized(5, FALSE);

    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                           (brick, line1, GWY_DIMENSION_Z,
                            GWY_LINE_COMPAT_RES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                           (brick, line2, GWY_DIMENSION_Z,
                            GWY_LINE_COMPAT_RES),
                     ==, GWY_LINE_COMPAT_RES);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                           (brick, line1, GWY_DIMENSION_Z,
                            GWY_LINE_COMPAT_REAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                           (brick, line2, GWY_DIMENSION_Z,
                            GWY_LINE_COMPAT_REAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                           (brick, line1, GWY_DIMENSION_Z,
                            GWY_LINE_COMPAT_DX),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                           (brick, line2, GWY_DIMENSION_Z,
                            GWY_LINE_COMPAT_DX),
                     ==, GWY_LINE_COMPAT_DX);

    g_object_unref(line1);
    g_object_unref(line2);
    g_object_unref(brick);
}

void
test_brick_compatibility_line_real(void)
{
    GwyBrick *brick = gwy_brick_new_sized(2, 3, 4, FALSE);
    GwyLine *line1 = gwy_line_new_sized(4, FALSE);
    GwyLine *line2 = gwy_line_new_sized(4, FALSE);

    gwy_line_set_real(line2, 2.0);

    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                           (brick, line1, GWY_DIMENSION_Z,
                            GWY_LINE_COMPAT_RES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                           (brick, line2, GWY_DIMENSION_Z,
                            GWY_LINE_COMPAT_RES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                           (brick, line1, GWY_DIMENSION_Z,
                            GWY_LINE_COMPAT_REAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                           (brick, line2, GWY_DIMENSION_Z,
                            GWY_LINE_COMPAT_REAL),
                     ==, GWY_LINE_COMPAT_REAL);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                           (brick, line1, GWY_DIMENSION_Z,
                            GWY_LINE_COMPAT_DX),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                           (brick, line2, GWY_DIMENSION_Z,
                            GWY_LINE_COMPAT_DX),
                     ==, GWY_LINE_COMPAT_DX);

    g_object_unref(line1);
    g_object_unref(line2);
    g_object_unref(brick);
}

void
test_brick_compatibility_line_units(void)
{
    GwyBrick *brick = gwy_brick_new_sized(2, 3, 4, FALSE);
    GwyLine *line1 = gwy_line_new_sized(4, FALSE);
    GwyLine *line2 = gwy_line_new_sized(4, FALSE);
    GwyLine *line3 = gwy_line_new_sized(4, FALSE);
    GwyLine *line4 = gwy_line_new_sized(4, FALSE);

    gwy_unit_set_from_string(gwy_brick_get_xunit(brick), "m", NULL);
    gwy_unit_set_from_string(gwy_brick_get_yunit(brick), "m", NULL);
    gwy_unit_set_from_string(gwy_brick_get_zunit(brick), "s", NULL);
    gwy_unit_set_from_string(gwy_brick_get_wunit(brick), "A", NULL);
    gwy_unit_set_from_string(gwy_line_get_xunit(line1), "s", NULL);
    gwy_unit_set_from_string(gwy_line_get_yunit(line1), "A", NULL);
    gwy_unit_set_from_string(gwy_line_get_xunit(line3), "s", NULL);
    gwy_unit_set_from_string(gwy_line_get_yunit(line4), "A", NULL);

    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                           (brick, line1, GWY_DIMENSION_Z,
                            GWY_LINE_COMPAT_LATERAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                           (brick, line1, GWY_DIMENSION_Z,
                            GWY_LINE_COMPAT_VALUE),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                           (brick, line1, GWY_DIMENSION_Z,
                            GWY_LINE_COMPAT_UNITS),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                           (brick, line2, GWY_DIMENSION_Z,
                            GWY_LINE_COMPAT_LATERAL),
                     ==, GWY_LINE_COMPAT_LATERAL);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                           (brick, line2, GWY_DIMENSION_Z,
                            GWY_LINE_COMPAT_VALUE),
                     ==, GWY_LINE_COMPAT_VALUE);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                           (brick, line2, GWY_DIMENSION_Z,
                            GWY_LINE_COMPAT_UNITS),
                     ==, GWY_LINE_COMPAT_UNITS);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                           (brick, line3, GWY_DIMENSION_Z,
                            GWY_LINE_COMPAT_LATERAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                           (brick, line3, GWY_DIMENSION_Z,
                            GWY_LINE_COMPAT_VALUE),
                     ==, GWY_LINE_COMPAT_VALUE);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                           (brick, line3, GWY_DIMENSION_Z,
                            GWY_LINE_COMPAT_UNITS),
                     ==, GWY_LINE_COMPAT_VALUE);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                           (brick, line4, GWY_DIMENSION_Z,
                            GWY_LINE_COMPAT_LATERAL),
                     ==, GWY_LINE_COMPAT_LATERAL);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                           (brick, line4, GWY_DIMENSION_Z,
                            GWY_LINE_COMPAT_VALUE),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                           (brick, line4, GWY_DIMENSION_Z,
                            GWY_LINE_COMPAT_UNITS),
                     ==, GWY_LINE_COMPAT_LATERAL);

    g_object_unref(line1);
    g_object_unref(line2);
    g_object_unref(line3);
    g_object_unref(line4);
    g_object_unref(brick);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
