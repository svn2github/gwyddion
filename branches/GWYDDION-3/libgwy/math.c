/*
 *  @(#) $Id: pack.c 7 2009-01-03 19:16:14Z yeti $
 *  Copyright (C) 2009 David Necas (Yeti).
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

#include "libgwy/math.h"

/**
 * SECTION:math
 * @title: math
 * @short_description: Mathematical functions
 *
 * A number of less standard but useful mathematical functions is provided to
 * ensure they are available on all platforms.
 * 
 * These functions exist in two flavours:
 * <itemizedlist>
 *   <listitem>
 *     Namespace-clean, that is prefixed with <literal>gwy_</literal>.
 *     For instance, gwy_cbrt() or gwy_hypot().  These are defined always.
 *   </listitem>
 *   <listitem>
 *     Bare-named, i.e. with the same name as the C library function, for
 *     instance cbrt() ot hypot().  There are defined only when explicitly
 *     requested with:
 * |[#define GWY_MATH_POLLUTE_NAMESPACE]|
 *     to avoid problems when Gwyddion headers are included indirectly or
 *     in combination with other libraries.
 *   </listitem>
 * </itemizedlist>
 *
 * Both kinds of symbols can be either functions or macros expanding to a
 * function name so it it always possible to take their addresses.  The bare
 * names are in no case exported.  If the system C library provides a specific
 * function, both kinds of symbols are defined so that the system
 * implementation is directly used.
 **/

/**
 * gwy_exp10:
 * @x: Exponent of 10.
 *
 * Calculates the value of 10 raised to given power.
 *
 * Returns: Value of 10 raised to @x.
 **/

/**
 * exp10:
 *
 * System function <literal>exp10</literal> or alias of gwy_exp10().
 **/

/**
 * gwy_exp2:
 * @x: Exponent of 2.
 *
 * Calculates the value of 2 raised to given power.
 *
 * Returns: Value of 2 raised to @x.
 **/

/**
 * exp2:
 *
 * System function <literal>exp2</literal> or alias of gwy_exp2().
 **/

/**
 * gwy_log2:
 * @x: Number.
 *
 * Calculates the base 2 logarithm of a number.
 *
 * Returns: Base 2 logarithm of @x.
 **/

/**
 * log2:
 *
 * System function <literal>log2</literal> or alias of gwy_log2().
 **/

/**
 * gwy_cbrt:
 * @x: Number.
 *
 * Calculates the real cube root of a number.
 *
 * Returns: Cube root of @x.
 **/

/**
 * cbrt:
 *
 * System function <literal>cbrt</literal> or alias of gwy_cbrt().
 **/

/**
 * gwy_hypot:
 * @x: Triangle side.
 * @y: Triangle side.
 *
 * Calculates the length of the hypotenuse of a right-angled triangle.
 *
 * Returns: Length of hypotenuse of a right-angled triangle with sides @x and
 *          @y.
 **/

/**
 * hypot:
 *
 * System function <literal>hypot</literal> or alias of gwy_hypot().
 **/

/**
 * gwy_acosh:
 * @x: Number.
 *
 * Calculates the inverse hyperbolic cosine of a number.
 *
 * Returns: Inverse hyperbolic cosine of @x.
 **/

/**
 * acosh:
 *
 * System function <literal>acosh</literal> or alias of gwy_acosh().
 **/

/**
 * gwy_asinh:
 * @x: Number.
 *
 * Calculates the inverse hyperbolic sine of a number.
 *
 * Returns: Inverse hyperbolic sine of @x.
 **/

/**
 * asinh:
 *
 * System function <literal>asinh</literal> or alias of gwy_asinh().
 **/

/**
 * gwy_atanh:
 * @x: Number.
 *
 * Calculates the inverse hyperbolic tangent of a number.
 *
 * Returns: Inverse hyperbolic tangent of @x.
 **/

/**
 * atanh:
 *
 * System function <literal>atanh</literal> or alias of gwy_atanh().
 **/

/**
 * gwy_powi:
 * @x: Base.
 * @i: Integer exponent.
 *
 * Calculates the integer power of a number.
 *
 * Returns: Value of @x raised to the integer power @i.
 **/

/**
 * powi:
 *
 * GNU C function <literal>__builtin_powi</literal> or alias of gwy_powi().
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
