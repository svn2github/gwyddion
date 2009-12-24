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
#include "libgwy/field-statistics.h"
#include "libgwy/libgwy-aliases.h"
#include "libgwy/processing-internal.h"

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
    g_return_if_fail(GWY_IS_FIELD(field));
    g_return_if_fail(col + width <= field->xres);
    g_return_if_fail(row + height <= field->yres);
    gboolean part_mask = FALSE;
    if (mask && masking != GWY_MASK_IGNORE) {
        g_return_if_fail(GWY_IS_MASK_FIELD(mask));
        if (mask->xres == field->xres && mask->yres == field->yres)
            part_mask = FALSE;
        else if (mask->xres == width && mask->yres == height)
            part_mask = TRUE;
        else {
            g_critical("Mask dimensions match neither the entire field "
                       "nor the rectangle.");
            return;
        }
    }
    else
        masking = GWY_MASK_IGNORE;

    if (!min && !max)
        return;

    gdouble min1 = G_MAXDOUBLE, max1 = -G_MAXDOUBLE;
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
    guint maskcol = part_mask ? 0 : col;
    guint maskrow = part_mask ? 0 : row;
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
 **/
void
gwy_field_min_max(GwyField *field,
                  gdouble *min,
                  gdouble *max)
{
    gwy_field_part_min_max(field, NULL, GWY_MASK_IGNORE,
                           0, 0, field->xres, field->yres, min, max);
}


#define __LIBGWY_FIELD_STATISTICS_C__
#include "libgwy/libgwy-aliases.c"

/**
 * SECTION: field-statistics
 * @title: GwyField statistics
 * @short_description: Statistical characteristics of fields
 *
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
