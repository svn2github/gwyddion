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
 * Container
 *
 ***************************************************************************/

static void
container_item_changed(G_GNUC_UNUSED GwyContainer *container,
                       gpointer arg1,
                       const gchar **item_key)
{
    *item_key = g_quark_to_string(GPOINTER_TO_UINT(arg1));
}

static void
container_item_changed_count(G_GNUC_UNUSED GwyContainer *container,
                             G_GNUC_UNUSED gpointer arg1,
                             guint *called)
{
    (*called)++;
}

void
test_container_data(void)
{
    GwyContainer *container = gwy_container_new();
    const gchar *item_key;
    guint int_changed = 0;
    GQuark quark;
    gboolean ok;
    guint n;

    g_assert_cmpuint(gwy_container_size(container), ==, 0);
    g_signal_connect(container, "item-changed",
                     G_CALLBACK(container_item_changed), &item_key);
    g_signal_connect(container, "item-changed::/pfx/int",
                     G_CALLBACK(container_item_changed_count), &int_changed);

    item_key = "";
    gwy_container_set_int32_n(container, "/pfx/int", 42);
    quark = g_quark_try_string("/pfx/int");
    g_assert(quark);

    g_assert(gwy_container_contains(container, quark));
    g_assert(gwy_container_get_int32(container, quark) == 42);
    g_assert_cmpstr(item_key, ==, "/pfx/int");
    g_assert_cmpuint(int_changed, ==, 1);

    item_key = "";
    gwy_container_set_int32(container, quark, -3);
    g_assert(gwy_container_contains_n(container, "/pfx/int"));
    g_assert_cmpint(gwy_container_get_int32_n(container, "/pfx/int"), ==, -3);
    g_assert_cmpstr(item_key, ==, "/pfx/int");
    g_assert_cmpuint(int_changed, ==, 2);

    gint32 i32;
    g_assert(gwy_container_gis_int32_n(container, "/pfx/int", &i32));
    g_assert_cmpint(i32, ==, -3);

    item_key = "";
    gwy_container_set_char_n(container, "/pfx/char", '@');
    g_assert(gwy_container_contains_n(container, "/pfx/char"));
    g_assert_cmpint(gwy_container_get_char_n(container, "/pfx/char"), ==, '@');
    g_assert_cmpstr(item_key, ==, "/pfx/char");

    gchar c;
    g_assert(gwy_container_gis_char_n(container, "/pfx/char", &c));
    g_assert_cmpint(c, ==, '@');

    item_key = "";
    gwy_container_set_int64_n(container, "/pfx/int64",
                              G_GUINT64_CONSTANT(0xdeadbeefdeadbeef));
    g_assert(gwy_container_contains_n(container, "/pfx/int64"));
    g_assert((guint64)gwy_container_get_int64_n(container, "/pfx/int64")
             == G_GUINT64_CONSTANT(0xdeadbeefdeadbeef));
    g_assert_cmpstr(item_key, ==, "/pfx/int64");

    guint64 i64;
    g_assert(gwy_container_gis_int64_n(container, "/pfx/int64", (gint64*)&i64));
    g_assert_cmpuint(i64, ==, G_GUINT64_CONSTANT(0xdeadbeefdeadbeef));

    item_key = "";
    gwy_container_set_double_n(container, "/pfx/double", G_LN2);
    g_assert(gwy_container_contains_n(container, "/pfx/double"));
    g_assert_cmpfloat(gwy_container_get_double_n(container, "/pfx/double"),
                      ==, G_LN2);
    g_assert_cmpstr(item_key, ==, "/pfx/double");

    gdouble flt;
    g_assert(gwy_container_gis_double_n(container, "/pfx/double", &flt));
    g_assert_cmpfloat(flt, ==, G_LN2);

    item_key = "";
    gwy_container_take_string_n(container, "/pfx/string",
                                g_strdup("Test Test"));
    g_assert(gwy_container_contains_n(container, "/pfx/string"));
    g_assert_cmpstr(gwy_container_get_string_n(container, "/pfx/string"),
                    ==, "Test Test");
    g_assert_cmpstr(item_key, ==, "/pfx/string");

    const gchar *s = "";
    g_assert(gwy_container_gis_string_n(container, "/pfx/string", &s));
    g_assert_cmpstr(s, ==, "Test Test");

    /* No value change */
    item_key = "";
    gwy_container_set_string_n(container, "/pfx/string", "Test Test");
    g_assert(gwy_container_contains_n(container, "/pfx/string"));
    g_assert_cmpstr(gwy_container_get_string_n(container, "/pfx/string"),
                    ==, "Test Test");
    g_assert_cmpstr(item_key, ==, "");

    item_key = "";
    gwy_container_set_string_n(container, "/pfx/string", "Test Test Test");
    g_assert(gwy_container_contains_n(container, "/pfx/string"));
    g_assert_cmpstr(gwy_container_get_string_n(container, "/pfx/string"),
                    ==, "Test Test Test");
    g_assert_cmpstr(item_key, ==, "/pfx/string");

    g_assert_cmpuint(gwy_container_size(container), ==, 5);

    gwy_container_transfer(container, container, "/pfx", "/elsewhere",
                           TRUE, TRUE);
    g_assert_cmpuint(gwy_container_size(container), ==, 10);

    ok = gwy_container_remove_n(container, "/pfx/string/ble");
    g_assert(!ok);

    n = gwy_container_remove_prefix(container, "/pfx/string/ble");
    g_assert_cmpuint(n, ==, 0);

    item_key = "";
    ok = gwy_container_remove_n(container, "/pfx/string");
    g_assert(ok);
    g_assert_cmpuint(gwy_container_size(container), ==, 9);
    g_assert_cmpstr(item_key, ==, "/pfx/string");

    item_key = "";
    n = gwy_container_remove_prefix(container, "/pfx/int");
    g_assert_cmpuint(n, ==, 1);
    g_assert_cmpuint(gwy_container_size(container), ==, 8);
    g_assert_cmpstr(item_key, ==, "/pfx/int");

    item_key = "";
    ok = gwy_container_rename(container,
                             g_quark_try_string("/pfx/int64"),
                             g_quark_try_string("/pfx/int"),
                             TRUE);
    g_assert(ok);
    g_assert_cmpuint(int_changed, ==, 4);
    g_assert_cmpstr(item_key, ==, "/pfx/int");

    n = gwy_container_remove_prefix(container, "/pfx");
    g_assert_cmpuint(n, ==, 3);
    g_assert_cmpuint(gwy_container_size(container), ==, 5);
    g_assert_cmpuint(int_changed, ==, 5);

    gwy_container_remove_prefix(container, NULL);
    g_assert_cmpuint(gwy_container_size(container), ==, 0);

    g_object_unref(container);
}

