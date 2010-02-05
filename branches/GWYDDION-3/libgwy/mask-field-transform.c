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

#include <string.h>
#include "libgwy/macros.h"
#include "libgwy/math.h"
#include "libgwy/mask-field-transform.h"
#include "libgwy/libgwy-aliases.h"
#include "libgwy/line-internal.h"
#include "libgwy/mask-field-internal.h"

// XXX: Not very efficient, but probably sufficiently efficient.
static void
flip_row(GwyMaskIter iter,  // Initialized to the *end* of the row!
         guint32 *dest,
         guint width)
{
    for (guint j = 0; j < width >> 5; j++) {
        guint32 v = 0;
        for (guint k = 0; k < 0x20; k++) {
            if (gwy_mask_iter_get(iter))
                v |= NTH_BIT(k);
            gwy_mask_iter_prev(iter);
        }
        *(dest++) = v;
    }
    if (width & 0x1f) {
        guint32 v = 0;
        for (guint k = 0; k < (width & 0x1f); k++) {
            if (gwy_mask_iter_get(iter))
                v |= NTH_BIT(k);
            gwy_mask_iter_prev(iter);
        }
        *dest = v;
    }
}

static void
flip_both(GwyMaskField *field,
          guint32 *buffer)
{
    guint xres = field->xres, yres = field->yres, stride = field->stride;
    gsize rowsize = stride * sizeof(guint32);
    for (guint i = 0; i < yres/2; i++) {
        GwyMaskIter iter;
        gwy_mask_field_iter_init(field, iter, xres-1, yres-1 - i);
        flip_row(iter, buffer, xres);
        gwy_mask_field_iter_init(field, iter, xres-1, i);
        flip_row(iter, field->data + (yres-1 - i)*stride, xres);
        memcpy(field->data + i*stride, buffer, rowsize);
    }
    if (yres % 2) {
        GwyMaskIter iter;
        gwy_mask_field_iter_init(field, iter, xres-1, yres/2);
        flip_row(iter, buffer, xres);
        memcpy(field->data + yres/2*stride, buffer, rowsize);
    }
}

static void
flip_horizontally(GwyMaskField *field,
                  guint32 *buffer)
{
    guint xres = field->xres, yres = field->yres, stride = field->stride;
    gsize rowsize = stride * sizeof(guint32);
    for (guint i = 0; i < yres; i++) {
        GwyMaskIter iter;
        gwy_mask_field_iter_init(field, iter, xres-1, i);
        flip_row(iter, buffer, xres);
        memcpy(field->data + i*stride, buffer, rowsize);
    }
}

static void
flip_vertically(GwyMaskField *field,
                guint32 *buffer)
{
    guint yres = field->yres, stride = field->stride;
    gsize rowsize = stride * sizeof(guint32);
    for (guint i = 0; i < yres/2; i++) {
        memcpy(buffer, field->data + (yres-1 - i)*stride, rowsize);
        memcpy(field->data + (yres-1 - i)*stride,
               field->data + i*stride,
               rowsize);
        memcpy(field->data + i*stride, buffer, rowsize);
    }
}

/**
 * gwy_mask_field_flip:
 * @field: A two-dimensional mask field.
 * @horizontally: %TRUE to flip the field horizontally, i.e. about the vertical
 *                axis.
 * @vertically: %TRUE to flip the field vertically, i.e. about the horizontal
 *              axis.
 *
 * Flips a two-dimensional mask field about either axis.
 **/
void
gwy_mask_field_flip(GwyMaskField *field,
                    gboolean horizontally,
                    gboolean vertically)
{
    g_return_if_fail(GWY_IS_MASK_FIELD(field));
    if (!horizontally && !vertically)
        return;

    gsize rowsize = field->stride * sizeof(guint32);
    guint32 *buffer = g_slice_alloc(rowsize);

    if (horizontally && vertically)
        flip_both(field, buffer);
    else if (horizontally)
        flip_horizontally(field, buffer);
    else if (vertically)
        flip_vertically(field, buffer);

    g_slice_free1(rowsize, buffer);
    gwy_mask_field_invalidate(field);
}

