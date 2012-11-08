/*
 *  $Id$
 *  Copyright (C) 2009,2011 David Nečas (Yeti).
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
#include <glib/gstdio.h>
#include <stdio.h>

/***************************************************************************
 *
 * Gradient
 *
 ***************************************************************************/

static void
test_gradient_load_check(const gchar *filename,
                         const gchar *expected_name,
                         const guint expected_n_points,
                         const GwyGradientPoint *expected_point0,
                         const GwyGradientPoint *expected_pointn)
{
    GError *error = NULL;
    GwyResource *resource = gwy_resource_load(filename, GWY_TYPE_GRADIENT, TRUE,
                                              &error);
    g_assert_no_error(error);
    g_assert(GWY_IS_GRADIENT(resource));
    GwyGradient *gradient = GWY_GRADIENT(resource);
    g_assert_cmpstr(gwy_resource_get_name(resource), ==, expected_name);
    g_assert(!gwy_resource_is_managed(resource));
    g_assert(gwy_resource_is_modifiable(resource));

    resource_check_file(resource, filename);

    g_assert_cmpuint(gwy_gradient_n_points(gradient), ==, expected_n_points);
    GwyGradientPoint pt;
    pt = gwy_gradient_get(gradient, 0);
    g_assert_cmpint(memcmp(&pt, expected_point0, sizeof(GwyGradientPoint)),
                    ==, 0);
    pt = gwy_gradient_get(gradient, expected_n_points-1);
    g_assert_cmpint(memcmp(&pt, expected_pointn, sizeof(GwyGradientPoint)),
                    ==, 0);
    GWY_OBJECT_UNREF(gradient);
}

void
test_gradient_load(void)
{
    static const GwyGradientPoint gradient_point_black0 = { 0, { 0, 0, 0, 1 } };
    static const GwyGradientPoint gradient_point_white1 = { 1, { 1, 1, 1, 1 } };

    static const gchar gradient_v2[] =
        "Gwyddion resource GwyGradient\n"
        "0 0 0 0 1\n"
        "0.33 0 0 1 1\n"
        "0.5 0 0.919997 0 1\n"
        "0.67 1 1 0 1\n"
        "1 1 1 1 1\n";

    static const gchar gradient_v3[] =
        "Gwyddion3 resource GwyGradient\n"
        "name Yellow Blue 2\n"
        "0 0 0 0 1\n"
        "0.33 0 0 1 1\n"
        "0.5 0 0.919997 0 1\n"
        "0.67 1 1 0 1\n"
        "1 1 1 1 1\n";

    static const gchar gradient_v3_ugly[] =
        "Gwyddion3 resource   GwyGradient   \n"
        "  name         Testing Gray²  \n"
        "\n"
        "  0   0 0 0 1\n"
        "\n"
        "1  1   1  1e0  1\n";

    GError *error = NULL;

    // Version2 resource
    g_assert(g_file_set_contents("Yellow Blue 2", gradient_v2, -1, &error));
    g_test_queue_destroy((GDestroyNotify)g_unlink, "Yellow Blue 2");
    g_assert_no_error(error);
    test_gradient_load_check("Yellow Blue 2", "Yellow Blue 2", 5,
                             &gradient_point_black0, &gradient_point_white1);

    // Version3 resource
    g_assert(g_file_set_contents("YBL2", gradient_v3, -1, &error));
    g_test_queue_destroy((GDestroyNotify)g_unlink, "YBL2");
    g_assert_no_error(error);
    test_gradient_load_check("YBL2", "Yellow Blue 2", 5,
                             &gradient_point_black0, &gradient_point_white1);

    // Version3 ugly resource
    g_assert(g_file_set_contents("Ugly-Gray", gradient_v3_ugly, -1, &error));
    g_test_queue_destroy((GDestroyNotify)g_unlink, "Ugly-Gray");
    g_assert_no_error(error);
    test_gradient_load_check("Ugly-Gray", "Testing Gray²", 2,
                             &gradient_point_black0, &gradient_point_white1);

}

