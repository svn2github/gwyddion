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
 * GL Material
 *
 ***************************************************************************/

static void
test_gl_material_load_check(const gchar *filename,
                            const gchar *expected_name,
                            const GwyRGBA *expected_ambient,
                            const GwyRGBA *expected_diffuse,
                            const GwyRGBA *expected_specular,
                            const GwyRGBA *expected_emission,
                            gdouble expected_shininess)
{
    GError *error = NULL;
    GwyResource *resource = gwy_resource_load(filename, GWY_TYPE_GL_MATERIAL,
                                              TRUE,
                                              &error);
    g_assert_no_error(error);
    g_assert(GWY_IS_GL_MATERIAL(resource));
    GwyGLMaterial *gl_material = GWY_GL_MATERIAL(resource);
    g_assert_cmpstr(gwy_resource_get_name(resource), ==, expected_name);
    g_assert(!gwy_resource_is_managed(resource));
    g_assert(gwy_resource_is_modifiable(resource));

    resource_check_file(resource, filename);

    GwyRGBA color;
    color = gwy_gl_material_get_ambient(gl_material);
    g_assert(gwy_serializable_boxed_equal(GWY_TYPE_RGBA,
                                          &color, expected_ambient));
    color = gwy_gl_material_get_diffuse(gl_material);
    g_assert(gwy_serializable_boxed_equal(GWY_TYPE_RGBA,
                                          &color, expected_diffuse));
    color = gwy_gl_material_get_specular(gl_material);
    g_assert(gwy_serializable_boxed_equal(GWY_TYPE_RGBA,
                                          &color, expected_specular));
    color = gwy_gl_material_get_emission(gl_material);
    g_assert(gwy_serializable_boxed_equal(GWY_TYPE_RGBA,
                                          &color, expected_emission));
    g_assert_cmpfloat(gwy_gl_material_get_shininess(gl_material),
                      ==, expected_shininess);
    GWY_OBJECT_UNREF(gl_material);
}

void
test_gl_material_load(void)
{
    static const GwyRGBA red   = { 0.8, 0,   0,   1 };
    static const GwyRGBA green = { 0,   0.6, 0,   1 };
    static const GwyRGBA blue  = { 0,   0,   0.9, 1 };
    static const GwyRGBA white = { 1,   1,   1,   1 };

    static const gchar gl_material_v2[] =
        "Gwyddion resource GwyGLMaterial\n"
        "0.8 0 0 1\n"
        "0 0.6 0 1\n"
        "0 0 0.9 1\n"
        "1 1 1 1\n"
        "0.5";

    static const gchar gl_material_v3[] =
        "Gwyddion3 resource GwyGLMaterial\n"
        "name Rainbow Surprise\n"
        "0.8 0 0 1\n"
        "0 0.6 0 1\n"
        "0 0 0.9 1\n"
        "1 1 1 1\n"
        "0.5";

    static const gchar gl_material_v3_ugly[] =
        "Gwyddion3 resource GwyGLMaterial   \n"
        "\tname     \t\t   α→ω    \t \n"
        "\n"
        "0.80000 0 0 1\n"
        "     0 6e-1 0 1\n"
        "\n"
        "0\t0\t.9\t\t\t\t1\n"
        "1. 1. 1. 0.1e1     \n"
        "\n"
        "\n"
        "0.5    "
        "\n";

    GError *error = NULL;

    // Version2 resource
    g_assert(g_file_set_contents("Rainbow Surprise", gl_material_v2, -1,
                                 &error));
    g_test_queue_destroy((GDestroyNotify)g_unlink, "Rainbow Surprise");
    g_assert_no_error(error);
    test_gl_material_load_check("Rainbow Surprise", "Rainbow Surprise",
                                &red, &green, &blue, &white, 0.5);

    // Version3 resource
    g_assert(g_file_set_contents("RS3", gl_material_v3, -1, &error));
    g_test_queue_destroy((GDestroyNotify)g_unlink, "RS3");
    g_assert_no_error(error);
    test_gl_material_load_check("RS3", "Rainbow Surprise",
                                &red, &green, &blue, &white, 0.5);

    // Version3 ugly resource
    g_assert(g_file_set_contents("Alpha-Omega", gl_material_v3_ugly, -1,
                                 &error));
    g_test_queue_destroy((GDestroyNotify)g_unlink, "Alpha-Omega");
    g_assert_no_error(error);
    test_gl_material_load_check("Alpha-Omega", "α→ω",
                                &red, &green, &blue, &white, 0.5);
}

