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

// Testing macros is a bit crazy.  We might get *compile-time* errors if things
// go wrong.

void
test_macros_swap(void)
{
    gint a, b, *pa = &a, *pb = &b;

    a = 1;
    b = 2;
    GWY_SWAP(gint, a, b);
    g_assert_cmpint(a, ==, 2);
    g_assert_cmpint(b, ==, 1);

    a = 1;
    b = 2;
    GWY_SWAP(gint, *pa, b);
    g_assert_cmpint(a, ==, 2);
    g_assert_cmpint(b, ==, 1);

    a = 1;
    b = 2;
    GWY_SWAP(gint, a, *pb);
    g_assert_cmpint(a, ==, 2);
    g_assert_cmpint(b, ==, 1);

    a = 1;
    b = 2;
    GWY_SWAP(gint, *pa, *pb);
    g_assert_cmpint(a, ==, 2);
    g_assert_cmpint(b, ==, 1);
}

void
test_macros_order(void)
{
    gint a, b, *pa = &a, *pb = &b;

    a = 1;
    b = 1;
    GWY_ORDER(gint, a, b);
    g_assert_cmpint(a, ==, 1);
    g_assert_cmpint(b, ==, 1);

    a = 1;
    b = 2;
    GWY_ORDER(gint, a, b);
    g_assert_cmpint(a, ==, 1);
    g_assert_cmpint(b, ==, 2);

    a = 2;
    b = 1;
    GWY_ORDER(gint, a, b);
    g_assert_cmpint(a, ==, 1);
    g_assert_cmpint(b, ==, 2);

    a = 1;
    b = 2;
    GWY_ORDER(gint, *pa, b);
    g_assert_cmpint(a, ==, 1);
    g_assert_cmpint(b, ==, 2);

    a = 2;
    b = 1;
    GWY_ORDER(gint, *pa, b);
    g_assert_cmpint(a, ==, 1);
    g_assert_cmpint(b, ==, 2);

    a = 1;
    b = 2;
    GWY_ORDER(gint, a, *pb);
    g_assert_cmpint(a, ==, 1);
    g_assert_cmpint(b, ==, 2);

    a = 2;
    b = 1;
    GWY_ORDER(gint, a, *pb);
    g_assert_cmpint(a, ==, 1);
    g_assert_cmpint(b, ==, 2);

    a = 1;
    b = 2;
    GWY_ORDER(gint, *pa, *pb);
    g_assert_cmpint(a, ==, 1);
    g_assert_cmpint(b, ==, 2);

    a = 2;
    b = 1;
    GWY_ORDER(gint, *pa, *pb);
    g_assert_cmpint(a, ==, 1);
    g_assert_cmpint(b, ==, 2);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

