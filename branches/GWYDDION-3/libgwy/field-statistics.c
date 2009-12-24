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
#include "libgwy/macros.h"
#include "libgwy/math.h"
#include "libgwy/field-statistics.h"
#include "libgwy/libgwy-aliases.h"
#include "libgwy/processing-internal.h"

#define ASSIGN(p, q, n) memcpy((p), (q), (n)*sizeof(gdouble))

static gboolean
stats_check_args(GwyField *field,
                 GwyMaskField *mask,
                 GwyMaskingType *masking,
                 guint col, guint row,
                 guint width, guint height,
                 guint *maskcol, guint *maskrow)
{
    g_return_val_if_fail(GWY_IS_FIELD(field), FALSE);
    g_return_val_if_fail(col + width <= field->xres, FALSE);
    g_return_val_if_fail(row + height <= field->yres, FALSE);
    if (mask && (*masking == GWY_MASK_INCLUDE
                 || *masking == GWY_MASK_EXCLUDE)) {
        g_return_val_if_fail(GWY_IS_MASK_FIELD(mask), FALSE);
        if (mask->xres == field->xres && mask->yres == field->yres) {
            *maskcol = col;
            *maskrow = row;
        }
        else if (mask->xres == width && mask->yres == height)
            *maskcol = *maskrow = 0;
        else {
            g_critical("Mask dimensions match neither the entire field "
                       "nor the rectangle.");
            return FALSE;
        }
    }
    else
        *masking = GWY_MASK_IGNORE;

    return TRUE;
}

/**
 * gwy_field_part_min_max:
 * @field: A two-dimensional data field.
 * @mask: Mask specifying which values to take into account/exclude, or %NULL.
 *        Its dimensions must match either the dimensions of @field or the
 *        rectangular part.  In the first case the mask is placed over the
 *        entire field, in the second case over the rectangle.
 * @masking: Masking mode to use (has any effect only with non-%NULL @mask).
 * @col: Column index of the upper-left corner of the rectangle in @field.
 * @row: Row index of the upper-left corner of the rectangle in @field.
 * @width: Rectangle width (number of columns).
 * @height: Rectangle height (number of rows).
 * @min: Location to store the minimum to, or %NULL.
 * @max: Location to store the maximum to, or %NULL.
 *
 * Finds the minimum and maximum value in a rectangular part of a field.
 *
 * The maximum value of no data is <constant>-HUGE_VAL</constant>, the
 * minimum is <constant>HUGE_VAL</constant>.
 **/
void
gwy_field_part_min_max(GwyField *field,
                       GwyMaskField *mask,
                       GwyMaskingType masking,
                       guint col, guint row,
                       guint width, guint height,
                       gdouble *min,
                       gdouble *max)
{
    guint maskcol, maskrow;
    if (!stats_check_args(field, mask, &masking,
                          col, row, width, height, &maskcol, &maskrow))
        return;
    if (!min && !max)
        return;

    gdouble min1 = HUGE_VAL, max1 = -HUGE_VAL;
    if (!width || !height) {
        GWY_MAYBE_SET(min, min1);
        GWY_MAYBE_SET(max, max1);
        return;
    }

    const gdouble *base = field->data + row*field->xres + col;

    // No mask.  If full field is processed we must use the cache.
    if (masking == GWY_MASK_IGNORE) {
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
        }
        GWY_MAYBE_SET(min, min1);
        GWY_MAYBE_SET(max, max1);
        return;
    }

    // Masking is in use.
    if (masking == GWY_MASK_INCLUDE) {
        for (guint i = 0; i < height; i++) {
            const gdouble *d = base + i*field->xres;
            GwyMaskFieldIter iter;
            gwy_mask_field_iter_init(mask, iter, maskcol, maskrow + i);
            for (guint j = width; j; j--, d++) {
                if (gwy_mask_field_iter_get(iter)) {
                    if (min1 > *d)
                        min1 = *d;
                    if (max1 < *d)
                        max1 = *d;
                }
                gwy_mask_field_iter_next(iter);
            }
        }
    }
    else {
        for (guint i = 0; i < height; i++) {
            const gdouble *d = base + i*field->xres;
            GwyMaskFieldIter iter;
            gwy_mask_field_iter_init(mask, iter, maskcol, maskrow + i);
            for (guint j = width; j; j--, d++) {
                if (!gwy_mask_field_iter_get(iter)) {
                    if (min1 > *d)
                        min1 = *d;
                    if (max1 < *d)
                        max1 = *d;
                }
                gwy_mask_field_iter_next(iter);
            }
        }
    }

    GWY_MAYBE_SET(min, min1);
    GWY_MAYBE_SET(max, max1);
}

