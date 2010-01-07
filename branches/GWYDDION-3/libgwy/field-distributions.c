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

#include "libgwy/macros.h"
#include "libgwy/math.h"
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

/* Level a row of data, keeping polynomial terms of degree equal to or higher
 * than @degree, which can be at most 0, 1, or 2.  This means @degree=2 is
 * linear levelling. */
static void
row_level(gdouble *data,
          guint n,
          guint degree)
{
    gdouble sumxi, sumxixi, sumsi, sumsixi, a, b;

    degree = MIN(degree, n);
    if (!degree)
        return;

    if (degree == 1) {
        gdouble sumsi = 0.0;
        gdouble *pdata = data;
        for (guint i = n; i; i--, pdata++)
            sumsi += *pdata;

        gdouble a = sumsi/n;
        pdata = data;
        for (guint i = n; i; i--, pdata++)
            *pdata -= a;

        return;
    }

    g_return_if_fail(degree == 2);

    /* These are already averages, not sums */
    gdouble sumxi = (n + 1.0)/2.0;
    gdouble sumxixi = (2.0*n + 1.0)*(n + 1.0)/6.0;
    gdouble sumsi = 0.0, sumsixi = 0.0;
    gdouble *pdata = data;
    for (guint i = n; i; i--, pdata++) {
        sumsi += *pdata;
        sumsixi += *pdata * i;
    }
    sumsi /= n;
    sumsixi /= n;

    gdouble b = (sumsixi - sumsi*sumxi)/(sumxixi - sumxi*sumxi);
    gdouble a = (sumsi*sumxixi - sumxi*sumsixi)/(sumxixi - sumxi*sumxi);

    pdata = data;
    sumsi = 0;
    for (guint i = n; i; i--, pdata++) {
        *pdata -= a + b*i;
        sumsi += *pdata;
    }
}

static void
row_window(gdouble *data, const gdouble *window, guint n)
{
    for (guint i = n; i; i--, data++, window++)
        *data *= *window;
}

/*
 * Calculate the RMS value of possibly complex source data rows @re, @im.
 *
 * @im can be %NULL if it was a R2C transform (the RMS is then calculated
 * from @re only).
 */
static gdouble
row_rms(const gdouble *re, const gdouble *im, guint n)
{
    /* Calculate original RMS */
    gdouble sum0 = 0.0, sum02 = 0.0;
    for (guint i = n; i; i--, re++) {
        sum0 += *re;
        sum02 += *re * *re;
    }
    gdouble a = sum02 - sum0*sum0/n;
    if (im) {
        sum0 = sum02 = 0.0;
        for (guint i = n; i; i--, re++) {
            sum0 += *im;
            sum02 += *im * *im;
        }
        a += sum02 - sum0*sum0/n;
    }
    return (a > 0.0) ? sqrt(a/n) : 0.0;
}

/* Make the RMS of Fourier coefficients @re, @im the same as the RMS
 * of source  @rms.
 *
 * The zeroth element of @{re,im} is ignored, assumed to be the constant
 * coefficient.
 */
static void
row_preserve_rms(gdouble *re, gdouble *im, guint n,
                 gdouble rms)
{
    // FIXME: Do something more smart such as clearing the data?
    if (rms == 0.0)
        return;

    /* Calculare new RMS ignoring 0th elements that correspond to constants */
    gdouble *p;
    gdouble sum2 = 0.0;
    for (guint i = n-1, p = re + 1; i; i--, p++)
        sum2 += *p * *p;
    for (guint i = n-1, p = im + 1; i; i--, p++)
        sum2 += *p * *p;
    if (sum2 == 0.0)
        return;

    /* Multiply output to get the same RMS */
    gdouble q = rms/sqrt(sum2/n);
    for (guint i = n, p = re; i; i--, p++)
        *p *= q;
    for (guint i = n, p = im; i; i--, p++)
        *p *= q;
}

