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
#include <glib/gstdio.h>
#include <stdio.h>

/***************************************************************************
 *
 * User Grain Value
 *
 ***************************************************************************/

static void
user_grain_value_load_check(const gchar *filename,
                            const gchar *expected_name,
                            const gchar *expected_group,
                            const gchar *expected_formula,
                            gint expected_power_xy,
                            gint expected_power_z,
                            gboolean expected_same_units,
                            gboolean expected_is_angle)
{
    GError *error = NULL;
    GwyResource *resource = gwy_resource_load(filename,
                                              GWY_TYPE_USER_GRAIN_VALUE,
                                              TRUE, &error);
    g_assert(!error);
    g_assert(GWY_IS_USER_GRAIN_VALUE(resource));
    GwyUserGrainValue *usergrainvalue = GWY_USER_GRAIN_VALUE(resource);
    g_assert_cmpstr(gwy_resource_get_name(resource), ==, expected_name);
    g_assert_cmpstr(gwy_user_grain_value_get_group(usergrainvalue),
                    ==, expected_group);
    g_assert(!gwy_resource_is_managed(resource));
    g_assert(gwy_resource_is_modifiable(resource));

    gchar *res_filename = NULL;
    g_object_get(resource, "file-name", &res_filename, NULL);
    GFile *gfile = g_file_new_for_path(filename);
    GFile *res_gfile = g_file_new_for_path(res_filename);
    g_assert(g_file_equal(gfile, res_gfile));
    g_object_unref(gfile);
    g_object_unref(res_gfile);
    g_free(res_filename);

    g_assert_cmpstr(gwy_user_grain_value_get_formula(usergrainvalue),
                    ==, expected_formula);
    g_assert_cmpint(gwy_user_grain_value_get_power_xy(usergrainvalue),
                    ==, expected_power_xy);
    g_assert_cmpint(gwy_user_grain_value_get_power_z(usergrainvalue),
                    ==, expected_power_z);
    g_assert_cmpint(gwy_user_grain_value_get_same_units(usergrainvalue),
                    ==, expected_same_units);
    g_assert_cmpint(gwy_user_grain_value_get_is_angle(usergrainvalue),
                    ==, expected_is_angle);
    GWY_OBJECT_UNREF(usergrainvalue);
}

void
test_user_grain_value_load(void)
{
    static const gchar usergrainvalue_v2[] =
        "Gwyddion resource GwyUserGrainValue\n"
        "formula (z_max - z_min)/D_max\n"
        "power_xy 0\n"
        "power_z 0\n"
        "is_angle 1\n"
        "same_units 1\n";

    static const gchar usergrainvalue_v3[] =
        "Gwyddion3 resource GwyUserGrainValue\n"
        "name Sloppiness v3\n"
        "group Group that hopefully does not exist\n"
        "formula (z_max - z_min)/D_max\n"
        "power_xy 0\n"
        "power_z 0\n"
        "is_angle 1\n"
        "same_units 1\n";

    static const gchar usergrainvalue_v3_ugly[] =
        "Gwyddion3 resource GwyUserGrainValue\n"
        " name    Sloppiness³   \t\n"
        "        formula\t(z_max - z_min)/D_max\n"
        "            \tis_angle \t1\n"
        "\t \n"
        "\tsame_units\t1\t           \t\n";

    GError *error = NULL;

    // Version2 resource
    g_assert(g_file_set_contents("Sloppiness", usergrainvalue_v2, -1, &error));
    g_test_queue_destroy((GDestroyNotify)g_unlink, "Sloppiness2");
    g_assert(!error);
    user_grain_value_load_check("Sloppiness", "Sloppiness", "User",
                                "(z_max - z_min)/D_max", 0, 0, TRUE, TRUE);

    // Version3 resource
    g_assert(g_file_set_contents("Sloppiness3", usergrainvalue_v3, -1, &error));
    g_test_queue_destroy((GDestroyNotify)g_unlink, "Sloppiness3");
    g_assert(!error);
    user_grain_value_load_check("Sloppiness3", "Sloppiness v3",
                                "Group that hopefully does not exist",
                                "(z_max - z_min)/D_max", 0, 0, TRUE, TRUE);

    // Version3 ugly resource
    g_assert(g_file_set_contents("Ugly-Slop", usergrainvalue_v3_ugly, -1,
                                 &error));
    g_test_queue_destroy((GDestroyNotify)g_unlink, "Ugly-Slop");
    g_assert(!error);
    user_grain_value_load_check("Ugly-Slop", "Sloppiness³", "User",
                                "(z_max - z_min)/D_max", 0, 0, TRUE, TRUE);
}

