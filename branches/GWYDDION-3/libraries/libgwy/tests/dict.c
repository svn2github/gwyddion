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
 * Dict
 *
 ***************************************************************************/

static void
dict_item_changed(G_GNUC_UNUSED GwyDict *dict,
                  gpointer arg1,
                  const gchar **item_key)
{
    *item_key = g_quark_to_string(GPOINTER_TO_UINT(arg1));
}

void
test_dict_data(void)
{
    GwyDict *dict = gwy_dict_new();
    const gchar *item_key;
    guint int_changed = 0;
    GQuark quark;
    gboolean ok;
    guint n;

    g_assert_cmpuint(gwy_dict_size(dict), ==, 0);
    g_signal_connect(dict, "item-changed",
                     G_CALLBACK(dict_item_changed), &item_key);
    g_signal_connect_swapped(dict, "item-changed::/pfx/int",
                             G_CALLBACK(record_signal), &int_changed);

    item_key = "";
    gwy_dict_set_int32_n(dict, "/pfx/int", 42);
    quark = g_quark_try_string("/pfx/int");
    g_assert(quark);

    g_assert(gwy_dict_contains(dict, quark));
    g_assert(gwy_dict_get_int32(dict, quark) == 42);
    g_assert_cmpstr(item_key, ==, "/pfx/int");
    g_assert_cmpuint(int_changed, ==, 1);

    item_key = "";
    gwy_dict_set_int32(dict, quark, -3);
    g_assert(gwy_dict_contains_n(dict, "/pfx/int"));
    g_assert_cmpint(gwy_dict_get_int32_n(dict, "/pfx/int"), ==, -3);
    g_assert_cmpstr(item_key, ==, "/pfx/int");
    g_assert_cmpuint(int_changed, ==, 2);

    gint32 i32;
    g_assert(gwy_dict_pick_int32_n(dict, "/pfx/int", &i32));
    g_assert_cmpint(i32, ==, -3);

    item_key = "";
    gwy_dict_set_schar_n(dict, "/pfx/char", '@');
    g_assert(gwy_dict_contains_n(dict, "/pfx/char"));
    g_assert_cmpint(gwy_dict_get_schar_n(dict, "/pfx/char"), ==, '@');
    g_assert_cmpstr(item_key, ==, "/pfx/char");

    gint8 c;
    g_assert(gwy_dict_pick_schar_n(dict, "/pfx/char", &c));
    g_assert_cmpint(c, ==, '@');

    item_key = "";
    gwy_dict_set_int64_n(dict, "/pfx/int64",
                         G_GUINT64_CONSTANT(0xdeadbeefdeadbeef));
    g_assert(gwy_dict_contains_n(dict, "/pfx/int64"));
    g_assert((guint64)gwy_dict_get_int64_n(dict, "/pfx/int64")
             == G_GUINT64_CONSTANT(0xdeadbeefdeadbeef));
    g_assert_cmpstr(item_key, ==, "/pfx/int64");

    guint64 i64;
    g_assert(gwy_dict_pick_int64_n(dict, "/pfx/int64", (gint64*)&i64));
    g_assert_cmpuint(i64, ==, G_GUINT64_CONSTANT(0xdeadbeefdeadbeef));

    item_key = "";
    gwy_dict_set_double_n(dict, "/pfx/double", G_LN2);
    g_assert(gwy_dict_contains_n(dict, "/pfx/double"));
    g_assert_cmpfloat(gwy_dict_get_double_n(dict, "/pfx/double"),
                      ==, G_LN2);
    g_assert_cmpstr(item_key, ==, "/pfx/double");

    gdouble flt;
    g_assert(gwy_dict_pick_double_n(dict, "/pfx/double", &flt));
    g_assert_cmpfloat(flt, ==, G_LN2);

    item_key = "";
    gwy_dict_take_string_n(dict, "/pfx/string",
                                g_strdup("Test Test"));
    g_assert(gwy_dict_contains_n(dict, "/pfx/string"));
    g_assert_cmpstr(gwy_dict_get_string_n(dict, "/pfx/string"),
                    ==, "Test Test");
    g_assert_cmpstr(item_key, ==, "/pfx/string");

    const gchar *s = "";
    g_assert(gwy_dict_pick_string_n(dict, "/pfx/string", &s));
    g_assert_cmpstr(s, ==, "Test Test");

    /* No value change */
    item_key = "";
    gwy_dict_set_string_n(dict, "/pfx/string", "Test Test");
    g_assert(gwy_dict_contains_n(dict, "/pfx/string"));
    g_assert_cmpstr(gwy_dict_get_string_n(dict, "/pfx/string"),
                    ==, "Test Test");
    g_assert_cmpstr(item_key, ==, "");

    item_key = "";
    gwy_dict_set_string_n(dict, "/pfx/string", "Test Test Test");
    g_assert(gwy_dict_contains_n(dict, "/pfx/string"));
    g_assert_cmpstr(gwy_dict_get_string_n(dict, "/pfx/string"),
                    ==, "Test Test Test");
    g_assert_cmpstr(item_key, ==, "/pfx/string");

    g_assert_cmpuint(gwy_dict_size(dict), ==, 5);

    gwy_dict_transfer(dict, dict, "/pfx", "/elsewhere",
                           TRUE, TRUE);
    g_assert_cmpuint(gwy_dict_size(dict), ==, 10);

    ok = gwy_dict_remove_n(dict, "/pfx/string/ble");
    g_assert(!ok);

    n = gwy_dict_remove_prefix(dict, "/pfx/string/ble");
    g_assert_cmpuint(n, ==, 0);

    item_key = "";
    ok = gwy_dict_remove_n(dict, "/pfx/string");
    g_assert(ok);
    g_assert_cmpuint(gwy_dict_size(dict), ==, 9);
    g_assert_cmpstr(item_key, ==, "/pfx/string");

    item_key = "";
    n = gwy_dict_remove_prefix(dict, "/pfx/int");
    g_assert_cmpuint(n, ==, 1);
    g_assert_cmpuint(gwy_dict_size(dict), ==, 8);
    g_assert_cmpstr(item_key, ==, "/pfx/int");

    item_key = "";
    ok = gwy_dict_rename(dict,
                         g_quark_try_string("/pfx/int64"),
                         g_quark_try_string("/pfx/int"),
                         TRUE);
    g_assert(ok);
    g_assert_cmpuint(int_changed, ==, 4);
    g_assert_cmpstr(item_key, ==, "/pfx/int");

    n = gwy_dict_remove_prefix(dict, "/pfx");
    g_assert_cmpuint(n, ==, 3);
    g_assert_cmpuint(gwy_dict_size(dict), ==, 5);
    g_assert_cmpuint(int_changed, ==, 5);

    gwy_dict_remove_prefix(dict, NULL);
    g_assert_cmpuint(gwy_dict_size(dict), ==, 0);

    g_object_unref(dict);
}

