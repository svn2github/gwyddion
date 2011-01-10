/*
 *  $Id$
 *  Copyright (C) 2009-2010 David Necas (Yeti).
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

#include <fftw3.h>
#include <string.h>
#include "libgwy/macros.h"
#include "libgwy/math.h"
#include "libgwy/fft.h"
#include "libgwy/line-arithmetic.h"
#include "libgwy/field-statistics.h"
#include "libgwy/field-distributions.h"
#include "libgwy/math-internal.h"
#include "libgwy/field-internal.h"

static void
sanitize_range(gdouble *min,
               gdouble *max)
{
    if (*max > *min)
        return;
    if (*max) {
        gdouble d = fabs(*max);
        *max += 0.1*d;
        *min -= 0.1*d;
        return;
    }
    *min = -1.0;
    *max = 1.0;
}

/**
 * gwy_field_value_dist:
 * @field: A two-dimensional data field.
 * @rectangle: Area in @field to process.  Pass %NULL to process entire @field.
 * @mask: Mask specifying which values to take into account/exclude, or %NULL.
 * @masking: Masking mode to use (has any effect only with non-%NULL @mask).
 * @cumulative: %TRUE to calculate cumulative distribution, %FALSE to calculate
 *              density.
 * @npoints: Distribution resolution, i.e. the number of histogram bins.
 *           Pass zero to choose a suitable resolution automatically.
 * @min: Minimum value of the range to calculate the distribution in.
 * @max: Maximum value of the range to calculate the distribution in.
 *
 * Calculates the distribution of values in a field.
 *
 * Pass @max <= @min to calculate the distribution in the full data range
 * (with masking possibly considered).
 *
 * Returns: A new one-dimensional data line with the value distribution.
 **/
GwyLine*
gwy_field_value_dist(const GwyField *field,
                     const GwyRectangle *rectangle,
                     const GwyMaskField *mask,
                     GwyMaskingType masking,
                     gboolean cumulative,
                     guint npoints,
                     gdouble min, gdouble max)
{
    guint col, row, width, height, maskcol, maskrow;
    GwyLine *line = NULL;
    if (!_gwy_field_check_mask(field, rectangle, mask, &masking,
                               &col, &row, &width, &height, &maskcol, &maskrow))
        goto fail;

    guint n;
    if (min < max) {
        // We know the range but have to figure out how many pixels we have.
        guint nabove, nbelow;
        n = gwy_field_count_above_below(field, rectangle, mask, masking,
                                        max, min, TRUE, &nabove, &nbelow);
        n -= nabove + nbelow;
    }
    else {
        // We know the number of pixels but have to figure out the range.
        GwyRectangle rect = { maskcol, maskrow, width, height };
        if (masking == GWY_MASK_INCLUDE)
            n = gwy_mask_field_part_count(mask, &rect, TRUE);
        else if (masking == GWY_MASK_EXCLUDE)
            n = gwy_mask_field_part_count(mask, &rect, FALSE);
        else
            n = width*height;

        gwy_field_min_max(field, rectangle, mask, masking, &min, &max);
        sanitize_range(&min, &max);
    }
    if (!npoints) {
        npoints = gwy_round(3.49*cbrt(n));
        npoints = MAX(npoints, 1);
    }
    line = gwy_line_new_sized(npoints, TRUE);
    if (!n)
        goto fail;

    const gdouble *base = field->data + row*field->xres + col;
    gdouble q = (max - min)*npoints;
    guint ndata = 0;

    if (masking == GWY_MASK_IGNORE) {
        for (guint i = 0; i < height; i++) {
            const gdouble *d = base + i*field->xres;
            for (guint j = width; j; j--, d++) {
                guint k = (guint)((*d - min)/q);
                // Fix rounding errors.
                if (G_UNLIKELY(k >= npoints))
                    line->data[npoints-1] += 1;
                else
                    line->data[k] += 1;
            }
        }
        ndata = width*height;
    }
    else {
        const gboolean invert = (masking == GWY_MASK_EXCLUDE);
        for (guint i = 0; i < height; i++) {
            const gdouble *d = base + i*field->xres;
            GwyMaskIter iter;
            gwy_mask_field_iter_init(mask, iter, maskcol, maskrow + i);
            for (guint j = width; j; j--, d++) {
                if (!gwy_mask_iter_get(iter) == invert) {
                    guint k = (guint)((*d - min)/q);
                    // Fix rounding errors.
                    if (G_UNLIKELY(k >= npoints))
                        line->data[npoints-1] += 1;
                    else
                        line->data[k] += 1;
                    ndata++;
                }
                gwy_mask_iter_next(iter);
            }
        }
    }

    line->off = min;
    line->real = max - min;
    if (cumulative) {
        gwy_line_accumulate(line);
        gwy_line_multiply(line, 1.0/line->data[npoints-1]);
    }
    else
        gwy_line_multiply(line, npoints/(max - min)/ndata);

fail:
    if (!line)
        line = gwy_line_new();

    gwy_unit_assign(gwy_line_get_unit_x(line), gwy_field_get_unit_z(field));
    if (!cumulative)
        gwy_unit_power(gwy_line_get_unit_y(line),
                       gwy_field_get_unit_z(field), -1);

    return line;
}

