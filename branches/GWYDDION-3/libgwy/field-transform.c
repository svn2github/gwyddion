/*
 *  $Id$
 *  Copyright (C) 2009-2010 David Nečas (Yeti).
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
#include "libgwy/field-transform.h"
#include "libgwy/mask-field.h"
#include "libgwy/object-internal.h"
#include "libgwy/line-internal.h"
#include "libgwy/field-internal.h"

#define DSWAP(x, y) GWY_SWAP(gdouble, x, y)

// For rotations. The largest value before the performance starts to
// deteriorate on Phenom II; probably after that threshold on older hardware.
//
// The run-time of transpose is 40 to 120% higher compared to duplicate (the
// worst performance occurs for `nicely' sized fields due to cache line
// contention).  A cache-oblivious algorithm stopping at size 8x8 would be
// nice.
enum { BLOCK_SIZE = 64 };

// XXX: The primitives do not emit signals, it's up to the API-level function
// to do that.
static void
flip_both(GwyField *field, gboolean transform_offsets)
{
    guint n = field->xres * field->yres;
    gdouble *d = field->data, *e = d + n-1;
    for (guint i = n/2; i; i--, d++, e--)
        DSWAP(*d, *e);

    if (transform_offsets) {
        field->xoff = -(field->xreal + field->xoff);
        field->yoff = -(field->yreal + field->yoff);
    }
}

static void
flip_horizontally(GwyField *field, gboolean transform_offsets)
{
    guint xres = field->xres, yres = field->yres;
    for (guint i = 0; i < yres; i++) {
        gdouble *d = field->data + i*xres, *e = d + xres-1;
        for (guint j = xres/2; j; j--, d++, e--)
            DSWAP(*d, *e);
    }

    if (transform_offsets)
        field->xoff = -(field->xreal + field->xoff);
}

static void
flip_vertically(GwyField *field, gboolean transform_offsets)
{
    guint xres = field->xres, yres = field->yres;
    for (guint i = 0; i < yres/2; i++) {
        gdouble *d = field->data + i*xres,
                *e = field->data + (yres-1 - i)*xres;
        for (guint j = xres; j; j--, d++, e++)
            DSWAP(*d, *e);
    }

    if (transform_offsets)
        field->yoff = -(field->yreal + field->yoff);
}

/**
 * gwy_field_flip:
 * @field: A two-dimensional data field.
 * @horizontally: %TRUE to flip the field horizontally, i.e. about the vertical
 *                axis.
 * @vertically: %TRUE to flip the field vertically, i.e. about the horizontal
 *              axis.
 * @transform_offsets: %TRUE to transform the field origin offset as if the
 *                     reflections occured around the Cartesian coordinate
 *                     system axes, %FALSE to keep them intact.
 *
 * Flips a two-dimensional data field about either axis.
 **/
void
gwy_field_flip(GwyField *field,
               gboolean horizontally,
               gboolean vertically,
               gboolean transform_offsets)
{
    g_return_if_fail(GWY_IS_FIELD(field));

    if (horizontally && vertically)
        flip_both(field, transform_offsets);
    else if (horizontally)
        flip_horizontally(field, transform_offsets);
    else if (vertically)
        flip_vertically(field, transform_offsets);
    // Cached values do not change

    if (transform_offsets) {
        if (horizontally)
            g_object_notify(G_OBJECT(field), "x-offset");
        if (vertically)
            g_object_notify(G_OBJECT(field), "y-offset");
    }
}

// Block sizes are measured in destination, in source, the dims are swapped.
static inline void
swap_block(const gdouble *sb, gdouble *db,
           guint xblocksize, guint yblocksize,
           guint dxres, guint sxres)
{
    for (guint i = 0; i < yblocksize; i++) {
        const gdouble *s = sb + i;
        gdouble *d = db + i*dxres;
        for (guint j = xblocksize; j; j--, d++, s += sxres)
            *d = *s;
    }
}

// No argument checking.  The source is assumed to be large enough to contain
// all the data we copy to the destination.
static void
swap_xy(const GwyField *source,
        guint col, guint row,
        guint width, guint height,
        GwyField *dest,
        guint destcol, guint destrow)
{
    guint dxres = dest->xres, dyres = dest->yres, sxres = source->xres;
    guint jmax = height/BLOCK_SIZE * BLOCK_SIZE;
    guint imax = width/BLOCK_SIZE * BLOCK_SIZE;
    const gdouble *sbase = source->data + sxres*row + col;
    gdouble *dbase = dest->data + dxres*destrow + destcol;

    for (guint ib = 0; ib < imax; ib += BLOCK_SIZE) {
        for (guint jb = 0; jb < jmax; jb += BLOCK_SIZE)
            swap_block(sbase + (jb*sxres + ib), dbase + (ib*dxres + jb),
                       BLOCK_SIZE, BLOCK_SIZE, dxres, sxres);
        if (jmax != dxres)
            swap_block(sbase + (jmax*sxres + ib), dbase + (ib*dxres + jmax),
                       dxres - jmax, BLOCK_SIZE, dxres, sxres);
    }
    if (imax != dyres) {
        for (guint jb = 0; jb < jmax; jb += BLOCK_SIZE)
            swap_block(sbase + (jb*sxres + imax), dbase + (imax*dxres + jb),
                       BLOCK_SIZE, dyres - imax, dxres, sxres);
        if (jmax != dxres)
            swap_block(sbase + (jmax*sxres + imax),
                       dbase + (imax*dxres + jmax),
                       dxres - jmax, dyres - imax, dxres, sxres);
    }
}

