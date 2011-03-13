/*
 *  $Id$
 *  Copyright (C) 2009-2010 David Nečas (Yeti).
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
#include "libgwy/macros.h"
#include "libgwy/math.h"
#include "libgwy/field-statistics.h"
#include "libgwy/math-internal.h"
#include "libgwy/field-internal.h"

#define CVAL GWY_FIELD_CVAL
#define CBIT GWY_FIELD_CBIT
#define CTEST GWY_FIELD_CTEST

typedef struct {
    gdouble s;
    gdouble q;
    gdouble dx2;
    gdouble dy2;
} SurfaceAreaData;

/**
 * gwy_field_min_max:
 * @field: A two-dimensional data field.
 * @fpart: (allow-none):
 *         Area in @field to process.  Pass %NULL to process entire @field.
 * @mask: (allow-none):
 *        Mask specifying which values to take into account/exclude, or %NULL.
 *        Its dimensions must match either the dimensions of @field or the
 *        rectangular part.  In the first case the mask is placed over the
 *        entire field, in the second case over the part.
 * @masking: Masking mode to use (has any effect only with non-%NULL @mask).
 * @min: (out) (allow-none):
 *       Location to store the minimum to, or %NULL.
 * @max: (out) (allow-none):
 *       Location to store the maximum to, or %NULL.
 *
 * Finds the minimum and maximum value of a field.
 *
 * The maximum value of no data is <constant>-HUGE_VAL</constant>, the
 * minimum is <constant>HUGE_VAL</constant>.
 *
 * The minimum and maximum of the entire field are cached, see
 * gwy_field_invalidate().
 **/
void
gwy_field_min_max(const GwyField *field,
                  const GwyFieldPart *fpart,
                  const GwyMaskField *mask,
                  GwyMaskingType masking,
                  gdouble *min,
                  gdouble *max)
{
    guint col, row, width, height, maskcol, maskrow;
    if (!_gwy_field_check_mask(field, fpart, mask, &masking,
                               &col, &row, &width, &height,
                               &maskcol, &maskrow)) {
        GWY_MAYBE_SET(min, HUGE_VAL);
        GWY_MAYBE_SET(max, -HUGE_VAL);
        return;
    }
    if (!min && !max)
        return;

    const gdouble *base = field->data + row*field->xres + col;
    gdouble min1 = G_MAXDOUBLE, max1 = -G_MAXDOUBLE;

    if (masking == GWY_MASK_IGNORE) {
        // No mask.  If full field is processed we must use the cache.
        gboolean full_field = (width == field->xres && height == field->yres);
        if (full_field && CTEST(field->priv, MIN) && CTEST(field->priv, MAX)) {
            GWY_MAYBE_SET(min, CVAL(field->priv, MIN));
            GWY_MAYBE_SET(max, CVAL(field->priv, MAX));
            return;
        }
        for (guint i = 0; i < height; i++) {
            const gdouble *d = base + i*field->xres;
            for (guint j = width; j; j--, d++) {
                if (min1 > *d)
                    min1 = *d;
                if (max1 < *d)
                    max1 = *d;
            }
        }
        if (full_field) {
            CVAL(field->priv, MIN) = min1;
            CVAL(field->priv, MAX) = max1;
            field->priv->cached |= CBIT(MIN) | CBIT(MAX);
        }
    }
    else {
        const gboolean invert = (masking == GWY_MASK_EXCLUDE);
        for (guint i = 0; i < height; i++) {
            const gdouble *d = base + i*field->xres;
            GwyMaskIter iter;
            gwy_mask_field_iter_init(mask, iter, maskcol, maskrow + i);
            for (guint j = width; j; j--, d++) {
                if (!gwy_mask_iter_get(iter) == invert) {
                    if (min1 > *d)
                        min1 = *d;
                    if (max1 < *d)
                        max1 = *d;
                }
                gwy_mask_iter_next(iter);
            }
        }
    }

    GWY_MAYBE_SET(min, min1);
    GWY_MAYBE_SET(max, max1);
    return;
}

/**
 * gwy_field_min_max_full:
 * @field: A two-dimensional data field.
 * @min: (out) (allow-none):
 *       Location to store the minimum to, or %NULL.
 * @max: (out) (allow-none):
 *       Location to store the maximum to, or %NULL.
 *
 * Finds the minimum and maximum value of an entire field.
 *
 * See gwy_field_min_max() for discussion.
 **/
void
gwy_field_min_max_full(const GwyField *field,
                       gdouble *min,
                       gdouble *max)
{
    gwy_field_min_max(field, NULL, NULL, GWY_MASK_IGNORE, min, max);
}

/**
 * gwy_field_mean:
 * @field: A two-dimensional data field.
 * @fpart: (allow-none):
 *         Area in @field to process.  Pass %NULL to process entire @field.
 * @mask: (allow-none):
 *        Mask specifying which values to take into account/exclude, or %NULL.
 *        Its dimensions must match either the dimensions of @field or the
 *        rectangular part.  In the first case the mask is placed over the
 *        entire field, in the second case just over the part.
 * @masking: Masking mode to use (has any effect only with non-%NULL @mask).
 *
 * Calculates the mean value of a field.
 *
 * The mean value of the entire field is cached, see gwy_field_invalidate().
 *
 * Returns: The mean value.  The mean value of no data is NaN.
 **/
