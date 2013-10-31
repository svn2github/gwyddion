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
 * Mask Field transformations
 *
 ***************************************************************************/

static void
mask_field_congr_inplace_one(const gchar *orig_str,
                             const gchar *reference_str,
                             GwyPlaneCongruenceType transformation)
{
    GwyMaskField *field = mask_field_from_string(orig_str);
    gwy_mask_field_transform_congruent(field, transformation);
    GwyMaskField *reference = mask_field_from_string(reference_str);
    mask_field_assert_equal(field, reference);
    g_object_unref(field);
    g_object_unref(reference);
}

static void
mask_field_congr_new_one(const gchar *orig_str,
                         const gchar *reference_str,
                         GwyPlaneCongruenceType transformation)
{
    GwyMaskField *source = mask_field_from_string(orig_str);
    GwyMaskField *field = gwy_mask_field_new_congruent(source, NULL,
                                                       transformation);
    GwyMaskField *reference = mask_field_from_string(reference_str);
    mask_field_assert_equal(field, reference);
    g_object_unref(field);
    g_object_unref(reference);
    g_object_unref(source);
}

void
mask_field_congruence_4x3(void (*test)(const gchar *orig_str,
                                       const gchar *reference_str,
                                       GwyPlaneCongruenceType transformation))
{
    const gchar *orig_str =
        "####\n"
        "#   \n"
        "#   \n";
    const gchar *hflip_str =
        "####\n"
        "   #\n"
        "   #\n";
    const gchar *vflip_str =
        "#   \n"
        "#   \n"
        "####\n";
    const gchar *bflip_str =
        "   #\n"
        "   #\n"
        "####\n";
    const gchar *dflip_str =
        "###\n"
        "#  \n"
        "#  \n"
        "#  \n";
    const gchar *aflip_str =
        "  #\n"
        "  #\n"
        "  #\n"
        "###\n";
    const gchar *cwrot_str =
        "###\n"
        "  #\n"
        "  #\n"
        "  #\n";
    const gchar *ccwrot_str =
        "#  \n"
        "#  \n"
        "#  \n"
        "###\n";

    test(orig_str, orig_str, GWY_PLANE_IDENTITY);
    test(orig_str, hflip_str, GWY_PLANE_MIRROR_HORIZONTALLY);
    test(orig_str, vflip_str, GWY_PLANE_MIRROR_VERTICALLY);
    test(orig_str, bflip_str, GWY_PLANE_MIRROR_BOTH);
    test(orig_str, dflip_str, GWY_PLANE_MIRROR_DIAGONALLY);
    test(orig_str, aflip_str, GWY_PLANE_MIRROR_ANTIDIAGONALLY);
    test(orig_str, cwrot_str, GWY_PLANE_ROTATE_CLOCKWISE);
    test(orig_str, ccwrot_str, GWY_PLANE_ROTATE_COUNTERCLOCKWISE);
}

void
test_mask_field_congruence_in_place_4x3(void)
{
    mask_field_congruence_4x3(&mask_field_congr_inplace_one);
}

void
test_mask_field_congruence_new_4x3(void)
{
    mask_field_congruence_4x3(&mask_field_congr_new_one);
}

static void
mask_field_congruence_3x4(void (*test)(const gchar *orig_str,
                                       const gchar *reference_str,
                                       GwyPlaneCongruenceType transformation))
{
    const gchar *orig_str =
        "###\n"
        "#  \n"
        "#  \n"
        "#  \n";
    const gchar *hflip_str =
        "###\n"
        "  #\n"
        "  #\n"
        "  #\n";
    const gchar *vflip_str =
        "#  \n"
        "#  \n"
        "#  \n"
        "###\n";
    const gchar *bflip_str =
        "  #\n"
        "  #\n"
        "  #\n"
        "###\n";
    const gchar *dflip_str =
        "####\n"
        "#   \n"
        "#   \n";
    const gchar *aflip_str =
        "   #\n"
        "   #\n"
        "####\n";
    const gchar *cwrot_str =
        "####\n"
        "   #\n"
        "   #\n";
    const gchar *ccwrot_str =
        "#   \n"
        "#   \n"
        "####\n";

    test(orig_str, orig_str, GWY_PLANE_IDENTITY);
    test(orig_str, hflip_str, GWY_PLANE_MIRROR_HORIZONTALLY);
    test(orig_str, vflip_str, GWY_PLANE_MIRROR_VERTICALLY);
    test(orig_str, bflip_str, GWY_PLANE_MIRROR_BOTH);
    test(orig_str, dflip_str, GWY_PLANE_MIRROR_DIAGONALLY);
    test(orig_str, aflip_str, GWY_PLANE_MIRROR_ANTIDIAGONALLY);
    test(orig_str, cwrot_str, GWY_PLANE_ROTATE_CLOCKWISE);
    test(orig_str, ccwrot_str, GWY_PLANE_ROTATE_COUNTERCLOCKWISE);
}

