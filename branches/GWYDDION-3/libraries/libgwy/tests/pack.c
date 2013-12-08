/*
 *  $Id$
 *  Copyright (C) 2009,2013 David Nečas (Yeti).
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

#include "testlibgwy.h"

/***************************************************************************
 *
 * Packing and unpacking
 *
 ***************************************************************************/

void
test_pack_format(void)
{
    enum { expected_size = 57 };
    struct {
        gdouble d, dp;
        gint32 i;
        gint16 a1, a2;
        gint64 q;
        guint64 qu;
        gchar chars[5];
    } out, in = {
        /* XXX: The Pascal format allows for some rounding errors at this
         * moment.  Use a test number that is expected to work exactly. */
        G_PI, 1234.5,
        -3,
        -1, 73,
        G_GINT64_CONSTANT(0x12345678),
        G_GUINT64_CONSTANT(0xfedcba98),
        "test",
    };
    gchar format[] = ". x d r i 2h 13x q Q 5S";
    const gchar *byte_orders = "<>";
    guchar *buf;
    gsize packed_size, consumed;

    buf = g_new(guchar, expected_size);

    while (*byte_orders) {
        format[0] = *byte_orders;
        g_assert(gwy_pack_size(format, NULL) == expected_size);
        packed_size = gwy_pack(format, buf, expected_size, NULL,
                               &in.d, &in.dp,
                               &in.i,
                               &in.a1, &in.a2,
                               &in.q,
                               &in.qu,
                               in.chars);
        g_assert(packed_size == expected_size);
        consumed = gwy_unpack(format, buf, expected_size, NULL,
                              &out.d, &out.dp,
                              &out.i,
                              &out.a1, &out.a2,
                              &out.q,
                              &out.qu,
                              out.chars);
        g_assert(consumed == expected_size);
        g_assert(out.d == in.d);
        g_assert(out.dp == in.dp);
        g_assert(out.i == in.i);
        g_assert(out.a1 == in.a1);
        g_assert(out.a2 == in.a2);
        g_assert(out.q == in.q);
        g_assert(out.qu == in.qu);
        g_assert(memcmp(out.chars, in.chars, sizeof(in.chars)) == 0);

        byte_orders++;
    }

    g_free(buf);
}

void
test_pack_read_boolean(void)
{
    static const guchar boolean8_data[] = { 0x00, 0x01, 0x80, 0xff };
    const guchar *p = boolean8_data;
    gboolean v;

    v = gwy_read_boolean8(&p);
    g_assert_cmpint(p - boolean8_data, ==, 1);
    g_assert_cmpint(v, ==, FALSE);

    v = gwy_read_boolean8(&p);
    g_assert_cmpint(p - boolean8_data, ==, 2);
    g_assert_cmpint(v, ==, TRUE);

    v = gwy_read_boolean8(&p);
    g_assert_cmpint(p - boolean8_data, ==, 3);
    g_assert_cmpint(v, ==, TRUE);

    v = gwy_read_boolean8(&p);
    g_assert_cmpint(p - boolean8_data, ==, 4);
    g_assert_cmpint(v, ==, TRUE);
}

void
test_pack_read_int16_le(void)
{
    static const guchar int16_data[] = {
        0x34, 0x12,
        0xff, 0x7f,
        0x00, 0x80,
        0xff, 0xff,
    };
    const guchar *p = int16_data;
    gint16 v;

    v = gwy_read_int16_le(&p);
    g_assert_cmpint(p - int16_data, ==, 2);
    g_assert_cmpint(v, ==, 0x1234);

    v = gwy_read_int16_le(&p);
    g_assert_cmpint(p - int16_data, ==, 4);
    g_assert_cmpint(v, ==, G_MAXINT16);

    v = gwy_read_int16_le(&p);
    g_assert_cmpint(p - int16_data, ==, 6);
    g_assert_cmpint(v, ==, G_MININT16);

    v = gwy_read_int16_le(&p);
    g_assert_cmpint(p - int16_data, ==, 8);
    g_assert_cmpint(v, ==, -1);
}

void
test_pack_read_int16_be(void)
{
    static const guchar int16_data[] = {
        0x12, 0x34,
        0x7f, 0xff,
        0x80, 0x00,
        0xff, 0xff,
    };
    const guchar *p = int16_data;
    gint16 v;

    v = gwy_read_int16_be(&p);
    g_assert_cmpint(p - int16_data, ==, 2);
    g_assert_cmpint(v, ==, 0x1234);

    v = gwy_read_int16_be(&p);
    g_assert_cmpint(p - int16_data, ==, 4);
    g_assert_cmpint(v, ==, G_MAXINT16);

    v = gwy_read_int16_be(&p);
    g_assert_cmpint(p - int16_data, ==, 6);
    g_assert_cmpint(v, ==, G_MININT16);

    v = gwy_read_int16_be(&p);
    g_assert_cmpint(p - int16_data, ==, 8);
    g_assert_cmpint(v, ==, -1);
}

