/*
 *  $Id$
 *  Copyright (C) 2013 David Nečas (Yeti).
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

/***************************************************************************
 *
 * Field arithmetic
 *
 ***************************************************************************/

void
test_field_compatibility_res(void)
{
    GwyField *field1 = gwy_field_new_sized(2, 3, FALSE);
    GwyField *field2 = gwy_field_new_sized(2, 2, FALSE);
    GwyField *field3 = gwy_field_new_sized(3, 2, FALSE);

    g_assert_cmpuint(gwy_field_is_incompatible(field1, field2,
                                               GWY_FIELD_COMPAT_XRES),
                     ==, 0);
    g_assert_cmpuint(gwy_field_is_incompatible(field1, field2,
                                               GWY_FIELD_COMPAT_YRES),
                     ==, GWY_FIELD_COMPAT_YRES);
    g_assert_cmpuint(gwy_field_is_incompatible(field1, field2,
                                               GWY_FIELD_COMPAT_RES),
                     ==, GWY_FIELD_COMPAT_YRES);
    g_assert_cmpuint(gwy_field_is_incompatible(field2, field1,
                                               GWY_FIELD_COMPAT_XRES),
                     ==, 0);
    g_assert_cmpuint(gwy_field_is_incompatible(field2, field1,
                                               GWY_FIELD_COMPAT_YRES),
                     ==, GWY_FIELD_COMPAT_YRES);
    g_assert_cmpuint(gwy_field_is_incompatible(field2, field1,
                                               GWY_FIELD_COMPAT_RES),
                     ==, GWY_FIELD_COMPAT_YRES);

    g_assert_cmpuint(gwy_field_is_incompatible(field2, field3,
                                               GWY_FIELD_COMPAT_XRES),
                     ==, GWY_FIELD_COMPAT_XRES);
    g_assert_cmpuint(gwy_field_is_incompatible(field2, field3,
                                               GWY_FIELD_COMPAT_YRES),
                     ==, 0);
    g_assert_cmpuint(gwy_field_is_incompatible(field2, field3,
                                               GWY_FIELD_COMPAT_RES),
                     ==, GWY_FIELD_COMPAT_XRES);
    g_assert_cmpuint(gwy_field_is_incompatible(field3, field2,
                                               GWY_FIELD_COMPAT_XRES),
                     ==, GWY_FIELD_COMPAT_XRES);
    g_assert_cmpuint(gwy_field_is_incompatible(field3, field2,
                                               GWY_FIELD_COMPAT_YRES),
                     ==, 0);
    g_assert_cmpuint(gwy_field_is_incompatible(field3, field2,
                                               GWY_FIELD_COMPAT_RES),
                     ==, GWY_FIELD_COMPAT_XRES);

    g_assert_cmpuint(gwy_field_is_incompatible(field1, field3,
                                               GWY_FIELD_COMPAT_XRES),
                     ==, GWY_FIELD_COMPAT_XRES);
    g_assert_cmpuint(gwy_field_is_incompatible(field1, field3,
                                               GWY_FIELD_COMPAT_YRES),
                     ==, GWY_FIELD_COMPAT_YRES);
    g_assert_cmpuint(gwy_field_is_incompatible(field1, field3,
                                               GWY_FIELD_COMPAT_RES),
                     ==, GWY_FIELD_COMPAT_RES);
    g_assert_cmpuint(gwy_field_is_incompatible(field3, field1,
                                               GWY_FIELD_COMPAT_XRES),
                     ==, GWY_FIELD_COMPAT_XRES);
    g_assert_cmpuint(gwy_field_is_incompatible(field3, field1,
                                               GWY_FIELD_COMPAT_YRES),
                     ==, GWY_FIELD_COMPAT_YRES);
    g_assert_cmpuint(gwy_field_is_incompatible(field3, field1,
                                               GWY_FIELD_COMPAT_RES),
                     ==, GWY_FIELD_COMPAT_RES);

    g_assert_cmpuint(gwy_field_is_incompatible(field1, field2,
                                               GWY_FIELD_COMPAT_DX),
                     ==, 0);
    g_assert_cmpuint(gwy_field_is_incompatible(field1, field2,
                                               GWY_FIELD_COMPAT_DY),
                     ==, GWY_FIELD_COMPAT_DY);
    g_assert_cmpuint(gwy_field_is_incompatible(field1, field2,
                                               GWY_FIELD_COMPAT_DXDY),
                     ==, GWY_FIELD_COMPAT_DY);
    g_assert_cmpuint(gwy_field_is_incompatible(field2, field1,
                                               GWY_FIELD_COMPAT_DX),
                     ==, 0);
    g_assert_cmpuint(gwy_field_is_incompatible(field2, field1,
                                               GWY_FIELD_COMPAT_DY),
                     ==, GWY_FIELD_COMPAT_DY);
    g_assert_cmpuint(gwy_field_is_incompatible(field2, field1,
                                               GWY_FIELD_COMPAT_DXDY),
                     ==, GWY_FIELD_COMPAT_DY);

    g_assert_cmpuint(gwy_field_is_incompatible(field2, field3,
                                               GWY_FIELD_COMPAT_DX),
                     ==, GWY_FIELD_COMPAT_DX);
    g_assert_cmpuint(gwy_field_is_incompatible(field2, field3,
                                               GWY_FIELD_COMPAT_DY),
                     ==, 0);
    g_assert_cmpuint(gwy_field_is_incompatible(field2, field3,
                                               GWY_FIELD_COMPAT_DXDY),
                     ==, GWY_FIELD_COMPAT_DX);
    g_assert_cmpuint(gwy_field_is_incompatible(field3, field2,
                                               GWY_FIELD_COMPAT_DX),
                     ==, GWY_FIELD_COMPAT_DX);
    g_assert_cmpuint(gwy_field_is_incompatible(field3, field2,
                                               GWY_FIELD_COMPAT_DY),
                     ==, 0);
    g_assert_cmpuint(gwy_field_is_incompatible(field3, field2,
                                               GWY_FIELD_COMPAT_DXDY),
                     ==, GWY_FIELD_COMPAT_DX);

    g_assert_cmpuint(gwy_field_is_incompatible(field1, field3,
                                               GWY_FIELD_COMPAT_DX),
                     ==, GWY_FIELD_COMPAT_DX);
    g_assert_cmpuint(gwy_field_is_incompatible(field1, field3,
                                               GWY_FIELD_COMPAT_DY),
                     ==, GWY_FIELD_COMPAT_DY);
    g_assert_cmpuint(gwy_field_is_incompatible(field1, field3,
                                               GWY_FIELD_COMPAT_DXDY),
                     ==, GWY_FIELD_COMPAT_DXDY);
    g_assert_cmpuint(gwy_field_is_incompatible(field3, field1,
                                               GWY_FIELD_COMPAT_DX),
                     ==, GWY_FIELD_COMPAT_DX);
    g_assert_cmpuint(gwy_field_is_incompatible(field3, field1,
                                               GWY_FIELD_COMPAT_DY),
                     ==, GWY_FIELD_COMPAT_DY);
    g_assert_cmpuint(gwy_field_is_incompatible(field3, field1,
                                               GWY_FIELD_COMPAT_DXDY),
                     ==, GWY_FIELD_COMPAT_DXDY);

    g_assert_cmpuint(gwy_field_is_incompatible(field1, field2,
                                               GWY_FIELD_COMPAT_REAL),
                     ==, 0);
    g_assert_cmpuint(gwy_field_is_incompatible(field2, field1,
                                               GWY_FIELD_COMPAT_REAL),
                     ==, 0);
    g_assert_cmpuint(gwy_field_is_incompatible(field1, field3,
                                               GWY_FIELD_COMPAT_REAL),
                     ==, 0);
    g_assert_cmpuint(gwy_field_is_incompatible(field3, field1,
                                               GWY_FIELD_COMPAT_REAL),
                     ==, 0);
    g_assert_cmpuint(gwy_field_is_incompatible(field2, field3,
                                               GWY_FIELD_COMPAT_REAL),
                     ==, 0);
    g_assert_cmpuint(gwy_field_is_incompatible(field3, field2,
                                               GWY_FIELD_COMPAT_REAL),
                     ==, 0);

    g_object_unref(field1);
    g_object_unref(field2);
    g_object_unref(field3);
}

