/*
 *  $Id$
 *  Copyright (C) 2009 David Nečas (Yeti).
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
#include <stdarg.h>
#include "libgwy/pack.h"
#include "libgwy/math.h"

#if (G_BYTE_ORDER == G_LITTLE_ENDIAN)
#define DIRECT_ORDER '<'
#define SWAP_ORDER '>'
#elif (G_BYTE_ORDER == G_BIG_ENDIAN)
#define DIRECT_ORDER '>'
#define SWAP_ORDER '<'
#else
#error Byte order used on this system is not supported.
#endif

#define PASCAL_REAL_B 549755813888.0
#define PASCAL_REAL_2B 1099511627776.0

enum { PASCAL_REAL_BIAS = 129 };

static const guint sizes_from64[] = {
/*  @  A  B  C  D  E  F  G  H  I  J  K  L  M  N  O  */
    0, 0, 0, 1, 0, 0, 0, 0, 2, 4, 0, 0, 0, 0, 0, 0,
/*  P  Q  R  S  T  U  V  W  X  Y  Z  [  \  ]  ^  _  */
    0, 8, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/*  `  a  b  c  d  e  f  g  h  i  j  k  l  m  n  o  */
    0, 0, 0, 1, 8, 0, 4, 0, 2, 4, 0, 0, 0, 0, 0, 0,
/*  p  q  r  s  t  u  v  w  x  y  z  {  |  }  ~     */
    1, 8, 6, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,
};

/**
 * gwy_pack_size:
 * @format: Packing format specifier.
 * @error: Return location for the error, or %NULL.
 *
 * Gets the data size corresponding to a binary data packing format.
 *
 * Since this functions does not get the actual data, it cannot determine the
 * lengths of variable-length items such as strings.  The minimum lengths are
 * used instead.  For both nul-terminated and Pascal strings the minimum
 * length is 1 (the terminating nul character or the size byte, respectively).
 *
 * Returns: Packed data size in bytes.  Zero is returned on error (some
 *          degenerate valid formats lead to zero data size too, though).
 **/
gsize
gwy_pack_size(const gchar *format,
              GError **error)
{
    gsize count = 0, itemsize = 0, size = 0;
    gboolean seen_count = FALSE;
    guint f;

    f = *(format++);
    if (f != DIRECT_ORDER && f != SWAP_ORDER) {
        g_set_error(error, GWY_PACK_ERROR, GWY_PACK_ERROR_FORMAT,
                    "Missing byte order marker.");
        return 0;
    }

    for ( ; (f = *format); format++) {
        if (f >= '0' && f <= '9') {
            if (G_UNLIKELY(count > 100000000UL)) {
                g_set_error(error, GWY_PACK_ERROR, GWY_PACK_ERROR_FORMAT,
                            "Item count overflow");
                return 0;
            }
            count = 10*count + (f - '0');
            seen_count = TRUE;
            continue;
        }
        /* Test this after numbers.  We do not allow them segmented. */
        if (g_ascii_isspace(f))
            continue;

        g_warn_if_fail(count || !seen_count);
        itemsize = (f >= 64 && f < 128) ? sizes_from64[f - 64] : 0;
        if (G_UNLIKELY(!itemsize)) {
            g_set_error(error, GWY_PACK_ERROR, GWY_PACK_ERROR_FORMAT,
                        "Invalid format character 0x%02x", f);
            return 0;
        }
        size += (count ? count : 1)*itemsize;
        count = 0;
        seen_count = FALSE;
    }

    if (G_UNLIKELY(seen_count)) {
        g_set_error(error, GWY_PACK_ERROR, GWY_PACK_ERROR_FORMAT,
                    "Format ends with a count");
        return 0;
    }

    return size;
}

/* Writes 6 bytes */
static void
gwy_pack_pascal_real_le(guchar *p, gdouble x)
{
    guchar s = 0;
    union { guint64 i; guchar c[8]; } u;
    gint power;

    if (!x) {
        memset(p, 0, 6);
        return;
    }

    if (x < 0.0) {
        x = -x;
        s = 0x80;
    }

    power = (gint)floor(log2(x));
    x *= PASCAL_REAL_B/exp2(power);
    if (x < PASCAL_REAL_B) {
        x *= 2.0;
        power--;
    }
    x = floor(x + 0.499);
    if (x >= PASCAL_REAL_2B) {
        x = floor(x/2.0 + 0.499);
        power++;
    }

    /* Crude domain handling.  We could perhaps use denormalized numbers for
     * small underflows, but... */
    power += PASCAL_REAL_BIAS;
    if (G_UNLIKELY(power < 0)) {
        memset(p, 0, 6);
        return;
    }
    if (G_UNLIKELY(power > 255)) {
        memset(p, 0xff, 6);
        p[5] &= (0x7f | s);
        return;
    }

    p[0] = (guint)power;

    u.i = (guint64)(x - PASCAL_REAL_B);
#if (G_BYTE_ORDER == G_LITTLE_ENDIAN)
    memcpy(p + 1, u.c, 5);
#elif (G_BYTE_ORDER == G_BIG_ENDIAN)
    p[1] = u.c[7];
    p[2] = u.c[6];
    p[3] = u.c[5];
    p[4] = u.c[4];
    p[5] = u.c[3];
#else
#error Byte order used on this system is not supported.
#endif
    p[5] |= s;
}