void
test_mask_field_congruence_in_place_3x4(void)
{
    mask_field_congruence_3x4(&mask_field_congr_inplace_one);
}

void
test_mask_field_congruence_new_3x4(void)
{
    mask_field_congruence_3x4(&mask_field_congr_new_one);
}

static void
mask_field_congruence_33x4(void (*test)(const gchar *orig_str,
                                        const gchar *reference_str,
                                        GwyPlaneCongruenceType transformation))
{
    const gchar *orig_str =
        "#################################\n"
        "#                                \n"
        "#                                \n"
        "# # # # # # # # # # # # # # # # #\n";
    const gchar *hflip_str =
        "#################################\n"
        "                                #\n"
        "                                #\n"
        "# # # # # # # # # # # # # # # # #\n";
    const gchar *vflip_str =
        "# # # # # # # # # # # # # # # # #\n"
        "#                                \n"
        "#                                \n"
        "#################################\n";
    const gchar *bflip_str =
        "# # # # # # # # # # # # # # # # #\n"
        "                                #\n"
        "                                #\n"
        "#################################\n";
    const gchar *dflip_str =
        "####\n"
        "#   \n"
        "#  #\n"
        "#   \n"
        "#  #\n"
        "#   \n"
        "#  #\n"
        "#   \n"
        "#  #\n"
        "#   \n"
        "#  #\n"
        "#   \n"
        "#  #\n"
        "#   \n"
        "#  #\n"
        "#   \n"
        "#  #\n"
        "#   \n"
        "#  #\n"
        "#   \n"
        "#  #\n"
        "#   \n"
        "#  #\n"
        "#   \n"
        "#  #\n"
        "#   \n"
        "#  #\n"
        "#   \n"
        "#  #\n"
        "#   \n"
        "#  #\n"
        "#   \n"
        "#  #\n";
    const gchar *aflip_str =
        "#  #\n"
        "   #\n"
        "#  #\n"
        "   #\n"
        "#  #\n"
        "   #\n"
        "#  #\n"
        "   #\n"
        "#  #\n"
        "   #\n"
        "#  #\n"
        "   #\n"
        "#  #\n"
        "   #\n"
        "#  #\n"
        "   #\n"
        "#  #\n"
        "   #\n"
        "#  #\n"
        "   #\n"
        "#  #\n"
        "   #\n"
        "#  #\n"
        "   #\n"
        "#  #\n"
        "   #\n"
        "#  #\n"
        "   #\n"
        "#  #\n"
        "   #\n"
        "#  #\n"
        "   #\n"
        "####\n";
    const gchar *cwrot_str =
        "####\n"
        "   #\n"
        "#  #\n"
        "   #\n"
        "#  #\n"
        "   #\n"
        "#  #\n"
        "   #\n"
        "#  #\n"
        "   #\n"
        "#  #\n"
        "   #\n"
        "#  #\n"
        "   #\n"
        "#  #\n"
        "   #\n"
        "#  #\n"
        "   #\n"
        "#  #\n"
        "   #\n"
        "#  #\n"
        "   #\n"
        "#  #\n"
        "   #\n"
        "#  #\n"
        "   #\n"
        "#  #\n"
        "   #\n"
        "#  #\n"
        "   #\n"
        "#  #\n"
        "   #\n"
        "#  #\n";
    const gchar *ccwrot_str =
        "#  #\n"
        "#   \n"
        "#  #\n"
        "#   \n"
        "#  #\n"
        "#   \n"
        "#  #\n"
        "#   \n"
        "#  #\n"
        "#   \n"
        "#  #\n"
        "#   \n"
        "#  #\n"
        "#   \n"
        "#  #\n"
        "#   \n"
        "#  #\n"
        "#   \n"
        "#  #\n"
        "#   \n"
        "#  #\n"
        "#   \n"
        "#  #\n"
        "#   \n"
        "#  #\n"
        "#   \n"
        "#  #\n"
        "#   \n"
        "#  #\n"
        "#   \n"
        "#  #\n"
        "#   \n"
        "####\n";

    test(orig_str, orig_str, GWY_PLANE_IDENTITY);
    test(orig_str, hflip_str, GWY_PLANE_MIRROR_HORIZONTALLY);
    test(orig_str, vflip_str, GWY_PLANE_MIRROR_VERTICALLY);
    test(orig_str, bflip_str, GWY_PLANE_MIRROR_BOTH);
    test(orig_str, dflip_str, GWY_PLANE_MIRROR_DIAGONALLY);
    test(orig_str, aflip_str, GWY_PLANE_MIRROR_ANTIDIAGONALLY);
    test(orig_str, cwrot_str, GWY_PLANE_ROTATE_CLOCKWISE);
    test(orig_str, ccwrot_str, GWY_PLANE_ROTATE_COUNTERCLOCKWISE);
}