void
test_gradient_save(void)
{
    static const GwyGradientPoint gradient_point_red0 = { 0, { 0.8, 0, 0, 1 } };
    static const GwyGradientPoint gradient_point_hg = { 0.5, { 0, 0.6, 0, 1 } };
    static const GwyGradientPoint gradient_point_blue1 = { 1, { 0, 0, 1, 1 } };

    GwyGradient *gradient = gwy_gradient_new();
    GwyResource *resource = GWY_RESOURCE(gradient);

    gwy_resource_set_name(resource, "Tricolor");
    g_assert_cmpstr(gwy_resource_get_name(resource), ==, "Tricolor");

    gwy_gradient_set_color(gradient, 0, &gradient_point_red0.color);
    gwy_gradient_set_color(gradient, 1, &gradient_point_blue1.color);
    gwy_gradient_insert_sorted(gradient, &gradient_point_hg);
    g_assert_cmpuint(gwy_gradient_n_points(gradient), ==, 3);
    GwyGradientPoint pt = gwy_gradient_get(gradient, 1);
    g_assert_cmpint(memcmp(&pt, &gradient_point_hg, sizeof(GwyGradientPoint)),
                    ==, 0);

    GError *error = NULL;
    gwy_resource_set_filename(resource, "Tricolor");
    g_assert(gwy_resource_save(resource, &error));
    g_assert_no_error(error);
    g_test_queue_destroy((GDestroyNotify)g_unlink, "Tricolor");
    resource_check_file(resource, "Tricolor");
    GWY_OBJECT_UNREF(gradient);
    test_gradient_load_check("Tricolor", "Tricolor", 3,
                             &gradient_point_red0, &gradient_point_blue1);
}

void
test_gradient_serialize(void)
{
    static const GwyGradientPoint gradient_point_red0 = { 0, { 0.8, 0, 0, 1 } };
    static const GwyGradientPoint gradient_point_blue1 = { 1, { 0, 0, 1, 1 } };

    GwyGradient *gradient = gwy_gradient_new();
    GwyResource *resource = GWY_RESOURCE(gradient);

    gwy_resource_set_name(resource, "Red-Blue");
    g_assert_cmpstr(gwy_resource_get_name(resource), ==, "Red-Blue");

    GwyGradientPoint pt;
    gwy_gradient_set(gradient, 0, &gradient_point_red0);
    gwy_gradient_set(gradient, 1, &gradient_point_blue1);
    pt = gwy_gradient_get(gradient, 0);
    g_assert_cmpint(memcmp(&pt, &gradient_point_red0,
                           sizeof(GwyGradientPoint)), ==, 0);
    pt = gwy_gradient_get(gradient, 1);
    g_assert_cmpint(memcmp(&pt, &gradient_point_blue1,
                           sizeof(GwyGradientPoint)), ==, 0);

    serializable_duplicate(GWY_SERIALIZABLE(gradient), NULL);
    serializable_assign(GWY_SERIALIZABLE(gradient), NULL);

    GwyGradient *newgradient
        = (GwyGradient*)serialize_and_back(G_OBJECT(gradient), NULL);
    GwyResource *newresource = GWY_RESOURCE(newgradient);
    g_assert_cmpstr(gwy_resource_get_name(newresource), ==, "Red-Blue");

    pt = gwy_gradient_get(newgradient, 0);
    g_assert_cmpint(memcmp(&pt, &gradient_point_red0,
                           sizeof(GwyGradientPoint)), ==, 0);
    pt = gwy_gradient_get(newgradient, 1);
    g_assert_cmpint(memcmp(&pt, &gradient_point_blue1,
                           sizeof(GwyGradientPoint)), ==, 0);

    g_object_unref(gradient);
    g_object_unref(newgradient);
}

void
test_gradient_serialize_failure_noparent(void)
{
    GOutputStream *stream = g_memory_output_stream_new(NULL, 0,
                                                       g_realloc, g_free);
    GDataOutputStream *datastream = g_data_output_stream_new(stream);
    g_data_output_stream_set_byte_order(datastream,
                                        G_DATA_STREAM_BYTE_ORDER_LITTLE_ENDIAN);

    guint len = 10;
    data_stream_put_string0(datastream, "GwyGradient", NULL, NULL);
    g_data_output_stream_put_uint64(datastream, 0, NULL, NULL);
    data_stream_put_string0(datastream, "data", NULL, NULL);
    g_data_output_stream_put_byte(datastream, GWY_SERIALIZABLE_DOUBLE_ARRAY,
                                  NULL, NULL);
    g_data_output_stream_put_uint64(datastream, len, NULL, NULL);
    for (guint i = 0; i < len; i++)
        data_stream_put_double(datastream, i, NULL, NULL);

    GwyErrorList *error_list = NULL;
    gwy_error_list_add(&error_list,
                       GWY_DESERIALIZE_ERROR, GWY_DESERIALIZE_ERROR_PARENT,
                       "Object ‘%s’ has a serializable parent but "
                       "its representation does not contain any "
                       "parent class item.",
                       "GwyGradient");

    deserialize_assert_failure(G_MEMORY_OUTPUT_STREAM(stream), error_list);
    gwy_error_list_clear(&error_list);
    g_object_unref(datastream);
    g_object_unref(stream);
}

