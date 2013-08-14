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
 * Field fast Fourier transform
 *
 ***************************************************************************/

void
test_field_row_fft_humanize_inversion(void)
{
    enum { max_size = 16 };
    GRand *rng = g_rand_new_with_seed(42);
    gsize niter = 50;

    for (guint iter = 0; iter < niter; iter++) {
        guint width = g_rand_int_range(rng, 1, max_size);
        guint height = g_rand_int_range(rng, 1, max_size);
        GwyField *reference = gwy_field_new_sized(width, height, FALSE);
        field_randomize(reference, rng);
        gwy_field_set_xoffset(reference, -0.5*gwy_field_dx(reference));
        GwyField *field = gwy_field_duplicate(reference);
        gwy_field_row_fft_humanize(field);
        gwy_field_row_fft_dehumanize(field);
        field_assert_equal(field, reference);
        g_object_unref(field);
        g_object_unref(reference);
    }

    g_rand_free(rng);
}

void
test_field_fft_humanize_inversion(void)
{
    enum { max_size = 16 };
    GRand *rng = g_rand_new_with_seed(42);
    gsize niter = 50;

    for (guint iter = 0; iter < niter; iter++) {
        guint width = g_rand_int_range(rng, 1, max_size);
        guint height = g_rand_int_range(rng, 1, max_size);
        GwyField *reference = gwy_field_new_sized(width, height, FALSE);
        field_randomize(reference, rng);
        gwy_field_set_xoffset(reference, -0.5*gwy_field_dx(reference));
        gwy_field_set_yoffset(reference, -0.5*gwy_field_dy(reference));
        GwyField *field = gwy_field_duplicate(reference);
        gwy_field_fft_humanize(field);
        gwy_field_fft_dehumanize(field);
        field_assert_equal(field, reference);
        g_object_unref(field);
        g_object_unref(reference);
    }

    g_rand_free(rng);
}

typedef void (*RawFFTFunc)(const GwyField*, const GwyField*,
                           GwyField*, GwyField*,
                           GwyTransformDirection);

static void
field_fft_inversion_c2c_one(GwyTransformDirection direction,
                            RawFFTFunc fft)
{
    enum { max_size = 17 };
    GRand *rng = g_rand_new_with_seed(42);
    gsize niter = 50;

    GwyField *fftre = gwy_field_new(), *fftim = gwy_field_new();
    for (guint iter = 0; iter < niter; iter++) {
        guint width = g_rand_int_range(rng, 1, max_size);
        guint height = g_rand_int_range(rng, 1, max_size);
        GwyField *refield = gwy_field_new_sized(width, height, FALSE);
        GwyField *imfield = gwy_field_new_sized(width, height, FALSE);
        field_randomize(refield, rng);
        field_randomize(imfield, rng);
        GwyField *rereference = gwy_field_duplicate(refield);
        GwyField *imreference = gwy_field_duplicate(imfield);
        fft(refield, imfield, fftre, fftim, direction);
        field_assert_equal(refield, rereference);
        field_assert_equal(imfield, imreference);
        fft(fftre, fftim, refield, imfield, -direction);
        field_assert_numerically_equal(refield, rereference, 1e-14);
        field_assert_numerically_equal(imfield, imreference, 1e-14);
        g_object_unref(rereference);
        g_object_unref(imreference);
        g_object_unref(refield);
        g_object_unref(imfield);
    }
    g_object_unref(fftre);
    g_object_unref(fftim);

    g_rand_free(rng);
}

void
test_field_row_fft_inversion_c2c_forward(void)
{
    field_fft_inversion_c2c_one(GWY_TRANSFORM_FORWARD,
                                &gwy_field_row_fft);
}

void
test_field_row_fft_inversion_c2c_backward(void)
{
    field_fft_inversion_c2c_one(GWY_TRANSFORM_BACKWARD,
                                &gwy_field_row_fft);
}

void
test_field_fft_inversion_c2c_forward(void)
{
    field_fft_inversion_c2c_one(GWY_TRANSFORM_FORWARD,
                                &gwy_field_fft);
}

