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

/***************************************************************************
 *
 * Brick
 *
 ***************************************************************************/

void
brick_randomize(GwyBrick *brick,
                GRand *rng)
{
    gdouble *d = brick->data;
    for (guint n = brick->xres*brick->yres*brick->zres; n; n--, d++)
        *d = g_rand_double(rng);
    gwy_brick_invalidate(brick);
}

void
brick_assert_equal(const GwyBrick *result,
                   const GwyBrick *reference)
{
    g_assert(GWY_IS_BRICK(result));
    g_assert(GWY_IS_BRICK(reference));
    g_assert_cmpuint(result->xres, ==, reference->xres);
    g_assert_cmpuint(result->yres, ==, reference->yres);
    g_assert_cmpuint(result->zres, ==, reference->zres);
    compare_properties(G_OBJECT(result), G_OBJECT(reference));

    guint xres = result->xres, yres = result->yres, zres = result->zres;
    for (guint l = 0; l < zres; l++) {
        const gdouble *result_level
            = result->data + l*xres*yres;
        const gdouble *reference_level
            = reference->data + l*xres*yres;
        for (guint i = 0; i < yres; i++) {
            const gdouble *result_row = result_level + i*xres;
            const gdouble *reference_row = reference_level + i*xres;
            for (guint j = 0; j < xres; j++)
                g_assert_cmpfloat(result_row[j], ==, reference_row[j]);
        }
    }
}

static void
brick_assert_equal_object(GObject *object, GObject *reference)
{
    brick_assert_equal(GWY_BRICK(object), GWY_BRICK(reference));
}

void
brick_assert_numerically_equal(const GwyBrick *result,
                               const GwyBrick *reference,
                               gdouble eps)
{
    g_assert(GWY_IS_BRICK(result));
    g_assert(GWY_IS_BRICK(reference));
    g_assert_cmpuint(result->xres, ==, reference->xres);
    g_assert_cmpuint(result->yres, ==, reference->yres);
    g_assert_cmpuint(result->zres, ==, reference->zres);

    guint xres = result->xres, yres = result->yres, zres = result->zres;
    for (guint l = 0; l < zres; l++) {
        const gdouble *result_level
            = result->data + l*xres*yres;
        const gdouble *reference_level
            = reference->data + l*xres*yres;
        for (guint i = 0; i < yres; i++) {
            const gdouble *result_row = result_level + i*xres;
            const gdouble *reference_row = reference_level + i*xres;
            for (guint j = 0; j < xres; j++) {
                gdouble value = result_row[j], ref = reference_row[j];
                //g_printerr("[%u,%u,%u] %g %g\n", j, i, l, value, ref);
                g_assert_cmpfloat(fabs(value - ref), <=, eps);
            }
        }
    }
}

G_GNUC_UNUSED
static void
print_brick(const gchar *name, const GwyBrick *brick)
{
    g_printerr("=====[ %s ]=====\n", name);
    g_printerr("size %u x %u x %u, real %g x %g x %g, offsets %g x %g x %g\n",
               brick->xres, brick->yres, brick->zres,
               brick->xreal, brick->yreal, brick->zreal,
               brick->xoff, brick->yoff, brick->zoff);
    for (guint l = 0; l < brick->zres; l++) {
        for (guint i = 0; i < brick->yres; i++) {
            g_printerr("[%02u][%02u]", l, i);
            for (guint j = 0; j < brick->xres; j++) {
                g_printerr(" % .4f", gwy_brick_index(brick, j, i, l));
            }
            g_printerr("\n");
        }
    }
}

void
test_brick_get(void)
{
    enum { max_size = 15, niter = 40 };

    GRand *rng = g_rand_new_with_seed(42);

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size);
        guint zres = g_rand_int_range(rng, 1, max_size);
        GwyBrick *brick = gwy_brick_new_sized(xres, yres, zres, FALSE);
        brick_randomize(brick, rng);

        for (guint k = 0; k < zres; k++) {
            for (guint i = 0; i < yres; i++) {
                for (guint j = 0; j < xres; j++) {
                    g_assert_cmpfloat(brick->data[(k*yres + i)*xres + j],
                                      ==,
                                      gwy_brick_get(brick, j, i, k));
                }
            }
        }
        g_object_unref(brick);
    }

    g_rand_free(rng);
}

void
test_brick_set(void)
{
    enum { max_size = 15, niter = 40 };

    GRand *rng = g_rand_new_with_seed(42);

    for (guint iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size);
        guint zres = g_rand_int_range(rng, 1, max_size);
        GwyBrick *brick = gwy_brick_new_sized(xres, yres, zres, FALSE);

        for (guint k = 0; k < zres; k++) {
            for (guint i = 0; i < yres; i++) {
                for (guint j = 0; j < xres; j++)
                    gwy_brick_set(brick, j, i, k, (k*yres + i)*xres + j);
            }
        }

        for (guint k = 0; k < xres*yres*zres; k++) {
            g_assert_cmpfloat(brick->data[k], ==, k);
        }
        g_object_unref(brick);
    }

    g_rand_free(rng);
}

void
test_brick_props(void)
{
    GwyBrick *brick = gwy_brick_new_sized(21, 17, 14, FALSE);
    guint xres, yres, zres;
    gdouble xreal, yreal, zreal, xoff, yoff, zoff;
    g_object_get(brick,
                 "x-res", &xres,
                 "y-res", &yres,
                 "z-res", &zres,
                 "x-real", &xreal,
                 "y-real", &yreal,
                 "z-real", &zreal,
                 "x-offset", &xoff,
                 "y-offset", &yoff,
                 "z-offset", &zoff,
                 NULL);
    g_assert_cmpuint(xres, ==, brick->xres);
    g_assert_cmpuint(yres, ==, brick->yres);
    g_assert_cmpuint(zres, ==, brick->zres);
    g_assert_cmpfloat(xreal, ==, brick->xreal);
    g_assert_cmpfloat(yreal, ==, brick->yreal);
    g_assert_cmpfloat(zreal, ==, brick->zreal);
    g_assert_cmpfloat(xoff, ==, brick->xoff);
    g_assert_cmpfloat(yoff, ==, brick->yoff);
    g_assert_cmpfloat(zoff, ==, brick->zoff);
    xreal = 5.0;
    yreal = 7.0e-14;
    zreal = 4.44e8;
    xoff = -3;
    yoff = 1e-15;
    zoff = -2e7;
    gwy_brick_set_xreal(brick, xreal);
    gwy_brick_set_yreal(brick, yreal);
    gwy_brick_set_zreal(brick, zreal);
    gwy_brick_set_xoffset(brick, xoff);
    gwy_brick_set_yoffset(brick, yoff);
    gwy_brick_set_zoffset(brick, zoff);
    g_assert_cmpfloat(xreal, ==, brick->xreal);
    g_assert_cmpfloat(yreal, ==, brick->yreal);
    g_assert_cmpfloat(zreal, ==, brick->zreal);
    g_assert_cmpfloat(xoff, ==, brick->xoff);
    g_assert_cmpfloat(yoff, ==, brick->yoff);
    g_assert_cmpfloat(zoff, ==, brick->zoff);
    g_object_unref(brick);
}