void
test_container_refcount(void)
{
    GwyContainer *container = gwy_container_new();

    GwySerTest *st1 = g_object_newv(GWY_TYPE_SER_TEST, 0, NULL);
    g_assert_cmpuint(G_OBJECT(st1)->ref_count, ==, 1);
    gwy_container_set_object_n(container, "/pfx/object", st1);
    g_assert_cmpuint(G_OBJECT(st1)->ref_count, ==, 2);

    GwySerTest *st2 = g_object_newv(GWY_TYPE_SER_TEST, 0, NULL);
    g_assert_cmpuint(G_OBJECT(st2)->ref_count, ==, 1);
    gwy_container_set_object_n(container, "/pfx/object", st2);
    g_assert_cmpuint(G_OBJECT(st2)->ref_count, ==, 2);
    g_assert_cmpuint(G_OBJECT(st1)->ref_count, ==, 1);
    g_object_unref(st1);

    GwySerTest *st3 = g_object_newv(GWY_TYPE_SER_TEST, 0, NULL);
    g_assert_cmpuint(G_OBJECT(st3)->ref_count, ==, 1);
    g_object_ref(st3);
    gwy_container_take_object_n(container, "/pfx/taken", st3);
    g_assert_cmpuint(G_OBJECT(st3)->ref_count, ==, 2);

    g_object_unref(container);
    g_assert_cmpuint(G_OBJECT(st2)->ref_count, ==, 1);
    g_object_unref(st2);
    g_assert_cmpuint(G_OBJECT(st3)->ref_count, ==, 1);
    g_object_unref(st3);

    container = gwy_container_new();
    st1 = g_object_newv(GWY_TYPE_SER_TEST, 0, NULL);
    gwy_container_set_object_n(container, "/pfx/object", st1);
    gwy_container_transfer(container, container, "/pfx", "/elsewhere",
                           FALSE, TRUE);
    g_assert_cmpuint(G_OBJECT(st1)->ref_count, ==, 3);
    st2 = gwy_container_get_object_n(container, "/elsewhere/object");
    g_assert(st2 == st1);

    gwy_container_transfer(container, container, "/pfx", "/faraway",
                           TRUE, TRUE);
    st2 = gwy_container_get_object_n(container, "/faraway/object");
    g_assert(GWY_IS_SER_TEST(st2));
    g_assert(st2 != st1);
    g_assert_cmpuint(G_OBJECT(st1)->ref_count, ==, 3);
    g_assert_cmpuint(G_OBJECT(st2)->ref_count, ==, 1);
    g_object_ref(st2);

    g_object_unref(container);
    g_assert_cmpuint(G_OBJECT(st1)->ref_count, ==, 1);
    g_assert_cmpuint(G_OBJECT(st2)->ref_count, ==, 1);
    g_object_unref(st1);
    g_object_unref(st2);
}

