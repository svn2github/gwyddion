/*
 *  $Id$
 *  Copyright (C) 2009-2011 David Nečas (Yeti).
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
#include "libgwy/field-statistics.h"
#include "libgwy/field-arithmetic.h"
#include "libgwy/field-internal.h"

#define CVAL GWY_FIELD_CVAL
#define CBIT GWY_FIELD_CBIT
#define CTEST GWY_FIELD_CTEST

#define DSWAP(x, y) GWY_SWAP(gdouble, x, y)

/* for compatibility checks */
#define EPSILON 1e-6

enum { NORMALIZE_ALL = 0x07 };

/**
 * gwy_field_is_incompatible:
 * @field1: A data field.
 * @field2: Another data field.
 * @check: Properties to check for compatibility.
 *
 * Checks whether two fields are compatible.
 *
 * Returns: Zero if all tested properties are compatible.  Flags corresponding
 *          to failed tests if fields are not compatible.
 **/
GwyFieldCompatibilityFlags
gwy_field_is_incompatible(GwyField *field1,
                          GwyField *field2,
                          GwyFieldCompatibilityFlags check)
{

    g_return_val_if_fail(GWY_IS_FIELD(field1), check);
    g_return_val_if_fail(GWY_IS_FIELD(field2), check);

    guint xres1 = field1->xres;
    guint xres2 = field2->xres;
    guint yres1 = field1->yres;
    guint yres2 = field2->yres;
    gdouble xreal1 = field1->xreal;
    gdouble xreal2 = field2->xreal;
    gdouble yreal1 = field1->yreal;
    gdouble yreal2 = field2->yreal;
    GwyFieldCompatibilityFlags result = 0;

    /* Resolution */
    if (check & GWY_FIELD_COMPATIBLE_XRES) {
        if (xres1 != xres2)
            result |= GWY_FIELD_COMPATIBLE_XRES;
    }
    if (check & GWY_FIELD_COMPATIBLE_YRES) {
        if (yres1 != yres2)
            result |= GWY_FIELD_COMPATIBLE_YRES;
    }

    /* Real size */
    /* Keeps the conditions for real numbers in negative form to catch NaNs and
     * odd values as incompatible. */
    if (check & GWY_FIELD_COMPATIBLE_XREAL) {
        if (!(fabs(log(xreal1/xreal2)) <= EPSILON))
            result |= GWY_FIELD_COMPATIBLE_XREAL;
    }
    if (check & GWY_FIELD_COMPATIBLE_YREAL) {
        if (!(fabs(log(yreal1/yreal2)) <= EPSILON))
            result |= GWY_FIELD_COMPATIBLE_YREAL;
    }

    /* Measure */
    if (check & GWY_FIELD_COMPATIBLE_DX) {
        if (!(fabs(log(xreal1/xres1*xres2/xreal2)) <= EPSILON))
            result |= GWY_FIELD_COMPATIBLE_DX;
    }
    if (check & GWY_FIELD_COMPATIBLE_DY) {
        if (!(fabs(log(yreal1/yres1*yres2/yreal2)) <= EPSILON))
            result |= GWY_FIELD_COMPATIBLE_DY;
    }

    /* Lateral units */
    if (check & GWY_FIELD_COMPATIBLE_LATERAL) {
        /* This can cause instantiation of field units as a side effect */
        GwyUnit *unit1 = gwy_field_get_unit_xy(field1);
        GwyUnit *unit2 = gwy_field_get_unit_xy(field2);
        if (!gwy_unit_equal(unit1, unit2))
            result |= GWY_FIELD_COMPATIBLE_LATERAL;
    }

    /* Value units */
    if (check & GWY_FIELD_COMPATIBLE_VALUE) {
        /* This can cause instantiation of field units as a side effect */
        GwyUnit *unit1 = gwy_field_get_unit_z(field1);
        GwyUnit *unit2 = gwy_field_get_unit_z(field2);
        if (!gwy_unit_equal(unit1, unit2))
            result |= GWY_FIELD_COMPATIBLE_VALUE;
    }

    return result;
}

