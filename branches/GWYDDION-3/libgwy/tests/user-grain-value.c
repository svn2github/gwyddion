/*
 *  $Id$
 *  Copyright (C) 2011-2012 David Nečas (Yeti).
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
                            gint expected_power_x,
                            gint expected_power_y,
                            gint expected_power_z,
                            GwyGrainValueSameUnits expected_same_units,
                            gboolean expected_is_angle)
{
    GError *error = NULL;
    GwyResource *resource = gwy_resource_load(filename,
                                              GWY_TYPE_USER_GRAIN_VALUE,
                                              TRUE, &error);
    g_assert_no_error(error);
    g_assert(GWY_IS_USER_GRAIN_VALUE(resource));
    GwyUserGrainValue *usergrainvalue = GWY_USER_GRAIN_VALUE(resource);
    g_assert_cmpstr(gwy_resource_get_name(resource), ==, expected_name);
    g_assert_cmpstr(gwy_user_grain_value_get_group(usergrainvalue),
                    ==, expected_group);
    g_assert(!gwy_resource_is_managed(resource));
    g_assert(gwy_resource_is_modifiable(resource));

    resource_check_file(resource, filename);

    g_assert_cmpstr(gwy_user_grain_value_get_formula(usergrainvalue),
                    ==, expected_formula);
    g_assert_cmpint(gwy_user_grain_value_get_power_x(usergrainvalue),
                    ==, expected_power_x);
    g_assert_cmpint(gwy_user_grain_value_get_power_y(usergrainvalue),
                    ==, expected_power_y);
    g_assert_cmpint(gwy_user_grain_value_get_power_z(usergrainvalue),
                    ==, expected_power_z);
    g_assert_cmpuint(gwy_user_grain_value_get_same_units(usergrainvalue),
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
        "formula atan((z_max - z_min)/D_max)\n"
        "ident sloppiness_2\n"
        "power_x 0\n"
        "power_y 0\n"
        "power_z 0\n"
        "is_angle 1\n"
        "same_units 1\n";

    static const gchar usergrainvalue_v3[] =
        "Gwyddion3 resource GwyUserGrainValue\n"
        "name Sloppiness v3\n"
        "group Group that hopefully does not exist\n"
        "formula atan((z_max - z_min)/D_max)\n"
        "ident sloppiness_2\n"
        "power_x 0\n"
        "power_y 0\n"
        "power_z 0\n"
        "is_angle 1\n"
        "same_units 1\n";

    static const gchar usergrainvalue_v3_ugly[] =
        "Gwyddion3 resource GwyUserGrainValue\n"
        " name    Sloppiness³   \t\n"
        "  ident\t \tsloppiness_2               \n"
        "        formula\tatan((z_max - z_min)/D_max)\n"
        "            \tis_angle \t1\n"
        "\t \n"
        "\tsame_units\t1\t           \t\n";

    GError *error = NULL;

    // Version2 resource
    g_assert(g_file_set_contents("Sloppiness", usergrainvalue_v2, -1, &error));
    g_test_queue_destroy((GDestroyNotify)g_unlink, "Sloppiness");
    g_assert_no_error(error);
    user_grain_value_load_check("Sloppiness", "Sloppiness", "User",
                                "atan((z_max - z_min)/D_max)",
                                0, 0, 0,
                                GWY_GRAIN_VALUE_SAME_UNITS_LATERAL, TRUE);

    // Version3 resource
    g_assert(g_file_set_contents("Sloppiness3", usergrainvalue_v3, -1, &error));
    g_test_queue_destroy((GDestroyNotify)g_unlink, "Sloppiness3");
    g_assert_no_error(error);
    user_grain_value_load_check("Sloppiness3", "Sloppiness v3",
                                "Group that hopefully does not exist",
                                "atan((z_max - z_min)/D_max)",
                                0, 0, 0,
                                GWY_GRAIN_VALUE_SAME_UNITS_LATERAL, TRUE);

    // Version3 ugly resource
    g_assert(g_file_set_contents("Ugly-Slop", usergrainvalue_v3_ugly, -1,
                                 &error));
    g_test_queue_destroy((GDestroyNotify)g_unlink, "Ugly-Slop");
    g_assert_no_error(error);
    user_grain_value_load_check("Ugly-Slop", "Sloppiness³", "User",
                                "atan((z_max - z_min)/D_max)",
                                0, 0, 0,
                                GWY_GRAIN_VALUE_SAME_UNITS_LATERAL, TRUE);
}

static GwyUserGrainValue*
make_test_grain_value(void)
{
    GwyUserGrainValue *usergrainvalue = gwy_user_grain_value_new();
    GwyResource *resource = GWY_RESOURCE(usergrainvalue);

    gwy_resource_set_name(resource, "Bloat");
    g_assert_cmpstr(gwy_resource_get_name(resource), ==, "Bloat");
    gwy_user_grain_value_set_group(usergrainvalue, "The Bloat Group");
    g_assert_cmpstr(gwy_user_grain_value_get_group(usergrainvalue),
                    ==, "The Bloat Group");

    gwy_user_grain_value_set_symbol(usergrainvalue, "Blt₃");
    g_assert_cmpstr(gwy_user_grain_value_get_symbol(usergrainvalue),
                    ==, "Blt₃");

    gwy_user_grain_value_set_ident(usergrainvalue, "Blt_3");
    g_assert_cmpstr(gwy_user_grain_value_get_ident(usergrainvalue),
                    ==, "Blt_3");

    g_assert(gwy_user_grain_value_set_formula(usergrainvalue, "V_0/L_b0^3",
                                              NULL));
    g_assert_cmpstr(gwy_user_grain_value_get_formula(usergrainvalue),
                    ==, "V_0/L_b0^3");

    gwy_user_grain_value_set_power_x(usergrainvalue, -1);
    g_assert_cmpint(gwy_user_grain_value_get_power_x(usergrainvalue), ==, -1);

    gwy_user_grain_value_set_power_y(usergrainvalue, 0);
    g_assert_cmpint(gwy_user_grain_value_get_power_y(usergrainvalue), ==, 0);

    gwy_user_grain_value_set_power_z(usergrainvalue, 1);
    g_assert_cmpint(gwy_user_grain_value_get_power_z(usergrainvalue), ==, 1);

    gwy_user_grain_value_set_same_units(usergrainvalue,
                                        GWY_GRAIN_VALUE_SAME_UNITS_LATERAL);
    g_assert_cmpuint(gwy_user_grain_value_get_same_units(usergrainvalue),
                     ==, GWY_GRAIN_VALUE_SAME_UNITS_LATERAL);

    g_assert(!gwy_user_grain_value_get_is_angle(usergrainvalue));

    return usergrainvalue;
}

void
test_user_grain_value_save(void)
{
    GwyUserGrainValue *usergrainvalue = make_test_grain_value();
    GwyResource *resource = GWY_RESOURCE(usergrainvalue);

    GError *error = NULL;
    gwy_resource_set_filename(resource, "Bloat3");
    g_assert(gwy_resource_save(resource, &error));
    g_assert_no_error(error);
    g_test_queue_destroy((GDestroyNotify)g_unlink, "Bloat3");
    resource_check_file(resource, "Bloat3");
    GWY_OBJECT_UNREF(usergrainvalue);
    user_grain_value_load_check("Bloat3", "Bloat", "The Bloat Group",
                                "V_0/L_b0^3",
                                -1, 0, 1,
                                GWY_GRAIN_VALUE_SAME_UNITS_LATERAL, FALSE);
}

void
test_user_grain_value_serialize(void)
{
    GwyUserGrainValue *usergrainvalue = make_test_grain_value();

    serializable_duplicate(GWY_SERIALIZABLE(usergrainvalue), NULL);
    serializable_assign(GWY_SERIALIZABLE(usergrainvalue), NULL);

    GwyUserGrainValue *newusergrainvalue
        = (GwyUserGrainValue*)serialize_and_back(G_OBJECT(usergrainvalue),
                                                 NULL);
    GwyResource *newresource = GWY_RESOURCE(newusergrainvalue);
    g_assert_cmpstr(gwy_resource_get_name(newresource), ==, "Bloat");
    g_assert_cmpstr(gwy_user_grain_value_get_group(usergrainvalue),
                    ==, "The Bloat Group");
    g_assert_cmpstr(gwy_user_grain_value_get_formula(usergrainvalue),
                    ==, "V_0/L_b0^3");
    g_assert_cmpint(gwy_user_grain_value_get_power_x(usergrainvalue), ==, -1);
    g_assert_cmpint(gwy_user_grain_value_get_power_y(usergrainvalue), ==, 0);
    g_assert_cmpint(gwy_user_grain_value_get_power_z(usergrainvalue), ==, 1);
    g_assert_cmpuint(gwy_user_grain_value_get_same_units(usergrainvalue),
                     ==, GWY_GRAIN_VALUE_SAME_UNITS_LATERAL);
    g_assert(!gwy_user_grain_value_get_is_angle(usergrainvalue));

    g_object_unref(usergrainvalue);
    g_object_unref(newusergrainvalue);
}

void
test_user_grain_value_inventory(void)
{
    GwyUserGrainValue *usergrainvalue = make_test_grain_value();
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
    g_assert_cmpstr(gwy_resource_get_name(resource), ==, "Bloat");
    g_assert(gwy_resource_is_managed(resource));
    g_assert(gwy_resource_is_modifiable(resource));

    usergrainvalue = gwy_inventory_get_default(usergrainvalues);
    g_assert(!usergrainvalue);

    gwy_inventory_copy(usergrainvalues, "Bloat", "Another");
    g_assert_cmpuint(gwy_inventory_size(usergrainvalues), ==, 2);
    usergrainvalue = gwy_inventory_get(usergrainvalues, "Another");
    resource = GWY_RESOURCE(usergrainvalue);
    g_assert_cmpstr(gwy_resource_get_name(resource), ==, "Another");
    gboolean modified;
    g_object_get(usergrainvalue, "modified", &modified, NULL);
    g_assert(gwy_resource_is_managed(resource));
    g_assert(gwy_resource_is_modifiable(resource));
    g_assert(modified);

    g_object_ref(usergrainvalue);
    gwy_inventory_delete(usergrainvalues, "Another");
    g_assert(GWY_IS_USER_GRAIN_VALUE(usergrainvalue));
    g_assert(!gwy_resource_is_managed(resource));
    g_assert(gwy_resource_is_modifiable(resource));
    g_object_unref(usergrainvalue);
}

void
test_user_grain_value_error_bad_line(void)
{
    resource_assert_load_error
        ("Gwyddion3 resource GwyUserGrainValue\n"
         "name Broken grain value\n"
         "WTF?!\n"
         "group User\n"
         "formula z_min\n"
         "ident broken_grain_value\n",
         GWY_TYPE_USER_GRAIN_VALUE,
         GWY_RESOURCE_ERROR, GWY_RESOURCE_ERROR_DATA);
}

void
test_user_grain_value_error_no_name(void)
{
    resource_assert_load_error
        ("Gwyddion3 resource GwyUserGrainValue\n"
         "group User\n"
         "formula z_min\n",
         GWY_TYPE_USER_GRAIN_VALUE,
         GWY_RESOURCE_ERROR, GWY_RESOURCE_ERROR_NAME);
}

void
test_user_grain_value_error_bad_name(void)
{
    resource_assert_load_error
        ("Gwyddion3 resource GwyUserGrainValue\n"
         "name \n"
         "group User\n"
         "formula z_min\n",
         GWY_TYPE_USER_GRAIN_VALUE,
         GWY_RESOURCE_ERROR, GWY_RESOURCE_ERROR_NAME);
    resource_assert_load_error
        ("Gwyddion3 resource GwyUserGrainValue\n"
         "name \x8e\n"
         "group User\n"
         "formula z_min\n",
         GWY_TYPE_USER_GRAIN_VALUE,
         GWY_RESOURCE_ERROR, GWY_RESOURCE_ERROR_NAME);
}

void
test_user_grain_value_error_bad_group(void)
{
    resource_assert_load_error
        ("Gwyddion3 resource GwyUserGrainValue\n"
         "name Broken grain value\n"
         "group \n"
         "formula z_min\n",
         GWY_TYPE_USER_GRAIN_VALUE,
         GWY_RESOURCE_ERROR, GWY_RESOURCE_ERROR_DATA);
    resource_assert_load_error
        ("Gwyddion3 resource GwyUserGrainValue\n"
         "name Broken grain value\n"
         "group \x8e\n"
         "formula z_min\n",
         GWY_TYPE_USER_GRAIN_VALUE,
         GWY_RESOURCE_ERROR, GWY_RESOURCE_ERROR_DATA);
}

void
test_user_grain_value_error_bad_formula(void)
{
    resource_assert_load_error
        ("Gwyddion3 resource GwyUserGrainValue\n"
         "name Broken grain value\n"
         "group User\n"
         "formula /z_min\n",
         GWY_TYPE_USER_GRAIN_VALUE,
         GWY_RESOURCE_ERROR, GWY_RESOURCE_ERROR_DATA);
    resource_assert_load_error
        ("Gwyddion3 resource GwyUserGrainValue\n"
         "name Broken grain value\n"
         "group User\n"
         "formula z_min\x8e\n",
         GWY_TYPE_USER_GRAIN_VALUE,
         GWY_RESOURCE_ERROR, GWY_RESOURCE_ERROR_DATA);
    resource_assert_load_error
        ("Gwyddion3 resource GwyUserGrainValue\n"
         "name Broken grain value\n"
         "group User\n"
         "formula nonexistent_builtin_grain_value_foobar\n",
         GWY_TYPE_USER_GRAIN_VALUE,
         GWY_RESOURCE_ERROR, GWY_RESOURCE_ERROR_DATA);
}

void
test_user_grain_value_error_bad_ident(void)
{
    resource_assert_load_error
        ("Gwyddion3 resource GwyUserGrainValue\n"
         "name Broken grain value\n"
         "group User\n"
         "formula z_min\n"
         "ident \x80\n"
         "symbol BrokenX80",
         GWY_TYPE_USER_GRAIN_VALUE,
         GWY_RESOURCE_ERROR, GWY_RESOURCE_ERROR_DATA);
    resource_assert_load_error
        ("Gwyddion3 resource GwyUserGrainValue\n"
         "name Broken grain value\n"
         "group User\n"
         "formula z_min\n"
         "ident \n"
         "symbol BrokenX80",
         GWY_TYPE_USER_GRAIN_VALUE,
         GWY_RESOURCE_ERROR, GWY_RESOURCE_ERROR_DATA);
    resource_assert_load_error
        ("Gwyddion3 resource GwyUserGrainValue\n"
         "name Broken grain value\n"
         "group User\n"
         "formula z_min\n"
         "ident just testing\n"
         "symbol BrokenX80",
         GWY_TYPE_USER_GRAIN_VALUE,
         GWY_RESOURCE_ERROR, GWY_RESOURCE_ERROR_DATA);
    resource_assert_load_error
        ("Gwyddion3 resource GwyUserGrainValue\n"
         "name Broken grain value\n"
         "group User\n"
         "formula z_min\n"
         "symbol BrokenX80",
         GWY_TYPE_USER_GRAIN_VALUE,
         GWY_RESOURCE_ERROR, GWY_RESOURCE_ERROR_DATA);
}

void
test_user_grain_value_error_bad_symbol(void)
{
    resource_assert_load_error
        ("Gwyddion3 resource GwyUserGrainValue\n"
         "name Broken grain value\n"
         "group User\n"
         "formula z_min\n"
         "ident broken_grain_value\n"
         "symbol \x80",
         GWY_TYPE_USER_GRAIN_VALUE,
         GWY_RESOURCE_ERROR, GWY_RESOURCE_ERROR_DATA);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
