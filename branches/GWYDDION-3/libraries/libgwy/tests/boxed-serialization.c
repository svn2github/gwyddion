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

#include "testlibgwy.h"
#include <stdlib.h>

/***************************************************************************
 *
 * Serialization and deserialization of boxed types
 *
 ***************************************************************************/

#define GWY_TYPE_SER_BOX_TEST \
    (gwy_ser_box_test_get_type())
#define GWY_SER_BOX_TEST(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_SER_BOX_TEST, GwySerBoxTest))
#define GWY_IS_SER_BOX_TEST(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_SER_BOX_TEST))
#define GWY_SER_BOX_TEST_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_SER_BOX_TEST, GwySerBoxTestClass))

#define gwy_ser_box_test_duplicate(ser_test) \
        (GWY_SER_BOX_TEST(gwy_serializable_duplicate(GWY_SERIALIZABLE(ser_test))))

GType gwy_ser_box_test_get_type(void) G_GNUC_CONST;

typedef struct _GwySerBoxTest      GwySerBoxTest;
typedef struct _GwySerBoxTestClass GwySerBoxTestClass;

struct _GwySerBoxTestClass {
    GObjectClass g_object_class;
};

struct _GwySerBoxTest {
    GObject g_object;
    GwyRGBA color;
};

static gsize
gwy_ser_box_test_n_items(G_GNUC_UNUSED GwySerializable *serializable)
{
    return 1 + gwy_serializable_boxed_n_items(GWY_TYPE_RGBA);
}

// The remaining members get zero-initialized which saves us from doing it.
static const GwySerializableItem default_items_box[] = {
    /*0*/ { .name = "color", .ctype = GWY_SERIALIZABLE_BOXED, },
};

#define add_item(id) \
    g_return_val_if_fail(items->len - items->n, 0); \
    items->items[items->n++] = it[id]; \
    n_items++

static gsize
gwy_ser_box_test_itemize(GwySerializable *serializable,
                         GwySerializableItems *items)
{
    GwySerBoxTest *sertest = GWY_SER_BOX_TEST(serializable);
    g_return_val_if_fail(items->len - items->n
                         >= G_N_ELEMENTS(default_items_box), 0);

    GwySerializableItem it = default_items_box[0];
    it.value.v_boxed = &sertest->color;
    items->items[items->n++] = it;

    gwy_serializable_boxed_itemize(GWY_TYPE_RGBA, &sertest->color, items);

    return G_N_ELEMENTS(default_items_box);
}

static gboolean
gwy_ser_box_test_construct(GwySerializable *serializable,
                           GwySerializableItems *items,
                           GwyErrorList **error_list)
{
    GwySerializableItem it[G_N_ELEMENTS(default_items_box)];

    memcpy(it, default_items_box, sizeof(default_items_box));
    gwy_deserialize_filter_items(it, G_N_ELEMENTS(it), items, NULL,
                                 "GwySerBoxTest", error_list);

    GwySerBoxTest *sertest = GWY_SER_BOX_TEST(serializable);

    if (it[0].value.v_boxed) {
        g_assert(it[0].array_size == GWY_TYPE_RGBA);
        GwyRGBA *rgba = it[0].value.v_boxed;
        sertest->color = *rgba;
        gwy_rgba_free(rgba);
        it[0].value.v_boxed = NULL;
    }

    return TRUE;
}

static GObject*
gwy_ser_box_test_duplicate_(GwySerializable *serializable)
{
    GwySerBoxTest *sertest = GWY_SER_BOX_TEST(serializable);
    GwySerBoxTest *copy = g_object_newv(GWY_TYPE_SER_BOX_TEST, 0, NULL);
    copy->color = sertest->color;
    return G_OBJECT(copy);
}

static void
gwy_ser_box_test_serializable_init(GwySerializableInterface *iface)
{
    iface->n_items   = gwy_ser_box_test_n_items;
    iface->itemize   = gwy_ser_box_test_itemize;
    iface->construct = gwy_ser_box_test_construct;
    iface->duplicate = gwy_ser_box_test_duplicate_;
    /*
    iface->assign = gwy_ser_box_test_assign_;
    */
}

G_DEFINE_TYPE_EXTENDED
    (GwySerBoxTest, gwy_ser_box_test, G_TYPE_OBJECT, 0,
     GWY_IMPLEMENT_SERIALIZABLE(gwy_ser_box_test_serializable_init))

static void
gwy_ser_box_test_class_init(G_GNUC_UNUSED GwySerBoxTestClass *klass)
{
}

static void
gwy_ser_box_test_init(G_GNUC_UNUSED GwySerBoxTest *sertest)
{
}

