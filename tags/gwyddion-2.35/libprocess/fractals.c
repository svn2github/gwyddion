/*
 *  @(#) $Id$
 *  Copyright (C) 2004 Jindrich Bilek.
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include <glib.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/datafield.h>
#include <libprocess/stats.h>

static void gwy_data_field_fractal_fit(GwyDataLine *xresult,
                                       GwyDataLine *yresult,
                                       gdouble *a,
                                       gdouble *b);

/**
 * gwy_data_field_fractal_partitioning:
 * @data_field: A data field.
 * @xresult: Data line to store x-values for log-log plot to.
 * @yresult: Data line to store y-values for log-log plot to.
 * @interpolation: Interpolation type.
 *
 * Computes data for log-log plot by partitioning.
 *
 * Data lines @xresult and @yresult will be resized to the output size and
 * they will contain corresponding values at each position.
 **/
void
gwy_data_field_fractal_partitioning(GwyDataField *data_field,
                                    GwyDataLine *xresult,
                                    GwyDataLine *yresult,
                                    GwyInterpolationType interpolation)
{
    GwyDataField *buffer;
    gint i, j, l, rp, dimexp, xnewres;
    gdouble rms;


    dimexp = (gint)floor(log((gdouble)data_field->xres)/log(2.0) + 0.5);
    xnewres = (1 << dimexp) + 1;

    buffer = gwy_data_field_new_resampled(data_field, xnewres, xnewres,
                                          interpolation);
    gwy_data_line_resample(xresult, dimexp-1, GWY_INTERPOLATION_NONE);
    gwy_data_line_resample(yresult, dimexp-1, GWY_INTERPOLATION_NONE);
    gwy_data_line_clear(yresult);

    for (l = 1; l < dimexp; l++) {
        rp = 1 << l;
        for (i = 0; i < (buffer->xres - 1)/rp - 1; i++) {
            for (j = 0; j < (buffer->yres - 1)/rp - 1; j++) {
                rms = gwy_data_field_area_get_rms(buffer, NULL,
                                                  i*rp, j*rp, rp, rp);
                yresult->data[l-1] += rms * rms;
            }
        }
        xresult->data[l-1] = log(rp);
        yresult->data[l-1] = log(yresult->data[l-1]
                           /(((xnewres - 1)/rp - 1) * ((xnewres - 1)/rp - 1)));

    }
    g_object_unref(buffer);
}

static void
fractal_partitioning_nomask(GwyDataField *data_field,
                            GwyDataField *mask_field,
                            GwyDataLine *xresult,
                            GwyDataLine *yresult,
                            GwyInterpolationType interpolation)
{
    GwyDataField *buffer, *maskbuffer;
    gint i, j, l, rp, dimexp, xnewres;
    gdouble rms;


    dimexp = (gint)floor(log((gdouble)data_field->xres)/log(2.0) + 0.5);
    xnewres = (1 << dimexp) + 1;

    buffer = gwy_data_field_new_resampled(data_field, xnewres, xnewres,
                                          interpolation);
    maskbuffer = gwy_data_field_new_resampled(mask_field, xnewres, xnewres,
                                              GWY_INTERPOLATION_ROUND);
    gwy_data_line_resample(xresult, dimexp, GWY_INTERPOLATION_NONE);
    gwy_data_line_resample(yresult, dimexp, GWY_INTERPOLATION_NONE);
    gwy_data_line_clear(yresult);

    for (l = 0; l < dimexp; l++) {
        rp = 1 << l;
        for (i = 0; i < (buffer->xres - 1)/rp - 1; i++) {
            for (j = 0; j < (buffer->yres - 1)/rp - 1; j++) {
                rms = gwy_data_field_area_get_rms(buffer, NULL,
                                                  i*rp, j*rp, rp, rp);
                yresult->data[l] += rms * rms;
            }
        }
        /* Work around floating point arithmetic implementations that yield
         * NaNs when one attempts to calculate exp(log(0)) expecting 0
         * (greetings to Microsoft). */
        if (!yresult->data[l])
            yresult->data[l] = G_MINDOUBLE;

        yresult->data[l]
            = log(yresult->data[l]
              /(((buffer->xres - 1)/rp - 1) * ((buffer->yres - 1)/rp - 1)));

    }
    g_object_unref(buffer);
    g_object_unref(maskbuffer);
}

/**
 * gwy_data_field_fractal_cubecounting:
 * @data_field: A data field.
 * @xresult: Data line to store x-values for log-log plot to.
 * @yresult: Data line to store y-values for log-log plot to.
 * @interpolation: Interpolation type.
 *
 * Computes data for log-log plot by cube counting.
 *
 * Data lines @xresult and @yresult will be resized to the output size and
 * they will contain corresponding values at each position.
 **/
