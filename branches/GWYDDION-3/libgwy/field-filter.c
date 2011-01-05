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

#include "config.h"
#include <fftw3.h>
#include <string.h>
#include "libgwy/macros.h"
#include "libgwy/math.h"
#include "libgwy/field-filter.h"
#include "libgwy/mask-field.h"
#include "libgwy/line-arithmetic.h"
#include "libgwy/line-statistics.h"
#include "libgwy/math-internal.h"
#include "libgwy/field-arithmetic.h"
#include "libgwy/field-internal.h"

#ifndef HAVE_SINCOS
static inline void
sincos(double x, double *s, double *c)
{
    *s = sin(x);
    *c = cos(x);
}
#endif

#if 0
static void
gwy_part_row_convolve3(GwyField *field,
                       const gdouble *kdata,
                       guint col, guint row,
                       guint width, guint height)
{
    if (width == 1) {
        gdouble s = kdata[0] + kdata[1] + kdata[2];
        gwy_field_multiply(field, &((GwyRectangle){col, row, width, height}),
                           NULL, GWY_MASK_IGNORE, s);
        return;
    }

    guint xres = field->xres;
    for (guint i = 0; i < height; i++) {
        gdouble *drow = field->data + (row + i)*xres + col;
        gdouble buf[3];
        buf[0] = (kdata[2]*(col ? *(drow - 1) : drow[1])
                  + kdata[1]*drow[0]
                  + kdata[0]*drow[1]);
        buf[1] = kdata[2]*drow[0] + kdata[1]*drow[1];
        buf[2] = kdata[2]*drow[1];
        for (guint j = 0; j < width; j++) {
            guint k = j + 2;
            if (G_UNLIKELY(col + k >= xres))
                k = 2*xres-1 - k;
            gdouble v = drow[k];
            drow[j] = buf[0];
            buf[0] = buf[1] + kdata[0]*v;
            buf[1] = buf[2] + kdata[1]*v;
            buf[2] = kdata[2]*v;
        }
    }
}
#endif

// Generally, there are three reasonable strategies:
// (a) small kernels (kres comparable to log(res)) → direct calculation
// (b) medium-sized kernels (kres ≤ res) → use explicit extension/mirroring to
//     a nice-for-FFT size ≥ 2res and cyclic convolution
// (c) large kernels (kres > res) → fold kernel to 2res size and use implicit
//     mirroring and DCT-II for data

// FIXME: This is not very clever:
// - if mirroring does NOT have to be used we can extend the array a bit in
//   order to get a nice-for-FFT length
// - if mirroring is used but the kernel is short we may prefer to mirror it
//   manually into a nice-for-FFT length
// - only if the kernel is long it is preferable to let it be and use a
//   data-sized transform whatever it takes
static void
input_range_for_convolution(guint pos, guint len, guint size, guint klen,
                            guint *ipos, guint *ilen)
{
    guint k2 = klen/2;

    *ipos = (pos <= k2) ? 0 : pos - k2;
    *ilen = (pos + len-1 + k2 < size) ? pos + len + k2 - *ipos : size - *ipos;
}

static void
mirror_kernel(const gdouble *kernel, guint klen,
              gdouble *mirrored, guint size)
{
    gwy_clear(mirrored, size);
    for (guint i = klen/2; i < klen; i++)
        mirrored[(i - klen/2) % size] += kernel[i];
    for (guint i = klen/2; i; i--)
        mirrored[size-1 - (klen/2 - i) % size] += kernel[i-1];
}

// Apply the factor exp[πiν/(2N)] that FFTW leaves out but we need it to make
// the meaning of R2R transform coefficients the same as in R2C.  So, the
// factor actually belongs to the RODFT10 of data but it is better to multiply
// the kernel than each data row.
static void
multiply_with_halfexp(gdouble *hc, guint size)
{
    guint size2 = 2*size;

    hc[0] /= size2;
    for (guint i = 1; i < size; i++) {
        gdouble s, c, re, im;
        sincos(0.5*G_PI*i/size, &s, &c);
        re = c*hc[i] - s*hc[size2 - i];
        im = s*hc[i] + c*hc[size2 - i];
        hc[i] = re/size2;
        hc[size2 - i] = im/size2;
    }
    hc[size] = 0.0;
}