void
test_field_compatibility_real(void)
{
    GwyField *field1 = gwy_field_new_sized(2, 2, FALSE);
    GwyField *field2 = gwy_field_new_sized(2, 2, FALSE);
    GwyField *field3 = gwy_field_new_sized(2, 2, FALSE);

    gwy_field_set_yreal(field1, 1.5);
    gwy_field_set_xreal(field3, 1.5);

    g_assert_cmpuint(gwy_field_is_incompatible(field1, field2,
                                               GWY_FIELD_COMPAT_XREAL),
                     ==, 0);
    g_assert_cmpuint(gwy_field_is_incompatible(field1, field2,
                                               GWY_FIELD_COMPAT_YREAL),
                     ==, GWY_FIELD_COMPAT_YREAL);
    g_assert_cmpuint(gwy_field_is_incompatible(field1, field2,
                                               GWY_FIELD_COMPAT_REAL),
                     ==, GWY_FIELD_COMPAT_YREAL);
    g_assert_cmpuint(gwy_field_is_incompatible(field2, field1,
                                               GWY_FIELD_COMPAT_XREAL),
                     ==, 0);
    g_assert_cmpuint(gwy_field_is_incompatible(field2, field1,
                                               GWY_FIELD_COMPAT_YREAL),
                     ==, GWY_FIELD_COMPAT_YREAL);
    g_assert_cmpuint(gwy_field_is_incompatible(field2, field1,
                                               GWY_FIELD_COMPAT_REAL),
                     ==, GWY_FIELD_COMPAT_YREAL);

    g_assert_cmpuint(gwy_field_is_incompatible(field2, field3,
                                               GWY_FIELD_COMPAT_XREAL),
                     ==, GWY_FIELD_COMPAT_XREAL);
    g_assert_cmpuint(gwy_field_is_incompatible(field2, field3,
                                               GWY_FIELD_COMPAT_YREAL),
                     ==, 0);
    g_assert_cmpuint(gwy_field_is_incompatible(field2, field3,
                                               GWY_FIELD_COMPAT_REAL),
                     ==, GWY_FIELD_COMPAT_XREAL);
    g_assert_cmpuint(gwy_field_is_incompatible(field3, field2,
                                               GWY_FIELD_COMPAT_XREAL),
                     ==, GWY_FIELD_COMPAT_XREAL);
    g_assert_cmpuint(gwy_field_is_incompatible(field3, field2,
                                               GWY_FIELD_COMPAT_YREAL),
                     ==, 0);
    g_assert_cmpuint(gwy_field_is_incompatible(field3, field2,
                                               GWY_FIELD_COMPAT_REAL),
                     ==, GWY_FIELD_COMPAT_XREAL);

    g_assert_cmpuint(gwy_field_is_incompatible(field1, field3,
                                               GWY_FIELD_COMPAT_XREAL),
                     ==, GWY_FIELD_COMPAT_XREAL);
    g_assert_cmpuint(gwy_field_is_incompatible(field1, field3,
                                               GWY_FIELD_COMPAT_YREAL),
                     ==, GWY_FIELD_COMPAT_YREAL);
    g_assert_cmpuint(gwy_field_is_incompatible(field1, field3,
                                               GWY_FIELD_COMPAT_REAL),
                     ==, GWY_FIELD_COMPAT_REAL);
    g_assert_cmpuint(gwy_field_is_incompatible(field3, field1,
                                               GWY_FIELD_COMPAT_XREAL),
                     ==, GWY_FIELD_COMPAT_XREAL);
    g_assert_cmpuint(gwy_field_is_incompatible(field3, field1,
                                               GWY_FIELD_COMPAT_YREAL),
                     ==, GWY_FIELD_COMPAT_YREAL);
    g_assert_cmpuint(gwy_field_is_incompatible(field3, field1,
                                               GWY_FIELD_COMPAT_REAL),
                     ==, GWY_FIELD_COMPAT_REAL);

    g_assert_cmpuint(gwy_field_is_incompatible(field1, field2,
                                               GWY_FIELD_COMPAT_DX),
                     ==, 0);
    g_assert_cmpuint(gwy_field_is_incompatible(field1, field2,
                                               GWY_FIELD_COMPAT_DY),
                     ==, GWY_FIELD_COMPAT_DY);
    g_assert_cmpuint(gwy_field_is_incompatible(field1, field2,
                                               GWY_FIELD_COMPAT_DXDY),
                     ==, GWY_FIELD_COMPAT_DY);
    g_assert_cmpuint(gwy_field_is_incompatible(field2, field1,
                                               GWY_FIELD_COMPAT_DX),
                     ==, 0);
    g_assert_cmpuint(gwy_field_is_incompatible(field2, field1,
                                               GWY_FIELD_COMPAT_DY),
                     ==, GWY_FIELD_COMPAT_DY);
    g_assert_cmpuint(gwy_field_is_incompatible(field2, field1,
                                               GWY_FIELD_COMPAT_DXDY),
                     ==, GWY_FIELD_COMPAT_DY);

    g_assert_cmpuint(gwy_field_is_incompatible(field2, field3,
                                               GWY_FIELD_COMPAT_DX),
                     ==, GWY_FIELD_COMPAT_DX);
    g_assert_cmpuint(gwy_field_is_incompatible(field2, field3,
                                               GWY_FIELD_COMPAT_DY),
                     ==, 0);
    g_assert_cmpuint(gwy_field_is_incompatible(field2, field3,
                                               GWY_FIELD_COMPAT_DXDY),
                     ==, GWY_FIELD_COMPAT_DX);
    g_assert_cmpuint(gwy_field_is_incompatible(field3, field2,
                                               GWY_FIELD_COMPAT_DX),
                     ==, GWY_FIELD_COMPAT_DX);
    g_assert_cmpuint(gwy_field_is_incompatible(field3, field2,
                                               GWY_FIELD_COMPAT_DY),
                     ==, 0);
    g_assert_cmpuint(gwy_field_is_incompatible(field3, field2,
                                               GWY_FIELD_COMPAT_DXDY),
                     ==, GWY_FIELD_COMPAT_DX);

    g_assert_cmpuint(gwy_field_is_incompatible(field1, field3,
                                               GWY_FIELD_COMPAT_DX),
                     ==, GWY_FIELD_COMPAT_DX);
    g_assert_cmpuint(gwy_field_is_incompatible(field1, field3,
                                               GWY_FIELD_COMPAT_DY),
                     ==, GWY_FIELD_COMPAT_DY);
    g_assert_cmpuint(gwy_field_is_incompatible(field1, field3,
                                               GWY_FIELD_COMPAT_DXDY),
                     ==, GWY_FIELD_COMPAT_DXDY);
    g_assert_cmpuint(gwy_field_is_incompatible(field3, field1,
                                               GWY_FIELD_COMPAT_DX),
                     ==, GWY_FIELD_COMPAT_DX);
    g_assert_cmpuint(gwy_field_is_incompatible(field3, field1,
                                               GWY_FIELD_COMPAT_DY),
                     ==, GWY_FIELD_COMPAT_DY);
    g_assert_cmpuint(gwy_field_is_incompatible(field3, field1,
                                               GWY_FIELD_COMPAT_DXDY),
                     ==, GWY_FIELD_COMPAT_DXDY);

    g_assert_cmpuint(gwy_field_is_incompatible(field1, field2,
                                               GWY_FIELD_COMPAT_RES),
                     ==, 0);
    g_assert_cmpuint(gwy_field_is_incompatible(field2, field1,
                                               GWY_FIELD_COMPAT_RES),
                     ==, 0);
    g_assert_cmpuint(gwy_field_is_incompatible(field1, field3,
                                               GWY_FIELD_COMPAT_RES),
                     ==, 0);
    g_assert_cmpuint(gwy_field_is_incompatible(field3, field1,
                                               GWY_FIELD_COMPAT_RES),
                     ==, 0);
    g_assert_cmpuint(gwy_field_is_incompatible(field2, field3,
                                               GWY_FIELD_COMPAT_RES),
                     ==, 0);
    g_assert_cmpuint(gwy_field_is_incompatible(field3, field2,
                                               GWY_FIELD_COMPAT_RES),
                     ==, 0);

    g_object_unref(field1);
    g_object_unref(field2);
    g_object_unref(field3);
}