void
test_brick_data_changed(void)
{
    // Plain emission
    GwyBrick *brick = gwy_brick_new();
    guint counter = 0;
    g_signal_connect_swapped(brick, "data-changed",
                             G_CALLBACK(record_signal), &counter);
    gwy_brick_data_changed(brick, NULL);
    g_assert_cmpuint(counter, ==, 1);
    g_object_unref(brick);

    // Specified part argument
    brick = gwy_brick_new_sized(10, 10, 10, FALSE);
    GwyBrickPart bpart = { 1, 2, 3, 4, 5, 6 };
    g_signal_connect_swapped(brick, "data-changed",
                             G_CALLBACK(brick_part_assert_equal), &bpart);
    gwy_brick_data_changed(brick, &bpart);
    g_object_unref(brick);

    // NULL part argument
    brick = gwy_brick_new_sized(2, 3, 4, FALSE);
    g_signal_connect_swapped(brick, "data-changed",
                             G_CALLBACK(brick_part_assert_equal), NULL);
    gwy_brick_data_changed(brick, NULL);
    g_object_unref(brick);
}

void
test_brick_units(void)
{
    enum { max_size = 30 };
    GRand *rng = g_rand_new_with_seed(42);
    gsize niter = 50;

    for (guint iter = 0; iter < niter; iter++) {
        guint width = g_rand_int_range(rng, 1, max_size);
        guint height = g_rand_int_range(rng, 1, max_size);
        guint depth = g_rand_int_range(rng, 1, max_size);
        GwyBrick *brick = gwy_brick_new_sized(width, height, depth, FALSE);
        g_object_set(brick,
                     "x-real", -log(1.0 - g_rand_double(rng)),
                     "y-real", -log(1.0 - g_rand_double(rng)),
                     "z-real", -log(1.0 - g_rand_double(rng)),
                     NULL);
        GString *prev = g_string_new(NULL), *next = g_string_new(NULL);
        GwyValueFormat *format = gwy_brick_format_xy(brick,
                                                     GWY_VALUE_FORMAT_PLAIN);
        gdouble dx = gwy_brick_dx(brick);
        for (gint i = 0; i <= 10; i++) {
            GWY_SWAP(GString*, prev, next);
            g_string_assign(next,
                            gwy_value_format_print_number(format, (i - 5)*dx));
            if (i)
                g_assert_cmpstr(prev->str, !=, next->str);
        }
        gdouble dy = gwy_brick_dy(brick);
        for (gint i = 0; i <= 10; i++) {
            GWY_SWAP(GString*, prev, next);
            g_string_assign(next,
                            gwy_value_format_print_number(format, (i - 5)*dy));
            if (i)
                g_assert_cmpstr(prev->str, !=, next->str);
        }
        g_object_unref(format);

        format = gwy_brick_format_z(brick, GWY_VALUE_FORMAT_PLAIN);
        gdouble dz = gwy_brick_dz(brick);
        for (gint i = 0; i <= 10; i++) {
            GWY_SWAP(GString*, prev, next);
            g_string_assign(next,
                            gwy_value_format_print_number(format, (i - 5)*dz));
            if (i)
                g_assert_cmpstr(prev->str, !=, next->str);
        }
        g_object_unref(format);
        g_object_unref(brick);
        g_string_free(prev, TRUE);
        g_string_free(next, TRUE);
    }
    g_rand_free(rng);
}

void
test_brick_serialize(void)
{
    enum { max_size = 25 };
    GRand *rng = g_rand_new_with_seed(42);
    gsize niter = g_test_slow() ? 50 : 10;

    for (guint iter = 0; iter < niter; iter++) {
        guint width = g_rand_int_range(rng, 1, max_size);
        guint height = g_rand_int_range(rng, 1, max_size/2);
        guint depth = g_rand_int_range(rng, 1, max_size/4);
        GwyBrick *original = gwy_brick_new_sized(width, height, depth, FALSE);
        brick_randomize(original, rng);
        GwyBrick *copy;

        serializable_duplicate(GWY_SERIALIZABLE(original),
                               brick_assert_equal_object);
        serializable_assign(GWY_SERIALIZABLE(original),
                            brick_assert_equal_object);
        copy = GWY_BRICK(serialize_and_back(G_OBJECT(original),
                                            brick_assert_equal_object));
        g_object_unref(copy);
        g_object_unref(original);
    }
    g_rand_free(rng);
}

void
test_brick_set_size(void)
{
    GwyBrick *brick = gwy_brick_new_sized(13, 11, 8, TRUE);
    guint xres_changed = 0, yres_changed = 0, zres_changed = 0;

    g_signal_connect_swapped(brick, "notify::x-res",
                             G_CALLBACK(record_signal), &xres_changed);
    g_signal_connect_swapped(brick, "notify::y-res",
                             G_CALLBACK(record_signal), &yres_changed);
    g_signal_connect_swapped(brick, "notify::z-res",
                             G_CALLBACK(record_signal), &zres_changed);

    gwy_brick_set_size(brick, 13, 11, 8, TRUE);
    g_assert_cmpuint(brick->xres, ==, 13);
    g_assert_cmpuint(brick->yres, ==, 11);
    g_assert_cmpuint(brick->zres, ==, 8);
    g_assert_cmpuint(xres_changed, ==, 0);
    g_assert_cmpuint(yres_changed, ==, 0);
    g_assert_cmpuint(zres_changed, ==, 0);

    gwy_brick_set_size(brick, 13, 10, 8, TRUE);
    g_assert_cmpuint(brick->xres, ==, 13);
    g_assert_cmpuint(brick->yres, ==, 10);
    g_assert_cmpuint(brick->zres, ==, 8);
    g_assert_cmpuint(xres_changed, ==, 0);
    g_assert_cmpuint(yres_changed, ==, 1);
    g_assert_cmpuint(zres_changed, ==, 0);

    gwy_brick_set_size(brick, 11, 10, 8, TRUE);
    g_assert_cmpuint(brick->xres, ==, 11);
    g_assert_cmpuint(brick->yres, ==, 10);
    g_assert_cmpuint(brick->zres, ==, 8);
    g_assert_cmpuint(xres_changed, ==, 1);
    g_assert_cmpuint(yres_changed, ==, 1);
    g_assert_cmpuint(zres_changed, ==, 0);

    gwy_brick_set_size(brick, 11, 10, 9, TRUE);
    g_assert_cmpuint(brick->xres, ==, 11);
    g_assert_cmpuint(brick->yres, ==, 10);
    g_assert_cmpuint(brick->zres, ==, 9);
    g_assert_cmpuint(xres_changed, ==, 1);
    g_assert_cmpuint(yres_changed, ==, 1);
    g_assert_cmpuint(zres_changed, ==, 1);

    gwy_brick_set_size(brick, 15, 14, 4, TRUE);
    g_assert_cmpuint(brick->xres, ==, 15);
    g_assert_cmpuint(brick->yres, ==, 14);
    g_assert_cmpuint(brick->zres, ==, 4);
    g_assert_cmpuint(xres_changed, ==, 2);
    g_assert_cmpuint(yres_changed, ==, 2);
    g_assert_cmpuint(zres_changed, ==, 2);

    g_object_unref(brick);
}