void
test_gradient_serialize_failure_odd(void)
{
    GOutputStream *stream = g_memory_output_stream_new(NULL, 0,
                                                       g_realloc, g_free);
    GDataOutputStream *datastream = g_data_output_stream_new(stream);
    g_data_output_stream_set_byte_order(datastream,
                                        G_DATA_STREAM_BYTE_ORDER_LITTLE_ENDIAN);

    guint len = 11;
    data_stream_put_string0(datastream, "GwyGradient", NULL, NULL);
    g_data_output_stream_put_uint64(datastream, 0, NULL, NULL);
    data_stream_put_string0(datastream, "data", NULL, NULL);
    g_data_output_stream_put_byte(datastream, GWY_SERIALIZABLE_DOUBLE_ARRAY,
                                  NULL, NULL);
    g_data_output_stream_put_uint64(datastream, len, NULL, NULL);
    for (guint i = 0; i < len; i++)
        data_stream_put_double(datastream, i, NULL, NULL);
    data_stream_put_string0(datastream, "GwyResource", NULL, NULL);
    g_data_output_stream_put_byte(datastream, GWY_SERIALIZABLE_PARENT,
                                  NULL, NULL);
    data_stream_put_string0(datastream, "name", NULL, NULL);
    g_data_output_stream_put_byte(datastream, GWY_SERIALIZABLE_STRING,
                                  NULL, NULL);
    data_stream_put_string0(datastream, "Gradient", NULL, NULL);

    GwyErrorList *error_list = NULL;
    gwy_error_list_add(&error_list,
                       GWY_DESERIALIZE_ERROR, GWY_DESERIALIZE_ERROR_INVALID,
                       "Data length of ‘GwyGradient’ is %lu which is not "
                       "a multiple of 5.",
                       (gulong)len);

    deserialize_assert_failure(G_MEMORY_OUTPUT_STREAM(stream), error_list);
    gwy_error_list_clear(&error_list);
    g_object_unref(datastream);
    g_object_unref(stream);
}

void
test_gradient_inventory(void)
{
    static const GwyGradientPoint gradient_point_red = { 0.5, { 1, 0, 0, 1 } };
    const GwyInventoryItemType *item_type;
    GwyGradient *gradient;
    GwyResource *resource;

    GwyInventory *gradients = gwy_gradients();
    g_assert(GWY_IS_INVENTORY(gradients));
    item_type = gwy_inventory_get_item_type(gradients);
    g_assert(item_type);
    g_assert_cmpuint(item_type->type, ==, GWY_TYPE_GRADIENT);
    g_assert(gwy_inventory_can_make_copies(gradients));
    g_assert_cmpstr(gwy_inventory_get_default_name(gradients),
                    ==, GWY_GRADIENT_DEFAULT);

    item_type = gwy_resource_type_get_item_type(GWY_TYPE_GRADIENT);
    g_assert(item_type);
    g_assert_cmpuint(item_type->type, ==, GWY_TYPE_GRADIENT);
    gradient = gwy_gradients_get(NULL);
    g_assert(GWY_IS_GRADIENT(gradient));
    resource = GWY_RESOURCE(gradient);
    g_assert_cmpstr(gwy_resource_get_name(resource), ==, GWY_GRADIENT_DEFAULT);
    g_assert(gwy_resource_is_managed(resource));
    g_assert(!gwy_resource_is_modifiable(resource));

    gradient = gwy_inventory_get_default(gradients);
    g_assert(GWY_IS_GRADIENT(gradient));
    resource = GWY_RESOURCE(gradient);
    g_assert_cmpstr(gwy_resource_get_name(resource), ==, GWY_GRADIENT_DEFAULT);
    g_assert(gwy_resource_is_managed(resource));
    g_assert(!gwy_resource_is_modifiable(resource));

    g_assert(!gwy_resource_get_preferred(resource));
    gwy_resource_set_preferred(resource, TRUE);
    g_assert(gwy_resource_get_preferred(resource));

    gwy_inventory_copy(gradients, GWY_GRADIENT_DEFAULT, "Another");
    g_assert_cmpuint(gwy_inventory_size(gradients), ==, 2);
    gradient = gwy_inventory_get(gradients, "Another");
    resource = GWY_RESOURCE(gradient);
    g_assert_cmpstr(gwy_resource_get_name(resource), ==, "Another");
    gboolean modified;
    g_object_get(gradient, "modified", &modified, NULL);
    g_assert(gwy_resource_is_managed(resource));
    g_assert(gwy_resource_is_modifiable(resource));
    g_assert(modified);
    gwy_gradient_insert_sorted(gradient, &gradient_point_red);
    g_assert_cmpuint(gwy_gradient_n_points(gradient), ==, 3);

    g_object_ref(gradient);
    gwy_inventory_delete(gradients, "Another");
    g_assert(GWY_IS_GRADIENT(gradient));
    g_assert(!gwy_resource_is_managed(resource));
    g_assert(gwy_resource_is_modifiable(resource));
    g_object_unref(gradient);
}