gdouble
gwy_field_mean(const GwyField *field,
               const GwyFieldPart *fpart,
               const GwyMaskField *mask,
               GwyMaskingType masking)
{
    guint col, row, width, height, maskcol, maskrow;
    if (!_gwy_field_check_mask(field, fpart, mask, &masking,
                               &col, &row, &width, &height, &maskcol, &maskrow))
        return NAN;

    const gdouble *base = field->data + row*field->xres + col;
    gdouble mean = 0.0;
    gboolean full_field = FALSE;
    guint n = 0;

    if (masking == GWY_MASK_IGNORE) {
        // No mask.  If full field is processed we must use the cache.
        full_field = (width == field->xres && height == field->yres);
        if (full_field && CTEST(field->priv, AVG))
            return CVAL(field->priv, AVG);
        for (guint i = 0; i < height; i++) {
            const gdouble *d = base + i*field->xres;
            for (guint j = width; j; j--, d++)
                mean += *d;
        }
        n = width*height;
    }
    else {
        // Masking is in use.
        const gboolean invert = (masking == GWY_MASK_EXCLUDE);
        for (guint i = 0; i < height; i++) {
            const gdouble *d = base + i*field->xres;
            GwyMaskIter iter;
            gwy_mask_field_iter_init(mask, iter, maskcol, maskrow + i);
            for (guint j = width; j; j--, d++) {
                if (!gwy_mask_iter_get(iter) == invert) {
                    mean += *d;
                    n++;
                }
                gwy_mask_iter_next(iter);
            }
        }
    }
    if (!n)
        return NAN;
    mean = mean/n;
    if (full_field) {
        CVAL(field->priv, AVG) = mean;
        field->priv->cached |= CBIT(AVG);
    }
    return mean;
}

/**
 * gwy_field_mean_full:
 * @field: A two-dimensional data field.
 *
 * Calculates the mean value of an entire field.
 *
 * See gwy_field_mean() for discussion.
 *
 * Returns: The mean value.
 **/
gdouble
gwy_field_mean_full(const GwyField *field)
{
    return gwy_field_mean(field, NULL, NULL, GWY_MASK_IGNORE);
}

/**
 * gwy_field_median:
 * @field: A two-dimensional data field.
 * @fpart: (allow-none):
 *         Area in @field to process.  Pass %NULL to process entire @field.
 * @mask: (allow-none):
 *        Mask specifying which values to take into account/exclude, or %NULL.
 *        Its dimensions must match either the dimensions of @field or the
 *        rectangular part.  In the first case the mask is placed over the
 *        entire field, in the second case over the part.
 * @masking: Masking mode to use (has any effect only with non-%NULL @mask).
 *
 * Calculates the median value of a field.
 *
 * The median value of the entire field is cached, see gwy_field_invalidate().
 *
 * Returns: The median value.  The median value of no data is NaN.
 **/
gdouble
gwy_field_median(const GwyField *field,
                 const GwyFieldPart *fpart,
                 const GwyMaskField *mask,
                 GwyMaskingType masking)
{
    guint col, row, width, height, maskcol, maskrow;
    if (!_gwy_field_check_mask(field, fpart, mask, &masking,
                               &col, &row, &width, &height, &maskcol, &maskrow))
        return NAN;

    const gdouble *base = field->data + row*field->xres + col;
    gsize bufsize = width*height*sizeof(gdouble);
    gdouble *buffer = g_slice_alloc(bufsize);

    if (masking == GWY_MASK_IGNORE) {
        // No mask.  If full field is processed we must use the cache.
        gboolean full_field = (width == field->xres && height == field->yres);
        if (full_field && CTEST(field->priv, MED))
            return CVAL(field->priv, MED);
        for (guint i = 0; i < height; i++)
            gwy_assign(buffer + i*width, base + i*field->xres, width);
        gdouble median = gwy_math_median(buffer, width*height);
        g_slice_free1(bufsize, buffer);
        if (full_field) {
            CVAL(field->priv, MED) = median;
            field->priv->cached |= CBIT(MED);
        }
        return median;
    }

    // Masking is in use.
    gdouble *p = buffer;
    const gboolean invert = (masking == GWY_MASK_EXCLUDE);
    for (guint i = 0; i < height; i++) {
        const gdouble *d = base + i*field->xres;
        GwyMaskIter iter;
        gwy_mask_field_iter_init(mask, iter, maskcol, maskrow + i);
        for (guint j = width; j; j--, d++) {
            if (!gwy_mask_iter_get(iter) == invert)
                *(p++) = *d;
            gwy_mask_iter_next(iter);
        }
    }

    gdouble median = (p != buffer) ? gwy_math_median(buffer, p - buffer) : NAN;
    g_slice_free1(bufsize, buffer);
    return median;
}

/**
 * gwy_field_median_full:
 * @field: A two-dimensional data field.
 *
 * Calculates the median value of an entire field.
 *
 * See gwy_field_median() for discussion.
 *
 * Returns: The median value.
 **/
