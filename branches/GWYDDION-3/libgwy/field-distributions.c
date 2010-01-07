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
#include "libgwy/libgwy-aliases.h"
#include "libgwy/processing-internal.h"

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
            GwyMaskFieldIter iter;
            gwy_mask_field_iter_init(mask, iter, maskcol, maskrow + i);
            for (guint j = width; j; j--, d++) {
                if (!gwy_mask_field_iter_get(iter) == invert) {
                    guint k = (guint)((*d - min)/q);
                    // Fix rounding errors.
                    if (G_UNLIKELY(k >= npoints))
                        line->data[npoints-1] += 1;
                    else
                        line->data[k] += 1;
                    ndata++;
                }
                gwy_mask_field_iter_next(iter);
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
        GwyMaskFieldIter iter;
        gwy_mask_field_iter_init(mask, iter, maskcol, maskrow + i);
        gboolean prev = !gwy_mask_field_iter_get(iter) == invert;
        for (guint j = width-1; j; j--, d++) {
            gboolean curr = !gwy_mask_field_iter_get(iter) == invert;
            if (prev && curr) {
                gdouble v = d[1] - d[0];
                if (v < min1)
                    min1 = v;
                if (v > max1)
                    max1 = v;
                ndata++;
            }
            gwy_mask_field_iter_next(iter);
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
        GwyMaskFieldIter iter1, iter2;
        gwy_mask_field_iter_init(mask, iter1, maskcol, maskrow + i);
        gwy_mask_field_iter_init(mask, iter2, maskcol, maskrow + i+1);
        for (guint j = width; j; j--, d1++, d2++) {
            if ((!gwy_mask_field_iter_get(iter1) == invert)
                && (!gwy_mask_field_iter_get(iter2) == invert)) {
                gdouble v = *d2 - *d1;
                if (v < min1)
                    min1 = v;
                if (v > max1)
                    max1 = v;
                ndata++;
            }
            gwy_mask_field_iter_next(iter1);
            gwy_mask_field_iter_next(iter2);
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
        GwyMaskFieldIter iter;
        gwy_mask_field_iter_init(mask, iter, maskcol, maskrow + i);
        gboolean prev = !gwy_mask_field_iter_get(iter) == invert;
        for (guint j = width-1; j; j--, d++) {
            gboolean curr = !gwy_mask_field_iter_get(iter) == invert;
            if (prev && curr) {
                gdouble v = d[1] - d[0];
                guint k = (guint)((v - min)/q);
                // Fix rounding errors.
                if (G_UNLIKELY(k >= npoints))
                    line->data[npoints-1] += 1;
                else
                    line->data[k] += 1;
            }
            gwy_mask_field_iter_next(iter);
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
        GwyMaskFieldIter iter1, iter2;
        gwy_mask_field_iter_init(mask, iter1, maskcol, maskrow + i);
        gwy_mask_field_iter_init(mask, iter2, maskcol, maskrow + i+1);
        for (guint j = width; j; j--, d1++, d2++) {
            if ((!gwy_mask_field_iter_get(iter1) == invert)
                && (!gwy_mask_field_iter_get(iter2) == invert)) {
                gdouble v = *d2 - *d1;
                guint k = (guint)((v - min)/q);
                // Fix rounding errors.
                if (G_UNLIKELY(k >= npoints))
                    line->data[npoints-1] += 1;
                else
                    line->data[k] += 1;
            }
            gwy_mask_field_iter_next(iter1);
            gwy_mask_field_iter_next(iter2);
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

/* Level a row of data by subtracting the mean value. */
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

/* Window a row using a sampled windowing function. */
static void
row_window(gdouble *data, const gdouble *window, guint n)
{
    for (guint i = n; i; i--, data++, window++)
        *data *= *window;
}

/* Calculate the sum of squares value of source data row @re. */
static gdouble
row_sum_squares(const gdouble *re, guint n)
{
    gdouble sum2 = 0.0;
    for (guint i = n; i; i--, re++)
        sum2 += *re * *re;
    return sum2;
}

/*
 * Calculate PSDF, normalizing the sum of squares to the previously calculated
 * value.  The 0th real element of PSDF is not excluded from the norm as
 * (a) it should be 0 anyway (b) we do not want the constant coefficient
 * to contribute.
 */
static void
row_psdf(const gdouble *ffthc,
         gdouble *psdf,
         guint n,
         gdouble sum_norm)
{
    gdouble s = 0.0;
    /*
     * The halfcomplex format is as follows:
     * r0, r1, r2, ..., r(n/2), i((n+1)/2-1), ..., i2, i1
     * More precisely, for even n = 2k:
     * r0, r1, r2, ..., rk, i(k-1), i(k-2), ..., i1
     * while for odd n = 2k+1:
     * r0, r1, r2, ..., rk, ik, i(k-1), ..., i1
     */
    psdf[0] = ffthc[0]*ffthc[0]/n;
    for (guint i = 1; i < (n + 1)/2; i++)
        s += psdf[i] = (ffthc[i]*ffthc[i] + ffthc[n-1]*ffthc[n-i])/n;
    if (n % 2 == 0)
        s += psdf[n/2] = (ffthc[n/2]*ffthc[n/2])/n;

    if (s == 0.0 || sum_norm == 0.0)
        return;

    // Normalize the sum of squares (total energy)
    gdouble q = sum_norm/s;
    for (guint i = 0; i <= n/2; i++)
        psdf[i] *= q;
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
 *
 * Calculates the row-wise power spectrum density function of a rectangular
 * part of a field.
 *
 * Returns: A new one-dimensional data line with the PSDF.
 **/
GwyLine*
gwy_field_part_row_psdf(GwyField *field,
                        const GwyMaskField *mask,
                        GwyMaskingType masking,
                        guint col, guint row,
                        guint width, guint height,
                        GwyWindowingType windowing)
{
    guint maskcol, maskrow;
    GwyLine *line = NULL;
    if (!_gwy_field_check_mask(field, mask, &masking,
                               col, row, width, height, &maskcol, &maskrow)
        || width < 2)
        goto fail;

    // An even size is necessary due to alignment constraints in FFTW.
    // Using this size for all buffers is a bit excessive but safe.
    line = gwy_line_new_sized((width + 1)/2, TRUE);
    gsize size = 2*line->res;
    const gdouble *base = field->data + row*field->xres + col;
    gdouble *buffer = fftw_malloc(3*size*sizeof(gdouble));
    gdouble *window = buffer + size;
    gdouble *ffthc = window + size;

    gwy_fft_window_sample(window, width, windowing);
    fftw_plan plan = fftw_plan_dft_r2c_1d(width, buffer, (fftw_complex*)ffthc,
                                          _GWY_FFTW_PATIENCE);
    for (guint i = 0; i < height; i++) {
        const gdouble *d = base + i*field->xres;
        ASSIGN(buffer, d, width);
        row_level(buffer, width);
        gdouble sum2 = row_sum_squares(buffer, width);
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

    gwy_line_multiply(line, gwy_field_dx(field)/(2*G_PI*height));
    gwy_line_set_real(line, G_PI/gwy_field_dx(field));

fail:
    if (!line)
        line = gwy_line_new();

    gwy_unit_power(gwy_line_get_unit_x(line), gwy_field_get_unit_xy(field), -1);
    gwy_unit_power_multiply(gwy_line_get_unit_y(line),
                            gwy_field_get_unit_xy(field), 1,
                            gwy_field_get_unit_z(field), 2);
    return line;
}

#define __LIBGWY_FIELD_DISTRIBUTIONS_C__
#include "libgwy/libgwy-aliases.c"

/**
 * SECTION: field-distributions
 * @title: GwyField distributions
 * @short_description: One-dimensional distributions and functionals of fields
 *
 * Statistical distribution densities are normalized so that their integral,
 * that can also be calculated as gwy_line_mean(line)*line->real, is unity.
 * Cumulative distribution values then always line in the interval [0,1].
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