void
test_gl_material_save(void)
{
    static const GwyRGBA black = { 0,   0,   0,   1 };
    static const GwyRGBA red   = { 0.8, 0,   0,   1 };
    static const GwyRGBA green = { 0,   0.6, 0,   1 };
    static const GwyRGBA blue  = { 0,   0,   0.9, 1 };

    GwyGLMaterial *gl_material = gwy_gl_material_new();
    GwyResource *resource = GWY_RESOURCE(gl_material);

    gwy_resource_set_name(resource, "AlienGL");
    g_assert_cmpstr(gwy_resource_get_name(resource), ==, "AlienGL");

    gwy_gl_material_set_ambient(gl_material, &blue);
    gwy_gl_material_set_diffuse(gl_material, &green);
    gwy_gl_material_set_specular(gl_material, &red);
    gwy_gl_material_set_emission(gl_material, &black);
    gwy_gl_material_set_shininess(gl_material, 0.1);

    GError *error = NULL;
    gwy_resource_set_filename(resource, "AlienGL");
    g_assert(gwy_resource_save(resource, &error));
    g_assert_no_error(error);
    g_test_queue_destroy((GDestroyNotify)g_unlink, "AlienGL");
    resource_check_file(resource, "AlienGL");
    GWY_OBJECT_UNREF(gl_material);
    test_gl_material_load_check("AlienGL", "AlienGL",
                                &blue, &green, &red, &black, 0.1);
}

void
test_gl_material_serialize(void)
{
    static const GwyRGBA white = { 1,   1,   1,   1 };
    static const GwyRGBA red   = { 0.8, 0,   0,   1 };
    static const GwyRGBA green = { 0,   0.6, 0,   1 };
    static const GwyRGBA blue  = { 0,   0,   0.9, 1 };

    GwyGLMaterial *gl_material = gwy_gl_material_new();
    GwyResource *resource = GWY_RESOURCE(gl_material);

    gwy_resource_set_name(resource, "WRBG");
    g_assert_cmpstr(gwy_resource_get_name(resource), ==, "WRBG");

    gwy_gl_material_set_ambient(gl_material, &white);
    gwy_gl_material_set_diffuse(gl_material, &red);
    gwy_gl_material_set_specular(gl_material, &blue);
    gwy_gl_material_set_emission(gl_material, &green);
    gwy_gl_material_set_shininess(gl_material, 0.67);

    serializable_duplicate(GWY_SERIALIZABLE(gl_material), NULL);
    serializable_assign(GWY_SERIALIZABLE(gl_material), NULL);

    GwyGLMaterial *newgl_material
        = (GwyGLMaterial*)serialize_and_back(G_OBJECT(gl_material), NULL);
    GwyResource *newresource = GWY_RESOURCE(newgl_material);
    g_assert_cmpstr(gwy_resource_get_name(newresource), ==, "WRBG");

    GwyRGBA color;
    color = gwy_gl_material_get_ambient(gl_material);
    g_assert(gwy_serializable_boxed_equal(GWY_TYPE_RGBA, &color, &white));
    color = gwy_gl_material_get_diffuse(gl_material);
    g_assert(gwy_serializable_boxed_equal(GWY_TYPE_RGBA, &color, &red));
    color = gwy_gl_material_get_specular(gl_material);
    g_assert(gwy_serializable_boxed_equal(GWY_TYPE_RGBA, &color, &blue));
    color = gwy_gl_material_get_emission(gl_material);
    g_assert(gwy_serializable_boxed_equal(GWY_TYPE_RGBA, &color, &green));
    g_assert_cmpfloat(gwy_gl_material_get_shininess(gl_material), ==, 0.67);

    g_object_unref(gl_material);
    g_object_unref(newgl_material);
}

void
test_gl_material_inventory(void)
{
    const GwyInventoryItemType *item_type;
    GwyGLMaterial *gl_material;
    GwyResource *resource;

    GwyInventory *gl_materials = gwy_gl_materials();
    g_assert(GWY_IS_INVENTORY(gl_materials));
    item_type = gwy_inventory_get_item_type(gl_materials);
    g_assert(item_type);
    g_assert_cmpuint(item_type->type, ==, GWY_TYPE_GL_MATERIAL);
    g_assert(gwy_inventory_can_make_copies(gl_materials));
    g_assert_cmpstr(gwy_inventory_get_default_name(gl_materials),
                    ==, GWY_GL_MATERIAL_DEFAULT);

    item_type = gwy_resource_type_get_item_type(GWY_TYPE_GL_MATERIAL);
    g_assert(item_type);
    g_assert_cmpuint(item_type->type, ==, GWY_TYPE_GL_MATERIAL);
    gl_material = gwy_gl_materials_get(NULL);
    g_assert(GWY_IS_GL_MATERIAL(gl_material));
    resource = GWY_RESOURCE(gl_material);
    g_assert_cmpstr(gwy_resource_get_name(resource), ==,
                    GWY_GL_MATERIAL_DEFAULT);
    g_assert(gwy_resource_is_managed(resource));
    g_assert(!gwy_resource_is_modifiable(resource));

    gl_material = gwy_inventory_get_default(gl_materials);
    g_assert(GWY_IS_GL_MATERIAL(gl_material));
    resource = GWY_RESOURCE(gl_material);
    g_assert_cmpstr(gwy_resource_get_name(resource), ==,
                    GWY_GL_MATERIAL_DEFAULT);
    g_assert(gwy_resource_is_managed(resource));
    g_assert(!gwy_resource_is_modifiable(resource));

    g_assert(!gwy_resource_get_is_preferred(resource));
    gwy_resource_set_is_preferred(resource, TRUE);
    g_assert(gwy_resource_get_is_preferred(resource));

    gwy_inventory_copy(gl_materials, GWY_GL_MATERIAL_DEFAULT, "Another");
    g_assert_cmpuint(gwy_inventory_size(gl_materials), ==, 3);
    gl_material = gwy_inventory_get(gl_materials, "Another");
    resource = GWY_RESOURCE(gl_material);
    g_assert_cmpstr(gwy_resource_get_name(resource), ==, "Another");
    gboolean is_modified;
    g_object_get(gl_material, "is-modified", &is_modified, NULL);
    g_assert(gwy_resource_is_managed(resource));
    g_assert(gwy_resource_is_modifiable(resource));
    g_assert(is_modified);

    g_object_ref(gl_material);
    gwy_inventory_delete(gl_materials, "Another");
    g_assert(GWY_IS_GL_MATERIAL(gl_material));
    g_assert(!gwy_resource_is_managed(resource));
    g_assert(gwy_resource_is_modifiable(resource));
    g_object_unref(gl_material);
}

