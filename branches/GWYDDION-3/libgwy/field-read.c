/*
 *  $Id$
 *  Copyright (C) 2011 David Nečas (Yeti).
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
#include "libgwy/field-read.h"

static inline guint
elliptical_xlen(gdouble eta, gdouble rx, guint ax)
{
    guint xlen = ax - gwy_round(rx*sqrt(1.0 - eta*eta) - 0.5);
    g_assert(2*xlen <= 2*ax + 1);
    return xlen;
}

static gdouble
exterior_value(const GwyField *field,
               gint col, gint row,
               GwyExteriorType exterior,
               gdouble fill_value)
{
    // Interior
    if (col >= 0 && (guint)col < field->xres
        && row >= 0 && (guint)row < field->yres)
        return field->data[col + row*field->xres];

    // Exteriors
    if (exterior == GWY_EXTERIOR_UNDEFINED)
        return NAN;

    if (exterior == GWY_EXTERIOR_FIXED_VALUE)
        return fill_value;

    if (exterior == GWY_EXTERIOR_BORDER_EXTEND) {
        col = CLAMP(col, 0, (gint)field->xres-1);
        row = CLAMP(row, 0, (gint)field->yres-1);
    }
    else if (exterior == GWY_EXTERIOR_PERIODIC) {
        col = (col % field->xres) + (col < 0 ? field->xres : 0);
        row = (row % field->yres) + (row < 0 ? field->yres : 0);
    }
    else if (exterior == GWY_EXTERIOR_MIRROR_EXTEND) {
        guint xres2 = 2*field->xres, yres2 = 2*field->yres;
        col = (col % xres2) + (col < 0 ? xres2 : 0);
        if ((guint)col >= field->xres)
            col = xres2-1 - col;
        row = (row % yres2) + (row < 0 ? yres2 : 0);
        if ((guint)row >= field->yres)
            row = yres2-1 - row;
    }
    else {
        g_return_val_if_reached(NAN);
    }
    return field->data[col + row*field->xres];
}

static gboolean
exterior_mask(const GwyMaskField *field,
              gboolean invert,
              gint col, gint row,
              GwyExteriorType exterior)
{
    // No masking
    if (!field)
        return 1;

    // Interior
    if (col >= 0 && (guint)col < field->xres
        && row >= 0 && (guint)row < field->yres)
        return !gwy_mask_field_get(field, col, row) == invert;

    // Exteriors
    if (exterior == GWY_EXTERIOR_UNDEFINED)
        return 0;

    if (exterior == GWY_EXTERIOR_FIXED_VALUE)
        return 1;

    if (exterior == GWY_EXTERIOR_BORDER_EXTEND) {
        col = CLAMP(col, 0, (gint)field->xres-1);
        row = CLAMP(row, 0, (gint)field->yres-1);
    }
    else if (exterior == GWY_EXTERIOR_PERIODIC) {
        col = (col % field->xres) + (col < 0 ? field->xres : 0);
        row = (row % field->yres) + (row < 0 ? field->yres : 0);
    }
    else if (exterior == GWY_EXTERIOR_MIRROR_EXTEND) {
        guint xres2 = 2*field->xres, yres2 = 2*field->yres;
        col = (col % xres2) + (col < 0 ? xres2 : 0);
        if ((guint)col >= field->xres)
            col = xres2-1 - col;
        row = (row % yres2) + (row < 0 ? yres2 : 0);
        if ((guint)row >= field->yres)
            row = yres2-1 - row;
    }
    else {
        g_return_val_if_reached(0);
    }
    return !gwy_mask_field_get(field, col, row) == invert;
}

/**
 * gwy_field_value:
 * @field: A two-dimensional data field.
 * @col: Column index.
 * @row: Row index.
 * @exterior: Exterior pixels handling.
 * @fill_value: The value to use with %GWY_EXTERIOR_FIXED_VALUE exterior.
 *
 * Reads a single value in a field.
 *
 * The indices @col and @row may correspond to a position outside the field
 * data.  In such case the value is determined by the specified exterior type.
 *
 * Note this method is quite slow compared to direct access of @field data.
 * If you need to read more than a handful of pixels consider processing the
 * data directly, possibly after application of gwy_field_extend().
 *
 * Returns: Field value at position (@col,@row), possibly extrapolated.
 **/