static const guchar ser_test_box[] = {
    0x47, 0x77, 0x79, 0x53, 0x65, 0x72, 0x42, 0x6f, 0x78, 0x54, 0x65, 0x73,
    0x74, 0x00, 0x43, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x63, 0x6f,
    0x6c, 0x6f, 0x72, 0x00, 0x78, 0x47, 0x77, 0x79, 0x52, 0x47, 0x42, 0x41,
    0x00, 0x2c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x72, 0x00, 0x64,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x3f, 0x67, 0x00, 0x64, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x62, 0x00, 0x64, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x61, 0x00, 0x64, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0xe0, 0x3f
};

void
test_serialize_boxed(void)
{
    GwySerBoxTest *sertest;
    GOutputStream *stream;
    GMemoryOutputStream *memstream;
    GError *error = NULL;
    gboolean ok;
    guint len;

    sertest = g_object_newv(GWY_TYPE_SER_BOX_TEST, 0, NULL);
    sertest->color.r = 1.0;
    sertest->color.a = 0.5;
    stream = g_memory_output_stream_new(malloc(200), 200, NULL, &free);
    memstream = G_MEMORY_OUTPUT_STREAM(stream);
    ok = gwy_serialize_gio(GWY_SERIALIZABLE(sertest), stream, &error);
    g_assert(ok);
    len = g_memory_output_stream_get_data_size(memstream);
    g_assert_cmpuint(len, ==, sizeof(ser_test_box));
    g_assert_cmpuint(memcmp(g_memory_output_stream_get_data(memstream),
                            ser_test_box, sizeof(ser_test_box)), ==, 0);
    g_object_unref(stream);
    g_object_unref(sertest);
    g_clear_error(&error);
}

void
test_deserialize_boxed(void)
{
    GwySerBoxTest *sertest;
    GwyErrorList *error_list = NULL;
    GError *err;
    gsize bytes_consumed;
    guchar *bad_ser_test_box, *t;

    /* Good */
    g_type_class_ref(GWY_TYPE_SER_BOX_TEST);
    sertest = (GwySerBoxTest*)gwy_deserialize_memory(ser_test_box,
                                                     sizeof(ser_test_box),
                                                     &bytes_consumed,
                                                     &error_list);
    g_assert(sertest);
    g_assert(GWY_IS_SER_BOX_TEST(sertest));
    g_assert_cmpuint(bytes_consumed, ==, sizeof(ser_test_box));
    g_assert_cmpuint(g_slist_length(error_list), ==, 0);
    g_assert_cmpfloat(sertest->color.r, ==, 1.0);
    g_assert_cmpfloat(sertest->color.g, ==, 0.0);
    g_assert_cmpfloat(sertest->color.b, ==, 0.0);
    g_assert_cmpfloat(sertest->color.a, ==, 0.5);
    GWY_OBJECT_UNREF(sertest);

    /* Soft error (deserialization has to dispose the boxed type itself) */
    bad_ser_test_box = g_memdup(ser_test_box, sizeof(ser_test_box));
    t = gwy_memmem(bad_ser_test_box, sizeof(ser_test_box),
                   "color", strlen("color"));
    g_assert(t);
    g_assert(strlen("color") == strlen("skunk"));
    memcpy(t, "skunk", strlen("skunk"));

    sertest = (GwySerBoxTest*)gwy_deserialize_memory(bad_ser_test_box,
                                                     sizeof(ser_test_box),
                                                     &bytes_consumed,
                                                     &error_list);
    g_assert_cmpuint(g_slist_length(error_list), ==, 1);
    err = error_list->data;
    g_assert_cmpuint(err->domain, ==, GWY_DESERIALIZE_ERROR);
    g_assert_cmpuint(err->code, ==, GWY_DESERIALIZE_ERROR_ITEM);
    g_assert(sertest);
    g_assert(GWY_IS_SER_BOX_TEST(sertest));
    g_assert_cmpuint(bytes_consumed, ==, sizeof(ser_test_box));
    GWY_OBJECT_UNREF(sertest);
    gwy_error_list_clear(&error_list);
    g_free(bad_ser_test_box);

    /* Hard error */
    bad_ser_test_box = g_memdup(ser_test_box, sizeof(ser_test_box));
    t = gwy_memmem(bad_ser_test_box, sizeof(ser_test_box),
                   "GwyRGBA", strlen("GwyRGBA"));
    g_assert(t);
    g_assert(strlen("GwyRGBA") == strlen("BREAKME"));
    memcpy(t, "BREAKME", strlen("BREAKME"));

    sertest = (GwySerBoxTest*)gwy_deserialize_memory(bad_ser_test_box,
                                                     sizeof(ser_test_box),
                                                     &bytes_consumed,
                                                     &error_list);
    g_assert_cmpuint(g_slist_length(error_list), ==, 1);
    err = error_list->data;
    g_assert_cmpuint(err->domain, ==, GWY_DESERIALIZE_ERROR);
    g_assert_cmpuint(err->code, ==, GWY_DESERIALIZE_ERROR_OBJECT);
    g_assert(!sertest);
    g_assert_cmpuint(bytes_consumed, ==, sizeof(ser_test_box));
    gwy_error_list_clear(&error_list);
    g_free(bad_ser_test_box);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
