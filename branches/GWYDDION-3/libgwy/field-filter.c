/*
 *  $Id$
 *  Copyright (C) 2010-2011 David Necas (Yeti).
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
#include "libgwy/field-filter.h"
#include "libgwy/line-arithmetic.h"
#include "libgwy/line-statistics.h"
#include "libgwy/field-arithmetic.h"
#include "libgwy/object-internal.h"
#include "libgwy/math-internal.h"
#include "libgwy/field-internal.h"

enum { CORRELATION_ALL = 0x07 };

typedef void (*RowExtendFunc)(const gdouble *in,
                              gdouble *out,
                              guint pos,
                              guint width,
                              guint res,
                              guint extend_left,
                              guint extend_right,
                              gdouble value);
typedef void (*RectExtendFunc)(const gdouble *in,
                               guint inrowstride,
                               gdouble *out,
                               guint outrowstride,
                               guint xpos,
                               guint ypos,
                               guint width,
                               guint height,
                               guint xres,
                               guint yres,
                               guint extend_left,
                               guint extend_right,
                               guint extend_up,
                               guint extend_down,
                               gdouble value);
typedef gdouble (*CombineResultsFunc)(const gdouble *results);

// Permit to choose the algorithm explicitly in testing and benchmarking.
enum {
    CONVOLUTION_AUTO,
    CONVOLUTION_DIRECT,
    CONVOLUTION_FFT
};
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
static inline void
make_symmetrical_extension(guint size, guint extsize,
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
 * @out: Output row of lenght @width+@extend_left+@extend_right.
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
get_row_extend_func(GwyExteriorType exterior)
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
    make_symmetrical_extension(width, size, &extend_left, &extend_right);

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
    gdouble *extdata = fftw_malloc(size*sizeof(gdouble));
    gwycomplex *datac = fftw_malloc(2*cstride*sizeof(gwycomplex));
    gwycomplex *kernelc = datac + cstride;
    // The R2C plan for transforming the extended data row (or kernel).
    fftw_plan dplan = fftw_plan_dft_r2c_1d(size, extdata, datac,
                                           FFTW_DESTROY_INPUT
                                            | _gwy_fft_rigour());
    // The C2R plan the backward transform of the convolution of each row.
    fftw_plan cplan = fftw_plan_dft_c2r_1d(size, datac, extdata,
                                           _gwy_fft_rigour());

    // Transform the kernel.
    extend_kernel_row(kernel->data, kres, extdata, size);
    fftw_execute(dplan);
    gwy_assign(kernelc, datac, cstride);

    // Convolve rows
    guint extend_left, extend_right;
    make_symmetrical_extension(width, size, &extend_left, &extend_right);
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
 * @rectangle: Area in @field to process.  Pass %NULL to process entire @field.
 * @target: A two-dimensional data field where the result will be placed.
 *          It may be @field for an in-place modification.  Its dimensions may
 *          match either @field or @rectangle.  In the former case the
 *          placement of result is determined by @rectangle; in the latter case
 *          the result fills the entire @target.
 * @kernel: Kenrel to convolve @field with.
 * @exterior: Exterior pixels handling.
 * @fill_value: The value to use with %GWY_EXTERIOR_FIXED_VALUE.
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
                       const GwyRectangle *rectangle,
                       GwyField *target,
                       const GwyLine *kernel,
                       GwyExteriorType exterior,
                       gdouble fill_value)
{
    guint col, row, width, height, targetcol, targetrow;
    if (!_gwy_field_check_rectangle(field, rectangle,
                                    &col, &row, &width, &height)
        || !_gwy_field_check_target(field, target, col, row, width, height,
                                    &targetcol, &targetrow))
        return;

    g_return_if_fail(GWY_IS_LINE(kernel));

    if (exterior == GWY_EXTERIOR_UNDEFINED) {
        g_warning("Do not use GWY_EXTERIOR_UNDEFINED for convolutions and "
                  "correlations.  Fixing to zero-filled exterior.");
        exterior = GWY_EXTERIOR_FIXED_VALUE;
        fill_value = 0.0;
    }
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

static inline void
extend_kernel_rect(const gdouble *kernel, guint kxlen, guint kylen,
                   gdouble *extended, guint xsize, guint ysize, guint rowstride)
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

static RectExtendFunc
get_rect_extend_func(GwyExteriorType exterior)
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
                     CombineResultsFunc combine_results,
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
    make_symmetrical_extension(width, xsize, &extend_left, &extend_right);
    make_symmetrical_extension(height, ysize, &extend_up, &extend_down);

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
    make_symmetrical_extension(width, xsize, &extend_left, &extend_right);
    make_symmetrical_extension(height, ysize, &extend_up, &extend_down);
    // Use in-place transforms.  Let FFTW figure out whether allocating
    // temporary buffers worths it or not.
    gwycomplex *datac = fftw_malloc(cstride*ysize*sizeof(gwycomplex));
    gdouble *extdata = (gdouble*)datac;
    gwycomplex *kernelc = fftw_malloc(cstride*ysize*sizeof(gwycomplex));
    // The R2C plan for transforming the extended kernel.  The input is in
    // extdata to make it an out-of-place transform (this also means the input
    // data row stride is just xsize, not 2*cstride).
    fftw_plan kplan = fftw_plan_dft_r2c_2d(ysize, xsize, extdata, kernelc,
                                           FFTW_DESTROY_INPUT
                                           | _gwy_fft_rigour());
    // The R2C plan for transforming the extended data.  This one is in-place.
    fftw_plan dplan = fftw_plan_dft_r2c_2d(ysize, xsize, extdata, datac,
                                           FFTW_DESTROY_INPUT
                                           | _gwy_fft_rigour());
    // The C2R plan the backward transform of the convolution.  The input
    // is in fact in kernelc to make it an out-of-place transform.  So, again,
    // the output has cstride of only xsize.
    fftw_plan cplan = fftw_plan_dft_c2r_2d(ysize, xsize, kernelc, extdata,
                                           _gwy_fft_rigour());

    // Transform the kernel.
    extend_kernel_rect(kernel->data, kxres, kyres,
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
 * @rectangle: Area in @field to process.  Pass %NULL to process entire @field.
 * @target: A two-dimensional data field where the result will be placed.
 *          It may be @field for an in-place modification.  Its dimensions may
 *          match either @field or @rectangle.  In the former case the
 *          placement of result is determined by @rectangle; in the latter case
 *          the result fills the entire @target.
 * @kernel: Kenrel to convolve @field with.
 * @exterior: Exterior pixels handling.
 * @fill_value: The value to use with %GWY_EXTERIOR_FIXED_VALUE.
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
                   const GwyRectangle* rectangle,
                   GwyField *target,
                   const GwyField *kernel,
                   GwyExteriorType exterior,
                   gdouble fill_value)
{
    guint col, row, width, height, targetcol, targetrow;
    if (!_gwy_field_check_rectangle(field, rectangle,
                                    &col, &row, &width, &height)
        || !_gwy_field_check_target(field, target, col, row, width, height,
                                    &targetcol, &targetrow))
        return;

    g_return_if_fail(GWY_IS_FIELD(kernel));

    if (exterior == GWY_EXTERIOR_UNDEFINED) {
        g_warning("Do not use GWY_EXTERIOR_UNDEFINED for convolutions and "
                  "correlations.  Fixing to zero-filled exterior.");
        exterior = GWY_EXTERIOR_FIXED_VALUE;
        fill_value = 0.0;
    }
    RectExtendFunc extend_rect = get_rect_extend_func(exterior);
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

    gwy_field_invalidate(target);
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
    gwy_line_multiply(kernel, 1.0/s);

    return TRUE;
}

/**
 * gwy_field_extend:
 * @field: A two-dimensional data field.
 * @rectangle: Area in @field to extend.  Pass %NULL to extend entire @field.
 * @left: Number of pixels to extend to the left (towards lower column indices).
 * @right: Number of pixels to extend to the right (towards higher column
 *         indices).
 * @up: Number of pixels to extend up (towards lower row indices).
 * @down: Number of pixels to extend down (towards higher row indices).
 * @exterior: Exterior pixels handling.
 * @fill_value: The value to use with %GWY_EXTERIOR_FIXED_VALUE.
 *
 * Extends a field using the specified method of exterior handling.
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
 * Returns: A newly created field.
 **/
