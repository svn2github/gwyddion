/*
 *  $Id$
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

#include "libgwy/macros.h"
#include "libgwy/libgwy-aliases.h"

#define __GWY_MACROS_C__
#include "libgwy/libgwy-aliases.c"

/**
 * SECTION: macros
 * @title: macros
 * @short_description: Miscellaneous utility macros
 **/

/**
 * GWY_SWAP:
 * @t: C type.
 * @x: Variable of type @t to swap with @x.
 * @y: Variable of type @t to swap with @y.
 *
 * Swaps the values of two variables.
 *
 * More precisely, @x and @y must be usable as both lhs and rhs expressions of
 * type @t.
 *
 * This macro may evaluate its arguments several times.
 * This macro is usable as a single statement.
 */

/**
 * GWY_FREE:
 * @ptr: Pointer (must be an l-value).
 *
 * Frees memory if it is allocated.
 *
 * This is an idempotent wrapper of g_free(): if @ptr is not %NULL
 * g_free() is called on it and @ptr is set to %NULL.
 *
 * This macro may evaluate its arguments several times.
 * This macro is usable as a single statement.
 **/

/**
 * GWY_OBJECT_UNREF:
 * @obj: Pointer to #GObject or %NULL (must be an l-value).
 *
 * Unreferences an object if it exists.
 *
 * This is an idempotent wrapper of g_object_unref(): if @obj is not %NULL
 * g_object_unref() is called on it and @obj is set to %NULL.
 *
 * If the object reference count is greater than one ensure it is referenced
 * elsewhere, otherwise it leaks memory.
 *
 * This macro may evaluate its arguments several times.
 * This macro is usable as a single statement.
 **/

/**
 * GWY_SIGNAL_HANDLER_DISCONNECT:
 * @obj: Pointer to #GObject or %NULL.
 * @hid: Id of a signal handler connected to @obj, or 0 (must be an l-value).
 *
 * Disconnects a signal handler if it exists.
 *
 * This is an idempotent wrapper of g_signal_handler_disconnect(): if @hid is
 * nonzero and @obj is not %NULL, the signal handler identified by
 * @hid is disconnected and @hid is set to 0.
 *
 * This macro may evaluate its arguments several times.
 * This macro is usable as a single statement.
 **/

/**
 * gwy_strequal:
 * @a: String.
 * @b: Another string.
 *
 * Expands to %TRUE if strings are equal, to %FALSE otherwise.
 *
 * This is a shorthand for strcmp() which does not require mentally inverting
 * the result to test two strings for euality.
 **/

/**
 * gwy_memclear:
 * @array: Pointer to an array of values to clear.
 *         This argument may be evaluated several times.
 * @n: Number of items to clear.
 *
 * Fills memory block representing an array with zeroes.
 *
 * This is a shorthand for memset(), with the number of bytes to fill
 * calculated from the type of the pointer.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