void
gwy_data_field_fractal_cubecounting(GwyDataField *data_field,
                                    GwyDataLine *xresult,
                                    GwyDataLine *yresult,
                                    GwyInterpolationType interpolation)
{
    GwyDataField *buffer;
    gint i, j, l, m, n, rp, rp2, dimexp;

    gdouble a, max, min, imin, hlp, height, xnewres;

    dimexp = (gint)floor(log((gdouble)data_field->xres)/G_LN2 + 0.5);
    xnewres = (1 << dimexp) + 1;

    buffer = gwy_data_field_duplicate(data_field);
    gwy_data_field_resample(buffer, xnewres, xnewres, interpolation);
    gwy_data_line_resample(xresult, dimexp, GWY_INTERPOLATION_NONE);
    gwy_data_line_resample(yresult, dimexp, GWY_INTERPOLATION_NONE);
    gwy_data_line_clear(yresult);

    gwy_data_field_get_min_max(buffer, &imin, &height);
    height -= imin;

    for (l = 0; l < dimexp; l++) {
        rp = 1 << (l + 1);
        rp2 = (1 << dimexp)/rp;
        a = height/rp;
        for (i = 0; i < rp; i++) {
            for (j = 0; j < rp; j++) {
                max = -G_MAXDOUBLE;
                min = G_MAXDOUBLE;
                for (m = 0; m <= rp2; m++) {
                    for (n = 0; n <= rp2; n++) {
                        hlp = buffer->data[(i * rp2 + m)
                              + buffer->xres * (j * rp2 + n)] - imin;
                        if (hlp > max)
                            max = hlp;
                        if (hlp < min)
                            min = hlp;
                    }
                }
                yresult->data[l] +=
                    rp - floor(min/a) - floor((height - max)/a);
            }
        }
        xresult->data[l] = (l+1 - dimexp)*G_LN2;
    }
    for (l = 0; l < dimexp; l++) {
        yresult->data[l] = log(yresult->data[l]);
    }
    g_object_unref(buffer);
}

/**
 * gwy_data_field_fractal_triangulation:
 * @data_field: A data field.
 * @xresult: Data line to store x-values for log-log plot to.
 * @yresult: Data line to store y-values for log-log plot to.
 * @interpolation: Interpolation type.
 *
 * Computes data for log-log plot by triangulation.
 *
 * Data lines @xresult and @yresult will be resized to the output size and
 * they will contain corresponding values at each position.
 **/
void
gwy_data_field_fractal_triangulation(GwyDataField *data_field,
                                     GwyDataLine *xresult,
                                     GwyDataLine *yresult,
                                     GwyInterpolationType interpolation)
{
    GwyDataField *buffer;
    gint i, j, l, rp, rp2, dimexp, xnewres;

    gdouble dil, a, b, c, d, e, s1, s2, s, z1, z2, z3, z4, height;

    dimexp = (gint)floor(log((gdouble)data_field->xres)/log(2.0) + 0.5);
    xnewres = (1 << dimexp) + 1;

    buffer = gwy_data_field_duplicate(data_field);
    gwy_data_field_resample(buffer, xnewres, xnewres, interpolation);
    gwy_data_line_resample(xresult, dimexp + 1, GWY_INTERPOLATION_NONE);
    gwy_data_line_resample(yresult, dimexp + 1, GWY_INTERPOLATION_NONE);
    gwy_data_line_clear(yresult);

    height = gwy_data_field_get_max(buffer) - gwy_data_field_get_min(buffer);
    dil = (gdouble)(1 << dimexp)/height;
    dil *= dil;

    for (l = 0; l <= dimexp; l++) {
        rp = 1 << l;
        rp2 = (1 << dimexp)/rp;
        for (i = 0; i < rp; i++) {
            for (j = 0; j < rp; j++) {
                z1 = buffer->data[(i * rp2) + buffer->xres * (j * rp2)];
                z2 = buffer->data[((i + 1) * rp2) + buffer->xres * (j * rp2)];
                z3 = buffer->data[(i * rp2) + buffer->xres * ((j + 1) * rp2)];
                z4 = buffer->data[((i + 1) * rp2)
                     + buffer->xres * ((j + 1) * rp2)];

                a = sqrt(rp2 * rp2 + dil * (z1 - z2) * (z1 - z2));
                b = sqrt(rp2 * rp2 + dil * (z1 - z3) * (z1 - z3));
                c = sqrt(rp2 * rp2 + dil * (z3 - z4) * (z3 - z4));
                d = sqrt(rp2 * rp2 + dil * (z2 - z4) * (z2 - z4));
                e = sqrt(2 * rp2 * rp2 + dil * (z3 - z2) * (z3 - z2));

                s1 = (a + b + e)/2;
                s2 = (c + d + e)/2;
                s = sqrt(s1 * (s1 - a) * (s1 - b) * (s1 - e))
                    + sqrt(s2 * (s2 - c) * (s2 - d) * (s2 - e));

                yresult->data[l] += s;
            }
        }
        xresult->data[l] = (l - dimexp)*G_LN2;
    }
    for (l = 0; l <= dimexp; l++) {
        yresult->data[l] = log(yresult->data[l]);
    }
    g_object_unref(buffer);
}

