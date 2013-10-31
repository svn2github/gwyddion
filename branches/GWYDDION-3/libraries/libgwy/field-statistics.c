/*
 *  $Id$
 *  Copyright (C) 2009-2013 David Nečas (Yeti).
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
#include "libgwy/object-utils.h"
#include "libgwy/field-mark.h"
#include "libgwy/field-statistics.h"
#include "libgwy/mask-field-arithmetic.h"
#include "libgwy/math-internal.h"
#include "libgwy/field-internal.h"

#define CVAL GWY_FIELD_CVAL
#define CBIT GWY_FIELD_CBIT
#define CTEST GWY_FIELD_CTEST

typedef struct {
    gdouble s;
    gdouble dx;
    gdouble dy;
} SurfaceAreaData;

typedef struct {
    gdouble s;
    gdouble wself;
    gdouble wortho;
    gdouble wall;
} VolumeQuadratureData;

typedef struct {
    gdouble base;
    gdouble v;
} MaterialQuadratureData;

// We store only the self and orthogonal weight, the diagonal is always 1.
static const gdouble volume_weights[][2] = {
    { 484.0, 22.0, },  // Default = biqudratic
    { 52.0, 10.0, },   // Gwyddion2
    { 36.0, 6.0, },    // Triangular
    { 28.0, 4.0, },    // Bilinear
    { 484.0, 22.0, },  // Biqudratic
};

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
                  GwyMasking masking,
                  gdouble *min,
                  gdouble *max)
{
    guint col, row, width, height, maskcol, maskrow;
    if (!gwy_field_check_mask(field, fpart, mask, &masking,
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
               GwyMasking masking)
{
    guint col, row, width, height, maskcol, maskrow;
    if (!gwy_field_check_mask(field, fpart, mask, &masking,
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
                 GwyMasking masking)
{
    guint col, row, width, height, maskcol, maskrow;
    if (!gwy_field_check_mask(field, fpart, mask, &masking,
                              &col, &row, &width, &height, &maskcol, &maskrow))
        return NAN;

    const gdouble *base = field->data + row*field->xres + col;
    gdouble *buffer = g_new(gdouble, width*height);

    if (masking == GWY_MASK_IGNORE) {
        // No mask.  If full field is processed we must use the cache.
        gboolean full_field = (width == field->xres && height == field->yres);
        if (full_field && CTEST(field->priv, MED))
            return CVAL(field->priv, MED);
        for (guint i = 0; i < height; i++)
            gwy_assign(buffer + i*width, base + i*field->xres, width);
        gdouble median = gwy_math_median(buffer, width*height);
        g_free(buffer);
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
    g_free(buffer);
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
              GwyMasking masking)
{
    guint col, row, width, height, maskcol, maskrow;
    if (!gwy_field_check_mask(field, fpart, mask, &masking,
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
                 GwyMasking masking)
{
    guint col, row, width, height, maskcol, maskrow;
    if (!gwy_field_check_mask(field, fpart, mask, &masking,
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
 * gwy_field_meansq_full:
 * @field: A two-dimensional data field.
 *
 * Calculates the mean square of an entire field.
 *
 * See gwy_field_meansq() for discussion.
 *
 * Returns: The mean square of data values.
 **/