void
test_mask_field_congruence_in_place_33x4(void)
{
    mask_field_congruence_33x4(&mask_field_congr_inplace_one);
}

void
test_mask_field_congruence_new_33x4(void)
{
    mask_field_congruence_33x4(&mask_field_congr_new_one);
}

void
test_mask_field_congruence_in_place_group(void)
{
    check_congruence_group_sanity();

    enum { max_size = 97 };
    GRand *rng = g_rand_new_with_seed(42);
    guint32 *pool = mask_field_random_pool_new(rng, max_size);
    gsize niter = 500;

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size);
        GwyMaskField *field = gwy_mask_field_new_sized(xres, yres, FALSE);
        mask_field_randomize(field, pool, max_size, rng);
        GwyMaskField *reference = gwy_mask_field_duplicate(field);

        GwyPlaneCongruenceType trans1 = g_rand_int_range(rng, 0, 8);
        GwyPlaneCongruenceType trans2 = g_rand_int_range(rng, 0, 8);
        GwyPlaneCongruenceType compound = plane_congruence_group[trans1][trans2];
        gwy_mask_field_transform_congruent(field, trans1);
        gwy_mask_field_transform_congruent(field, trans2);
        gwy_mask_field_transform_congruent(reference, compound);
        mask_field_assert_equal(field, reference);

        g_object_unref(reference);
        g_object_unref(field);
    }

    mask_field_random_pool_free(pool);
    g_rand_free(rng);
}

void
test_mask_field_congruence_new_group(void)
{
    check_congruence_group_sanity();

    enum { max_size = 97 };
    GRand *rng = g_rand_new_with_seed(42);
    guint32 *pool = mask_field_random_pool_new(rng, max_size);
    gsize niter = 500;

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size);
        GwyMaskField *field = gwy_mask_field_new_sized(xres, yres, FALSE);
        mask_field_randomize(field, pool, max_size, rng);

        GwyPlaneCongruenceType trans1 = g_rand_int_range(rng, 0, 8);
        GwyPlaneCongruenceType trans2 = g_rand_int_range(rng, 0, 8);
        GwyPlaneCongruenceType compound = plane_congruence_group[trans1][trans2];
        GwyMaskField *reference = gwy_mask_field_new_congruent(field, NULL,
                                                               compound);
        GwyMaskField *tmp = gwy_mask_field_new_congruent(field, NULL, trans1);
        GwyMaskField *result = gwy_mask_field_new_congruent(tmp, NULL, trans2);
        mask_field_assert_equal(result, reference);

        g_object_unref(result);
        g_object_unref(tmp);
        g_object_unref(reference);
        g_object_unref(field);
    }

    mask_field_random_pool_free(pool);
    g_rand_free(rng);
}

void
test_mask_field_congruence_new_unaligned(void)
{
    enum { max_size = 97 };
    GRand *rng = g_rand_new_with_seed(42);
    guint32 *pool = mask_field_random_pool_new(rng, max_size);
    gsize niter = 500;

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size);
        GwyMaskField *field = gwy_mask_field_new_sized(xres, yres, FALSE);
        mask_field_randomize(field, pool, max_size, rng);
        guint width = g_rand_int_range(rng, 1, xres+1);
        guint height = g_rand_int_range(rng, 1, yres+1);
        guint col = g_rand_int_range(rng, 0, xres-width+1);
        guint row = g_rand_int_range(rng, 0, yres-height+1);
        GwyFieldPart fpart = { col, row, width, height };
        GwyMaskField *part = gwy_mask_field_new_part(field, &fpart);
        GwyPlaneCongruenceType trans = g_rand_int_range(rng, 0, 8);
        // We assume here that aligned transformations were tested and work.
        GwyMaskField *reference = gwy_mask_field_new_congruent(part, NULL,
                                                               trans);
        GwyMaskField *result = gwy_mask_field_new_congruent(field, &fpart,
                                                            trans);
        mask_field_assert_equal(result, reference);

        g_object_unref(result);
        g_object_unref(reference);
        g_object_unref(part);
        g_object_unref(field);
    }

    mask_field_random_pool_free(pool);
    g_rand_free(rng);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