/* Writes 6 bytes */
static void
gwy_pack_pascal_real_be(guchar *p, gdouble x)
{
    guchar s = 0;
    union { guint64 i; guchar c[8]; } u;
    gint power;

    if (!x) {
        memset(p, 0, 6);
        return;
    }

    if (x < 0.0) {
        x = -x;
        s = 0x80;
    }

    power = (gint)floor(log2(x));
    x *= PASCAL_REAL_B/exp2(power);
    if (x < PASCAL_REAL_B) {
        x *= 2.0;
        power--;
    }
    x = floor(x + 0.499);
    if (x >= PASCAL_REAL_2B) {
        x = floor(x/2.0 + 0.499);
        power++;
    }

    /* Crude domain handling.  We could perhaps use denormalized numbers for
     * small underflows, but... */
    power += PASCAL_REAL_BIAS;
    if (G_UNLIKELY(power < 0)) {
        memset(p, 0, 6);
        return;
    }
    if (G_UNLIKELY(power > 255)) {
        memset(p, 0xff, 6);
        p[0] &= (0x7f | s);
        return;
    }

    p[5] = (guint)power;

    u.i = (guint64)(x - PASCAL_REAL_B);
#if (G_BYTE_ORDER == G_BIG_ENDIAN)
    memcpy(p, u.c + 3, 5);
#elif (G_BYTE_ORDER == G_LITTLE_ENDIAN)
    p[4] = u.c[0];
    p[3] = u.c[1];
    p[2] = u.c[2];
    p[1] = u.c[3];
    p[0] = u.c[4];
#else
#error Byte order used on this system is not supported.
#endif
    p[0] |= s;
}

#if (G_BYTE_ORDER == G_LITTLE_ENDIAN)
#define gwy_pack_pascal_real_direct gwy_pack_pascal_real_le
#define gwy_pack_pascal_real_swap gwy_pack_pascal_real_be
#elif (G_BYTE_ORDER == G_BIG_ENDIAN)
#define gwy_pack_pascal_real_direct gwy_pack_pascal_real_be
#define gwy_pack_pascal_real_swap gwy_pack_pascal_real_le
#else
#error Byte order used on this system is not supported.
#endif

/**
 * gwy_pack:
 * @format: Packing format specifier.
 * @buffer: Memory buffer to pack data into.
 * @size: Size of buffer.
 * @error: Return location for the error, or %NULL.
 * @...: Pointers to data to pack.
 *
 * Packs binary data into a memory buffer.
 *
 * This function validitates the format string, but it does not check validity
 * (or even number) of the arguments.
 *
 * Returns: Packed data size in bytes.  Zero is returned on error (some
 *          degenerate valid formats lead to zero data size too, though).
 **/
