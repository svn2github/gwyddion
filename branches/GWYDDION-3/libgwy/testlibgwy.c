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

// The remaining members get zero-initialized which saves us from doing it.
static const GwySerializableItem default_items[] = {
    { .name = "flag",  .ctype = GWY_SERIALIZABLE_BOOLEAN,      },
    { .name = "data",  .ctype = GWY_SERIALIZABLE_DOUBLE_ARRAY, },
    { .name = "s",     .ctype = GWY_SERIALIZABLE_STRING,       },
    { .name = "raw",   .ctype = GWY_SERIALIZABLE_INT8_ARRAY,   },
    { .name = "child", .ctype = GWY_SERIALIZABLE_OBJECT,       },
};

#define add_item(id) \
    g_return_val_if_fail(items->len - items->n_items, 0); \
    items->items[items->n_items++] = it[id]; \
    n_items++

static gsize
gwy_ser_test_itemize(GwySerializable *serializable,
                     GwySerializableItems *items)
{
    GwySerTest *sertest = GWY_SER_TEST(serializable);
    GwySerializableItem it[G_N_ELEMENTS(default_items)];
    gsize n_items = 0;

    memcpy(it, default_items, sizeof(default_items));

    if (sertest->flag) {
        it[0].value.v_boolean = sertest->flag;
        add_item(0);
    }

    if (sertest->data) {
        it[1].value.v_double_array = sertest->data;
        it[1].array_size = sertest->len;
        add_item(1);
    }

    if (sertest->s) {
        it[2].value.v_string = sertest->s;
        add_item(2);
    }

    it[3].value.v_uint8_array = sertest->raw;
    it[3].array_size = 4;
    add_item(3);

    if (sertest->child) {
        it[4].value.v_object = G_OBJECT(sertest->child);
        add_item(4);
        gwy_serializable_itemize(GWY_SERIALIZABLE(sertest->child), items);
    }

    sertest->done_called--;

    return n_items;
}

