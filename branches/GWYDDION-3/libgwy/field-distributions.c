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

static GwyLine*
create_line_for_dist(const GwyMaskField *mask,
                     GwyMaskingType masking,
                     guint col, guint row,
                     guint width, guint height,
                     guint *npoints)
{
    if (!*npoints) {
        guint ndata = width*height;
        if (masking == GWY_MASK_INCLUDE)
            ndata = gwy_mask_field_part_count(mask, col, row, width, height,
                                              TRUE);
        else if (masking == GWY_MASK_EXCLUDE)
            ndata = gwy_mask_field_part_count(mask, col, row, width, height,
                                              FALSE);

        if (!ndata)
            return NULL;
        *npoints = gwy_round(3.49*cbrt(ndata));
        *npoints = MAX(*npoints, 1);
    }
    return gwy_line_new_sized(*npoints, TRUE);
}

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
 * gwy_field_part_value_dist:
 * @field: A two-dimensional data field.
 * @mask: Mask specifying which values to take into account/exclude, or %NULL.
 * @masking: Masking mode to use (has any effect only with non-%NULL @mask).
 * @col: Column index of the upper-left corner of the rectangle.
 * @row: Row index of the upper-left corner of the rectangle.
 * @width: Rectangle width (number of columns).
 * @height: Rectangle height (number of rows).
 * @cumulative: %TRUE to calculate cumulative distribution, %FALSE to calculate
 *              density.
 * @npoints: Distribution resolution, i.e. the number of histogram bins.
 *           Pass zero to choose a suitable resolution automatically.
 *
 * Calculates the distribution of values in a rectangular part of a field.
 *
 * Returns: A new one-dimensional data line with the value distribution.
 **/