static inline void
swap_xy_32x32(const guint32 *src,
              guint slen,
              guint32 *dest)
{
    gwy_memclear(dest, 0x20);
    guint32 bit = FIRST_BIT;
    for (guint i = 0; i < slen; i++) {
        guint32 v = src[i];
        for (guint32 *d = dest; v; d++, v = v SHL 1) {
            if (v & FIRST_BIT)
                *d |= bit;
        }
        bit = bit SHR 1;
    }
}

// Block sizes are measured in destination; in source, the dims are swapped.
static inline void
swap_block_both_aligned(const guint32 *sb, guint32 *db,
                        guint xblocksize, guint yblocksize,
                        guint dstride, guint sstride)
{
    guint32 sbuff[0x20], dbuff[0x20];
    const guint32 *s = sb;
    for (guint i = 0; i < xblocksize; i++, s += sstride)
        sbuff[i] = *s;
    swap_xy_32x32(sbuff, xblocksize, dbuff);
    guint32 *d = db;
    // Must not overwrite destination data outside the target area.
    if (xblocksize < 0x20) {
        guint32 m = MAKE_MASK(0, xblocksize);
        for (guint i = 0; i < yblocksize; i++, d += dstride)
            *d = (*d & ~m) | (dbuff[i] & m);
    }
    else {
        for (guint i = 0; i < yblocksize; i++, d += dstride)
            *d = dbuff[i];
    }
}

// No argument checking.  Columns must be aligned, i.e. multiples of 0x20.
// The source is assumed to be large enough to contain all the data we copy to
// the destination.
static void
swap_xy_both_aligned(const GwyMaskField *source,
                     guint col, guint row,
                     guint width, guint height,
                     GwyMaskField *dest,
                     guint destcol, guint destrow)
{
    guint sstride = source->stride, dstride = dest->stride;
    guint imax = width >> 5, iend = width & 0x1f;
    guint jmax = height >> 5, jend = height & 0x1f;
    const guint32 *sbase = source->data + sstride*row + (col >> 5);
    guint32 *dbase = dest->data + dstride*destrow + (destcol >> 5);

    g_assert((col & 0x1f) == 0);
    g_assert((destcol & 0x1f) == 0);
    for (guint ib = 0; ib < imax; ib++) {
        for (guint jb = 0; jb < jmax; jb++)
            swap_block_both_aligned(sbase + ((jb << 5)*sstride + ib),
                                    dbase + ((ib << 5)*dstride + jb),
                                    0x20, 0x20, dstride, sstride);
        if (jend)
            swap_block_both_aligned(sbase + ((jmax << 5)*sstride + ib),
                                    dbase + ((ib << 5)*dstride + jmax),
                                    jend, 0x20, dstride, sstride);
    }
    if (iend) {
        for (guint jb = 0; jb < jmax; jb++)
            swap_block_both_aligned(sbase + ((jb << 5)*sstride + imax),
                                    dbase + ((imax << 5)*dstride + jb),
                                    0x20, iend, dstride, sstride);
        if (jend)
            swap_block_both_aligned(sbase + ((jmax << 5)*sstride + imax),
                                    dbase + ((imax << 5)*dstride + jmax),
                                    jend, iend, dstride, sstride);
    }
}

// Block sizes are measured in destination; in source, the dims are swapped.
static inline void
swap_block_dest_aligned(const guint32 *sb, guint soff, guint32 *db,
                        guint xblocksize, guint yblocksize,
                        guint dstride, guint sstride)
{
    guint32 sbuff[0x20], dbuff[0x20];
    const guint32 *s = sb;
    // Avoid touching s[1] if we do not actually need it, not to optimize but
    // primarily to avoid buffer overruns.
    if (yblocksize <= 0x20 - soff) {
        for (guint i = 0; i < xblocksize; i++, s += sstride)
            sbuff[i] = (s[0] SHL soff);
    }
    else {
        for (guint i = 0; i < xblocksize; i++, s += sstride)
            sbuff[i] = (s[0] SHL soff) | (s[1] SHR (0x20 - soff));
    }
    swap_xy_32x32(sbuff, xblocksize, dbuff);
    guint32 *d = db;
    // Must not overwrite destination data outside the target area.
    if (xblocksize < 0x20) {
        guint32 m = MAKE_MASK(0, xblocksize);
        for (guint i = 0; i < yblocksize; i++, d += dstride)
            *d = (*d & ~m) | (dbuff[i] & m);
    }
    else {
        for (guint i = 0; i < yblocksize; i++, d += dstride)
            *d = dbuff[i];
    }
}

