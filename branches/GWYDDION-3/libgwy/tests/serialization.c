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

#include "testlibgwy.h"
#include <stdlib.h>

/***************************************************************************
 *
 * Serialization and deserialization
 *
 ***************************************************************************/

#define gwy_ser_test_duplicate(ser_test) \
        (GWY_SER_TEST(gwy_serializable_duplicate(GWY_SERIALIZABLE(ser_test))))

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
    gdouble dbl;
    gint16 i16;
    gint32 i32;
    gint64 i64;
    gchar **strlist;
    GwySerTest *child;

    /* Testing stuff. */
    gint done_called;
};

static gsize
gwy_ser_test_n_items(GwySerializable *serializable)
{
    GwySerTest *sertest = GWY_SER_TEST(serializable);

    return 10 + (sertest->child
                 ? gwy_serializable_n_items(GWY_SERIALIZABLE(sertest->child))
                 : 0);
}

// The remaining members get zero-initialized which saves us from doing it.
static const GwySerializableItem default_items[] = {
    /*0*/ { .name = "flag",  .ctype = GWY_SERIALIZABLE_BOOLEAN,      },
    /*1*/ { .name = "data",  .ctype = GWY_SERIALIZABLE_DOUBLE_ARRAY, },
    /*2*/ { .name = "s",     .ctype = GWY_SERIALIZABLE_STRING,       },
    /*3*/ { .name = "raw",   .ctype = GWY_SERIALIZABLE_INT8_ARRAY,   },
    /*4*/ { .name = "child", .ctype = GWY_SERIALIZABLE_OBJECT,       },
    /*5*/ { .name = "dbl",   .ctype = GWY_SERIALIZABLE_DOUBLE,       },
    /*6*/ { .name = "i16",   .ctype = GWY_SERIALIZABLE_INT16,        },
    /*7*/ { .name = "i32",   .ctype = GWY_SERIALIZABLE_INT32,        },
    /*8*/ { .name = "i64",   .ctype = GWY_SERIALIZABLE_INT64,        },
    /*9*/ { .name = "ss",    .ctype = GWY_SERIALIZABLE_STRING_ARRAY, },
};

#define add_item(id) \
    g_return_val_if_fail(items->len - items->n, 0); \
    items->items[items->n++] = it[id]; \
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

    if (sertest->dbl != G_LN2) {
        it[5].value.v_double = sertest->dbl;
        add_item(5);
    }

    if (sertest->i16) {
        it[6].value.v_int16 = sertest->i16;
        add_item(6);
    }

    if (sertest->i32) {
        it[7].value.v_int32 = sertest->i32;
        add_item(7);
    }

    if (sertest->i64) {
        it[8].value.v_int64 = sertest->i64;
        add_item(8);
    }

    if (sertest->strlist) {
        it[9].value.v_string_array = sertest->strlist;
        it[9].array_size = g_strv_length(sertest->strlist);
        add_item(9);
    }

    sertest->done_called--;

    return n_items;
}

