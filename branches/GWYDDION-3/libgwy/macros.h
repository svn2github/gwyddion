/*
 *  $Id$
 *  Copyright (C) 2009-2012 David Nečas (Yeti).
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

#ifndef __LIBGWY_MACROS_H__
#define __LIBGWY_MACROS_H__

#define GWY_SWAP(t, x, y) \
    do { t ___gwy_swap; ___gwy_swap = x; x = y; y = ___gwy_swap; } while (0)

#define GWY_ORDER(t, x, y) \
    do { \
        if (y < x) { \
            t ___gwy_swap; \
            ___gwy_swap = x; \
            x = y; \
            y = ___gwy_swap; \
        } \
    } while (0)

#define GWY_FREE(ptr) \
    do { if (ptr) { g_free(ptr); (ptr) = NULL; } } while (0)

#define GWY_STRING_FREE(str) \
    do { if (str) { g_string_free((str), TRUE); (str) = NULL; } } while (0)

#define GWY_SLICE_FREE(type, ptr) \
    do { if (ptr) { g_slice_free(type, ptr); (ptr) = NULL; } } while (0)

#define GWY_MAYBE_SET(pointer, value) \
    do { if (pointer) *(pointer) = (value); } while (0)

#define gwy_clear(array, n) \
    memset((array), 0, (n)*sizeof((array)[0]))

#define gwy_clear1(var) \
    memset(&(var), 0, sizeof(var))

#define gwy_assign(dest, source, n) \
    memcpy((dest), (source), (n)*sizeof((dest)[0]))

// The -1 array trick causes error if sizes do not match.
// FIXME: Improve using some gccism.
#define gwy_equal(a, b) \
    (!memcmp((a), (b), sizeof((a)[0]) \
                       + (int[1 - 2*(sizeof((a)[0]) != sizeof((b)[0]))]){0}[0]))

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
