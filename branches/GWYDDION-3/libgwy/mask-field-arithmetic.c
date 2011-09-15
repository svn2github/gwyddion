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

/* NB: copying is also implemented here as it shares some macros even though it
 * is declared in the main header. */

#include <string.h>
#include "libgwy/macros.h"
#include "libgwy/math.h"
#include "libgwy/mask-field-arithmetic.h"
#include "libgwy/mask-field-grains.h"
#include "libgwy/object-internal.h"
#include "libgwy/mask-field-internal.h"

// Only one item is modified per row but the mask may need bits cut off from
// both sides (unlike in all other cases).
#define LOGICAL_OP_PART_SINGLE(masked) \
    do { \
        const guint32 m = MAKE_MASK(doff, width); \
        for (guint i = 0; i < height; i++) { \
            const guint32 *p = sbase + i*src->stride; \
            guint32 *q = dbase + i*dest->stride; \
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
        } \
    } while (0)

// Multiple items are modified per row but the offsets match so no shifts are
// necessary, just masking at the ends.
#define LOGICAL_OP_PART_ALIGNED(simple, masked) \
    do { \
        const guint32 m0d = MAKE_MASK(doff, 0x20 - doff); \
        const guint32 m1d = MAKE_MASK(0, dend); \
        for (guint i = 0; i < height; i++) { \
            const guint32 *p = sbase + i*src->stride; \
            guint32 *q = dbase + i*dest->stride; \
            guint j = width; \
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
        } \
    } while (0)

// The general case, multi-item transfer and different offsets.
#define LOGICAL_OP_PART_GENERAL(simple, masked) \
    do { \
        const guint32 m0d = MAKE_MASK(doff, 0x20 - doff); \
        const guint32 m1d = MAKE_MASK(0, dend); \
        for (guint i = 0; i < height; i++) { \
            const guint32 *p = sbase + i*src->stride; \
            guint32 *q = dbase + i*dest->stride; \
            guint j = width; \
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
        } \
    } while (0)

#define LOGICAL_OP_PART(simple, masked) \
    if (width <= 0x20 - doff) \
        LOGICAL_OP_PART_SINGLE(masked); \
    else if (doff == soff) \
        LOGICAL_OP_PART_ALIGNED(simple, masked); \
    else \
        LOGICAL_OP_PART_GENERAL(simple, masked) \

static void
copy_part(const GwyMaskField *src,
          guint col,
          guint row,
          guint width,
          guint height,
          GwyMaskField *dest,
          guint destcol,
          guint destrow)
{
    const guint32 *sbase = src->data + src->stride*row + (col >> 5);
    guint32 *dbase = dest->data + dest->stride*destrow + (destcol >> 5);
    const guint soff = col & 0x1f;
    const guint doff = destcol & 0x1f;
    const guint send = (col + width) & 0x1f;
    const guint dend = (destcol + width) & 0x1f;
    const guint k = (doff + 0x20 - soff) & 0x1f;
    const guint kk = 0x20 - k;
    LOGICAL_OP_PART(*q = vp, *q = (*q & ~m) | (vp & m));
}

/**
 * gwy_mask_field_copy:
 * @src: Source two-dimensional mask field.
 * @srcrectangle: Area in field @src to copy.  Pass %NULL to copy entire @src.
 * @dest: Destination two-dimensional mask field.
 * @destcol: Destination column in @dest.
 * @destrow: Destination row in @dest.
 *
 * Copies data from one mask field to another.
 *
 * The copied rectangle is defined by @srcpart and it is copied to @dest
 * starting from (@destcol, @destrow).
 *
 * There are no limitations on the row and column indices or dimensions.  Only
 * the part of the rectangle that corresponds to data inside @src and @dest
 * is copied.  This can also mean nothing is copied at all.
 *
 * If @src is equal to @dest the areas may <emphasis>not</emphasis> overlap.
 **/
void
gwy_mask_field_copy(const GwyMaskField *src,
                    const GwyFieldPart *srcpart,
                    GwyMaskField *dest,
                    guint destcol,
                    guint destrow)
{
    guint col, row, width, height;
    if (!gwy_mask_field_limit_parts(src, srcpart, dest, destcol, destrow,
                                    FALSE, &col, &row, &width, &height))
        return;

    if (width == src->xres
        && width == dest->xres
        && dest->stride == src->stride) {
        g_assert(col == 0 && destcol == 0);
        guint rowsize = src->stride * sizeof(guint32);
        memcpy(dest->data + dest->stride*destrow,
               src->data + src->stride*row, height*rowsize);
    }
    else
        copy_part(src, col, row, width, height, dest, destcol, destrow);
    gwy_mask_field_invalidate(dest);
}