static gboolean
gwy_ser_test_construct(GwySerializable *serializable,
                       GwySerializableItems *items,
                       GwyErrorList **error_list)
{
    GwySerializableItem it[G_N_ELEMENTS(default_items)];
    gpointer child;

    memcpy(it, default_items, sizeof(default_items));
    gwy_deserialize_filter_items(it, G_N_ELEMENTS(it), items, NULL,
                                 "GwySerTest", error_list);

    if (it[3].array_size != 4) {
        gwy_error_list_add(error_list, GWY_DESERIALIZE_ERROR,
                           GWY_DESERIALIZE_ERROR_INVALID,
                           "Item ‘raw’ has %" G_GSIZE_FORMAT " bytes "
                           "instead of 4.",
                           it[3].array_size);
        goto fail;
    }
    if ((child = it[4].value.v_object) && !GWY_IS_SER_TEST(child)) {
        gwy_error_list_add(error_list, GWY_DESERIALIZE_ERROR,
                           GWY_DESERIALIZE_ERROR_INVALID,
                           "Item ‘child’ is %s instead of GwySerTest.",
                           G_OBJECT_TYPE_NAME(child));
        goto fail;
    }

    GwySerTest *sertest = GWY_SER_TEST(serializable);

    sertest->flag = it[0].value.v_boolean;
    sertest->len  = it[1].array_size;
    sertest->data = it[1].value.v_double_array;
    sertest->s    = it[2].value.v_string;
    sertest->dbl  = it[5].value.v_double;
    sertest->i16  = it[6].value.v_int16;
    sertest->i32  = it[7].value.v_int32;
    sertest->i64  = it[8].value.v_int64;
    memcpy(sertest->raw, it[3].value.v_uint8_array, 4);
    g_free(it[3].value.v_uint8_array);
    sertest->child = child;
    if (it[9].value.v_string_array) {
        sertest->strlist = g_new0(gchar*, it[9].array_size + 1);
        memcpy(sertest->strlist, it[9].value.v_string_array,
               it[9].array_size*sizeof(gchar*));
        g_free(it[9].value.v_string_array);
        it[9].value.v_string_array = NULL;
        it[9].array_size = 0;
    }

    return TRUE;

fail:
    GWY_FREE(it[1].value.v_double_array);
    GWY_FREE(it[2].value.v_string);
    GWY_FREE(it[3].value.v_uint8_array);
    GWY_OBJECT_UNREF(it[4].value.v_object);
    if (it[9].value.v_string_array) {
        for (gsize i = 0; i < it[9].array_size; i++)
            g_free(it[9].value.v_string_array[i]);
        g_free(it[9].value.v_string_array);
        it[9].value.v_string_array = NULL;
        it[9].array_size = 0;
    }

    return FALSE;
}