void
test_dict_refcount(void)
{
    GwyDict *dict = gwy_dict_new();

    GwySerTest *st1 = g_object_newv(GWY_TYPE_SER_TEST, 0, NULL);
    g_assert_cmpuint(G_OBJECT(st1)->ref_count, ==, 1);
    gwy_dict_set_object_n(dict, "/pfx/object", st1);
    g_assert_cmpuint(G_OBJECT(st1)->ref_count, ==, 2);

    GwySerTest *st2 = g_object_newv(GWY_TYPE_SER_TEST, 0, NULL);
    g_assert_cmpuint(G_OBJECT(st2)->ref_count, ==, 1);
    gwy_dict_set_object_n(dict, "/pfx/object", st2);
    g_assert_cmpuint(G_OBJECT(st2)->ref_count, ==, 2);
    g_assert_cmpuint(G_OBJECT(st1)->ref_count, ==, 1);
    g_object_unref(st1);

    GwySerTest *st3 = g_object_newv(GWY_TYPE_SER_TEST, 0, NULL);
    g_assert_cmpuint(G_OBJECT(st3)->ref_count, ==, 1);
    g_object_ref(st3);
    gwy_dict_take_object_n(dict, "/pfx/taken", st3);
    g_assert_cmpuint(G_OBJECT(st3)->ref_count, ==, 2);

    g_object_unref(dict);
    g_assert_cmpuint(G_OBJECT(st2)->ref_count, ==, 1);
    g_object_unref(st2);
    g_assert_cmpuint(G_OBJECT(st3)->ref_count, ==, 1);
    g_object_unref(st3);

    dict = gwy_dict_new();
    st1 = g_object_newv(GWY_TYPE_SER_TEST, 0, NULL);
    gwy_dict_set_object_n(dict, "/pfx/object", st1);
    gwy_dict_transfer(dict, dict, "/pfx", "/elsewhere", FALSE, TRUE);
    g_assert_cmpuint(G_OBJECT(st1)->ref_count, ==, 3);
    st2 = gwy_dict_get_object_n(dict, "/elsewhere/object");
    g_assert(st2 == st1);

    gwy_dict_transfer(dict, dict, "/pfx", "/faraway", TRUE, TRUE);
    st2 = gwy_dict_get_object_n(dict, "/faraway/object");
    g_assert(GWY_IS_SER_TEST(st2));
    g_assert(st2 != st1);
    g_assert_cmpuint(G_OBJECT(st1)->ref_count, ==, 3);
    g_assert_cmpuint(G_OBJECT(st2)->ref_count, ==, 1);
    g_object_ref(st2);

    g_object_unref(dict);
    g_assert_cmpuint(G_OBJECT(st1)->ref_count, ==, 1);
    g_assert_cmpuint(G_OBJECT(st2)->ref_count, ==, 1);
    g_object_unref(st1);
    g_object_unref(st2);
}