static guint
slope_dist_horiz_analyse(const GwyField *field,
                         guint col, guint row,
                         guint width, guint height,
                         gdouble *min, gdouble *max)
{
    const gdouble *base = field->data + row*field->xres + col;
    gdouble min1 = HUGE_VAL, max1 = -HUGE_VAL;

    for (guint i = 0; i < height; i++) {
        const gdouble *d = base + i*field->xres;
        for (guint j = width-1; j; j--, d++) {
            gdouble v = d[1] - d[0];
            if (v < min1)
                min1 = v;
            if (v > max1)
                max1 = v;
        }
    }
    *min = min1;
    *max = max1;
    return width*(height - 1);
}

static guint
slope_dist_horiz_analyse_mask(const GwyField *field,
                              const GwyMaskField *mask,
                              GwyMaskingType masking,
                              guint col, guint row,
                              guint width, guint height,
                              guint maskcol, guint maskrow,
                              gdouble *min, gdouble *max)
{
    const gdouble *base = field->data + row*field->xres + col;
    const gboolean invert = (masking == GWY_MASK_EXCLUDE);
    gdouble min1 = HUGE_VAL, max1 = -HUGE_VAL;
    guint ndata = 0;

    for (guint i = 0; i < height; i++) {
        const gdouble *d = base + i*field->xres;
        GwyMaskIter iter;
        gwy_mask_field_iter_init(mask, iter, maskcol, maskrow + i);
        gboolean prev = !gwy_mask_iter_get(iter) == invert;
        for (guint j = width-1; j; j--, d++) {
            gboolean curr = !gwy_mask_iter_get(iter) == invert;
            if (prev && curr) {
                gdouble v = d[1] - d[0];
                if (v < min1)
                    min1 = v;
                if (v > max1)
                    max1 = v;
                ndata++;
            }
            gwy_mask_iter_next(iter);
            prev = curr;
        }
    }
    *min = min1;
    *max = max1;
    return ndata;
}

static guint
slope_dist_vert_analyse(const GwyField *field,
                        guint col, guint row,
                        guint width, guint height,
                        gdouble *min, gdouble *max)
{
    const gdouble *base = field->data + row*field->xres + col;
    gdouble min1 = HUGE_VAL, max1 = -HUGE_VAL;

    for (guint i = 0; i < height-1; i++) {
        const gdouble *d1 = base + i*field->xres;
        const gdouble *d2 = d1 + field->xres;
        for (guint j = width; j; j--, d1++, d2++) {
            gdouble v = *d2 - *d1;
            if (v < min1)
                min1 = v;
            if (v > max1)
                max1 = v;
        }
    }
    *min = min1;
    *max = max1;
    return width*(height - 1);
}

static guint
slope_dist_vert_analyse_mask(const GwyField *field,
                             const GwyMaskField *mask,
                             GwyMaskingType masking,
                             guint col, guint row,
                             guint width, guint height,
                             guint maskcol, guint maskrow,
                             gdouble *min, gdouble *max)
{
    const gdouble *base = field->data + row*field->xres + col;
    const gboolean invert = (masking == GWY_MASK_EXCLUDE);
    gdouble min1 = HUGE_VAL, max1 = -HUGE_VAL;
    guint ndata = 0;

    for (guint i = 0; i < height-1; i++) {
        const gdouble *d1 = base + i*field->xres;
        const gdouble *d2 = d1 + field->xres;
        GwyMaskIter iter1, iter2;
        gwy_mask_field_iter_init(mask, iter1, maskcol, maskrow + i);
        gwy_mask_field_iter_init(mask, iter2, maskcol, maskrow + i+1);
        for (guint j = width; j; j--, d1++, d2++) {
            if ((!gwy_mask_iter_get(iter1) == invert)
                && (!gwy_mask_iter_get(iter2) == invert)) {
                gdouble v = *d2 - *d1;
                if (v < min1)
                    min1 = v;
                if (v > max1)
                    max1 = v;
                ndata++;
            }
            gwy_mask_iter_next(iter1);
            gwy_mask_iter_next(iter2);
        }
    }
    *min = min1;
    *max = max1;
    return ndata;
}

static void
slope_dist_horiz_gather(const GwyField *field,
                        guint col, guint row,
                        guint width, guint height,
                        GwyLine *line)
{
    const gdouble *base = field->data + row*field->xres + col;
    guint npoints = line->res;
    gdouble q = line->real*npoints*gwy_field_dx(field);
    gdouble min = line->off;

    for (guint i = 0; i < height; i++) {
        const gdouble *d = base + i*field->xres;
        for (guint j = width-1; j; j--, d++) {
            gdouble v = d[1] - d[0];
            guint k = (guint)((v - min)/q);
            // Fix rounding errors.
            if (G_UNLIKELY(k >= npoints))
                line->data[npoints-1] += 1;
            else
                line->data[k] += 1;
        }
    }
}

