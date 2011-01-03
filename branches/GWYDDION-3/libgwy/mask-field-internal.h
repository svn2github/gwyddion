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

/*< private_header >*/

#ifndef __LIBGWY_MASK_FIELD_INTERNAL_H__
#define __LIBGWY_MASK_FIELD_INTERNAL_H__

G_BEGIN_DECLS

typedef struct {
    guint row;
    guint col;
    guint len;
} GwyGrainSegment;

typedef struct {
    guint id;
    guint len;
    GwyGrainSegment segments[];
} GwyGrain;

/* Note we use run-time conditions for endianess-branching even though it is
 * known at compile time.  This is to get the big-endian branch at least
 * syntax-checked.  A good optimizing compiler then eliminates the unused
 * branch entirely so we do not need to care.  (Verified that GCC 4.4 really
 * generates identical code with run-time and compile-time branches.) */
#if (G_BYTE_ORDER != G_LITTLE_ENDIAN && G_BYTE_ORDER != G_BIG_ENDIAN)
#error Byte order used on this system is not supported.
#endif

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

static inline guint32
swap_bits_32(guint32 v)
{
    v = ((v >> 1) & 0x55555555u) | ((v & 0x55555555u) << 1);
    v = ((v >> 2) & 0x33333333u) | ((v & 0x33333333u) << 2);
    v = ((v >> 4) & 0x0f0f0f0fu) | ((v & 0x0f0f0f0fu) << 4);
    return GUINT32_SWAP_LE_BE(v);
}

G_GNUC_INTERNAL
gboolean _gwy_mask_field_check_rectangle(const GwyMaskField *field,
                                         const GwyRectangle *rectangle,
                                         guint *col,
                                         guint *row,
                                         guint *width,
                                         guint *height);

G_GNUC_INTERNAL
gboolean _gwy_mask_field_limit_rectangles(const GwyMaskField *src,
                                          const GwyRectangle *srcrectangle,
                                          const GwyMaskField *dest,
                                          guint destcol,
                                          guint destrow,
                                          gboolean transpose,
                                          guint *col,
                                          guint *row,
                                          guint *width,
                                          guint *height);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