void
test_dict_keys(void)
{
    GwyDict *dict = gwy_dict_new();
    GQuark *qkeys = gwy_dict_keys(dict);
    g_assert(qkeys);
    g_assert(!qkeys[0]);
    g_free(qkeys);
    const gchar **skeys = gwy_dict_keys_n(dict);
    g_assert(skeys);
    g_assert(!skeys[0]);
    g_free(skeys);

    gwy_dict_set_enum_n(dict, "test", GWY_WINDOWING_RECT);
    gwy_dict_set_enum_n(dict, "rect", GWY_WINDOWING_RECT);
    gwy_dict_set_enum_n(dict, "welch", GWY_WINDOWING_WELCH);
    gwy_dict_set_enum_n(dict, "blackmann", GWY_WINDOWING_LANCZOS);
    gwy_dict_remove_n(dict, "test");
    gwy_dict_set_enum_n(dict, "blackmann", GWY_WINDOWING_BLACKMANN);

    const gchar *expected_keys[] = { "rect", "welch", "blackmann" };
    g_assert_cmpuint(gwy_dict_size(dict), ==, G_N_ELEMENTS(expected_keys));

    qkeys = gwy_dict_keys(dict);
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

    skeys = gwy_dict_keys_n(dict);
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

    g_object_unref(dict);
}

static void
check_value(GQuark key, const GValue *value, gpointer user_data)
{
    GwyDict *reference = (GwyDict*)user_data;
    g_assert(gwy_dict_contains(reference, key));

    GValue refvalue;
    gwy_clear1(refvalue);
    gwy_dict_get_value(reference, key, &refvalue);
    g_assert(values_are_equal(value, &refvalue));
    g_value_unset(&refvalue);
}

static void
dict_assert_equal(GwyDict *dict, GwyDict *reference)
{
    g_assert_cmpuint(gwy_dict_size(dict), ==, gwy_dict_size(reference));
    gwy_dict_foreach(dict, NULL, check_value, reference);
}

static void
dict_assert_equal_object(GObject *object, GObject *reference)
{
    dict_assert_equal(GWY_DICT(object), GWY_DICT(reference));
}

void
test_dict_serialize(void)
{
    GwyDict *dict = gwy_dict_new();
    gwy_dict_set_schar_n(dict, "char", '\xfe');
    gwy_dict_set_boolean_n(dict, "bool", TRUE);
    gwy_dict_set_int32_n(dict, "int32", -123456);
    gwy_dict_set_int64_n(dict, "int64", G_GINT64_CONSTANT(-1234567890));
    gwy_dict_set_string_n(dict, "string", "Mud");
    gwy_dict_set_double_n(dict, "double", G_E);
    GwyUnit *unit = gwy_unit_new_from_string("uPa", NULL);
    gwy_dict_take_object_n(dict, "unit", unit);

    serializable_duplicate(GWY_SERIALIZABLE(dict), dict_assert_equal_object);
    serializable_assign(GWY_SERIALIZABLE(dict), dict_assert_equal_object);

    GOutputStream *stream;
    GMemoryOutputStream *memstream;
    stream = g_memory_output_stream_new(malloc(200), 200, NULL, &free);
    memstream = G_MEMORY_OUTPUT_STREAM(stream);
    GError *error = NULL;
    gboolean ok = gwy_serialize_gio(GWY_SERIALIZABLE(dict), stream, &error);
    g_assert(ok);
    g_assert_no_error(error);

    gsize len = g_memory_output_stream_get_data_size(memstream);
    const guchar *buffer = g_memory_output_stream_get_data(memstream);
    gsize bytes_consumed = 0;
    GwyErrorList *error_list = NULL;
    GwyDict *copy = GWY_DICT(gwy_deserialize_memory(buffer, len,
                                                    &bytes_consumed,
                                                    &error_list));
    g_assert(GWY_IS_DICT(copy));
    g_assert(!error_list);
    g_assert_cmpuint(bytes_consumed, ==, len);
    g_object_unref(stream);
    gwy_error_list_clear(&error_list);

    dict_assert_equal(copy, dict);

    GwyUnit *unitcopy = NULL;
    g_assert(gwy_dict_pick_object_n(dict, "unit", &unitcopy));
    g_assert(GWY_IS_UNIT(unitcopy));
    g_assert(gwy_unit_equal(unitcopy, unit));

    guint change_count = 0;
    g_signal_connect_swapped(dict, "item-changed",
                             G_CALLBACK(record_signal), &change_count);
    gwy_dict_transfer(copy, dict, "", "", FALSE, TRUE);
    /* Unit must change, but others should be detected as same-value. */
    g_assert_cmpuint(change_count, ==, 1);
    /* Not even units can change here. */
    gwy_dict_transfer(copy, dict, "", "", FALSE, TRUE);
    /* Unit must change, but others should be detected as same-value. */
    gwy_dict_transfer(copy, dict, "", "", TRUE, TRUE);

    g_object_unref(copy);
    g_object_unref(dict);
}