static void
slope_dist_horiz_gather_mask(const GwyField *field,
                             const GwyMaskField *mask,
                             GwyMaskingType masking,
                             guint col, guint row,
                             guint width, guint height,
                             guint maskcol, guint maskrow,
                             GwyLine *line)
{
    const gdouble *base = field->data + row*field->xres + col;
    const gboolean invert = (masking == GWY_MASK_EXCLUDE);
    guint npoints = line->res;
    gdouble q = line->real*npoints*gwy_field_dx(field);
    gdouble min = line->off;

    for (guint i = 0; i < height; i++) {
        const gdouble *d = base + i*field->xres;
        GwyMaskIter iter;
        gwy_mask_field_iter_init(mask, iter, maskcol, maskrow + i);
        gboolean prev = !gwy_mask_iter_get(iter) == invert;
        for (guint j = width-1; j; j--, d++) {
            gboolean curr = !gwy_mask_iter_get(iter) == invert;
            if (prev && curr) {
                gdouble v = d[1] - d[0];
                guint k = (guint)((v - min)/q);
                // Fix rounding errors.
                if (G_UNLIKELY(k >= npoints))
                    line->data[npoints-1] += 1;
                else
                    line->data[k] += 1;
            }
            gwy_mask_iter_next(iter);
            prev = curr;
        }
    }
}

static void
slope_dist_vert_gather(const GwyField *field,
                       guint col, guint row,
                       guint width, guint height,
                       GwyLine *line)
{
    const gdouble *base = field->data + row*field->xres + col;
    guint npoints = line->res;
    gdouble q = line->real*npoints*gwy_field_dy(field);
    gdouble min = line->off;

    for (guint i = 0; i < height-1; i++) {
        const gdouble *d1 = base + i*field->xres;
        const gdouble *d2 = d1 + field->xres;
        for (guint j = width; j; j--, d1++, d2++) {
            gdouble v = *d2 - *d1;
            guint k = (guint)((v - min)/q);
            // Fix rounding errors.
            if (G_UNLIKELY(k >= npoints))
                line->data[npoints-1] += 1;
            else
                line->data[k] += 1;
        }
    }
}

static void
slope_dist_vert_gather_mask(const GwyField *field,
                            const GwyMaskField *mask,
                            GwyMaskingType masking,
                            guint col, guint row,
                            guint width, guint height,
                            guint maskcol, guint maskrow,
                            GwyLine *line)
{
    const gdouble *base = field->data + row*field->xres + col;
    const gboolean invert = (masking == GWY_MASK_EXCLUDE);
    guint npoints = line->res;
    gdouble q = line->real*npoints*gwy_field_dy(field);
    gdouble min = line->off;

    for (guint i = 0; i < height-1; i++) {
        const gdouble *d1 = base + i*field->xres;
        const gdouble *d2 = d1 + field->xres;
        GwyMaskIter iter1, iter2;
        gwy_mask_field_iter_init(mask, iter1, maskcol, maskrow + i);
        gwy_mask_field_iter_init(mask, iter2, maskcol, maskrow + i+1);
        for (guint j = width; j; j--, d1++, d2++) {
            if ((!gwy_mask_iter_get(iter1) == invert)
                && (!gwy_mask_iter_get(iter2) == invert)) {
                gdouble v = *d2 - *d1;
                guint k = (guint)((v - min)/q);
                // Fix rounding errors.
                if (G_UNLIKELY(k >= npoints))
                    line->data[npoints-1] += 1;
                else
                    line->data[k] += 1;
            }
            gwy_mask_iter_next(iter1);
            gwy_mask_iter_next(iter2);
        }
    }
}

/**
 * gwy_field_slope_dist:
 * @field: A two-dimensional data field.
 * @rectangle: Area in @field to process.  Pass %NULL to process entire @field.
 * @mask: Mask specifying which values to take into account/exclude, or %NULL.
 * @masking: Masking mode to use (has any effect only with non-%NULL @mask).
 * @orientation: Orientation in which to compute the derivatives.
 * @cumulative: %TRUE to calculate cumulative distribution, %FALSE to calculate
 *              density.
 * @npoints: Distribution resolution, i.e. the number of histogram bins.
 *           Pass zero to choose a suitable resolution automatically.
 *
 * Calculates the distribution of slopes in a field.
 *
 * Slopes are calculated as horizontal or vertical derivatives of the value,
 * i.e. dz/dx or dz/dy.
 *
 * Returns: A new one-dimensional data line with the slope distribution.
 **/