/**
 * gwy_mask_field_copy_full:
 * @src: Source two-dimensional mask field.
 * @dest: Destination two-dimensional mask field.
 *
 * Copies entire data from one mask field to another.
 *
 * The two fields must be of identical dimensions.
 **/
void
gwy_mask_field_copy_full(const GwyMaskField *src,
                         GwyMaskField *dest)
{
    g_return_if_fail(GWY_IS_MASK_FIELD(src));
    g_return_if_fail(GWY_IS_MASK_FIELD(dest));
    // This is a sanity check as gwy_mask_field_copy() can handle anything.
    g_return_if_fail(src->xres == dest->xres && src->yres == dest->yres);
    gwy_mask_field_copy(src, NULL, dest, 0, 0);
}

static void
set_part(GwyMaskField *field,
         guint col,
         guint row,
         guint width,
         guint height)
{
    guint32 *base = field->data + field->stride*row + (col >> 5);
    const guint off = col & 0x1f;
    const guint end = (col + width) & 0x1f;
    if (width <= 0x20 - off) {
        const guint32 m = MAKE_MASK(off, width);
        for (guint i = 0; i < height; i++) {
            guint32 *p = base + i*field->stride;
            *p |= m;
        }
    }
    else {
        const guint32 m0 = MAKE_MASK(off, 0x20 - off);
        const guint32 m1 = MAKE_MASK(0, end);
        for (guint i = 0; i < height; i++) {
            guint32 *p = base + i*field->stride;
            guint j = width;
            *p |= m0;
            j -= 0x20 - off, p++;
            while (j >= 0x20) {
                *p = ALL_SET;
                j -= 0x20, p++;
            }
            if (!end)
                continue;
            *p |= m1;
        }
    }
}

static void
clear_part(GwyMaskField *field,
           guint col,
           guint row,
           guint width,
           guint height)
{
    guint32 *base = field->data + field->stride*row + (col >> 5);
    const guint off = col & 0x1f;
    const guint end = (col + width) & 0x1f;
    if (width <= 0x20 - off) {
        const guint32 m = ~MAKE_MASK(off, width);
        for (guint i = 0; i < height; i++) {
            guint32 *p = base + i*field->stride;
            *p &= m;
        }
    }
    else {
        const guint32 m0 = ~MAKE_MASK(off, 0x20 - off);
        const guint32 m1 = ~MAKE_MASK(0, end);
        for (guint i = 0; i < height; i++) {
            guint32 *p = base + i*field->stride;
            guint j = width;
            *p &= m0;
            j -= 0x20 - off, p++;
            while (j >= 0x20) {
                *p = ALL_CLEAR;
                j -= 0x20, p++;
            }
            if (!end)
                continue;
            *p &= m1;
        }
    }
}

/**
 * gwy_mask_field_fill:
 * @field: A two-dimensional mask field.
 * @fpart: (allow-none):
 *         Area in @field to process.  Pass %NULL to process entire @field.
 * @value: Value to fill the rectangle with.
 *
 * Fills a mask field with the specified value.
 **/
void
gwy_mask_field_fill(GwyMaskField *field,
                    const GwyFieldPart *fpart,
                    gboolean value)
{
    guint col, row, width, height;
    if (!gwy_mask_field_check_part(field, fpart, &col, &row, &width, &height))
        return;

    if (width == field->xres) {
        g_assert(col == 0);
        memset(field->data + row*field->stride,
               value ? 0xff : 0x00,
               height*field->stride*sizeof(guint32));
    }
    else {
        if (value)
            set_part(field, col, row, width, height);
        else
            clear_part(field, col, row, width, height);
    }
    gwy_mask_field_invalidate(field);
}

