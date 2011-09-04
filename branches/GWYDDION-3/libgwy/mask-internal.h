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

/*< private_header >*/

#ifndef __LIBGWY_MASK_INTERNAL_H__
#define __LIBGWY_MASK_INTERNAL_H__

G_BEGIN_DECLS

/* Note we use run-time conditions for endianess-branching even though it is
 * known at compile time.  This is to get the big-endian branch at least
 * syntax-checked.  A good optimizing compiler then eliminates the unused
 * branch entirely so we do not need to care.  (Verified that GCC 4.4 really
 * generates identical code with run-time and compile-time branches.) */
#if (G_BYTE_ORDER != G_LITTLE_ENDIAN && G_BYTE_ORDER != G_BIG_ENDIAN)
#error Byte order used on this system is not supported.
#endif

// Ensure that ambiguous cases are rounded up when transforming masks by not
// comparing to exactly 1/2 but a slightly smaller number.
#define MASK_ROUND_THRESHOLD 0.4999999

#define ALL_SET ((guint32)0xffffffffu)
#define ALL_CLEAR ((guint32)0x00000000u)

/* SHR moves the bits right in the mask field, which means towards the higher
 * bits on little-endian and towards the lower bits on big endian. */
#if (G_BYTE_ORDER == G_LITTLE_ENDIAN)
#define FIRST_BIT ((guint32)0x1u)
#define SHR <<
#define SHL >>
#endif

#if (G_BYTE_ORDER == G_BIG_ENDIAN)
#define FIRST_BIT ((guint32)0x80000000u)
#define SHR >>
#define SHL <<
#endif

/* Make a 32bit bit mask with nbits set, starting from bit firstbit.  The
 * lowest bit is 0, the highest 0x1f for little endian and the reverse for
 * big endian. */
#define NTH_BIT(n) (FIRST_BIT SHR (n))
#define MAKE_MASK(firstbit, nbits) \
    (nbits ? ((ALL_SET SHL (0x20 - (nbits))) SHR (firstbit)) : 0u)

/* Revert the bit order. The first part reorders bits within bytes, then we
 * just use GUINT32_SWAP_LE_BE() to reorder the bytes. */
static inline guint32
swap_bits_32(guint32 v)
{
    v = ((v >> 1) & 0x55555555u) | ((v & 0x55555555u) << 1);
    v = ((v >> 2) & 0x33333333u) | ((v & 0x33333333u) << 2);
    v = ((v >> 4) & 0x0f0f0f0fu) | ((v & 0x0f0f0f0fu) << 4);
    return GUINT32_SWAP_LE_BE(v);
}

static inline void
swap_bits_uint32_array(guint32 *data,
                       gsize n)
{
    while (n--) {
        *data = swap_bits_32(*data);
        data++;
    }
}

static inline guint
stride_for_width(guint width)
{
    // The return value is measured in sizeof(guint32), i.e. doublewords.
    // The row alignment is to start each row on 8byte boudnary.
    guint stride = (width + 0x1f) >> 5;
    return stride + (stride & 1);
}

// GCC has a built-in __builtin_popcount().  However it is either 3x faster
// (if something like -march=amdfam10 is given) or 3x slower.  It would be nice
// to use it if it's actually faster.  See benchmarks/bit-count.c for some
// benchmarks.
static inline guint
count_set_bits(guint32 x)
{
    static const guint16 table[256] = {
        0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
        1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
        1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
        1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
        3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
        4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8,
    };
    guint count = 0;
    count += table[x & 0xff];
    x >>= 8;
    count += table[x & 0xff];
    x >>= 8;
    count += table[x & 0xff];
    x >>= 8;
    count += table[x & 0xff];
    return count;
}

static inline guint
count_row_single(const guint32 *row,
                 guint32 m,
                 gboolean value)
{
    return count_set_bits((value ? *row : ~*row) & m);
}

static inline guint
count_row(const guint32 *row,
          guint width,
          guint off, guint end,
          guint32 m0, guint32 m1,
          gboolean value)
{
    guint j = width;
    guint count = count_set_bits(*row & m0);
    j -= 0x20 - off, row++;
    while (j >= 0x20) {
        count += count_set_bits(*row);
        j -= 0x20, row++;
    }
    if (end)
        count += count_set_bits(*row & m1);
    return value ? count : width - count;
}

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

