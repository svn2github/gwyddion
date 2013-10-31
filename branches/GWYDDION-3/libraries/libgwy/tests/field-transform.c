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
 * Field transformations
 *
 ***************************************************************************/

// Must match the indices, i == 0, h == 1, etc.  This is verified below.
#define i GWY_PLANE_IDENTITY
#define h GWY_PLANE_MIRROR_HORIZONTALLY
#define v GWY_PLANE_MIRROR_VERTICALLY
#define d GWY_PLANE_MIRROR_DIAGONALLY
#define a GWY_PLANE_MIRROR_ANTIDIAGONALLY
#define c GWY_PLANE_MIRROR_BOTH
#define r GWY_PLANE_ROTATE_CLOCKWISE
#define l GWY_PLANE_ROTATE_COUNTERCLOCKWISE

// Order of indexing is the order of transformations.  It matters!
const guint plane_congruence_group[8][8] = {
    { i, h, v, d, a, c, r, l },
    { h, i, c, l, r, v, a, d },
    { v, c, i, r, l, h, d, a },
    { d, r, l, i, c, a, h, v },
    { a, l, r, c, i, d, v, h },
    { c, v, h, a, d, i, l, r },
    { r, d, a, v, h, l, c, i },
    { l, a, d, h, v, r, i, c },
};

#undef i
#undef i
#undef v
#undef d
#undef a
#undef c
#undef r
#undef l

void
check_congruence_group_sanity(void)
{
    // Identity is identity.
    for (guint i = 0; i < 8; i++) {
        g_assert_cmpuint(plane_congruence_group[i][0], ==, i);
        g_assert_cmpuint(plane_congruence_group[0][i], ==, i);
    }

    // Each result is once in each row and each column.
    guint rfound[8], cfound[8];
    gwy_clear(rfound, 8);
    gwy_clear(cfound, 8);

    for (guint i = 0; i < 8; i++) {
        for (guint j = 0; j < 8; j++) {
            g_assert_cmpuint(plane_congruence_group[i][j], <, 8);
            rfound[i] |= (1 << plane_congruence_group[i][j]);
            cfound[j] |= (1 << plane_congruence_group[i][j]);
        }
    }
    for (guint j = 0; j < 8; j++) {
        g_assert_cmpuint(rfound[j], ==, 0xff);
        g_assert_cmpuint(cfound[j], ==, 0xff);
    }

    // Associativity.
    for (guint i = 0; i < 8; i++) {
        for (guint j = 0; j < 8; j++) {
            for (guint k = 0; k < 8; k++) {
                guint ij = plane_congruence_group[i][j];
                guint ij_k = plane_congruence_group[ij][k];
                guint jk = plane_congruence_group[j][k];
                guint i_jk = plane_congruence_group[i][jk];
                g_assert_cmpuint(ij_k, ==, i_jk);
            }
        }
    }
}

static void
field_congr_inplace_one(const gdouble *orig,
                        const gdouble *reference,
                        guint xres, guint yres,
                        GwyPlaneCongruenceType transformation)
{
    GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
    gwy_field_set_xreal(field, xres);
    gwy_field_set_yreal(field, yres);
    gwy_assign(field->data, orig, xres*yres);
    gwy_field_transform_congruent(field, transformation);
    if (gwy_plane_congruence_is_transposition(transformation)) {
        g_assert_cmpuint(field->xres, ==, yres);
        g_assert_cmpuint(field->yres, ==, xres);
        g_assert_cmpfloat(field->xreal, ==, yres);
        g_assert_cmpfloat(field->yreal, ==, xres);
    }
    else {
        g_assert_cmpuint(field->xres, ==, xres);
        g_assert_cmpuint(field->yres, ==, yres);
        g_assert_cmpfloat(field->xreal, ==, xres);
        g_assert_cmpfloat(field->yreal, ==, yres);
    }
    for (guint i = 0; i < xres*yres; i++) {
        g_assert_cmpfloat(field->data[i], ==, reference[i]);
    }
    g_object_unref(field);
}

static void
field_congr_new_one(const gdouble *orig,
                    const gdouble *reference,
                    guint xres, guint yres,
                    GwyPlaneCongruenceType transformation)
{
    GwyField *source = gwy_field_new_sized(xres, yres, FALSE);
    gwy_field_set_xreal(source, xres);
    gwy_field_set_yreal(source, yres);
    gwy_assign(source->data, orig, xres*yres);
    GwyField *field = gwy_field_new_congruent(source, NULL, transformation);
    if (gwy_plane_congruence_is_transposition(transformation)) {
        g_assert_cmpuint(field->xres, ==, yres);
        g_assert_cmpuint(field->yres, ==, xres);
        g_assert_cmpfloat(field->xreal, ==, yres);
        g_assert_cmpfloat(field->yreal, ==, xres);
    }
    else {
        g_assert_cmpuint(field->xres, ==, xres);
        g_assert_cmpuint(field->yres, ==, yres);
        g_assert_cmpfloat(field->xreal, ==, xres);
        g_assert_cmpfloat(field->yreal, ==, yres);
    }
    for (guint i = 0; i < xres*yres; i++) {
        g_assert_cmpfloat(field->data[i], ==, reference[i]);
    }
    g_object_unref(field);
    g_object_unref(source);
}