static void
gwy_ser_test_done(GwySerializable *serializable)
{
    GwySerTest *sertest = GWY_SER_TEST(serializable);

    sertest->done_called++;
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

static GObject*
gwy_ser_test_duplicate_(GwySerializable *serializable)
{
    GwySerTest *sertest = GWY_SER_TEST(serializable);
    GwySerTest *copy = gwy_ser_test_new_filled(sertest->flag,
                                               sertest->data, sertest->len,
                                               sertest->s, 0);
    copy->dbl = sertest->dbl;
    copy->i16 = sertest->i16;
    copy->i32 = sertest->i32;
    copy->i64 = sertest->i64;
    memcpy(copy->raw, sertest->raw, 4);
    copy->strlist = g_strdupv(sertest->strlist);
    copy->child = gwy_ser_test_duplicate(sertest->child);

    return G_OBJECT(copy);
}

static void
gwy_ser_test_serializable_init(GwySerializableInterface *iface)
{
    iface->n_items   = gwy_ser_test_n_items;
    iface->itemize   = gwy_ser_test_itemize;
    iface->done      = gwy_ser_test_done;
    iface->construct = gwy_ser_test_construct;
    iface->duplicate = gwy_ser_test_duplicate_;
    /*
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
    g_strfreev(sertest->strlist);
}

static void
gwy_ser_test_class_init(GwySerTestClass *klass)
{
    GObjectClass *g_object_class = G_OBJECT_CLASS(klass);

    g_object_class->dispose = gwy_ser_test_dispose;
    g_object_class->finalize = gwy_ser_test_finalize;
}

static void
gwy_ser_test_init(GwySerTest *sertest)
{
    sertest->dbl = G_LN2;
}

static const guchar ser_test_simple[] = {
    0x47, 0x77, 0x79, 0x53, 0x65, 0x72, 0x54, 0x65, 0x73, 0x74, 0x00, 0x11,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x72, 0x61, 0x77, 0x00, 0x43,
    0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/* Serialize a simple GwySerTest object and check if it matches byte-for-byte
 * with the stored representation above. */
void
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
    ok = gwy_serialize_gio(GWY_SERIALIZABLE(sertest), stream, &error);
    g_assert(ok);
    g_assert_cmpint(sertest->done_called, ==, 0);
    len = g_memory_output_stream_get_data_size(memstream);
    g_assert_cmpuint(len, ==, sizeof(ser_test_simple));
    g_assert_cmpuint(memcmp(g_memory_output_stream_get_data(memstream),
                            ser_test_simple, sizeof(ser_test_simple)), ==, 0);
    g_object_unref(stream);
    g_object_unref(sertest);
    g_clear_error(&error);
}

/* Deserialize a simple GwySerTest object and check if the restored data match
 * what we expect (mostly defaults). */
void
test_deserialize_simple(void)
{
    GwySerTest *sertest;
    GwyErrorList *error_list = NULL;
    gsize bytes_consumed;

    g_type_class_ref(GWY_TYPE_SER_TEST);
    sertest = (GwySerTest*)gwy_deserialize_memory(ser_test_simple,
                                                  sizeof(ser_test_simple),
                                                  &bytes_consumed, &error_list);
    g_assert(sertest);
    g_assert(GWY_IS_SER_TEST(sertest));
    g_assert_cmpuint(bytes_consumed, ==, sizeof(ser_test_simple));
    g_assert_cmpuint(g_slist_length(error_list), ==, 0);
    g_assert_cmpuint(sertest->flag, ==, FALSE);
    g_assert_cmpuint(sertest->len, ==, 0);
    g_assert(sertest->data == NULL);
    g_assert(sertest->s == NULL);

    GWY_OBJECT_UNREF(sertest);
}

static const guchar ser_test_data[] = {
    0x47, 0x77, 0x79, 0x53, 0x65, 0x72, 0x54, 0x65, 0x73, 0x74, 0x00, 0xad,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x66, 0x6c, 0x61, 0x67, 0x00,
    0x62, 0x01, 0x64, 0x61, 0x74, 0x61, 0x00, 0x44, 0x04, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x3f,
    0x18, 0x2d, 0x44, 0x54, 0xfb, 0x21, 0x09, 0x40, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xf0, 0x7f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80,
    0x73, 0x00, 0x73, 0x54, 0x65, 0x73, 0x74, 0x20, 0x54, 0x65, 0x73, 0x74,
    0x00, 0x72, 0x61, 0x77, 0x00, 0x43, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x78, 0x56, 0x34, 0x12, 0x64, 0x62, 0x6c, 0x00, 0x64, 0x5e,
    0x5a, 0x75, 0x04, 0x23, 0xcf, 0xe2, 0x3f, 0x69, 0x31, 0x36, 0x00, 0x68,
    0x34, 0x12, 0x69, 0x33, 0x32, 0x00, 0x69, 0xef, 0xbe, 0xad, 0xde, 0x69,
    0x36, 0x34, 0x00, 0x71, 0x80, 0x70, 0x60, 0x50, 0x40, 0x30, 0x20, 0x10,
    0x73, 0x73, 0x00, 0x53, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x46, 0x69, 0x72, 0x73, 0x74, 0x20, 0x74, 0x68, 0x69, 0x6e, 0x67, 0x73,
    0x20, 0x66, 0x69, 0x72, 0x73, 0x74, 0x00, 0x57, 0x61, 0x69, 0x74, 0x20,
    0x61, 0x20, 0x73, 0x65, 0x63, 0x6f, 0x6e, 0x64, 0x2e, 0x2e, 0x2e, 0x00
};

/* Serialize a GwySerTest object with some non-trivial data and check if it
 * matches byte-for-byte with the stored representation above. */
void
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
    sertest->i16 = 0x1234;
    sertest->i32 = (gint32)0xdeadbeef;
    sertest->i64 = G_GINT64_CONSTANT(0x1020304050607080);
    sertest->dbl = sin(G_PI/5);
    sertest->strlist = g_new0(gchar*, 3);
    sertest->strlist[0] = g_strdup("First things first");
    sertest->strlist[1] = g_strdup("Wait a second...");
    stream = g_memory_output_stream_new(malloc(200), 200, NULL, &free);
    memstream = G_MEMORY_OUTPUT_STREAM(stream);
    ok = gwy_serialize_gio(GWY_SERIALIZABLE(sertest), stream, &error);
    g_assert(ok);
    g_assert_cmpint(sertest->done_called, ==, 0);
    len = g_memory_output_stream_get_data_size(memstream);
    g_assert_cmpuint(len, ==, sizeof(ser_test_data));
    g_assert_cmpint(memcmp(g_memory_output_stream_get_data(memstream),
                           ser_test_data, sizeof(ser_test_data)), ==, 0);
    g_object_unref(stream);
    g_object_unref(sertest);
    g_clear_error(&error);
}