/**
 * gwy_field_clear:
 * @field: A two-dimensional data field.
 * @fpart: (allow-none):
 *             Area in @field to clear.  Pass %NULL to clear entire @field.
 * @mask: Mask specifying which values to modify, or %NULL.
 * @masking: Masking mode to use (has any effect only with non-%NULL @mask).
 *
 * Fills a field with zeroes.
 **/
void
gwy_field_clear(GwyField *field,
                const GwyFieldPart *fpart,
                const GwyMaskField *mask,
                GwyMaskingType masking)
{
    guint col, row, width, height, maskcol, maskrow;
    if (!_gwy_field_check_mask(field, fpart, mask, &masking,
                               &col, &row, &width, &height, &maskcol, &maskrow))
        return;

    if (masking == GWY_MASK_IGNORE) {
        gdouble *base = field->data + row*field->xres + col;
        for (guint i = 0; i < height; i++)
            gwy_clear(base + i*field->xres, width);
        if (width == field->xres && height == field->yres)
            _gwy_field_set_cache_for_flat(field, 0.0);
        else
            gwy_field_invalidate(field);
    }
    else {
        GwyMaskIter iter;
        const gboolean invert = (masking == GWY_MASK_EXCLUDE);
        for (guint i = 0; i < height; i++) {
            gdouble *d = field->data + (row + i)*field->xres + col;
            gwy_mask_field_iter_init(mask, iter, maskcol, maskrow + i);
            for (guint j = width; j; j--, d++) {
                if (!gwy_mask_iter_get(iter) == invert)
                    *d = 0.0;
                gwy_mask_iter_next(iter);
            }
        }
        gwy_field_invalidate(field);
    }
}

/**
 * gwy_field_clear_full:
 * @field: A two-dimensional data field.
 *
 * Fills an entire field with zeroes.
 **/
void
gwy_field_clear_full(GwyField *field)
{
    gwy_field_clear(field, NULL, NULL, GWY_MASK_IGNORE);
}

/**
 * gwy_field_fill:
 * @field: A two-dimensional data field.
 * @fpart: (allow-none):
 *             Area in @field to fill.  Pass %NULL to fill entire @field.
 * @mask: Mask specifying which values to modify, or %NULL.
 * @masking: Masking mode to use (has any effect only with non-%NULL @mask).
 * @value: Value to fill the part with.
 *
 * Fills a field with the specified value.
 **/
void
gwy_field_fill(GwyField *field,
               const GwyFieldPart *fpart,
               const GwyMaskField *mask,
               GwyMaskingType masking,
               gdouble value)
{
    if (!value) {
        gwy_field_clear(field, fpart, mask, masking);
        return;
    }

    guint col, row, width, height, maskcol, maskrow;
    if (!_gwy_field_check_mask(field, fpart, mask, &masking,
                               &col, &row, &width, &height, &maskcol, &maskrow))
        return;

    if (masking == GWY_MASK_IGNORE) {
        for (guint i = 0; i < height; i++) {
            gdouble *p = field->data + (i + row)*field->xres + col;
            for (guint j = width; j; j--)
                *(p++) = value;
        }
        if (width == field->xres && height == field->yres)
            _gwy_field_set_cache_for_flat(field, value);
        else
            gwy_field_invalidate(field);
    }
    else {
        GwyMaskIter iter;
        const gboolean invert = (masking == GWY_MASK_EXCLUDE);
        for (guint i = 0; i < height; i++) {
            gdouble *d = field->data + (row + i)*field->xres + col;
            gwy_mask_field_iter_init(mask, iter, maskcol, maskrow + i);
            for (guint j = width; j; j--, d++) {
                if (!gwy_mask_iter_get(iter) == invert)
                    *d = value;
                gwy_mask_iter_next(iter);
            }
        }
        gwy_field_invalidate(field);
    }
}

/**
 * gwy_field_fill_full:
 * @field: A two-dimensional data field.
 * @value: Value to fill the field with.
 *
 * Fills an entire field with the specified value.
 **/
void
gwy_field_fill_full(GwyField *field,
                    gdouble value)
{
    gwy_field_fill(field, NULL, NULL, GWY_MASK_IGNORE, value);
}

