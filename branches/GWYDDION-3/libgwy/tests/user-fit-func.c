/*
 *  $Id$
 *  Copyright (C) 2010 David Necas (Yeti).
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
 * User Fitting Function
 *
 ***************************************************************************/

static void
test_user_fit_func_param_check(GwyUserFitFunc *userfitfunc,
                               guint expected_n_params,
                               GwyFitParam **expected_params)
{
    g_assert_cmpuint(gwy_user_fit_func_n_params(userfitfunc),
                     ==, expected_n_params);
    for (guint i = 0; i < expected_n_params; i++) {
        GwyFitParam *expected = expected_params[i];
        const gchar *expected_name = gwy_fit_param_get_name(expected);
        GwyFitParam *param = gwy_user_fit_func_param(userfitfunc,
                                                     expected_name);
        g_assert(param);
        g_assert_cmpstr(gwy_fit_param_get_name(param),
                        ==, gwy_fit_param_get_name(expected));
        g_assert_cmpstr(gwy_fit_param_get_estimate(param),
                        ==, gwy_fit_param_get_estimate(expected));
        g_assert_cmpint(gwy_fit_param_get_power_x(param),
                        ==, gwy_fit_param_get_power_x(expected));
        g_assert_cmpint(gwy_fit_param_get_power_y(param),
                        ==, gwy_fit_param_get_power_y(expected));
    }
}

static void
test_user_fit_func_load_check(const gchar *filename,
                              const gchar *expected_name,
                              const gchar *expected_formula,
                              guint expected_n_params,
                              GwyFitParam **expected_params)
{
    GError *error = NULL;
    GwyResource *resource = gwy_resource_load(filename, GWY_TYPE_USER_FIT_FUNC,
                                              TRUE, &error);
    g_assert(!error);
    g_assert(GWY_IS_USER_FIT_FUNC(resource));
    GwyUserFitFunc *userfitfunc = GWY_USER_FIT_FUNC(resource);
    g_assert_cmpstr(gwy_resource_get_name(resource), ==, expected_name);
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

    g_assert_cmpstr(gwy_user_fit_func_get_formula(userfitfunc),
                    ==, expected_formula);
    test_user_fit_func_param_check(userfitfunc,
                                   expected_n_params, expected_params);
    GWY_OBJECT_UNREF(userfitfunc);
}

void
test_user_fit_func_load(void)
{
    static const gchar userfitfunc_v2[] =
        "Gwyddion resource GwyUserFitFunc\n"
        "formula a+b*x\n"
        "param a\n"
        "power_x 0\n"
        "power_y 1\n"
        "estimate 1.0\n"
        "param b\n"
        "power_x -1\n"
        "power_y 1\n"
        "estimate 0.0\n";

    static const gchar userfitfunc_v3[] =
        "Gwyddion3 resource GwyUserFitFunc\n"
        "name Linear\n"
        "formula a+b*x\n"
        "param a\n"
        "power_x 0\n"
        "power_y 1\n"
        "estimate 1.0\n"
        "param b\n"
        "power_x -1\n"
        "power_y 1\n"
        "estimate 0.0\n";

    static const gchar userfitfunc_v3_ugly[] =
        "Gwyddion3 resource GwyUserFitFunc\n"
        "name      Linear that is up to x¹    \n"
        "  formula    a+b*x\t\t\n"
        "param  a \n"
        "\n"
        "    power_x   0\n"
        "    power_y  1        \n"
        "\testimate 1.0\n"
        "param b\n"
        "    power_x         -1     \n"
        "    power_y 1\n"
        "    estimate 0.0 \t\t \t\n";

    GwyFitParam *params[] = {
        gwy_fit_param_new_set("a", 0, 1, "1.0"),
        gwy_fit_param_new_set("b", -1, 1, "0.0"),
    };
    enum { np = G_N_ELEMENTS(params) };
    GError *error = NULL;

    // Version2 resource
    g_assert(g_file_set_contents("Linear", userfitfunc_v2, -1, &error));
    g_test_queue_destroy((GDestroyNotify)g_unlink, "Linear");
    g_assert(!error);
    test_user_fit_func_load_check("Linear", "Linear", "a+b*x", np, params);

    // Version3 resource
    g_assert(g_file_set_contents("LLL", userfitfunc_v3, -1, &error));
    g_test_queue_destroy((GDestroyNotify)g_unlink, "LLL");
    g_assert(!error);
    test_user_fit_func_load_check("LLL", "Linear", "a+b*x", np, params);

    // Version3 ugly resource
    g_assert(g_file_set_contents("Ugly-Lin", userfitfunc_v3_ugly, -1, &error));
    g_test_queue_destroy((GDestroyNotify)g_unlink, "Ugly-Lin");
    g_assert(!error);
    test_user_fit_func_load_check("Ugly-Lin", "Linear that is up to x¹",
                                  "a+b*x", np, params);

    for (guint i = 0; i < np; i++)
        GWY_OBJECT_UNREF(params[i]);
}

