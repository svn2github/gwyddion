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

// The primitives perform no argument checking and no invalidation or signal
// emission.  It's up to the caller to get that right.
static void
mirror_centrally_in_place(GwyField *field,
                          guint col, guint row,
                          guint width, guint height)
{
    guint xres = field->xres;
    gdouble *base = field->data + xres*row + col;
    for (guint i = 0; i < height/2; i++) {
        gdouble *d = base + i*xres, *s = base + (height-1 - i)*xres + width-1;
        for (guint j = width; j; j--, s--, d++)
            DSWAP(*d, *s);
    }
    if (height % 2 == 0) {
        gdouble *d = base + (height/2)*xres, *s = d + width-1;
        for (guint j = width/2; j; j--, s--, d++)
            DSWAP(*d, *s);
    }
}

static void
mirror_horizontally_in_place(GwyField *field,
                             guint col, guint row,
                             guint width, guint height)
{
    guint xres = field->xres;
    gdouble *base = field->data + xres*row + col;
    for (guint i = 0; i < height; i++) {
        gdouble *d = base + i*xres, *s = d + width-1;
        for (guint j = width/2; j; j--, s--, d++)
            DSWAP(*d, *s);
    }
}

static void
mirror_vertically_in_place(GwyField *field,
                           guint col, guint row,
                           guint width, guint height)
{
    guint xres = field->xres;
    gdouble *base = field->data + xres*row + col;
    for (guint i = 0; i < height/2; i++) {
        gdouble *d = base + i*xres, *s = base + (height-1 - i)*xres;
        for (guint j = width; j; j--, s++, d++)
            DSWAP(*d, *s);
    }
}

static void
mirror_centrally_to(const GwyField *source,
                    guint col, guint row,
                    guint width, guint height,
                    GwyField *dest,
                    guint destcol, guint destrow)
{
    guint dxres = dest->xres, sxres = source->xres;
    const gdouble *sbase = source->data + sxres*row + col;
    gdouble *dbase = dest->data + dxres*destrow + destcol;

    for (guint i = 0; i < height; i++) {
        const gdouble *s = sbase + i*sxres + width-1;
        gdouble *d = dbase + (height-1 - i)*dxres;
        for (guint j = width; j; j--, s--, d++)
            *d = *s;
    }
}

static void
mirror_horizontally_to(const GwyField *source,
                       guint col, guint row,
                       guint width, guint height,
                       GwyField *dest,
                       guint destcol, guint destrow)
{
    guint dxres = dest->xres, sxres = source->xres;
    const gdouble *sbase = source->data + sxres*row + col;
    gdouble *dbase = dest->data + dxres*destrow + destcol;

    for (guint i = 0; i < height; i++) {
        const gdouble *s = sbase + i*sxres + width-1;
        gdouble *d = dbase + i*dxres;
        for (guint j = width; j; j--, s--, d++)
            *d = *s;
    }
}

static void
mirror_vertically_to(const GwyField *source,
                     guint col, guint row,
                     guint width, guint height,
                     GwyField *dest,
                     guint destcol, guint destrow)
{
    guint dxres = dest->xres, sxres = source->xres;
    const gdouble *sbase = source->data + sxres*row + col;
    gdouble *dbase = dest->data + dxres*destrow + destcol;

    for (guint i = 0; i < height; i++)
        gwy_assign(dbase + (height-1 - i)*dxres, sbase + i*sxres, width);
}