static void
brick_part_copy_dumb(const GwyBrick *src,
                     guint col,
                     guint row,
                     guint level,
                     guint width,
                     guint height,
                     guint depth,
                     GwyBrick *dest,
                     guint destcol,
                     guint destrow,
                     guint destlevel)
{
    for (guint l = 0; l < depth; l++) {
        if (level + l >= src->zres || destlevel + l >= dest->zres)
            continue;
        for (guint i = 0; i < height; i++) {
            if (row + i >= src->yres || destrow + i >= dest->yres)
                continue;
            for (guint j = 0; j < width; j++) {
                if (col + j >= src->xres || destcol + j >= dest->xres)
                    continue;

                gdouble val = gwy_brick_index(src, col + j, row + i, level + l);
                gwy_brick_index(dest, destcol + j, destrow + i, destlevel + l)
                    = val;
            }
        }
    }
}

void
test_brick_copy(void)
{
    enum { max_size = 19 };
    GRand *rng = g_rand_new();
    g_rand_set_seed(rng, 42);
    gsize niter = g_test_slow() ? 1000 : 200;

    for (gsize iter = 0; iter < niter; iter++) {
        guint sxres = g_rand_int_range(rng, 1, max_size);
        guint syres = g_rand_int_range(rng, 1, max_size/2);
        guint szres = g_rand_int_range(rng, 1, max_size/3);
        guint dxres = g_rand_int_range(rng, 1, max_size);
        guint dyres = g_rand_int_range(rng, 1, max_size/2);
        guint dzres = g_rand_int_range(rng, 1, max_size/3);
        GwyBrick *source = gwy_brick_new_sized(sxres, syres, szres, FALSE);
        GwyBrick *dest = gwy_brick_new_sized(dxres, dyres, dzres, FALSE);
        GwyBrick *reference = gwy_brick_new_sized(dxres, dyres, dzres, FALSE);
        brick_randomize(source, rng);
        brick_randomize(reference, rng);
        gwy_brick_copy(reference, NULL, dest, 0, 0, 0);
        guint width = g_rand_int_range(rng, 0, MAX(sxres, dxres));
        guint height = g_rand_int_range(rng, 0, MAX(syres, dyres));
        guint depth = g_rand_int_range(rng, 0, MAX(szres, dzres));
        guint col = g_rand_int_range(rng, 0, sxres);
        guint row = g_rand_int_range(rng, 0, syres);
        guint level = g_rand_int_range(rng, 0, szres);
        guint destcol = g_rand_int_range(rng, 0, dxres);
        guint destrow = g_rand_int_range(rng, 0, dyres);
        guint destlevel = g_rand_int_range(rng, 0, dzres);
        if (sxres == dxres && syres == dyres
            && g_rand_int_range(rng, 0, 2) == 0) {
            // Check the fast path
            col = destcol = 0;
            row = destrow = 0;
            depth = szres;
        }
        GwyBrickPart bpart = { col, row, level, width, height, depth };
        gwy_brick_copy(source, &bpart, dest, destcol, destrow, destlevel);
        brick_part_copy_dumb(source, col, row, level, width, height, depth,
                             reference, destcol, destrow, destlevel);
        brick_assert_equal(dest, reference);
        g_object_unref(source);
        g_object_unref(dest);
        g_object_unref(reference);
    }

    GwyBrick *brick = gwy_brick_new_sized(17, 4, 12, TRUE);
    brick_randomize(brick, rng);
    GwyBrick *dest = gwy_brick_new_alike(brick, FALSE);
    gwy_brick_copy_full(brick, dest);
    g_assert_cmpint(memcmp(brick->data, dest->data,
                           brick->xres*brick->yres*brick->zres*sizeof(gdouble)),
                    ==, 0);
    g_object_unref(dest);
    g_object_unref(brick);

    g_rand_free(rng);
}

void
test_brick_new_part(void)
{
    enum { max_size = 18 };
    GRand *rng = g_rand_new_with_seed(42);
    gsize niter = g_test_slow() ? 1000 : 200;

    for (gsize iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size/2);
        guint zres = g_rand_int_range(rng, 1, max_size/3);
        GwyBrick *source = gwy_brick_new_sized(xres, yres, zres, FALSE);
        brick_randomize(source, rng);
        guint width = g_rand_int_range(rng, 1, xres+1);
        guint height = g_rand_int_range(rng, 1, yres+1);
        guint depth = g_rand_int_range(rng, 1, zres+1);
        guint col = g_rand_int_range(rng, 0, xres-width+1);
        guint row = g_rand_int_range(rng, 0, yres-height+1);
        guint level = g_rand_int_range(rng, 0, zres-depth+1);
        GwyBrickPart bpart = { col, row, level, width, height, depth };
        GwyBrick *part = gwy_brick_new_part(source, &bpart, TRUE);
        GwyBrick *reference = gwy_brick_new_sized(width, height, depth, FALSE);
        gwy_brick_set_xreal(reference, source->xreal*width/xres);
        gwy_brick_set_yreal(reference, source->yreal*height/yres);
        gwy_brick_set_zreal(reference, source->zreal*depth/zres);
        gwy_brick_set_xoffset(reference, source->xreal*col/xres);
        gwy_brick_set_yoffset(reference, source->yreal*row/yres);
        gwy_brick_set_zoffset(reference, source->zreal*level/zres);
        brick_part_copy_dumb(source, col, row, level, width, height, depth,
                             reference, 0, 0, 0);
        brick_assert_equal(part, reference);
        g_object_unref(source);
        g_object_unref(part);
        g_object_unref(reference);
    }
    g_rand_free(rng);
}

