/*
 *  $Id$
 *  Copyright (C) 2013 David Nečas (Yeti).
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
#include "libgwy/types.h"
#include "libgwy/mask-field-arithmetic.h"
#include "libgwy/field-statistics.h"
#include "libgwy/field-mark.h"
#include "libgwy/mask-field-internal.h"

// √2 erf⁻¹(2/3)
#define K2_3 0.9674215661017013
// √2 erf⁻¹(1/2)
#define K1_2 0.6744897501960818

/**
 * gwy_field_mark_outliers:
 * @field: A two-dimensional data field.
 * @fpart: (allow-none):
 *         Area in @field to process.  Pass %NULL to process entire @field.
 * @outliers: Target mask field for the outliers mask.  Its dimenisions may
 *            match either @field ot @fpart.  In the former case only data
 *            inside @fpart are modified.
 * @mask: (allow-none):
 *        Mask specifying which values to take into account/exclude, or %NULL.
 * @masking: Masking mode to use (has any effect only with non-%NULL @mask).
 * @deviation: Deviation type to mask.
 * @threshold: Threshold for outlier recognition, larger thresholds mean less
 *             sensitivity, smaller thresholds mean more data will be marked
 *             as outliers.  Value of 3 or 4 should be a reasonable threshold.
 *             Pass zero to use the default thresholding, whatever it is.
 *
 * Mask global outliers in a two-dimensional data field using distribution
 * thresholding.
 *
 * The mask values are set for field data found to be outliers and cleared for
 * other field data within the processed area.  If masking is in effect, only
 * values in @outliers corresponding to actually processed @field data are set
 * or cleared.
 *
 * Note if the area (including masking) has less than 6 pixels, no value is
 * ever marked as an outlier.
 *
 * Returns: The total number of outliers marked.
 **/
guint
gwy_field_mark_outliers(const GwyField *field,
                        const GwyFieldPart *fpart,
                        GwyMaskField *outliers,
                        const GwyMaskField *mask,
                        GwyMasking masking,
                        GwyDeviation deviation,
                        gdouble threshold)
{
    guint col, row, width, height, maskcol, maskrow;
    if (!gwy_field_check_mask(field, fpart, mask, &masking,
                              &col, &row, &width, &height, &maskcol, &maskrow))
        return 0;

    guint targetcol, targetrow;
    if (!gwy_field_check_target_mask(field, outliers,
                                     &(GwyFieldPart){ col, row, width, height },
                                     &targetcol, &targetrow))
         return 0;

    g_return_val_if_fail(gwy_deviation_is_valid(deviation), 0);
    g_return_val_if_fail(threshold >= 0.0, 0);
    if (threshold == 0.0)
        threshold = 3.5;

    GwyFieldPart mpart = { maskcol, maskrow, width, height };
    GwyFieldPart tpart = { targetrow, targetrow, width, height };

    guint n = width*height;
    if (masking != GWY_MASK_IGNORE) {
        n = gwy_mask_field_part_count(mask, &mpart,
                                      masking == GWY_MASK_INCLUDE);
        gwy_mask_field_part_logical(outliers, &tpart, mask, maskcol, maskrow,
                                    (masking == GWY_MASK_INCLUDE
                                     ? GWY_LOGICAL_NIMPL
                                     : GWY_LOGICAL_AND));
    }
    else {
        gwy_mask_field_fill(outliers, &tpart, FALSE);
    }

    if (n < 6)
        return 0;

    guint stride = gwy_round(0.618*sqrt(n));
    guint ns = (n/stride + 12);
    ns = MIN(ns, n);
    gdouble *samples = g_new(gdouble, ns);

    guint xres = field->xres;
    const gdouble *base = field->data + xres*row + col;

    if (masking == GWY_MASK_IGNORE) {
        for (guint k = 0; k < ns; k++) {
            guint kk = k*(n-1)/(ns-1);
            guint i = kk/width, j = kk % width;
            gdouble v = base[i*xres + j];
            samples[k] = v;
        }
    }
    else {
        const gboolean invert = (masking == GWY_MASK_EXCLUDE);
        guint k = 0, kk = 0, next = 0;
        for (guint i = 0; i < height; i++) {
            const gdouble *d = base + i*field->xres;
            GwyMaskIter iter;
            gwy_mask_field_iter_init(mask, iter, maskcol, maskrow + i);
            for (guint j = width; j; j--, d++) {
                if (!gwy_mask_iter_get(iter) == invert) {
                    if (kk == next) {
                        gdouble v = base[i*xres + j];
                        samples[k++] = v;
                        next = k*(n-1)/(ns-1);
                    }
                    kk++;
                }
                gwy_mask_iter_next(iter);
            }
        }
        g_assert(k == ns);
    }

    gwy_math_sort(samples, NULL, ns);

    gdouble median = ((ns % 2)
                      ? samples[ns/2]
                      : 0.5*(samples[ns/2] + samples[ns/2 + 1]));
    gdouble lowerthresh, upperthresh;
    if (ns >= 40) {
        gdouble upper56 = samples[(5*ns - 1)/6],
                lower16 = samples[ns/6];
        lowerthresh = (lower16 - median)*threshold/K2_3 + median;
        upperthresh = (upper56 - median)*threshold/K2_3 + median;
    }
    else {
        gdouble upper34 = samples[(3*ns - 1)/4],
                lower14 = samples[ns/4];
        lowerthresh = (lower14 - median)*threshold/K1_2 + median;
        upperthresh = (upper34 - median)*threshold/K1_2 + median;
    }

    g_free(samples);

    gboolean marklower = (deviation == GWY_DEVIATION_DOWN
                          || deviation == GWY_DEVIATION_BOTH);
    gboolean markupper = (deviation == GWY_DEVIATION_UP
                          || deviation == GWY_DEVIATION_BOTH);
    guint nout = 0;

    if (masking == GWY_MASK_IGNORE) {
        for (guint i = 0; i < height; i++) {
            const gdouble *d = base + i*field->xres;
            for (guint j = 0; j < width; j++, d++) {
                if (markupper && *d > upperthresh) {
                    gwy_mask_field_set(outliers, j+targetcol, i+targetrow,
                                       TRUE);
                    nout++;
                }
                else if (marklower && *d < lowerthresh) {
                    gwy_mask_field_set(outliers, j+targetcol, i+targetrow,
                                       TRUE);
                    nout++;
                }
            }
        }
    }
    else {
        const gboolean invert = (masking == GWY_MASK_EXCLUDE);
        for (guint i = 0; i < height; i++) {
            const gdouble *d = base + i*field->xres;
            GwyMaskIter iter;
            gwy_mask_field_iter_init(mask, iter, maskcol, maskrow + i);
            for (guint j = 0; j < width; j++, d++) {
                if (!gwy_mask_iter_get(iter) == invert) {
                    if (markupper && *d > upperthresh) {
                        gwy_mask_field_set(outliers, j+targetcol, i+targetrow,
                                           TRUE);
                        nout++;
                    }
                    else if (marklower && *d < lowerthresh) {
                        gwy_mask_field_set(outliers, j+targetcol, i+targetrow,
                                           TRUE);
                        nout++;
                    }
                }
                gwy_mask_iter_next(iter);
            }
        }
    }

    gwy_mask_field_invalidate(extrema);

    return nout;
}