// No argument checking.  Destination column must be aligned, i.e. a multiple
// of 0x20.  The source is assumed to be large enough to contain all the data
// we copy to the destination.
static void
swap_xy_dest_aligned(const GwyMaskField *source,
                     guint col, guint row,
                     guint width, guint height,
                     GwyMaskField *dest,
                     guint destcol, guint destrow)
{
    guint sstride = source->stride, dstride = dest->stride;
    // We loop over dst blocks so these are the same as in the aligned case.
    guint imax = width >> 5, iend = width & 0x1f;
    guint jmax = height >> 5, jend = height & 0x1f;
    const guint32 *sbase = source->data + sstride*row + (col >> 5);
    guint32 *dbase = dest->data + dstride*destrow + (destcol >> 5);
    guint soff = col & 0x1f;

    g_assert((destcol & 0x1f) == 0);
    for (guint ib = 0; ib < imax; ib++) {
        for (guint jb = 0; jb < jmax; jb++)
            swap_block_dest_aligned(sbase + ((jb << 5)*sstride + ib), soff,
                                    dbase + ((ib << 5)*dstride + jb),
                                    0x20, 0x20, dstride, sstride);
        if (jend)
            swap_block_dest_aligned(sbase + ((jmax << 5)*sstride + ib), soff,
                                    dbase + ((ib << 5)*dstride + jmax),
                                    jend, 0x20, dstride, sstride);
    }
    if (iend) {
        for (guint jb = 0; jb < jmax; jb++)
            swap_block_dest_aligned(sbase + ((jb << 5)*sstride + imax), soff,
                                    dbase + ((imax << 5)*dstride + jb),
                                    0x20, iend, dstride, sstride);
        if (jend)
            swap_block_dest_aligned(sbase + ((jmax << 5)*sstride + imax), soff,
                                    dbase + ((imax << 5)*dstride + jmax),
                                    jend, iend, dstride, sstride);
    }
}

// Block sizes are measured in destination; in source, the dims are swapped.
static inline void
swap_block_src_aligned(const guint32 *sb, guint32 *db, guint doff,
                       guint xblocksize, guint yblocksize,
                       guint dstride, guint sstride)
{
    guint32 sbuff[0x20], dbuff[0x20];
    const guint32 *s = sb;
    for (guint i = 0; i < xblocksize; i++, s += sstride)
        sbuff[i] = *s;
    swap_xy_32x32(sbuff, xblocksize, dbuff);
    guint32 *d = db;
    // Must not overwrite destination data outside the target area.
    if (xblocksize <= 0x20 - doff) {
        guint32 m = MAKE_MASK(doff, xblocksize);
        for (guint i = 0; i < yblocksize; i++, d += dstride)
            *d = (*d & ~m) | ((dbuff[i] SHR doff) & m);
    }
    else {
        guint32 m0d = MAKE_MASK(doff, 0x20 - doff);
        guint32 m1d = MAKE_MASK(0, xblocksize + doff - 0x20);
        for (guint i = 0; i < yblocksize; i++, d += dstride) {
            // No need to mask in the first line, the unneeded bits are shifted
            // away.
            d[0] = (d[0] & ~m0d) | (dbuff[i] SHR doff);
            d[1] = (d[1] & ~m1d) | ((dbuff[i] SHL (0x20 - doff)) & m1d);
        }
    }
}

