/*
 *  $Id$
 *  Copyright (C) 2011 David Nečas (Yeti).
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
 * Brick part
 *
 ***************************************************************************/

void
test_brick_part_boxed(void)
{
    GwyBrickPart bpart = { 1, 2, 3, 4, 5, 6 };
    GwyBrickPart *copy = serialize_boxed_and_back(&bpart,
                                                  GWY_TYPE_BRICK_PART);
    g_assert_cmpfloat(bpart.col, ==, copy->col);
    g_assert_cmpfloat(bpart.row, ==, copy->row);
    g_assert_cmpfloat(bpart.level, ==, copy->level);
    g_assert_cmpfloat(bpart.width, ==, copy->width);
    g_assert_cmpfloat(bpart.height, ==, copy->height);
    g_assert_cmpfloat(bpart.depth, ==, copy->depth);
    g_boxed_free(GWY_TYPE_BRICK_PART, copy);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
