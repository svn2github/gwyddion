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
 *     @col=0 in gwy_field_read().
 * @y: Vertical coordinate, pixel-centered.  This means @y=0.5 corresponds to
 *     @row=0 in gwy_field_read().
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
                        gint col, gint row,
                        guint ax, guint ay,
                        gboolean elliptical,
                        GwyExteriorType exterior,
                        gdouble fill_value)
{
    if (!ax && !ay)
        return gwy_field_value(field, col, row, exterior, fill_value);

    g_return_val_if_fail(GWY_IS_FIELD(field), NAN);
    gdouble sz = 0.0;
    guint n = 0;

    if (elliptical) {
        // We could use Bresenham but we need to fill the ellipse so it would
        // complicate things.  One floating point calculation per row is fine.
        gdouble rx = 0.5*(ax + 1), ry = 0.5*(ay + 1);
        for (gint i = -(gint)ay; i <= (gint)ay; i++) {
            gdouble eta = (i + 0.5)/ry;
            gint xlen = gwy_round(rx*(1.0 - sqrt(eta*fmax(2.0 - eta, 0.0))));
            g_assert(2*(guint)xlen <= 2*ax + 1);
            for (gint j = -(gint)ax + xlen; j <= (gint)ax - xlen; j++) {
                sz += exterior_value(field, j + col, i + row,
                                    exterior, fill_value);
            }
            n += (2*ax + 1) - 2*xlen;
        }
    }
    else {
        for (gint i = -(gint)ay; i <= (gint)ay; i++) {
            for (gint j = -(gint)ax; j <= (gint)ax; j++) {
                sz += exterior_value(field, j + col, i + row,
                                    exterior, fill_value);
            }
        }
        n = (2*ax + 1)*(2*ay + 1);
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
 * and size.  The slope in the direction of zero averaging radius is
 * identically zero.
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
        // We could use Bresenham but we need to fill the ellipse so it would
        // complicate things.  One floating point calculation per row is fine.
        gdouble rx = 0.5*(ax + 1), ry = 0.5*(ay + 1);
        for (gint i = -(gint)ay; i <= (gint)ay; i++) {
            gdouble eta = (i + 0.5)/ry;
            gint xlen = gwy_round(rx*(1.0 - sqrt(eta*fmax(2.0 - eta, 0.0))));
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
 *
 * Calculates local curvature in a field.
 *
 * This makes sense only if the field values are the same physical quantity
 * as the lateral dimensions.
 *
 * All calculated curvatures are in physical units.  See gwy_math_curvature()
 * for handling of degenerate cases; the antural centre is at physical
 * coordinates corresponding to (@col+½,@row+½) in this case.
 *
 * See gwy_field_value_averaged() for description of the neighbourhood shape
 * and size.  The curvature in the direction of zero averaging radius is
 * identically zero.
 **/
void
gwy_field_curvature(const GwyField *field,
                    gint col, gint row,
                    guint ax, guint ay,
                    gboolean elliptical,
                    GwyExteriorType exterior,
                    gdouble fill_value,
                    gdouble *kappa1, gdouble *kappa2,
                    gdouble *phi1, gdouble *phi2,
                    gdouble *xc, gdouble *yc, gdouble *zc)
{
    g_return_if_fail(GWY_IS_FIELD(field));
    gdouble sz = 0.0, sxz = 0.0, syz = 0.0, sxxz = 0.0, sxyz = 0.0, syyz = 0.0,
            sx2 = 0.0, sy2 = 0.0, sx4 = 0.0, sx2y2 = 0.0, sy4 = 0.0;
    guint n = 0;

    if (elliptical) {
        // We could use Bresenham but we need to fill the ellipse so it would
        // complicate things.  One floating point calculation per row is fine.
        gdouble rx = 0.5*(ax + 1), ry = 0.5*(ay + 1);
        for (gint i = -(gint)ay; i <= (gint)ay; i++) {
            gdouble eta = (i + 0.5)/ry;
            gint xlen = gwy_round(rx*(1.0 - sqrt(eta*fmax(2.0 - eta, 0.0))));
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
            n += d;
            sx2 += 2*gwy_power_sum(ax - xlen, 2);
            sy2 += d*i*i;
            sx4 += 2*gwy_power_sum(ax - xlen, 4);
            sx2y2 = 2*i*i*gwy_power_sum(ax - xlen, 2);
            sy4 += d*i*i*i*i;
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

    gdouble coeffs[6];

    gdouble alpha = sx4*sx4 - sx2y2*sx2y2,
            betax = sx2y2*sy2 - sx2*sy4,
            betay = sx2y2*sx2 - sx4*sy2,
            delta = sx2*sy2 - sx2y2;

    gdouble D = alpha + sx2*betax + sy2*betay,
            Da = sz*alpha + sxxz*betax + syyz*betay,
            Dxx = sxxz*(sy4 - sy2*sy2) + syyz*delta + sz*(sx2y2*sy2 - sx2*sy4),
            Dyy = sxxz*delta + syyz*(sx4 - sx2*sx2) + sz*(sx2*sx2y2 - sx4*sy2);

    // TODO: Handle degenerate cases: sx2 == 0, sy2 == 0, sx2sy2 == 0 and D == 0
    coeffs[0] = Da/D;         // 1
    coeffs[1] = sxz/sx2;      // x
    coeffs[2] = syz/sy2;      // y
    coeffs[3] = Dxx/D;        // x²
    coeffs[4] = sxyz/sx2y2;   // xy
    coeffs[5] = Dyy/D;        // y²
    // TODO: Transform to coordinates with the same aspect ratio as physical
    // but with lateral dimensions ≈ 1 – see Gwyddion 2.x.
    // TODO: Calculate the curvature using gwy_math_curvature().
    // TODO: Transform the curvature parameters to physical units.
    // TODO: Set the results
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
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