gdouble
gwy_field_median_full(const GwyField *field)
{
    return gwy_field_median(field, NULL, NULL, GWY_MASK_IGNORE);
}

/**
 * gwy_field_rms:
 * @field: A two-dimensional data field.
 * @fpart: (allow-none):
 *         Area in @field to process.  Pass %NULL to process entire @field.
 * @mask: (allow-none):
 *        Mask specifying which values to take into account/exclude, or %NULL.
 *        Its dimensions must match either the dimensions of @field or the
 *        rectangular part.  In the first case the mask is placed over the
 *        entire field, in the second case over the part.
 * @masking: Masking mode to use (has any effect only with non-%NULL @mask).
 *
 * Calculates the root mean square of a field.
 *
 * This function sums the squares of differences from values from the mean
 * values.  If you need a plain sum of squares see gwy_field_meansq().
 *
 * The rms value of the entire field is cached, see gwy_field_invalidate().
 *
 * Returns: The root mean square of differences from the mean value.  The rms
 *          of no data is zero.
 **/
gdouble
gwy_field_rms(const GwyField *field,
              const GwyFieldPart *fpart,
              const GwyMaskField *mask,
              GwyMaskingType masking)
{
    guint col, row, width, height, maskcol, maskrow;
    if (!_gwy_field_check_mask(field, fpart, mask, &masking,
                               &col, &row, &width, &height, &maskcol, &maskrow))
        return 0.0;

    const gdouble *base = field->data + row*field->xres + col;
    gdouble avg = gwy_field_mean(field, fpart, mask, masking);
    gdouble rms = 0.0;
    gboolean full_field = FALSE;
    guint n = 0;

    if (isnan(avg))
        return 0.0;

    if (masking == GWY_MASK_IGNORE) {
        // No mask.  If full field is processed we must use the cache.
        full_field = (width == field->xres && height == field->yres);
        if (full_field && CTEST(field->priv, RMS))
            return CVAL(field->priv, RMS);
        for (guint i = 0; i < height; i++) {
            const gdouble *d = base + i*field->xres;
            for (guint j = width; j; j--, d++) {
                gdouble v = *d - avg;
                rms += v*v;
            }
        }
        n = width*height;
    }
    else {
        // Masking is in use.
        const gboolean invert = (masking == GWY_MASK_EXCLUDE);
        for (guint i = 0; i < height; i++) {
            const gdouble *d = base + i*field->xres;
            GwyMaskIter iter;
            gwy_mask_field_iter_init(mask, iter, maskcol, maskrow + i);
            for (guint j = width; j; j--, d++) {
                if (!gwy_mask_iter_get(iter) == invert) {
                    gdouble v = *d - avg;
                    rms += v*v;
                    n++;
                }
                gwy_mask_iter_next(iter);
            }
        }
    }

    rms = sqrt(rms/n);
    if (full_field) {
        CVAL(field->priv, RMS) = rms;
        field->priv->cached |= CBIT(RMS);
    }
    return rms;
}

/**
 * gwy_field_rms_full:
 * @field: A two-dimensional data field.
 *
 * Calculates the root mean square of an entire field.
 *
 * See gwy_field_rms() for discussion.
 *
 * Returns: The rms value.
 **/
gdouble
gwy_field_rms_full(const GwyField *field)
{
    return gwy_field_rms(field, NULL, NULL, GWY_MASK_IGNORE);
}

/**
 * gwy_field_meansq:
 * @field: A two-dimensional data field.
 * @fpart: (allow-none):
 *         Area in @field to process.  Pass %NULL to process entire @field.
 * @mask: (allow-none):
 *        Mask specifying which values to take into account/exclude, or %NULL.
 *        Its dimensions must match either the dimensions of @field or the
 *        rectangular part.  In the first case the mask is placed over the
 *        entire field, in the second case over the part.
 * @masking: Masking mode to use (has any effect only with non-%NULL @mask).
 *
 * Calculates the mean square of a field.
 *
 * Unlike rms, calculated with gwy_field_rms(), this function does not subtract
 * the mean value.  It calculates squares of the data as-is.
 *
 * The mean square value of the entire field is cached, see
 * gwy_field_invalidate().
 *
 * Returns: The mean square of data values.  The mean square of no data is
 *          zero.
 **/
gdouble
gwy_field_meansq(const GwyField *field,
                 const GwyFieldPart *fpart,
                 const GwyMaskField *mask,
                 GwyMaskingType masking)
{
    guint col, row, width, height, maskcol, maskrow;
    if (!_gwy_field_check_mask(field, fpart, mask, &masking,
                               &col, &row, &width, &height, &maskcol, &maskrow))
        return 0.0;

    const gdouble *base = field->data + row*field->xres + col;
    gdouble meansq = 0.0;
    gboolean full_field = FALSE;
    guint n = 0;

    if (masking == GWY_MASK_IGNORE) {
        // No mask.  If full field is processed we must use the cache.
        full_field = (width == field->xres && height == field->yres);
        if (full_field && CTEST(field->priv, RMS))
            return CVAL(field->priv, RMS);
        for (guint i = 0; i < height; i++) {
            const gdouble *d = base + i*field->xres;
            for (guint j = width; j; j--, d++)
                meansq += (*d)*(*d);
        }
        n = width*height;
    }
    else {
        // Masking is in use.
        const gboolean invert = (masking == GWY_MASK_EXCLUDE);
        for (guint i = 0; i < height; i++) {
            const gdouble *d = base + i*field->xres;
            GwyMaskIter iter;
            gwy_mask_field_iter_init(mask, iter, maskcol, maskrow + i);
            for (guint j = width; j; j--, d++) {
                if (!gwy_mask_iter_get(iter) == invert) {
                    meansq += (*d)*(*d);
                    n++;
                }
                gwy_mask_iter_next(iter);
            }
        }
    }

    meansq = n ? meansq/n : 0.0;
    if (full_field) {
        CVAL(field->priv, MSQ) = meansq;
        field->priv->cached |= CBIT(MSQ);
    }
    return meansq;
}