static void
multiply_hc_with_redft10(const gdouble *hcin,
                         const gdouble *dct,
                         gdouble *hcout,
                         guint size)
{
    const gdouble *rein = hcin, *imin = hcin + 2*size-1;
    gdouble *reout = hcout, *imout = hcout + 2*size-1;

    *reout = *dct * *rein;
    reout++, rein++, dct++;
    for (guint j = size-1; j; j--, reout++, rein++, dct++, imout--, imin--) {
        *reout = *dct * *rein;
        *imout = *dct * *imin;
    }
    *reout = 0.0;
}

void
gwy_field_row_convolve_fft(GwyField *field,
                           const GwyRectangle *rectangle,
                           const GwyLine *kernel)
{
    guint col, row, width, height;
    if (!_gwy_field_check_rectangle(field, rectangle,
                                    &col, &row, &width, &height))
        return;

    g_return_if_fail(GWY_IS_LINE(kernel));
    g_return_if_fail(kernel->res % 2 == 1);

    guint icol, iwidth;
    input_range_for_convolution(col, width, field->xres, kernel->res,
                                &icol, &iwidth);

    // An even size is necessary due to alignment constraints in FFTW.
    // Using this size for all buffers is a bit excessive but safe.
    guint size = (iwidth + 1)/2*2;
    gdouble *data_in = fftw_malloc(2*size*sizeof(gdouble));
    gdouble *data_dct = data_in + size;
    // Create all plans before trying to do anything, the planner may destroy
    // input data!
    // The DCT-II plan for transforming each data row.
    fftw_plan dplan = fftw_plan_r2r_1d(iwidth, data_in, data_dct, FFTW_REDFT10,
                                       _GWY_FFTW_PATIENCE);

    gdouble *kernelhc = fftw_malloc(4*size*sizeof(gdouble));
    gdouble *convolved = kernelhc + 2*size;
    // The plan for the HC2R transform of the convolution of each row.
    // Note how entire data_in[] serves also here as a buffer.
    fftw_plan cplan = fftw_plan_r2r_1d(2*iwidth, convolved, data_in,
                                       FFTW_HC2R, _GWY_FFTW_PATIENCE);
    // The plan for the initial R2HC transform of the kernel.
    fftw_plan kplan = fftw_plan_r2r_1d(2*iwidth, convolved, kernelhc,
                                       FFTW_R2HC, _GWY_FFTW_PATIENCE);

    // Transform and premultiply the kernel.
    mirror_kernel(kernel->data, kernel->res, convolved, 2*iwidth);
    fftw_execute(kplan);
    multiply_with_halfexp(kernelhc, iwidth);

    // Convolve rows
    gdouble *base = field->data + row*field->xres;
    for (guint i = 0; i < height; i++) {
        gwy_assign(data_in, base + i*field->xres + icol, iwidth);
        fftw_execute(dplan);
        multiply_hc_with_redft10(kernelhc, data_dct, convolved, iwidth);
        fftw_execute(cplan);
        gwy_assign(base + i*field->xres + col, data_in + col - icol, width);
    }

    fftw_destroy_plan(kplan);
    fftw_destroy_plan(cplan);
    fftw_destroy_plan(dplan);
    fftw_free(kernelhc);
    fftw_free(data_in);

    gwy_field_invalidate(field);
}

/**
 * gwy_field_row_convolve:
 * @field: A two-dimensional data field.
 * @rectangle: Area in @field to process.  Pass %NULL to process entire @field.
 * @kernel: Kenrel to convolve @field with.
 *
 * Convolve a field with a horizontal kernel.
 *
 * The kernel must have an odd number of pixels.  The convolution is then
 * performed with the kernel centred on the respective field pixels.
 * If you want to perform convolution with an even-sized kernel pad it with a
 * zero on the left or right side, depending on whether you prefer the result
 * to be shifted left or right.
 *
 * Data outside the field are represented using mirroring.  Data outside the
 * rectangle but still inside the field are taken from the field, i.e. the
 * result is influenced by data outside the rectangle.
 **/