/**
 * gwy_field_min_max:
 * @field: A two-dimensional data field.
 * @min: Location to store the minimum to, or %NULL.
 * @max: Location to store the maximum to, or %NULL.
 *
 * Finds the minimum and maximum value in a field.
 *
 * The minimum and maximum are cached, see gwy_field_invalidate().
 **/
void
gwy_field_min_max(GwyField *field,
                  gdouble *min,
                  gdouble *max)
{
    g_return_if_fail(GWY_IS_FIELD(field));
    gwy_field_part_min_max(field, NULL, GWY_MASK_IGNORE,
                           0, 0, field->xres, field->yres, min, max);
}

/**
 * gwy_field_part_mean:
 * @field: A two-dimensional data field.
 * @mask: Mask specifying which values to take into account/exclude, or %NULL.
 *        Its dimensions must match either the dimensions of @field or the
 *        rectangular part.  In the first case the mask is placed over the
 *        entire field, in the second case over the rectangle.
 * @masking: Masking mode to use (has any effect only with non-%NULL @mask).
 * @col: Column index of the upper-left corner of the rectangle in @field.
 * @row: Row index of the upper-left corner of the rectangle in @field.
 * @width: Rectangle width (number of columns).
 * @height: Rectangle height (number of rows).
 *
 * Calculates the mean value of a rectangular part of a data field.
 *
 * Returns: The mean value.  The mean value of no data is NaN.
 **/
gdouble
gwy_field_part_mean(GwyField *field,
                    GwyMaskField *mask,
                    GwyMaskingType masking,
                    guint col, guint row,
                    guint width, guint height)
{
    guint maskcol, maskrow;
    if (!stats_check_args(field, mask, &masking,
                          col, row, width, height, &maskcol, &maskrow))
        return NAN;

    if (!width || !height)
        return NAN;

    const gdouble *base = field->data + row*field->xres + col;
    gdouble mean = 0.0;

    // No mask.  If full field is processed we must use the cache.
    if (masking == GWY_MASK_IGNORE) {
        gboolean full_field = (width == field->xres && height == field->yres);
        if (full_field && CTEST(field->priv, AVG))
            return CVAL(field->priv, AVG);
        for (guint i = 0; i < height; i++) {
            const gdouble *d = base + i*field->xres;
            for (guint j = width; j; j--, d++)
                mean += *d;
        }
        mean /= width*height;
        if (full_field)
            CVAL(field->priv, AVG) = mean;
        return mean;
    }

    // Masking is in use.
    guint n = 0;
    if (masking == GWY_MASK_INCLUDE) {
        for (guint i = 0; i < height; i++) {
            const gdouble *d = base + i*field->xres;
            GwyMaskFieldIter iter;
            gwy_mask_field_iter_init(mask, iter, maskcol, maskrow + i);
            for (guint j = width; j; j--, d++) {
                if (gwy_mask_field_iter_get(iter)) {
                    mean += *d;
                    n++;
                }
                gwy_mask_field_iter_next(iter);
            }
        }
    }
    else {
        for (guint i = 0; i < height; i++) {
            const gdouble *d = base + i*field->xres;
            GwyMaskFieldIter iter;
            gwy_mask_field_iter_init(mask, iter, maskcol, maskrow + i);
            for (guint j = width; j; j--, d++) {
                if (!gwy_mask_field_iter_get(iter)) {
                    mean += *d;
                    n++;
                }
                gwy_mask_field_iter_next(iter);
            }
        }
    }
    return n ? mean/n : NAN;
}