GwyField*
gwy_field_extend(const GwyField *field,
                 const GwyRectangle *rectangle,
                 guint left, guint right,
                 guint up, guint down,
                 GwyExteriorType exterior,
                 gdouble fill_value)
{
    guint col, row, width, height;
    if (!_gwy_field_check_rectangle(field, rectangle,
                                    &col, &row, &width, &height))
        return NULL;

    RectExtendFunc extend_rect = get_rect_extend_func(exterior);
    if (!extend_rect)
        return NULL;

    GwyField *result = gwy_field_new_sized(width + left + right,
                                           height + up + down,
                                           FALSE);
    extend_rect(field->data, field->xres, result->data, result->xres,
                col, row, width, height, field->xres, field->yres,
                left, right, up, down, fill_value);

    result->xreal = (width + left + right)*gwy_field_dx(field);
    result->yreal = (height + up + down)/gwy_field_dy(field);
    result->xoff = field->xoff + left*gwy_field_dx(field);
    result->yoff = field->yoff + up*gwy_field_dy(field);
    ASSIGN_UNITS(result->priv->unit_xy, field->priv->unit_xy);
    ASSIGN_UNITS(result->priv->unit_z, field->priv->unit_z);

    return result;
}

/**
 * gwy_field_filter_gaussian:
 * @field: A two-dimensional data field.
 * @rectangle: Area in @field to process.  Pass %NULL to process entire @field.
 * @target: A two-dimensional data field where the result will be placed.
 *          It may be @field for an in-place modification.  Its dimensions may
 *          match either @field or @rectangle.  In the former case the
 *          placement of result is determined by @rectangle; in the latter case
 *          the result fills the entire @target.
 * @hsigma: Horizontal sigma parameter (square root of dispersion) of the
 *          Gaussian, in pixels.
 * @vsigma: Vertical sigma parameter (square root of dispersion) of the
 *          Gaussian, in pixels.
 * @exterior: Exterior pixels handling.
 * @fill_value: The value to use with %GWY_EXTERIOR_FIXED_VALUE.
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
gwy_field_filter_gaussian(const GwyField *field,
                          const GwyRectangle* rectangle,
                          GwyField *target,
                          gdouble hsigma, gdouble vsigma,
                          GwyExteriorType exterior,
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
    if (make_gaussian_kernel(colkernel, vsigma)) {
        make_gaussian_kernel(rowkernel, hsigma);
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
kuwahara_block(const gdouble *a)
{
   static const guint r1[] = { 0, 1, 2, 5, 6, 7, 10, 11, 12 };
   static const guint r2[] = { 2, 3, 4, 7, 8, 9, 12, 13, 14 };
   static const guint r3[] = { 12, 13, 14, 17, 18, 19, 22, 23, 24 };
   static const guint r4[] = { 10, 11, 12, 15, 16, 17, 20, 21, 22 };
   gdouble mean1 = 0.0, mean2 = 0.0, mean3 = 0.0, mean4 = 0.0;
   gdouble var1 = 0.0, var2 = 0.0, var3 = 0.0, var4 = 0.0;

   for (guint i = 0; i < 9; i++) {
       gdouble v1 = a[r1[i]], v2 = a[r2[i]], v3 = a[r3[i]], v4 = a[r4[i]];
       mean1 += v1;
       mean2 += v2;
       mean3 += v3;
       mean4 += v4;
       var1 += v1*v1;
       var2 += v2*v2;
       var3 += v3*v3;
       var4 += v4*v4;
   }
   mean1 /= 9.0;
   mean2 /= 9.0;
   mean3 /= 9.0;
   mean4 /= 9.0;
   var1 /= 9.0;
   var2 /= 9.0;
   var3 /= 9.0;
   var4 /= 9.0;

   var1 -= mean1 * mean1;
   var2 -= mean2 * mean2;
   var3 -= mean3 * mean3;
   var4 -= mean4 * mean4;

   if (var1 <= var2 && var1 <= var3 && var1 <= var4)
       return mean1;
   if (var2 <= var3 && var2 <= var4 && var2 <= var1)
       return mean2;
   if (var3 <= var4 && var3 <= var1 && var3 <= var2)
       return mean3;
   if (var4 <= var1 && var4 <= var2 && var4 <= var3)
       return mean4;

   // XXX: IMO this can happen only with NaNs.  Maybe we should return NaN too.
   return a[12];
}

static void
filter_kuwahara(const GwyField *field,
                const GwyRectangle* rectangle,
                GwyField *target,
                GwyExteriorType exterior,
                gdouble fill_value)
{
    guint col, row, width, height, targetcol, targetrow;
    if (!_gwy_field_check_rectangle(field, rectangle,
                                    &col, &row, &width, &height)
        || !_gwy_field_check_target(field, target, col, row, width, height,
                                    &targetcol, &targetrow))
        return;

    RectExtendFunc extend_rect = get_rect_extend_func(exterior);
    if (!extend_rect)
        return;

    guint xres = field->xres, yres = field->yres;
    guint xsize = width + 4;
    guint ysize = height + 4;
    gdouble *extdata = g_new(gdouble, xsize*ysize);
    gdouble workspace[25];
    extend_rect(field->data, xres, extdata, xsize,
                col, row, width, height, xres, yres, 2, 2, 2, 2, fill_value);

    for (guint i = 0; i < height; i++) {
        gdouble *trow = target->data + (targetrow + i)*target->xres + targetcol;
        for (guint j = 0; j < width; j++, trow++) {
            for (guint ii = 0; ii < 5; ii++)
                gwy_assign(workspace + 5*ii, extdata + (i + ii)*xsize + j, 5);
            *trow = kuwahara_block(workspace);
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

/**
 * gwy_field_filter_standard:
 * @field: A two-dimensional data field.
 * @rectangle: Area in @field to process.  Pass %NULL to process entire @field.
 * @target: A two-dimensional data field where the result will be placed.
 *          It may be @field for an in-place modification.  Its dimensions may
 *          match either @field or @rectangle.  In the former case the
 *          placement of result is determined by @rectangle; in the latter case
 *          the result fills the entire @target.
 * @filter: Filter to apply.
 * @exterior: Exterior pixels handling.
 * @fill_value: The value to use with %GWY_EXTERIOR_FIXED_VALUE.
 *
 * Applies a predefined standard filter to a field.
 **/
