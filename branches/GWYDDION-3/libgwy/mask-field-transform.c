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
#include "libgwy/processing-internal.h"

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
    guint xres = field->xres, yres = field->yres;
    gsize rowsize = field->stride * sizeof(guint32);
    for (guint i = 0; i < yres/2; i++) {
        GwyMaskIter iter;
        gwy_mask_field_iter_init(field, iter, xres-1, yres-1 - i);
        flip_row(iter, buffer, xres);
        gwy_mask_field_iter_init(field, iter, xres-1, i);
        flip_row(iter, field->data + (yres-1 - i)*xres, xres);
        memcpy(field->data + i*xres, buffer, rowsize);
    }
    if (yres % 2) {
        GwyMaskIter iter;
        gwy_mask_field_iter_init(field, iter, xres-1, yres/2);
        flip_row(iter, buffer, xres);
        memcpy(field->data + yres/2*xres, buffer, rowsize);
    }
}

static void
flip_horizontally(GwyMaskField *field,
                  guint32 *buffer)
{
    guint xres = field->xres, yres = field->yres;
    gsize rowsize = field->stride * sizeof(guint32);
    for (guint i = 0; i < yres; i++) {
        GwyMaskIter iter;
        gwy_mask_field_iter_init(field, iter, xres-1, i);
        flip_row(iter, buffer, xres);
        memcpy(field->data + i*xres, buffer, rowsize);
    }
}

static void
flip_vertically(GwyMaskField *field,
                guint32 *buffer)
{
    guint xres = field->xres, yres = field->yres;
    gsize rowsize = field->stride * sizeof(guint32);
    for (guint i = 0; i < yres/2; i++) {
        memcpy(buffer, field->data + (yres-1 - i)*xres, rowsize);
        memcpy(field->data + (yres-1 - i)*xres, field->data + i*xres, rowsize);
        memcpy(field->data + i*xres, buffer, rowsize);
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
              guint32 *dest)
{
    gwy_memclear(dest, 0x20);
    guint32 bit = FIRST_BIT;
    for (guint i = 0; i < 0x20; i++) {
        guint32 v = src[i];
        for (guint32 *d = dest; v; d++, v = v SHL 1) {
            if (v & FIRST_BIT)
                *d |= bit;
        }
        bit = bit SHR 1;
    }
}

// Block sizes are measured in destination, in source, the dims are swapped.
static inline void
swap_block(const guint32 *sb, guint32 *db,
           guint xblocksize, guint yblocksize,
           guint dstride, guint sstride)
{
    guint32 sbuff[0x20], dbuff[0x20];
    const guint32 *s = sb;
    for (guint i = 0; i < xblocksize; i++, s += sstride)
        sbuff[i] = *s;
    swap_xy_32x32(sbuff, dbuff);
    guint32 *d = db;
    for (guint i = 0; i < yblocksize; i++, d += dstride)
        *d = dbuff[i];
}

static void
swap_xy(const GwyMaskField *source,
        GwyMaskField *dest)
{
    guint dxres = dest->xres, dyres = dest->yres;
    guint sstride = source->stride, dstride = dest->stride;
    guint dxmax = dxres >> 5, jend = dxres & 0x1f;
    guint dymax = dyres >> 5, iend = dyres & 0x1f;

    for (guint ib = 0; ib < dymax; ib++) {
        for (guint jb = 0; jb < dxmax; jb++)
            swap_block(source->data + ((jb << 5)*sstride + ib),
                       dest->data + ((ib << 5)*dstride + jb),
                       0x20, 0x20, dstride, sstride);
        if (jend)
            swap_block(source->data + ((dxmax << 5)*sstride + ib),
                       dest->data + ((ib << 5)*dstride + dxmax),
                       jend, 0x20, dstride, sstride);
    }
    if (iend) {
        for (guint jb = 0; jb < dxmax; jb++)
            swap_block(source->data + (jb*dyres + dymax),
                       dest->data + (dymax*dxres + jb),
                       0x20, iend, dstride, sstride);
        if (jend)
            swap_block(source->data + (dxmax*dyres + dymax),
                       dest->data + (dymax*dxres + dxmax),
                       jend, iend, dstride, sstride);
    }
}

/**
 * gwy_mask_field_transpose:
 * @field: A two-dimensional mask field.
 *
 * Transposes a mask field, i.e. field rows become columns and vice versa.
 *
 * Returns: A new two-dimensional mask field.
 **/
GwyMaskField*
gwy_mask_field_transpose(const GwyMaskField *field)
{
    g_return_val_if_fail(GWY_IS_MASK_FIELD(field), NULL);

    GwyMaskField *newfield = gwy_mask_field_new_sized(field->yres, field->xres,
                                                      FALSE);
    swap_xy(field, newfield);
    return newfield;
}

/**
 * gwy_mask_field_rotate_simple:
 * @field: A two-dimensional mask field.
 * @rotation: Rotation amount (it can also be any positive multiple of 90).
 *
 * Rotates a two-dimensional mask field by a multiple by 90 degrees.
 *
 * Returns: A new two-dimensional mask field.
 **/
GwyMaskField*
gwy_mask_field_rotate_simple(const GwyMaskField *field,
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

    GwyMaskField *newfield = gwy_mask_field_transpose(field);

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
 * @title: GwyMaskField transformations
 * @short_description: Geometrical transformations of mask fields
 *
 * Some field transformations are performed in place while others create new
 * fields.  This simply follows which transformations <emphasis>can</emphasis>
 * be performed in place.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