/**
 * gwy_field_mean:
 * @field: A two-dimensional data field.
 *
 * Calculates the mean value of a data field.
 *
 * The mean value is cached, see gwy_field_invalidate().
 *
 * Returns: The mean value.
 **/
gdouble
gwy_field_mean(GwyField *field)
{
    g_return_val_if_fail(GWY_IS_FIELD(field), NAN);
    return gwy_field_part_mean(field, NULL, GWY_MASK_IGNORE,
                               0, 0, field->xres, field->yres);
}

/**
 * gwy_field_part_median:
 * @field: A two-dimensional data field.
 * @mask: Mask specifying which values to take into account/exclude, or %NULL.
 *        Its dimensions must match either the dimensions of @field or the
 *        rectangular part.  In the first case the mask is placed over the
 *        entire field, in the second case over the rectangle.
 * @masking: Masking mode to use (has any effect only with non-%NULL @mask).
 * @col: Column index of the upper-left corner of the rectangle in @field.
 * @row: Row index of the upper-left corner of the rectangle in @field.
 * @width: Rectangle width (number of columns).
 * @height: Rectangle height (number of rows).
 *
 * Calculates the median value of a rectangular part of a data field.
 *
 * Returns: The median value.  The median value of no data is NaN.
 **/
gdouble
gwy_field_part_median(GwyField *field,
                      GwyMaskField *mask,
                      GwyMaskingType masking,
                      guint col, guint row,
                      guint width, guint height)
{
    guint maskcol, maskrow;
    if (!stats_check_args(field, mask, &masking,
                          col, row, width, height, &maskcol, &maskrow))
        return NAN;

    if (!width || !height)
        return NAN;

    const gdouble *base = field->data + row*field->xres + col;
    gsize bufsize = width*height*sizeof(gdouble);
    gdouble *buffer = g_slice_alloc(bufsize);

    // No mask.  If full field is processed we must use the cache.
    if (masking == GWY_MASK_IGNORE) {
        gboolean full_field = (width == field->xres && height == field->yres);
        if (full_field && CTEST(field->priv, MED))
            return CVAL(field->priv, MED);
        for (guint i = 0; i < height; i++)
            ASSIGN(buffer + i*width, base + i*field->xres, width);
        gdouble median = gwy_math_median(buffer, width*height);
        g_slice_free1(bufsize, buffer);
        if (full_field)
            CVAL(field->priv, MED) = median;
        return median;
    }

    // Masking is in use.
    gdouble *p = buffer;
    if (masking == GWY_MASK_INCLUDE) {
        for (guint i = 0; i < height; i++) {
            const gdouble *d = base + i*field->xres;
            GwyMaskFieldIter iter;
            gwy_mask_field_iter_init(mask, iter, maskcol, maskrow + i);
            for (guint j = width; j; j--, d++) {
                if (gwy_mask_field_iter_get(iter))
                    *(p++) = *d;
                gwy_mask_field_iter_next(iter);
            }
        }
    }
    else {
        for (guint i = 0; i < height; i++) {
            const gdouble *d = base + i*field->xres;
            GwyMaskFieldIter iter;
            gwy_mask_field_iter_init(mask, iter, maskcol, maskrow + i);
            for (guint j = width; j; j--, d++) {
                if (!gwy_mask_field_iter_get(iter))
                    *(p++) = *d;
                gwy_mask_field_iter_next(iter);
            }
        }
    }

    gdouble median = (p != buffer) ? gwy_math_median(buffer, p - buffer) : NAN;
    g_slice_free1(bufsize, buffer);
    return median;
}

/**
 * gwy_field_median:
 * @field: A two-dimensional data field.
 *
 * Calculates the median value of a data field.
 *
 * The median value is cached, see gwy_field_invalidate().
 *
 * Returns: The median value.
 **/
gdouble
gwy_field_median(GwyField *field)
{
    g_return_val_if_fail(GWY_IS_FIELD(field), NAN);
    return gwy_field_part_median(field, NULL, GWY_MASK_IGNORE,
                                 0, 0, field->xres, field->yres);
}

