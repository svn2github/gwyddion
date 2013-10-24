/*
 *  $Id$
 *  Copyright (C) 2011 David Nečas (Yeti).
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

GType gwy_prop_test_get_type(void) G_GNUC_CONST;

typedef struct _GwyPropTest      GwyPropTest;
typedef struct _GwyPropTestClass GwyPropTestClass;

#define GWY_TYPE_PROP_TEST \
    (gwy_prop_test_get_type())
#define GWY_PROP_TEST(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_PROP_TEST, GwyPropTest))
#define GWY_IS_PROP_TEST(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_PROP_TEST))
#define GWY_PROP_TEST_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_PROP_TEST, GwyPropTestClass))

struct _GwyPropTestClass {
    GwyFitterClass g_object_class;
};

struct _GwyPropTest {
    GwyFitter g_object;
    guint set_n_params;
    guint get_n_params;
    guint set_lambda_start;
    guint get_lambda_start;
};

enum {
    PROP_0,
    PROP_LAMBDA_START,
    PROP_N_PARAMS,
    N_PROPS
};

G_DEFINE_TYPE(GwyPropTest, gwy_prop_test, GWY_TYPE_FITTER);

static GParamSpec *properties[N_PROPS];

static void
must_not_be_called(void)
{
    g_assert_not_reached();
}

static void
gwy_prop_test_set_property(GObject *object,
                           guint prop_id,
                           const GValue *value,
                           G_GNUC_UNUSED GParamSpec *pspec)
{
    GwyPropTest *proptest = GWY_PROP_TEST(object);

    switch (prop_id) {
        case PROP_N_PARAMS:
        g_assert_cmpuint(g_value_get_uint(value), ==, 1);
        proptest->set_n_params++;
        break;

        case PROP_LAMBDA_START:
        g_assert_cmpfloat(g_value_get_double(value), ==, 1/G_PI);
        proptest->set_lambda_start++;
        break;

        default:
        g_assert_not_reached();
        break;
    }
}

static void
gwy_prop_test_get_property(GObject *object,
                           guint prop_id,
                           GValue *value,
                           G_GNUC_UNUSED GParamSpec *pspec)
{
    GwyPropTest *proptest = GWY_PROP_TEST(object);

    switch (prop_id) {
        case PROP_N_PARAMS:
        g_value_set_uint(value, 1);
        proptest->get_n_params++;
        break;

        case PROP_LAMBDA_START:
        g_value_set_double(value, 1/G_PI);
        proptest->get_lambda_start++;
        break;

        default:
        g_assert_not_reached();
        break;
    }
}

static void
gwy_prop_test_class_init(GwyPropTestClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->set_property = gwy_prop_test_set_property;
    gobject_class->get_property = gwy_prop_test_get_property;

    gwy_override_class_properties(gobject_class, properties,
                                  "lambda-start", PROP_LAMBDA_START,
                                  "n-params", PROP_N_PARAMS,
                                  NULL);
    g_assert(properties[PROP_0] == NULL);
    g_assert(properties[PROP_LAMBDA_START] != NULL);
    g_assert_cmpstr(properties[PROP_LAMBDA_START]->name, ==, "lambda-start");
    g_assert(properties[PROP_N_PARAMS] != NULL);
    g_assert_cmpstr(properties[PROP_N_PARAMS]->name, ==, "n-params");
}

static void
gwy_prop_test_init(G_GNUC_UNUSED GwyPropTest *proptest)
{
}

void
test_object_utils_override_properties(void)
{
    GObject *object = g_object_new(GWY_TYPE_PROP_TEST,
                                   "n-params", 1,
                                   NULL);
    GwyPropTest *proptest = GWY_PROP_TEST(object);
    g_assert_cmpuint(proptest->set_lambda_start, ==, 0);
    g_assert_cmpuint(proptest->set_n_params, ==, 1);
    g_assert_cmpuint(proptest->get_lambda_start, ==, 0);
    g_assert_cmpuint(proptest->get_n_params, ==, 0);
    g_object_set(object, "lambda-start", 1/G_PI, NULL);
    g_assert_cmpuint(proptest->set_lambda_start, ==, 1);
    g_assert_cmpuint(proptest->set_n_params, ==, 1);
    g_assert_cmpuint(proptest->get_lambda_start, ==, 0);
    g_assert_cmpuint(proptest->get_n_params, ==, 0);
    guint n = 3;
    g_object_get(object, "n-params", &n, NULL);
    g_assert_cmpuint(n, ==, 1);
    g_assert_cmpuint(proptest->set_lambda_start, ==, 1);
    g_assert_cmpuint(proptest->set_n_params, ==, 1);
    g_assert_cmpuint(proptest->get_lambda_start, ==, 0);
    g_assert_cmpuint(proptest->get_n_params, ==, 1);
    g_object_unref(object);
}

void
test_object_utils_set_member(void)
{
    gpointer member_field = NULL;
    gulong handler_id = 0, prev_id;
    guint change_count = 0;

    g_assert(!gwy_set_member_object(GUINT_TO_POINTER(12345), NULL,
                                    GWY_TYPE_FIELD, &member_field,
                                    "notify", G_CALLBACK(must_not_be_called),
                                    &handler_id, G_CONNECT_SWAPPED,
                                    NULL));
    g_assert_cmpuint(handler_id, ==, 0);
    g_assert(member_field == NULL);

    GwyField *field1 = gwy_field_new();
    g_assert(gwy_set_member_object(GUINT_TO_POINTER(12345), field1,
                                   GWY_TYPE_FIELD, &member_field,
                                   "notify", G_CALLBACK(must_not_be_called),
                                   &handler_id, G_CONNECT_SWAPPED,
                                   NULL));
    g_assert_cmpuint(handler_id, !=, 0);
    g_assert(member_field == field1);
    prev_id = handler_id;
    // Here we assert gwy_set_member_object() took another reference.
    g_object_unref(field1);
    g_assert(!gwy_set_member_object(GUINT_TO_POINTER(12345), field1,
                                    GWY_TYPE_FIELD, &member_field,
                                    "notify", G_CALLBACK(must_not_be_called),
                                    &handler_id, G_CONNECT_SWAPPED,
                                    NULL));
    g_assert_cmpuint(handler_id, !=, 0);
    g_assert_cmpuint(handler_id, ==, prev_id);
    g_assert(member_field == field1);

    GwyField *field2 = gwy_field_new();
    g_assert(gwy_set_member_object(&change_count, field2,
                                   GWY_TYPE_FIELD, &member_field,
                                   "notify", G_CALLBACK(record_signal),
                                   &handler_id, G_CONNECT_SWAPPED,
                                   NULL));
    g_assert_cmpuint(handler_id, !=, 0);
    g_assert_cmpuint(handler_id, !=, prev_id);
    g_assert(member_field == field2);
    gwy_field_set_size(field2, 2, 2, FALSE);
    g_assert_cmpuint(change_count, ==, 2);
    prev_id = handler_id;
    g_assert(!gwy_set_member_object(&change_count, field2,
                                    GWY_TYPE_FIELD, &member_field,
                                    "notify", G_CALLBACK(record_signal),
                                    &handler_id, G_CONNECT_SWAPPED,
                                    NULL));
    g_assert_cmpuint(handler_id, !=, 0);
    g_assert_cmpuint(handler_id, ==, prev_id);
    g_assert(member_field == field2);
    gwy_field_set_size(field2, 3, 3, FALSE);
    g_assert_cmpuint(change_count, ==, 4);

    g_assert(gwy_set_member_object(&change_count, NULL,
                                   GWY_TYPE_FIELD, &member_field,
                                   "notify", G_CALLBACK(record_signal),
                                   &handler_id, G_CONNECT_SWAPPED,
                                   NULL));
    g_assert_cmpuint(handler_id, ==, 0);
    g_assert(member_field == NULL);
    gwy_field_set_size(field2, 4, 4, FALSE);
    g_assert_cmpuint(change_count, ==, 4);

    g_object_unref(field2);
}

void
test_object_utils_assign_string(void)
{
    gchar *p = NULL, *porig;

    g_assert(!gwy_assign_string(&p, NULL));
    g_assert(p == NULL);

    porig = p;
    g_assert(gwy_assign_string(&p, "test"));
    g_assert(p != porig);
    g_assert_cmpstr(p, ==, "test");

    porig = p;
    g_assert(!gwy_assign_string(&p, "test"));
    g_assert(p == porig);
    g_assert_cmpstr(p, ==, "test");

    porig = p;
    g_assert(gwy_assign_string(&p, p + 2));
    g_assert(p != porig);
    g_assert_cmpstr(p, ==, "st");

    g_assert(gwy_assign_string(&p, NULL));
    g_assert(p == NULL);

    // FIXME: We would like to check that there are no leaks etc.  Probably
    // can only be done in valgrind...
}

static guint destroy1_called = 0, destroy2_called = 0;

static void
data_destroy1(gpointer user_data)
{
    g_assert_cmpuint(GPOINTER_TO_SIZE(user_data), ==, 0x12345678UL);
    destroy1_called++;
}

static void
data_destroy2(gpointer user_data)
{
    g_assert_cmpuint(GPOINTER_TO_SIZE(user_data), ==, 0x87654321UL);
    destroy2_called++;
}

void
test_object_utils_set_user_func(void)
{
    gpointer func = NULL, data = NULL;
    GDestroyNotify destroy = NULL;

    gwy_set_user_func(NULL, GSIZE_TO_POINTER(0x12345678UL), &data_destroy1,
                      &func, &data, &destroy);
    g_assert(destroy == &data_destroy1);
    g_assert(func == NULL);
    g_assert_cmphex(GPOINTER_TO_SIZE(data), ==, 0x12345678UL);
    g_assert_cmpuint(destroy1_called, ==, 0);
    g_assert_cmpuint(destroy2_called, ==, 0);

    gwy_set_user_func(NULL, GSIZE_TO_POINTER(0x87654321UL), &data_destroy2,
                      &func, &data, &destroy);
    g_assert(destroy == &data_destroy2);
    g_assert(func == NULL);
    g_assert_cmphex(GPOINTER_TO_SIZE(data), ==, 0x87654321UL);
    g_assert_cmpuint(destroy1_called, ==, 1);
    g_assert_cmpuint(destroy2_called, ==, 0);

    gwy_set_user_func(NULL, NULL, NULL, &func, &data, &destroy);
    g_assert(destroy == NULL);
    g_assert(func == NULL);
    g_assert(data == NULL);
    g_assert_cmpuint(destroy1_called, ==, 1);
    g_assert_cmpuint(destroy2_called, ==, 1);

    gwy_set_user_func(NULL, GSIZE_TO_POINTER(0x12345678UL), &data_destroy1,
                      &func, &data, &destroy);
    g_assert(destroy == &data_destroy1);
    g_assert(func == NULL);
    g_assert_cmphex(GPOINTER_TO_SIZE(data), ==, 0x12345678UL);
    g_assert_cmpuint(destroy1_called, ==, 1);
    g_assert_cmpuint(destroy2_called, ==, 1);

    gwy_set_user_func(GSIZE_TO_POINTER(0xdeadbeefUL),
                      GSIZE_TO_POINTER(0x12345678UL),
                      &data_destroy1,
                      &func, &data, &destroy);
    g_assert(destroy == &data_destroy1);
    g_assert_cmphex(GPOINTER_TO_SIZE(func), ==, 0xdeadbeefUL);
    g_assert_cmphex(GPOINTER_TO_SIZE(data), ==, 0x12345678UL);
    g_assert_cmpuint(destroy1_called, ==, 2);
    g_assert_cmpuint(destroy2_called, ==, 1);

    gwy_set_user_func(NULL, NULL, NULL, &func, &data, &destroy);
    g_assert(destroy == NULL);
    g_assert(func == NULL);
    g_assert(data == NULL);
    g_assert_cmpuint(destroy1_called, ==, 3);
    g_assert_cmpuint(destroy2_called, ==, 1);
}

static void
assert_type_present(const GType *types, guint n, GType type)
{
    gboolean found = FALSE;
    for (guint i = 0; i < n; i++) {
        if (types[i] == type)
            return;
    }
    g_critical("Type %s not found in the list", g_type_name(type));
    g_assert(found);
}

static void
assert_type_absent(const GType *types, guint n, GType type)
{
    gboolean found = FALSE;
    for (guint i = 0; i < n; i++) {
        if (types[i] == type) {
            found = TRUE;
            g_critical("Type %s found unexpectedly in the list",
                       g_type_name(type));
            g_assert(!found);
            return;
        }
    }
}

void
test_object_utils_all_type_children_all(void)
{
    gwy_type_init();
    guint n;
    GType *types = gwy_all_type_children(GWY_TYPE_ARRAY, FALSE, &n);

    assert_type_present(types, n, GWY_TYPE_COORDS);
    assert_type_present(types, n, GWY_TYPE_COORDS_POINT);
    assert_type_present(types, n, GWY_TYPE_COORDS_RECTANGLE);
    assert_type_present(types, n, GWY_TYPE_COORDS_LINE);
    g_assert_cmpuint(n, ==, 4);

    g_free(types);
}

void
test_object_utils_all_type_children_concrete(void)
{
    gwy_type_init();
    guint n;
    GType *types = gwy_all_type_children(GWY_TYPE_ARRAY, TRUE, &n);

    assert_type_absent(types, n, GWY_TYPE_COORDS);
    assert_type_present(types, n, GWY_TYPE_COORDS_POINT);
    assert_type_present(types, n, GWY_TYPE_COORDS_RECTANGLE);
    assert_type_present(types, n, GWY_TYPE_COORDS_LINE);
    g_assert_cmpuint(n, ==, 3);

    g_free(types);
}

void
test_object_utils_enum_nick(void)
{
    g_assert_cmpstr(gwy_genum_value_nick(GWY_TYPE_WINDOWING,
                                         GWY_WINDOWING_NONE),
                    ==, "none");
    g_assert_cmpstr(gwy_genum_value_nick(GWY_TYPE_WINDOWING,
                                         GWY_WINDOWING_FLAT_TOP),
                    ==, "flat-top");
    g_assert_cmpstr(gwy_genum_value_nick(GWY_TYPE_WINDOWING,
                                         GWY_WINDOWING_KAISER25),
                    ==, "kaiser25");
    g_assert_cmpstr(gwy_genum_value_nick(GWY_TYPE_WINDOWING,
                                         1000),
                    ==, "<INVALID-VALUE 1000>");
}

void
test_object_utils_flags_nick(void)
{
    g_assert_cmpstr(gwy_gflags_value_nick(GWY_TYPE_LINE_COMPAT_FLAGS,
                                          0),
                    ==, "0");
    g_assert_cmpstr(gwy_gflags_value_nick(GWY_TYPE_LINE_COMPAT_FLAGS,
                                          GWY_LINE_COMPAT_RES),
                    ==, "res");
    g_assert_cmpstr(gwy_gflags_value_nick(GWY_TYPE_LINE_COMPAT_FLAGS,
                                          GWY_LINE_COMPAT_RES
                                          | GWY_LINE_COMPAT_DX),
                    ==, "res|dx");
    g_assert_cmpstr(gwy_gflags_value_nick(GWY_TYPE_LINE_COMPAT_FLAGS,
                                          GWY_LINE_COMPAT_RES
                                          | GWY_LINE_COMPAT_DX
                                          | GWY_LINE_COMPAT_VALUE),
                    ==, "res|dx|value");
    g_assert_cmpstr(gwy_gflags_value_nick(GWY_TYPE_LINE_COMPAT_FLAGS,
                                          GWY_LINE_COMPAT_DX
                                          | 0x10000),
                    ==, "dx|<INVALID-FLAGS 0x10000>");
    g_assert_cmpstr(gwy_gflags_value_nick(GWY_TYPE_LINE_COMPAT_FLAGS,
                                          0x10000),
                    ==, "<INVALID-FLAGS 0x10000>");
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