gdouble
gwy_field_meansq_full(const GwyField *field)
{
    return gwy_field_meansq(field, NULL, NULL, GWY_MASK_IGNORE);
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
                     GwyMasking masking,
                     gdouble *mean, gdouble *ra, gdouble *rms,
                     gdouble *skew, gdouble *kurtosis)
{
    guint col, row, width, height, maskcol, maskrow;
    if (!gwy_field_check_mask(field, fpart, mask, &masking,
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
                gdouble v = *d - avg, v2 = v*v, v3 = v2*v, v4 = v3*v;
                sumabs += fabs(v);
                sum2 += v2;
                sum3 += v3;
                sum4 += v4;
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
                    gdouble v = *d - avg, v2 = v*v, v3 = v2*v, v4 = v3*v;
                    sumabs += fabs(v);
                    sum2 += v2;
                    sum3 += v3;
                    sum4 += v4;
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

static inline gdouble
xlnx_int(guint x)
{
    static const gdouble xlnx_table[] = {
        0.0,
        0.0,
        1.38629436111989061882,
        3.29583686600432907417,
        5.54517744447956247532,
        8.04718956217050187300,
        10.75055681536833000486,
        13.62137104338719313570,
        16.63553233343868742600,
        19.77502119602597444511,
        23.02585092994045684010,
        26.37684800078207598466,
        29.81887979745600372264,
        33.34434164699997756865,
        36.94680261461362060328,
        40.62075301653315098985,
        44.36141955583649980256,
        48.16462684895567336408,
        52.02669164213096445960,
        55.94434060416236874000,
        59.91464547107981986860,
        63.93497119219188292650,
        68.00293397388294877634,
        72.11636696637044288840,
        76.27329192835069487136,
        80.47189562170501873000,
    };

    if (x < G_N_ELEMENTS(xlnx_table))
        return xlnx_table[x];

    return x*log(x);
}

/**
 * gwy_field_entropy:
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
 * Estimates the entropy of field data distribution.
 *
 * The estimate is calculated as @S = ln(@n Δ) − 1/@n ∑ @n_i ln(@n_i), where
 * @n is the number of pixels considered, Δ the bin size and @n_i the count in
 * the @i-th bin.  If @S is plotted as a function of the bin size Δ, it is,
 * generally, a growing function with a plateau for ‘reasonable’ bin sizes.
 * The estimate is taken at the plateau.
 *
 * It should be noted that this estimate may be biased.
 *
 * Returns: The estimated entropy of the data values.  The entropy of no data
 *          is NaN, the entropy of single-valued data is infinity.
 **/
gdouble
gwy_field_entropy(const GwyField *field,
                  const GwyFieldPart *fpart,
                  const GwyMaskField *mask,
                  GwyMasking masking)
{
    guint col, row, width, height, maskcol, maskrow;
    if (!gwy_field_check_mask(field, fpart, mask, &masking,
                              &col, &row, &width, &height, &maskcol, &maskrow))
        return NAN;

    GwyFieldPart mpart = { maskcol, maskrow, width, height };
    guint n = width*height;
    if (masking != GWY_MASK_IGNORE) {
        n = gwy_mask_field_part_count(mask, &mpart,
                                      masking == GWY_MASK_INCLUDE);
        if (!n)
            return NAN;
    }

    gdouble min, max;
    gwy_field_min_max(field, fpart, mask, masking, &min, &max);
    if (min == max)
        return HUGE_VAL;
    // Return explicit estimates for n < 4, making maxdiv at least 2.
    if (n == 2)
        return log(max - min);
    if (n == 3)
        return log(max - min) + 0.5*log(1.5) - G_LN2/3.0;

    // If we have serious outlies, we must get rid of them and update min, max.
    // Do that by fixing the mask but keeping @n.  This corresponds to the
    // reasonable assumption each outlier would get its own bin and thus
    // contribute zero to the n_i*log(n_i) sum.  There is a reasoanble upper
    // bound on the error so induced.
    GwyMaskField *tmpmask = gwy_mask_field_new_sized(width, height, FALSE);
    if (gwy_field_mark_outliers(field, fpart, tmpmask, mask, masking,
                                GWY_DEVIATION_BOTH, 0.0)) {
        if (masking != GWY_MASK_IGNORE) {
            GwyLogicalOperator op = (masking == GWY_MASK_EXCLUDE
                                     ? GWY_LOGICAL_OR
                                     : GWY_LOGICAL_NCIMPL);
            if (mask->xres == width && mask->yres == height)
                gwy_mask_field_logical(tmpmask, mask, NULL, op);
            else {
                GwyMaskField *xmask = gwy_mask_field_new_part(mask, &mpart);
                gwy_mask_field_logical(tmpmask, xmask, NULL, op);
                g_object_unref(xmask);
            }
        }
        else {
            masking = GWY_MASK_EXCLUDE;
        }
        mask = tmpmask;
        maskcol = maskrow = 0;
        gwy_field_min_max(field, fpart, mask, masking, &min, &max);
    }

    guint maxdiv = (guint)floor(log2(n) + 1e-12);
    g_assert(maxdiv >= 2);
    guint size = 1 << maxdiv;
    guint *counts = g_new0(guint, size);
    gdouble *ecurve = g_new(gdouble, maxdiv+1);
    const gdouble *base = field->data + row*field->xres + col;

    // Gather counts at the finest scale.
    if (masking == GWY_MASK_IGNORE) {
        for (guint i = 0; i < height; i++) {
            const gdouble *d = base + i*field->xres;
            for (guint j = width; j; j--, d++) {
                gint k = floor((*d - min)/(max - min)*size);
                k = CLAMP(k, 0, (gint)size-1);
                counts[k]++;
            }
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
                    gint k = floor((*d - min)/(max - min)*size);
                    k = CLAMP(k, 0, (gint)size-1);
                    counts[k]++;
                }
                gwy_mask_iter_next(iter);
            }
        }
    }

    // Calculate the entropy for all bin sizes in the logarithmic progression.
    for (guint div = 0; div <= maxdiv; div++) {
        gdouble S = 0.0;
        guint *ck = counts;
        for (guint k = size; k; k--, ck++)
            S += xlnx_int(*ck);
        S = log(n*(max - min)/size) - S/n;
        ecurve[div] = S;
        size >>= 1;

        // Make the bins twice as large.
        guint *ck2 = counts;
        ck = counts;
        for (guint k = size; k; k--, ck2 += 2)
            *(ck++) = *(ck2) + *(ck2 + 1);
    }

    g_free(counts);

    // Find the flattest part of the curve and use the value there as the
    // entropy estimate.  Handle the too-few-pixels cases gracefully.
    gdouble S;
    if (maxdiv < 5) {
        gdouble mindiff = G_MAXDOUBLE;
        guint imin = 1;

        for (guint k = 0; k <= maxdiv-2; k++) {
            gdouble diff = (fabs(ecurve[k] - ecurve[k+1])
                            + fabs(ecurve[k+1] - ecurve[k+2]));
            if (diff < mindiff) {
                mindiff = diff;
                imin = k+1;
            }
        }
        S = ecurve[imin];
    }
    else {
        gdouble mindiff = G_MAXDOUBLE;
        guint imin = 2;

        for (guint k = 0; k <= maxdiv-4; k++) {
            gdouble diff = (fabs(ecurve[k] - ecurve[k+1])
                            + fabs(ecurve[k+1] - ecurve[k+2])
                            + fabs(ecurve[k+2] - ecurve[k+3])
                            + fabs(ecurve[k+3] - ecurve[k+4]));
            if (diff < mindiff) {
                mindiff = diff;
                imin = k+2;
            }
        }
        S = (ecurve[imin-1] + ecurve[imin] + ecurve[imin+1])/3.0;
    }

    g_free(ecurve);
    g_object_unref(tmpmask);

    return S;
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
                            GwyMasking masking,
                            gdouble above, gdouble below,
                            gboolean strict,
                            guint *nabove, guint *nbelow)
{
    guint col, row, width, height, maskcol, maskrow;
    if (!gwy_field_check_mask(field, fpart, mask, &masking,
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
 * pixel_quarter_area:
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
pixel_quarter_area(gdouble z1, gdouble z2, gdouble z3, gdouble z4,
                   guint w1, guint w2, guint w3, guint w4,
                   gpointer user_data)
{
    SurfaceAreaData *sadata = (SurfaceAreaData*)user_data;
    gdouble dx = sadata->dx, dy = sadata->dy,
            d21 = (z2 - z1)/dx, d23 = (z2 - z3)/dy,
            d14 = (z1 - z4)/dy, d34 = (z3 - z4)/dx,
            d1423 = 0.75*d14 + 0.25*d23, d2134 = 0.75*d21 + 0.25*d34,
            d2314 = 0.75*d23 + 0.25*d14, d3421 = 0.75*d34 + 0.25*d21,
            D1423 = d1423*d1423, D2134 = d2134*d2134,
            D2314 = d2314*d2314, D3421 = d3421*d3421,
            D21 = 1.0 + d21*d21, D14 = 1.0 + d14*d14,
            D34 = 1.0 + d34*d34, D23 = 1.0 + d23*d23,
            Dv = 1.0 + 0.25*(d14 + d23)*(d14 + d23),
            Dh = 1.0 + 0.25*(d21 + d34)*(d21 + d34);
    guint w;
    gdouble s = 0.0;

    if ((w = (w1 + w2)))
        s += w*sqrt(Dv + D2134);
    if ((w = (w2 + w3)))
        s += w*sqrt(Dh + D2314);
    if ((w = (w3 + w4)))
        s += w*sqrt(Dv + D3421);
    if ((w = (w4 + w1)))
        s += w*sqrt(Dh + D1423);
    if (w1)
        s += sqrt(D21 + D1423) + sqrt(D14 + D2134);
    if (w2)
        s += sqrt(D21 + D2314) + sqrt(D23 + D2134);
    if (w3)
        s += sqrt(D34 + D2314) + sqrt(D23 + D3421);
    if (w4)
        s += sqrt(D34 + D1423) + sqrt(D14 + D3421);

    sadata->s += s;
}

/**
 * pixel_allquarter_area:
 * @z1: Z-value in first corner.
 * @z2: Z-value in second corner.
 * @z3: Z-value in third corner.
 * @z4: Z-value in fourth corner.
 * @user_data: #SurfaceAreaData.
 *
 * Calculates approximate area of a one general rectangular pixel with some
 * corners possibly missing.
 **/
static void
pixel_allquarter_area(gdouble z1, gdouble z2, gdouble z3, gdouble z4,
                      gpointer user_data)
{
    SurfaceAreaData *sadata = (SurfaceAreaData*)user_data;
    gdouble dx = sadata->dx, dy = sadata->dy,
            d21 = (z2 - z1)/dx, d23 = (z2 - z3)/dy,
            d14 = (z1 - z4)/dy, d34 = (z3 - z4)/dx,
            d1423 = 0.75*d14 + 0.25*d23, d2134 = 0.75*d21 + 0.25*d34,
            d2314 = 0.75*d23 + 0.25*d14, d3421 = 0.75*d34 + 0.25*d21,
            D1423 = d1423*d1423, D2134 = d2134*d2134,
            D2314 = d2314*d2314, D3421 = d3421*d3421,
            D21 = 1.0 + d21*d21, D14 = 1.0 + d14*d14,
            D34 = 1.0 + d34*d34, D23 = 1.0 + d23*d23,
            Dv = 1.0 + 0.25*(d14 + d23)*(d14 + d23),
            Dh = 1.0 + 0.25*(d21 + d34)*(d21 + d34);
    gdouble s = 2.0*(sqrt(Dv + D2134) + sqrt(Dh + D2314)
                     + sqrt(Dv + D3421) + sqrt(Dh + D1423));
    s += (sqrt(D21 + D1423) + sqrt(D14 + D2134)
          + sqrt(D21 + D2314) + sqrt(D23 + D2134)
          + sqrt(D34 + D2314) + sqrt(D23 + D3421)
          + sqrt(D34 + D1423) + sqrt(D14 + D3421));

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
                       GwyMasking masking)
{
    g_return_val_if_fail(GWY_IS_FIELD(field), 0.0);

    gdouble dx = gwy_field_dx(field), dy = gwy_field_dy(field);
    gboolean full_field = ((!mask || masking == GWY_MASK_IGNORE)
                           && (!fpart || (fpart->width == field->xres
                                          && fpart->height == field->yres
                                          && fpart->col == 0
                                          && fpart->row == 0)));
    SurfaceAreaData sadata = { 0.0, dx, dy };
    gwy_field_process_quarters(field, fpart, mask, masking, TRUE,
                               &pixel_quarter_area, &pixel_allquarter_area,
                               &sadata);
    gdouble area = sadata.s*dx*dy/16.0;
    if (full_field) {
        CVAL(field->priv, ARE) = area;
        field->priv->cached |= CBIT(ARE);
    }
    return area;
}

static void
volume_quadrature(gdouble z1, gdouble z2, gdouble z3, gdouble z4,
                  guint w1, guint w2, guint w3, guint w4,
                  gpointer user_data)
{
    VolumeQuadratureData *vqdata = (VolumeQuadratureData*)user_data;
    gdouble ss = (w1*z1 + w2*z2 + w3*z3 + w4*z4)*vqdata->wself;
    gdouble so = ((w1 + w3)*(z2 + z4) + (w2 + w4)*(z1 + z3))*vqdata->wortho;
    gdouble sd = w1*z3 + w2*z4 + w3*z1 + w4*z2;
    vqdata->s += ss + so + sd;
}

static void
volume_quadrature_all(gdouble z1, gdouble z2, gdouble z3, gdouble z4,
                      gpointer user_data)
{
    VolumeQuadratureData *vqdata = (VolumeQuadratureData*)user_data;
    vqdata->s += (z1 + z2 + z3 + z4)*vqdata->wall;
}

/**
 * gwy_field_volume:
 * @field: A two-dimensional data field.
 * @fpart: (allow-none):
 *         Area in @field to process.  Pass %NULL to process entire @field.
 * @mask: (allow-none):
 *        Mask specifying which values to take into account/exclude, or %NULL.
 * @masking: Masking mode to use (has any effect only with non-%NULL @mask).
 * @method: Quadrature method.  Pass %GWY_FIELD_VOLUME_DEFAULT (zero).
 *
 * Calculates the volume under a field surface.
 *
 * This method calculates the classical quadrature representing volume under
 * the surface with basis at @z=0, positive field values adding to the volume,
 * negative field values subtracting from the volume.
 *
 * Returns: The volume under the field surface.
 **/
gdouble
gwy_field_volume(const GwyField *field,
                 const GwyFieldPart *fpart,
                 const GwyMaskField *mask,
                 GwyMasking masking,
                 GwyFieldVolumeMethod method)
{
    g_return_val_if_fail(GWY_IS_FIELD(field), 0.0);
    g_return_val_if_fail(method < G_N_ELEMENTS(volume_weights), 0.0);

    gdouble dx = gwy_field_dx(field), dy = gwy_field_dy(field);
    gdouble wself = volume_weights[method][0],
            wortho = volume_weights[method][1];
    VolumeQuadratureData vqdata = {
        .s = 0.0,
        .wself = 0.25*wself,
        .wortho = 0.5*wortho,
        .wall = 0.25*wself + wortho + 1.0,
    };
    gwy_field_process_quarters(field, fpart, mask, masking, TRUE,
                               &volume_quadrature, &volume_quadrature_all,
                               &vqdata);
    gdouble volume = vqdata.s*dx*dy/(wself + 4.0*wortho + 4.0);
    return volume;
}

static gdouble
volume_triprism_material(gdouble za, gdouble zb, gdouble zc)
{
    gdouble min1 = fmin(za, zc);
    gdouble min = fmin(min1, zb);
    if (min >= 0.0)
        return za + zb + zc;

    gdouble max1 = fmax(za, zc);
    gdouble max = fmax(max1, zb);
    if (max <= 0.0)
        return 0.0;

    // Zero level crosses the triangle, must calculate the positive part.
    gdouble mid = zb;
    if (min1 != min)
        mid = min1;
    else if (max1 != max)
        mid = max1;

    if (mid <= 0.0)
        return max*max*max/(max - min)/(max - mid);

    gdouble p = mid/(mid - min), q = max/(max - min);
    return p*mid + q*max - p*q*min;
}

static gdouble
volume_material_quadrature1(gdouble z1, gdouble z2, gdouble z3, gdouble z4,
                           guint w1, guint w2, guint w3, guint w4)
{
    gdouble zc = 0.25*(z1 + z2 + z3 + z4);
    gdouble v = 0.0;
    if (w1) {
        v += (volume_triprism_material(0.5*(z1 + z2), z1, zc)
              + volume_triprism_material(0.5*(z4 + z1), zc, z1));
    }
    if (w2) {
        v += (volume_triprism_material(0.5*(z1 + z2), z2, zc)
              + volume_triprism_material(0.5*(z2 + z3), zc, z2));
    }
    if (w3) {
        v += (volume_triprism_material(0.5*(z2 + z3), zc, z3)
              + volume_triprism_material(0.5*(z3 + z4), z3, zc));
    }
    if (w4) {
        v += (volume_triprism_material(0.5*(z4 + z1), zc, z4)
              + volume_triprism_material(0.5*(z3 + z4), z4, zc));
    }
    return v;
}

static void
volume_material_quadrature(gdouble z1, gdouble z2, gdouble z3, gdouble z4,
                           guint w1, guint w2, guint w3, guint w4,
                           gpointer user_data)
{
    MaterialQuadratureData *mqdata = (MaterialQuadratureData*)user_data;
    mqdata->v += volume_material_quadrature1(z1 - mqdata->base,
                                             z2 - mqdata->base,
                                             z3 - mqdata->base,
                                             z4 - mqdata->base,
                                             w1, w2, w3, w4);
}

static gdouble
volume_material_quadrature_all1(gdouble z1, gdouble z2, gdouble z3, gdouble z4)
{
    gdouble zc = 0.25*(z1 + z2 + z3 + z4);
    return 2.0*(volume_triprism_material(zc, z1, z2)
                + volume_triprism_material(zc, z2, z3)
                + volume_triprism_material(zc, z3, z4)
                + volume_triprism_material(zc, z4, z1));
}

static void
volume_material_quadrature_all(gdouble z1, gdouble z2, gdouble z3, gdouble z4,
                               gpointer user_data)
{
    MaterialQuadratureData *mqdata = (MaterialQuadratureData*)user_data;
    mqdata->v += volume_material_quadrature_all1(z1 - mqdata->base,
                                                 z2 - mqdata->base,
                                                 z3 - mqdata->base,
                                                 z4 - mqdata->base);
}

static void
volume_voids_quadrature(gdouble z1, gdouble z2, gdouble z3, gdouble z4,
                        guint w1, guint w2, guint w3, guint w4,
                        gpointer user_data)
{
    MaterialQuadratureData *mqdata = (MaterialQuadratureData*)user_data;
    mqdata->v += volume_material_quadrature1(mqdata->base - z1,
                                             mqdata->base - z2,
                                             mqdata->base - z3,
                                             mqdata->base - z4,
                                             w1, w2, w3, w4);
}

static void
volume_voids_quadrature_all(gdouble z1, gdouble z2, gdouble z3, gdouble z4,
                            gpointer user_data)
{
    MaterialQuadratureData *mqdata = (MaterialQuadratureData*)user_data;
    mqdata->v += volume_material_quadrature_all1(mqdata->base - z1,
                                                 mqdata->base - z2,
                                                 mqdata->base - z3,
                                                 mqdata->base - z4);
}

/**
 * gwy_field_material_volume:
 * @field: A two-dimensional data field.
 * @fpart: (allow-none):
 *         Area in @field to process.  Pass %NULL to process entire @field.
 * @mask: (allow-none):
 *        Mask specifying which values to take into account/exclude, or %NULL.
 * @masking: Masking mode to use (has any effect only with non-%NULL @mask).
 * @material: %TRUE to calculate the material volume above @base, %FALSE to
 *            calculate the volume of voids below @base.
 * @base: Base level above or below which the volume is to be calculated.
 *        Usually, but not necessarily, it is the field minimum or maximum.
 *
 * Calculates the volume of material or voids between a field surface and given
 * base level.
 *
 * This functions differs from gwy_field_volume() substantially.  If material
 * volume above a certain base is calculated then only the parts of the surface
 * that are above the base contribute to the result.  The parts below the base
 * do not contribute at all, whereas in gwy_field_volume() they contribute a
 * negative volume.
 *
 * The quadrature method corresponds to %GWY_FIELD_VOLUME_TRIANGULAR since
 * this method actually specifies the surface shape and its intersections with
 * the base level are well-defined.
 *
 * Returns: The volume of material above @base and below the field.
 **/
gdouble
gwy_field_material_volume(const GwyField *field,
                          const GwyFieldPart *fpart,
                          const GwyMaskField *mask,
                          GwyMasking masking,
                          gboolean material,
                          gdouble base)
{
    g_return_val_if_fail(GWY_IS_FIELD(field), 0.0);

    MaterialQuadratureData mqdata = { .base = base, .v = 0.0 };
    if (material)
        gwy_field_process_quarters(field, fpart, mask, masking, TRUE,
                                   &volume_material_quadrature,
                                   &volume_material_quadrature_all,
                                   &mqdata);
    else {
        gwy_field_process_quarters(field, fpart, mask, masking, TRUE,
                                   &volume_voids_quadrature,
                                   &volume_voids_quadrature_all,
                                   &mqdata);
    }

    gdouble dx = gwy_field_dx(field), dy = gwy_field_dy(field);
    gdouble volume = mqdata.v*dx*dy/24.0;
    return volume;
}

static void
process_quarters_unmasked(const GwyField *field,
                          guint col, guint row,
                          guint width, guint height,
                          gboolean include_borders,
                          GwyFieldQuartersFunc function,
                          GwyFieldAllQuartersFunc allfunction,
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
        if (include_borders || col > 0)
            function(d1[F], d1[1], d2[1], d2[F], 0, 0, 1, 0, user_data);
        d1++, d2++;
        for (guint j = width-1; j; j--, d1++, d2++)
            function(d1[0], d1[1], d2[1], d2[0], 0, 0, 1, 1, user_data);
        if (include_borders || col + width < xres)
            function(d1[0], d1[L], d2[L], d2[0], 0, 0, 0, 1, user_data);
    }

    // Middle part
    for (guint i = 0; i+1 < height; i++) {
        d1 = base-1 + i*xres;
        d2 = d1 + xres;
        if (include_borders || col > 0)
            function(d1[F], d1[1], d2[1], d2[F], 0, 1, 1, 0, user_data);
        d1++, d2++;
        if (allfunction) {
            for (guint j = width-1; j; j--, d1++, d2++)
                allfunction(d1[0], d1[1], d2[1], d2[0], user_data);
        }
        else {
            for (guint j = width-1; j; j--, d1++, d2++)
                function(d1[0], d1[1], d2[1], d2[0], 1, 1, 1, 1, user_data);
        }
        if (include_borders || col + width < xres)
            function(d1[0], d1[L], d2[L], d2[0], 1, 0, 0, 1, user_data);
    }

    // Bottom row.
    if (include_borders || row + height < yres) {
        d1 = base-1 + (height - 1)*xres;
        d2 = (row + height == yres) ? d1 : d1 + xres;
        if (include_borders || col > 0)
            function(d1[F], d1[1], d2[1], d2[F], 0, 1, 0, 0, user_data);
        d1++, d2++;
        for (guint j = width-1; j; j--, d1++, d2++)
            function(d1[0], d1[1], d2[1], d2[0], 1, 1, 0, 0, user_data);
        if (include_borders || col + width < xres)
            function(d1[0], d1[L], d2[L], d2[0], 1, 0, 0, 0, user_data);
    }
}

static void
process_quarters_masked(const GwyField *field,
                        guint col, guint row,
                        guint width, guint height,
                        const GwyMaskField *mask,
                        GwyMasking masking,
                        guint maskcol, guint maskrow,
                        gboolean include_borders,
                        GwyFieldQuartersFunc function,
                        GwyFieldAllQuartersFunc allfunction,
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
        if ((include_borders || col > 0) && w3)
            function(d1[F], d1[1], d2[1], d2[F], 0, 0, w3, 0, user_data);
        d1++, d2++;
        gwy_mask_iter_next(iter2);
        for (guint j = width-1; j; j--, d1++, d2++) {
            w4 = w3;
            w3 = !gwy_mask_iter_get(iter2) == invert;
            if (w3 | w4)
                function(d1[0], d1[1], d2[1], d2[0], 0, 0, w3, w4, user_data);
            gwy_mask_iter_next(iter2);
        }
        w4 = w3;
        if ((include_borders || col + width < xres) && w4)
            function(d1[0], d1[L], d2[L], d2[0], 0, 0, 0, w4, user_data);
    }

    // Middle part
    for (guint i = 0; i+1 < height; i++) {
        d1 = base-1 + i*xres;
        d2 = d1 + xres;
        gwy_mask_field_iter_init(mask, iter1, maskcol, maskrow + i);
        w2 = !gwy_mask_iter_get(iter1) == invert;
        gwy_mask_field_iter_init(mask, iter2, maskcol, maskrow + i+1);
        w3 = !gwy_mask_iter_get(iter2) == invert;
        if ((include_borders || col > 0) && (w2 | w3))
            function(d1[F], d1[1], d2[1], d2[F], 0, w2, w3, 0, user_data);
        d1++, d2++;
        gwy_mask_iter_next(iter1);
        gwy_mask_iter_next(iter2);
        if (allfunction) {
            for (guint j = width-1; j; j--, d1++, d2++) {
                w1 = w2;
                w2 = !gwy_mask_iter_get(iter1) == invert;
                w4 = w3;
                w3 = !gwy_mask_iter_get(iter2) == invert;
                if (w1 | w2 | w3 | w4) {
                    if (w1 & w2 & w3 & w4)
                        allfunction(d1[0], d1[1], d2[1], d2[0], user_data);
                    else
                        function(d1[0], d1[1], d2[1], d2[0], w1, w2, w3, w4,
                                 user_data);
                }
                gwy_mask_iter_next(iter1);
                gwy_mask_iter_next(iter2);
            }
        }
        else {
            for (guint j = width-1; j; j--, d1++, d2++) {
                w1 = w2;
                w2 = !gwy_mask_iter_get(iter1) == invert;
                w4 = w3;
                w3 = !gwy_mask_iter_get(iter2) == invert;
                if (w1 | w2 | w3 | w4)
                    function(d1[0], d1[1], d2[1], d2[0], w1, w2, w3, w4,
                             user_data);
                gwy_mask_iter_next(iter1);
                gwy_mask_iter_next(iter2);
            }
        }
        w1 = w2;
        w4 = w3;
        if ((include_borders || col + width < xres) && (w1 | w4))
            function(d1[0], d1[L], d2[L], d2[0], w1, 0, 0, w4, user_data);
    }

    // Bottom row.
    if (include_borders || row + height < yres) {
        d1 = base-1 + (height - 1)*xres;
        d2 = (row + height == yres) ? d1 : d1 + xres;
        gwy_mask_field_iter_init(mask, iter1, maskcol, maskrow + height-1);
        w2 = !gwy_mask_iter_get(iter1) == invert;
        if ((include_borders || col > 0) && w2)
            function(d1[F], d1[1], d2[1], d2[F], 0, w2, 0, 0, user_data);
        d1++, d2++;
        gwy_mask_iter_next(iter1);
        for (guint j = width-1; j; j--, d1++, d2++) {
            w1 = w2;
            w2 = !gwy_mask_iter_get(iter1) == invert;
            if (w1 | w2)
                function(d1[0], d1[1], d2[1], d2[0], w1, w2, 0, 0, user_data);
            gwy_mask_iter_next(iter1);
        }
        w1 = w2;
        if ((include_borders || col + width < xres) && w1)
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
 * @function: (scope call):
 *            Function to apply to each set of four neighbour pixels.
 * @allfunction: (scope call) (allow-none):
 *               Function to apply to sets of four neighbour pixels if all four
 *               are included.  This function is optional, it is sufficient
 *               to supply @function.  However, knowing that all four pixels
 *               are included often permits a simpler and faster calculation.
 * @user_data: User data passed to @function and @allfunction (if given).
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
                           GwyMasking masking,
                           gboolean include_borders,
                           GwyFieldQuartersFunc function,
                           GwyFieldAllQuartersFunc allfunction,
                           gpointer user_data)
{
    g_return_if_fail(function);

    guint col, row, width, height, maskcol, maskrow;
    if (!gwy_field_check_mask(field, fpart, mask, &masking,
                              &col, &row, &width, &height, &maskcol, &maskrow))
        return;

    if (masking != GWY_MASK_IGNORE)
        process_quarters_masked(field, col, row, width, height,
                                mask, masking, maskcol, maskrow,
                                include_borders,
                                function, allfunction, user_data);
    else
        process_quarters_unmasked(field, col, row, width, height,
                                  include_borders,
                                  function, allfunction, user_data);
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

/**
 * GwyFieldAllQuartersFunc:
 * @zul: Upper-left value.
 * @zur: Upper-right value.
 * @zlr: Lower-right value.
 * @zll: Lower-left value.
 * @user_data: User data passed to gwy_field_process_quarters().
 *
 * Type of function used in processing fields by quarters for all four quarters
 * included.
 *
 * This is a simpler variant #GwyFieldQuartersFunc used in the case of all four
 * quarters are included.  Therefore, no weights need to be passed.
 **/

/**
 * GwyFieldVolumeMethod:
 * @GWY_FIELD_VOLUME_DEFAULT: The default method, whatever it is (currently
 *                            %GWY_FIELD_VOLUME_BIQUADRATIC).  The only method
 *                            you need.
 * @GWY_FIELD_VOLUME_GWYDDION2: Reproducing how Gwyddion 2.x calculated the
 *                              volume by using the same quadrature weights,
 *                              although they are of unclear origin.
 * @GWY_FIELD_VOLUME_TRIANGULAR: Calculate the volume under the Gwyddion 2.x
 *                               surface area style triangulation.
 * @GWY_FIELD_VOLUME_BILINEAR: Exactly integrate bilinear interpolation in
 *                             each quarter.
 * @GWY_FIELD_VOLUME_BIQUADRATIC: Exactly integrate biquadratic interpolation.
 *
 * Field volume calculation methods.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