/**
 * gwy_field_new_transposed:
 * @field: A two-dimensional data field.
 * @fpart: (allow-none):
 *         Area in @field to extract.  Pass %NULL to process entire @field.
 * @transform_offsets: %TRUE to set the X and Y offsets of the new field
 *                     using @col, @row and @field offsets.  %FALSE to set
 *                     offsets of the new field to zeroes.
 *
 * Transposes a field, making rows columns and vice versa.
 *
 * The real dimensions and offsets are also transposed (offsets only if
 * requested with @transform_offsets).
 *
 * The transposition is performed by a low cache-miss algorithm.  It may be
 * used to change column-wise operations to row-wise at a relatively low cost
 * and then benefit from the improved memory locality and simplicity of
 * row-wise processing.
 *
 * Returns: (transfer full):
 *          A new two-dimensional data field containing the transposed part
 *          of @field.
 **/
GwyField*
gwy_field_new_transposed(const GwyField *field,
                         const GwyFieldPart *fpart,
                         gboolean transform_offsets)
{
    guint col, row, width, height;
    if (!gwy_field_check_part(field, fpart, &col, &row, &width, &height))
        return NULL;

    GwyField *part = gwy_field_new_sized(height, width, FALSE);
    swap_xy(field, col, row, width, height, part, 0, 0);
    part->xreal = height*gwy_field_dy(field);
    part->yreal = width*gwy_field_dx(field);

    Field *spriv = field->priv, *dpriv = part->priv;
    _gwy_assign_units(&dpriv->unit_xy, spriv->unit_xy);
    _gwy_assign_units(&dpriv->unit_z, spriv->unit_z);
    if (transform_offsets) {
        part->xoff = field->yoff + row*gwy_field_dy(field);
        part->yoff = field->xoff + col*gwy_field_dx(field);
    }
    if (width == field->xres && height == field->yres) {
        gwy_assign(dpriv->cache, spriv->cache, GWY_FIELD_CACHE_SIZE);
        dpriv->cached = spriv->cached;
    }

    return part;
}

/**
 * gwy_field_transpose:
 * @src: Source two-dimensional data field.
 * @srcpart: Area in field @src to transpose.  Pass %NULL to operate on
 *           entire @src.
 * @dest: Destination two-dimensional data field.
 * @destcol: Destination column in @dest.
 * @destrow: Destination row in @dest.
 *
 * Copies data from one field to another, transposing it.
 *
 * The transposed rectangle is defined by @srcpart and it is copied to
 * @dest starting from (@destcol, @destrow).  Its width in the source
 * corresponds to height in the destination and vice versa.
 *
 * There are no limitations on the row and column indices or dimensions.  Only
 * the part of the rectangle that is corresponds to data inside @src and @dest
 * is copied.  This can also mean nothing is copied at all.
 *
 * If @src is equal to @dest the areas may <emphasis>not</emphasis> overlap.
 **/
void
gwy_field_transpose(const GwyField *src,
                    const GwyFieldPart *srcpart,
                    GwyField *dest,
                    guint destcol, guint destrow)
{
    guint col, row, width, height;
    if (!gwy_field_limit_parts(src, srcpart, dest, destcol, destrow,
                               TRUE, &col, &row, &width, &height))
        return;

    swap_xy(src, col, row, width, height, dest, destcol, destrow);
    gwy_field_invalidate(dest);
}

/**
 * gwy_field_new_rotated_simple:
 * @field: A two-dimensional data field.
 * @rotation: Rotation amount (it can also be any positive multiple of 90).
 * @transform_offsets: %TRUE to transform the field origin offset as if the
 *                     rotation occured around the Cartesian coordinate system
 *                     origin, %FALSE to keep them intact (except for swapping
 *                     x and y for odd multiples of 90 degrees).
 *
 * Rotates a two-dimensional data field by a multiple by 90 degrees.
 *
 * The real dimensions and, depending on @transform, the offsets are
 * transformed accordingly.
 *
 * Returns: (transfer full):
 *          A new two-dimensional data field.
 **/
