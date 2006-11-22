/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 */

#include "config.h"
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/interpolation.h>


static const gdouble synth_func_values_bspline3[] = {
    1.0/6.0, 2.0/3.0, 1.0/6.0,
};

static const gdouble synth_func_values_omoms3[] = {
    4.0/21.0, 13.0/21.0, 4.0/21.0,
};


static inline void
gwy_interpolation_get_weights(gdouble x,
                              GwyInterpolationType interpolation,
                              gdouble *w)
{
    g_return_if_fail(x >= 0.0 && x <= 1.0);

    switch (interpolation) {
        case GWY_INTERPOLATION_NONE:
        /* Don't really care. */
        break;

        /* Silently use first order B-spline instead of NN for symmetry */
        case GWY_INTERPOLATION_ROUND:
        if (x < 0.5) {
            w[0] = 1.0;
            w[1] = 0.0;
        }
        else if (x > 0.5) {
            w[0] = 0.0;
            w[1] = 1.0;
        }
        else
            w[0] = w[1] = 0.5;
        break;

        case GWY_INTERPOLATION_LINEAR:
        w[0] = 1.0 - x;
        w[1] = x;
        break;

        case GWY_INTERPOLATION_KEY:
        w[0] = (-0.5 + (1.0 - x/2.0)*x)*x;
        w[1] = 1.0 + (-2.5 + 1.5*x)*x*x;
        w[2] = (0.5 + (2.0 - 1.5*x)*x)*x;
        w[3] = (-0.5 + x/2.0)*x*x;
        break;

        case GWY_INTERPOLATION_BSPLINE:
        w[0] = (1.0 - x)*(1.0 - x)*(1.0 - x)/6.0;
        w[1] = 2.0/3.0 - x*x*(1.0 - x/2.0);
        w[2] = (1.0/3.0 + x*(1.0 + x*(1.0 - x)))/2.0;
        w[3] = x*x*x/6.0;
        break;

        case GWY_INTERPOLATION_OMOMS:
        w[0] = 4.0/21.0 - x*(11.0/21.0 - x*(1.0/2.0 - x/6.0));
        w[1] = 13.0/21.0 - x*(1.0/14.0 + x*(1.0 - x/2.0));
        w[2] = 4.0/21.0 + x*(3.0/7.0 + x*(1.0 - x)/2.0);
        w[3] = x*(1.0/7.0 + x*x)/6.0;
        break;

        case GWY_INTERPOLATION_NNA:
        w[0] = x + 1.0;
        w[1] = x;
        w[2] = 1.0 - x;
        w[3] = 2.0 - x;
        w[0] = 1.0/(w[0]*w[0]);
        w[0] *= w[0];
        w[1] = 1.0/(w[1]*w[1]);
        w[1] *= w[1];
        w[2] = 1.0/(w[2]*w[2]);
        w[2] *= w[2];
        w[3] = 1.0/(w[3]*w[3]);
        w[3] *= w[3];
        x = w[0] + w[1] + w[2] + w[3];
        w[0] /= x;
        w[1] /= x;
        w[2] /= x;
        w[3] /= x;
        break;

        case GWY_INTERPOLATION_SCHAUM:
        w[0] = -x*(x - 1.0)*(x - 2.0)/6.0;
        w[1] = (x*x - 1.0)*(x - 2.0)/2.0;
        w[2] = -x*(x + 1.0)*(x - 2.0)/2.0;
        w[3] = x*(x*x - 1.0)/6.0;
        break;

        default:
        g_return_if_reached();
        break;
    }
}
/**
 * gwy_interpolation_get_dval:
 * @x: requested value coordinate
 * @x1_: x coordinate of first value
 * @y1_: y coordinate of first value
 * @x2_: x coordinate of second value
 * @y2_: y coordinate of second value
 * @interpolation: interpolation type
 *
 * This function uses two-point interpolation
 * methods to get interpolated value between
 * two arbitrary data points.
 *
 * Returns: interpolated value
 **/
gdouble
gwy_interpolation_get_dval(gdouble x,
                           gdouble x1_, gdouble y1_,
                           gdouble x2_, gdouble y2_,
                           GwyInterpolationType interpolation)
{
    if (x1_ > x2_) {
        GWY_SWAP(gdouble, x1_, x2_);
        GWY_SWAP(gdouble, y1_, y2_);
    }

    switch (interpolation) {
        case GWY_INTERPOLATION_ROUND:
        if ((x - x1_) < (x2_ - x))
            return y1_;
        else
            return y2_;
        break;


        case GWY_INTERPOLATION_LINEAR:
        return y1_ + (x - x1_)/(x2_ - x1_)*(y2_ - y1_);
        break;

        default:
        g_warning("Interpolation not implemented yet.\n");
        break;
    }
    return 0.0;
}