void
test_container_keys(void)
{
    GwyContainer *container = gwy_container_new();
    GQuark *qkeys = gwy_container_keys(container);
    g_assert(qkeys);
    g_assert(!qkeys[0]);
    g_free(qkeys);
    const gchar **skeys = gwy_container_keys_n(container);
    g_assert(skeys);
    g_assert(!skeys[0]);
    g_free(skeys);

    gwy_container_set_enum_n(container, "test", GWY_WINDOWING_RECT);
    gwy_container_set_enum_n(container, "rect", GWY_WINDOWING_RECT);
    gwy_container_set_enum_n(container, "welch", GWY_WINDOWING_WELCH);
    gwy_container_set_enum_n(container, "blackmann", GWY_WINDOWING_LANCZOS);
    gwy_container_remove_n(container, "test");
    gwy_container_set_enum_n(container, "blackmann", GWY_WINDOWING_BLACKMANN);

    const gchar *expected_keys[] = { "rect", "welch", "blackmann" };
    g_assert_cmpuint(gwy_container_size(container),
                     ==,
                     G_N_ELEMENTS(expected_keys));

    qkeys = gwy_container_keys(container);
    g_assert(qkeys);
    for (guint i = 0; i < G_N_ELEMENTS(expected_keys); i++) {
        g_assert(qkeys[i]);
        for (guint j = 0; j < i; j++)
            g_assert_cmpuint(qkeys[j], !=, qkeys[i]);
        gboolean found = FALSE;
        for (guint j = 0; j < G_N_ELEMENTS(expected_keys); j++) {
            if (qkeys[i] == g_quark_from_string(expected_keys[j])) {
                found = TRUE;
                break;
            }
        }
        g_assert(found);
    }
    g_assert(!qkeys[G_N_ELEMENTS(expected_keys)]);
    g_free(qkeys);

    skeys = gwy_container_keys_n(container);
    g_assert(skeys);
    for (guint i = 0; i < G_N_ELEMENTS(expected_keys); i++) {
        g_assert(skeys[i]);
        for (guint j = 0; j < i; j++)
            g_assert_cmpstr(skeys[j], !=, skeys[i]);
        gboolean found = FALSE;
        for (guint j = 0; j < G_N_ELEMENTS(expected_keys); j++) {
            if (gwy_strequal(skeys[i], expected_keys[j])) {
                found = TRUE;
                break;
            }
        }
        g_assert(found);
    }
    g_assert(!skeys[G_N_ELEMENTS(expected_keys)]);
    g_free(skeys);

    g_object_unref(container);
}

static void
check_value(GQuark key, const GValue *value, gpointer user_data)
{
    GwyContainer *reference = (GwyContainer*)user_data;
    g_assert(gwy_container_contains(reference, key));

    GValue refvalue;
    gwy_clear1(refvalue);
    gwy_container_get_value(reference, key, &refvalue);
    g_assert(values_are_equal(value, &refvalue));
    g_value_unset(&refvalue);
}

static void
container_assert_equal(GwyContainer *container, GwyContainer *reference)
{
    g_assert_cmpuint(gwy_container_size(container),
                     ==,
                     gwy_container_size(reference));
    gwy_container_foreach(container, NULL, check_value, reference);
}

static void
container_assert_equal_object(GObject *object, GObject *reference)
{
    container_assert_equal(GWY_CONTAINER(object), GWY_CONTAINER(reference));
}

