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

#ifdef __LIBGWY_ALIASES_H__
#error Public headers must be included before the aliasing header.
#endif

#ifndef __LIBGWY_MACROS_H__
#define __LIBGWY_MACROS_H__

#define GWY_SWAP(t, x, y) \
    do { t ___gwy_swap; ___gwy_swap = x; x = y; y = ___gwy_swap; } while (0)

#define GWY_FREE(ptr) \
    do { if (ptr) { g_free(ptr); (ptr) = NULL; } } while (0)

#define GWY_STRING_FREE(str) \
    do { if (str) { g_string_free((str), TRUE); (str) = NULL; } } while (0)

#define GWY_SLICE_FREE(type, ptr) \
    do { if (ptr) { g_slice_free(type, ptr); (ptr) = NULL; } } while (0)

#define GWY_OBJECT_UNREF(obj) \
    do { if (obj) { g_object_unref(obj); (obj) = NULL; } } while (0)

#define GWY_SIGNAL_HANDLER_DISCONNECT(obj, hid) \
    do { \
        if (hid && obj) { g_signal_handler_disconnect(obj, hid); (hid) = 0; } \
    } while (0)

#define GWY_MAYBE_SET(pointer, value) \
    do { if (pointer) *(pointer) = (value); } while (0)

#define gwy_memclear(array, n) \
    memset((array), 0, (n)*sizeof((array)[0]))

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