/**
 * gwy_mask_field_fill_ellipse:
 * @field: A two-dimensional mask field.
 * @fpart: (allow-none):
 *         Area in @field to process.  Pass %NULL to process entire @field.
 * @entire_rectangle: Pass %TRUE to set or clear all pixels in @fpart.
 *                    Pixels inside the ellipse will be set and those outside
 *                    will be cleared (or the other way round, depending on
 *                    @value).  Passing %FALSE means only pixels inside the
 *                    ellipse are modified.
 * @value: Value to fill the ellipse with.
 *
 * Fills an elliptical part of a mask field.
 *
 * The ellipse to fill is bound by @fpart.
 *
 * Note discretised ellipses with one dimension very small and even and the
 * other large (at least hundred times larger than the small one) may not
 * actually touch the distant sides because the pixel centres in the boundary
 * row/column lie outside the exact ellipse.
 *
 * Parameter @entire_rectangle is namely useful if you need an empty mask field
 * with a maximum-sized ellipse as it ensures all pixels are initialised
 * (either set or cleared) in a single step:
 * |[
 * GwyMaskField *mask = gwy_mask_field_new_sized(xres, yres, FALSE);
 * gwy_mask_field_fill_ellipse(mask, NULL, TRUE, TRUE);
 * ]|
 **/
void
gwy_mask_field_fill_ellipse(GwyMaskField *field,
                            const GwyFieldPart *fpart,
                            gboolean entire_rectangle,
                            gboolean value)
{
    guint col, row, width, height;
    if (!gwy_mask_field_check_part(field, fpart, &col, &row, &width, &height))
        return;

    // We could use Bresenham but we need to fill the ellipse so it would
    // complicate things.  One floating point calculation per row is fine.
    gdouble rx = 0.5*width, ry = 0.5*height;
    for (guint i = 0; i < height; i++) {
        gdouble eta = (i + 0.5)/ry;
        guint xlen = gwy_round(rx*(1.0 - sqrt(eta*fmax(2.0 - eta, 0.0))));
        g_assert(2*xlen <= width);
        if (value) {
            if (entire_rectangle && xlen)
                clear_part(field, col, row + i, xlen, 1);
            set_part(field, col + xlen, row + i, width - 2*xlen, 1);
            if (entire_rectangle && xlen)
                clear_part(field, col + width - xlen, row + i, xlen, 1);
        }
        else {
            if (entire_rectangle && xlen)
                set_part(field, col, row + i, xlen, 1);
            clear_part(field, col + xlen, row + i, width - 2*xlen, 1);
            if (entire_rectangle && xlen)
                set_part(field, col + width - xlen, row + i, xlen, 1);
        }
    }
}

static void
invert_part(GwyMaskField *field,
            guint col,
            guint row,
            guint width,
            guint height)
{
    guint32 *base = field->data + field->stride*row + (col >> 5);
    const guint off = col & 0x1f;
    const guint end = (col + width) & 0x1f;
    if (width <= 0x20 - off) {
        const guint32 m = MAKE_MASK(off, width);
        for (guint i = 0; i < height; i++) {
            guint32 *p = base + i*field->stride;
            *p ^= m;
        }
    }
    else {
        const guint32 m0 = MAKE_MASK(off, 0x20 - off);
        const guint32 m1 = MAKE_MASK(0, end);
        for (guint i = 0; i < height; i++) {
            guint32 *p = base + i*field->stride;
            guint j = width;
            *p ^= m0;
            j -= 0x20 - off, p++;
            while (j >= 0x20) {
                *p ^= ALL_SET;
                j -= 0x20, p++;
            }
            if (!end)
                continue;
            *p ^= m1;
        }
    }
}

// FIXME: Does it worth publishing?  One usually inverts the complete mask.
/**
 * gwy_mask_field_invert:
 * @field: A two-dimensional mask field.
 * @fpart: (allow-none):
 *         Area in @field to process.  Pass %NULL to process entire @field.
 *
 * Inverts values in a mask field.
 **/
static void
gwy_mask_field_invert(GwyMaskField *field,
                      const GwyFieldPart *fpart)
{
    guint col, row, width, height;
    if (!gwy_mask_field_check_part(field, fpart, &col, &row, &width, &height))
        return;
    invert_part(field, col, row, width, height);
    gwy_mask_field_invalidate(field);
}

#define LOGICAL_OP_LOOP(simple, masked) \
    do { \
        if (mask) { \
            const guint32 *m = mask->data; \
            for (guint i = n; i; i--, p++, q++, m++) \
                masked; \
        } \
        else { \
            for (guint i = n; i; i--, p++, q++) \
                simple; \
        } \
    } while (0)

