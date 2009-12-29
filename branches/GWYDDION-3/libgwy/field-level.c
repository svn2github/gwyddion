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
#include "libgwy/field-level.h"
#include "libgwy/libgwy-aliases.h"
#include "libgwy/processing-internal.h"

typedef struct {
    const gdouble *p;
    guint width;
    guint height;
    const gdouble *base;
    guint xres;
    guint yres;
    // Only with masking
    const GwyMaskField *mask;
    GwyMaskFieldIter iter;
    guint maskcol;
    guint maskrow;
    gboolean mode;
    // Only with general polynomial
    guint nterms;
    const guint *xpowers;
    const guint *ypowers;
    const gdouble *xp;
    const gdouble *yp;
} PolyFitData;

static gboolean
plane_fit(guint id,
          gdouble *fvalues,
          gdouble *value,
          gpointer user_data)
{
    PolyFitData *data = (PolyFitData*)user_data;
    guint i = id/data->width;
    guint j = id % data->width;
    if (j == 0)
        data->p = data->base + i*data->xres;
    gdouble x = 2*j/(data->xres - 1.0) - 1.0;
    gdouble y = 2*i/(data->yres - 1.0) - 1.0;
    fvalues[0] = 1.0;
    fvalues[1] = x;
    fvalues[2] = y;
    *value = *data->p;
    data->p++;
    return TRUE;
}

static gboolean
plane_fit_mask(guint id,
               gdouble *fvalues,
               gdouble *value,
               gpointer user_data)
{
    PolyFitData *data = (PolyFitData*)user_data;
    guint i = id/data->width;
    guint j = id % data->width;
    if (j == 0) {
        data->p = data->base + i*data->xres;
        gwy_mask_field_iter_init(data->mask, data->iter,
                                 data->maskcol, data->maskrow+i);
    }
    gboolean ok = (!!gwy_mask_field_iter_get(data->iter) == data->mode);
    if (ok) {
        gdouble x = 2*j/(data->xres - 1.0) - 1.0;
        gdouble y = 2*i/(data->yres - 1.0) - 1.0;
        fvalues[0] = 1.0;
        fvalues[1] = x;
        fvalues[2] = y;
        *value = *data->p;
    }
    gwy_mask_field_iter_next(data->iter);
    data->p++;
    return ok;
}

/**
 * gwy_field_part_fit_plane:
 * @field: A two-dimensional data field.
 * @mask: Mask specifying which values to take into account/exclude, or %NULL.
 * @masking: Masking mode to use (has any effect only with non-%NULL @mask).
 * @col: Column index of the upper-left corner of the rectangle.
 * @row: Row index of the upper-left corner of the rectangle.
 * @width: Rectangle width (number of columns).  It should be at least 2.
 * @height: Rectangle height (number of rows).  It should be at least 2.
 * @a: Location to store the constant coefficient, or %NULL.
 * @bx: Location to store the x coefficient, or %NULL.
 * @by: Location to store the y coefficient, or %NULL.
 *
 * Fits a plane through a rectangular part of a field.
 *
 * The coefficients correspond to coordinates normalized to [-1,1], see the
 * introduction for details.
 *
 * Returns: %TRUE if the plane was fitted, %FALSE if the there were too few
 *          points to fit a plane or there were no points with at least two
 *          different x or y coordinates.  The coefficients @a, @bx and @by are
 *          set in all cases, on failure, they are set to zeroes.
 **/
