/*
 *  $Id$
 *  Copyright (C) 2009-2010 David Nečas (Yeti).
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

/**
 * SECTION: macros
 * @title: macros
 * @short_description: Miscellaneous utility macros
 *
 * Several macros are wrappers of resource freeing functions.  They differ from
 * the original functions by setting the resource variable to an
 * unallocated/invalid state (typically %NULL).  This improves readability
 * of code such as #GObjectClass.dispose() methods that can simply do
 * |[
 * GWY_SIGNAL_HANDLER_DISCONNECT(object, handler);
 * GWY_OBJECT_UNREF(object);
 * ]|
 * if they know @object and @handler have either valid values or they are nul.
 * This code can be executed several times without errors from GLib.
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
 * GWY_STRING_FREE:
 * @str: A #GString (must be an l-value).
 *
 * Frees a #GString if it is allocated.
 *
 * This is an idempotent wrapper of g_string_free(): if @str is not %NULL
 * g_string_free() is called on it and @str is set to %NULL.
 *
 * The return value of g_string_free() is not made accessible, use the original
 * if you need that.
 *
 * This macro may evaluate its arguments several times.
 * This macro is usable as a single statement.
 **/

/**
 * GWY_SLICE_FREE:
 * @type: Type of the data to free, typically a structure name.
 * @ptr: Pointer (must be an l-value).
 *
 * Frees GSlice‐allocated memory if it is allocated.
 *
 * This is an idempotent wrapper of g_slice_free(): if @ptr is not %NULL
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
 * GWY_MAYBE_SET:
 * @pointer: Pointer to a value to update, or %NULL.  This argument may be
 *           evaluated multiple times.
 * @value: Value to assign.  This argument is evaluated only once.
 *
 * Sets a value to which a pointer points, but only if the pointer is not
 * %NULL.
 *
 * This macro is a shorthand for the common idiom
 * |[
 * if (pointer)
 *     *pointer = value;
 * ]|
 **/

/**
 * gwy_clear:
 * @array: Pointer to an array of values to clear.
 *         This argument may be evaluated several times.
 * @n: Number of items to clear.
 *
 * Fills memory block representing an array with zeroes.
 *
 * This is a shorthand for memset(), with the number of bytes to fill
 * calculated from the type of the pointer.
 **/

/**
 * gwy_assign:
 * @dest: Destination array.
 *        This argument may be evaluated several times.
 * @source: Source array.
 * @n: Number of items to copy (in @dest units if the array types differ).
 *
 * Copies a block of items.
 *
 * This is a shorthand for memcpy(), with the number of bytes to fill
 * calculated from the type of the @dest pointer.  Consequently, the arrays
 * may not overlap.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
