/*
 *  $Id$
 *  Copyright (C) 2005,2009-2010,2013 David Nečas (Yeti).
 *  Copyright (C) 2003 Petr Klapetek.
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

#include <string.h>
#include "libgwy/math.h"
#include "libgwy/interpolation.h"

enum { SUPPORT_LENGTH_MAX = 6 };

static const gdouble synth_func_values_bspline4[] = {
    2.0/3.0, 1.0/6.0,
};

static const gdouble synth_func_values_omoms4[] = {
    13.0/21.0, 4.0/21.0,
};

static inline void
gwy_interpolation_get_weights(gdouble x,
                              GwyInterpolation interpolation,
                              gdouble *w)
{
    gdouble t, u, v;

    g_return_if_fail(x >= 0.0 && x <= 1.0);

    switch (interpolation) {
        /* Silently use first order B-spline instead of NN for symmetry */
        case GWY_INTERPOLATION_BSPLINE1:
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

        case GWY_INTERPOLATION_BSPLINE2:
        w[0] = 1.0 - x;
        w[1] = x;
        break;

        case GWY_INTERPOLATION_KEYS:
        w[0] = (-0.5 + (1.0 - x/2.0)*x)*x;
        w[1] = 1.0 + (-2.5 + 1.5*x)*x*x;
        w[2] = (0.5 + (2.0 - 1.5*x)*x)*x;
        w[3] = (-0.5 + x/2.0)*x*x;
        break;

        case GWY_INTERPOLATION_BSPLINE4:
        t = 1.0 - x;
        w[0] = t*t*t/6.0;
        w[1] = 2.0/3.0 - x*x*(1.0 - x/2.0);
        w[2] = (1.0/3.0 + x*(1.0 + x*t))/2.0;
        w[3] = x*x*x/6.0;
        break;

        case GWY_INTERPOLATION_OMOMS4:
        w[0] = 4.0/21.0 + (-11.0/21.0 + (0.5 - x/6.0)*x)*x;
        w[1] = 13.0/21.0 + (1.0/14.0 + (-1.0 + x/2.0)*x)*x;
        w[2] = 4.0/21.0 + (3.0/7.0 + (0.5 - x/2.0)*x)*x;
        w[3] = (1.0/42.0 + x*x/6.0)*x;
        break;

        case GWY_INTERPOLATION_NNA:
        if (x == 0.0) {
            w[0] = w[2] = w[3] = 0.0;
            w[1] = 1.0;
        }
        else if (x == 1.0) {
            w[0] = w[1] = w[3] = 0.0;
            w[2] = 1.0;
        }
        else {
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
        }
        break;

        case GWY_INTERPOLATION_SCHAUM4:
        t = 1.0 - x;
        u = 1.0 + x;
        v = 2.0 - x;
        w[0] = -x*t*v/6.0;
        w[1] = t*u*v/2.0;
        w[2] = x*u*v/2.0;
        w[3] = -x*t*u/6.0;
        break;

        case GWY_INTERPOLATION_BSPLINE6:
        t = 1.0 - x;
        w[0] = t*t*t*t*t/120.0;
        w[1] = 13.0/60.0 - x*(5.0/12.0 - x*(1.0/6.0 + x*(1.0/6.0 - x*(1.0/6.0 - x/24.0))));
        w[2] = 11.0/20.0 - x*x*(1.0/2.0 - x*x*(1.0/4.0 - x/12.0));
        w[3] = 13.0/60.0 + x*(5.0/12.0 + x*(1.0/6.0 - x*(1.0/6.0 + x*(1.0/6.0 - 1.0/12.0*x))));
        w[4] = 1.0/120.0 + x*(1.0/24.0 + x*(1.0/12.0 + x*(1.0/12.0 + x*(1.0/24.0 - x/24.0))));
        w[5] = x*x*x*x*x/120.0;
        break;

        default:
        g_return_if_reached();
        break;
    }
}

/**
 * gwy_interpolate_1d:
 * @x: Position in interval [0,1) to get value at.
 * @coeff: Array of support-length size with interpolation coefficients
 *         (that are equal to data values for an interpolating basis).
 * @interpolation: Interpolation type to use.
 *
 * Interpolates a signle data point in one dimension.
 *
 * The interpolation basis support size can be obtained generically with
 * gwy_interpolation_get_support_size().
 *
 * Returns: Interpolated value.
 **/