gdouble
gwy_field_value(const GwyField *field,
                gint col, gint row,
                GwyExteriorType exterior,
                gdouble fill_value)
{
    g_return_val_if_fail(GWY_IS_FIELD(field), NAN);
    return exterior_value(field, col, row, exterior, fill_value);
}

/**
 * gwy_field_value_interpolated:
 * @field: A two-dimensional data field containing interpolation coefficients.
 * @x: Horizontal coordinate, pixel-centered.  This means @x=0.5 corresponds to
 *     @col=0 in gwy_field_value().
 * @y: Vertical coordinate, pixel-centered.  This means @y=0.5 corresponds to
 *     @row=0 in gwy_field_value().
 * @interpolation: Interpolation type to use.  If it is a type with
 *                 non-interpolating basis then @field is assumed to contain
 *                 the coefficients (such a field can be created with
 *                 gwy_field_interpolation_coeffs()).
 *                 Using such interpolating directly with the data leads to
 *                 incorrect results.
 * @exterior: Exterior pixels handling.
 * @fill_value: The value to use with %GWY_EXTERIOR_FIXED_VALUE exterior.
 *
 * Reads a single value in a field with interpolation.
 *
 * The coordinates @x and @y may correspond to a position outside the field
 * data.  In such case the value is determined by the specified exterior type
 * (with interpolation still applied).  Since interpolation uses mirroring
 * specifying exterior other than %GWY_EXTERIOR_MIRROR_EXTEND is somewhat
 * inconsistent.
 *
 * Returns: Field value at position (@x,@y), interpolated and possibly
 *          extrapolated.
 **/
gdouble
gwy_field_value_interpolated(const GwyField *field,
                             gdouble x, gdouble y,
                             GwyInterpolationType interpolation,
                             GwyExteriorType exterior,
                             gdouble fill_value)
{
    g_return_val_if_fail(GWY_IS_FIELD(field), NAN);

    gint suplen = gwy_interpolation_get_support_size(interpolation);
    gint sf = -(suplen - 1)/2;
    g_return_val_if_fail(suplen > 0, NAN);

    x -= 0.5;
    y -= 0.5;
    gint col = (gint)floor(x), row = (gint)floor(y);
    x -= col;
    y -= row;

    gdouble *coeff = g_slice_alloc(suplen*suplen*sizeof(gdouble));
    for (gint i = 0; i < suplen; i++) {
        for (gint j = 0; j < suplen; j++)
            coeff[i*suplen + j] = exterior_value(field,
                                                 col + j + sf, row + i + sf,
                                                 exterior, fill_value);
    }
    gdouble retval = gwy_interpolate_2d(x, y, suplen, coeff, interpolation);
    g_slice_free1(suplen*suplen*sizeof(gdouble), coeff);

    return retval;
}

/**
 * gwy_field_value_averaged:
 * @field: A two-dimensional data field.
 * @mask: (allow-none):
 *        Mask specifying which values to take into account/exclude, or %NULL.
 *        Its dimensions must match either the dimensions of @field.
 * @masking: Masking mode to use (has any effect only with non-%NULL @mask).
 * @col: Column index.
 * @row: Row index.
 * @ax: Horizontal averaging radius (half-axis).
 * @ay: Vertical averaging radius (half-axis).
 * @elliptical: %TRUE to process an elliptical area, %FALSE to process its
 *              bounding rectangle.
 * @exterior: Exterior pixels handling.
 * @fill_value: The value to use with %GWY_EXTERIOR_FIXED_VALUE exterior.
 *
 * Reads a single value in a field with averaging.
 *
 * The precise meaning of @ax and @ay is that the elliptical averaging area
 * consist of pixels with centres lying within the ellpise of half-axes @ax+½
 * and @ay+½ centered on the (@col,@row) pixel.  Consequently, the rectangular
 * area has sides 2@ax+1 and 2@ay+1.  Furthermore, passing zero for either
 * @ax and @ay means no averaging in the corresponding direction.
 *
 * Returns: Field value at position (@col,@row), possibly extrapolated and
 *          averaged.
 **/
