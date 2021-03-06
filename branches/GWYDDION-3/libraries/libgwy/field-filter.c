/*
 *  $Id$
 *  Copyright (C) 2010-2013 David Nečas (Yeti).
 *  E-mail: yeti@gwyddion.net.
 *  Copyright (C) 2005 Rok Zitko.
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
// Including before fftw3.h ensures fftw_complex is C99 ‘double complex’.
#include <complex.h>
#include <fftw3.h>
#include "libgwy/macros.h"
#include "libgwy/strfuncs.h"
#include "libgwy/math.h"
#include "libgwy/fft.h"
#include "libgwy/field-statistics.h"
#include "libgwy/field-transform.h"
#include "libgwy/field-filter.h"
#include "libgwy/line-arithmetic.h"
#include "libgwy/line-statistics.h"
#include "libgwy/field-arithmetic.h"
#include "libgwy/object-internal.h"
#include "libgwy/math-internal.h"
#include "libgwy/field-internal.h"

// Permit to choose the algorithm explicitly in testing and benchmarking.
enum {
    CONVOLUTION_AUTO,
    CONVOLUTION_DIRECT,
    CONVOLUTION_FFT
};

typedef void (*RowExtendFunc)(const gdouble *in,
                              gdouble *out,
                              guint pos,
                              guint width,
                              guint res,
                              guint extend_left,
                              guint extend_right,
                              gdouble value);
typedef gdouble (*DoubleArrayFunc)(gdouble *results);

static guint convolution_method = CONVOLUTION_AUTO;

void
_gwy_tune_convolution_method(const gchar *method)
{
    g_return_if_fail(method);
    if (gwy_strequal(method, "auto"))
        convolution_method = CONVOLUTION_AUTO;
    else if (gwy_strequal(method, "direct"))
        convolution_method = CONVOLUTION_DIRECT;
    else if (gwy_strequal(method, "fft"))
        convolution_method = CONVOLUTION_FFT;
    else {
        g_warning("Unknown convolution method %s.", method);
    }
}

void
_gwy_ensure_defined_exterior(GwyExterior *exterior,
                             gdouble *fill_value)
{
    if (*exterior == GWY_EXTERIOR_UNDEFINED) {
        g_warning("Do not use GWY_EXTERIOR_UNDEFINED for convolutions and "
                  "correlations.  Fixing to zero-filled exterior.");
        *exterior = GWY_EXTERIOR_FIXED_VALUE;
        *fill_value = 0.0;
    }
}

// Generally, there are three reasonable convolution strategies:
// (a) small kernels (kres comparable to log(res)) → direct calculation
// (b) medium-sized kernels (kres ≤ res) → use explicit extension/mirroring to
//     a nice-for-FFT size ≥ 2res and cyclic convolution
// (c) large kernels (kres > res) → fold kernel to 2res size and use implicit
//     mirroring and DCT-II for data
//
// Strategy (c) is not implemented as the conditions are quite exotic,
// especially if res should be large.  Strategy (a) is implemented in a
// somewhat memory-hungry manner.

// Expand the kernel into an array of size @size, putting the central item
// at zero (or 0.5 pixel to the left for even sizes).
static inline void
extend_kernel_row(const gdouble *kernel, guint klen,
                  gdouble *extended, guint size)
{
    guint llen = klen/2, rlen = klen - llen;
    gwy_assign(extended, kernel + llen, rlen);
    gwy_clear(extended + rlen, size - klen);
    gwy_assign(extended + size - llen, kernel, llen);
}

static inline void
fill_block(gdouble *data, guint len, gdouble value)
{
    while (len--)
        *(data++) = value;
}

// Symmetrically means that for even @extsize-@size it holds
// @extend_begining=@extend_end while for an odd difference it holds
// @extend_begining+1=@extend_end, i.e. it's extended one pixel more at the
// end.
void
_gwy_make_symmetrical_extension(guint size, guint extsize,
                                guint *extend_begining, guint *extend_end)
{
    guint extend = extsize - size;
    *extend_begining = extend/2;
    *extend_end = extend - *extend_begining;
}

static inline void
row_extend_base(const gdouble *in, gdouble *out,
                guint *pos, guint *width, guint res,
                guint *extend_left, guint *extend_right)
{

    // Expand the ROI to the right as far as possible
    guint e2r = MIN(*extend_right, res - (*pos + *width));
    *width += e2r;
    *extend_right -= e2r;

    // Expand the ROI to the left as far as possible
    guint e2l = MIN(*extend_left, *pos);
    *width += e2l;
    *extend_left -= e2l;
    *pos -= e2l;

    // Direct copy of the ROI
    gwy_assign(out + *extend_left, in + *pos, *width);
}

/***
 * row_extend_foo:
 * @in: Input row of length @res.
 * @out: Output row of length @width+@extend_left+@extend_right.
 * @pos: Position in @in where the ROI starts.
 * @width: Length of ROI in @in.
 * @res: Total length of @in.
 * @extend_left: Number of pixels to extend the data to the left.
 * @extend_right: Number of pixels to extend the data to the right.
 * @value: Value to fill the exterior with for foo=fill, otherwise ignored.
 *
 * Extend a row.
 **/
static void
row_extend_mirror(const gdouble *in, gdouble *out,
                  guint pos, guint width, guint res,
                  guint extend_left, guint extend_right,
                  G_GNUC_UNUSED gdouble value)
{
    row_extend_base(in, out, &pos, &width, res, &extend_left, &extend_right);
    // Forward-extend
    guint res2 = 2*res;
    gdouble *out2 = out + extend_left + width;
    for (guint j = 0; j < extend_right; j++, out2++) {
        guint k = (pos + width + j) % res2;
        *out2 = (k < res) ? in[k] : in[res2-1 - k];
    }
    // Backward-extend
    guint k0 = (extend_left/res2 + 1)*res2;
    out2 = out + extend_left-1;
    for (guint j = 1; j <= extend_left; j++, out2--) {
        guint k = (k0 + pos - j) % res2;
        *out2 = (k < res) ? in[k] : in[res2-1 - k];
    }
}

static void
row_extend_periodic(const gdouble *in, gdouble *out,
                    guint pos, guint width, guint res,
                    guint extend_left, guint extend_right,
                    G_GNUC_UNUSED gdouble value)
{
    row_extend_base(in, out, &pos, &width, res, &extend_left, &extend_right);
    // Forward-extend
    gdouble *out2 = out + extend_left + width;
    for (guint j = 0; j < extend_right; j++, out2++) {
        guint k = (pos + width + j) % res;
        *out2 = in[k];
    }
    // Backward-extend
    guint k0 = (extend_left/res + 1)*res;
    out2 = out + extend_left-1;
    for (guint j = 1; j <= extend_left; j++, out2--) {
        guint k = (k0 + pos - j) % res;
        *out2 = in[k];
    }
}

static void
row_extend_border(const gdouble *in, gdouble *out,
                  guint pos, guint width, guint res,
                  guint extend_left, guint extend_right,
                  G_GNUC_UNUSED gdouble value)
{
    row_extend_base(in, out, &pos, &width, res, &extend_left, &extend_right);
    // Forward-extend
    fill_block(out + extend_left + width, extend_right, in[res-1]);
    // Backward-extend
    fill_block(out, extend_left, in[0]);
}

static void
row_extend_undef(const gdouble *in, gdouble *out,
                 guint pos, guint width, guint res,
                 guint extend_left, guint extend_right,
                 G_GNUC_UNUSED gdouble value)
{
    row_extend_base(in, out, &pos, &width, res, &extend_left, &extend_right);
    // That's all.  Happily keep uninitialised memory in the rest.
}

