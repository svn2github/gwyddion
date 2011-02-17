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
 * XY and XYZ
 *
 ***************************************************************************/

void
test_xy_boxed(void)
{
    GwyXY xy = { G_PI, -1 };
    GwyXY *copy = serialize_boxed_and_back(&xy, GWY_TYPE_XY);
    g_assert_cmpfloat(xy.x, ==, copy->x);
    g_assert_cmpfloat(xy.y, ==, copy->y);
    g_boxed_free(GWY_TYPE_XY, copy);
}

void
test_xyz_boxed(void)
{
    GwyXYZ xyz = { G_PI, -1, G_MAXDOUBLE };
    GwyXYZ *copy = serialize_boxed_and_back(&xyz, GWY_TYPE_XYZ);
    g_assert_cmpfloat(xyz.x, ==, copy->x);
    g_assert_cmpfloat(xyz.y, ==, copy->y);
    g_assert_cmpfloat(xyz.z, ==, copy->z);
    g_boxed_free(GWY_TYPE_XYZ, copy);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