void
test_field_fft_inversion_c2c_backward(void)
{
    field_fft_inversion_c2c_one(GWY_TRANSFORM_BACKWARD,
                                &gwy_field_fft);
}

static void
field_fft_inversion_x2c_one(GwyTransformDirection direction,
                                RawFFTFunc fft,
                                gboolean real)
{
    enum { max_size = 17 };
    GRand *rng = g_rand_new_with_seed(42);
    gsize niter = 50;

    GwyField *fftre = gwy_field_new(), *fftim = gwy_field_new();
    for (guint iter = 0; iter < niter; iter++) {
        guint width = g_rand_int_range(rng, 1, max_size);
        guint height = g_rand_int_range(rng, 1, max_size);
        GwyField *field = gwy_field_new_sized(width, height, FALSE);
        field_randomize(field, rng);
        GwyField *reference = gwy_field_duplicate(field);
        fft(real ? field : NULL, real ? NULL : field, fftre, fftim, direction);
        field_assert_equal(field, reference);
        fft(fftre, fftim, real ? field : NULL, real ? NULL : field, -direction);
        field_assert_numerically_equal(field, reference, 1e-14);
        g_object_unref(reference);
        g_object_unref(field);
    }
    g_object_unref(fftre);
    g_object_unref(fftim);

    g_rand_free(rng);
}

void
test_field_row_fft_inversion_r2c_forward(void)
{
    field_fft_inversion_x2c_one(GWY_TRANSFORM_FORWARD,
                                &gwy_field_row_fft, TRUE);
}

void
test_field_row_fft_inversion_r2c_backward(void)
{
    field_fft_inversion_x2c_one(GWY_TRANSFORM_BACKWARD,
                                &gwy_field_row_fft, TRUE);
}

void
test_field_row_fft_inversion_i2c_forward(void)
{
    field_fft_inversion_x2c_one(GWY_TRANSFORM_FORWARD,
                                &gwy_field_row_fft, FALSE);
}

void
test_field_row_fft_inversion_i2c_backward(void)
{
    field_fft_inversion_x2c_one(GWY_TRANSFORM_BACKWARD,
                                &gwy_field_row_fft, FALSE);
}

void
test_field_fft_inversion_r2c_forward(void)
{
    field_fft_inversion_x2c_one(GWY_TRANSFORM_FORWARD,
                                &gwy_field_fft, TRUE);
}

void
test_field_fft_inversion_r2c_backward(void)
{
    field_fft_inversion_x2c_one(GWY_TRANSFORM_BACKWARD,
                                &gwy_field_fft, TRUE);
}

void
test_field_fft_inversion_i2c_forward(void)
{
    field_fft_inversion_x2c_one(GWY_TRANSFORM_FORWARD,
                                &gwy_field_fft, FALSE);
}

void
test_field_fft_inversion_i2c_backward(void)
{
    field_fft_inversion_x2c_one(GWY_TRANSFORM_BACKWARD,
                                &gwy_field_fft, FALSE);
}

static void
field_sine_wave_fill(GwyField *field,
                     gdouble alpha, gdouble beta)
{
    guint xres = field->xres, yres = field->yres;
    gdouble dx = gwy_field_dx(field), dy = gwy_field_dy(field),
            xoff = field->xoff, yoff = field->yoff;
    for (guint i = 0; i < yres; i++) {
        gdouble y = (i + 0.5)*dy + yoff;
        for (guint j = 0; j < xres; j++) {
            gdouble x = (j + 0.5)*dx + xoff;
            gwy_field_index(field, j, i) = cos(alpha*x + beta*y);
        }
    }
}