void
test_dict_text(void)
{
    GwyDict *dict = gwy_dict_new();
    gwy_dict_set_schar_n(dict, "char", '\xfe');
    gwy_dict_set_boolean_n(dict, "bool", TRUE);
    gwy_dict_set_int32_n(dict, "int32", -123456);
    gwy_dict_set_int64_n(dict, "int64", G_GINT64_CONSTANT(-1234567890));
    gwy_dict_set_string_n(dict, "string", "Mud");
    gwy_dict_set_double_n(dict, "double", G_E);

    gchar **lines = gwy_dict_dump_to_text(dict);
    g_assert(lines);
    gchar *text = g_strjoinv("\n", lines);
    g_strfreev(lines);
    GwyDict *copy = gwy_dict_new_from_text(text);
    g_free(text);
    g_assert(GWY_IS_DICT(copy));
    g_assert_cmpuint(gwy_dict_size(dict), ==, 6);

    guint change_count = 0;
    g_signal_connect_swapped(dict, "item-changed",
                             G_CALLBACK(record_signal), &change_count);
    gwy_dict_transfer(copy, dict, "", "", FALSE, TRUE);
    /* Atomic types must be detected as same-value. */
    g_assert_cmpuint(change_count, ==, 0);

    g_object_unref(copy);
    g_object_unref(dict);
}

void
test_dict_boxed(void)
{
    GwyDict *dict = gwy_dict_new();
    GwyRGBA color = { 0.9, 0.6, 0.3, 1.0 };
    gwy_dict_set_boxed_n(dict, "color", GWY_TYPE_RGBA, &color);
    GwyRGBA color2 = { 0.0, 0.5, 0.5, 1.0 };
    gwy_dict_set_boxed_n(dict, "color", GWY_TYPE_RGBA, &color2);
    const GwyRGBA *color3 = gwy_dict_get_boxed_n(dict, "color", GWY_TYPE_RGBA);
    g_assert(color3);
    g_assert_cmpint(memcmp(&color2, color3, sizeof(GwyRGBA)), ==, 0);
    GwyRGBA color4;
    g_assert(gwy_dict_pick_boxed_n(dict, "color", GWY_TYPE_RGBA,
                                       &color4));
    g_assert_cmpint(memcmp(&color2, &color4, sizeof(GwyRGBA)), ==, 0);
    g_assert(gwy_dict_pick_boxed_n(dict, "color", G_TYPE_BOXED,
                                       &color4));
    g_assert_cmpint(memcmp(&color2, &color4, sizeof(GwyRGBA)), ==, 0);
    g_object_unref(dict);
}

gpointer
serialize_boxed_and_back(gpointer boxed, GType type)
{
    GwyDict *dict = gwy_dict_new();
    gwy_dict_set_boxed_n(dict, "boxed", type, boxed);
    GwyDict *copy = (GwyDict*)serialize_and_back(G_OBJECT(dict), NULL);
    g_assert(GWY_IS_DICT(copy));
    g_object_unref(dict);
    g_assert_cmpuint(gwy_dict_item_type_n(copy, "boxed"), ==, type);
    gconstpointer cboxed = gwy_dict_get_boxed_n(copy, "boxed", type);
    g_assert(cboxed);
    boxed = g_boxed_copy(type, cboxed);
    g_object_unref(copy);
    return boxed;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