static void
row_extend_fill(const gdouble *in, gdouble *out,
                guint pos, guint width, guint res,
                guint extend_left, guint extend_right,
                gdouble value)
{
    row_extend_base(in, out, &pos, &width, res, &extend_left, &extend_right);
    // Forward-extend
    fill_block(out + extend_left + width, extend_right, value);
    // Backward-extend
    fill_block(out, extend_left, value);
}

static RowExtendFunc
get_row_extend_func(GwyExterior exterior)
{
    if (exterior == GWY_EXTERIOR_UNDEFINED)
        return &row_extend_undef;
    if (exterior == GWY_EXTERIOR_FIXED_VALUE)
        return &row_extend_fill;
    if (exterior == GWY_EXTERIOR_BORDER_EXTEND)
        return &row_extend_border;
    if (exterior == GWY_EXTERIOR_MIRROR_EXTEND)
        return &row_extend_mirror;
    if (exterior == GWY_EXTERIOR_PERIODIC)
        return &row_extend_periodic;
    g_return_val_if_reached(NULL);
}

static void
row_convolve_direct(const GwyField *field,
                    guint col, guint row,
                    guint width, guint height,
                    GwyField *target,
                    guint targetcol, guint targetrow,
                    const GwyLine *kernel,
                    RowExtendFunc extend_row,
                    gdouble fill_value)
{
    guint xres = field->xres;
    guint kres = kernel->res;
    const gdouble *kdata = kernel->data;
    guint size = width + kres - 1;
    gdouble *extdata = g_new(gdouble, size);
    guint extend_left, extend_right;
    _gwy_make_symmetrical_extension(width, size, &extend_left, &extend_right);

    // The direct method is used only if kres ≪ res.  Don't bother optimising
    // the boundaries, just make the inner loop tight.
    for (guint i = 0; i < height; i++) {
        gdouble *trow = target->data + (targetrow + i)*target->xres + targetcol;
        extend_row(field->data + (row + i)*xres, extdata,
                   col, width, xres, extend_left, extend_right, fill_value);
        for (guint j = 0; j < width; j++) {
            const gdouble *d = extdata + extend_left + kres/2 + j;
            gdouble v = 0.0;
            for (guint k = 0; k < kres; k++, d--)
                v += kdata[k] * *d;
            trow[j] = v;
        }
    }

    g_free(extdata);
}

static void
row_convolve_fft(const GwyField *field,
                 guint col, guint row,
                 guint width, guint height,
                 GwyField *target,
                 guint targetcol, guint targetrow,
                 const GwyLine *kernel,
                 RowExtendFunc extend_row,
                 gdouble fill_value)
{
    guint xres = field->xres, kres = kernel->res;
    guint size = gwy_fft_nice_transform_size(width + kres - 1);
    // The innermost (contiguous) dimension of R2C the complex output is
    // slightly larger than the real input.  Note @cstride is measured in
    // gwycomplex, multiply it by 2 for doubles.
    guint cstride = size/2 + 1;
    gdouble *extdata = fftw_alloc_real(size);
    gwycomplex *datac = fftw_alloc_complex(2*cstride);
    gwycomplex *kernelc = datac + cstride;
    // The R2C plan for transforming the extended data row (or kernel).
    fftw_plan dplan = fftw_plan_dft_r2c_1d(size, extdata, datac,
                                           FFTW_DESTROY_INPUT
                                            | _gwy_fft_rigour());
    g_assert(dplan);
    // The C2R plan the backward transform of the convolution of each row.
    fftw_plan cplan = fftw_plan_dft_c2r_1d(size, datac, extdata,
                                           _gwy_fft_rigour());
    g_assert(cplan);

    // Transform the kernel.
    extend_kernel_row(kernel->data, kres, extdata, size);
    fftw_execute(dplan);
    gwy_assign(kernelc, datac, cstride);

    // Convolve rows
    guint extend_left, extend_right;
    _gwy_make_symmetrical_extension(width, size, &extend_left, &extend_right);
    for (guint i = 0; i < height; i++) {
        extend_row(field->data + (row + i)*xres, extdata,
                   col, width, xres, extend_left, extend_right, fill_value);
        fftw_execute(dplan);
        for (guint j = 0; j < cstride; j++)
            datac[j] *= kernelc[j]/size;
        fftw_execute(cplan);
        gwy_assign(target->data + (targetrow + i)*target->xres + targetcol,
                   extdata + extend_left,
                   width);
    }

    fftw_destroy_plan(cplan);
    fftw_destroy_plan(dplan);
    fftw_free(datac);
    fftw_free(extdata);
}

/**
 * gwy_field_row_convolve:
 * @field: A two-dimensional data field.
 * @fpart: (allow-none):
 *         Area in @field to process.  Pass %NULL to process entire @field.
 * @target: A two-dimensional data field where the result will be placed.
 *          It may be @field for an in-place modification.  Its dimensions may
 *          match either @field or @fpart.  In the former case the
 *          placement of result is determined by @fpart; in the latter case
 *          the result fills the entire @target.
 * @kernel: Kernel to convolve @field with.
 * @exterior: Exterior pixels handling.
 * @fill_value: The value to use with %GWY_EXTERIOR_FIXED_VALUE exterior.
 *
 * Convolve a field with a horizontal kernel.
 *
 * The convolution is performed with the kernel centred on the respective field
 * pixels.  For an odd-sized kernel this holds precisely.  For an even-sized
 * kernel this means the kernel centre is placed 0.5 pixel to the left
 * (towards lower column indices) from the respective field pixel.
 *
 * See gwy_field_extend() for what constitutes the exterior and how it is
 * handled.
 **/
void
gwy_field_row_convolve(const GwyField *field,
                       const GwyFieldPart *fpart,
                       GwyField *target,
                       const GwyLine *kernel,
                       GwyExterior exterior,
                       gdouble fill_value)
{
    guint col, row, width, height, targetcol, targetrow;
    if (!gwy_field_check_part(field, fpart, &col, &row, &width, &height)
        || !gwy_field_check_target(field, target,
                                   &(GwyFieldPart){ col, row, width, height },
                                   &targetcol, &targetrow))
        return;

    g_return_if_fail(GWY_IS_LINE(kernel));

    _gwy_ensure_defined_exterior(&exterior, &fill_value);
    RowExtendFunc extend_row = get_row_extend_func(exterior);
    if (!extend_row)
        return;

    // The threshold was estimated empirically.  See benchmarks/convolve-row.c
    if (convolution_method == CONVOLUTION_DIRECT
        || (convolution_method == CONVOLUTION_AUTO
            && (width <= 12 || kernel->res <= 3.0*(log(width) - 1.0))))
        row_convolve_direct(field, col, row, width, height,
                            target, targetcol, targetrow,
                            kernel, extend_row, fill_value);
    else
        row_convolve_fft(field, col, row, width, height,
                         target, targetcol, targetrow,
                         kernel, extend_row, fill_value);

    gwy_field_invalidate(target);
}

void
_gwy_extend_kernel_rect(const gdouble *kernel,
                        guint kxlen, guint kylen,
                        gdouble *extended,
                        guint xsize, guint ysize, guint rowstride)
{
    guint ulen = kylen/2, dlen = kylen - ulen;
    for (guint i = 0; i < dlen; i++)
        extend_kernel_row(kernel + (i + ulen)*kxlen, kxlen,
                          extended + i*rowstride, xsize);
    gwy_clear(extended + dlen*rowstride, (ysize - kylen)*rowstride);
    for (guint i = 0; i < ulen; i++)
        extend_kernel_row(kernel + i*kxlen, kxlen,
                          extended + (ysize - ulen + i)*rowstride, xsize);
}