/**
 * gwy_mask_field_logical:
 * @field: A two-dimensional mask field to modify and the first operand of
 *         the logical operation.
 * @operand: A two-dimensional mask field representing second operand of the
 *           logical operation.  It can be %NULL for degenerate operations that
 *           do not depend on the second operand.
 * @mask: A two-dimensional mask field determining to which bits of
 *        @field to apply the logical operation to.  If it is %NULL the
 *        opperation is applied to all bits (as if all bits in @mask were set).
 * @op: Logical operation to perform.
 *
 * Modifies a mask field by logical operation with another mask field.
 **/
void
gwy_mask_field_logical(GwyMaskField *field,
                       const GwyMaskField *operand,
                       const GwyMaskField *mask,
                       GwyLogicalOperator op)
{
    g_return_if_fail(GWY_IS_MASK_FIELD(field));
    g_return_if_fail(op <= GWY_LOGICAL_ONE);
    if (op == GWY_LOGICAL_A)
        return;
    if (mask) {
        g_return_if_fail(GWY_IS_MASK_FIELD(mask));
        g_return_if_fail(field->xres == mask->xres);
        g_return_if_fail(field->yres == mask->yres);
        g_return_if_fail(field->stride == mask->stride);
    }
    if (op == GWY_LOGICAL_ZERO) {
        if (mask) {
            op = GWY_LOGICAL_NIMPL;
            operand = mask;
            mask = NULL;
        }
    }
    else if (op == GWY_LOGICAL_ONE) {
        if (mask) {
            op = GWY_LOGICAL_OR;
            operand = mask;
            mask = NULL;
        }
    }
    else if (op == GWY_LOGICAL_NA) {
        if (mask) {
            op = GWY_LOGICAL_XOR;
            operand = mask;
            mask = NULL;
        }
    }
    else {
        g_return_if_fail(GWY_IS_MASK_FIELD(operand));
        g_return_if_fail(field->xres == operand->xres);
        g_return_if_fail(field->yres == operand->yres);
        g_return_if_fail(field->stride == operand->stride);
    }

    guint n = field->stride * field->yres;
    const guint32 *p = operand ? operand->data : NULL;
    guint32 *q = field->data;

    // GWY_LOGICAL_ZERO cannot have mask.
    if (op == GWY_LOGICAL_ZERO)
        gwy_mask_field_fill(field, NULL, FALSE);
    else if (op == GWY_LOGICAL_AND)
        LOGICAL_OP_LOOP(*q &= *p, *q &= ~*m | (*p & *m));
    else if (op == GWY_LOGICAL_NIMPL)
        LOGICAL_OP_LOOP(*q &= ~*p, *q &= ~*m | (~*p & *m));
    // GWY_LOGICAL_A cannot get here.
    else if (op == GWY_LOGICAL_NCIMPL)
        LOGICAL_OP_LOOP(*q = ~*q & *p, *q = (*q & ~*m) | (~*q & *p & *m));
    else if (op == GWY_LOGICAL_B)
        LOGICAL_OP_LOOP(*q = *p, *q = (*q & ~*m) | (*p & *m));
    else if (op == GWY_LOGICAL_XOR)
        LOGICAL_OP_LOOP(*q ^= *p, *q ^= *m & *p);
    else if (op == GWY_LOGICAL_OR)
        LOGICAL_OP_LOOP(*q |= *p, *q |= *m & *p);
    else if (op == GWY_LOGICAL_NOR)
        LOGICAL_OP_LOOP(*q = ~(*q | *p), *q = (*q & ~*m) | (~(*q | *p) & *m));
    else if (op == GWY_LOGICAL_NXOR)
        LOGICAL_OP_LOOP(*q = ~(*q ^ *p), *q = (*q & ~*m) | (~(*q ^ *p) & *m));
    else if (op == GWY_LOGICAL_NB)
        LOGICAL_OP_LOOP(*q = ~*p, *q = (*q & ~*m) | (~*p & *m));
    else if (op == GWY_LOGICAL_CIMPL)
        LOGICAL_OP_LOOP(*q |= ~*p, *q |= ~*p & *m);
    // GWY_LOGICAL_NA cannot have mask.
    else if (op == GWY_LOGICAL_NA)
        LOGICAL_OP_LOOP(*q = ~*q, g_assert_not_reached());
    else if (op == GWY_LOGICAL_IMPL)
        LOGICAL_OP_LOOP(*q = ~*q | *p, *q = (*q & ~*m) | ((~*q | *p) & *m));
    else if (op == GWY_LOGICAL_NAND)
        LOGICAL_OP_LOOP(*q = ~(*q & *p),  *q = (*q & ~*m) | (~(*q & *p) & *m));
    // GWY_LOGICAL_ONE cannot have mask.
    else if (op == GWY_LOGICAL_ONE)
        gwy_mask_field_fill(field, NULL, TRUE);
    else {
        g_assert_not_reached();
    }
    gwy_mask_field_invalidate(field);
}

