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
 * Rectangle
 *
 ***************************************************************************/

void
test_field_part_boxed(void)
{
    GwyFieldPart fpart = { 1, 2, 3, 4 };
    GwyFieldPart *copy = serialize_boxed_and_back(&fpart,
                                                  GWY_TYPE_FIELD_PART);
    g_assert_cmpfloat(fpart.col, ==, copy->col);
    g_assert_cmpfloat(fpart.row, ==, copy->row);
    g_assert_cmpfloat(fpart.width, ==, copy->width);
    g_assert_cmpfloat(fpart.height, ==, copy->height);
    g_boxed_free(GWY_TYPE_FIELD_PART, copy);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
