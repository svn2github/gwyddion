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
#include <stdlib.h>
#include "libgwy/libgwy.h"

/***************************************************************************
 *
 * Packing and unpacking
 *
 ***************************************************************************/

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

/***************************************************************************
 *
 * Math sorting
 *
 ***************************************************************************/

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

/***************************************************************************
 *
 * Error lists
 *
 ***************************************************************************/

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

/***************************************************************************
 *
 * Serialization and deserialization
 *
 ***************************************************************************/

#define GWY_TYPE_SER_TEST \
    (gwy_ser_test_get_type())
#define GWY_SER_TEST(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_SER_TEST, GwySerTest))
#define GWY_IS_SER_TEST(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_SER_TEST))
#define GWY_SER_TEST_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_SER_TEST, GwySerTestClass))

GType gwy_ser_test_get_type(void) G_GNUC_CONST;

typedef struct _GwySerTest      GwySerTest;
typedef struct _GwySerTestClass GwySerTestClass;

struct _GwySerTestClass {
    GObjectClass g_object_class;
};

struct _GwySerTest {
    GObject g_object;

    guint len;
    gboolean flag;
    gdouble *data;
    gchar *s;
    guchar raw[4];
    GwySerTest *child;

    /* Testing stuff. */
    gint done_called;
};

static gsize
gwy_ser_test_n_items(GwySerializable *serializable)
{
    GwySerTest *sertest = GWY_SER_TEST(serializable);

    return 5 + (sertest->child
                ? gwy_serializable_n_items(GWY_SERIALIZABLE(sertest->child))
                : 0);
}

static gsize
gwy_ser_test_itemize(GwySerializable *serializable,
                     GwySerializableItems *items)
{
    GwySerTest *sertest = GWY_SER_TEST(serializable);
    GwySerializableItem *item;
    gsize n_items = 0;

    if (sertest->flag) {
        g_return_val_if_fail(items->len - items->n_items, 0);
        item = items->items + items->n_items++, n_items++;

        item->name = "flag";
        item->value.v_boolean = sertest->flag;
        item->ctype = GWY_SERIALIZABLE_BOOLEAN;
    }

    if (sertest->len) {
        g_return_val_if_fail(items->len - items->n_items, 0);
        item = items->items + items->n_items++, n_items++;

        item->name = "data";
        item->array_size = sertest->len;
        item->value.v_double_array = sertest->data;
        item->ctype = GWY_SERIALIZABLE_DOUBLE_ARRAY;
    }

    if (sertest->s) {
        g_return_val_if_fail(items->len - items->n_items, 0);
        item = items->items + items->n_items++, n_items++;

        item->name = "s";
        item->value.v_string = sertest->s;
        item->ctype = GWY_SERIALIZABLE_STRING;
    }

    g_return_val_if_fail(items->len - items->n_items, 0);
    item = items->items + items->n_items++, n_items++;

    item->name = "raw";
    item->array_size = 4;
    item->value.v_uint8_array = sertest->raw;
    item->ctype = GWY_SERIALIZABLE_INT8_ARRAY;

    if (sertest->child) {
        g_return_val_if_fail(items->len - items->n_items, 0);
        item = items->items + items->n_items++, n_items++;

        item->name = "child";
        item->value.v_object = G_OBJECT(sertest->child);
        item->ctype = GWY_SERIALIZABLE_OBJECT;
        gwy_serializable_itemize(GWY_SERIALIZABLE(sertest->child), items);
    }

    sertest->done_called--;

    return n_items;
}

static void
gwy_ser_test_done(GwySerializable *serializable)
{
    GwySerTest *sertest = GWY_SER_TEST(serializable);

    sertest->done_called++;
}

static void
gwy_ser_test_serializable_init(GwySerializableInterface *iface)
{
    iface->n_items = gwy_ser_test_n_items;
    iface->itemize = gwy_ser_test_itemize;
    iface->done = gwy_ser_test_done;
    /*
    iface->request = gwy_ser_test_request;
    iface->construct = gwy_ser_test_construct;
    iface->duplicate = gwy_ser_test_duplicate_;
    iface->assign = gwy_ser_test_assign_;
    */
}

G_DEFINE_TYPE_EXTENDED
    (GwySerTest, gwy_ser_test, G_TYPE_OBJECT, 0,
     GWY_IMPLEMENT_SERIALIZABLE(gwy_ser_test_serializable_init))

