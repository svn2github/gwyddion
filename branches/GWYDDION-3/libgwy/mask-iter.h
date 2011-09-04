/*
 *  $Id$
 *  Copyright (C) 2009-2011 David Nečas (Yeti).
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

#ifndef __LIBGWY_MASK_ITER_H__
#define __LIBGWY_MASK_ITER_H__

G_BEGIN_DECLS

typedef enum {
    GWY_MASK_IGNORE  = 0,
    GWY_MASK_INCLUDE = 1,
    GWY_MASK_EXCLUDE = 2,
} GwyMaskingType;

typedef struct {
    guint32 *p;
    guint32 bit;
} GwyMaskIter;

#if (G_BYTE_ORDER == G_LITTLE_ENDIAN)
#define gwy_mask_iter_next(iter) \
    do { if (!(iter.bit <<= 1)) { iter.bit = 1u; iter.p++; } } while (0)

#define gwy_mask_iter_prev(iter) \
    do { if (!(iter.bit >>= 1)) { iter.bit = 0x80000000u; iter.p--; } } while (0)
#endif

#if (G_BYTE_ORDER == G_BIG_ENDIAN)
#define gwy_mask_iter_next(iter) \
    do { if (!(iter.bit >>= 1)) { iter.bit = 0x80000000u; iter.p++; } } while (0)

#define gwy_mask_iter_prev(iter) \
    do { if (!(iter.bit <<= 1)) { iter.bit = 1u; iter.p--; } } while (0)
#endif

#define gwy_mask_iter_get(iter) \
    (*(iter).p & (iter).bit)

#define gwy_mask_iter_set(iter, value) \
    do { if (value) *(iter).p |= (iter).bit; else *(iter).p &= ~(iter).bit; } while (0)

typedef struct {
    gdouble w0;
    gdouble w1;
    guint move;
} GwyMaskScalingSegment;

GwyMaskScalingSegment* gwy_mask_prepare_scaling(gdouble pos,
                                                gdouble step,
                                                guint nsteps,
                                                guint *required_bits) G_GNUC_MALLOC;

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