GwyLine*
gwy_field_slope_dist(const GwyField *field,
                     const GwyRectangle *rectangle,
                     const GwyMaskField *mask,
                     GwyMaskingType masking,
                     GwyOrientation orientation,
                     gboolean cumulative,
                     guint npoints)
{
    guint col, row, width, height, maskcol, maskrow;
    GwyLine *line = NULL;
    if (!_gwy_field_check_mask(field, rectangle, mask, &masking,
                               &col, &row, &width, &height, &maskcol, &maskrow))
        goto fail;

    guint ndata;
    gdouble min, max;
    if (orientation == GWY_ORIENTATION_HORIZONTAL) {
        if (masking == GWY_MASK_IGNORE)
            ndata = slope_dist_horiz_analyse(field, col, row, width, height,
                                             &min, &max);
        else
            ndata = slope_dist_horiz_analyse_mask(field, mask, masking,
                                                  col, row, width, height,
                                                  maskcol, maskrow,
                                                  &min, &max);
        min /= gwy_field_dx(field);
        max /= gwy_field_dx(field);
    }
    else if (orientation == GWY_ORIENTATION_VERTICAL) {
        if (masking == GWY_MASK_IGNORE)
            ndata = slope_dist_vert_analyse(field, col, row, width, height,
                                            &min, &max);
        else
            ndata = slope_dist_vert_analyse_mask(field, mask, masking,
                                                 col, row, width, height,
                                                 maskcol, maskrow,
                                                 &min, &max);
        min /= gwy_field_dy(field);
        max /= gwy_field_dy(field);
    }
    else {
        g_critical("Invalid orientation %d", orientation);
        goto fail;
    }

    if (!ndata)
        goto fail;
    if (!npoints)
        npoints = gwy_round(3.49*cbrt(ndata));

    line = gwy_line_new_sized(npoints, TRUE);
    sanitize_range(&min, &max);
    line->off = min;
    line->real = max - min;

    if (orientation == GWY_ORIENTATION_HORIZONTAL) {
        if (masking == GWY_MASK_IGNORE)
            slope_dist_horiz_gather(field, col, row, width, height, line);
        else
            slope_dist_horiz_gather_mask(field, mask, masking,
                                         col, row, width, height,
                                         maskcol, maskrow,
                                         line);
    }
    else {
        if (masking == GWY_MASK_IGNORE)
            slope_dist_vert_gather(field, col, row, width, height, line);
        else
            slope_dist_vert_gather_mask(field, mask, masking,
                                        col, row, width, height,
                                        maskcol, maskrow,
                                        line);
    }

    if (cumulative) {
        gwy_line_accumulate(line);
        gwy_line_multiply(line, 1.0/line->data[npoints-1]);
    }
    else
        gwy_line_multiply(line, npoints/(max - min)/ndata);

fail:
    if (!line)
        line = gwy_line_new();

    gwy_unit_divide(gwy_line_get_unit_x(line),
                    gwy_field_get_unit_z(field), gwy_field_get_unit_xy(field));
    if (!cumulative)
        gwy_unit_power(gwy_line_get_unit_y(line),
                       gwy_field_get_unit_z(field), -1);

    return line;
}

static inline void
row_assign_mask(const GwyMaskField *mask,
                gsize col,
                gsize row,
                gsize width,
                gboolean invert,
                gdouble *out)
{
    GwyMaskIter iter;

    gwy_mask_field_iter_init(mask, iter, col, row);
    for (gsize j = width; j; j--, out++) {
        *out = (!gwy_mask_iter_get(iter) == invert);
        gwy_mask_iter_next(iter);
    }
}

static inline void
row_accumulate(gdouble *accum,
               const gdouble *data,
               gsize size)
{
    for (gsize j = size; j; j--, accum++, data++)
        *accum += *data;
}

// FFTW calculates unnormalised DFT so we divide the result of the first
// transformation with (1/√size)² = 1/size and keep the second transfrom as-is
// to obtain exactly g_k.

// An existing R2HC plan is usually used also for the final DCT.  Reconstruct
// the symmetrical real output from the half-complex output by throwing away
// the imaginary parts.
static inline void
row_hc_real_expand(const gdouble *in,
                   gdouble *out,
                   gsize size)
{
    gdouble *out2 = out + size-1;

    *out = *in;
    out++, in++;
    for (gsize j = (size + 1)/2 - 1; j; j--, out++, in++, out2--)
        *out = *out2 = *in;
    if (size % 2 == 0)
        *out = *in;
}

// Calculate the complex norm of R2HC output items (the result is stored in
// @out including the redundant even terms).
static inline void
row_hc_cnorm(const gdouble *in,
             gdouble *out,
             gsize size)
{
    const gdouble *in2 = in + size-1;
    gdouble *out2 = out + size-1;

    *out = (*in)*(*in)/size;
    out++, in++;
    for (gsize j = (size + 1)/2 - 1; j; j--, out++, in++, out2--, in2--)
        *out = *out2 = ((*in)*(*in) + (*in2)*(*in2))/size;
    if (size % 2 == 0)
        *out = (*in)*(*in)/size;
}

// Calculate the complex absolute value of R2HC output items, excluding the
// redundant even terms.  So the size of @out must be @size/2 + 1.
static inline void
row_hc_cabs_2(const gdouble *in,
              gdouble *out,
              gsize size)
{
    const gdouble *in2 = in + size-1;

    *out = fabs(*in);
    out++, in++;
    for (gsize j = (size + 1)/2 - 1; j; j--, out++, in++,in2--)
        *out = hypot(*in, *in2);
    if (size % 2 == 0)
        *out = fabs(*in);
}

// Calculate squared Fourier coefficients of zero-extended data.  The output
// is in @data again.
static inline void
row_extfft_cnorm(fftw_plan plan,
                 gdouble *data,
                 gdouble *workspace,
                 gsize size,
                 gsize width)
{
    gwy_clear(data + width, size - width);
    fftw_execute(plan);   // R2C transform data -> workspace
    row_hc_cnorm(workspace, data, size);
}

// Calculate the product A*B+AB*, equal to 2*(Re A Re B + Im A Im B), of two
// R2HC outputs (the result is added to @out including the redundant even
// terms).
static inline void
row_hc_cprod_accumulate(const gdouble *ina,
                        const gdouble *inb,
                        gdouble *out,
                        gsize size)
{
    const gdouble *ina2 = ina + size-1;
    const gdouble *inb2 = inb + size-1;
    gdouble *out2 = out + size-1;

    *out += 2.0*(*ina)*(*inb)/size;
    out++, ina++, inb++;
    for (guint j = (size + 1)/2 - 1;
         j;
         j--, out++, ina++, inb++, out2--, ina2--, inb2--) {
        gdouble v = 2.0*((*ina)*(*inb) + (*ina2)*(*inb2))/size;
        *out += v;
        *out2 += v;
    }
    if (size % 2 == 0)
        *out += 2.0*(*ina)*(*inb)/size;
}