void
test_gl_material_error_bad_line(void)
{
    resource_assert_load_error
        ("Gwyddion3 resource GwyGLMaterial\n"
         "name Broken gl_material\n"
         "WTF?!\n"
         "0 0 0 0\n"
         "0 0 0 0\n"
         "0 0 0 0\n"
         "0 0 0 0\n"
         "0.5\n",
         GWY_TYPE_GL_MATERIAL,
         GWY_RESOURCE_ERROR, GWY_RESOURCE_ERROR_DATA);
}

void
test_gl_material_error_no_name(void)
{
    resource_assert_load_error
        ("Gwyddion3 resource GwyGLMaterial\n"
         "0 0 0 0\n"
         "0 0 0 0\n"
         "0 0 0 0\n"
         "0 0 0 0\n"
         "0.5\n",
         GWY_TYPE_GL_MATERIAL,
         GWY_RESOURCE_ERROR, GWY_RESOURCE_ERROR_NAME);
}

void
test_gl_material_error_bad_name(void)
{
    resource_assert_load_error
        ("Gwyddion3 resource GwyGLMaterial\n"
         "name \n"
         "0 0 0 0\n"
         "0 0 0 0\n"
         "0 0 0 0\n"
         "0 0 0 0\n"
         "0.5\n",
         GWY_TYPE_GL_MATERIAL,
         GWY_RESOURCE_ERROR, GWY_RESOURCE_ERROR_NAME);
    resource_assert_load_error
        ("Gwyddion3 resource GwyGLMaterial\n"
         "name \x8e\n"
         "0 0 0 0\n"
         "0 0 0 0\n"
         "0 0 0 0\n"
         "0 0 0 0\n"
         "0.5\n",
         GWY_TYPE_GL_MATERIAL,
         GWY_RESOURCE_ERROR, GWY_RESOURCE_ERROR_NAME);
}

void
test_gl_material_error_bad_data(void)
{
    resource_assert_load_error
        ("Gwyddion3 resource GwyGLMaterial\n"
         "name Broken GL material 3\n"
         "0 0 0 0\n"
         "0 0 0 0\n"
         "0 0 0\n"
         "0 0 0 0\n"
         "0.5\n",
         GWY_TYPE_GL_MATERIAL,
         GWY_RESOURCE_ERROR, GWY_RESOURCE_ERROR_DATA);
    resource_assert_load_error
        ("Gwyddion3 resource GwyGLMaterial\n"
         "name Broken GL material 3\n"
         "0 0 0 0\n"
         "0 0 0 0\n"
         "0 0 0 0 0\n"
         "0 0 0 0\n"
         "0.5\n",
         GWY_TYPE_GL_MATERIAL,
         GWY_RESOURCE_ERROR, GWY_RESOURCE_ERROR_DATA);
    resource_assert_load_error
        ("Gwyddion3 resource GwyGLMaterial\n"
         "name Broken GL material 3\n"
         "0 0 0 0\n"
         "0 0 0 0\n"
         "0 0 0 0\n"
         "0.5\n",
         GWY_TYPE_GL_MATERIAL,
         GWY_RESOURCE_ERROR, GWY_RESOURCE_ERROR_DATA);
    resource_assert_load_error
        ("Gwyddion3 resource GwyGLMaterial\n"
         "name Broken GL material 3\n"
         "0 0 0 0\n"
         "0 0 0 0\n"
         "0 0 0 0\n"
         "0 0 0 0\n",
         GWY_TYPE_GL_MATERIAL,
         GWY_RESOURCE_ERROR, GWY_RESOURCE_ERROR_DATA);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