static GObject*
gwy_ser_test_construct(GwySerializableItems *items,
                       GwyErrorList **error_list)
{
    GwySerializableItem it[G_N_ELEMENTS(default_items)];
    gpointer child;

    memcpy(it, default_items, sizeof(default_items));
    gwy_serializable_filter_items(it, G_N_ELEMENTS(it), items, "GwySerTest",
                                  error_list);

    if (it[3].array_size != 4) {
        gwy_error_list_add(error_list, GWY_SERIALIZABLE_ERROR,
                           GWY_SERIALIZABLE_ERROR_INVALID,
                           "Item ‘raw’ has %" G_GSIZE_FORMAT " bytes "
                           "instead of 4.",
                           it[3].array_size);
        goto fail;
    }
    if ((child = it[4].value.v_object) && !GWY_IS_SER_TEST(child)) {
        gwy_error_list_add(error_list, GWY_SERIALIZABLE_ERROR,
                           GWY_SERIALIZABLE_ERROR_INVALID,
                           "Item ‘child’ is %s instead of GwySerTest.",
                           G_OBJECT_TYPE_NAME(child));
        goto fail;
    }

    GwySerTest *sertest = g_object_newv(GWY_TYPE_SER_TEST, 0, NULL);

    sertest->flag = it[0].value.v_boolean;
    sertest->len  = it[1].array_size;
    sertest->data = it[1].value.v_double_array;
    sertest->s    = it[2].value.v_string;
    memcpy(sertest->raw, it[3].value.v_uint8_array, 4);
    g_free(it[3].value.v_uint8_array);
    sertest->child = child;

    return G_OBJECT(sertest);

fail:
    GWY_FREE(it[1].value.v_double_array);
    GWY_FREE(it[2].value.v_string);
    GWY_FREE(it[3].value.v_uint8_array);
    GWY_OBJECT_UNREF(it[4].value.v_object);

    return NULL;
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
    iface->n_items   = gwy_ser_test_n_items;
    iface->itemize   = gwy_ser_test_itemize;
    iface->done      = gwy_ser_test_done;
    iface->construct = gwy_ser_test_construct;
    /*
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

static void
test_deserialize_simple(void)
{
    GwySerTest *sertest;
    GwyErrorList *error_list = NULL;
    gsize bytes_consumed;

    sertest = (GwySerTest*)gwy_serializable_deserialize(ser_test_simple,
                                                        sizeof(ser_test_simple),
                                                        &bytes_consumed,
                                                        &error_list);
    g_assert(sertest);
    g_assert(GWY_IS_SER_TEST(sertest));
    g_assert_cmpuint(bytes_consumed, ==, sizeof(ser_test_simple));
    g_assert_cmpuint(g_slist_length(error_list), ==, 0);
    g_assert(sertest->flag == FALSE);
    g_assert_cmpuint(sertest->len, ==, 0);
    g_assert(sertest->data == NULL);
    g_assert(sertest->s == NULL);

    GWY_OBJECT_UNREF(sertest);
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

static void
test_deserialize_data(void)
{
    static const gdouble data[] = { 1.0, G_PI, HUGE_VAL, -0.0 };

    GwySerTest *sertest;
    GwyErrorList *error_list = NULL;
    gsize bytes_consumed;

    sertest = (GwySerTest*)gwy_serializable_deserialize(ser_test_data,
                                                        sizeof(ser_test_data),
                                                        &bytes_consumed,
                                                        &error_list);
    g_assert(sertest);
    g_assert(GWY_IS_SER_TEST(sertest));
    g_assert_cmpuint(bytes_consumed, ==, sizeof(ser_test_data));
    g_assert_cmpuint(g_slist_length(error_list), ==, 0);
    g_assert(sertest->flag == TRUE);
    g_assert_cmpuint(sertest->len, ==, G_N_ELEMENTS(data));
    g_assert(memcmp(sertest->data, data, sizeof(data)) == 0);
    g_assert_cmpstr(sertest->s, ==, "Test Test");

    GWY_OBJECT_UNREF(sertest);
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
    g_assert_cmpuint(len, ==, sizeof(ser_test_nested));
    g_assert(memcmp(g_memory_output_stream_get_data(memstream),
                    ser_test_nested, sizeof(ser_test_nested)) == 0);
    g_object_unref(stream);
    g_object_unref(sertest);
    g_clear_error(&error);
}

static void
test_deserialize_nested(void)
{
    GwySerTest *sertest, *child;
    GwyErrorList *error_list = NULL;
    gsize bytes_consumed;

    sertest = (GwySerTest*)gwy_serializable_deserialize(ser_test_nested,
                                                        sizeof(ser_test_nested),
                                                        &bytes_consumed,
                                                        &error_list);
    g_assert(sertest);
    g_assert(GWY_IS_SER_TEST(sertest));
    g_assert_cmpuint(bytes_consumed, ==, sizeof(ser_test_nested));
    g_assert_cmpuint(g_slist_length(error_list), ==, 0);
    g_assert(sertest->flag == FALSE);
    g_assert_cmpuint(sertest->len, ==, 0);
    g_assert(sertest->data == NULL);
    g_assert(sertest->s == NULL);

    child = sertest->child;
    g_assert(child);
    g_assert(GWY_IS_SER_TEST(child));
    g_assert_cmpuint(bytes_consumed, ==, sizeof(ser_test_nested));
    g_assert_cmpuint(g_slist_length(error_list), ==, 0);
    g_assert(child->flag == FALSE);
    g_assert_cmpuint(child->len, ==, 0);
    g_assert(child->data == NULL);
    g_assert(child->s == NULL);

    child = child->child;
    g_assert(child);
    g_assert(GWY_IS_SER_TEST(child));
    g_assert_cmpuint(bytes_consumed, ==, sizeof(ser_test_nested));
    g_assert_cmpuint(g_slist_length(error_list), ==, 0);
    g_assert(child->flag == FALSE);
    g_assert_cmpuint(child->len, ==, 0);
    g_assert(child->data == NULL);
    g_assert(child->s == NULL);
    g_assert(child->child == NULL);

    GWY_OBJECT_UNREF(sertest);
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

static void
test_deserialize_garbage(void)
{
    typedef struct { const guchar *buffer; gsize size; } OrigBuffer;
    static const OrigBuffer origs[] = {
        { ser_test_simple, sizeof(ser_test_simple), },
        { ser_test_data,   sizeof(ser_test_data),   },
        { ser_test_nested, sizeof(ser_test_nested), },
    };
    GRand *rng = g_rand_new();

    /* Make it realy reproducible. */
    g_rand_set_seed(rng, 42);

    gsize niter = g_test_slow() ? 100000 : 10000;

    for (gsize i = 0; i < niter; i++) {
        GObject *object;
        GwyErrorList *error_list = NULL;
        gsize bytes_consumed;
        guint n;

        /* Choose a serialized representation and perturb it. */
        n = g_rand_int_range(rng, 0, G_N_ELEMENTS(origs));
        GArray *buffer = g_array_sized_new(FALSE, FALSE, 1, origs[n].size + 20);
        g_array_append_vals(buffer, origs[n].buffer, origs[n].size);

        n = g_rand_int_range(rng, 1, 12);
        for (guint j = 0; j < n; j++) {
            guint pos, pos2;
            guint8 b, b2;

            switch (g_rand_int_range(rng, 0, 3)) {
                case 0:
                pos = g_rand_int_range(rng, 0, buffer->len);
                g_array_remove_index(buffer, pos);
                break;

                case 1:
                pos = g_rand_int_range(rng, 0, buffer->len+1);
                b = g_rand_int_range(rng, 0, 0x100);
                g_array_insert_val(buffer, pos, b);
                break;

                case 2:
                pos = g_rand_int_range(rng, 0, buffer->len);
                pos2 = g_rand_int_range(rng, 0, buffer->len);
                b = g_array_index(buffer, guint8, pos);
                b2 = g_array_index(buffer, guint8, pos2);
                g_array_index(buffer, guint8, pos) = b2;
                g_array_index(buffer, guint8, pos2) = b;
                break;
            }
        }

        object = gwy_serializable_deserialize((const guchar*)buffer->data,
                                              buffer->len,
                                              &bytes_consumed,
                                              &error_list);

        /* No checks.  The goal is not to crash... */
        g_array_free(buffer, TRUE);
        GWY_OBJECT_UNREF(object);
        gwy_error_list_clear(&error_list);
    }

    g_rand_free(rng);
}