gboolean
gwy_field_part_fit_plane(const GwyField *field,
                         const GwyMaskField *mask,
                         GwyMaskingType masking,
                         guint col, guint row, guint width, guint height,
                         gdouble *a, gdouble *bx, gdouble *by)
{
    guint maskcol, maskrow;
    if (!_gwy_field_check_mask(field, mask, &masking,
                               col, row, width, height, &maskcol, &maskrow)
        || width < 2 || height < 2)
        goto fail;

    PolyFitData data = {
        .width = width,
        .height = height,
        .base = field->data + row*field->xres + col,
        .xres = field->xres,
        .yres = field->yres,
        .mask = mask,
        .maskcol = maskcol,
        .maskrow = maskrow,
    };
    gdouble params[3];
    gboolean ok;
    if (masking == GWY_MASK_IGNORE) {
        ok = gwy_linear_fit(plane_fit, width*height, params, 3, NULL,
                            &data);
    }
    else {
        data.mode = (masking == GWY_MASK_INCLUDE);
        ok = gwy_linear_fit(plane_fit_mask, width*height, params, 3, NULL,
                            &data);
    }
    if (!ok)
        goto fail;

    GWY_MAYBE_SET(a, params[0]);
    GWY_MAYBE_SET(bx, params[1]);
    GWY_MAYBE_SET(by, params[2]);
    return TRUE;

fail:
    GWY_MAYBE_SET(a, 0.0);
    GWY_MAYBE_SET(bx, 0.0);
    GWY_MAYBE_SET(by, 0.0);
    return FALSE;
}

/**
 * gwy_field_subtract_plane:
 * @field: A two-dimensional data field.
 * @a: Constant coefficient.
 * @bx: X-coefficient.
 * @by: Y-coefficient.
 *
 * Subtracts a plane from a field.
 *
 * The coefficients correspond to coordinates normalized to [-1,1], see the
 * introduction for details.
 **/
void
gwy_field_subtract_plane(GwyField *field,
                         gdouble a, gdouble bx, gdouble by)
{
    g_return_if_fail(GWY_IS_FIELD(field));
    a -= bx + by;
    bx *= (field->xres > 1) ? 2.0/(field->xres - 1) : 0.0;
    by *= (field->yres > 1) ? 2.0/(field->yres - 1) : 0.0;
    gdouble *d = field->data;
    for (guint i = 0; i < field->yres; i++) {
        for (guint j = 0; j < field->xres; j++, d++)
            *d -= a + j*bx + i*by;
    }
    gwy_field_invalidate(field);
}

/**
 * gwy_field_part_inclination:
 * @field: A two-dimensional data field.
 * @mask: Mask specifying which values to take into account/exclude, or %NULL.
 * @masking: Masking mode to use (has any effect only with non-%NULL @mask).
 * @col: Column index of the upper-left corner of the rectangle.
 * @row: Row index of the upper-left corner of the rectangle.
 * @width: Rectangle width (number of columns).  It should be at least 2.
 * @height: Rectangle height (number of rows).  It should be at least 2.
 * @damping: Damping factor determining the range of deviations of local normal
 *           vectors from (0,0,1) that non-negligibly contribute to the result.
 *           It must be positive, a reasonable value may be e.g. 20.
 * @bx: Location to store the x coefficient, or %NULL.
 * @by: Location to store the y coefficient, or %NULL.
 *
 * Fits a plane through a rectangular part of a field by straighenting up
 * facets.
 *
 * The coefficients correspond to coordinates normalized to [-1,1], see the
 * introduction for details.
 *
 * Although @bx and @by have the same meaning as in gwy_field_area_fit_plane()
 * they are calculated differently.  Instead of fitting a plane through the
 * points, local factes are determined from 2×2 areas and averaged, leading to
 * a mean normal to the surface.
 *
 * If @damping was zero (which is not permitted), the normal would correspond
 * to the mean normal to the mean plane and the coefficients found by this
 * method would be equal to those calculated by gwy_field_area_fit_plane().
 *
 * Positive values of @damping suppress large slopes by weighting the normals
 * by a Gaussian function of the slope magnitude.  This means edges and noise
 * do not influence the mean normal much and it is effectively calculated
 * only from relatively flat areas of the surface.
 *
 * Since this method is non-linear, it is often used iteratively until it
 * converges.  If there are several competing directions of normals of flat
 * areas, one of them usually wins and the final normal corresponds
 * approximately to the orientation of these facets.
 *
 * Returns: %TRUE if the plane was fitted, %FALSE in degenerate cases when the
 *          plane is impossible to find. The coefficients @bx and @by are
 *          set in all cases, on failure, they are set to zeroes.
 **/