gsize
gwy_pack(const gchar *format,
         guchar *buffer,
         gsize size,
         GError **error,
         ...)
{
    gsize count = 0, pos = 0, itemsize;
    gboolean seen_count = FALSE;
    gboolean do_swap;
    va_list ap;
    guint f;

    f = *(format++);
    if (f == DIRECT_ORDER)
        do_swap = FALSE;
    else if (f == SWAP_ORDER)
        do_swap = TRUE;
    else {
        g_set_error(error, GWY_PACK_ERROR, GWY_PACK_ERROR_FORMAT,
                    "Missing byte order marker.");
        return 0;
    }

    va_start(ap, error);
    for ( ; (f = *format); format++) {
        if (f >= '0' && f <= '9') {
            if (G_UNLIKELY(count > 100000000UL)) {
                g_set_error(error, GWY_PACK_ERROR, GWY_PACK_ERROR_FORMAT,
                            "Item count overflow");
                goto fail;
            }
            count = 10*count + (f - '0');
            seen_count = TRUE;
            continue;
        }
        /* Test this after numbers.  We do not allow them segmented. */
        if (g_ascii_isspace(f))
            continue;

        if (G_UNLIKELY(seen_count && !count)) {
            g_warning("Zero item count");
            seen_count = FALSE;
            continue;
        }
        itemsize = (f >= 64 && f < 128) ? sizes_from64[f - 64] : 0;
        if (G_UNLIKELY(!itemsize)) {
            g_set_error(error, GWY_PACK_ERROR, GWY_PACK_ERROR_FORMAT,
                        "Invalid format character 0x%02x", f);
            goto fail;
        }
        if (!count)
            count = 1;
        if (G_UNLIKELY(pos + count*itemsize > size)) {
            g_set_error(error, GWY_PACK_ERROR, GWY_PACK_ERROR_SIZE,
                        "Packed data do not fit into buffer");
            goto fail;
        }

        if (f == 'x') {
            memset(buffer + pos, 0, count);
            pos += count;
            count = 0;
        }
        else if (f == 'r') {
            while (count) {
                const gdouble arg = *va_arg(ap, const gdouble*);
                if (do_swap)
                    gwy_pack_pascal_real_swap(buffer + pos, arg);
                else
                    gwy_pack_pascal_real_direct(buffer + pos, arg);
                pos += itemsize;
                count--;
            }
        }
        else if (f == 'S') {
            const guchar *arg = va_arg(ap, const guchar*);
            memcpy(buffer + pos, arg, count);
            pos += count;
            count = 0;
        }
        else if (f == 's') {
            while (count) {
                const gchar *arg = *va_arg(ap, const gchar**);
                itemsize = strlen(arg) + 1;
                if (G_UNLIKELY(pos + itemsize > size)) {
                    g_set_error(error, GWY_PACK_ERROR, GWY_PACK_ERROR_SIZE,
                                "Packed data do not fit into buffer");
                    goto fail;
                }
                memcpy(buffer + pos, arg, itemsize);
                pos += itemsize;
                count--;
            }
        }
        else if (f == 'p') {
            while (count) {
                const gchar *arg = *va_arg(ap, const gchar**);
                itemsize = strlen(arg);
                if (G_UNLIKELY(pos + itemsize + 1 > size)) {
                    g_set_error(error, GWY_PACK_ERROR, GWY_PACK_ERROR_SIZE,
                                "Packed data do not fit into buffer");
                    goto fail;
                }
                if (G_UNLIKELY(itemsize > 0xff)) {
                    g_set_error(error, GWY_PACK_ERROR, GWY_PACK_ERROR_ARGUMENTS,
                                "Pascal string is longer than 255 bytes");
                    goto fail;
                }
                buffer[pos++] = itemsize;
                memcpy(buffer + pos, arg, itemsize);
                pos += itemsize;
                count--;
            }
        }
        /* We do not care what bloody type the user passed to us.  For
         * byte-swapping, we only need to know the item size.   Since
         * itemsize is a known constant in each branch, hopefully the compiler
         * will replace memcpy with something more appropriate. */
        else if (itemsize == 1) {
            while (count) {
                guchar arg = *va_arg(ap, const guchar*);
                buffer[pos++] = arg;
                count--;
            }
        }
        else if (itemsize == 2) {
            if (do_swap) {
                while (count) {
                    guint16 arg = *va_arg(ap, const guint16*);
                    arg = GUINT16_SWAP_LE_BE(arg);
                    memcpy(buffer + pos, &arg, itemsize);
                    pos += itemsize;
                    count--;
                }
            }
            else {
                while (count) {
                    guint16 arg = *va_arg(ap, const guint16*);
                    memcpy(buffer + pos, &arg, itemsize);
                    pos += itemsize;
                    count--;
                }
            }
        }
        else if (itemsize == 4) {
            if (do_swap) {
                while (count) {
                    guint32 arg = *va_arg(ap, const guint32*);
                    arg = GUINT32_SWAP_LE_BE(arg);
                    memcpy(buffer + pos, &arg, itemsize);
                    pos += itemsize;
                    count--;
                }
            }
            else {
                while (count) {
                    guint32 arg = *va_arg(ap, const guint32*);
                    memcpy(buffer + pos, &arg, itemsize);
                    pos += itemsize;
                    count--;
                }
            }
        }
        else if (itemsize == 8) {
            if (do_swap) {
                while (count) {
                    guint64 arg = *va_arg(ap, const guint64*);
                    arg = GUINT64_SWAP_LE_BE(arg);
                    memcpy(buffer + pos, &arg, itemsize);
                    pos += itemsize;
                    count--;
                }
            }
            else {
                while (count) {
                    guint64 arg = *va_arg(ap, const guint64*);
                    memcpy(buffer + pos, &arg, itemsize);
                    pos += itemsize;
                    count--;
                }
            }
        }
        else {
            g_return_val_if_reached(0);
        }
        seen_count = FALSE;
    }
    va_end(ap);

    if (G_UNLIKELY(seen_count)) {
        g_set_error(error, GWY_PACK_ERROR, GWY_PACK_ERROR_FORMAT,
                    "Format ends with a count");
        pos = 0;
    }

    return pos;

fail:
    va_end(ap);
    return 0;
}