/* Deserialize a GwySerTest object with some non-trivial data and check if the
 * restored data match what we expect. */
void
test_deserialize_data(void)
{
    static const gdouble data[] = { 1.0, G_PI, HUGE_VAL, -0.0 };

    GwySerTest *sertest;
    GwyErrorList *error_list = NULL;
    gsize bytes_consumed;

    g_type_class_ref(GWY_TYPE_SER_TEST);
    sertest = (GwySerTest*)gwy_deserialize_memory(ser_test_data,
                                                  sizeof(ser_test_data),
                                                  &bytes_consumed, &error_list);
    g_assert(sertest);
    g_assert(GWY_IS_SER_TEST(sertest));
    g_assert_cmpuint(bytes_consumed, ==, sizeof(ser_test_data));
    g_assert_cmpuint(g_slist_length(error_list), ==, 0);
    g_assert_cmpint(sertest->flag, ==, TRUE);
    g_assert_cmpuint(sertest->len, ==, G_N_ELEMENTS(data));
    g_assert_cmpint(memcmp(sertest->data, data, sizeof(data)), ==, 0);
    g_assert_cmpstr(sertest->s, ==, "Test Test");
    g_assert_cmpfloat(sertest->dbl, ==, sin(G_PI/5));
    g_assert_cmpint(sertest->i16, ==, 0x1234);
    g_assert_cmpint(sertest->i32, ==, (gint32)0xdeadbeef);
    g_assert_cmpint(sertest->i64, ==, G_GINT64_CONSTANT(0x1020304050607080));
    g_assert_cmpuint(g_strv_length(sertest->strlist), ==, 2);
    g_assert_cmpstr(sertest->strlist[0], ==, "First things first");
    g_assert_cmpstr(sertest->strlist[1], ==, "Wait a second...");

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

/* Serialize a GwySerTest object with contained GwySerTest objects (2 levels
 * deep) and check if it matches byte-for-byte with the stored representation
 * above. */
void
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
    ok = gwy_serialize_gio(GWY_SERIALIZABLE(sertest), stream, &error);
    g_assert(ok);
    g_assert_cmpint(sertest->done_called, ==, 0);
    g_assert_cmpint(child->done_called, ==, 0);
    g_assert_cmpint(grandchild->done_called, ==, 0);
    len = g_memory_output_stream_get_data_size(memstream);
    g_assert_cmpuint(len, ==, sizeof(ser_test_nested));
    g_assert_cmpint(memcmp(g_memory_output_stream_get_data(memstream),
                           ser_test_nested, sizeof(ser_test_nested)), ==, 0);
    g_object_unref(stream);
    g_object_unref(sertest);
    g_clear_error(&error);
}

/* Deserialize a GwySerTest object with contained GwySerTest objects (2 levels
 * deep) and check if the restored object tree looks as expected. */