gboolean
gwy_field_part_inclination(const GwyField *field,
                           const GwyMaskField *mask,
                           GwyMaskingType masking,
                           guint col, guint row, guint width, guint height,
                           gdouble damping,
                           gdouble *bx,
                           gdouble *by)
{
    guint maskcol, maskrow;
    if (!_gwy_field_check_mask(field, mask, &masking,
                               col, row, width, height, &maskcol, &maskrow)
        || width < 2 || height < 2)
        goto fail;

    if (!(damping > 0.0)) {
        g_critical("Damping must be positive.");
        goto fail;
    }

    guint xres = field->xres;
    gdouble dx = gwy_field_dx(field), dy = gwy_field_dy(field);
    const gdouble *base = field->data + row*field->xres + col;

    // First determine the RMS of local slopes.
    // This gives us a scale we work on so the algorithm can adapt to both
    // surfaces with small and large ratio of heights and lateral dimensions.
    gdouble sigma2 = 0.0;
    guint n = 0;
    if (masking == GWY_MASK_IGNORE) {
        for (guint i = 0; i+1 < height; i++) {
            const gdouble *d1 = base + i*xres, *d2 = d1 + xres;
            for (guint j = width-1; j; j--, d1++, d2++) {
                gdouble vx = 0.5*(d1[1] + d2[1] - d1[0] - d2[0])/dx;
                gdouble vy = 0.5*(d2[0] + d2[1] - d1[0] - d1[1])/dy;
                sigma2 += vx*vx + vy*vy;
            }
        }
        n = width*height;
    }
    else {
        GwyMaskFieldIter iter1, iter2;
        const gboolean invert = (masking == GWY_MASK_EXCLUDE);
        for (guint i = 0; i+1 < height; i++) {
            const gdouble *d1 = base + i*xres, *d2 = d1 + xres;
            gwy_mask_field_iter_init(mask, iter1, maskcol, maskrow);
            gwy_mask_field_iter_init(mask, iter2, maskcol, maskrow+1);
            gboolean m2 = !!gwy_mask_field_iter_get(iter1) ^ invert;
            gboolean m4 = !!gwy_mask_field_iter_get(iter2) ^ invert;
            for (guint j = width-1; j; j--, d1++, d2++) {
                gboolean m1 = m2, m3 = m4;
                gwy_mask_field_iter_next(iter1);
                m2 = !!gwy_mask_field_iter_get(iter1) ^ invert;
                gwy_mask_field_iter_next(iter2);
                m4 = !!gwy_mask_field_iter_get(iter2) ^ invert;
                if (m1 & m2 & m3 & m4) {
                    gdouble vx = 0.5*(d1[1] + d2[1] - d1[0] - d2[0])/dx;
                    gdouble vy = 0.5*(d2[0] + d2[1] - d1[0] - d1[1])/dy;
                    sigma2 += vx*vx + vy*vy;
                    n++;
                }
            }
        }
    }

    if (n < 4)
        goto fail;

    sigma2 = sigma2/(n*damping);

    gdouble sumvx = 0.0, sumvy = 0.0, sumvz = 0.0;
    if (masking == GWY_MASK_IGNORE) {
        for (guint i = 0; i+1 < height; i++) {
            const gdouble *d1 = base + i*xres, *d2 = d1 + xres;
            for (guint j = width-1; j; j--, d1++, d2++) {
                gdouble vx = 0.5*(d1[1] + d2[1] - d1[0] - d2[0])/dx;
                gdouble vy = 0.5*(d2[0] + d2[1] - d1[0] - d1[1])/dy;
                gdouble q = exp((vx*vx + vy*vy)/sigma2);
                sumvx += vx/q;
                sumvy += vy/q;
                sumvz += 1.0/q;
            }
        }
    }
    else {
        GwyMaskFieldIter iter1, iter2;
        const gboolean invert = (masking == GWY_MASK_EXCLUDE);
        for (guint i = 0; i+1 < height; i++) {
            const gdouble *d1 = base + i*xres, *d2 = d1 + xres;
            gwy_mask_field_iter_init(mask, iter1, maskcol, maskrow);
            gwy_mask_field_iter_init(mask, iter2, maskcol, maskrow+1);
            gboolean m2 = !!gwy_mask_field_iter_get(iter1) ^ invert;
            gboolean m4 = !!gwy_mask_field_iter_get(iter2) ^ invert;
            for (guint j = width-1; j; j--, d1++, d2++) {
                gboolean m1 = m2, m3 = m4;
                gwy_mask_field_iter_next(iter1);
                m2 = !!gwy_mask_field_iter_get(iter1) ^ invert;
                gwy_mask_field_iter_next(iter2);
                m4 = !!gwy_mask_field_iter_get(iter2) ^ invert;
                if (m1 & m2 & m3 & m4) {
                    gdouble vx = 0.5*(d1[1] + d2[1] - d1[0] - d2[0])/dx;
                    gdouble vy = 0.5*(d2[0] + d2[1] - d1[0] - d1[1])/dy;
                    gdouble q = exp((vx*vx + vy*vy)/sigma2);
                    sumvx += vx/q;
                    sumvy += vy/q;
                    sumvz += 1.0/q;
                }
            }
        }
    }
    GWY_MAYBE_SET(bx, 0.5*sumvx/sumvz*field->xreal);
    GWY_MAYBE_SET(by, 0.5*sumvy/sumvz*field->yreal);
    return TRUE;

fail:
    GWY_MAYBE_SET(bx, 0.0);
    GWY_MAYBE_SET(by, 0.0);
    return FALSE;
}

