/*
 *  $Id$
 *  Copyright (C) 2011 David Necas (Yeti).
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

/* NB: copying is also implemented here as it shares some macros even though it
 * is declared in the main header.  In fact, copying is the only function here
 * at present but logical operations similar to GwyMaskField's should follow. */

#include <string.h>
#include "libgwy/macros.h"
#include "libgwy/math.h"
#include "libgwy/mask-line.h"
//#include "libgwy/mask-line-arithmetic.h"
#include "libgwy/math-internal.h"
#include "libgwy/object-internal.h"
#include "libgwy/mask-line-internal.h"

// Only one item is modified but the mask may need bits cut off from
// both sides (unlike in all other cases).
#define LOGICAL_OP_PART_SINGLE(masked) \
    do { \
        const guint32 m = MAKE_MASK(doff, len); \
        const guint32 *p = sbase; \
        guint32 *q = dbase; \
        guint32 v0 = *p; \
        if (send && send <= soff) { \
            guint32 v1 = *(++p); \
            guint32 vp = (v0 SHL kk) | (v1 SHR k); \
            masked; \
        } \
        else if (doff > soff) { \
            guint32 vp = v0 SHR k; \
            masked; \
        } \
        else { \
            guint32 vp = v0 SHL kk; \
            masked; \
        } \
    } while (0)

// Multiple items are modified but the offsets match so no shifts are
// necessary, just masking at the ends.
#define LOGICAL_OP_PART_ALIGNED(simple, masked) \
    do { \
        const guint32 m0d = MAKE_MASK(doff, 0x20 - doff); \
        const guint32 m1d = MAKE_MASK(0, dend); \
        const guint32 *p = sbase; \
        guint32 *q = dbase; \
        guint j = len; \
        guint32 vp = *p; \
        guint32 m = m0d; \
        masked; \
        j -= 0x20 - doff, p++, q++; \
        while (j >= 0x20) { \
            vp = *p; \
            simple; \
            j -= 0x20, p++, q++; \
        } \
        if (!dend) \
            continue; \
        vp = *p; \
        m = m1d; \
        masked; \
    } while (0)

// The general case, multi-item transfer and different offsets.
#define LOGICAL_OP_PART_GENERAL(simple, masked) \
    do { \
        const guint32 m0d = MAKE_MASK(doff, 0x20 - doff); \
        const guint32 m1d = MAKE_MASK(0, dend); \
        const guint32 *p = sbase; \
        guint32 *q = dbase; \
        guint j = len; \
        guint32 v0 = *p; \
        if (doff > soff) { \
            guint32 vp = v0 SHR k; \
            guint32 m = m0d; \
            masked; \
        } \
        else { \
            guint32 v1 = *(++p); \
            guint32 vp = (v0 SHL kk) | (v1 SHR k); \
            guint32 m = m0d; \
            masked; \
            v0 = v1; \
        } \
        j -= (0x20 - doff), q++; \
        while (j >= 0x20) { \
            guint32 v1 = *(++p); \
            guint32 vp = (v0 SHL kk) | (v1 SHR k); \
            simple; \
            j -= 0x20, q++; \
            v0 = v1; \
        } \
        if (!dend) \
            continue; \
        if (send && dend > send) { \
            guint32 v1 = *(++p); \
            guint32 vp = (v0 SHL kk) | (v1 SHR k); \
            guint32 m = m1d; \
            masked; \
        } \
        else { \
            guint32 vp = (v0 SHL kk); \
            guint32 m = m1d; \
            masked; \
        } \
    } while (0)

#define LOGICAL_OP_PART(simple, masked) \
    if (len <= 0x20 - doff) \
        LOGICAL_OP_PART_SINGLE(masked); \
    else if (doff == soff) \
        LOGICAL_OP_PART_ALIGNED(simple, masked); \
    else \
        LOGICAL_OP_PART_GENERAL(simple, masked) \

static void
copy_part(const GwyMaskLine *src,
          guint pos,
          guint len,
          GwyMaskLine *dest,
          guint destpos)
{
    const guint32 *sbase = src->data + (pos >> 5);
    guint32 *dbase = dest->data + (destpos >> 5);
    const guint soff = pos & 0x1f;
    const guint doff = destpos & 0x1f;
    const guint send = (pos + len) & 0x1f;
    const guint dend = (destpos + len) & 0x1f;
    const guint k = (doff + 0x20 - soff) & 0x1f;
    const guint kk = 0x20 - k;
    LOGICAL_OP_PART(*q = vp, *q = (*q & ~m) | (vp & m));
}

/**
 * gwy_mask_line_copy:
 * @src: Source one-dimensional mask line.
 * @srcpos: Position of the line part start.
 * @srclen: Part length (number of items).
 * @dest: Destination one-dimensional mask line.
 * @destpos: Destination position in @dest.
 *
 * Copies data from one mask line to another.
 *
 * The copied block starts at @pos in @src and its lenght is @len.  It is
 * copied to @dest starting from @destpos.
 *
 * There are no limitations on the indices or dimensions.  Only the part of the
 * block that is corrsponds to data inside @src and @dest is copied.  This can
 * also mean nothing is copied at all.
 *
 * If @src is equal to @dest the areas may <emphasis>not</emphasis> overlap.
 **/
void
gwy_mask_line_copy(const GwyMaskLine *src,
                   guint srcpos,
                   guint srclen,
                   GwyMaskLine *dest,
                   guint destpos)
{
    if (!_gwy_mask_line_limit_interval(src, &srcpos, &srclen, dest, destpos))
        return;

    if (srclen == src->res) {
        g_assert(srcpos == 0 && destpos == 0);
        gwy_assign(dest->data, src->data, src->priv->stride);
    }
    else
        copy_part(src, srcpos, srclen, dest, destpos);
    gwy_mask_line_invalidate(dest);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