void
test_field_compatibility_units(void)
{
    enum { N = 4 };
    static const GwyFieldCompatFlags incompat[N] = {
        0,
        GWY_FIELD_COMPAT_X,
        GWY_FIELD_COMPAT_Y,
        GWY_FIELD_COMPAT_VALUE,
    };
    GwyField *fields[N];
    for (guint i = 0; i < N; i++)
        fields[i] = gwy_field_new();

    gwy_unit_set_from_string(gwy_field_get_xunit(fields[1]), "m", NULL);
    gwy_unit_set_from_string(gwy_field_get_yunit(fields[2]), "m", NULL);
    gwy_unit_set_from_string(gwy_field_get_zunit(fields[3]), "m", NULL);

    for (guint tocheck = 0; tocheck <= GWY_FIELD_COMPAT_ALL; tocheck++) {
        for (guint i = 0; i < N; i++) {
            for (guint j = 0; j < N; j++) {
                GwyFieldCompatFlags expected = ((i == j) ? 0
                                                : incompat[i] | incompat[j]);
                g_assert_cmpuint(gwy_field_is_incompatible(fields[i], fields[j],
                                                           tocheck),
                                 ==, expected & tocheck);
            }
        }
    }

    for (guint i = 0; i < N; i++)
        g_object_unref(fields[i]);
}