/* Takes 6 bytes */
static gdouble
gwy_unpack_pascal_real_le(const guchar *p)
{
    gdouble x;

    if (!p[0])
        return 0.0;

    x = 1.0 + ((((p[1]/256.0 + p[2])/256.0 + p[3])/256.0 + p[4])/256.0
               + (p[5] & 0x7f))/128.0;
    if (p[5] & 0x80)
        x = -x;

    return x*gwy_powi(2.0, (gint)p[0] - PASCAL_REAL_BIAS);
}

/* Takes 6 bytes */
static gdouble
gwy_unpack_pascal_real_be(const guchar *p)
{
    gdouble x;

    if (!p[5])
        return 0.0;

    x = 1.0 + ((((p[4]/256.0 + p[3])/256.0 + p[2])/256.0 + p[1])/256.0
               + (p[0] & 0x7f))/128.0;
    if (p[0] & 0x80)
        x = -x;

    return x*gwy_powi(2.0, (gint)p[5] - PASCAL_REAL_BIAS);
}

#if (G_BYTE_ORDER == G_LITTLE_ENDIAN)
#define gwy_unpack_pascal_real_direct gwy_unpack_pascal_real_le
#define gwy_unpack_pascal_real_swap gwy_unpack_pascal_real_be
#elif (G_BYTE_ORDER == G_BIG_ENDIAN)
#define gwy_unpack_pascal_real_direct gwy_unpack_pascal_real_be
#define gwy_unpack_pascal_real_swap gwy_unpack_pascal_real_le
#else
#error Byte order used on this system is not supported.
#endif

/**
 * gwy_unpack:
 * @format: Packing format specifier.
 * @buffer: Memory buffer to unpack data from.
 * @size: Size of buffer.
 * @error: Return location for the error, or %NULL.
 * @...: Pointers to locations to fill with the unpacked data.
 *
 * Unpacks binary data from a memory buffer.
 *
 * String data pointers are filled with newly allocated strings that must be
 * freed with g_free() by the caller.  If an error occurs all allocated memory
 * is freed, the contents of the memory pointed by arguments must be considered
 * undefined.
 *
 * This function validitates the format string, but it does not check validity
 * (or even number) of the arguments.  It also check for string types
 * overflowing the end of the buffer.
 *
 * Returns: Number of bytes consumed from buffer.  Zero is returned on error
 *          (some degenerate valid formats lead to zero data size too,
 *          though).
 **/
