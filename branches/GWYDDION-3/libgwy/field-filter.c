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
#include <string.h>
#include <fftw3.h>
#include "libgwy/macros.h"
#include "libgwy/math.h"
#include "libgwy/fft.h"
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
static inline void
input_range_for_convolution(guint pos, guint len, guint size, guint klen,
                            guint *ipos, guint *ilen)
{
    guint k2 = klen/2;

    *ipos = (pos <= k2) ? 0 : pos - k2;
    *ilen = (pos + len-1 + k2 < size) ? pos + len + k2 - *ipos : size - *ipos;
}

// Expand the kernel into an array of size @size, putting the central item
// at zero.  If the kernel is larger than @size it is periodically folded into
// the array,
static inline void
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
static inline void
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

static inline void
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

#if 0
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
    // The DCT-II plan for transforming each data row.
    fftw_plan dplan = fftw_plan_r2r_1d(iwidth, data_in, data_dct, FFTW_REDFT10,
                                       FFTW_DESTROY_INPUT | _gwy_fft_rigour());

    gdouble *kernelhc = fftw_malloc(4*size*sizeof(gdouble));
    gdouble *convolved = kernelhc + 2*size;
    // The plan for the HC2R transform of the convolution of each row.
    // Note how entire data_in[] serves also here as a buffer.
    fftw_plan cplan = fftw_plan_r2r_1d(2*iwidth, convolved, data_in,
                                       FFTW_HC2R,
                                       FFTW_DESTROY_INPUT | _gwy_fft_rigour());
    // The plan for the initial R2HC transform of the kernel.
    fftw_plan kplan = fftw_plan_r2r_1d(2*iwidth, convolved, kernelhc,
                                       FFTW_R2HC,
                                       FFTW_DESTROY_INPUT | _gwy_fft_rigour());

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
#endif

// XXX: This seems to give worse performance than per-row convolutions, maybe
// due to worse memory locality.
#if 0
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

    guint xres = field->xres, kres = kernel->res;
    guint icol, iwidth;
    input_range_for_convolution(col, width, xres, kres, &icol, &iwidth);

    // An even size is necessary due to alignment constraints in FFTW.
    // Using this size for all buffers is a bit excessive but safe.
    guint size = (iwidth + 1)/2*2;
    guint iwidth2 = 2*iwidth;
    gdouble *data_in = fftw_malloc((4*size*height + 2*size)*sizeof(gdouble));
    gdouble *data_dct = data_in + size*height;
    gdouble *data_hc = data_in + 2*size*height;
    gdouble *kernelhc = data_in + 4*size*height;
    gint fftsize[1] = { iwidth }, fft2size[1] = { iwidth2 };
    fftw_r2r_kind redft0[1] = { FFTW_REDFT10 }, hc2r[1] = { FFTW_HC2R };
    // The R2HC plan for the initial transform of the kernel.  We put the
    // folded real-space kernel into data_hc that is unused at that time.
    fftw_plan kplan = fftw_plan_r2r_1d(iwidth2, data_hc, kernelhc,
                                       FFTW_R2HC,
                                       FFTW_DESTROY_INPUT | _gwy_fft_rigour());
    // The DCT-II plan for transforming mirror-extended data.
    fftw_plan dplan = fftw_plan_many_r2r(1, fftsize, height,
                                         data_in, NULL, 1, size,
                                         data_dct, NULL, 1, size,
                                         redft0, (FFTW_DESTROY_INPUT
                                                   | _gwy_fft_rigour()));
    // The HC2R plan the backward transform of the convolution of each row.
    // Both data_in + data_dct serve together as the output buffer.
    fftw_plan cplan = fftw_plan_many_r2r(1, fft2size, height,
                                         data_hc, NULL, 1, 2*size,
                                         data_in, NULL, 1, 2*size,
                                         hc2r, _gwy_fft_rigour());

    // Transform and premultiply the kernel.
    mirror_kernel(kernel->data, kres, data_hc, iwidth2);
    fftw_execute(kplan);
    multiply_with_halfexp(kernelhc, iwidth);

    // Convolve rows
    const gdouble *inbase = field->data + row*xres + icol;
    gdouble *outbase = field->data + row*xres + col;
    for (guint i = 0; i < height; i++)
        gwy_assign(data_in + i*size, inbase + i*xres, iwidth);
    fftw_execute(dplan);
    for (guint i = 0; i < height; i++)
        multiply_hc_with_redft10(kernelhc, data_dct + i*size, data_hc, iwidth);
    fftw_execute(cplan);
    for (guint i = 0; i < height; i++)
        gwy_assign(outbase + i*xres, data_in + (col - icol), width);

    fftw_destroy_plan(kplan);
    fftw_destroy_plan(cplan);
    fftw_destroy_plan(dplan);
    fftw_free(data_in);

    gwy_field_invalidate(field);
}
#endif