// No argument checking.  Source column must be aligned, i.e. a multiple of
// 0x20.  The source is assumed to be large enough to contain all the data we
// copy to the destination.
static void
swap_xy_src_aligned(const GwyMaskField *source,
                    guint col, guint row,
                    guint width, guint height,
                    GwyMaskField *dest,
                    guint destcol, guint destrow)
{
    guint sstride = source->stride, dstride = dest->stride;
    // We loop over src blocks so these are the same as in the aligned case.
    guint imax = width >> 5, iend = width & 0x1f;
    guint jmax = height >> 5, jend = height & 0x1f;
    const guint32 *sbase = source->data + sstride*row + (col >> 5);
    guint32 *dbase = dest->data + dstride*destrow + (destcol >> 5);
    guint doff = destcol & 0x1f;

    g_assert((col & 0x1f) == 0);
    for (guint ib = 0; ib < imax; ib++) {
        for (guint jb = 0; jb < jmax; jb++)
            swap_block_src_aligned(sbase + ((jb << 5)*sstride + ib),
                                   dbase + ((ib << 5)*dstride + jb), doff,
                                   0x20, 0x20, dstride, sstride);
        if (jend)
            swap_block_src_aligned(sbase + ((jmax << 5)*sstride + ib),
                                   dbase + ((ib << 5)*dstride + jmax), doff,
                                   jend, 0x20, dstride, sstride);
    }
    if (iend) {
        for (guint jb = 0; jb < jmax; jb++)
            swap_block_src_aligned(sbase + ((jb << 5)*sstride + imax),
                                   dbase + ((imax << 5)*dstride + jb), doff,
                                   0x20, iend, dstride, sstride);
        if (jend)
            swap_block_src_aligned(sbase + ((jmax << 5)*sstride + imax),
                                   dbase + ((imax << 5)*dstride + jmax), doff,
                                   jend, iend, dstride, sstride);
    }
}

/**
 * gwy_mask_field_new_transposed:
 * @field: A two-dimensional mask field.
 *
 * Transposes a mask field, i.e. field rows become columns and vice versa.
 *
 * Returns: A new two-dimensional mask field.
 **/
GwyMaskField*
gwy_mask_field_new_transposed(const GwyMaskField *field)
{
    g_return_val_if_fail(GWY_IS_MASK_FIELD(field), NULL);

    GwyMaskField *newfield = gwy_mask_field_new_sized(field->yres, field->xres,
                                                      FALSE);
    swap_xy_both_aligned(field, 0, 0, field->xres, field->yres, newfield, 0, 0);
    return newfield;
}

/**
 * gwy_mask_field_new_part_transposed:
 * @field: A two-dimensional mask field.
 * @col: Column index of the upper-left corner of the rectangle.
 * @row: Row index of the upper-left corner of the rectangle.
 * @width: Rectangle width (number of columns) in @field, the height of
 *         the newly created mask field.
 * @height: Rectangle height (number of rows) in @field, the width of
 *          the newly created mask field.
 *
 * Transposes a rectangular part of a mask field, making rows columns and vice
 * versa.
 *
 * Returns: A new two-dimensional mask field containing the transposed part
 *          of @field.
 **/
GwyMaskField*
gwy_mask_field_new_part_transposed(const GwyMaskField *field,
                                   guint col, guint row,
                                   guint width, guint height)
{
    g_return_val_if_fail(GWY_IS_MASK_FIELD(field), NULL);
    g_return_val_if_fail(width && height, NULL);
    g_return_val_if_fail(col + width <= field->xres, NULL);
    g_return_val_if_fail(row + height <= field->yres, NULL);

    GwyMaskField *newfield = gwy_mask_field_new_sized(height, width, FALSE);

    if (col & 0x1f)
        swap_xy_dest_aligned(field, col, row, width, height, newfield, 0, 0);
    else
        swap_xy_both_aligned(field, col, row, width, height, newfield, 0, 0);

    return newfield;
}

/**
 * gwy_mask_field_part_transpose:
 * @src: Source two-dimensional mask field.
 * @col: Column index of the upper-left corner of the rectangle in @src.
 * @row: Row index of the upper-left corner of the rectangle in @src.
 * @width: Rectangle width (number of columns) in the source, height in the
 *         destination.
 * @height: Rectangle height (number of rows) in the source, width in the
 *          destination.
 * @dest: Destination two-dimensional mask field.
 * @destcol: Destination column in @dest.
 * @destrow: Destination row in @dest.
 *
 * Copies a rectangular part from one mask field to another, transposing it.
 *
 * The rectangle starts at (@col, @row) in @src and its dimensions are
 * @width×@height. It is transposed to the rectangle @height×@width in @dest
 * starting from (@destcol, @destrow).
 *
 * There are no limitations on the row and column indices or dimensions.  Only
 * the part of the rectangle that is corrsponds to data inside @src and @dest
 * is copied.  This can also mean nothing is copied at all.
 *
 * If @src is equal to @dest, the areas may not overlap.
 **/
