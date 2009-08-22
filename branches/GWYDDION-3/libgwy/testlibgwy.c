/*
 *  @(#) $Id$
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
#include "libgwy/libgwy.h"

static void
test_pack(void)
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

static gboolean
test_sort_is_strictly_ordered(const gdouble *array, gsize n)
{
    gsize i;

    for (i = 1; i < n; i++, array++) {
        if (array[0] >= array[1])
            return FALSE;
    }
    return TRUE;
}

static gboolean
test_sort_is_ordered_with_index(const gdouble *array, const guint *index_array,
                                const gdouble *orig_array, gsize n)
{
    gsize i;

    for (i = 0; i < n; i++) {
        if (index_array[i] >= n)
            return FALSE;
        if (array[i] != orig_array[index_array[i]])
            return FALSE;
    }
    return TRUE;
}

static void
test_sort(void)
{
    gsize n, i, nmin = 0, nmax = 65536;
    gdouble *array, *orig_array;
    guint *index_array;

    if (g_test_quick())
        nmax = 8192;

    array = g_new(gdouble, nmax);
    orig_array = g_new(gdouble, nmax);
    index_array = g_new(guint, nmax);
    for (n = nmin; n < nmax; n = 7*n/6 + 1) {
        for (i = 0; i < n; i++)
            orig_array[i] = sin(n/G_SQRT2 + 1.618*i);

        memcpy(array, orig_array, n*sizeof(gdouble));
        gwy_math_sort(array, NULL, n);
        g_assert(test_sort_is_strictly_ordered(array, n));

        memcpy(array, orig_array, n*sizeof(gdouble));
        for (i = 0; i < n; i++)
            index_array[i] = i;
        gwy_math_sort(array, index_array, n);
        g_assert(test_sort_is_strictly_ordered(array, n));
        g_assert(test_sort_is_ordered_with_index(array, index_array,
                                                 orig_array, n));
    }
    g_free(index_array);
    g_free(orig_array);
    g_free(array);
}

static void
test_error_list(void)
{
    GwyErrorList *errlist = NULL;
    GError *err;

    gwy_error_list_add(&errlist, GWY_PACK_ERROR, GWY_PACK_ERROR_FORMAT,
                       "Just testing...");
    g_assert_cmpuint(g_slist_length(errlist), ==, 1);

    gwy_error_list_add(&errlist, GWY_PACK_ERROR, GWY_PACK_ERROR_FORMAT,
                       "Just testing %d...", 2);
    g_assert_cmpuint(g_slist_length(errlist), ==, 2);
    err = errlist->data;
    g_assert_cmpuint(err->domain, ==, GWY_PACK_ERROR);
    g_assert_cmpuint(err->code, ==, GWY_PACK_ERROR_FORMAT);
    g_assert_cmpstr(err->message, ==, "Just testing 2...");

    gwy_error_list_clear(&errlist);
    g_assert_cmpuint(g_slist_length(errlist), ==, 0);

    gwy_error_list_add(NULL, GWY_PACK_ERROR, GWY_PACK_ERROR_FORMAT,
                       "Ignored error");
    gwy_error_list_clear(NULL);
}

int
main(int argc, char *argv[])
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/testlibgwy/error_list", test_error_list);
    g_test_add_func("/testlibgwy/pack", test_pack);
    g_test_add_func("/testlibgwy/sort", test_sort);

    return g_test_run();
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