/* TODO: Center the original data in the extended row.  This makes a slighty
 * more complicated to locate them there.  The advantage is that convolutions
 * then never need to wrap around the array edges and it also makes it usable
 * for something like gwy_field_extend(). */
static inline void
row_extend_mirror(const gdouble *in, gdouble *out,
                  guint pos, guint width, guint res, guint extsize)
{
    for (guint j = 0; j < extsize; j++)
        out[j] = 42.0;

    // The easy part
    gwy_assign(out, in + pos, width);

    guint res2 = 2*res;
    guint extend = extsize - width;
    gdouble *out2 = out + width;
    // Forward-extend
    for (guint j = 0; j < extend/2; j++, out2++) {
        guint k = (pos + width + j) % res2;
        g_assert(*out2 == 42.0);
        *out2 = (k < res) ? in[k] : in[res2-1 - k];
    }
    // Backward-extend (from the end)
    guint k0 = (extsize/res2 + 1)*res2;
    out2 = out + extsize-1;
    for (guint j = 1; j <= extend - extend/2; j++, out2--) {
        guint k = (k0 + pos - j) % res2;
        g_assert(*out2 == 42.0);
        *out2 = (k < res) ? in[k] : in[res2-1 - k];
    }

    gboolean failed = FALSE;
    for (guint j = 0; j < extsize; j++) {
        if (out[j] == 42.0) {
            g_printerr("BAD #%u\n", j);
            failed = TRUE;
        }
    }
    if (failed) {
        g_printerr("pos=%u width=%u res=%u extsize=%u\n", pos, width, res, extsize);
        g_assert_not_reached();
    }
}

static inline void
multiply_hc_with_hc(const gdouble *hcin,
                    gdouble *hcout,
                    guint size)
{
    const gdouble *rein = hcin, *imin = hcin + size-1;
    gdouble *reout = hcout, *imout = hcout + size-1;

    *reout *= *rein/size;
    reout++, rein++;
    for (guint j = (size + 1)/2 - 1; j; j--, reout++, rein++, imout--, imin--) {
        gdouble re = *rein * *reout - *imin * *imout;
        gdouble im = *rein * *imout + *imin * *reout;
        *reout = re/size;
        *imout = im/size;
    }
    // Always even
    *reout *= *rein/size;
}