void
gwy_field_row_convolve(GwyField *field,
                       const GwyRectangle *rectangle,
                       const GwyLine *kernel)
{
    guint col, row, width, height;
    if (!_gwy_field_check_rectangle(field, rectangle,
                                    &col, &row, &width, &height))
        return;

    g_return_if_fail(GWY_IS_LINE(kernel));
    g_return_if_fail(kernel->res % 2 == 1);

    guint kres, mres, k0, i, j, k, pos;
    const gdouble *kdata;
    gdouble *buf, *drow;
    gdouble d;

    kres = kernel->res;
    kdata = kernel->data;
    mres = 2*width;
    k0 = (kres/2 + 1)*mres;
    buf = g_new(gdouble, kres);

    for (i = 0; i < height; i++) {
        drow = field->data + (row + i)*field->xres + col;
        /* Initialize with a triangluar sums, mirror-extend */
        gwy_clear(buf, kres);
        for (j = 0; j < kres; j++) {
            k = (j + k0 - kres/2) % mres;
            d = drow[k < width ? k : mres-1 - k];
            for (k = 0; k <= j; k++)
                buf[k] += kdata[kres-1 - (j - k)]*d;
        }
        pos = 0;
        /* Middle part and tail with mirror extension again, we do some
         * O(1/2*k^2) of useless work here by not separating the tail */
        for (j = 0; j < width; j++) {
            drow[j] = buf[pos];
            buf[pos] = 0.0;
            pos = (pos + 1) % kres;
            k = (j + k0 + kres - kres/2) % mres;
            d = drow[G_LIKELY(k < width) ? k : mres-1 - k];
            for (k = pos; k < kres; k++)
                buf[k] += kdata[k - pos]*d;
            for (k = 0; k < pos; k++)
                buf[k] += kdata[kres-1 - (pos-1 - k)]*d;
        }
    }

    g_free(buf);
}

#if 0
static void
gwy_field_col_convolve3(GwyField *field,
                        const gdouble *kdata,
                        guint col, guint row,
                        guint width, guint height)
{
    if (height == 1) {
        gdouble s = kdata[0] + kdata[1] + kdata[2];
        gwy_field_multiply(field, &((GwyRectangle){col, row, width, height}),
                           NULL, GWY_MASK_IGNORE, s);
        return;
    }

    guint xres = field->xres, yres = field->yres;
    gdouble *buf = g_new(gdouble, 3*width);
    gdouble *drow = field->data + row*xres + col;
    for (guint j = 0; j < width; j++) {
        buf[3*j] = (kdata[2]*(row ? *(drow - xres) : drow[j + xres])
                    + kdata[1]*drow[j]
                    + kdata[0]*drow[j + xres]);
        buf[3*j + 1] = kdata[2]*drow[j] + kdata[1]*drow[xres];
        buf[3*j + 2] = kdata[2]*drow[xres];
    }
    for (guint i = 0; i < height; i++) {
        drow = field->data + (row + i)*xres + col;
        guint k = i + 2;
        if (G_UNLIKELY(row + k >= yres))
            k = 2*yres-1 - k;
        gdouble *drow2 = field->data + (row + k)*xres + col;
        for (guint j = 0; j < width; j++) {
            gdouble v = drow2[j];
            drow[j] = buf[3*j];
            buf[3*j] = buf[3*j + 1] + kdata[0]*v;
            buf[3*j + 1] = buf[3*j + 2] + kdata[1]*v;
            buf[3*j + 2] = kdata[2]*v;
        }
    }

    g_free(buf);
    gwy_field_invalidate(field);
}
#endif

/**
 * gwy_field_col_convolve:
 * @field: A two-dimensional data field.
 * @rectangle: Area in @field to process.  Pass %NULL to process entire @field.
 * @kernel: Kenrel to convolve @field with.
 *
 * Convolve a rectangular part of a data field with a vertical kernel.
 *
 * The kernel must have an odd number of pixels.  The convolution is then
 * performed with the kernel centred on the respective field pixels.
 * If you want to perform convolution with an even-sized kernel pad it with a
 * zero on the left or right side, depending on whether you prefer the result
 * to be shifted left or right.
 *
 * Data outside the field are represented using mirroring.  Data outside the
 * rectangle but still inside the field are taken from the field, i.e. the
 * result is influenced by data outside the rectangle.
 **/
