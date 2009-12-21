/*
 *  $Id$
 *  Copyright (C) 2009 David Necas (Yeti).
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
#include <locale.h>

#ifdef HAVE_VALGRIND
#include <valgrind/valgrind.h>
#else
#define RUNNING_ON_VALGRIND 0
#endif

/***************************************************************************
 *
 * Main
 *
 ***************************************************************************/

int
main(int argc, char *argv[])
{
    setenv("LC_NUMERIC", "C", TRUE);
    setlocale(LC_NUMERIC, "C");
    if (RUNNING_ON_VALGRIND)
        setenv("G_SLICE", "always-malloc", TRUE);

    g_test_init(&argc, &argv, NULL);
    g_type_init();

    g_test_add_func("/testlibgwy/version", test_version);
    g_test_add_func("/testlibgwy/error-list", test_error_list);
    g_test_add_func("/testlibgwy/memmem", test_memmem);
    g_test_add_func("/testlibgwy/next-line", test_next_line);
    g_test_add_func("/testlibgwy/pack", test_pack);
    g_test_add_func("/testlibgwy/math/sort", test_math_sort);
    g_test_add_func("/testlibgwy/math/median", test_math_median);
    g_test_add_func("/testlibgwy/math/cholesky", test_math_cholesky);
    g_test_add_func("/testlibgwy/math/linalg", test_math_linalg);
    g_test_add_func("/testlibgwy/interpolation", test_interpolation);
    g_test_add_func("/testlibgwy/expr/evaluate", test_expr_evaluate);
    g_test_add_func("/testlibgwy/expr/vector", test_expr_vector);
    g_test_add_func("/testlibgwy/expr/garbage", test_expr_garbage);
    g_test_add_func("/testlibgwy/fit-task/point", test_fit_task_point);
    g_test_add_func("/testlibgwy/fit-task/vector", test_fit_task_vector);
    g_test_add_func("/testlibgwy/fit-task/vfunc", test_fit_task_vfunc);
    g_test_add_func("/testlibgwy/fit-task/fixed", test_fit_task_fixed);
    g_test_add_func("/testlibgwy/serialize/simple", test_serialize_simple);
    g_test_add_func("/testlibgwy/serialize/data", test_serialize_data);
    g_test_add_func("/testlibgwy/serialize/nested", test_serialize_nested);
    g_test_add_func("/testlibgwy/serialize/error", test_serialize_error);
    g_test_add_func("/testlibgwy/serialize/boxed", test_serialize_boxed);
    g_test_add_func("/testlibgwy/deserialize/simple", test_deserialize_simple);
    g_test_add_func("/testlibgwy/deserialize/data", test_deserialize_data);
    g_test_add_func("/testlibgwy/deserialize/nested", test_deserialize_nested);
    g_test_add_func("/testlibgwy/deserialize/boxed", test_deserialize_boxed);
    g_test_add_func("/testlibgwy/deserialize/garbage", test_deserialize_garbage);
    g_test_add_func("/testlibgwy/unit/parse", test_unit_parse);
    g_test_add_func("/testlibgwy/unit/arithmetic", test_unit_arithmetic);
    g_test_add_func("/testlibgwy/unit/serialize", test_unit_serialize);
    g_test_add_func("/testlibgwy/unit/garbage", test_unit_garbage);
    g_test_add_func("/testlibgwy/value-format/simple", test_value_format_simple);
    g_test_add_func("/testlibgwy/mask-field/copy", test_mask_field_copy);
    g_test_add_func("/testlibgwy/mask-field/logical", test_mask_field_logical);
    g_test_add_func("/testlibgwy/container/data", test_container_data);
    g_test_add_func("/testlibgwy/container/refcount", test_container_refcount);
    g_test_add_func("/testlibgwy/container/serialize", test_container_serialize);
    g_test_add_func("/testlibgwy/container/text", test_container_text);
    g_test_add_func("/testlibgwy/container/boxed", test_container_boxed);
    g_test_add_func("/testlibgwy/array/data", test_array_data);
    g_test_add_func("/testlibgwy/inventory/data", test_inventory_data);
    g_test_add_func("/testlibgwy/gradient/load", test_gradient_load);
    g_test_add_func("/testlibgwy/gradient/save", test_gradient_save);
    g_test_add_func("/testlibgwy/gradient/serialize", test_gradient_serialize);
    g_test_add_func("/testlibgwy/gradient/inventory", test_gradient_inventory);
    g_test_add_func("/testlibgwy/gl-material/load", test_gl_material_load);
    g_test_add_func("/testlibgwy/gl-material/save", test_gl_material_save);
    g_test_add_func("/testlibgwy/gl-material/serialize", test_gl_material_serialize);
    g_test_add_func("/testlibgwy/gl-material/inventory", test_gl_material_inventory);

    return g_test_run();
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