#if 0
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

    guint xres = field->xres, kres = kernel->res;
    guint size = gwy_fft_nice_transform_size(xres + kres);
    gdouble *data_in = fftw_malloc((3*size*height + size)*sizeof(gdouble));
    gdouble *data_hc = data_in + size*height;
    gdouble *kernelhc = data_in + 3*size*height;
    gint fftsize[1] = { size };
    fftw_r2r_kind r2hc[1] = { FFTW_R2HC }, hc2r[1] = { FFTW_HC2R };
    // The R2HC plan for the initial transform of the kernel.  We put the
    // 0-extended real-space kernel into data_hc that is unused at that time.
    fftw_plan kplan = fftw_plan_r2r_1d(size, data_hc, kernelhc,
                                       FFTW_R2HC,
                                       FFTW_DESTROY_INPUT | _gwy_fft_rigour());
    // The R2HC plan for transforming the mirror-extended data.
    fftw_plan dplan = fftw_plan_many_r2r(1, fftsize, height,
                                         data_in, NULL, 1, size,
                                         data_hc, NULL, 1, size,
                                         r2hc, (FFTW_DESTROY_INPUT
                                                | _gwy_fft_rigour()));
    // The HC2R plan the backward transform of the convolution of each row.
    fftw_plan cplan = fftw_plan_many_r2r(1, fftsize, height,
                                         data_hc, NULL, 1, size,
                                         data_in, NULL, 1, size,
                                         hc2r, _gwy_fft_rigour());

    // Transform the kernel.
    mirror_kernel(kernel->data, kres, data_hc, size);
    fftw_execute(kplan);

    // Convolve rows
    for (guint i = 0; i < height; i++)
        row_extend_mirror(field->data + (row + i)*xres, data_in + i*size,
                          col, width, xres, size);
    fftw_execute(dplan);
    for (guint i = 0; i < height; i++)
        multiply_hc_with_hc(kernelhc, data_hc + i*size, size);
    fftw_execute(cplan);
    for (guint i = 0; i < height; i++)
        gwy_assign(field->data + (row + i)*xres + col, data_in, width);

    fftw_destroy_plan(kplan);
    fftw_destroy_plan(cplan);
    fftw_destroy_plan(dplan);
    fftw_free(data_in);

    gwy_field_invalidate(field);
}
#endif

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

    guint xres = field->xres, kres = kernel->res;
    guint size = gwy_fft_nice_transform_size(xres + kres - 1);
    g_printerr("xres=%u kres=%u size=%u\n", xres, kres, size);
    gdouble *data_in = fftw_malloc(3*size*sizeof(gdouble));
    gdouble *data_hc = data_in + size;
    gdouble *kernelhc = data_in + 2*size;
    // The R2HC plan for the initial transform of the kernel.  We put the
    // 0-extended real-space kernel into data_hc that is unused at that time.
    fftw_plan kplan = fftw_plan_r2r_1d(size, data_hc, kernelhc,
                                       FFTW_R2HC,
                                       FFTW_DESTROY_INPUT | _gwy_fft_rigour());
    // The R2HC plan for transforming the mirror-extended data.
    fftw_plan dplan = fftw_plan_r2r_1d(size, data_in, data_hc,
                                       FFTW_R2HC,
                                       FFTW_DESTROY_INPUT | _gwy_fft_rigour());
    // The HC2R plan the backward transform of the convolution of each row.
    fftw_plan cplan = fftw_plan_r2r_1d(size, data_hc, data_in,
                                       FFTW_HC2R,
                                       _gwy_fft_rigour());

    // Transform the kernel.
    mirror_kernel(kernel->data, kres, data_hc, size);
    fftw_execute(kplan);

    // Convolve rows
    for (guint i = 0; i < height; i++) {
        row_extend_mirror(field->data + (row + i)*xres, data_in,
                          col, width, xres, size);
        fftw_execute(dplan);
        multiply_hc_with_hc(kernelhc, data_hc, size);
        fftw_execute(cplan);
        gwy_assign(field->data + (row + i)*xres + col, data_in, width);
    }

    fftw_destroy_plan(kplan);
    fftw_destroy_plan(cplan);
    fftw_destroy_plan(dplan);
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
 * The convolution is then performed with the kernel centred on the respective
 * field pixels.  For an odd-sized kernel this holds precisely.  For an
 * even-sized kernel this means the kernel centre is placed 0.5 pixel to the
 * right from the respective field pixel.
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

    guint i, j, k, pos;
    gdouble d;

    guint xres = field->xres;
    guint kres = kernel->res;
    const gdouble *kdata = kernel->data;
    guint mres = 2*width;
    guint k0 = (kres/2 + 1)*mres;
    guint size = xres + kres - 1;
    gdouble *data_in = g_new(gdouble, size);

    // The direct method is used only if kres ≪ xres.  Don't bother with
    // optimising the boundaries, make the main loop tight.
    for (i = 0; i < height; i++) {
        gdouble *drow = field->data + (row + i)*xres + col;
        row_extend_mirror(field->data + (row + i)*xres, data_in,
                          col, width, xres, size);
        for (j = 0; j < width; j++) {
            // TODO
            /*
            drow[j] = buf[pos];
            buf[pos] = 0.0;
            pos = (pos + 1) % kres;
            k = (k0 + kres/2 + 1 + j) % mres;
            d = data_in[k < width ? k : mres-1 - k];
            for (k = pos; k < kres; k++)
                buf[k] += kdata[k - pos]*d;
            for (k = 0; k < pos; k++)
                buf[k] += kdata[kres + k - pos]*d;
                */
        }
    }

    g_free(data_in);
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