void
gwy_field_col_convolve(GwyField *field,
                       const GwyRectangle* rectangle,
                       const GwyLine *kernel)
{
    guint col, row, width, height;
    if (!_gwy_field_check_rectangle(field, rectangle,
                                    &col, &row, &width, &height))
        return;

    g_return_if_fail(GWY_IS_LINE(kernel));
    g_return_if_fail(kernel->res % 2 == 1);

    guint kres, xres, mres, k0, i, j, k, pos;
    const gdouble *kdata;
    gdouble *buf, *dcol;
    gdouble d;

    kres = kernel->res;
    kdata = kernel->data;
    xres = field->xres;
    mres = 2*height;
    k0 = (kres/2 + 1)*mres;
    buf = g_new(gdouble, kres);

    /* This looks like a bad memory access pattern.  And for small kernels it
     * indeed is (we should iterate row-wise and directly calculate the sums).
     * For large kernels this is mitigated by the maximum possible amount of
     * work done per a data field access. */
    for (j = 0; j < width; j++) {
        dcol = field->data + row*xres + (col + j);
        /* Initialize with a triangluar sums, mirror-extend */
        gwy_clear(buf, kres);
        for (i = 0; i < kres; i++) {
            k = (i + k0 - kres/2) % mres;
            d = dcol[k < height ? k*xres : (mres-1 - k)*xres];
            for (k = 0; k <= i; k++)
                buf[k] += kdata[kres-1 - (i - k)]*d;
        }
        pos = 0;
        /* Middle part and tail with mirror extension again, we do some
         * O(1/2*k^2) of useless work here by not separating the tail */
        for (i = 0; i < height; i++) {
            dcol[i*xres] = buf[pos];
            buf[pos] = 0.0;
            pos = (pos + 1) % kres;
            k = (i + k0 + kres - kres/2) % mres;
            d = dcol[G_LIKELY(k < height) ? k*xres : (mres-1 - k)*xres];
            for (k = pos; k < kres; k++)
                buf[k] += kdata[k - pos]*d;
            for (k = 0; k < pos; k++)
                buf[k] += kdata[kres-1 - (pos-1 - k)]*d;
        }
    }

    g_free(buf);
    gwy_field_invalidate(field);
}

static gboolean
make_gaussian_kernel(GwyLine *kernel,
                     gdouble sigma)
{
    // Smaller values lead to underflow even the first non-central term
    if (sigma < 0.026)
        return FALSE;

    // Exclude terms smaller than machine eps compared to the central term
    guint res = 2*(guint)ceil(8.0*sigma) + 1;
    // But do at least a 3-term filter so that they don't say we didn't try
    res = MAX(res, 3);
    gdouble center = (res - 1.0)/2.0;
    double s = 0.0;

    gwy_line_set_size(kernel, res, FALSE);
    for (guint k = 0; k < res; k++) {
        gdouble x = k - center;
        x /= sigma;
        s += kernel->data[k] = exp(-x*x/2.0);
    }
    gwy_line_multiply(kernel, 1.0/s);

    return TRUE;
}

/**
 * gwy_field_filter_gaussian:
 * @field: A two-dimensional data field.
 * @rectangle: Area in @field to process.  Pass %NULL to process entire @field.
 * @hsigma: Horizontal sigma parameter (square root of dispersion) of the
 *          Gaussian, in pixels.
 * @vsigma: Vertical sigma parameter (square root of dispersion) of the
 *          Gaussian, in pixels.
 *
 * Filters a rectangular part of a field with a Gaussian filter.
 *
 * The Gausian is normalised, i.e. the filter is sum-preserving.
 *
 * Pass the same @hsigma and @vsigma for a pixel-wise rotationally symmetrical
 * Gaussian which is usually wanted.  However, if the horizontal and vertical
 * physical pixel dimensions differ or filtering only in one direction is
 * required then passing different @hsigma and @vsigma can be useful.
 **/
void
gwy_field_filter_gaussian(GwyField *field,
                          const GwyRectangle* rectangle,
                          gdouble hsigma, gdouble vsigma)
{
    /* The field and rectangle are checked by the actual convolution funcs. */
    g_return_if_fail(hsigma >= 0.0 && vsigma >= 0.0);

    /* XXX: Expressing the convolution using a cyclic convolution of the kernel
     * and mirrored data, the kernel can be always reduced (projected) onto
     * size of the mirrored data, i.e. max 2*data_size. */
    GwyLine *kernel = gwy_line_new();
    gboolean done = FALSE;

    if (make_gaussian_kernel(kernel, hsigma)) {
        gwy_field_row_convolve(field, rectangle, kernel);
        if (vsigma == hsigma) {
            gwy_field_col_convolve(field, rectangle, kernel);
            done = TRUE;
        }
    }
    if (!done && make_gaussian_kernel(kernel, vsigma))
        gwy_field_col_convolve(field, rectangle, kernel);
    g_object_unref(kernel);
}

/**
 * SECTION: field-filter
 * @section_id: GwyField-filter
 * @title: GwyField filtering
 * @short_description: Field filtering
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