/***************************************************************************
 *
 * Units
 *
 ***************************************************************************/

static void
test_unit_parse(void)
{
    GwyUnit *u1, *u2, *u3, *u4, *u5, *u6, *u7, *u8, *u9;
    gint pw1, pw2, pw3, pw4, pw5, pw6, pw7, pw8, pw9;

    /* Simple notations */
    u1 = gwy_unit_new_from_string("m", &pw1);
    u2 = gwy_unit_new_from_string("km", &pw2);
    u3 = gwy_unit_new_from_string("Å", &pw3);

    g_assert(gwy_unit_equal(u1, u2));
    g_assert(gwy_unit_equal(u2, u3));
    g_assert(gwy_unit_equal(u3, u1));
    g_assert_cmpint(pw1, ==, 0);
    g_assert_cmpint(pw2, ==, 3);
    g_assert_cmpint(pw3, ==, -10);

    g_object_unref(u1);
    g_object_unref(u2);
    g_object_unref(u3);

    /* Powers and comparision */
    u4 = gwy_unit_new_from_string("um s^-1", &pw4);
    u5 = gwy_unit_new_from_string("mm/ps", &pw5);
    u6 = gwy_unit_new_from_string("μs<sup>-1</sup> cm", &pw6);

    g_assert(gwy_unit_equal(u4, u5));
    g_assert(gwy_unit_equal(u5, u6));
    g_assert(gwy_unit_equal(u6, u4));
    g_assert_cmpint(pw4, ==, -6);
    g_assert_cmpint(pw5, ==, 9);
    g_assert_cmpint(pw6, ==, 4);

    g_object_unref(u4);
    g_object_unref(u5);
    g_object_unref(u6);

    /* Cancellation */
    u7 = gwy_unit_new_from_string(NULL, &pw7);
    u8 = gwy_unit_new_from_string("10%", &pw8);
    u9 = gwy_unit_new_from_string("m^3 cm^-2/km", &pw9);

    g_assert(gwy_unit_equal(u7, u8));
    g_assert(gwy_unit_equal(u8, u9));
    g_assert(gwy_unit_equal(u9, u7));
    g_assert_cmpint(pw7, ==, 0);
    g_assert_cmpint(pw8, ==, -1);
    g_assert_cmpint(pw9, ==, 1);

    g_object_unref(u7);
    g_object_unref(u8);
    g_object_unref(u9);
}

