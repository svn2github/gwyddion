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
#include "libgwy/field-arithmetic.h"
#include "libgwy/field-internal.h"

#define DSWAP(x, y) GWY_SWAP(gdouble, x, y)

/* for compatibility checks */
#define EPSILON 1e-6

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
 * gwy_field_add:
 * @field: A two-dimensional data field.
 * @rectangle: Area in @field to process.  Pass %NULL to process entire @field.
 * @mask: Mask specifying which values to modify, or %NULL.
 * @masking: Masking mode to use (has any effect only with non-%NULL @mask).
 * @value: Value to add to the data.
 *
 * Adds a value to data in a field.
 **/
void
gwy_field_add(GwyField *field,
              const GwyRectangle *rectangle,
              const GwyMaskField *mask,
              GwyMaskingType masking,
              gdouble value)
{
    guint col, row, width, height, maskcol, maskrow;
    if (!_gwy_field_check_mask(field, rectangle, mask, &masking,
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
            gwy_mask_field_iter_init(mask, iter, maskcol, maskrow);
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
 * @rectangle: Area in @field to process.  Pass %NULL to process entire @field.
 * @mask: Mask specifying which values to modify, or %NULL.
 * @masking: Masking mode to use (has any effect only with non-%NULL @mask).
 * @value: Value to multiply the data with.
 *
 * Multiplies data in a field with a value.
 **/
void
gwy_field_multiply(GwyField *field,
                   const GwyRectangle *rectangle,
                   const GwyMaskField *mask,
                   GwyMaskingType masking,
                   gdouble value)
{
    guint col, row, width, height, maskcol, maskrow;
    if (!_gwy_field_check_mask(field, rectangle, mask, &masking,
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
                             | CBIT(MED) | CBIT(ARF) | CBIT(ART));
            CVAL(priv, MIN) *= value;
            CVAL(priv, MAX) *= value;
            if (value < 0.0)
                DSWAP(CVAL(priv, MIN), CVAL(priv, MAX));
            CVAL(priv, AVG) *= value;
            CVAL(priv, RMS) *= fabs(value);
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
            gwy_mask_field_iter_init(mask, iter, maskcol, maskrow);
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
 * gwy_field_apply_func:
 * @field: A two-dimensional data field.
 * @rectangle: Area in @field to process.  Pass %NULL to process entire @field.
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
                     const GwyRectangle *rectangle,
                     const GwyMaskField *mask,
                     GwyMaskingType masking,
                     GwyRealFunc func,
                     gpointer user_data)
{
    g_return_if_fail(func);

    guint col, row, width, height, maskcol, maskrow;
    if (!_gwy_field_check_mask(field, rectangle, mask, &masking,
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
            gwy_mask_field_iter_init(mask, iter, maskcol, maskrow);
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
 * SECTION: field-arithmetic
 * @section_id: GwyField-arithmetic
 * @title: GwyField arithmetic
 * @short_description: Arithmetic operations with fields
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

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