GwyLine*
gwy_field_part_value_dist(GwyField *field,
                          const GwyMaskField *mask,
                          GwyMaskingType masking,
                          guint col, guint row,
                          guint width, guint height,
                          gboolean cumulative,
                          guint npoints)
{
    guint maskcol, maskrow;
    GwyLine *line = NULL;
    if (!_gwy_field_check_mask(field, mask, &masking,
                               col, row, width, height, &maskcol, &maskrow)
        || !(line = create_line_for_dist(mask, masking,
                                         maskcol, maskrow, width, height,
                                         &npoints)))
        goto fail;

    gdouble min, max;
    gwy_field_part_min_max(field, mask, masking, col, row, width, height,
                           &min, &max);
    sanitize_range(&min, &max);
    npoints = line->res;

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
 * gwy_field_part_slope_dist:
 * @field: A two-dimensional data field.
 * @mask: Mask specifying which values to take into account/exclude, or %NULL.
 * @masking: Masking mode to use (has any effect only with non-%NULL @mask).
 * @col: Column index of the upper-left corner of the rectangle.
 * @row: Row index of the upper-left corner of the rectangle.
 * @width: Rectangle width (number of columns).
 * @height: Rectangle height (number of rows).
 * @orientation: Orientation in which to compute the derivatives.
 * @cumulative: %TRUE to calculate cumulative distribution, %FALSE to calculate
 *              density.
 * @npoints: Distribution resolution, i.e. the number of histogram bins.
 *           Pass zero to choose a suitable resolution automatically.
 *
 * Calculates the distribution of slopes in a rectangular part of a field.
 *
 * Slopes are calculated as horizontal or vertical derivatives of the value,
 * i.e. dz/dx or dz/dy.
 *
 * Returns: A new one-dimensional data line with the slope distribution.
 **/
GwyLine*
gwy_field_part_slope_dist(GwyField *field,
                          const GwyMaskField *mask,
                          GwyMaskingType masking,
                          guint col, guint row,
                          guint width, guint height,
                          GwyOrientation orientation,
                          gboolean cumulative,
                          guint npoints)
{
    guint maskcol, maskrow;
    GwyLine *line = NULL;
    if (!_gwy_field_check_mask(field, mask, &masking,
                               col, row, width, height, &maskcol, &maskrow))
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
row_accumulate(const gdouble *data,
               gdouble *accum,
               gsize size)
{
    for (gsize j = size; j; j--, accum++, data++)
        *accum += *data;
}

// FFTW calculates unnormalized DFT so we divide the result of the first
// transformation with (1/√size)² = 1/size and keep the second transfrom as-is
// to obtain exactly g_k.

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

// Calculate squared Fourier coefficients of zero-extended data.  The output
// is in @data again.
static inline void
row_extfft_cnorm(fftw_plan plan,
                 gdouble *data,
                 gdouble *workspace,
                 gsize width,
                 gsize size)
{
    gwy_clear(data + width, size - width);
    fftw_execute(plan);   // R2C transform data -> workspace
    row_hc_cnorm(workspace, data, size);
}

// Level a row of data by subtracting the mean value.
static void
row_level(gdouble *data,
          guint n)
{
    gdouble sumsi = 0.0;
    gdouble *pdata = data;
    for (guint i = n; i; i--, pdata++)
        sumsi += *pdata;

    gdouble a = sumsi/n;
    pdata = data;
    for (guint i = n; i; i--, pdata++)
        *pdata -= a;
}

// Level a row of data by subtracting the mean value of data under mask and
// clear (set to zero) all data not under mask.  Note how the zeroes nicely
// ensure that the subsequent functions Just Work(TM) and don't need to know we
// use masking at all.
static guint
row_level_mask(gdouble *data,
               guint n,
               GwyMaskIter iter0,
               gboolean invert)
{
    GwyMaskIter iter = iter0;
    gdouble sumsi = 0.0;
    gdouble *pdata = data;
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
    pdata = data;
    iter = iter0;
    for (guint i = n; i; i--, pdata++) {
        if (!gwy_mask_iter_get(iter) == invert)
            *pdata -= a;
        else
            *pdata = 0.0;
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
row_level_and_count(gdouble *buffer,
                    guint width,
                    const GwyMaskField *mask,
                    GwyMaskingType masking,
                    guint maskcol,
                    guint maskrow,
                    guint level)
{
    if (masking == GWY_MASK_IGNORE) {
        if (level)
            row_level(buffer, width);
        return width;
    }

    if (level) {
        GwyMaskIter iter;
        gwy_mask_field_iter_init(mask, iter, maskcol, maskrow);
        return row_level_mask(buffer, width, iter, masking == GWY_MASK_EXCLUDE);
    }
    return gwy_mask_field_part_count(mask, maskcol, maskrow, width, 1,
                                     masking == GWY_MASK_INCLUDE);
}

/**
 * gwy_field_part_row_psdf:
 * @field: A two-dimensional data field.
 * @mask: Mask specifying which values to take into account/exclude, or %NULL.
 * @masking: Masking mode to use (has any effect only with non-%NULL @mask).
 * @col: Column index of the upper-left corner of the rectangle.
 * @row: Row index of the upper-left corner of the rectangle.
 * @width: Rectangle width (number of columns).
 * @height: Rectangle height (number of rows).
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
#if 0
GwyLine*
gwy_field_part_row_psdf(GwyField *field,
                        const GwyMaskField *mask,
                        GwyMaskingType masking,
                        guint col, guint row,
                        guint width, guint height,
                        GwyWindowingType windowing,
                        guint level)
{
    guint maskcol, maskrow;
    GwyLine *line = NULL;
    if (!_gwy_field_check_mask(field, mask, &masking,
                               col, row, width, height, &maskcol, &maskrow))
        goto fail;

    if (level > 1) {
        g_warning("Levelling degree %u is not supported, changing to 1.",
                  level);
        level = 1;
    }

    // An even size is necessary due to alignment constraints in FFTW.
    // Using this size for all buffers is a bit excessive but safe.
    line = gwy_line_new_sized(width/2 + 1, TRUE);
    gsize size = (width + 1)/2*2;
    const gdouble *base = field->data + row*field->xres + col;
    gdouble *buffer = fftw_malloc(3*size*sizeof(gdouble));
    gdouble *window = buffer + size;
    gdouble *ffthc = window + size;
    gdouble weight = 0.0;

    gwy_fft_window_sample(window, width, windowing);
    fftw_plan plan = fftw_plan_dft_r2c_1d(width, buffer, (fftw_complex*)ffthc,
                                          _GWY_FFTW_PATIENCE);
    for (guint i = 0; i < height; i++) {
        const gdouble *d = base + i*field->xres;
        ASSIGN(buffer, d, width);
        guint ndata = row_level_and_count(buffer, width,
                                          mask, masking, maskcol, maskrow + i,
                                          level);
        if (!ndata)
            continue;
        weight += ndata;
        // We want to estimate the full PSDF from the incomplete data which
        // would involve multiplying it by @width/@ndata.  On the other hand,
        // we want to weight the input data fairly which would mean dividing it
        // by the same factor again.  So, keep @sum2 as-is and divive
        // everything with @weight at the end instead.
        gdouble sum2 = row_sum_squares(buffer, width);
        if (!sum2)
            continue;
        row_window(buffer, window, width);
        fftw_execute(plan);
        row_psdf(ffthc, buffer, width, sum2);
        gdouble *p = line->data;
        const gdouble *q = buffer;
        for (guint j = line->res; j; j--, p++, q++)
            *p += *q;
    }
    fftw_destroy_plan(plan);
    fftw_free(buffer);

    if (weight)
        gwy_line_multiply(line, gwy_field_dx(field)/(2*G_PI*weight));
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
#endif
GwyLine*
gwy_field_part_row_psdf(GwyField *field,
                        const GwyMaskField *mask,
                        GwyMaskingType masking,
                        guint col, guint row,
                        guint width, guint height,
                        GwyWindowingType windowing,
                        guint level)
{
    guint maskcol, maskrow;
    GwyLine *line = NULL;
    if (!_gwy_field_check_mask(field, mask, &masking,
                               col, row, width, height, &maskcol, &maskrow))
        goto fail;

    if (level > 1) {
        g_warning("Levelling degree %u is not supported, changing to 1.",
                  level);
        level = 1;
    }

    // An even size is necessary due to alignment constraints in FFTW.
    // Using this size for all buffers is a bit excessive but safe.
    line = gwy_line_new_sized(width/2 + 1, TRUE);
    gsize size = (width + 1)/2*2;
    const gdouble *base = field->data + row*field->xres + col;
    const gboolean invert = (masking == GWY_MASK_EXCLUDE);
    gdouble *fftin = fftw_malloc(5*size*sizeof(gdouble));
    gdouble *ffthcout = fftin + size;
    gdouble *accum_data = fftin + 2*size;
    gdouble *accum_mask = fftin + 3*size;
    gdouble *window = fftin + 4*size;
    guint nfullrows = 0;

    gwy_clear(accum_data, size);
    gwy_clear(accum_mask, size);

    gwy_fft_window_sample(window, width, windowing, 2);
    fftw_plan plan = fftw_plan_dft_r2c_1d(width, fftin, (fftw_complex*)ffthcout,
                                          _GWY_FFTW_PATIENCE);
    for (guint i = 0; i < height; i++) {
        const gdouble *d = base + i*field->xres;

        ASSIGN(fftin, d, width);
        guint count = row_level_and_count(fftin, width,
                                          mask, masking, maskcol, maskrow + i,
                                          level);
        if (!count)
            continue;

        // Calculate and gather squared Fourier coefficients of the data.
        row_window(fftin, window, width);
        row_extfft_cnorm(plan, fftin, ffthcout, width, width);
        row_accumulate(fftin, accum_data, width);

        // If all points in the row are included just note that as we can
        // calculate the corresponding denominators directly.  Otherwise
        // the mask has to be transformed too.
        if (masking == GWY_MASK_IGNORE || count == width) {
            nfullrows++;
            continue;
        }

        // Calculate and gather squared Fourier coefficients of the mask.
        row_assign_mask(mask, maskcol, maskrow + i, width, invert, fftin);
        row_extfft_cnorm(plan, fftin, ffthcout, width, width);
        row_accumulate(fftin, accum_mask, width);
    }

    // Numerator of A_k, i.e. FFT of squared data Fourier coefficients.
    ASSIGN(fftin, accum_data, size);
    fftw_execute(plan);
    // The FFTW halfcomplex format starts with the real data.
    ASSIGN(accum_data, ffthcout, width);
    for (guint j = 0; j < width; j++)
        accum_data[j] /= width;

    // Denominator of A_k, i.e. FFT of squared mask Fourier coefficients.
    ASSIGN(fftin, accum_mask, size);
    fftw_execute(plan);
    // The FFTW halfcomplex format starts with the real data.
    ASSIGN(accum_mask, ffthcout, width);
    for (guint j = 0; j < width; j++) {
        // Denominators must be rounded to integers because they are integers
        // and this permits to detect zeroes in the denominator.
        accum_mask[j] += gwy_round(accum_mask[j]/width) + nfullrows*width;
    }

    for (guint j = 0; j < width; j++)
        fftin[j] = accum_mask[j] ? accum_data[j]/accum_mask[j] : 0.0;

    // The transform is the other way round – for complex numbers.  Since it
    // is in fact a DCT here we don't care and run it as a forward transform.
    fftw_execute(plan);
    // The FFTW halfcomplex format starts with the real data.
    ASSIGN(line->data, ffthcout, line->res);
    fftw_destroy_plan(plan);
    fftw_free(fftin);

    // TODO
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

// FIXME: As the second transform input is real and even we could use DCT here
// instead of R2C. However, this requires even @size as for even size the data
// layout is 01234321 which is even and supported by FFTW, whereas for an odd
// size the layout is 0123321 which is odd and unsupported by FFTW.  Since we
// obtain @size by multiplication by 4 this should not be a problem to ensure.
// FFTW docs contain a note about R00 DCT transforms being slow though.
static inline void
row_acf(fftw_plan plan,
        gdouble *in,
        gdouble *out,
        guint width,
        guint size)
{
    gwy_clear(in + width, size - width);
    fftw_execute(plan);   // R2C transform in -> out
    row_hc_cnorm(out, in, size);
    fftw_execute(plan);   // R2C transform in -> out
}

/**
 * gwy_field_part_row_acf:
 * @field: A two-dimensional data field.
 * @mask: Mask specifying which values to take into account/exclude, or %NULL.
 * @masking: Masking mode to use (has any effect only with non-%NULL @mask).
 * @col: Column index of the upper-left corner of the rectangle.
 * @row: Row index of the upper-left corner of the rectangle.
 * @width: Rectangle width (number of columns).
 * @height: Rectangle height (number of rows).
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
gwy_field_part_row_acf(GwyField *field,
                       const GwyMaskField *mask,
                       GwyMaskingType masking,
                       guint col, guint row,
                       guint width, guint height,
                       guint level)
{
    guint maskcol, maskrow;
    GwyLine *line = NULL;
    if (!_gwy_field_check_mask(field, mask, &masking,
                               col, row, width, height, &maskcol, &maskrow)
        || width < 2)
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
    gdouble *accum_mask = fftin + 3*size;    // used only with mask
    guint nfullrows = 0;

    fftw_plan plan = fftw_plan_dft_r2c_1d(width, fftin, (fftw_complex*)ffthcout,
                                          _GWY_FFTW_PATIENCE);
    gwy_clear(accum_data, size);
    gwy_clear(accum_mask, size);

    // Gather squared Fourier coefficients for all rows
    for (guint i = 0; i < height; i++) {
        const gdouble *d = base + i*field->xres;

        ASSIGN(fftin, d, width);
        guint count = row_level_and_count(fftin, width,
                                          mask, masking, maskcol, maskrow + i,
                                          level);
        if (!count)
            continue;

        // Calculate and gather squared Fourier coefficients of the data.
        row_extfft_cnorm(plan, fftin, ffthcout, size, width);
        row_accumulate(fftin, accum_data, size);

        // If all points in the row are included just note that as we can
        // calculate the corresponding denominators directly.  Otherwise
        // the mask has to be transformed too.
        if (masking == GWY_MASK_IGNORE || count == width) {
            nfullrows++;
            continue;
        }

        // Calculate and gather squared Fourier coefficients of the mask.
        row_assign_mask(mask, maskcol, maskrow + i, width, invert, fftin);
        row_extfft_cnorm(plan, fftin, ffthcout, size, width);
        row_accumulate(fftin, accum_mask, size);
    }

    // Numerator of G_k, i.e. FFT of squared data Fourier coefficients.
    ASSIGN(fftin, accum_data, size);
    fftw_execute(plan);
    // The FFTW halfcomplex format starts with the real data.
    ASSIGN(accum_data, ffthcout, width);
    for (guint j = 0; j < width; j++)
        accum_data[j] /= size;

    // Denominator of G_k, i.e. FFT of squared mask Fourier coefficients.
    ASSIGN(fftin, accum_mask, size);
    fftw_execute(plan);
    // The FFTW halfcomplex format starts with the real data.
    ASSIGN(accum_mask, ffthcout, width);
    for (guint j = 0; j < width; j++) {
        // Denominators must be rounded to integers because they are integers
        // and this permits to detect zeroes in the denominator.
        accum_mask[j] += gwy_round(accum_mask[j]/size) + nfullrows*(width - j);
    }

    for (guint j = 0; j < line->res; j++) {
        line->data[j] = accum_mask[j] ? accum_data[j]/accum_mask[j] : 0.0;
    }

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

/*
 * HHCF of incomplete data.
 *
 * The HHCF is defined as follows
 *
 *       1  N-1-k
 * H  = –––   ∑  (z  - z   )²                             (1)
 *  k   N-k  j=0   j    j+k
 *
 * To extend this definition to incomplete data, we sum only over the available
 * data in (1) and instead of dividing by N-k we divide by P_k that has been
 * already derived for ACF.  So, again, we focus on the unnormalized
 * height-height correlation function
 *
 *      N-1-k
 * h  =   ∑  (z  - z   )² m  m                            (2)
 *  k    j=0   j    j+k    j  j+k
 *
 * Note the data mask has to be explicitly included to ensure vanishing of the
 * terms where at least one of z_j and z_{j+k} is unavailable.  Note that if
 * we put z_j ≡ 0 for unavailable and padding data as usual, it holds
 *
 * m  z  = z                                              (3)
 *  j  j    j
 *
 * Using this identity, we expand the square in (2) to three terms that can be
 * each summed separately and obtain
 *
 *      N-1-k          N-1-k           N-1-k
 * h  =   ∑  z² m    +   ∑  z²   m  - 2  ∑   z  z         (4)
 *  k    j=0  j  j+k    j=0  j+k  j     j=0   j  j+k
 *
 * Unlike the sum in (1), each of these sums can be extended to S ≥ 2N terms
 * and made periodic without changing its value.  The last sum is g_k and the
 * first two are convolutions of mask m_j with u_j = (z_j)².  They are equal to
 *
 * S-1     *  -2πikν/S        S-1  *     -2πikν/S
 *  ∑  U  M  e          and    ∑  U  M  e                 (5)
 * ν=0  ν  ν                  ν=0  ν  ν
 *
 * respectively.  Since
 *
 *     *    *
 * U  M  + U  M  = 2V                                     (6)
 *  ν  ν    ν  ν     ν
 *
 * is again real and even even, we can write
 *
 *      S-1     -2πikν/S
 * h  =  ∑  V  e          - 2g  = v  - 2g                 (7)
 *  k   j=0  ν                k    k     k
 *
 * which is a DCT-type transfrom.
 *
 * The weighting should be done as in the case of ACF.
 *
 * To summarize, the evaluation of HHCF of partial data requires three R2C
 * transforms (to calculate Z_ν, M_ν and U_ν) and three DCT transforms (to
 * calculate g_k, P_k and v_k). For full data, only one R2C and one DCT is
 * sufficient -- the same as for ACF because in this case v_k can be expressed
 * directly.  Defining the partial sum of squares
 *
 *       k
 * s  =  ∑  z² + z²                                      (8)
 *  k   j=0  j    N-1-j
 *
 * that can be calculated incrementally, it is obvious that v_k = s_{N-1-k}.
 *
 * Calculation scheme for complete data:
 *
 *      ┌────→ Z_ν ──────→ g_k ─────┐
 *      │ R2C        DCT            │
 * z_j ─┤                           ├─→ h_k
 *      │                           │
 *      └────→ s_k ──────→ v_k ─────┘
 *         ∑
 *
 * Calculation scheme for incomplete data:
 *
 *      ┌────→ Z_ν ──────→ g_k ──────────────────────┐
 *      │ R2C        DCT                             │
 * z_j ─┤                                            │
 *      │                                            ├─→ h_k
 *      └────→ u_j ──────→ U_ν ─┐                    │
 *                   R2C        │                    │
 *                              ├─→ V_ν ──────→ v_k ─┘
 *                              │         DCT
 *                  ┌───────────┘
 *                  │
 * m_j ──────→ M_ν ─┤
 *       R2C        │
 *                  └────→ P_k
 *                    DCT
 *
 * Arrows with symbols R2C, DCT and ∑ represent the corresponding transforms,
 * arrows without a symbol correspond to simple item-wise arithmetic
 * operations.
 */

// Calculate the product (A*B+AB*)/2, equal to (Re A Re B + Im A Im B), of two
// R2HC outputs (the result is stored in @out including the redundant even
// terms).
static inline void
row_hc_cprod(const gdouble *ina,
             const gdouble *inb,
             gdouble *out,
             gsize size)
{
    const gdouble *ina2 = ina + size-1;
    const gdouble *inb2 = inb + size-1;
    gdouble *out2 = out + size-1;

    *out = (*ina)*(*inb)/size;
    out++, ina++, inb++;
    for (guint j = (size + 1)/2 - 1;
         j;
         j--, out++, ina++, inb++, out2--, ina2--, inb2--)
        *out = *out2 = ((*ina)*(*inb) + (*ina2)*(*inb2))/size;
    if (size % 2 == 0)
        *out = (*ina)*(*inb)/size;
}

/**
 * gwy_field_part_row_hhcf:
 * @field: A two-dimensional data field.
 * @mask: Mask specifying which values to take into account/exclude, or %NULL.
 * @masking: Masking mode to use (has any effect only with non-%NULL @mask).
 * @col: Column index of the upper-left corner of the rectangle.
 * @row: Row index of the upper-left corner of the rectangle.
 * @width: Rectangle width (number of columns).
 * @height: Rectangle height (number of rows).
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
gwy_field_part_row_hhcf(GwyField *field,
                        const GwyMaskField *mask,
                        GwyMaskingType masking,
                        guint col, guint row,
                        guint width, guint height,
                        guint level)
{
    guint maskcol, maskrow;
    GwyLine *line = NULL;
    if (!_gwy_field_check_mask(field, mask, &masking,
                               col, row, width, height, &maskcol, &maskrow)
        || width < 2)
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
    guint nbuffers = (masking == GWY_MASK_IGNORE) ? 2 : 4;
    gdouble *buffer = fftw_malloc(nbuffers*size*sizeof(gdouble));
    gdouble *ffthc = buffer + size;
    gdouble *cached = buffer + size;    // used only with mask
    gdouble *weights = cached + size;    // used only with mask

    fftw_plan plan = fftw_plan_dft_r2c_1d(width, buffer, (fftw_complex*)ffthc,
                                          _GWY_FFTW_PATIENCE);
    if (masking != GWY_MASK_IGNORE)
        gwy_clear(weights, size);
    for (guint i = 0; i < height; i++) {
        const gdouble *d = base + i*field->xres;
        ASSIGN(buffer, d, width);
        if (!row_level_and_count(buffer, width,
                                 mask, masking, maskcol, maskrow + i,
                                 level))
            continue;
        if (masking == GWY_MASK_IGNORE) {
            // Calculate v_k before FFT destroys the input levelled/filtered
            // data. There is no need to keep it as an array, added the values
            // directly to the output line.
            gdouble sum = 0.0;
            const gdouble *q = buffer;
            const gdouble *qq = buffer + (width-1);
            gdouble *p = line->data + (width-1);
            for (guint j = 0; j < width; j++, q++, qq--, p--)
                *p = sum += (*q)*(*q) + (*qq)*(*qq);
        }
        else {
            // With masking, we will need the levelled/filtered data later.
            ASSIGN(cached, buffer, width);
        }

        row_acf(plan, buffer, ffthc, size, width);
        gdouble *p = line->data;
        const gdouble *q = ffthc;
        for (guint j = line->res; j; j--, p++, q++)
            *p -= 2*(*q);

        if (masking == GWY_MASK_IGNORE)
            continue;

        // If there is a mask the hairy part begins here.  First calculate U_ν,
        // save the result to @cached again.
        q = cached;
        p = buffer;
        for (guint j = line->res; j; j--, p++, q++)
            *p = (*q)*(*q);
        gwy_clear(buffer + width, size - width);
        fftw_execute(plan);
        ASSIGN(cached, buffer, size);
        // Then mask.  We need the intermediate result M_ν to combine it with
        // U_ν so don't use row_acf().
        GwyMaskIter iter;
        gwy_mask_field_iter_init(mask, iter, maskcol, maskrow + i);
        p = buffer;
        for (guint j = width; j; j--, p++) {
            *p = !gwy_mask_iter_get(iter) == invert;
            gwy_mask_iter_next(iter);
        }
        fftw_execute(plan);
        // Save V_ν/2 (calculated from M_ν and U_ν) to @cached and finish P_k
        // calculation.
        row_hc_cprod(cached, ffthc, buffer, size);
        ASSIGN(cached, buffer, size);
        row_hc_cnorm(ffthc, buffer, size);
        fftw_execute(plan);
        p = weights;
        q = ffthc;
        // Round to integers, this is most important for correct
        // preservation of zeroes as we need to treat g_k with no
        // contributions specially
        for (guint j = width; j; j--, p++, q++)
            *p = gwy_round(*q);
        // The only remaining step is to take V_ν/2 from @cached, transform
        // to v_k/2 and add v_k to the output line.
        ASSIGN(buffer, cached, size);
        fftw_execute(plan);
        p = line->data;
        q = ffthc;
        for (guint j = line->res; j; j--, p++, q++)
            *p += 2*(*q);

    }
    fftw_destroy_plan(plan);
    if (masking == GWY_MASK_IGNORE) {
        for (guint j = 0; j < line->res; j++)
            line->data[j] /= height*(line->res - j);
    }
    else {
        for (guint j = 0; j < line->res; j++) {
            if (weights[j])
                line->data[j] /= weights[j];
            else
                line->data[j] = 0.0;
        }
    }
    fftw_free(buffer);
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
 * Statistical distribution densities are normalized so that their integral,
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
