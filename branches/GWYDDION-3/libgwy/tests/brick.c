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
    GwyBrick *brick = gwy_brick_new();
    guint counter = 0;
    g_signal_connect_swapped(brick, "data-changed",
                             G_CALLBACK(record_signal), &counter);
    gwy_brick_data_changed(brick);
    g_assert_cmpuint(counter, ==, 1);
    g_object_unref(brick);
}

void
test_brick_units(void)
{
    enum { max_size = 30 };
    GRand *rng = g_rand_new();
    g_rand_set_seed(rng, 42);
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
    GRand *rng = g_rand_new();
    g_rand_set_seed(rng, 42);
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
    GRand *rng = g_rand_new();
    g_rand_set_seed(rng, 42);
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

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