void
test_brick_compatibility_res(void)
{
    GwyBrick *brick1 = gwy_brick_new_sized(2, 3, 4, FALSE);
    GwyBrick *brick2 = gwy_brick_new_sized(2, 2, 4, FALSE);
    GwyBrick *brick3 = gwy_brick_new_sized(3, 2, 4, FALSE);
    GwyBrick *brick4 = gwy_brick_new_sized(3, 2, 1, FALSE);

    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick2,
                                               GWY_BRICK_COMPATIBLE_XRES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick2,
                                               GWY_BRICK_COMPATIBLE_YRES),
                     ==, GWY_BRICK_COMPATIBLE_YRES);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick2,
                                               GWY_BRICK_COMPATIBLE_RES),
                     ==, GWY_BRICK_COMPATIBLE_YRES);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick1,
                                               GWY_BRICK_COMPATIBLE_XRES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick1,
                                               GWY_BRICK_COMPATIBLE_YRES),
                     ==, GWY_BRICK_COMPATIBLE_YRES);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick1,
                                               GWY_BRICK_COMPATIBLE_RES),
                     ==, GWY_BRICK_COMPATIBLE_YRES);

    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick3,
                                               GWY_BRICK_COMPATIBLE_XRES),
                     ==, GWY_BRICK_COMPATIBLE_XRES);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick3,
                                               GWY_BRICK_COMPATIBLE_YRES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick3,
                                               GWY_BRICK_COMPATIBLE_RES),
                     ==, GWY_BRICK_COMPATIBLE_XRES);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick2,
                                               GWY_BRICK_COMPATIBLE_XRES),
                     ==, GWY_BRICK_COMPATIBLE_XRES);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick2,
                                               GWY_BRICK_COMPATIBLE_YRES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick2,
                                               GWY_BRICK_COMPATIBLE_RES),
                     ==, GWY_BRICK_COMPATIBLE_XRES);

    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick3,
                                               GWY_BRICK_COMPATIBLE_XRES),
                     ==, GWY_BRICK_COMPATIBLE_XRES);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick3,
                                               GWY_BRICK_COMPATIBLE_YRES),
                     ==, GWY_BRICK_COMPATIBLE_YRES);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick3,
                                               GWY_BRICK_COMPATIBLE_RES),
                     ==, GWY_BRICK_COMPATIBLE_XRES | GWY_BRICK_COMPATIBLE_YRES);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick1,
                                               GWY_BRICK_COMPATIBLE_XRES),
                     ==, GWY_BRICK_COMPATIBLE_XRES);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick1,
                                               GWY_BRICK_COMPATIBLE_YRES),
                     ==, GWY_BRICK_COMPATIBLE_YRES);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick1,
                                               GWY_BRICK_COMPATIBLE_RES),
                     ==, GWY_BRICK_COMPATIBLE_XRES | GWY_BRICK_COMPATIBLE_YRES);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick3,
                                               GWY_BRICK_COMPATIBLE_ZRES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick1,
                                               GWY_BRICK_COMPATIBLE_ZRES),
                     ==, 0);

    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick4,
                                               GWY_BRICK_COMPATIBLE_ZRES),
                     ==, GWY_BRICK_COMPATIBLE_ZRES);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick4, brick3,
                                               GWY_BRICK_COMPATIBLE_ZRES),
                     ==, GWY_BRICK_COMPATIBLE_ZRES);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick4,
                                               GWY_BRICK_COMPATIBLE_RES),
                     ==, GWY_BRICK_COMPATIBLE_ZRES);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick4, brick3,
                                               GWY_BRICK_COMPATIBLE_RES),
                     ==, GWY_BRICK_COMPATIBLE_ZRES);

    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick2,
                                               GWY_BRICK_COMPATIBLE_DX),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick2,
                                               GWY_BRICK_COMPATIBLE_DY),
                     ==, GWY_BRICK_COMPATIBLE_DY);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick2,
                                               GWY_BRICK_COMPATIBLE_DXDY),
                     ==, GWY_BRICK_COMPATIBLE_DY);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick1,
                                               GWY_BRICK_COMPATIBLE_DX),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick1,
                                               GWY_BRICK_COMPATIBLE_DY),
                     ==, GWY_BRICK_COMPATIBLE_DY);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick1,
                                               GWY_BRICK_COMPATIBLE_DXDY),
                     ==, GWY_BRICK_COMPATIBLE_DY);

    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick3,
                                               GWY_BRICK_COMPATIBLE_DX),
                     ==, GWY_BRICK_COMPATIBLE_DX);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick3,
                                               GWY_BRICK_COMPATIBLE_DY),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick3,
                                               GWY_BRICK_COMPATIBLE_DXDY),
                     ==, GWY_BRICK_COMPATIBLE_DX);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick2,
                                               GWY_BRICK_COMPATIBLE_DX),
                     ==, GWY_BRICK_COMPATIBLE_DX);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick2,
                                               GWY_BRICK_COMPATIBLE_DY),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick2,
                                               GWY_BRICK_COMPATIBLE_DXDY),
                     ==, GWY_BRICK_COMPATIBLE_DX);

    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick3,
                                               GWY_BRICK_COMPATIBLE_DX),
                     ==, GWY_BRICK_COMPATIBLE_DX);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick3,
                                               GWY_BRICK_COMPATIBLE_DY),
                     ==, GWY_BRICK_COMPATIBLE_DY);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick3,
                                               GWY_BRICK_COMPATIBLE_DXDY),
                     ==, GWY_BRICK_COMPATIBLE_DXDY);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick1,
                                               GWY_BRICK_COMPATIBLE_DX),
                     ==, GWY_BRICK_COMPATIBLE_DX);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick1,
                                               GWY_BRICK_COMPATIBLE_DY),
                     ==, GWY_BRICK_COMPATIBLE_DY);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick1,
                                               GWY_BRICK_COMPATIBLE_DXDY),
                     ==, GWY_BRICK_COMPATIBLE_DXDY);

    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick2,
                                               GWY_BRICK_COMPATIBLE_REAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick1,
                                               GWY_BRICK_COMPATIBLE_REAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick3,
                                               GWY_BRICK_COMPATIBLE_REAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick1,
                                               GWY_BRICK_COMPATIBLE_REAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick3,
                                               GWY_BRICK_COMPATIBLE_REAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick2,
                                               GWY_BRICK_COMPATIBLE_REAL),
                     ==, 0);

    g_object_unref(brick1);
    g_object_unref(brick2);
    g_object_unref(brick3);
    g_object_unref(brick4);
}