/**
 * gwy_field_statistics:
 * @field: A two-dimensional data field.
 * @fpart: (allow-none):
 *         Area in @field to process.  Pass %NULL to process entire @field.
 * @mask: (allow-none):
 *        Mask specifying which values to take into account/exclude, or %NULL.
 *        Its dimensions must match either the dimensions of @field or the
 *        rectangular part.  In the first case the mask is placed over the
 *        entire field, in the second case over the part.
 * @masking: Masking mode to use (has any effect only with non-%NULL @mask).
 * @mean: Location to store the mean value (Ra) to, or %NULL.
 * @ra: (out) (allow-none):
 *      Location to store the mean difference from the mean value (Ra) to,
 *      or %NULL.
 * @rms: (out) (allow-none):
 *       Location to store the root mean square of difference from the mean
 *       value (Rq) to, or %NULL.
 * @skew: (out) (allow-none):
 *        Location to store the skew (symmetry of value distribution) to,
 *        or %NULL.
 * @kurtosis: (out) (allow-none):
 *            Location to store the kurtosis (peakedness of value ditribution)
 *            to, or %NULL.
 *
 * Calculates numerical statistical characteristics of a field.
 *
 * The Ra value of no data is zero.  The skew and kurtosis of no data is NaN,
 * they are NaN also for flat data.
 **/
void
gwy_field_statistics(const GwyField *field,
                     const GwyFieldPart *fpart,
                     const GwyMaskField *mask,
                     GwyMaskingType masking,
                     gdouble *mean, gdouble *ra, gdouble *rms,
                     gdouble *skew, gdouble *kurtosis)
{
    guint col, row, width, height, maskcol, maskrow;
    if (!_gwy_field_check_mask(field, fpart, mask, &masking,
                               &col, &row, &width, &height, &maskcol, &maskrow))
        goto fail;
    if (!mean && !ra && !rms && !skew && !kurtosis)
        return;

    const gdouble *base = field->data + row*field->xres + col;
    gdouble avg = gwy_field_mean(field, fpart, mask, masking);
    gdouble sumabs = 0.0, sum2 = 0.0, sum3 = 0.0, sum4 = 0.0;
    gboolean full_field = FALSE;
    guint n = 0;

    if (masking == GWY_MASK_IGNORE) {
        // No mask.  If full field is processed we must use the cache.
        full_field = (width == field->xres && height == field->yres);
        for (guint i = 0; i < height; i++) {
            const gdouble *d = base + i*field->xres;
            for (guint j = width; j; j--, d++) {
                gdouble v = *d - avg;
                sumabs += fabs(v);
                v *= v;
                sum2 += v;
                v *= v;
                sum3 += v;
                v *= v;
                sum4 += v;
            }
        }
        n = width*height;
    }
    else {
        // Masking is in use.
        if (isnan(avg))
            goto fail;

        const gboolean invert = (masking == GWY_MASK_EXCLUDE);
        for (guint i = 0; i < height; i++) {
            const gdouble *d = base + i*field->xres;
            GwyMaskIter iter;
            gwy_mask_field_iter_init(mask, iter, maskcol, maskrow + i);
            for (guint j = width; j; j--, d++) {
                if (!gwy_mask_iter_get(iter) == invert) {
                    gdouble v = *d - avg;
                    sumabs += fabs(v);
                    v *= v;
                    sum2 += v;
                    v *= v;
                    sum3 += v;
                    v *= v;
                    sum4 += v;
                    n++;
                }
                gwy_mask_iter_next(iter);
            }
        }
    }
    g_assert(n);
    sumabs /= n;
    sum2 /= n;
    sum3 /= n;
    sum4 /= n;
    gdouble rms1 = sqrt(sum2);
    sum3 /= sum2*rms1;
    sum4 = sum4/(sum2*sum2) - 3;
    if (full_field) {
        CVAL(field->priv, RMS) = rms1;
        CVAL(field->priv, MSQ) = sum2 + avg*avg;
        field->priv->cached |= CBIT(RMS) | CBIT(MSQ);
    }

    GWY_MAYBE_SET(mean, avg);
    GWY_MAYBE_SET(ra, sumabs);
    GWY_MAYBE_SET(rms, rms1);
    GWY_MAYBE_SET(skew, sum3);
    GWY_MAYBE_SET(kurtosis, sum4);
    return;

fail:
    GWY_MAYBE_SET(mean, NAN);
    GWY_MAYBE_SET(ra, 0.0);
    GWY_MAYBE_SET(rms, 0.0);
    GWY_MAYBE_SET(skew, NAN);
    GWY_MAYBE_SET(kurtosis, NAN);
}