gsize
gwy_unpack(const gchar *format,
           const guchar *buffer,
           gsize size,
           GError **error,
           ...)
{
    gsize count = 0, pos = 0, itemsize;
    gboolean seen_count = FALSE;
    GPtrArray *to_free = NULL;
    gboolean do_swap;
    va_list ap;
    guint f;

    f = *(format++);
    if (f == DIRECT_ORDER)
        do_swap = FALSE;
    else if (f == SWAP_ORDER)
        do_swap = TRUE;
    else {
        g_set_error(error, GWY_PACK_ERROR, GWY_PACK_ERROR_FORMAT,
                    "Missing byte order marker.");
        return 0;
    }

    va_start(ap, error);
    for ( ; (f = *format); format++) {
        if (f >= '0' && f <= '9') {
            if (G_UNLIKELY(count > 100000000UL)) {
                g_set_error(error, GWY_PACK_ERROR, GWY_PACK_ERROR_FORMAT,
                            "Item count overflow");
                goto fail;
            }
            count = 10*count + (f - '0');
            seen_count = TRUE;
            continue;
        }
        /* Test this after numbers.  We do not allow them segmented. */
        if (g_ascii_isspace(f))
            continue;

        if (G_UNLIKELY(seen_count && !count)) {
            g_warning("Zero item count");
            seen_count = FALSE;
            continue;
        }

        itemsize = (f >= 64 && f < 128) ? sizes_from64[f - 64] : 0;
        if (G_UNLIKELY(!itemsize)) {
            g_set_error(error, GWY_PACK_ERROR, GWY_PACK_ERROR_FORMAT,
                        "Invalid format character 0x%02x", f);
            goto fail;
        }
        if (!count)
            count = 1;
        if (G_UNLIKELY(pos + count*itemsize > size)) {
            g_set_error(error, GWY_PACK_ERROR, GWY_PACK_ERROR_SIZE,
                        "Packed data do not fit into buffer");
            goto fail;
        }

        if (f == 'x') {
            pos += count;
            count = 0;
        }
        else if (f == 'r') {
            while (count) {
                gdouble *arg = va_arg(ap, gdouble*);
                if (do_swap)
                    *arg = gwy_unpack_pascal_real_swap(buffer + pos);
                else
                    *arg = gwy_unpack_pascal_real_direct(buffer + pos);
                pos += itemsize;
                count--;
            }
        }
        else if (f == 'S') {
            guchar *arg = va_arg(ap, guchar*);
            memcpy(arg, buffer + pos, count);
            pos += count;
            count = 0;
        }
        else if (f == 's') {
            while (count) {
                guchar **arg = va_arg(ap, guchar**);
                guchar *end = memchr(buffer + pos, '\0', size - pos);
                if (!end) {
                    g_set_error(error, GWY_PACK_ERROR, GWY_PACK_ERROR_DATA,
                                "String does not end within buffer");
                    goto fail;
                }
                itemsize = end - (buffer + pos) + 1;
                *arg = g_memdup(buffer + pos, itemsize);
                if (!to_free)
                    to_free = g_ptr_array_new();
                g_ptr_array_add(to_free, *arg);
                pos += itemsize;
                count--;
            }
        }
        else if (f == 'p') {
            while (count) {
                guchar **arg = va_arg(ap, guchar**);
                itemsize = buffer[pos++];
                if (G_UNLIKELY(pos + itemsize > size)) {
                    g_set_error(error, GWY_PACK_ERROR, GWY_PACK_ERROR_DATA,
                                "String does not end within buffer");
                    goto fail;
                }
                *arg = g_memdup(buffer + pos, itemsize);
                if (!to_free)
                    to_free = g_ptr_array_new();
                g_ptr_array_add(to_free, *arg);
                pos += itemsize;
                count--;
            }
        }
        /* We do not care what bloody type the user passed to us.  For
         * byte-swapping, we only need to know the item size.   Since
         * itemsize is a known constant in each branch, hopefully the compiler
         * will replace memcpy with something more appropriate. */
        else if (itemsize == 1) {
            while (count) {
                guchar *arg = va_arg(ap, guchar*);
                *arg = buffer[pos++];
                count--;
            }
        }
        else if (itemsize == 2) {
            if (do_swap) {
                while (count) {
                    guint16 *arg = va_arg(ap, guint16*);
                    memcpy(arg, buffer + pos, itemsize);
                    *arg = GUINT16_SWAP_LE_BE(*arg);
                    pos += itemsize;
                    count--;
                }
            }
            else {
                while (count) {
                    guint16 *arg = va_arg(ap, guint16*);
                    memcpy(arg, buffer + pos, itemsize);
                    pos += itemsize;
                    count--;
                }
            }
        }
        else if (itemsize == 4) {
            if (do_swap) {
                while (count) {
                    guint32 *arg = va_arg(ap, guint32*);
                    memcpy(arg, buffer + pos, itemsize);
                    *arg = GUINT32_SWAP_LE_BE(*arg);
                    pos += itemsize;
                    count--;
                }
            }
            else {
                while (count) {
                    guint32 *arg = va_arg(ap, guint32*);
                    memcpy(arg, buffer + pos, itemsize);
                    pos += itemsize;
                    count--;
                }
            }
        }
        else if (itemsize == 8) {
            if (do_swap) {
                while (count) {
                    guint64 *arg = va_arg(ap, guint64*);
                    memcpy(arg, buffer + pos, itemsize);
                    *arg = GUINT64_SWAP_LE_BE(*arg);
                    pos += itemsize;
                    count--;
                }
            }
            else {
                while (count) {
                    guint64 *arg = va_arg(ap, guint64*);
                    memcpy(arg, buffer + pos, itemsize);
                    pos += itemsize;
                    count--;
                }
            }
        }
        else {
            g_return_val_if_reached(0);
        }
        seen_count = FALSE;
    }
    va_end(ap);

    if (G_UNLIKELY(seen_count)) {
        g_set_error(error, GWY_PACK_ERROR, GWY_PACK_ERROR_FORMAT,
                    "Format ends with a count");
        pos = 0;
    }

    return pos;

fail:
    va_end(ap);
    if (to_free) {
        for (count = 0; count < to_free->len; count++)
            g_free(g_ptr_array_index(to_free, count));
        g_ptr_array_free(to_free, TRUE);
    }
    return 0;
}