gdouble
gwy_interpolate_1d(gdouble x,
                   const gdouble *coeff,
                   GwyInterpolation interpolation)
{
    g_return_val_if_fail(x >= 0.0 && x <= 1.0, 0.0);
    guint suplen = gwy_interpolation_get_support_size(interpolation);
    g_return_val_if_fail(suplen > 0, 0.0);

    gdouble w[SUPPORT_LENGTH_MAX];
    gwy_interpolation_get_weights(x, interpolation, w);

    gdouble v = 0.0;
    for (guint i = 0; i < suplen; i++)
        v += w[i]*coeff[i];

    return v;
}

/**
 * gwy_interpolate_2d:
 * @x: X-position in interval [0,1) to get value at.
 * @y: Y-position in interval [0,1) to get value at.
 * @rowstride: Row stride of @coeff.
 * @coeff: Array of support-length-squared size with interpolation coefficients
 *         (that are equal to data values for an interpolating basis).
 * @interpolation: Interpolation type to use.
 *
 * Interpolates a signle data point in two dimensions.
 *
 * Returns: Interpolated value.
 **/
gdouble
gwy_interpolate_2d(gdouble x,
                   gdouble y,
                   guint rowstride,
                   const gdouble *coeff,
                   GwyInterpolation interpolation)
{
    gdouble wx[SUPPORT_LENGTH_MAX], wy[SUPPORT_LENGTH_MAX];
    gint i, j, suplen;
    gdouble v, vx;

    g_return_val_if_fail(x >= 0.0 && x <= 1.0 && y >= 0.0 && y <= 1.0, 0.0);
    suplen = gwy_interpolation_get_support_size(interpolation);
    g_return_val_if_fail(suplen > 0, 0.0);
    gwy_interpolation_get_weights(x, interpolation, wx);
    gwy_interpolation_get_weights(y, interpolation, wy);

    v = 0.0;
    for (i = 0; i < suplen; i++) {
        vx = 0.0;
        for (j = 0; j < suplen; j++)
            vx += coeff[i*rowstride + j]*wx[j];
        v += wy[i]*vx;
    }

    return v;
}

/**
 * deconvolve3_rows:
 * @width: Number of items in @data.
 * @height: Number of rows in @data.
 * @rowstride: Total row length (including width).
 * @data: An array to deconvolve of size @width.
 * @buffer: Scratch space of at least @width items.
 * @a: Central convolution filter element.
 * @b: Side convolution filter element.
 *
 * Undoes the effect of mirror-extended with border value repeated (@b, @a, @b)
 * horizontal convolution filter on a two-dimensional array.  It can be also
 * used for one-dimensional arrays, pass @height=1, @rowstride=@width then.
 *
 * This function acts on a two-dimensional data array, accessing it as linearly
 * as possible for CPU cache utilization reasons.
 **/