void
test_brick_compatibility_real(void)
{
    GwyBrick *brick1 = gwy_brick_new_sized(2, 2, 2, FALSE);
    GwyBrick *brick2 = gwy_brick_new_sized(2, 2, 2, FALSE);
    GwyBrick *brick3 = gwy_brick_new_sized(2, 2, 2, FALSE);
    GwyBrick *brick4 = gwy_brick_new_sized(2, 2, 2, FALSE);

    gwy_brick_set_yreal(brick1, 1.5);
    gwy_brick_set_xreal(brick3, 1.5);
    gwy_brick_set_zreal(brick4, 1.5);

    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick2,
                                               GWY_BRICK_COMPATIBLE_XREAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick2,
                                               GWY_BRICK_COMPATIBLE_YREAL),
                     ==, GWY_BRICK_COMPATIBLE_YREAL);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick2,
                                               GWY_BRICK_COMPATIBLE_REAL),
                     ==, GWY_BRICK_COMPATIBLE_YREAL);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick1,
                                               GWY_BRICK_COMPATIBLE_XREAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick1,
                                               GWY_BRICK_COMPATIBLE_YREAL),
                     ==, GWY_BRICK_COMPATIBLE_YREAL);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick1,
                                               GWY_BRICK_COMPATIBLE_REAL),
                     ==, GWY_BRICK_COMPATIBLE_YREAL);

    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick3,
                                               GWY_BRICK_COMPATIBLE_XREAL),
                     ==, GWY_BRICK_COMPATIBLE_XREAL);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick3,
                                               GWY_BRICK_COMPATIBLE_YREAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick3,
                                               GWY_BRICK_COMPATIBLE_REAL),
                     ==, GWY_BRICK_COMPATIBLE_XREAL);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick2,
                                               GWY_BRICK_COMPATIBLE_XREAL),
                     ==, GWY_BRICK_COMPATIBLE_XREAL);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick2,
                                               GWY_BRICK_COMPATIBLE_YREAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick2,
                                               GWY_BRICK_COMPATIBLE_REAL),
                     ==, GWY_BRICK_COMPATIBLE_XREAL);

    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick3,
                                               GWY_BRICK_COMPATIBLE_XREAL),
                     ==, GWY_BRICK_COMPATIBLE_XREAL);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick3,
                                               GWY_BRICK_COMPATIBLE_YREAL),
                     ==, GWY_BRICK_COMPATIBLE_YREAL);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick3,
                                               GWY_BRICK_COMPATIBLE_REAL),
                     ==, GWY_BRICK_COMPATIBLE_XREAL | GWY_BRICK_COMPATIBLE_YREAL);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick1,
                                               GWY_BRICK_COMPATIBLE_XREAL),
                     ==, GWY_BRICK_COMPATIBLE_XREAL);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick1,
                                               GWY_BRICK_COMPATIBLE_YREAL),
                     ==, GWY_BRICK_COMPATIBLE_YREAL);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick1,
                                               GWY_BRICK_COMPATIBLE_REAL),
                     ==, GWY_BRICK_COMPATIBLE_XREAL | GWY_BRICK_COMPATIBLE_YREAL);

    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick2,
                                               GWY_BRICK_COMPATIBLE_DX),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick2,
                                               GWY_BRICK_COMPATIBLE_DY),
                     ==, GWY_BRICK_COMPATIBLE_DY);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick2,
                                               GWY_BRICK_COMPATIBLE_DXDY),
                     ==, GWY_BRICK_COMPATIBLE_DY);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick1,
                                               GWY_BRICK_COMPATIBLE_DX),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick1,
                                               GWY_BRICK_COMPATIBLE_DY),
                     ==, GWY_BRICK_COMPATIBLE_DY);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick1,
                                               GWY_BRICK_COMPATIBLE_DXDY),
                     ==, GWY_BRICK_COMPATIBLE_DY);

    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick3,
                                               GWY_BRICK_COMPATIBLE_DX),
                     ==, GWY_BRICK_COMPATIBLE_DX);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick3,
                                               GWY_BRICK_COMPATIBLE_DY),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick3,
                                               GWY_BRICK_COMPATIBLE_DXDY),
                     ==, GWY_BRICK_COMPATIBLE_DX);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick2,
                                               GWY_BRICK_COMPATIBLE_DX),
                     ==, GWY_BRICK_COMPATIBLE_DX);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick2,
                                               GWY_BRICK_COMPATIBLE_DY),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick2,
                                               GWY_BRICK_COMPATIBLE_DXDY),
                     ==, GWY_BRICK_COMPATIBLE_DX);

    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick3,
                                               GWY_BRICK_COMPATIBLE_DX),
                     ==, GWY_BRICK_COMPATIBLE_DX);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick3,
                                               GWY_BRICK_COMPATIBLE_DY),
                     ==, GWY_BRICK_COMPATIBLE_DY);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick3,
                                               GWY_BRICK_COMPATIBLE_DXDY),
                     ==, GWY_BRICK_COMPATIBLE_DXDY);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick1,
                                               GWY_BRICK_COMPATIBLE_DX),
                     ==, GWY_BRICK_COMPATIBLE_DX);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick1,
                                               GWY_BRICK_COMPATIBLE_DY),
                     ==, GWY_BRICK_COMPATIBLE_DY);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick1,
                                               GWY_BRICK_COMPATIBLE_DXDY),
                     ==, GWY_BRICK_COMPATIBLE_DXDY);

    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick4,
                                               GWY_BRICK_COMPATIBLE_DZ),
                     ==, GWY_BRICK_COMPATIBLE_DZ);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick4,
                                               GWY_BRICK_COMPATIBLE_DXDY),
                     ==, GWY_BRICK_COMPATIBLE_DY);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick4,
                                               GWY_BRICK_COMPATIBLE_DXDYDZ),
                     ==, GWY_BRICK_COMPATIBLE_DY | GWY_BRICK_COMPATIBLE_DZ);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick4, brick1,
                                               GWY_BRICK_COMPATIBLE_DZ),
                     ==, GWY_BRICK_COMPATIBLE_DZ);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick4, brick1,
                                               GWY_BRICK_COMPATIBLE_DXDY),
                     ==, GWY_BRICK_COMPATIBLE_DY);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick4, brick1,
                                               GWY_BRICK_COMPATIBLE_DXDYDZ),
                     ==, GWY_BRICK_COMPATIBLE_DY | GWY_BRICK_COMPATIBLE_DZ);

    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick4,
                                               GWY_BRICK_COMPATIBLE_DZ),
                     ==, GWY_BRICK_COMPATIBLE_DZ);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick4,
                                               GWY_BRICK_COMPATIBLE_DXDY),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick4,
                                               GWY_BRICK_COMPATIBLE_DXDYDZ),
                     ==, GWY_BRICK_COMPATIBLE_DZ);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick4, brick2,
                                               GWY_BRICK_COMPATIBLE_DZ),
                     ==, GWY_BRICK_COMPATIBLE_DZ);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick4, brick2,
                                               GWY_BRICK_COMPATIBLE_DXDY),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick4, brick2,
                                               GWY_BRICK_COMPATIBLE_DXDYDZ),
                     ==, GWY_BRICK_COMPATIBLE_DZ);

    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick2,
                                               GWY_BRICK_COMPATIBLE_RES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick1,
                                               GWY_BRICK_COMPATIBLE_RES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick3,
                                               GWY_BRICK_COMPATIBLE_RES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick1,
                                               GWY_BRICK_COMPATIBLE_RES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick3,
                                               GWY_BRICK_COMPATIBLE_RES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick2,
                                               GWY_BRICK_COMPATIBLE_RES),
                     ==, 0);

    g_object_unref(brick1);
    g_object_unref(brick2);
    g_object_unref(brick3);
    g_object_unref(brick4);
}