static void
logical_part(const GwyMaskField *src,
             guint col,
             guint row,
             guint width,
             guint height,
             GwyMaskField *dest,
             guint destcol,
             guint destrow,
             GwyLogicalOperator op)
{
    const guint32 *sbase = src->data + src->stride*row + (col >> 5);
    guint32 *dbase = dest->data + dest->stride*destrow + (destcol >> 5);
    const guint soff = col & 0x1f;
    const guint doff = destcol & 0x1f;
    const guint send = (col + width) & 0x1f;
    const guint dend = (destcol + width) & 0x1f;
    const guint k = (doff + 0x20 - soff) & 0x1f;
    const guint kk = 0x20 - k;
    // GWY_LOGICAL_ZERO cannot get here.
    if (op == GWY_LOGICAL_AND)
        LOGICAL_OP_PART(*q &= vp, *q &= ~m | (vp & m));
    else if (op == GWY_LOGICAL_NIMPL)
        LOGICAL_OP_PART(*q &= ~vp, *q &= ~m | (~vp & m));
    // GWY_LOGICAL_A cannot get here.
    else if (op == GWY_LOGICAL_NCIMPL)
        LOGICAL_OP_PART(*q = ~*q & vp, *q = (*q & ~m) | (~*q & vp & m));
    // GWY_LOGICAL_B cannot get here.
    else if (op == GWY_LOGICAL_XOR)
        LOGICAL_OP_PART(*q ^= vp, *q ^= m & vp);
    else if (op == GWY_LOGICAL_OR)
        LOGICAL_OP_PART(*q |= vp, *q |= m & vp);
    else if (op == GWY_LOGICAL_NOR)
        LOGICAL_OP_PART(*q = ~(*q | vp), *q = (*q & ~m) | (~(*q | vp) & m));
    else if (op == GWY_LOGICAL_NXOR)
        LOGICAL_OP_PART(*q = ~(*q ^ vp), *q = (*q & ~m) | (~(*q ^ vp) & m));
    else if (op == GWY_LOGICAL_NB)
        LOGICAL_OP_PART(*q = ~vp, *q = (*q & ~m) | (~vp & m));
    else if (op == GWY_LOGICAL_CIMPL)
        LOGICAL_OP_PART(*q |= ~vp, *q |= ~vp & m);
    // GWY_LOGICAL_NA cannot get here.
    else if (op == GWY_LOGICAL_IMPL)
        LOGICAL_OP_PART(*q = ~*q | vp, *q = (*q & ~m) | ((~*q | vp) & m));
    else if (op == GWY_LOGICAL_NAND)
        LOGICAL_OP_PART(*q = ~(*q & vp),  *q = (*q & ~m) | (~(*q & vp) & m));
    // GWY_LOGICAL_ONE cannot get here.
    else {
        g_assert_not_reached();
    }
}

/**
 * gwy_mask_field_part_logical:
 * @field: A two-dimensional mask field to modify and the first operand of
 *         the logical operation.
 * @fpart: (allow-none):
 *         Area in @field to process.  Pass %NULL to process entire @field.
 * @operand: A two-dimensional mask field representing second operand of the
 *           logical operation.  It can be %NULL for degenerate operations that
 *           do not depend on the second operand.
 * @opcol: Operand column in @dest.
 * @oprow: Operand row in @dest.
 * @op: Logical operation to perform.
 *
 * Modifies a mask field by logical operation with another mask field.
 *
 * The copied rectangle is defined by @fpart and it is modified using data
 * in @operand starting from (@opcol, @oprow).  Note that although this method
 * resembles gwy_mask_field_copy() the arguments convention is different: the
 * destination comes first then the operand, similarly to in
 * gwy_mask_field_logical().
 *
 * There are no limitations on the row and column indices or dimensions.  Only
 * the part of the rectangle that is corresponds to data inside @field and
 * @operand is modified.  This can also mean nothing is modified at all.
 *
 * If @operand is equal to @field, the areas may not overlap.
 **/