static void
field_congruence_3x2(void (*test)(const gdouble *orig,
                                  const gdouble *reference,
                                  guint xres, guint yres,
                                  GwyPlaneCongruenceType transformation))
{
    enum { xres = 3, yres = 2 };
    const gdouble orig[xres*yres] = {
        1, 2, 3,
        4, 5, 6,
    };
    const gdouble hflip[xres*yres] = {
        3, 2, 1,
        6, 5, 4,
    };
    const gdouble vflip[xres*yres] = {
        4, 5, 6,
        1, 2, 3,
    };
    const gdouble bflip[xres*yres] = {
        6, 5, 4,
        3, 2, 1,
    };
    const gdouble dflip[xres*yres] = {
        1, 4,
        2, 5,
        3, 6,
    };
    const gdouble aflip[xres*yres] = {
        6, 3,
        5, 2,
        4, 1,
    };
    const gdouble cwrot[xres*yres] = {
        4, 1,
        5, 2,
        6, 3,
    };
    const gdouble ccwrot[xres*yres] = {
        3, 6,
        2, 5,
        1, 4,
    };

    test(orig, orig, xres, yres, GWY_PLANE_IDENTITY);
    test(orig, hflip, xres, yres, GWY_PLANE_MIRROR_HORIZONTALLY);
    test(orig, vflip, xres, yres, GWY_PLANE_MIRROR_VERTICALLY);
    test(orig, bflip, xres, yres, GWY_PLANE_MIRROR_BOTH);
    test(orig, dflip, xres, yres, GWY_PLANE_MIRROR_DIAGONALLY);
    test(orig, aflip, xres, yres, GWY_PLANE_MIRROR_ANTIDIAGONALLY);
    test(orig, bflip, xres, yres, GWY_PLANE_ROTATE_UPSIDE_DOWN);
    test(orig, cwrot, xres, yres, GWY_PLANE_ROTATE_CLOCKWISE);
    test(orig, ccwrot, xres, yres, GWY_PLANE_ROTATE_COUNTERCLOCKWISE);
}

void
test_field_congruence_in_place_3x2(void)
{
    field_congruence_3x2(&field_congr_inplace_one);
}

void
test_field_congruence_new_3x2(void)
{
    field_congruence_3x2(&field_congr_new_one);
}

static void
field_congruence_2x3(void (*test)(const gdouble *orig,
                                  const gdouble *reference,
                                  guint xres, guint yres,
                                  GwyPlaneCongruenceType transformation))
{
    enum { xres = 2, yres = 3 };
    const gdouble orig[xres*yres] = {
        1, 2,
        3, 4,
        5, 6,
    };
    const gdouble hflip[xres*yres] = {
        2, 1,
        4, 3,
        6, 5,
    };
    const gdouble vflip[xres*yres] = {
        5, 6,
        3, 4,
        1, 2,
    };
    const gdouble bflip[xres*yres] = {
        6, 5,
        4, 3,
        2, 1,
    };
    const gdouble dflip[xres*yres] = {
        1, 3, 5,
        2, 4, 6,
    };
    const gdouble aflip[xres*yres] = {
        6, 4, 2,
        5, 3, 1,
    };
    const gdouble cwrot[xres*yres] = {
        5, 3, 1,
        6, 4, 2,
    };
    const gdouble ccwrot[xres*yres] = {
        2, 4, 6,
        1, 3, 5,
    };

    test(orig, orig, xres, yres, GWY_PLANE_IDENTITY);
    test(orig, hflip, xres, yres, GWY_PLANE_MIRROR_HORIZONTALLY);
    test(orig, vflip, xres, yres, GWY_PLANE_MIRROR_VERTICALLY);
    test(orig, bflip, xres, yres, GWY_PLANE_MIRROR_BOTH);
    test(orig, dflip, xres, yres, GWY_PLANE_MIRROR_DIAGONALLY);
    test(orig, aflip, xres, yres, GWY_PLANE_MIRROR_ANTIDIAGONALLY);
    test(orig, bflip, xres, yres, GWY_PLANE_ROTATE_UPSIDE_DOWN);
    test(orig, cwrot, xres, yres, GWY_PLANE_ROTATE_CLOCKWISE);
    test(orig, ccwrot, xres, yres, GWY_PLANE_ROTATE_COUNTERCLOCKWISE);
}

void
test_field_congruence_in_place_2x3(void)
{
    field_congruence_2x3(&field_congr_inplace_one);
}

