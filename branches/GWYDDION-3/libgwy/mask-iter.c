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

#include "libgwy/macros.h"
#include "libgwy/math.h"
#include "libgwy/mask-iter.h"

/**
 * gwy_mask_prepare_scaling:
 * @pos: Initial position in the mask.  Can be non-integral to start within a
 *       pixel.
 * @step: The size of one destination pixel in the source (the inverse of
 *        zoom).
 * @nsteps: The number of pixels wanted for the destination.
 * @required_bits: (out):
 *                 How many input bits it will consume.  This can be used for
 *                 verifying that the source is large enough.  All bits that
 *                 are used with nonzero weight are counted as consumed.
 *
 * Prepares auxiliary data for bit mask scaling.
 *
 * Returns: (transfer full) (array length=nsteps):
 *          Newly allocated array of scaling segment descriptors.
 **/
GwyMaskScalingSegment*
gwy_mask_prepare_scaling(gdouble pos, gdouble step, guint nsteps,
                         guint *required_bits)
{
    GwyMaskScalingSegment *segments = g_new(GwyMaskScalingSegment, nsteps),
                          *seg = segments;
    guint end = floor(pos), first = end;
    gdouble x = pos - end;

    for (guint i = nsteps; i; i--, seg++) {
        guint begin = end;
        pos += step;
        end = floor(pos);
        if (end == begin) {
            seg->w0 = 1.0;
            x = pos - end;
            seg->w1 = 0.0;
            seg->move = 0;
        }
        else {
            seg->w0 = (1.0 - x)/step;
            x = pos - end;
            seg->w1 = x/step;
            seg->move = end - begin;
        }
    }

    // Try to avoid reading a slightly after the last bit.
    seg--;
    if (seg->move && seg->w1 < 1e-6) {
        seg->move--;
        seg->w1 = seg->move ? 1.0/step : 0.0;
        end--;
    }

    GWY_MAYBE_SET(required_bits, end+1 - first);

    return segments;
}

/**
 * gwy_mask_scale_row_weighted:
 * @srciter: Mask iterator set to the first bit in the source row.
 * @seg: (array length=res):
 *       Precalculated scaling segments determining the scaling.
 * @dest: (inout) (array length=res):
 *        Destination array.  Its contents is not plainly overwritten, the
 *        weights are added.  If you want to start from scratch clear it
 *        beforehand.
 * @res: Destination resolution, i.e. the number of items in @seg and @dest.
 * @step: The size of one destination pixel in the source (the inverse of
 *        zoom).
 * @weight: Factor to multiply all values with.  It is useful for
 *          multi-dimensional interpolation where it can represent the weight
 *          of the entire source row.
 *
 * Resamples one bit mask row to doubles with weighting.
 *
 * This is an auxiliary function chiefly used for mask resampling.
 **/
void
gwy_mask_scale_row_weighted(GwyMaskIter srciter,
                            const GwyMaskScalingSegment *seg,
                            gdouble *dest,
                            guint res, gdouble step, gdouble weight)
{
    if (step > 1.0) {
        // seg->move is always nonzero, except perhaps the last one.
        for (guint i = res-1; i; i--, seg++) {
            gdouble s = seg->w0 * !!gwy_mask_iter_get(srciter);
            guint c = 0;
            for (guint k = seg->move-1; k; k--) {
                gwy_mask_iter_next(srciter);
                c += !!gwy_mask_iter_get(srciter);
            }
            gwy_mask_iter_next(srciter);
            s += c/step + seg->w1 * !!gwy_mask_iter_get(srciter);
            *(dest++) += weight*s;
        }

        // The last iteration, seg->move may be zero.  Avoid accessing the w1
        // bit in such case.
        gdouble s = seg->w0 * !!gwy_mask_iter_get(srciter);
        if (seg->move) {
            guint c = 0;
            for (guint k = seg->move-1; k; k--) {
                gwy_mask_iter_next(srciter);
                c += !!gwy_mask_iter_get(srciter);
            }
            gwy_mask_iter_next(srciter);
            s += c/step + seg->w1 * !!gwy_mask_iter_get(srciter);
        }
        *(dest++) += weight*s;
    }
    else {
        // seg->move is at most 1.
        for (guint i = res; i; i--, seg++) {
            gdouble s = seg->w0 * !!gwy_mask_iter_get(srciter);
            if (seg->move) {
                gwy_mask_iter_next(srciter);
                s += seg->w1 * !!gwy_mask_iter_get(srciter);
            }
            *(dest++) += weight*s;
        }
    }
}