gdouble
gwy_field_value_averaged(const GwyField *field,
                         const GwyMaskField *mask,
                         GwyMaskingType masking,
                         gint col, gint row,
                         guint ax, guint ay,
                         gboolean elliptical,
                         GwyExteriorType exterior,
                         gdouble fill_value)
{
    g_return_val_if_fail(GWY_IS_FIELD(field), NAN);
    if (!mask || masking == GWY_MASK_IGNORE) {
        mask = NULL;
        masking = GWY_MASK_IGNORE;
    }
    else
        g_return_val_if_fail(GWY_IS_MASK_FIELD(mask), NAN);

    gboolean invert = (masking == GWY_MASK_EXCLUDE);
    gdouble rx = ax + 0.5, ry = ay + 0.5;
    gdouble sz = 0.0;
    guint n = 0;

    for (gint i = -(gint)ay; i <= (gint)ay; i++) {
        gint xlen = elliptical ? elliptical_xlen(i/ry, rx, ax) : 0;
        for (gint j = -(gint)ax + xlen; j <= (gint)ax - xlen; j++) {
            if (exterior_mask(mask, invert, j + col, i + row, exterior)) {
                sz += exterior_value(field, j + col, i + row,
                                     exterior, fill_value);
                n++;
            }
        }
    }

    return sz/n;
}

/**
 * gwy_field_slope:
 * @field: A two-dimensional data field.
 * @col: Column index.
 * @row: Row index.
 * @ax: Horizontal averaging radius (half-axis).
 * @ay: Vertical averaging radius (half-axis).
 * @elliptical: %TRUE to process an elliptical area, %FALSE to process its
 *              bounding rectangle.
 * @exterior: Exterior pixels handling.
 * @fill_value: The value to use with %GWY_EXTERIOR_FIXED_VALUE exterior.
 * @a: (out) (allow-none):
 *     Location to store the local mean value to.
 * @bx: (out) (allow-none):
 *      Location to store the horizontal gradient to.
 * @by: (out) (allow-none):
 *      Location to store the vertical gradient to.
 *
 * Calculates local slope in a field.
 *
 * The calculated gradients are in physical units, i.e. they represent changes
 * per one lateral unit, not per pixel.
 *
 * See gwy_field_value_averaged() for description of the neighbourhood shape
 * and size.  The slope in the direction of zero averaging radius is considered
 * to be identically zero.
 **/
void
gwy_field_slope(const GwyField *field,
                gint col, gint row,
                guint ax, guint ay,
                gboolean elliptical,
                GwyExteriorType exterior,
                gdouble fill_value,
                gdouble *a,
                gdouble *bx, gdouble *by)
{
    g_return_if_fail(GWY_IS_FIELD(field));
    gdouble sz = 0.0, sxz = 0.0, syz = 0.0, sx2 = 0.0, sy2 = 0.0;
    guint n = 0;

    if (elliptical) {
        gdouble rx = ax + 0.5, ry = ay + 0.5;
        for (gint i = -(gint)ay; i <= (gint)ay; i++) {
            gdouble eta = i/ry;
            gint xlen = ax - gwy_round(rx*sqrt(1.0 - eta*eta) - 0.5);
            g_assert(2*(guint)xlen <= 2*ax + 1);
            for (gint j = -(gint)ax + xlen; j <= (gint)ax - xlen; j++) {
                gdouble z = exterior_value(field, j + col, i + row,
                                           exterior, fill_value);
                sz += z;
                sxz += j*z;
                syz += i*z;
            }
            guint d = (2*ax + 1) - 2*xlen;
            n += d;
            sx2 += 2*gwy_power_sum(ax - xlen, 2);
            sy2 += d*i*i;
        }
    }
    else {
        for (gint i = -(gint)ay; i <= (gint)ay; i++) {
            for (gint j = -(gint)ax; j <= (gint)ax; j++) {
                gdouble z = exterior_value(field, j + col, i + row,
                                           exterior, fill_value);
                sz += z;
                sxz += j*z;
                syz += i*z;
            }
        }
        n = (2*ax + 1)*(2*ay + 1);
        sx2 = (2*ay + 1)*2*gwy_power_sum(ax, 2);
        sy2 = (2*ax + 1)*2*gwy_power_sum(ay, 2);
    }

    GWY_MAYBE_SET(a, sz/n);
    GWY_MAYBE_SET(bx, ax ? sxz/sx2/gwy_field_dx(field) : 0.0);
    GWY_MAYBE_SET(by, ay ? syz/sy2/gwy_field_dy(field) : 0.0);
}