void
gwy_mask_field_part_logical(GwyMaskField *field,
                            const GwyFieldPart *fpart,
                            const GwyMaskField *operand,
                            guint opcol,
                            guint oprow,
                            GwyLogicalOperator op)
{
    g_return_if_fail(GWY_IS_MASK_FIELD(field));
    g_return_if_fail(op <= GWY_LOGICAL_ONE);

    guint col, row, width, height;
    if (fpart) {
        col = fpart->col;
        row = fpart->row;
        width = fpart->width;
        height = fpart->height;
        if (col >= field->xres || row >= field->yres)
            return;
        width = MIN(width, field->xres - col);
        height = MIN(height, field->yres - row);
    }
    else {
        col = row = 0;
        width = field->xres;
        height = field->yres;
    }

    if (opcol >= operand->xres || oprow >= operand->yres)
        return;
    width = MIN(width, operand->xres - opcol);
    height = MIN(height, operand->yres - oprow);
    if (!width || !height)
        return;

    if (op == GWY_LOGICAL_A)
        return;

    GwyFieldPart rect = { col, row, width, height };
    if (op == GWY_LOGICAL_ZERO) {
        gwy_mask_field_fill(field, &rect, FALSE);
        return;
    }
    if (op == GWY_LOGICAL_B) {
        rect.col = opcol;
        rect.row = oprow;
        gwy_mask_field_copy(operand, &rect, field, col, row);
        return;
    }
    if (op == GWY_LOGICAL_NA) {
        gwy_mask_field_invert(field, &rect);
        return;
    }
    if (op == GWY_LOGICAL_ONE) {
        gwy_mask_field_fill(field, &rect, TRUE);
        return;
    }

    g_return_if_fail(GWY_IS_MASK_FIELD(operand));

    logical_part(operand, opcol, oprow, width, height, field, col, row, op);
    gwy_mask_field_invalidate(field);
}

static void
shrink_row(const guint32 *u,
           const guint32 *p,
           const guint32 *d,
           guint32 m0,
           guint len,
           guint end,
           gboolean from_borders,
           guint32 *q)
{
    guint32 v, vl, vr;

    if (!len) {
        v = *p & m0;
        vl = (v SHR 1) | (from_borders ? 0 : (v & NTH_BIT(0)));
        vr = (v SHL 1) | (from_borders ? 0 : (v & NTH_BIT(end-1)));
        *q = v & vl & vr & *u & *d;
        return;
    }

    v = *p;
    vl = (v SHR 1) | (from_borders ? 0 : (v & NTH_BIT(0)));
    vr = (v SHL 1) | (*(p+1) SHR 0x1f);
    *q = v & vl & vr & *u & *d;
    q++, d++, p++, u++;

    for (guint j = 1; j < len; j++, p++, q++, u++, d++) {
        v = *p;
        vl = (v SHR 1) | (*(p-1) SHL 0x1f);
        vr = (v SHL 1) | (*(p+1) SHR 0x1f);
        *q = v & vl & vr & *u & *d;
    }

    v = *p & m0;
    vl = (v SHR 1) | (*(p-1) SHL 0x1f);
    vr = (v SHL 1) | (from_borders ? 0 : (v & NTH_BIT(end-1)));
    *q = v & vl & vr & *u & *d;
}

/**
 * gwy_mask_field_shrink:
 * @field: A two-dimensional mask field.
 * @from_borders: %TRUE to shrink grains from field borders.
 *
 * Shrinks grains in a mask field by one pixel from all four directions.
 **/