/**
 * gwy_unpack_data:
 * @format: Two-character format string.  The first character determines the
 *          byte order, the second character determines the packed data type.
 *          Not all types are allowed, only integer and floating point types.
 * @buffer: Packed data.
 * @len: Number of data items to unpack.
 * @data: Array to store unpacked data to.
 * @data_stride: Stride to use in @data (measured in items).
 * @factor: Factor to multiply unpacked values with.
 * @shift: Constant to add to unpacked values.
 *
 * Unpacks values into an array of double precision floating point numbers.
 *
 * Since the caller knows how long the buffer must be to contain @len items,
 * this function does not check its size (in fact, it does not know the size).
 **/
void
gwy_unpack_data(const gchar *format,
                gconstpointer buffer,
                gsize len,
                gdouble *data,
                gint data_stride,
                gdouble factor,
                gdouble shift)
{
    gchar byteorder, fmt;
    gsize i;

    byteorder = format[0];
    fmt = format[1];
    if (byteorder == DIRECT_ORDER) {
        if (fmt == 'd') {
            const gdouble *p = (const gdouble*)buffer;
            for (i = len; i; i--) {
                *data = (*(p++))*factor + shift;
                data += data_stride;
            }
        }
        else if (fmt == 'f') {
            const gfloat *p = (const gfloat*)buffer;
            for (i = len; i; i--) {
                *data = (*(p++))*factor + shift;
                data += data_stride;
            }
        }
        else if (fmt == 'h') {
            const gint16 *p = (const gint16*)buffer;
            for (i = len; i; i--) {
                *data = (*(p++))*factor + shift;
                data += data_stride;
            }
        }
        else if (fmt == 'H') {
            const guint16 *p = (const guint16*)buffer;
            for (i = len; i; i--) {
                *data = (*(p++))*factor + shift;
                data += data_stride;
            }
        }
        else if (fmt == 'i') {
            const gint32 *p = (const gint32*)buffer;
            for (i = len; i; i--) {
                *data = (*(p++))*factor + shift;
                data += data_stride;
            }
        }
        else if (fmt == 'I') {
            const guint32 *p = (const guint32*)buffer;
            for (i = len; i; i--) {
                *data = (*(p++))*factor + shift;
                data += data_stride;
            }
        }
        else if (fmt == 'c') {
            const gchar *p = (const gchar*)buffer;
            for (i = len; i; i--) {
                *data = (*(p++))*factor + shift;
                data += data_stride;
            }
        }
        else if (fmt == 'C') {
            const guchar *p = (const guchar*)buffer;
            for (i = len; i; i--) {
                *data = (*(p++))*factor + shift;
                data += data_stride;
            }
        }
        else if (fmt == 'q') {
            const gint64 *p = (const gint64*)buffer;
            for (i = len; i; i--) {
                *data = (*(p++))*factor + shift;
                data += data_stride;
            }
        }
        else if (fmt == 'Q') {
            const guint64 *p = (const guint64*)buffer;
            for (i = len; i; i--) {
                *data = (*(p++))*factor + shift;
                data += data_stride;
            }
        }
        else if (fmt == 'r') {
            const guchar *p = (const guchar*)buffer;
            for (i = len; i; i--) {
                *data = gwy_unpack_pascal_real_direct(p)*factor + shift;
                data += data_stride;
                p += 6;
            }
        }
        else {
            g_critical("Format %c is not implemented", fmt);
        }
    }
    else if (byteorder == SWAP_ORDER) {
        if (fmt == 'd') {
            union { guint64 i; gdouble f; } u;
            const guint64 *p = (const guint64*)buffer;
            for (i = len; i; i--) {
                u.i = *(p++);
                u.i = GUINT64_SWAP_LE_BE(u.i);
                *data = u.f*factor + shift;
                data += data_stride;
            }
        }
        else if (fmt == 'f') {
            union { guint32 i; gfloat f; } u;
            const guint32 *p = (const guint32*)buffer;
            for (i = len; i; i--) {
                u.i = *(p++);
                u.i = GUINT32_SWAP_LE_BE(u.i);
                *data = u.f*factor + shift;
                data += data_stride;
            }
        }
        else if (fmt == 'h') {
            const gint16 *p = (const gint16*)buffer;
            for (i = len; i; i--) {
                gint16 u = *(p++);
                *data = ((gint16)GUINT16_SWAP_LE_BE(u))*factor + shift;
                data += data_stride;
            }
        }
        else if (fmt == 'H') {
            const guint16 *p = (const guint16*)buffer;
            for (i = len; i; i--) {
                guint16 u = *(p++);
                *data = GUINT16_SWAP_LE_BE(u)*factor + shift;
                data += data_stride;
            }
        }
        else if (fmt == 'i') {
            const gint32 *p = (const gint32*)buffer;
            for (i = len; i; i--) {
                gint32 u = *(p++);
                *data = ((gint32)GUINT32_SWAP_LE_BE(u))*factor + shift;
                data += data_stride;
            }
        }
        else if (fmt == 'I') {
            const guint32 *p = (const guint32*)buffer;
            for (i = len; i; i--) {
                guint32 u = *(p++);
                *data = GUINT32_SWAP_LE_BE(u)*factor + shift;
                data += data_stride;
            }
        }
        else if (fmt == 'c') {
            const gchar *p = (const gchar*)buffer;
            for (i = len; i; i--) {
                *data = (*(p++))*factor + shift;
                data += data_stride;
            }
        }
        else if (fmt == 'C') {
            const guchar *p = (const guchar*)buffer;
            for (i = len; i; i--) {
                *data = (*(p++))*factor + shift;
                data += data_stride;
            }
        }
        else if (fmt == 'q') {
            const gint64 *p = (const gint64*)buffer;
            for (i = len; i; i--) {
                gint64 u = *(p++);
                *data = ((gint64)GUINT64_SWAP_LE_BE(u))*factor + shift;
                data += data_stride;
            }
        }
        else if (fmt == 'Q') {
            const guint64 *p = (const guint64*)buffer;
            for (i = len; i; i--) {
                guint64 u = *(p++);
                *data = GUINT64_SWAP_LE_BE(u)*factor + shift;
                data += data_stride;
            }
        }
        else if (fmt == 'r') {
            const guchar *p = (const guchar*)buffer;
            for (i = len; i; i--) {
                *data = gwy_unpack_pascal_real_swap(p)*factor + shift;
                data += data_stride;
                p += 6;
            }
        }
        else {
            g_critical("Format %c is not implemented", fmt);
        }
    }
    else {
        g_critical("Invalid byte order %c", byteorder);
    }
}