/**
 * gwy_field_count_above_below:
 * @field: A two-dimensional data field.
 * @fpart: (allow-none):
 *         Area in @field to process.  Pass %NULL to process entire @field.
 * @mask: (allow-none):
 *        Mask specifying which values to take into account/exclude, or %NULL.
 * @masking: Masking mode to use (has any effect only with non-%NULL @mask).
 * @above: Lower bound to compare the field values to.
 * @below: Upper bound to compare the field values to.
 * @strict: %TRUE to use strict inequalities and consequently count values in
 *          open intervals, %FALSE to use non-strict inequalities and count
 *          values in closed intervals.
 * @nabove: (out) (allow-none):
 *          Location to store the number of values greater than (or equal to)
 *          @above, or %NULL.
 * @nbelow: (out) (allow-none):
 *          Location to store the number of values less than (or equal to)
 *          @below, or %NULL.
 *
 * Counts the values in a field above and/or below specified bounds.
 *
 * The counts stored in @nabove and @nbelow are completely independent.
 *
 * Counting values <emphasis>outside</emphasis> the closed interval [@a,@b] is
 * thus straightfoward:
 * |[
 * guint nabove, nbelow, count;
 * gwy_data_field_count_in_range(field, fpart, mask, masking,
 *                               b, a, TRUE, &nabove, &nbelow);
 * count = nabove + nbelow;
 * ]|
 *
 * To count values <emphasis>inside</emphasis> the closed interval [@a,@b]
 * complement the intervals and use the return value (note @strict = %TRUE as
 * the complements are open intervals):
 * |[
 * guint ntotal, nabove, nbelow, count;
 * ntotal = gwy_data_field_count_in_range(field, fpart, mask, masking,
 *                                        b, a, TRUE, &nabove, &nbelow);
 * count = ntotal - (nabove + nbelow);
 * ]|
 *
 * Similarly, counting values <emphasis>outside</emphasis> the open interval
 * (@a,@b) is straightfoward:
 * |[
 * guint nabove, nbelow, count;
 * gwy_data_field_count_in_range(field, fpart, mask, masking,
 *                               b, a, FALSE, &nabove, &nbelow);
 * count = nabove + nbelow;
 * ]|
 *
 * Whereas counting values <emphasis>inside</emphasis> the open interval
 * (@a,@b) uses complements:
 * |[
 * guint ntotal, nabove, nbelow, count;
 * ntotal = gwy_data_field_count_in_range(field, fpart, mask, masking,
 *                                        b, a, TRUE, &nabove, &nbelow);
 * count = ntotal - (nabove + nbelow);
 * ]|
 *
 * It is also possible obtain the same counts by passing @above = @a and
 * @below = @b and utilising the fact that the values within the interval are
 * counted twice.  This is left as an excercise to the reader.
 *
 * Returns: The total number of values considered.  This is namely useful with
 *          masking, otherwise the returned value always equals to the number
 *          of pixels in @fpart (or the entire field).
 **/
guint
gwy_field_count_above_below(const GwyField *field,
                            const GwyFieldPart *fpart,
                            const GwyMaskField *mask,
                            GwyMaskingType masking,
                            gdouble above, gdouble below,
                            gboolean strict,
                            guint *nabove, guint *nbelow)
{
    guint col, row, width, height, maskcol, maskrow;
    if (!_gwy_field_check_mask(field, fpart, mask, &masking,
                               &col, &row, &width, &height,
                               &maskcol, &maskrow)) {
        GWY_MAYBE_SET(nabove, 0);
        GWY_MAYBE_SET(nbelow, 0);
        return 0;
    }

    if (!nabove && !nbelow) {
        // Silly
        if (mask->xres != field->xres
            || mask->yres != field->yres)
            return gwy_mask_field_count(mask, NULL, TRUE);
        else
            return gwy_mask_field_part_count(mask, fpart, TRUE);
    }

    const gdouble *base = field->data + row*field->xres + col;
    guint na = 0, nb = 0, n = 0;

    if (masking == GWY_MASK_IGNORE) {
        // No mask.
        for (guint i = 0; i < height; i++) {
            const gdouble *d = base + i*field->xres;
            if (strict) {
                for (guint j = width; j; j--, d++) {
                    if (*d > above)
                        na++;
                    if (*d < below)
                        nb++;
                }
            }
            else {
                for (guint j = width; j; j--, d++) {
                    if (*d >= above)
                        na++;
                    if (*d <= below)
                        nb++;
                }
            }
        }
        n = width*height;
    }
    else {
        // Masking is in use.
        gboolean invert = (masking == GWY_MASK_EXCLUDE);
        for (guint i = 0; i < height; i++) {
            const gdouble *d = base + i*field->xres;
            GwyMaskIter iter;
            gwy_mask_field_iter_init(mask, iter, maskcol, maskrow + i);
            if (strict) {
                for (guint j = width; j; j--, d++) {
                    if (!gwy_mask_iter_get(iter) == invert) {
                        if (*d > above)
                            na++;
                        if (*d < below)
                            nb++;
                        n++;
                    }
                    gwy_mask_iter_next(iter);
                }
            }
            else {
                for (guint j = width; j; j--, d++) {
                    if (!gwy_mask_iter_get(iter) == invert) {
                        if (*d >= above)
                            na++;
                        if (*d <= below)
                            nb++;
                        n++;
                    }
                    gwy_mask_iter_next(iter);
                }
            }
        }
    }

    GWY_MAYBE_SET(nabove, na);
    GWY_MAYBE_SET(nbelow, nb);
    return n;
}