static gboolean
poly_fit(guint id,
         gdouble *fvalues,
         gdouble *value,
         gpointer user_data)
{
    PolyFitData *data = (PolyFitData*)user_data;
    guint i = id/data->width;
    guint j = id % data->width;
    if (j == 0)
        data->p = data->base + i*data->xres;
    const gdouble *xp = data->xp + j;
    const gdouble *yp = data->yp + i;
    for (guint k = 0; k < data->nterms; k++)
        fvalues[k] = xp[data->xpowers[k]] * yp[data->ypowers[k]];
    *value = *data->p;
    data->p++;
    return TRUE;
}

static gboolean
poly_fit_mask(guint id,
              gdouble *fvalues,
              gdouble *value,
              gpointer user_data)
{
    PolyFitData *data = (PolyFitData*)user_data;
    guint i = id/data->width;
    guint j = id % data->width;
    if (j == 0) {
        data->p = data->base + i*data->xres;
        gwy_mask_field_iter_init(data->mask, data->iter,
                                 data->maskcol, data->maskrow+i);
    }
    gboolean ok = (!!gwy_mask_field_iter_get(data->iter) == data->mode);
    if (ok) {
        const gdouble *xp = data->xp + j;
        const gdouble *yp = data->yp + i;
        for (guint k = 0; k < data->nterms; k++)
            fvalues[k] = xp[data->xpowers[k]] * yp[data->ypowers[k]];
        *value = *data->p;
    }
    gwy_mask_field_iter_next(data->iter);
    data->p++;
    return ok;
}

static gdouble*
enumerate_powers(const guint *powers, guint nterms,
                 guint first, guint len, guint dim,
                 gsize *size)
{
    guint maxpower = powers[0];
    for (guint k = 1; k < nterms; k++) {
        if (powers[k] > maxpower)
            maxpower = powers[k];
    }

    *size = (maxpower + 1)*len*sizeof(gdouble);
    gdouble *powertable = g_slice_alloc(*size);
    if (dim == 1) {
        gwy_memclear(powertable, (maxpower + 1)*len);
        return powertable;
    }
    gdouble *p = powertable;
    for (guint i = 0; i < len; i++) {
        gdouble t = 2*(i + first)/(dim - 1.0) - 1.0;
        gdouble tp = 1.0;
        for (guint j = 0; j <= maxpower; j++, p++) {
            *p = tp;
            tp *= t;
        }
        *p = tp;
        p++;
    }
    return powertable;
}