void
test_field_arithmetic_cache(void)
{
    enum { xres = 2, yres = 2 };
    const gdouble data[xres*yres] = {
        -1, 0,
        1, 2,
    };
    gdouble min, max;

    GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
    gwy_assign(field->data, data, xres*yres);
    gwy_field_min_max_full(field, &min, &max);
    g_assert_cmpfloat(min, ==, -1.0);
    g_assert_cmpfloat(max, ==, 2.0);
    g_assert_cmpfloat(gwy_field_mean_full(field), ==, 0.5);
    gwy_assert_floatval(gwy_field_rms_full(field), 0.5*sqrt(5.0), 1e-15);

    gwy_field_add_full(field, 0.0);
    gwy_field_min_max_full(field, &min, &max);
    g_assert_cmpfloat(min, ==, -1.0);
    g_assert_cmpfloat(max, ==, 2.0);
    g_assert_cmpfloat(gwy_field_mean_full(field), ==, 0.5);
    gwy_assert_floatval(gwy_field_rms_full(field), 0.5*sqrt(5.0), 1e-15);

    gwy_field_add_full(field, -1.0);
    gwy_field_min_max_full(field, &min, &max);
    g_assert_cmpfloat(min, ==, -2.0);
    g_assert_cmpfloat(max, ==, 1.0);
    g_assert_cmpfloat(gwy_field_mean_full(field), ==, -0.5);
    gwy_assert_floatval(gwy_field_rms_full(field), 0.5*sqrt(5.0), 1e-15);

    gwy_field_multiply_full(field, 1.0);
    gwy_field_min_max_full(field, &min, &max);
    g_assert_cmpfloat(min, ==, -2.0);
    g_assert_cmpfloat(max, ==, 1.0);
    g_assert_cmpfloat(gwy_field_mean_full(field), ==, -0.5);
    gwy_assert_floatval(gwy_field_rms_full(field), 0.5*sqrt(5.0), 1e-15);

    gwy_field_multiply_full(field, -2.0);
    gwy_field_min_max_full(field, &min, &max);
    g_assert_cmpfloat(min, ==, -2.0);
    g_assert_cmpfloat(max, ==, 4.0);
    g_assert_cmpfloat(gwy_field_mean_full(field), ==, 1.0);
    gwy_assert_floatval(gwy_field_rms_full(field), sqrt(5.0), 1e-15);

    gwy_assign(field->data, data, xres*yres);
    gwy_field_invalidate(field);
    gwy_field_min_max_full(field, &min, &max);
    g_assert_cmpfloat(min, ==, -1.0);
    g_assert_cmpfloat(max, ==, 2.0);
    g_assert_cmpfloat(gwy_field_mean_full(field), ==, 0.5);
    gwy_assert_floatval(gwy_field_rms_full(field), 0.5*sqrt(5.0), 1e-15);

    gwy_field_addmul_full(field, 1.0, 0.0);
    gwy_field_min_max_full(field, &min, &max);
    g_assert_cmpfloat(min, ==, -1.0);
    g_assert_cmpfloat(max, ==, 2.0);
    g_assert_cmpfloat(gwy_field_mean_full(field), ==, 0.5);
    gwy_assert_floatval(gwy_field_rms_full(field), 0.5*sqrt(5.0), 1e-15);

    gwy_field_addmul_full(field, 2.0, 2.0);
    gwy_field_min_max_full(field, &min, &max);
    g_assert_cmpfloat(min, ==, 0.0);
    g_assert_cmpfloat(max, ==, 6.0);
    g_assert_cmpfloat(gwy_field_mean_full(field), ==, 3.0);
    gwy_assert_floatval(gwy_field_rms_full(field), sqrt(5.0), 1e-15);

    gwy_field_addmul_full(field, -0.5, 1.0);
    gwy_field_min_max_full(field, &min, &max);
    g_assert_cmpfloat(min, ==, -2.0);
    g_assert_cmpfloat(max, ==, 1.0);
    g_assert_cmpfloat(gwy_field_mean_full(field), ==, -0.5);
    gwy_assert_floatval(gwy_field_rms_full(field), 0.5*sqrt(5.0), 1e-15);

    g_object_unref(field);
}

