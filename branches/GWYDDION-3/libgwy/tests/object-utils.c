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

static GParamSpec *prop_test_pspecs[N_PROPS];

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

    gwy_override_class_properties(gobject_class, prop_test_pspecs,
                                  "lambda-start", PROP_LAMBDA_START,
                                  "n-params", PROP_N_PARAMS,
                                  NULL);
    g_assert(prop_test_pspecs[PROP_0] == NULL);
    g_assert(prop_test_pspecs[PROP_LAMBDA_START] != NULL);
    g_assert_cmpstr(prop_test_pspecs[PROP_LAMBDA_START]->name,
                    ==, "lambda-start");
    g_assert(prop_test_pspecs[PROP_N_PARAMS] != NULL);
    g_assert_cmpstr(prop_test_pspecs[PROP_N_PARAMS]->name,
                    ==, "n-params");
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

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