void
gwy_field_filter_standard(const GwyField *field,
                          const GwyRectangle* rectangle,
                          GwyField *target,
                          GwyFilterType filter,
                          GwyExteriorType exterior,
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

    // Special-case the non-convolution filters.
    if (filter == GWY_FILTER_KUWAHARA) {
        filter_kuwahara(field, rectangle, target, exterior, fill_value);
        return;
    }

    GwyField *kernel;
    if (filter == GWY_FILTER_LAPLACE)
        kernel = make_kernel_from_data(laplace, 3, 3);
    if (filter == GWY_FILTER_LAPLACE_SCHARR)
        kernel = make_kernel_from_data(laplace_scharr, 3, 3);
    else if (filter == GWY_FILTER_HSOBEL)
        kernel = make_kernel_from_data(hsobel, 3, 3);
    else if (filter == GWY_FILTER_VSOBEL)
        kernel = make_kernel_from_data(vsobel, 3, 3);
    else if (filter == GWY_FILTER_HPREWITT)
        kernel = make_kernel_from_data(hprewitt, 3, 3);
    else if (filter == GWY_FILTER_VPREWITT)
        kernel = make_kernel_from_data(vprewitt, 3, 3);
    else if (filter == GWY_FILTER_HSCHARR)
        kernel = make_kernel_from_data(hscharr, 3, 3);
    else if (filter == GWY_FILTER_VSCHARR)
        kernel = make_kernel_from_data(vscharr, 3, 3);
    else if (filter == GWY_FILTER_DECHECKER)
        kernel = make_kernel_from_data(dechecker, 5, 5);
    else {
        // TODO: Total derivative filters: Sobel, Prewitt, Scharr
        g_critical("Bad standard filter type %u.", filter);
        return;
    }

    gwy_field_convolve(field, rectangle, target, kernel,
                       exterior, fill_value);
    g_object_unref(kernel);
}