/**
 * gwy_field_add:
 * @field: A two-dimensional data field.
 * @fpart: (allow-none):
 *             Area in @field to process.  Pass %NULL to process entire @field.
 * @mask: Mask specifying which values to modify, or %NULL.
 * @masking: Masking mode to use (has any effect only with non-%NULL @mask).
 * @value: Value to add to the data.
 *
 * Adds a value to data in a field.
 **/
void
gwy_field_add(GwyField *field,
              const GwyFieldPart *fpart,
              const GwyMaskField *mask,
              GwyMaskingType masking,
              gdouble value)
{
    guint col, row, width, height, maskcol, maskrow;
    if (!_gwy_field_check_mask(field, fpart, mask, &masking,
                               &col, &row, &width, &height, &maskcol, &maskrow))
        return;

    if (masking == GWY_MASK_IGNORE) {
        for (guint i = 0; i < height; i++) {
            gdouble *d = field->data + (row + i)*field->xres + col;
            for (guint j = width; j; j--, d++)
                *d += value;
        }
        if (width == field->xres && height == field->yres) {
            Field *priv = field->priv;
            priv->cached &= (CBIT(MIN) | CBIT(MAX) | CBIT(AVG) | CBIT(RMS)
                             | CBIT(MED) | CBIT(ARF) | CBIT(ART) | CBIT(ARE));
            CVAL(priv, MIN) += value;
            CVAL(priv, MAX) += value;
            CVAL(priv, AVG) += value;
            // RMS does not change
            CVAL(priv, MED) += value;
            CVAL(priv, ARF) += value;
            CVAL(priv, ART) += value;
            // ARE does not change
        }
        else
            gwy_field_invalidate(field);
    }
    else {
        GwyMaskIter iter;
        const gboolean invert = (masking == GWY_MASK_EXCLUDE);
        for (guint i = 0; i < height; i++) {
            gdouble *d = field->data + (row + i)*field->xres + col;
            gwy_mask_field_iter_init(mask, iter, maskcol, maskrow + i);
            for (guint j = width; j; j--, d++) {
                if (!gwy_mask_iter_get(iter) == invert)
                    *d += value;
                gwy_mask_iter_next(iter);
            }
        }
        gwy_field_invalidate(field);
    }
}

/**
 * gwy_field_multiply:
 * @field: A two-dimensional data field.
 * @fpart: (allow-none):
 *             Area in @field to process.  Pass %NULL to process entire @field.
 * @mask: Mask specifying which values to modify, or %NULL.
 * @masking: Masking mode to use (has any effect only with non-%NULL @mask).
 * @value: Value to multiply the data with.
 *
 * Multiplies data in a field with a value.
 **/
void
gwy_field_multiply(GwyField *field,
                   const GwyFieldPart *fpart,
                   const GwyMaskField *mask,
                   GwyMaskingType masking,
                   gdouble value)
{
    guint col, row, width, height, maskcol, maskrow;
    if (!_gwy_field_check_mask(field, fpart, mask, &masking,
                               &col, &row, &width, &height, &maskcol, &maskrow))
        return;

    if (masking == GWY_MASK_IGNORE) {
        for (guint i = 0; i < height; i++) {
            gdouble *d = field->data + (row + i)*field->xres + col;
            for (guint j = width; j; j--, d++)
                *d *= value;
        }
        if (width == field->xres && height == field->yres) {
            Field *priv = field->priv;
            priv->cached &= (CBIT(MIN) | CBIT(MAX) | CBIT(AVG) | CBIT(RMS)
                             | CBIT(MSQ) | CBIT(MED) | CBIT(ARF) | CBIT(ART));
            CVAL(priv, MIN) *= value;
            CVAL(priv, MAX) *= value;
            if (value < 0.0)
                DSWAP(CVAL(priv, MIN), CVAL(priv, MAX));
            CVAL(priv, AVG) *= value;
            CVAL(priv, RMS) *= fabs(value);
            CVAL(priv, MSQ) *= value*value;
            CVAL(priv, MED) *= value;
            CVAL(priv, ARF) *= value;
            CVAL(priv, ART) *= value;
            if (value < 0.0)
                DSWAP(CVAL(priv, ARF), CVAL(priv, ART));
        }
        else
            gwy_field_invalidate(field);
    }
    else {
        GwyMaskIter iter;
        const gboolean invert = (masking == GWY_MASK_EXCLUDE);
        for (guint i = 0; i < height; i++) {
            gdouble *d = field->data + (row + i)*field->xres + col;
            gwy_mask_field_iter_init(mask, iter, maskcol, maskrow + i);
            for (guint j = width; j; j--, d++) {
                if (!gwy_mask_iter_get(iter) == invert)
                    *d *= value;
                gwy_mask_iter_next(iter);
            }
        }
        gwy_field_invalidate(field);
    }
}

