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

#include "config.h"
#include <string.h>
#include "libgwy/macros.h"
#include "libgwy/math.h"
#include "libgwy/types.h"
#include "libgwy/mask-field-transform.h"
#include "libgwy/line-internal.h"
#include "libgwy/mask-field-internal.h"

/*
 * Valgrind follows the validity of individual bits.  However, it cannot follow
 * them through our twiddling tables.  What it sees as indexing with a value
 * that contains uninitialised padding bits (and thus an uninitialised memory
 * use) in fact only redistributes the bits in a complicatd manner and we
 * subsequently only use the good bits of the result.
 *
 * So apply VALGRIND_MAKE_MEM_DEFINED() where necessary.  At this moment the
 * only such place is swap_xy_32x32() where we simply declare the entire source
 * buffer as defined.
 */
#ifdef HAVE_VALGRIND
#include <valgrind/memcheck.h>
#else
#define VALGRIND_MAKE_MEM_DEFINED(addr, len) /* */
#endif

// Aligned dest is the only case we need.
static void
flip_row_dest_aligned(const guint32 *src,
                      guint32 *dest,
                      guint width)
{
    guint len = (width + 0x1f) >> 5;
    guint end = width & 0x1f;
    guint beg = 0x20 - end;
    // Process src from the end
    src += len - 1;
    if (end) {
        guint32 v0 = *(src--);
        for (guint i = len-1; i; i--, dest++) {
            guint32 v1 = *(src--);
            *dest = swap_bits_32((v0 SHR beg) | (v1 SHL end));
            v0 = v1;
        }
        *dest = swap_bits_32(v0 SHR beg);
    }
    else {
        for (guint i = len; i; i--, src--, dest++)
            *dest = swap_bits_32(*src);
    }
}

static void
flip_both(GwyMaskField *field,
          guint32 *buffer)
{
    guint xres = field->xres, yres = field->yres, stride = field->stride;
    gsize rowsize = stride * sizeof(guint32);
    for (guint i = 0; i < yres/2; i++) {
        guint32 *rowu = field->data + i*stride;
        guint32 *rowd = field->data + (yres-1 - i)*stride;
        flip_row_dest_aligned(rowd, buffer, xres);
        flip_row_dest_aligned(rowu, rowd, xres);
        memcpy(rowu, buffer, rowsize);
    }
    if (yres % 2) {
        guint32 *rowc = field->data + (yres/2)*stride;
        flip_row_dest_aligned(rowc, buffer, xres);
        memcpy(rowc, buffer, rowsize);
    }
}

static void
flip_both_to(const GwyMaskField *source,
             guint row, guint height,
             GwyMaskField *dest)
{
    g_assert(dest->xres == source->xres && dest->yres == height);
    guint xres = dest->xres, stride = dest->stride;
    for (guint i = 0; i < height; i++) {
        const guint32 *rows = source->data + (row + i)*stride;
        guint32 *rowd = dest->data + (height-1 - i)*stride;
        flip_row_dest_aligned(rows, rowd, xres);
    }
}

static void
flip_horizontally(GwyMaskField *field,
                  guint32 *buffer)
{
    guint xres = field->xres, yres = field->yres, stride = field->stride;
    gsize rowsize = stride * sizeof(guint32);
    for (guint i = 0; i < yres; i++) {
        guint32 *rowi = field->data + i*stride;
        flip_row_dest_aligned(rowi, buffer, xres);
        memcpy(rowi, buffer, rowsize);
    }
}