static inline void
rect_extend_base(const gdouble *in, guint inrowstride,
                 gdouble *out, guint outrowstride,
                 guint xpos, guint *ypos,
                 guint width, guint *height,
                 guint xres, guint yres,
                 guint extend_left, guint extend_right,
                 guint *extend_up, guint *extend_down,
                 RowExtendFunc extend_row, gdouble fill_value)
{
    // Expand the ROI down as far as possible
    guint e2r = MIN(*extend_down, yres - (*ypos + *height));
    *height += e2r;
    *extend_down -= e2r;

    // Expand the ROI up as far as possible
    guint e2l = MIN(*extend_up, *ypos);
    *height += e2l;
    *extend_up -= e2l;
    *ypos -= e2l;

    // Row-wise extension within the vertical range of the ROI
    for (guint i = 0; i < *height; i++)
        extend_row(in + (*ypos + i)*inrowstride,
                   out + (*extend_up + i)*outrowstride,
                   xpos, width, xres, extend_left, extend_right, fill_value);
}

static void
rect_extend_mirror(const gdouble *in, guint inrowstride,
                   gdouble *out, guint outrowstride,
                   guint xpos, guint ypos,
                   guint width, guint height,
                   guint xres, guint yres,
                   guint extend_left, guint extend_right,
                   guint extend_up, guint extend_down,
                   G_GNUC_UNUSED gdouble value)
{
    rect_extend_base(in, inrowstride, out, outrowstride,
                     xpos, &ypos, width, &height, xres, yres,
                     extend_left, extend_right, &extend_up, &extend_down,
                     &row_extend_mirror, value);
    // Forward-extend
    guint yres2 = 2*yres;
    gdouble *out2 = out + outrowstride*(extend_up + height);
    for (guint i = 0; i < extend_down; i++, out2 += outrowstride) {
        guint k = (ypos + height + i) % yres2;
        if (k >= yres)
            k = yres2-1 - k;
        row_extend_mirror(in + k*inrowstride, out2,
                          xpos, width, xres, extend_left, extend_right, value);
    }
    // Backward-extend
    guint k0 = (extend_up/yres2 + 1)*yres2;
    out2 = out + outrowstride*(extend_up - 1);
    for (guint i = 1; i <= extend_up; i++, out2 -= outrowstride) {
        guint k = (k0 + ypos - i) % yres2;
        if (k >= yres)
            k = yres2-1 - k;
        row_extend_mirror(in + k*inrowstride, out2,
                          xpos, width, xres, extend_left, extend_right, value);
    }
}

static void
rect_extend_periodic(const gdouble *in, guint inrowstride,
                     gdouble *out, guint outrowstride,
                     guint xpos, guint ypos,
                     guint width, guint height,
                     guint xres, guint yres,
                     guint extend_left, guint extend_right,
                     guint extend_up, guint extend_down,
                     G_GNUC_UNUSED gdouble value)
{
    rect_extend_base(in, inrowstride, out, outrowstride,
                     xpos, &ypos, width, &height, xres, yres,
                     extend_left, extend_right, &extend_up, &extend_down,
                     &row_extend_periodic, value);
    // Forward-extend
    gdouble *out2 = out + outrowstride*(extend_up + height);
    for (guint i = 0; i < extend_down; i++, out2 += outrowstride) {
        guint k = (ypos + height + i) % yres;
        row_extend_periodic(in + k*inrowstride, out2,
                            xpos, width, xres, extend_left, extend_right, value);
    }
    // Backward-extend
    guint k0 = (extend_up/yres + 1)*yres;
    out2 = out + outrowstride*(extend_up - 1);
    for (guint i = 1; i <= extend_up; i++, out2 -= outrowstride) {
        guint k = (k0 + ypos - i) % yres;
        row_extend_periodic(in + k*inrowstride, out2,
                            xpos, width, xres, extend_left, extend_right, value);
    }
}

static void
rect_extend_border(const gdouble *in, guint inrowstride,
                   gdouble *out, guint outrowstride,
                   guint xpos, guint ypos,
                   guint width, guint height,
                   guint xres, guint yres,
                   guint extend_left, guint extend_right,
                   guint extend_up, guint extend_down,
                   G_GNUC_UNUSED gdouble value)
{
    rect_extend_base(in, inrowstride, out, outrowstride,
                     xpos, &ypos, width, &height, xres, yres,
                     extend_left, extend_right, &extend_up, &extend_down,
                     &row_extend_border, value);
    // Forward-extend
    gdouble *out2 = out + outrowstride*(extend_up + height);
    for (guint i = 0; i < extend_down; i++, out2 += outrowstride)
        row_extend_border(in + (yres-1)*inrowstride, out2,
                          xpos, width, xres, extend_left, extend_right, value);
    // Backward-extend
    out2 = out + outrowstride*(extend_up - 1);
    for (guint i = 1; i <= extend_up; i++, out2 -= outrowstride)
        row_extend_border(in, out2,
                          xpos, width, xres, extend_left, extend_right, value);
}

static void
rect_extend_undef(const gdouble *in, guint inrowstride,
                  gdouble *out, guint outrowstride,
                  guint xpos, guint ypos,
                  guint width, guint height,
                  guint xres, guint yres,
                  guint extend_left, guint extend_right,
                  guint extend_up, guint extend_down,
                  G_GNUC_UNUSED gdouble value)
{
    rect_extend_base(in, inrowstride, out, outrowstride,
                     xpos, &ypos, width, &height, xres, yres,
                     extend_left, extend_right, &extend_up, &extend_down,
                     &row_extend_undef, value);
    // That's all.  Happily keep uninitialised memory in the rest.
}

static void
rect_extend_fill(const gdouble *in, guint inrowstride,
                 gdouble *out, guint outrowstride,
                 guint xpos, guint ypos,
                 guint width, guint height,
                 guint xres, guint yres,
                 guint extend_left, guint extend_right,
                 guint extend_up, guint extend_down,
                 gdouble value)
{
    rect_extend_base(in, inrowstride, out, outrowstride,
                     xpos, &ypos, width, &height, xres, yres,
                     extend_left, extend_right, &extend_up, &extend_down,
                     &row_extend_fill, value);
    // Forward-extend
    gdouble *out2 = out + outrowstride*(extend_up + height);
    for (guint i = 0; i < extend_down; i++, out2 += outrowstride)
        fill_block(out2, extend_left + width + extend_right, value);
    // Backward-extend
    out2 = out + outrowstride*(extend_up - 1);
    for (guint i = 1; i <= extend_up; i++, out2 -= outrowstride)
        fill_block(out2, extend_left + width + extend_right, value);
}

RectExtendFunc
_gwy_get_rect_extend_func(GwyExterior exterior)
{
    if (exterior == GWY_EXTERIOR_UNDEFINED)
        return &rect_extend_undef;
    if (exterior == GWY_EXTERIOR_FIXED_VALUE)
        return &rect_extend_fill;
    if (exterior == GWY_EXTERIOR_BORDER_EXTEND)
        return &rect_extend_border;
    if (exterior == GWY_EXTERIOR_MIRROR_EXTEND)
        return &rect_extend_mirror;
    if (exterior == GWY_EXTERIOR_PERIODIC)
        return &rect_extend_periodic;
    g_return_val_if_reached(NULL);
}

/**
 * multiconvolve_direct:
 * @field: A two-dimensional data field.
 * @col: First ROI column.
 * @row: First RIO row.
 * @width: ROI width.
 * @height: RIO height.
 * @target: A two-dimensional data field where the result will be placed.
 *          It may be @field itself.
 * @targetcol: Column to place the result into @target.
 * @targetrow: Target to place the result into @target.
 * @kernel: Array of @nkernel equally-sized kernel.
 * @nkernel: Number of items in @kernel.
 * @combine_results: Function to combine results of individual convolutions
 *                   to the final result put to @target.  May be %NULL if
 *                   @nkernel is 1.
 * @extend_rect: Rectangle extending method.
 * @fill_value: The value to use with fixed-value exterior.
 *
 * Performs convolution of a field with a number of equally-sized kenrels,
 * combining the results of individual convolutions into a single value.
 */