/**
 * surface_area_nonsquare:
 * @z1: Z-value in first corner.
 * @z2: Z-value in second corner.
 * @z3: Z-value in third corner.
 * @z4: Z-value in fourth corner.
 * @w1: Weight of first corner (0 or 1).
 * @w2: Weight of second corner (0 or 1).
 * @w3: Weight of third corner (0 or 1).
 * @w4: Weight of fourth corner (0 or 1).
 * @user_data: #SurfaceAreaData.
 *
 * Calculates approximate area of a one square pixel with some corners possibly
 * missing.
 **/
static void
surface_area_square(gdouble z1, gdouble z2, gdouble z3, gdouble z4,
                    guint w1, guint w2, guint w3, guint w4,
                    gpointer user_data)
{
    SurfaceAreaData *sadata = (SurfaceAreaData*)user_data;
    gdouble s, c = (z1 + z2 + z3 + z4)/4.0;

    z1 -= c;
    z2 -= c;
    z3 -= c;
    z4 -= c;

    s = ((w1 + w2)*sqrt(1.0 + 2.0*(z1*z1 + z2*z2)/sadata->q)
         + (w2 + w3)*sqrt(1.0 + 2.0*(z2*z2 + z3*z3)/sadata->q)
         + (w3 + w4)*sqrt(1.0 + 2.0*(z3*z3 + z4*z4)/sadata->q)
         + (w4 + w1)*sqrt(1.0 + 2.0*(z4*z4 + z1*z1)/sadata->q))/2.0;
    sadata->s += s;
}

/**
 * surface_area_nonsquare:
 * @z1: Z-value in first corner.
 * @z2: Z-value in second corner.
 * @z3: Z-value in third corner.
 * @z4: Z-value in fourth corner.
 * @w1: Weight of first corner (0 or 1).
 * @w2: Weight of second corner (0 or 1).
 * @w3: Weight of third corner (0 or 1).
 * @w4: Weight of fourth corner (0 or 1).
 * @user_data: #SurfaceAreaData.
 *
 * Calculates approximate area of a one general rectangular pixel with some
 * corners possibly missing.
 **/
static void
surface_area_nonsquare(gdouble z1, gdouble z2, gdouble z3, gdouble z4,
                       guint w1, guint w2, guint w3, guint w4,
                       gpointer user_data)
{
    SurfaceAreaData *sadata = (SurfaceAreaData*)user_data;
    gdouble s, c = (z1 + z2 + z3 + z4)/2.0;

    s = ((w1 + w2)*sqrt(1.0 + (z1 - z2)*(z1 - z2)/sadata->dx2
                        + (z1 + z2 - c)*(z1 + z2 - c)/sadata->dy2)
         + (w2 + w3)*sqrt(1.0 + (z2 - z3)*(z2 - z3)/sadata->dy2
                          + (z2 + z3 - c)*(z2 + z3 - c)/sadata->dx2)
         + (w3 + w4)*sqrt(1.0 + (z3 - z4)*(z3 - z4)/sadata->dx2
                          + (z3 + z4 - c)*(z3 + z4 - c)/sadata->dy2)
         + (w4 + w1)*sqrt(1.0 + (z1 - z4)*(z1 - z4)/sadata->dy2
                          + (z1 + z4 - c)*(z1 + z4 - c)/sadata->dx2))/2.0;
    sadata->s += s;
}

/**
 * gwy_field_surface_area:
 * @field: A two-dimensional data field.
 * @fpart: (allow-none):
 *         Area in @field to process.  Pass %NULL to process entire @field.
 * @mask: (allow-none):
 *        Mask specifying which values to take into account/exclude, or %NULL.
 * @masking: Masking mode to use (has any effect only with non-%NULL @mask).
 *
 * Calculates the surface area of a field.
 *
 * The surface area value of the entire field is cached,
 * see gwy_field_invalidate().
 *
 * Returns: The surface area.  The surface area value is meaningless if lateral
 *          and value (height) are different physical quantities.
 **/