void
test_brick_compatibility_units(void)
{
    GwyBrick *brick1 = gwy_brick_new();
    GwyBrick *brick2 = gwy_brick_new();
    GwyBrick *brick3 = gwy_brick_new();
    GwyBrick *brick4 = gwy_brick_new();

    gwy_unit_set_from_string(gwy_brick_get_unit_xy(brick1), "m", NULL);
    gwy_unit_set_from_string(gwy_brick_get_unit_z(brick3), "m", NULL);
    gwy_unit_set_from_string(gwy_brick_get_unit_w(brick4), "m", NULL);

    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick2,
                                               GWY_BRICK_COMPATIBLE_DEPTH),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick1,
                                               GWY_BRICK_COMPATIBLE_DEPTH),
                     ==, 0);

    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick3,
                                               GWY_BRICK_COMPATIBLE_DEPTH),
                     ==, GWY_BRICK_COMPATIBLE_DEPTH);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick2,
                                               GWY_BRICK_COMPATIBLE_DEPTH),
                     ==, GWY_BRICK_COMPATIBLE_DEPTH);

    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick3,
                                               GWY_BRICK_COMPATIBLE_DEPTH),
                     ==, GWY_BRICK_COMPATIBLE_DEPTH);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick1,
                                               GWY_BRICK_COMPATIBLE_DEPTH),
                     ==, GWY_BRICK_COMPATIBLE_DEPTH);

    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick2,
                                               GWY_BRICK_COMPATIBLE_LATERAL),
                     ==, GWY_BRICK_COMPATIBLE_LATERAL);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick1,
                                               GWY_BRICK_COMPATIBLE_LATERAL),
                     ==, GWY_BRICK_COMPATIBLE_LATERAL);

    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick3,
                                               GWY_BRICK_COMPATIBLE_LATERAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick2,
                                               GWY_BRICK_COMPATIBLE_LATERAL),
                     ==, 0);

    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick3,
                                               GWY_BRICK_COMPATIBLE_LATERAL),
                     ==, GWY_BRICK_COMPATIBLE_LATERAL);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick1,
                                               GWY_BRICK_COMPATIBLE_LATERAL),
                     ==, GWY_BRICK_COMPATIBLE_LATERAL);

    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick2,
                                               GWY_BRICK_COMPATIBLE_VALUE),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick1,
                                               GWY_BRICK_COMPATIBLE_VALUE),
                     ==, 0);

    g_assert_cmpuint(gwy_brick_is_incompatible(brick2, brick3,
                                               GWY_BRICK_COMPATIBLE_VALUE),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick2,
                                               GWY_BRICK_COMPATIBLE_VALUE),
                     ==, 0);

    g_assert_cmpuint(gwy_brick_is_incompatible(brick1, brick3,
                                               GWY_BRICK_COMPATIBLE_VALUE),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible(brick3, brick1,
                                               GWY_BRICK_COMPATIBLE_VALUE),
                     ==, 0);

    g_object_unref(brick1);
    g_object_unref(brick2);
    g_object_unref(brick3);
    g_object_unref(brick4);
}

void
test_brick_compatibility_field_res(void)
{
    GwyBrick *brick = gwy_brick_new_sized(2, 3, 4, FALSE);
    GwyField *field1 = gwy_field_new_sized(2, 3, FALSE);
    GwyField *field2 = gwy_field_new_sized(3, 2, FALSE);
    GwyField *field3 = gwy_field_new_sized(3, 3, FALSE);
    GwyField *field4 = gwy_field_new_sized(2, 2, FALSE);

    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field1, GWY_FIELD_COMPATIBLE_XRES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field1, GWY_FIELD_COMPATIBLE_YRES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field1, GWY_FIELD_COMPATIBLE_RES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field2, GWY_FIELD_COMPATIBLE_XRES),
                     ==, GWY_FIELD_COMPATIBLE_XRES);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field2, GWY_FIELD_COMPATIBLE_YRES),
                     ==, GWY_FIELD_COMPATIBLE_YRES);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field2, GWY_FIELD_COMPATIBLE_RES),
                     ==, GWY_FIELD_COMPATIBLE_RES);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field3, GWY_FIELD_COMPATIBLE_XRES),
                     ==, GWY_FIELD_COMPATIBLE_XRES);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field3, GWY_FIELD_COMPATIBLE_YRES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field3, GWY_FIELD_COMPATIBLE_RES),
                     ==, GWY_FIELD_COMPATIBLE_XRES);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field4, GWY_FIELD_COMPATIBLE_XRES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field4, GWY_FIELD_COMPATIBLE_YRES),
                     ==, GWY_FIELD_COMPATIBLE_YRES);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field4, GWY_FIELD_COMPATIBLE_RES),
                     ==, GWY_FIELD_COMPATIBLE_YRES);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field1, GWY_FIELD_COMPATIBLE_XREAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field1, GWY_FIELD_COMPATIBLE_YREAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field1, GWY_FIELD_COMPATIBLE_REAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field2, GWY_FIELD_COMPATIBLE_XREAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field2, GWY_FIELD_COMPATIBLE_YREAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field2, GWY_FIELD_COMPATIBLE_REAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field3, GWY_FIELD_COMPATIBLE_XREAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field3, GWY_FIELD_COMPATIBLE_YREAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field3, GWY_FIELD_COMPATIBLE_REAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field4, GWY_FIELD_COMPATIBLE_XREAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field4, GWY_FIELD_COMPATIBLE_YREAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field4, GWY_FIELD_COMPATIBLE_REAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field1, GWY_FIELD_COMPATIBLE_DX),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field1, GWY_FIELD_COMPATIBLE_DY),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field1, GWY_FIELD_COMPATIBLE_DXDY),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field2, GWY_FIELD_COMPATIBLE_DX),
                     ==, GWY_FIELD_COMPATIBLE_DX);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field2, GWY_FIELD_COMPATIBLE_DY),
                     ==, GWY_FIELD_COMPATIBLE_DY);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field2, GWY_FIELD_COMPATIBLE_DXDY),
                     ==, GWY_FIELD_COMPATIBLE_DXDY);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field3, GWY_FIELD_COMPATIBLE_DX),
                     ==, GWY_FIELD_COMPATIBLE_DX);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field3, GWY_FIELD_COMPATIBLE_DY),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field3, GWY_FIELD_COMPATIBLE_DXDY),
                     ==, GWY_FIELD_COMPATIBLE_DX);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field4, GWY_FIELD_COMPATIBLE_DX),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field4, GWY_FIELD_COMPATIBLE_DY),
                     ==, GWY_FIELD_COMPATIBLE_DY);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field4, GWY_FIELD_COMPATIBLE_DXDY),
                     ==, GWY_FIELD_COMPATIBLE_DY);

    g_object_unref(field1);
    g_object_unref(field2);
    g_object_unref(field3);
    g_object_unref(field4);
    g_object_unref(brick);
}