// Level a row of data by subtracting the mean value.
static void
row_level(const gdouble *in,
          gdouble *out,
          guint n)
{
    gdouble sumsi = 0.0;
    const gdouble *pdata = in;
    for (guint i = n; i; i--, pdata++)
        sumsi += *pdata;

    gdouble a = sumsi/n;
    pdata = in;
    for (guint i = n; i; i--, pdata++, out++)
        *out = *pdata - a;
}

// Level a row of data by subtracting the mean value of data under mask and
// clear (set to zero) all data not under mask.  Note how the zeroes nicely
// ensure that the subsequent functions Just Work(TM) and don't need to know we
// use masking at all.
static guint
row_level_mask(const gdouble *in,
               gdouble *out,
               guint n,
               GwyMaskIter iter0,
               gboolean invert)
{
    GwyMaskIter iter = iter0;
    gdouble sumsi = 0.0;
    const gdouble *pdata = in;
    guint nd = 0;
    for (guint i = n; i; i--, pdata++) {
        if (!gwy_mask_iter_get(iter) == invert) {
            sumsi += *pdata;
            nd++;
        }
        gwy_mask_iter_next(iter);
    }

    // This can be division by zero but in that case we never use the value.
    gdouble a = sumsi/nd;
    pdata = in;
    iter = iter0;
    for (guint i = n; i; i--, pdata++, out++) {
        *out = (!gwy_mask_iter_get(iter) == invert) ? *pdata - a : 0.0;
        gwy_mask_iter_next(iter);
    }
    return nd;
}

/* Window a row using a sampled windowing function. */
static void
row_window(gdouble *data, const gdouble *window, guint n)
{
    for (guint i = n; i; i--, data++, window++)
        *data *= *window;
}

/* Level and count the number of valid data in a row */
static guint
row_level_and_count(const gdouble *in,
                    gdouble *out,
                    guint width,
                    const GwyMaskField *mask,
                    GwyMaskingType masking,
                    guint maskcol,
                    guint maskrow,
                    guint level)
{
    if (masking == GWY_MASK_IGNORE) {
        if (level)
            row_level(in, out, width);
        else
            gwy_assign(out, in, width);
        return width;
    }

    GwyMaskIter iter;
    gwy_mask_field_iter_init(mask, iter, maskcol, maskrow);
    gboolean invert = (masking == GWY_MASK_EXCLUDE);
    if (level)
        return row_level_mask(in, out, width, iter, invert);

    guint count = 0;
    for (guint i = width; i; i--, in++, out++) {
        if (!gwy_mask_iter_get(iter) == invert) {
           *out = *in;
           count++;
        }
        else
            *out = 0.0;
        gwy_mask_iter_next(iter);
    }
    return count;
}

/**
 * gwy_field_row_psdf:
 * @field: A two-dimensional data field.
 * @rectangle: Area in @field to process.  Pass %NULL to process entire @field.
 * @mask: Mask specifying which values to take into account/exclude, or %NULL.
 * @masking: Masking mode to use (has any effect only with non-%NULL @mask).
 * @windowing: Windowing type to use.
 * @level: The first polynomial degree to keep in the rows, lower degrees than
 *         @level are subtracted.  Note only values 0 (no levelling) and 1
 *         (subtract the mean value of each row) are available at present.  For
 *         SPM data, you usually wish to pass 1.
 *
 * Calculates the row-wise power spectrum density function of a rectangular
 * part of a field.
 *
 * The calculated PSDF has the natural number of points that follows from DFT,
 * i.e. @width/2+1.
 *
 * The reduction of the total energy by windowing is compensated by multiplying
 * the PSDF to make its sum of squares equal to the input data sum of squares.
 *
 * Masking is performed by omitting all terms that contain excluded pixels.
 * Since different rows contain different numbers of pixels, the resulting
 * PSDF is calculated as a weighted sum where each row's weight is proportional
 * to the number of contributing pixels.  In other words, the weighting is
 * fair: each contributing pixel has the same influence on the result.
 *
 * Returns: A new one-dimensional data line with the PSDF.
 **/