void
test_field_arithmetic_fill(void)
{
    enum { max_size = 18, niter = 50 };
    GRand *rng = g_rand_new_with_seed(42);
    const gdouble x = GWY_SQRT3, y = -M_PI;

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size);
        GwyField *field = gwy_field_new_sized(xres, yres, TRUE);

        guint width = g_rand_int_range(rng, 1, xres+1);
        guint height = g_rand_int_range(rng, 1, yres+1);
        guint col = g_rand_int_range(rng, 0, xres-width+1);
        guint row = g_rand_int_range(rng, 0, yres-height+1);
        GwyFieldPart fpart = { col, row, width, height };
        GwyMaskField *mask = random_mask_field(width, height, rng);
        guint n = gwy_mask_field_count(mask, NULL, TRUE);
        gboolean empty = !n, full = (n == width*height);

        gwy_field_fill(field, &fpart, mask, GWY_MASK_INCLUDE, x);
        if (!empty)
            gwy_assert_floatval(gwy_field_mean(field, &fpart,
                                               mask, GWY_MASK_INCLUDE),
                                x, 2e-15);
        if (!full)
            gwy_assert_floatval(gwy_field_mean(field, &fpart,
                                               mask, GWY_MASK_EXCLUDE),
                                0.0, 2e-15);
        for (guint i = 0; i < yres; i++) {
            for (guint j = 0; j < xres; j++) {
                if (i < fpart.row || i >= fpart.row + fpart.height
                    || j < fpart.col || j >= fpart.col + fpart.width)
                    g_assert_cmpfloat(field->data[i*xres + j], ==, 0.0);
            }
        }

        gwy_field_fill(field, &fpart, mask, GWY_MASK_EXCLUDE, x);
        if (!empty)
            gwy_assert_floatval(gwy_field_mean(field, &fpart,
                                               mask, GWY_MASK_INCLUDE),
                                x, 2e-15);
        if (!full)
            gwy_assert_floatval(gwy_field_mean(field, &fpart,
                                               mask, GWY_MASK_EXCLUDE),
                                x, 2e-15);
        for (guint i = 0; i < yres; i++) {
            for (guint j = 0; j < xres; j++) {
                if (i < fpart.row || i >= fpart.row + fpart.height
                    || j < fpart.col || j >= fpart.col + fpart.width)
                    g_assert_cmpfloat(field->data[i*xres + j], ==, 0.0);
            }
        }

        gwy_field_fill_full(field, y);
        gwy_assert_floatval(gwy_field_mean_full(field), y, 1e-15);

        gwy_field_clear(field, &fpart, mask, GWY_MASK_INCLUDE);
        if (!empty)
            gwy_assert_floatval(gwy_field_mean(field, &fpart,
                                               mask, GWY_MASK_INCLUDE),
                                0.0, 3e-15);
        if (!full)
            gwy_assert_floatval(gwy_field_mean(field, &fpart,
                                               mask, GWY_MASK_EXCLUDE),
                                y, 3e-15);
        for (guint i = 0; i < yres; i++) {
            for (guint j = 0; j < xres; j++) {
                if (i < fpart.row || i >= fpart.row + fpart.height
                    || j < fpart.col || j >= fpart.col + fpart.width)
                    g_assert_cmpfloat(field->data[i*xres + j], ==, y);
            }
        }

        gwy_field_clear(field, &fpart, mask, GWY_MASK_EXCLUDE);
        if (!empty)
            gwy_assert_floatval(gwy_field_mean(field, &fpart,
                                               mask, GWY_MASK_INCLUDE),
                                0.0, 3e-15);
        if (!full)
            gwy_assert_floatval(gwy_field_mean(field, &fpart,
                                               mask, GWY_MASK_EXCLUDE),
                                0.0, 3e-15);
        for (guint i = 0; i < yres; i++) {
            for (guint j = 0; j < xres; j++) {
                if (i < fpart.row || i >= fpart.row + fpart.height
                    || j < fpart.col || j >= fpart.col + fpart.width)
                    g_assert_cmpfloat(field->data[i*xres + j], ==, y);
            }
        }

        gwy_field_clear_full(field);
        g_assert_cmpfloat(gwy_field_mean_full(field), ==, 0.0);

        g_object_unref(mask);
        g_object_unref(field);
    }
    g_rand_free(rng);
}

static void
field_arithmetic_operation_one(void (*operation)(GwyField *field,
                                                 const GwyFieldPart *fpart,
                                                 const GwyMaskField *mask,
                                                 GwyMasking masking,
                                                 gdouble value),
                               void (*operation_full)(GwyField *field,
                                                      gdouble value))
{
    enum { max_size = 28, niter = 60 };
    GRand *rng = g_rand_new_with_seed(42);

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size);
        GwyField *field = gwy_field_new_sized(xres, yres, TRUE);

        guint width = g_rand_int_range(rng, 1, xres+1);
        guint height = g_rand_int_range(rng, 1, yres+1);
        guint col = g_rand_int_range(rng, 0, xres-width+1);
        guint row = g_rand_int_range(rng, 0, yres-height+1);
        GwyFieldPart fpart = { col, row, width, height };
        GwyMaskField *mask = random_mask_field(width, height, rng);
        GwyMasking masking = (g_rand_boolean(rng)
                              ? GWY_MASK_INCLUDE
                              : GWY_MASK_EXCLUDE);
        guint n = gwy_mask_field_count(mask, NULL, masking == GWY_MASK_INCLUDE);

        if (n == 0 || n == width*height) {
            g_object_unref(mask);
            g_object_unref(field);
            continue;
        }

        gdouble value = g_rand_double(rng);
        field_randomize(field, rng);
        GwyField *original = gwy_field_duplicate(field);
        GwyField *full = gwy_field_duplicate(field);
        GwyField *diff = gwy_field_new_alike(field, FALSE);
        gdouble avg, rms;

        operation(field, &fpart, mask, masking, value);
        operation_full(full, value);

        // Included data must match full.
        gwy_field_copy_full(field, diff);
        gwy_field_add_field(full, NULL, diff, 0, 0, -1.0);

        avg = gwy_field_mean(diff, &fpart, mask, masking);
        rms = gwy_field_rms(diff, &fpart, mask, masking);
        gwy_assert_floatval(avg, 0.0, 1e-14);
        gwy_assert_floatval(rms, 0.0, 1e-14);

        // Excluded data must match original.
        gwy_field_copy_full(field, diff);
        gwy_field_add_field(original, NULL, diff, 0, 0, -1.0);

        masking = GWY_MASK_INCLUDE + GWY_MASK_EXCLUDE - masking;
        avg = gwy_field_mean(diff, &fpart, mask, masking);
        rms = gwy_field_rms(diff, &fpart, mask, masking);
        gwy_assert_floatval(avg, 0.0, 1e-14);
        gwy_assert_floatval(rms, 0.0, 1e-14);

        g_object_unref(diff);
        g_object_unref(full);
        g_object_unref(original);
        g_object_unref(mask);
        g_object_unref(field);
    }
    g_rand_free(rng);
}