static void
deconvolve3_rows(guint width,
                 guint height,
                 guint rowstride,
                 gdouble *data,
                 gdouble *buffer,
                 gdouble a,
                 gdouble b)
{
    g_return_if_fail(height < 2 || rowstride >= width);
    g_return_if_fail(2.0*b < a);

    if (!height || !width)
        return;

    gdouble q, *row;
    guint j;

    if (width == 1) {
        q = a + 2.0*b;
        for (guint i = 0; i < height; i++)
            data[i*rowstride] /= q;
        return;
    }
    if (width == 2) {
        q = a*(a + 2.0*b);
        for (guint i = 0; i < height; i++) {
            row = data + i*rowstride;
            buffer[0] = (a + b)/q*row[0] - b/q*row[1];
            row[1] = (a + b)/q*row[1] - b/q*row[0];
            row[0] = buffer[0];
        }
        return;
    }

    /* Special-case first item */
    buffer[0] = a + b;
    /* Inner items */
    for (j = 1; j < width-1; j++) {
        q = b/buffer[j-1];
        buffer[j] = a - q*b;
        data[j] -= q*data[j-1];
    }
    /* Special-case last item */
    q = b/buffer[j-1];
    buffer[j] = a + b*(1.0 - q);
    data[j] -= q*data[j-1];
    /* Go back */
    data[j] /= buffer[j];
    do {
        j--;
        data[j] = (data[j] - b*data[j+1])/buffer[j];
    } while (j > 0);

    /* Remaining rows */
    for (guint i = 1; i < height; i++) {
        row = data + i*rowstride;
        /* Forward */
        for (j = 1; j < width-1; j++)
            row[j] -= b*row[j-1]/buffer[j-1];
        row[j] -= b*row[j-1]/buffer[j-1];
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
 * @width: Number of columns in @data.
 * @height: Number of rows in @data.
 * @rowstride: Total row length (including width).
 * @data: A two-dimensional array of size @width*height to deconvolve.
 * @buffer: Scratch space of at least @height items.
 * @a: Central convolution filter element.
 * @b: Side convolution filter element.
 *
 * Undoes the effect of mirror-extended with border value repeated (@b, @a, @b)
 * vertical convolution filter on a two-dimensional array.
 *
 * This function acts on a two-dimensional data array, accessing it as linearly
 * as possible for CPU cache utilization reasons.
 **/
static void
deconvolve3_columns(guint width,
                    guint height,
                    guint rowstride,
                    gdouble *data,
                    gdouble *buffer,
                    gdouble a,
                    gdouble b)
{
    g_return_if_fail(height < 2 || rowstride >= width);
    g_return_if_fail(2.0*b < a);

    if (!height || !width)
        return;

    gdouble q, *row;
    guint i;

    if (height == 1) {
        q = a + 2.0*b;
        for (guint j = 0; j < width; j++)
            data[j] /= q;
        return;
    }
    if (height == 2) {
        q = a*(a + 2.0*b);
        for (guint j = 0; j < width; j++) {
            buffer[0] = (a + b)/q*data[j] - b/q*data[rowstride + j];
            data[rowstride + j] = (a + b)/q*data[rowstride + j] - b/q*data[j];
            data[j] = buffer[0];
        }
        return;
    }

    /* Special-case first row */
    buffer[0] = a + b;
    /* Inner rows */
    for (i = 1; i < height-1; i++) {
        q = b/buffer[i-1];
        buffer[i] = a - q*b;
        row = data + (i - 1)*rowstride;
        for (guint j = 0; j < width; j++)
            row[rowstride + j] -= q*row[j];
    }
    /* Special-case last row */
    q = b/buffer[i-1];
    buffer[i] = a + b*(1.0 - q);
    row = data + (i - 1)*rowstride;
    for (guint j = 0; j < width; j++)
        row[rowstride + j] -= q*row[j];
    /* Go back */
    row += rowstride;
    for (guint j = 0; j < width; j++)
        row[j] /= buffer[i];
    do {
        i--;
        row = data + i*rowstride;
        for (guint j = 0; j < width; j++)
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
 * Generalised interpolation types (with non-interpolation basis) require to
 * preprocess the data values to obtain interpolation coefficients first.  On
 * the ohter hand they typically offer much higher interpolation quality.
 *
 * Returns: %TRUE if the inteprolation type has interpolating basis,
 *          %FALSE if data values cannot be directly used for interpolation
 *          of this type.
 **/
gboolean
gwy_interpolation_has_interpolating_basis(GwyInterpolation interpolation)
{
    switch (interpolation) {
        case GWY_INTERPOLATION_BSPLINE1:
        case GWY_INTERPOLATION_BSPLINE2:
        case GWY_INTERPOLATION_KEYS:
        case GWY_INTERPOLATION_NNA:
        case GWY_INTERPOLATION_SCHAUM4:
        return TRUE;
        break;

        case GWY_INTERPOLATION_BSPLINE4:
        case GWY_INTERPOLATION_OMOMS4:
        case GWY_INTERPOLATION_BSPLINE6:
        return FALSE;
        break;

        default:
        g_return_val_if_reached(FALSE);
        break;
    }
}

/**
 * gwy_interpolation_get_support_size:
 * @interpolation: Interpolation type.
 *
 * Obtains the basis support size for an interpolation type.
 *
 * Returns: The length of the support interval of the interpolation basis.
 **/
guint
gwy_interpolation_get_support_size(GwyInterpolation interpolation)
{
    switch (interpolation) {
        case GWY_INTERPOLATION_BSPLINE1:
        case GWY_INTERPOLATION_BSPLINE2:
        return 2;
        break;

        case GWY_INTERPOLATION_KEYS:
        case GWY_INTERPOLATION_BSPLINE4:
        case GWY_INTERPOLATION_OMOMS4:
        case GWY_INTERPOLATION_NNA:
        case GWY_INTERPOLATION_SCHAUM4:
        return 4;
        break;

        case GWY_INTERPOLATION_BSPLINE6:
        return 6;
        break;

        default:
        g_return_val_if_reached(0);
        break;
    }
}

/**
 * gwy_interpolation_resolve_coeffs_1d:
 * @data: (array length=n) (inout):
 *        An array of data values.  It will be rewritten with the coefficients.
 * @n: Number of points in @data.
 * @interpolation: Interpolation type to prepare @data for.
 *
 * Transforms data values in a one-dimensional array to interpolation
 * coefficients.
 *
 * This function is no-op for interpolation types with finite-support
 * interpolating function.  Therefore you can also omit it and use the data
 * array directly for these interpolation types.
 **/
void
gwy_interpolation_resolve_coeffs_1d(gdouble *data,
                                    guint n,
                                    GwyInterpolation interpolation)
{
    if (gwy_interpolation_has_interpolating_basis(interpolation))
        return;

    gdouble *buffer = g_slice_alloc(sizeof(gdouble)*n);

    if (interpolation == GWY_INTERPOLATION_BSPLINE4) {
        const double a = synth_func_values_bspline4[0];
        const double b = synth_func_values_bspline4[1];
        deconvolve3_rows(n, 1, n, data, buffer, a, b);
    }
    else if (interpolation == GWY_INTERPOLATION_OMOMS4) {
        const double a = synth_func_values_omoms4[0];
        const double b = synth_func_values_omoms4[1];
        deconvolve3_rows(n, 1, n, data, buffer, a, b);
    }
    else if (interpolation == GWY_INTERPOLATION_BSPLINE6) {
        const double a = synth_func_values_bspline4[0];
        const double b = synth_func_values_bspline4[1];
        deconvolve3_rows(n, 1, n, data, buffer, a, b);
        deconvolve3_rows(n, 1, n, data, buffer, a, b);
    }
    else {
        g_critical("Unknown interpolation type %u\n", interpolation);
    }

    g_slice_free1(sizeof(gdouble)*n, buffer);
}

/**
 * gwy_interpolation_resolve_coeffs_2d:
 * @data: (inout):
 *        An array of data values.  It will be rewritten with the coefficients.
 * @width: Number of columns in @data.
 * @height: Number of rows in @data.
 * @rowstride: Total row length (including @width).
 * @interpolation: Interpolation type to prepare @data for.
 *
 * Transforms data values in a two-dimensional array to interpolation
 * coefficients.
 *
 * This function is no-op for interpolation types with interpolating-basis
 * interpolating function.  Therefore you can also omit it and use the data
 * array directly for these interpolation types.
 **/
void
gwy_interpolation_resolve_coeffs_2d(gdouble *data,
                                    guint width,
                                    guint height,
                                    guint rowstride,
                                    GwyInterpolation interpolation)
{
    if (interpolation == GWY_INTERPOLATION_BSPLINE1
        || interpolation == GWY_INTERPOLATION_BSPLINE2
        || interpolation == GWY_INTERPOLATION_KEYS
        || interpolation == GWY_INTERPOLATION_NNA
        || interpolation == GWY_INTERPOLATION_SCHAUM4)
        return;

    const guint n = MAX(width, height);
    gdouble *buffer = g_slice_alloc(sizeof(gdouble)*n);

    if (interpolation == GWY_INTERPOLATION_BSPLINE4) {
        const double a = synth_func_values_bspline4[0];
        const double b = synth_func_values_bspline4[1];
        deconvolve3_rows(width, height, rowstride, data, buffer, a, b);
        deconvolve3_columns(width, height, rowstride, data, buffer, a, b);
    }
    else if (interpolation == GWY_INTERPOLATION_OMOMS4) {
        const double a = synth_func_values_omoms4[0];
        const double b = synth_func_values_omoms4[1];
        deconvolve3_rows(width, height, rowstride, data, buffer, a, b);
        deconvolve3_columns(width, height, rowstride, data, buffer, a, b);
    }
    else if (interpolation == GWY_INTERPOLATION_BSPLINE6) {
        const double a = synth_func_values_bspline4[0];
        const double b = synth_func_values_bspline4[1];
        deconvolve3_rows(width, height, rowstride, data, buffer, a, b);
        deconvolve3_rows(width, height, rowstride, data, buffer, a, b);
        deconvolve3_columns(width, height, rowstride, data, buffer, a, b);
        deconvolve3_columns(width, height, rowstride, data, buffer, a, b);
    }
    else {
        g_critical("Unknown interpolation type %u\n", interpolation);
    }

    g_slice_free1(sizeof(gdouble)*n, buffer);
}

/**
 * gwy_interpolation_resample_block_1d:
 * @data: (array length=length) (inout):
 *        Data block to resample.
 * @length: Data block length.
 * @newdata: (array length=newlength) (out):
 *           Array to put the resampled data to.
 * @newlength: Requested length after resampling.
 * @interpolation: Interpolation type to use.
 * @preserve: %TRUE to preserve the content of @data, %FALSE to permit its
 *            overwriting with temporary data.
 *
 * Resamples a one-dimensional data array.
 *
 * This is a primitive operation, in most cases methods such as
 * gwy_line_new_resampled() provide more convenient interface.
 **/
void
gwy_interpolation_resample_block_1d(gdouble *data,
                                    guint length,
                                    gdouble *newdata,
                                    guint newlength,
                                    GwyInterpolation interpolation,
                                    gboolean preserve)
{
    gdouble *coeffs = NULL;

    guint suplen = gwy_interpolation_get_support_size(interpolation);
    g_return_if_fail(suplen > 0);
    gint sf = -(((gint)suplen - 1)/2);
    gint st = suplen/2;

    if (!gwy_interpolation_has_interpolating_basis(interpolation)) {
        if (preserve)
            data = coeffs = g_slice_copy(sizeof(gdouble)*length, data);
        gwy_interpolation_resolve_coeffs_1d(data, length, interpolation);
    }

    gdouble q = (gdouble)length/newlength;
    gdouble x0 = (q - 1.0)/2.0;
    for (guint newi = 0; newi < newlength; newi++) {
        gdouble w[suplen];
        gdouble x = q*newi + x0;
        gint oldi = (gint)floor(x);
        x -= oldi;
        gwy_interpolation_get_weights(x, interpolation, w);
        gdouble v = 0.0;
        for (gint i = sf; i <= st; i++) {
            guint ii = (oldi + i + 2*st*length) % (2*length);
            if (G_UNLIKELY(ii >= length))
                ii = 2*length-1 - ii;
            v += data[ii]*w[i - sf];
        }
        newdata[newi] = v;
    }

    if (coeffs)
        g_slice_free1(length*sizeof(gdouble), coeffs);
}

static void
calculate_weights_for_rescale(guint oldn,
                              guint newn,
                              gint *positions,
                              gdouble *weights,
                              GwyInterpolation interpolation)
{
    guint suplen = gwy_interpolation_get_support_size(interpolation);
    gdouble q = (gdouble)oldn/newn;
    gdouble x0 = (q - 1.0)/2.0;
    for (guint i = 0; i < newn; i++) {
        gdouble x = q*i + x0;
        positions[i] = (gint)floor(x);
        x -= positions[i];
        gwy_interpolation_get_weights(x, interpolation, weights + suplen*i);
    }
}

/**
 * gwy_interpolation_resample_block_2d:
 * @data: Data block to resample.
 * @width: Number of columns in @data.
 * @height: Number of rows in @data.
 * @rowstride: Total row length (including @width).
 * @newdata: Array to put the resampled data to.
 * @newwidth: Requested number of columns after resampling.
 * @newheight: Requested number of rows after resampling.
 * @newrowstride: Requested total row length after resampling (including
 *                @newwidth).
 * @interpolation: Interpolation type to use.
 * @preserve: %TRUE to preserve the content of @data, %FALSE to permit its
 *            overwriting with temporary data.
 *
 * Resamples a two-dimensional data array.
 *
 * This is a primitive operation, in most cases methods such as
 * gwy_field_new_resampled() provide more convenient interface.
 **/
void
gwy_interpolation_resample_block_2d(gdouble *data,
                                    guint width,
                                    guint height,
                                    guint rowstride,
                                    gdouble *newdata,
                                    guint newwidth,
                                    guint newheight,
                                    guint newrowstride,
                                    GwyInterpolation interpolation,
                                    gboolean preserve)
{
    gdouble *coeffs = NULL;

    guint suplen = gwy_interpolation_get_support_size(interpolation);
    g_return_if_fail(suplen > 0);
    gint sf = -(((gint)suplen - 1)/2);
    gint st = suplen/2;

    if (!gwy_interpolation_has_interpolating_basis(interpolation)) {
        if (preserve) {
            if (rowstride == width)
                data = coeffs = g_memdup(data, width*height*sizeof(gdouble));
            else {
                coeffs = g_slice_alloc(sizeof(gdouble)*width*height);
                for (guint i = 0; i < height; i++) {
                    memcpy(coeffs + i*width,
                           data + i*rowstride,
                           width*sizeof(gdouble));
                }
                data = coeffs;
                rowstride = width;
            }
        }
        gwy_interpolation_resolve_coeffs_2d(data, width, height, rowstride,
                                            interpolation);
    }

    gdouble *xw = g_slice_alloc(sizeof(gdouble)*suplen*newwidth);
    gdouble *yw = g_slice_alloc(sizeof(gdouble)*suplen*newheight);
    gint *xp = g_slice_alloc(sizeof(gint)*newwidth);
    gint *yp = g_slice_alloc(sizeof(gint)*newheight);
    calculate_weights_for_rescale(width, newwidth, xp, xw, interpolation);
    calculate_weights_for_rescale(height, newheight, yp, yw, interpolation);
    for (guint newi = 0; newi < newheight; newi++) {
        gint oldi = yp[newi];
        for (guint newj = 0; newj < newwidth; newj++) {
            gint oldj = xp[newj];
            gdouble v = 0.0;
            for (gint i = sf; i <= st; i++) {
                guint ii = (oldi + i + 2*st*height) % (2*height);
                if (G_UNLIKELY(ii >= height))
                    ii = 2*height-1 - ii;
                gdouble vx = 0.0;
                for (gint j = sf; j <= st; j++) {
                    guint jj = (oldj + j + 2*st*width) % (2*width);
                    if (G_UNLIKELY(jj >= width))
                        jj = 2*width-1 - jj;
                    vx += data[ii*rowstride + jj]*xw[newj*suplen + j - sf];
                }
                v += vx*yw[newi*suplen + i - sf];
            }
            newdata[newi*newrowstride + newj] = v;
        }
    }
    g_slice_free1(sizeof(gdouble)*suplen*newwidth, xw);
    g_slice_free1(sizeof(gdouble)*suplen*newheight, yw);
    g_slice_free1(sizeof(gint)*newwidth, xp);
    g_slice_free1(sizeof(gint)*newheight, yp);

    if (coeffs)
        g_slice_free1(sizeof(gdouble)*width*height, coeffs);
}

/**
 * gwy_interpolation_shift_block_1d:
 * @data: (array length=length) (inout):
 *         Data block to shift.
 * @newdata: (array length=length) (inout):
 *           Array to put the shifted data to.
 * @length: Data block length.
 * @offset: The shift, in corrective sense.  Shift value of 1.0 means the
 *          zeroth value of @newdata will be set to the first value of @data.
 * @interpolation: Interpolation type to use.
 * @exterior: Exterior pixels handling.
 * @fill_value: The value to use with @GWY_EXTERIOR_FIXED_VALUE.
 * @preserve: %TRUE to preserve the content of @data, %FALSE to permit its
 *            overwriting with temporary data.
 *
 * Shifts a one-dimensional data block by a possibly non-integer offset.
 **/
void
gwy_interpolation_shift_block_1d(gdouble *data,
                                 gdouble *newdata,
                                 guint length,
                                 gdouble offset,
                                 GwyInterpolation interpolation,
                                 GwyExterior exterior,
                                 gdouble fill_value,
                                 gboolean preserve)
{
    gdouble *coeffs = NULL;
    gboolean warned = FALSE;

    guint suplen = gwy_interpolation_get_support_size(interpolation);
    g_return_if_fail(suplen > 0);
    gint sf = -(((gint)suplen - 1)/2);
    gint st = suplen/2;

    gdouble d0 = data[0];
    gdouble dn = data[length-1];

    if (!gwy_interpolation_has_interpolating_basis(interpolation)) {
        if (preserve)
            data = coeffs = g_slice_copy(sizeof(gdouble)*length, data);
        gwy_interpolation_resolve_coeffs_1d(data, length, interpolation);
    }

    gint off = (gint)floor(offset);
    gdouble w[suplen];
    gwy_interpolation_get_weights(offset - off, interpolation, w);

    for (guint newi = 0; newi < length; newi++) {
        gint oldi = newi + off;
        if (G_LIKELY(oldi + sf >= 0 && oldi + st < (gint)length)) {
            /* The fast path, we are safely inside, directly use coeffs */
            gdouble v = 0.0;
            for (gint i = sf; i <= st; i++)
                v += w[i - sf]*data[oldi + i];
            newdata[newi] = v;
        }
        else {
            /* Exterior or too near to the border to feel the mirroring.
             * Use mirror extend for all points not really outside. */
            if (exterior == GWY_EXTERIOR_MIRROR_EXTEND
                || exterior == GWY_EXTERIOR_PERIODIC
                || (oldi >= 0 && oldi + 1 < (gint)length)
                || (oldi == (gint)length-1 && off == offset)) {
                if (exterior == GWY_EXTERIOR_PERIODIC)
                    oldi = oldi % length + (oldi >= 0 ? 0 : length);
                gdouble v = 0.0;
                for (gint i = sf; i <= st; i++) {
                    guint ii = (oldi + i + 2*st*length) % (2*length);
                    if (ii >= length)
                        ii = 2*length-1 - ii;
                    v += w[i - sf]*data[ii];
                }
                newdata[newi] = v;
            }
            else if (exterior == GWY_EXTERIOR_FIXED_VALUE)
                newdata[newi] = fill_value;
            else if (exterior == GWY_EXTERIOR_BORDER_EXTEND) {
                if (oldi < 0)
                    newdata[newi] = d0;
                else
                    newdata[newi] = dn;
            }
            else if (exterior == GWY_EXTERIOR_UNDEFINED) {
                /* Do nothing */
            }
            else {
                if (!warned) {
                    g_warning("Unsupported exterior type, assuming undefined");
                    warned = TRUE;
                }
            }
        }
    }

    if (coeffs)
        g_slice_free1(sizeof(gdouble)*length, coeffs);
}


/**
 * SECTION: interpolation
 * @title: Interpolation
 * @section_id: libgwy4-interpolation
 * @short_description: Low-level interpolation functions
 *
 * Data interpolation is usually pixel-like in Gwyddion, not function-like.
 * That means that the contribution of individual data samples is preserved on
 * scaling.  The area that <quote>belongs</quote> to all values is the same,
 * it is not reduced to half for edge pixels.
 *
 * Most of the functions listed here are quite low-level.  Usually,
 * #GwyField and #GwyLine methods offer the same functionality with a
 * more convenient interface.
 *
 * Using interpolations with a non-interpolating basis requires some care.
 * If the basis is interpolating the coefficients are identical to data values.
 * However, for a non-interpolating basis they differ and have to be
 * precalculated using functions such as gwy_interpolation_resolve_coeffs_2d()
 * or their high-level equivalents such as gwy_field_interpolation_coeffs().
 *
 * At present, Gwyddion implements two-point and four-point interpolations.
 * However, do not rely on this.  Generic code that works with any
 * interpolation type should always use gwy_interpolation_get_support_size()
 * and gwy_interpolation_has_interpolating_basis() to obtain the interpolation
 * properties.
 **/

/**
 * GwyInterpolation:
 * @GWY_INTERPOLATION_BSPLINE1: First-order (constant) B-spline interpolation.
 *                              Also known as symmetric nearest neighbour
 *                              interpolation, which differs from rounding in
 *                              that values exactly in the middle are mean
 *                              values from the two neighbours.
 * @GWY_INTERPOLATION_ROUND: Rounding interpolation, this is an alias for
 *                           %GWY_INTERPOLATION_BSPLINE1.  Note this is not
 *                           true rounding interpolation, the support size is
 *                           2, not 1.
 * @GWY_INTERPOLATION_BSPLINE2: Second-order (linear) B-spline interpolation.
 * @GWY_INTERPOLATION_LINEAR: Linear interpolation.   This is an alias for
 *                            %GWY_INTERPOLATION_BSPLINE2.
 * @GWY_INTERPOLATION_KEYS: Cubic Keys interpolation (with a=-1/2).
 * @GWY_INTERPOLATION_BSPLINE4: Fourth-order (cubic) B-spline interpolation.
 * @GWY_INTERPOLATION_BSPLINE: An alias for %GWY_INTERPOLATION_BSPLINE4.
 * @GWY_INTERPOLATION_OMOMS4: Fourth-order (cubic) o-MOMS interpolation.
 * @GWY_INTERPOLATION_OMOMS: An alias for %GWY_INTERPOLATION_OMOMS4.
 * @GWY_INTERPOLATION_NNA: Nearest neighbour approximation.
 * @GWY_INTERPOLATION_SCHAUM4: Fourth-order (cubic) Schaum's interpolation.
 * @GWY_INTERPOLATION_SCHAUM: An alias for %GWY_INTERPOLATION_SCHAUM4.
 * @GWY_INTERPOLATION_BSPLINE6: Sixth-order (quntic) B-spline interpolation.
 *
 * Interpolation types.
 **/

/**
 * GwyExterior:
 * @GWY_EXTERIOR_UNDEFINED: The values corresponding to or calculated from
 *                          exterior data values are undefined, they may be
 *                          left unset or set to bogus values, including NaN.
 *                          The caller must handle them afterwards, for
 *                          instance by resizing the result to consist of valid
 *                          data only.
 * @GWY_EXTERIOR_BORDER_EXTEND: Values of exterior pixels are considered to be
 *                              equal to the values of the nearest interior
 *                              pixels.
 * @GWY_EXTERIOR_MIRROR_EXTEND: The data is considered to be periodically
 *                              repeated, with odd instances reflected
 *                              (the total period is thus twice the size of
 *                              the data).
 * @GWY_EXTERIOR_PERIODIC: The data is considered to be periodically repeated.
 * @GWY_EXTERIOR_FIXED_VALUE: Values of exterior pixels are considered to
 *                            be all equal to a user-specified value.
 *
 * Methods of handling pixels outside data.
 *
 * Not all functions offer the exterior handling method choice.  Generally,
 * functions that need to extrapolate just a pixel or two outside the field use
 * a fixed method, namely mirroring which is the recommended method and that
 * also for a single-pixel extension coincides with border-extending.  For
 * instance interpolation or surface area calculation uses this method.
 *
 * Functions that may need to work with larger exterior, such as convolutions
 * and correlations offer, generally, the full set of exterior handling
 * options.
 *
 * See also the generic functions gwy_field_extend() that can to apply the
 * extension methods to a field.
 *
 * <warning>Undefined exterior is sometimes the fastest method if you are not
 * interested in the border values.  But it is often dangerous if the undefined
 * values enter the calculation.  Then, even if the precise mathematical result
 * does not depend on the exterior values, the actual result calculated using
 * finite-precision floating point numbers can be still completely ruined.
 * This occurs namely if the undefined exterior happens to contain NaNs or
 * values such as 10³⁰⁰.  Therefore, convolutions and correlations that may be
 * calculated using FFT and interpolations with non-interpolating basis must
 * always be used only with a defined exterior.</warning>
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