GwyLine*
gwy_field_row_psdf(const GwyField *field,
                   const GwyRectangle *rectangle,
                   const GwyMaskField *mask,
                   GwyMaskingType masking,
                   GwyWindowingType windowing,
                   guint level)
{
    guint col, row, width, height, maskcol, maskrow;
    GwyLine *line = NULL;
    if (!_gwy_field_check_mask(field, rectangle, mask, &masking,
                               &col, &row, &width, &height, &maskcol, &maskrow))
        goto fail;

    if (level > 1) {
        g_warning("Levelling degree %u is not supported, changing to 1.",
                  level);
        level = 1;
    }

    // An even size is necessary due to alignment constraints in FFTW.
    // Using this size for all buffers is a bit excessive but safe.
    line = gwy_line_new_sized(width/2 + 1, TRUE);
    gsize size = (width + 3)/4*4;
    const gdouble *base = field->data + row*field->xres + col;
    const gboolean invert = (masking == GWY_MASK_EXCLUDE);
    gdouble *fftin = fftw_malloc(5*size*sizeof(gdouble));
    gdouble *ffthcout = fftin + size;
    gdouble *accum_data = fftin + 2*size;
    gdouble *accum_mask = fftin + 3*size;
    gdouble *window = fftin + 4*size;
    guint nfullrows = 0, nemptyrows = 0;

    gwy_clear(accum_data, size);
    gwy_clear(accum_mask, size);

    gwy_fft_window_sample(window, width, windowing, 2);
    fftw_plan plan = fftw_plan_r2r_1d(width, fftin, ffthcout, FFTW_R2HC,
                                      FFTW_DESTROY_INPUT | _gwy_fft_rigour());
    for (guint i = 0; i < height; i++) {
        guint count = row_level_and_count(base + i*field->xres, fftin, width,
                                          mask, masking, maskcol, maskrow + i,
                                          level);
        if (!count) {
            nemptyrows++;
            continue;
        }

        // Calculate and gather squared Fourier coefficients of the data.
        row_window(fftin, window, width);
        row_extfft_cnorm(plan, fftin, ffthcout, width, width);
        row_accumulate(accum_data, fftin, width);

        // If all points in the row are included just note that as we can
        // calculate the corresponding denominators directly.  Otherwise
        // the mask has to be transformed too.
        if (count == width) {
            nfullrows++;
            continue;
        }

        // Calculate and gather squared Fourier coefficients of the mask.
        row_assign_mask(mask, maskcol, maskrow + i, width, invert, fftin);
        row_extfft_cnorm(plan, fftin, ffthcout, width, width);
        row_accumulate(accum_mask, fftin, width);
    }

    // Numerator of A_k, i.e. FFT of squared data Fourier coefficients.
    gwy_assign(fftin, accum_data, width);
    fftw_execute(plan);
    row_hc_real_expand(ffthcout, accum_data, width);

    // Denominator of A_k, i.e. FFT of squared mask Fourier coefficients.
    // Don't perform the FFT if there were no partial rows.
    if (nfullrows + nemptyrows < height) {
        gwy_assign(fftin, accum_mask, width);
        fftw_execute(plan);
        row_hc_real_expand(ffthcout, accum_mask, width);
    }
    for (guint j = 0; j < width; j++) {
        // Denominators must be rounded to integers because they are integers
        // and this permits to detect zeroes in the denominator.
        accum_mask[j] = gwy_round(accum_mask[j]) + nfullrows*width;
    }

    for (guint j = 0; j < width; j++)
        fftin[j] = accum_mask[j] ? accum_data[j]/accum_mask[j] : 0.0;

    // The transform is the other way round – for complex numbers.  Since it
    // is in fact a DCT here we don't care and run it as a forward transform.
    fftw_execute(plan);
    row_hc_cabs_2(ffthcout, line->data, width);
    fftw_destroy_plan(plan);
    fftw_free(fftin);

    gwy_line_multiply(line, gwy_field_dx(field)/(2*G_PI));
    line->real = G_PI/gwy_field_dx(field);
    line->off = -0.5*gwy_line_dx(line);

fail:
    if (!line)
        line = gwy_line_new();

    gwy_unit_power(gwy_line_get_unit_x(line), gwy_field_get_unit_xy(field), -1);
    gwy_unit_power_multiply(gwy_line_get_unit_y(line),
                            gwy_field_get_unit_xy(field), 1,
                            gwy_field_get_unit_z(field), 2);
    return line;
}

/**
 * gwy_field_row_acf:
 * @field: A two-dimensional data field.
 * @rectangle: Area in @field to process.  Pass %NULL to process entire @field.
 * @mask: Mask specifying which values to take into account/exclude, or %NULL.
 * @masking: Masking mode to use (has any effect only with non-%NULL @mask).
 * @level: The first polynomial degree to keep in the rows, lower degrees than
 *         @level are subtracted.  Note only values 0 (no levelling) and 1
 *         (subtract the mean value of each row) are available at present.  For
 *         SPM data, you usually wish to pass 1.
 *
 * Calculates the row-wise autocorrelation function of a rectangular part of a
 * field.
 *
 * The calculated ACF has the natural number of points, i.e. @width.
 *
 * Masking is performed by omitting all terms that contain excluded pixels.
 * Since different rows contain different numbers of pixels, the resulting
 * ACF values are calculated as a weighted sums where weight of each row's
 * contribution is proportional to the number of contributing terms.  In other
 * words, the weighting is fair: each contributing pixel has the same influence
 * on the result.
 *
 * Returns: A new one-dimensional data line with the ACF.
 **/