/**
 * gwy_data_field_fractal_psdf:
 * @data_field: A data field.
 * @xresult: Data line to store x-values for log-log plot to.
 * @yresult: Data line to store y-values for log-log plot to.
 * @interpolation: Interpolation type.
 *
 * Computes data for log-log plot by spectral density method.
 *
 * Data lines @xresult and @yresult will be resized to the output size and
 * they will contain corresponding values at each position.
 **/
void
gwy_data_field_fractal_psdf(GwyDataField *data_field,
                            GwyDataLine *xresult,
                            GwyDataLine *yresult,
                            GwyInterpolationType interpolation)
{
    gint i;

    gwy_data_field_psdf(data_field, yresult, GWY_ORIENTATION_HORIZONTAL,
                        interpolation, GWY_WINDOWING_HANN, data_field->xres);
    gwy_data_line_resample(xresult, yresult->res, GWY_INTERPOLATION_NONE);

    for (i = 1; i < yresult->res; i++) {
        xresult->data[i - 1] = log(i);
        yresult->data[i - 1] = log(yresult->data[i]);
    }
    gwy_data_line_resize(xresult, 0, yresult->res - 1);
    gwy_data_line_resize(yresult, 0, yresult->res - 1);
}


/**
 * gwy_data_field_fractal_cubecounting_dim:
 * @xresult: Log-log fractal data (x values).
 * @yresult: Log-log fractal data (y values).
 * @a: Location to store linear fit constant to.
 * @b: Location to store linear fit slope to.
 *
 * Computes fractal dimension by cube counting method from log-log plot data.
 *
 * The @xresult and @yresult data lines are usually calculated by
 * gwy_data_field_fractal_cubecounting().
 *
 * Returns: The fractal dimension.
 **/
gdouble
gwy_data_field_fractal_cubecounting_dim(GwyDataLine *xresult,
                                        GwyDataLine *yresult,
                                        gdouble *a,
                                        gdouble *b)
{
    gwy_data_field_fractal_fit(xresult, yresult, a, b);

    return *a;
}

/**
 * gwy_data_field_fractal_triangulation_dim:
 * @xresult: Log-log fractal data (x values).
 * @yresult: Log-log fractal data (y values).
 * @a: Location to store linear fit constant to.
 * @b: Location to store linear fit slope to.
 *
 * Computes fractal dimension by triangulation method from log-log plot data.
 *
 * The @xresult and @yresult data lines are usually calculated by
 * gwy_data_field_fractal_triangulation().
 *
 * Returns: The fractal dimension.
 **/
gdouble
gwy_data_field_fractal_triangulation_dim(GwyDataLine *xresult,
                                         GwyDataLine *yresult,
                                         gdouble *a,
                                         gdouble *b)
{
    gwy_data_field_fractal_fit(xresult, yresult, a, b);

    return 2.0 + (*a);
}

/**
 * gwy_data_field_fractal_partitioning_dim:
 * @xresult: Log-log fractal data (x values).
 * @yresult: Log-log fractal data (y values).
 * @a: Location to store linear fit constant to.
 * @b: Location to store linear fit slope to.
 *
 * Computes fractal dimension by partitioning method from log-log plot data.
 *
 * The @xresult and @yresult data lines are usually calculated by
 * gwy_data_field_fractal_partitioning().
 *
 * Returns: The fractal dimension.
 **/
gdouble
gwy_data_field_fractal_partitioning_dim(GwyDataLine *xresult,
                                        GwyDataLine *yresult,
                                        gdouble *a,
                                        gdouble *b)
{
    gwy_data_field_fractal_fit(xresult, yresult, a, b);

    return 3.0 - (*a)/2.0;
}

/**
 * gwy_data_field_fractal_psdf_dim:
 * @xresult: Log-log fractal data (x values).
 * @yresult: Log-log fractal data (y values).
 * @a: Location to store linear fit constant to.
 * @b: Location to store linear fit slope to.
 *
 * Computes fractal dimension by spectral density function method from
 * log-log plot data.
 *
 * The @xresult and @yresult data lines are usually calculated by
 * gwy_data_field_fractal_psdf().
 *
 * Returns: The fractal dimension.
 **/
gdouble
gwy_data_field_fractal_psdf_dim(GwyDataLine *xresult, GwyDataLine *yresult,
                                gdouble *a, gdouble *b)
{
    gwy_data_field_fractal_fit(xresult, yresult, a, b);

    return 3.5 + (*a)/2.0;
}