void
test_brick_compatibility_field_real(void)
{
    GwyBrick *brick = gwy_brick_new_sized(2, 2, 4, FALSE);
    GwyField *field1 = gwy_field_new_sized(2, 2, FALSE);
    GwyField *field2 = gwy_field_new_sized(2, 2, FALSE);
    GwyField *field3 = gwy_field_new_sized(2, 2, FALSE);
    GwyField *field4 = gwy_field_new_sized(2, 2, FALSE);

    gwy_field_set_xreal(field2, 2.0);
    gwy_field_set_yreal(field2, 2.0);
    gwy_field_set_xreal(field3, 2.0);
    gwy_field_set_yreal(field4, 2.0);

    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field1, GWY_FIELD_COMPATIBLE_XRES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field1, GWY_FIELD_COMPATIBLE_YRES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field1, GWY_FIELD_COMPATIBLE_RES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field2, GWY_FIELD_COMPATIBLE_XRES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field2, GWY_FIELD_COMPATIBLE_YRES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field2, GWY_FIELD_COMPATIBLE_RES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field3, GWY_FIELD_COMPATIBLE_XRES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field3, GWY_FIELD_COMPATIBLE_YRES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field3, GWY_FIELD_COMPATIBLE_RES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field4, GWY_FIELD_COMPATIBLE_XRES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field4, GWY_FIELD_COMPATIBLE_YRES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field4, GWY_FIELD_COMPATIBLE_RES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field1, GWY_FIELD_COMPATIBLE_XREAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field1, GWY_FIELD_COMPATIBLE_YREAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field1, GWY_FIELD_COMPATIBLE_REAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field2, GWY_FIELD_COMPATIBLE_XREAL),
                     ==, GWY_FIELD_COMPATIBLE_XREAL);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field2, GWY_FIELD_COMPATIBLE_YREAL),
                     ==, GWY_FIELD_COMPATIBLE_YREAL);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field2, GWY_FIELD_COMPATIBLE_REAL),
                     ==, GWY_FIELD_COMPATIBLE_REAL);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field3, GWY_FIELD_COMPATIBLE_XREAL),
                     ==, GWY_FIELD_COMPATIBLE_XREAL);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field3, GWY_FIELD_COMPATIBLE_YREAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field3, GWY_FIELD_COMPATIBLE_REAL),
                     ==, GWY_FIELD_COMPATIBLE_XREAL);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field4, GWY_FIELD_COMPATIBLE_XREAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field4, GWY_FIELD_COMPATIBLE_YREAL),
                     ==, GWY_FIELD_COMPATIBLE_YREAL);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field4, GWY_FIELD_COMPATIBLE_REAL),
                     ==, GWY_FIELD_COMPATIBLE_YREAL);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field1, GWY_FIELD_COMPATIBLE_DX),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field1, GWY_FIELD_COMPATIBLE_DY),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field1, GWY_FIELD_COMPATIBLE_DXDY),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field2, GWY_FIELD_COMPATIBLE_DX),
                     ==, GWY_FIELD_COMPATIBLE_DX);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field2, GWY_FIELD_COMPATIBLE_DY),
                     ==, GWY_FIELD_COMPATIBLE_DY);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field2, GWY_FIELD_COMPATIBLE_DXDY),
                     ==, GWY_FIELD_COMPATIBLE_DXDY);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field3, GWY_FIELD_COMPATIBLE_DX),
                     ==, GWY_FIELD_COMPATIBLE_DX);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field3, GWY_FIELD_COMPATIBLE_DY),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field3, GWY_FIELD_COMPATIBLE_DXDY),
                     ==, GWY_FIELD_COMPATIBLE_DX);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field4, GWY_FIELD_COMPATIBLE_DX),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field4, GWY_FIELD_COMPATIBLE_DY),
                     ==, GWY_FIELD_COMPATIBLE_DY);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field4, GWY_FIELD_COMPATIBLE_DXDY),
                     ==, GWY_FIELD_COMPATIBLE_DY);

    g_object_unref(field1);
    g_object_unref(field2);
    g_object_unref(field3);
    g_object_unref(field4);
    g_object_unref(brick);
}

void
test_brick_compatibility_field_units(void)
{
    GwyBrick *brick = gwy_brick_new_sized(2, 2, 2, FALSE);
    GwyField *field1 = gwy_field_new_sized(2, 2, FALSE);
    GwyField *field2 = gwy_field_new_sized(2, 2, FALSE);
    GwyField *field3 = gwy_field_new_sized(2, 2, FALSE);
    GwyField *field4 = gwy_field_new_sized(2, 2, FALSE);

    gwy_unit_set_from_string(gwy_brick_get_unit_xy(brick), "m", NULL);
    gwy_unit_set_from_string(gwy_brick_get_unit_z(brick), "s", NULL);
    gwy_unit_set_from_string(gwy_brick_get_unit_w(brick), "A", NULL);
    gwy_unit_set_from_string(gwy_field_get_unit_xy(field1), "m", NULL);
    gwy_unit_set_from_string(gwy_field_get_unit_z(field1), "A", NULL);
    gwy_unit_set_from_string(gwy_field_get_unit_xy(field3), "m", NULL);
    gwy_unit_set_from_string(gwy_field_get_unit_z(field4), "A", NULL);

    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field1, GWY_FIELD_COMPATIBLE_LATERAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field1, GWY_FIELD_COMPATIBLE_VALUE),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field1, GWY_FIELD_COMPATIBLE_UNITS),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field2, GWY_FIELD_COMPATIBLE_LATERAL),
                     ==, GWY_FIELD_COMPATIBLE_LATERAL);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field2, GWY_FIELD_COMPATIBLE_VALUE),
                     ==, GWY_FIELD_COMPATIBLE_VALUE);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field2, GWY_FIELD_COMPATIBLE_UNITS),
                     ==, GWY_FIELD_COMPATIBLE_UNITS);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field3, GWY_FIELD_COMPATIBLE_LATERAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field3, GWY_FIELD_COMPATIBLE_VALUE),
                     ==, GWY_FIELD_COMPATIBLE_VALUE);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field3, GWY_FIELD_COMPATIBLE_UNITS),
                     ==, GWY_FIELD_COMPATIBLE_VALUE);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field4, GWY_FIELD_COMPATIBLE_LATERAL),
                     ==, GWY_FIELD_COMPATIBLE_LATERAL);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field4, GWY_FIELD_COMPATIBLE_VALUE),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_field
                                  (brick, field4, GWY_FIELD_COMPATIBLE_UNITS),
                     ==, GWY_FIELD_COMPATIBLE_LATERAL);

    g_object_unref(field1);
    g_object_unref(field2);
    g_object_unref(field3);
    g_object_unref(field4);
    g_object_unref(brick);
}

