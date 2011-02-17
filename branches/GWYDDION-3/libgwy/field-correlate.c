/*
 *  $Id$
 *  Copyright (C) 2010-2011 David Nečas (Yeti).
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
// Including before fftw3.h ensures fftw_complex is C99 ‘double complex’.
#include <complex.h>
#include <fftw3.h>
#include "libgwy/macros.h"
#include "libgwy/math.h"
#include "libgwy/fft.h"
#include "libgwy/field-transform.h"
#include "libgwy/field-arithmetic.h"
#include "libgwy/field-statistics.h"
#include "libgwy/field-correlate.h"
#include "libgwy/object-internal.h"
#include "libgwy/math-internal.h"
#include "libgwy/field-internal.h"

enum {
    CORRELATION_ALL = 0x07,
    CROSSCORRELATION_ALL = 0x0b,
};

/**
 * gwy_field_correlate:
 * @field: A two-dimensional data field.
 * @fpart: (allow-none):
 *             Area in @field to process.  Pass %NULL to process entire @field.
 * @score: A two-dimensional data field where the result will be placed.
 *         It may be @field for an in-place modification.  Its dimensions may
 *         match either @field or @fpart.  In the former case the
 *         placement of result is determined by @fpart; in the latter case
 *         the result fills the entire @score.
 * @kernel: Kernel, i.e. the detail for which the correlation score is
 *          calculated.  It is always used as-is.  If you want it to have zero
 *          mean and rms of unity (which you often want) it is easy to ensure
 *          it beforehand using gwy_field_normalize().
 * @kmask: (allow-none):
 *         Kernel mask.  If non-%NULL it must have the same dimensions as
 *         @kernel and only kernel pixels covered by the mask then contribute
 *         to the correlation score.  This makes possible to search for a
 *         non-rectangular detail.
 * @flags: Flags controlling correlation score calculation.
 * @exterior: Exterior pixels handling.
 * @fill_value: The value to use with %GWY_EXTERIOR_FIXED_VALUE exterior.
 *
 * Calculates correlation scores for a detail searched in a field.
 *
 * Similarly to correlations, the scores in @scores correspond to kernel placed
 * centered on the respective pixels.  This has the advantage that the detail
 * can be found even if it stick out of the image a bit.  However, it also
 * means that the top-left corner of the detail is shifted by
 * (@kernel->yres-1)/2 upward and (@kernel->xres-1)/2 to the left with respect
 * to the correspond score pixel.
 **/
