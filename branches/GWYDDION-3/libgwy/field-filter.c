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

#include <string.h>
#include <fftw3.h>
#include "libgwy/macros.h"
#include "libgwy/strfuncs.h"
#include "libgwy/math.h"
#include "libgwy/fft.h"
#include "libgwy/field-filter.h"
#include "libgwy/line-arithmetic.h"
#include "libgwy/line-statistics.h"
#include "libgwy/math-internal.h"
#include "libgwy/field-arithmetic.h"
#include "libgwy/field-internal.h"

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

// Generally, there are three reasonable strategies:
// (a) small kernels (kres comparable to log(res)) → direct calculation
// (b) medium-sized kernels (kres ≤ res) → use explicit extension/mirroring to
//     a nice-for-FFT size ≥ 2res and cyclic convolution
// (c) large kernels (kres > res) → fold kernel to 2res size and use implicit
//     mirroring and DCT-II for data
//
// Strategy (c) is not implemented as the conditions are quite exotic,
// especially if res should be large.

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
    // Nice size is always even
    *reout *= *rein/size;
}

static void
row_convolve_direct(GwyField *field,
                    guint col, guint row,
                    guint width, guint height,
                    const GwyLine *kernel,
                    RowExtendFunc extend_row,
                    gdouble fill_value)
{
    guint xres = field->xres;
    guint kres = kernel->res;
    const gdouble *kdata = kernel->data;
    guint size = width + kres - 1;
    gdouble *data_in = g_new(gdouble, size);
    guint extend_left, extend_right;
    make_symmetrical_extension(width, size, &extend_left, &extend_right);

    // The direct method is used only if kres ≪ xres.  Don't bother optimising
    // the boundaries, just make the inner loop tight.
    for (guint i = 0; i < height; i++) {
        gdouble *drow = field->data + (row + i)*xres + col;
        extend_row(field->data + (row + i)*xres, data_in,
                   col, width, xres, extend_left, extend_right, fill_value);
        for (guint j = 0; j < width; j++) {
            const gdouble *d = data_in + extend_left + kres/2 + j;
            gdouble v = 0.0;
            for (guint k = 0; k < kres; k++, d--)
                v += kdata[k] * *d;
            drow[j] = v;
        }
    }

    g_free(data_in);
}

static void
row_convolve_fft(GwyField *field,
                 guint col, guint row,
                 guint width, guint height,
                 const GwyLine *kernel,
                 RowExtendFunc extend_row,
                 gdouble fill_value)
{
    guint xres = field->xres, kres = kernel->res;
    guint size = gwy_fft_nice_transform_size(width + kres - 1);
    gdouble *data_in = fftw_malloc(3*size*sizeof(gdouble));
    gdouble *data_hc = data_in + size;
    gdouble *kernelhc = data_in + 2*size;
    // The R2HC plan for the initial transform of the kernel.  We put the
    // 0-extended real-space kernel into data_hc that is unused at that time.
    fftw_plan kplan = fftw_plan_r2r_1d(size, data_hc, kernelhc,
                                       FFTW_R2HC,
                                       FFTW_DESTROY_INPUT | _gwy_fft_rigour());
    // The R2HC plan for transforming the extended data.
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
    guint extend_left, extend_right;
    make_symmetrical_extension(width, size, &extend_left, &extend_right);
    for (guint i = 0; i < height; i++) {
        extend_row(field->data + (row + i)*xres, data_in,
                   col, width, xres, extend_left, extend_right, fill_value);
        fftw_execute(dplan);
        multiply_hc_with_hc(kernelhc, data_hc, size);
        fftw_execute(cplan);
        gwy_assign(field->data + (row + i)*xres + col, data_in + extend_left,
                   width);
    }

    fftw_destroy_plan(kplan);
    fftw_destroy_plan(cplan);
    fftw_destroy_plan(dplan);
    fftw_free(data_in);
}