static void
row_psdf(gdouble *buffer,
         gdouble *psdf,
         guint n)
{
    fftw_execute(plan);

    for (guint i = 0; i < n; i++) {
    }
    /* Complete the missing half of transform.  */
    for (k = 0; k < rin->yres; k++) {
        gdouble *re, *im;

        re = rout->data + k*rin->xres;
        im = iout->data + k*rin->xres;
        for (j = rin->xres/2 + 1; j < rin->xres; j++) {
            re[j] = re[rin->xres - j];
            im[j] = -im[rin->xres - j];
        }
    }

    gwy_data_field_multiply(rout, 1.0/sqrt(rin->xres));
    if (direction == GWY_TRANSFORM_DIRECTION_BACKWARD)
        gwy_data_field_multiply(iout, 1.0/sqrt(rin->xres));
    else
        gwy_data_field_multiply(iout, -1.0/sqrt(rin->xres));
}

/**
 * gwy_field_part_psdf:
 * @field: A data field.
 * @line: A data line to store the distribution to.  It will be
 *               resampled to requested width.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 * @interpolation: Interpolation to use when @nstats is given and requires
 *                 resampling (and possibly in FFT too).
 * @windowing: Windowing type to use.
 * @nstats: The number of samples to take on the distribution function.  If
 *          nonpositive, data field width (height) is used.
 *
 * Calculates one-dimensional power spectrum density function of a rectangular
 * part of a data field.
 **/
void
gwy_field_part_row_psdf(GwyDataField *field,
                        GwyDataLine *line,
                        gint col, gint row,
                        gint width, gint height,
                        GwyWindowingType windowing)
{
    GwyDataField *re_field, *im_field;
    GwySIUnit *xyunit, *zunit, *lineunit;
    gdouble *re, *im, *target;
    guint i, j, xres, yres;

    g_return_if_fail(GWY_IS_FIELD(field));
    g_return_if_fail(GWY_IS_LINE(line));
    xres = field->xres;
    yres = field->yres;
    g_return_if_fail(col >= 0 && row >= 0
                     && height >= 1 && width >= 4
                     && col + width <= xres
                     && row + height <= yres);

    //gwy_line_resample(line, width/2, GWY_INTERPOLATION_NONE);
    gwy_line_clear(line);
    gwy_line_set_offset(line, 0.0);

    target = line->data;

    // Use one size good for everything, fftw requires floor(n/2)+1 complex
    // elements in the output.  In addition we require an even size due to
    // alignment constraints.
    gsize size = (width + 3)/2*2;
    const gdouble *base = field->data + row*field->xres + col;
    gdouble *buffer = ffftw_malloc(3*size*sizeof(gdouble));
    gdouble *window = buffer + size;
    gdouble *psdf = window + size;
    // XXX: We perform the transform from @psdf to @buffer because then we
    // actually calculate the PSDF from @buffer and store it to @psdf
    fftw_plan plan = fftw_plan_dft_r2c_1d(width, psdf, (fftw_complex*)buffer,
                                          _GWY_FFTW_PATIENCE);

    gwy_fft_window_sample(window, width);
    for (guint i = 0; i < height; i++) {
        const gdouble *d = base + i*field->xres;
        ASSING(buffer, d, width);
        row_level(buffer, width, 1);
        gdouble rms = row_rms(buffer, width);
        row_window(buffer, window, width);
        row_psdf(buffer, psdf, plan, rms);
    }
    fftw_destroy_plan(plan);

    re = re_field->data;
    im = im_field->data;
    for (i = 0; i < height; i++) {
        for (j = 0; j < width/2; j++)
            target[j] += re[i*width + j]*re[i*width + j]
                + im[i*width + j]*im[i*width + j];
    }
    gwy_line_multiply(line,
                      field->xreal/xres/(2*G_PI*height));
    gwy_line_set_real(line, G_PI*xres/field->xreal);

    g_object_unref(re_field);
    g_object_unref(im_field);

    /* Set proper units */
    xyunit = gwy_field_get_si_unit_xy(field);
    zunit = gwy_field_get_si_unit_z(field);
    lineunit = gwy_line_get_si_unit_x(line);
    gwy_si_unit_power(xyunit, -1, lineunit);
    lineunit = gwy_line_get_si_unit_y(line);
    gwy_si_unit_power(zunit, 2, lineunit);
    gwy_si_unit_multiply(lineunit, xyunit, lineunit);
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