void
test_brick_compatibility_line_res(void)
{
    GwyBrick *brick = gwy_brick_new_sized(2, 3, 4, FALSE);
    GwyLine *line1 = gwy_line_new_sized(4, FALSE);
    GwyLine *line2 = gwy_line_new_sized(5, FALSE);

    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                                  (brick, line1, GWY_LINE_COMPATIBLE_RES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                                  (brick, line2, GWY_LINE_COMPATIBLE_RES),
                     ==, GWY_LINE_COMPATIBLE_RES);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                                  (brick, line1, GWY_LINE_COMPATIBLE_REAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                                  (brick, line2, GWY_LINE_COMPATIBLE_REAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                                  (brick, line1, GWY_LINE_COMPATIBLE_DX),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                                  (brick, line2, GWY_LINE_COMPATIBLE_DX),
                     ==, GWY_LINE_COMPATIBLE_DX);

    g_object_unref(line1);
    g_object_unref(line2);
    g_object_unref(brick);
}

void
test_brick_compatibility_line_real(void)
{
    GwyBrick *brick = gwy_brick_new_sized(2, 3, 4, FALSE);
    GwyLine *line1 = gwy_line_new_sized(4, FALSE);
    GwyLine *line2 = gwy_line_new_sized(4, FALSE);

    gwy_line_set_real(line2, 2.0);

    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                                  (brick, line1, GWY_LINE_COMPATIBLE_RES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                                  (brick, line2, GWY_LINE_COMPATIBLE_RES),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                                  (brick, line1, GWY_LINE_COMPATIBLE_REAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                                  (brick, line2, GWY_LINE_COMPATIBLE_REAL),
                     ==, GWY_LINE_COMPATIBLE_REAL);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                                  (brick, line1, GWY_LINE_COMPATIBLE_DX),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                                  (brick, line2, GWY_LINE_COMPATIBLE_DX),
                     ==, GWY_LINE_COMPATIBLE_DX);

    g_object_unref(line1);
    g_object_unref(line2);
    g_object_unref(brick);
}

void
test_brick_compatibility_line_units(void)
{
    GwyBrick *brick = gwy_brick_new_sized(2, 3, 4, FALSE);
    GwyLine *line1 = gwy_line_new_sized(4, FALSE);
    GwyLine *line2 = gwy_line_new_sized(4, FALSE);
    GwyLine *line3 = gwy_line_new_sized(4, FALSE);
    GwyLine *line4 = gwy_line_new_sized(4, FALSE);

    gwy_unit_set_from_string(gwy_brick_get_unit_xy(brick), "m", NULL);
    gwy_unit_set_from_string(gwy_brick_get_unit_z(brick), "s", NULL);
    gwy_unit_set_from_string(gwy_brick_get_unit_w(brick), "A", NULL);
    gwy_unit_set_from_string(gwy_line_get_unit_x(line1), "s", NULL);
    gwy_unit_set_from_string(gwy_line_get_unit_y(line1), "A", NULL);
    gwy_unit_set_from_string(gwy_line_get_unit_x(line3), "s", NULL);
    gwy_unit_set_from_string(gwy_line_get_unit_y(line4), "A", NULL);

    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                                  (brick, line1, GWY_LINE_COMPATIBLE_LATERAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                                  (brick, line1, GWY_LINE_COMPATIBLE_VALUE),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                                  (brick, line1, GWY_LINE_COMPATIBLE_UNITS),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                                  (brick, line2, GWY_LINE_COMPATIBLE_LATERAL),
                     ==, GWY_LINE_COMPATIBLE_LATERAL);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                                  (brick, line2, GWY_LINE_COMPATIBLE_VALUE),
                     ==, GWY_LINE_COMPATIBLE_VALUE);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                                  (brick, line2, GWY_LINE_COMPATIBLE_UNITS),
                     ==, GWY_LINE_COMPATIBLE_UNITS);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                                  (brick, line3, GWY_LINE_COMPATIBLE_LATERAL),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                                  (brick, line3, GWY_LINE_COMPATIBLE_VALUE),
                     ==, GWY_LINE_COMPATIBLE_VALUE);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                                  (brick, line3, GWY_LINE_COMPATIBLE_UNITS),
                     ==, GWY_LINE_COMPATIBLE_VALUE);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                                  (brick, line4, GWY_LINE_COMPATIBLE_LATERAL),
                     ==, GWY_LINE_COMPATIBLE_LATERAL);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                                  (brick, line4, GWY_LINE_COMPATIBLE_VALUE),
                     ==, 0);
    g_assert_cmpuint(gwy_brick_is_incompatible_with_line
                                  (brick, line4, GWY_LINE_COMPATIBLE_UNITS),
                     ==, GWY_LINE_COMPATIBLE_LATERAL);

    g_object_unref(line1);
    g_object_unref(line2);
    g_object_unref(line3);
    g_object_unref(line4);
    g_object_unref(brick);
}

static void
summarize_lines_one(GwyBrickLineSummary quantity,
                    gdouble (*line_stat_function)(const GwyLine *line))
{
    enum { max_size = 19 };
    GRand *rng = g_rand_new();
    g_rand_set_seed(rng, 42);
    gsize niter = g_test_slow() ? 100 : 30;

    for (gsize iter = 0; iter < niter; iter++) {
        guint xres = g_rand_int_range(rng, 1, max_size);
        guint yres = g_rand_int_range(rng, 1, max_size/2);
        guint zres = g_rand_int_range(rng, 2, max_size/2);
        GwyBrick *brick = gwy_brick_new_sized(xres, yres, zres, FALSE);
        brick_randomize(brick, rng);
        guint width = g_rand_int_range(rng, 1, xres+1);
        guint height = g_rand_int_range(rng, 1, yres+1);
        guint depth = g_rand_int_range(rng, 2, zres+1);
        guint col = g_rand_int_range(rng, 0, xres-width+1);
        guint row = g_rand_int_range(rng, 0, yres-height+1);
        guint level = g_rand_int_range(rng, 0, zres-depth+1);
        GwyBrickPart bpart = { col, row, level, width, height, depth };
        GwyField *fullfield = gwy_field_new_sized(width, height, FALSE);
        GwyField *partfield = gwy_field_new_sized(xres, yres, TRUE);

        gwy_brick_summarize_lines(brick, &bpart, fullfield, quantity);
        gwy_brick_summarize_lines(brick, &bpart, partfield, quantity);

        for (guint i = 0; i < yres; i++) {
            for (guint j = 0; j < xres; j++) {
                if (i < row || i >= row + height
                    || j < col || j > col + width)
                    g_assert_cmpfloat(partfield->data[i*xres + j], ==, 0.0);
            }
        }

        GwyLine *line = gwy_line_new_sized(depth, FALSE);
        GwyLinePart lpart = { level, depth };
        for (guint i = 0; i < height; i++) {
            for (guint j = 0; j < width; j++) {
                gdouble vfull = fullfield->data[i*width + j],
                        vpart = partfield->data[(i + row)*xres + (j + col)];
                g_assert_cmpfloat(vpart, ==, vfull);
                gwy_brick_extract_line(brick, line, &lpart,
                                       j + col, i + row, FALSE);
                gdouble vline = line_stat_function(line);
                g_assert_cmpfloat(fabs(vfull - vline), <, 1e-14);
            }
        }

        g_object_unref(line);
        g_object_unref(brick);
        g_object_unref(partfield);
        g_object_unref(fullfield);
    }
    g_rand_free(rng);
}

static gdouble
line_min(const GwyLine *line)
{
    gdouble min, max;
    gwy_line_min_max_full(line, &min, &max);
    return min;
}

static gdouble
line_max(const GwyLine *line)
{
    gdouble min, max;
    gwy_line_min_max_full(line, &min, &max);
    return max;
}

void
test_brick_summarize_lines_minimum(void)
{
    summarize_lines_one(GWY_BRICK_LINE_MINIMUM, &line_min);
}

void
test_brick_summarize_lines_maximum(void)
{
    summarize_lines_one(GWY_BRICK_LINE_MAXIMUM, &line_max);
}

void
test_brick_summarize_lines_mean(void)
{
    summarize_lines_one(GWY_BRICK_LINE_MEAN, &gwy_line_mean_full);
}

void
test_brick_summarize_lines_rms(void)
{
    summarize_lines_one(GWY_BRICK_LINE_RMS, &gwy_line_rms_full);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
