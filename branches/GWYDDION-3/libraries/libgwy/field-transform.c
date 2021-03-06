/*
 *  $Id$
 *  Copyright (C) 2009-2013 David Nečas (Yeti).
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
#include "libgwy/types.h"
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
    if (height % 2 == 1) {
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
    guint dxres = dest->xres, sxres = source->xres;
    guint jmax = height/BLOCK_SIZE * BLOCK_SIZE;
    guint imax = width/BLOCK_SIZE * BLOCK_SIZE;
    const gdouble *sbase = source->data + sxres*row + col;
    gdouble *dbase = dest->data + dxres*destrow + destcol;

    for (guint ib = 0; ib < imax; ib += BLOCK_SIZE) {
        for (guint jb = 0; jb < jmax; jb += BLOCK_SIZE)
            swap_block(sbase + (jb*sxres + ib), dbase + (ib*dxres + jb),
                       BLOCK_SIZE, BLOCK_SIZE, dxres, sxres);
        if (jmax != height)
            swap_block(sbase + (jmax*sxres + ib), dbase + (ib*dxres + jmax),
                       height - jmax, BLOCK_SIZE, dxres, sxres);
    }
    if (imax != width) {
        for (guint jb = 0; jb < jmax; jb += BLOCK_SIZE)
            swap_block(sbase + (jb*sxres + imax), dbase + (imax*dxres + jb),
                       BLOCK_SIZE, width - imax, dxres, sxres);
        if (jmax != height)
            swap_block(sbase + (jmax*sxres + imax),
                       dbase + (imax*dxres + jmax),
                       height - jmax, width - imax, dxres, sxres);
    }
}

static void
transform_congruent_to(const GwyField *source,
                       guint col, guint row,
                       guint width, guint height,
                       GwyField *dest,
                       guint destcol, guint destrow,
                       GwyPlaneCongruence transformation)
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
        mirror_centrally_in_place(dest, destcol, destrow, height, width);
    }
    else if (transformation == GWY_PLANE_ROTATE_CLOCKWISE) {
        transpose_to(source, col, row, width, height, dest, destcol, destrow);
        mirror_horizontally_in_place(dest, destcol, destrow, height, width);
    }
    else if (transformation == GWY_PLANE_ROTATE_COUNTERCLOCKWISE) {
        transpose_to(source, col, row, width, height, dest, destcol, destrow);
        mirror_vertically_in_place(dest, destcol, destrow, height, width);
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
gwy_plane_congruence_is_transposition(GwyPlaneCongruence transformation)
{
    g_return_val_if_fail(gwy_plane_congruence_is_valid(transformation), FALSE);
    return (transformation == GWY_PLANE_MIRROR_DIAGONALLY
            || transformation == GWY_PLANE_MIRROR_ANTIDIAGONALLY
            || transformation == GWY_PLANE_ROTATE_CLOCKWISE
            || transformation == GWY_PLANE_ROTATE_COUNTERCLOCKWISE);
}

/**
 * gwy_plane_congruence_invert:
 * @transformation: Type of plane congruence transformation.
 *
 * Finds inverse transformation for a plane congruence transform.
 *
 * Most transformation are involutory, i.e. they are their own inversions, but
 * rotations are not.
 *
 * Returns: The inverse transformation of @transformation.
 **/