/**
 * gwy_field_normalize:
 * @field: A two-dimensional data field.
 * @fpart: (allow-none):
 *             Area in @field to process.  Pass %NULL to process entire @field.
 * @mask: Mask specifying which values to modify, or %NULL.
 * @masking: Masking mode to use (has any effect only with non-%NULL @mask).
 * @mean: New mean value.
 * @rms: New rms value (it must be non-negative).
 * @flags: Flags controlling normalisation.
 *
 * Normalises a field to speficied mean and rms.
 *
 * Returns: %TRUE if the normalisation was successfull.  It can fail in two
 *          circumstances: if the mask is empty or if the current rms is zero
 *          but the requested rms is non-zero.
 **/
gboolean
gwy_field_normalize(GwyField *field,
                    const GwyFieldPart *fpart,
                    const GwyMaskField *mask,
                    GwyMaskingType masking,
                    gdouble mean,
                    gdouble rms,
                    GwyNormalizeFlags flags)
{
    guint col, row, width, height, maskcol, maskrow;
    if (!_gwy_field_check_mask(field, fpart, mask, &masking,
                               &col, &row, &width, &height, &maskcol, &maskrow))
        return FALSE;

    gboolean retval = TRUE;
    if (flags & ~NORMALIZE_ALL)
        g_warning("Unknown normalization flags 0x%x.", flags & ~NORMALIZE_ALL);

    gdouble fmean = gwy_field_mean(field, fpart, mask, masking);
    gdouble frms = gwy_field_rms(field, fpart, mask, masking);
    // If only rms is requested we must correct the shift to keep the mean.
    if ((flags & GWY_NORMALIZE_RMS) && !(flags & GWY_NORMALIZE_MEAN)) {
        mean = fmean;
        flags |= GWY_NORMALIZE_MEAN;
    }
    // Cleverly do the correct thing if rms == frms == 0.
    gdouble q = 1.0;
    if ((flags & GWY_NORMALIZE_RMS) && frms != rms) {
        if (frms) {
            q = rms/frms;
            if (flags & GWY_NORMALIZE_ENTIRE_DATA)
                gwy_field_multiply(field, NULL, NULL, GWY_MASK_IGNORE, q);
            else
                gwy_field_multiply(field, fpart, mask, masking, q);
        }
        else
            retval = FALSE;
    }
    if ((flags & GWY_NORMALIZE_MEAN) && q*fmean != mean) {
        if (flags & GWY_NORMALIZE_ENTIRE_DATA)
            gwy_field_add(field, NULL, NULL, GWY_MASK_IGNORE, mean - q*fmean);
        else
            gwy_field_add(field, fpart, mask, masking, mean - q*fmean);
    }

    return retval;
}

/**
 * gwy_field_apply_func:
 * @field: A two-dimensional data field.
 * @fpart: (allow-none):
 *             Area in @field to process.  Pass %NULL to process entire @field.
 * @mask: Mask specifying which values to modify, or %NULL.
 * @masking: Masking mode to use (has any effect only with non-%NULL @mask).
 * @func: Function to apply to the value of each pixel (or each pixel under/
 *        not under the mask if masking is used).
 * @user_data: User data passed to @func.
 *
 * Applies a function to each pixel in a field.
 **/