#if 0
static GwyUserGrainValue*
make_test_grain_value(GwyFitParam **funcparams)
{
    GwyFitParam *params[] = {
        gwy_fit_param_new_set("a", 0, 1, "1.0"),
        gwy_fit_param_new_set("b", -1, 1, "(yxmax-yxmin)/(xmax-xmin)"),
    };
    enum { np = G_N_ELEMENTS(params) };

    GwyUserGrainValue *usergrainvalue = gwy_user_grain_value_new();
    GwyResource *resource = GWY_RESOURCE(usergrainvalue);

    gwy_resource_set_name(resource, "Linear 2");
    g_assert_cmpstr(gwy_resource_get_name(resource), ==, "Linear 2");

    gwy_user_grain_value_set_group(usergrainvalue,
                                "Another group we assume to be nonexistent");
    g_assert_cmpstr(gwy_user_grain_value_get_group(usergrainvalue),
                    ==,
                    "Another group we assume to be nonexistent");

    g_assert(gwy_user_grain_value_set_formula(usergrainvalue, "a+b*x", NULL));
    g_assert_cmpstr(gwy_user_grain_value_get_formula(usergrainvalue), ==, "a+b*x");
    g_assert_cmpuint(gwy_user_grain_value_n_params(usergrainvalue), ==, np);
    GwyFitParam *param;
    g_assert(gwy_user_grain_value_param(usergrainvalue, "a"));
    param = gwy_user_grain_value_param(usergrainvalue, "a");
    gwy_fit_param_assign(param, params[0]);
    g_assert(gwy_user_grain_value_param(usergrainvalue, "b"));
    param = gwy_user_grain_value_param(usergrainvalue, "b");
    gwy_fit_param_assign(param, params[1]);

    funcparams[0] = params[0];
    funcparams[1] = params[1];

    return usergrainvalue;
}

void
test_user_grain_value_save(void)
{
    enum { np = 2 };
    GwyFitParam *params[np];
    GwyUserGrainValue *usergrainvalue = make_test_grain_value(params);
    GwyResource *resource = GWY_RESOURCE(usergrainvalue);

    GError *error = NULL;
    gwy_resource_set_filename(resource, "Linear2");
    g_assert(gwy_resource_save(resource, &error));
    g_assert(!error);
    g_test_queue_destroy((GDestroyNotify)g_unlink, "Linear2");

    gchar *res_filename = NULL;
    g_object_get(resource, "file-name", &res_filename, NULL);
    GFile *gfile = g_file_new_for_path("Linear2");
    GFile *res_gfile = g_file_new_for_path(res_filename);
    g_assert(g_file_equal(gfile, res_gfile));
    g_object_unref(gfile);
    g_object_unref(res_gfile);
    g_free(res_filename);

    GWY_OBJECT_UNREF(usergrainvalue);

    user_grain_value_load_check("Linear2", "Linear 2",
                             "Another group we assume to be nonexistent",
                             "a+b*x", np, params);

    for (guint i = 0; i < np; i++)
        GWY_OBJECT_UNREF(params[i]);
}