static void
multiconvolve_direct(const GwyField *field,
                     guint col, guint row,
                     guint width, guint height,
                     GwyField *target,
                     guint targetcol, guint targetrow,
                     const GwyField **kernel,
                     guint nkernel,
                     DoubleArrayFunc combine_results,
                     RectExtendFunc extend_rect,
                     gdouble fill_value)
{
    g_return_if_fail(nkernel);
    g_return_if_fail(kernel);
    g_return_if_fail(nkernel == 1 || combine_results);

    guint xres = field->xres, yres = field->yres,
          kxres = kernel[0]->xres, kyres = kernel[0]->yres;
    for (guint kno = 1; kno < nkernel; kno++) {
        g_return_if_fail(kernel[kno]->xres == kxres
                         && kernel[kno]->yres == kyres);
    }

    guint xsize = width + kxres - 1;
    guint ysize = height + kyres - 1;
    gdouble *extdata = g_new(gdouble, xsize*ysize);
    guint extend_left, extend_right, extend_up, extend_down;
    _gwy_make_symmetrical_extension(width, xsize, &extend_left, &extend_right);
    _gwy_make_symmetrical_extension(height, ysize, &extend_up, &extend_down);

    extend_rect(field->data, xres, extdata, xsize,
                col, row, width, height, xres, yres,
                extend_left, extend_right, extend_up, extend_down, fill_value);

    // The direct method is used only if kres ≪ res.  Don't bother optimising
    // the boundaries, just make the inner loop tight.
    if (nkernel == 1) {
        const gdouble *kdata = kernel[0]->data;
        for (guint i = 0; i < height; i++) {
            gdouble *trow = target->data + ((targetrow + i)*target->xres
                                            + targetcol);
            for (guint j = 0; j < width; j++) {
                const gdouble *id = extdata + (extend_up + kyres/2 + i)*xsize;
                gdouble v = 0.0;
                for (guint ik = 0; ik < kyres; ik++, id -= xsize) {
                    const gdouble *jd = id + extend_left + kxres/2 + j;
                    const gdouble *krow = kdata + ik*kxres;
                    for (guint jk = 0; jk < kxres; jk++, jd--)
                        v += krow[jk] * *jd;
                }
                trow[j] = v;
            }
        }
    }
    else {
        for (guint i = 0; i < height; i++) {
            gdouble *trow = target->data + ((targetrow + i)*target->xres
                                            + targetcol);
            for (guint j = 0; j < width; j++) {
                gdouble results[nkernel];
                for (guint kno = 0; kno < nkernel; kno++) {
                    const gdouble *id = extdata + (extend_up
                                                   + kyres/2 + i)*xsize;
                    const gdouble *kdata = kernel[kno]->data;
                    gdouble v = 0.0;
                    for (guint ik = 0; ik < kyres; ik++, id -= xsize) {
                        const gdouble *jd = id + extend_left + kxres/2 + j;
                        const gdouble *krow = kdata + ik*kxres;
                        for (guint jk = 0; jk < kxres; jk++, jd--)
                            v += krow[jk] * *jd;
                    }
                    results[kno] = v;
                }
                trow[j] = combine_results(results);
            }
        }
    }

    g_free(extdata);
}

static void
convolve_fft(const GwyField *field,
             guint col, guint row,
             guint width, guint height,
             GwyField *target,
             guint targetcol, guint targetrow,
             const GwyField *kernel,
             RectExtendFunc extend_rect,
             gdouble fill_value)
{
    guint xres = field->xres, yres = field->yres,
          kxres = kernel->xres, kyres = kernel->yres;
    guint xsize = gwy_fft_nice_transform_size(width + kxres - 1);
    guint ysize = gwy_fft_nice_transform_size(height + kyres - 1);
    // The innermost (contiguous) dimension of R2C the complex output is
    // slightly larger than the real input.  If the transform is in-place the
    // input array needs to be padded.  Note @cstride is measured in
    // gwycomplex, multiply it by 2 for doubles.
    guint cstride = xsize/2 + 1;
    guint extend_left, extend_right, extend_up, extend_down;
    _gwy_make_symmetrical_extension(width, xsize, &extend_left, &extend_right);
    _gwy_make_symmetrical_extension(height, ysize, &extend_up, &extend_down);
    // Use in-place transforms.  Let FFTW figure out whether allocating
    // temporary buffers worths it or not.
    gwycomplex *datac = fftw_alloc_complex(cstride*ysize);
    gdouble *extdata = (gdouble*)datac;
    gwycomplex *kernelc = fftw_alloc_complex(cstride*ysize);
    // The R2C plan for transforming the extended kernel.  The input is in
    // extdata to make it an out-of-place transform (this also means the input
    // data row stride is just xsize, not 2*cstride).
    fftw_plan kplan = fftw_plan_dft_r2c_2d(ysize, xsize, extdata, kernelc,
                                           FFTW_DESTROY_INPUT
                                           | _gwy_fft_rigour());
    g_assert(kplan);
    // The R2C plan for transforming the extended data.  This one is in-place.
    fftw_plan dplan = fftw_plan_dft_r2c_2d(ysize, xsize, extdata, datac,
                                           FFTW_DESTROY_INPUT
                                           | _gwy_fft_rigour());
    g_assert(dplan);
    // The C2R plan the backward transform of the convolution.  The input
    // is in fact in kernelc to make it an out-of-place transform.  So, again,
    // the output has cstride of only xsize.
    fftw_plan cplan = fftw_plan_dft_c2r_2d(ysize, xsize, kernelc, extdata,
                                           _gwy_fft_rigour());
    g_assert(cplan);

    // Transform the kernel.
    _gwy_extend_kernel_rect(kernel->data, kxres, kyres,
                            extdata, xsize, ysize, xsize);
    fftw_execute(kplan);

    // Convolve
    extend_rect(field->data, xres, extdata, 2*cstride,
                col, row, width, height, xres, yres,
                extend_left, extend_right, extend_up, extend_down, fill_value);
    fftw_execute(dplan);

    for (guint k = 0; k < cstride*ysize; k++)
        kernelc[k] *= datac[k]/(xsize*ysize);
    fftw_execute(cplan);

    for (guint i = 0; i < height; i++)
        gwy_assign(target->data + (targetrow + i)*target->xres + targetcol,
                   extdata + (extend_up + i)*xsize + extend_left,
                   width);

    fftw_destroy_plan(kplan);
    fftw_destroy_plan(cplan);
    fftw_destroy_plan(dplan);
    fftw_free(kernelc);
    fftw_free(datac);
}

/**
 * gwy_field_convolve:
 * @field: A two-dimensional data field.
 * @fpart: (allow-none):
 *         Area in @field to process.  Pass %NULL to process entire @field.
 * @target: A two-dimensional data field where the result will be placed.
 *          It may be @field for an in-place modification.  Its dimensions may
 *          match either @field or @fpart.  In the former case the
 *          placement of result is determined by @fpart; in the latter case
 *          the result fills the entire @target.
 * @kernel: Kernel to convolve @field with.
 * @exterior: Exterior pixels handling.
 * @fill_value: The value to use with %GWY_EXTERIOR_FIXED_VALUE exterior.
 *
 * Convolve a field with a two-dimensional kernel.
 *
 * The convolution is performed with the kernel centred on the respective field
 * pixels.  For directions in which the kernel has an odd size this holds
 * precisely.  For an even-sized kernel this means the kernel centre is placed
 * 0.5 pixel left or up (towards lower indices) from the respective field
 * pixel.
 *
 * See gwy_field_extend() for what constitutes the exterior and how it is
 * handled.
 **/