void
test_field_congruence_new_2x3(void)
{
    field_congruence_2x3(&field_congr_new_one);
}

void
test_field_congruence_in_place_group(void)
{
    check_congruence_group_sanity();

    enum { max_size = 17 };
    GRand *rng = g_rand_new_with_seed(42);
    gsize niter = 500;

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size);
        GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
        field_randomize(field, rng);
        GwyField *reference = gwy_field_duplicate(field);

        GwyPlaneCongruenceType trans1 = g_rand_int_range(rng, 0, 8);
        GwyPlaneCongruenceType trans2 = g_rand_int_range(rng, 0, 8);
        GwyPlaneCongruenceType compound = plane_congruence_group[trans1][trans2];
        gwy_field_transform_congruent(field, trans1);
        gwy_field_transform_congruent(field, trans2);
        gwy_field_transform_congruent(reference, compound);
        field_assert_equal(field, reference);

        g_object_unref(reference);
        g_object_unref(field);
    }

    g_rand_free(rng);
}

void
test_field_congruence_new_group(void)
{
    check_congruence_group_sanity();

    enum { max_size = 17 };
    GRand *rng = g_rand_new_with_seed(42);
    gsize niter = 500;

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size);
        GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
        field_randomize(field, rng);

        GwyPlaneCongruenceType trans1 = g_rand_int_range(rng, 0, 8);
        GwyPlaneCongruenceType trans2 = g_rand_int_range(rng, 0, 8);
        GwyPlaneCongruenceType compound = plane_congruence_group[trans1][trans2];
        GwyField *reference = gwy_field_new_congruent(field, NULL, compound);
        GwyField *tmp = gwy_field_new_congruent(field, NULL, trans1);
        GwyField *result = gwy_field_new_congruent(tmp, NULL, trans2);
        field_assert_equal(result, reference);

        g_object_unref(result);
        g_object_unref(tmp);
        g_object_unref(reference);
        g_object_unref(field);
    }

    g_rand_free(rng);
}

void
test_field_congruence_copy(void)
{
    enum { max_size = 17 };
    GRand *rng = g_rand_new_with_seed(42);
    gsize niter = 500;

    for (guint iter = 0; iter < niter; iter++) {
        guint sxres = g_rand_int_range(rng, 1, max_size);
        guint syres = g_rand_int_range(rng, 1, max_size);
        guint dxres = g_rand_int_range(rng, 1, max_size);
        guint dyres = g_rand_int_range(rng, 1, max_size);
        guint width = g_rand_int_range(rng, 1, sxres+1);
        guint height = g_rand_int_range(rng, 1, syres+1);
        guint col = g_rand_int_range(rng, 0, sxres+1-width);
        guint row = g_rand_int_range(rng, 0, syres+1-height);
        guint destcol = g_rand_int_range(rng, 0, dxres);
        guint destrow = g_rand_int_range(rng, 0, dyres);
        GwyPlaneCongruenceType trans = g_rand_int_range(rng, 0, 8);
        GwyFieldPart srcpart = { col, row, width, height };
        GwyField *source = gwy_field_new_sized(sxres, syres, FALSE);
        GwyField *dest = gwy_field_new_sized(dxres, dyres, FALSE);
        field_randomize(source, rng);
        field_randomize(dest, rng);
        GwyField *reference = gwy_field_duplicate(dest);
        gwy_field_copy_congruent(source, &srcpart, dest, destcol, destrow,
                                 trans);
        GwyField *cut = gwy_field_new_part(source, &srcpart, FALSE);
        gwy_field_transform_congruent(cut, trans);
        gwy_field_copy(cut, NULL, reference, destcol, destrow);
        field_assert_equal(dest, reference);

        g_object_unref(cut);
        g_object_unref(reference);
        g_object_unref(dest);
        g_object_unref(source);
    }

    g_rand_free(rng);
}

void
test_field_congruence_invert(void)
{
    enum { max_size = 7 };
    GRand *rng = g_rand_new_with_seed(42);
    gsize niter = 50;

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size);
        GwyField *orig = gwy_field_new_sized(xres, yres, FALSE);
        field_randomize(orig, rng);
        for (GwyPlaneCongruenceType trans = GWY_PLANE_IDENTITY;
             trans <= GWY_PLANE_ROTATE_COUNTERCLOCKWISE;
             trans++) {
            GwyField *transformed = gwy_field_new_congruent(orig, NULL, trans);
            GwyPlaneCongruenceType itrans = gwy_plane_congruence_invert(trans);
            GwyField *result = gwy_field_new_congruent(transformed, NULL,
                                                       itrans);
            field_assert_equal(result, orig);
            g_object_unref(result);
            g_object_unref(transformed);
        }
        g_object_unref(orig);
    }

    g_rand_free(rng);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