GwyPlaneCongruence
gwy_plane_congruence_invert(GwyPlaneCongruence transformation)
{
    g_return_val_if_fail(gwy_plane_congruence_is_valid(transformation),
                         GWY_PLANE_IDENTITY);
    if (transformation == GWY_PLANE_ROTATE_CLOCKWISE)
        return GWY_PLANE_ROTATE_COUNTERCLOCKWISE;
    if (transformation == GWY_PLANE_ROTATE_COUNTERCLOCKWISE)
        return GWY_PLANE_ROTATE_CLOCKWISE;
    return transformation;
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
                              GwyPlaneCongruence transformation)
{
    g_return_if_fail(GWY_IS_FIELD(field));
    g_return_if_fail(gwy_plane_congruence_is_valid(transformation));

    if (!gwy_plane_congruence_is_transposition(transformation)) {
        if (transformation == GWY_PLANE_IDENTITY)
            return;
        if (transformation == GWY_PLANE_MIRROR_HORIZONTALLY)
            mirror_horizontally_in_place(field, 0, 0, field->xres, field->yres);
        else if (transformation == GWY_PLANE_MIRROR_VERTICALLY)
            mirror_vertically_in_place(field, 0, 0, field->xres, field->yres);
        else if (transformation == GWY_PLANE_MIRROR_BOTH)
            mirror_centrally_in_place(field, 0, 0, field->xres, field->yres);
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

    Field *priv = field->priv;
    if (!gwy_unit_equal(priv->xunit, priv->yunit)) {
        if (!priv->xunit)
            priv->xunit = gwy_unit_new();
        if (!priv->yunit)
            priv->yunit = gwy_unit_new();
        gwy_unit_swap(priv->xunit, priv->yunit);
    }
    // In this order any notifications are emitted when everything is already
    // in the final state.
    _gwy_notify_properties(G_OBJECT(field), propnames, n);
}

/**
 * gwy_field_new_congruent: (constructor)
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
                        GwyPlaneCongruence transformation)
{
    guint col, row, width, height;
    if (!gwy_field_check_part(field, fpart, &col, &row, &width, &height))
        return NULL;
    g_return_val_if_fail(gwy_plane_congruence_is_valid(transformation), NULL);

    gboolean is_trans = gwy_plane_congruence_is_transposition(transformation);
    guint dwidth = is_trans ? height : width;
    guint dheight = is_trans ? width : height;
    GwyField *part = gwy_field_new_sized(dwidth, dheight, FALSE);
    const Field *priv = field->priv;
    Field *ppriv = part->priv;

    if (!gwy_unit_is_empty(priv->xunit))
        ppriv->xunit = gwy_unit_duplicate(priv->xunit);
    if (!gwy_unit_is_empty(priv->yunit))
        ppriv->yunit = gwy_unit_duplicate(priv->yunit);
    if (is_trans) {
        part->xreal = height*gwy_field_dy(field);
        part->yreal = width*gwy_field_dx(field);
        GWY_SWAP(GwyUnit*, ppriv->xunit, ppriv->yunit);
    }
    else {
        part->xreal = width*gwy_field_dx(field);
        part->yreal = height*gwy_field_dy(field);
    }
    transform_congruent_to(field, col, row, width, height, part, 0, 0,
                           transformation);

    return part;
}

/**
 * gwy_field_copy_congruent:
 * @src: Source two-dimensional data data field.
 * @srcpart: (allow-none):
 *           Area in field @src to copy.  Pass %NULL to copy entire @src.
 * @dest: Destination two-dimensional data field.
 * @destcol: Destination column in @dest.
 * @destrow: Destination row in @dest.
 * @transformation: Type of plane congruence transformation.
 *
 * Copies data from one field to another, performing a congruence
 * transformation.
 *
 * The copied rectangle is defined by @srcpart; it is first transformed
 * according to @transformation and then copied to @dest starting from
 * (@destcol, @destrow).  More precisely, the result is the same as extracting
 * the entire @srcpart from @src, transforming it according to @transformation
 * and then copying with gwy_field_copy() to the destination.
 * <informalfigure id='GwyField-fig-copy-congruent'>
 *   <mediaobject>
 *     <imageobject>
 *       <imagedata fileref='field-copy-congruent.png' format='PNG'/>
 *     </imageobject>
 *   </mediaobject>
 * </informalfigure>
 *
 * There are no limitations on the row and column indices or dimensions.  Only
 * the part of the rectangle that corresponds to data inside @src and @dest
 * is copied.  This can also mean no data are copied at all.
 *
 * If @src is equal to @dest the areas may <emphasis>not</emphasis> overlap.
 *
 * This most general copy-and-transform function is seldom needed and may be a
 * bit confusing.  Sometimes, however, a calculation is much better performed
 * row-wise but you need to perform it on columns.  Then
 * gwy_field_copy_congruent() comes handy:
 * |[
 * GwyField *workspace = gwy_field_new_congruent(field, &fpart,
 *                                               GWY_PLANE_MIRROR_DIAGONALLY);
 * // Do some row-wise operation with part...
 * GwyFieldPart ppart = { 0, 0, fpart.height, fpart.width };
 * gwy_field_copy_congruent(workspace, &ppart,
 *                          field, fpart.col, fpart.row,
 *                          GWY_PLANE_MIRROR_DIAGONALLY);
 * g_object_unref(workspace);
 * ]|
 **/
void
gwy_field_copy_congruent(const GwyField *src,
                         const GwyFieldPart *srcpart,
                         GwyField *dest,
                         guint destcol,
                         guint destrow,
                         GwyPlaneCongruence transformation)
{
    guint col, row, width, height;
    gboolean is_trans = gwy_plane_congruence_is_transposition(transformation);
    if (!gwy_field_limit_parts(src, srcpart, dest, destcol, destrow,
                               is_trans, &col, &row, &width, &height))
        return;
    g_return_if_fail(gwy_plane_congruence_is_valid(transformation));

    // Now we have good dimensions of the copied area but at a bad position.
    // The result should be the same as first performing the transformation of
    // the entire source area, then limiting the size.  So if, for instance, we
    // mirror horizontally, we need to take the right part not the left when
    // shrinking.  So fix the position according to the good dimensions, then
    // redo the size limitation.
    GwyFieldPart fsrcpart = (srcpart
                             ? *srcpart
                             : (GwyFieldPart){ 0, 0, src->xres, src->yres });
    GwyFieldPart transpart = { col, row, width, height };
    if (transformation == GWY_PLANE_MIRROR_HORIZONTALLY
        || transformation == GWY_PLANE_MIRROR_ANTIDIAGONALLY
        || transformation == GWY_PLANE_ROTATE_COUNTERCLOCKWISE
        || transformation == GWY_PLANE_MIRROR_BOTH) {
        guint right = MIN(src->xres, fsrcpart.col + fsrcpart.width);
        g_assert(right >= transpart.col + transpart.width);
        transpart.col = right - transpart.width;
    }
    if (transformation == GWY_PLANE_MIRROR_VERTICALLY
        || transformation == GWY_PLANE_MIRROR_ANTIDIAGONALLY
        || transformation == GWY_PLANE_ROTATE_CLOCKWISE
        || transformation == GWY_PLANE_MIRROR_BOTH) {
        guint lower = MIN(src->yres, fsrcpart.row + fsrcpart.height);
        g_assert(lower >= transpart.row + transpart.height);
        transpart.row = lower - transpart.height;
    }
    gboolean ok = gwy_field_limit_parts(src, &transpart, dest, destcol, destrow,
                                        is_trans, &col, &row, &width, &height);
    g_assert(ok);

    transform_congruent_to(src, col, row, width, height,
                           dest, destcol, destrow,
                           transformation);
}

static void
transform_offsets(GwyField *dest,
                  gdouble xoff, gdouble yoff,
                  gdouble xend, gdouble yend,
                  GwyPlaneCongruence transformation,
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
                            GwyPlaneCongruence transformation,
                            const GwyXY *origin)
{
    guint col, row, width, height;
    if (!gwy_field_check_part(source, srcpart, &col, &row, &width, &height))
        return;
    g_return_if_fail(gwy_plane_congruence_is_valid(transformation));
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
 * Congruent transformations, i.e. those retaining pixels and not requiring
 * interpolation, can be performed both in-place using
 * gwy_field_transform_congruent() and into a new field using
 * gwy_field_new_congruent().  However, some are efficient only out-of place
 * because they require temporary buffers, while other are more efficient
 * in-place.  The difference is whether the transformation is transposing,
 * as reported by gwy_plane_congruence_is_transposition().  Transposing
 * transformations are not efficient in-place and if the result is going to end
 * up in another field it is usually better to create it right away with
 * gwy_field_new_congruent().
 **/

/**
 * GwyPlaneCongruence:
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
 * More precisely, #GwyPlaneCongruence represents transformations that
 * can be performed with pixel fields, leading to precise 1-to-1 pixel-wise
 * mapping.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