void
test_gradient_manipulate(void)
{
    static const GwyGradientPoint gradient_point_red0 = { 0, { 0.8, 0, 0, 1 } };
    static const GwyGradientPoint gradient_point_p1 = { 0.5, { 0, 0.6, 0, 1 } };
    static const GwyGradientPoint gradient_point_p2 = { 0.6, { 0, 0.5, 0, 1 } };
    static const GwyGradientPoint gradient_point_blue1 = { 1, { 0, 0, 1, 1 } };

    GwyGradient *gradient = gwy_gradient_new();
    GwyResource *resource = GWY_RESOURCE(gradient);

    gwy_resource_set_name(resource, "Unstable");
    g_assert_cmpstr(gwy_resource_get_name(resource), ==, "Unstable");

    gwy_gradient_set_color(gradient, 0, &gradient_point_red0.color);
    gwy_gradient_set_color(gradient, 1, &gradient_point_blue1.color);
    gwy_gradient_insert(gradient, 1, &gradient_point_p2);
    g_assert_cmpuint(gwy_gradient_n_points(gradient), ==, 3);
    GwyGradientPoint pt = gwy_gradient_get(gradient, 1);
    g_assert_cmpint(memcmp(&pt, &gradient_point_p2, sizeof(GwyGradientPoint)),
                    ==, 0);

    gwy_gradient_insert(gradient, 1, &gradient_point_p1);
    g_assert_cmpuint(gwy_gradient_n_points(gradient), ==, 4);
    pt = gwy_gradient_get(gradient, 1);
    g_assert_cmpint(memcmp(&pt, &gradient_point_p1, sizeof(GwyGradientPoint)),
                    ==, 0);
    pt = gwy_gradient_get(gradient, 2);
    g_assert_cmpint(memcmp(&pt, &gradient_point_p2, sizeof(GwyGradientPoint)),
                    ==, 0);

    GwyRGBA color;
    gwy_gradient_color(gradient, gradient_point_red0.x, &color);
    g_assert_cmpint(memcmp(&color, &gradient_point_red0.color, sizeof(GwyRGBA)),
                    ==, 0);
    gwy_gradient_color(gradient, gradient_point_p1.x, &color);
    g_assert_cmpint(memcmp(&color, &gradient_point_p1.color, sizeof(GwyRGBA)),
                    ==, 0);
    gwy_gradient_color(gradient, gradient_point_p2.x, &color);
    g_assert_cmpint(memcmp(&color, &gradient_point_p2.color, sizeof(GwyRGBA)),
                    ==, 0);
    gwy_gradient_color(gradient, gradient_point_blue1.x, &color);
    g_assert_cmpint(memcmp(&color, &gradient_point_blue1.color, sizeof(GwyRGBA)),
                    ==, 0);

    guint n;
    const GwyGradientPoint *gdata = gwy_gradient_get_data(gradient, &n);
    g_assert_cmpuint(n, ==, 4);
    g_assert(gdata);
    GwyGradientPoint *mydata = g_memdup(gdata, n*sizeof(GwyGradientPoint));

    gwy_gradient_delete(gradient, 1);
    g_assert_cmpuint(gwy_gradient_n_points(gradient), ==, 3);
    pt = gwy_gradient_get(gradient, 1);
    g_assert_cmpint(memcmp(&pt, &gradient_point_p2, sizeof(GwyGradientPoint)),
                    ==, 0);

    gwy_gradient_delete(gradient, 1);
    g_assert_cmpuint(gwy_gradient_n_points(gradient), ==, 2);

    gwy_gradient_set_data(gradient, n, mydata);
    gdata = gwy_gradient_get_data(gradient, &n);
    g_assert_cmpuint(n, ==, 4);
    g_assert(gdata);
    g_assert_cmpint(memcmp(gdata, mydata, n*sizeof(GwyGradientPoint)), ==, 0);

    g_free(mydata);
    g_object_unref(gradient);
}