void
gwy_mask_field_shrink(GwyMaskField *field,
                      gboolean from_borders)
{
    g_return_if_fail(GWY_IS_MASK_FIELD(field));

    guint stride = field->stride;
    guint rowsize = stride * sizeof(guint32);
    if (from_borders && field->yres <= 2) {
        memset(field->data, 0x00, rowsize * field->yres);
        gwy_mask_field_invalidate(field);
        return;
    }

    const guint end = (field->xres & 0x1f) ? field->xres & 0x1f : 0x20;
    const guint32 m0 = MAKE_MASK(0, end);
    const guint len = (field->xres >> 5) - (end == 0x20 ? 1 : 0);

    if (field->yres == 1) {
        guint32 *row = g_slice_alloc(rowsize);
        memcpy(row, field->data, rowsize);
        shrink_row(row, row, row, m0, len, end, from_borders, field->data);
        g_slice_free1(rowsize, row);
        gwy_mask_field_invalidate(field);
        return;
    }

    guint32 *prev = g_slice_alloc(rowsize);
    guint32 *row = g_slice_alloc(rowsize);

    memcpy(prev, field->data, rowsize);
    if (from_borders)
        memset(field->data, 0x00, rowsize);
    else {
        guint32 *q = field->data;
        guint32 *next = q + stride;
        shrink_row(prev, prev, next, m0, len, end, from_borders, q);
    }

    for (guint i = 1; i+1 < field->yres; i++) {
        guint32 *q = field->data + i*stride;
        guint32 *next = q + stride;
        memcpy(row, q, rowsize);
        shrink_row(prev, row, next, m0, len, end, from_borders, q);
        GWY_SWAP(guint32*, prev, row);
    }

    if (from_borders)
        memset(field->data + (field->yres - 1)*stride, 0x00, rowsize);
    else {
        guint32 *q = field->data + (field->yres - 1)*stride;
        memcpy(row, q, rowsize);
        shrink_row(prev, row, row, m0, len, end, from_borders, q);
    }

    g_slice_free1(rowsize, row);
    g_slice_free1(rowsize, prev);
    gwy_mask_field_invalidate(field);
}

static void
grow_row(const guint32 *u,
         const guint32 *p,
         const guint32 *d,
         guint32 m0,
         guint len,
         guint end,
         guint32 *q)
{
    guint32 v, vl, vr;

    if (!len) {
        v = *p & m0;
        vl = v SHR 1;
        vr = v SHL 1;
        *q = v | vl | vr | *u | *d;
        return;
    }

    v = *p;
    vl = v SHR 1;
    vr = (v SHL 1) | (*(p+1) SHR 0x1f);
    *q = v | vl | vr | *u | *d;
    q++, d++, p++, u++;

    for (guint j = 1; j < len; j++, p++, q++, u++, d++) {
        v = *p;
        vl = (v SHR 1) | (*(p-1) SHL 0x1f);
        vr = (v SHL 1) | (*(p+1) SHR 0x1f);
        *q = v | vl | vr | *u | *d;
    }

    if (!end)
        return;

    v = *p & m0;
    vl = (v SHR 1) | (*(p-1) SHL 0x1f);
    vr = v SHL 1;
    *q = v | vl | vr | *u | *d;
}

static void
grow_field(GwyMaskField *field)
{
    guint stride = field->stride;
    guint rowsize = stride * sizeof(guint32);

    const guint end = field->xres & 0x1f;
    const guint32 m0 = MAKE_MASK(0, end);
    const guint len = field->xres >> 5;

    if (field->yres == 1) {
        guint32 *row = g_slice_alloc(rowsize);
        memcpy(row, field->data, rowsize);
        grow_row(row, row, row, m0, len, end, field->data);
        g_slice_free1(rowsize, row);
        return;
    }

    guint32 *prev = g_slice_alloc(rowsize);
    guint32 *row = g_slice_alloc(rowsize);
    guint32 *q, *next;

    q = field->data;
    memcpy(prev, q, rowsize);
    next = q + stride;
    grow_row(prev, prev, next, m0, len, end, q);

    for (guint i = 1; i+1 < field->yres; i++) {
        q = field->data + i*stride;
        next = q + stride;
        memcpy(row, q, rowsize);
        grow_row(prev, row, next, m0, len, end, q);
        GWY_SWAP(guint32*, prev, row);
    }

    q = field->data + (field->yres - 1)*stride;
    memcpy(row, q, rowsize);
    grow_row(prev, row, row, m0, len, end, q);

    g_slice_free1(rowsize, row);
    g_slice_free1(rowsize, prev);
}