/**
 * gwy_field_curvature:
 * @field: A two-dimensional data field.
 * @col: Column index.
 * @row: Row index.
 * @ax: Horizontal averaging radius (half-axis).
 * @ay: Vertical averaging radius (half-axis).
 * @elliptical: %TRUE to process an elliptical area, %FALSE to process its
 *              bounding rectangle.
 * @exterior: Exterior pixels handling.
 * @fill_value: The value to use with %GWY_EXTERIOR_FIXED_VALUE exterior.
 * @curvature: (out):
 *             Location to store the curvature parameters.
 *
 * Calculates local curvature in a field.
 *
 * This makes sense only if the field values are the same physical quantity
 * as the lateral dimensions.
 *
 * All calculated curvatures are in physical units.  See gwy_math_curvature()
 * for handling of degenerate cases; the natural centre is at physical
 * coordinates corresponding to (@col+½,@row+½) in this case.
 *
 * See gwy_field_value_averaged() for description of the neighbourhood shape
 * and size.  The curvature in the direction of zero averaging radius is
 * considered to be identically zero.
 *
 * Returns: The number of curved dimensions, simiarly to gwy_math_curvature().
 **/
guint
gwy_field_curvature(const GwyField *field,
                    gint col, gint row,
                    guint ax, guint ay,
                    gboolean elliptical,
                    GwyExteriorType exterior,
                    gdouble fill_value,
                    GwyCurvatureParams *curvature)
{
    g_return_val_if_fail(GWY_IS_FIELD(field), 0);
    g_return_val_if_fail(curvature, 0);

    gdouble sz = 0.0, sxz = 0.0, syz = 0.0, sxxz = 0.0, sxyz = 0.0, syyz = 0.0,
            sx2 = 0.0, sy2 = 0.0, sx4 = 0.0, sx2y2 = 0.0, sy4 = 0.0;
    guint n = 0;

    if (elliptical) {
        gdouble rx = ax + 0.5, ry = ay + 0.5;
        for (gint i = -(gint)ay; i <= (gint)ay; i++) {
            gdouble eta = i/ry;
            gint xlen = ax - gwy_round(rx*sqrt(1.0 - eta*eta) - 0.5);
            g_assert(2*(guint)xlen <= 2*ax + 1);
            for (gint j = -(gint)ax + xlen; j <= (gint)ax - xlen; j++) {
                gdouble z = exterior_value(field, j + col, i + row,
                                           exterior, fill_value);
                sz += z;
                sxz += j*z;
                syz += i*z;
                sxxz += j*j*z;
                sxyz += j*i*z;
                syyz += i*i*z;
            }
            guint d = (2*ax + 1) - 2*xlen;
            gdouble i2 = i*i;
            n += d;
            sx2 += 2*gwy_power_sum(ax - xlen, 2);
            sy2 += d*i2;
            sx4 += 2*gwy_power_sum(ax - xlen, 4);
            sx2y2 += 2*i2*gwy_power_sum(ax - xlen, 2);
            sy4 += d*i2*i2;
        }
    }
    else {
        for (gint i = -(gint)ay; i <= (gint)ay; i++) {
            for (gint j = -(gint)ax; j <= (gint)ax; j++) {
                gdouble z = exterior_value(field, j + col, i + row,
                                           exterior, fill_value);
                sz += z;
                sxz += j*z;
                syz += i*z;
                sxxz += j*j*z;
                sxyz += j*i*z;
                syyz += i*i*z;
            }
        }
        n = (2*ax + 1)*(2*ay + 1);
        sx2 = (2*ay + 1)*2*gwy_power_sum(ax, 2);
        sy2 = (2*ax + 1)*2*gwy_power_sum(ay, 2);
        sx4 = (2*ay + 1)*2*gwy_power_sum(ax, 4);
        sx2y2 = 4*gwy_power_sum(ax, 2)*gwy_power_sum(ay, 2);
        sy4 = (2*ax + 1)*2*gwy_power_sum(ay, 4);
    }

    gdouble alpha = sx4*sy4 - sx2y2*sx2y2,
            betax = sx2y2*sy2 - sx2*sy4,
            betay = sx2y2*sx2 - sx4*sy2,
            delta = sx2*sy2 - n*sx2y2;

    gdouble D = n*alpha + sx2*betax + sy2*betay,
            Da = sz*alpha + sxxz*betax + syyz*betay,
            Dxx = sxxz*(n*sy4 - sy2*sy2) + syyz*delta + sz*(sx2y2*sy2 - sx2*sy4),
            Dyy = sxxz*delta + syyz*(n*sx4 - sx2*sx2) + sz*(sx2*sx2y2 - sx4*sy2);
    // s is used for transformation to coordinates with correct aspect ratio
    // and pixel area of 1; q is then the remaining scaling factor.
    gdouble s = sqrt(gwy_field_dy(field)*gwy_field_dx(field));
    gdouble q = sqrt(gwy_field_dy(field)/gwy_field_dx(field));
    gdouble coeffs[6];

    if (ax == 0 && ay == 0) {
        gwy_clear(coeffs, G_N_ELEMENTS(coeffs));
        coeffs[0] = sz/n;
    }
    else if (ax == 0) {
        D = n*sy4 - sy2*sy2;
        coeffs[1] = coeffs[3] = coeffs[4] = 0.0;
        coeffs[2] = syz/sy2;
        coeffs[0] = (sz*sy4 - sy2*syyz)/D;
        coeffs[5] = (n*syyz - sz*sy2)/D;
    }
    else if (ay == 0) {
        D = n*sx4 - sx2*sx2;
        coeffs[2] = coeffs[4] = coeffs[5] = 0.0;
        coeffs[1] = sxz/sx2;
        coeffs[0] = (sz*sx4 - sx2*sxxz)/D;
        coeffs[3] = (n*sxxz - sz*sx2)/D;
    }
    else {
        coeffs[0] = Da/D;                // 1
        coeffs[1] = sxz/sx2    * q;      // x
        coeffs[2] = syz/sy2    / q;      // y
        coeffs[3] = Dxx/D      * q*q;    // x²
        coeffs[4] = sxyz/sx2y2;          // xy
        coeffs[5] = Dyy/D      / (q*q);  // y²
    }

    guint ndims = gwy_math_curvature(coeffs, curvature);
    // Now the angles and z-values are correct, but curvatures and xy must be
    // transformed to real physical units.
    curvature->k1 /= s*s;
    curvature->k2 /= s*s;
    curvature->xc *= s;
    curvature->xc += field->xoff + (col + 0.5)*gwy_field_dx(field);
    curvature->yc *= s;
    curvature->yc += field->yoff + (row + 0.5)*gwy_field_dy(field);

    return ndims;
}