// Mark sharp maxima with 2, known non-maxima with 1.
static guint
mark_maxima(const GwyField *field,
            guint *types)
{
    guint xres = field->xres, yres = field->yres;
    const gdouble *d = field->data;

    guint k = 0, unmarked = xres*yres;
    for (guint i = 0; i < yres; i++) {
        for (guint j = 0; j < xres; j++, k++) {
            // Mark non-maxima.
            if ((i && d[k] < d[k-xres])
                || (j && d[k] < d[k-1])
                || (j < xres-1 && d[k] < d[k+1])
                || (i < yres-1 && d[k] < d[k+xres])) {
                types[k] = 1;
                unmarked--;
            }
            // Mark maxima.
            else if ((!i || d[k] > d[k-xres])
                     && (!j || d[k] > d[k-1])
                     && (j == xres-1 || d[k] > d[k+1])
                     && (i == yres-1 || d[k] > d[k+xres])) {
                types[k] = 2;
                unmarked--;
            }
        }
    }

    return unmarked;
}

// Mark sharp minima with 2, known non-minima with 1.
static guint
mark_minima(const GwyField *field,
            guint *types)
{
    guint xres = field->xres, yres = field->yres;
    const gdouble *d = field->data;

    guint k = 0, unmarked = xres*yres;
    for (guint i = 0; i < yres; i++) {
        for (guint j = 0; j < xres; j++, k++) {
            // Mark non-minima.
            if ((i && d[k] > d[k-xres])
                || (j && d[k] > d[k-1])
                || (j < xres-1 && d[k] > d[k+1])
                || (i < yres-1 && d[k] > d[k+xres])) {
                types[k] = 1;
                unmarked--;
            }
            // Mark minima.
            else if ((!i || d[k] < d[k-xres])
                     && (!j || d[k] < d[k-1])
                     && (j == xres-1 || d[k] < d[k+1])
                     && (i == yres-1 || d[k] < d[k+xres])) {
                types[k] = 2;
                unmarked--;
            }
        }
    }

    return unmarked;
}