void
test_user_grain_value_serialize(void)
{
    enum { np = 2 };
    GwyFitParam *params[np];
    GwyUserGrainValue *usergrainvalue = make_test_grain_value(params);

    serializable_duplicate(GWY_SERIALIZABLE(usergrainvalue), NULL);
    serializable_assign(GWY_SERIALIZABLE(usergrainvalue), NULL);

    GwyUserGrainValue *newusergrainvalue
        = (GwyUserGrainValue*)serialize_and_back(G_OBJECT(usergrainvalue), NULL);
    GwyResource *newresource = GWY_RESOURCE(newusergrainvalue);
    g_assert_cmpstr(gwy_resource_get_name(newresource), ==, "Linear 2");
    g_assert_cmpstr(gwy_user_grain_value_get_formula(usergrainvalue), ==, "a+b*x");
    user_grain_value_param_check(newusergrainvalue, np, params);

    g_object_unref(usergrainvalue);
    g_object_unref(newusergrainvalue);
    for (guint i = 0; i < np; i++)
        GWY_OBJECT_UNREF(params[i]);
}

void
test_user_grain_value_inventory(void)
{
    enum { np = 2 };
    GwyFitParam *params[np];
    GwyUserGrainValue *usergrainvalue = make_test_grain_value(params);
    GwyResource *resource = GWY_RESOURCE(usergrainvalue);

    const GwyInventoryItemType *item_type;

    GwyInventory *usergrainvalues = gwy_user_grain_values();
    g_assert(GWY_IS_INVENTORY(usergrainvalues));
    g_assert_cmpuint(gwy_inventory_size(usergrainvalues), ==, 0);
    item_type = gwy_inventory_get_item_type(usergrainvalues);
    g_assert(item_type);
    g_assert_cmpuint(item_type->type, ==, GWY_TYPE_USER_GRAIN_VALUE);
    g_assert(gwy_inventory_can_make_copies(usergrainvalues));
    g_assert(!gwy_inventory_get_default_name(usergrainvalues));

    g_assert(!gwy_resource_is_managed(resource));
    g_assert(gwy_resource_is_modifiable(resource));
    gwy_inventory_insert(usergrainvalues, usergrainvalue);
    g_object_unref(usergrainvalue);
    g_assert(gwy_resource_is_managed(resource));
    g_assert_cmpuint(gwy_inventory_size(usergrainvalues), ==, 1);

    item_type = gwy_resource_type_get_item_type(GWY_TYPE_USER_GRAIN_VALUE);
    g_assert(item_type);
    g_assert_cmpuint(item_type->type, ==, GWY_TYPE_USER_GRAIN_VALUE);
    usergrainvalue = gwy_user_grain_values_get(NULL);
    g_assert(GWY_IS_USER_GRAIN_VALUE(usergrainvalue));
    resource = GWY_RESOURCE(usergrainvalue);
    g_assert_cmpstr(gwy_resource_get_name(resource), ==, "Linear 2");
    g_assert(gwy_resource_is_managed(resource));
    g_assert(gwy_resource_is_modifiable(resource));

    usergrainvalue = gwy_inventory_get_default(usergrainvalues);
    g_assert(!usergrainvalue);

    gwy_inventory_copy(usergrainvalues, "Linear 2", "Another");
    g_assert_cmpuint(gwy_inventory_size(usergrainvalues), ==, 2);
    usergrainvalue = gwy_inventory_get(usergrainvalues, "Another");
    resource = GWY_RESOURCE(usergrainvalue);
    g_assert_cmpstr(gwy_resource_get_name(resource), ==, "Another");
    gboolean is_modified;
    g_object_get(usergrainvalue, "is-modified", &is_modified, NULL);
    g_assert(gwy_resource_is_managed(resource));
    g_assert(gwy_resource_is_modifiable(resource));
    g_assert(is_modified);

    g_object_ref(usergrainvalue);
    gwy_inventory_delete(usergrainvalues, "Another");
    g_assert(GWY_IS_USER_GRAIN_VALUE(usergrainvalue));
    g_assert(!gwy_resource_is_managed(resource));
    g_assert(gwy_resource_is_modifiable(resource));
    g_object_unref(usergrainvalue);

    for (guint i = 0; i < np; i++)
        GWY_OBJECT_UNREF(params[i]);
}
#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