void
test_gradient_error_bad_line(void)
{
    resource_assert_load_error
        ("Gwyddion3 resource GwyGradient\n"
         "name Broken gradient\n"
         "WTF?!\n"
         "0 0 0 0 0\n",
         GWY_TYPE_GRADIENT,
         GWY_RESOURCE_ERROR, GWY_RESOURCE_ERROR_DATA);
}

void
test_gradient_error_no_name(void)
{
    resource_assert_load_error
        ("Gwyddion3 resource GwyGradient\n"
         "0 0 0 0 0\n",
         GWY_TYPE_GRADIENT,
         GWY_RESOURCE_ERROR, GWY_RESOURCE_ERROR_NAME);
}

void
test_gradient_error_bad_name(void)
{
    resource_assert_load_error
        ("Gwyddion3 resource GwyGradient\n"
         "name \n",
         GWY_TYPE_GRADIENT,
         GWY_RESOURCE_ERROR, GWY_RESOURCE_ERROR_NAME);
    resource_assert_load_error
        ("Gwyddion3 resource GwyGradient\n"
         "name \x8e\n",
         GWY_TYPE_GRADIENT,
         GWY_RESOURCE_ERROR, GWY_RESOURCE_ERROR_NAME);
}

void
test_gradient_error_bad_data(void)
{
    resource_assert_load_error
        ("Gwyddion3 resource GwyGradient\n"
         "name Broken gradient 3\n"
         "0 0 0 0 0\n"
         "1 0 0 0 0 0\n",
         GWY_TYPE_GRADIENT,
         GWY_RESOURCE_ERROR, GWY_RESOURCE_ERROR_DATA);
    resource_assert_load_error
        ("Gwyddion3 resource GwyGradient\n"
         "name Broken gradient 3\n"
         "0 0 0 0 0\n"
         "1 0 0 0\n",
         GWY_TYPE_GRADIENT,
         GWY_RESOURCE_ERROR, GWY_RESOURCE_ERROR_DATA);
}

void
test_gradient_point_boxed(void)
{
    GwyGradientPoint pt = { 0.5, { 0.25, 0.0, 1.0, 0.75 } };
    GwyGradientPoint *copy = g_boxed_copy(GWY_TYPE_GRADIENT_POINT, &pt);
    g_assert_cmpfloat(copy->x, ==, pt.x);
    g_assert_cmpfloat(copy->color.r, ==, pt.color.r);
    g_assert_cmpfloat(copy->color.g, ==, pt.color.g);
    g_assert_cmpfloat(copy->color.b, ==, pt.color.b);
    g_boxed_free(GWY_TYPE_GRADIENT_POINT, copy);
}

void
test_gradient_load_builtins(void)
{
    GwyErrorList *error_list = NULL;
    GwyInventory *inventory = gwy_resource_type_get_inventory(GWY_TYPE_GRADIENT);
    guint nbefore = gwy_inventory_size(inventory);
    gwy_resource_type_load_builtins(GWY_TYPE_GRADIENT, &error_list);
    assert_error_list(error_list, NULL);
    guint nafter = gwy_inventory_size(inventory);
    // We assert there are some builtin gradients
    g_assert_cmpuint(nafter, >, nbefore);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