void
gwy_field_correlate(const GwyField *field,
                    const GwyRectangle* rectangle,
                    GwyField *target,
                    const GwyField *kernel,
                    const GwyMaskField *kmask,
                    GwyCorrelationFlags flags,
                    GwyExteriorType exterior,
                    gdouble fill_value)
{
    guint col, row, width, height, targetcol, targetrow;
    if (!_gwy_field_check_rectangle(field, rectangle,
                                    &col, &row, &width, &height)
        || !_gwy_field_check_target(field, target, col, row, width, height,
                                    &targetcol, &targetrow))
        return;

    if (kmask) {
        g_return_if_fail(GWY_IS_MASK_FIELD(kmask));
        g_return_if_fail(kmask->xres == kernel->xres
                         && kmask->yres == kernel->yres);
    }

    if (exterior == GWY_EXTERIOR_UNDEFINED) {
        g_warning("Do not use GWY_EXTERIOR_UNDEFINED for convolutions and "
                  "correlations.  Fixing to zero-filled exterior.");
        exterior = GWY_EXTERIOR_FIXED_VALUE;
        fill_value = 0.0;
    }

    RectExtendFunc extend_rect = get_rect_extend_func(exterior);
    if (!extend_rect)
        return;

    if (flags & ~CORRELATION_ALL)
        g_warning("Unknown correlation flags 0x%x.", flags & ~CORRELATION_ALL);

    gboolean level = flags & GWY_CORRELATION_LEVEL;
    gboolean normalise = flags & GWY_CORRELATION_NORMALIZE;
    gboolean poc = flags & GWY_CORRELATION_PHASE_ONLY;
    if (poc && (level || normalise)) {
        g_warning("Levelling and normalisation is ignored for "
                  "phase-only correlation.");
        level = normalise = FALSE;
    }

    // Ensure the kernel has zeroes outside the mask.
    GwyField *maskedkernel = gwy_field_duplicate(kernel);
    if (kmask)
        gwy_field_clear(maskedkernel, NULL, kmask, GWY_MASK_EXCLUDE);

    guint xres = field->xres, yres = field->yres,
          kxres = maskedkernel->xres, kyres = maskedkernel->yres;
    gdouble kavg = gwy_field_mean(maskedkernel, NULL, kmask, GWY_MASK_INCLUDE);
    guint kcount = kmask ? gwy_mask_field_count(kmask, NULL, TRUE) : kxres*kyres;
    if (!kcount) {
        g_object_unref(maskedkernel);
        GwyRectangle rect = { targetcol, targetrow, width, height };
        gwy_field_fill(target, &rect, NULL, GWY_MASK_IGNORE, 0.0);
        return;
    }

    guint xsize = gwy_fft_nice_transform_size(width + kxres - 1);
    guint ysize = gwy_fft_nice_transform_size(height + kyres - 1);
    // The innermost (contiguous) dimension of R2C the complex output is
    // slightly larger than the real input.  If the transform is in-place the
    // input array needs to be padded.  Note @cstride is measured in
    // gwycomplex, multiply it by 2 for doubles.
    guint cstride = xsize/2 + 1;
    guint extend_left, extend_right, extend_up, extend_down;
    make_symmetrical_extension(width, xsize, &extend_left, &extend_right);
    make_symmetrical_extension(height, ysize, &extend_up, &extend_down);
    gwycomplex *datac = fftw_malloc(cstride*ysize*sizeof(gwycomplex));
    gwycomplex *kernelc = fftw_malloc(cstride*ysize*sizeof(gwycomplex));
    gdouble *extdata = fftw_malloc(xsize*ysize*sizeof(gdouble));
    gdouble *extkernel = fftw_malloc(xsize*ysize*sizeof(gdouble));
    gdouble *targetbase = target->data + targetrow*target->xres + targetcol;
    gdouble *extkernelbase = extkernel + extend_up*xsize + extend_left;
    gdouble *extdatabase = extdata + extend_up*xsize + extend_left;
    // The out-of-place R2C plan.  We use it with the new-array excution
    // functions also for extkernel → kernelc.
    fftw_plan dplan = fftw_plan_dft_r2c_2d(ysize, xsize, extdata, datac,
                                           FFTW_DESTROY_INPUT
                                           | _gwy_fft_rigour());
    // The C2R plan the backward transform of the correlation.  Again, it's
    // used also for kernels.
    fftw_plan cplan = fftw_plan_dft_c2r_2d(ysize, xsize, datac, extdata,
                                           _gwy_fft_rigour());

    // Transform the kernel.
    extend_kernel_rect(maskedkernel->data, kxres, kyres,
                       extkernel, xsize, ysize, xsize);
    fftw_execute_dft_r2c(dplan, extkernel, kernelc);

    // Transform the data.
    // TODO: if levelling is requested, perform overall levelling first to
    // reduce the numerical errors.
    extend_rect(field->data, xres, extdata, xsize,
                col, row, width, height, xres, yres,
                extend_left, extend_right, extend_up, extend_down, fill_value);
    fftw_execute(dplan);

    // Convolve, putting the result into kernelc as we have no other use for it.
    for (guint k = 0; k < cstride*ysize; k++)
        kernelc[k] = conj(kernelc[k])*datac[k]/(xsize*ysize);
    if (poc) {
        for (guint k = 0; k < cstride*ysize; k++) {
            gdouble c = cabs(kernelc[k]);
            if (G_LIKELY(c))
                kernelc[k] /= c;
        }
    }
    fftw_execute_dft_c2r(cplan, kernelc, extkernel);

    // Store the result of the plain correlation to the target.
    for (guint i = 0; i < height; i++)
        gwy_assign(targetbase + i*target->xres, extkernelbase + i*xsize, width);

    // Levelling is done by calculating the local mean values by convolution
    // with 1-filled kernel.  Similarly, the local mean square values are
    // calculated using convolution of squared data with 1-filled kernel.
    // So for either we the Fourier image of such kernel.
    if (level || normalise) {
        gwy_field_fill(maskedkernel, NULL, kmask, GWY_MASK_INCLUDE, 1.0/kcount);
        extend_kernel_rect(maskedkernel->data, kxres, kyres,
                           extkernel, xsize, ysize, xsize);
        fftw_execute_dft_r2c(dplan, extkernel, kernelc);
    }

    // Level.  Keep the local means in extkernel we may need it below also for
    // normalisation.
    if (level) {
        for (guint k = 0; k < cstride*ysize; k++)
            datac[k] = conj(kernelc[k])*datac[k]/(xsize*ysize);
        fftw_execute_dft_c2r(cplan, datac, extkernel);
        for (guint i = 0; i < height; i++) {
            const gdouble *srow = extkernelbase + i*xsize;
            gdouble *trow = targetbase + i*target->xres;
            for (guint j = 0; j < width; j++)
                trow[i] -= srow[i]*kavg;
        }
    }

    // Normalise.
    if (normalise) {
        for (guint k = 0; k < xsize*ysize; k++)
            extdata[k] *= extdata[k];
        fftw_execute(dplan);

        for (guint k = 0; k < cstride*ysize; k++)
            datac[k] = conj(kernelc[k])*datac[k]/(xsize*ysize);
        fftw_execute(cplan);

        for (guint i = 0; i < height; i++) {
            const gdouble *srow = extkernelbase + i*xsize;
            const gdouble *qrow = extdatabase + i*xsize;
            gdouble *trow = targetbase + i*target->xres;
            if (level) {
                for (guint j = 0; j < width; j++) {
                    gdouble q = qrow[i] - srow[i]*srow[i];
                    trow[i] = (q > 0.0) ? trow[i]/sqrt(q) : 0.0;
                }
            }
            else {
                for (guint j = 0; j < width; j++) {
                    gdouble q = qrow[i];
                    trow[i] = G_LIKELY(q) ? trow[i]/sqrt(q) : 0.0;
                }
            }
        }
    }

    fftw_destroy_plan(cplan);
    fftw_destroy_plan(dplan);
    fftw_free(kernelc);
    fftw_free(datac);
    fftw_free(extkernel);
    fftw_free(extdata);
    g_object_unref(maskedkernel);

    gwy_field_invalidate(target);
}