static void
flip_horizontally_to(const GwyMaskField *source,
                     guint row, guint height,
                     GwyMaskField *dest)
{
    g_assert(dest->xres == source->xres && dest->yres == height);
    guint xres = dest->xres, stride = dest->stride;
    for (guint i = 0; i < height; i++) {
        const guint32 *rows = source->data + (row + i)*stride;
        guint32 *rowd = dest->data + i*stride;
        flip_row_dest_aligned(rows, rowd, xres);
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

static void
flip_vertically_to(const GwyMaskField *source,
                   guint row, guint height,
                   GwyMaskField *dest)
{
    g_assert(dest->xres == source->xres && dest->yres == height);
    guint stride = dest->stride;
    gsize rowsize = stride * sizeof(guint32);
    for (guint i = 0; i < height; i++) {
        const guint32 *rows = source->data + (row + i)*stride;
        guint32 *rowd = dest->data + (height-1 - i)*stride;
        memcpy(rowd, rows, rowsize);
    }
}

#define C64 G_GUINT64_CONSTANT

static const guint64 swap_table[0x100] = {  // {{{
#if (G_BYTE_ORDER == G_LITTLE_ENDIAN)
    C64(0x0000000000000000), C64(0x0000000000000001), C64(0x0000000000000100),
    C64(0x0000000000000101), C64(0x0000000000010000), C64(0x0000000000010001),
    C64(0x0000000000010100), C64(0x0000000000010101), C64(0x0000000001000000),
    C64(0x0000000001000001), C64(0x0000000001000100), C64(0x0000000001000101),
    C64(0x0000000001010000), C64(0x0000000001010001), C64(0x0000000001010100),
    C64(0x0000000001010101), C64(0x0000000100000000), C64(0x0000000100000001),
    C64(0x0000000100000100), C64(0x0000000100000101), C64(0x0000000100010000),
    C64(0x0000000100010001), C64(0x0000000100010100), C64(0x0000000100010101),
    C64(0x0000000101000000), C64(0x0000000101000001), C64(0x0000000101000100),
    C64(0x0000000101000101), C64(0x0000000101010000), C64(0x0000000101010001),
    C64(0x0000000101010100), C64(0x0000000101010101), C64(0x0000010000000000),
    C64(0x0000010000000001), C64(0x0000010000000100), C64(0x0000010000000101),
    C64(0x0000010000010000), C64(0x0000010000010001), C64(0x0000010000010100),
    C64(0x0000010000010101), C64(0x0000010001000000), C64(0x0000010001000001),
    C64(0x0000010001000100), C64(0x0000010001000101), C64(0x0000010001010000),
    C64(0x0000010001010001), C64(0x0000010001010100), C64(0x0000010001010101),
    C64(0x0000010100000000), C64(0x0000010100000001), C64(0x0000010100000100),
    C64(0x0000010100000101), C64(0x0000010100010000), C64(0x0000010100010001),
    C64(0x0000010100010100), C64(0x0000010100010101), C64(0x0000010101000000),
    C64(0x0000010101000001), C64(0x0000010101000100), C64(0x0000010101000101),
    C64(0x0000010101010000), C64(0x0000010101010001), C64(0x0000010101010100),
    C64(0x0000010101010101), C64(0x0001000000000000), C64(0x0001000000000001),
    C64(0x0001000000000100), C64(0x0001000000000101), C64(0x0001000000010000),
    C64(0x0001000000010001), C64(0x0001000000010100), C64(0x0001000000010101),
    C64(0x0001000001000000), C64(0x0001000001000001), C64(0x0001000001000100),
    C64(0x0001000001000101), C64(0x0001000001010000), C64(0x0001000001010001),
    C64(0x0001000001010100), C64(0x0001000001010101), C64(0x0001000100000000),
    C64(0x0001000100000001), C64(0x0001000100000100), C64(0x0001000100000101),
    C64(0x0001000100010000), C64(0x0001000100010001), C64(0x0001000100010100),
    C64(0x0001000100010101), C64(0x0001000101000000), C64(0x0001000101000001),
    C64(0x0001000101000100), C64(0x0001000101000101), C64(0x0001000101010000),
    C64(0x0001000101010001), C64(0x0001000101010100), C64(0x0001000101010101),
    C64(0x0001010000000000), C64(0x0001010000000001), C64(0x0001010000000100),
    C64(0x0001010000000101), C64(0x0001010000010000), C64(0x0001010000010001),
    C64(0x0001010000010100), C64(0x0001010000010101), C64(0x0001010001000000),
    C64(0x0001010001000001), C64(0x0001010001000100), C64(0x0001010001000101),
    C64(0x0001010001010000), C64(0x0001010001010001), C64(0x0001010001010100),
    C64(0x0001010001010101), C64(0x0001010100000000), C64(0x0001010100000001),
    C64(0x0001010100000100), C64(0x0001010100000101), C64(0x0001010100010000),
    C64(0x0001010100010001), C64(0x0001010100010100), C64(0x0001010100010101),
    C64(0x0001010101000000), C64(0x0001010101000001), C64(0x0001010101000100),
    C64(0x0001010101000101), C64(0x0001010101010000), C64(0x0001010101010001),
    C64(0x0001010101010100), C64(0x0001010101010101), C64(0x0100000000000000),
    C64(0x0100000000000001), C64(0x0100000000000100), C64(0x0100000000000101),
    C64(0x0100000000010000), C64(0x0100000000010001), C64(0x0100000000010100),
    C64(0x0100000000010101), C64(0x0100000001000000), C64(0x0100000001000001),
    C64(0x0100000001000100), C64(0x0100000001000101), C64(0x0100000001010000),
    C64(0x0100000001010001), C64(0x0100000001010100), C64(0x0100000001010101),
    C64(0x0100000100000000), C64(0x0100000100000001), C64(0x0100000100000100),
    C64(0x0100000100000101), C64(0x0100000100010000), C64(0x0100000100010001),
    C64(0x0100000100010100), C64(0x0100000100010101), C64(0x0100000101000000),
    C64(0x0100000101000001), C64(0x0100000101000100), C64(0x0100000101000101),
    C64(0x0100000101010000), C64(0x0100000101010001), C64(0x0100000101010100),
    C64(0x0100000101010101), C64(0x0100010000000000), C64(0x0100010000000001),
    C64(0x0100010000000100), C64(0x0100010000000101), C64(0x0100010000010000),
    C64(0x0100010000010001), C64(0x0100010000010100), C64(0x0100010000010101),
    C64(0x0100010001000000), C64(0x0100010001000001), C64(0x0100010001000100),
    C64(0x0100010001000101), C64(0x0100010001010000), C64(0x0100010001010001),
    C64(0x0100010001010100), C64(0x0100010001010101), C64(0x0100010100000000),
    C64(0x0100010100000001), C64(0x0100010100000100), C64(0x0100010100000101),
    C64(0x0100010100010000), C64(0x0100010100010001), C64(0x0100010100010100),
    C64(0x0100010100010101), C64(0x0100010101000000), C64(0x0100010101000001),
    C64(0x0100010101000100), C64(0x0100010101000101), C64(0x0100010101010000),
    C64(0x0100010101010001), C64(0x0100010101010100), C64(0x0100010101010101),
    C64(0x0101000000000000), C64(0x0101000000000001), C64(0x0101000000000100),
    C64(0x0101000000000101), C64(0x0101000000010000), C64(0x0101000000010001),
    C64(0x0101000000010100), C64(0x0101000000010101), C64(0x0101000001000000),
    C64(0x0101000001000001), C64(0x0101000001000100), C64(0x0101000001000101),
    C64(0x0101000001010000), C64(0x0101000001010001), C64(0x0101000001010100),
    C64(0x0101000001010101), C64(0x0101000100000000), C64(0x0101000100000001),
    C64(0x0101000100000100), C64(0x0101000100000101), C64(0x0101000100010000),
    C64(0x0101000100010001), C64(0x0101000100010100), C64(0x0101000100010101),
    C64(0x0101000101000000), C64(0x0101000101000001), C64(0x0101000101000100),
    C64(0x0101000101000101), C64(0x0101000101010000), C64(0x0101000101010001),
    C64(0x0101000101010100), C64(0x0101000101010101), C64(0x0101010000000000),
    C64(0x0101010000000001), C64(0x0101010000000100), C64(0x0101010000000101),
    C64(0x0101010000010000), C64(0x0101010000010001), C64(0x0101010000010100),
    C64(0x0101010000010101), C64(0x0101010001000000), C64(0x0101010001000001),
    C64(0x0101010001000100), C64(0x0101010001000101), C64(0x0101010001010000),
    C64(0x0101010001010001), C64(0x0101010001010100), C64(0x0101010001010101),
    C64(0x0101010100000000), C64(0x0101010100000001), C64(0x0101010100000100),
    C64(0x0101010100000101), C64(0x0101010100010000), C64(0x0101010100010001),
    C64(0x0101010100010100), C64(0x0101010100010101), C64(0x0101010101000000),
    C64(0x0101010101000001), C64(0x0101010101000100), C64(0x0101010101000101),
    C64(0x0101010101010000), C64(0x0101010101010001), C64(0x0101010101010100),
    C64(0x0101010101010101),
#endif
#if (G_BYTE_ORDER == G_BIG_ENDIAN)
    C64(0x0000000000000000), C64(0x0000000000000080), C64(0x0000000000008000),
    C64(0x0000000000008080), C64(0x0000000000800000), C64(0x0000000000800080),
    C64(0x0000000000808000), C64(0x0000000000808080), C64(0x0000000080000000),
    C64(0x0000000080000080), C64(0x0000000080008000), C64(0x0000000080008080),
    C64(0x0000000080800000), C64(0x0000000080800080), C64(0x0000000080808000),
    C64(0x0000000080808080), C64(0x0000008000000000), C64(0x0000008000000080),
    C64(0x0000008000008000), C64(0x0000008000008080), C64(0x0000008000800000),
    C64(0x0000008000800080), C64(0x0000008000808000), C64(0x0000008000808080),
    C64(0x0000008080000000), C64(0x0000008080000080), C64(0x0000008080008000),
    C64(0x0000008080008080), C64(0x0000008080800000), C64(0x0000008080800080),
    C64(0x0000008080808000), C64(0x0000008080808080), C64(0x0000800000000000),
    C64(0x0000800000000080), C64(0x0000800000008000), C64(0x0000800000008080),
    C64(0x0000800000800000), C64(0x0000800000800080), C64(0x0000800000808000),
    C64(0x0000800000808080), C64(0x0000800080000000), C64(0x0000800080000080),
    C64(0x0000800080008000), C64(0x0000800080008080), C64(0x0000800080800000),
    C64(0x0000800080800080), C64(0x0000800080808000), C64(0x0000800080808080),
    C64(0x0000808000000000), C64(0x0000808000000080), C64(0x0000808000008000),
    C64(0x0000808000008080), C64(0x0000808000800000), C64(0x0000808000800080),
    C64(0x0000808000808000), C64(0x0000808000808080), C64(0x0000808080000000),
    C64(0x0000808080000080), C64(0x0000808080008000), C64(0x0000808080008080),
    C64(0x0000808080800000), C64(0x0000808080800080), C64(0x0000808080808000),
    C64(0x0000808080808080), C64(0x0080000000000000), C64(0x0080000000000080),
    C64(0x0080000000008000), C64(0x0080000000008080), C64(0x0080000000800000),
    C64(0x0080000000800080), C64(0x0080000000808000), C64(0x0080000000808080),
    C64(0x0080000080000000), C64(0x0080000080000080), C64(0x0080000080008000),
    C64(0x0080000080008080), C64(0x0080000080800000), C64(0x0080000080800080),
    C64(0x0080000080808000), C64(0x0080000080808080), C64(0x0080008000000000),
    C64(0x0080008000000080), C64(0x0080008000008000), C64(0x0080008000008080),
    C64(0x0080008000800000), C64(0x0080008000800080), C64(0x0080008000808000),
    C64(0x0080008000808080), C64(0x0080008080000000), C64(0x0080008080000080),
    C64(0x0080008080008000), C64(0x0080008080008080), C64(0x0080008080800000),
    C64(0x0080008080800080), C64(0x0080008080808000), C64(0x0080008080808080),
    C64(0x0080800000000000), C64(0x0080800000000080), C64(0x0080800000008000),
    C64(0x0080800000008080), C64(0x0080800000800000), C64(0x0080800000800080),
    C64(0x0080800000808000), C64(0x0080800000808080), C64(0x0080800080000000),
    C64(0x0080800080000080), C64(0x0080800080008000), C64(0x0080800080008080),
    C64(0x0080800080800000), C64(0x0080800080800080), C64(0x0080800080808000),
    C64(0x0080800080808080), C64(0x0080808000000000), C64(0x0080808000000080),
    C64(0x0080808000008000), C64(0x0080808000008080), C64(0x0080808000800000),
    C64(0x0080808000800080), C64(0x0080808000808000), C64(0x0080808000808080),
    C64(0x0080808080000000), C64(0x0080808080000080), C64(0x0080808080008000),
    C64(0x0080808080008080), C64(0x0080808080800000), C64(0x0080808080800080),
    C64(0x0080808080808000), C64(0x0080808080808080), C64(0x8000000000000000),
    C64(0x8000000000000080), C64(0x8000000000008000), C64(0x8000000000008080),
    C64(0x8000000000800000), C64(0x8000000000800080), C64(0x8000000000808000),
    C64(0x8000000000808080), C64(0x8000000080000000), C64(0x8000000080000080),
    C64(0x8000000080008000), C64(0x8000000080008080), C64(0x8000000080800000),
    C64(0x8000000080800080), C64(0x8000000080808000), C64(0x8000000080808080),
    C64(0x8000008000000000), C64(0x8000008000000080), C64(0x8000008000008000),
    C64(0x8000008000008080), C64(0x8000008000800000), C64(0x8000008000800080),
    C64(0x8000008000808000), C64(0x8000008000808080), C64(0x8000008080000000),
    C64(0x8000008080000080), C64(0x8000008080008000), C64(0x8000008080008080),
    C64(0x8000008080800000), C64(0x8000008080800080), C64(0x8000008080808000),
    C64(0x8000008080808080), C64(0x8000800000000000), C64(0x8000800000000080),
    C64(0x8000800000008000), C64(0x8000800000008080), C64(0x8000800000800000),
    C64(0x8000800000800080), C64(0x8000800000808000), C64(0x8000800000808080),
    C64(0x8000800080000000), C64(0x8000800080000080), C64(0x8000800080008000),
    C64(0x8000800080008080), C64(0x8000800080800000), C64(0x8000800080800080),
    C64(0x8000800080808000), C64(0x8000800080808080), C64(0x8000808000000000),
    C64(0x8000808000000080), C64(0x8000808000008000), C64(0x8000808000008080),
    C64(0x8000808000800000), C64(0x8000808000800080), C64(0x8000808000808000),
    C64(0x8000808000808080), C64(0x8000808080000000), C64(0x8000808080000080),
    C64(0x8000808080008000), C64(0x8000808080008080), C64(0x8000808080800000),
    C64(0x8000808080800080), C64(0x8000808080808000), C64(0x8000808080808080),
    C64(0x8080000000000000), C64(0x8080000000000080), C64(0x8080000000008000),
    C64(0x8080000000008080), C64(0x8080000000800000), C64(0x8080000000800080),
    C64(0x8080000000808000), C64(0x8080000000808080), C64(0x8080000080000000),
    C64(0x8080000080000080), C64(0x8080000080008000), C64(0x8080000080008080),
    C64(0x8080000080800000), C64(0x8080000080800080), C64(0x8080000080808000),
    C64(0x8080000080808080), C64(0x8080008000000000), C64(0x8080008000000080),
    C64(0x8080008000008000), C64(0x8080008000008080), C64(0x8080008000800000),
    C64(0x8080008000800080), C64(0x8080008000808000), C64(0x8080008000808080),
    C64(0x8080008080000000), C64(0x8080008080000080), C64(0x8080008080008000),
    C64(0x8080008080008080), C64(0x8080008080800000), C64(0x8080008080800080),
    C64(0x8080008080808000), C64(0x8080008080808080), C64(0x8080800000000000),
    C64(0x8080800000000080), C64(0x8080800000008000), C64(0x8080800000008080),
    C64(0x8080800000800000), C64(0x8080800000800080), C64(0x8080800000808000),
    C64(0x8080800000808080), C64(0x8080800080000000), C64(0x8080800080000080),
    C64(0x8080800080008000), C64(0x8080800080008080), C64(0x8080800080800000),
    C64(0x8080800080800080), C64(0x8080800080808000), C64(0x8080800080808080),
    C64(0x8080808000000000), C64(0x8080808000000080), C64(0x8080808000008000),
    C64(0x8080808000008080), C64(0x8080808000800000), C64(0x8080808000800080),
    C64(0x8080808000808000), C64(0x8080808000808080), C64(0x8080808080000000),
    C64(0x8080808080000080), C64(0x8080808080008000), C64(0x8080808080008080),
    C64(0x8080808080800000), C64(0x8080808080800080), C64(0x8080808080808000),
    C64(0x8080808080808080),
#endif
};  // }}}

#undef C64

// Spread bits of bytes from an 8-byte vertical block to a 64bit integer so
// that the individual bits of an original byte are always 8-bits apart and the
// space between them is occupied by bits of the other bytes.
static inline guint64
gather8(const guint8 *src8)
{
    if (G_BYTE_ORDER == G_LITTLE_ENDIAN)
        return (swap_table[src8[0]] | (swap_table[src8[4]] << 1)
                | (swap_table[src8[8]] << 2) | (swap_table[src8[12]] << 3)
                | (swap_table[src8[16]] << 4) | (swap_table[src8[20]] << 5)
                | (swap_table[src8[24]] << 6) | (swap_table[src8[28]] << 7));
    if (G_BYTE_ORDER == G_BIG_ENDIAN)
        return (swap_table[src8[0]] | (swap_table[src8[4]] >> 1)
                | (swap_table[src8[8]] >> 2) | (swap_table[src8[12]] >> 3)
                | (swap_table[src8[16]] >> 4) | (swap_table[src8[20]] >> 5)
                | (swap_table[src8[24]] >> 6) | (swap_table[src8[28]] >> 7));
    g_return_val_if_reached(0);
}

// Distribute entire bytes of a 64bit integer to an 8-byte vertical block.
static inline void
distribute8(guint64 q,
            guint8 *dest8)
{
    if (G_BYTE_ORDER == G_LITTLE_ENDIAN) {
        for (guint i = 8; i; i--, dest8 += 4, q >>= 8)
            *dest8 = q & 0xff;
    }
    if (G_BYTE_ORDER == G_BIG_ENDIAN) {
        for (guint i = 8; i; i--, dest8 -= 4, q >>= 8)
            *dest8 = q & 0xff;
    }
}

/*
 * Swap by distributing the 64 bits of a vertical 8-byte block (i.e. 8x8 bits)
 * to a guint64 using swap_table[] and then just sorting its bytes to correct
 * places in dest.
 *
 * It probably has a poor performance on 32bit machines, but who gives a damn.
 *
 * The swap table has 2kB which is rather lot but it should fit to the L1 cache
 * on a reasonable CPU.
 *
 * The performace is about 20x better than bit-by-bit transpostion in a 64bit
 * AMD.  Still it consumes about an amortized couple of instructions per bit.
 */
static inline void
swap_xy_32x32(const guint32 *src,
              guint slen,
              guint32 *dest)
{
    VALGRIND_MAKE_MEM_DEFINED(src, 0x20*sizeof(guint32));
    gwy_clear(dest, 0x20);
    const guint8 *src8 = (const guint8*)src;
    guint8 *dest8 = (guint8*)dest;
    slen = (slen + 7)/8;
    for (guint i = 0; i < slen; i++) {
        for (guint j = 0; j < 4; j++) {
            if (G_BYTE_ORDER == G_LITTLE_ENDIAN)
                distribute8(gather8(src8 + (0x20*i + j)),
                            dest8 + (0x20*j + i));
            if (G_BYTE_ORDER == G_BIG_ENDIAN)
                distribute8(gather8(src8 + (0x20*i + j)),
                            dest8 + (0x20*(4 - j) - i - 1));
        }
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
G_GNUC_UNUSED
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
 * gwy_mask_field_transform_congruent:
 * @field: A two-dimensional mask field.
 * @transformation: Type of plane congruence transformation.
 *
 * Performs an in-place congruence transformation of a two-dimensional mask
 * field.
 *
 * Some operations can be performed in-place more efficiently than others.
 * Namely transposing transformations (see
 * gwy_plane_congruence_is_transposition() for description) are require
 * temporary buffers.  It, therefore, makes sense to perform them using this
 * method only if you have no use for the original data.
 **/
void
gwy_mask_field_transform_congruent(GwyMaskField *field,
                                   GwyPlaneCongruence transformation)
{
    g_return_if_fail(GWY_IS_MASK_FIELD(field));
    g_return_if_fail(gwy_plane_congruence_is_valid(transformation));

    if (!gwy_plane_congruence_is_transposition(transformation)) {
        if (transformation == GWY_PLANE_IDENTITY)
            return;

        gsize rowsize = field->stride * sizeof(guint32);
        guint32 *buffer = g_slice_alloc(rowsize);
        if (transformation == GWY_PLANE_MIRROR_HORIZONTALLY)
            flip_horizontally(field, buffer);
        else if (transformation == GWY_PLANE_MIRROR_VERTICALLY)
            flip_vertically(field, buffer);
        else if (transformation == GWY_PLANE_MIRROR_BOTH)
            flip_both(field, buffer);
        else {
            g_assert_not_reached();
        }
        g_slice_free1(rowsize, buffer);
        return;
    }

    GwyMaskField *buffer = gwy_mask_field_new_sized(field->yres, field->xres,
                                                    FALSE);
    swap_xy_both_aligned(field, 0, 0, field->xres, field->yres, buffer, 0, 0);
    // Use gwy_mask_field_set_size() to esnure rowstride is correctly
    // recalculated and property notifications emitted, but delay the emission
    // until we are done.
    g_object_freeze_notify(G_OBJECT(field));
    gwy_mask_field_set_size(field, buffer->xres, buffer->yres, FALSE);
    if (transformation == GWY_PLANE_MIRROR_DIAGONALLY)
        gwy_mask_field_copy(buffer, NULL, field, 0, 0);
    else if (transformation == GWY_PLANE_MIRROR_ANTIDIAGONALLY)
        flip_both_to(buffer, 0, field->yres, field);
    else if (transformation == GWY_PLANE_ROTATE_CLOCKWISE)
        flip_horizontally_to(buffer, 0, field->yres, field);
    else if (transformation == GWY_PLANE_ROTATE_COUNTERCLOCKWISE)
        flip_vertically_to(buffer, 0, field->yres, field);
    else {
        g_assert_not_reached();
    }
    g_object_unref(buffer);
    g_object_thaw_notify(G_OBJECT(field));
}

static void
transform_congruent_to(const GwyMaskField *source,
                       guint col, guint row,
                       guint width, guint height,
                       GwyMaskField *dest,
                       GwyPlaneCongruence transformation)
{
    GwyFieldPart fpart = { col, row, width, height };

    if (transformation == GWY_PLANE_IDENTITY) {
        gwy_mask_field_copy(source, &fpart, dest, 0, 0);
        return;
    }

    if (gwy_plane_congruence_is_transposition(transformation))
        g_assert(dest->xres == height && dest->yres == width);
    else
        g_assert(dest->xres == width && dest->yres == height);

    if (width == source->xres) {
        g_assert(col == 0);
        if (transformation == GWY_PLANE_MIRROR_HORIZONTALLY) {
            flip_horizontally_to(source, row, height, dest);
            return;
        }
        if (transformation == GWY_PLANE_MIRROR_VERTICALLY) {
            flip_vertically_to(source, row, height, dest);
            return;
        }
        if (transformation == GWY_PLANE_MIRROR_BOTH) {
            flip_both_to(source, row, height, dest);
            return;
        }
        if (transformation == GWY_PLANE_MIRROR_DIAGONALLY) {
            swap_xy_both_aligned(source, col, row, width, height, dest, 0, 0);
            return;
        }

        guint rowsize = dest->stride * sizeof(guint32);
        guint32 *buffer = g_slice_alloc(rowsize);
        if (transformation == GWY_PLANE_MIRROR_ANTIDIAGONALLY) {
            swap_xy_both_aligned(source, col, row, width, height, dest, 0, 0);
            flip_both(dest, buffer);
        }
        else if (transformation == GWY_PLANE_ROTATE_CLOCKWISE) {
            swap_xy_both_aligned(source, col, row, width, height, dest, 0, 0);
            flip_horizontally(dest, buffer);
        }
        else if (transformation == GWY_PLANE_ROTATE_COUNTERCLOCKWISE) {
            swap_xy_both_aligned(source, col, row, width, height, dest, 0, 0);
            flip_vertically(dest, buffer);
        }
        else {
            g_assert_not_reached();
        }
        g_slice_free1(rowsize, buffer);
        return;
    }

    // If we do not operate on full rows copy to dest first (possibly
    // transposing) and then mirror in-place to finish the operation.
    if (gwy_plane_congruence_is_transposition(transformation)) {
        if (col & 0x1f)
            swap_xy_dest_aligned(source, col, row, width, height, dest, 0, 0);
        else
            swap_xy_both_aligned(source, col, row, width, height, dest, 0, 0);
    }
    else
        gwy_mask_field_copy(source, &fpart, dest, 0, 0);

    if (transformation == GWY_PLANE_MIRROR_DIAGONALLY)
        return;

    guint rowsize = dest->stride * sizeof(guint32);
    guint32 *buffer = g_slice_alloc(rowsize);
    if (transformation == GWY_PLANE_MIRROR_HORIZONTALLY
        || transformation == GWY_PLANE_ROTATE_CLOCKWISE)
        flip_horizontally(dest, buffer);
    else if (transformation == GWY_PLANE_MIRROR_VERTICALLY
             || transformation == GWY_PLANE_ROTATE_COUNTERCLOCKWISE)
        flip_vertically(dest, buffer);
    else if (transformation == GWY_PLANE_MIRROR_BOTH
             || transformation == GWY_PLANE_MIRROR_ANTIDIAGONALLY)
        flip_both(dest, buffer);
    else {
        g_assert_not_reached();
    }
    g_slice_free1(rowsize, buffer);
}

/**
 * gwy_mask_field_new_congruent:
 * @field: A two-dimensional mask field.
 * @fpart: (allow-none):
 *         Area in @field to extract to the new field.  Passing %NULL extracts
 *         entire @field.
 * @transformation: Type of plane congruence transformation.
 *
 * Creates a new two-dimensional mask field by performing a congruence
 * transformation of another mask field.
 *
 * Returns: (transfer full):
 *          A new two-dimensional mask field.
 **/
GwyMaskField*
gwy_mask_field_new_congruent(const GwyMaskField *field,
                             const GwyFieldPart *fpart,
                             GwyPlaneCongruence transformation)
{
    guint col, row, width, height;
    if (!gwy_mask_field_check_part(field, fpart, &col, &row, &width, &height))
        return NULL;
    g_return_val_if_fail(gwy_plane_congruence_is_valid(transformation), NULL);

    gboolean is_trans = gwy_plane_congruence_is_transposition(transformation);
    guint dwidth = is_trans ? height : width;
    guint dheight = is_trans ? width : height;
    GwyMaskField *part = gwy_mask_field_new_sized(dwidth, dheight, FALSE);

    transform_congruent_to(field, col, row, width, height, part,
                           transformation);

    return part;
}

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