void
gwy_field_apply_func(GwyField *field,
                     const GwyFieldPart *fpart,
                     const GwyMaskField *mask,
                     GwyMaskingType masking,
                     GwyRealFunc func,
                     gpointer user_data)
{
    g_return_if_fail(func);

    guint col, row, width, height, maskcol, maskrow;
    if (!_gwy_field_check_mask(field, fpart, mask, &masking,
                               &col, &row, &width, &height, &maskcol, &maskrow))
        return;

    if (masking == GWY_MASK_IGNORE) {
        for (guint i = 0; i < height; i++) {
            gdouble *d = field->data + (row + i)*field->xres + col;
            for (guint j = width; j; j--, d++)
                *d = func(*d, user_data);
        }
    }
    else {
        GwyMaskIter iter;
        const gboolean invert = (masking == GWY_MASK_EXCLUDE);
        for (guint i = 0; i < height; i++) {
            gdouble *d = field->data + (row + i)*field->xres + col;
            gwy_mask_field_iter_init(mask, iter, maskcol, maskrow + i);
            for (guint j = width; j; j--, d++) {
                if (!gwy_mask_iter_get(iter) == invert)
                    *d = func(*d, user_data);
                gwy_mask_iter_next(iter);
            }
        }
    }
    gwy_field_invalidate(field);
}

/**
 * gwy_field_add_field:
 * @src: Source two-dimensional data data field.
 * @srcpart: Area in field @src to add.  Pass %NULL to add entire @src.
 * @dest: Destination two-dimensional data field.
 * @destcol: Destination column in @dest.
 * @destrow: Destination row in @dest.
 * @factor: Value to multiply @src data with before adding.
 *
 * Adds data from one field to another.
 *
 * The rectangle of added data is defined by @srcpart and the values are
 * added to @dest starting from (@destcol, @destrow).
 *
 * There are no limitations on the row and column indices or dimensions.  Only
 * the part of the rectangle that is corresponds to data inside @src and @dest
 * is added.  This can also mean @dest is not modified at all.
 *
 * If @src is equal to @dest the areas may <emphasis>not</emphasis> overlap.
 *
 * This function can be used also to subtract fields by passing @factor equal
 * to -1:
 * |[
 * // Add field2 to equally-sized field1
 * gwy_field_add_field(field2, NULL, field1, 0, 0, 1.0);
 *
 * // Subtract it again to get field1 back (rounding errors aside)
 * gwy_field_add_field(field2, NULL, field1, 0, 0, -1.0);
 * ]|
 **/
void
gwy_field_add_field(const GwyField *src,
                    const GwyFieldPart *srcpart,
                    GwyField *dest,
                    guint destcol,
                    guint destrow,
                    gdouble factor)
{
    guint col, row, width, height;
    if (!_gwy_field_limit_parts(src, srcpart, dest, destcol, destrow,
                                FALSE, &col, &row, &width, &height))
        return;
    if (!factor)
        return;

    const gdouble *srcbase = src->data + src->xres*row + col;
    gdouble *destbase = dest->data + dest->xres*destrow + destcol;
    for (guint i = 0; i < height; i++) {
        const gdouble *srow = srcbase + i*src->xres;
        gdouble *drow = destbase + i*dest->xres;
        for (guint j = width; j; j--, srow++, drow++)
            *drow += factor*(*srow);
    }
    gwy_field_invalidate(dest);
}

/**
 * gwy_field_hypot_field:
 * @field: A two-dimensional data data field.
 *          It may be one of @operand1, @operand2.
 * @operand1: A two-dimensional data field forming one of the operands.
 * @operand2: A two-dimensional data field forming another of the operands.
 *
 * Calculates the hypotenuse of two fields.
 **/
void
gwy_field_hypot_field(GwyField *field,
                      const GwyField *operand1,
                      const GwyField *operand2)
{
    g_return_if_fail(GWY_IS_FIELD(field));
    g_return_if_fail(GWY_IS_FIELD(operand1));
    g_return_if_fail(GWY_IS_FIELD(operand2));
    g_return_if_fail(field->xres == operand1->xres
                     && field->xres == operand2->xres);
    g_return_if_fail(field->yres == operand1->yres
                     && field->yres == operand2->yres);

    for (guint k = 0; k < field->xres*field->yres; k++)
        field->data[k] = hypot(operand1->data[k], operand2->data[k]);

    gwy_field_invalidate(field);
}