static void
test_unit_arithmetic(void)
{
    GwyUnit *u1, *u2, *u3, *u4, *u5, *u6, *u7, *u8, *u9, *u0;

    u1 = gwy_unit_new_from_string("kg m s^-2", NULL);
    u2 = gwy_unit_new_from_string("s/kg", NULL);
    u3 = gwy_unit_new_from_string("m/s", NULL);

    u4 = gwy_unit_multiply(NULL, u1, u2);
    g_assert(gwy_unit_equal(u3, u4));

    u5 = gwy_unit_power(NULL, u1, -1);
    u6 = gwy_unit_power_multiply(NULL, u5, 2, u2, -2);
    u7 = gwy_unit_power(NULL, u3, -2);
    g_assert(gwy_unit_equal(u6, u7));

    u8 = gwy_unit_nth_root(NULL, u6, 2);
    gwy_unit_power(u8, u8, -1);
    g_assert(gwy_unit_equal(u8, u3));

    gwy_unit_divide(u8, u8, u3);
    u0 = gwy_unit_new();
    g_assert(gwy_unit_equal(u8, u0));

    u9 = gwy_unit_power(NULL, u3, 4);
    gwy_unit_power_multiply(u9, u9, 1, u1, -3);
    gwy_unit_power_multiply(u9, u2, 3, u9, -1);
    gwy_unit_multiply(u9, u9, u3);
    g_assert(gwy_unit_equal(u9, u0));

    g_object_unref(u1);
    g_object_unref(u2);
    g_object_unref(u3);
    g_object_unref(u4);
    g_object_unref(u5);
    g_object_unref(u6);
    g_object_unref(u7);
    g_object_unref(u8);
    g_object_unref(u9);
    g_object_unref(u0);
}

/***************************************************************************
 *
 * Main
 *
 ***************************************************************************/

int
main(int argc, char *argv[])
{
    g_test_init(&argc, &argv, NULL);
    g_type_init();

    g_test_add_func("/testlibgwy/error_list", test_error_list);
    g_test_add_func("/testlibgwy/pack", test_pack);
    g_test_add_func("/testlibgwy/sort", test_sort);
    g_test_add_func("/testlibgwy/serialize-simple", test_serialize_simple);
    g_test_add_func("/testlibgwy/serialize-data", test_serialize_data);
    g_test_add_func("/testlibgwy/serialize-nested", test_serialize_nested);
    g_test_add_func("/testlibgwy/serialize-error", test_serialize_error);
    /* Require error_list */
    g_test_add_func("/testlibgwy/deserialize-simple", test_deserialize_simple);
    g_test_add_func("/testlibgwy/deserialize-data", test_deserialize_data);
    g_test_add_func("/testlibgwy/deserialize-nested", test_deserialize_nested);
    g_test_add_func("/testlibgwy/deserialize-garbage", test_deserialize_garbage);
    /* Require serializable */
    g_test_add_func("/testlibgwy/unit-parse", test_unit_parse);
    g_test_add_func("/testlibgwy/unit-arithmetic", test_unit_arithmetic);

    return g_test_run();
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
