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
#include "libgwy/field-transform.h"
#include "libgwy/libgwy-aliases.h"
#include "libgwy/math-internal.h"
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

// The source is assumed to be large enough to contain all the data we copy
// to the destination.
static void
swap_xy(const GwyField *source, guint col, guint row,
        GwyField *dest)
{
    guint dxres = dest->xres, dyres = dest->yres, sxres = source->xres;
    guint dxmax = dxres/BLOCK_SIZE * BLOCK_SIZE;
    guint dymax = dyres/BLOCK_SIZE * BLOCK_SIZE;
    const gdouble *sbase = source->data + sxres*row + col;
    gdouble *dbase = dest->data;

    for (guint ib = 0; ib < dymax; ib += BLOCK_SIZE) {
        for (guint jb = 0; jb < dxmax; jb += BLOCK_SIZE)
            swap_block(sbase + (jb*sxres + ib), dbase + (ib*dxres + jb),
                       BLOCK_SIZE, BLOCK_SIZE, dxres, sxres);
        if (dxmax != dxres)
            swap_block(sbase + (dxmax*sxres + ib), dbase + (ib*dxres + dxmax),
                       dxres - dxmax, BLOCK_SIZE, dxres, sxres);
    }
    if (dymax != dyres) {
        for (guint jb = 0; jb < dxmax; jb += BLOCK_SIZE)
            swap_block(sbase + (jb*sxres + dymax), dbase + (dymax*dxres + jb),
                       BLOCK_SIZE, dyres - dymax, dxres, sxres);
        if (dxmax != dxres)
            swap_block(sbase + (dxmax*sxres + dymax),
                       dbase + (dymax*dxres + dxmax),
                       dxres - dxmax, dyres - dymax, dxres, sxres);
    }
}

/**
 * gwy_field_transpose:
 * @field: A two-dimensional data field.
 * @col: Column index of the upper-left corner of the rectangle.
 * @row: Row index of the upper-left corner of the rectangle.
 * @width: Rectangle width (number of columns).
 * @height: Rectangle height (number of rows).
 * @keep_offsets: %TRUE to set the X and Y offsets of the new field
 *                using @col, @row and @field offsets.  %FALSE to set offsets
 *                of the new field to zeroes.
 *
 * Transposes a rectangular part of a field, making rows columns and vice
 * versa.
 *
 * The real dimensions and offsets are also transposed (offsets only if
 * requested with @keep_offsets).
 *
 * Returns: A new two-dimensional data field containing the transposed part
 *          of @field.
 **/
GwyField*
gwy_field_part_transpose(const GwyField *field,
                         guint col, guint row,
                         guint width, guint height,
                         gboolean transform_offsets)
{
    g_return_val_if_fail(GWY_IS_FIELD(field), NULL);
    g_return_val_if_fail(col + width <= field->xres, NULL);
    g_return_val_if_fail(row + height <= field->yres, NULL);

    GwyField *newfield = gwy_field_new_sized(height, width, FALSE);
    newfield->xreal = height*gwy_field_dy(field);
    newfield->yreal = width*gwy_field_dx(field);
    if (transform_offsets) {
        newfield->xoff = field->yoff + row*gwy_field_dy(field);
        newfield->yoff = field->xoff + col*gwy_field_dx(field);
    }

    swap_xy(field, col, row, newfield);

    Field *spriv = field->priv, *dpriv = newfield->priv;
    ASSIGN_UNITS(dpriv->unit_xy, spriv->unit_xy);
    ASSIGN_UNITS(dpriv->unit_z, spriv->unit_z);

    return newfield;
}

/**
 * gwy_field_transpose:
 * @field: A two-dimensional data field.
 *
 * Transposes a field, i.e. field rows become columns and vice versa.
 *
 * The transposition is performed by a low cache-miss algorithm.  It may be
 * used to change column-wise operations to row-wise at a relatively low cost
 * and then benefit from the improved memory locality and simplicity of
 * row-wise processing.
 *
 * The real dimensions and offsets are also transposed.
 *
 * Returns: A new two-dimensional data field.
 **/
GwyField*
gwy_field_transpose(const GwyField *field)
{
    g_return_val_if_fail(GWY_IS_FIELD(field), NULL);

    GwyField *newfield = gwy_field_new_alike(field, FALSE);
    // The field is new, no need to emit signals.
    GWY_SWAP(guint, newfield->xres, newfield->yres);
    DSWAP(newfield->xreal, newfield->yreal);
    DSWAP(newfield->xoff, newfield->yoff);

    swap_xy(field, 0, 0, newfield);

    Field *spriv = field->priv, *dpriv = newfield->priv;
    ASSIGN_UNITS(dpriv->unit_xy, spriv->unit_xy);
    ASSIGN_UNITS(dpriv->unit_z, spriv->unit_z);
    ASSIGN(dpriv->cache, spriv->cache, GWY_FIELD_CACHE_SIZE);
    dpriv->cached = spriv->cached;

    return newfield;
}

/**
 * gwy_field_rotate_simple:
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
 * Returns: A new two-dimensional data field.
 **/
GwyField*
gwy_field_rotate_simple(const GwyField *field,
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

    GwyField *newfield = gwy_field_transpose(field);
    // The field is new, no need to emit signals.
    if (rotation == GWY_SIMPLE_ROTATE_COUNTERCLOCKWISE)
        flip_vertically(newfield, transform_offsets);
    if (rotation == GWY_SIMPLE_ROTATE_CLOCKWISE)
        flip_horizontally(newfield, transform_offsets);

    return newfield;
}

#define __LIBGWY_FIELD_TRANSFORM_C__
#include "libgwy/libgwy-aliases.c"

/**
 * SECTION: field-transform
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
 * GwyField *newfield = gwy_field_rotate(field, 90, FALSE);
 * gwy_field_assign(field, newfield);
 * g_object_unref(newfield);
 * ]|
 * which again costs one extra memcpy().
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
 * They are used for instance in gwy_field_rotate_simple() and
 * gwy_mask_field_rotate_simple().  The numerical values are equal to the
 * rotation angle in degrees.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