void
gwy_field_convolve(const GwyField *field,
                   const GwyFieldPart *fpart,
                   GwyField *target,
                   const GwyField *kernel,
                   GwyExterior exterior,
                   gdouble fill_value)
{
    guint col, row, width, height, targetcol, targetrow;
    if (!gwy_field_check_part(field, fpart, &col, &row, &width, &height)
        || !gwy_field_check_target(field, target,
                                   &(GwyFieldPart){ col, row, width, height },
                                   &targetcol, &targetrow))
        return;

    g_return_if_fail(GWY_IS_FIELD(kernel));

    _gwy_ensure_defined_exterior(&exterior, &fill_value);
    RectExtendFunc extend_rect = _gwy_get_rect_extend_func(exterior);
    if (!extend_rect)
        return;

    // The threshold was estimated empirically.  See benchmarks/convolve-rect.c
    guint size = height*width;
    if (convolution_method == CONVOLUTION_DIRECT
        || (convolution_method == CONVOLUTION_AUTO && size <= 25))
        multiconvolve_direct(field, col, row, width, height,
                             target, targetcol, targetrow,
                             &kernel, 1, NULL, extend_rect, fill_value);
    else {
        convolve_fft(field, col, row, width, height,
                     target, targetcol, targetrow,
                     kernel, extend_rect, fill_value);
    }

    if (target != field) {
        _gwy_assign_unit(&target->priv->xunit, field->priv->xunit);
        _gwy_assign_unit(&target->priv->yunit, field->priv->yunit);
    }

    if (target->priv->zunit || field->priv->zunit) {
        // Force instantiation of units
        gwy_unit_multiply(gwy_field_get_zunit(target),
                          gwy_field_get_zunit(kernel),
                          gwy_field_get_zunit(field));
    }

    gwy_field_invalidate(target);
}

/**
 * gwy_field_new_extended:
 * @field: A two-dimensional data field.
 * @fpart: (allow-none):
 *         Area in @field to extend.  Pass %NULL to extend entire @field.
 * @left: Number of pixels to extend to the left (towards lower column indices).
 * @right: Number of pixels to extend to the right (towards higher column
 *         indices).
 * @up: Number of pixels to extend up (towards lower row indices).
 * @down: Number of pixels to extend down (towards higher row indices).
 * @exterior: Exterior pixels handling.
 * @fill_value: The value to use with %GWY_EXTERIOR_FIXED_VALUE exterior.
 * @keep_offsets: %TRUE to set the X and Y offsets of the new field
 *                using @fpart and @field offsets.  %FALSE to set offsets
 *                of the new field to zeroes.
 *
 * Creates a new field by extending another field using the specified method of
 * exterior handling.
 *
 * Data outside the <emphasis>entire field</emphasis> are represented using the
 * specified exterior handling method.  However, data outside the specified
 * rectangle but still inside the field are taken from the field.  This means
 * that for a sufficiently small rectangle and extensions this method reduces
 * to mere gwy_field_new_part().
 *
 * If you need to extend a rectangular part of a field as if it was an entire
 * field then extract the part to a temporary field and use gwy_field_extend()
 * on that.
 *
 * Returns: (transfer full):
 *          A newly created field.
 **/
GwyField*
gwy_field_new_extended(const GwyField *field,
                       const GwyFieldPart *fpart,
                       guint left, guint right,
                       guint up, guint down,
                       GwyExterior exterior,
                       gdouble fill_value,
                       gboolean keep_offsets)
{
    guint col, row, width, height;
    if (!gwy_field_check_part(field, fpart, &col, &row, &width, &height))
        return NULL;

    RectExtendFunc extend_rect = _gwy_get_rect_extend_func(exterior);
    if (!extend_rect)
        return NULL;

    GwyField *result = gwy_field_new_sized(width + left + right,
                                           height + up + down,
                                           FALSE);
    extend_rect(field->data, field->xres, result->data, result->xres,
                col, row, width, height, field->xres, field->yres,
                left, right, up, down, fill_value);

    gdouble dx = gwy_field_dx(field), dy = gwy_field_dy(field);
    // Can just assign the values as no one is watching yet.
    result->xreal = (width + left + right)*dx;
    result->yreal = (height + up + down)*dy;
    if (keep_offsets) {
        result->xoff = field->xoff + col*dx - left*dx;
        result->yoff = field->yoff + row*dy - up*dy;
    }
    _gwy_assign_unit(&result->priv->xunit, field->priv->xunit);
    _gwy_assign_unit(&result->priv->yunit, field->priv->yunit);
    _gwy_assign_unit(&result->priv->zunit, field->priv->zunit);

    return result;
}

/**
 * gwy_field_extend:
 * @field: A two-dimensional data field.
 * @fpart: (allow-none):
 *         Area in @field to extend.  Pass %NULL to extend entire @field.
 * @target: A two-dimensional data field where the result will be placed.
 *          It must not be @field itself.
 * @left: Number of pixels to extend to the left (towards lower column indices).
 * @right: Number of pixels to extend to the right (towards higher column
 *         indices).
 * @up: Number of pixels to extend up (towards lower row indices).
 * @down: Number of pixels to extend down (towards higher row indices).
 * @exterior: Exterior pixels handling.
 * @fill_value: The value to use with %GWY_EXTERIOR_FIXED_VALUE exterior.
 * @keep_offsets: %TRUE to set the X and Y offsets of the new field
 *                using @fpart and @field offsets.  %FALSE to set offsets
 *                of the new field to zeroes.
 *
 * Extends a field using the specified method of exterior handling into another
 * field.
 *
 * The target field is resized, as necessary, and its dimensions and offsets
 * are recalculated based on @field.
 *
 * See gwy_field_new_extended() for exterior discussion.
 **/
void
gwy_field_extend(const GwyField *field,
                 const GwyFieldPart *fpart,
                 GwyField *target,
                 guint left, guint right,
                 guint up, guint down,
                 GwyExterior exterior,
                 gdouble fill_value,
                 gboolean keep_offsets)
{
    g_return_if_fail(GWY_IS_FIELD(target));
    g_return_if_fail((const GwyField*)target != field);

    guint col, row, width, height;
    if (!gwy_field_check_part(field, fpart, &col, &row, &width, &height))
        return;

    RectExtendFunc extend_rect = _gwy_get_rect_extend_func(exterior);
    if (!extend_rect)
        return;

    gwy_field_set_size(target, width + left + right, height + up + down, FALSE);
    extend_rect(field->data, field->xres, target->data, target->xres,
                col, row, width, height, field->xres, field->yres,
                left, right, up, down, fill_value);

    gdouble dx = gwy_field_dx(field), dy = gwy_field_dy(field);
    gwy_field_set_xreal(target, (width + left + right)*dx);
    gwy_field_set_yreal(target, (height + up + down)*dy);
    if (keep_offsets) {
        gwy_field_set_xoffset(target, field->xoff + col*dx - left*dx);
        gwy_field_set_yoffset(target, field->yoff + row*dy - up*dy);
    }
    else {
        gwy_field_set_xoffset(target, 0.0);
        gwy_field_set_yoffset(target, 0.0);
    }
    _gwy_assign_unit(&target->priv->xunit, field->priv->xunit);
    _gwy_assign_unit(&target->priv->yunit, field->priv->yunit);
    _gwy_assign_unit(&target->priv->zunit, field->priv->zunit);
}