// Propagate non-maxima type to all pixels of the same value.  Or minima. This
// alogorithm no longer depends on how the states was marked, it just
// propagates the 1 state though identical values.
static void
propagate_non_extrema_marking(guint *types, const gdouble *d,
                              guint xres, guint yres)
{
    IntList *inqueue = int_list_new(16);
    IntList *outqueue = int_list_new(16);
    guint k = 0;
    for (guint i = 0; i < yres; i++) {
        for (guint j = 0; j < xres; j++, k++) {
            if (types[k])
                continue;
            // If the value is equal to some neighbour which is a known
            // non-maximum then this pixel is also non-maximum.  (If the
            // neighbour is a known maximum this pixel cannot be unknown.)
            if ((i && types[k-xres] && d[k] == d[k-xres])
                || (j && types[k-1] && d[k] == d[k-1])
                || (j < xres-1 && types[k+1] && d[k] == d[k+1])
                || (i < yres-1 && types[k+xres] && d[k] == d[k+xres])) {
                types[k] = 1;
                int_list_add(outqueue, k);
            }
        }
    }
    GWY_SWAP(IntList*, inqueue, outqueue);

    while (inqueue->len) {
        for (guint m = 0; m < inqueue->len; m++) {
            k = inqueue->data[m];
            guint i = k/xres, j = k % xres;

            // Propagate the non-maximum type to all still unknown
            // neighbours.  Since we set them to known immediately, double
            // queuing is avoided.
            if (i && !types[k-xres]) {
                types[k-xres] = 1;
                int_list_add(outqueue, k-xres);
            }
            if (j && !types[k-1]) {
                types[k-1] = 1;
                int_list_add(outqueue, k-1);
            }
            if (j < xres-1 && !types[k+1]) {
                types[k+1] = 1;
                int_list_add(outqueue, k+1);
            }
            if (i < yres-1 && !types[k+xres]) {
                types[k+xres] = 1;
                int_list_add(outqueue, k+xres);
            }
        }

        inqueue->len = 0;
        GWY_SWAP(IntList*, inqueue, outqueue);
    }

    int_list_free(inqueue);
    int_list_free(outqueue);
}

// TODO: Support parts/masks.
/**
 * gwy_field_mark_extrema:
 * @field: A two-dimensional data field.
 * @extrema: Target mask field for the extrema mask.
 * @maxima: %TRUE to mark maxima, %FALSE to mark minima.
 *
 * Marks local maxima or minima in a two-dimensional field.
 *
 * Maximum is a contiguous set of pixels that have the same value and this
 * value is sharply greater than the value of any pixel touching the set.  A
 * minimum is defined analogously.  A field filled with a single value is
 * considered to have neither minimum nor maximum.
 **/
void
gwy_field_mark_extrema(const GwyField *field,
                       GwyMaskField *extrema,
                       gboolean maxima)
{
    g_return_if_fail(GWY_IS_FIELD(field));
    g_return_if_fail(GWY_IS_MASK_FIELD(extrema));
    guint xres = field->xres, yres = field->yres;
    g_return_if_fail(extrema->xres == xres);
    g_return_if_fail(extrema->yres == yres);

    gwy_mask_field_fill(extrema, NULL, FALSE);

    gdouble min, max;
    gwy_field_min_max_full(field, &min, &max);
    // This takes care of 1×1 fields too.
    if (min == max)
        return;

    guint *types = g_new0(guint, xres*yres);
    guint unmarked = (maxima ? mark_maxima : mark_minima)(field, types);

    if (unmarked)
        propagate_non_extrema_marking(types, field->data, xres, yres);

    // Mark 1 as 0 (non-extremum); mark 0 and 2 as 1 (extremum).  The remaining
    // 0s are exactly those flat areas which cannot be made non-maximum, i.e.
    // they must be maxima.  Assume extrema are relatively sparse so prefer
    // fast iteration compared to fast mask bit setting.
    for (guint k = 0; k < xres*yres; k++) {
        if (!(types[k] & 1))
            gwy_mask_field_set(extrema, k % xres, k/xres, TRUE);
    }

    g_free(types);

    gwy_mask_field_invalidate(extrema);
}

/**
 * SECTION: field-mark
 * @section_id: GwyField-mark
 * @title: GwyField data marking
 * @short_description: Marking of features or defects in fields
 **/

/**
 * GwyDeviation:
 * @GWY_DEVIATION_BOTH: Deviation towards both smaller and larger values.
 * @GWY_DEVIATION_DOWN: Deviation towards smaller values.
 * @GWY_DEVIATION_UP: Deviation towards larger values.
 *
 * Type of deviation or value change.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