void
gwy_field_correlate(const GwyField *field,
                    const GwyFieldPart *fpart,
                    GwyField *score,
                    const GwyField *kernel,
                    const GwyMaskField *kmask,
                    GwyCorrelationFlags flags,
                    GwyExteriorType exterior,
                    gdouble fill_value)
{
    guint col, row, width, height, scorecol, scorerow;
    if (!_gwy_field_check_part(field, fpart, &col, &row, &width, &height)
        || !_gwy_field_check_target(field, score, col, row, width, height,
                                    &scorecol, &scorerow))
        return;

    if (kmask) {
        g_return_if_fail(GWY_IS_MASK_FIELD(kmask));
        g_return_if_fail(kmask->xres == kernel->xres
                         && kmask->yres == kernel->yres);
    }

    _gwy_ensure_defined_exterior(&exterior, &fill_value);
    RectExtendFunc extend_rect = _gwy_get_rect_extend_func(exterior);
    if (!extend_rect)
        return;

    if (flags & ~CORRELATION_ALL)
        g_warning("Unknown correlation flags 0x%x.",
                  flags & ~CORRELATION_ALL);

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
        GwyFieldPart rect = { scorecol, scorerow, width, height };
        gwy_field_clear(score, &rect, NULL, GWY_MASK_IGNORE);
        return;
    }
    // Turn convolution into correlation.
    gwy_field_flip(maskedkernel, TRUE, TRUE, FALSE);

    guint xsize = gwy_fft_nice_transform_size(width + kxres - 1);
    guint ysize = gwy_fft_nice_transform_size(height + kyres - 1);
    // The innermost (contiguous) dimension of R2C the complex output is
    // slightly larger than the real input.  If the transform is in-place the
    // input array needs to be padded.  Note @cstride is measured in
    // gwycomplex, multiply it by 2 for doubles.
    guint cstride = xsize/2 + 1;
    guint extend_left, extend_right, extend_up, extend_down;
    // Can overflow guint.
    gdouble qnorm = 1.0/xsize/ysize/kcount;
    _gwy_make_symmetrical_extension(width, xsize, &extend_left, &extend_right);
    _gwy_make_symmetrical_extension(height, ysize, &extend_up, &extend_down);
    gwycomplex *datac = fftw_malloc(cstride*ysize*sizeof(gwycomplex));
    gwycomplex *kernelc = fftw_malloc(cstride*ysize*sizeof(gwycomplex));
    gdouble *extdata = fftw_malloc(xsize*ysize*sizeof(gdouble));
    gdouble *extkernel = fftw_malloc(xsize*ysize*sizeof(gdouble));
    gdouble *scorebase = score->data + scorerow*score->xres + scorecol;
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
    _gwy_extend_kernel_rect(maskedkernel->data, kxres, kyres,
                            extkernel, xsize, ysize, xsize);
    fftw_execute_dft_r2c(dplan, extkernel, kernelc);

    // Transform the data.
    extend_rect(field->data, xres, extdata, xsize,
                col, row, width, height, xres, yres,
                extend_left, extend_right, extend_up, extend_down, fill_value);
    // If levelling is requested, shift the global mean value to zero first to
    // reduce numerical errors.
    if (level) {
        gdouble mean = gwy_field_mean(field, fpart, NULL, GWY_MASK_IGNORE);
        for (guint k = 0; k < xsize*ysize; k++)
            extdata[k] -= mean;
    }
    fftw_execute(dplan);

    // Convolve, putting the result into kernelc as we have no other use for it.
    for (guint k = 0; k < cstride*ysize; k++)
        kernelc[k] = qnorm*kernelc[k]*datac[k];
    if (poc) {
        for (guint k = 0; k < cstride*ysize; k++) {
            gdouble c = cabs(kernelc[k]);
            if (G_LIKELY(c))
                kernelc[k] /= c;
        }
    }
    fftw_execute_dft_c2r(cplan, kernelc, extkernel);

    // Store the result of the plain correlation to the score.
    for (guint i = 0; i < height; i++)
        gwy_assign(scorebase + i*score->xres, extkernelbase + i*xsize, width);

    // Levelling is done by calculating the local mean values by convolution
    // with 1-filled kernel.  Similarly, the local mean square values are
    // calculated using convolution of squared data with 1-filled kernel.
    // So for either we the Fourier image of such kernel.
    if (level || normalise) {
        gwy_field_fill(maskedkernel, NULL, kmask, GWY_MASK_INCLUDE, 1.0);
        _gwy_extend_kernel_rect(maskedkernel->data, kxres, kyres,
                                extkernel, xsize, ysize, xsize);
        fftw_execute_dft_r2c(dplan, extkernel, kernelc);
    }

    // Level.  Keep the local means in extkernel we may need it below also for
    // normalisation.
    if (level) {
        for (guint k = 0; k < cstride*ysize; k++)
            datac[k] = qnorm*kernelc[k]*datac[k];
        fftw_execute_dft_c2r(cplan, datac, extkernel);
        for (guint i = 0; i < height; i++) {
            const gdouble *srow = extkernelbase + i*xsize;
            gdouble *trow = scorebase + i*score->xres;
            for (guint j = 0; j < width; j++)
                trow[j] -= srow[j]*kavg;
        }
    }

    // Normalise.
    if (normalise) {
        for (guint k = 0; k < xsize*ysize; k++)
            extdata[k] *= extdata[k];
        fftw_execute(dplan);

        for (guint k = 0; k < cstride*ysize; k++)
            datac[k] = qnorm*kernelc[k]*datac[k];
        fftw_execute(cplan);

        for (guint i = 0; i < height; i++) {
            const gdouble *srow = extkernelbase + i*xsize;
            const gdouble *qrow = extdatabase + i*xsize;
            gdouble *trow = scorebase + i*score->xres;
            if (level) {
                for (guint j = 0; j < width; j++) {
                    gdouble q = qrow[j] - srow[j]*srow[j];
                    trow[j] = (q > 0.0) ? trow[j]/sqrt(q) : 0.0;
                }
            }
            else {
                for (guint j = 0; j < width; j++) {
                    gdouble q = qrow[j];
                    trow[j] = G_LIKELY(q) ? trow[j]/sqrt(q) : 0.0;
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

    if (score != field)
        ASSIGN_UNITS(score->priv->unit_xy, field->priv->unit_xy);

    if (normalise) {
        ASSIGN_UNITS(score->priv->unit_z, kernel->priv->unit_z);
    }
    else if (score->priv->unit_z || field->priv->unit_z) {
        // Force instantiation of units
        gwy_unit_multiply(gwy_field_get_unit_z(score),
                          gwy_field_get_unit_z(kernel),
                          gwy_field_get_unit_z(field));
    }

    gwy_field_invalidate(score);
}

static void
calculate_local_mean_and_rms(fftw_plan dplan, fftw_plan cplan,
                             gdouble *extdata, gdouble *extdatabase,
                             const gdouble *extfield,
                             const gwycomplex *kernelc,
                             gwycomplex *datac,
                             GwyField **fieldmean, GwyField **fieldrms,
                             guint width, guint height,
                             guint xsize, guint ysize, guint cstride,
                             gdouble qnorm,
                             gboolean level, gboolean normalise)
{
    if (!level && !normalise)
        return;

    *fieldmean = gwy_field_new_sized(width, height, FALSE);
    gwy_assign(extdata, extfield, xsize*ysize);
    fftw_execute(dplan);
    for (guint k = 0; k < cstride*ysize; k++)
        datac[k] *= qnorm*kernelc[k];
    fftw_execute(cplan);
    for (guint i = 0; i < height; i++) {
        const gdouble *srow = extdatabase + i*xsize;
        gdouble *trow = (*fieldmean)->data + i*width;
        gwy_assign(trow, srow, width);
    }

    if (!normalise)
        return;

    *fieldrms = gwy_field_new_sized(width, height, FALSE);
    for (guint k = 0; k < xsize*ysize; k++)
        extdata[k] = extfield[k]*extfield[k];
    fftw_execute(dplan);
    for (guint k = 0; k < cstride*ysize; k++)
        datac[k] *= qnorm*kernelc[k];
    fftw_execute(cplan);
    for (guint i = 0; i < height; i++) {
        const gdouble *srow = extdatabase + i*xsize;
        const gdouble *mrow = (*fieldmean)->data + i*width;
        gdouble *trow = (*fieldrms)->data + i*width;
        for (guint j = 0; j < width; j++) {
            gdouble q = srow[j] - mrow[j]*mrow[j];
            trow[j] = (q > 0.0) ? sqrt(q) : 0.0;
        }
    }
}

// Shift field by (xshift, yshift) and point-wise multiply with reference,
// storing the result into data.  Irrelevant border values are cleared to make
// the behaviour well-defined.
static void
multiply_shifted_rects(const gdouble *extfield,
                       const gdouble *extreference,
                       gdouble *extdata,
                       guint xsize, guint ysize,
                       gint xshift, gint yshift)
{
    guint absxs = ABS(xshift), absys = ABS(yshift);

    for (guint i = 0; i < ysize; i++) {
        if ((yshift > 0 && i < absys) || (yshift < 0 && i >= ysize - absys)) {
            gwy_clear(extdata + i*xsize, xsize);
            continue;
        }

        const gdouble *rrow = extreference + i*xsize;
        const gdouble *frow = extfield + (i - yshift)*xsize;
        gdouble *drow = extdata + i*xsize;

        if (xshift > 0) {
            gwy_clear(drow, absxs);
            drow += absxs;
            rrow += absxs;
        }
        else
            frow += absxs;

        for (guint j = xsize - absxs; j; j--, drow++, frow++, rrow++)
            *drow = (*frow)*(*rrow);

        if (xshift < 0)
            gwy_clear(drow, absxs);
    }
}

/**
 * gwy_field_crosscorrelate:
 * @field: A two-dimensional data field.
 * @reference: A field to cross-corelate with @field.  It must have the same
 *             dimensions as @field.
 * @fpart: (allow-none):
 *         Area in @field and @reference to process.  Pass %NULL to process
 *         entire fields.
 * @score: (out caller-allocates) (allow-none):
 *         A two-dimensional data field where the scores will be placed,
 *         or %NULL.
 * @xoff: (out caller-allocates) (allow-none):
 *        A two-dimensional data field where the horizontal shifts
 *        corresponding to the maximum scores will be placed, or %NULL.
 * @yoff: (out caller-allocates) (allow-none):
 *        A two-dimensional data field where the horizontal shifts
 *        corresponding to the maximum scores will be placed, or %NULL.
 * @kernel: Mask defining the shape of the detail correlated, with ones in
 *          pixels to consider and zeroes in pixels to ignore.  A suitable
 *          kernel can often be created with gwy_mask_field_fill_ellipse().
 * @colsearch: Horizontal search range; the detail is searched no farther than
 *             @colsearch left or right from the position in @field.
 * @rowsearch: Vertical search range; the detail is searched no farther than
 *             @rowsearch upward or downward from the position in @field.
 * @flags: Flags controlling cross-correlation score calculation.
 * @exterior: Exterior pixels handling.
 * @fill_value: The value to use with %GWY_EXTERIOR_FIXED_VALUE exterior.
 *
 * Cross-correlates two fields, calculating scores and/or offsets.
 *
 * Cross-correlation is an algorithm for matching two different images of the
 * same object under changes.  Conceptually, it searches @kernel-sized details
 * of @field in near positions in @reference and finds the best matches and
 * their shifts.  This search is performed for details at all possible
 * positions in @field.  In practice, this all is of course done using FFT.
 *
 * An offset is considered positive if the detail is placed down or to the
 * right in @reference with respect to @field; it is positive if the detail is
 * placed up or to the left in @reference with respect to @field.
 *
 * The dimensions of of @score, @xoff and @yoff fields may match either @field
 * or @fpart (all three must have the same dimensions though).  In the
 * former case the placement of result is determined by @fpart; in the
 * latter case the result fills the entire target field.
 **/
// FIXME: Instead of colsearch and rowsearch we could also accept an arbitrary
// mask.  It would be actually easier...
// XXX: We cannot do subpixel precision this way as it would require insane
// storage.  We could perform all the DFTs second time after finding the
// maxima but that's perhaps too much.  What about passing the offsets as
// integers then?
void
gwy_field_crosscorrelate(const GwyField *field,
                         const GwyField *reference,
                         const GwyFieldPart *fpart,
                         GwyField *score,
                         GwyField *xoff,
                         GwyField *yoff,
                         const GwyMaskField *kernel,
                         guint colsearch,
                         guint rowsearch,
                         GwyCrosscorrelationFlags flags,
                         GwyExteriorType exterior,
                         gdouble fill_value)
{
    // Check if all target fields are compatible; @target is set to whichever
    // of them is non-NULL.
    GwyField *target = NULL;
    if (score) {
        g_return_if_fail(GWY_IS_FIELD(score));
        target = score;
    }
    if (xoff) {
        g_return_if_fail(GWY_IS_FIELD(xoff));
        if (target)
            g_return_if_fail(target->xres == xoff->xres
                             && target->yres == xoff->yres);
        else
            target = xoff;
    }
    if (yoff) {
        g_return_if_fail(GWY_IS_FIELD(yoff));
        if (target)
            g_return_if_fail(target->xres == yoff->xres
                             && target->yres == yoff->yres);
        else
            target = yoff;
    }
    if (!target)
        return;

    guint col, row, width, height, targetcol, targetrow;
    if (!_gwy_field_check_part(field, fpart, &col, &row, &width, &height)
        || !_gwy_field_check_target(field, target, col, row, width, height,
                                    &targetcol, &targetrow))
        return;

    g_return_if_fail(GWY_IS_MASK_FIELD(kernel));
    g_return_if_fail(GWY_IS_FIELD(reference));
    g_return_if_fail(reference->xres == field->xres
                     && reference->yres == field->yres);

    _gwy_ensure_defined_exterior(&exterior, &fill_value);
    RectExtendFunc extend_rect = _gwy_get_rect_extend_func(exterior);
    if (!extend_rect)
        return;

    if (flags & ~CROSSCORRELATION_ALL)
        g_warning("Unknown correlation flags 0x%x.",
                  flags & ~CROSSCORRELATION_ALL);

    gboolean level = flags & GWY_CROSSCORRELATION_LEVEL;
    gboolean normalise = flags & GWY_CROSSCORRELATION_NORMALIZE;
    gboolean elliptical = flags & GWY_CROSSCORRELATION_ELLIPTICAL;

    guint kcount = gwy_mask_field_count(kernel, NULL, TRUE);

    if (!kcount) {
        GwyFieldPart rect = { targetcol, targetrow, width, height };
        if (score)
            gwy_field_clear(score, &rect, NULL, GWY_MASK_IGNORE);
        if (xoff)
            gwy_field_clear(xoff, &rect, NULL, GWY_MASK_IGNORE);
        if (yoff)
            gwy_field_clear(yoff, &rect, NULL, GWY_MASK_IGNORE);
        return;
    }

    // Turn convolution into correlation.
    GwyField *unitkernel = gwy_field_new_from_mask(kernel, 0.0, 1.0);
    gwy_field_flip(unitkernel, TRUE, TRUE, FALSE);

    guint xres = field->xres, yres = field->yres,
          kxres = unitkernel->xres, kyres = unitkernel->yres;

    GwyField *fieldmean = NULL, *referencemean = NULL,
             *fieldrms = NULL, *referencerms = NULL;

    guint xsize = gwy_fft_nice_transform_size(width + 2*colsearch + kxres - 1);
    guint ysize = gwy_fft_nice_transform_size(height + 2*rowsearch + kyres - 1);
    // The innermost (contiguous) dimension of R2C the complex output is
    // slightly larger than the real input.  If the transform is in-place the
    // input array needs to be padded.  Note @cstride is measured in
    // gwycomplex, multiply it by 2 for doubles.
    guint cstride = xsize/2 + 1;
    guint extend_left, extend_right, extend_up, extend_down;
    // Can overflow guint.
    gdouble qnorm = 1.0/xsize/ysize/kcount;
    _gwy_make_symmetrical_extension(width, xsize, &extend_left, &extend_right);
    _gwy_make_symmetrical_extension(height, ysize, &extend_up, &extend_down);
    gwycomplex *datac = fftw_malloc(cstride*ysize*sizeof(gwycomplex));
    gwycomplex *kernelc = fftw_malloc(cstride*ysize*sizeof(gwycomplex));
    gdouble *extdata = fftw_malloc(xsize*ysize*sizeof(gdouble));
    gdouble *extkernel = fftw_malloc(xsize*ysize*sizeof(gdouble));
    gdouble *extfield = g_new(gdouble, xsize*ysize);
    gdouble *extreference = g_new(gdouble, xsize*ysize);
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
    _gwy_extend_kernel_rect(unitkernel->data, kxres, kyres,
                            extkernel, xsize, ysize, xsize);
    fftw_execute_dft_r2c(dplan, extkernel, kernelc);

    // Calculate local means/mean squares of field.  We need to have them for
    // larger area than just width×height as field is shifted within the
    // search range.
    guint fxres = width + 2*colsearch, fyres = height + 2*rowsearch;
    extend_rect(field->data, xres, extfield, xsize,
                col, row, width, height, xres, yres,
                extend_left, extend_right, extend_up, extend_down, fill_value);
    if (level) {
        gdouble mean = gwy_field_mean(field, fpart, NULL, GWY_MASK_IGNORE);
        for (guint k = 0; k < xsize*ysize; k++)
            extfield[k] -= mean;
    }
    calculate_local_mean_and_rms(dplan, cplan,
                                 extdata,
                                 extdata + (extend_up - rowsearch)*xsize
                                 + (extend_left - colsearch),
                                 extfield,
                                 kernelc, datac,
                                 &fieldmean, &fieldrms,
                                 fxres, fyres, xsize, ysize, cstride,
                                 qnorm, level, normalise);

    // Calculate local means/mean squares of reference.
    extend_rect(reference->data, xres, extreference, xsize,
                col, row, width, height, xres, yres,
                extend_left, extend_right, extend_up, extend_down, fill_value);
    if (level) {
        gdouble mean = gwy_field_mean(reference, fpart, NULL, GWY_MASK_IGNORE);
        for (guint k = 0; k < xsize*ysize; k++)
            extreference[k] -= mean;
    }
    calculate_local_mean_and_rms(dplan, cplan,
                                 extdata, extdatabase, extreference,
                                 kernelc, datac,
                                 &referencemean, &referencerms,
                                 width, height, xsize, ysize, cstride,
                                 qnorm, level, normalise);

    // We no longer need these.
    g_object_unref(unitkernel);
    fftw_free(extkernel);

    GwyFieldPart targetrect = { targetcol, targetrow, width, height };
    if (xoff)
        gwy_field_clear(xoff, &targetrect, NULL, GWY_MASK_IGNORE);
    if (yoff)
        gwy_field_clear(yoff, &targetrect, NULL, GWY_MASK_IGNORE);
    // We need to keep the scores somewhere to find the highest score.
    if (score)
        g_object_ref(score);
    else
        score = gwy_field_new_alike(target, FALSE);
    gwy_field_fill(score, &targetrect, NULL, GWY_MASK_IGNORE, -G_MAXDOUBLE);

    // Scan the neighbourhood.  Vars ii and jj represent the shifts of the
    // field with respect to reference.  Consequently, if jj is positive the
    // field is shifted to the right and values are taken from smaller indices
    // than in reference.  But to detect it, we have to move it in the other
    // direction.  Yeah, it's a bit confusing.
    for (gint ii = -(gint)rowsearch; ii <= (gint)rowsearch; ii++) {
        for (gint jj = -(gint)colsearch; jj <= (gint)colsearch; jj++) {
            if (elliptical) {
                gdouble ay = ii/(rowsearch + 0.5);
                gdouble ax = jj/(colsearch + 0.5);
                if (ax*ax + ay*ay > 1.0)
                    continue;
            }

            multiply_shifted_rects(extfield, extreference, extdata,
                                   xsize, ysize, -jj, -ii);
            fftw_execute(dplan);
            for (guint k = 0; k < cstride*ysize; k++)
                datac[k] *= qnorm*kernelc[k];
            fftw_execute(cplan);

            for (guint i = 0; i < height; i++) {
                for (guint j = 0; j < width; j++) {
                    guint ifa = i + rowsearch + ii, jfa = j + colsearch + jj;
                    gdouble s = extdatabase[i*xsize + j];

                    if (level)
                        s -= (fieldmean->data[ifa*fxres + jfa]
                              * referencemean->data[i*width + j]);

                    if (normalise) {
                        gdouble q = (fieldrms->data[ifa*fxres + jfa]
                                     * referencerms->data[i*width + j]);
                        s = q ? s/q : 0.0;
                    }

                    guint k = (i + targetrow)*score->xres + (targetcol + j);
                    if (s > score->data[k]) {
                        score->data[k] = s;
                        if (xoff)
                            xoff->data[k] = jj;
                        if (yoff)
                            yoff->data[k] = ii;
                    }
                }
            }
        }
    }

    fftw_destroy_plan(cplan);
    fftw_destroy_plan(dplan);
    g_free(extreference);
    g_free(extfield);
    fftw_free(extdata);
    fftw_free(kernelc);
    fftw_free(datac);
    GWY_OBJECT_UNREF(referencerms);
    GWY_OBJECT_UNREF(fieldrms);
    GWY_OBJECT_UNREF(referencemean);
    GWY_OBJECT_UNREF(fieldmean);
    gwy_field_invalidate(score);
    g_object_unref(score);
    if (xoff)
        gwy_field_invalidate(xoff);
    if (yoff)
        gwy_field_invalidate(yoff);
}

/**
 * SECTION: field-correlate
 * @section_id: GwyField-correlate
 * @title: GwyField correlation and cross-correlation
 * @short_description: Field correlation and cross-correlation
 **/

/**
 * GwyCorrelationFlags:
 * @GWY_CORRELATION_LEVEL: Subtract the local mean value from each kernel-sized
 *                         area before multiplying it with the kernel to
 *                         obtain the correlation score.
 * @GWY_CORRELATION_NORMALIZE: Normalise the rms of each kernel-sized area
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

/**
 * GwyCrosscorrelationFlags:
 * @GWY_CROSSCORRELATION_LEVEL: Subtract the local mean values from the
 *                              kernel-sized areas before multiplying them to
 *                              obtain the correlation score.
 * @GWY_CROSSCORRELATION_NORMALIZE: Normalise the rms of the kernel-sized areas
 *                                  before multiplying them to obtain
 *                                  the correlation score.
 * @GWY_CROSSCORRELATION_ELLIPTICAL: Search the elliptical neighbourhood
 *                                   inscribed to the search range rectangle
 *                                   instead of the full rectangle.
 *
 * Flags controlling behaviour of cross-correlation functions.
 *
 * Flag %GWY_CROSSCORRELATION_LEVEL should be usually used for data that do not
 * have well-defined zero level, such as AFM data.
 *
 * The levelling and normalisation is applied only to the corresponding masked
 * area in data.  If both flags are specified local levelling is performed
 * first, then normalisation.  Note even if levelling and normalisation are
 * used correlation scores are still calculated using FFT, just in a slightly
 * more involved way.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