/**
 * gwy_data_field_fractal_fit:
 * @xresult: Log-log fractal data (x values).
 * @yresult: Log-log fractal data (y values).
 * @a: Resulting shift.
 * @b: Resulting direction.
 *
 * Fits fractal dimension from paritioning data.
 *
 * Currently simply fits results from the
 * gwy_data_field_fractal_partitioning and smilar functions
 * by straight line.
 **/
static void
gwy_data_field_fractal_fit(GwyDataLine *xresult, GwyDataLine *yresult,
                           gdouble *a, gdouble *b)
{
    gdouble sx = 0, sxy = 0, sx2 = 0, sy = 0;
    gint i, size;

    size = gwy_data_line_get_res(xresult);
    for (i = 0; i < size; i++) {
        sx += xresult->data[i];
        sx2 += xresult->data[i] * xresult->data[i];
        sy += yresult->data[i];
        sxy += xresult->data[i] * yresult->data[i];
    }
    *a = (sxy - sx * sy/size)/(sx2 - sx * sx/size);
    *b = (sx2 * sy - sx * sxy)/(sx2 * size - sx * sx);
}

/**
 * gaussian_random_number:
 * @rng: A random number generator.
 *
 * Returns a normally distributed deviate with zero mean and unit variance.
 *
 * Returns: A Gaussian random number.
 **/
static gdouble
gaussian_random_number(GRand *rng)
{
    gdouble x, y, w;

    /* to avoid four random number generations for a signle gaussian number,
     * we use g_rand_int() instead of g_rand_double() and use the second
     * gaussian number to add noise to the lower bits */
    do {
        x = -1.0 + 2.0/4294967295.0*g_rand_int(rng);
        y = -1.0 + 2.0/4294967295.0*g_rand_int(rng);
        w = x*x + y*y;
    } while (w >= 1.0 || w == 0);

    w = sqrt(-2.0*log(w)/w);

    return (x + y/4294967295.0)*w;
}

/*
 * data repair tool using succesive random additional midpoint displacement
 * method.
 * Uses fields of size 2^k+1, *vars - y result field of
 * gwy_data_field_fractal_partitioning(), *mask to specify which points to
 * correct and *z data
 */