GwyField*
gwy_field_new_rotated_simple(const GwyField *field,
                             GwySimpleRotation rotation,
                             gboolean transform_offsets)
{
    g_return_val_if_fail(GWY_IS_FIELD(field), NULL);
    rotation %= 360;

    if (rotation == GWY_SIMPLE_ROTATE_NONE)
        return gwy_field_duplicate(field);

    if (rotation == GWY_SIMPLE_ROTATE_UPSIDEDOWN) {
        GwyField *newfield = gwy_field_duplicate(field);
        gwy_field_flip(newfield, TRUE, TRUE, transform_offsets);
        return newfield;
    }

    if (rotation != GWY_SIMPLE_ROTATE_CLOCKWISE
        && rotation != GWY_SIMPLE_ROTATE_COUNTERCLOCKWISE) {
        g_critical("Invalid simple rotation amount %u.", rotation);
        return NULL;
    }

    GwyField *newfield = gwy_field_new_transposed(field, NULL, TRUE);
    // The field is new, no need to emit signals.
    if (rotation == GWY_SIMPLE_ROTATE_COUNTERCLOCKWISE)
        flip_vertically(newfield, transform_offsets);
    if (rotation == GWY_SIMPLE_ROTATE_CLOCKWISE)
        flip_horizontally(newfield, transform_offsets);

    return newfield;
}


/**
 * SECTION: field-transform
 * @section_id: GwyField-transform
 * @title: GwyField transformations
 * @short_description: Geometrical transformations of fields
 *
 * Some field transformations are performed in place while others create new
 * fields.  This simply follows which transformations <emphasis>can</emphasis>
 * be performed in place.
 *
 * If the transformation can be performed in place, such as flipping, it is
 * performed in place.  Doing
 * |[
 * GwyField *newfield = gwy_field_duplicate(field);
 * gwy_field_flip(newfield, FALSE, TRUE, FALSE);
 * ]|
 * if you need a copy is reasonably efficient requiring one extra memcpy().
 *
 * On the other hand if the transformation cannot be performed in place and it
 * requires allocation of a data buffer then a new field is created and
 * returned.  You can replace the old field with the new one or, if the object
 * identity is important, use
 * |[
 * GwyField *newfield = gwy_field_new_rotated_simple(field, 90, FALSE);
 * gwy_field_assign(field, newfield);
 * g_object_unref(newfield);
 * ]|
 * which again costs one extra memcpy().
 *
 * The most low-level and performance-critical operation is transposition as it
 * does not permit a strictly linear memory access (rotations by odd multiples
 * of 90 degrees are essentially transpositions of combined with flipping).
 * An efficient implementation (which the Gwyddion's is) can be used to convert
 * otherwise slow column-wise operations on fields to row-wise operations.
 *
 * Several transposition functions are available, the simplest is
 * gwy_field_new_transposed() that creates a new field as the transposition of
 * another field or its rectangular part.
 *
 * If the columns can be processed separately or their interrelation is simple
 * you can avoid allocating entire transposed rectangular part and work by
 * blocks using gwy_field_part_transpose():
 * |[
 * enum { block_size = 64 };
 * GwyField *workspace = gwy_field_sized_new(height, block_size, FALSE);
 * guint i, remainder;
 *
 * for (i = 0; i < width/block_size; i++) {
 *     GwyFieldPart fpart = {
 *         col + i*block_size, row, block_size, height
 *     };
 *     gwy_field_part_transpose(field, &fpart, workspace, 0, 0);
 *     // Process block_size rows in workspace row-wise fashion, possibly put
 *     // back the processed data with another gwy_field_part_transpose.
 * }
 * remainder = width % block_size;
 * if (remainder) {
 *     GwyFieldPart fpart = {
 *         col + width/block_size*block_size, row, remainder, height
 *     };
 *     gwy_field_part_transpose(field, &fpart, workspace, 0, 0);
 *     // Process block_size rows in workspace row-wise fashion, possibly put
 *     // back the processed data with another gwy_field_part_transpose.
 * }
 * g_object_unref(workspace);
 * ]|
 **/

/**
 * GwySimpleRotation:
 * @GWY_SIMPLE_ROTATE_NONE: No rotation.
 * @GWY_SIMPLE_ROTATE_COUNTERCLOCKWISE: Rotate by 90 degrees counterclockwise.
 * @GWY_SIMPLE_ROTATE_UPSIDEDOWN: Rotate by 180 degrees (the same as flipping
 *                                in both directions).
 * @GWY_SIMPLE_ROTATE_CLOCKWISE: Rotate by 90 degrees clockwise.
 *
 * Rotations by multiples of 90 degrees, i.e. not requiring interpolation.
 *
 * They are used for instance in gwy_field_new_rotated_simple() and
 * gwy_mask_field_new_rotated_simple().  The numerical values are equal to the
 * rotation angle in degrees.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