/**
 * gwy_pack_error_quark:
 *
 * Gets the error domain for binary data packing and unpacking.
 *
 * See and use %GWY_PACK_ERROR.
 *
 * Returns: The error domain.
 **/
GQuark
gwy_pack_error_quark(void)
{
    static GQuark error_domain = 0;

    if (!error_domain)
        error_domain = g_quark_from_static_string("gwy-pack-error-quark");

    return error_domain;
}


/**
 * SECTION: pack
 * @title: pack
 * @short_description: Binary data packing and unpacking
 *
 * Format specifiers must start with a byte order mark.
 * The possible byte orders are:
 *
 * <informaltable>
 * <tgroup cols='2' align='left'>
 * <thead>
 *   <row>
 *     <entry>Format</entry>
 *     <entry>Description</entry>
 *   </row>
 * </thead>
 * <tbody>
 *   <row>
 *     <entry><literal>&gt;</literal></entry>
 *     <entry>big-endian (MSB, Motorola) byte order</entry>
 *   </row>
 *   <row>
 *     <entry><literal>&lt;</literal></entry>
 *     <entry>little-endian (LSB, Intel) byte order</entry>
 *   </row>
 * </tbody>
 * </tgroup>
 * </informaltable>
 *
 * There is no byter order mark corresponding to the native byte order to help
 * you resist the temptation.
 *
 * The rest of the format string consists of a sequence of type specifiers from
 * the table below.  Each can be preceeded by a decimal number that denotes the
 * number of subsequent arguments that will be of this type.  For instance,
 * "4i" means that four 32bit integer arguments follow and is fully equivalent
 * to "iiii".  The first form is usually slightly more efficient.
 *
 * To make the specifiers more readable, whitespace characters are permitted
 * (and ignored) anywhere between the specifiers and even between number and
 * argument type, numbers may not be split though.
 *
 * <informaltable>
 * <tgroup cols='3' align='left'>
 * <thead>
 *   <row>
 *     <entry>Format</entry>
 *     <entry>Packed Data Description</entry>
 *     <entry>C Type</entry>
 *   </row>
 * </thead>
 * <tbody>
 *   <row>
 *     <entry><literal>c</literal></entry>
 *     <entry>signed byte</entry>
 *     <entry>#gchar, #gint8, #int8_t, signed char</entry>
 *   </row>
 *   <row>
 *     <entry><literal>C</literal></entry>
 *     <entry>unsigned byte</entry>
 *     <entry>#guchar, #guint8, #uint8_t, unsigned char</entry>
 *   </row>
 *   <row>
 *     <entry><literal>h</literal></entry>
 *     <entry>signed short integer</entry>
 *     <entry>#gint16, #int16_t</entry>
 *   </row>
 *   <row>
 *     <entry><literal>H</literal></entry>
 *     <entry>unsigned short integer</entry>
 *     <entry>#guint16, #uint16_t</entry>
 *   </row>
 *   <row>
 *     <entry><literal>i</literal></entry>
 *     <entry>signed integer</entry>
 *     <entry>#gint32, #int32_t</entry>
 *   </row>
 *   <row>
 *     <entry><literal>I</literal></entry>
 *     <entry>unsigned integer</entry>
 *     <entry>#guint32, #uint32_t</entry>
 *   </row>
 *   <row>
 *     <entry><literal>q</literal></entry>
 *     <entry>signed long integer</entry>
 *     <entry>#gint64, #int64_t</entry>
 *   </row>
 *   <row>
 *     <entry><literal>Q</literal></entry>
 *     <entry>unsigned long integer</entry>
 *     <entry>#guint64, #uint64_t</entry>
 *   </row>
 *   <row>
 *     <entry><literal>f</literal></entry>
 *     <entry>IEEE single precision floating point number</entry>
 *     <entry>#gfloat, #float</entry>
 *   </row>
 *   <row>
 *     <entry><literal>d</literal></entry>
 *     <entry>IEEE double precision floating point number</entry>
 *     <entry>#gdouble, #double</entry>
 *   </row>
 *   <row>
 *     <entry><literal>r</literal></entry>
 *     <entry>Pascal Real floating point number</entry>
 *     <entry>#gdouble, #double</entry>
 *   </row>
 *   <row>
 *     <entry><literal>s</literal></entry>
 *     <entry>nul-terminated C string</entry>
 *     <entry>#gchar*</entry>
 *   </row>
 *   <row>
 *     <entry><literal>S</literal></entry>
 *     <entry>fixed-size array of characters</entry>
 *     <entry>#gchar[]</entry>
 *   </row>
 *   <row>
 *     <entry><literal>p</literal></entry>
 *     <entry>Pascal string (one-byte size + contents)</entry>
 *     <entry>#gchar*</entry>
 *   </row>
 *   <row>
 *     <entry><literal>x</literal></entry>
 *     <entry>nul byte (padding)</entry>
 *     <entry>#gchar*</entry>
 *   </row>
 * </tbody>
 * </tgroup>
 * </informaltable>
 *
 * All arguments of both gwy_pack() and gwy_unpack() are passed as pointers to
 * the packed type.  This means a pointer to #gint16 needs to be passed for
 * <literal>h</literal> specifier and that no type promotion occurs anywhere.
 * For strings, this means you need to pass #gchar** (or #guchar**), not just
 * the string.
 *
 * Pascal real type <literal>r</literal> is an exception to this rule as the
 * packed type is not a C type.   The corresponding arguments are pointers to
 * #gdouble instead.
 *
 * Both C <literal>s</literal> and Pascal <literal>p</literal> string accept or
 * allocate nul-terminated strings for their arguments (unlike
 * <literal>S</literal>).
 * Only the packed representation differs.  It is an error to pack a string
 * longer than 255 characters with <literal>p</literal> format.
 *
 * The difference between <literal>10c</literal> and <literal>10S</literal> is
 * that the former packs/unpacks 10 bytes from/into 10 separate arguments,
 * while the latter packs/unpacks 10 bytes from/into a single array pointed to
 * by the argument.  There is no guarantee of nul-termination for
 * <literal>S</literal>.
 *
 * Type <literal>x</literal> represents padding bytes and does not consume any
 * arguments.
 **/

/**
 * GwyPackError:
 * @GWY_PACK_ERROR_FORMAT: Packing format is invalid.
 * @GWY_PACK_ERROR_SIZE: Packed data size exceeds buffer size.
 * @GWY_PACK_ERROR_ARGUMENTS: Arguments do not match format.
 *                            At this moment it only concerns attempts to pack
 *                            Pascal strings longer than 255 characters.
 * @GWY_PACK_ERROR_DATA: Packed data do not match format, e.g. a nul-terminated
 *                       string is requested but there is no nul character in
 *                       the bufer.
 *
 * Error codes returned by binary data packing and unpacking.
 **/

/**
 * GWY_PACK_ERROR:
 *
 * Error domain for binary data packing and unpacking.
 *
 * Errors in this domain will be from the #GwyPackError enumeration.
 * See #GError for information on error domains.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