GwyLine*
gwy_field_row_acf(const GwyField *field,
                  const GwyRectangle *rectangle,
                  const GwyMaskField *mask,
                  GwyMaskingType masking,
                  guint level)
{
    guint col, row, width, height, maskcol, maskrow;
    GwyLine *line = NULL;
    if (!_gwy_field_check_mask(field, rectangle, mask, &masking,
                               &col, &row, &width, &height, &maskcol, &maskrow))
        goto fail;

    if (level > 1) {
        g_warning("Levelling degree %u is not supported, changing to 1.",
                  level);
        level = 1;
    }

    // Transform size must be at least twice the data size for zero padding.
    // An even size is necessary due to alignment constraints in FFTW.
    // Using this size for all buffers is a bit excessive but safe.
    line = gwy_line_new_sized(width, TRUE);
    gsize size = gwy_fft_nice_transform_size((width + 1)/2*4);
    const gdouble *base = field->data + row*field->xres + col;
    const gboolean invert = (masking == GWY_MASK_EXCLUDE);
    gdouble *fftin = fftw_malloc(4*size*sizeof(gdouble));
    gdouble *ffthcout = fftin + size;
    gdouble *accum_data = fftin + 2*size;
    gdouble *accum_mask = fftin + 3*size;
    guint nfullrows = 0, nemptyrows = 0;

    fftw_plan plan = fftw_plan_r2r_1d(size, fftin, ffthcout, FFTW_R2HC,
                                      FFTW_DESTROY_INPUT | _gwy_fft_rigour());
    gwy_clear(accum_data, size);
    gwy_clear(accum_mask, size);

    // Gather squared Fourier coefficients for all rows
    for (guint i = 0; i < height; i++) {
        guint count = row_level_and_count(base + i*field->xres, fftin, width,
                                          mask, masking, maskcol, maskrow + i,
                                          level);
        if (!count) {
            nemptyrows++;
            continue;
        }

        // Calculate and gather squared Fourier coefficients of the data.
        row_extfft_cnorm(plan, fftin, ffthcout, size, width);
        row_accumulate(accum_data, fftin, size);

        // If all points in the row are included just note that as we can
        // calculate the corresponding denominators directly.  Otherwise
        // the mask has to be transformed too.
        if (count == width) {
            nfullrows++;
            continue;
        }

        // Calculate and gather squared Fourier coefficients of the mask.
        row_assign_mask(mask, maskcol, maskrow + i, width, invert, fftin);
        row_extfft_cnorm(plan, fftin, ffthcout, size, width);
        row_accumulate(accum_mask, fftin, size);
    }

    // Numerator of G_k, i.e. FFT of squared data Fourier coefficients.
    // The FFTW halfcomplex format starts with the real data.
    gwy_assign(fftin, accum_data, size);
    fftw_execute(plan);
    gwy_assign(accum_data, ffthcout, width);

    // Denominator of G_k, i.e. FFT of squared mask Fourier coefficients.
    // Don't perform the FFT if there were no partial rows.
    if (nfullrows + nemptyrows < height) {
        gwy_assign(fftin, accum_mask, size);
        fftw_execute(plan);
        gwy_assign(accum_mask, ffthcout, width);
    }
    // The FFTW halfcomplex format starts with the real data.
    for (guint j = 0; j < width; j++) {
        // Denominators must be rounded to integers because they are integers
        // and this permits to detect zeroes in the denominator.
        accum_mask[j] = gwy_round(accum_mask[j]) + nfullrows*(width - j);
    }

    for (guint j = 0; j < line->res; j++)
        line->data[j] = accum_mask[j] ? accum_data[j]/accum_mask[j] : 0.0;

    fftw_destroy_plan(plan);
    fftw_free(fftin);
    line->real = gwy_field_dx(field)*line->res;
    line->off = -0.5*gwy_line_dx(line);

fail:
    if (!line)
        line = gwy_line_new();

    gwy_unit_power(gwy_line_get_unit_x(line), gwy_field_get_unit_xy(field), -1);
    gwy_unit_power_multiply(gwy_line_get_unit_y(line),
                            gwy_field_get_unit_xy(field), 1,
                            gwy_field_get_unit_z(field), 2);
    return line;
}

/**
 * gwy_field_row_hhcf:
 * @field: A two-dimensional data field.
 * @rectangle: Area in @field to process.  Pass %NULL to process entire @field.
 * @mask: Mask specifying which values to take into account/exclude, or %NULL.
 * @masking: Masking mode to use (has any effect only with non-%NULL @mask).
 * @level: The first polynomial degree to keep in the rows, lower degrees than
 *         @level are subtracted.  Note only values 0 (no levelling) and 1
 *         (subtract the mean value of each row) are available at present.
 *         There is no difference for HHCF.
 *
 * Calculates the row-wise height-height correlation function of a rectangular
 * part of a field.
 *
 * The calculated HHCF has the natural number of points, i.e. @width.
 *
 * Masking is performed by omitting all terms that contain excluded pixels.
 * Since different rows contain different numbers of pixels, the resulting
 * HHCF values are calculated as a weighted sums where weight of each row's
 * contribution is proportional to the number of contributing terms.  In other
 * words, the weighting is fair: each contributing pixel has the same influence
 * on the result.
 *
 * Returns: A new one-dimensional data line with the HHCF.
 **/
