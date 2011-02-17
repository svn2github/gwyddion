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

#include "libgwy/mask-iter.h"

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

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