static gboolean
make_gaussian_kernel(GwyLine *kernel,
                     gdouble sigma)
{
    // Smaller values lead to underflow even in the first non-central term.
    if (sigma < 0.026)
        return FALSE;

    // Exclude terms smaller than machine eps compared to the central term.
    guint res = 2*(guint)ceil(8.0*sigma) + 1;
    // But do at least a 3-term filter so that they don't say we didn't try.
    res = MAX(res, 3);
    gdouble center = (res - 1.0)/2.0;
    double s = 0.0;

    gwy_line_set_size(kernel, res, FALSE);
    for (guint k = 0; k < res; k++) {
        gdouble x = k - center;
        x /= sigma;
        s += kernel->data[k] = exp(-x*x/2.0);
    }
    gwy_line_multiply_full(kernel, 1.0/s);

    return TRUE;
}

/**
 * gwy_field_filter_gaussian:
 * @field: A two-dimensional data field.
 * @fpart: (allow-none):
 *         Area in @field to process.  Pass %NULL to process entire @field.
 * @target: A two-dimensional data field where the result will be placed.
 *          It may be @field for an in-place modification.  Its dimensions may
 *          match either @field or @fpart.  In the former case the
 *          placement of result is determined by @fpart; in the latter case
 *          the result fills the entire @target.
 * @hsigma: Horizontal sigma parameter (square root of dispersion) of the
 *          Gaussian, in pixels.
 * @vsigma: Vertical sigma parameter (square root of dispersion) of the
 *          Gaussian, in pixels.
 * @exterior: Exterior pixels handling.
 * @fill_value: The value to use with %GWY_EXTERIOR_FIXED_VALUE exterior.
 *
 * Filters a field with a Gaussian filter.
 *
 * The Gausian is normalised, i.e. the filter is sum-preserving.
 *
 * Pass the same @hsigma and @vsigma for a pixel-wise rotationally symmetrical
 * Gaussian which is usually wanted.  However, if the horizontal and vertical
 * physical pixel dimensions differ or filtering only in one direction is
 * required then passing different @hsigma and @vsigma can be useful.
 **/
void
gwy_field_filter_gaussian(const GwyField *field,
                          const GwyFieldPart* rectangle,
                          GwyField *target,
                          gdouble hsigma, gdouble vsigma,
                          GwyExterior exterior,
                          gdouble fill_value)
{
    // The fields and rectangle are checked by the actual convolution funcs.
    g_return_if_fail(hsigma >= 0.0 && vsigma >= 0.0);

    // The common case.
    if (hsigma == vsigma) {
        GwyLine *linekernel = gwy_line_new();
        if (make_gaussian_kernel(linekernel, hsigma)) {
            GwyField *kernel = gwy_line_outer_product(linekernel, linekernel);
            gwy_field_convolve(field, rectangle, target, kernel,
                               exterior, fill_value);
            g_object_unref(kernel);
        }
        g_object_unref(linekernel);
        return;
    }

    // The general case.
    GwyLine *rowkernel = gwy_line_new(), *colkernel = gwy_line_new();
    make_gaussian_kernel(rowkernel, hsigma);
    if (make_gaussian_kernel(colkernel, vsigma)) {
        GwyField *kernel = gwy_line_outer_product(colkernel, rowkernel);
        gwy_field_convolve(field, rectangle, target, kernel,
                           exterior, fill_value);
        g_object_unref(kernel);
    }
    else {
        gwy_field_row_convolve(field, rectangle, target, rowkernel,
                               exterior, fill_value);
    }
    g_object_unref(colkernel);
    g_object_unref(rowkernel);
}

/**
 * kuwahara_block:
 * @a: points to a 5x5 matrix (array of 25 doubles)
 *
 * Computes a new value of the center pixel according to the Kuwahara filter.
 *
 * Returns: Filtered value.
 */
static gdouble
kuwahara_block(gdouble *a)
{
   static const guint r[4][9] = {
       { 0,  1,  2,  5,  6,  7,  10, 11, 12, },
       { 2,  3,  4,  7,  8,  9,  12, 13, 14, },
       { 12, 13, 14, 17, 18, 19, 22, 23, 24, },
       { 10, 11, 12, 15, 16, 17, 20, 21, 22, },
   };
   gdouble mean[4], var[4];

   gwy_clear(mean, 4);
   for (guint i = 0; i < 9; i++) {
       for (guint j = 0; j < 4; j++) {
           gdouble z = a[r[j][i]];
           mean[j] += z;
       }
   }
   for (guint j = 0; j < 4; j++)
       mean[j] /= 9.0;
   gwy_clear(var, 4);
   for (guint i = 0; i < 9; i++) {
       for (guint j = 0; j < 4; j++) {
           gdouble z = a[r[j][i]] - mean[j];
           var[j] += z*z;
       }
   }

   if (var[0] <= var[1]) {
       if (var[0] <= var[2])
           return (var[0] <= var[3]) ? mean[0] : mean[3];
       else
           return (var[2] <= var[3]) ? mean[2] : mean[3];
   }
   else {
       if (var[1] <= var[2])
           return (var[1] <= var[3]) ? mean[1] : mean[3];
       else
           return (var[2] <= var[3]) ? mean[2] : mean[3];
   }
}

static gdouble
step_block(gdouble *a)
{
    // We don't use the four corner elements, replace them by values from
    // the end of the array that we use.
    a[0] = a[21];
    a[4] = a[22];
    a[20] = a[23];
    gwy_math_sort(a, NULL, 21);
    return a[15] - a[5];
}

static gdouble
nonlinearity_block(gdouble *a,
                   gdouble dx,
                   gdouble dy)
{
    gdouble m = (a[1] + a[2] + a[3]
                 + a[5] + a[6] + a[7] + a[8] + a[9]
                 + a[10] + a[11] + a[12] + a[13] + a[14]
                 + a[15] + a[16] + a[17] + a[18] + a[19]
                 + a[21] + a[22] + a[23])/21.0;
    gdouble bx = (((a[3] - a[1]) + (a[8] - a[6]) + (a[13] - a[11])
                   + (a[18] - a[16]) + (a[23] - a[21]))/32.0
                  + ((a[9] - a[5]) + (a[14] - a[10])
                     + (a[19] - a[15]))/16.0)/dx;
    gdouble by = (((a[15] - a[5]) + (a[16] - a[6]) + (a[17] - a[7])
                   + (a[18] - a[8]) + (a[19] - a[9]))/32.0
                  + ((a[21] - a[1]) + (a[22] - a[2])
                     + (a[23] - a[3]))/16.0)/dy;
    gdouble w2 = bx*bx + by*by;
    gdouble s2 = ((a[1] - m)*(a[1] - m) + (a[2] - m)*(a[2] - m)
                  + (a[3] - m)*(a[3] - m) + (a[5] - m)*(a[5] - m)
                  + (a[6] - m)*(a[6] - m) + (a[7] - m)*(a[7] - m)
                  + (a[8] - m)*(a[8] - m) + (a[9] - m)*(a[9] - m)
                  + (a[10] - m)*(a[10] - m) + (a[11] - m)*(a[11] - m)
                  + (a[12] - m)*(a[12] - m) + (a[13] - m)*(a[13] - m)
                  + (a[14] - m)*(a[14] - m) + (a[15] - m)*(a[15] - m)
                  + (a[16] - m)*(a[16] - m) + (a[17] - m)*(a[17] - m)
                  + (a[18] - m)*(a[18] - m) + (a[19] - m)*(a[19] - m)
                  + (a[21] - m)*(a[21] - m) + (a[22] - m)*(a[22] - m)
                  + (a[23] - m)*(a[23] - m))/21.0;
    gdouble nlin = (s2 - w2)/(1.0 + w2);

    // It should be > 0 except for rounding errors.
    return nlin > 0.0 ? sqrt(nlin) : 0.0;
}