/**
 * SECTION: mask-iter
 * @title: GwyMaskIter
 * @short_description: Bit mask iterator
 *
 * A mask iterator represents a relatively efficient method of sequentially
 * accessing mask structures such as #GwyMaskLine and #GwyMaskField.  In case
 * of fields, sequential access means row-wise access.  Each class provides
 * an initialisation method such as gwy_mask_line_iter_init() or
 * gwy_mask_field_iter_init() that set the iterator to a specific bit and
 * the functions of the iterator can be the used to read and set bits and move
 * forward or backward.
 *
 * The following example demonstrates the typical use on finding the minimum of
 * masked two-dimensional data.  Notice how the iterator is initialised for
 * each row because the mask field rows do not form one contiguous block
 * although each individual row is contiguous.
 * |[
 * gdouble min = G_MAXDOUBLE;
 * for (guint i = 0; i < field->height; i++) {
 *     const gdouble *d = field->data + i*field->xres;
 *     GwyMaskIter iter;
 *     gwy_mask_field_iter_init(mask, iter, 0, i);
 *     for (guint j = 0; j < field->xres; j++) {
 *         if (gwy_mask_iter_get(iter)) {
 *             if (min > d[j])
 *                 min = d[j];
 *         }
 *         gwy_mask_iter_next(iter);
 *     }
 * }
 * ]|
 **/

/**
 * GwyMaskIter:
 * @p: Pointer to the current mask data item.
 * @bit: The current bit, i.e. value with always exactly one bit set.
 *
 * Mask iterator.
 *
 * The mask iterator is a method of accessing individual mask pixels within a
 * contiguous block, suitable especially for sequential processing.  It can be
 * used for reading and writing bits and it is possible to move it one pixel
 * forward or backward within the block with gwy_mask_iter_next() or
 * gwy_mask_iter_prev(), respectively.
 *
 * The iterator is represented by a quite simple structure that is supposed to
 * be allocated as an automatic variable and passed/copied by value.  It can be
 * re-initialised any number of times, even to iterate in completely different
 * mask objects.  It can be simply forgotten when no longer useful (i.e. there
 * is no teardown function).
 **/

/**
 * gwy_mask_iter_next:
 * @iter: Mask iterator.  It must be an identifier.
 *
 * Moves a mask iterator one pixel right.
 *
 * This macro is usable as a single statement.
 *
 * No argument validation is performed.
 * The caller must ensure the position does not leave the contiguous mask
 * block.
 **/

/**
 * gwy_mask_iter_prev:
 * @iter: Mask iterator.  It must be an identifier.
 *
 * Moves a mask iterator one pixel left.
 *
 * This macro is usable as a single statement.
 *
 * No argument validation is performed.
 * The caller must ensure the position does not leave the contiguous mask
 * block.
 **/

/**
 * gwy_mask_iter_get:
 * @iter: Mask iterator.  It must be an identifier.
 *
 * Obtains the value a mask iterator points to.
 *
 * No argument validation is performed.
 *
 * Returns: Nonzero value (not necessarily 1) if the mask bit is set, zero if
 *          it is unset.
 **/

/**
 * gwy_mask_iter_set:
 * @iter: Mask iterator.  It must be an identifier.
 * @value: Nonzero value to set the bit, zero to clear it.
 *
 * Sets the value a mask iterator points to.
 *
 * This macro is usable as a single statement.
 *
 * No argument validation is performed.
 *
 * This is a low-level macro and it does not invalidate the mask object.
 **/

/**
 * GwyMaskingType:
 * @GWY_MASK_EXCLUDE: Exclude data under mask, i.e. take into account only
 *                    data not covered by the mask.
 * @GWY_MASK_INCLUDE: Take into account only data under the mask.
 * @GWY_MASK_IGNORE: Ignore mask, if present, and use all data.
 *
 * Mask interpretation in procedures that can apply masks.
 **/

/**
 * GwyMaskScalingSegment:
 * @w0: Contribution of the first bit.
 * @w1: Contribution of the last bit.
 * @move: How many bits to move forward to the last bit.  If it is zero @w1 is
 *        always zero too; the entire weight is consolidated to @w0.
 *
 * Precomputed auxiliary data for mask scaling.
 *
 * If scaling up, @move is at least 1 and @w0 and @w1 then refer to different
 * bits and there may be some pixels between to include completely.  There is
 * one exception: the very last pixel may have @w0=1, @w1=0, @move=0.
 *
 * If, on the other hand, scaling down, @move is at most 1 and there are never
 * any pixels between to include completely.  In this case @move can be also 0
 * which means the entire contribution is within one pixel.
 *
 * The bit weights can be visualised as follows:
 * <programlisting>
 * ┌────┬───┬───┬───┬────┐      ┌────┬────┐      ┌────┐
 * │ w₀ │ 1 │ 1 │ 1 │ w₁ │      │ w₀ │ w₁ │      │ w₀ │
 * └────┴───┴───┴───┴────┘      └────┴────┘      └────┘
 * │───────────────→│           │───→│           │
 *      move ≥ 2                move = 1         move = 0
 * </programlisting>
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