void
test_user_fit_func_save(void)
{
    GwyFitParam *params[] = {
        gwy_fit_param_new_set("a", 0, 1, "1.0"),
        gwy_fit_param_new_set("b", -1, 1, "(yxmax-yxmin)/(xmax-xmin)"),
    };
    enum { np = G_N_ELEMENTS(params) };

    GwyUserFitFunc *userfitfunc = gwy_user_fit_func_new();
    GwyResource *resource = GWY_RESOURCE(userfitfunc);

    gwy_resource_set_name(resource, "Linear 2");
    g_assert_cmpstr(gwy_resource_get_name(resource), ==, "Linear 2");

    g_assert(gwy_user_fit_func_set_formula(userfitfunc, "a+b*x", NULL));
    g_assert_cmpstr(gwy_user_fit_func_get_formula(userfitfunc), ==, "a+b*x");
    g_assert_cmpuint(gwy_user_fit_func_n_params(userfitfunc), ==, np);
    GwyFitParam *param;
    g_assert(gwy_user_fit_func_param(userfitfunc, "a"));
    param = gwy_user_fit_func_param(userfitfunc, "a");
    gwy_fit_param_assign(param, params[0]);
    g_assert(gwy_user_fit_func_param(userfitfunc, "b"));
    param = gwy_user_fit_func_param(userfitfunc, "b");
    gwy_fit_param_assign(param, params[1]);

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

    GWY_OBJECT_UNREF(userfitfunc);

    test_user_fit_func_load_check("Linear2", "Linear 2", "a+b*x", np, params);

    for (guint i = 0; i < np; i++)
        GWY_OBJECT_UNREF(params[i]);
}

void
test_user_fit_func_serialize(void)
{
    GwyFitParam *params[] = {
        gwy_fit_param_new_set("a", 0, 1, "1.0"),
        gwy_fit_param_new_set("b", -1, 1, "(yxmax-yxmin)/(xmax-xmin)"),
    };
    enum { np = G_N_ELEMENTS(params) };

    GwyUserFitFunc *userfitfunc = gwy_user_fit_func_new();
    GwyResource *resource = GWY_RESOURCE(userfitfunc);

    gwy_resource_set_name(resource, "Linear-3");
    g_assert_cmpstr(gwy_resource_get_name(resource), ==, "Linear-3");

    g_assert(gwy_user_fit_func_set_formula(userfitfunc, "a+b*x", NULL));
    g_assert_cmpstr(gwy_user_fit_func_get_formula(userfitfunc), ==, "a+b*x");
    g_assert_cmpuint(gwy_user_fit_func_n_params(userfitfunc), ==, np);
    GwyFitParam *param;
    g_assert(gwy_user_fit_func_param(userfitfunc, "a"));
    param = gwy_user_fit_func_param(userfitfunc, "a");
    gwy_fit_param_assign(param, params[0]);
    g_assert(gwy_user_fit_func_param(userfitfunc, "b"));
    param = gwy_user_fit_func_param(userfitfunc, "b");
    gwy_fit_param_assign(param, params[1]);

    GwyUserFitFunc *newuserfitfunc
        = (GwyUserFitFunc*)serialize_and_back(G_OBJECT(userfitfunc));
    GwyResource *newresource = GWY_RESOURCE(newuserfitfunc);
    g_assert_cmpstr(gwy_resource_get_name(newresource), ==, "Linear-3");
    g_assert_cmpstr(gwy_user_fit_func_get_formula(userfitfunc), ==, "a+b*x");
    test_user_fit_func_param_check(newuserfitfunc, np, params);

    g_object_unref(userfitfunc);
    g_object_unref(newuserfitfunc);
    for (guint i = 0; i < np; i++)
        GWY_OBJECT_UNREF(params[i]);
}