static gboolean
fractal_correct(GwyDataField *z, GwyDataField *mask, GwyDataLine *vars, gint k)
{
    GRand *rng;
    gint i, j, l, p, ii, jj, pp, n, xres;
    gdouble r, sg, avh;

    rng = g_rand_new();
    avh = gwy_data_field_get_avg(z);

    xres = z->xres;

    for (l = 0; l < k; l++) {
        pp = 1 << l;
        p = 1 << (k-1 - l);
        n = (z->xres + 1) / pp;
        sg = sqrt((4.0*n*n - 6.0*n + 2.0)/((2.0 + G_SQRT2)*n*n
                                           - (4.0 + G_SQRT2)*n + 2.0)
                  * exp(vars->data[k-1 - l]));

        for (i = 0; i < pp; i++)
            for (j = 0; j < pp; j++) {
                ii = (2 * i + 1) * p;
                jj = (2 * j + 1) * p;
                r = sg * gaussian_random_number(rng);
                if (mask->data[ii * xres + jj] != 0) {
                    if (l == 0)
                        z->data[ii * xres + jj] = avh;
                    else
                        z->data[ii * xres + jj]
                        = (z->data[(ii - p) * xres + (jj - p)]
                         + z->data[(ii - p) * xres + (jj + p)]
                         + z->data[(ii + p) * xres + (jj - p)]
                         + z->data[(ii + p) * xres + (jj + p)])/4 + r;
                }
            }

        for (i = 0; i < pp; i++)
            for (j = 0; j < pp; j++) {
                ii = (2 * i + 1) * p;
                jj = (2 * j + 1) * p;
                if ((jj + p) == n - 1) {
                    if ((ii + p) == n - 1) {
                        r = sg * gaussian_random_number(rng);
                        if (mask->data[(ii + p) * xres + (jj + p)] != 0)
                            z->data[(ii + p) * xres + (jj + p)]
                                = z->data[(ii + p) * xres + (jj + p)] + r;
                        r = sg * gaussian_random_number(rng);
                        if (mask->data[(ii + p) * xres + (jj - p)] != 0)
                            z->data[(ii + p) * xres + (jj - p)]
                                = z->data[(ii + p) * xres + (jj - p)] + r;
                        r = sg * gaussian_random_number(rng);
                        if (mask->data[(ii - p) * xres + (jj + p)] != 0)
                            z->data[(ii - p) * xres + (jj + p)]
                                = z->data[(ii - p) * xres + (jj + p)] + r;
                        r = sg * gaussian_random_number(rng);
                        if (mask->data[(ii - p) * xres + (jj - p)] != 0)
                            z->data[(ii - p) * xres + (jj - p)]
                                = z->data[(ii - p) * xres + (jj - p)] + r;
                    }
                    else {
                        r = sg * gaussian_random_number(rng);
                        if (mask->data[(ii - p) * xres + (jj + p)] != 0) {
                            z->data[(ii - p) * xres + (jj + p)]
                                = z->data[(ii - p) * xres + (jj + p)] + r;
                        }
                        r = sg * gaussian_random_number(rng);
                        if (mask->data[(ii - p) * xres + (jj - p)] != 0) {
                            z->data[(ii - p) * xres + (jj - p)]
                                = z->data[(ii - p) * xres + (jj - p)] + r;
                        }
                    }
                }
                else {
                    if ((ii + p) == n - 1) {
                        r = sg * gaussian_random_number(rng);
                        if (mask->data[(ii + p) * xres + (jj - p)] != 0) {
                            z->data[(ii + p) * xres + (jj - p)]
                                = z->data[(ii + p) * xres + (jj - p)] + r;
                        }
                        r = sg * gaussian_random_number(rng);
                        if (mask->data[(ii - p) * xres + (jj - p)] != 0) {
                            z->data[(ii - p) * xres + (jj - p)]
                                = z->data[(ii - p) * xres + (jj - p)] + r;
                        }
                    }
                    else {
                        r = sg * gaussian_random_number(rng);
                        if (mask->data[(ii - p) * xres + (jj - p)] != 0) {
                            z->data[(ii - p) * xres + (jj - p)]
                                = z->data[(ii - p) * xres + (jj - p)] + r;
                        }
                    }
                }
            }

        sg /= G_SQRT2;
        for (i = 0; i < pp; i++)
            for (j = 0; j < pp; j++) {
                ii = (2*i + 1) * p;
                jj = (2*j + 1) * p;
                if (l == 0) {
                    r = sg * gaussian_random_number(rng);
                    if (mask->data[ii * xres + (jj - p)] != 0)
                        z->data[ii * xres + (jj - p)]
                            = (z->data[(ii - p) * xres + (jj - p)]
                               + z->data[(ii + p) * xres + (jj - p)]
                               + z->data[ii * xres + jj])/3 + r;
                    r = sg * gaussian_random_number(rng);
                    if (mask->data[ii * xres + (jj + p)] != 0)
                        z->data[ii * xres + (jj + p)]
                            = (z->data[(ii - p) * xres + (jj + p)]
                               + z->data[(ii + p) * xres + (jj + p)]
                               + z->data[ii * xres + jj])/3 + r;
                    r = sg * gaussian_random_number(rng);
                    if (mask->data[(ii - p) * xres + jj] != 0)
                        z->data[(ii - p) * xres + jj]
                            = (z->data[(ii - p) * xres + (jj - p)]
                               + z->data[(ii - p) * xres + (jj + p)]
                               + z->data[ii * xres + jj])/3 + r;
                    r = sg * gaussian_random_number(rng);
                    if (mask->data[(ii + p) * xres + jj] != 0)
                        z->data[(ii + p) * xres + jj]
                            = (z->data[(ii + p) * xres + (jj - p)]
                               + z->data[(ii + p) * xres + (jj + p)]
                               + z->data[ii * xres + jj])/3 + r;
                }
                else {
                    if ((jj + p) == n - 1) {
                        if ((ii + p) == n - 1) {
                            r = sg * gaussian_random_number(rng);
                            if (mask->data[(ii + p) * xres + jj] != 0)
                                z->data[(ii + p) * xres + jj]
                                    = (z->data[ii * xres + jj]
                                       + z->data[(ii + p) * xres + (jj + p)]
                                       + z->data[(ii + p) * xres + (jj - p)])/3
                                      + r;
                            r = sg * gaussian_random_number(rng);
                            if (mask->data[ii * xres + (jj + p)] != 0)
                                z->data[ii * xres + (jj + p)]
                                    = (z->data[ii * xres + jj]
                                       + z->data[(ii + p) * xres + (jj + p)]
                                       + z->data[(ii - p) * xres + (jj + p)])/3
                                      + r;
                            r = sg * gaussian_random_number(rng);
                            if (mask->data[(ii - p) * xres + jj] != 0)
                                z->data[(ii - p) * xres + jj]
                                    = (z->data[ii * xres + jj]
                                       + z->data[(ii - p) * xres + (jj + p)]
                                       + z->data[(ii - 2 * p) * xres + jj]
                                       + z->data[(ii - p) * xres + (jj - p)])/4
                                      + r;
                        }
                        if ((ii + p) != n && (ii - p) != 1) {
                            r = sg * gaussian_random_number(rng);
                            if (mask->data[ii * xres + (jj + p)] != 0)
                                z->data[ii * xres + (jj + p)]
                                    = (z->data[ii * xres + jj]
                                       + z->data[(ii + p) * xres + (jj + p)]
                                       + z->data[(ii - p) * xres + (jj + p)])/3
                                      + r;
                            r = sg * gaussian_random_number(rng);
                            if (mask->data[(ii - p) * xres + jj] != 0)
                                z->data[(ii - p) * xres + jj]
                                    = (z->data[ii * xres + jj]
                                       + z->data[(ii - p) * xres + (jj + p)]
                                       + z->data[(ii - 2 * p) * xres + jj]
                                       + z->data[(ii - p) * xres + (jj - p)])/4
                                      + r;
                        }
                        if ((ii - p) == 0) {
                            r = sg * gaussian_random_number(rng);
                            if (mask->data[ii * xres + (jj + p)] != 0)
                                z->data[ii * xres + (jj + p)]
                                    = (z->data[ii * xres + jj]
                                       + z->data[(ii - p) * xres + (jj + p)]
                                       + z->data[(ii + p) * xres + (jj + p)])/3
                                      + r;
                            r = sg * gaussian_random_number(rng);
                            if (mask->data[(ii - p) * xres + jj] != 0)
                                z->data[(ii - p) * xres + jj]
                                    = (z->data[ii * xres + jj]
                                       + z->data[(ii - p) * xres + (jj - p)]
                                       + z->data[(ii - p) * xres + (jj + p)])/3
                                      + r;
                        }
                    }
                    if ((jj + p) != (n - 1) && (jj - p) != 0) {
                        if ((ii + p) == n - 1) {
                            r = sg * gaussian_random_number(rng);
                            if (mask->data[(ii + p) * xres + jj] != 0)
                                z->data[(ii + p) * xres + jj]
                                    = (z->data[ii * xres + jj]
                                       + z->data[(ii + p) * xres + (jj - p)]
                                       + z->data[(ii + p) * xres + (jj + p)])/3
                                      + r;
                            r = sg * gaussian_random_number(rng);
                            if (mask->data[ii * xres + (jj + p)] != 0)
                                z->data[ii * xres + (jj + p)]
                                    = (z->data[ii * xres + jj]
                                       + z->data[ii * xres + jj + 2 * p]
                                       + z->data[(ii - p) * xres + (jj + p)]
                                       + z->data[(ii + p) * xres + (jj + p)])/4
                                      + r;
                            r = sg * gaussian_random_number(rng);
                            if (mask->data[(ii - p) * xres + jj] != 0)
                                z->data[(ii - p) * xres + jj]
                                    = (z->data[ii * xres + jj]
                                       + z->data[(ii - 2 * p) * xres + jj]
                                       + z->data[(ii - p) * xres + (jj + p)]
                                       + z->data[(ii - p) * xres + (jj - p)])/4
                                      + r;
                        }
                        if ((ii + p) != n - 1 && (ii - p) != 0) {
                            r = sg * gaussian_random_number(rng);
                            if (mask->data[ii * xres + (jj + p)] != 0)
                                z->data[ii * xres + (jj + p)]
                                    = (z->data[ii * xres + jj]
                                       + z->data[ii * xres + jj + 2 * p]
                                       + z->data[(ii - p) * xres + (jj + p)]
                                       + z->data[(ii + p) * xres + (jj + p)])/4
                                      + r;
                            r = sg * gaussian_random_number(rng);
                            if (mask->data[(ii - p) * xres + jj] != 0)
                                z->data[(ii - p) * xres + jj]
                                    = (z->data[ii * xres + jj]
                                       + z->data[(ii - 2 * p) * xres + jj]
                                       + z->data[(ii - p) * xres + (jj + p)]
                                       + z->data[(ii - p) * xres + (jj - p)])/4
                                      + r;
                        }
                        if ((ii - p) == 0) {
                            r = sg * gaussian_random_number(rng);
                            if (mask->data[ii * xres + (jj + p)] != 0)
                                z->data[ii * xres + (jj + p)]
                                    = (z->data[ii * xres + jj]
                                       + z->data[ii * xres + jj + 2 * p]
                                       + z->data[(ii - p) * xres + (jj + p)]
                                       + z->data[(ii + p) * xres + (jj + p)])/4
                                      + r;
                            r = sg * gaussian_random_number(rng);
                            if (mask->data[(ii - p) * xres + jj] != 0)
                                z->data[(ii - p) * xres + jj]
                                    = (z->data[ii * xres + jj]
                                       + z->data[(ii - p) * xres + (jj + p)]
                                       + z->data[(ii - p) * xres + (jj - p)])/3
                                      + r;
                        }
                    }
                    if ((jj - p) == 0) {
                        if ((ii + p) == n - 1) {
                            r = sg * gaussian_random_number(rng);
                            if (mask->data[(ii + p) * xres + jj] != 0)
                                z->data[(ii + p) * xres + jj]
                                    = (z->data[ii * xres + jj]
                                       + z->data[(ii + p) * xres + (jj - p)]
                                       + z->data[(ii + p) * xres + (jj + p)])/3
                                      + r;
                            r = sg * gaussian_random_number(rng);
                            if (mask->data[ii * xres + (jj + p)] != 0)
                                z->data[ii * xres + (jj + p)]
                                    = (z->data[ii * xres + jj]
                                       + z->data[ii * xres + jj + 2 * p]
                                       + z->data[(ii - p) * xres + (jj + p)]
                                       + z->data[(ii + p) * xres + (jj + p)])/4
                                      + r;
                            r = sg * gaussian_random_number(rng);
                            if (mask->data[(ii - p) * xres + jj] != 0)
                                z->data[(ii - p) * xres + jj]
                                    = (z->data[ii * xres + jj]
                                       + z->data[(ii - 2 * p) * xres + jj]
                                       + z->data[(ii - p) * xres + (jj + p)]
                                       + z->data[(ii - p) * xres + (jj - p)])/4
                                      + r;
                            r = sg * gaussian_random_number(rng);
                            if (mask->data[ii * xres + (jj - p)] != 0)
                                z->data[ii * xres + (jj - p)]
                                    = (z->data[ii * xres + jj]
                                       + z->data[(ii - p) * xres + (jj - p)]
                                       + z->data[(ii + p) * xres + (jj - p)])/3
                                      + r;
                        }
                        if ((ii + p) != n - 1 && (ii - p) != 0) {
                            r = sg * gaussian_random_number(rng);
                            if (mask->data[ii * xres + (jj + p)] != 0)
                                z->data[ii * xres + (jj + p)]
                                    = (z->data[ii * xres + jj]
                                       + z->data[ii * xres + jj + 2 * p]
                                       + z->data[(ii - p) * xres + (jj + p)]
                                       + z->data[(ii + p) * xres + (jj + p)])/4
                                      + r;
                            r = sg * gaussian_random_number(rng);
                            if (mask->data[(ii - p) * xres + jj] != 0)
                                z->data[(ii - p) * xres + jj]
                                    = (z->data[ii * xres + jj]
                                       + z->data[(ii - 2 * p) * xres + jj]
                                       + z->data[(ii - p) * xres + (jj + p)]
                                       + z->data[(ii - p) * xres + (jj - p)])/4
                                      + r;
                            r = sg * gaussian_random_number(rng);
                            if (mask->data[ii * xres + (jj - p)] != 0)
                                z->data[ii * xres + (jj - p)]
                                    = (z->data[ii * xres + jj]
                                       + z->data[(ii - p) * xres + (jj - p)]
                                       + z->data[(ii + p) * xres + (jj - p)])/3
                                      + r;
                        }
                        if ((ii - p) == 0) {
                            r = sg * gaussian_random_number(rng);
                            if (mask->data[ii * xres + (jj + p)] != 0)
                                z->data[ii * xres + (jj + p)]
                                    = (z->data[ii * xres + jj]
                                       + z->data[ii * xres + jj + 2 * p]
                                       + z->data[(ii - p) * xres + (jj + p)]
                                       + z->data[(ii + p) * xres + (jj + p)])/4
                                      + r;
                            r = sg * gaussian_random_number(rng);
                            if (mask->data[(ii - p) * xres + jj] != 0)
                                z->data[(ii - p) * xres + jj]
                                    = (z->data[ii * xres + jj]
                                       + z->data[(ii - p) * xres + (jj + p)]
                                       + z->data[(ii - p) * xres + (jj - p)])/3
                                      + r;
                            r = sg * gaussian_random_number(rng);
                            if (mask->data[ii * xres + (jj - p)] != 0)
                                z->data[ii * xres + (jj - p)]
                                    = (z->data[ii * xres + jj]
                                       + z->data[(ii - p) * xres + (jj - p)]
                                       + z->data[(ii + p) * xres + (jj - p)])/3
                                      + r;
                        }
                    }
                }
            }

        for (i = 0; i < pp; i++)
            for (j = 0; j < pp; j++) {
                ii = (2*i + 1) * p;
                jj = (2*j + 1) * p;
                if ((jj + p) == n - 1) {
                    if ((ii + p) == n - 1) {
                        r = sg * gaussian_random_number(rng);
                        if (mask->data[(ii + p) * xres + (jj + p)] != 0)
                            z->data[(ii + p) * xres + (jj + p)]
                                = z->data[(ii + p) * xres + (jj + p)] + r;
                        r = sg * gaussian_random_number(rng);
                        if (mask->data[(ii + p) * xres + (jj - p)] != 0)
                            z->data[(ii + p) * xres + (jj - p)]
                                = z->data[(ii + p) * xres + (jj - p)] + r;
                        r = sg * gaussian_random_number(rng);
                        if (mask->data[(ii - p) * xres + (jj + p)] != 0)
                            z->data[(ii - p) * xres + (jj + p)]
                                = z->data[(ii - p) * xres + (jj + p)] + r;
                        r = sg * gaussian_random_number(rng);
                        if (mask->data[(ii - p) * xres + (jj - p)] != 0)
                            z->data[(ii - p) * xres + (jj - p)]
                                = z->data[(ii - p) * xres + (jj - p)] + r;
                        r = sg * gaussian_random_number(rng);
                        if (mask->data[ii * xres + jj] != 0)
                            z->data[ii * xres + jj]
                                = z->data[ii * xres + jj] + r;
                    }
                    else {
                        r = sg * gaussian_random_number(rng);
                        if (mask->data[(ii - p) * xres + (jj + p)] != 0)
                            z->data[(ii - p) * xres + (jj + p)]
                                = z->data[(ii - p) * xres + (jj + p)] + r;
                        r = sg * gaussian_random_number(rng);
                        if (mask->data[(ii - p) * xres + (jj - p)] != 0)
                            z->data[(ii - p) * xres + (jj - p)]
                                = z->data[(ii - p) * xres + (jj - p)] + r;
                        r = sg * gaussian_random_number(rng);
                        if (mask->data[ii * xres + jj] != 0)
                            z->data[ii * xres + jj]
                                = z->data[ii * xres + jj] + r;
                    }
                }
                else {
                    if ((ii + p) == n - 1) {
                        r = sg * gaussian_random_number(rng);
                        if (mask->data[(ii + p) * xres + (jj - p)] != 0)
                            z->data[(ii + p) * xres + (jj - p)]
                                = z->data[(ii + p) * xres + (jj - p)] + r;
                        r = sg * gaussian_random_number(rng);
                        if (mask->data[(ii - p) * xres + (jj - p)] != 0)
                            z->data[(ii - p) * xres + (jj - p)]
                                = z->data[(ii - p) * xres + (jj - p)] + r;
                        r = sg * gaussian_random_number(rng);
                        if (mask->data[ii * xres + jj] != 0)
                            z->data[ii * xres + jj]
                                = z->data[ii * xres + jj] + r;
                    }
                    else {
                        r = sg * gaussian_random_number(rng);
                        if (mask->data[(ii - p) * xres + (jj - p)] != 0)
                            z->data[(ii - p) * xres + (jj - p)]
                                = z->data[(ii - p) * xres + (jj - p)] + r;
                        r = sg * gaussian_random_number(rng);
                        if (mask->data[ii * xres + jj] != 0)
                            z->data[ii * xres + jj]
                                = z->data[ii * xres + jj] + r;
                    }
                }
            }
    }

    g_rand_free(rng);
    gwy_data_field_invalidate(z);

    return TRUE;
}