static void
filter_5x5(const GwyField *field,
           const GwyFieldPart *fpart,
           GwyField *target,
           GwyStandardFilter filter,
           GwyExterior exterior,
           gdouble fill_value)
{
    guint col, row, width, height, targetcol, targetrow;
    if (!gwy_field_check_part(field, fpart, &col, &row, &width, &height)
        || !gwy_field_check_target(field, target,
                                   &(GwyFieldPart){ col, row, width, height },
                                   &targetcol, &targetrow))
        return;

    RectExtendFunc extend_rect = _gwy_get_rect_extend_func(exterior);
    if (!extend_rect)
        return;

    guint xres = field->xres, yres = field->yres;
    guint xsize = width + 4;
    guint ysize = height + 4;
    gdouble dx = gwy_field_dx(field), dy = gwy_field_dy(field);
    gdouble *extdata = g_new(gdouble, xsize*ysize);
    gdouble workspace[25];
    extend_rect(field->data, xres, extdata, xsize,
                col, row, width, height, xres, yres, 2, 2, 2, 2, fill_value);

    for (guint i = 0; i < height; i++) {
        gdouble *trow = target->data + (targetrow + i)*target->xres + targetcol;
        for (guint j = 0; j < width; j++, trow++) {
            for (guint ii = 0; ii < 5; ii++)
                gwy_assign(workspace + 5*ii, extdata + (i + ii)*xsize + j, 5);

            if (filter == GWY_STANDARD_FILTER_KUWAHARA)
                *trow = kuwahara_block(workspace);
            else if (filter == GWY_STANDARD_FILTER_STEP)
                *trow = step_block(workspace);
            else if (filter == GWY_STANDARD_FILTER_NONLINEARITY)
                *trow = nonlinearity_block(workspace, dx, dy);
            else {
                g_return_if_reached();
            }
        }
    }

    g_free(extdata);
    gwy_field_invalidate(target);
}

static GwyField*
make_kernel_from_data(const gdouble *data, guint xres, guint yres)
{
    GwyField *field = gwy_field_new_sized(xres, yres, FALSE);
    gwy_assign(field->data, data, xres*yres);
    return field;
}

static gdouble
combine_results_hypot(gdouble *results)
{
    return hypot(results[0], results[1]);
}

static void
combined_gradient_filter(const GwyField *field,
                         const GwyFieldPart *fpart,
                         GwyField *target,
                         const gdouble *kdata1, const gdouble *kdata2,
                         guint kxres, guint kyres,
                         GwyExterior exterior,
                         gdouble fill_value)
{
    guint col, row, width, height, targetcol, targetrow;
    if (!gwy_field_check_part(field, fpart, &col, &row, &width, &height)
        || !gwy_field_check_target(field, target,
                                   &(GwyFieldPart){ col, row, width, height },
                                   &targetcol, &targetrow))
        return;

    _gwy_ensure_defined_exterior(&exterior, &fill_value);
    RectExtendFunc extend_rect = _gwy_get_rect_extend_func(exterior);
    if (!extend_rect)
        return;

    GwyField *kernels[2] = {
        make_kernel_from_data(kdata1, kxres, kyres),
        make_kernel_from_data(kdata2, kxres, kyres),
    };
    multiconvolve_direct(field, col, row, width, height,
                         target, targetcol, targetrow,
                         (const GwyField**)kernels, 2, combine_results_hypot,
                         extend_rect, fill_value);

    g_object_unref(kernels[0]);
    g_object_unref(kernels[1]);
    gwy_field_invalidate(target);
}

/**
 * gwy_field_filter_standard:
 * @field: A two-dimensional data field.
 * @fpart: (allow-none):
 *         Area in @field to process.  Pass %NULL to process entire @field.
 * @target: A two-dimensional data field where the result will be placed.
 *          It may be @field for an in-place modification.  Its dimensions may
 *          match either @field or @fpart.  In the former case the
 *          placement of result is determined by @fpart; in the latter case
 *          the result fills the entire @target.
 * @filter: Filter to apply.
 * @exterior: Exterior pixels handling.
 * @fill_value: The value to use with %GWY_EXTERIOR_FIXED_VALUE exterior.
 *
 * Applies a predefined standard filter to a field.
 **/
void
gwy_field_filter_standard(const GwyField *field,
                          const GwyFieldPart* rectangle,
                          GwyField *target,
                          GwyStandardFilter filter,
                          GwyExterior exterior,
                          gdouble fill_value)
{
    static const gdouble laplace[] = {
        0.0,      1.0/4.0, 0.0,
        1.0/4.0, -1.0,     1.0/4.0,
        0.0,      1.0/4.0, 0.0,
    };
    // Derived by expressing the Scharr's optimised Laplacian kernel explicitly
    // and normalising the sum of values of one sign to unity.
    static const gdouble laplace_scharr[] = {
        0.05780595912336862,  0.19219404087663136, 0.05780595912336862,
        0.19219404087663136, -1.0,                 0.19219404087663136,
        0.05780595912336862,  0.19219404087663136, 0.05780595912336862,
    };
    /*
    static const gdouble laplace_scharr_5x5[] = {
         0.0012270305859819238, -0.012266564912910756, 0.0033743341114502922, -0.012266564912910756,  0.0012270305859819238,
        -0.012266564912910756,   0.07841024720177171,  0.16698838810079605,    0.07841024720177171,  -0.012266564912910756,
         0.0033743341114502922,  0.16698838810079605, -0.901867480696714,      0.16698838810079605,   0.0033743341114502922,
        -0.012266564912910756,   0.07841024720177171,  0.16698838810079605,    0.07841024720177171,  -0.012266564912910756,
         0.0012270305859819238, -0.012266564912910756, 0.0033743341114502922, -0.012266564912910756,  0.0012270305859819238,
    };
    */
    static const gdouble hsobel[] = {
        1.0/4.0, 0.0, -1.0/4.0,
        1.0/2.0, 0.0, -1.0/2.0,
        1.0/4.0, 0.0, -1.0/4.0,
    };
    static const gdouble vsobel[] = {
         1.0/4.0,  1.0/2.0,  1.0/4.0,
         0.0,      0.0,      0.0,
        -1.0/4.0, -1.0/2.0, -1.0/4.0,
    };
    static const gdouble hprewitt[] = {
        1.0/3.0, 0.0, -1.0/3.0,
        1.0/3.0, 0.0, -1.0/3.0,
        1.0/3.0, 0.0, -1.0/3.0,
    };
    static const gdouble vprewitt[] = {
         1.0/3.0,  1.0/3.0,  1.0/3.0,
         0.0,      0.0,      0.0,
        -1.0/3.0, -1.0/3.0, -1.0/3.0,
    };
    static const gdouble hscharr[] = {
         3.0/16.0, 0.0, -3.0/16.0,
         5.0/8.0,  0.0, -5.0/8.0,
         3.0/16.0, 0.0, -3.0/16.0,
    };
    static const gdouble vscharr[] = {
          3.0/16.0,  5.0/8.0,  3.0/16.0,
          0.0,       0.0,      0.0,
         -3.0/16.0, -5.0/8.0, -3.0/16.0,
    };
    static const gdouble dechecker[] = {
         0.0,        1.0/144.0, -1.0/72.0,  1.0/144.0,  0.0,
         1.0/144.0, -1.0/18.0,   1.0/9.0,  -1.0/18.0,   1.0/144.0,
        -1.0/72.0,   1.0/9.0,    7.0/9.0,   1.0/9.0,   -1.0/72.0,
         1.0/144.0, -1.0/18.0,   1.0/9.0,  -1.0/18.0,   1.0/144.0,
         0.0,        1.0/144.0, -1.0/72.0,  1.0/144.0,  0.0,
    };


    if (filter == GWY_STANDARD_FILTER_KUWAHARA
        || filter == GWY_STANDARD_FILTER_STEP
        || filter == GWY_STANDARD_FILTER_NONLINEARITY)
        filter_5x5(field, rectangle, target, filter, exterior, fill_value);
    else if (filter == GWY_STANDARD_FILTER_SOBEL)
        combined_gradient_filter(field, rectangle, target,
                                 hsobel, vsobel, 3, 3,
                                 exterior, fill_value);
    else if (filter == GWY_STANDARD_FILTER_PREWITT)
        combined_gradient_filter(field, rectangle, target,
                                 hprewitt, vprewitt, 3, 3,
                                 exterior, fill_value);
    else if (filter == GWY_STANDARD_FILTER_SCHARR)
        combined_gradient_filter(field, rectangle, target,
                                 hscharr, vscharr, 3, 3,
                                 exterior, fill_value);
    else {
        GwyField *kernel;
        if (filter == GWY_STANDARD_FILTER_LAPLACE)
            kernel = make_kernel_from_data(laplace, 3, 3);
        if (filter == GWY_STANDARD_FILTER_LAPLACE_SCHARR)
            kernel = make_kernel_from_data(laplace_scharr, 3, 3);
        else if (filter == GWY_STANDARD_FILTER_HSOBEL)
            kernel = make_kernel_from_data(hsobel, 3, 3);
        else if (filter == GWY_STANDARD_FILTER_VSOBEL)
            kernel = make_kernel_from_data(vsobel, 3, 3);
        else if (filter == GWY_STANDARD_FILTER_HPREWITT)
            kernel = make_kernel_from_data(hprewitt, 3, 3);
        else if (filter == GWY_STANDARD_FILTER_VPREWITT)
            kernel = make_kernel_from_data(vprewitt, 3, 3);
        else if (filter == GWY_STANDARD_FILTER_HSCHARR)
            kernel = make_kernel_from_data(hscharr, 3, 3);
        else if (filter == GWY_STANDARD_FILTER_VSCHARR)
            kernel = make_kernel_from_data(vscharr, 3, 3);
        else if (filter == GWY_STANDARD_FILTER_DECHECKER)
            kernel = make_kernel_from_data(dechecker, 5, 5);
        else {
            g_critical("Bad standard filter type %u.", filter);
            return;
        }
        gwy_field_convolve(field, rectangle, target, kernel,
                           exterior, fill_value);
        g_object_unref(kernel);
    }
}