void
test_deserialize_nested(void)
{
    GwySerTest *sertest, *child;
    GwyErrorList *error_list = NULL;
    gsize bytes_consumed;

    g_type_class_ref(GWY_TYPE_SER_TEST);
    sertest = (GwySerTest*)gwy_deserialize_memory(ser_test_nested,
                                                  sizeof(ser_test_nested),
                                                  &bytes_consumed, &error_list);
    g_assert(sertest);
    g_assert(GWY_IS_SER_TEST(sertest));
    g_assert_cmpuint(bytes_consumed, ==, sizeof(ser_test_nested));
    g_assert_cmpuint(g_slist_length(error_list), ==, 0);
    g_assert_cmpint(sertest->flag, ==, FALSE);
    g_assert_cmpuint(sertest->len, ==, 0);
    g_assert(sertest->data == NULL);
    g_assert(sertest->s == NULL);

    child = sertest->child;
    g_assert(child);
    g_assert(GWY_IS_SER_TEST(child));
    g_assert_cmpuint(bytes_consumed, ==, sizeof(ser_test_nested));
    g_assert_cmpuint(g_slist_length(error_list), ==, 0);
    g_assert_cmpint(child->flag, ==, FALSE);
    g_assert_cmpuint(child->len, ==, 0);
    g_assert(child->data == NULL);
    g_assert(child->s == NULL);

    child = child->child;
    g_assert(child);
    g_assert(GWY_IS_SER_TEST(child));
    g_assert_cmpuint(bytes_consumed, ==, sizeof(ser_test_nested));
    g_assert_cmpuint(g_slist_length(error_list), ==, 0);
    g_assert_cmpint(child->flag, ==, FALSE);
    g_assert_cmpuint(child->len, ==, 0);
    g_assert(child->data == NULL);
    g_assert(child->s == NULL);
    g_assert(child->child == NULL);

    GWY_OBJECT_UNREF(sertest);
}

/* Serialization to a one-byte too short buffer, check failure. */
void
test_serialize_error(void)
{
    static const gdouble data[] = { 1.0, G_PI, HUGE_VAL, -0.0 };

    GwySerTest *sertest;
    GOutputStream *stream;
    GError *error = NULL;
    gboolean ok;

    /* Too small buffer */
    sertest = gwy_ser_test_new_filled(TRUE, data, G_N_ELEMENTS(data),
                                      "Test Test", 0x12345678);
    for (gsize i = 1; i < 102; i++) {
        stream = g_memory_output_stream_new(malloc(i), i, NULL, &free);
        ok = gwy_serialize_gio(GWY_SERIALIZABLE(sertest), stream, &error);
        g_assert(!ok);
        g_assert(error);
        g_assert_cmpint(sertest->done_called, ==, 0);
        g_object_unref(stream);
        g_clear_error(&error);
    }
    g_object_unref(sertest);
}

/* Randomly perturb serialized object representations above and try to
 * deserialize the result. */
void
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

    g_type_class_ref(GWY_TYPE_SER_TEST);
    for (gsize i = 0; i < niter; i++) {
        GObject *object = NULL;
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

        object = gwy_deserialize_memory((const guchar*)buffer->data,
                                        buffer->len,
                                        &bytes_consumed, &error_list);

        /* No checks.  The goal is not to crash... */
        g_array_free(buffer, TRUE);
        GWY_OBJECT_UNREF(object);
        gwy_error_list_clear(&error_list);
    }

    g_rand_free(rng);
}

gboolean
values_are_equal(const GValue *value1,
                 const GValue *value2)
{
    if (!value1 || !value2)
        return FALSE;

    if (!G_IS_VALUE(value1) || !G_IS_VALUE(value2))
        return FALSE;

    GType type = G_VALUE_TYPE(value1);
    if (type != G_VALUE_TYPE(value2))
        return FALSE;

    switch (type) {
        case G_TYPE_BOOLEAN:
        return !g_value_get_boolean(value1) == !g_value_get_boolean(value2);

        case G_TYPE_CHAR:
        return g_value_get_char(value1) == g_value_get_char(value2);

        case G_TYPE_INT:
        return g_value_get_int(value1) == g_value_get_int(value2);

        case G_TYPE_UINT:
        return g_value_get_uint(value1) == g_value_get_uint(value2);

        case G_TYPE_INT64:
        return g_value_get_int64(value1) == g_value_get_int64(value2);

        case G_TYPE_UINT64:
        return g_value_get_uint64(value1) == g_value_get_uint64(value2);

        case G_TYPE_DOUBLE:
        return fabs(g_value_get_double(value1) - g_value_get_double(value2))
               <= 2e-16*(fabs(g_value_get_double(value1))
                         + fabs(g_value_get_double(value2)));

        case G_TYPE_STRING:
        return (!g_value_get_string(value1) && !g_value_get_string(value2))
               || gwy_strequal(g_value_get_string(value1),
                               g_value_get_string(value2));
    }

    if (g_type_is_a(type, G_TYPE_ENUM))
        return g_value_get_enum(value1) == g_value_get_enum(value2);

    if (g_type_is_a(type, G_TYPE_OBJECT))
        return compare_properties(g_value_get_object(value1),
                                  g_value_get_object(value2));

    if (g_type_is_a(type, G_TYPE_BOXED))
        return gwy_serializable_boxed_equal(type,
                                            g_value_get_boxed(value1),
                                            g_value_get_boxed(value2));

    g_warning("Cannot test values of type %s for equality.  Extend me!\n",
              g_type_name(type));
    return TRUE;
}