/**
 * gwy_interpolation_get_dval_of_equidists:
 * @x: Noninteger part of requested x, that is a number from interval [0,1).
 * @data: Array of 4 values to interpolate between (see below).
 * @interpolation: Interpolation type to use.
 *
 * Computes interpolated value from 2 or 4 equidistant values.
 *
 * For %GWY_INTERPOLATION_NONE no @data value is actually used, and zero is
 * returned.
 *
 * For %GWY_INTERPOLATION_ROUND or %GWY_INTERPOLATION_LINEAR
 * it is enough to set middle two @data values, that to use @data in format
 * {0, data[i], data[i+1], 0} and function computes value at data[i+x]
 * (the outer values are not used).
 *
 * For four value interpolations you have to prepare @data as
 * {data[i-1], data[i], data[i+1], data[i+2]} and function again
 * returns value at data[i+x].
 *
 * Returns: Interpolated value.
 **/
gdouble
gwy_interpolation_get_dval_of_equidists(gdouble x,
                                        gdouble *data,
                                        GwyInterpolationType interpolation)
{
    gint l;
    gdouble w[4];
    gdouble rest;

    x += 1.0;
    l = floor(x);
    rest = x - (gdouble)l;

    g_return_val_if_fail(x >= 1 && x < 2, 0.0);

    if (rest == 0)
        return data[l];

    switch (interpolation) {
        case GWY_INTERPOLATION_NONE:
        return 0.0;
        break;

        case GWY_INTERPOLATION_ROUND:
        case GWY_INTERPOLATION_LINEAR:
        gwy_interpolation_get_weights(rest, interpolation, w);
        return w[0]*data[l] + w[1]*data[l + 1];
        break;

        /* One cannot do B-spline and o-MOMS this way.  Read e.g.
         * `Interpolation Revisited' by Philippe Thevenaz for explanation.
         * Replace them with Key. */
        case GWY_INTERPOLATION_BSPLINE:
        case GWY_INTERPOLATION_OMOMS:
        interpolation = GWY_INTERPOLATION_KEY;
        case GWY_INTERPOLATION_KEY:
        case GWY_INTERPOLATION_NNA:
        case GWY_INTERPOLATION_SCHAUM:
        gwy_interpolation_get_weights(rest, interpolation, w);
        return w[0]*data[l - 1] + w[1]*data[l]
               + w[2]*data[l + 1] + w[3]*data[l + 2];
        break;

        default:
        g_return_val_if_reached(0.0);
        break;
    }
}

/**
 * deconvolve3_rows:
 * @width: The number of items in @data.
 * @height: The number of rows in @data.
 * @rowstride: The total row length (including width).
 * @data: An array to deconvolve of size @width.
 * @buffer: Scratch space of at least @width items.
 * @a: The central convolution filter element.
 * @b: The side convolution filter element.
 *
 * Undoes the effect of mirror-extended (@b, @a, @b) vertical convolution
 * filter on a two-dimensional array.  It can be also used for one-dimensional
 * arrays, pass @height=1, @rowstride=@width then.
 *
 * This function acts on a two-dimensional data array, accessing it at linearly
 * as possible for CPU cache utilization reasons.
 **/