void
test_pack_read_uint16_le(void)
{
    static const guchar uint16_data[] = {
        0x34, 0x12,
        0xff, 0x7f,
        0x00, 0x80,
        0xff, 0xff,
    };
    const guchar *p = uint16_data;
    guint16 v;

    v = gwy_read_uint16_le(&p);
    g_assert_cmpuint(p - uint16_data, ==, 2);
    g_assert_cmpuint(v, ==, 0x1234u);

    v = gwy_read_uint16_le(&p);
    g_assert_cmpuint(p - uint16_data, ==, 4);
    g_assert_cmpuint(v, ==, 0x7fffu);

    v = gwy_read_uint16_le(&p);
    g_assert_cmpuint(p - uint16_data, ==, 6);
    g_assert_cmpuint(v, ==, 0x8000u);

    v = gwy_read_uint16_le(&p);
    g_assert_cmpuint(p - uint16_data, ==, 8);
    g_assert_cmpuint(v, ==, G_MAXUINT16);
}

void
test_pack_read_uint16_be(void)
{
    static const guchar uint16_data[] = {
        0x12, 0x34,
        0x7f, 0xff,
        0x80, 0x00,
        0xff, 0xff,
    };
    const guchar *p = uint16_data;
    guint16 v;

    v = gwy_read_uint16_be(&p);
    g_assert_cmpuint(p - uint16_data, ==, 2);
    g_assert_cmpuint(v, ==, 0x1234u);

    v = gwy_read_uint16_be(&p);
    g_assert_cmpuint(p - uint16_data, ==, 4);
    g_assert_cmpuint(v, ==, 0x7fffu);

    v = gwy_read_uint16_be(&p);
    g_assert_cmpuint(p - uint16_data, ==, 6);
    g_assert_cmpuint(v, ==, 0x8000u);

    v = gwy_read_uint16_be(&p);
    g_assert_cmpuint(p - uint16_data, ==, 8);
    g_assert_cmpuint(v, ==, G_MAXUINT16);
}

void
test_pack_read_int32_le(void)
{
    static const guchar int32_data[] = {
        0x78, 0x56, 0x34, 0x12,
        0xff, 0xff, 0xff, 0x7f,
        0x00, 0x00, 0x00, 0x80,
        0xff, 0xff, 0xff, 0xff,
    };
    const guchar *p = int32_data;
    gint32 v;

    v = gwy_read_int32_le(&p);
    g_assert_cmpint(p - int32_data, ==, 4);
    g_assert_cmpint(v, ==, 0x12345678);

    v = gwy_read_int32_le(&p);
    g_assert_cmpint(p - int32_data, ==, 8);
    g_assert_cmpint(v, ==, G_MAXINT32);

    v = gwy_read_int32_le(&p);
    g_assert_cmpint(p - int32_data, ==, 12);
    g_assert_cmpint(v, ==, G_MININT32);

    v = gwy_read_int32_le(&p);
    g_assert_cmpint(p - int32_data, ==, 16);
    g_assert_cmpint(v, ==, -1);
}

void
test_pack_read_int32_be(void)
{
    static const guchar int32_data[] = {
        0x12, 0x34, 0x56, 0x78,
        0x7f, 0xff, 0xff, 0xff,
        0x80, 0x00, 0x00, 0x00,
        0xff, 0xff, 0xff, 0xff,
    };
    const guchar *p = int32_data;
    gint32 v;

    v = gwy_read_int32_be(&p);
    g_assert_cmpint(p - int32_data, ==, 4);
    g_assert_cmpint(v, ==, 0x12345678);

    v = gwy_read_int32_be(&p);
    g_assert_cmpint(p - int32_data, ==, 8);
    g_assert_cmpint(v, ==, G_MAXINT32);

    v = gwy_read_int32_be(&p);
    g_assert_cmpint(p - int32_data, ==, 12);
    g_assert_cmpint(v, ==, G_MININT32);

    v = gwy_read_int32_be(&p);
    g_assert_cmpint(p - int32_data, ==, 16);
    g_assert_cmpint(v, ==, -1);
}

void
test_pack_read_uint32_le(void)
{
    static const guchar uint32_data[] = {
        0x78, 0x56, 0x34, 0x12,
        0xff, 0xff, 0xff, 0x7f,
        0x00, 0x00, 0x00, 0x80,
        0xff, 0xff, 0xff, 0xff,
    };
    const guchar *p = uint32_data;
    guint32 v;

    v = gwy_read_uint32_le(&p);
    g_assert_cmpuint(p - uint32_data, ==, 4);
    g_assert_cmpuint(v, ==, 0x12345678u);

    v = gwy_read_uint32_le(&p);
    g_assert_cmpuint(p - uint32_data, ==, 8);
    g_assert_cmpuint(v, ==, 0x7fffffffu);

    v = gwy_read_uint32_le(&p);
    g_assert_cmpuint(p - uint32_data, ==, 12);
    g_assert_cmpuint(v, ==, 0x80000000u);

    v = gwy_read_uint32_le(&p);
    g_assert_cmpuint(p - uint32_data, ==, 16);
    g_assert_cmpuint(v, ==, G_MAXUINT32);
}