static void
gwy_ser_test_dispose(GObject *object)
{
    GwySerTest *sertest = GWY_SER_TEST(object);

    GWY_OBJECT_UNREF(sertest->child);
    G_OBJECT_CLASS(gwy_ser_test_parent_class)->dispose(object);
}

static void
gwy_ser_test_finalize(GObject *object)
{
    GwySerTest *sertest = GWY_SER_TEST(object);

    GWY_FREE(sertest->data);
    GWY_FREE(sertest->s);
    G_OBJECT_CLASS(gwy_ser_test_parent_class)->finalize(object);
}

static void
gwy_ser_test_class_init(GwySerTestClass *klass)
{
    GObjectClass *g_object_class = G_OBJECT_CLASS(klass);

    g_object_class->dispose = gwy_ser_test_dispose;
    g_object_class->finalize = gwy_ser_test_finalize;
}

static void
gwy_ser_test_init(G_GNUC_UNUSED GwySerTest *sertest)
{
}

GwySerTest*
gwy_ser_test_new_filled(gboolean flag,
                        const gdouble *data,
                        guint ndata,
                        const gchar *str,
                        guint32 raw)
{
    GwySerTest *sertest = g_object_newv(GWY_TYPE_SER_TEST, 0, NULL);

    sertest->flag = flag;
    sertest->len = ndata;
    if (sertest->len)
        sertest->data = g_memdup(data, ndata*sizeof(gdouble));
    sertest->s = g_strdup(str);
    memcpy(sertest->raw, &raw, 4);

    return sertest;
}