/**
 * SECTION: field-filter
 * @section_id: GwyField-filter
 * @title: GwyField filtering
 * @short_description: Field filtering
 **/

/**
 * GwyFilterType:
 * @GWY_FILTER_LAPLACE: Laplacian filter.
 *                      It represents the limit of Laplacian of Gaussians for
 *                      small Gaussian size.
 * @GWY_FILTER_LAPLACE_SCHARR: Scharr's Laplacian filter optimised for
 *                             rotational symmetry.
 * @GWY_FILTER_HSOBEL: Horizontal Sobel filter.
 *                     It represents the horizontal derivative.
 * @GWY_FILTER_VSOBEL: Vertical Sobel filter.
 *                     It represents the vertical derivative.
 * @GWY_FILTER_SOBEL: Sobel filter.
 *                    It represents the absolute value of the derivative.
 * @GWY_FILTER_HPREWITT: Horizontal Prewitt filter.
 *                       It represents the horizontal derivative.
 * @GWY_FILTER_VPREWITT: Vertical Prewitt filter.
 *                       It represents the vertical derivative.
 * @GWY_FILTER_PREWITT: Prewitt filter.
 *                      It represents the absolute value of the derivative.
 * @GWY_FILTER_HSCHARR: Horizontal Scharr filter.
 *                      It represents the horizontal derivative.
 * @GWY_FILTER_VSCHARR: Vertical Scharr filter.
 *                      It represents the vertical derivative.
 * @GWY_FILTER_SCHARR: Scharr filter.
 *                     It represents the absolute value of the derivative.
 * @GWY_FILTER_DECHECKER: Checker-pattern removal filter.
 *                        It behaves like a slightly smoothing filter except
 *                        for single-pixel checker pattern that, if present, is
 *                        almost completely removed.
 * @GWY_FILTER_KUWAHARA: Kuwahara edge-preserving smoothing filter.  This is
 *                       a non-linear (non-convolution) filter.
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
 **/

/**
 * GwyCorrelationFlags:
 * @GWY_CORRELATION_LEVEL: Subtract the mean value from each kernel-sized
 *                         rectangle before multiplying it with the kernel to
 *                         obtain the correlation score.
 * @GWY_CORRELATION_NORMALIZE: Normalise the rms of each kernel-sized rectangle
 *                             before multiplying it with the kernel to obtain
 *                             the correlation score.
 * @GWY_CORRELATION_PHASE_ONLY: Normalise all Fourier coefficients of both
 *                              kernel and data to unity before calculating
 *                              the correlation.
 *
 * Flags controlling behaviour of correlation functions.
 *
 * Flag %GWY_CORRELATION_PHASE_ONLY is mutually exclusive with the other flag.
 *
 * Flag %GWY_CORRELATION_LEVEL should be usually used for data that do not have
 * well-defined zero level, such as AFM data.
 *
 * If kernel masking is in use the levelling and normalisation is applied only
 * to the corresponding masked area in data.  If both flags are specified local
 * levelling is performed first, then normalisation.  Note even if levelling
 * and normalisation are used correlation scores are still calculated using
 * FFT, just in a slightly more involved way.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