#if 0
void
test_user_fit_func_inventory(void)
{
    static const GwyUserFitFuncPoint userfitfunc_point_red = { 0.5, { 1, 0, 0, 1 } };
    const GwyInventoryItemType *item_type;
    GwyUserFitFunc *userfitfunc;
    GwyResource *resource;

    GwyInventory *userfitfuncs = gwy_user_fit_funcs();
    g_assert(GWY_IS_INVENTORY(userfitfuncs));
    item_type = gwy_inventory_get_item_type(userfitfuncs);
    g_assert(item_type);
    g_assert_cmpuint(item_type->type, ==, GWY_TYPE_USER_FIT_FUNC);
    g_assert(gwy_inventory_can_make_copies(userfitfuncs));
    g_assert_cmpstr(gwy_inventory_get_default_name(userfitfuncs),
                    ==, GWY_USER_FIT_FUNC_DEFAULT);

    item_type = gwy_resource_type_get_item_type(GWY_TYPE_USER_FIT_FUNC);
    g_assert(item_type);
    g_assert_cmpuint(item_type->type, ==, GWY_TYPE_USER_FIT_FUNC);
    userfitfunc = gwy_user_fit_funcs_get(NULL);
    g_assert(GWY_IS_USER_FIT_FUNC(userfitfunc));
    resource = GWY_RESOURCE(userfitfunc);
    g_assert_cmpstr(gwy_resource_get_name(resource), ==, GWY_USER_FIT_FUNC_DEFAULT);
    g_assert(gwy_resource_is_managed(resource));
    g_assert(!gwy_resource_is_modifiable(resource));

    userfitfunc = gwy_inventory_get_default(userfitfuncs);
    g_assert(GWY_IS_USER_FIT_FUNC(userfitfunc));
    resource = GWY_RESOURCE(userfitfunc);
    g_assert_cmpstr(gwy_resource_get_name(resource), ==, GWY_USER_FIT_FUNC_DEFAULT);
    g_assert(gwy_resource_is_managed(resource));
    g_assert(!gwy_resource_is_modifiable(resource));

    g_assert(!gwy_resource_get_is_preferred(resource));
    gwy_resource_set_is_preferred(resource, TRUE);
    g_assert(gwy_resource_get_is_preferred(resource));

    gwy_inventory_copy(userfitfuncs, GWY_USER_FIT_FUNC_DEFAULT, "Another");
    g_assert_cmpuint(gwy_inventory_n_items(userfitfuncs), ==, 2);
    userfitfunc = gwy_inventory_get(userfitfuncs, "Another");
    resource = GWY_RESOURCE(userfitfunc);
    g_assert_cmpstr(gwy_resource_get_name(resource), ==, "Another");
    gboolean is_modified;
    g_object_get(userfitfunc, "is-modified", &is_modified, NULL);
    g_assert(gwy_resource_is_managed(resource));
    g_assert(gwy_resource_is_modifiable(resource));
    g_assert(is_modified);
    gwy_user_fit_func_insert_sorted(userfitfunc, &userfitfunc_point_red);
    g_assert_cmpuint(gwy_user_fit_func_n_points(userfitfunc), ==, 3);

    g_object_ref(userfitfunc);
    gwy_inventory_delete(userfitfuncs, "Another");
    g_assert(GWY_IS_USER_FIT_FUNC(userfitfunc));
    g_assert(!gwy_resource_is_managed(resource));
    g_assert(gwy_resource_is_modifiable(resource));
    g_object_unref(userfitfunc);
}
#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