void
test_field_arithmetic_masking_fill(void)
{
    field_arithmetic_operation_one(gwy_field_fill, gwy_field_fill_full);
}

void
test_field_arithmetic_masking_add(void)
{
    field_arithmetic_operation_one(gwy_field_add, gwy_field_add_full);
}

void
test_field_arithmetic_masking_multiply(void)
{
    field_arithmetic_operation_one(gwy_field_multiply, gwy_field_multiply_full);
}

void
test_field_arithmetic_clamp(void)
{
    enum { max_size = 70 };
    GRand *rng = g_rand_new_with_seed(42);
    enum { niter = 20 };

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size);
        GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
        field_randomize(field, rng);
        gwy_field_add_full(field, -0.5);

        guint width = g_rand_int_range(rng, 1, xres+1);
        guint height = g_rand_int_range(rng, 1, yres+1);
        guint col = g_rand_int_range(rng, 0, xres-width+1);
        guint row = g_rand_int_range(rng, 0, yres-height+1);
        GwyFieldPart fpart = { col, row, width, height };
        gdouble lo = 2.0*g_rand_double(rng) - 1.0;
        gdouble hi = 2.0*g_rand_double(rng) - 1.0;
        GWY_ORDER(gdouble, lo, hi);

        gwy_field_clamp(field, &fpart, lo, hi);
        gdouble minp, maxp;
        gwy_field_min_max(field, &fpart, NULL, GWY_MASK_IGNORE, &minp, &maxp);
        g_assert_cmpfloat(minp, >=, lo);
        g_assert_cmpfloat(maxp, <=, hi);

        gwy_field_clamp(field, NULL, lo, hi);
        gdouble minf, maxf;
        gwy_field_min_max_full(field, &minf, &maxf);
        g_assert_cmpfloat(minf, >=, lo);
        g_assert_cmpfloat(maxf, <=, hi);
        g_assert_cmpfloat(minf, <=, minp);
        g_assert_cmpfloat(maxf, >=, maxp);

        gwy_field_invalidate(field);
        gdouble minv, maxv;
        gwy_field_min_max_full(field, &minv, &maxv);
        g_assert_cmpfloat(minf, ==, minv);
        g_assert_cmpfloat(maxf, ==, maxv);

        g_object_unref(field);
    }
    g_rand_free(rng);
}

void
test_field_arithmetic_normalize(void)
{
    enum { max_size = 70 };
    GRand *rng = g_rand_new_with_seed(42);
    enum { niter = 10 };

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size);
        GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
        field_randomize(field, rng);
        GwyMaskField *mask = random_mask_field(xres, yres, rng);
        GwyMasking masking = g_rand_boolean(rng) ? GWY_MASK_INCLUDE : GWY_MASK_EXCLUDE;

        gdouble wantmean = 20.0*(g_rand_double(rng) - 0.5);
        gdouble wantrms = -log(g_rand_double(rng));
        gwy_field_normalize(field, NULL, mask, masking,
                            wantmean, wantrms,
                            GWY_NORMALIZE_MEAN | GWY_NORMALIZE_RMS);
        gwy_field_invalidate(field);
        gwy_assert_floatval(gwy_field_mean(field, NULL, mask, masking),
                            wantmean, 1e-13);
        gwy_assert_floatval(gwy_field_rms(field, NULL, mask, masking),
                            wantrms, 1e-12);

        gwy_field_normalize(field, NULL, NULL, GWY_MASK_IGNORE,
                            wantmean, wantrms,
                            GWY_NORMALIZE_MEAN | GWY_NORMALIZE_RMS);
        gwy_field_invalidate(field);
        gwy_assert_floatval(gwy_field_mean_full(field), wantmean, 1e-13);
        gwy_assert_floatval(gwy_field_rms_full(field), wantrms, 1e-12);

        g_object_unref(mask);
        g_object_unref(field);
    }
    g_rand_free(rng);
}

static gdouble
square_func(gdouble x, gpointer user_data)
{
    g_assert_cmpuint(GPOINTER_TO_UINT(user_data), ==, 69);
    return x*x;
}