gboolean
compare_properties(GObject *object,
                   GObject *reference)
{
    g_assert(G_OBJECT_TYPE(object) == G_OBJECT_TYPE(reference));
    GObjectClass *klass = G_OBJECT_GET_CLASS(reference);
    guint nprops;
    GParamSpec **props = g_object_class_list_properties(klass, &nprops);
    for (guint i = 0; i < nprops; i++) {
        GValue value1, value2;
        //g_printerr("%s\n", props[i]->name);
        gwy_clear(&value1, 1);
        gwy_clear(&value2, 1);
        g_value_init(&value1, props[i]->value_type);
        g_value_init(&value2, props[i]->value_type);
        g_object_get_property(object, props[i]->name, &value1);
        g_object_get_property(reference, props[i]->name, &value2);
        g_assert(values_are_equal(&value1, &value2));
        g_value_unset(&value1);
        g_value_unset(&value2);
    }
    g_free(props);
    return TRUE;
}

GObject*
serialize_and_back(GObject *object,
                   CompareObjectDataFunc compare)
{
    enum { buffer_size = 0x10000 };

    g_assert(GWY_IS_SERIALIZABLE(object));
    GOutputStream *stream = g_memory_output_stream_new(malloc(buffer_size),
                                                       buffer_size, NULL,
                                                       &free);
    GMemoryOutputStream *memstream = G_MEMORY_OUTPUT_STREAM(stream);
    GError *error = NULL;
    gboolean ok = gwy_serialize_gio(GWY_SERIALIZABLE(object), stream, &error);
    g_assert(ok);
    g_assert(!error);
    gsize datalen = g_memory_output_stream_get_data_size(memstream);
    gpointer data = g_memory_output_stream_get_data(memstream);
    //g_file_set_contents("ser.gwy", data, datalen, NULL);
    gsize bytes_consumed = 0;
    GwyErrorList *error_list = NULL;
    GObject *retval = gwy_deserialize_memory(data, datalen,
                                             &bytes_consumed, &error_list);
    if (error_list)
        dump_error_list(error_list);
    g_assert(!error_list);
    g_assert(retval);
    g_assert_cmpuint(G_OBJECT_TYPE(retval), ==, G_OBJECT_TYPE(object));
    g_assert_cmpuint(bytes_consumed, ==, datalen);
    g_object_unref(stream);
    compare_properties(retval, object);
    if (compare)
        compare(retval, object);
    return retval;
}

void
serializable_duplicate(GwySerializable *serializable,
                       CompareObjectDataFunc compare)
{
    g_assert(GWY_IS_SERIALIZABLE(serializable));
    GwySerializable *copy = GWY_SERIALIZABLE(gwy_serializable_duplicate(serializable));
    g_assert(GWY_IS_SERIALIZABLE(copy));
    compare_properties(G_OBJECT(copy), G_OBJECT(serializable));
    if (compare)
        compare(G_OBJECT(copy), G_OBJECT(serializable));
    g_object_unref(copy);
}

void
serializable_assign(GwySerializable *serializable,
                    CompareObjectDataFunc compare)
{
    g_assert(GWY_IS_SERIALIZABLE(serializable));
    GType type = G_OBJECT_TYPE(serializable);
    g_assert(type);
    GwySerializable *copy = GWY_SERIALIZABLE(g_object_new(type, NULL));
    gwy_serializable_assign(copy, serializable);
    compare_properties(G_OBJECT(copy), G_OBJECT(serializable));
    if (compare)
        compare(G_OBJECT(copy), G_OBJECT(serializable));
    g_object_unref(copy);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