/**
 * SECTION: field-filter
 * @section_id: GwyField-filter
 * @title: GwyField filtering
 * @short_description: Field filtering
 *
 * This section describes various filters operating on pixels and their
 * neigbourhood such as smoothing or gradients, and also general convolutions.
 *
 * Pixel-wise operations, also called arithmetic operations, are described in
 * section <link linkend='GwyField-arithmetic'>GwyField arithmetic</link>.
 **/

/**
 * GwyStandardFilter:
 * @GWY_STANDARD_FILTER_LAPLACE: Laplacian filter.
 *                               It represents the limit of Laplacian of
 *                               Gaussians for small Gaussian size.
 * @GWY_STANDARD_FILTER_LAPLACE_SCHARR: Scharr's Laplacian filter optimised for
 *                                      rotational symmetry.
 * @GWY_STANDARD_FILTER_HSOBEL: Horizontal Sobel filter.
 *                              It represents the horizontal derivative.
 * @GWY_STANDARD_FILTER_VSOBEL: Vertical Sobel filter.
 *                              It represents the vertical derivative.
 * @GWY_STANDARD_FILTER_SOBEL: Sobel filter.
 *                             It represents the absolute value of the
 *                             derivative.
 * @GWY_STANDARD_FILTER_HPREWITT: Horizontal Prewitt filter.
 *                                It represents the horizontal derivative.
 * @GWY_STANDARD_FILTER_VPREWITT: Vertical Prewitt filter.
 *                                It represents the vertical derivative.
 * @GWY_STANDARD_FILTER_PREWITT: Prewitt filter.
 *                               It represents the absolute value of the
 *                               derivative.
 * @GWY_STANDARD_FILTER_HSCHARR: Horizontal Scharr filter.
 *                               It represents the horizontal derivative.
 * @GWY_STANDARD_FILTER_VSCHARR: Vertical Scharr filter.
 *                               It represents the vertical derivative.
 * @GWY_STANDARD_FILTER_SCHARR: Scharr filter.
 *                              It represents the absolute value of the
 *                              derivative.
 * @GWY_STANDARD_FILTER_DECHECKER: Checker-pattern removal filter.
 *                                 It behaves like a slightly smoothing filter
 *                                 except for single-pixel checker pattern
 *                                 that, if present, is almost completely
 *                                 removed.
 * @GWY_STANDARD_FILTER_KUWAHARA: Kuwahara edge-preserving smoothing filter.
 *                                This is a non-linear (non-convolution)
 *                                filter.
 * @GWY_STANDARD_FILTER_STEP: Non-linear rank-based step detection filter.
 *                            It is somewhat more smoothing than Sobel, Prewitt
 *                            and Scharr but considerably more noise-resistant.
 * @GWY_STANDARD_FILTER_NONLINEARITY: Local non-linearity edge detection
 *                                    filter.  It really detects edges, not
 *                                    steps as most other filters do.
 *
 * Predefined standard filter types.
 *
 * The Laplacian filters have the following 3×3 kernels:
 * |[
 * Standard Laplace          Laplace optimised by Scharr
 * 0     1/4   0             0.0578    0.1922    0.0578
 * 1/4  -1     1/4           0.1922   -1         0.1922
 * 0     1/4   0             0.0578    0.1922    0.0578
 * ]|
 *
 * The Sobel derivative filters have the following 3×3 kernels:
 * |[
 * Horizontal                Vertical
 * 1/4   0   -1/4            1/4    1/2    1/4
 * 1/2   0   -1/2            0      0      0
 * 1/4   0   -1/4           -1/4   -1/2   -1/2
 * ]|
 *
 * The Prewitt derivative filters have the following 3×3 kernels:
 * |[
 * Horizontal                Vertical
 * 1/3   0   -1/3            1/3    1/3    1/3
 * 1/3   0   -1/3            0      0      0
 * 1/3   0   -1/3           -1/3   -1/3   -1/3
 * ]|
 *
 * The Scharr derivative filters have the following 3×3 kernels:
 * |[
 * Horizontal                Vertical
 * 3/16   0   -3/16          3/16   5/8    3/16
 * 5/8    0   -5/8           0      0      0
 * 3/16   0   -3/16         -3/16   5/8    3/16
 * ]|
 *
 * The absolute value of the derivative is calculated as hypotenuse of the
 * derivatives calculated by the corresponding directional filters.
 *
 * The dechecker filter is a convolution filter with the following 5×5 kernel:
 * |[
 *  0        1/144   -1/72    1/144     0
 *  1/144   -1/18     1/9    -1/18      1/144
 * -1/72     1/9      7/9     1/9      -1/72
 *  1/144   -1/18     1/9    -1/18      1/144
 *  0        1/144   -1/72    1/144     0
 * ]|
 *
 * The Kuwahara filter works on blocks 5×5.  In each such block the corner 3×3
 * sub-block with the lowest variance is chosen and its mean value is the
 * filter result.
 *
 * The step filter also works on blocks 5×5, excluding their corners so that
 * an approximately circular area is processed.  The 21 remaining values are
 * then sorted and the difference between the 16th and 6th value forms the
 * filter result.
 *
 * The local non-linearity filter works on the same neighbourhoods as the step
 * filter.  It calculates the residuum after fitting a plane through the
 * neighbourhood, but normalised by the area of this plane.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