gdouble
gwy_field_surface_area(const GwyField *field,
                       const GwyFieldPart *fpart,
                       const GwyMaskField *mask,
                       GwyMaskingType masking)
{
    g_return_val_if_fail(GWY_IS_FIELD(field), 0.0);

    gdouble dx = gwy_field_dx(field);
    gdouble dy = gwy_field_dy(field);
    gboolean square_pixels = fabs(log(dx/dy)) < COMPAT_EPSILON;
    gboolean full_field = ((!mask || masking == GWY_MASK_IGNORE)
                           && (!fpart || (fpart->width == field->xres
                                          && fpart->height == field->yres
                                          && fpart->col == 0
                                          && fpart->row == 0)));
    SurfaceAreaData sadata = { 0.0, dx*dy, dx*dx, dy*dy };
    if (square_pixels)
        gwy_field_process_quarters(field, fpart, mask, masking, TRUE,
                                   surface_area_square, &sadata);
    else
        gwy_field_process_quarters(field, fpart, mask, masking, TRUE,
                                   surface_area_nonsquare, &sadata);

    gdouble area = sadata.s*sadata.q/4.0;
    if (full_field) {
        CVAL(field->priv, ARE) = area;
        field->priv->cached |= CBIT(ARE);
    }
    return area;
}

static void
process_quarters(const GwyField *field,
                 guint col, guint row,
                 guint width, guint height,
                 gboolean include_borders,
                 GwyFieldQuartersFunc function,
                 gpointer user_data)
{
    guint xres = field->xres;
    guint yres = field->yres;
    const gdouble *base = field->data + xres*row + col;

    const guint F = (col == 0) ? 1 : 0;
    const guint L = (col + width == xres) ? 0 : 1;
    const gdouble *d1, *d2;

    // Top row.
    if (include_borders || row > 0) {
        d1 = (row == 0) ? base-1 : base-1 - xres;
        d2 = base-1;
        function(d1[F], d1[1], d2[1], d2[F], 0, 0, 1, 0, user_data);
        d1++, d2++;
        for (guint j = width-1; j; j--, d1++, d2++)
            function(d1[0], d1[1], d2[1], d2[0], 0, 0, 1, 1, user_data);
        function(d1[0], d1[L], d2[L], d2[0], 0, 0, 0, 1, user_data);
    }

    // Middle part
    for (guint i = 0; i+1 < height; i++) {
        d1 = base-1 + i*xres;
        d2 = d1 + xres;
        if (include_borders || col > 0)
            function(d1[F], d1[1], d2[1], d2[F], 0, 1, 1, 0, user_data);
        d1++, d2++;
        for (guint j = width-1; j; j--, d1++, d2++)
            function(d1[0], d1[1], d2[1], d2[0], 1, 1, 1, 1, user_data);
        if (include_borders || col + width < xres)
            function(d1[0], d1[L], d2[L], d2[0], 1, 0, 0, 1, user_data);
    }

    // Bottom row.
    if (include_borders || row + height < yres) {
        d1 = base-1 + (height - 1)*xres;
        d2 = (row + height == yres) ? d1 : d1 + xres;
        function(d1[F], d1[1], d2[1], d2[F], 0, 1, 0, 0, user_data);
        d1++, d2++;
        for (guint j = width-1; j; j--, d1++, d2++)
            function(d1[0], d1[1], d2[1], d2[0], 1, 1, 0, 0, user_data);
        function(d1[0], d1[L], d2[L], d2[0], 1, 0, 0, 0, user_data);
    }
}

static void
process_quarters_masked(const GwyField *field,
                        guint col, guint row,
                        guint width, guint height,
                        const GwyMaskField *mask,
                        GwyMaskingType masking,
                        guint maskcol, guint maskrow,
                        gboolean include_borders,
                        GwyFieldQuartersFunc function,
                        gpointer user_data)
{
    guint xres = field->xres;
    guint yres = field->yres;
    const gdouble *base = field->data + xres*row + col;

    const guint F = (col == 0) ? 1 : 0;
    const guint L = (col + width == xres) ? 0 : 1;
    const gboolean invert = (masking == GWY_MASK_EXCLUDE);
    GwyMaskIter iter1, iter2;
    const gdouble *d1, *d2;
    guint w1, w2, w3, w4;

    // Top row.
    if (include_borders || row > 0) {
        d1 = (row == 0) ? base-1 : base-1 - xres;
        d2 = base-1;
        gwy_mask_field_iter_init(mask, iter2, maskcol, maskrow);
        w3 = !gwy_mask_iter_get(iter2) == invert;
        if (w3)
            function(d1[F], d1[1], d2[1], d2[F], 0, 0, w3, 0, user_data);
        d1++, d2++;
        for (guint j = width-1; j; j--, d1++, d2++) {
            w4 = w3;
            w3 = !gwy_mask_iter_get(iter2) == invert;
            if (w3 || w4)
                function(d1[0], d1[1], d2[1], d2[0], 0, 0, w3, w4, user_data);
            gwy_mask_iter_next(iter2);
        }
        w4 = w3;
        if (w4)
            function(d1[0], d1[L], d2[L], d2[0], 0, 0, 0, w4, user_data);
    }

    // Middle part
    for (guint i = 0; i+1 < height; i++) {
        d1 = base-1 + i*xres;
        d2 = d1 + xres;
        gwy_mask_field_iter_init(mask, iter1, maskcol, maskrow + i+1);
        w2 = !gwy_mask_iter_get(iter1) == invert;
        gwy_mask_field_iter_init(mask, iter2, maskcol, maskrow + i+1);
        w3 = !gwy_mask_iter_get(iter2) == invert;
        if ((include_borders || col > 0) && (w2 || w3))
            function(d1[F], d1[1], d2[1], d2[F], 0, w2, w3, 0, user_data);
        d1++, d2++;
        for (guint j = width-1; j; j--, d1++, d2++) {
            w1 = w2;
            w2 = !gwy_mask_iter_get(iter1) == invert;
            w4 = w3;
            w3 = !gwy_mask_iter_get(iter2) == invert;
            if (w1 || w2 || w3 || w4)
                function(d1[0], d1[1], d2[1], d2[0], w1, w2, w3, w4, user_data);
            gwy_mask_iter_next(iter1);
            gwy_mask_iter_next(iter2);
        }
        w1 = w2;
        w4 = w3;
        if ((include_borders || col + width < xres) && (w1 || w4))
            function(d1[0], d1[L], d2[L], d2[0], w1, 0, 0, w4, user_data);
    }

    // Bottom row.
    if (include_borders || row + height < yres) {
        d1 = base-1 + (height - 1)*xres;
        d2 = (row + height == yres) ? d1 : d1 + xres;
        gwy_mask_field_iter_init(mask, iter1, maskcol, maskrow + height-1);
        w2 = !gwy_mask_iter_get(iter1) == invert;
        if (w2)
            function(d1[F], d1[1], d2[1], d2[F], 0, w2, 0, 0, user_data);
        d1++, d2++;
        for (guint j = width-1; j; j--, d1++, d2++) {
            w1 = w2;
            w2 = !gwy_mask_iter_get(iter1) == invert;
            if (w1 || w2)
                function(d1[0], d1[1], d2[1], d2[0], w1, w2, 0, 0, user_data);
            gwy_mask_iter_next(iter1);
        }
        w1 = w2;
        if (w1)
            function(d1[0], d1[L], d2[L], d2[0], w1, 0, 0, 0, user_data);
    }
}