/**
 * gwy_field_part_rms:
 * @field: A two-dimensional data field.
 * @mask: Mask specifying which values to take into account/exclude, or %NULL.
 *        Its dimensions must match either the dimensions of @field or the
 *        rectangular part.  In the first case the mask is placed over the
 *        entire field, in the second case over the rectangle.
 * @masking: Masking mode to use (has any effect only with non-%NULL @mask).
 * @col: Column index of the upper-left corner of the rectangle in @field.
 * @row: Row index of the upper-left corner of the rectangle in @field.
 * @width: Rectangle width (number of columns).
 * @height: Rectangle height (number of rows).
 *
 * Calculates the rms value of a rectangular part of a data field.
 *
 * Returns: The rms value.  The rms value of no data is zero.
 **/
gdouble
gwy_field_part_rms(GwyField *field,
                   GwyMaskField *mask,
                   GwyMaskingType masking,
                   guint col, guint row,
                   guint width, guint height)
{
    guint maskcol, maskrow;
    if (!stats_check_args(field, mask, &masking,
                          col, row, width, height, &maskcol, &maskrow))
        return 0.0;

    if (!width || !height)
        return 0.0;

    const gdouble *base = field->data + row*field->xres + col;
    gdouble rms = 0.0, avg = 0.0;

    // No mask.  If full field is processed we must use the cache.
    if (masking == GWY_MASK_IGNORE) {
        gboolean full_field = (width == field->xres && height == field->yres);
        if (full_field && CTEST(field->priv, RMS))
            return CVAL(field->priv, RMS);
        for (guint i = 0; i < height; i++) {
            const gdouble *d = base + i*field->xres;
            for (guint j = width; j; j--, d++) {
                avg += *d;
                rms += (*d)*(*d);
            }
        }
        rms /= width*height;
        avg /= width*height;
        rms -= avg*avg;
        rms = sqrt(MAX(rms, 0.0));
        if (full_field)
            CVAL(field->priv, RMS) = rms;
        return rms;
    }

    // Masking is in use.
    guint n = 0;
    if (masking == GWY_MASK_INCLUDE) {
        for (guint i = 0; i < height; i++) {
            const gdouble *d = base + i*field->xres;
            GwyMaskFieldIter iter;
            gwy_mask_field_iter_init(mask, iter, maskcol, maskrow + i);
            for (guint j = width; j; j--, d++) {
                if (gwy_mask_field_iter_get(iter)) {
                    avg += *d;
                    rms += (*d)*(*d);
                    n++;
                }
                gwy_mask_field_iter_next(iter);
            }
        }
    }
    else {
        for (guint i = 0; i < height; i++) {
            const gdouble *d = base + i*field->xres;
            GwyMaskFieldIter iter;
            gwy_mask_field_iter_init(mask, iter, maskcol, maskrow + i);
            for (guint j = width; j; j--, d++) {
                if (!gwy_mask_field_iter_get(iter)) {
                    avg += *d;
                    rms += (*d)*(*d);
                    n++;
                }
                gwy_mask_field_iter_next(iter);
            }
        }
    }

    if (!n)
        return 0.0;

    rms /= n;
    avg /= n;
    rms -= avg*avg;
    return sqrt(MAX(rms, 0.0));
}

/**
 * gwy_field_rms:
 * @field: A two-dimensional data field.
 *
 * Calculates the rms value of a data field.
 *
 * The rms value is cached, see gwy_field_invalidate().
 *
 * Returns: The rms value.
 **/
gdouble
gwy_field_rms(GwyField *field)
{
    g_return_val_if_fail(GWY_IS_FIELD(field), 0.0);
    return gwy_field_part_rms(field, NULL, GWY_MASK_IGNORE,
                              0, 0, field->xres, field->yres);
}

#define __LIBGWY_FIELD_STATISTICS_C__
#include "libgwy/libgwy-aliases.c"

/**
 * SECTION: field-statistics
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

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