GwyLine*
gwy_field_row_hhcf(const GwyField *field,
                   const GwyRectangle *rectangle,
                   const GwyMaskField *mask,
                   GwyMaskingType masking,
                   guint level)
{
    guint col, row, width, height, maskcol, maskrow;
    GwyLine *line = NULL;
    if (!_gwy_field_check_mask(field, rectangle, mask, &masking,
                               &col, &row, &width, &height, &maskcol, &maskrow))
        goto fail;

    if (level > 1) {
        g_warning("Levelling degree %u is not supported, changing to 1.",
                  level);
        level = 1;
    }

    // Transform size must be at least twice the data size for zero padding.
    // An even size is necessary due to alignment constraints in FFTW.
    // Using this size for all buffers is a bit excessive but safe.
    line = gwy_line_new_sized(width, TRUE);
    gsize size = gwy_fft_nice_transform_size((width + 1)/2*4);
    const gdouble *base = field->data + row*field->xres + col;
    const gboolean invert = (masking == GWY_MASK_EXCLUDE);
    gdouble *fftin = fftw_malloc(6*size*sizeof(gdouble));
    gdouble *ffthcout = fftin + size;
    gdouble *accum_data = fftin + 2*size;
    gdouble *accum_mask = fftin + 3*size;
    gdouble *accum_v = fftin + 4*size;
    gdouble *tmp = fftin + 5*size;
    guint nfullrows = 0, nemptyrows = 0;
    gdouble *p;
    const gdouble *q, *qq;

    fftw_plan plan = fftw_plan_r2r_1d(size, fftin, ffthcout, FFTW_R2HC,
                                      FFTW_DESTROY_INPUT | _gwy_fft_rigour());
    gwy_clear(accum_data, size);
    gwy_clear(accum_mask, size);
    gwy_clear(accum_v, size);

    // Gather V_ν-2|Z_ν|² for all rows, except that for full rows we actually
    // gather just -2|Z_ν|² because v_k can be calculated without DFT.
    for (guint i = 0; i < height; i++) {
        guint count = row_level_and_count(base + i*field->xres, fftin, width,
                                          mask, masking, maskcol, maskrow + i,
                                          level);
        if (!count) {
            nemptyrows++;
            continue;
        }

        if (count == width) {
            // Calculate v_k before FFT destroys the input levelled/filtered
            // data.
            gdouble sum = 0.0;
            q = fftin;
            qq = fftin + (width-1);
            p = accum_v + (width-1);
            for (guint j = 0; j < width; j++, q++, qq--, p--) {
                sum += (*q)*(*q) + (*qq)*(*qq);
                *p += sum;
            }
        }
        else {
            // For partial rows, we will need the data later to calculate FFT
            // of their squares.  Save them to the line that conveniently has
            // the right size.
            gwy_assign(line->data, fftin, width);
        }

        // Calculate and gather -2 times squared Fourier coefficients.
        row_extfft_cnorm(plan, fftin, ffthcout, size, width);
        q = fftin;
        p = accum_data;
        for (guint j = size; j; j--, p++, q++)
            *p += -2.0*(*q);

        if (count == width) {
            nfullrows++;
            continue;
        }

        // First calculate U_ν (Fourier cofficients of squared data).  Save
        // them to tmp.
        q = line->data;
        p = fftin;
        for (guint j = width; j; j--, p++, q++)
            *p = (*q)*(*q);
        gwy_clear(fftin + width, size - width);
        fftw_execute(plan);
        gwy_assign(tmp, ffthcout, size);

        // Mask.  We need the intermediate result C_ν to combine it with U_ν.
        row_assign_mask(mask, maskcol, maskrow + i, width, invert, fftin);
        gwy_clear(fftin + width, size - width);
        fftw_execute(plan);

        // Accumulate V_ν (calculated from C_ν and U_ν) to accum_data.
        row_hc_cprod_accumulate(tmp, ffthcout, accum_data, size);

        // And accumulate squared mask Fourier coeffs |C_ν|².
        row_hc_cnorm(ffthcout, fftin, size);
        row_accumulate(accum_mask, fftin, size);
    }

    // Numerator of H_k, excluding non-DFT data in v_k.
    // The FFTW halfcomplex format starts with the real data.
    gwy_assign(fftin, accum_data, size);
    fftw_execute(plan);
    // Combine it with v_k to get the full numerator in accum_data.
    q = ffthcout;
    qq = accum_v;
    p = accum_data;
    for (guint j = width; j; j--, p++, q++, qq++)
        *p = *q + *qq;

    // Denominator of H_k, i.e. FFT of squared mask Fourier coefficients.
    // Don't perform the FFT if there were no partial rows.
    if (nfullrows + nemptyrows < height) {
        gwy_assign(fftin, accum_mask, size);
        fftw_execute(plan);
        gwy_assign(accum_mask, ffthcout, width);
    }
    // The FFTW halfcomplex format starts with the real data.
    for (guint j = 0; j < width; j++) {
        // Denominators must be rounded to integers because they are integers
        // and this permits to detect zeroes in the denominator.
        accum_mask[j] = gwy_round(accum_mask[j]) + nfullrows*(width - j);
    }

    for (guint j = 0; j < line->res; j++)
        line->data[j] = accum_mask[j] ? accum_data[j]/accum_mask[j] : 0.0;

    fftw_destroy_plan(plan);
    fftw_free(fftin);
    line->real = gwy_field_dx(field)*line->res;
    line->off = -0.5*gwy_line_dx(line);

fail:
    if (!line)
        line = gwy_line_new();

    gwy_unit_power(gwy_line_get_unit_x(line), gwy_field_get_unit_xy(field), -1);
    gwy_unit_power_multiply(gwy_line_get_unit_y(line),
                            gwy_field_get_unit_xy(field), 1,
                            gwy_field_get_unit_z(field), 2);
    return line;
}


/**
 * SECTION: field-distributions
 * @section_id: GwyField-distributions
 * @title: GwyField distributions
 * @short_description: One-dimensional distributions and functionals of fields
 *
 * Statistical distribution densities are normalised so that their integral,
 * that can also be calculated as gwy_line_mean(line)*line->real, is unity.
 * Cumulative distribution values then always line in the interval [0,1].
 **/

/**
 * GwyOrientation:
 * @GWY_ORIENTATION_HORIZONTAL: Horizontal orientation.
 * @GWY_ORIENTATION_VERTICAL: Vertical orientation.
 *
 * Orientation type.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