/**
 * gwy_field_row_convolve:
 * @field: A two-dimensional data field.
 * @rectangle: Area in @field to process.  Pass %NULL to process entire @field.
 * @kernel: Kenrel to convolve @field with.
 * @exterior: Exterior pixels handling.
 * @fill_value: The value to use with %GWY_EXTERIOR_FIXED_VALUE.
 *
 * Convolve a field with a horizontal kernel.
 *
 * The convolution is performed with the kernel centred on the respective field
 * pixels.  For an odd-sized kernel this holds precisely.  For an even-sized
 * kernel this means the kernel centre is placed 0.5 pixel to the right
 * (towards higher column indices) from the respective field pixel.
 *
 * See gwy_field_extend() for what constitutes the exterior and how it is
 * handled.
 **/
void
gwy_field_row_convolve(GwyField *field,
                       const GwyRectangle *rectangle,
                       const GwyLine *kernel,
                       GwyExteriorType exterior,
                       gdouble fill_value)
{
    guint col, row, width, height;
    if (!_gwy_field_check_rectangle(field, rectangle,
                                    &col, &row, &width, &height))
        return;

    g_return_if_fail(GWY_IS_LINE(kernel));
    RowExtendFunc extend_row = get_row_extend_func(exterior);
    if (!extend_row)
        return;

    // The threshold was estimated empirically.  See benchmarks/convolve-row.c
    if (convolution_method == CONVOLUTION_DIRECT
        || (convolution_method == CONVOLUTION_AUTO
            && (width <= 20 || kernel->res <= 5.0*(log(width) - 1.0))))
        row_convolve_direct(field, col, row, width, height, kernel,
                            extend_row, fill_value);
    else
        row_convolve_fft(field, col, row, width, height, kernel,
                         extend_row, fill_value);

    gwy_field_invalidate(field);
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

static void
col_convolve_direct(GwyField *field,
                    guint col, guint row,
                    guint width, guint height,
                    const GwyLine *kernel,
                    RectExtendFunc extend_rect,
                    gdouble fill_value)
{
    guint xres = field->xres, yres = field->yres, kres = kernel->res;
    const gdouble *kdata = kernel->data;
    guint size = height + kres - 1;
    gdouble *data_in = g_new(gdouble, size*width);
    guint extend_up, extend_down;
    make_symmetrical_extension(height, size, &extend_up, &extend_down);

    extend_rect(field->data, xres, data_in, width,
                col, row, width, height, xres, yres,
                0, 0, extend_up, extend_down, fill_value);
    GwyRectangle rectangle = { col, row, width, height };
    gwy_field_clear(field, &rectangle, NULL, GWY_MASK_IGNORE);

    // The direct method is used only if kres ≪ yres.  Don't bother optimising
    // the boundaries, just make the inner loop tight.
    for (guint i = 0; i < height; i++) {
        gdouble *drow = field->data + (row + i)*xres + col;
        const gdouble *d = data_in + (extend_up + kres/2 + i)*width;
        for (guint k = 0; k < kres; k++, d -= width) {
            gdouble v = kdata[k];
            for (guint j = 0; j < width; j++)
                drow[j] += v * d[j];
        }
    }

    g_free(data_in);
}

static void
col_convolve_fft(GwyField *field,
                 guint col, guint row,
                 guint width, guint height,
                 const GwyLine *kernel,
                 RectExtendFunc extend_rect,
                 gdouble fill_value)
{
    guint xres = field->xres, yres = field->yres, kres = kernel->res;
    guint size = gwy_fft_nice_transform_size(height + kres - 1);
    guint extend_up, extend_down;
    make_symmetrical_extension(height, size, &extend_up, &extend_down);
    // Use in-place transforms.  Let FFTW figure out whether allocating
    // temporary buffers worths it or not.
    gdouble *extdata = fftw_malloc(size*width*sizeof(gdouble));
    gdouble *kernelhc = fftw_malloc(size);
    // The R2HC plan for the initial transform of the kernel.  We put the
    // 0-extended real-space kernel into data_hc that is unused at that time.
    fftw_plan kplan = fftw_plan_r2r_1d(size, extdata, kernelhc,
                                       FFTW_R2HC,
                                       FFTW_DESTROY_INPUT | _gwy_fft_rigour());
    // The R2HC plan for transforming the extended data.
    int n[1] = { size };
    fftw_r2r_kind fkind[1] = { FFTW_R2HC }, bkind[1] = { FFTW_HC2R };
    fftw_plan dplan = fftw_plan_many_r2r(1, n, width,
                                         extdata, NULL, width, 1,
                                         extdata, NULL, width, 1,
                                         fkind, _gwy_fft_rigour());
    // The HC2R plan the backward transform of the convolution.
    fftw_plan cplan = fftw_plan_many_r2r(1, n, width,
                                         extdata, NULL, width, 1,
                                         extdata, NULL, width, 1,
                                         bkind, _gwy_fft_rigour());

    // Transform the kernel.
    mirror_kernel(kernel->data, kres, extdata, size);
    fftw_execute(kplan);

    // Convolve rows
    extend_rect(field->data, xres, extdata, width,
                col, row, width, height, xres, yres,
                0, 0, extend_up, extend_down, fill_value);
    fftw_execute(dplan);
    // TODO: actually multiply HC data with HC kernel
    fftw_execute(cplan);

    for (guint i = 0; i < height; i++)
        gwy_assign(field->data + (row + i)*xres + col,
                   extdata + (extend_up + i)*width,
                   width);

    fftw_destroy_plan(kplan);
    fftw_destroy_plan(cplan);
    fftw_destroy_plan(dplan);
    fftw_free(kernelhc);
    fftw_free(extdata);
}

/**
 * gwy_field_col_convolve:
 * @field: A two-dimensional data field.
 * @rectangle: Area in @field to process.  Pass %NULL to process entire @field.
 * @kernel: Kenrel to convolve @field with.
 * @exterior: Exterior pixels handling.
 * @fill_value: The value to use with %GWY_EXTERIOR_FIXED_VALUE.
 *
 * Convolve a field with a vertical kernel.
 *
 * The convolution is performed with the kernel centred on the respective field
 * pixels.  For an odd-sized kernel this holds precisely.  For an even-sized
 * kernel this means the kernel centre is placed 0.5 pixel down (towards higher
 * row indices) from the respective field pixel.
 *
 * See gwy_field_extend() for what constitutes the exterior and how it is
 * handled.
 **/
void
gwy_field_col_convolve(GwyField *field,
                       const GwyRectangle* rectangle,
                       const GwyLine *kernel,
                       GwyExteriorType exterior,
                       gdouble fill_value)
{
    guint col, row, width, height;
    if (!_gwy_field_check_rectangle(field, rectangle,
                                    &col, &row, &width, &height))
        return;

    g_return_if_fail(GWY_IS_LINE(kernel));
    RectExtendFunc extend_rect = get_rect_extend_func(exterior);
    if (!extend_rect)
        return;

    // The threshold was estimated empirically.  See benchmarks/convolve-col.c
    if (convolution_method == CONVOLUTION_DIRECT
        || (convolution_method == CONVOLUTION_AUTO
            && (width <= 20 || kernel->res <= 5.0*(log(width) - 1.0))))
        col_convolve_direct(field, col, row, width, height, kernel,
                            extend_rect, fill_value);
    else
        col_convolve_fft(field, col, row, width, height, kernel,
                         extend_rect, fill_value);

    gwy_field_invalidate(field);
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

    GwyLine *kernel = gwy_line_new();
    gboolean done = FALSE;

    if (make_gaussian_kernel(kernel, hsigma)) {
        gwy_field_row_convolve(field, rectangle, kernel,
                               GWY_EXTERIOR_MIRROR_EXTEND, 0.0);
        if (vsigma == hsigma) {
            gwy_field_col_convolve(field, rectangle, kernel,
                                   GWY_EXTERIOR_MIRROR_EXTEND, 0.0);
            done = TRUE;
        }
    }
    if (!done && make_gaussian_kernel(kernel, vsigma))
        gwy_field_col_convolve(field, rectangle, kernel,
                               GWY_EXTERIOR_MIRROR_EXTEND, 0.0);
    g_object_unref(kernel);
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

    return result;
}

/**
 * SECTION: field-filter
 * @section_id: GwyField-filter
 * @title: GwyField filtering
 * @short_description: Field filtering
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