static void
copy_to(const GwyField *source,
        guint col, guint row,
        guint width, guint height,
        GwyField *dest,
        guint destcol, guint destrow)
{
    guint dxres = dest->xres, sxres = source->xres;
    const gdouble *sbase = source->data + sxres*row + col;
    gdouble *dbase = dest->data + dxres*destrow + destcol;

    if (width == source->xres && width == dest->xres)
        gwy_assign(dbase, sbase, width*height);
    else {
        for (guint i = 0; i < height; i++)
            gwy_assign(dbase + i*dxres, sbase + i*sxres, width);
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

static void
transpose_to(const GwyField *source,
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

static void
transform_congruent_to(const GwyField *source,
                       guint col, guint row,
                       guint width, guint height,
                       GwyField *dest,
                       guint destcol, guint destrow,
                       GwyPlaneCongruenceType transformation)
{
    if (transformation == GWY_PLANE_IDENTITY)
        copy_to(source, col, row, width, height, dest, destcol, destrow);
    else if (transformation == GWY_PLANE_MIRROR_HORIZONTALLY)
        mirror_horizontally_to(source, col, row, width, height,
                               dest, destcol, destrow);
    else if (transformation == GWY_PLANE_MIRROR_VERTICALLY)
        mirror_vertically_to(source, col, row, width, height,
                             dest, destcol, destrow);
    else if (transformation == GWY_PLANE_MIRROR_BOTH)
        mirror_centrally_to(source, col, row, width, height,
                            dest, destcol, destrow);
    else if (transformation == GWY_PLANE_MIRROR_DIAGONALLY)
        transpose_to(source, col, row, width, height, dest, destcol, destrow);
    else if (transformation == GWY_PLANE_MIRROR_ANTIDIAGONALLY) {
        transpose_to(source, col, row, width, height, dest, destcol, destrow);
        mirror_centrally_in_place(dest, 0, 0, height, width);
    }
    else if (transformation == GWY_PLANE_ROTATE_CLOCKWISE) {
        transpose_to(source, col, row, width, height, dest, destcol, destrow);
        mirror_horizontally_in_place(dest, 0, 0, height, width);
    }
    else if (transformation == GWY_PLANE_ROTATE_COUNTERCLOCKWISE) {
        transpose_to(source, col, row, width, height, dest, destcol, destrow);
        mirror_vertically_in_place(dest, 0, 0, height, width);
    }
    else {
        g_assert_not_reached();
    }
}

/*
 * Simple operations:
 * in-place horizontal mirroring
 * in-place vertical mirroring
 * in-place central mirroring
 * out-of-place transposition
 * out-of-place horizontal mirroring
 * out-of-place vertical mirroring
 * out-of-place central mirroring
 * copy
 *
 * The plan (indirect in-place congruences require temporary buffers):
 * in-place identity = no-op
 * in-place mirror horizontally = in-place horizontal mirroring
 * in-place mirror vertically = in-place vertical mirroring
 * in-place mirror diagonally = out-of-place transposition, copy
 *                              (temporary buffer)
 * in-place mirror antidiagonally = out-of-place transposition, out-of-place
 *                                  central mirroring (temporary buffer)
 * in-place mirror both = in-place central mirroring
 * in-place rotate clockwise = out-of-place transposition, out-of-place
 *                             horizontal mirroring (temporary buffer)
 * in-place rotate counterclockwise = out-of-place transposition, out-of-place
 *                                    verical mirroring (temporary buffer)
 * out-of-place identity = copy
 * out-of-place mirror horizontally = out-of-place horizontal mirroring
 * out-of-place mirror vertically = out-of-place vertical mirroring
 * out-of-place mirror diagonally = out-of-place transposition
 * out-of-place mirror antidiagonally = out-of-place transposition, in-place
 *                                      central mirroring
 * out-of-place mirror both = out-of-place central mirroring
 * out-of-place rotate clockwise = out-of-place transposition, in-place
 *                                 horizontal mirroring
 * out-of-place rotate counterclockwise = out-of-place transposition, in-place
 *                                    verical mirroring
 */

/**
 * gwy_plane_congruence_is_transposition:
 * @transformation: Type of plane congruence transformation.
 *
 * Tells whether a plane congruence transform is transposition.
 *
 * If the transformation is transposition it means @x and @y are exchanged.
 * Non-transpositions keep @x as @x and @y as @y.
 *
 * Returns: %TRUE if @transformation is transposing.
 **/
gboolean
gwy_plane_congruence_is_transposition(GwyPlaneCongruenceType transformation)
{
    g_return_val_if_fail(transformation <= GWY_PLANE_ROTATE_COUNTERCLOCKWISE,
                         FALSE);
    return (transformation == GWY_PLANE_MIRROR_DIAGONALLY
            || transformation == GWY_PLANE_MIRROR_ANTIDIAGONALLY
            || transformation == GWY_PLANE_ROTATE_CLOCKWISE
            || transformation == GWY_PLANE_ROTATE_COUNTERCLOCKWISE);
}

/**
 * gwy_field_transform_congruent:
 * @field: A two-dimensional data field.
 * @transformation: Type of plane congruence transformation.
 *
 * Performs an in-place congruence transformation of a two-dimensional field.
 *
 * Transposing transformations (see gwy_plane_congruence_is_transposition()
 * for description) are not performed in-place directly and require temporary
 * buffers.  It, therefore, makes sense to perform them using this method only
 * if you have no use for the original data.
 *
 * The @field offsets are not modified.  Generally, you either do not care in
 * which case the offsets were probably zeroes anyway and thus it is all right
 * to keep them so.  Or you may want to reset them with
 * gwy_field_clear_offsets().  Finally, may want to transform the offsets using
 * gwy_field_transform_offsets() with @source and @dest both being @field.
 **/
void
gwy_field_transform_congruent(GwyField *field,
                              GwyPlaneCongruenceType transformation)
{
    g_return_if_fail(GWY_IS_FIELD(field));
    g_return_if_fail(transformation <= GWY_PLANE_ROTATE_COUNTERCLOCKWISE);

    if (!gwy_plane_congruence_is_transposition(transformation)) {
        if (transformation == GWY_PLANE_MIRROR_HORIZONTALLY)
            mirror_horizontally_in_place(field, 0, 0, field->xres, field->yres);
        else if (transformation == GWY_PLANE_MIRROR_VERTICALLY)
            mirror_vertically_in_place(field, 0, 0, field->xres, field->yres);
        else if (transformation == GWY_PLANE_MIRROR_BOTH)
            mirror_centrally_in_place(field, 0, 0, field->xres, field->yres);
        else if (transformation == GWY_PLANE_IDENTITY) {
        }
        else {
            g_assert_not_reached();
        }
        return;
    }

    GwyField *buffer = gwy_field_new_sized(field->yres, field->xres, FALSE);
    transpose_to(field, 0, 0, field->xres, field->yres, buffer, 0, 0);
    GWY_SWAP(guint, field->xres, field->yres);
    GWY_SWAP(gdouble, field->xreal, field->yreal);
    if (transformation == GWY_PLANE_MIRROR_DIAGONALLY)
        copy_to(buffer, 0, 0, field->xres, field->yres, field, 0, 0);
    else if (transformation == GWY_PLANE_MIRROR_ANTIDIAGONALLY)
        mirror_centrally_to(buffer, 0, 0, field->xres, field->yres,
                            field, 0, 0);
    else if (transformation == GWY_PLANE_ROTATE_CLOCKWISE)
        mirror_horizontally_to(buffer, 0, 0, field->xres, field->yres,
                               field, 0, 0);
    else if (transformation == GWY_PLANE_ROTATE_COUNTERCLOCKWISE)
        mirror_vertically_to(buffer, 0, 0, field->xres, field->yres,
                             field, 0, 0);
    else {
        g_assert_not_reached();
    }
    g_object_unref(buffer);

    const gchar *propnames[4];
    guint n = 0;
    if (field->xres != field->yres) {
        propnames[n++] = "x-res";
        propnames[n++] = "y-res";
    }
    if (field->xreal != field->yreal) {
        propnames[n++] = "x-real";
        propnames[n++] = "y-real";
    }
    _gwy_notify_properties(G_OBJECT(field), propnames, n);
}

/**
 * gwy_field_new_congruent:
 * @field: A two-dimensional data field.
 * @fpart: (allow-none):
 *         Area in @field to extract to the new field.  Passing %NULL extracts
 *         entire @field.
 * @transformation: Type of plane congruence transformation.
 *
 * Creates a new two-dimensional data field by performing a congruence
 * transformation of another field.
 *
 * The new field is created with zero offsets.  If you want to transform the
 * offsets use gwy_field_transform_offsets() afterwards.
 *
 * Returns: (transfer full):
 *          A new two-dimensional data field.
 **/
GwyField*
gwy_field_new_congruent(const GwyField *field,
                        const GwyFieldPart *fpart,
                        GwyPlaneCongruenceType transformation)
{
    guint col, row, width, height;
    if (!gwy_field_check_part(field, fpart, &col, &row, &width, &height))
        return NULL;
    g_return_val_if_fail(transformation <= GWY_PLANE_ROTATE_COUNTERCLOCKWISE,
                         NULL);

    gboolean is_trans = gwy_plane_congruence_is_transposition(transformation);
    guint dwidth = is_trans ? height : width;
    guint dheight = is_trans ? width : height;
    GwyField *part = gwy_field_new_sized(dwidth, dheight, FALSE);

    if (is_trans) {
        part->xreal = height*gwy_field_dy(field);
        part->yreal = width*gwy_field_dx(field);
    }
    else {
        part->xreal = width*gwy_field_dx(field);
        part->yreal = height*gwy_field_dy(field);
    }
    transform_congruent_to(field, col, row, width, height, part, 0, 0,
                           transformation);

    return part;
}

static void
transform_offsets(GwyField *dest,
                  gdouble xoff, gdouble yoff,
                  gdouble xend, gdouble yend,
                  GwyPlaneCongruenceType transformation,
                  gdouble xaxis,
                  gdouble yaxis)
{
    /* The general mirroring formula with mirroring axis passing through the
     * point A is X → X' + (A - A') where prime denotes mirroring about a
     * parallel axis passing through the origin.  In addition, we must take
     * into account that the first edge becomes the second edge and thus using
     * the end instead of offset where appropriate.  */
    if (transformation == GWY_PLANE_IDENTITY) {
    }
    else if (transformation == GWY_PLANE_MIRROR_HORIZONTALLY) {
        xoff = 2.0*xaxis - xend;
    }
    else if (transformation == GWY_PLANE_MIRROR_VERTICALLY) {
        yoff = 2.0*yaxis - yend;
    }
    else if (transformation == GWY_PLANE_MIRROR_BOTH) {
        xoff = 2.0*xaxis - xend;
        yoff = 2.0*yaxis - yend;
    }
    else if (transformation == GWY_PLANE_MIRROR_DIAGONALLY) {
        gdouble a = xaxis - yaxis,
                tx = xoff + a,
                ty = yoff - a;
        xoff = tx;
        yoff = ty;
    }
    else if (transformation == GWY_PLANE_MIRROR_ANTIDIAGONALLY) {
        gdouble a = xaxis + yaxis;
        xoff = a - yend;
        yoff = a - xend;
    }
    else if (transformation == GWY_PLANE_ROTATE_CLOCKWISE) {
        yoff = xoff;
        xoff = -yend;
    }
    else if (transformation == GWY_PLANE_ROTATE_COUNTERCLOCKWISE) {
        xoff = yoff;
        yoff = -xend;
    }
    else {
        g_assert_not_reached();
    }

    const gchar *propnames[2];
    guint n = 0;
    if (fabs(xoff - dest->xoff) > 1e-12*dest->xreal) {
        dest->xoff = xoff;
        propnames[n++] = "x-offset";
    }
    if (fabs(yoff - dest->yoff) > 1e-12*dest->yreal) {
        dest->yoff = yoff;
        propnames[n++] = "y-offset";
    }
    _gwy_notify_properties(G_OBJECT(dest), propnames, n);
}

/**
 * gwy_field_transform_offsets:
 * @source: A source two-dimenstional field from which @dest was created using
 *          @transformation.  As a special case, if @source is the same object
 *          as @dest it is assumed the field was transformed in-place and the
 *          offset recalculation is modified accordingly.
 * @srcpart: (allow-none):
 *           Area in @source that was transformed to create @dest.  If
 *           @source is the same object as @dest you should pass %NULL.
 * @dest: Destination two-dimenstional field whose offsets are to be
 *        recalculated.
 * @transformation: Type of plane congruence transformation.
 * @origin: (allow-none):
 *          Rotation centre or the point the mirroring axis passes through.
 *          Passing %NULL means the origin of coordinates (0, 0).
 *
 * Recalculates offsets of a two-dimenstional data field after a congruence
 * transformation.
 **/
void
gwy_field_transform_offsets(const GwyField *source,
                            const GwyFieldPart *srcpart,
                            GwyField *dest,
                            GwyPlaneCongruenceType transformation,
                            const GwyXY *origin)
{
    guint col, row, width, height;
    if (!gwy_field_check_part(source, srcpart, &col, &row, &width, &height))
        return;
    g_return_if_fail(transformation <= GWY_PLANE_ROTATE_COUNTERCLOCKWISE);
    g_return_if_fail(GWY_IS_FIELD(dest));
    gboolean is_trans = gwy_plane_congruence_is_transposition(transformation);
    if (is_trans)
        g_return_if_fail(dest->xres == height && dest->yres == width);
    else
        g_return_if_fail(dest->xres == width && dest->yres == height);

    gdouble xoff = source->xoff + gwy_field_dx(source)*col,
            yoff = source->yoff + gwy_field_dy(source)*row;
    gdouble xend = xoff + dest->xreal,
            yend = yoff + dest->yreal;
    gdouble x = origin ? origin->x : 0.0,
            y = origin ? origin->y : 0.0;

    if ((const GwyField*)dest == source) {
        g_return_if_fail(source->xres == width && source->yres == height);
        if (is_trans) {
            // This looks weird.  Remember the dimensions were swapped but the
            // offsets were kept as-is.
            xoff = source->xoff + gwy_field_dy(source)*row;
            yoff = source->yoff + gwy_field_dx(source)*col;
            xend = xoff + dest->yreal;
            yend = yoff + dest->xreal;
        }
    }

    transform_offsets(dest, xoff, yoff, xend, yend, transformation, x, y);
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
 * GwyPlaneCongruenceType:
 * @GWY_PLANE_IDENTITY: Identity, i.e. no transformation at all.
 * @GWY_PLANE_MIRROR_HORIZONTALLY: Horizontal mirroring, i.e. reflection about
 *                                 a vertical axis.
 * @GWY_PLANE_MIRROR_VERTICALLY: Vertical mirroring, i.e. reflection about a
 *                               horizontal axis.
 * @GWY_PLANE_MIRROR_DIAGONALLY: Reflection about the main diagonal.
 * @GWY_PLANE_MIRROR_ANTIDIAGONALLY: Reflection about the anti-diagonal.
 * @GWY_PLANE_MIRROR_BOTH: Central reflection.
 * @GWY_PLANE_ROTATE_UPSIDE_DOWN: Rotation by π, an alternative name for
 *                                @GWY_PLANE_MIRROR_BOTH.
 * @GWY_PLANE_ROTATE_CLOCKWISE: Clockwise rotation by π/2.
 * @GWY_PLANE_ROTATE_COUNTERCLOCKWISE: Counterclockwise rotation by π/2.
 *
 * Type of plane congruences.
 *
 * More precisely, #GwyPlaneCongruenceType represents transformations that
 * can be performed with pixel fields, leading to precise 1-to-1 pixel-wise
 * mapping.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
