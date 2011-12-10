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
#include "libgwy/object-internal.h"
#include "libgwy/curve-internal.h"
#include "libgwy/field-internal.h"

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
        col = (col % (gint)field->xres);
        if (col < 0)
            col += field->xres;
        row = (row % (gint)field->yres);
        if (row < 0)
            row += field->yres;
    }
    else if (exterior == GWY_EXTERIOR_MIRROR_EXTEND) {
        gint xres2 = 2*field->xres, yres2 = 2*field->yres;
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
        col = (col % (gint)field->xres);
        if (col < 0)
            col += field->xres;
        row = (row % (gint)field->yres);
        if (row < 0)
            row += field->yres;
    }
    else if (exterior == GWY_EXTERIOR_MIRROR_EXTEND) {
        gint xres2 = 2*field->xres, yres2 = 2*field->yres;
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

static guint
local_centre(const GwyField *field,
             const GwyMaskField *mask,
             GwyMaskingType masking,
             gint col, gint row,
             gint ax, gint ay,
             gboolean elliptical,
             GwyExteriorType exterior,
             gdouble fill_value,
             gdouble *xmean, gdouble *ymean, gdouble *zmean)
{
    gboolean invert = (masking == GWY_MASK_EXCLUDE);
    gdouble rx = ax + 0.5, ry = ay + 0.5;
    gdouble sx = 0.0, sy = 0.0, sz = 0.0;
    guint n = 0;

    for (gint i = -ay; i <= ay; i++) {
        gint xlen = elliptical ? elliptical_xlen(i/ry, rx, ax) : 0;
        for (gint j = -ax + xlen; j <= ax - xlen; j++) {
            if (exterior_mask(mask, invert, j + col, i + row, exterior)) {
                sx += j;
                sy += i;
                sz += exterior_value(field, j + col, i + row,
                                     exterior, fill_value);
                n++;
            }
        }
    }

    *xmean = sx/n;
    *ymean = sy/n;
    *zmean = sz/n;

    return n;
}

/**
 * gwy_field_value_averaged:
 * @field: A two-dimensional data field.
 * @mask: (allow-none):
 *        Mask specifying which values to take into account/exclude, or %NULL.
 *        Its dimensions must match the dimensions of @field.
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
 *          averaged.  The returned value may be NaN if the neighbourhood is
 *          empty.
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
 * @mask: (allow-none):
 *        Mask specifying which values to take into account/exclude, or %NULL.
 *        Its dimensions must match the dimensions of @field.
 * @masking: Masking mode to use (has any effect only with non-%NULL @mask).
 * @col: Column index.
 * @row: Row index.
 * @ax: Horizontal averaging radius (half-axis).  It must be at least 1 to
 *      actually obtain any slope information.
 * @ay: Vertical averaging radius (half-axis).  It must be at least 1 to
 *      actually obtain any slope information.
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
 * and size.  If the neighbourhood contains an insufficient number of pixels
 * to determine the slope (taking masking into account) the coefficients are
 * set to 0 and %FALSE is returned.  If the neighbourhood is completely empty
 * @a is set to NaN.
 *
 * Returns: %TRUE if the neighbourhood was sufficient to determine the slopes,
 *          %FALSE otherwise.
 **/
gboolean
gwy_field_slope(const GwyField *field,
                const GwyMaskField *mask,
                GwyMaskingType masking,
                gint col, gint row,
                guint ax, guint ay,
                gboolean elliptical,
                GwyExteriorType exterior,
                gdouble fill_value,
                gdouble *a,
                gdouble *bx, gdouble *by)
{
    g_return_val_if_fail(GWY_IS_FIELD(field), FALSE);
    if (!mask || masking == GWY_MASK_IGNORE) {
        mask = NULL;
        masking = GWY_MASK_IGNORE;
    }
    else
        g_return_val_if_fail(GWY_IS_MASK_FIELD(mask), FALSE);

    gboolean invert = (masking == GWY_MASK_EXCLUDE);
    gdouble rx = ax + 0.5, ry = ay + 0.5;
    gdouble sxy = 0.0, sxz = 0.0, syz = 0.0, sxx = 0.0, syy = 0.0;
    gdouble xmean, ymean, zmean;
    guint n = local_centre(field, mask, masking,
                           col, row, ax, ay, elliptical,
                           exterior, fill_value,
                           &xmean, &ymean, &zmean);

    GWY_MAYBE_SET(a, zmean);
    GWY_MAYBE_SET(bx, 0.0);
    GWY_MAYBE_SET(by, 0.0);
    if (n < 2)
        return FALSE;

    for (gint i = -(gint)ay; i <= (gint)ay; i++) {
        gint xlen = elliptical ? elliptical_xlen(i/ry, rx, ax) : 0;
        gdouble y = i - ymean;
        gdouble yy = y*y;
        for (gint j = -(gint)ax + xlen; j <= (gint)ax - xlen; j++) {
            if (exterior_mask(mask, invert, j + col, i + row, exterior)) {
                gdouble z = exterior_value(field, j + col, i + row,
                                           exterior, fill_value) - zmean;
                gdouble x = j - xmean;
                gdouble xx = x*x;
                sxz += x*z;
                syz += y*z;
                sxy += x*y;
                sxx += xx;
                syy += yy;
            }
        }
    }

    // Both sxx and syy can be zero only if n < 2.
    gdouble dx = gwy_field_dx(field), dy = gwy_field_dy(field);
    gdouble D = sxx*syy - sxy*sxy;

    if (fabs(D) < 1e-10*fmax(fmax(sxx*syy, sxy*sxy), 1.0))
        return FALSE;

    GWY_MAYBE_SET(bx, (sxz*syy - sxy*syz)/(dx*D));
    GWY_MAYBE_SET(by, (syz*sxx - sxy*sxz)/(dy*D));
    return TRUE;
}

/**
 * gwy_field_curvature:
 * @field: A two-dimensional data field.
 * @mask: (allow-none):
 *        Mask specifying which values to take into account/exclude, or %NULL.
 *        Its dimensions must match the dimensions of @field.
 * @masking: Masking mode to use (has any effect only with non-%NULL @mask).
 * @col: Column index.
 * @row: Row index.
 * @ax: Horizontal averaging radius (half-axis).  It must be at least 1 to
 *      actually obtain any slope information for rectangular neighbourhoods
 *      and larger than 1 for elliptical areas.
 * @ay: Vertical averaging radius (half-axis).  It must be at least 1 to
 *      actually obtain any slope information for rectangular neighbourhoods
 *      and larger than 1 for elliptical areas.
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
 * and size.  If the neighbourhood contains an insufficient number of pixels to
 * determine the slope (taking masking into account) the parameters are set to
 * values corresponding to a flat surface and a negative value is returned.
 *
 * Returns: The number of curved dimensions, simiarly to gwy_math_curvature().
 *          If the the 
 **/
gint
gwy_field_curvature(const GwyField *field,
                    const GwyMaskField *mask,
                    GwyMaskingType masking,
                    gint col, gint row,
                    guint ax, guint ay,
                    gboolean elliptical,
                    GwyExteriorType exterior,
                    gdouble fill_value,
                    GwyCurvatureParams *curvature)
{
    g_return_val_if_fail(GWY_IS_FIELD(field), -1);
    g_return_val_if_fail(curvature, -1);
    if (!mask || masking == GWY_MASK_IGNORE) {
        mask = NULL;
        masking = GWY_MASK_IGNORE;
    }
    else
        g_return_val_if_fail(GWY_IS_MASK_FIELD(mask), -1);

    gboolean invert = (masking == GWY_MASK_EXCLUDE);
    gdouble rx = ax + 0.5, ry = ay + 0.5;
    // Do not use local_centre() here, that would move the natural origin from
    // (col, row) to the local centre.
    gdouble sx = 0.0, sy = 0.0,
            sxx = 0.0, sxy = 0.0, syy = 0.0,
            sxxx = 0.0, sxxy = 0.0, sxyy = 0.0, syyy = 0.0,
            sxxxx = 0.0, sxxxy = 0.0, sxxyy = 0.0, sxyyy = 0.0, syyyy = 0.0,
            sz = 0.0, sxz = 0.0, syz = 0.0, sxxz = 0.0, sxyz = 0.0, syyz = 0.0;
    guint n = 0;

    for (gint i = -(gint)ay; i <= (gint)ay; i++) {
        gint xlen = elliptical ? elliptical_xlen(i/ry, rx, ax) : 0;
        gdouble y = i;
        gdouble yy = y*y;
        for (gint j = -(gint)ax + xlen; j <= (gint)ax - xlen; j++) {
            if (exterior_mask(mask, invert, j + col, i + row, exterior)) {
                gdouble z = exterior_value(field, j + col, i + row,
                                           exterior, fill_value);
                gdouble x = j;
                gdouble xx = x*x;
                gdouble xy = x*y;
                sx += x;
                sy += y;
                sxx += xx;
                sxy += xy;
                syy += yy;
                sxxx += xx*x;
                sxxy += xx*y;
                sxyy += x*yy;
                syyy += y*yy;
                sxxxx += xx*xx;
                sxxxy += xx*xy;
                sxxyy += xx*yy;
                sxyyy += xy*yy;
                syyyy += yy*yy;
                sz += z;
                sxz += x*z;
                syz += y*z;
                sxxz += xx*z;
                sxyz += xy*z;
                syyz += yy*z;
                n++;
            }
        }
    }

    // q is used for transformation from square pixels to coordinates with
    // correct aspect ratio and pixel area of 1; s is then the remaining
    // uniform scaling factor.
    gdouble s = sqrt(gwy_field_dy(field)*gwy_field_dx(field));
    gdouble q = sqrt(gwy_field_dy(field)/gwy_field_dx(field));
    gdouble coeffs[6] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
    gint ok = -1;

    if (n) {
        gdouble matrix[21];
        matrix[0] = n;
        matrix[1] = sx;
        matrix[2] = matrix[6] = sxx;
        matrix[3] = sy;
        matrix[4] = matrix[10] = sxy;
        matrix[5] = matrix[15] = syy;
        matrix[7] = sxxx;
        matrix[8] = matrix[11] = sxxy;
        matrix[9] = sxxxx;
        matrix[12] = matrix[16] = sxyy;
        matrix[13] = sxxxy;
        matrix[14] = matrix[18] = sxxyy;
        matrix[17] = syyy;
        matrix[19] = sxyyy;
        matrix[20] = syyyy;
        coeffs[0] = sz;
        coeffs[1] = sxz;
        coeffs[2] = syz;
        coeffs[3] = sxxz;
        coeffs[4] = sxyz;
        coeffs[5] = syyz;
        if (gwy_cholesky_decompose(matrix, 6)) {
            gwy_cholesky_solve(matrix, coeffs, 6);
            coeffs[1] *= q;
            coeffs[2] /= q;
            coeffs[3] *= q*q;
            coeffs[5] /= q*q;
            ok = G_MAXINT;
        }
        else
            gwy_clear(coeffs, 6);
    }

    // gwy_math_curvature() does The Right Thing if coeffs are all zeroes.
    gint ndims = gwy_math_curvature(coeffs, curvature);
    // Now the angles and z-values are correct, but curvatures and xy must be
    // transformed to real physical units.
    curvature->k1 /= s*s;
    curvature->k2 /= s*s;
    curvature->xc *= s;
    curvature->xc += field->xoff + (col + 0.5)*gwy_field_dx(field);
    curvature->yc *= s;
    curvature->yc += field->yoff + (row + 0.5)*gwy_field_dy(field);

    return MIN(ndims, ok);
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
 * Returns: (transfer full):
 *          A field containing interpolation coefficients for @field.
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
 * gwy_field_profile:
 * @field: A two-dimensional data field.
 * @xfrom: Horizontal coordinate of the profile start, pixel-centered.
 * @yfrom: Vertical coordinate of the profile start, pixel-centered.
 * @xto: Horizontal coordinate of the profile end, pixel-centered.
 * @yto: Vertical coordinate of the profile end, pixel-centered.
 * @res: Number of samples taken on the line.  If zero it is chosen to make
 *       the sample distance as close to the size of one @field pixel as
 *       possible.
 * @thickness: Profile thickness, i.e. distance in direction orthogonal to the
 *             profile direction on each side to average over, taking
 *             @averaging points.  Zero means no averaging.
 * @averaging: The number of extra samples to take on each side when averaging.
 *             It is meaningful only with non-zero @thickness.
 * @interpolation: Interpolation type to use.  If it is a type with
 *                 non-interpolating basis then @field is assumed to contain
 *                 the coefficients (such a field can be created with
 *                 gwy_field_interpolation_coeffs()).
 *                 Using such interpolating directly with the data leads to
 *                 incorrect results.
 * @exterior: Exterior pixels handling.
 * @fill_value: The value to use with %GWY_EXTERIOR_FIXED_VALUE exterior.
 *
 * Extracts a profile from a field.
 *
 * See gwy_field_value_interpolated() for a discussion of precise meaning of
 * the coordinates arguments.
 *
 * Returns: (transfer full):
 *          A newly created curve.
 **/
GwyCurve*
gwy_field_profile(GwyField *field,
                  gdouble xfrom, gdouble yfrom,
                  gdouble xto, gdouble yto,
                  guint res,
                  gdouble thickness,
                  guint averaging,
                  GwyInterpolationType interpolation,
                  GwyExteriorType exterior,
                  gdouble fill_value)
{
    g_return_val_if_fail(GWY_IS_FIELD(field), NULL);
    g_return_val_if_fail(thickness >= 0.0, NULL);

    gdouble sx = xto - xfrom, sy = yto - yfrom;
    gdouble len = hypot(fabs(sx) + 1, fabs(sy) + 1);
    gdouble h = hypot(xto - xfrom, yto - yfrom);
    gdouble hreal = hypot(sx*gwy_field_dx(field), sy*gwy_field_dy(field));
    gdouble cosortho = 0.0, sinortho = 0.0;

    if (res < 1)
        res = gwy_round(fmax(len, 1));
    if (thickness == 0.0)
        averaging = 0;
    if (averaging) {
        // FIXME: Shouldn't we find the orthogonal direction in real
        // coordinates instead of pixel ones?
        if (h) {
            cosortho = sy/h * thickness/averaging;
            sinortho = -sx/h * thickness/averaging;
        }
    }

    GwyCurve *curve = gwy_curve_new_sized(res);

    for (guint n = 0; n < res; n++) {
        gdouble t = (res > 1) ? n/(res - 1.0) : 0.5,
                x = xfrom*(1.0 - t) + xto*t,
                y = yfrom*(1.0 - t) + yto*t;
        gdouble value = gwy_field_value_interpolated(field, x, y,
                                                     interpolation,
                                                     exterior, fill_value);

        for (guint k = 1; k <= averaging; k++) {
            gdouble xt = cosortho*k, yt = sinortho*k;
            value += gwy_field_value_interpolated(field, x + xt, y + yt,
                                                  interpolation,
                                                  exterior, fill_value);
            value += gwy_field_value_interpolated(field, x - xt, y - yt,
                                                  interpolation,
                                                  exterior, fill_value);
        }
        curve->data[n] = (GwyXY){ t*hreal, value/(2*averaging + 1) };
    }

    Field *fpriv = field->priv;
    Curve *cpriv = curve->priv;
    _gwy_assign_units(&cpriv->unit_x, fpriv->unit_xy);
    _gwy_assign_units(&cpriv->unit_y, fpriv->unit_z);

    return curve;
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