static void
deconvolve3_rows(gint width,
                 gint height,
                 gint rowstride,
                 gdouble *data,
                 gdouble *buffer,
                 gdouble a,
                 gdouble b)
{
    gdouble *row;
    gdouble q, b2;
    gint i, j;

    g_return_if_fail(height < 2 || rowstride >= width);
    b2 = 2.0*b;
    g_return_if_fail(b2 < a);

    if (!height || !width)
        return;

    if (width == 1) {
        for (i = 0; i < height; i++)
            data[i*rowstride] /= (a + b2);
        return;
    }
    if (width == 2) {
        q = a*a - b2*b2;
        for (i = 0; i < height; i++) {
            row = data + i*rowstride;
            buffer[0] = a*row[0] - b2*row[1];
            row[1] = a*row[1] - b2*row[0];
            row[0] = buffer[0];
        }
        return;
    }

    /* Special-case first item */
    buffer[0] = a/2.0;
    data[0] /= 2.0;
    /* Inner items */
    for (j = 1; j < width-1; j++) {
        q = b/buffer[j-1];
        buffer[j] = a - q*b;
        data[j] -= q*data[j-1];
    }
    /* Special-case last item */
    q = b2/buffer[j-1];
    buffer[j] = a - q*b;
    data[j] -= q*data[j-1];
    /* Go back */
    data[j] /= buffer[j];
    do {
        j--;
        data[j] = (data[j] - b*data[j+1])/buffer[j];
    } while (j > 0);

    /* Remaining rows */
    for (i = 1; i < height; i++) {
        row = data + i*rowstride;
        /* Forward */
        row[0] /= 2.0;
        for (j = 1; j < width-1; j++)
            row[j] -= b*row[j-1]/buffer[j-1];
        row[j] -= b2*row[j-1]/buffer[j-1];
        /* Back */
        row[j] /= buffer[j];
        do {
            j--;
            row[j] = (row[j] - b*row[j+1])/buffer[j];
        } while (j > 0);
    }
}

/**
 * deconvolve3_columns:
 * @width: The number of columns in @data.
 * @height: The number of rows in @data.
 * @rowstride: The total row length (including width).
 * @data: A two-dimensional array of size @width*height to deconvolve.
 * @buffer: Scratch space of at least @height items.
 * @a: The central convolution filter element.
 * @b: The side convolution filter element.
 *
 * Undoes the effect of mirror-extended (@b, @a, @b) vertical convolution
 * filter on a two-dimensional array.
 *
 * This function acts on a two-dimensional data array, accessing it at linearly
 * as possible for CPU cache utilization reasons.
 **/
static void
deconvolve3_columns(gint width,
                    gint height,
                    gint rowstride,
                    gdouble *data,
                    gdouble *buffer,
                    gdouble a,
                    gdouble b)
{
    gdouble *row;
    gdouble q, b2;
    gint i, j;

    g_return_if_fail(height < 2 || rowstride >= width);
    b2 = 2.0*b;
    g_return_if_fail(b2 < a);

    if (!height || !width)
        return;

    if (height == 1) {
        for (j = 0; j < width; j++)
            data[j] /= (a + b2);
        return;
    }
    if (height == 2) {
        q = a*a - b2*b2;
        for (j = 0; j < width; j++) {
            buffer[0] = a*data[j] - b2*data[rowstride + j];
            data[rowstride + j] = a*data[rowstride + j] - b2*data[j];
            data[j] = buffer[0];
        }
        return;
    }

    /* Special-case first row */
    buffer[0] = a/2.0;
    for (j = 0; j < width; j++)
        data[j] /= 2.0;
    /* Inner rows */
    for (i = 1; i < height-1; i++) {
        q = b/buffer[i-1];
        buffer[i] = a - q*b;
        row = data + (i - 1)*rowstride;
        for (j = 0; j < width; j++)
            row[rowstride + j] -= q*row[j];
    }
    /* Special-case last row */
    q = b2/buffer[i-1];
    buffer[i] = a - q*b;
    row = data + (i - 1)*rowstride;
    for (j = 0; j < width; j++)
        row[rowstride + j] -= q*row[j];
    /* Go back */
    row += rowstride;
    for (j = 0; j < width; j++)
        row[j] /= buffer[i];
    do {
        i--;
        row = data + i*rowstride;
        for (j = 0; j < width; j++)
            row[j] = (row[j] - b*row[rowstride + j])/buffer[i];
    } while (i > 0);
}

/**
 * gwy_interpolation_has_interpolating_basis:
 * @interpolation: Interpolation type.
 *
 * Obtains the interpolating basis property of an inteprolation type.
 *
 * Interpolation types with inteprolating basis directly use data values
 * for interpolation.  For these types gwy_interpolation_resolve_coeffs_1d()
 * and gwy_interpolation_resolve_coeffs_2d() are no-op.
 *
 * Generalized interpolation types (with non-interpolation basis) require to
 * preprocess the data values to obtain interpolation coefficients first.  On
 * the ohter hand they typically offer much higher interpolation quality.
 *
 * Returns: %TRUE if the inteprolation type has interpolating basis,
 *          %FALSE if data values cannot be directly used for interpolation
 *          of this type.
 **/