/**
 * gwy_field_clamp:
 * @field: A two-dimensional data field.
 * @fpart: (allow-none):
 *             Area in @field to process.  Pass %NULL to process entire @field.
 * @lower: Lower limit value (it must not be greater than @upper).
 * @upper: Upper limit value (it must not be smaller than @lower).
 *
 * Limits values in a field to the specified range.
 *
 * Returns: The number of changed values, i.e. values that were outside
 *          the interval [@lower, @upper].
 **/
guint
gwy_field_clamp(GwyField *field,
                const GwyFieldPart *fpart,
                gdouble lower, gdouble upper)
{
    g_return_val_if_fail(lower <= upper, 0);

    guint col, row, width, height;
    if (!_gwy_field_check_part(field, fpart, &col, &row, &width, &height))
        return 0;

    guint count = 0;
    for (guint i = 0; i < height; i++) {
        gdouble *drow = field->data + (row + i)*field->xres + col;
        for (guint j = width; j; drow++, j--) {
            if (*drow < lower) {
                *drow = lower;
                count++;
            }
            else if (*drow > upper) {
                *drow = upper;
                count++;
            }
        }

    }

    if (!count)
        return count;

    if (width == field->xres && height == field->yres) {
        Field *priv = field->priv;
        gdouble min = CVAL(priv, MIN), max = CVAL(priv, MAX),
                med = CVAL(priv, MED);
        priv->cached &= (CBIT(MIN) | CBIT(MAX)
                         | (med >= lower && med <= upper ? CBIT(MED) : 0));
        CVAL(priv, MIN) = CLAMP(min, lower, upper);
        CVAL(priv, MAX) = CLAMP(max, lower, upper);
    }
    else
        gwy_field_invalidate(field);

    return count;
}

/**
 * SECTION: field-arithmetic
 * @section_id: GwyField-arithmetic
 * @title: GwyField arithmetic
 * @short_description: Arithmetic operations with fields
 *
 * Arithmetic operations with fields are, in a wider sense, pixel-wise
 * operations with field data or operations that combine the corresponding
 * pixels of several equally-sized fields.  The first class includes methods
 * such as gwy_field_add() or gwy_field_apply_func() while the second includes
 * methods that usually have <quote>field</quote> repeated in their name such
 * as gwy_field_add_field() or gwy_field_hypot_field().
 *
 * Non-pixel-wise operations that work also on the neighbourhood pixels
 * are described in section <link linkend='GwyField-filter'>GwyField
 * filtering</link>.
 **/

/**
 * GwyFieldCompatibilityFlags:
 * @GWY_FIELD_COMPATIBLE_XRES: X-resolution (width).
 * @GWY_FIELD_COMPATIBLE_YRES: Y-resolution (height)
 * @GWY_FIELD_COMPATIBLE_RES: Both resolutions.
 * @GWY_FIELD_COMPATIBLE_XREAL: Physical x-dimension.
 * @GWY_FIELD_COMPATIBLE_YREAL: Physical y-dimension.
 * @GWY_FIELD_COMPATIBLE_REAL: Both physical dimensions.
 * @GWY_FIELD_COMPATIBLE_DX: Pixel size in x-direction.
 * @GWY_FIELD_COMPATIBLE_DY: Pixel size in y-direction.
 * @GWY_FIELD_COMPATIBLE_DXDY: Pixel dimensions.
 * @GWY_FIELD_COMPATIBLE_LATERAL: Lateral units.
 * @GWY_FIELD_COMPATIBLE_VALUE: Value units.
 * @GWY_FIELD_COMPATIBLE_ALL: All defined properties.
 *
 * Field properties that can be checked for compatibility.
 **/

/**
 * GwyNormalizeFlags:
 * @GWY_NORMALIZE_MEAN: Normalise the mean value to speficied.
 * @GWY_NORMALIZE_RMS: Nomalise the root mean square to speficied.
 * @GWY_NORMALIZE_ENTIRE_DATA: Apply the normalisation to entire data if
 *                             masking is used, i.e. masking and part are
 *                             used only for the calculation of the mean or rms
 *                             value.
 *
 * Flags controlling behaviour of normalisation functions.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