/**
 * gwy_field_part_fit_poly:
 * @field: A two-dimensional data field.
 * @mask: Mask specifying which values to take into account/exclude, or %NULL.
 * @masking: Masking mode to use (has any effect only with non-%NULL @mask).
 * @col: Column index of the upper-left corner of the rectangle.
 * @row: Row index of the upper-left corner of the rectangle.
 * @width: Rectangle width (number of columns).  It should be at least 2.
 * @height: Rectangle height (number of rows).  It should be at least 2.
 * @xpowers: Array of length @nterms containing the powers of @x to fit.
 * @ypowers: Array of length @nterms containing the powers of @y to fit.
 * @nterms: Number of polynomial terms, i.e. the length of @xpowers,
 *          @ypowers and @coeffs.
 * @coeffs: Array of length @nterms to store the individual term coefficients
 *          to.
 *
 * Fits a general polynomial through a rectangular part of a field.
 *
 * The coefficients correspond to coordinates normalized to [-1,1], see the
 * introduction for details.
 *
 * The arrays @xpowers and @ypowers define the individual terms to fit.  For
 * instance the polynomial
 * @a + @b<subscript>x</subscript>@x + @b<subscript>y</subscript>@y + @c @x²
 * contains terms with the following (@x,@y) powers: (0,0), (1,0), (0,1) and
 * (2,0).  Therefore, to fit this polynomial and obtain
 * [@a, @b<subscript>x</subscript>, @b<subscript>y</subscript>, @c] in @coeffs,
 * the arrays should be set to
 * @xpowers = [0, 1, 0, 2] and @ypowers = [0, 0, 1, 0].
 *
 * Returns: %TRUE if the polynomial was fitted, %FALSE in degenerate cases,
 *          i.e. when points do not uniquely determine the polynomial with the
 *          requested terms.  For high polynomial degrees %FALSE can also be
 *          returned because the terms are no longer numerically independent.
 **/
gboolean
gwy_field_part_fit_poly(const GwyField *field,
                        const GwyMaskField *mask,
                        GwyMaskingType masking,
                        guint col, guint row, guint width, guint height,
                        const guint *xpowers, const guint *ypowers,
                        guint nterms,
                        gdouble *coeffs)
{
    if (!nterms || !coeffs)
        return TRUE;

    guint maskcol, maskrow;
    if (!_gwy_field_check_mask(field, mask, &masking,
                               col, row, width, height, &maskcol, &maskrow))
        goto fail;

    g_return_val_if_fail(xpowers && ypowers, FALSE);

    gsize xpsize, ypsize;
    gdouble *xp = enumerate_powers(xpowers, nterms, col, width, field->xres,
                                   &xpsize);
    gdouble *yp = enumerate_powers(ypowers, nterms, row, height, field->yres,
                                   &ypsize);

    PolyFitData data = {
        .width = width,
        .height = height,
        .base = field->data + row*field->xres + col,
        .xres = field->xres,
        .yres = field->yres,
        .mask = mask,
        .maskcol = maskcol,
        .maskrow = maskrow,
        .nterms = nterms,
        .xpowers = xpowers,
        .ypowers = ypowers,
        .xp = xp,
        .yp = yp,
    };
    gboolean ok;
    if (masking == GWY_MASK_IGNORE) {
        ok = gwy_linear_fit(poly_fit, width*height, coeffs, nterms, NULL,
                            &data);
    }
    else {
        data.mode = (masking == GWY_MASK_INCLUDE);
        ok = gwy_linear_fit(poly_fit_mask, width*height, coeffs, nterms, NULL,
                            &data);
    }
    g_slice_free1(xpsize, xp);
    g_slice_free1(ypsize, yp);
    if (!ok)
        goto fail;

    return TRUE;

fail:
    gwy_memclear(coeffs, nterms);
    return FALSE;
}