gboolean
gwy_interpolation_has_interpolating_basis(GwyInterpolationType interpolation)
{
    switch (interpolation) {
        case GWY_INTERPOLATION_NONE:
        case GWY_INTERPOLATION_ROUND:
        case GWY_INTERPOLATION_LINEAR:
        case GWY_INTERPOLATION_KEY:
        case GWY_INTERPOLATION_NNA:
        case GWY_INTERPOLATION_SCHAUM:
        return TRUE;
        break;

        case GWY_INTERPOLATION_BSPLINE:
        case GWY_INTERPOLATION_OMOMS:
        return FALSE;
        break;

        default:
        g_return_val_if_reached(FALSE);
        break;
    }
}

/**
 * gwy_interpolation_resolve_coeffs_1d:
 * @n: The number of points in @data.
 * @data: An array of data values.  It will be rewritten with the coefficients.
 * @interpolation: Interpolation type to prepare @data for.
 *
 * Transforms data values in a one-dimensional array to interpolation
 * coefficients.
 *
 * This function is no-op for interpolation types with finite-support
 * interpolating function.  Therefore you can also omit it and use the data
 * array directly for these interpolation types.
 *
 * Since: 2.2
 **/
void
gwy_interpolation_resolve_coeffs_1d(gint n,
                                    gdouble *data,
                                    GwyInterpolationType interpolation)
{
    gdouble *buffer;
    gdouble a, b;

    switch (interpolation) {
        case GWY_INTERPOLATION_NONE:
        case GWY_INTERPOLATION_ROUND:
        case GWY_INTERPOLATION_LINEAR:
        case GWY_INTERPOLATION_KEY:
        case GWY_INTERPOLATION_NNA:
        case GWY_INTERPOLATION_SCHAUM:
        return;

        case GWY_INTERPOLATION_BSPLINE:
        a = synth_func_values_bspline3[1];
        b = synth_func_values_bspline3[0];
        break;

        case GWY_INTERPOLATION_OMOMS:
        a = synth_func_values_omoms3[1];
        b = synth_func_values_omoms3[0];
        break;

        default:
        g_return_if_reached();
        break;
    }

    buffer = g_new(gdouble, n);
    deconvolve3_rows(n, 1, n, data, buffer, a, b);
    g_free(buffer);
}

/**
 * gwy_interpolation_resolve_coeffs_2d:
 * @width: The number of columns in @data.
 * @height: The number of rows in @data.
 * @rowstride: The total row length (including @width).
 * @data: An array of data values.  It will be rewritten with the coefficients.
 * @interpolation: Interpolation type to prepare @data for.
 *
 * Transforms data values in a two-dimensional array to interpolation
 * coefficients.
 *
 * This function is no-op for interpolation types with finite-support
 * interpolating function.  Therefore you can also omit it and use the data
 * array directly for these interpolation types.
 *
 * Since: 2.2
 **/
void
gwy_interpolation_resolve_coeffs_2d(gint width,
                                    gint height,
                                    gint rowstride,
                                    gdouble *data,
                                    GwyInterpolationType interpolation)
{
    gdouble *buffer;
    gdouble a, b;

    switch (interpolation) {
        case GWY_INTERPOLATION_NONE:
        case GWY_INTERPOLATION_ROUND:
        case GWY_INTERPOLATION_LINEAR:
        case GWY_INTERPOLATION_KEY:
        case GWY_INTERPOLATION_NNA:
        case GWY_INTERPOLATION_SCHAUM:
        return;

        case GWY_INTERPOLATION_BSPLINE:
        a = synth_func_values_bspline3[1];
        b = synth_func_values_bspline3[0];
        break;

        case GWY_INTERPOLATION_OMOMS:
        a = synth_func_values_omoms3[1];
        b = synth_func_values_omoms3[0];
        break;

        default:
        g_return_if_reached();
        break;
    }

    buffer = g_new(gdouble, MAX(width, height));
    deconvolve3_rows(width, height, rowstride, data, buffer, a, b);
    deconvolve3_columns(width, height, rowstride, data, buffer, a, b);
    g_free(buffer);
}

/************************** Documentation ****************************/

/**
 * SECTION:interpolation
 * @title: interpolation
 * @short_description: General interpolation functions
 *
 * Data interpolation is usually pixel-like in Gwyddion, not function-like.
 * That means the contribution of individual data saples is preserved on
 * scaling.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