static void
prevent_grain_merging(GwyMaskField *field)
{
    /* We know in the grains did not touch before the growth step.
     * Therefore, we examine pixels that are in the mask now but were not
     * in the last iteration, i.e. grains[k] == 0 but data[k] is set */
    guint xres = field->xres, yres = field->yres;
    guint *grains = field->priv->grains;
    for (guint i = 0; i < yres; i++) {
        GwyMaskIter iter;
        gwy_mask_field_iter_init(field, iter, 0, i);
        for (guint j = 0; j < xres; j++) {
            guint k = i*xres + j;
            if (!grains[k] && gwy_mask_iter_get(iter)) {
                guint g1 = i          ? grains[k-xres] : 0;
                guint g2 = j          ? grains[k-1]    : 0;
                guint g3 = j+1 < xres ? grains[k+1]    : 0;
                guint g4 = i+1 < yres ? grains[k+xres] : 0;
                /* If all nonzero grain numbers are equal they are also equal
                 * to this value. */
                guint gno = g1 | g2 | g3 | g4;
                if ((!g1 || g1 == gno)
                    && (!g2 || g2 == gno)
                    && (!g3 || g3 == gno)
                    && (!g4 || g4 == gno))
                    grains[k] = gno;
                else {
                    /* Now we have a conflict and it has to be resolved.
                     * We just get rid of this pixel. */
                    gwy_mask_iter_set(iter, FALSE);
                }
            }
            gwy_mask_iter_next(iter);
        }
    }
}

/**
 * gwy_mask_field_grow:
 * @field: A two-dimensional mask field.
 * @separate_grains: %TRUE to prevent merging of grains, their growth stops
 *                   if they should touch another grain.
 *
 * Grows grains in a mask field by one pixel from all four directions.
 **/
void
gwy_mask_field_grow(GwyMaskField *field,
                    gboolean separate_grains)
{
    g_return_if_fail(GWY_IS_MASK_FIELD(field));
    // Separated grain growth needs numbered grains but it also updates the
    // numbers to the valid state again.
    if (separate_grains)
        gwy_mask_field_grain_numbers(field, NULL);
    grow_field(field);
    if (separate_grains) {
        prevent_grain_merging(field);
        // Preserve numbered grains.
        guint *grains = field->priv->grains;
        field->priv->grains = NULL;
        gwy_mask_field_invalidate(field);
        field->priv->grains = grains;
    }
    else
        gwy_mask_field_invalidate(field);
}

/**
 * SECTION: mask-field-arithmetic
 * @section_id: GwyMaskField-arithmetic
 * @title: GwyMaskField arithmetic
 * @short_description: Arithmetic and logical operations with mask fields
 **/

/**
 * GwyLogicalOperator:
 * @GWY_LOGICAL_ZERO: Always zero, mask clearing.
 * @GWY_LOGICAL_AND: Logical conjuction @A ∧ @B, mask intersection.
 * @GWY_LOGICAL_NIMPL: Negated implication @A ⇏ @B, @A ∧ ¬@B, mask
 *                     subtraction.
 * @GWY_LOGICAL_A: First operand @A, no change to the mask.
 * @GWY_LOGICAL_NCIMPL: Negated converse implication @A ⇍ @B, ¬@A ∧ @B.
 * @GWY_LOGICAL_B: Second operand @B, mask copying.
 * @GWY_LOGICAL_XOR: Exclusive disjunction @A ⊻ @B, symmetrical mask
 *                   subtraction.
 * @GWY_LOGICAL_OR: Disjunction @A ∨ @B, mask union.
 * @GWY_LOGICAL_NOR: Negated disjunction @A ⊽ @B.
 * @GWY_LOGICAL_NXOR: Negated exclusive disjunction ¬(@A ⊻ @B).
 * @GWY_LOGICAL_NB: Negated second operand ¬@B, copying of inverted mask.
 * @GWY_LOGICAL_CIMPL: Converse implication @A ⇐ @B, @A ∨ ¬@B.
 * @GWY_LOGICAL_NA: Negated first operand ¬@A, mask inversion.
 * @GWY_LOGICAL_IMPL: Implication @A ⇒ @B, ¬@A ∨ @B.
 * @GWY_LOGICAL_NAND: Negated conjuction @A ⊼ @B.
 * @GWY_LOGICAL_ONE: Always one, mask filling.
 *
 * Logical operators applicable to masks.
 *
 * All possible 16 logical operators are available although some are not as
 * useful as others.  If a common mask operation corresponds to the logical
 * operator it is mentioned in the list.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