void
test_field_fft_sine(void)
{
    GRand *rng = g_rand_new_with_seed(42);
    gsize niter = 100;

    GwyField *fftre = gwy_field_new(), *fftim = gwy_field_new();
    for (guint iter = 0; iter < niter; iter++) {
        guint width = g_rand_int_range(rng, 100, 200);
        guint height = g_rand_int_range(rng, 100, 200);
        GwyField *field = gwy_field_new_sized(width, height, FALSE);
        gdouble alpha = g_rand_double_range(rng, 5.0, 90.0);
        gdouble beta = g_rand_double_range(rng, 5.0, 90.0);
        GwyWindowingType windowing = g_rand_int_range(rng,
                                                      0,
                                                      GWY_WINDOWING_KAISER25+1);
        field_sine_wave_fill(field, alpha, beta);
        gwy_field_fft_window(field, windowing, TRUE, TRUE);
        gwy_field_fft(field, NULL, fftre, fftim, GWY_TRANSFORM_FORWARD);
        gwy_field_fft_humanize(fftre);
        gwy_field_fft_humanize(fftim);
        gwy_field_hypot_field(fftre, fftre, fftim);
        guint nex, indices;
        GwyFieldPart fpart = { 0, height/2, width, height - height/2 };
        nex = gwy_field_local_extrema(fftre, &fpart, NULL, GWY_MASK_IGNORE,
                                      &indices, 1, TRUE);
        g_assert_cmpuint(nex, ==, 1);
        guint i = indices/width, j = indices % width;
        gdouble dx = gwy_field_dx(fftre), dy = gwy_field_dy(fftre);
        GwyCurvatureParams curv;
        gint nc;
        nc = gwy_field_curvature(fftre, NULL, GWY_MASK_IGNORE, j, i, 1, 1,
                                 FALSE, TRUE, GWY_EXTERIOR_MIRROR_EXTEND, NAN,
                                 &curv);
        g_assert_cmpint(nc, ==, 2);
        //g_printerr("[%u] alpha=%g, beta=%g, delta=(%g,%g), tol=(%g,%g)\n", iter, alpha, beta, curv.xc - alpha, curv.yc - beta, 0.25*dx, 0.25*dy);
        gwy_assert_floatval(curv.xc, alpha, 0.25*dx);
        gwy_assert_floatval(curv.yc, beta, 0.25*dy);
        g_object_unref(field);
    }
    g_object_unref(fftre);
    g_object_unref(fftim);

    g_rand_free(rng);
}

static gdouble
find_row_max_pos(const GwyField *field,
                 guint row)
{
    guint xres = field->xres;
    const gdouble *data = field->data + row*xres;
    gdouble m = -G_MAXDOUBLE;
    guint im = xres/2;

    for (guint i = xres/2; i+1 < xres; i++) {
        if (data[i] > m) {
            m = data[i];
            im = i;
        }
    }

    gdouble p = (data[im] - data[im+1])/(data[im] - data[im-1]);
    p = 0.5*(1.0 - p)/(p + 1.0) + im;
    return gwy_field_dx(field)*(p + 0.5) + field->xoff;
}

void
test_field_row_fft_sine(void)
{
    GRand *rng = g_rand_new_with_seed(42);
    gsize niter = 100;

    GwyField *fftre = gwy_field_new(), *fftim = gwy_field_new();
    for (guint iter = 0; iter < niter; iter++) {
        guint width = g_rand_int_range(rng, 100, 200);
        guint height = g_rand_int_range(rng, 100, 200);
        GwyField *field = gwy_field_new_sized(width, height, FALSE);
        gdouble alpha = g_rand_double_range(rng, 15.0, 90.0);
        gdouble beta = g_rand_double_range(rng, 1.0, 10.0);
        GwyWindowingType windowing = g_rand_int_range(rng,
                                                      0,
                                                      GWY_WINDOWING_KAISER25+1);
        field_sine_wave_fill(field, alpha, beta);
        gwy_field_fft_window(field, windowing, FALSE, TRUE);
        gwy_field_row_fft(field, NULL, fftre, fftim, GWY_TRANSFORM_FORWARD);
        gwy_field_fft_humanize(fftre);
        gwy_field_fft_humanize(fftim);
        gwy_field_hypot_field(fftre, fftre, fftim);
        for (guint row = 0; row < height; row++) {
            gdouble xc = find_row_max_pos(fftre, row);
            //g_printerr("[%u] %g (%g)\n", row, xc, alpha);
            gwy_assert_floatval(xc, alpha, 0.4*gwy_field_dx(fftre));
        }
        g_object_unref(field);
    }
    g_object_unref(fftre);
    g_object_unref(fftim);

    g_rand_free(rng);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
