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

#ifndef __TESTLIBGWY_H__
#define __TESTLIBGWY_H__ 1

#undef G_DISABLE_ASSERT
#define GWY_MATH_POLLUTE_NAMESPACE 1
#include "config.h"
#include <gwyconfig.h>
#include "libgwy/libgwy.h"

void test_version              (void);
void test_error_list           (void);
void test_memmem               (void);
void test_next_line            (void);
void test_pack                 (void);
void test_math_sort            (void);
void test_math_median          (void);
void test_math_cholesky        (void);
void test_math_linalg          (void);
void test_interpolation        (void);
void test_expr_evaluate        (void);
void test_expr_vector          (void);
void test_expr_garbage         (void);
void test_fit_task_point       (void);
void test_fit_task_vector      (void);
void test_fit_task_vfunc       (void);
void test_fit_task_fixed       (void);
void test_serialize_simple     (void);
void test_serialize_data       (void);
void test_serialize_nested     (void);
void test_serialize_error      (void);
void test_serialize_boxed      (void);
void test_deserialize_simple   (void);
void test_deserialize_data     (void);
void test_deserialize_nested   (void);
void test_deserialize_boxed    (void);
void test_deserialize_garbage  (void);
void test_unit_parse           (void);
void test_unit_arithmetic      (void);
void test_unit_serialize       (void);
void test_unit_garbage         (void);
void test_value_format_simple  (void);
void test_mask_field_copy      (void);
void test_mask_field_logical   (void);
void test_container_data       (void);
void test_container_refcount   (void);
void test_container_serialize  (void);
void test_container_text       (void);
void test_container_boxed      (void);
void test_array_data           (void);
void test_inventory_data       (void);
void test_gradient_load        (void);
void test_gradient_save        (void);
void test_gradient_serialize   (void);
void test_gradient_inventory   (void);
void test_gl_material_load     (void);
void test_gl_material_save     (void);
void test_gl_material_serialize(void);
void test_gl_material_inventory(void);

// Helpers
GObject* serialize_and_back(GObject *object);
void     record_item_change(GObject *object,
                            guint pos,
                            guint64 *counter);

GType gwy_ser_test_get_type(void) G_GNUC_CONST;

typedef struct _GwySerTest      GwySerTest;
typedef struct _GwySerTestClass GwySerTestClass;

#define GWY_TYPE_SER_TEST \
    (gwy_ser_test_get_type())
#define GWY_SER_TEST(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_SER_TEST, GwySerTest))
#define GWY_IS_SER_TEST(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_SER_TEST))
#define GWY_SER_TEST_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_SER_TEST, GwySerTestClass))

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