/**
 * gwy_field_process_quarters:
 * @field: A two-dimensional data field.
 * @fpart: (allow-none):
 *         Area in @field to process.  Pass %NULL to process entire @field.
 * @mask: (allow-none):
 *        Mask specifying which values to take into account/exclude, or %NULL.
 * @masking: Masking mode to use (has any effect only with non-%NULL @mask).
 * @include_borders: Pass %TRUE to include the field (not area) half-pixel
 *                   borders in the processing, %FALSE to exclude them.
 * @function: Function to apply to each set of four neighbour pixels.
 * @user_data: User data passed to @function.
 *
 * Processes a field by quarter-pixels.
 *
 * Certain quantities, for instance volume, surface area, or contiguous value
 * and slope distributions, are defined using the squares formed by four
 * neighbour pixels.  However, masks lie on pixels, not between them.
 * Therefore, the smallest intersections of the mask with these squares and
 * thus the smallest elements to be processed are quarter-pixels.
 *
 * Thus function simplifies such processing by calling @function for all sets
 * of four neighbour pixels and with weights corresponding to the masking mode
 * used.  More precisely, @function is called only if at least one of the four
 * pixels is included in the calculation according to the masking mode.
 *
 * The half-pixel stripes at the field boundaries are correctly handled using
 * mirroring for the outside values if @include_borders is TRUE.  The exterior
 * area is not included as such but may still enter the formulae and thus
 * influence the result.
 **/
void
gwy_field_process_quarters(const GwyField *field,
                           const GwyFieldPart *fpart,
                           const GwyMaskField *mask,
                           GwyMaskingType masking,
                           gboolean include_borders,
                           GwyFieldQuartersFunc function,
                           gpointer user_data)
{
    g_return_if_fail(function);

    guint col, row, width, height, maskcol, maskrow;
    if (!_gwy_field_check_mask(field, fpart, mask, &masking,
                               &col, &row, &width, &height, &maskcol, &maskrow))
        return;

    if (masking != GWY_MASK_IGNORE)
        process_quarters_masked(field, col, row, width, height,
                                mask, masking, maskcol, maskrow,
                                include_borders, function, user_data);
    else
        process_quarters(field, col, row, width, height,
                         include_borders, function, user_data);
}

/**
 * SECTION: field-statistics
 * @section_id: GwyField-statistics
 * @title: GwyField statistics
 * @short_description: Statistical characteristics of fields
 *
 * Overall field characteristics such as the minimum, maximum or mean value are
 * cached and they are not recalculated until the field data changes.  Usually
 * you don't need to worry about this detail.  However, if you modify the data
 * directly, #GwyField does not always know you have changed it and you may
 * have to use gwy_field_invalidate() to explicitly invalidate the cached
 * values.
 **/

/**
 * GwyFieldQuartersFunc:
 * @zul: Upper-left value.
 * @zur: Upper-right value.
 * @zlr: Lower-right value.
 * @zll: Lower-left value.
 * @wul: Upper-left weight (0 or 1).
 * @wur: Upper-right weight (0 or 1).
 * @wlr: Lower-right weight (0 or 1).
 * @wll: Lower-left weight (0 or 1).
 * @user_data: User data passed to gwy_field_process_quarters().
 *
 * Type of function used in processing fields by quarters.
 *
 * Note the values and weights are passed in a clock-wise order around the
 * square starting from the top-left corner.  This means that subsequent values
 * share a side of the square but the order is different from a 2×2 field.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