void
test_field_arithmetic_apply_func(void)
{
    enum { max_size = 70 };
    GRand *rng = g_rand_new_with_seed(42);
    enum { niter = 20 };

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size);
        GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
        field_randomize(field, rng);
        gwy_field_add_full(field, -0.5);
        GwyMaskField *mask = random_mask_field(xres, yres, rng);

        guint width = g_rand_int_range(rng, 1, xres+1);
        guint height = g_rand_int_range(rng, 1, yres+1);
        guint col = g_rand_int_range(rng, 0, xres-width+1);
        guint row = g_rand_int_range(rng, 0, yres-height+1);
        GwyFieldPart fpart = { col, row, width, height };

        GwyField *result = gwy_field_new_alike(field, FALSE);

        // Ignored mask
        gwy_field_copy_full(field, result);
        gwy_field_apply_func(result, &fpart, mask, GWY_MASK_IGNORE,
                             square_func, GUINT_TO_POINTER(69));
        for (guint i = 0; i < yres; i++) {
            for (guint j = 0; j < xres; j++) {
                guint k = i*xres + j;
                if (i >= fpart.row && i < fpart.row + fpart.height
                    && j >= fpart.col && j < fpart.col + fpart.width)
                    g_assert_cmpfloat(result->data[k],
                                      ==,
                                      field->data[k]*field->data[k]);
                else
                    g_assert_cmpfloat(result->data[k], ==, field->data[k]);
            }
        }

        // Included mask
        gwy_field_copy_full(field, result);
        gwy_field_apply_func(result, &fpart, mask, GWY_MASK_INCLUDE,
                             square_func, GUINT_TO_POINTER(69));
        for (guint i = 0; i < yres; i++) {
            for (guint j = 0; j < xres; j++) {
                guint k = i*xres + j;
                if (i >= fpart.row && i < fpart.row + fpart.height
                    && j >= fpart.col && j < fpart.col + fpart.width
                    && gwy_mask_field_get(mask, j, i))
                    g_assert_cmpfloat(result->data[k],
                                      ==,
                                      field->data[k]*field->data[k]);
                else
                    g_assert_cmpfloat(result->data[k], ==, field->data[k]);
            }
        }

        // Excluded mask
        gwy_field_copy_full(field, result);
        gwy_field_apply_func(result, &fpart, mask, GWY_MASK_EXCLUDE,
                             square_func, GUINT_TO_POINTER(69));
        for (guint i = 0; i < yres; i++) {
            for (guint j = 0; j < xres; j++) {
                guint k = i*xres + j;
                if (i >= fpart.row && i < fpart.row + fpart.height
                    && j >= fpart.col && j < fpart.col + fpart.width
                    && !gwy_mask_field_get(mask, j, i))
                    g_assert_cmpfloat(result->data[k],
                                      ==,
                                      field->data[k]*field->data[k]);
                else
                    g_assert_cmpfloat(result->data[k], ==, field->data[k]);
            }
        }

        g_object_unref(result);
        g_object_unref(field);
        g_object_unref(mask);
    }
    g_rand_free(rng);
}

static void
field_arithmetic_one_func(void (*field_func)(GwyField *field,
                                             const GwyFieldPart *fpart,
                                             const GwyMaskField *mask,
                                             GwyMasking masking),
                          gdouble (*scalar_func)(gdouble x))
{
    enum { max_size = 70 };
    GRand *rng = g_rand_new_with_seed(42);
    enum { niter = 20 };

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size);
        GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
        field_randomize(field, rng);
        GwyMaskField *mask = random_mask_field(xres, yres, rng);

        guint width = g_rand_int_range(rng, 1, xres+1);
        guint height = g_rand_int_range(rng, 1, yres+1);
        guint col = g_rand_int_range(rng, 0, xres-width+1);
        guint row = g_rand_int_range(rng, 0, yres-height+1);
        GwyFieldPart fpart = { col, row, width, height };

        GwyField *result = gwy_field_new_alike(field, FALSE);

        // Ignored mask
        gwy_field_copy_full(field, result);
        field_func(result, &fpart, mask, GWY_MASK_IGNORE);
        for (guint i = 0; i < yres; i++) {
            for (guint j = 0; j < xres; j++) {
                guint k = i*xres + j;
                if (i >= fpart.row && i < fpart.row + fpart.height
                    && j >= fpart.col && j < fpart.col + fpart.width)
                    g_assert_cmpfloat(result->data[k],
                                      ==,
                                      scalar_func(field->data[k]));
                else
                    g_assert_cmpfloat(result->data[k], ==, field->data[k]);
            }
        }

        // Included mask
        gwy_field_copy_full(field, result);
        field_func(result, &fpart, mask, GWY_MASK_INCLUDE);
        for (guint i = 0; i < yres; i++) {
            for (guint j = 0; j < xres; j++) {
                guint k = i*xres + j;
                if (i >= fpart.row && i < fpart.row + fpart.height
                    && j >= fpart.col && j < fpart.col + fpart.width
                    && gwy_mask_field_get(mask, j, i))
                    g_assert_cmpfloat(result->data[k],
                                      ==,
                                      scalar_func(field->data[k]));
                else
                    g_assert_cmpfloat(result->data[k], ==, field->data[k]);
            }
        }

        // Excluded mask
        gwy_field_copy_full(field, result);
        field_func(result, &fpart, mask, GWY_MASK_EXCLUDE);
        for (guint i = 0; i < yres; i++) {
            for (guint j = 0; j < xres; j++) {
                guint k = i*xres + j;
                if (i >= fpart.row && i < fpart.row + fpart.height
                    && j >= fpart.col && j < fpart.col + fpart.width
                    && !gwy_mask_field_get(mask, j, i))
                    g_assert_cmpfloat(result->data[k],
                                      ==,
                                      scalar_func(field->data[k]));
                else
                    g_assert_cmpfloat(result->data[k], ==, field->data[k]);
            }
        }

        g_object_unref(result);
        g_object_unref(field);
        g_object_unref(mask);
    }
    g_rand_free(rng);
}

void
test_field_arithmetic_sqrt(void)
{
    field_arithmetic_one_func(&gwy_field_sqrt, &sqrt);
}

static void
sculpt_dumb(GwyField *field, const GwyFieldPart *rpart,
            GwyField *feature, const GwyFieldPart *fpart,
            GwySculpting method)
{
    GwyMaskField *mask = NULL;
    gdouble lim;

    if (method == GWY_SCULPTING_UPWARD) {
        mask = gwy_mask_field_new_from_field(feature, fpart,
                                             G_MAXDOUBLE, 0.0, TRUE);
        gwy_field_min_max(field, rpart, mask, GWY_MASK_INCLUDE, &lim, NULL);
        gwy_field_add(feature, fpart, mask, GWY_MASK_INCLUDE, lim);
        gwy_field_fill(feature, fpart, mask, GWY_MASK_EXCLUDE, -G_MAXDOUBLE);
        gwy_field_max_field(feature, fpart, field, rpart->col, rpart->row);
    }
    else if (method == GWY_SCULPTING_DOWNWARD) {
        mask = gwy_mask_field_new_from_field(feature, fpart,
                                             0.0, -G_MAXDOUBLE, TRUE);
        gwy_field_min_max(field, rpart, mask, GWY_MASK_INCLUDE, NULL, &lim);
        gwy_field_add(feature, fpart, mask, GWY_MASK_INCLUDE, lim);
        gwy_field_fill(feature, fpart, mask, GWY_MASK_EXCLUDE, G_MAXDOUBLE);
        gwy_field_min_field(feature, fpart, field, rpart->col, rpart->row);
    }
    else {
        g_assert_not_reached();
    }

    g_object_unref(mask);
}