/**
 * gwy_data_field_fractal_correction:
 * @data_field: A data field.
 * @mask_field: Mask of places to be corrected.
 * @interpolation: Interpolation type.
 *
 * Replaces data under mask with interpolated values using fractal
 * interpolation.
 **/
void
gwy_data_field_fractal_correction(GwyDataField *data_field,
                                  GwyDataField *mask_field,
                                  GwyInterpolationType interpolation)
{
    GwyDataField *buffer, *maskbuffer;
    GwyDataLine *xresult, *yresult;

    gint dimexp;
    gint xnewres;
    gint i;

    dimexp = (gint)floor(log((gdouble)data_field->xres)/G_LN2 + 0.5);
    xnewres = (1 << dimexp) + 1;

    buffer = gwy_data_field_duplicate(data_field);
    maskbuffer = gwy_data_field_duplicate(mask_field);
    gwy_data_field_resample(buffer, xnewres, xnewres, interpolation);
    gwy_data_field_resample(maskbuffer, xnewres, xnewres, interpolation);

    xresult = gwy_data_line_new(1, 1, FALSE);
    yresult = gwy_data_line_new(1, 1, FALSE);

    fractal_partitioning_nomask(data_field, mask_field,
                                xresult, yresult, interpolation);

    if (fractal_correct(buffer, maskbuffer, yresult, dimexp)) {
        gwy_data_field_resample(buffer, data_field->xres, data_field->yres,
                                interpolation);
        for (i = 0; i < (data_field->xres * data_field->yres); i++) {
            if (mask_field->data[i] != 0)
                data_field->data[i] = buffer->data[i];
        }
    }

    g_object_unref(buffer);
    g_object_unref(maskbuffer);
    g_object_unref(xresult);
    g_object_unref(yresult);
}

/************************** Documentation ****************************/

/**
 * SECTION:fractals
 * @title: fractals
 * @short_description: Fractal dimension calculation, fractal interpolation
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