/**
 * gwy_field_interpolation_coeffs:
 * @field: A two-dimensional data field.
 * @interpolation: Interpolation type to use.
 *
 * Resolves interpolation coefficients for a field.
 *
 * If the interpolation has an interpolating basis @field itself is returned,
 * with increased reference count.  Otherwise a new field is created and
 * returned.  This means in both cases you have to release the returned field
 * using g_object_unref().
 *
 * Returns: A field containing interpolation coefficients for @field.
 **/
GwyField*
gwy_field_interpolation_coeffs(GwyField *field,
                               GwyInterpolationType interpolation)
{
    if (gwy_interpolation_has_interpolating_basis(interpolation))
        return g_object_ref(field);

    GwyField *result = gwy_field_duplicate(field);
    gwy_interpolation_resolve_coeffs_2d(result->xres, result->yres,
                                        result->xres, result->data,
                                        interpolation);
    return result;
}

/**
 * SECTION: field-read
 * @section_id: GwyField-read
 * @title: GwyField data reading
 * @short_description: Reading and extraction of fields values
 *
 * Functions that gather information from the neighbourhood of the selected
 * point, such as gwy_field_value_averaged(), can apply masking.  Note if this
 * causes the entire neighbourhood to be excluded they can return NaNs.
 * Furthermore, if the neighbourhood sticks out of the field the exterior must
 * be handled not only for the field but also for the mask.  Exterior types
 * that replicate the field data somehow, i.e. %GWY_EXTERIOR_BORDER_EXTEND,
 * %GWY_EXTERIOR_MIRROR_EXTEND and %GWY_EXTERIOR_PERIODIC, are applied to the
 * mask to replicate its data in the same fashion.  However,
 * %GWY_EXTERIOR_UNDEFINED causes all exterior pixels to be ignored while
 * %GWY_EXTERIOR_FIXED_VALUE causes all exterior pixels to be included.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