// Here we put the feature completely within the destination field.
static void
field_arithmetic_sculpt_one_contained(GwySculpting method, gboolean periodic)
{
    enum { max_size = 30, max_feature = 20 };
    GRand *rng = g_rand_new_with_seed(42);
    gsize niter = 200;

    for (guint iter = 0; iter < niter; iter++) {
        guint width = g_rand_int_range(rng, 1, max_size);
        guint height = g_rand_int_range(rng, 1, max_size);
        GwyField *field = gwy_field_new_sized(width, height, FALSE);
        field_randomize(field, rng);
        GwyField *reference = gwy_field_duplicate(field);
        guint featurewidth = g_rand_int_range(rng,
                                              1, MIN(max_feature, width)+1);
        guint featureheight = g_rand_int_range(rng,
                                               1, MIN(max_feature, height)+1);
        guint featurecol = g_rand_int_range(rng,
                                            0, max_feature+1 - featurewidth);
        guint featurerow = g_rand_int_range(rng,
                                            0, max_feature+1 - featureheight);
        GwyFieldPart fpart = {
            featurecol, featurerow, featurewidth, featureheight
        };
        GwyField *feature = gwy_field_new_sized(max_feature, max_feature,
                                                FALSE);
        field_randomize(feature, rng);
        gwy_field_addmul_full(feature, 2.0, -1.0);
        gint col = g_rand_int_range(rng, 0, width+1 - featurewidth);
        gint row = g_rand_int_range(rng, 0, height+1 - featureheight);
        gwy_field_sculpt(feature, &fpart, field, col, row, method, periodic);

        GwyFieldPart rpart = {
            col, row, featurewidth, featureheight
        };
        sculpt_dumb(reference, &rpart, feature, &fpart, method);
        field_assert_numerically_equal(field, reference, 1e-15);

        g_object_unref(reference);
        g_object_unref(feature);
        g_object_unref(field);
    }

    g_rand_free(rng);
}

void
test_field_arithmetic_sculpt_upward_contained(void)
{
    field_arithmetic_sculpt_one_contained(GWY_SCULPTING_UPWARD, FALSE);
}

void
test_field_arithmetic_sculpt_downward_contained(void)
{
    field_arithmetic_sculpt_one_contained(GWY_SCULPTING_DOWNWARD, FALSE);
}

void
test_field_arithmetic_sculpt_upward_contained_periodic(void)
{
    field_arithmetic_sculpt_one_contained(GWY_SCULPTING_UPWARD, TRUE);
}

void
test_field_arithmetic_sculpt_downward_contained_periodic(void)
{
    field_arithmetic_sculpt_one_contained(GWY_SCULPTING_DOWNWARD, TRUE);
}

static void
field_arithmetic_sculpt_one_periodic(GwySculpting method)
{
    enum { max_size = 30, max_feature = 20 };
    GRand *rng = g_rand_new_with_seed(42);
    gsize niter = 200;

    for (guint iter = 0; iter < niter; iter++) {
        guint width = g_rand_int_range(rng, 1, max_size);
        guint height = g_rand_int_range(rng, 1, max_size);
        GwyField *field = gwy_field_new_sized(width, height, FALSE);
        // Must use a flat dest field to ensure uniform minima.
        gwy_field_fill_full(field, 2.0*g_rand_double(rng) - 1.0);
        GwyField *reference = gwy_field_duplicate(field);
        // XXX: Unfortunately, we don't have a dumb extension method for a
        // feature larger than field.
        guint featurewidth = g_rand_int_range(rng,
                                              1, MIN(max_feature, width)+1);
        guint featureheight = g_rand_int_range(rng,
                                               1, MIN(max_feature, height)+1);
        guint featurecol = g_rand_int_range(rng,
                                            0, max_feature+1 - featurewidth);
        guint featurerow = g_rand_int_range(rng,
                                            0, max_feature+1 - featureheight);
        GwyFieldPart fpart = {
            featurecol, featurerow, featurewidth, featureheight
        };
        GwyField *feature = gwy_field_new_sized(max_feature, max_feature,
                                                FALSE);
        field_randomize(feature, rng);
        gwy_field_addmul_full(feature, 2.0, -1.0);
        gint col = g_rand_int_range(rng, -4*width, 4*width+1);
        gint row = g_rand_int_range(rng, -4*height, 4*height+1);
        gwy_field_sculpt(feature, &fpart, field, col, row, method, TRUE);

        for (gint i = -5; i <= 5; i++) {
            for (gint j = -5; j <= 5; j++) {
                gwy_field_sculpt(feature, &fpart, reference,
                                 col + j*(gint)width, row + i*(gint)height,
                                 method, FALSE);
            }
        }

        field_assert_numerically_equal(field, reference, 1e-15);

        g_object_unref(reference);
        g_object_unref(feature);
        g_object_unref(field);
    }

    g_rand_free(rng);
}

void
test_field_arithmetic_sculpt_upward_periodic(void)
{
    field_arithmetic_sculpt_one_periodic(GWY_SCULPTING_UPWARD);
}

void
test_field_arithmetic_sculpt_downward_periodic(void)
{
    field_arithmetic_sculpt_one_periodic(GWY_SCULPTING_DOWNWARD);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