/**
 * gwy_field_subtract_poly:
 * @field: A two-dimensional data field.
 * @xpowers: Array of length @nterms containing the powers of @x to subtract.
 * @ypowers: Array of length @nterms containing the powers of @y to subtract.
 * @nterms: Number of polynomial terms, i.e. the length of @xpowers,
 *          @ypowers and @coeffs.
 * @coeffs: Array of length @nterms with the individual term coefficients.
 *
 * Subtracts a general polynomial from a field.
 *
 * The coefficients correspond to coordinates normalized to [-1,1], see the
 * introduction for details.  The meaning of @xpowers and @ypowers is described
 * in detail in gwy_field_part_fit_poly().
 **/
void
gwy_field_subtract_poly(GwyField *field,
                        const guint *xpowers, const guint *ypowers,
                        guint nterms,
                        const gdouble *coeffs)
{
    if (!nterms)
        return;
    g_return_if_fail(GWY_IS_FIELD(field));
    g_return_if_fail(xpowers && ypowers);
    g_return_if_fail(coeffs);

    gsize xpsize, ypsize;
    gdouble *xp = enumerate_powers(xpowers, nterms, 0, field->xres, field->xres,
                                   &xpsize);
    gdouble *yp = enumerate_powers(ypowers, nterms, 0, field->yres, field->yres,
                                   &ypsize);
    gdouble *d = field->data;
    for (guint i = 0; i < field->yres; i++) {
        const gdouble *y = yp + i;
        for (guint j = 0; j < field->xres; j++, d++) {
            const gdouble *x = xp + j;
            gdouble s = 0.0;
            for (guint k = 0; k < nterms; k++)
                s += coeffs[k] * x[xpowers[k]] * y[ypowers[k]];
            *d -= s;
        }
    }
    g_slice_free1(xpsize, xp);
    g_slice_free1(ypsize, yp);
    gwy_field_invalidate(field);
}

// TODO: We might want to use GwyLine once we have it.
/**
 * gwy_field_shift_rows:
 * @field: A two-dimensional data field.
 * @row: First row.
 * @height: Number of rows.
 * @shifts: Array of length @height with row value shifts.
 *
 * Shifts values in rows of a field by specified values.
 *
 * The shifts are absolute, i.e. values in each row is simply shifted by
 * the corresponding number in @shifts.  If you have relative shifts, i.e.
 * always with respect to the previous row, you can use gwy_line_cumulate()
 * to transform them to absolute shifts first.
 **/
void
gwy_field_shift_rows(GwyField *field,
                     guint row,
                     guint height,
                     const gdouble *shifts)
{
    if (!height)
        return;

    g_return_if_fail(GWY_IS_FIELD(field));
    g_return_if_fail(row + height <= field->yres);
    g_return_if_fail(shifts);

    for (guint i = 0; i < height; i++) {
        gdouble s = shifts[i];
        if (s) {
            gdouble *d = field->data + (row + i)*field->xres;
            for (guint j = field->xres; j; j--, d++)
                *d -= s;
        }
    }

    gwy_field_invalidate(field);
}

#define __LIBGWY_FIELD_LEVEL_C__
#include "libgwy/libgwy-aliases.c"

/**
 * SECTION: field-level
 * @title: GwyField levelling
 * @short_description: Field levelling and background subtraction
 *
 * All plane and polynomial fitting and subtracting methods use the following
 * convention for @x and @y coordinates (abscissas of the polynoms):  The
 * entire field range in either coordinate is transformed to interval [-1,1].
 * More precisely, the zeroth column/row corresponds to abscissa value of -1,
 * whereas the last column/row to 1.
 *
 * This permits to easily apply the same polynomial levelling to resampled
 * fields, however, the primary reason is improving the orthogonality of the
 * polynomial terms (which is rather poor) in the common case.
 *
 * Knowing the exact interpretation of the coefficients is not necessary to
 * just subtract the background as the fitting and subtracting functions follow
 * the same convention.  To obtain plane coefficients for pixel coordinates
 * (i.e. row and column) the following transformation can be used:
 * |[
 * ap = a - bx - by;
 * bcol = 2*bx/(field->xres - 1);
 * brow = 2*by/(field->yres - 1);
 * ]|
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