void
test_container_serialize(void)
{
    GwyContainer *container = gwy_container_new();
    gwy_container_set_char_n(container, "char", '\xfe');
    gwy_container_set_boolean_n(container, "bool", TRUE);
    gwy_container_set_int32_n(container, "int32", -123456);
    gwy_container_set_int64_n(container, "int64",
                              G_GINT64_CONSTANT(-1234567890));
    gwy_container_set_string_n(container, "string", "Mud");
    gwy_container_set_double_n(container, "double", G_E);
    GwyUnit *unit = gwy_unit_new_from_string("uPa", NULL);
    gwy_container_take_object_n(container, "unit", unit);

    serializable_duplicate(GWY_SERIALIZABLE(container),
                           container_assert_equal_object);
    serializable_assign(GWY_SERIALIZABLE(container),
                        container_assert_equal_object);

    GOutputStream *stream;
    GMemoryOutputStream *memstream;
    stream = g_memory_output_stream_new(malloc(200), 200, NULL, &free);
    memstream = G_MEMORY_OUTPUT_STREAM(stream);
    GError *error = NULL;
    gboolean ok = gwy_serialize_gio(GWY_SERIALIZABLE(container), stream,
                                    &error);
    g_assert(ok);
    g_assert(!error);
    g_clear_error(&error);

    gsize len = g_memory_output_stream_get_data_size(memstream);
    const guchar *buffer = g_memory_output_stream_get_data(memstream);
    gsize bytes_consumed = 0;
    GwyErrorList *error_list = NULL;
    GwyContainer *copy = GWY_CONTAINER(gwy_deserialize_memory(buffer, len,
                                                              &bytes_consumed,
                                                              &error_list));
    g_assert(GWY_IS_CONTAINER(copy));
    g_assert(!error_list);
    g_assert_cmpuint(bytes_consumed, ==, len);
    g_object_unref(stream);
    gwy_error_list_clear(&error_list);

    container_assert_equal(copy, container);

    GwyUnit *unitcopy = NULL;
    g_assert(gwy_container_gis_object_n(container, "unit", &unitcopy));
    g_assert(GWY_IS_UNIT(unitcopy));
    g_assert(gwy_unit_equal(unitcopy, unit));

    guint change_count = 0;
    g_signal_connect(container, "item-changed",
                     G_CALLBACK(container_item_changed_count), &change_count);
    gwy_container_transfer(copy, container, "", "", FALSE, TRUE);
    /* Unit must change, but others should be detected as same-value. */
    g_assert_cmpuint(change_count, ==, 1);
    /* Not even units can change here. */
    gwy_container_transfer(copy, container, "", "", FALSE, TRUE);
    /* Unit must change, but others should be detected as same-value. */
    gwy_container_transfer(copy, container, "", "", TRUE, TRUE);

    g_object_unref(copy);
    g_object_unref(container);
}

void
test_container_text(void)
{
    GwyContainer *container = gwy_container_new();
    gwy_container_set_char_n(container, "char", '\xfe');
    gwy_container_set_boolean_n(container, "bool", TRUE);
    gwy_container_set_int32_n(container, "int32", -123456);
    gwy_container_set_int64_n(container, "int64",
                              G_GINT64_CONSTANT(-1234567890));
    gwy_container_set_string_n(container, "string", "Mud");
    gwy_container_set_double_n(container, "double", G_E);

    gchar **lines = gwy_container_dump_to_text(container);
    g_assert(lines);
    gchar *text = g_strjoinv("\n", lines);
    g_strfreev(lines);
    GwyContainer *copy = gwy_container_new_from_text(text);
    g_free(text);
    g_assert(GWY_IS_CONTAINER(copy));
    g_assert_cmpuint(gwy_container_size(container), ==, 6);

    guint change_count = 0;
    g_signal_connect(container, "item-changed",
                     G_CALLBACK(container_item_changed_count), &change_count);
    gwy_container_transfer(copy, container, "", "", FALSE, TRUE);
    /* Atomic types must be detected as same-value. */
    g_assert_cmpuint(change_count, ==, 0);

    g_object_unref(copy);
    g_object_unref(container);
}

void
test_container_boxed(void)
{
    GwyContainer *container = gwy_container_new();
    GwyRGBA color = { 0.9, 0.6, 0.3, 1.0 };
    gwy_container_set_boxed_n(container, "color", GWY_TYPE_RGBA, &color);
    GwyRGBA color2 = { 0.0, 0.5, 0.5, 1.0 };
    gwy_container_set_boxed_n(container, "color", GWY_TYPE_RGBA, &color2);
    const GwyRGBA *color3 = gwy_container_get_boxed_n(container, "color",
                                                      GWY_TYPE_RGBA);
    g_assert(color3);
    g_assert_cmpint(memcmp(&color2, color3, sizeof(GwyRGBA)), ==, 0);
    GwyRGBA color4;
    g_assert(gwy_container_gis_boxed_n(container, "color", GWY_TYPE_RGBA,
                                       &color4));
    g_assert_cmpint(memcmp(&color2, &color4, sizeof(GwyRGBA)), ==, 0);
    g_assert(gwy_container_gis_boxed_n(container, "color", G_TYPE_BOXED,
                                       &color4));
    g_assert_cmpint(memcmp(&color2, &color4, sizeof(GwyRGBA)), ==, 0);
    g_object_unref(container);
}

gpointer
serialize_boxed_and_back(gpointer boxed, GType type)
{
    GwyContainer *container = gwy_container_new();
    gwy_container_set_boxed_n(container, "boxed", type, boxed);
    GwyContainer *copy = (GwyContainer*)serialize_and_back(G_OBJECT(container),
                                                           NULL);
    g_assert(GWY_IS_CONTAINER(copy));
    g_object_unref(container);
    g_assert_cmpuint(gwy_container_item_type_n(copy, "boxed"), ==, type);
    gconstpointer cboxed = gwy_container_get_boxed_n(copy, "boxed", type);
    g_assert(cboxed);
    boxed = g_boxed_copy(type, cboxed);
    g_object_unref(copy);
    return boxed;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