static const guchar ser_test_simple[] = {
    0x47, 0x77, 0x79, 0x53, 0x65, 0x72, 0x54, 0x65, 0x73, 0x74, 0x00, 0x11,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x72, 0x61, 0x77, 0x00, 0x43,
    0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static void
test_serialize_simple(void)
{
    GwySerTest *sertest;
    GOutputStream *stream;
    GMemoryOutputStream *memstream;
    GError *error = NULL;
    gboolean ok;
    guint len;

    sertest = g_object_newv(GWY_TYPE_SER_TEST, 0, NULL);
    stream = g_memory_output_stream_new(malloc(200), 200, NULL, &free);
    memstream = G_MEMORY_OUTPUT_STREAM(stream);
    ok = gwy_serializable_serialize(GWY_SERIALIZABLE(sertest), stream, &error);
    g_assert(ok);
    g_assert_cmpint(sertest->done_called, ==, 0);
    len = g_memory_output_stream_get_data_size(memstream);
    g_assert_cmpuint(len, ==, sizeof(ser_test_simple));
    g_assert(memcmp(g_memory_output_stream_get_data(memstream),
                    ser_test_simple, sizeof(ser_test_simple)) == 0);
    g_object_unref(stream);
    g_object_unref(sertest);
    g_clear_error(&error);
}

static const guchar ser_test_data[] = {
    0x47, 0x77, 0x79, 0x53, 0x65, 0x72, 0x54, 0x65, 0x73, 0x74, 0x00, 0x53,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x66, 0x6c, 0x61, 0x67, 0x00,
    0x62, 0x01, 0x64, 0x61, 0x74, 0x61, 0x00, 0x44, 0x04, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x3f,
    0x18, 0x2d, 0x44, 0x54, 0xfb, 0x21, 0x09, 0x40, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xf0, 0x7f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80,
    0x73, 0x00, 0x73, 0x54, 0x65, 0x73, 0x74, 0x20, 0x54, 0x65, 0x73, 0x74,
    0x00, 0x72, 0x61, 0x77, 0x00, 0x43, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x78, 0x56, 0x34, 0x12
};

static void
test_serialize_data(void)
{
    static const gdouble data[] = { 1.0, G_PI, HUGE_VAL, -0.0 };

    GwySerTest *sertest;
    GOutputStream *stream;
    GMemoryOutputStream *memstream;
    GError *error = NULL;
    gboolean ok;
    guint len;

    /* Less simple object */
    sertest = gwy_ser_test_new_filled(TRUE, data, G_N_ELEMENTS(data),
                                      "Test Test", 0x12345678);
    stream = g_memory_output_stream_new(malloc(200), 200, NULL, &free);
    memstream = G_MEMORY_OUTPUT_STREAM(stream);
    ok = gwy_serializable_serialize(GWY_SERIALIZABLE(sertest), stream, &error);
    g_assert(ok);
    g_assert_cmpint(sertest->done_called, ==, 0);
    len = g_memory_output_stream_get_data_size(memstream);
    g_assert_cmpuint(len, ==, sizeof(ser_test_data));
    g_assert(memcmp(g_memory_output_stream_get_data(memstream),
                    ser_test_data, sizeof(ser_test_data)) == 0);
    g_object_unref(stream);
    g_object_unref(sertest);
    g_clear_error(&error);
}

static const guchar ser_test_nested[] = {
    0x47, 0x77, 0x79, 0x53, 0x65, 0x72, 0x54, 0x65, 0x73, 0x74, 0x00, 0x67,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x72, 0x61, 0x77, 0x00, 0x43,
    0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x63, 0x68, 0x69, 0x6c, 0x64, 0x00, 0x6f, 0x47, 0x77, 0x79, 0x53, 0x65,
    0x72, 0x54, 0x65, 0x73, 0x74, 0x00, 0x3c, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x72, 0x61, 0x77, 0x00, 0x43, 0x04, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x63, 0x68, 0x69, 0x6c, 0x64,
    0x00, 0x6f, 0x47, 0x77, 0x79, 0x53, 0x65, 0x72, 0x54, 0x65, 0x73, 0x74,
    0x00, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x72, 0x61, 0x77,
    0x00, 0x43, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00
};

static void
test_serialize_nested(void)
{
    GwySerTest *sertest, *child, *grandchild;
    GOutputStream *stream;
    GMemoryOutputStream *memstream;
    GError *error = NULL;
    gboolean ok;
    guint len;

    /* Nested objects */
    sertest = g_object_newv(GWY_TYPE_SER_TEST, 0, NULL);
    child = sertest->child = g_object_newv(GWY_TYPE_SER_TEST, 0, NULL);
    grandchild = child->child = g_object_newv(GWY_TYPE_SER_TEST, 0, NULL);
    stream = g_memory_output_stream_new(malloc(200), 200, NULL, &free);
    memstream = G_MEMORY_OUTPUT_STREAM(stream);
    ok = gwy_serializable_serialize(GWY_SERIALIZABLE(sertest), stream, &error);
    g_assert(ok);
    g_assert_cmpint(sertest->done_called, ==, 0);
    g_assert_cmpint(child->done_called, ==, 0);
    g_assert_cmpint(grandchild->done_called, ==, 0);
    len = g_memory_output_stream_get_data_size(memstream);
    g_file_set_contents("foo.gwy", g_memory_output_stream_get_data(memstream),
                        len, NULL);
    g_assert_cmpuint(len, ==, sizeof(ser_test_nested));
    g_assert(memcmp(g_memory_output_stream_get_data(memstream),
                    ser_test_nested, sizeof(ser_test_nested)) == 0);
    g_object_unref(stream);
    g_object_unref(sertest);
    g_clear_error(&error);
}

static void
test_serialize_error(void)
{
    static const gdouble data[] = { 1.0, G_PI, HUGE_VAL, -0.0 };

    GwySerTest *sertest;
    GOutputStream *stream;
    GMemoryOutputStream *memstream;
    GError *error = NULL;
    gboolean ok;

    /* Too small buffer */
    sertest = gwy_ser_test_new_filled(TRUE, data, G_N_ELEMENTS(data),
                                      "Test Test", 0x12345678);
    stream = g_memory_output_stream_new(malloc(100), 100, NULL, &free);
    memstream = G_MEMORY_OUTPUT_STREAM(stream);
    ok = gwy_serializable_serialize(GWY_SERIALIZABLE(sertest), stream, &error);
    g_assert(!ok);
    g_assert_cmpint(sertest->done_called, ==, 0);
    g_object_unref(stream);
    g_object_unref(sertest);
    g_clear_error(&error);
}

int
main(int argc, char *argv[])
{
    g_test_init(&argc, &argv, NULL);
    g_type_init();

    g_test_add_func("/testlibgwy/error_list", test_error_list);
    g_test_add_func("/testlibgwy/pack", test_pack);
    g_test_add_func("/testlibgwy/serialize-simple", test_serialize_simple);
    g_test_add_func("/testlibgwy/serialize-data", test_serialize_data);
    g_test_add_func("/testlibgwy/serialize-nested", test_serialize_nested);
    g_test_add_func("/testlibgwy/serialize-error", test_serialize_error);
    g_test_add_func("/testlibgwy/sort", test_sort);

    return g_test_run();
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