void
test_pack_read_uint32_be(void)
{
    static const guchar uint32_data[] = {
        0x12, 0x34, 0x56, 0x78,
        0x7f, 0xff, 0xff, 0xff,
        0x80, 0x00, 0x00, 0x00,
        0xff, 0xff, 0xff, 0xff,
    };
    const guchar *p = uint32_data;
    guint32 v;

    v = gwy_read_uint32_be(&p);
    g_assert_cmpuint(p - uint32_data, ==, 4);
    g_assert_cmpuint(v, ==, 0x12345678u);

    v = gwy_read_uint32_be(&p);
    g_assert_cmpuint(p - uint32_data, ==, 8);
    g_assert_cmpuint(v, ==, 0x7fffffffu);

    v = gwy_read_uint32_be(&p);
    g_assert_cmpuint(p - uint32_data, ==, 12);
    g_assert_cmpuint(v, ==, 0x80000000u);

    v = gwy_read_uint32_be(&p);
    g_assert_cmpuint(p - uint32_data, ==, 16);
    g_assert_cmpuint(v, ==, G_MAXUINT32);
}

void
test_pack_read_int64_le(void)
{
    static const guchar int64_data[] = {
        0xf0, 0xde, 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    };
    const guchar *p = int64_data;
    gint64 v;

    v = gwy_read_int64_le(&p);
    g_assert_cmpint(p - int64_data, ==, 8);
    g_assert_cmpint(v, ==, G_GINT64_CONSTANT(0x123456789abcdef0));

    v = gwy_read_int64_le(&p);
    g_assert_cmpint(p - int64_data, ==, 16);
    g_assert_cmpint(v, ==, G_MAXINT64);

    v = gwy_read_int64_le(&p);
    g_assert_cmpint(p - int64_data, ==, 24);
    g_assert_cmpint(v, ==, G_MININT64);

    v = gwy_read_int64_le(&p);
    g_assert_cmpint(p - int64_data, ==, 32);
    g_assert_cmpint(v, ==, -1);
}

void
test_pack_read_int64_be(void)
{
    static const guchar int64_data[] = {
        0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0,
        0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    };
    const guchar *p = int64_data;
    gint64 v;

    v = gwy_read_int64_be(&p);
    g_assert_cmpint(p - int64_data, ==, 8);
    g_assert_cmpint(v, ==, G_GINT64_CONSTANT(0x123456789abcdef0));

    v = gwy_read_int64_be(&p);
    g_assert_cmpint(p - int64_data, ==, 16);
    g_assert_cmpint(v, ==, G_MAXINT64);

    v = gwy_read_int64_be(&p);
    g_assert_cmpint(p - int64_data, ==, 24);
    g_assert_cmpint(v, ==, G_MININT64);

    v = gwy_read_int64_be(&p);
    g_assert_cmpint(p - int64_data, ==, 32);
    g_assert_cmpint(v, ==, -1);
}

void
test_pack_read_uint64_le(void)
{
    static const guchar uint64_data[] = {
        0xf0, 0xde, 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    };
    const guchar *p = uint64_data;
    guint64 v;

    v = gwy_read_uint64_le(&p);
    g_assert_cmpuint(p - uint64_data, ==, 8);
    g_assert_cmpuint(v, ==, G_GINT64_CONSTANT(0x123456789abcdef0u));

    v = gwy_read_uint64_le(&p);
    g_assert_cmpuint(p - uint64_data, ==, 16);
    g_assert_cmpuint(v, ==, G_GINT64_CONSTANT(0x7fffffffffffffffu));

    v = gwy_read_uint64_le(&p);
    g_assert_cmpuint(p - uint64_data, ==, 24);
    g_assert_cmpuint(v, ==, G_GINT64_CONSTANT(0x8000000000000000u));

    v = gwy_read_uint64_le(&p);
    g_assert_cmpuint(p - uint64_data, ==, 32);
    g_assert_cmpuint(v, ==, G_MAXUINT64);
}

void
test_pack_read_uint64_be(void)
{
    static const guchar uint64_data[] = {
        0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0,
        0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    };
    const guchar *p = uint64_data;
    guint64 v;

    v = gwy_read_uint64_be(&p);
    g_assert_cmpuint(p - uint64_data, ==, 8);
    g_assert_cmpuint(v, ==, G_GINT64_CONSTANT(0x123456789abcdef0u));

    v = gwy_read_uint64_be(&p);
    g_assert_cmpuint(p - uint64_data, ==, 16);
    g_assert_cmpuint(v, ==, G_GINT64_CONSTANT(0x7fffffffffffffffu));

    v = gwy_read_uint64_be(&p);
    g_assert_cmpuint(p - uint64_data, ==, 24);
    g_assert_cmpuint(v, ==, G_GINT64_CONSTANT(0x8000000000000000u));

    v = gwy_read_uint64_be(&p);
    g_assert_cmpuint(p - uint64_data, ==, 32);
    g_assert_cmpuint(v, ==, G_MAXUINT64);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