void
gwy_mask_field_part_transpose(const GwyMaskField *src,
                              guint col, guint row,
                              guint width, guint height,
                              GwyMaskField *dest,
                              guint destcol, guint destrow)
{
    g_return_if_fail(GWY_IS_MASK_FIELD(src));
    g_return_if_fail(GWY_IS_MASK_FIELD(dest));

    if (col >= src->xres || destcol >= dest->xres
        || row >= src->yres || destrow >= dest->yres)
        return;

    width = MIN(width, src->xres - col);
    height = MIN(height, src->yres - row);
    width = MIN(width, dest->yres - destrow);
    height = MIN(height, dest->xres - destcol);
    if (!width || !height)
        return;

    if (src == dest
        && OVERLAPPING(col, width, destcol, height)
        && OVERLAPPING(row, height, destrow, width)) {
        g_warning("Source and destination blocks overlap.  "
                  "Data corruption is imminent.");
    }

    // FIXME: Direct transposition of unaligned data would be nice.  Can it be
    // written without going insane?
    if ((col & 0x1f) && (destcol & 0x1f)) {
        GwyMaskField *buffer
            = gwy_mask_field_new_part_transposed(src, col, row, width, height);
        gwy_mask_field_part_copy(buffer, 0, 0, height, width,
                                 dest, destcol, destrow);
        g_object_unref(buffer);
    }
    else if (col & 0x1f)
        swap_xy_dest_aligned(src, col, row, width, height,
                             dest, destcol, destrow);
    else if (destcol & 0x1f)
        swap_xy_src_aligned(src, col, row, width, height,
                            dest, destcol, destrow);
    else
        swap_xy_both_aligned(src, col, row, width, height,
                             dest, destcol, destrow);

    gwy_mask_field_invalidate(dest);
}

/**
 * gwy_mask_field_new_rotated_simple:
 * @field: A two-dimensional mask field.
 * @rotation: Rotation amount (it can also be any positive multiple of 90).
 *
 * Rotates a two-dimensional mask field by a multiple by 90 degrees.
 *
 * Returns: A new two-dimensional mask field.
 **/
GwyMaskField*
gwy_mask_field_new_rotated_simple(const GwyMaskField *field,
                                  GwySimpleRotation rotation)
{
    g_return_val_if_fail(GWY_IS_MASK_FIELD(field), NULL);
    rotation %= 360;

    if (rotation == GWY_SIMPLE_ROTATE_NONE)
        return gwy_mask_field_duplicate(field);

    if (rotation == GWY_SIMPLE_ROTATE_UPSIDEDOWN) {
        GwyMaskField *newfield = gwy_mask_field_duplicate(field);
        gwy_mask_field_flip(newfield, TRUE, TRUE);
        return newfield;
    }

    if (rotation != GWY_SIMPLE_ROTATE_CLOCKWISE
        && rotation != GWY_SIMPLE_ROTATE_COUNTERCLOCKWISE) {
        g_critical("Invalid simple rotation amount %u.", rotation);
        return NULL;
    }

    GwyMaskField *newfield = gwy_mask_field_new_transposed(field);

    gsize rowsize = field->stride * sizeof(guint32);
    guint32 *buffer = g_slice_alloc(rowsize);

    if (rotation == GWY_SIMPLE_ROTATE_COUNTERCLOCKWISE)
        flip_vertically(newfield, buffer);
    if (rotation == GWY_SIMPLE_ROTATE_CLOCKWISE)
        flip_horizontally(newfield, buffer);

    g_slice_free1(rowsize, buffer);

    return newfield;
}

#define __LIBGWY_MASK_FIELD_TRANSFORM_C__
#include "libgwy/libgwy-aliases.c"

/**
 * SECTION: mask-field-transform
 * @section_id: GwyMaskField-transform
 * @title: GwyMaskField transformations
 * @short_description: Geometrical transformations of mask fields
 *
 * Some field transformations are performed in place while others create new
 * fields.  This simply follows which transformations <emphasis>can</emphasis>
 * be performed in place.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
